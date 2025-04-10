#ifndef _WRAPPERS_H_INCLUDED_
#define _WRAPPERS_H_INCLUDED_

#include <vector>

#include "extension.h"
#include "IBinTools.h"
#include "ISDKHooks.h"
#include "CDetour/detours.h"

#include "edict.h"
#include "igameevents.h"
#include "iplayerinfo.h"
#include "engine/IEngineTrace.h"
#include "engine/IStaticPropMgr.h"
#include "ihandleentity.h"
#include "mathlib.h"
#include "usercmd.h"

#define EYE_ANGLE_UP_HEIGHT 15.0
#define NAV_MESH_HEIGHT 20.0
#define FALL_DETECT_HEIGHT 120.0
#define COMMAND_INTERVAL 1.0
#define PLAYER_HEIGHT 72.0
#define TURN_ANGLE_DIVIDE 3.0

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

class CTraceFilterSimple : public CTraceFilter
{
public:
	static void* pFnCTraceFilterSimple;
	static ICallWrapper* pCallCTraceFilterSimple;
	static ICallWrapper* pCallCTraceFilterSimple2;

public:
	CTraceFilterSimple(const IHandleEntity *passedict = NULL, Collision_Group_t collisionGroup = COLLISION_GROUP_NONE, ShouldHitFunc_t pExtraShouldHitFunc = NULL);
	CTraceFilterSimple(const IHandleEntity *passedict = NULL, Collision_Group_t collisionGroup = COLLISION_GROUP_NONE, ShouldHitFunc2_t pExtraShouldHitFunc = NULL, void *data = NULL);

	virtual bool ShouldHitEntity(IHandleEntity *pHandleEntity, int contentsMask) override;
	void SetTraceType(TraceType_t traceType) {
		m_TraceType = traceType;
	}

private:
	TraceType_t m_TraceType;
	const IHandleEntity* m_pPassEnt;
	int m_collisionGroup;
	ShouldHitFunc_t m_pExtraShouldHitCheckFunction;
	ShouldHitFunc2_t m_pExtraShouldHitCheckFunction2;
	void *m_data;
};

class CNavAreaExt {
public:
	static int m_iOff_m_flow;
	
public:
	float GetFlow();
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
	static int m_iOff_m_vecVelocity;

	static int vtblindex_CBaseEntity_Teleport;
	static ICallWrapper *pCallTeleport;

	static int vtblindex_CBaseEntity_GetEyeAngle;
	static ICallWrapper *pCallGetEyeAngle;

	static int vtblindex_CBaseEntity_PostThink;

public:
	edict_t* edict();
	int entindex();
	const char *GetClassName();
	void GetVelocity(Vector *velocity);
	CBaseEntity *GetOwnerEntity();
	MoveType_t GetMoveType();
	void Teleport(Vector *newPosition, QAngle *newAngles, Vector *newVelocity);
	void GetEyeAngles(QAngle *pRetAngle);
};

class CEnvPhysicsBlocker : public CBaseEntity {
public:
	static int m_iOff_m_nBlockType;

public:
	BlockType_t GetBlockType();
};

class CBaseAbility : public CBaseEntity {
public:
	static int m_iOff_m_isSpraying;

public:
	bool IsSpraying();
};

class CBaseCombatWeaponExt : public CBaseEntity {
public:
	static int m_iOff_m_bInReload;

public:
	bool IsReloading();
};

class CBasePlayer : public CBaseEntity {
public:
	static int m_iOff_m_fFlags;

public:
	inline int GetFlags() const {
		return *(int*)((byte*)(this) + CBasePlayer::m_iOff_m_fFlags);
	}

	inline bool IsBot() {
		return (GetFlags() & FL_FAKECLIENT) != 0;
	}

	int GetButton();
	CUserCmd *GetCurrentCommand();
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

	inline IGamePlayer *GetGamePlayer() {
		return playerhelpers->GetGamePlayer(entindex());
	}

	Vector GetEyeOrigin();

	inline Vector GetAbsOrigin() {
		return GetPlayerInfo()->GetAbsOrigin();
	}

	// this is fastest and the most simple.
	// if not, use m_lifeState.
	inline bool IsDead() {
		return GetPlayerInfo()->IsDead();
	}

	inline Vector GetPlayerMins() {
		return GetPlayerInfo()->GetPlayerMins();
	}

	inline Vector GetPlayerMaxs() {
		return GetPlayerInfo()->GetPlayerMaxs();
	}

	inline bool IsFakeClient() {
		IGamePlayer *pPlayer = GetGamePlayer();
		return pPlayer->IsFakeClient();
	}

	inline bool IsInGame() {
		return GetGamePlayer()->IsInGame();
	}

	inline bool IsIncapped() {
		return *(bool*)((byte*)(this) + m_iOff_m_isIncapacitated);
	}

	L4D2Teams GetTeam();

	inline bool IsSurvivor() {
		return (GetTeam() == L4D2Teams_Survivor);
	}

	inline bool IsInfected() {
		return (GetTeam() == L4D2Teams_Infected);
	}

	inline bool IsSpectator() {
		return (GetTeam() == L4D2Teams_Spectator);
	}

	inline bool HasVisibleThreats() {
		return *(bool*)((byte*)(this) + m_iOff_m_hasVisibleThreats);
	}

	CBaseAbility *GetAbility();
	CBaseEntity *GetGroundEntity();
	CBaseCombatWeaponExt *GetActiveWeapon();
	CTerrorPlayer *GetTongueVictim();

	inline int GetClass() {
		return *(uint8_t *)((uint8_t *)this + m_iOff_m_zombieClass);
	}

	inline bool IsBoomer() {
        return (GetClass() == ZC_BOOMER);
    }

	inline bool IsSmoker() {
        return (GetClass() == ZC_SMOKER);
	}

	CBaseEntity *OffsetEHandleToEntity(int iOff);

	void OnVomitedUpon(CBasePlayer *pAttacker, bool bIsExplodedByBoomer);
	CTerrorPlayer *GetSpecialInfectedDominatingMe();
	bool IsStaggering();
	CNavAreaExt *GetLastKnownArea();

	void DTRCallBack_OnVomitedUpon(CBasePlayer *pAttacker, bool bIsExplodedByBoomer);
};

class TerrorNavMesh {
public:
	static int m_iOff_m_fMapMaxFlowDistance;

public:
	inline float GetMapMaxFlowDistance() {
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

class CTerrorEntityListner : public ISMEntityListener {
public:
	virtual void OnEntityCreated(CBaseEntity *pEntity, const char *classname) {};
	virtual void OnEntityDestroyed(CBaseEntity *pEntity) {};
	
protected:
	void OnPostThink();
};

class BossZombiePlayerBot : public CTerrorPlayer {
public:
	static void *pFnChooseVictim;
	static CDetour *DTR_ChooseVictim;

public:
	CTerrorPlayer *OnBoomerChooseVictim(CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer);
	CTerrorPlayer *OnSmokerChooseVictim(CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer);
	CTerrorPlayer *OnJockeyChooseVictim(CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer);
	CTerrorPlayer *OnHunterChooseVictim(CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer);
	CTerrorPlayer *OnSpitterChooseVictim(CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer);
	CTerrorPlayer *OnChargerChooseVictim(CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer);
	CTerrorPlayer *OnTankChooseVictim(CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer);
};

#endif // _WRAPPERS_H_INCLUDED