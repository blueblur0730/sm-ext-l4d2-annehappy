#include <vector>

#include "convar.h"
#include "wrappers.h"
#include "eiface.h"

ConVar z_boomer_bhop("z_boomer_bhop", "1", FCVAR_NOTIFY, "Enable boomer bhop.", true, 0.0f, true, 1.0f);
ConVar z_boomer_bhop_speed("z_boomer_bhop_speed", "150.0", FCVAR_NOTIFY, "Boomer bhop speed.", true, 0.0f, false, 0.0f);
ConVar z_boomer_vision_up_on_vomit("z_boomer_vision_up_on_vomit", "1", FCVAR_NOTIFY, "Boomer vision will turn up when vomitting.", true, 0.0f, true, 1.0f);
ConVar z_boomer_vision_spin_on_vomit("z_boomer_vision_spin_on_vomit", "1", FCVAR_NOTIFY, "Boomer vision will spin when vomitting.", true, 0.0f, true, 1.0f);
ConVar z_boomer_force_bile("z_boomer_force_bile", "1", FCVAR_NOTIFY, "Boomer will force to vomit when survivors are in the range of vomitting.", true, 0.0f, true, 1.0f);
ConVar z_boomer_bile_find_range("z_boomer_bile_find_range", "300.0", FCVAR_NOTIFY, "Boomer will vomit to the incapacited survivors in this range first. 0 = disabled.", true, 0.0f, false, 0.0f);
ConVar z_boomer_spin_interval("z_boomer_spin_interval", "15", FCVAR_NOTIFY, "Boomer will spin to another survivors in every this many frames.", true, 0.0f, false, 0.0f);
ConVar z_boomer_degree_force_bile("z_boomer_degree_force_bile", "10", FCVAR_NOTIFY, "Boomer will force to vomit to the survivors whoes pos with boomer's eye is on this degree, 0 = disable.", true, 0.0f, false, 0.0f);
ConVar z_boomer_predict_pos("z_boomer_predict_pos", "1", FCVAR_NOTIFY, "Boomer will predict the frame of the next target's vision according to the target's angle.", true, 0.0f, true, 1.0f);

class CBoomerEventListner : public IGameEventListener2 {
public:
    virtual void FireGameEvent(IGameEvent *event);
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
    void OnPostThink(CBaseEntity *pEntity);
};

CBoomerEntityListner *g_BoomerEntityListner = NULL;
CBoomerEventListner g_BoomerEventListner;
CBoomerTimerEvent g_BoomerTimerEvent;