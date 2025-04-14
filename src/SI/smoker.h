#ifndef _SMOKER_H_INCLUDED_
#define _SMOKER_H_INCLUDED_

#include "convar.h"
#include "wrappers.h"

#define SMOKER_MELEE_RANGE 300
#define SMOKER_ATTACK_COORDINATE 5.0

extern ConVar z_smoker_bhop;
extern ConVar z_smoker_bhop_speed;
extern ConVar z_smoker_target_rules;
extern ConVar z_smoker_melee_avoid;
extern ConVar z_smoker_left_behind_distance;
extern ConVar z_smoker_left_behind_time;
extern ConVar z_smoker_instant_shoot_range_cofficient;

class CSmokerEventListner : public IGameEventListener2 {
public:
    virtual void FireGameEvent(IGameEvent *event);
    virtual int GetEventDebugID(void);
};

class CSmokerTimerEvent : public ITimedEvent {
public:
    virtual ResultType OnTimer(ITimer *pTimer, void *pData);
    virtual void OnTimerEnd(ITimer *pTimer, void *pData);
};

class CSmoker : public CTerrorPlayer {
public:
    bool m_bCanTongue;
};

class CTerrorSmokerVictim : public CTerrorPlayer {
public:
    bool m_bLeftBehind;
};

class CSmokerCmdListner {
public:
    void OnPlayerRunCmd(CBaseEntity *pEntity, CUserCmd *pCmd);
};

static CTerrorPlayer *SmokerTargetChoose(int method, CTerrorPlayer *pSmoker, CTerrorPlayer *pSpecificTarget = NULL);
static bool TR_RayFilterBySmoker(IHandleEntity *pHandleEntity, int contentsMask, void *data);
static int TeamMeleeCheck();

#endif // _SMOKER_H_INCLUDED_