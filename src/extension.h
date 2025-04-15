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

#ifndef _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_
#define _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_

#pragma once

/**
 * @file extension.h
 * @brief Sample extension code header.
 */

#include "smsdk_ext.h"
#include "igameevents.h"
#include "IBinTools.h"
#include "ISDKHooks.h"
#include "wrappers.h"
#include "eiface.h"
#include "engine/IEngineTrace.h"
#include "engine/IStaticPropMgr.h"
#include "CDetour/detours.h"
#include "vtable_hook_helper.h"
#include "usercmd.h"

#define GAMEDATA_FILE "l4d2_annehappy.gamedata"

class TerrorNavMesh;
class ZombieManager;

/**
 * @brief Sample implementation of the SDK Extension.
 * Note: Uncomment one of the pre-defined virtual functions in order to use it.
 */
class CAnneHappy : public SDKExtension, public IConCommandBaseAccessor, public IClientListener, public IGameEventListener2
{
public:
	virtual bool SDK_OnLoad(char *error, size_t maxlen, bool late);
	
	virtual void SDK_OnUnload();

	virtual void SDK_OnAllLoaded();

	virtual bool SDK_OnMetamodLoad(ISmmAPI* ismm, char* error, size_t maxlen, bool late);

	virtual bool RegisterConCommandBase(ConCommandBase* command);

	virtual void OnClientPutInServer(int client);

	virtual void FireGameEvent(IGameEvent *event);

	virtual int GetEventDebugID(void);

protected:
	void PlayerRunCmdHook(int client);

	void PlayerRunCmd(CUserCmd *ucmd, IMoveHelper *moveHelper);

	bool LoadSDKToolsData(IGameConfig *pGameData, char* error, size_t maxlen);

	bool LoadGameData(IGameConfig *pGameData, char* error, size_t maxlen);

	bool FindSendProps(IGameConfig *pGameData, char* error, size_t maxlen);

	bool AddEventListner();

	void RemoveEventListner();

	void DestroyCalls(ICallWrapper *pCall);

	void DestroyDetours(CDetour *pDetour);

private:
	std::vector<CVTableHook *> g_hookList;
};

// convar interface.
extern ICvar* icvar;

// game entities interface
extern IServerGameEnts *gameents;

// game event manager interface
extern IGameEventManager2 *gameevents;

// global variables interface
extern CGlobalVars *gpGlobals;

// sdk hooks interface.
extern ISDKHooks *sdkhooks;

// server game clients interface.
extern IServerGameClients *serverClients;

// bin tools interface.
extern IBinTools *bintools;

// engine trace interface.
extern IEngineTrace *enginetrace;

// static prop entity manager interface.
extern IStaticPropMgr *staticpropmgr;

extern TerrorNavMesh *g_pNavMesh;
extern ZombieManager *g_pZombieManager;

#endif // _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_