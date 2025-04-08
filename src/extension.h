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

/**
 * @file extension.h
 * @brief Sample extension code header.
 */

#include "smsdk_ext.h"
#include "igameevents.h"
#include "ISDKHooks.h"
#include "wrappers.h"
#include "eiface.h"

#define GAMEDATA_FILE "l4d2_annehappy.gamedata"

/**
 * @brief Sample implementation of the SDK Extension.
 * Note: Uncomment one of the pre-defined virtual functions in order to use it.
 */
class CAnneHappy : public SDKExtension, public IConCommandBaseAccessor
{
public:
	/**
	 * @brief This is called after the initial loading sequence has been processed.
	 *
	 * @param error		Error message buffer.
	 * @param maxlen	Size of error message buffer.
	 * @param late		Whether or not the module was loaded after map load.
	 * @return			True to succeed loading, false to fail.
	 */
	virtual bool SDK_OnLoad(char *error, size_t maxlen, bool late) override;
	
	/**
	 * @brief This is called right before the extension is unloaded.
	 */
	virtual void SDK_OnUnload() override;

	virtual void SDK_OnAllLoaded() override;

	virtual bool SDK_OnMetamodLoad(ISmmAPI* ismm, char* error, size_t maxlen, bool late) override;

	virtual bool RegisterConCommandBase(ConCommandBase* command) override;

protected:
	bool LoadGameData(IGameConfig *pGameData, char* error, size_t maxlen);
	bool FindSendProps(char* error, size_t maxlen);
	bool AddEventListner();
};

#endif // _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_

// convar interface.
extern ICvar* icvar = NULL;

// game entities interface
extern IServerGameEnts *gameents = NULL;

// game event manager interface
extern IGameEventManager2 *gameevents = NULL;

// global variables interface
extern CGlobalVars *gpGlobals = NULL;

// sdk hooks interface.
extern ISDKHooks *sdkhooks = NULL;

// server game clients interface.
extern IServerGameClients *serverClients = NULL;

// bin tools interface.
extern IBinTools *bintools = NULL;

// engine trace interface.
extern IEngineTrace *enginetrace = NULL;

// static prop entity manager interface.
extern IStaticPropMgr *staticpropmgr = NULL;

// ai_boomer
extern ConVar z_boomer_bhop;
extern ConVar z_boomer_bhop_speed;
extern ConVar z_boomer_vision_up_on_vomit;
extern ConVar z_boomer_vision_spin_on_vomit;
extern ConVar z_boomer_force_bile;
extern ConVar z_boomer_bile_find_range;
extern ConVar z_boomer_spin_interval;
extern ConVar z_boomer_degree_force_bile;
extern ConVar z_boomer_predict_pos;

// ai_smoker_new
extern ConVar z_smoker_bhop;
extern ConVar z_smoker_bhop_speed;
extern ConVar z_smoker_target_rules;
extern ConVar z_smoker_melee_avoid;
extern ConVar z_smoker_left_behind_distance;
extern ConVar z_smoker_left_behind_time;
extern ConVar z_smoker_intant_shoot_cofficient;