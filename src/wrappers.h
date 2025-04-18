#ifndef _WRAPPERS_H_INCLUDED_
#define _WRAPPERS_H_INCLUDED_

#pragma once

#include <vector>

#include "extension.h"
#include "IBinTools.h"
#include "ISDKHooks.h"
#include "CDetour/detours.h"

#include "edict.h"
#include "eiface.h"
#include "igameevents.h"
#include "iplayerinfo.h"
#include "engine/IEngineTrace.h"
#include "engine/IStaticPropMgr.h"
#include "ihandleentity.h"
#include "mathlib.h"
#include "usercmd.h"

#define NAV_MESH_HEIGHT 20.0
#define PLAYER_HEIGHT 72.0

extern IServerGameEnts *gameents;

// https://developer.valvesoftware.com/wiki/Env_physics_blocker
enum BlockType_t {
	BlockType_Everyone = 0,
	BlockType_Survivors = 1,
	BlockType_PlayerInfected = 2,
	BlockType_AllSIInfected = 3,	// both ai and players.
	BlockType_AllPlayerAndPhysicsObjects =  4
};

enum L4D2Teams {
	L4D2Teams_Unasigned = 0,
	L4D2Teams_Spectator = 1,
	L4D2Teams_Survivor = 2,
	L4D2Teams_Infected = 3,
	L4D2Teams_L4D1Survivors = 4
};

enum ZombieClassType {
	ZC_NONE = 0,

	ZC_SMOKER = 1,
	ZC_BOOMER = 2,
	ZC_HUNTER = 3,
	ZC_SPITTER = 4,
	ZC_JOCKEY = 5,
	ZC_CHARGER = 6,
	ZC_TANK = 8,
};

typedef bool (*ShouldHitFunc_t)(IHandleEntity* pHandleEntity, int contentsMask);
typedef bool (*ShouldHitFunc2_t)(IHandleEntity* pHandleEntity, int contentsMask, void* data);

class CTraceFilterSimpleExt : public CTraceFilter
{
public:
	static void* pFnCTraceFilterSimple;
	static ICallWrapper* pCallCTraceFilterSimple;
	static ICallWrapper* pCallCTraceFilterSimple2;

public:
	CTraceFilterSimpleExt(const IHandleEntity *passedict = NULL, Collision_Group_t collisionGroup = COLLISION_GROUP_NONE, ShouldHitFunc_t pExtraShouldHitFunc = NULL);
	CTraceFilterSimpleExt(const IHandleEntity *passedict = NULL, Collision_Group_t collisionGroup = COLLISION_GROUP_NONE, ShouldHitFunc2_t pExtraShouldHitFunc = NULL, void *data = NULL);

	virtual bool ShouldHitEntity(IHandleEntity *pHandleEntity, int contentsMask);

private:
	TraceType_t m_TraceType;
	const IHandleEntity* m_pPassEnt;
	int m_collisionGroup;
	ShouldHitFunc_t m_pExtraShouldHitCheckFunction;
	ShouldHitFunc2_t m_pExtraShouldHitCheckFunction2;
	void *m_data;
};

class CNavArea {
public:
	static int m_iOff_m_flow;
	
public:
	inline float GetFlow()
	{
		return *(float *)((byte *)(this) + m_iOff_m_flow);
	}
};

/*
class CBaseTraceFilter : public ITraceFilter {
public:

	virtual bool ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask);
	void SetTraceType(TraceType_t traceType) {
		m_TraceType = traceType;
	}

private:
	TraceType_t m_TraceType;
};
*/

class CBaseEntity : public IServerEntity {
public:
	//static int vtblindex_CBaseEntity_GetVelocity;
	//static ICallWrapper *pCallGetVelocity;

	static int vtblindex_CBaseEntity_Teleport;
	static ICallWrapper *pCallTeleport;

	static int vtblindex_CBaseEntity_GetEyeAngle;
	static ICallWrapper *pCallGetEyeAngle;

public:
	inline int GetDataOffset(const char *name)
	{
		sm_datatable_info_t offset_data_info;
		datamap_t *offsetMap = gamehelpers->GetDataMap((CBaseEntity *)this);
		if (!gamehelpers->FindDataMapInfo(offsetMap, "name", &offset_data_info))
			return -1;

		return offset_data_info.actual_offset;
	}

	inline edict_t* edict()
	{
		return gameents->BaseEntityToEdict(this);
	}

	inline int entindex()
	{
		return gamehelpers->EntityToBCompatRef(this);
	}

	inline const char *GetClassName()
	{
		return edict()->GetClassName();
	}

	//void GetVelocity(Vector *velocity, AngularImpulse *vAngVelocity);

	inline Vector GetVelocity()
	{
		return *(Vector*)((byte*)(this) + GetDataOffset("m_vecAbsVelocity"));
	}

	inline CBaseEntity *GetOwnerEntity()
	{
		return gamehelpers->ReferenceToEntity(((CBaseHandle *)((byte *)(this) + GetDataOffset("m_hOwnerEntity")))->GetEntryIndex());
	}

	inline MoveType_t GetMoveType()
	{
		return *(MoveType_t*)((byte*)(this) + GetDataOffset("m_MoveType"));
	}

	void Teleport(Vector *newPosition, QAngle *newAngles, Vector *newVelocity);

	void GetEyeAngles(QAngle *pRetAngle);
};

class CEnvPhysicsBlocker : public CBaseEntity {
public:
	static int m_iOff_m_nBlockType;

public:
	inline BlockType_t GetBlockType()
	{
		return *(BlockType_t*)((byte*)(this) + CEnvPhysicsBlocker::m_iOff_m_nBlockType);
	}
};

class CBaseAbility : public CBaseEntity {
};

class CBaseCombatWeapon : public CBaseEntity {
public:
	static int m_iOff_m_bInReload;

public:
	inline bool IsReloading()
	{
		return *(bool*)((byte*)(this) + CBaseCombatWeapon::m_iOff_m_bInReload);
	}
};

class CBasePlayer : public CBaseEntity {
public:
	static int m_iOff_m_fFlags;

public:
	inline int GetFlags()
	{
		return *(int*)((byte*)(this) + CBasePlayer::m_iOff_m_fFlags);
	}

	inline bool IsBot()
	{
		return (GetFlags() & FL_FAKECLIENT) != 0;
	}

	inline int GetButton()
	{
		return *(int*)((byte*)(this) + GetDataOffset("m_nButtons"));
	}

	// Thanks to Forgetest from his l4d_lagcomp_skeet.
	inline CUserCmd *GetCurrentCommand() 
	{
		return *(CUserCmd **)((byte *)(this) + 
				(GetDataOffset("m_hViewModel") 
				+ 4 * 2 /* CHandle<CBaseViewModel> * MAX_VIEWMODELS */
				+ 88 /* sizeof(m_LastCmd) */));
	}
};

class CTerrorPlayer : public CBasePlayer {
public:
    static int m_iOff_m_zombieClass;
	static int m_iOff_m_customAbility;
	static int m_iOff_m_hasVisibleThreats;
	static int m_iOff_m_isIncapacitated;
	static int m_iOff_m_tongueVictim;
	static int m_iOff_m_hGroundEntity;
	static int m_iOff_m_hActiveWeapon;

	static void *pFnOnVomitedUpon;
	static ICallWrapper *pCallOnVomitedUpon;
	static CDetour* DTR_OnVomitedUpon;

	static void *pFnGetSpecialInfectedDominatingMe;
	static ICallWrapper *pCallGetSpecialInfectedDominatingMe;

	static void *pFnIsStaggering;
	static ICallWrapper *pCallIsStaggering;

	static int vtblindex_CTerrorPlayer_GetLastKnownArea;
	static ICallWrapper *pCallGetLastKnownArea;

public:
	IPlayerInfo *GetPlayerInfo();

	inline IGamePlayer *GetGamePlayer()
	{
		return playerhelpers->GetGamePlayer(entindex());
	}

	Vector GetEyeOrigin();

	inline Vector GetAbsOrigin()
	{
		IPlayerInfo *pPlayerInfo = GetPlayerInfo();
		if (!pPlayerInfo)
			return Vector(0, 0, 0);
	
		return GetPlayerInfo()->GetAbsOrigin();
	}

	// this is fastest and the most simple.
	// if not, use m_lifeState.
	inline bool IsDead()
	{
		IPlayerInfo *pPlayerInfo = GetPlayerInfo();
		if (!pPlayerInfo)
			return false;
	
		return GetPlayerInfo()->IsDead();
	}

	inline Vector GetPlayerMins()
	{
		IPlayerInfo *pPlayerInfo = GetPlayerInfo();
		if (!pPlayerInfo)
			return Vector(0, 0, 0);
	
		return GetPlayerInfo()->GetPlayerMins();
	}

	inline Vector GetPlayerMaxs()
	{
		IPlayerInfo *pPlayerInfo = GetPlayerInfo();
		if (!pPlayerInfo)
			return Vector(0, 0, 0);
	
		return GetPlayerInfo()->GetPlayerMaxs();
	}

	inline bool IsFakeClient()
	{   
		IGamePlayer *pPlayerInfo = GetGamePlayer();
		if (!pPlayerInfo)
			return false;
	
		return GetGamePlayer()->IsFakeClient();
	}

	inline bool IsInGame()
	{
		IGamePlayer *pPlayerInfo = GetGamePlayer();
		if (!pPlayerInfo)
			return false;
	
		return GetGamePlayer()->IsInGame();
	}

	inline bool IsIncapped()
	{
		return *(bool*)((byte*)(this) + m_iOff_m_isIncapacitated);
	}

	L4D2Teams GetTeam();

	inline bool IsSurvivor()
	{
		return (GetTeam() == L4D2Teams_Survivor);
	}

	inline bool IsInfected()
	{
		return (GetTeam() == L4D2Teams_Infected);
	}

	inline bool IsSpectator()
	{
		return (GetTeam() == L4D2Teams_Spectator);
	}

	inline bool HasVisibleThreats()
	{
		return *(bool*)((byte*)(this) + m_iOff_m_hasVisibleThreats);
	}

	CBaseAbility *GetAbility();

	CBaseEntity *GetGroundEntity();

	CBaseCombatWeapon *GetActiveWeapon();
	
	CTerrorPlayer *GetTongueVictim();

	inline ZombieClassType GetClass()
	{
		return *(ZombieClassType *)((uint8_t *)this + m_iOff_m_zombieClass);
	}

	inline bool IsBoomer()
	{
		return (GetClass() == ZC_BOOMER);
	}

	inline bool IsSmoker()
	{
		return (GetClass() == ZC_SMOKER);
	}
	

	CBaseEntity *OffsetEHandleToEntity(int iOff);

	void OnVomitedUpon(CBasePlayer *pAttacker, bool bIsExplodedByBoomer);

	CTerrorPlayer *GetSpecialInfectedDominatingMe();

	bool IsStaggering();

	CNavArea *GetLastKnownArea();

	void DTRCallBack_OnVomitedUpon(CBasePlayer *pAttacker, bool bIsExplodedByBoomer);
};

class TerrorNavMesh {
public:
	static int m_iOff_m_fMapMaxFlowDistance;

public:
	inline float GetMapMaxFlowDistance()
	{
		return *(float *)((byte *)(this) + m_iOff_m_fMapMaxFlowDistance);
	}
};

class ZombieManager {
public:
	static void *pFnGetRandomPZSpawnPosition;
	static ICallWrapper *pCallGetRandomPZSpawnPosition;

public:
	bool GetRandomPZSpawnPosition(ZombieClassType type, int attampts, CTerrorPlayer *pPlayer, Vector *pOutPos);
};

/*
class CTerrorEntityListner : public ISMEntityListener {
public:
	virtual void OnEntityCreated(CBaseEntity *pEntity, const char *classname);
	virtual void OnEntityDestroyed(CBaseEntity *pEntity);
	
protected:
	void OnPostThink();
};
*/

class BossZombiePlayerBot /*: public CTerrorPlayer*/ {
public:
	static void *pFnChooseVictim;
	static CDetour *DTR_ChooseVictim;

public:
	CTerrorPlayer *OnBoomerChooseVictim(CTerrorPlayer *pAttacker, CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer);
	CTerrorPlayer *OnSmokerChooseVictim(CTerrorPlayer *pAttacker, CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer);
	CTerrorPlayer *OnJockeyChooseVictim(CTerrorPlayer *pAttacker, CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer);
	CTerrorPlayer *OnHunterChooseVictim(CTerrorPlayer *pAttacker, CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer);
	CTerrorPlayer *OnSpitterChooseVictim(CTerrorPlayer *pAttacker, CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer);
	CTerrorPlayer *OnChargerChooseVictim(CTerrorPlayer *pAttacker, CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer);
	CTerrorPlayer *OnTankChooseVictim(CTerrorPlayer *pAttacker, CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer);
};

#endif // _WRAPPERS_H_INCLUDED