#include <vector>

#include "extension.h"
#include "IBinTools.h"
#include "ISDKHooks.h"
#include "detours.h"

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

public:
	CTraceFilterSimple(const IHandleEntity* passedict = NULL, Collision_Group_t collisionGroup = COLLISION_GROUP_NONE, ShouldHitFunc_t pExtraShouldHitFunc = NULL)
	{
		m_pPassEnt = passedict;
		m_collisionGroup = collisionGroup;
		m_pExtraShouldHitCheckFunction = pExtraShouldHitFunc;

		// Call contrustor to replace vtable on this instance
		struct {
			const CTraceFilterSimple* pFilter;
			const IHandleEntity* passedict;
			Collision_Group_t collisionGroup;
			ShouldHitFunc_t pExtraShouldHitFunc;
		} stack{ this, passedict, collisionGroup, pExtraShouldHitFunc };

		pCallCTraceFilterSimple->Execute(&stack, NULL);
	}

	CTraceFilterSimple(const IHandleEntity* passedict = NULL, Collision_Group_t collisionGroup = COLLISION_GROUP_NONE, ShouldHitFunc2_t pExtraShouldHitFunc = NULL, void* data)
	{
		m_pPassEnt = passedict;
		m_collisionGroup = collisionGroup;
		m_pExtraShouldHitCheckFunction2 = pExtraShouldHitFunc;
		m_data = data;

		// Call contrustor to replace vtable on this instance
		struct {
			const CTraceFilterSimple* pFilter;
			const IHandleEntity* passedict;
			Collision_Group_t collisionGroup;
			ShouldHitFunc2_t pExtraShouldHitFunc;
		} stack{ this, passedict, collisionGroup, pExtraShouldHitFunc };

		pCallCTraceFilterSimple->Execute(&stack, NULL);
	}

	virtual bool ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask) override {
		// Don't test if the game code tells us we should ignore this collision...
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( !pEntity )
			return false;

		if ( m_pPassEnt )
		{
			if ( !PassServerEntityFilter( pHandleEntity, m_pPassEnt ) )
			{
				return false;
			}
		}

		// not going to collide today :)
		//if ( !pEntity->ShouldCollide( m_collisionGroup, contentsMask ) )
			//return false;

		//if ( pEntity && !g_pGameRules->ShouldCollide( m_collisionGroup, pEntity->GetCollisionGroup() ) )
			//return false;

		if ( m_pExtraShouldHitCheckFunction &&
			(! ( m_pExtraShouldHitCheckFunction(pHandleEntity, contentsMask) )))
			return false;

		if ( m_pExtraShouldHitCheckFunction2 &&
			(! ( m_pExtraShouldHitCheckFunction2(pHandleEntity, contentsMask, m_data) )))
			return false;

		return true;
	}

	virtual TraceType_t GetTraceType() const {
		return m_TraceType;
	}
	void SetTraceType(TraceType_t traceType) {
		m_TraceType = traceType;
	}

	static inline CBaseEntity *EntityFromEntityHandle(const IHandleEntity *pHandleEntity) {
		if ( staticpropmgr->IsStaticProp( pHandleEntity ) )
			return NULL;
	
		IServerUnknown *pUnk = (IServerUnknown*)pHandleEntity;
		return (CBaseEntity *)pUnk->GetBaseEntity();
	}

private:
	TraceType_t m_TraceType;
	const IHandleEntity* m_pPassEnt;
	int m_collisionGroup;
	ShouldHitFunc_t m_pExtraShouldHitCheckFunction;
	ShouldHitFunc2_t m_pExtraShouldHitCheckFunction2;
	void *m_data;
};

class CBaseTraceFilter : public ITraceFilter {
public:

	virtual bool ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask);
	virtual TraceType_t GetTraceType() const {
		return m_TraceType;
	}

	void SetTraceType(TraceType_t traceType) {
		m_TraceType = traceType;
	}

private:
	TraceType_t m_TraceType;
};

class CBaseEntity : public IServerEntity {
public:
	static int m_iOff_m_vecVelocity;

	static int vtblindex_CBaseEntity_Teleport;
	static ICallWrapper *pCallTeleport;

	static int vtblindex_CBaseEntity_GetEyeAngle;
	static ICallWrapper *pCallGetEyeAngle;

	static int vtblindex_CBaseEntity_PostThink;

public:
	edict_t* edict() {
		return gameents->BaseEntityToEdict(this);
	}

	int entindex() {
		return gamehelpers->EntityToBCompatRef(this);
	}

	const char* GetClassName() {
		return edict()->GetClassName();
	}

	void GetVelocity(Vector *velocity) {
		velocity = (Vector *)((byte *)(this) + CBaseEntity::m_iOff_m_vecVelocity);
	}

	CBaseEntity *GetOwnerEntity() {
		sm_datatable_info_t offset_data_info;
		datamap_t *offsetMap = gamehelpers->GetDataMap(this);
		if (!gamehelpers->FindDataMapInfo(offsetMap, "m_hOwnerEntity", &offset_data_info))
			return NULL;
		
		CBaseHandle *hndl = (CBaseHandle *)((byte *)(this) + offset_data_info.actual_offset);
		return gamehelpers->ReferenceToEntity(hndl->GetEntryIndex());
	}

	MoveType_t GetMoveType() {
		sm_datatable_info_t offset_data_info;
		datamap_t *offsetMap = gamehelpers->GetDataMap(this);
		if (!gamehelpers->FindDataMapInfo(offsetMap, "m_MoveType", &offset_data_info))
			return MOVETYPE_NONE;

		return (MoveType_t)*(char*)((byte *)(this) + offset_data_info.actual_offset);
	}

	void Teleport(Vector *newPosition, QAngle *newAngles, Vector *newVelocity) {
		unsigned char params[sizeof(void *) * 4];
		unsigned char *vptr = params;
		*(CBaseEntity **)vptr = this;
		vptr += sizeof(CBaseEntity *);
		*(Vector **)vptr = newPosition;
		vptr += sizeof(Vector *);
		*(QAngle **)vptr = newAngles;
		vptr += sizeof(QAngle *);
		*(Vector **)vptr = newVelocity;
		
		pCallTeleport->Execute(params, NULL);
	}

	void GetEyeAngles(QAngle *pRetAngle) {
		unsigned char params[sizeof(void *)];
		unsigned char *vptr = params;
	
		*(CBaseEntity **)vptr = this;
		pCallGetEyeAngle->Execute(params, &pRetAngle);
	}
};

class CEnvPhysicsBlocker : public CBaseEntity {
public:
	static int m_iOff_m_nBlockType;

public:
	BlockType_t GetBlockType() {
		return *(BlockType_t*)((byte*)(this) + CEnvPhysicsBlocker::m_iOff_m_nBlockType);
	}
};

class CBaseAbility : public CBaseEntity {
public:
	static int m_iOff_m_isSpraying;

public:
	bool IsSpraying() {
		return *(bool*)((byte*)(this) + CBaseAbility::m_iOff_m_isSpraying);
	}
};

class CBaseCombatWeaponExt : public CBaseEntity {
public:
	static int m_iOff_m_bInReload;

public:
	bool IsReloading() {
		return *(bool*)((byte*)(this) + CBaseCombatWeaponExt::m_iOff_m_bInReload);
	}
};

class CBasePlayer : public CBaseEntity {
public:
	static int m_iOff_m_fFlags;

public:
	inline int GetFlags() {
		return *(int*)((byte*)(this) + CBasePlayer::m_iOff_m_fFlags);
	}

	bool IsBot() {
		return (GetFlags() & FL_FAKECLIENT) != 0;
	}

	int GetButton() {
		sm_datatable_info_t pDataTable;
		if (!gamehelpers->FindDataMapInfo(gamehelpers->GetDataMap(this), "m_nButtons", &pDataTable))
			return -1;

		return *(int*)((byte*)(this) + pDataTable.actual_offset);
	}

	// Thanks to Forgetest from his l4d_lagcomp_skeet.
	CUserCmd *GetCurrentCommand() {
		sm_datatable_info_t pDataTable;
		if (!gamehelpers->FindDataMapInfo(gamehelpers->GetDataMap(this), "m_hViewModel", &pDataTable))
			return NULL;

		int offset = pDataTable.actual_offset 
					+ 4 * 2 /* CHandle<CBaseViewModel> * MAX_VIEWMODELS */
					+ 88 /* sizeof(m_LastCmd) */;

		return (CUserCmd *)((byte*)(this) + offset);
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
	enum L4D2Teams {
		L4D2Teams_Unasigned = 0,
		L4D2Teams_Spectator = 1,
		L4D2Teams_Survivor = 2,
		L4D2Teams_Infected = 3,
		L4D2Teams_L4D1Survivors = 4
	};

	IPlayerInfo *GetPlayerInfo() {
		IGamePlayer *pPlayer = GetGamePlayer();
		if (!pPlayer)
			return NULL;

		return pPlayer->GetPlayerInfo();
	}

	inline IGamePlayer *GetGamePlayer() {
		return playerhelpers->GetGamePlayer(entindex());
	}

	// IPlayerInfo: abs origin
	// CServerGameClients: abs origin + view offset
	Vector GetEyeOrigin() {
		IGamePlayer *pGamePlayer = GetGamePlayer();
		if (!pGamePlayer)
			return Vector(0, 0, 0);

		Vector vecRet;
		serverClients->ClientEarPosition(pGamePlayer->GetEdict(), &vecRet);
		return vecRet;
	}

	Vector GetAbsOrigin() {
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

	L4D2Teams GetTeam() {
		IPlayerInfo *pPlayerInfo = GetPlayerInfo();
		if (!pPlayerInfo)
			return L4D2Teams_Unasigned;

		return (L4D2Teams)pPlayerInfo->GetTeamIndex();
	}

	inline bool IsSurvivor() {
		return (GetTeam() == L4D2Teams_Survivor);
	}

	inline bool IsInfected() {
		return (GetTeam() == L4D2Teams_Infected);
	}

	inline bool IsSpectator() {
		return (GetTeam() == TEAM_SPECTATOR);
	}

	inline bool HasVisibleThreats() {
		return *(bool*)((byte*)(this) + m_iOff_m_hasVisibleThreats);
	}

	CBaseAbility *GetAbility() {
		return (CBaseAbility *)OffsetEHandleToEntity(m_iOff_m_customAbility);
	}

	CBaseEntity *GetGroundEntity() {
		return OffsetEHandleToEntity(m_iOff_m_hGroundEntity);
	}

	CBaseCombatWeaponExt *GetActiveWeapon() {
		return (CBaseCombatWeaponExt *)OffsetEHandleToEntity(m_iOff_m_hActiveWeapon);
	}

	CTerrorPlayer *GetTongueVictim() {
		return (CTerrorPlayer *)OffsetEHandleToEntity(m_iOff_m_tongueVictim);
	}

	inline int GetClass() {
		return *(uint8_t *)((uint8_t *)this + m_iOff_m_zombieClass);
	}

	inline bool IsBoomer() {
        return (GetClass() == ZC_BOOMER);
    }

	inline bool IsSmoker() {
        return (GetClass() == ZC_SMOKER);
	}

	CBaseEntity* OffsetEHandleToEntity(int iOff) {
		edict_t* pEdict = gamehelpers->GetHandleEntity(*(CBaseHandle*)((byte*)(this) + iOff));
		if (pEdict == NULL) 
			return NULL;

		// Make sure it's a player
		if (engine->GetPlayerUserId(pEdict) == -1) 
			return NULL;

		return gameents->EdictToBaseEntity(pEdict);
	}

	void OnVomitedUpon(CBasePlayer *pAttacker, bool bIsExplodedByBoomer) {
		struct {
			CTerrorPlayer *pVictim;
			CBasePlayer *pAttacker;
			bool bIsExplodedByBoomer;
		} stake {this, pAttacker, bIsExplodedByBoomer};

		pCallOnVomitedUpon->Execute(&stake, NULL);
	}

	CTerrorPlayer *GetSpecialInfectedDominatingMe() {
		struct {
			CTerrorPlayer *pVictim;
		} stake {this};

		void *ret;
		pCallGetSpecialInfectedDominatingMe->Execute(&stake, &ret);
		return (CTerrorPlayer *)ret;
	}

	bool IsStaggering() {
		struct {
			CTerrorPlayer *pVictim;
		} stake {this};

		void *ret;
		pCallIsStaggering->Execute(&stake, &ret);
		return *(bool *)((byte *)ret);
	}

	CNavAreaExt *GetLastKnownArea() {
		struct {
			CTerrorPlayer *pThis;
		} stake {this};

		void *ret;
		pCallGetLastKnownArea->Execute(&stake, &ret);
		return (CNavAreaExt *)ret;
	}

	void DTRCallBack_OnVomitedUpon(CBasePlayer *pAttacker, bool bIsExplodedByBoomer);
};

class CNavAreaExt {
public:
	static int m_iOff_m_flow;

public:
	float GetFlow() {
		return *(float *)((byte *)(this) + m_iOff_m_flow);
	}
};

class TerrorNavMesh {
public:
	static int m_iOff_m_fMapMaxFlowDistance;

public:
	float GetMapMaxFlowDistance() {
		return *(float *)((byte *)(this) + m_iOff_m_fMapMaxFlowDistance);
	}
};

class ZombieManager {
public:
	static void *pFnGetRandomPZSpawnPosition;
	static ICallWrapper *pCallGetRandomPZSpawnPosition;

public:
	bool GetRandomPZSpawnPosition(ZombieClassType type, int attampts, CTerrorPlayer *pPlayer, Vector *pOutPos) {
		struct {
			ZombieManager *_this;
			ZombieClassType zombieClass;
			int attampts;
			CTerrorPlayer *pPlayer;
			Vector *pOutPos;
		} stake {this, type, attampts, pPlayer, pOutPos};

		void *ret;
		pCallGetRandomPZSpawnPosition->Execute(&stake, &ret);

		return *(bool *)((byte *)ret);
	}
};

class CTerrorEntityListner : public ISMEntityListener {
public:
	virtual void OnEntityCreated(CBaseEntity *pEntity, const char *classname) {};
	virtual void OnEntityDestroyed(CBaseEntity *pEntity) {};

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

static bool PassServerEntityFilter( const IHandleEntity *pTouch, const IHandleEntity *pPass ) 
{
	if ( !pPass )
		return true;

	if ( pTouch == pPass )
		return false;

	CBaseEntity *pEntTouch = CTraceFilterSimple::EntityFromEntityHandle( pTouch );
	CBaseEntity *pEntPass = CTraceFilterSimple::EntityFromEntityHandle( pPass );
	if ( !pEntTouch || !pEntPass )
		return true;

	// don't clip against own missiles
	if ( pEntTouch->GetOwnerEntity() == pEntPass )
		return false;
	
	// don't clip against owner
	if ( pEntPass->GetOwnerEntity() == pEntTouch )
		return false;	

	return true;
}