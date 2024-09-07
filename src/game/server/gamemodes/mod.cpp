#include "mod.h"

#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include <climits>

// Exchange this to a string that identifies your game mode.
// DM, TDM and CTF are reserved for teeworlds original modes.
// DDraceNetwork and TestDDraceNetwork are used by DDNet.
#define GAME_TYPE_NAME "Race"
#define TEST_TYPE_NAME "TestRace"

CGameControllerMod::CGameControllerMod(class CGameContext *pGameServer) :
	IGameController(pGameServer)
{
	m_pGameType = g_Config.m_SvTestingCommands ? TEST_TYPE_NAME : GAME_TYPE_NAME;

	// m_GameFlags = GAMEFLAG_TEAMS; // GAMEFLAG_TEAMS makes it a two-team gamemode
}

CGameControllerMod::~CGameControllerMod() = default;

CScore *CGameControllerMod::Score()
{
	return GameServer()->Score();
}

void CGameControllerMod::HandleCharacterTiles(CCharacter *pChr, int MapIndex)
{
	CPlayer *pPlayer = pChr->GetPlayer();
	const int ClientId = pPlayer->GetCid();

	int TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
	int TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);

	// Sensitivity
	int S1 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x + pChr->GetProximityRadius() / 3.f, pChr->GetPos().y - pChr->GetProximityRadius() / 3.f));
	int S2 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x + pChr->GetProximityRadius() / 3.f, pChr->GetPos().y + pChr->GetProximityRadius() / 3.f));
	int S3 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x - pChr->GetProximityRadius() / 3.f, pChr->GetPos().y - pChr->GetProximityRadius() / 3.f));
	int S4 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x - pChr->GetProximityRadius() / 3.f, pChr->GetPos().y + pChr->GetProximityRadius() / 3.f));
	int Tile1 = GameServer()->Collision()->GetTileIndex(S1);
	int Tile2 = GameServer()->Collision()->GetTileIndex(S2);
	int Tile3 = GameServer()->Collision()->GetTileIndex(S3);
	int Tile4 = GameServer()->Collision()->GetTileIndex(S4);
	int FTile1 = GameServer()->Collision()->GetFTileIndex(S1);
	int FTile2 = GameServer()->Collision()->GetFTileIndex(S2);
	int FTile3 = GameServer()->Collision()->GetFTileIndex(S3);
	int FTile4 = GameServer()->Collision()->GetFTileIndex(S4);

	const int PlayerDDRaceState = pChr->m_DDRaceState;
	bool IsOnStartTile = (TileIndex == TILE_START) || (TileFIndex == TILE_START) || FTile1 == TILE_START || FTile2 == TILE_START || FTile3 == TILE_START || FTile4 == TILE_START || Tile1 == TILE_START || Tile2 == TILE_START || Tile3 == TILE_START || Tile4 == TILE_START;
	// start
	if(IsOnStartTile && PlayerDDRaceState != DDRACE_CHEAT)
	{
		const int Team = GameServer()->GetDDRaceTeam(ClientId);
		if(Teams().GetSaving(Team))
		{
			GameServer()->SendStartWarning(ClientId, "You can't start while loading/saving of team is in progress");
			pChr->Die(ClientId, WEAPON_WORLD);
			return;
		}
		if(g_Config.m_SvTeam == SV_TEAM_MANDATORY && (Team == TEAM_FLOCK || Teams().Count(Team) <= 1))
		{
			GameServer()->SendStartWarning(ClientId, "You have to be in a team with other tees to start");
			pChr->Die(ClientId, WEAPON_WORLD);
			return;
		}
		if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && Team > TEAM_FLOCK && Team < TEAM_SUPER && Teams().Count(Team) < g_Config.m_SvMinTeamSize && !Teams().TeamFlock(Team))
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Your team has fewer than %d players, so your team rank won't count", g_Config.m_SvMinTeamSize);
			GameServer()->SendStartWarning(ClientId, aBuf);
		}
		if(g_Config.m_SvResetPickups)
		{
			pChr->ResetPickups();
		}

		Teams().OnCharacterStart(ClientId);
		pChr->m_LastTimeCp = -1;
		pChr->m_LastTimeCpBroadcasted = -1;
		for(float &CurrentTimeCp : pChr->m_aCurrentTimeCp)
		{
			CurrentTimeCp = 0.0f;
		}
	}

	// finish
	if(((TileIndex == TILE_FINISH) || (TileFIndex == TILE_FINISH) || FTile1 == TILE_FINISH || FTile2 == TILE_FINISH || FTile3 == TILE_FINISH || FTile4 == TILE_FINISH || Tile1 == TILE_FINISH || Tile2 == TILE_FINISH || Tile3 == TILE_FINISH || Tile4 == TILE_FINISH) && PlayerDDRaceState == DDRACE_STARTED)
		Teams().OnCharacterFinish(ClientId);

	// unlock team
	else if(((TileIndex == TILE_UNLOCK_TEAM) || (TileFIndex == TILE_UNLOCK_TEAM)) && Teams().TeamLocked(GameServer()->GetDDRaceTeam(ClientId)))
	{
		Teams().SetTeamLock(GameServer()->GetDDRaceTeam(ClientId), false);
		GameServer()->SendChatTeam(GameServer()->GetDDRaceTeam(ClientId), "Your team was unlocked by an unlock team tile");
	}

	// solo part
	if(((TileIndex == TILE_SOLO_ENABLE) || (TileFIndex == TILE_SOLO_ENABLE)) && !Teams().m_Core.GetSolo(ClientId))
	{
		GameServer()->SendChatTarget(ClientId, "You are now in a solo part");
		pChr->SetSolo(true);
	}
	else if(((TileIndex == TILE_SOLO_DISABLE) || (TileFIndex == TILE_SOLO_DISABLE)) && Teams().m_Core.GetSolo(ClientId))
	{
		GameServer()->SendChatTarget(ClientId, "You are now out of the solo part");
		pChr->SetSolo(false);
	}

	// Detect when a player hit CHALLENGEQUEUE tile
	bool IsOnQueue = (TileIndex == TILE_CHALLENGEQUEUE) || (TileFIndex == TILE_CHALLENGEQUEUE) || FTile1 == TILE_CHALLENGEQUEUE || FTile2 == TILE_CHALLENGEQUEUE || FTile3 == TILE_CHALLENGEQUEUE || FTile4 == TILE_CHALLENGEQUEUE || Tile1 == TILE_CHALLENGEQUEUE || Tile2 == TILE_CHALLENGEQUEUE || Tile3 == TILE_CHALLENGEQUEUE || Tile4 == TILE_CHALLENGEQUEUE;
	if(IsOnQueue)
	{
		if(pChr->m_DDRaceState != DDRACE_STARTED)
		{
			startChallenge(pChr);
		}
	}
}

void CGameControllerMod::SetArmorProgress(CCharacter *pCharacer, int Progress)
{
	pCharacer->SetArmor(clamp(10 - (Progress / 15), 0, 10));
}

void CGameControllerMod::OnPlayerConnect(CPlayer *pPlayer)
{
	IGameController::OnPlayerConnect(pPlayer);
	int ClientId = pPlayer->GetCid();

	// init the player
	Score()->PlayerData(ClientId)->Reset();

	// Can't set score here as LoadScore() is threaded, run it in
	// LoadScoreThreaded() instead
	Score()->LoadPlayerData(ClientId);

	if(!Server()->ClientPrevIngame(ClientId))
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "'%s' entered and joined the %s", Server()->ClientName(ClientId), GetTeamName(pPlayer->GetTeam()));
		GameServer()->SendChat(-1, TEAM_ALL, aBuf, -1, CGameContext::FLAG_SIX);

		GameServer()->SendChatTarget(ClientId, "DDraceNetwork Mod. Version: " GAME_VERSION);
		GameServer()->SendChatTarget(ClientId, "please visit DDNet.org or say /info and make sure to read our /rules");
	}
}

void CGameControllerMod::OnPlayerDisconnect(CPlayer *pPlayer, const char *pReason)
{
	int ClientId = pPlayer->GetCid();
	bool WasModerator = pPlayer->m_Moderating && Server()->ClientIngame(ClientId);

	IGameController::OnPlayerDisconnect(pPlayer, pReason);

	if(!GameServer()->PlayerModerating() && WasModerator)
		GameServer()->SendChat(-1, TEAM_ALL, "Server kick/spec votes are no longer actively moderated.");

	if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO)
		Teams().SetForceCharacterTeam(ClientId, TEAM_FLOCK);

	for(int Team = TEAM_FLOCK + 1; Team < TEAM_SUPER; Team++)
		if(Teams().IsInvited(Team, ClientId))
			Teams().SetClientInvited(Team, ClientId, false);
}

void CGameControllerMod::OnReset()
{
	IGameController::OnReset();
	Teams().Reset();
}

void CGameControllerMod::Tick()
{
	IGameController::Tick();
	Teams().ProcessSaveTeam();
	Teams().Tick();
	checkTeamFail();
}

void CGameControllerMod::DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg)
{
	Team = ClampTeam(Team);
	if(Team == pPlayer->GetTeam())
		return;

	CCharacter *pCharacter = pPlayer->GetCharacter();

	if(Team == TEAM_SPECTATORS)
	{
		if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && pCharacter)
		{
			// Joining spectators should not kill a locked team, but should still
			// check if the team finished by you leaving it.
			int DDRTeam = pCharacter->Team();
			Teams().SetForceCharacterTeam(pPlayer->GetCid(), TEAM_FLOCK);
			Teams().CheckTeamFinished(DDRTeam);
		}
	}

	IGameController::DoTeamChange(pPlayer, Team, DoChatMsg);
}

void CGameControllerMod::startChallenge(CCharacter *pChr)
{
	CPlayer *pPlayer = pChr->GetPlayer();
	const int ClientId = pPlayer->GetCid();

	if(pPlayer == nullptr)
		return;

	const int Team = GameServer()->GetDDRaceTeam(ClientId);
	if(Team == TEAM_FLOCK)
	{
		int newTeam = GameServer()->m_pController->Teams().GetFirstEmptyTeam();
		if(newTeam == -1)
		{
			GameServer()->SendChatTarget(ClientId, "No team available to join");
			pChr->Die(ClientId, WEAPON_WORLD);
			return;
		}
		GameServer()->m_pController->Teams().SetForceCharacterTeam(ClientId, newTeam);
		return;
	}

	if(Teams().GetSaving(Team))
	{
		GameServer()->SendStartWarning(ClientId, "You can't start while loading/saving of team is in progress");
		pChr->Die(ClientId, WEAPON_WORLD);
		return;
	}
	if(g_Config.m_SvTeam == SV_TEAM_MANDATORY && (Team == TEAM_FLOCK || Teams().Count(Team) <= 1))
	{
		GameServer()->SendStartWarning(ClientId, "You have to be in a team with other tees to start");
		pChr->Die(ClientId, WEAPON_WORLD);
		return;
	}
	if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && Team > TEAM_FLOCK && Team < TEAM_SUPER && Teams().Count(Team) < g_Config.m_SvMinTeamSize && !Teams().TeamFlock(Team))
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Your team has fewer than %d players, so your team rank won't count", g_Config.m_SvMinTeamSize);
		GameServer()->SendStartWarning(ClientId, aBuf);
	}
	if(g_Config.m_SvResetPickups)
	{
		pChr->ResetPickups();
	}

	// Teleport to the start of the challenge
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i] && GameServer()->GetDDRaceTeam(i) == Team)
		{
			CCharacter *pChar = GameServer()->m_apPlayers[i]->GetCharacter();
			vec2 spawnPoint = vec2(10, 10);
			Teams().OnCharacterStart(i);
			pChar->m_LastTimeCp = -1;
			pChar->m_LastTimeCpBroadcasted = -1;
			for(float &CurrentTimeCp : pChar->m_aCurrentTimeCp)
			{
				CurrentTimeCp = 0.0f;
			}
			spawnPoint = GameServer()->m_pController->GetChallengeStartPos();

			Teleport(pChar, spawnPoint);
		}
	}
}

void CGameControllerMod::checkTeamFail()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(IsTeamFail(i))
		{
			int lowest = INT_MAX;
			for(int x = 0; x < MAX_CLIENTS; x++)
			{
				CPlayer *p = GameServer()->m_apPlayers[x];
				if(p == nullptr)
					continue;
				if(GameServer()->GetDDRaceTeam(p->GetCid()) == i)
				{
					if(p->GetCharacter()->m_TeleCheckpoint < lowest)
						lowest = p->GetCharacter()->m_TeleCheckpoint;
				}
			}
			TeleTeamToCheckpoint(i, lowest);
		}
	}
}

bool CGameControllerMod::IsTeamFail(int team)
{
	if(team == TEAM_FLOCK)
	{
		return false;
	}

	bool Empty = true;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i] == nullptr || GameServer()->m_apPlayers[i]->GetCharacter() == nullptr)
		{
			continue;
		}
		if(GameServer()->GetDDRaceTeam(i) != team)
		{
			continue;
		}

		Empty = false;

		float velx = ((float)((int)(GameServer()->m_apPlayers[i]->GetCharacter()->Core()->m_Vel.x * 10))) / 10;
		float vely = ((float)((int)(GameServer()->m_apPlayers[i]->GetCharacter()->Core()->m_Vel.y * 10))) / 10;
		bool velx0 = (velx > -0.1 && velx < 0.1);
		bool vely0 = (vely > -0.1 && vely < 0.1);
		bool isMoving = !(velx0 && vely0);
		bool isFreeze = GameServer()->m_apPlayers[i]->GetCharacter()->Core()->m_IsInFreeze;
		if(!isFreeze)
		{
			return false;
		}
		else
		{
			if(isMoving)
			{
				return false;
			}
		}
	}
	return !Empty;
}

void CGameControllerMod::TeleTeamToCheckpoint(int Team, int TeleTo, vec2 Pos)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->GetDDRaceTeam(i) == Team)
		{
			if(GameServer()->m_apPlayers[i] == nullptr)
				continue;

			vec2 TelePos = vec2(10, 10);
			if(TeleTo == 0)
			{
				TelePos = GameServer()->m_pController->GetChallengeStartPos();
			}
			else
			{
				if(!GameServer()->Collision()->TeleCheckOuts(TeleTo - 1).empty())
				{
					CCharacter *pChr = GameServer()->GetPlayerChar(i);
					if(pChr)
					{
						int TeleOut = GameServer()->m_World.m_Core.RandomOr0(GameServer()->Collision()->TeleCheckOuts(TeleTo - 1).size());
						TelePos = GameServer()->Collision()->TeleCheckOuts(TeleTo - 1)[TeleOut];
					}
				}
			}

			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetCharacter())
			{
				Teleport(GameServer()->m_apPlayers[i]->GetCharacter(), TelePos);
				GameServer()->m_apPlayers[i]->GetCharacter()->m_FreezeTime = (TeleTo == 0) ? 50 : 100;
			}
		}
	}
}

void CGameControllerMod::Teleport(CCharacter *pChr, vec2 Pos)
{
	pChr->SetPosition(Pos);
	pChr->m_Pos = Pos;
	pChr->m_PrevPos = Pos;
	pChr->ResetJumps();
	pChr->UnFreeze();
	pChr->SetVelocity(vec2(0, 0));
}