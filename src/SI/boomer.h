#ifndef _BOMMER_H_INCLUDED_
#define _BOMMER_H_INCLUDED_

#pragma once

#include <vector>

#include "convar.h"
#include "wrappers.h"

#define TURN_ANGLE_DIVIDE 3.0
#define EYE_ANGLE_UP_HEIGHT 15.0
#define COMMAND_INTERVAL 1.0

extern ConVar z_boomer_bhop;
extern ConVar z_boomer_bhop_speed;
extern ConVar z_boomer_vision_up_on_vomit;
extern ConVar z_boomer_vision_spin_on_vomit;
extern ConVar z_boomer_force_bile;
extern ConVar z_boomer_bile_find_range;
extern ConVar z_boomer_spin_interval;
extern ConVar z_boomer_degree_force_bile;
extern ConVar z_boomer_predict_pos;

class CBoomerEventListner : public IGameEventListener2 {
public:
    virtual void FireGameEvent(IGameEvent *event);
    virtual int	 GetEventDebugID( void );
    void OnPlayerSpawned(IGameEvent *event);
    void OnPlayerShoved(IGameEvent *event);
    void OnPlayerNowIt(IGameEvent *event);
    void OnRoundStart(IGameEvent *event);
};

struct targetInfo_t {
    vec_t flAngle;
    CTerrorPlayer *pPlayer;
};

class CBoomer : public CTerrorPlayer {
public:
    bool m_bCanBile;
    bool m_bIsInCoolDown;
    bool m_bBiling;
    std::vector<targetInfo_t> m_aTargetInfo;
    int m_nBileFrame[2];
};

class CTerrorBoomerVictim : public CTerrorPlayer {
public:
    bool m_bBiled;
    int m_iSecondCheckFrame;
};

class CBoomerTimerEvent : public ITimedEvent {
public:
	virtual ResultType OnTimer(ITimer *pTimer, void *pData);
	virtual void OnTimerEnd(ITimer *pTimer, void *pData);
};

class CBoomerEntityListner {
public:
    void OnPostThink(CBaseEntity *pPlayer);
};

static bool secondCheck(CBaseEntity *pPlayer, CBaseEntity *pTarget);
static bool TR_VomitClientFilter(IHandleEntity *pHandleEntity, int contentsMask, void *data);
static bool DoBhop(CBasePlayer *pPlayer, int buttons, Vector vec);

#endif // _BOMMER_H_INCLUDED_