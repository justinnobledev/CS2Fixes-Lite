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

#include <../cs2fixes.h>
#include "utlstring.h"
#include "playermanager.h"
#include "entity/ccsplayercontroller.h"
#include "utils/entity.h"
#include "serversideclient.h"
#include "ctime"

#define VPROF_ENABLED
#include "tier0/vprof.h"

#include "tier0/memdbgon.h"


extern IVEngineServer2 *g_pEngineServer2;
extern CGameEntitySystem *g_pEntitySystem;
extern CGlobalVars *gpGlobals;
extern INetworkGameServer *g_pNetworkGameServer;

extern CUtlVector<CServerSideClient *> *GetClientList();

CUtlVector<ClientJoinInfo_t> g_ClientsPendingAddon;

void AddPendingClient(uint64 steamid)
{
	ClientJoinInfo_t PendingCLient {steamid, 0.f};
	g_ClientsPendingAddon.AddToTail(PendingCLient);
}

ClientJoinInfo_t *GetPendingClient(uint64 steamid, int &index)
{
	index = 0;

	FOR_EACH_VEC(g_ClientsPendingAddon, i)
	{
		if (g_ClientsPendingAddon[i].steamid == steamid)
		{
			index = i;
			return &g_ClientsPendingAddon[i];
		}
	}

	return nullptr;
}

ClientJoinInfo_t *GetPendingClient(INetChannel *pNetChan)
{
	CUtlVector<CServerSideClient *> *pClients = GetClientList();

	if (!pClients)
		return nullptr;

	FOR_EACH_VEC(*pClients, i)
	{
		CServerSideClient *pClient = pClients->Element(i);

		if (pClient && pClient->GetNetChannel() == pNetChan)
			return GetPendingClient(pClient->GetClientSteamID()->ConvertToUint64(), i); // just pass i here, it's discarded anyway
	}

	return nullptr;
}

ZEPlayerHandle::ZEPlayerHandle() : m_Index(INVALID_ZEPLAYERHANDLE_INDEX) {};

ZEPlayerHandle::ZEPlayerHandle(CPlayerSlot slot)
{
	m_Parts.m_PlayerSlot = slot.Get();
	m_Parts.m_Serial = ++iZEPlayerHandleSerial;
}

ZEPlayerHandle::ZEPlayerHandle(const ZEPlayerHandle &other)
{
	m_Index = other.m_Index;
}

ZEPlayerHandle::ZEPlayerHandle(ZEPlayer *pZEPlayer)
{
	Set(pZEPlayer);
}

bool ZEPlayerHandle::operator==(ZEPlayer *pZEPlayer) const
{
	return Get() == pZEPlayer;
}

bool ZEPlayerHandle::operator!=(ZEPlayer *pZEPlayer) const
{
	return Get() != pZEPlayer;
}

void ZEPlayerHandle::Set(ZEPlayer *pZEPlayer)
{
	if (pZEPlayer)
		m_Index = pZEPlayer->GetHandle().m_Index;
	else
		m_Index = INVALID_ZEPLAYERHANDLE_INDEX;
}

ZEPlayer *ZEPlayerHandle::Get() const
{
	ZEPlayer *pZEPlayer = g_playerManager->GetPlayer((CPlayerSlot) m_Parts.m_PlayerSlot);

	if (!pZEPlayer)
		return nullptr;
	
	if (pZEPlayer->GetHandle().m_Index != m_Index)
		return nullptr;
	
	return pZEPlayer;
}

void ZEPlayer::OnAuthenticated()
{
}

void CPlayerManager::OnBotConnected(CPlayerSlot slot)
{
	m_vecPlayers[slot.Get()] = new ZEPlayer(slot, true);
}

bool CPlayerManager::OnClientConnected(CPlayerSlot slot, uint64 xuid, const char* pszNetworkID)
{
	Assert(m_vecPlayers[slot.Get()] == nullptr);

	Message("%d connected\n", slot.Get());

	ZEPlayer *pPlayer = new ZEPlayer(slot);
	pPlayer->SetUnauthenticatedSteamId(new CSteamID(xuid));

	std::string ip(pszNetworkID);

	// Remove port
	for (int i = 0; i < ip.length(); i++)
	{
		if (ip[i] == ':')
		{
			ip = ip.substr(0, i);
			break;
		}
	}

	pPlayer->SetIpAddress(ip);

	pPlayer->SetConnected();
	m_vecPlayers[slot.Get()] = pPlayer;

	return true;
}

void CPlayerManager::OnClientDisconnect(CPlayerSlot slot)
{
	Message("%d disconnected\n", slot.Get());

	delete m_vecPlayers[slot.Get()];
	m_vecPlayers[slot.Get()] = nullptr;
}

void CPlayerManager::OnClientPutInServer(CPlayerSlot slot)
{
	ZEPlayer* pPlayer = m_vecPlayers[slot.Get()];

	pPlayer->SetInGame(true);
}

void CPlayerManager::OnLateLoad()
{
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);

		if (!pController || !pController->IsController() || !pController->IsConnected())
			continue;

		OnClientConnected(i, pController->m_steamID(), "0.0.0.0:0");
	}
}

void CPlayerManager::TryAuthenticate()
{
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		if (m_vecPlayers[i] == nullptr || !m_vecPlayers[i]->IsConnected())
			continue;

		if (m_vecPlayers[i]->IsAuthenticated() || m_vecPlayers[i]->IsFakeClient())
			continue;

		if (g_pEngineServer2->IsClientFullyAuthenticated(i))
		{
			m_vecPlayers[i]->SetAuthenticated();
			m_vecPlayers[i]->SetSteamId(m_vecPlayers[i]->GetUnauthenticatedSteamId());
			Message("%lli authenticated %d\n", m_vecPlayers[i]->GetSteamId64(), i);
			m_vecPlayers[i]->OnAuthenticated();
		}
	}
}

ZEPlayer *CPlayerManager::GetPlayer(CPlayerSlot slot)
{
	if (slot.Get() < 0 || slot.Get() >= gpGlobals->maxClients)
		return nullptr;

	return m_vecPlayers[slot.Get()];
};

// In userids, the lower byte is always the player slot
CPlayerSlot CPlayerManager::GetSlotFromUserId(uint16 userid)
{
	return CPlayerSlot(userid & 0xFF);
}

ZEPlayer *CPlayerManager::GetPlayerFromUserId(uint16 userid)
{
	uint8 index = userid & 0xFF;

	if (index >= gpGlobals->maxClients)
		return nullptr;

	return m_vecPlayers[index];
}

ZEPlayer* CPlayerManager::GetPlayerFromSteamId(uint64 steamid)
{
	for (ZEPlayer* player : m_vecPlayers)
	{
		if (player && player->IsAuthenticated() && player->GetSteamId64() == steamid)
			return player;
	}

	return nullptr;
}