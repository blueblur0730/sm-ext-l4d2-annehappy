#include "utils.h"
#include "extension.h"
#include "in_buttons.h"
#include "IEngineTrace.h"

CBasePlayer* UTIL_PlayerByIndexExt(int playerIndex)
{
	if (playerIndex > 0 && playerIndex <= playerhelpers->GetMaxClients()) 
	{
		IGamePlayer* pPlayer = playerhelpers->GetGamePlayer(playerIndex);
		if (pPlayer != NULL) 
			return (CBasePlayer *)gameents->EdictToBaseEntity(pPlayer->GetEdict());
	}

	return NULL;
}

CBasePlayer* UTIL_PlayerByUserIdExt(int userID)
{
	for (int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndexExt(i);

		if (!pPlayer)
			continue;

		if (playerhelpers->GetGamePlayer(i)->GetUserId() == userID)
			return pPlayer;
	}

	return NULL;
}

CBasePlayer* UTIL_GetClosetSurvivor(CBasePlayer* pPlayer, CBasePlayer* pIgnorePlayer, bool bCheckIncapp, bool bCheckDominated)
{
    int index = -1;
    index = pPlayer->entindex();
    if (index <= 0 || index > gpGlobals->maxClients)
        return NULL;

    IPlayerInfo *playerinfo = ((CTerrorPlayer *)pPlayer)->GetPlayerInfo();
    if (!playerinfo)
        return NULL;
    
    Vector vecOrigin = playerinfo->GetAbsOrigin();
    std::vector<utils_t> aTargetList;
    for (int i = 1; i <= gpGlobals->maxClients; i++)
    {
        CTerrorPlayer *pIterPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(i);
        if (!pIterPlayer)
            continue;

        IPlayerInfo *pIterPlayerInfo = pIterPlayer->GetPlayerInfo();
        if (!pIterPlayerInfo)
            continue;

        if (!pIterPlayer->IsInGame() || pIterPlayer->IsDead() || !pIterPlayer->IsSurvivor())
            continue;

        if (bCheckIncapp && pIterPlayer->IsIncapped())
            continue;

        if (bCheckDominated && pIterPlayer->GetSpecialInfectedDominatingMe())
            continue;

        if (pIgnorePlayer && i == pIgnorePlayer->entindex())
            continue;

        utils_t info;
        info.dist = vecOrigin.DistTo(pIterPlayerInfo->GetAbsOrigin());
        info.index = i;
        aTargetList.push_back(info);
    }

    if (!aTargetList.size())
        return NULL;

    // sorting ascending.
    std::sort(aTargetList.begin(), aTargetList.end(), 
        [](const utils_t& a, const utils_t& b) {
            return a.dist < b.dist;
        }
    );

    int closetIndex = aTargetList[0].index;
    aTargetList.clear();
    return UTIL_PlayerByIndexExt(closetIndex);
}

Vector UTIL_MakeVectorFromPoints(Vector src1, Vector src2)
{
    Vector output;
    output.x = src2.x - src1.x;
    output.y = src2.y - src1.y;
    output.z = src2.z - src1.z;
    return output;
}

void UTIL_ComputeAimAngles(CBasePlayer *pPlayer, CBasePlayer *pTarget, QAngle *angles, AimType type)
{
    Vector vecTargetPos;

    switch (type)
    {
        case AimEye:
        {
            vecTargetPos = ((CTerrorPlayer *)pTarget)->GetEyeOrigin();
            break;
        }

        case AimBody:
        {
            IPlayerInfo *pInfo = ((CTerrorPlayer *)pTarget)->GetPlayerInfo();
            if (!pInfo)
                return;

            vecTargetPos = pInfo->GetAbsOrigin();
            break;
        }

        case AimChest:
        {
            IPlayerInfo *pInfo = ((CTerrorPlayer *)pTarget)->GetPlayerInfo();
            if (!pInfo)
                return;

            vecTargetPos = pInfo->GetAbsOrigin();
            vecTargetPos.z += 45.0;
            break;
        }
    }

    Vector vecSelfEyePos = ((CTerrorPlayer *)pPlayer)->GetEyeOrigin();
    Vector vecLookAt = UTIL_MakeVectorFromPoints(vecSelfEyePos, vecTargetPos);

    VectorAngles(vecLookAt, *angles);
}

vec_t GetSelfTargetAngle(CBasePlayer *pAttacker, CBasePlayer *pTarget)
{
    Vector vecSelfEyePos = ((CTerrorPlayer *)pAttacker)->GetEyeOrigin();
    Vector vecTargetEyePos = ((CTerrorPlayer *)pTarget)->GetEyeOrigin();

    vecSelfEyePos.z = vecTargetEyePos.z = 0.0f;
    Vector vecResult = UTIL_MakeVectorFromPoints(vecSelfEyePos, vecTargetEyePos);
    VectorNormalize(vecResult);

    QAngle angEye;
    pAttacker->GetEyeAngles(&angEye);
    angEye.x = angEye.z = 0.0f;

    Vector vecSelfEyeVector;
    AngleVectors(angEye, &vecSelfEyeVector, NULL, NULL);
    VectorNormalize(vecSelfEyeVector);

    return (acos(vecSelfEyeVector.Dot(vecResult)) * 180.0 / M_PI);
}

bool UTIL_IsInAimOffset(CBasePlayer *pAttacker, CBasePlayer *pTarget, float offset)
{
    vec_t angle = GetSelfTargetAngle(pAttacker, pTarget);
    return (angle != 1.0f && angle <= offset);
}

// false means will, true otherwise.
static bool WillHitWallOrFall(CBasePlayer *pPlayer, Vector vec)
{
    CTerrorPlayer *pTerrorPlayer = (CTerrorPlayer *)pPlayer;
    Vector vecSelfPos = pTerrorPlayer->GetAbsOrigin();
    Vector vecResult = vecSelfPos + vec;

    Vector vecMins = pTerrorPlayer->GetPlayerMins();
    Vector vecMaxs = pTerrorPlayer->GetPlayerMaxs();

    vecSelfPos.z += NAV_MESH_HEIGHT;
    vecResult.z += NAV_MESH_HEIGHT;

    // 由自身位置 +NAV_MESH_HEIGHT 高度 向前射出大小为 mins，maxs 的固体，检测前方 NAV_MESH_HEIGHT 距离是否能撞到，撞到则不允许连跳
    trace_t tr;
    UTIL_TraceHull(vecSelfPos, vecResult, vecMins, vecMaxs, MASK_NPCSOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr, TR_EntityFilter, NULL);

    bool bHullRayHit = false;
    if (tr.DidHit())
    {
        bHullRayHit = true;
#ifdef _UTILS_DEBUG
        rootconsole->ConsolePrint("### WillHitWallOrFall, vecSelfPos.DistTo(tr.endpos): %.02f", vecSelfPos.DistTo(tr.endpos));
#endif
        if (vecSelfPos.DistTo(tr.endpos) <= NAV_MESH_HEIGHT)
            return false;
    }

    vecResult.z -= NAV_MESH_HEIGHT;
    Vector vecDownHullRayStartPos;

    // 没有撞到，则说明前方 g_hAttackRange 距离内没有障碍物，接着进行下一帧理论位置向下的检测，检测是否有会对自身造成伤害的位置
    if (!bHullRayHit)
    {
        vecDownHullRayStartPos = vecResult;
    }

    Vector vecDownHullRayEndPos = vecDownHullRayStartPos;
    vecDownHullRayEndPos.z -= 100000.0f;

    trace_t tr2;
    UTIL_TraceHull(vecDownHullRayStartPos, vecDownHullRayEndPos, vecMins, vecMaxs, MASK_NPCSOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr2, TR_EntityFilter, NULL);

    if (tr2.DidHit() && tr2.m_pEnt && tr2.m_pEnt->edict())
    {
        // 如果向下的射线撞到的位置减去起始位置的高度大于 FALL_DETECT_HEIGHT 则说明会掉下去，返回 false
#ifdef _UTILS_DEBUG
        rootconsole->ConsolePrint("### WillHitWallOrFall, FloatAbs(vecDownHullRayStartPos.z - tr2.endpos.z): %.02f", FloatAbs(vecDownHullRayStartPos.z - tr2.endpos.z));
#endif
        if (FloatAbs(vecDownHullRayStartPos.z - tr2.endpos.z) > FALL_DETECT_HEIGHT)
        {
            return false;
        }

        if (V_strcmp(tr2.m_pEnt->GetClassName(), "trigger_hurt") == 0)
            return false;

        return true;
    }

    return false;
}

static bool TR_EntityFilter(IHandleEntity *ignore, int contentsMask, void *data)
{
    if (!ignore)
        return false;

    CBaseEntity *pEntity = (CBaseEntity *)EntityFromEntityHandle(ignore);
    if (!pEntity || !pEntity->edict())
        return false;

    int index = pEntity->entindex();
    if (index > 0 && index <= gpGlobals->maxClients)
        return false;

    const char *classname = pEntity->GetClassName();
    if (!classname)
        return false;
    
    if (V_strcmp(classname, "infected") == 0
    || V_strcmp(classname, "witch") == 0
    || V_strcmp(classname, "prop_physics") == 0
    || V_strcmp(classname, "tank_rock") == 0)
    {
        return false;
    }

    return true;
}

bool ClientPush(CBasePlayer *pPlayer, Vector vec)
{
    Vector vecVelocity = pPlayer->GetVelocity();
    vecVelocity += vec;

    if (WillHitWallOrFall(pPlayer, vecVelocity))
    {
        if (vecVelocity.Length() <= 250.0f)
        {
            VectorNormalize(vecVelocity);
            VectorScale(vecVelocity, 251.0f, vecVelocity);
        }

        pPlayer->Teleport(NULL, NULL, &vecVelocity);
        return true;
    }

    return false;
}

Vector UTIL_CaculateVel(const Vector& vecSelfPos, const Vector& vecTargetPos, vec_t flForce)
{
    Vector vecResult = vecTargetPos - vecSelfPos;
    VectorNormalize(vecResult);
    VectorScale(vecResult, flForce, vecResult);
    return vecResult;
}

// from sourcemod.
class CTraceFilterSimple2 : public CTraceFilterEntitiesOnly
{
public:
	CTraceFilterSimple2(const IHandleEntity *passentity): m_pPassEnt(passentity)
	{
	}
	virtual bool ShouldHitEntity(IHandleEntity *pServerEntity, int contentsMask)
	{
		if (pServerEntity == m_pPassEnt)
		{
			return false;
		}
		return true;
	}
private:
	const IHandleEntity *m_pPassEnt;
};

CBaseEntity *UTIL_GetClientAimTarget(CBaseEntity *pEntity, bool only_players)
{
	if (!pEntity)
		return NULL;

    int entIndex = pEntity->entindex();
    if (entIndex <= 0 || entIndex > gpGlobals->maxClients)
        return NULL;

    IGamePlayer *pGamePlayer = playerhelpers->GetGamePlayer(entIndex);
    if (!pGamePlayer || !pGamePlayer->IsInGame())
        return NULL;

    edict_t *pEdict = pGamePlayer->GetEdict();
    if (!pEdict)
        return NULL;

    Vector eye_position;
	QAngle eye_angles;

	/* Get the private information we need */
	serverClients->ClientEarPosition(pEdict, &eye_position);
    pEntity->GetEyeAngles(&eye_angles);

	Vector aim_dir;
	AngleVectors(eye_angles, &aim_dir);
	VectorNormalize(aim_dir);

	Vector vec_end = eye_position + aim_dir * 8000;

	Ray_t ray;
	ray.Init(eye_position, vec_end);

	trace_t tr;
    CTraceFilterSimple2 filter(pEdict->GetIServerEntity());
    enginetrace->TraceRay(ray, MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_HITBOX, &filter, &tr);
    //UTIL_TraceRay(ray, MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_HITBOX, pEdict->GetIServerEntity(), COLLISION_GROUP_NONE, &tr, NULL, NULL);
    if (tr.fraction == 1.0f || tr.m_pEnt == NULL)
        return NULL;

    int index = tr.m_pEnt->entindex();
	IGamePlayer *pTargetPlayer = playerhelpers->GetGamePlayer(index);
	if (pTargetPlayer != NULL && !pTargetPlayer->IsInGame())
	{
		return NULL;
	}
	else if (only_players && pTargetPlayer == NULL)
	{
		return NULL;
	}

	return tr.m_pEnt;
}

bool UTIL_IsLeftBehind(CTerrorPlayer *pPlayer)
{
    float flTeamDistacne = CalculateTeamDistance(pPlayer);

    CNavArea *pNav = pPlayer->GetLastKnownArea();
    if (!pNav)
        return false;

    float flPlayerDistance = pNav->GetFlow() / g_pNavMesh->GetMapMaxFlowDistance();

    if (flPlayerDistance > 0.0f && flPlayerDistance < 1.0f && flTeamDistacne != -1.0f)
    {
        return flPlayerDistance + z_smoker_left_behind_distance.GetFloat() < flTeamDistacne;
    }
    
    return false;
}

float CalculateTeamDistance(CTerrorPlayer *pIgnorePlayer)
{
    int counter = 0;
    float flTeamDistance = 0.0f;
    for (int i = 1; i <= gpGlobals->maxClients; i++)
    {
        CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(i);
        if (!pPlayer)
            continue;

        if (!pPlayer->IsInGame() || !pPlayer->IsSurvivor() || pPlayer->IsDead() || pPlayer->IsIncapped() || pPlayer->GetSpecialInfectedDominatingMe())
            continue;

        if (pPlayer == pIgnorePlayer)
            continue;

        CNavArea *pNav = pPlayer->GetLastKnownArea();
        if (!pNav)
            continue;

        float flPlayerFlow = pNav->GetFlow() / g_pNavMesh->GetMapMaxFlowDistance();
        if (flPlayerFlow > 0.0f && flPlayerFlow < 1.0f)
        {
            flTeamDistance += flPlayerFlow;
            counter += 1;
        }
    }

    if (counter > 1)
    {
        flTeamDistance /= counter;
    }
    else
    {
        flTeamDistance = -1.0f;
    }

    return flTeamDistance;
}

bool PassServerEntityFilter( const IHandleEntity *pTouch, const IHandleEntity *pPass )
{
	if ( !pPass )
		return true;

	if ( pTouch == pPass )
		return false;

	const CBaseEntity *pEntTouch = (CBaseEntity *)EntityFromEntityHandle( pTouch );
	const CBaseEntity *pEntPass = (CBaseEntity *)EntityFromEntityHandle( pPass );
	if ( !pEntTouch || !pEntPass )
		return true;

	// don't clip against own missiles
    // what the hell are the consts doing here?
	if ( (CBaseEntity *)(const_cast<CBaseEntity *>(pEntTouch)->GetOwnerEntity()) == pEntPass )
		return false;
	
	// don't clip against owner
	if ( (CBaseEntity *)(const_cast<CBaseEntity *>(pEntPass)->GetOwnerEntity()) == pEntTouch )
		return false;	

	return true;
}

bool DoBhop(CBasePlayer *pPlayer, int buttons, Vector vec)
{
    if (buttons & IN_FORWARD || buttons & IN_MOVELEFT || buttons & IN_MOVERIGHT)
        return ClientPush(pPlayer, vec);

    return false;
}