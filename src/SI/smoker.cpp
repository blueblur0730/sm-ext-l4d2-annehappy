#include "SI/smoker.h"
#include "utils.h"
#include "in_buttons.h"

CSmokerTimerEvent g_SmokerTimerEvent;
static ITimer *g_hTimerCoolDown = NULL;
static int g_iValidSurvivor = 0;

std::unordered_map<int, smokerInfo_t> g_MapSmokerInfo;
std::unordered_map<int, smokerVictimInfo_t> g_MapSmokerVictimInfo;

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

void CSmokerEventListner::FireGameEvent(IGameEvent *event)
{
    const char *name = event->GetName();
    if (!strcmp(name, "round_start"))
    {
        for (int i = 1; i <= gpGlobals->maxClients; i++)
        {
            CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(i);
            if (!pPlayer)
                continue;

            if (!pPlayer->IsInGame())
                continue;

            if (pPlayer->IsSurvivor() && g_MapSmokerVictimInfo.contains(i))
            {
                g_MapSmokerVictimInfo[i].m_bLeftBehind = false;
            }
            else if (pPlayer->IsSmoker() && g_MapSmokerInfo.contains(i))
            {
                g_MapSmokerInfo[i].m_bCanTongue = true;
            }
        }
    }
}

int CSmokerEventListner::GetEventDebugID()
{
    return EVENT_DEBUG_ID_INIT;
}

void CSmokerEventListner::OnClientPutInServer(int client)
{
    CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(client);
    if (!pPlayer || !pPlayer->IsInGame())
        return;

    if (pPlayer->IsSurvivor())
    {
        smokerVictimInfo_t info;
        info.Init();
        g_MapSmokerVictimInfo[client] = info;
    }
    else if (pPlayer->IsSmoker())
    {
        smokerInfo_t info;
        info.Init();
        g_MapSmokerInfo[client] = info;
    }
}

void CSmokerEventListner::OnClientDisconnecting(int client)
{
    CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(client);
    if (!pPlayer || !pPlayer->IsInGame())
        return;

    if (pPlayer->IsSurvivor())
    {
        if (g_MapSmokerVictimInfo.contains(client))
        {
            g_MapSmokerVictimInfo[client].Init();
            g_MapSmokerVictimInfo.erase(client);
        }
    }
    else if (pPlayer->IsSmoker())
    {
        if (g_MapSmokerInfo.contains(client))
        {
            g_MapSmokerInfo[client].Init();
            g_MapSmokerInfo.erase(client);
        }
    }
}

SourceMod::ResultType CSmokerTimerEvent::OnTimer(ITimer *pTimer, void *pData)
{
    int client = (int)(intptr_t)pData;
    CTerrorPlayer *pSmoker = (CTerrorPlayer *)UTIL_PlayerByIndexExt(client);
    if (!pSmoker || !pSmoker->IsInGame() || !pSmoker->IsSmoker())
        return Pl_Stop;

    if (g_MapSmokerInfo.contains(client))
        g_MapSmokerInfo[client].m_bCanTongue = true;

    return Pl_Continue;
}

void CSmokerTimerEvent::OnTimerEnd(ITimer *pTimer, void *pData)
{
    pTimer = nullptr;
}

void CSmokerEventListner::OnPlayerRunCmd(CBaseEntity *pEntity, CUserCmd *pCmd)
{
    CTerrorPlayer *pSmoker = reinterpret_cast<CTerrorPlayer *>(pEntity);
    if (!pSmoker || !pSmoker->IsSmoker() || pSmoker->IsDead())
        return;

    if (!pCmd)
        return;

    int smokerIndex = pSmoker->entindex();
    if (smokerIndex <= 0 || smokerIndex > gpGlobals->maxClients)
        return;

    if (!g_MapSmokerInfo.contains(smokerIndex) && pSmoker->IsSmoker())
    {
        smokerInfo_t info;
        info.Init();
        g_MapSmokerInfo[smokerIndex] = info;
    }

    if (pSmoker->IsStaggering())
        return;

    CTerrorPlayer *pTarget = (CTerrorPlayer *)UTIL_GetClientAimTarget(pSmoker, true);
    if (!pTarget || !pTarget->IsInGame() || !pTarget->IsSurvivor() || pTarget->IsDead())
    {
        if (!g_MapSmokerVictimInfo.contains(smokerIndex) && pSmoker->IsSurvivor())
        {
            smokerVictimInfo_t info;
            info.Init();
            g_MapSmokerVictimInfo[smokerIndex] = info;
        }

        // aimming target is invalid, check if it is tonguing someone.
        CTerrorPlayer *pVictim = pSmoker->GetTongueVictim();
        if (pVictim && pVictim->IsInGame() && !pVictim->IsDead() && pVictim->IsSurvivor())   
        {
            g_MapSmokerInfo[smokerIndex].m_bCanTongue = false;
            g_hTimerCoolDown = timersys->CreateTimer(&g_SmokerTimerEvent, g_pCVar->FindVar("tongue_hit_delay")->GetFloat(), (void *)(intptr_t)(pSmoker->entindex()), 0);
        }

        return;
    }

    Vector vecSelfPos = pSmoker->GetAbsOrigin();
    Vector vecTargetPos = pTarget->GetAbsOrigin();
    vec_t flDistance = vecSelfPos.DistTo(vecTargetPos);

    if (z_smoker_bhop.GetBool())
    {
        Vector vecVelocity = pSmoker->GetVelocity();

        // vertical velocity is not considered?
        vec_t flSpeed = (vec_t)FastSqrt(vecVelocity.x * vecVelocity.x + vecVelocity.y * vecVelocity.y);

        if (g_pCVar->FindVar("tongue_range")->GetFloat() * z_smoker_instant_shoot_range_cofficient.GetFloat()
            < flDistance < 2000.0f && flSpeed > 190.0f)
        {
            if (pSmoker->GetGroundEntity())
            {
                QAngle vecEyeAngle;
                pSmoker->GetEyeAngles(&vecEyeAngle);

                Vector vecForward;
                AngleVectors(vecEyeAngle, &vecForward);
                VectorNormalize(vecForward);
                VectorScale(vecForward, z_smoker_bhop_speed.GetFloat(), vecForward);

                pCmd->buttons |= IN_JUMP;
                pCmd->buttons |= IN_DUCK;

                if ((pCmd->buttons & IN_FORWARD) || (pCmd->buttons & IN_BACK) || (pCmd->buttons & IN_MOVELEFT) || (pCmd->buttons & IN_MOVERIGHT))
                {
                    ClientPush(pSmoker, vecForward);
                }
            }
        }
    }

    if (pSmoker->HasVisibleThreats())
    {
        QAngle vecTargetAngle;
        UTIL_ComputeAimAngles(pSmoker, pTarget, &vecTargetAngle, AimChest);
        vecTargetAngle.z = 0.0f;
        pSmoker->Teleport(NULL, &vecTargetAngle, NULL);
    }

    if (pSmoker->HasVisibleThreats() && !pTarget->IsIncapped() && pTarget->GetSpecialInfectedDominatingMe())
    {
        if (flDistance < SMOKER_MELEE_RANGE)
        {
            pCmd->buttons |= IN_ATTACK;
            pCmd->buttons |= IN_ATTACK2;
            return;
        }
        else if (flDistance < g_pCVar->FindVar("tongue_range")->GetFloat() * z_smoker_instant_shoot_range_cofficient.GetFloat() && g_MapSmokerInfo[smokerIndex].m_bCanTongue)
        {
            pCmd->buttons |= IN_ATTACK;
            pCmd->buttons |= IN_ATTACK2;
            g_MapSmokerInfo[smokerIndex].m_bCanTongue = false;
            g_hTimerCoolDown = timersys->CreateTimer(&g_SmokerTimerEvent, g_pCVar->FindVar("tongue_miss_delay")->GetFloat(), (void *)(intptr_t)(pSmoker->entindex()), 0);
            return;
        }
    }
}

CTerrorPlayer* BossZombiePlayerBot::OnSmokerChooseVictim(CTerrorPlayer *pAttacker, CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer)
{
    if (!pAttacker->IsSmoker() || pAttacker->IsDead())
        return NULL;

    if (pAttacker->GetTongueVictim())
        return NULL;

    if (!pLastVictim || !pLastVictim->IsInGame())
        return NULL;

    if (pLastVictim->IsSurvivor())
    {
        if (pLastVictim->IsIncapped() || pLastVictim->GetSpecialInfectedDominatingMe())
        {
            CTerrorPlayer *pNewTarget = SmokerTargetChoose(z_smoker_target_rules.GetInt(), pAttacker, pLastVictim);
            if (pNewTarget && pNewTarget->IsInGame() && pNewTarget->IsSurvivor())
            {
                return pNewTarget;
            }
        }
    }

    if (z_smoker_melee_avoid.GetBool())
    {
        if (g_iValidSurvivor == TeamMeleeCheck())
        {
            g_iValidSurvivor = 0;
            CTerrorPlayer *pNewTarget = SmokerTargetChoose(z_smoker_target_rules.GetInt(), pAttacker);
            if (pNewTarget && pNewTarget->IsInGame() && pNewTarget->IsSurvivor())
            {
                return pNewTarget;
            }
        }
        else
        {
            g_iValidSurvivor = 0;
        }

        if (pLastVictim->IsSurvivor())
        {
            CBaseCombatWeapon *pWeapon = pLastVictim->GetActiveWeapon();
            if (pWeapon && pWeapon->edict())
            {
                const char *szClassName = pWeapon->GetClassName();
                if (!strcmp(szClassName, "weapon_melee") || !strcmp(szClassName, "weapon_chiansaw"))
                {
                    CTerrorPlayer *pNewTarget = SmokerTargetChoose(z_smoker_target_rules.GetInt(), pAttacker, pLastVictim);
                    if (!pNewTarget || !pNewTarget->IsInGame() || !pNewTarget->IsSurvivor())
                        return NULL;
                    
                    for (int i = 1; i < gpGlobals->maxClients; i++)
                    {
                        CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(i);
                        if (!pPlayer)
                            continue;

                        if (!pPlayer->IsInGame() || !pPlayer->IsSurvivor() || pPlayer == pLastVictim)
                            continue;

                        Vector vecSelfEyePos = pAttacker->GetEyeOrigin();
                        Vector vecTargetEyePos = pPlayer->GetEyeOrigin();

                        Ray_t ray;
                        ray.Init(vecSelfEyePos, vecTargetEyePos);

                        trace_t tr;
                        UTIL_TraceRay(ray, MASK_SHOT | CONTENTS_MONSTERCLIP | CONTENTS_GRATE, NULL, COLLISION_GROUP_NONE, &tr, TR_RayFilterBySmoker, (void *)(intptr_t)(pAttacker->entindex()));

                        if (!tr.DidHit() && vecSelfEyePos.DistTo(vecTargetEyePos) < 600.0f)
                            return pNewTarget;
                    }
                }
            }
        }
    }

    return NULL;
}

static bool TR_RayFilterBySmoker(IHandleEntity* pHandleEntity, int contentsMask, void* data)
{
    CBaseHandle EHandle = pHandleEntity->GetRefEHandle();
    if (!EHandle.IsValid())
        return false;

    edict_t *pEdict = gamehelpers->GetHandleEntity(EHandle);
    if (!pEdict)
        return false;

    CBaseEntity *pEntity = (CBaseEntity *)gameents->EdictToBaseEntity(pEdict);
    if (!pEntity)
        return false;

    int index = pEntity->entindex();
    if (index > 0 || index <= gpGlobals->maxClients)
    {
        if (index == (int)(intptr_t)data)
            return false;

        if (!((CTerrorPlayer *)pEntity)->IsInfected())
            return false;
    }

    const char *szClassName = pEntity->GetClassName();
    if (!strcmp(szClassName, "infected") || !strcmp(szClassName, "witch"))
        return false;

    if (!strcmp(szClassName, "env_physics_blocker") && ((CEnvPhysicsBlocker *)pEntity)->GetBlockType() == BlockType_Survivors)
        return false;

    return true;
}

static CTerrorPlayer *SmokerTargetChoose(int method, CTerrorPlayer *pSmoker, CTerrorPlayer *pSpecificTarget)
{
    switch (method)
    {
        case 0:
        {
            return NULL;
        }

        case 1:
        {
            return (CTerrorPlayer *)UTIL_GetClosetSurvivor(pSmoker, NULL, true, true);
        }

        case 2:
        {
            for (int i = 1; i < gpGlobals->maxClients; i++)
            {
                CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(i);
                if (!pPlayer)
                    continue;

                if (!pPlayer->IsInGame() || !pPlayer->IsSurvivor() || pPlayer->IsDead() || pPlayer->IsIncapped() || pPlayer->GetSpecialInfectedDominatingMe())
                    continue;

                if (pSpecificTarget && pPlayer == pSpecificTarget)
                    continue;
                    
                CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
                if (pWeapon && pWeapon->edict())
                {
                    const char *szClassName = pWeapon->GetClassName();
                    if (!strcmp(szClassName, "weapon_pumpshotgun") || !strcmp(szClassName, "weapon_shotgun_chrome") ||
                        !strcmp(szClassName, "weapon_autoshotgun") || !strcmp(szClassName, "weapon_shotgun_spas"))
                    {
                        return pPlayer;
                    }
                }
            }
        }

        case 3:
        {
            for (int i = 1; i < gpGlobals->maxClients; i++)
            {
                CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(i);
                if (!pPlayer)
                    continue;

                if (!pPlayer->IsInGame() || !pPlayer->IsSurvivor() || pPlayer->IsDead() || pPlayer->IsIncapped() || pPlayer->GetSpecialInfectedDominatingMe())
                    continue;

                if (pSpecificTarget && pPlayer == pSpecificTarget)
                    continue;

                if (!g_MapSmokerVictimInfo.contains(i))
                {
                    smokerVictimInfo_t victimInfo;
                    victimInfo.Init();
                    g_MapSmokerVictimInfo[i] = victimInfo;
                }

                if (UTIL_IsLeftBehind(pPlayer))
                {
                    Vector vecTargetPos;
                    g_pZombieManager->GetRandomPZSpawnPosition(ZC_SMOKER, 5, pSmoker, &vecTargetPos);
                    pSmoker->Teleport(&vecTargetPos, NULL, NULL);
                    g_MapSmokerVictimInfo[i].m_bLeftBehind = true;
                    return pPlayer;
                }
                else
                {
                    g_MapSmokerVictimInfo[i].m_bLeftBehind = false;
                }

                float flTeamDistance = CalculateTeamDistance(pPlayer);

                CNavArea *pNav = pPlayer->GetLastKnownArea();
                if (!pNav)
                    return NULL;

                float flPlayerDistance = pNav->GetFlow() / g_pNavMesh->GetMapMaxFlowDistance();
                if (flPlayerDistance > 0.0f && flPlayerDistance < 1.0f && flTeamDistance != 1.0f)
                {
                    if (flTeamDistance + z_smoker_left_behind_distance.GetFloat() < flPlayerDistance)
                    {
                        Vector vecTargetPos;
                        g_pZombieManager->GetRandomPZSpawnPosition(ZC_SMOKER, 5, pSmoker, &vecTargetPos);
                        pSmoker->Teleport(&vecTargetPos, NULL, NULL);
                        return pPlayer;
                    }
                    else
                    {
                        g_MapSmokerVictimInfo[i].m_bLeftBehind = false;
                    }
                }

                g_MapSmokerVictimInfo[i].m_bLeftBehind = false;
            }
        }

        case 4:
        {
            for (int i = 1; i < gpGlobals->maxClients; i++)
            {
                CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(i);
                if (!pPlayer)
                    continue;

                if (!pPlayer->IsInGame() || !pPlayer->IsSurvivor() || pPlayer->IsDead() || pPlayer->IsIncapped() || pPlayer->GetSpecialInfectedDominatingMe())
                    continue;

                if (pSpecificTarget && pPlayer == pSpecificTarget)
                    continue;

                CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
                if (pWeapon && pWeapon->IsReloading())
                {
                    return pPlayer;
                }
            }
        }

        default:
        {
            return (CTerrorPlayer *)UTIL_GetClosetSurvivor(pSmoker, NULL, true, true);
        }
    }
}

static int TeamMeleeCheck()
{
    int iTeamMeleeCount = 0;
    for (int i = 1; i < gpGlobals->maxClients; i++)
    {
        CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(i);
        if (!pPlayer)
            continue;

        if (!pPlayer->IsInGame() || !pPlayer->IsSurvivor() || pPlayer->IsDead() || pPlayer->IsIncapped() || pPlayer->GetSpecialInfectedDominatingMe())
            continue;

        g_iValidSurvivor += 1;
        CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
        if (pWeapon && pWeapon->edict())
        {
            const char *szClassName = pWeapon->GetClassName();
            if (!strcmp(szClassName, "weapon_melee") || !strcmp(szClassName, "weapon_chiansaw"))
            {
                iTeamMeleeCount += 1;
            }
        }
    }

    return iTeamMeleeCount;
}