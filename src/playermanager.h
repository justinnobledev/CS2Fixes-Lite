/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2024 Source2ZE
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include "common.h"
#include "utlvector.h"
#include "steam/steamclientpublic.h"
#include <playerslot.h>
#include "bitvec.h"
#include "entity/lights.h"
#include "entity/cparticlesystem.h"

#define DECAL_PREF_KEY_NAME "hide_decals"
#define HIDE_DISTANCE_PREF_KEY_NAME "hide_distance"
#define SOUND_STATUS_PREF_KEY_NAME "sound_status"
#define INVALID_ZEPLAYERHANDLE_INDEX 0u

static uint32 iZEPlayerHandleSerial = 0u; // this should actually be 3 bytes large, but no way enough players join in servers lifespan for this to be an issue

struct ClientJoinInfo_t
{
	uint64 steamid;
	double signon_timestamp;
};

extern CUtlVector<ClientJoinInfo_t> g_ClientsPendingAddon;

void AddPendingClient(uint64 steamid);
ClientJoinInfo_t *GetPendingClient(uint64 steamid, int &index);
ClientJoinInfo_t *GetPendingClient(INetChannel *pNetChan);

enum class ETargetType {
	NONE,
	PLAYER,
	SELF,
	RANDOM,
	RANDOM_T,
	RANDOM_CT,
	ALL,
	SPECTATOR,
	T,
	CT,
};

class ZEPlayer;

class ZEPlayerHandle
{
public:
	ZEPlayerHandle();
	ZEPlayerHandle(CPlayerSlot slot); // used for initialization inside ZEPlayer constructor
	ZEPlayerHandle(const ZEPlayerHandle& other);
	ZEPlayerHandle(ZEPlayer *pZEPlayer);

	bool IsValid() const { return static_cast<bool>(Get()); }

	uint32 GetIndex() const { return m_Index; }
	uint32 GetPlayerSlot() const { return m_Parts.m_PlayerSlot; }
	uint32 GetSerial() const { return m_Parts.m_Serial; }

	bool operator==(const ZEPlayerHandle &other) const { return other.m_Index == m_Index; }
	bool operator!=(const ZEPlayerHandle &other) const { return other.m_Index != m_Index; }
	bool operator==(ZEPlayer *pZEPlayer) const;
	bool operator!=(ZEPlayer *pZEPlayer) const;

	void operator=(const ZEPlayerHandle &other) { m_Index = other.m_Index; }
	void operator=(ZEPlayer *pZEPlayer) { Set(pZEPlayer); }
	void Set(ZEPlayer *pZEPlayer);
	
	ZEPlayer *Get() const;

private:
	union
	{
		uint32 m_Index;
		struct
		{
			uint32 m_PlayerSlot : 6;
			uint32 m_Serial : 26;
		} m_Parts;
	};
};

class ZEPlayer
{
public:
	ZEPlayer(CPlayerSlot slot, bool m_bFakeClient = false): m_slot(slot), m_bFakeClient(m_bFakeClient), m_Handle(slot)
	{ 
		m_bAuthenticated = false;
		m_SteamID = nullptr;
		m_bConnected = false;
		m_bInGame = false;
	}

	~ZEPlayer()
	{
	}

	bool IsFakeClient() { return m_bFakeClient; }
	bool IsAuthenticated() { return m_bAuthenticated; }
	bool IsConnected() { return m_bConnected; }
	uint64 GetUnauthenticatedSteamId64() { return m_UnauthenticatedSteamID->ConvertToUint64(); }
	const CSteamID* GetUnauthenticatedSteamId() { return m_UnauthenticatedSteamID; }
	uint64 GetSteamId64() { return m_SteamID->ConvertToUint64(); }
	const CSteamID* GetSteamId() { return m_SteamID; }

	void SetAuthenticated() { m_bAuthenticated = true; }
	void SetConnected() { m_bConnected = true; }
	void SetUnauthenticatedSteamId(const CSteamID* steamID) { m_UnauthenticatedSteamID = steamID; }
	void SetSteamId(const CSteamID* steamID) { m_SteamID = steamID; }
	void SetPlayerSlot(CPlayerSlot slot) { m_slot = slot; }
	void SetIpAddress(std::string strIp) { m_strIp = strIp; }
	void SetInGame(bool bInGame) { m_bInGame = bInGame; }

	CPlayerSlot GetPlayerSlot() { return m_slot; }
	const char* GetIpAddress() { return m_strIp.c_str(); }
	bool IsInGame() { return m_bInGame; }
	ZEPlayerHandle GetHandle() { return m_Handle; }
	
	void OnAuthenticated();

private:
	bool m_bAuthenticated;
	bool m_bConnected;
	const CSteamID* m_UnauthenticatedSteamID;
	const CSteamID* m_SteamID;
	bool m_bFakeClient;
	CPlayerSlot m_slot;
	std::string m_strIp;
	bool m_bInGame;
	ZEPlayerHandle m_Handle;
};

class CPlayerManager
{
public:
	CPlayerManager(bool late = false)
	{
		V_memset(m_vecPlayers, 0, sizeof(m_vecPlayers));

		if (late)
			OnLateLoad();
	}

	bool OnClientConnected(CPlayerSlot slot, uint64 xuid, const char* pszNetworkID);
	void OnClientDisconnect(CPlayerSlot slot);
	void OnBotConnected(CPlayerSlot slot);
	void OnClientPutInServer(CPlayerSlot slot);
	void OnLateLoad();
	void TryAuthenticate();
	CPlayerSlot GetSlotFromUserId(uint16 userid);
	ZEPlayer *GetPlayerFromUserId(uint16 userid);
	ZEPlayer *GetPlayerFromSteamId(uint64 steamid);

	ZEPlayer *GetPlayer(CPlayerSlot slot);

private:
	ZEPlayer *m_vecPlayers[MAXPLAYERS];

};

extern CPlayerManager *g_playerManager;