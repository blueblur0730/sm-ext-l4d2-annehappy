#ifndef _WRAPPERS_H_INCLUDED_
#define _WRAPPERS_H_INCLUDED_

#pragma once

#include <vector>

#include "extension.h"
#include "IBinTools.h"
#include "ISDKHooks.h"
#include "CDetour/detours.h"

#include "const.h"
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

typedef bool (*ShouldHitFunc_t)(IHandleEntity* pHandleEntity, int contentsMask, void* data);

class CTraceFilterSimpleExt : public CTraceFilter
{
public:
	CTraceFilterSimpleExt(const IHandleEntity *passedict = NULL, Collision_Group_t collisionGroup = COLLISION_GROUP_NONE, ShouldHitFunc_t pExtraShouldHitFunc = NULL, void *data = NULL);
	virtual bool ShouldHitEntity(IHandleEntity *pHandleEntity, int contentsMask);

private:
	const IHandleEntity* m_pPassEnt;
	int m_collisionGroup;
	ShouldHitFunc_t m_pExtraShouldHitCheckFunction;
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

class CBaseEntity : public IServerEntity {
public:
	static int vtblindex_CBaseEntity_Teleport;
	static ICallWrapper *pCallTeleport;

	static int vtblindex_CBaseEntity_GetEyeAngle;
	static ICallWrapper *pCallGetEyeAngle;

	static int dataprop_m_vecAbsVelocity;
	static int dataprop_m_vecAbsOrigin;
	static int dataprop_m_hOwnerEntity;
	static int dataprop_m_MoveType;
	static int dataprop_m_lifeState;
	static int dataprop_m_iHealth;

public:
	inline int GetDataOffset(const char *name)
	{
		sm_datatable_info_t offset_data_info;
		datamap_t *offsetMap = gamehelpers->GetDataMap(this);
		if (!offsetMap || !gamehelpers->FindDataMapInfo(offsetMap, name, &offset_data_info))
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
		if (dataprop_m_vecAbsVelocity == -1)
			dataprop_m_vecAbsVelocity = GetDataOffset("m_vecAbsVelocity");

		return *(Vector*)((byte*)(this) + dataprop_m_vecAbsVelocity);
	}

	inline Vector GetAbsOrigin()
	{
		
		if (dataprop_m_vecAbsOrigin == -1)
			dataprop_m_vecAbsOrigin = GetDataOffset("m_vecAbsOrigin");

		return *(Vector*)((byte*)(this) + dataprop_m_vecAbsOrigin);
	}

	inline CBaseEntity *GetOwnerEntity()
	{
		if (dataprop_m_hOwnerEntity == -1)
			dataprop_m_hOwnerEntity = GetDataOffset("m_hOwnerEntity");

		return gamehelpers->ReferenceToEntity(((CBaseHandle *)((byte *)(this) + dataprop_m_hOwnerEntity))->GetEntryIndex());
	}

	inline MoveType_t GetMoveType()
	{
		if (dataprop_m_MoveType == -1)
			dataprop_m_MoveType = GetDataOffset("m_MoveType");

		return *(MoveType_t*)((byte*)(this) + dataprop_m_MoveType);
	}

	inline char& GetLifeState()
	{
		if (dataprop_m_lifeState == -1) 
			dataprop_m_lifeState = GetDataOffset("m_lifeState");

		return *(char*)((byte*)(this) + dataprop_m_lifeState);
	}

	// player will be dead but the health will be still 1. why?
	inline int GetHealth()
	{
		if (dataprop_m_iHealth == -1)
			dataprop_m_iHealth = GetDataOffset("m_iHealth");

		return *(int*)((byte*)(this) + dataprop_m_iHealth);
	}

	inline bool IsDead()
	{
		return GetLifeState() == LIFE_DEAD;
	}

	void Teleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity);

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