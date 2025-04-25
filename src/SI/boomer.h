#ifndef _BOMMER_H_INCLUDED_
#define _BOMMER_H_INCLUDED_

#pragma once

#include <vector>
#include <unordered_map>

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
    virtual int GetEventDebugID(void);
    
public:
    void OnClientPutInServer(int client);
    void OnClientDisconnecting(int client);

public:
    void OnPlayerRunCmd(CBaseEntity *pEntity, CUserCmd *pCmd);

public:
    bool CreateDetour(char *error, size_t maxlen);
};

struct targetInfo_t {
    vec_t flAngle;
    CTerrorPlayer *pPlayer;
};

struct boomerInfo_t {
    bool m_bCanBile;
    bool m_bIsInCoolDown;
    bool m_bBiling;
    std::vector<targetInfo_t> m_aTargetInfo;
    int m_nBileFrame[2];

    void Init() {
        m_bCanBile = false;
        m_bIsInCoolDown = false;
        m_aTargetInfo.clear();
        m_nBileFrame[0] = m_nBileFrame[1] = 0;
    }
};

struct boomerVictimInfo_t {
    bool m_bBiled;
    int m_iSecondCheckFrame;

    void Init() {
        m_bBiled = false;
        m_iSecondCheckFrame = 0;
    }
};

class CVomit : public CBaseAbility {
public:
	static int m_iOff_m_isSpraying;

public:
	inline bool IsSpraying()
	{
		return *(bool*)((byte*)(this) + CVomit::m_iOff_m_isSpraying);
	}
};

class CBoomerTimerEvent : public ITimedEvent {
public:
	virtual ResultType OnTimer(ITimer *pTimer, void *pData);
	virtual void OnTimerEnd(ITimer *pTimer, void *pData);
};

static bool secondCheck(CBaseEntity *pPlayer, CBaseEntity *pTarget);
static bool TR_VomitClientFilter(IHandleEntity *pHandleEntity, int contentsMask, void *data);

#endif // _BOMMER_H_INCLUDED_