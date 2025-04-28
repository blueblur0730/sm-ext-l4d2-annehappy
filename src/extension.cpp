/**
 * vim: set ts=4 :
 * =============================================================================
 * SourceMod Sample Extension
 * Copyright (C) 2004-2008 AlliedModders LLC.  All rights reserved.
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
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#include "extension.h"
#include "compat_wrappers.h"
#include "SI/boomer.h"
#include "SI/smoker.h"
#include "SI/charger.h"
#include "sourcehook.h"
#include <vector>
#include "dt_send.h"
#include "utils.h"
//#include "KeyValues.h"

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

CAnneHappy g_AnneHappy;		/**< Global singleton for extension's main interface */
SMEXT_LINK(&g_AnneHappy);

SH_DECL_MANUALHOOK2_void(PlayerRunCmdHook, 0, 0, 0, CUserCmd *, IMoveHelper *);

ICvar* icvar = NULL;
IServerGameEnts *gameents= NULL;
IGameEventManager2 *gameevents= NULL;
CGlobalVars *gpGlobals= NULL;
//ISDKHooks *sdkhooks= NULL;
IServerGameClients *serverClients= NULL;
IBinTools *bintools= NULL;
IEngineTrace *enginetrace= NULL;
IStaticPropMgr *staticpropmgr = NULL;

TerrorNavMesh *g_pNavMesh = NULL;
ZombieManager *g_pZombieManager = NULL;

BossZombiePlayerBot g_BossZombiePlayerBot;

CBoomerEventListner g_BoomerEventListner;
CSmokerEventListner g_SmokerEventListner;
CChargerEventListner g_ChargerEventListner;

int g_iOff_PlayerRunCmd = 0;
int CBaseEntity::vtblindex_CBaseEntity_Teleport = 0;
int CBaseEntity::vtblindex_CBaseEntity_GetEyeAngle = 0;
int CTerrorPlayer::vtblindex_CTerrorPlayer_GetLastKnownArea = 0;

//ICallWrapper *CTraceFilterSimpleExt::pCallCTraceFilterSimple = NULL;
ICallWrapper *CBaseEntity::pCallTeleport = NULL;
ICallWrapper *CBaseEntity::pCallGetEyeAngle = NULL;
ICallWrapper *CTerrorPlayer::pCallOnVomitedUpon = NULL;
ICallWrapper *CTerrorPlayer::pCallGetSpecialInfectedDominatingMe = NULL;
ICallWrapper *CTerrorPlayer::pCallIsStaggering = NULL;
ICallWrapper *CTerrorPlayer::pCallGetLastKnownArea = NULL;
ICallWrapper *ZombieManager::pCallGetRandomPZSpawnPosition = NULL;

int CVomit::m_iOff_m_isSpraying = 0;
int CBasePlayer::m_iOff_m_fFlags = 0;
int CBaseCombatWeapon::m_iOff_m_bInReload = 0;
int CEnvPhysicsBlocker::m_iOff_m_nBlockType = 0;
int CTerrorPlayer::m_iOff_m_zombieClass = 0;
int CTerrorPlayer::m_iOff_m_customAbility = 0;
int CTerrorPlayer::m_iOff_m_hasVisibleThreats = 0;
int CTerrorPlayer::m_iOff_m_isIncapacitated = 0;
int CTerrorPlayer::m_iOff_m_tongueVictim = 0;
int CTerrorPlayer::m_iOff_m_hGroundEntity = 0;
int CTerrorPlayer::m_iOff_m_hActiveWeapon = 0;

int TerrorNavMesh::m_iOff_m_fMapMaxFlowDistance = 0;
int CNavArea::m_iOff_m_flow = 0;

//void *CTraceFilterSimpleExt::pFnCTraceFilterSimple = NULL;
void *CTerrorPlayer::pFnOnVomitedUpon = NULL;
void *CTerrorPlayer::pFnGetSpecialInfectedDominatingMe = NULL;
void *CTerrorPlayer::pFnIsStaggering = NULL;
void *BossZombiePlayerBot::pFnChooseVictim = NULL;
void *ZombieManager::pFnGetRandomPZSpawnPosition = NULL;
Fn_IsVisibleToPlayer pFnIsVisibleToPlayer;

CDetour *CTerrorPlayer::DTR_OnVomitedUpon = NULL;
CDetour *BossZombiePlayerBot::DTR_ChooseVictim = NULL;

// so yeah the detours like to take the lead.
DETOUR_DECL_MEMBER3(DTRHandler_BossZombiePlayerBot_ChooseVictim, CTerrorPlayer *, CTerrorPlayer *, pLastVictim, int, targetScanFlags, CBasePlayer *, pIgnorePlayer)
{
	CTerrorPlayer *_this = reinterpret_cast<CTerrorPlayer *>(this);
	CTerrorPlayer *pPlayer = NULL;

	switch (_this->GetClass())
	{
		case ZC_BOOMER:
		{
			pPlayer = g_BossZombiePlayerBot.OnBoomerChooseVictim(_this, pLastVictim, targetScanFlags, pIgnorePlayer);
			break;
		}

		case ZC_SMOKER:
		{
			pPlayer = g_BossZombiePlayerBot.OnSmokerChooseVictim(_this, pLastVictim, targetScanFlags, pIgnorePlayer);
			break;
		}
	}

	return pPlayer == NULL ?
	DETOUR_MEMBER_CALL(DTRHandler_BossZombiePlayerBot_ChooseVictim)(pLastVictim, targetScanFlags, pIgnorePlayer) : 
	DETOUR_MEMBER_CALL(DTRHandler_BossZombiePlayerBot_ChooseVictim)(pPlayer, targetScanFlags, pIgnorePlayer);
}

bool CAnneHappy::SDK_OnLoad(char* error, size_t maxlen, bool late) 
{
	sharesys->AddDependency(myself, "bintools.ext", true, true);
	//sharesys->AddDependency(myself, "sdkhooks.ext", true, true);

	IGameConfig *pGameData = nullptr;
	if (!gameconfs->LoadGameConfigFile("sdktools.games", &pGameData, error, maxlen))
	{
		ke::SafeStrcpy(error, maxlen, "Extension failed to load gamedata file: 'sdktools.games'");
		return false;
	}
	else
	{
		if (!LoadSDKToolsData(pGameData, error, maxlen))
			return false;
	}

	if (!gameconfs->LoadGameConfigFile(GAMEDATA_FILE, &pGameData, error, maxlen))
	{
		ke::SafeStrcpy(error, maxlen, "Extension failed to load gamedata file: \"" GAMEDATA_FILE ".txt\"");
		return false;
	}
	else
	{
		if (!LoadGameData(pGameData, error, maxlen))
			return false;

		if (!FindSendProps(pGameData, error, maxlen))
			return false;
	}

	gameconfs->CloseGameConfigFile(pGameData);

	if (!AddEventListner())
		return false;

	// Game config is never used by detour class to handle errors ourselves
	CDetourManager::Init(smutils->GetScriptingEngine(), NULL);

	if (!g_BoomerEventListner.CreateDetour(error, maxlen))
	{
		ke::SafeStrcpy(error, maxlen, "Extension failed to create detour: 'CTerrorPlayer::DTR_OnVomitedUpon'");
		return false;
	}

	BossZombiePlayerBot::DTR_ChooseVictim = DETOUR_CREATE_MEMBER(DTRHandler_BossZombiePlayerBot_ChooseVictim, BossZombiePlayerBot::pFnChooseVictim);
	if (!BossZombiePlayerBot::DTR_ChooseVictim)
	{
		ke::SafeStrcpy(error, maxlen, "Extension failed to create detour: 'BossZombiePlayerBot_ChooseVictim'");
		return false;
	}

	playerhelpers->AddClientListener(this);
	smutils->LogMessage(myself, "Annehappy extension has been loaded.");
	sharesys->RegisterLibrary(myself, "annehappy");
	return true;
}

void CAnneHappy::SDK_OnAllLoaded()
{
	SM_GET_LATE_IFACE(BINTOOLS, bintools);
	//SM_GET_LATE_IFACE(SDKHOOKS, sdkhooks);

	if (!bintools/* || !sdkhooks*/)
	{
		smutils->LogError(myself, "Extension failed to load required libraries.");
		return;
	}

	PassInfo info[] = {
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(IHandleEntity *), NULL, 0},
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(int), NULL, 0},
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(ShouldHitFunc_t), NULL, 0},
	};

	PassInfo info2[] = {
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(CBasePlayer *), NULL, 0},
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(bool), NULL, 0},
	};

	CTerrorPlayer::pCallOnVomitedUpon = bintools->CreateCall(CTerrorPlayer::pFnOnVomitedUpon, CallConv_ThisCall, NULL, &info2[0], sizeof(info2));
	if (!CTerrorPlayer::pCallOnVomitedUpon)
	{
		smutils->LogError(myself, "Extension failed to create call: 'CTerrorPlayer::OnVomitedUpon'");
		return;
	}

	PassInfo ret[] = {
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(CTerrorPlayer *), NULL, 0},
	};

	CTerrorPlayer::pCallGetSpecialInfectedDominatingMe = bintools->CreateCall(CTerrorPlayer::pFnGetSpecialInfectedDominatingMe, CallConv_ThisCall, &ret[0], NULL, 0);
	if (!CTerrorPlayer::pCallGetSpecialInfectedDominatingMe)
	{
		smutils->LogError(myself, "Extension failed to create call: 'CTerrorPlayer::GetSpecialInfectedDominatingMe'");
		return;
	}

	PassInfo info3[] = {
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(Vector *), NULL, 0},
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(QAngle *), NULL, 0},
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(Vector *), NULL, 0}
	};

	CBaseEntity::pCallTeleport = bintools->CreateVCall(CBaseEntity::vtblindex_CBaseEntity_Teleport, 0, 0, NULL, &info3[0], sizeof(info3));
	if (!CBaseEntity::pCallTeleport)
	{
		smutils->LogError(myself, "Extension failed to create vcall: 'CBaseEntity::Teleport'");
		return;
	}

	PassInfo ret4[] = {
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(QAngle *), NULL, 0}
	};
	CBaseEntity::pCallGetEyeAngle = bintools->CreateVCall(CBaseEntity::vtblindex_CBaseEntity_GetEyeAngle, 0, 0, &ret4[0], NULL, 0);
	if (!CBaseEntity::pCallGetEyeAngle)
	{
		smutils->LogError(myself, "Extension failed to create vcall: 'CBaseEntity::GetEyeAngle'");
		return;
	}

	PassInfo ret1[] = {
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(bool), NULL, 0}
	};

	CTerrorPlayer::pCallIsStaggering = bintools->CreateCall(CTerrorPlayer::pFnIsStaggering, CallConv_ThisCall, &ret1[0], NULL, 0);
	if (!CTerrorPlayer::pCallIsStaggering)
	{
		smutils->LogError(myself, "Extension failed to create call: 'CTerrorPlayer::IsStaggering'");
		return;
	}

	PassInfo info5[] = {
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(ZombieClassType), NULL, 0},
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(int), NULL, 0},
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(CTerrorPlayer *), NULL, 0},
		{PassType_Basic, PASSFLAG_BYREF, sizeof(Vector *), NULL, 0}
	};

	PassInfo ret2[] = {
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(bool), NULL, 0},
	};

	ZombieManager::pCallGetRandomPZSpawnPosition = bintools->CreateCall(ZombieManager::pFnGetRandomPZSpawnPosition, CallConv_ThisCall, &ret2[0], &info5[0], sizeof(info5));
	if (!ZombieManager::pCallGetRandomPZSpawnPosition)
	{
		smutils->LogError(myself, "Extension failed to create call: 'ZombieManager::GetRandomPZSpawnPosition'");
		return;
	}

	PassInfo ret3[] = {
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(void *), NULL, 0},
	};
	CTerrorPlayer::pCallGetLastKnownArea = bintools->CreateVCall(CTerrorPlayer::vtblindex_CTerrorPlayer_GetLastKnownArea, 0, 0, &ret3[0], NULL, 0);
	if (!CTerrorPlayer::pCallGetLastKnownArea)
	{
		smutils->LogError(myself, "Extension failed to create vcall: 'CTerrorPlayer::GetLastKnownArea'");
		return;
	}

	PassInfo info6[] = {
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(Vector *), NULL, 0},
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(AngularImpulse *), NULL, 0},
	};

	CTerrorPlayer::DTR_OnVomitedUpon->EnableDetour();
	BossZombiePlayerBot::DTR_ChooseVictim->EnableDetour();
}

void CAnneHappy::SDK_OnUnload()
{
	for (auto it = g_hookList.begin(); it != g_hookList.end(); ++it)
	{
		delete *it;
	}

	g_hookList.clear();

	RemoveEventListner();

	//DestroyCalls(CTraceFilterSimpleExt::pCallCTraceFilterSimple);
	DestroyCalls(CBaseEntity::pCallTeleport);
	DestroyCalls(CBaseEntity::pCallGetEyeAngle);
	DestroyCalls(CTerrorPlayer::pCallGetSpecialInfectedDominatingMe);
	DestroyCalls(CTerrorPlayer::pCallIsStaggering);
	DestroyCalls(ZombieManager::pCallGetRandomPZSpawnPosition);
	DestroyDetours(CTerrorPlayer::DTR_OnVomitedUpon);
	DestroyDetours(BossZombiePlayerBot::DTR_ChooseVictim);

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		g_BoomerEventListner.OnClientDisconnecting(i);
		g_SmokerEventListner.OnClientDisconnecting(i);
	}

	playerhelpers->RemoveClientListener(this);
	smutils->LogMessage(myself, "Extension has been unloaded.");
}

bool CAnneHappy::SDK_OnMetamodLoad(ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	if (ismm->GetSourceEngineBuild() != SOURCE_ENGINE_LEFT4DEAD2)
	{
		snprintf(error, maxlen, "Extension only suport Left 4 Dead 2.");
		return false;
	}

	GET_V_IFACE_CURRENT(GetEngineFactory, icvar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetServerFactory, gameents, IServerGameEnts, INTERFACEVERSION_SERVERGAMEENTS);
	GET_V_IFACE_CURRENT(GetEngineFactory, gameevents, IGameEventManager2, INTERFACEVERSION_GAMEEVENTSMANAGER2);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer, INTERFACEVERSION_VENGINESERVER);
	GET_V_IFACE_CURRENT(GetEngineFactory, enginetrace, IEngineTrace, INTERFACEVERSION_ENGINETRACE_SERVER);
	GET_V_IFACE_CURRENT(GetEngineFactory, staticpropmgr, IStaticPropMgr, INTERFACEVERSION_STATICPROPMGR_SERVER)
	GET_V_IFACE_ANY(GetServerFactory, serverClients, IServerGameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
	gpGlobals = ismm->GetCGlobals();

	g_pCVar = icvar;
	CONVAR_REGISTER(this);
	return true;
}

bool CAnneHappy::RegisterConCommandBase(ConCommandBase* command)
{
	return META_REGCVAR(command);
}

bool CAnneHappy::LoadSDKToolsData(IGameConfig *pGameData, char* error, size_t maxlen)
{
	static const struct {
		char const* name;
		int& pOffset;
		char const *filename;
	} s_offsets[] = {
		{"PlayerRunCmd", g_iOff_PlayerRunCmd, "sdktools.games"},
		{"Teleport", CBaseEntity::vtblindex_CBaseEntity_Teleport, "sdktools.games"},
		{"EyeAngles", CBaseEntity::vtblindex_CBaseEntity_GetEyeAngle, "sdktools.games"},
	};

	for (auto &offset : s_offsets)
	{
		if (!pGameData->GetOffset(offset.name, &offset.pOffset))
		{
			ke::SafeSprintf(error, maxlen, "Could not find offset for \"%s\" in \"%s\"", offset.name, offset.filename);
			return false;
		}
	}

	SH_MANUALHOOK_RECONFIGURE(PlayerRunCmdHook, g_iOff_PlayerRunCmd, 0, 0);
	return true;
}

bool CAnneHappy::LoadGameData(IGameConfig *pGameData, char* error, size_t maxlen)
{
	static const struct {
		char const* name;
		int& pOffset;
		char const *filename;
	} s_offsets[] = {
		{"m_fMapMaxFlowDistance", TerrorNavMesh::m_iOff_m_fMapMaxFlowDistance, GAMEDATA_FILE},
		{"CTerrorPlayer::GetLastKnownArea", CTerrorPlayer::vtblindex_CTerrorPlayer_GetLastKnownArea, GAMEDATA_FILE},
		{"m_flow", CNavArea::m_iOff_m_flow, GAMEDATA_FILE},
		{"m_PlayerAnimState", CTerrorPlayer::m_iOff_m_PlayerAnimState, GAMEDATA_FILE},
		{"m_eCurrentMainSequenceActivity", CMultiPlayerAnimState::m_iOff_m_eCurrentMainSequenceActivity, GAMEDATA_FILE}
	};

	for (auto &offset : s_offsets)
	{
		if (!pGameData->GetOffset(offset.name, &offset.pOffset))
		{
			ke::SafeSprintf(error, maxlen, "Could not find offset for \"%s\" in \"%s\"", offset.name, offset.filename);
			return false;
		}
	}
	
	static const struct {
		char const* name;
		void** pFn;
	} s_sigs[] = {
		{"CTerrorPlayer::OnVomitedUpon", &CTerrorPlayer::pFnOnVomitedUpon},
		{"CTerrorPlayer::GetSpecialInfectedDominatingMe", &CTerrorPlayer::pFnGetSpecialInfectedDominatingMe},
		{"IsVisibleToPlayer", (void **)&pFnIsVisibleToPlayer},
		{"BossZombiePlayerBot::ChooseVictim", &BossZombiePlayerBot::pFnChooseVictim},
		{"CTerrorPlayer::IsStaggering", &CTerrorPlayer::pFnIsStaggering},
		{"ZombieManager::GetRandomPZSpawnPosition", &ZombieManager::pFnGetRandomPZSpawnPosition}
	};

	for (auto &sig : s_sigs)
	{
		if (!pGameData->GetMemSig(sig.name, sig.pFn))
		{
			ke::SafeSprintf(error, maxlen, "Could not find signature for \"%s\" in \"" GAMEDATA_FILE ".txt\".", sig.name);
			return false;
		}
	}

	if (!pGameData->GetAddress("TerrorNavMesh", (void **)(&g_pNavMesh)) || !g_pNavMesh)
	{
		ke::SafeSprintf(error, maxlen, "Could not find address for \"TerrorNavMesh\" in \"" GAMEDATA_FILE ".txt\".");
		return false;
	}

	if (!pGameData->GetAddress("ZombieManager", (void **)(&g_pZombieManager)) || !g_pZombieManager)
	{
		ke::SafeSprintf(error, maxlen, "Could not find address for \"ZombieManager\" in \"" GAMEDATA_FILE ".txt\".");
		return false;
	}

	return true;
}

bool CAnneHappy::AddEventListner()
{
	const char *sBoomerEventName[] = {
		"player_spawn",
		"player_shoved",
		"player_now_it",
		"round_start"
	};

	for (int i = 0; i < sizeof(sBoomerEventName); i++)
	{
		if (!gameevents->AddListener(&g_BoomerEventListner, sBoomerEventName[i], true))
		{
			smutils->LogError(myself, "Extension failed to add event listner: '%s' for boomer.", sBoomerEventName[i]);
			return false;
		}
	}

	if (!gameevents->AddListener(&g_SmokerEventListner, "round_start", true))
	{
		smutils->LogError(myself, "Extension failed to add event listner: 'round_start' for smoker.");
		return false;
	}

	if (!gameevents->AddListener(&g_ChargerEventListner, "player_spawn", true))
	{
		smutils->LogError(myself, "Extension failed to add event listner: 'player_spawn' for charger.");
		return false;
	}

	return true;
}

void CAnneHappy::RemoveEventListner()
{
	gameevents->RemoveListener(&g_BoomerEventListner);
	gameevents->RemoveListener(&g_SmokerEventListner);
	gameevents->RemoveListener(&g_ChargerEventListner);
}

bool CAnneHappy::FindSendProps(IGameConfig *pGameData, char* error, size_t maxlen)
{
	static const struct {
		char const *propName;
		char const *className;
		int& pOffset;
	} s_props[] = {
		{"m_isSpraying", "CVomit", CVomit::m_iOff_m_isSpraying},
		{"m_isCharging", "CCharge", CCharge::m_iOff_m_isCharging},
		{"m_nextActivationTimer", "CBaseAbility", CBaseAbility::m_iOff_m_nextActivationTimer},
		{"m_nBlockType", "CEnvPhysicsBlocker", CEnvPhysicsBlocker::m_iOff_m_nBlockType},
		{"m_bInReload", "CBaseCombatWeapon", CBaseCombatWeapon::m_iOff_m_bInReload},
		{"m_Gender", "CBaseEntity", CBaseEntity::m_iOff_m_Gender},
		{"m_fFlags", "CBasePlayer", CBasePlayer::m_iOff_m_fFlags},
		{"m_nSequence", "CBasePlayer", CBasePlayer::m_iOff_m_nSequence},
		{"m_zombieClass", "CTerrorPlayer", CTerrorPlayer::m_iOff_m_zombieClass},
		{"m_customAbility", "CTerrorPlayer", CTerrorPlayer::m_iOff_m_customAbility},
		{"m_hasVisibleThreats", "CTerrorPlayer", CTerrorPlayer::m_iOff_m_hasVisibleThreats},
		{"m_isIncapacitated", "CTerrorPlayer", CTerrorPlayer::m_iOff_m_isIncapacitated},
		{"m_tongueVictim", "CTerrorPlayer", CTerrorPlayer::m_iOff_m_tongueVictim},
		{"m_hGroundEntity", "CTerrorPlayer", CTerrorPlayer::m_iOff_m_hGroundEntity},
		{"m_hActiveWeapon", "CTerrorPlayer", CTerrorPlayer::m_iOff_m_hActiveWeapon},
		{"m_pummelVictim", "CTerrorPlayer", CTerrorPlayer::m_iOff_m_pummelVictim},
		{"m_carryVictim", "CTerrorPlayer", CTerrorPlayer::m_iOff_m_carryVictim},
	};

	sm_sendprop_info_t info;
	for (auto &prop : s_props)
	{
		if (!gamehelpers->FindSendPropInfo(prop.className, prop.propName, &info))
		{
			ke::SafeSprintf(error, maxlen, "Extension failed to find send props: '%s' in class '%s'.", prop.propName, prop.className);
			return false;
		}

		prop.pOffset = info.actual_offset;
	}

	return true;
}

void CAnneHappy::OnClientPutInServer(int client)
{
#ifdef _DEBUG
	rootconsole->ConsolePrint("### CAnneHappy::OnClientPutInServer: %d", client);
#endif

	g_BoomerEventListner.OnClientPutInServer(client);
	g_SmokerEventListner.OnClientPutInServer(client);

	PlayerRunCmdHook(client);
}

void CAnneHappy::OnClientDisconnecting(int client)
{
#ifdef _DEBUG
	rootconsole->ConsolePrint("### CAnneHappy::OnClientDisconnecting, client: %d", client);
#endif

	g_BoomerEventListner.OnClientDisconnecting(client);
	g_SmokerEventListner.OnClientDisconnecting(client);
}

void CAnneHappy::PlayerRunCmdHook(int client)
{
	// only listen SIs.
	CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndexExt(client);
	if (!pPlayer || ((CTerrorPlayer *)pPlayer)->IsSurvivor())
		return;

	edict_t *pEdict = pPlayer->edict();
	if (!pEdict)
		return;
	
	IServerUnknown *pUnknown = pEdict->GetUnknown();
	if (!pUnknown)
		return;

	CBaseEntity *pEntity = pUnknown->GetBaseEntity();
	if (!pEntity)
		return;

	CVTableHook hook(pEntity);
	for (size_t i = 0; i < g_hookList.size(); ++i)
	{
		if (hook == g_hookList[i])
			return;
	}

	int hookid = SH_ADD_MANUALVPHOOK(PlayerRunCmdHook, pEntity, SH_MEMBER(this, &CAnneHappy::PlayerRunCmd), false);

	hook.SetHookID(hookid);
	g_hookList.push_back(new CVTableHook(hook));
}

void CAnneHappy::PlayerRunCmd(CUserCmd *ucmd, IMoveHelper *moveHelper)
{
	if (!ucmd)
		RETURN_META(MRES_IGNORED);
	
	CTerrorPlayer *pPlayer = reinterpret_cast<CTerrorPlayer *>(META_IFACEPTR(CBaseEntity));

	if (!pPlayer)
		RETURN_META(MRES_IGNORED);

	edict_t *pEdict = gameents->BaseEntityToEdict(pPlayer);

	if (!pEdict)
		RETURN_META(MRES_IGNORED);
	
	int client = pPlayer->entindex();

	if (client < 1 || client > gpGlobals->maxClients)
		RETURN_META(MRES_IGNORED);
	
	if (!pPlayer->IsFakeClient() || !pPlayer->IsInfected())
		RETURN_META(MRES_IGNORED);

	switch (pPlayer->GetClass())
	{
		case ZC_BOOMER:
		{
			g_BoomerEventListner.OnPlayerRunCmd(pPlayer, ucmd);
			break;
		}

		case ZC_SMOKER:
		{
			g_SmokerEventListner.OnPlayerRunCmd(pPlayer, ucmd);
			break;
		}

		case ZC_CHARGER:
		{
			g_ChargerEventListner.OnPlayerRunCmd(pPlayer, ucmd);
			break;
		}
	}

	RETURN_META(MRES_IGNORED);
}

void CAnneHappy::DestroyCalls(ICallWrapper *pCall)
{
	if (pCall)
	{
		pCall->Destroy();
		pCall = NULL;
	}
}

void CAnneHappy::DestroyDetours(CDetour *pDetour)
{
	if (pDetour)
	{
		if (pDetour->IsEnabled())
		{
			pDetour->DisableDetour();
		}

		pDetour->Destroy();
		pDetour = NULL;
	}
}