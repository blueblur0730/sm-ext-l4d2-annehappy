#include "SI/charger.h"
#include "utils.h"
#include "in_buttons.h"

ConVar z_charger_bhop("z_charger_bhop", "1", FCVAR_NOTIFY | FCVAR_CHEAT, "Enable bhop for charger.", true, 0.0f, true, 1.0f);
ConVar z_charger_bhop_speed("z_charger_bhop_speed", "90.0", FCVAR_NOTIFY | FCVAR_CHEAT, "Bhop speed for charger.", true, 0.0f, false, 0.0f);
ConVar z_charger_min_charge_distance("z_charger_min_charge_distance", "250.0", FCVAR_NOTIFY | FCVAR_CHEAT, "Minimum distance to target to charge.", true, 0.0f, false, 0.0f);
ConVar z_charger_extra_target_range_min("z_charger_extra_target_range_min", "0", FCVAR_NOTIFY | FCVAR_CHEAT, "Minimum distance to find an another target.", true, 0.0f, false, 0.0f);
ConVar z_charger_extra_target_range_max("z_charger_extra_target_range_max", "350", FCVAR_NOTIFY | FCVAR_CHEAT, "Maximum distance to find an another target.", true, 0.0f, false, 0.0f);
ConVar z_charger_target_aim_offset("z_charger_target_aim_offset", "30.0", FCVAR_NOTIFY | FCVAR_CHEAT, "Charger will not charge when the target's aim offset is in this range with charger.", true, 0.0f, false, 0.0f);
ConVar z_charger_melee_avoid("z_charger_melee_avoid", "1", FCVAR_NOTIFY | FCVAR_CHEAT, "Enable melee avoid.", true, 0.0f, true, 1.0f);
ConVar z_charger_avoid_melee_target_hp("z_charger_avoid_melee_target_hp", "350", FCVAR_NOTIFY | FCVAR_CHEAT, "Avoid this target if the cureent target's got a melee weapon choosen and its hp is lower than this value.", true, 0.0f, false, 0.0f);
ConVar z_charger_target_rules("z_charger_target_rules", "1", FCVAR_NOTIFY | FCVAR_CHEAT,
                                "The priorior target choosing rules for charger.\
                                 0 - no rules\
                                , 1 - naturally choose\
                                , 2 - the closet\
                                , 3 - the direction that can crumble a bunch of survivors\
                                1 will be used if the condition of 2-3 is not met.", true, 0.0f, true, 3.0f);


std::unordered_map<int, chargerInfo_t> g_ChargerInfoMap;

void CChargerEventListner::FireGameEvent(IGameEvent *event)
{
    const char *eventName = event->GetName();
    if (!eventName)
        return;

    if (V_strcmp(eventName, "player_spawn") == 0)
    {
        CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByUserIdExt(event->GetInt("userid"));
        if (!pPlayer || !pPlayer->IsInGame() || !pPlayer->IsCharger())
            return;

        int index = pPlayer->entindex();
        if (index <= 0 || index > gpGlobals->maxClients)
            return;

        g_ChargerInfoMap[index].m_flChargeInterval = 0.0f - g_pCVar->FindVar("z_charge_interval")->GetFloat();
        g_ChargerInfoMap[index].m_bIsCharging = false;
    }
}

int CChargerEventListner::GetEventDebugID()
{
    return EVENT_DEBUG_ID_INIT;
}

void CChargerEventListner::OnClientPutInServer(int client)
{
    CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(client);
    if (!pPlayer || !pPlayer->IsInGame())
        return;

    if (pPlayer->IsCharger())
    {
        chargerInfo_t info;
        info.Init();
        g_ChargerInfoMap[client] = info;
    }
}

void CChargerEventListner::OnClientDisconnecting(int client)
{
    CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(client);
    if (!pPlayer || !pPlayer->IsInGame())
        return;

    if (pPlayer->IsCharger())
    {
        if (g_ChargerInfoMap.contains(client))
        {
            g_ChargerInfoMap[client].Init();
            g_ChargerInfoMap.erase(client);
        }
    }
}

void CChargerEventListner::OnPlayerRunCmd(CBaseEntity *pEntity, CUserCmd *pCmd)
{
    if (!pCmd)
        return;

    CTerrorPlayer *pCharger = reinterpret_cast<CTerrorPlayer *>(pEntity);
    if (!pCharger || !pCharger->IsInGame() || !pCharger->IsCharger() || pCharger->IsDead())
        return;

    int chargerIndex = pCharger->entindex();
    if (chargerIndex <= 0 || chargerIndex > gpGlobals->maxClients)
        return;

    if (!g_ChargerInfoMap.contains(chargerIndex) && pCharger->IsCharger())
    {
        chargerInfo_t info;
        info.Init();
        g_ChargerInfoMap[chargerIndex] = info;
    }

    if (pCharger->GetPummelVictim() || pCharger->GetCarryVictim())
        return;

    if (pCharger->IsStaggering())
        return;

    CCharge *pCharge = (CCharge *)pCharger->GetAbility();
    if (pCharge)
    {
        // 记录每次冲锋结束的时间戳
        bool bIsCharging = pCharge->IsCharging();
        if (bIsCharging && !g_ChargerInfoMap[chargerIndex].m_bIsCharging)
        {
            g_ChargerInfoMap[chargerIndex].m_bIsCharging = true;        
        }
        else if (!bIsCharging && g_ChargerInfoMap[chargerIndex].m_bIsCharging)
        {
            g_ChargerInfoMap[chargerIndex].m_flChargeInterval = gpGlobals->curtime;
            g_ChargerInfoMap[chargerIndex].m_bIsCharging = false;
        }
    }

    // 冲锋时，将 cmd intended velocities 三方向置 0.0
    if (pCmd->buttons & IN_ATTACK)
    {
        pCmd->upmove = pCmd->sidemove = pCmd->forwardmove = 0;
    }

    Vector vecSelfPos = pCharger->GetAbsOrigin();
    vec_t flClosetDistance = UTIL_GetClosetSurvivorDistance(pCharger, NULL, true, true);
    CTerrorPlayer *pTarget = (CTerrorPlayer *)UTIL_GetClosetSurvivor(pCharger, NULL, true, true);
    bool bHasVisibleThreats = pCharger->HasVisibleThreats();

    // 建立在距离小于冲锋限制距离，有视野且生还者有效的情况下
    if (flClosetDistance < z_charger_min_charge_distance.GetFloat())
    {
        
    }
}