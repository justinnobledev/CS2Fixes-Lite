#include "detour.h"
#include "common.h"
#include "mempatch.h"
#include "icvar.h"
#include "irecipientfilter.h"
#include "interfaces/cs2_interfaces.h"
#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"
#include "entity/cbasemodelentity.h"
#include "addresses.h"
#include "tier0/memdbgon.h"

extern CEntitySystem *g_pEntitySystem;

CMemPatch g_CommonPatches[] =
{
	CMemPatch(&modules::server, sigs::MovementUnlock, sigs::Patch_MovementUnlock, "ServerMovementUnlock", 1),
	CMemPatch(&modules::vscript, sigs::VScriptEnable, sigs::Patch_VScriptEnable, "VScriptEnable", 1),
};

CMemPatch g_ClientPatches[] =
{
	CMemPatch(&modules::client, sigs::MovementUnlock, sigs::Patch_MovementUnlock, "ClientMovementUnlock", 1),
};

#ifdef _WIN32
CMemPatch g_ToolsPatches[] =
{
	// Remove some -nocustomermachine checks without needing -nocustomermachine itself as it can break stuff, mainly to enable device selection in compiles
	CMemPatch(
		&modules::hammer,
		(byte*)"\xFF\x15\x2A\x2A\x2A\x2A\x84\xC0\x0F\x85\x2A\x2A\x2A\x2A\xB9",
		(byte*)"\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90",
		"HammerNoCustomerMachine",
		4
	),
};
#endif

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
	CDetour<decltype(Detour_Log)>(&modules::tier0, Detour_Log, "Msg" ),
	//CDetour<decltype(Detour_Log)>( modules::tier0, Detour_Log, "?ConMsg@@YAXPEBDZZ" ),
	//CDetour<decltype(Detour_Log)>( modules::tier0, Detour_Log, "?ConColorMsg@@YAXAEBVColor@@PEBDZZ" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "ConDMsg" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "DevMsg" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "Warning" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "DevWarning" ),
	//CDetour<decltype(Detour_Log)>( modules::tier0, Detour_Log, "?DevWarning@@YAXPEBDZZ" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "LoggingSystem_Log" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "LoggingSystem_LogDirect" ),
	CDetour<decltype(Detour_Log)>( &modules::tier0, Detour_Log, "LoggingSystem_LogAssert" ),
	//CDetour<decltype(Detour_Log)>( modules::tier0, Detour_IsChannelEnabled, "LoggingSystem_IsChannelEnabled" ),
};

void InitLoggingDetours()
{
	for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
		g_LoggingDetours[i].CreateDetour();
}

void ToggleLogs()
{
	static bool bBlock = false;

	if (!bBlock)
	{
		for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
			g_LoggingDetours[i].EnableDetour();
	}
	else
	{
		for (int i = 0; i < sizeof(g_LoggingDetours) / sizeof(*g_LoggingDetours); i++)
			g_LoggingDetours[i].DisableDetour();
	}

	bBlock = !bBlock;
}

void FASTCALL Detour_UTIL_SayTextFilter(IRecipientFilter &, const char *, CBasePlayerController *, uint64);
void FASTCALL Detour_UTIL_SayText2Filter(IRecipientFilter &, CBasePlayerController *, uint64, const char *, const char *, const char *, const char *, const char *);
void FASTCALL Detour_Host_Say(CCSPlayerController *, CCommand *, bool, int, const char *);
bool FASTCALL Detour_VoiceShouldHear(CCSPlayerController *a, CCSPlayerController *b, bool a3, bool voice);
void FASTCALL Detour_CSoundEmitterSystem_EmitSound(ISoundEmitterSystemBase *, CEntityIndex *, IRecipientFilter &, uint32, void *);

DECLARE_DETOUR(Host_Say, Detour_Host_Say, &modules::server);
DECLARE_DETOUR(UTIL_SayTextFilter, Detour_UTIL_SayTextFilter, &modules::server);
DECLARE_DETOUR(UTIL_SayText2Filter, Detour_UTIL_SayText2Filter, &modules::server);
DECLARE_DETOUR(VoiceShouldHear, Detour_VoiceShouldHear, &modules::server);
DECLARE_DETOUR(CSoundEmitterSystem_EmitSound, Detour_CSoundEmitterSystem_EmitSound, &modules::server);

int g_iBlockSoundIndex = -1;

void FASTCALL Detour_CSoundEmitterSystem_EmitSound(ISoundEmitterSystemBase *pSoundEmitterSystem, CEntityIndex *a2, IRecipientFilter &filter, uint32 a4, void *a5)
{
	if (g_iBlockSoundIndex == -1)
		return CSoundEmitterSystem_EmitSound(pSoundEmitterSystem, a2, filter, a4, a5);

	CCopyRecipientFilter copyFilter(&filter, g_iBlockSoundIndex);
	CSoundEmitterSystem_EmitSound(pSoundEmitterSystem, a2, copyFilter, a4, a5);
}

bool FASTCALL Detour_VoiceShouldHear(CCSPlayerController* a, CCSPlayerController* b, bool a3, bool voice)
{
	if (voice)
	{
		ConMsg("block");
		return 0;
	}

	ConMsg("a: %s, b: %s\n", &a->m_iszPlayerName(), &b->m_iszPlayerName());
	ConMsg("VoiceShouldHear: %llx %llx %i %i\n", a, b, a3, voice);
	//Detour_VoiceShouldHear(a, b, a3, a4);

	return VoiceShouldHear(a, b, a3, voice);
}

void FASTCALL Detour_UTIL_SayTextFilter(IRecipientFilter &filter, const char *pText, CBasePlayerController *pPlayer, uint64 eMessageType)
{
	int entindex = filter.GetRecipientIndex(0).Get();
	CBasePlayerController *target = (CBasePlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)entindex);

	if (target)
		Message("[CS2Fixes] Chat from %s to %s: %s\n", pPlayer ? &pPlayer->m_iszPlayerName() : "console", &target->m_iszPlayerName(), pText);

	if (pPlayer)
		return UTIL_SayTextFilter(filter, pText, pPlayer, eMessageType);

	char buf[256];
	V_snprintf(buf, sizeof(buf), "%s%s", " \7CONSOLE:\4", pText + sizeof("Console:"));

	UTIL_SayTextFilter(filter, buf, pPlayer, eMessageType);
}

void FASTCALL Detour_UTIL_SayText2Filter(
	IRecipientFilter &filter, 
	CBasePlayerController *pEntity, 
	uint64 eMessageType, 
	const char *msg_name,
	const char *param1, 
	const char *param2, 
	const char *param3, 
	const char *param4)
{
	int entindex = filter.GetRecipientIndex(0).Get() + 1;
	CBasePlayerController *target = (CBasePlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)entindex);

	if (target)	
		Message("[CS2Fixes] Chat from %s to %s: %s\n", param1, &target->m_iszPlayerName(), param2);

	UTIL_SayText2Filter(filter, pEntity, eMessageType, msg_name, param1, param2, param3, param4);
}

WeaponMapEntry_t WeaponMap[] = {
	{"bizon",		  "weapon_bizon",		  1400},
	{"mac10",		  "weapon_mac10",		  1400},
	{"mp7",			"weapon_mp7",			  1700},
	{"mp9",			"weapon_mp9",			  1250},
	{"p90",			"weapon_p90",			  2350},
	{"ump45",		  "weapon_ump45",		  1700},
	{"ak47",			 "weapon_ak47",			2500},
	{"aug",			"weapon_aug",			  3500},
	{"famas",		  "weapon_famas",		  2250},
	{"galilar",		"weapon_galilar",		  2000},
	{"m4a4",			 "weapon_m4a4",			3100},
	{"m4a1_silencer", "weapon_m4a1_silencer", 3100},
	{"m4a1",			 "weapon_m4a1_silencer", 3100},
	{"a1",			"weapon_m4a1_silencer", 3100},
	{"sg556",		  "weapon_sg556",		  3500},
	{"awp",			"weapon_awp",			  4750},
	{"g3sg1",		  "weapon_g3sg1",		  5000},
	{"scar20",		   "weapon_scar20",		5000},
	{"ssg08",		  "weapon_ssg08",		  2500},
	{"mag7",			 "weapon_mag7",			2000},
	{"nova",			 "weapon_nova",			1500},
	{"sawedoff",		 "weapon_sawedoff",		1500},
	{"xm1014",		   "weapon_xm1014",		3000},
	{"m249",			 "weapon_m249",			5750},
	{"negev",		  "weapon_negev",		  5750},
	{"deagle",		   "weapon_deagle",		700 },
	{"elite",		  "weapon_elite",		  800 },
	{"fiveseven",	  "weapon_fiveseven",	  500 },
	{"glock",		  "weapon_glock",		  200 },
	{"hkp2000",		"weapon_hkp2000",		  200 },
	{"p250",			 "weapon_p250",			300 },
	{"tec9",			 "weapon_tec9",			500 },
	{"usp_silencer",	 "weapon_usp_silencer",	200 },
	{"cz75a",		  "weapon_cz75a",		  500 },
	{"revolver",		 "weapon_revolver",		600 },
	{"kevlar",		   "item_kevlar",		  600 },
	{"he",			"weapon_hegrenade",	   300 },
	{"molotov",		"weapon_hegrenade",		850 },
};

void SendConsoleChat(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[256];
	V_vsnprintf(buf, sizeof(buf), msg, args);

	va_end(args);

	CCommand newArgs;
	newArgs.Tokenize(buf);
	Host_Say(nullptr, &newArgs, false, 0, nullptr);
}

void ParseChatCommand(const char *pMessage, CCSPlayerController *pController)
{
	if (!pController)
		return;

	char *token, *pos;

	char *pCommand = strdup(pMessage);
	token = strtok_s(strdup(pCommand) + 1, " ", &pos);

	if (!V_stricmp(token, "map"))
	{
		token = strtok_s(nullptr, " ", &pos);

		SendConsoleChat("[CS2Fixes] Changing map to %s", token);
		Message("[CS2Fixes] Changing map to %s\n", token);
	}
	else if (!V_stricmp(token, "takemoney"))
	{
		token = strtok_s(nullptr, " ", &pos);

		int money = pController->m_pInGameMoneyServices()->m_iAccount();
		pController->m_pInGameMoneyServices()->m_iAccount(money - atoi(token));
	}
	else if (!V_stricmp(token, "setcollision"))
	{
		Collision_Group_t colgroup = (Collision_Group_t)atoi(strtok_s(nullptr, " ", &pos));
		int uid = atoi(strtok_s(nullptr, " ", &pos));

		Z_CBaseEntity *target = (Z_CBaseEntity*)g_pEntitySystem->GetBaseEntity((CEntityIndex)uid);
		if (target)
			target->m_pCollision()->m_CollisionGroup(colgroup);
	}
	else if (!V_stricmp(token, "setcollisionplayer"))
	{
		Collision_Group_t colgroup = (Collision_Group_t)atoi(strtok_s(nullptr, " ", &pos));
		int uid = atoi(strtok_s(nullptr, " ", &pos));

		CCSPlayerController *targetController = (CCSPlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)uid);

		if (targetController)
			targetController->m_hPawn().Get()->m_pCollision()->m_CollisionGroup(colgroup);
	}
	else if (!V_stricmp(token, "message"))
	{
		// Note that the engine will treat this as a player slot number, not an entity index
		int uid = atoi(strtok_s(nullptr, " ", &pos));

		CBasePlayerController *player = (CBasePlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)(uid + 1));

		if (!player)
			return;

		char message[256];
		V_snprintf(message, sizeof(message), " \7[CS2Fixes]\1 Private message to %s: \5%s", &player->m_iszPlayerName(), pos);

		CSingleRecipientFilter filter(uid);

		UTIL_SayTextFilter(filter, message, nullptr, 0);
	}
	else if (!V_stricmp(token, "setsolid"))
	{
		SolidType_t colgroup = (SolidType_t)atoi(strtok_s(nullptr, " ", &pos));
		int uid = atoi(strtok_s(nullptr, " ", &pos));

		Z_CBaseEntity *target = (Z_CBaseEntity*)g_pEntitySystem->GetBaseEntity((CEntityIndex)uid);
		if (target)
			target->m_pCollision()->m_nSolidType(colgroup);
	}
	else if (!V_stricmp(token, "setglow"))
	{
		int uid = atoi(strtok_s(nullptr, " ", &pos));

		CBaseModelEntity *target = (CBaseModelEntity*)g_pEntitySystem->GetBaseEntity((CEntityIndex)uid);
		if (target)
		{
			target->m_Glow().m_bGlowing(true);
			target->m_Glow().m_fGlowColor(Vector(255, 255, 0));
			target->m_Glow().m_iGlowType(1);
			target->m_Glow().m_nGlowRange(1000);
		}
	}
	else if (!V_stricmp(token, "basevelocity"))
	{
		int uid = atoi(strtok_s(nullptr, " ", &pos));
		float x = atof(strtok_s(nullptr, " ", &pos));
		float y = atof(strtok_s(nullptr, " ", &pos));
		float z = atof(strtok_s(nullptr, " ", &pos));

		CBasePlayerController *target = (CBasePlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)uid);
		if (target)
			target->m_hPawn().Get()->m_vecBaseVelocity(Vector(x, y, z));
	}
	else if (!V_stricmp(token, "sethealth"))
	{
		int uid = atoi(strtok_s(nullptr, " ", &pos));
		int health = atoi(strtok_s(nullptr, " ", &pos));

		CBasePlayerController *target = (CBasePlayerController *)g_pEntitySystem->GetBaseEntity((CEntityIndex)uid);
		if (target)
			target->m_hPawn().Get()->m_iHealth(health);
	}
	else if (!V_stricmp(token, "gethealth"))
	{
		int uid = atoi(strtok_s(nullptr, " ", &pos));

		CBasePlayerController* target = (CBasePlayerController*)g_pEntitySystem->GetBaseEntity((CEntityIndex)uid);
		if (target)
			ConMsg("Health: %llx\n", target->m_hPawn().Get()->m_iHealth());
	}
	else if (!V_stricmp(token, "stopsound"))
	{
		g_iBlockSoundIndex = atoi(strtok_s(nullptr, " ", &pos));
	}
	else
	{
		for (int i = 0; i < sizeof(WeaponMap) / sizeof(*WeaponMap); i++)
		{
			WeaponMapEntry_t weaponEntry = WeaponMap[i];

			if (!V_stricmp(token, weaponEntry.command))
			{
				uintptr_t pItemServices = (uintptr_t)pController->m_hPawn().Get()->m_pItemServices();
				int money = pController->m_pInGameMoneyServices()->m_iAccount();
				if (money >= weaponEntry.iPrice)
				{
					pController->m_pInGameMoneyServices()->m_iAccount(money - weaponEntry.iPrice);
					addresses::GiveNamedItem(pItemServices, weaponEntry.szWeaponName, 0, 0, 0, 1);
				}

				break;
			}
		}
	}

	free(pCommand);
}

void FASTCALL Detour_Host_Say(CCSPlayerController *pController, CCommand *args, bool teamonly, int unk1, const char *unk2)
{
	if(g_pEntitySystem == nullptr)
		g_pEntitySystem = interfaces::pGameResourceServiceServer->GetGameEntitySystem();

	const char *pMessage = args->Arg(1);

	if (*pMessage == '!' || *pMessage == '/')
		ParseChatCommand(pMessage, pController);

	if (*pMessage == '/')
		return;

	Host_Say(pController, args, teamonly, unk1, unk2);
}

void InitPatches()
{
	for (int i = 0; i < sizeof(g_CommonPatches) / sizeof(*g_CommonPatches); i++)
		g_CommonPatches[i].PerformPatch();

	// Dedicated servers don't load client
	if (!CommandLine()->HasParm("-dedicated"))
	{
		for (int i = 0; i < sizeof(g_ClientPatches) / sizeof(*g_ClientPatches); i++)
			g_ClientPatches[i].PerformPatch();
	}

#ifdef _WIN32
	// None of the tools are loaded without, well, -tools
	if (CommandLine()->HasParm("-tools"))
	{
		for (int i = 0; i < sizeof(g_ToolsPatches) / sizeof(*g_ToolsPatches); i++)
			g_ToolsPatches[i].PerformPatch();
	}
#endif
	
	UTIL_SayTextFilter.CreateDetour();
	UTIL_SayTextFilter.EnableDetour();

	UTIL_SayText2Filter.CreateDetour();
	UTIL_SayText2Filter.EnableDetour();

	Host_Say.CreateDetour();
	Host_Say.EnableDetour();

	VoiceShouldHear.CreateDetour();
	VoiceShouldHear.EnableDetour();

	CSoundEmitterSystem_EmitSound.CreateDetour();
	CSoundEmitterSystem_EmitSound.EnableDetour();
}

void UndoPatches()
{
	for (int i = 0; i < sizeof(g_CommonPatches) / sizeof(*g_CommonPatches); i++)
		g_CommonPatches[i].UndoPatch();

	if (!CommandLine()->HasParm("-dedicated"))
	{
		for (int i = 0; i < sizeof(g_ClientPatches) / sizeof(*g_ClientPatches); i++)
			g_ClientPatches[i].UndoPatch();
	}

#ifdef _WIN32
	if (CommandLine()->HasParm("-tools"))
	{
		for (int i = 0; i < sizeof(g_ToolsPatches) / sizeof(*g_ToolsPatches); i++)
			g_ToolsPatches[i].UndoPatch();
	}
#endif
}
