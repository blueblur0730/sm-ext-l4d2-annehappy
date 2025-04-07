#include "extension.h"
#include "iplayerinfo.h"
#include "edict.h"
#include "mathlib.h"
#include <vector>
#include <algorithm>
#include <stdio.h>
#include "wrappers.h"

enum AimType {
	AimEye,
	AimBody,
	AimChest
};

inline float FloatAbs(float f)
{
	return f > 0 ? f : -f;
}

CBasePlayer* UTIL_PlayerByIndex(int playerIndex)
{
	if (playerIndex > 0 && playerIndex <= playerhelpers->GetMaxClients()) 
	{
		IGamePlayer* pPlayer = playerhelpers->GetGamePlayer(playerIndex);
		if (pPlayer != NULL) 
			return (CBasePlayer*)gameents->EdictToBaseEntity(pPlayer->GetEdict());
	}

	return NULL;
}

CBasePlayer* UTIL_PlayerByUserId(int userID)
{
	for (int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex(i);

		if (!pPlayer)
			continue;

		if (playerhelpers->GetGamePlayer(i)->GetUserId() == userID)
			return pPlayer;
	}

	return NULL;
}

CBasePlayer* UTIL_GetClosetSurvivor(CBasePlayer* pPlayer, CBasePlayer* pIgnorePlayer)
{
    int index = -1;
    index = pPlayer->entindex();
    if (index <= 0 || index > gpGlobals->maxClients)
        return NULL;

    IPlayerInfo *playerinfo = ((CTerrorPlayer *)pPlayer)->GetPlayerInfo();
    if (!playerinfo)
        return NULL;

    struct utils_t
    {
        vec_t dist;
        int index;
    };
    
    Vector vecOrigin = playerinfo->GetAbsOrigin();
    std::vector<utils_t> aTargetList;
    for (int i = 1; i <= gpGlobals->maxClients; i++)
    {
        CTerrorPlayer *pIterPlayer = (CTerrorPlayer *)UTIL_PlayerByIndex(i);
        if (!pIterPlayer)
            continue;

        IPlayerInfo *pIterPlayerInfo = pIterPlayer->GetPlayerInfo();
        if (!pIterPlayerInfo)
            continue;

        if (!pIterPlayer->IsInGame() || 
            pIterPlayer->IsDead() || 
            !pIterPlayer->IsSurvivor() ||
            i == pIgnorePlayer->entindex())
            continue;

        aTargetList[i].dist = vecOrigin.DistTo(pIterPlayerInfo->GetAbsOrigin());
        aTargetList[i].index = i;
    }

    if (!aTargetList.size())
        return NULL;

    // sorting ascending.
    std::sort(aTargetList.begin(), aTargetList.end());

    int closetIndex = aTargetList[0].index;
    aTargetList.clear();
    return (CBasePlayer *)UTIL_PlayerByIndex(closetIndex);
}

void UTIL_ComputeAimAngles(CBasePlayer* pPlayer, CBasePlayer* pTarget, QAngle* angles, AimType type = AimEye)
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
        }
    }

    Vector vecSelfEyePos = ((CTerrorPlayer *)pPlayer)->GetEyeOrigin();
    Vector vecLookAt = UTIL_MakeVectorFromPoints(vecSelfEyePos, vecTargetPos);

    VectorAngles(vecLookAt, *angles);
}

Vector UTIL_MakeVectorFromPoints(Vector src1, Vector src2)
{
    Vector output;
    output.x = src2.x - src1.x;
    output.y = src2.y - src1.y;
    output.z = src2.z - src1.z;
    return output;
}

bool UTIL_IsInAimOffset(CBasePlayer* pAttacker, CBasePlayer* pTarget, float offset)
{
    vec_t angle = GetSelfTargetAngle(pAttacker, pTarget);
    return (angle != 1.0f && angle <= offset);
}

vec_t GetSelfTargetAngle(CBasePlayer* pAttacker, CBasePlayer* pTarget)
{
    Vector vecSelfEyePos = ((CTerrorPlayer *)pAttacker)->GetEyeOrigin();
    Vector vecTargetEyePos = ((CTerrorPlayer *)pTarget)->GetEyeOrigin();

    vecSelfEyePos.z = vecTargetEyePos.z = 0.0f;
    Vector vecResult = UTIL_MakeVectorFromPoints(vecSelfEyePos, vecTargetEyePos);
    VectorNormalize(vecResult);

    QAngle vecAngle;
    ((CBaseEntity *)pAttacker)->GetEyeAngles(&vecAngle);
    vecAngle.x = vecAngle.z = 0.0f;

    Vector vecSelfEyeVector;
    AngleVectors(vecAngle, &vecSelfEyeVector, NULL, NULL);
    VectorNormalize(vecSelfEyeVector);

    return (acos(vecSelfEyeVector.Dot(vecResult)) * 180.0 / M_PI);
}

Vector UTIL_CaculateVel(const Vector& vecSelfPos, const Vector& vecTargetPos, vec_t flForce)
{
    Vector vecResult = vecTargetPos - vecSelfPos;
    VectorNormalize(vecResult);
    VectorScale(vecResult, flForce, vecResult);
    return vecResult;
}

inline void UTIL_TraceRay( const Ray_t &ray, unsigned int mask, 
						   IHandleEntity *ignore, Collision_Group_t collisionGroup, trace_t *ptr, ShouldHitFunc_t pExtraShouldHitCheckFn = NULL )
{
	CTraceFilterSimple traceFilter(ignore, collisionGroup, pExtraShouldHitCheckFn);
	enginetrace->TraceRay( ray, mask, &traceFilter, ptr );
}

inline void UTIL_TraceRay(  const Ray_t &ray, unsigned int mask, 
                            IHandleEntity *ignore, Collision_Group_t collisionGroup, trace_t *ptr, ShouldHitFunc2_t pExtraShouldHitCheckFn = NULL, void *data = NULL )
{
    CTraceFilterSimple traceFilter(ignore, collisionGroup, pExtraShouldHitCheckFn, data);
    enginetrace->TraceRay( ray, mask, &traceFilter, ptr );
}

inline void UTIL_TraceHull( const Vector& vecAbsStart, const Vector& vecAbsEnd, 
                            const Vector& hullMin, const Vector& hullMax, unsigned int mask, 
                            IHandleEntity* ignore, Collision_Group_t collisionGroup, trace_t* ptr, ShouldHitFunc_t pExtraShouldHitCheckFn = NULL)
{
	Ray_t ray;
	ray.Init(vecAbsStart, vecAbsEnd, hullMin, hullMax);
	CTraceFilterSimple traceFilter(ignore, collisionGroup, pExtraShouldHitCheckFn);

	enginetrace->TraceRay(ray, mask, &traceFilter, ptr);
}

static bool (__cdecl *pFnIsVisibleToPlayer)(const Vector&, CBasePlayer *, int, int, float, const IHandleEntity *, void *, bool *);

// int __usercall IsVisibleToPlayer@<eax>(long double@<st0>, float *, CBaseEntity *, int, char, int, const IHandleEntity *, CNavArea **, _BYTE *)
// bool IsVisibleToPlayer(const Vector vecTargetPos, CBasePlayer *pPlayer, int team, int team_target, float fl, const IHandleEntity *pIgnore, CNavArea *pArea, bool *b)
inline bool IsVisiableToPlayer(const Vector &vecTargetPos, CBasePlayer *pPlayer, int team, int team_target, float flUnknow, const IHandleEntity *pIgnore, void *pArea, bool *bUnknown)
{
    return pFnIsVisibleToPlayer(vecTargetPos, pPlayer, team, team_target, flUnknow, pIgnore, pArea, bUnknown);
}