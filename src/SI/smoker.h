#include "convar.h"
#include "wrappers.h"

#define SMOKER_MELEE_RANGE 300
#define SMOKER_ATTACK_COORDINATE 5.0

ConVar z_smoker_bhop("z_smoker_bhop", "1", FCVAR_NOTIFY | FCVAR_CHEAT, "Enable bhop for smoker", true, 0.0f, true, 1.0f);
ConVar z_smoker_bhop_speed("z_smoker_bhop_speed", "90.0", FCVAR_NOTIFY | FCVAR_CHEAT, "Bhop speed for smoker", true, 0.0f, false, 0.0f);
ConVar z_smoker_target_rules("z_smoker_target_rules", "1", FCVAR_NOTIFY | FCVAR_CHEAT,
	                        "The priorior target choosing rules for smoker.\
                             0 - no rules\
                            , 1 - the closest\
                            , 2 - have a shotgun\
                            , 3 - have the highest or the lowest flow distance\
                            , 4 - the one who is reloading. \
                            1 will be used if the condition of 2-4 is not met.", true, 0.0f, true, 4.0f);
ConVar z_smoker_melee_avoid("z_smoker_melee_avoid", "1", FCVAR_NOTIFY | FCVAR_CHEAT, "Re-choosing a target if the cureent target's got a melee weapon choosen.", true, 0.0f, true, 1.0f);
ConVar z_smoker_left_behind_distance("z_smoker_left_behind_distance", "7.0", FCVAR_NOTIFY | FCVAR_CHEAT, "Survivor whoes flow distance is beyond or lower than this range will be seen as lonely one.", true, 0.0f, false, 0.0f);
ConVar z_smoker_left_behind_time("z_smoker_left_behind_time", "5.0", FCVAR_NOTIFY | FCVAR_CHEAT, "The time to wait before choosing a new target.", true, 0.0f, false, 0.0f);
ConVar z_smoker_instant_shoot_range_cofficient("z_smoker_instant_shoot_range_cofficient", "0.8", FCVAR_NOTIFY | FCVAR_CHEAT, "Smoker will instantly shoot its tongue if the target is in this range (tongue distance * this cofficient).", true, 0.0f, true, 1.0f);

class CSmokerEventListner : public IGameEventListener2 {
public:
    virtual void FireGameEvent(IGameEvent *event);
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

class CSmokerEntityListner {
public:
    void OnPostThink(CBaseEntity *pEntity);
};