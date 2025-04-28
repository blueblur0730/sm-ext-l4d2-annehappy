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

    bool bIsCharging = false;
    CCharge *pCharge = (CCharge *)pCharger->GetAbility();

    if (pCharge)
    {
        // 记录每次冲锋结束的时间戳
        bIsCharging = pCharge->IsCharging();
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
    CTerrorPlayer *pTarget = (CTerrorPlayer *)UTIL_GetClientAimTarget(pCharger, true);
    bool bHasVisibleThreats = pCharger->HasVisibleThreats();
    
    if (!pTarget || !pTarget->IsInGame() || pTarget->IsDead())
        return;

    int targetIndex = pTarget->entindex();
    if (targetIndex <= 0 || targetIndex > gpGlobals->maxClients)
        return;

    int flags = pCharger->GetFlags();

    // 建立在距离小于冲锋限制距离，有视野且生还者有效的情况下
    if (flClosetDistance < z_charger_min_charge_distance.GetFloat() && !IsInChargeDuration(pCharger))
    {
        if (!bHasVisibleThreats)
            return;

        if (!pTarget->GetSpecialInfectedDominatingMe() && !pTarget->IsIncapped())
        {
            // 目标没有正在看着自身（被控不算看着自身），自身可以冲锋且血量大于限制血量，阻止冲锋，对目标挥拳
            if (pCharger->GetHealth() >= z_charger_avoid_melee_target_hp.GetInt() && !UTIL_IsInAimOffset(pCharger, pTarget, z_charger_target_aim_offset.GetInt()))
            {
                pCmd->buttons &= ~IN_ATTACK;
                SetChargeTimer(pCharge, g_pCVar->FindVar("z_charge_interval")->GetFloat());

                for (int i = 0; i < g_ChargerInfoMap[chargerIndex].m_iRangedIndex; i++)
                {
                    CTerrorPlayer *pRangedPlayer = (CTerrorPlayer *)UTIL_PlayerByIndex(g_ChargerInfoMap[chargerIndex].m_iRangedClientIndex[i]);
                    if (g_ChargerInfoMap[chargerIndex].m_iRangedClientIndex[i] == targetIndex 
                        && !pRangedPlayer->GetSpecialInfectedDominatingMe()
                        && UTIL_IsInAimOffset(pCharger, pRangedPlayer, z_charger_target_aim_offset.GetInt())
                        && !UTIL_IsInGetUpAnimation(pRangedPlayer)
                        && !pRangedPlayer->IsIncapped()
                        && bIsCharging
                        && (flags & FL_ONGROUND))
                    {
                        SetChargeTimer(pCharge, gpGlobals->curtime - 0.5);
                        Vector vecRangedClientPos = pRangedPlayer->GetAbsOrigin();
                        vecRangedClientPos = UTIL_MakeVectorFromPoints(vecSelfPos, vecRangedClientPos);

                        QAngle angVec;
                        AngleVectors(angVec, &vecRangedClientPos);

                        pCharger->Teleport(NULL, &angVec, NULL);

                        pCmd->buttons |= IN_ATTACK2;
                        pCmd->buttons |= IN_ATTACK;
                        return;
                    }    
                }
            }
            // 目标正在看着自身，自身可以冲锋，目标没有拿着近战，且不在倒地或起身状态时则直接冲锋，目标拿着近战，则转到 OnChooseVictim 处理，转移新目标或继续挥拳
            else if (UTIL_IsInAimOffset(pCharger, pTarget, z_charger_target_aim_offset.GetInt())
                && !CheckMelee(pTarget)
                && !UTIL_IsInGetUpAnimation(pTarget)
                && !pTarget->IsIncapped()
                && !bIsCharging
                && (flags & FL_ONGROUND))
            {
                SetChargeTimer(pCharge, gpGlobals->curtime - 0.5);
                pCmd->buttons |= IN_ATTACK2;
                pCmd->buttons |= IN_ATTACK;
                return;
            }
            else if (UTIL_IsInGetUpAnimation(pTarget) && pTarget->IsIncapped())
            {
                pCmd->buttons &= ~IN_ATTACK;
                SetChargeTimer(pCharge, g_pCVar->FindVar("z_charge_interval")->GetFloat());
                pCmd->buttons |= IN_ATTACK2;
            }
        }
        // 自身血量大于冲锋限制血量，且目标是被控的人时，检测冲锋范围内是否有其他人（可能拿着近战），有则对其冲锋，自身血量小于冲锋限制血量，对着被控的人冲锋
        else if (pTarget->GetSpecialInfectedDominatingMe())
        {
            if (pCharger->GetHealth() > z_charger_avoid_melee_target_hp.GetInt())
            {
                pCmd->buttons &= ~IN_ATTACK;
                SetChargeTimer(pCharge, g_pCVar->FindVar("z_chaarge_interval")->GetFloat());
                for (int i = 0; i < g_ChargerInfoMap[chargerIndex].m_iRangedIndex; i++)
                {
                    // 循环时，由于 ranged_index 增加时，数组中一定为有效生还者，故无需判断是否是有效生还者
                    CTerrorPlayer *pRangedPlayer = (CTerrorPlayer *)UTIL_PlayerByIndex(g_ChargerInfoMap[chargerIndex].m_iRangedClientIndex[i]);
                    if (!pRangedPlayer->GetSpecialInfectedDominatingMe()
                        && UTIL_IsInAimOffset(pCharger, pRangedPlayer, z_charger_target_aim_offset.GetInt())
                        && !UTIL_IsInGetUpAnimation(pRangedPlayer)
                        && !pRangedPlayer->IsIncapped()
                        && bIsCharging
                        && (flags & FL_ONGROUND))
                    {
                        SetChargeTimer(pCharge, gpGlobals->curtime - 0.5);
                        Vector vecRangedClientPos = pRangedPlayer->GetAbsOrigin();
                        vecRangedClientPos = UTIL_MakeVectorFromPoints(vecSelfPos, vecRangedClientPos);

                        QAngle angVec;
                        AngleVectors(angVec, &vecRangedClientPos);

                        pCharger->Teleport(NULL, &angVec, NULL);

                        pCmd->buttons |= IN_ATTACK2;
                        pCmd->buttons |= IN_ATTACK;
                        return;
                    }   
                }
            }
            else if (UTIL_IsInGetUpAnimation(pTarget) && pTarget->IsIncapped())
            {
                pCmd->buttons &= ~IN_ATTACK;
                SetChargeTimer(pCharge, g_pCVar->FindVar("z_charge_interval")->GetFloat());
                pCmd->buttons |= IN_ATTACK2;
            }
        }
    }
    else if (!bIsCharging && !IsInChargeDuration(pCharger))
    {
        pCmd->buttons &= ~IN_ATTACK;
        SetChargeTimer(pCharge, g_pCVar->FindVar("z_charge_interval")->GetFloat());
        pCmd->buttons |= IN_ATTACK2;
    }

    // 连跳，并阻止冲锋，可以攻击被控的人的时，将最小距离置 0，连跳追上被控的人
    int iBhopMinDist = g_ChargerInfoMap[chargerIndex].m_bCanAttackPinnedTarget ? 60 : z_charger_min_charge_distance.GetInt();
    Vector vecVelocity = pCharger->GetVelocity();
    vec_t flSpeed = sqrt(vecVelocity.x * vecVelocity.x + vecVelocity.y * vecVelocity.y);
    
    if (bHasVisibleThreats && z_charger_bhop.GetBool() && iBhopMinDist < flClosetDistance < 10000
        && flSpeed > 175.0f)
    {
        if (flags & FL_ONGROUND)
        {
            Vector vecTargetOrigin = pTarget->GetAbsOrigin();
            Vector vecBuffer = UTIL_CaculateVel(vecSelfPos, vecTargetOrigin, z_charger_bhop_speed.GetFloat());
    
            pCmd->buttons |= IN_JUMP;
            pCmd->buttons |= IN_DUCK;
    
            if (DoBhop(pCharger, pCmd->buttons, vecBuffer))
            {
                return;
            }
        }
    }

    // 梯子上，阻止连跳
    if (pCharger->GetMoveType() & MOVETYPE_LADDER)
    {
        pCmd->buttons &= ~IN_JUMP;
        pCmd->buttons &= ~IN_DUCK;
    }
}

// false means we can charge.
static bool IsInChargeDuration(CTerrorPlayer *pCharger)
{
    if (!pCharger || !pCharger->IsInGame() || !pCharger->IsCharger())
        return false;

    int chargerIndex = pCharger->entindex();
    if (chargerIndex <= 0 || chargerIndex > gpGlobals->maxClients)
        return false;

    if (!g_ChargerInfoMap.contains(chargerIndex))
        return false;

    return (gpGlobals->curtime - (g_ChargerInfoMap[chargerIndex].m_flChargeInterval) + g_pCVar->FindVar("z_charge_interval")->GetFloat()) < 0.0f;
}

static void SetChargeTimer(CCharge *pAbility, float flDuration)
{
    if (!pAbility)
        return;

    if (!pAbility->IsCharging())
    {
        CountdownTimer pTimer = pAbility->GetNextActivationTimer();
        pTimer.Start(flDuration);
    }
}

static bool CheckMelee(CTerrorPlayer *pPlayer)
{
    if (!pPlayer || !pPlayer->IsInGame() || !pPlayer->IsSurvivor())
        return false;

    CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
    if (!pWeapon || !pWeapon->edict())
        return false;

    const char *szClassName = pWeapon->GetClassName();
    if (!szClassName)
        return false;

    if (!V_strcmp(szClassName, "weapon_melee") || !V_strcmp(szClassName, "weapon_chainsaw"))
        return true;
    
    return false;
}