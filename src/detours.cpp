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

#include "networkbasetypes.pb.h"

#include "cdetour.h"
#include "common.h"
#include "module.h"
#include "addresses.h"
#include "commands.h"
#include "detours.h"
#include "ctimer.h"
#include "irecipientfilter.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "entity/cbasemodelentity.h"
#include "entity/ccsweaponbase.h"
#include "entity/ctriggerpush.h"
#include "entity/cgamerules.h"
#include "entity/ctakedamageinfo.h"
#include "entity/services.h"
#include "playermanager.h"
#include "igameevents.h"
#include "gameconfig.h"
#include "serversideclient.h"
#include "networksystem/inetworkserializer.h"

#define VPROF_ENABLED
#include "tier0/vprof.h"

#include "tier0/memdbgon.h"

extern CGlobalVars *gpGlobals;
extern CGameEntitySystem *g_pEntitySystem;
extern IGameEventManager2 *g_gameEventManager;
extern CCSGameRules *g_pGameRules;

DECLARE_DETOUR(TriggerPush_Touch, Detour_TriggerPush_Touch);
DECLARE_DETOUR(CGameRules_Constructor, Detour_CGameRules_Constructor);
DECLARE_DETOUR(CNavMesh_GetNearestNavArea, Detour_CNavMesh_GetNearestNavArea);

void FASTCALL Detour_CGameRules_Constructor(CGameRules *pThis)
{
	g_pGameRules = (CCSGameRules*)pThis;
	CGameRules_Constructor(pThis);
}

static bool g_bUseOldPush = false;

FAKE_BOOL_CVAR(cs2f_use_old_push, "Whether to use the old CSGO trigger_push behavior", g_bUseOldPush, false, false)

void FASTCALL Detour_TriggerPush_Touch(CTriggerPush* pPush, Z_CBaseEntity* pOther)
{
	// This trigger pushes only once (and kills itself) or pushes only on StartTouch, both of which are fine already
	if (!g_bUseOldPush || pPush->m_spawnflags() & SF_TRIG_PUSH_ONCE || pPush->m_bTriggerOnStartTouch())
	{
		TriggerPush_Touch(pPush, pOther);
		return;
	}

	MoveType_t movetype = pOther->m_nActualMoveType();

	// VPhysics handling doesn't need any changes
	if (movetype == MOVETYPE_VPHYSICS)
	{
		TriggerPush_Touch(pPush, pOther);
		return;
	}

	if (movetype == MOVETYPE_NONE || movetype == MOVETYPE_PUSH || movetype == MOVETYPE_NOCLIP)
		return;

	CCollisionProperty* collisionProp = pOther->m_pCollision();
	if (!IsSolid(collisionProp->m_nSolidType(), collisionProp->m_usSolidFlags()))
		return;

	if (!pPush->PassesTriggerFilters(pOther))
		return;

	if (pOther->m_CBodyComponent()->m_pSceneNode()->m_pParent())
		return;

	Vector vecAbsDir;

	matrix3x4_t mat = pPush->m_CBodyComponent()->m_pSceneNode()->EntityToWorldTransform();
	
	Vector pushDir = pPush->m_vecPushDirEntitySpace();

	// i had issues with vectorrotate on linux so i did it here
	vecAbsDir.x = pushDir.x * mat[0][0] + pushDir.y * mat[0][1] + pushDir.z * mat[0][2];
	vecAbsDir.y = pushDir.x * mat[1][0] + pushDir.y * mat[1][1] + pushDir.z * mat[1][2];
	vecAbsDir.z = pushDir.x * mat[2][0] + pushDir.y * mat[2][1] + pushDir.z * mat[2][2];

	Vector vecPush = vecAbsDir * pPush->m_flSpeed();

	uint32 flags = pOther->m_fFlags();

	if (flags & (FL_BASEVELOCITY))
	{
		vecPush = vecPush + pOther->m_vecBaseVelocity();
	}

	if (vecPush.z > 0 && (flags & FL_ONGROUND))
	{
		addresses::SetGroundEntity(pOther, nullptr);
		Vector origin = pOther->GetAbsOrigin();
		origin.z += 1.0f;

		pOther->Teleport(&origin, nullptr, nullptr);
	}

	pOther->m_vecBaseVelocity(vecPush);

	flags |= (FL_BASEVELOCITY);
	pOther->m_fFlags(flags);
}


void Detour_Log()
{
	return;
}

bool FASTCALL Detour_IsChannelEnabled(LoggingChannelID_t channelID, LoggingSeverity_t severity)
{
	return false;
}

CDetour<decltype(Detour_Log)> g_LoggingDetours[] =
{
	CDetour<decltype(Detour_Log)>( Detour_Log, "Msg" ),
	//CDetour<decltype(Detour_Log)>( Detour_Log, "?ConMsg@@YAXPEBDZZ" ),
	//CDetour<decltype(Detour_Log)>( Detour_Log, "?ConColorMsg@@YAXAEBVColor@@PEBDZZ" ),
	CDetour<decltype(Detour_Log)>( Detour_Log, "ConDMsg" ),
	CDetour<decltype(Detour_Log)>( Detour_Log, "DevMsg" ),
	CDetour<decltype(Detour_Log)>( Detour_Log, "Warning" ),
	CDetour<decltype(Detour_Log)>( Detour_Log, "DevWarning" ),
	//CDetour<decltype(Detour_Log)>( Detour_Log, "?DevWarning@@YAXPEBDZZ" ),
	CDetour<decltype(Detour_Log)>( Detour_Log, "LoggingSystem_Log" ),
	CDetour<decltype(Detour_Log)>( Detour_Log, "LoggingSystem_LogDirect" ),
	CDetour<decltype(Detour_Log)>( Detour_Log, "LoggingSystem_LogAssert" ),
	//CDetour<decltype(Detour_Log)>( Detour_IsChannelEnabled, "LoggingSystem_IsChannelEnabled" ),
};

CON_COMMAND_F(toggle_logs, "Toggle printing most logs and warnings", FCVAR_SPONLY | FCVAR_LINKED_CONCOMMAND)
{
	static bool bBlock = false;

	if (!bBlock)
	{
		Message("Logging is now OFF.\n");

		for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
			g_LoggingDetours[i].EnableDetour();
	}
	else
	{
		Message("Logging is now ON.\n");

		for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
			g_LoggingDetours[i].DisableDetour();
	}

	bBlock = !bBlock;
}

bool g_bBlockNavLookup = false;
FAKE_BOOL_CVAR(cs2f_block_nav_lookup, "Whether to block navigation mesh lookup, improves server performance but breaks bot navigation", g_bBlockNavLookup, false, false)
void* FASTCALL Detour_CNavMesh_GetNearestNavArea(int64_t unk1, float* unk2, unsigned int* unk3, unsigned int unk4, int64_t unk5, int64_t unk6, float unk7, int64_t unk8)
{
	if (g_bBlockNavLookup)
		return nullptr;

	return CNavMesh_GetNearestNavArea(unk1, unk2, unk3, unk4, unk5, unk6, unk7, unk8);
}

CUtlVector<CDetourBase *> g_vecDetours;

bool InitDetours(CGameConfig *gameConfig)
{
	bool success = true;

	g_vecDetours.PurgeAndDeleteElements();

	for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
	{
		if (!g_LoggingDetours[i].CreateDetour(gameConfig))
			success = false;
	}

	if (!TriggerPush_Touch.CreateDetour(gameConfig))
		success = false;
	TriggerPush_Touch.EnableDetour();

	if (!CGameRules_Constructor.CreateDetour(gameConfig))
		success = false;
	CGameRules_Constructor.EnableDetour();

	if (!CNavMesh_GetNearestNavArea.CreateDetour(gameConfig))
		success = false;
	CNavMesh_GetNearestNavArea.EnableDetour();

	return success;
}

void FlushAllDetours()
{
	g_vecDetours.Purge();
}
