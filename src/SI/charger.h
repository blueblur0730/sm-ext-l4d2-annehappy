#ifndef _CHARGER_H_INCLUDED_
#define _CHARGER_H_INCLUDED_

#include <unordered_map>

#include "convar.h"
#include "wrappers.h"

extern ConVar z_charger_bhop;
extern ConVar z_charger_bhop_speed;
extern ConVar z_charger_min_charge_distance;
extern ConVar z_charger_extra_target_range_min;
extern ConVar z_charger_extra_target_range_max;
extern ConVar z_charger_target_aim_offset;
extern ConVar z_charger_melee_avoid;
extern ConVar z_charger_avoid_melee_target_hp;
extern ConVar z_charger_target_rules;

class CChargerEventListner : public IGameEventListener2 {
public:
    virtual void FireGameEvent(IGameEvent *event);
    virtual int GetEventDebugID(void);
        
public:
    void OnClientPutInServer(int client);
    void OnClientDisconnecting(int client);
    
public:
    void OnPlayerRunCmd(CBaseEntity *pEntity, CUserCmd *pCmd);
};

class CCharge : public CBaseAbility {
public:
    static int m_iOff_m_isCharging;

public:
    inline bool IsCharging()
    {
        return *(bool*)((byte*)(this) + m_iOff_m_isCharging);
    }
};

struct chargerInfo_t {
    float m_flChargeInterval;
    bool m_bIsCharging;
    bool m_bCanAttackPinnedTarget;
    int m_iRangedClientIndex[MAX_PLAYERS];
    int m_iRangedIndex;

    void Init() {
        m_flChargeInterval = 0.0f;
        m_bIsCharging = false;
        m_bCanAttackPinnedTarget = false;
        m_iRangedIndex = -1;
        
        for (int i : m_iRangedClientIndex)
        {
            m_iRangedClientIndex[i] = -1;
        }
    }
};

static bool IsInChargeDuration(CTerrorPlayer *pCharger);
static void SetChargeTimer(CCharge *pAbility, float flDuration = 0.0f);
static bool CheckMelee(CTerrorPlayer *pCharger);

#endif // _CHARGER_H_INCLUDED_