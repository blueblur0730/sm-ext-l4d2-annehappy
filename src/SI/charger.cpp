#include "SI/charger.h"
#include "utils.h"
#include "in_buttons.h"

#include "random.h"

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
int CCharge::m_iOff_m_isCharging = 0;

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

    CTerrorPlayer *pTarget = (CTerrorPlayer *)UTIL_GetClientAimTarget(pCharger, true);
    if (!pTarget || !pTarget->IsInGame() || pTarget->IsDead())
        return;

    int targetIndex = pTarget->entindex();
    if (targetIndex <= 0 || targetIndex > gpGlobals->maxClients)
        return;

    Vector vecSelfPos = pCharger->GetAbsOrigin();
    vec_t flClosetDistance = UTIL_GetClosetSurvivorDistance(pCharger, NULL, true, true);
    bool bHasVisibleThreats = pCharger->HasVisibleThreats();
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
                    if (g_ChargerInfoMap[chargerIndex].m_iRangedClientIndex[i] == targetIndex)
                        continue;
                        
                    CTerrorPlayer *pRangedPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(g_ChargerInfoMap[chargerIndex].m_iRangedClientIndex[i]);
                    if (!pRangedPlayer || !pRangedPlayer->IsInGame() || pRangedPlayer->IsDead())
                        continue;

                    if (pRangedPlayer->GetSpecialInfectedDominatingMe() || pRangedPlayer->IsIncapped())
                        continue;

                    if (!UTIL_IsInAimOffset(pCharger, pRangedPlayer, z_charger_target_aim_offset.GetInt()) || UTIL_IsInGetUpAnimation(pRangedPlayer))
                        continue;

                    if (!bIsCharging && (flags & FL_ONGROUND))
                    {
                        SetChargeTimer(pCharge, -0.5f);
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
            else if (UTIL_IsInAimOffset(pCharger, pTarget, z_charger_target_aim_offset.GetInt()))
            {
                if (!bIsCharging && (flags & FL_ONGROUND))
                {
                    if (!CheckMelee(pTarget))
                    {
                        if (!UTIL_IsInGetUpAnimation(pTarget) && !pTarget->IsIncapped())
                        {
                            SetChargeTimer(pCharge, -0.5f);
                            pCmd->buttons |= IN_ATTACK2;
                            pCmd->buttons |= IN_ATTACK;
                            return;
                        }
                    }
                    else
                    {
                        pCmd->buttons &= ~IN_ATTACK;
                        SetChargeTimer(pCharge, g_pCVar->FindVar("z_charge_interval")->GetFloat());
                        pCmd->buttons |= IN_ATTACK2;
                    }
                }
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
                    CTerrorPlayer *pRangedPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(g_ChargerInfoMap[chargerIndex].m_iRangedClientIndex[i]);
                    if (!pRangedPlayer || !pRangedPlayer->IsInGame() || pRangedPlayer->IsDead())
                        continue;

                    if (pRangedPlayer->GetSpecialInfectedDominatingMe() || pRangedPlayer->IsIncapped())
                        continue;

                    if (!UTIL_IsInAimOffset(pCharger, pRangedPlayer, z_charger_target_aim_offset.GetInt()) || UTIL_IsInGetUpAnimation(pRangedPlayer))
                        continue;

                    if (!bIsCharging && (flags & FL_ONGROUND))
                    {
                        SetChargeTimer(pCharge, -0.5);
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
            // 被控的人在起身或者倒地状态，阻止冲锋
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
    
    bool bDistCheck = (iBhopMinDist < (int)flClosetDistance && flClosetDistance < 10000.0f);
    if (bHasVisibleThreats && z_charger_bhop.GetBool() && bDistCheck && flSpeed > 175.0f)
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

CTerrorPlayer *BossZombiePlayerBot::OnChargerChooseVictim(CTerrorPlayer *pAttacker, CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer)
{
    if (!pAttacker || !pAttacker->IsInGame() || !pAttacker->IsCharger())
        return NULL;

    if (!pLastVictim || !pLastVictim->IsInGame() || !pLastVictim->IsSurvivor() || pLastVictim->IsDead())
        return NULL;

    int attackerIndex = pAttacker->entindex();
    if (attackerIndex <= 0 || attackerIndex > gpGlobals->maxClients)
        return NULL;

    if (pAttacker->GetPummelVictim() || pAttacker->GetCarryVictim())
        return NULL;

    CCharge *pCharge = (CCharge *)pAttacker->GetAbility();
    if (!pCharge)
        return NULL;

    Vector vecSelfPos = pAttacker->GetEyeOrigin();
    FindRangedClients(pAttacker, z_charger_extra_target_range_min.GetFloat(), z_charger_extra_target_range_max.GetFloat());

    int health = pAttacker->GetHealth();
    Vector vecTargetPos = pLastVictim->GetEyeOrigin();

    // 范围内有人被控且自身血量大于限制血量，则先去对被控的人挥拳
    for (int i = 0; i < g_ChargerInfoMap[attackerIndex].m_iRangedIndex; i++)
    {
        if (health > z_charger_avoid_melee_target_hp.GetInt() && !IsInChargeDuration(pAttacker))
        {
            CTerrorPlayer *pRangedPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(g_ChargerInfoMap[attackerIndex].m_iRangedClientIndex[i]);
            if (pRangedPlayer &&
                (pRangedPlayer->GetPounceAttacker() ||
                pRangedPlayer->GetTongueOwner() ||
                pRangedPlayer->GetJockeyAttacker())
            )
            {
                g_ChargerInfoMap[attackerIndex].m_bCanAttackPinnedTarget = true;
                SetChargeTimer(pCharge, g_pCVar->FindVar("z_charge_interval")->GetFloat());
                return pRangedPlayer;
            }

            g_ChargerInfoMap[attackerIndex].m_bCanAttackPinnedTarget = false;
        }
    }

    vec_t flDist = vecSelfPos.DistTo(vecTargetPos);
    if (!pLastVictim->GetSpecialInfectedDominatingMe() && !pLastVictim->IsIncapped())
    {
        if (z_charger_melee_avoid.GetBool() && CheckMelee(pLastVictim) && health >= z_charger_avoid_melee_target_hp.GetInt())
        {
            // 允许近战回避且目标正拿着近战武器且血量高于冲锋限制血量，随机获取一个没有拿着近战武器且可视的目标，转移目标
            if (flDist >= z_charger_min_charge_distance.GetFloat())
            {
                CTerrorPlayer *pRandomPlayer = FindRandomNoneMeleeTarget();
                if (pRandomPlayer)
                {
                    bool bUnknown;
                    Vector vecTargetEyePos = pRandomPlayer->GetEyeOrigin();
                    if (IsVisiableToPlayer(vecTargetEyePos, (CBasePlayer *)pAttacker, L4D2Teams_Survivor, L4D2Teams_Infected, 0.0, NULL, NULL, &bUnknown))
                    {
                        return pRandomPlayer;
                    }
                }
            }
            // 不满足近战回避距离限制或血量要求的牛，阻止其冲锋，令其对手持近战的目标挥拳
            else if (!IsInChargeDuration(pAttacker) && flDist < z_charger_min_charge_distance.GetFloat())
            {
                SetChargeTimer(pCharge, g_pCVar->FindVar("z_charge_interval")->GetFloat());
            }
        }

        switch (z_charger_target_rules.GetInt())
        {
            case 2:
            {
                return (CTerrorPlayer *)UTIL_GetClosetSurvivor(pAttacker, NULL, true, true);
            }

            case 3:
            {
                return GetTargetThatCanCrumbleTo();
            }

            default:
            {
                return NULL;
            }
        }
    }
    
    if (g_ChargerInfoMap[attackerIndex].m_bCanAttackPinnedTarget && (pLastVictim->IsIncapped() || pLastVictim->GetSpecialInfectedDominatingMe()))
    {
        return (CTerrorPlayer *)UTIL_GetClosetSurvivor(pAttacker, NULL, true, true);
    }

    return NULL;
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
        CountdownTimer &pTimer = pAbility->GetNextActivationTimer();
        pTimer.SetTimeStamp(flDuration);    // we do not need to change the duration.
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

static int FindRangedClients(CTerrorPlayer *pCharger, float flMin, float flMax)
{
    int index = 0;
    if (!pCharger || !pCharger->IsInGame() || !pCharger->IsCharger())
        return -1;

    int chargerIndex = pCharger->entindex();
    if (chargerIndex <= 0 || chargerIndex > gpGlobals->maxClients)
        return -1;

    edict_t *pChargerEdict = pCharger->edict();
    if (!pChargerEdict)
        return -1;

    for (int i = 1; i < gpGlobals->maxClients; i++)
    {
        CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(i);
        if (!pPlayer || !pPlayer->IsInGame() || !pPlayer->IsSurvivor() || pPlayer->IsDead() || pPlayer->IsIncapped())
            continue;

        Vector vecSelfEyePos = pCharger->GetEyeOrigin();
        Vector vecTargetEyePos = pPlayer->GetEyeOrigin();
        vec_t flDist = vecSelfEyePos.DistTo(vecTargetEyePos);
        if (flDist >= flMin && flDist <= flMax)
        {
            Ray_t ray;
            trace_t tr;

            ray.Init(vecSelfEyePos, vecTargetEyePos);
            UTIL_TraceRay(ray, MASK_VISIBLE, pChargerEdict->GetIServerEntity(), COLLISION_GROUP_NONE, &tr, NULL, NULL);

            if (!tr.DidHit() || (tr.m_pEnt && (tr.m_pEnt->entindex() == i)))
            {
                g_ChargerInfoMap[chargerIndex].m_iRangedClientIndex[index] = i;
                index += 1;
            }
        }
    }

    g_ChargerInfoMap[chargerIndex].m_iRangedIndex = index;
    return index;
}

static CTerrorPlayer *FindRandomNoneMeleeTarget()
{
    std::vector<CTerrorPlayer *> aSurvivors;
    for (int i = 1; i < gpGlobals->maxClients; i++)
    {
        CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(i);
        if (!pPlayer || !pPlayer->IsInGame() || !pPlayer->IsSurvivor() || pPlayer->IsDead() || pPlayer->IsIncapped() || pPlayer->GetSpecialInfectedDominatingMe())
            continue;

        if (!CheckMelee(pPlayer))
            aSurvivors.push_back(pPlayer);
    }

    if (aSurvivors.empty())
        return NULL;
    
    int iRandomIndex = RandomInt(0, aSurvivors.size() - 1);
    CTerrorPlayer *pRandomPlayer = aSurvivors[iRandomIndex];
    aSurvivors.clear();
    return pRandomPlayer;
}

static CTerrorPlayer *GetTargetThatCanCrumbleTo()
{
    bool bSecondCheck = false;
    std::vector<CTerrorPlayer *> aSurvivors;
    std::vector<vec_t> aDistance;
    for (int i = 1; i < gpGlobals->maxClients; i++)
    {
        CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(i);
        if (!pPlayer || !pPlayer->IsInGame() || !pPlayer->IsSurvivor() || pPlayer->IsDead() || pPlayer->IsIncapped() || pPlayer->GetSpecialInfectedDominatingMe())
            continue;

        if (bSecondCheck && aSurvivors.size() >= 1)
        {
            Vector vecSelfPos = pPlayer->GetAbsOrigin();
            for (int j = 0; j < aSurvivors.size(); j++)
            {
                Vector vecTargetPos = aSurvivors[j]->GetAbsOrigin();
                aDistance[j] += vecSelfPos.DistTo(vecTargetPos);
            }

            continue;
        }

        aSurvivors.push_back(pPlayer);

        if (i == gpGlobals->maxClients && aSurvivors.size() >= 1 && !bSecondCheck)
        {
            i == 1;
            bSecondCheck = true;
            continue;
        }
    }

    if (aSurvivors.empty())
        return NULL;

    int index = 0;
    for (int i = 0; i < aSurvivors.size(); i++)
    {
        if (aDistance[aSurvivors[index]->entindex()] > aDistance[aSurvivors[i]->entindex()])
        {
            if (aDistance[aSurvivors[i]->entindex()] != -1.0f)
            {
                index = i;
            }
        }
    }

    CTerrorPlayer *pTarget = aSurvivors[index];
    aSurvivors.clear();
    aDistance.clear();

    return pTarget;
}