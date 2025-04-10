#include "utils.h"
#include "wrappers.h"

CTraceFilterSimple::CTraceFilterSimple(const IHandleEntity* passedict = NULL, Collision_Group_t collisionGroup = COLLISION_GROUP_NONE, ShouldHitFunc_t pExtraShouldHitFunc = NULL)
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

CTraceFilterSimple::CTraceFilterSimple(const IHandleEntity* passedict = NULL, Collision_Group_t collisionGroup = COLLISION_GROUP_NONE, ShouldHitFunc2_t pExtraShouldHitFunc = NULL, void* data = NULL)
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

    pCallCTraceFilterSimple2->Execute(&stack, NULL);
}

bool CTraceFilterSimple::ShouldHitEntity(IHandleEntity *pHandleEntity, int contentsMask)
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

void CTraceFilterSimple::SetTraceType(TraceType_t traceType)
{
    m_TraceType = traceType;
}

float CNavAreaExt::GetFlow()
{
    return *(float *)((byte *)(this) + m_iOff_m_flow);
}

edict_t *CBaseEntity::edict()
{
    return gameents->BaseEntityToEdict(this);
}

int CBaseEntity::entindex()
{
    return gamehelpers->EntityToBCompatRef(this);
}

const char *CBaseEntity::GetClassName()
{
    return edict()->GetClassName();
}

void CBaseEntity::GetVelocity(Vector *velocity)
{
    velocity = (Vector *)((byte *)(this) + CBaseEntity::m_iOff_m_vecVelocity);
}

CBaseEntity *CBaseEntity::GetOwnerEntity()
{
    sm_datatable_info_t offset_data_info;
    datamap_t *offsetMap = gamehelpers->GetDataMap(this);
    if (!gamehelpers->FindDataMapInfo(offsetMap, "m_hOwnerEntity", &offset_data_info))
        return NULL;
    
    CBaseHandle *hndl = (CBaseHandle *)((byte *)(this) + offset_data_info.actual_offset);
    return gamehelpers->ReferenceToEntity(hndl->GetEntryIndex());
}

MoveType_t CBaseEntity::GetMoveType()
{
    sm_datatable_info_t offset_data_info;
    datamap_t *offsetMap = gamehelpers->GetDataMap(this);
    if (!gamehelpers->FindDataMapInfo(offsetMap, "m_MoveType", &offset_data_info))
        return MOVETYPE_NONE;

    return (MoveType_t)*(char*)((byte *)(this) + offset_data_info.actual_offset);
}

void CBaseEntity::Teleport(Vector *newPosition, QAngle *newAngles, Vector *newVelocity)
{
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

void CBaseEntity::GetEyeAngles(QAngle *pRetAngle)
{
    unsigned char params[sizeof(void *)];
    unsigned char *vptr = params;

    *(CBaseEntity **)vptr = this;
    pCallGetEyeAngle->Execute(params, &pRetAngle);
}

BlockType_t CEnvPhysicsBlocker::GetBlockType()
{
    return *(BlockType_t*)((byte*)(this) + CEnvPhysicsBlocker::m_iOff_m_nBlockType);
}

bool CBaseAbility::IsSpraying()
{
    return *(bool*)((byte*)(this) + CBaseAbility::m_iOff_m_isSpraying);
}

bool CBaseCombatWeaponExt::IsReloading()
{
    return *(bool*)((byte*)(this) + CBaseCombatWeaponExt::m_iOff_m_bInReload);
}

int CBasePlayer::GetButton()
{
    sm_datatable_info_t pDataTable;
    if (!gamehelpers->FindDataMapInfo(gamehelpers->GetDataMap(this), "m_nButtons", &pDataTable))
        return -1;

    return *(int*)((byte*)(this) + pDataTable.actual_offset);
}

// Thanks to Forgetest from his l4d_lagcomp_skeet.
CUserCmd *CBasePlayer::GetCurrentCommand()
{
    sm_datatable_info_t pDataTable;
    if (!gamehelpers->FindDataMapInfo(gamehelpers->GetDataMap(this), "m_hViewModel", &pDataTable))
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

void CTerrorPlayer::OnVomitedUpon(CBasePlayer *pAttacker, bool bIsExplodedByBoomer)
{
    struct {
        CTerrorPlayer *pVictim;
        CBasePlayer *pAttacker;
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