#include "utils.h"
#include "wrappers.h"

CTraceFilterSimpleExt::CTraceFilterSimpleExt(const IHandleEntity* passedict, Collision_Group_t collisionGroup, ShouldHitFunc_t pExtraShouldHitFunc)
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

CTraceFilterSimpleExt::CTraceFilterSimpleExt(const IHandleEntity* passedict, Collision_Group_t collisionGroup, ShouldHitFunc2_t pExtraShouldHitFunc, void* data)
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
/*
void CBaseEntity::GetVelocity(Vector *velocity, AngularImpulse *vAngVelocity)
{
    unsigned char params[sizeof(void *) * 4];
    unsigned char *vptr = params;

    *(CBaseEntity **)vptr = (CBaseEntity *)this;
    vptr += sizeof(CBaseEntity *);
    *(Vector **)vptr = velocity;
    vptr += sizeof(Vector *);
    *(AngularImpulse **)vptr = vAngVelocity;
    
    pCallTeleport->Execute(params, NULL);
}
*/

void CBaseEntity::Teleport(Vector *newPosition, QAngle *newAngles, Vector *newVelocity)
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

void CBaseEntity::GetEyeAngles(QAngle *pRetAngle)
{
    unsigned char params[sizeof(void *)];
    unsigned char *vptr = params;

    *(CBaseEntity **)vptr = (CBaseEntity *)this;
    pCallGetEyeAngle->Execute(params, &pRetAngle);
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

CBaseCombatWeapon *CTerrorPlayer::GetActiveWeapon()
{
    return (CBaseCombatWeapon *)OffsetEHandleToEntity(m_iOff_m_hActiveWeapon);
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
    if (!pEdict) 
        return NULL;

    // Make sure it's a player
    //if (engine->GetPlayerUserId(pEdict) == -1) 
        //return NULL;

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
        CBaseEntity *pVictim;
    } stake {this};

    void *ret;
    pCallGetSpecialInfectedDominatingMe->Execute(&stake, &ret);
    return reinterpret_cast<CTerrorPlayer *>(ret);
}

bool CTerrorPlayer::IsStaggering()
{
    struct {
        CTerrorPlayer *pVictim;
    } stake {this};

    void *ret;
    pCallIsStaggering->Execute(&stake, &ret);
    
    if (!ret)
    {
        smutils->LogError(myself, "CTerrorPlayer::IsStaggering: ret is NULL!");
        return false;
    }

    return *(bool *)((byte *)ret);
}

CNavArea *CTerrorPlayer::GetLastKnownArea()
{
    struct {
        CTerrorPlayer *pThis;
    } stake {this};

    void *ret;
    pCallGetLastKnownArea->Execute(&stake, &ret);
    return reinterpret_cast<CNavArea *>(ret);
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