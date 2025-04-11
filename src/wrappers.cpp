#include "utils.h"
#include "wrappers.h"
#include "player.h"

CTraceFilterSimpleExt::CTraceFilterSimpleExt(const IHandleEntity* passedict = NULL, Collision_Group_t collisionGroup = COLLISION_GROUP_NONE, ShouldHitFunc_t pExtraShouldHitFunc = NULL)
{
    m_pPassEnt = passedict;
    m_collisionGroup = collisionGroup;
    m_pExtraShouldHitCheckFunction = pExtraShouldHitFunc;

    // Call contrustor to replace vtable on this instance
    struct {
        const CTraceFilterSimpleExt* pFilter;
        const IHandleEntity* passedict;
        Collision_Group_t collisionGroup;
        ShouldHitFunc_t pExtraShouldHitFunc;
    } stack{ this, passedict, collisionGroup, pExtraShouldHitFunc };

    pCallCTraceFilterSimple->Execute(&stack, NULL);
}

CTraceFilterSimpleExt::CTraceFilterSimpleExt(const IHandleEntity* passedict = NULL, Collision_Group_t collisionGroup = COLLISION_GROUP_NONE, ShouldHitFunc2_t pExtraShouldHitFunc = NULL, void* data = NULL)
{
    m_pPassEnt = passedict;
    m_collisionGroup = collisionGroup;
    m_pExtraShouldHitCheckFunction2 = pExtraShouldHitFunc;
    m_data = data;

    // Call contrustor to replace vtable on this instance
    struct {
        const CTraceFilterSimpleExt* pFilter;
        const IHandleEntity* passedict;
        Collision_Group_t collisionGroup;
        ShouldHitFunc2_t pExtraShouldHitFunc;
    } stack{ this, passedict, collisionGroup, pExtraShouldHitFunc };

    pCallCTraceFilterSimple2->Execute(&stack, NULL);
}

bool CTraceFilterSimpleExt::ShouldHitEntity(IHandleEntity *pHandleEntity, int contentsMask)
{
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

CBaseEntity *CBaseEntityExt::GetOwnerEntity()
{
    sm_datatable_info_t offset_data_info;
    datamap_t *offsetMap = gamehelpers->GetDataMap((CBaseEntity *)this);
    if (!gamehelpers->FindDataMapInfo(offsetMap, "m_hOwnerEntity", &offset_data_info))
        return NULL;
    
    CBaseHandle *hndl = (CBaseHandle *)((byte *)(this) + offset_data_info.actual_offset);
    return gamehelpers->ReferenceToEntity(hndl->GetEntryIndex());
}

MoveType_t CBaseEntityExt::GetMoveType()
{
    sm_datatable_info_t offset_data_info;
    datamap_t *offsetMap = gamehelpers->GetDataMap((CBaseEntity *)this);
    if (!gamehelpers->FindDataMapInfo(offsetMap, "m_MoveType", &offset_data_info))
        return MOVETYPE_NONE;

    return (MoveType_t)*(char*)((byte *)(this) + offset_data_info.actual_offset);
}

void CBaseEntityExt::Teleport(Vector *newPosition, QAngle *newAngles, Vector *newVelocity)
{
    unsigned char params[sizeof(void *) * 4];
    unsigned char *vptr = params;
    *(CBaseEntity **)vptr = (CBaseEntity *)this;
    vptr += sizeof(CBaseEntity *);
    *(Vector **)vptr = newPosition;
    vptr += sizeof(Vector *);
    *(QAngle **)vptr = newAngles;
    vptr += sizeof(QAngle *);
    *(Vector **)vptr = newVelocity;
    
    pCallTeleport->Execute(params, NULL);
}

void CBaseEntityExt::GetEyeAngles(QAngle *pRetAngle)
{
    unsigned char params[sizeof(void *)];
    unsigned char *vptr = params;

    *(CBaseEntity **)vptr = (CBaseEntity *)this;
    pCallGetEyeAngle->Execute(params, &pRetAngle);
}

int CBasePlayerExt::GetButton()
{
    sm_datatable_info_t pDataTable;
    if (!gamehelpers->FindDataMapInfo(gamehelpers->GetDataMap((CBaseEntity *)this), "m_nButtons", &pDataTable))
        return -1;

    return *(int*)((byte*)(this) + pDataTable.actual_offset);
}

// Thanks to Forgetest from his l4d_lagcomp_skeet.
CUserCmd *CBasePlayerExt::GetCurrentCommand()
{
    sm_datatable_info_t pDataTable;
    if (!gamehelpers->FindDataMapInfo(gamehelpers->GetDataMap((CBaseEntity *)this), "m_hViewModel", &pDataTable))
        return NULL;

    int offset = pDataTable.actual_offset 
                + 4 * 2 /* CHandle<CBaseViewModel> * MAX_VIEWMODELS */
                + 88 /* sizeof(m_LastCmd) */;

    return (CUserCmd *)((byte*)(this) + offset);
}

IPlayerInfo *CTerrorPlayer::GetPlayerInfo()
{
    IGamePlayer *pPlayer = GetGamePlayer();
    if (!pPlayer)
        return NULL;

    return pPlayer->GetPlayerInfo();
}

// IPlayerInfo: abs origin
// CServerGameClients: abs origin + view offset
Vector CTerrorPlayer::GetEyeOrigin()
{
    IGamePlayer *pGamePlayer = GetGamePlayer();
    if (!pGamePlayer)
        return Vector(0, 0, 0);

    Vector vecRet;
    serverClients->ClientEarPosition(pGamePlayer->GetEdict(), &vecRet);
    return vecRet;
}

L4D2Teams CTerrorPlayer::GetTeam()
{
    IPlayerInfo *pPlayerInfo = GetPlayerInfo();
    if (!pPlayerInfo)
        return L4D2Teams_Unasigned;

    return (L4D2Teams)pPlayerInfo->GetTeamIndex();
}

CBaseAbility *CTerrorPlayer::GetAbility()
{
    return (CBaseAbility *)OffsetEHandleToEntity(m_iOff_m_customAbility);
}

CBaseCombatWeaponExt *CTerrorPlayer::GetActiveWeapon()
{
    return (CBaseCombatWeaponExt *)OffsetEHandleToEntity(m_iOff_m_hActiveWeapon);
}

CTerrorPlayer *CTerrorPlayer::GetTongueVictim()
{
    return (CTerrorPlayer *)OffsetEHandleToEntity(m_iOff_m_tongueVictim);
}

CBaseEntity *CTerrorPlayer::GetGroundEntity()
{
    return OffsetEHandleToEntity(m_iOff_m_hGroundEntity);
}

CBaseEntity *CTerrorPlayer::OffsetEHandleToEntity(int iOff) {
    edict_t* pEdict = gamehelpers->GetHandleEntity(*(CBaseHandle*)((byte*)(this) + iOff));
    if (pEdict == NULL) 
        return NULL;

    // Make sure it's a player
    if (engine->GetPlayerUserId(pEdict) == -1) 
        return NULL;

    return gameents->EdictToBaseEntity(pEdict);
}

void CTerrorPlayer::OnVomitedUpon(CBasePlayerExt *pAttacker, bool bIsExplodedByBoomer)
{
    struct {
        CTerrorPlayer *pVictim;
        CBasePlayerExt *pAttacker;
        bool bIsExplodedByBoomer;
    } stake {this, pAttacker, bIsExplodedByBoomer};

    pCallOnVomitedUpon->Execute(&stake, NULL);
}

CTerrorPlayer *CTerrorPlayer::GetSpecialInfectedDominatingMe()
{
    struct {
        CTerrorPlayer *pVictim;
    } stake {this};

    void *ret;
    pCallGetSpecialInfectedDominatingMe->Execute(&stake, &ret);
    return (CTerrorPlayer *)ret;
}

bool CTerrorPlayer::IsStaggering()
{
    struct {
        CTerrorPlayer *pVictim;
    } stake {this};

    void *ret;
    pCallIsStaggering->Execute(&stake, &ret);
    return *(bool *)((byte *)ret);
}

CNavAreaExt *CTerrorPlayer::GetLastKnownArea()
{
    struct {
        CTerrorPlayer *pThis;
    } stake {this};

    void *ret;
    pCallGetLastKnownArea->Execute(&stake, &ret);
    return (CNavAreaExt *)ret;
}

bool ZombieManager::GetRandomPZSpawnPosition(ZombieClassType type, int attampts, CTerrorPlayer *pPlayer, Vector *pOutPos)
{
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

inline void CTraceFilterSimpleExt::SetTraceType(TraceType_t traceType)
{
    m_TraceType = traceType;
}

inline edict_t *CBaseEntityExt::edict()
{
    return gameents->BaseEntityToEdict((CBaseEntity *)this);
}

inline int CBaseEntityExt::entindex()
{
    return gamehelpers->EntityToBCompatRef((CBaseEntity *)this);
}

inline const char *CBaseEntityExt::GetClassName()
{
    return edict()->GetClassName();
}

inline float CNavAreaExt::GetFlow()
{
    return *(float *)((byte *)(this) + m_iOff_m_flow);
}

inline void CBaseEntityExt::GetVelocity(Vector *velocity)
{
    velocity = (Vector *)((byte *)(this) + CBaseEntityExt::m_iOff_m_vecVelocity);
}

inline BlockType_t CEnvPhysicsBlocker::GetBlockType()
{
    return *(BlockType_t*)((byte*)(this) + CEnvPhysicsBlocker::m_iOff_m_nBlockType);
}

inline bool CBaseAbility::IsSpraying()
{
    return *(bool*)((byte*)(this) + CBaseAbility::m_iOff_m_isSpraying);
}

inline bool CBaseCombatWeaponExt::IsReloading()
{
    return *(bool*)((byte*)(this) + CBaseCombatWeaponExt::m_iOff_m_bInReload);
}

inline int CBasePlayerExt::GetFlags()
{
    return *(int*)((byte*)(this) + CBasePlayerExt::m_iOff_m_fFlags);
}

inline bool CBasePlayerExt::IsBot()
{
    return (GetFlags() & FL_FAKECLIENT) != 0;
}

inline IGamePlayer *CTerrorPlayer::GetGamePlayer()
{
    return playerhelpers->GetGamePlayer(entindex());
}

inline Vector CTerrorPlayer::GetAbsOrigin()
{
    IPlayerInfo *pPlayerInfo = GetPlayerInfo();
    if (!pPlayerInfo)
        return Vector(0, 0, 0);

    return GetPlayerInfo()->GetAbsOrigin();
}

inline bool CTerrorPlayer::IsDead()
{
    IPlayerInfo *pPlayerInfo = GetPlayerInfo();
    if (!pPlayerInfo)
        return false;

    return GetPlayerInfo()->IsDead();
}

inline Vector CTerrorPlayer::GetPlayerMins()
{
    IPlayerInfo *pPlayerInfo = GetPlayerInfo();
    if (!pPlayerInfo)
        return Vector(0, 0, 0);

    return GetPlayerInfo()->GetPlayerMins();
}

inline Vector CTerrorPlayer::GetPlayerMaxs()
{
    IPlayerInfo *pPlayerInfo = GetPlayerInfo();
    if (!pPlayerInfo)
        return Vector(0, 0, 0);

    return GetPlayerInfo()->GetPlayerMaxs();
}

inline bool CTerrorPlayer::IsFakeClient()
{   
    IGamePlayer *pPlayerInfo = GetGamePlayer();
    if (!pPlayerInfo)
        return false;

    return GetGamePlayer()->IsFakeClient();
}

inline bool CTerrorPlayer::IsInGame()
{
    IGamePlayer *pPlayerInfo = GetGamePlayer();
    if (!pPlayerInfo)
        return false;

    return GetGamePlayer()->IsInGame();
}

inline bool CTerrorPlayer::IsIncapped()
{
    return *(bool*)((byte*)(this) + m_iOff_m_isIncapacitated);
}

inline bool CTerrorPlayer::IsSurvivor()
{
    return (GetTeam() == L4D2Teams_Survivor);
}

inline bool CTerrorPlayer::IsInfected()
{
    return (GetTeam() == L4D2Teams_Infected);
}

inline bool CTerrorPlayer::IsSpectator()
{
    return (GetTeam() == L4D2Teams_Spectator);
}

inline bool CTerrorPlayer::HasVisibleThreats()
{
    return *(bool*)((byte*)(this) + m_iOff_m_hasVisibleThreats);
}

inline ZombieClassType CTerrorPlayer::GetClass()
{
    return *(ZombieClassType *)((uint8_t *)this + m_iOff_m_zombieClass);
}

inline bool CTerrorPlayer::IsBoomer()
{
    return (GetClass() == ZC_BOOMER);
}

inline bool CTerrorPlayer::IsSmoker()
{
    return (GetClass() == ZC_SMOKER);
}

inline float TerrorNavMesh::GetMapMaxFlowDistance()
{
    return *(float *)((byte *)(this) + m_iOff_m_fMapMaxFlowDistance);
}