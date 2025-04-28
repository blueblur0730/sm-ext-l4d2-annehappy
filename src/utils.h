#ifndef _UTILS_H_INCLUDED_
#define _UTILS_H_INCLUDED_

#pragma once

#include "extension.h"
#include "iplayerinfo.h"
#include "engine/IStaticPropMgr.h"
#include "edict.h"
#include "mathlib.h"
#include <vector>
#include <algorithm>
#include <stdio.h>
#include "wrappers.h"

#include "SI/smoker.h"

typedef bool (__cdecl *Fn_IsVisibleToPlayer)(const Vector&, CBasePlayer *, int, int, float, const IHandleEntity *, void *, bool *);
extern Fn_IsVisibleToPlayer pFnIsVisibleToPlayer;

#define FALL_DETECT_HEIGHT 120.0

enum AimType {
	AimEye,
	AimBody,
	AimChest
};

struct utils_t {
    vec_t dist;
    int index;
};

enum L4D2Gender
{
	L4D2Gender_Neutral			= 0,
	L4D2Gender_Male				= 1,
	L4D2Gender_Female			= 2,
	L4D2Gender_Nanvet			= 3, //Bill
	L4D2Gender_TeenGirl			= 4, //Zoey
	L4D2Gender_Biker			= 5, //Francis
	L4D2Gender_Manager			= 6, //Louis
	L4D2Gender_Gambler			= 7, //Nick
	L4D2Gender_Producer			= 8, //Rochelle
	L4D2Gender_Coach			= 9, //Coach
	L4D2Gender_Mechanic			= 10, //Ellis
	L4D2Gender_Ceda				= 11,
	L4D2Gender_Crawler			= 12, //Mudman
	L4D2Gender_Undistractable	= 13, //Workman
	L4D2Gender_Fallen			= 14,
	L4D2Gender_Riot_Control		= 15, //RiotCop
	L4D2Gender_Clown			= 16,
	L4D2Gender_Jimmy			= 17, //JimmyGibbs
	L4D2Gender_Hospital_Patient	= 18,
	L4D2Gender_Witch_Bride		= 19,
	L4D2Gender_Police			= 20, //l4d1 RiotCop (was removed from the game)
	L4D2Gender_Male_L4D1		= 21,
	L4D2Gender_Female_L4D1		= 22,
	
	L4D2Gender_MaxSize //23 size
};

const int g_iGetUpAnimationsSequence[8][4] =
{
	{528, 759, 763, 764},	// 比尔
	{537, 819, 823, 824},	// 佐伊
	{528, 759, 763, 764},	// 路易斯
	{531, 762, 766, 767},	// 弗朗西斯
	{620, 667, 671, 672},	// 西装
	{629, 674, 678, 679},	// 女人
	{621, 656, 660, 661},	// 黑胖
	{625, 671, 675, 676},	// 帽子
};

CBasePlayer* UTIL_PlayerByIndexExt(int playerIndex);

CBasePlayer* UTIL_PlayerByUserIdExt(int userID);

CBasePlayer* UTIL_GetClosetSurvivor(CBasePlayer* pPlayer, CBasePlayer* pIgnorePlayer = NULL, bool bCheckIncapp = false, bool bCheckDominated = false);

vec_t UTIL_GetClosetSurvivorDistance(CBasePlayer *pPlayer, CBasePlayer *pIgnorePlayer = NULL, bool bCheckIncapp = false, bool bCheckDominated = false);

Vector UTIL_MakeVectorFromPoints(Vector src1, Vector src2);

void UTIL_ComputeAimAngles(CBasePlayer* pPlayer, CBasePlayer* pTarget, QAngle *angles, AimType type = AimEye);

vec_t GetSelfTargetAngle(CBasePlayer* pAttacker, CBasePlayer* pTarget);

bool UTIL_IsInAimOffset(CBasePlayer* pAttacker, CBasePlayer* pTarget, float offset);

static bool TR_EntityFilter(IHandleEntity *ignore, int contentsMask, void *data);

// false means will, true otherwise.
static bool WillHitWallOrFall(CBasePlayer* pPlayer, Vector vec);

bool ClientPush(CBasePlayer* pPlayer, Vector vec);

Vector UTIL_CaculateVel(const Vector& vecSelfPos, const Vector& vecTargetPos, vec_t flForce);

// from sourcemod.
CBaseEntity *UTIL_GetClientAimTarget(CBaseEntity *pEntity, bool only_players);

bool UTIL_IsLeftBehind(CTerrorPlayer *pPlayer);

float CalculateTeamDistance(CTerrorPlayer *pIgnorePlayer = NULL);

bool PassServerEntityFilter(const IHandleEntity *pTouch, const IHandleEntity *pPass);

bool DoBhop(CBasePlayer *pPlayer, int buttons, Vector vec);

bool UTIL_IsInGetUpAnimation(CBasePlayer *pPlayer);

inline const CBaseEntity *EntityFromEntityHandle( const IHandleEntity *pConstHandleEntity )
{
	IHandleEntity *pHandleEntity = const_cast<IHandleEntity*>(pConstHandleEntity);

#ifdef CLIENT_DLL
	IClientUnknown *pUnk = (IClientUnknown*)pHandleEntity;
	return pUnk->GetBaseEntity();
#else
	if ( staticpropmgr->IsStaticProp( pHandleEntity ) )
		return NULL;

	IServerUnknown *pUnk = (IServerUnknown*)pHandleEntity;
	return pUnk->GetBaseEntity();
#endif
}

inline CBaseEntity *EntityFromEntityHandle( IHandleEntity *pHandleEntity )
{
#ifdef CLIENT_DLL
	IClientUnknown *pUnk = (IClientUnknown*)pHandleEntity;
	return pUnk->GetBaseEntity();
#else
	if ( staticpropmgr->IsStaticProp( pHandleEntity ) )
		return NULL;

	IServerUnknown *pUnk = (IServerUnknown*)pHandleEntity;
	return pUnk->GetBaseEntity();
#endif
}

inline float FloatAbs(float f)
{
	return f > 0 ? f : -f;
}

// int __usercall IsVisibleToPlayer@<eax>(long double@<st0>, float *, CBaseEntity *, int, char, int, const IHandleEntity *, CNavArea **, _BYTE *)
// bool IsVisibleToPlayer(const Vector vecTargetPos, CBasePlayer *pPlayer, int team, int team_target, float fl, const IHandleEntity *pIgnore, CNavArea *pArea, bool *b)
inline bool IsVisiableToPlayer(const Vector &vecTargetPos, CBasePlayer *pPlayer, int team, int team_target, float flUnknow, const IHandleEntity *pIgnore, void *pArea, bool *bUnknown)
{
    return pFnIsVisibleToPlayer(vecTargetPos, pPlayer, team, team_target, flUnknow, pIgnore, pArea, bUnknown);
}

inline void UTIL_TraceRay(  const Ray_t &ray, unsigned int mask, 
    IHandleEntity *ignore, Collision_Group_t collisionGroup, trace_t *tr, ShouldHitFunc_t pExtraShouldHitCheckFn= NULL, void *data = NULL )
{
   CTraceFilterSimpleExt traceFilter(ignore, collisionGroup, pExtraShouldHitCheckFn, data);
   enginetrace->TraceRay( ray, mask, &traceFilter, tr );
}

inline void UTIL_TraceHull( const Vector& vecAbsStart, const Vector& vecAbsEnd, 
    const Vector& hullMin, const Vector& hullMax, unsigned int mask, 
    IHandleEntity* ignore, Collision_Group_t collisionGroup, trace_t* tr, ShouldHitFunc_t pExtraShouldHitCheckFn = NULL, void *data = NULL )
{
   Ray_t ray;
   ray.Init(vecAbsStart, vecAbsEnd, hullMin, hullMax);
   CTraceFilterSimpleExt traceFilter(ignore, collisionGroup, pExtraShouldHitCheckFn, data);

   enginetrace->TraceRay(ray, mask, &traceFilter, tr);
}
#endif // _UTILS_H_INCLUDED_
