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
#include <sourcehook.h>
#include "vtable_hook_helper.h"
#include <vector>
#include "wrappers.h"

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

CAnneHappy g_TemplateExtention;		/**< Global singleton for extension's main interface */
SMEXT_LINK(&g_TemplateExtention);

CTerrorEntityListner g_EntityListener;
SH_DECL_MANUALHOOK0_void(PostThink, 0, 0, 0);

int CBaseEntity::vtblindex_CBaseEntity_Teleport = 0;
int CBaseEntity::vtblindex_CBaseEntity_PostThink = 0;
int CBaseEntity::vtblindex_CBaseEntity_GetEyeAngle = 0;

ICallWrapper* CTraceFilterSimple::pCallCTraceFilterSimple = NULL;
ICallWrapper* CBaseEntity::pCallTeleport = NULL;
ICallWrapper* CBaseEntity::pCallGetEyeAngle = NULL;
ICallWrapper *CTerrorPlayer::pCallOnVomitedUpon = NULL;
ICallWrapper *CTerrorPlayer::pCallGetSpecialInfectedDominatingMe = NULL;

int CBaseEntity::m_iOff_m_vecVelocity = 0;
int CBaseAbility::m_iOff_m_isSpraying = 0;
int CBasePlayer::m_iOff_m_fFlags = 0;
int CTerrorPlayer::m_iOff_m_zombieClass = 0;
int CTerrorPlayer::m_iOff_m_customAbility = 0;
int CTerrorPlayer::m_iOff_m_hasVisibleThreats = 0;
int CTerrorPlayer::m_iOff_m_isIncapacitated = 0;

void* CTraceFilterSimple::pFnCTraceFilterSimple = NULL;
void *CTerrorPlayer::pFnOnVomitedUpon = NULL;
void *CTerrorPlayer::pFnGetSpecialInfectedDominatingMe = NULL;
void *BossZombiePlayerBot::pFnChooseVictim = NULL;
void *pFnIsVisibleToPlayer = NULL;

CDetour *CTerrorPlayer::DTR_OnVomitedUpon = NULL;
CDetour *DTR_ChooseVictim = NULL;

std::vector<CVTableHook *> g_hookList;

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

	if (!gameconfs->LoadGameConfigFile("sdkhooks.games", &pGameData, error, maxlen))
	{
		ke::SafeStrcpy(error, maxlen, "Extension failed to load gamedata file: 'sdkhooks.games'");
		return false;
	}

	if (!gameconfs->LoadGameConfigFile(GAMEDATA_FILE, &pGameData, error, maxlen))
	{
		ke::SafeStrcpy(error, maxlen, "Extension failed to load gamedata file: 'l4d2_annehappy.games'");
		return false;
	}

	if (!LoadGameData(pGameData, error, maxlen))
		return false;

	gameconfs->CloseGameConfigFile(pGameData);

	g_BoomerEntityListner = new CBoomerEntityListner();
	sdkhooks->AddEntityListener(&g_EntityListener);

	//smutils->LogMessage(myself, "Sample extension has been loaded.");
	sharesys->RegisterLibrary(myself, "annehappy");

	if (!AddEventListner())
		return false;

	char msg[64];
	if (!FindSendProps(msg, sizeof(msg)))
	{
		smutils->LogError(myself, "Extension failed to find send props: '%s'", msg);
		return false;
	}

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

	CTraceFilterSimple::pCallCTraceFilterSimple = bintools->CreateCall(CTraceFilterSimple::pFnCTraceFilterSimple, CallConv_ThisCall, NULL, &info[0], 3);
	if (!CTraceFilterSimple::pCallCTraceFilterSimple)
	{
		smutils->LogError(myself, "Extension failed to create call: 'CTraceFilterSimple::pFnCTraceFilterSimple'");
		return;
	}

	PassInfo info1[] = {
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(CBasePlayer *), NULL, 0},
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(bool), NULL, 0},
	};

	CTerrorPlayer::pCallOnVomitedUpon = bintools->CreateCall(CTerrorPlayer::pFnOnVomitedUpon, CallConv_ThisCall, NULL, &info1[0], 0);
	if (!CTerrorPlayer::pCallOnVomitedUpon)
	{
		smutils->LogError(myself, "Extension failed to create call: 'CTerrorPlayer::pFnOnVomitedUpon'");
		return;
	}

	PassInfo ret[] = {
		{PassType_Basic, PASSFLAG_BYVAL, sizeof(bool), NULL, 0},
	};

	CTerrorPlayer::pCallGetSpecialInfectedDominatingMe = bintools->CreateCall(CTerrorPlayer::pFnGetSpecialInfectedDominatingMe, CallConv_ThisCall, &ret[0], NULL, 0);
	if (!CTerrorPlayer::pCallGetSpecialInfectedDominatingMe)
	{
		smutils->LogError(myself, "Extension failed to create call: 'CTerrorPlayer::pFnGetSpecialInfectedDominatingMe'");
		return;
	}

	PassInfo info[3];
	info[0].flags = info[1].flags = info[2].flags = PASSFLAG_BYVAL;
	info[0].size = info[1].size = info[2].size = sizeof(void *);
	info[0].type = info[1].type = info[2].type = PassType_Basic;

	CBaseEntity::pCallTeleport = bintools->CreateVCall(CBaseEntity::vtblindex_CBaseEntity_Teleport, 0, 0, NULL, info, 3);
	if (!CBaseEntity::pCallTeleport)
	{
		smutils->LogError(myself, "Extension failed to create vcall: 'CBaseEntity::Teleport'");
		return;
	}

	PassInfo info1[1];
	info1[0].flags = PASSFLAG_BYVAL;
	info1[0].size = sizeof(void *);
	info1[0].type = PassType_Basic;
	
	CBaseEntity::pCallGetEyeAngle = bintools->CreateVCall(CBaseEntity::vtblindex_CBaseEntity_GetEyeAngle, 0, 0, info1, NULL, 0);
	if (!CBaseEntity::pCallGetEyeAngle)
	{
		smutils->LogError(myself, "Extension failed to create vcall: 'CBaseEntity::GetEyeAngle'");
		return;
	}

	SH_MANUALHOOK_RECONFIGURE(PostThink, CBaseEntity::vtblindex_CBaseEntity_PostThink, 0, 0);
}

void CAnneHappy::SDK_OnUnload()
{
	for (auto it = g_hookList.begin(); it != g_hookList.end(); ++it)
	{
		delete *it;
		g_hookList.erase(it);
	}

	delete g_BoomerEntityListner;
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

bool CAnneHappy::LoadGameData(IGameConfig *pGameData, char* error, size_t maxlen)
{
	static const struct {
		char const* name;
		int& pOffset;
		char const *filename;
	} s_offsets[] = {
		{"Teleport", CBaseEntity::vtblindex_CBaseEntity_Teleport, "sdktools.games"},
		{"EyeAngles", CBaseEntity::vtblindex_CBaseEntity_GetEyeAngle, "sdktools.games"},
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
	
	static const struct {
		char const* name;
		void** pFn;
	} s_sigs[] = {
		{"CTraceFilterSimple::CTraceFilterSimple", &CTraceFilterSimple::pFnCTraceFilterSimple},
		{"CTerrorPlayer::OnVomitedUpon", &CTerrorPlayer::pFnOnVomitedUpon},
		{"CTerrorPlayer::GetSpecialInfectedDominatingMe", &CTerrorPlayer::pFnGetSpecialInfectedDominatingMe},
		{"IsVisibleToPlayer", &pFnIsVisibleToPlayer},
		{"BossZombiePlayerBot::ChooseVictim", &BossZombiePlayerBot::pFnChooseVictim}
	};

	for (auto &sig : s_sigs)
	{
		if (!pGameData->GetMemSig(sig.name, sig.pFn))
		{
			ke::SafeSprintf(error, maxlen, "Could not find signature for \"%s\" in \"" GAMEDATA_FILE ".txt\".", sig.name);
			return false;
		}
	}

	return true;
}

bool CAnneHappy::AddEventListner()
{
	const char *eventName[] = {
		"player_spawn",
		"player_shoved",
		"player_now_it",
		"round_start"
	};

	for (int i = 0; i < sizeof(eventName); i++)
	{
		if (!gameevents->AddListener(&g_BoomerEventListner, eventName[i], true))
		{
			smutils->LogError(myself, "Extension failed to add event listner: '%s'", eventName[i]);
			return false;
		}
	}
}

bool CAnneHappy::FindSendProps(char* propName, size_t maxlen)
{
	static const struct {
		char const *className;
		char const *propName;
		int& pOffset;
	} s_props[] = {
		{"CBaseEntity", "m_vecVelocity", CBaseEntity::m_iOff_m_vecVelocity},
		{"CTerrorPlayer", "m_zombieClass", CTerrorPlayer::m_iOff_m_zombieClass},
		{"CTerrorPlayer", "m_customAbility", CTerrorPlayer::m_iOff_m_customAbility},
		{"CTerrorPlayer", "m_hasVisibleThreats", CTerrorPlayer::m_iOff_m_hasVisibleThreats},
		{"CBaseAbility", "m_isSpraying", CBaseAbility::m_iOff_m_isSpraying},
		{"CTerrorPlayer", "m_isIncapacitated", CTerrorPlayer::m_iOff_m_isIncapacitated},
	};

	sm_sendprop_info_t info;
	for (auto &prop : s_props)
	{
		if (!gamehelpers->FindSendPropInfo(prop.className, prop.propName, &info))
		{
			ke::SafeStrcpy(propName, maxlen, prop.propName);
			return false;
		}

		prop.pOffset = info.actual_offset;
	}
}

void CTerrorEntityListner::OnEntityCreated(CBaseEntity* pEntity, const char *classname)
{
	if (!IsPlayer(pEntity))
		return;

	if (!IsInfected(pEntity))
		return;

	CVTableHook *vhook = new CVTableHook(pEntity);
	int hookid = SH_ADD_MANUALVPHOOK(PostThink, pEntity, SH_MEMBER(&g_EntityListener, &CTerrorEntityListner::OnPostThink), false);
	vhook->SetHookID(hookid);
	g_hookList.push_back(vhook);
}

void CTerrorEntityListner::OnEntityDestroyed(CBaseEntity* pEntity)
{
	if (!IsPlayer(pEntity))
		return;

	if (!IsInfected(pEntity))
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

	if (!IsPlayer(pEntity))
		return;

	if (!IsInfected(pEntity))
		return;

	CTerrorPlayer *pPlayer = (CTerrorPlayer *)pEntity;
	if (!pPlayer->IsInGame())
		return;

	switch (pPlayer->GetClass())
	{
		case CTerrorPlayer::ZC_BOOMER:
		{
			g_BoomerEntityListner->OnPostThink(pEntity);
			break;
		}
	}

	RETURN_META(MRES_IGNORED);
}

DETOUR_DECL_MEMBER2(DTRHandler_CTerrorPlayer_OnVomitedUpon, void, CBasePlayer *, pAttacker, bool, bIsExplodedByBoomer)
{
	((CTerrorPlayer *)this)->DTRCallBack_OnVomitedUpon(pAttacker, bIsExplodedByBoomer);
}

DETOUR_DECL_MEMBER3(DTRHandler_BossZombiePlayerBot_ChooseVictim, CTerrorPlayer *, CTerrorPlayer *, pLasVictim, int, targetScanFlags, CBasePlayer *, pIgnorePlayer)
{
	CTerrorPlayer *_this = (CTerrorPlayer *)this;
	switch (_this->GetClass())
	{
		case CTerrorPlayer::ZC_BOOMER:
		{
			// if not found, call original, make sure they have a target.
			CTerrorPlayer *pPlayer = ((BossZombiePlayerBot *)_this)->OnBoomerChooseVictim(pLasVictim, targetScanFlags, pIgnorePlayer);
			pPlayer == NULL ? DETOUR_MEMBER_CALL(DTRHandler_BossZombiePlayerBot_ChooseVictim)(pLasVictim, targetScanFlags, pIgnorePlayer) : pPlayer;

			break;
		}
	}
}