#include "SI/smoker.h"
#include "utils.h"
#include "in_buttons.h"
#include "player.h"

CSmokerTimerEvent g_SmokerTimerEvent;
static ITimer *g_hTimerCoolDown = NULL;
static int g_iValidSurvivor = 0;

void CSmokerEventListner::FireGameEvent(IGameEvent *event)
{
    const char *name = event->GetName();
    if (!strcmp(name, "round_start"))
    {
        for (int i = 1; i <= gpGlobals->maxClients; i++)
        {
            CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndex(i);
            if (!pPlayer)
                continue;

            if (!pPlayer->IsInGame())
                continue;

            ((CSmoker *)pPlayer)->m_bCanTongue = true;
            ((CTerrorSmokerVictim *)pPlayer)->m_bLeftBehind = false;
        }
    }
}

SourceMod::ResultType CSmokerTimerEvent::OnTimer(ITimer *pTimer, void *pData)
{
    int client = (int)(intptr_t)pData;
    CSmoker *pSmoker = (CSmoker *)UTIL_PlayerByIndex(client);
    if (!pSmoker)
        return Pl_Stop;

    pSmoker->m_bCanTongue = true;
    return Pl_Continue;
}

void CSmokerTimerEvent::OnTimerEnd(ITimer *pTimer, void *pData)
{
    pTimer = nullptr;
}

void CSmokerEntityListner::OnPostThink(CBaseEntity *pEntity)
{
    CSmoker *pSmoker = (CSmoker *)pEntity;
    if (!pSmoker || !pSmoker->IsSmoker() || pSmoker->IsDead())
        return;

    if (pSmoker->IsStaggering())
        return;

    CTerrorPlayer *pTarget = (CTerrorPlayer *)UTIL_GetClientAimTarget(pSmoker, true);
    if (!pTarget || !pTarget->IsInGame() || !pTarget->IsSurvivor() ||pTarget->IsDead())
    {
        // aimming target is invalid, check if it is tonguing someone.
        CTerrorSmokerVictim *pVictim = (CTerrorSmokerVictim *)pSmoker->GetTongueVictim();
        if (pVictim && pVictim->IsInGame() && !pVictim->IsDead() && pVictim->IsSurvivor())   
        {
            pSmoker->m_bCanTongue = false;
            g_hTimerCoolDown = timersys->CreateTimer(&g_SmokerTimerEvent, g_pCVar->FindVar("tongue_hit_delay")->GetFloat(), (void *)(intptr_t)(pSmoker->entindex()), 0);
        }

        return;
    }

    CUserCmd *pCmd = pSmoker->GetCurrentCommand();
    if (!pCmd)
        return;

    Vector vecSelfPos = pSmoker->GetAbsOrigin();
    Vector vecTargetPos = pTarget->GetAbsOrigin();
    vec_t flDistance = vecSelfPos.DistTo(vecTargetPos);

    if (z_smoker_bhop.GetBool())
    {
        Vector vecVelocity;
        pSmoker->GetVelocity(&vecVelocity);

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
                    ClientPush((CBasePlayerExt *)pSmoker, vecForward);
                }
            }
        }
    }

    if (pSmoker->HasVisibleThreats())
    {
        QAngle vecTargetAngle;
        UTIL_ComputeAimAngles((CBasePlayerExt *)pSmoker, (CBasePlayerExt *)pTarget, &vecTargetAngle, AimChest);
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
        else if (flDistance < g_pCVar->FindVar("tongue_range")->GetFloat() * z_smoker_instant_shoot_range_cofficient.GetFloat() && pSmoker->m_bCanTongue)
        {
            pCmd->buttons |= IN_ATTACK;
            pCmd->buttons |= IN_ATTACK2;
            pSmoker->m_bCanTongue = false;
            g_hTimerCoolDown = timersys->CreateTimer(&g_SmokerTimerEvent, g_pCVar->FindVar("tongue_miss_delay")->GetFloat(), (void *)(intptr_t)(pSmoker->entindex()), 0);
            return;
        }
    }
}

CTerrorPlayer* BossZombiePlayerBot::OnSmokerChooseVictim(CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer)
{
    if (!this->IsSmoker() || this->IsDead())
        return NULL;

    if (this->GetTongueVictim())
        return NULL;

    if (!pLastVictim || !pLastVictim->IsInGame())
        return NULL;

    if (pLastVictim->IsSurvivor())
    {
        if (pLastVictim->IsIncapped() || pLastVictim->GetSpecialInfectedDominatingMe())
        {
            CTerrorPlayer *pNewTarget = SmokerTargetChoose(z_smoker_target_rules.GetInt(), this, pLastVictim);
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
            CTerrorPlayer *pNewTarget = SmokerTargetChoose(z_smoker_target_rules.GetInt(), this);
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
            CBaseCombatWeaponExt *pWeapon = pLastVictim->GetActiveWeapon();
            if (pWeapon && pWeapon->edict())
            {
                const char *szClassName = pWeapon->GetClassName();
                if (!strcmp(szClassName, "weapon_melee") || !strcmp(szClassName, "weapon_chiansaw"))
                {
                    CTerrorPlayer *pNewTarget = SmokerTargetChoose(z_smoker_target_rules.GetInt(), this, pLastVictim);
                    if (!pNewTarget || !pNewTarget->IsInGame() || !pNewTarget->IsSurvivor())
                        return NULL;
                    
                    for (int i = 1; i < gpGlobals->maxClients; i++)
                    {
                        CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndex(i);
                        if (!pPlayer)
                            continue;

                        if (!pPlayer->IsInGame() || !pPlayer->IsSurvivor() || pPlayer == pLastVictim)
                            continue;

                        Vector vecSelfEyePos = this->GetEyeOrigin();
                        Vector vecTargetEyePos = pPlayer->GetEyeOrigin();

                        Ray_t ray;
                        ray.Init(vecSelfEyePos, vecTargetEyePos);

                        trace_t tr;
                        UTIL_TraceRay(ray, MASK_SHOT | CONTENTS_MONSTERCLIP | CONTENTS_GRATE, NULL, COLLISION_GROUP_NONE, &tr, TR_RayFilterBySmoker, (void *)(intptr_t)(this->entindex()));

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

    CBaseEntityExt *pEntity = (CBaseEntityExt *)gameents->EdictToBaseEntity(pEdict);
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

static CTerrorPlayer *SmokerTargetChoose(int method, CTerrorPlayer *pSmoker, CTerrorPlayer *pSpecificTarget = NULL)
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
                CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndex(i);
                if (!pPlayer)
                    continue;

                if (!pPlayer->IsInGame() || !pPlayer->IsSurvivor() || pPlayer->IsDead() || pPlayer->IsIncapped() || pPlayer->GetSpecialInfectedDominatingMe())
                    continue;

                if (pSpecificTarget && pPlayer == pSpecificTarget)
                    continue;
                    
                CBaseCombatWeaponExt *pWeapon = pPlayer->GetActiveWeapon();
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
                CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndex(i);
                if (!pPlayer)
                    continue;

                if (!pPlayer->IsInGame() || !pPlayer->IsSurvivor() || pPlayer->IsDead() || pPlayer->IsIncapped() || pPlayer->GetSpecialInfectedDominatingMe())
                    continue;

                if (pSpecificTarget && pPlayer == pSpecificTarget)
                    continue;

                if (UTIL_IsLeftBehind(pPlayer))
                {
                    Vector vecTargetPos;
                    g_pZombieManager->GetRandomPZSpawnPosition(ZC_SMOKER, 5, pSmoker, &vecTargetPos);
                    pSmoker->Teleport(&vecTargetPos, NULL, NULL);
                    ((CTerrorSmokerVictim *)pPlayer)->m_bLeftBehind = true;
                    return pPlayer;
                }
                else
                {
                    ((CTerrorSmokerVictim *)pPlayer)->m_bLeftBehind = false;
                }

                float flTeamDistance = CalculateTeamDistance(pPlayer);

                CNavAreaExt *pNav = pPlayer->GetLastKnownArea();
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
                        ((CTerrorSmokerVictim *)pPlayer)->m_bLeftBehind = false;
                    }
                }

                ((CTerrorSmokerVictim *)pPlayer)->m_bLeftBehind = false;
            }
        }

        case 4:
        {
            for (int i = 1; i < gpGlobals->maxClients; i++)
            {
                CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndex(i);
                if (!pPlayer)
                    continue;

                if (!pPlayer->IsInGame() || !pPlayer->IsSurvivor() || pPlayer->IsDead() || pPlayer->IsIncapped() || pPlayer->GetSpecialInfectedDominatingMe())
                    continue;

                if (pSpecificTarget && pPlayer == pSpecificTarget)
                    continue;

                CBaseCombatWeaponExt *pWeapon = pPlayer->GetActiveWeapon();
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
        CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndex(i);
        if (!pPlayer)
            continue;

        if (!pPlayer->IsInGame() || !pPlayer->IsSurvivor() || pPlayer->IsDead() || pPlayer->IsIncapped() || pPlayer->GetSpecialInfectedDominatingMe())
            continue;

        g_iValidSurvivor += 1;
        CBaseCombatWeaponExt *pWeapon = pPlayer->GetActiveWeapon();
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