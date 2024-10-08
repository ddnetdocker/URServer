#ifndef GAME_SERVER_GAMEMODES_MOD_H
#define GAME_SERVER_GAMEMODES_MOD_H

#include <game/server/gamecontroller.h>

class CGameControllerMod : public IGameController
{
public:
	CGameControllerMod(class CGameContext *pGameServer);
	~CGameControllerMod();

	CScore *Score();

	void HandleCharacterTiles(class CCharacter *pChr, int MapIndex) override;
	void SetArmorProgress(CCharacter *pCharacer, int Progress) override;

	void OnPlayerConnect(class CPlayer *pPlayer) override;
	void OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason) override;

	void OnReset() override;

	void Tick() override;

	void DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg = true) override;

	void startChallenge(CCharacter *pChr);
	void checkTeamFail();
	bool IsTeamFail(int Team);
	void TeleTeamToCheckpoint(int Team, int TeleTo, vec2 Pos = vec2(-1, -1));
	void Teleport(CCharacter *pChr, vec2 Pos);

};
#endif // GAME_SERVER_GAMEMODES_MOD_H
