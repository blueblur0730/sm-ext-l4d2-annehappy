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
#include <compat_wrappers.h>
#include "SI/boomer.h"
#include "SI/smoker.h"
#include <sourcehook.h>
#include "vtable_hook_helper.h"
#include <vector>
#include "dt_send.h"
#include "utils.h"
//#include "KeyValues.h"

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

CAnneHappy g_TemplateExtention;		/**< Global singleton for extension's main interface */
SMEXT_LINK(&g_TemplateExtention);


ICvar* icvar = NULL;
IServerGameEnts *gameents= NULL;
IGameEventManager2 *gameevents= NULL;
CGlobalVars *gpGlobals= NULL;
ISDKHooks *sdkhooks= NULL;
IServerGameClients *serverClients= NULL;
IBinTools *bintools= NULL;
IEngineTrace *enginetrace= NULL;
IStaticPropMgr *staticpropmgr = NULL;

TerrorNavMesh *g_pNavMesh = NULL;
ZombieManager *g_pZombieManager = NULL;

CBoomerEventListner g_BoomerEventListner;
CSmokerEventListner g_SmokerEventListner;

CTerrorEntityListner g_EntityListener;
CBoomerEntityListner *g_BoomerEntityListner = NULL;
CSmokerEntityListner *g_SmokerEntityListner = NULL;

SH_DECL_MANUALHOOK0_void(PostThink, 0, 0, 0);

int CBaseEntity::vtblindex_CBaseEntity_GetVelocity = 0;
int CBaseEntity::vtblindex_CBaseEntity_Teleport = 0;
int CBaseEntity::vtblindex_CBaseEntity_PostThink = 0;
int CBaseEntity::vtblindex_CBaseEntity_GetEyeAngle = 0;
int CTerrorPlayer::vtblindex_CTerrorPlayer_GetLastKnownArea = 0;

ICallWrapper *CTraceFilterSimpleExt::pCallCTraceFilterSimple = NULL;
ICallWrapper *CTraceFilterSimpleExt::pCallCTraceFilterSimple2 = NULL;
ICallWrapper *CBaseEntity::pCallGetVelocity = NULL;
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

void *CTraceFilterSimpleExt::pFnCTraceFilterSimple = NULL;
void *CTerrorPlayer::pFnOnVomitedUpon = NULL;
void *CTerrorPlayer::pFnGetSpecialInfectedDominatingMe = NULL;
void *CTerrorPlayer::pFnIsStaggering = NULL;
void *BossZombiePlayerBot::pFnChooseVictim = NULL;
void *ZombieManager::pFnGetRandomPZSpawnPosition = NULL;
Fn_IsVisibleToPlayer pFnIsVisibleToPlayer = NULL;

CDetour *CTerrorPlayer::DTR_OnVomitedUpon = NULL;
CDetour *BossZombiePlayerBot::DTR_ChooseVictim = NULL;

std::vector<CVTableHook *> g_hookList;

// so yeah the detours like to take the lead.
DETOUR_DECL_MEMBER2(DTRHandler_CTerrorPlayer_OnVomitedUpon, void, CBasePlayer *, pAttacker, bool, bIsExplodedByBoomer)
{
	((CTerrorPlayer *)this)->DTRCallBack_OnVomitedUpon(pAttacker, bIsExplodedByBoomer);
}

DETOUR_DECL_MEMBER3(DTRHandler_BossZombiePlayerBot_ChooseVictim, CTerrorPlayer *, CTerrorPlayer *, pLastVictim, int, targetScanFlags, CBasePlayer *, pIgnorePlayer)
{
	CTerrorPlayer *_this = (CTerrorPlayer *)this;
	CTerrorPlayer *pPlayer = NULL;

	// if not found, call original, make sure they have a target.
	switch (_this->GetClass())
	{
		case ZC_BOOMER:
		{
			pPlayer = ((BossZombiePlayerBot *)_this)->OnBoomerChooseVictim(pLastVictim, targetScanFlags, pIgnorePlayer);

			pPlayer == NULL ?
			DETOUR_MEMBER_CALL(DTRHandler_BossZombiePlayerBot_ChooseVictim)(pLastVictim, targetScanFlags, pIgnorePlayer) : 
			DETOUR_MEMBER_CALL(DTRHandler_BossZombiePlayerBot_ChooseVictim)(pPlayer, targetScanFlags, pIgnorePlayer);
			break;
		}

		case ZC_SMOKER:
		{
			pPlayer = ((BossZombiePlayerBot *)_this)->OnSmokerChooseVictim(pLastVictim, targetScanFlags, pIgnorePlayer);

			pPlayer == NULL ?
			DETOUR_MEMBER_CALL(DTRHandler_BossZombiePlayerBot_ChooseVictim)(pLastVictim, targetScanFlags, pIgnorePlayer) : 
			DETOUR_MEMBER_CALL(DTRHandler_BossZombiePlayerBot_ChooseVictim)(pPlayer, targetScanFlags, pIgnorePlayer);
			break;
		}
	}

	return DETOUR_MEMBER_CALL(DTRHandler_BossZombiePlayerBot_ChooseVictim)(pPlayer, targetScanFlags, pIgnorePlayer);
}

bool CAnneHappy::SDK_OnLoad(char* error, size_t maxlen, bool late) 
{
	sharesys->AddDependency(myself, "bintools.ext", true, true);
	sharesys->AddDependency(myself, "sdkhooks.ext", true, true);

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

	if (!gameconfs->LoadGameConfigFile("sdkhooks.games", &pGameData, error, maxlen))
	{
		ke::SafeStrcpy(error, maxlen, "Extension failed to load gamedata file: 'sdkhooks.games'");
		return false;
	}
	else
	{
		if (!LoadSDKHooksData(pGameData, error, maxlen))
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

	CTerrorPlayer::DTR_OnVomitedUpon = DETOUR_CREATE_MEMBER(DTRHandler_CTerrorPlayer_OnVomitedUpon, CTerrorPlayer::pFnOnVomitedUpon);
	if (!CTerrorPlayer::DTR_OnVomitedUpon)
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

	smutils->LogMessage(myself, "Annehappy extension has been loaded.");
	sharesys->RegisterLibrary(myself, "annehappy");
	return true;
}

void CAnneHappy::SDK_OnAllLoaded()
{
	SM_GET_LATE_IFACE(BINTOOLS, bintools);
	SM_GET_LATE_IFACE(SDKHOOKS, sdkhooks);

	if (!bintools || !sdkhooks)
	{
		smutils->LogError(myself, "Extension failed to load required libraries.");
		return;
	}

	PassInfo info[] = {
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(IHandleEntity *), NULL, 0},
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(int), NULL, 0},
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(ShouldHitFunc_t), NULL, 0},
	};

	CTraceFilterSimpleExt::pCallCTraceFilterSimple = bintools->CreateCall(CTraceFilterSimpleExt::pFnCTraceFilterSimple, CallConv_ThisCall, NULL, &info[0], 3);
	if (!CTraceFilterSimpleExt::pCallCTraceFilterSimple)
	{
		smutils->LogError(myself, "Extension failed to create call: 'CTraceFilterSimpleExt::CTraceFilterSimpleExt'");
		return;
	}

	PassInfo info1[] = {
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(IHandleEntity *), NULL, 0},
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(int), NULL, 0},
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(ShouldHitFunc2_t), NULL, 0},
	};

	CTraceFilterSimpleExt::pCallCTraceFilterSimple2 = bintools->CreateCall(CTraceFilterSimpleExt::pFnCTraceFilterSimple, CallConv_ThisCall, NULL, &info1[0], 3);
	if (!CTraceFilterSimpleExt::pCallCTraceFilterSimple2)
	{
		smutils->LogError(myself, "Extension failed to create call: 'CTraceFilterSimpleExt::CTraceFilterSimpleExt'");
		return;
	}

	PassInfo info2[] = {
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(CBasePlayer *), NULL, 0},	// this is the size of a pointer, not a class.
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(bool), NULL, 0},
	};

	CTerrorPlayer::pCallOnVomitedUpon = bintools->CreateCall(CTerrorPlayer::pFnOnVomitedUpon, CallConv_ThisCall, NULL, &info2[0], 0);
	if (!CTerrorPlayer::pCallOnVomitedUpon)
	{
		smutils->LogError(myself, "Extension failed to create call: 'CTerrorPlayer::OnVomitedUpon'");
		return;
	}

	PassInfo ret[] = {
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(bool), NULL, 0},
	};

	CTerrorPlayer::pCallGetSpecialInfectedDominatingMe = bintools->CreateCall(CTerrorPlayer::pFnGetSpecialInfectedDominatingMe, CallConv_ThisCall, &ret[0], NULL, 0);
	if (!CTerrorPlayer::pCallGetSpecialInfectedDominatingMe)
	{
		smutils->LogError(myself, "Extension failed to create call: 'CTerrorPlayer::GetSpecialInfectedDominatingMe'");
		return;
	}

	PassInfo info3[3];
	info[0].flags = info[1].flags = info[2].flags = PASSFLAG_BYVAL;
	info[0].size = info[1].size = info[2].size = sizeof(void *);
	info[0].type = info[1].type = info[2].type = PassType_Basic;

	CBaseEntity::pCallTeleport = bintools->CreateVCall(CBaseEntity::vtblindex_CBaseEntity_Teleport, 0, 0, NULL, info3, 3);
	if (!CBaseEntity::pCallTeleport)
	{
		smutils->LogError(myself, "Extension failed to create vcall: 'CBaseEntity::Teleport'");
		return;
	}

	PassInfo info4[1];
	info1[0].flags = PASSFLAG_BYVAL;
	info1[0].size = sizeof(void *);
	info1[0].type = PassType_Basic;
	
	CBaseEntity::pCallGetEyeAngle = bintools->CreateVCall(CBaseEntity::vtblindex_CBaseEntity_GetEyeAngle, 0, 0, info4, NULL, 0);
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

	ZombieManager::pCallGetRandomPZSpawnPosition = bintools->CreateCall(ZombieManager::pFnGetRandomPZSpawnPosition, CallConv_ThisCall, NULL, &info5[0], 0);
	if (!ZombieManager::pCallGetRandomPZSpawnPosition)
	{
		smutils->LogError(myself, "Extension failed to create call: 'ZombieManager::GetRandomPZSpawnPosition'");
		return;
	}

	PassInfo ret2[] = {
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(void *), NULL, 0},
	};
	CTerrorPlayer::pCallGetLastKnownArea = bintools->CreateVCall(CTerrorPlayer::vtblindex_CTerrorPlayer_GetLastKnownArea, 0, 0, NULL, &ret2[0], 0);
	if (!CTerrorPlayer::pCallGetLastKnownArea)
	{
		smutils->LogError(myself, "Extension failed to create vcall: 'CTerrorPlayer::GetLastKnownArea'");
		return;
	}

	PassInfo info6[] = {
		{PassType_Basic, PASSFLAG_BYREF, sizeof(Vector *), NULL, 0},
		{PassType_Basic, PASSFLAG_BYREF, sizeof(AngularImpulse *), NULL, 0},
	};

	CBaseEntity::pCallGetVelocity = bintools->CreateVCall(CBaseEntity::vtblindex_CBaseEntity_GetVelocity, 0, 0, &info6[0], NULL, 0);
	if (!CBaseEntity::pCallGetVelocity)
	{
		smutils->LogError(myself, "Extension failed to create vcall: 'CBaseEntity::GetVelocity'");
		return;
	}

	SH_MANUALHOOK_RECONFIGURE(PostThink, CBaseEntity::vtblindex_CBaseEntity_PostThink, 0, 0);
	sdkhooks->AddEntityListener(&g_EntityListener);

	CTerrorPlayer::DTR_OnVomitedUpon->EnableDetour();
	BossZombiePlayerBot::DTR_ChooseVictim->EnableDetour();
}

void CAnneHappy::SDK_OnUnload()
{
	for (auto it = g_hookList.begin(); it != g_hookList.end(); ++it)
	{
		delete *it;
		g_hookList.erase(it);
	}

	RemoveEventListner();
	sdkhooks->RemoveEntityListener(&g_EntityListener);

	if (g_BoomerEntityListner)
		delete g_BoomerEntityListner;

	if (g_SmokerEntityListner)
		delete g_SmokerEntityListner;

	DestroyCalls(CTraceFilterSimpleExt::pCallCTraceFilterSimple);
	DestroyCalls(CTraceFilterSimpleExt::pCallCTraceFilterSimple2);
	DestroyCalls(CBaseEntity::pCallGetVelocity);
	DestroyCalls(CBaseEntity::pCallTeleport);
	DestroyCalls(CBaseEntity::pCallGetEyeAngle);
	DestroyCalls(CTerrorPlayer::pCallGetSpecialInfectedDominatingMe);
	DestroyCalls(CTerrorPlayer::pCallIsStaggering);
	DestroyCalls(ZombieManager::pCallGetRandomPZSpawnPosition);
	DestroyDetours(CTerrorPlayer::DTR_OnVomitedUpon);
	DestroyDetours(BossZombiePlayerBot::DTR_ChooseVictim);

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
		{"GetVelocity", CBaseEntity::vtblindex_CBaseEntity_GetVelocity, "sdktools.games"},
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

	return true;
}

bool CAnneHappy::LoadSDKHooksData(IGameConfig *pGameData, char* error, size_t maxlen)
{
	static const struct {
		char const* name;
		int& pOffset;
		char const *filename;
	} s_offsets[] = {
		{"PostThink", CBaseEntity::vtblindex_CBaseEntity_PostThink, "sdkhooks.games"},
	};

	for (auto &offset : s_offsets)
	{
		if (!pGameData->GetOffset(offset.name, &offset.pOffset))
		{
			ke::SafeSprintf(error, maxlen, "Could not find offset for \"%s\" in \"%s\"", offset.name, offset.filename);
			return false;
		}
	}

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
		{"CTraceFilterSimple::CTraceFilterSimple", &CTraceFilterSimpleExt::pFnCTraceFilterSimple},
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
			smutils->LogError(myself, "Extension failed to add event listner: '%s' for ai_boomer.", sBoomerEventName[i]);
			return false;
		}
	}

	if (!gameevents->AddListener(&g_SmokerEventListner, "round_start", true))
	{
		smutils->LogError(myself, "Extension failed to add event listner: '%s' for ai_smoker.", "round_start");
		return false;
	}

	return true;
}

void CAnneHappy::RemoveEventListner()
{
	gameevents->RemoveListener(&g_BoomerEventListner);
	gameevents->RemoveListener(&g_SmokerEventListner);
}

bool CAnneHappy::FindSendProps(IGameConfig *pGameData, char* error, size_t maxlen)
{
	static const struct {
		char const *propName;
		char const *className;
		int& pOffset;
	} s_props[] = {
		{"m_isSpraying", "CVomit", CVomit::m_iOff_m_isSpraying},
		{"m_nBlockType", "CEnvPhysicsBlocker", CEnvPhysicsBlocker::m_iOff_m_nBlockType},
		{"m_bInReload", "CBaseCombatWeapon", CBaseCombatWeapon::m_iOff_m_bInReload},
		{"m_zombieClass", "CTerrorPlayer", CTerrorPlayer::m_iOff_m_zombieClass},
		{"m_customAbility", "CTerrorPlayer", CTerrorPlayer::m_iOff_m_customAbility},
		{"m_hasVisibleThreats", "CTerrorPlayer", CTerrorPlayer::m_iOff_m_hasVisibleThreats},
		{"m_isIncapacitated", "CTerrorPlayer", CTerrorPlayer::m_iOff_m_isIncapacitated},
		{"m_tongueVictim", "CTerrorPlayer", CTerrorPlayer::m_iOff_m_tongueVictim},
		{"m_hGroundEntity", "CTerrorPlayer", CTerrorPlayer::m_iOff_m_hGroundEntity},
		{"m_hActiveWeapon", "CTerrorPlayer", CTerrorPlayer::m_iOff_m_hActiveWeapon}
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

/*
	for (auto &prop : s_props)
	{
		SendProp *sendprop = pGameData->GetSendProp(prop.propName);
		if (!sendprop)
		{
			ke::SafeSprintf(error, maxlen, "Extension failed to find send props: '%s'", prop.propName);
			return false;
		}

		prop.pOffset = sendprop->GetOffset();
	}
*/
	return true;
}

void CTerrorEntityListner::OnEntityCreated(CBaseEntity* pEntity, const char *classname)
{
	if (!pEntity)
		return;

	CTerrorPlayer *pPlayer = (CTerrorPlayer *)pEntity;
	if (pPlayer->IsBot())
		return;

	if (!pPlayer->IsInfected())
		return;

	CVTableHook *vhook = new CVTableHook(pEntity);
	int hookid = SH_ADD_MANUALVPHOOK(PostThink, pEntity, SH_MEMBER(&g_EntityListener, &CTerrorEntityListner::OnPostThink), false);
	vhook->SetHookID(hookid);
	g_hookList.push_back(vhook);
}

void CTerrorEntityListner::OnEntityDestroyed(CBaseEntity* pEntity)
{
	if (!pEntity)
		return;

	CTerrorPlayer *pPlayer = (CTerrorPlayer *)pEntity;
	if (!pPlayer->IsInGame() || pPlayer->IsBot())
		return;

	if (!pPlayer->IsInfected())
		return;

	for (auto it = g_hookList.begin(); it != g_hookList.end(); ++it)
	{
		if ((CBaseEntity *)(*it)->GetVTablePtr() == pEntity)
		{
			delete *it;
			g_hookList.erase(it);
			break;
		}
	}
}

void CTerrorEntityListner::OnPostThink()
{
	CBaseEntity *pEntity = META_IFACEPTR(CBaseEntity);

	if (!pEntity)
		return;

	CTerrorPlayer *pPlayer = (CTerrorPlayer *)pEntity;
	if (!pPlayer->IsInGame() || pPlayer->IsBot())
		return;

	if (!pPlayer->IsInfected())
		return;

	switch (pPlayer->GetClass())
	{
		case ZC_BOOMER:
		{
			if (!g_BoomerEntityListner)
			{
				g_BoomerEntityListner = new CBoomerEntityListner();
			}

			g_BoomerEntityListner->OnPostThink(pEntity);
			break;
		}

		case ZC_SMOKER:
		{
			if (!g_SmokerEntityListner)
			{
				g_SmokerEntityListner = new CSmokerEntityListner();
			}

			g_SmokerEntityListner->OnPostThink(pEntity);
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