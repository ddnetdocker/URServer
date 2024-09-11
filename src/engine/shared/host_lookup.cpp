/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "host_lookup.h"

#include <base/system.h>
#include "json.h"
#include <curl/curl.h>
#include <iostream>

CHostLookup::CHostLookup() = default;

CHostLookup::CHostLookup(const char *pHostname, int Nettype)
{
	str_copy(m_aHostname, pHostname);
	m_Nettype = Nettype;
	Abortable(true);
}

size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *output)
{
	size_t total_size = size * nmemb;
	output->append((char *)contents, total_size);
	return total_size;
}

void CHostLookup::Run()
{
	CURL *curl;
	CURLcode res;

	// Initialize cURL
	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();

	if(curl)
	{
		// Set the URL
		const char *url = m_aHostname;
		curl_easy_setopt(curl, CURLOPT_URL, url);

		// Set the callback function to receive the response
		std::string response;
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

		// Perform the request
		res = curl_easy_perform(curl);

		// Check for errors
		if(res != CURLE_OK) {
			dbg_msg("host_lookup", "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			m_Result = 0;
		} else {
			json_value *pJson = json_parse(response.c_str(), response.length());
			if(pJson->type != json_object)
			{
				json_value_free(pJson);
				dbg_msg("host_lookup", "invalid JSON response from host_lookup");
				return;
			}
			const json_value &Json = *pJson;
			const json_value &isBan = Json["isBan"];
			const json_value &state = Json["state"];
			const json_value &reason = Json["reason"];

			if (state.type != json_string)
			{
				json_value_free(pJson);
				m_Result = 0;
				dbg_msg("host_lookup", "invalid JSON response from host_lookup (state)");
				return;
			}
			if (isBan.type != json_boolean)
			{
				json_value_free(pJson);
				m_Result = 0;
				dbg_msg("host_lookup", "invalid JSON response from host_lookup (isBan)");
				return;
			}
			if (reason.type != json_string)
			{
				json_value_free(pJson);
				m_Result = 0;
				dbg_msg("host_lookup", "invalid JSON response from host_lookup (reason)");
				return;
			}

			// Set the result and reason
			m_Result = isBan.u.boolean ? 0 : 1;
/* 			// switch case for state string vpn, player, local, error	
			if (str_comp_nocase(state.u.string.ptr, "vpn") == 0)
			{
				str_copy(m_aReason, "VPN detected", sizeof(m_aReason));
			}
			else if (str_comp_nocase(state.u.string.ptr, "player") == 0)
			{
				str_copy(m_aReason, "Blacklisted name", sizeof(m_aReason));
			}
			else if (str_comp_nocase(state.u.string.ptr, "local") == 0)
			{
				str_copy(m_aReason, "Local player", sizeof(m_aReason));
			}
			else if (str_comp_nocase(state.u.string.ptr, "error") == 0)
			{
				str_copy(m_aReason, "Error occurred", sizeof(m_aReason));
			}
			dbg_msg("host_lookup", "request <%s> returned <%d> <%s>", m_aHostname, m_Result, m_aReason);
 */
		// Cleanup
		curl_easy_cleanup(curl);
	}
}
	// Cleanup cURL globally
	curl_global_cleanup();
}
