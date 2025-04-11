#ifndef _UTILS_H_INCLUDED_
#define _UTILS_H_INCLUDED_

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

CBasePlayer* UTIL_PlayerByIndex(int playerIndex);
CBasePlayer* UTIL_PlayerByUserId(int userID);
CBasePlayer* UTIL_GetClosetSurvivor(CBasePlayer* pPlayer, CBasePlayer* pIgnorePlayer = NULL, bool bCheckIncapp = false, bool bCheckDominated = false);
Vector UTIL_MakeVectorFromPoints(Vector src1, Vector src2);
void UTIL_ComputeAimAngles(CBasePlayer* pPlayer, CBasePlayer* pTarget, QAngle* angles, AimType type = AimEye);
vec_t GetSelfTargetAngle(CBasePlayer* pAttacker, CBasePlayer* pTarget);
bool UTIL_IsInAimOffset(CBasePlayer* pAttacker, CBasePlayer* pTarget, float offset);
bool TR_EntityFilter(IHandleEntity *ignore, int contentsMask);
// false means will, true otherwise.
bool WillHitWallOrFall(CBasePlayer* pPlayer, Vector vec);
bool ClientPush(CBasePlayer* pPlayer, Vector vec);
Vector UTIL_CaculateVel(const Vector& vecSelfPos, const Vector& vecTargetPos, vec_t flForce);
// from sourcemod.
CBaseEntity *UTIL_GetClientAimTarget(CBaseEntity *pEntity, bool only_players);
bool UTIL_IsLeftBehind(CTerrorPlayer *pPlayer);
static float CalculateTeamDistance(CTerrorPlayer *pIgnorePlayer = NULL);
inline const CBaseEntity *EntityFromEntityHandle(const IHandleEntity *pConstHandleEntity);
inline CBaseEntity *EntityFromEntityHandle(IHandleEntity *pHandleEntity);
bool PassServerEntityFilter(const IHandleEntity *pTouch, const IHandleEntity *pPass);

static bool (__cdecl *pFnIsVisibleToPlayer)(const Vector&, CBasePlayer *, int, int, float, const IHandleEntity *, void *, bool *);

// int __usercall IsVisibleToPlayer@<eax>(long double@<st0>, float *, CBaseEntity *, int, char, int, const IHandleEntity *, CNavArea **, _BYTE *)
// bool IsVisibleToPlayer(const Vector vecTargetPos, CBasePlayer *pPlayer, int team, int team_target, float fl, const IHandleEntity *pIgnore, CNavArea *pArea, bool *b)
inline bool IsVisiableToPlayer(const Vector &vecTargetPos, CBasePlayer *pPlayer, int team, int team_target, float flUnknow, const IHandleEntity *pIgnore, void *pArea, bool *bUnknown);
inline float FloatAbs(float f);

inline void UTIL_TraceRay(const Ray_t &ray, unsigned int mask,
                          IHandleEntity *ignore, Collision_Group_t collisionGroup, ShouldHitFunc_t pExtraShouldHitCheckFn = NULL, trace_t *ptr);

inline void UTIL_TraceRay(const Ray_t &ray, unsigned int mask,
                          IHandleEntity *ignore, Collision_Group_t collisionGroup, trace_t *ptr, ShouldHitFunc2_t pExtraShouldHitCheckFn = NULL, void *data = NULL);

inline void UTIL_TraceHull(const Vector &vecAbsStart, const Vector &vecAbsEnd,
                           const Vector &hullMin, const Vector &hullMax, unsigned int mask,
                           IHandleEntity *ignore, Collision_Group_t collisionGroup, trace_t *ptr, ShouldHitFunc_t pExtraShouldHitCheckFn = NULL);

#endif // _UTILS_H_INCLUDED_
