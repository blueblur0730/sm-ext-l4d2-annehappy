#include "boomer.h"
#include "utils.h"
#include "in_buttons.h"

ITimer *g_hResetBileTimer = NULL;
ITimer *g_hResetAbilityTimer = NULL;
ITimer *g_hResetBiledStateTimer = NULL;

void CBoomerEventListner::FireGameEvent(IGameEvent *event)
{
    const char *name = event->GetName();
    if (!strcmp(name, "player_spawn"))
        OnPlayerSpawned(event);
    else if (!strcmp(name, "player_shoved"))
        OnPlayerShoved(event);
    else if (!strcmp(name, "player_now_it"))
        OnPlayerNowIt(event);
    else if (!strcmp(name, "round_start"))
        OnRoundStart(event);
}

void CBoomerEventListner::OnPlayerSpawned(IGameEvent *event)
{
    CBoomer *pBoomer = (CBoomer *)UTIL_PlayerByUserId(event->GetInt("userid"));
    if (!pBoomer || !pBoomer->IsBoomer())
        return;

    pBoomer->m_bCanBile = true;
    pBoomer->m_bIsInCoolDown = false;
    pBoomer->m_nBileFrame[0] = 0;
    pBoomer->m_nBileFrame[1] = 0;
    pBoomer->m_aTargetInfo.clear();
}

void CBoomerEventListner::OnPlayerShoved(IGameEvent *event)
{
    CBoomer *pPlayer = (CBoomer *)UTIL_PlayerByUserId(event->GetInt("userid"));

    int index = pPlayer->entindex();
    if (index <= 0 || index > gpGlobals->maxClients)
        return;

    if (!pPlayer->IsBoomer())
        return;

    pPlayer->m_bCanBile = false;
    g_hResetBileTimer = timersys->CreateTimer(&g_BoomerTimerEvent, 1.5f, (void *)(intptr_t)index, 0);
}

void CBoomerEventListner::OnPlayerNowIt(IGameEvent *event)
{
    CBoomer *pAttacker = (CBoomer *)UTIL_PlayerByUserId(event->GetInt("attacker"));
    if (!pAttacker || !pAttacker->IsBoomer())
        return;

    CTerrorPlayer *pVictim = (CTerrorPlayer *)UTIL_PlayerByUserId(event->GetInt("userid"));

    int index = pVictim->entindex();
    if (index <= 0 || index > gpGlobals->maxClients)
        return;

    ((CTerrorBoomerVictim *)pVictim)->m_bBiled = true;
    g_hResetBiledStateTimer = timersys->CreateTimer(&g_BoomerTimerEvent, bridge->FindConVar("sb_vomit_blind_time")->GetFloat(), (void *)(intptr_t)index, 0);
}

void CBoomerEventListner::OnRoundStart(IGameEvent *event)
{
    for (int i = 1; i <= gpGlobals->maxClients; i++)
    {
        CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndex(i);
        if (!pPlayer)
            continue;

        ((CTerrorBoomerVictim *)pPlayer)->m_iSecondCheckFrame = 0;
    }
}

SourceMod::ResultType CBoomerTimerEvent::OnTimer(ITimer *pTimer, void *pData)
{
    if (pTimer == g_hResetBileTimer)
    {
        int client = (int)(intptr_t)pData;
        if (client <= 0 || client > gpGlobals->maxClients)
            return Pl_Stop;

        CBoomer *pBoomer = (CBoomer *)UTIL_PlayerByIndex(client);
        if (!pBoomer->IsBoomer() || pBoomer->IsDead())
            return Pl_Stop;

        pBoomer->m_bCanBile = false;
        pBoomer->m_bIsInCoolDown = true;
        g_hResetAbilityTimer = timersys->CreateTimer(&g_BoomerTimerEvent, bridge->FindConVar("z_vomit_interval")->GetFloat(), (void *)(intptr_t)client, 0);
        return Pl_Continue;
    }

    if (pTimer == g_hResetAbilityTimer)
    {
        int client = (int)(intptr_t)pData;
        if (client <= 0 || client > gpGlobals->maxClients)
            return Pl_Stop;

        CBoomer *pBoomer = (CBoomer *)UTIL_PlayerByIndex(client);
        if (!pBoomer->IsBoomer() || pBoomer->IsDead())
            return Pl_Stop;

        pBoomer->m_bCanBile = true;
        pBoomer->m_bIsInCoolDown = false;
    }

    if (pTimer == g_hResetBiledStateTimer)
    {
        int client = (int)(intptr_t)pData;
        if (client <= 0 || client > gpGlobals->maxClients)
            return Pl_Stop;

        CTerrorBoomerVictim *pVictim = (CTerrorBoomerVictim *)UTIL_PlayerByIndex(client);
        pVictim->m_bBiled = false;
        pVictim->m_iSecondCheckFrame = 0;
    }

    return Pl_Stop;
}

void CBoomerTimerEvent::OnTimerEnd(ITimer *pTimer, void *pData)
{
    pTimer = nullptr;
}

void CBoomerEntityListner::OnPostThink(CBaseEntity *pEntity)
{
    CBoomer *pPlayer = (CBoomer *)pEntity;
    if (!pPlayer || !pPlayer->IsBoomer() || pPlayer->IsDead())
        return;

    CTerrorPlayer *pTarget = (CTerrorPlayer *)UTIL_GetClosetSurvivor(pPlayer, NULL);
    if (!pTarget)
        return;

    CBaseAbility *pAbility = pPlayer->GetAbility();
    if (!pAbility)
        return;

    IPlayerInfo *pInfo = pPlayer->GetPlayerInfo();
    IPlayerInfo *pTargetInfo = pTarget->GetPlayerInfo();
    if (!pInfo || !pTargetInfo)
        return;

    Vector vecSelfPos = pInfo->GetAbsOrigin();
    Vector vecTargetPos = pTargetInfo->GetAbsOrigin();

    IGamePlayer *pGamePlaer = pPlayer->GetGamePlayer();
    if (!pGamePlaer)
        return;

    Vector vecSelfEyePos;
    serverClients->ClientEarPosition(pGamePlaer->GetEdict(), &vecSelfEyePos);

    vec_t flTargetDist = vecSelfPos.DistTo(vecTargetPos);
    if (pPlayer->HasVisibleThreats() && !pPlayer->m_bIsInCoolDown && pPlayer->m_aTargetInfo.size() < 1)
    {
        if (flTargetDist <= bridge->FindConVar("z_vomit_range")->GetFloat())
        {
            QAngle vecAngles;
            UTIL_ComputeAimAngles(pPlayer, pTarget, &vecAngles, AimEye);
            if (z_boomer_vision_up_on_vomit.GetBool())
            {
                vec_t flHight = vecSelfPos.z - vecTargetPos.z;
                if (flHight <= 0.0f)
                {
                    vecAngles.x -= (flTargetDist / (PLAYER_HEIGHT * 0.8));
                }
                else if (flHight > 0.0f)
                {
                    vecAngles.x -= (flTargetDist / (PLAYER_HEIGHT * 1.5));
                }
            }

            ((CBaseEntity *)pPlayer)->Teleport(NULL, &vecAngles, NULL);

            CTerrorBoomerVictim *pVictim = (CTerrorBoomerVictim *)pTarget;
            if (z_boomer_degree_force_bile.GetInt() > 0 && UTIL_IsInAimOffset(pPlayer, pTarget, z_boomer_degree_force_bile.GetFloat()) &&
                !pVictim->m_bBiled && pAbility->IsSpraying())
            {
                if (pVictim->m_iSecondCheckFrame < z_boomer_spin_interval.GetInt() && !pVictim->m_bBiled)
                {
                    pVictim->m_iSecondCheckFrame++;
                }
                else if (!pVictim->m_bBiled && secondCheck(pPlayer, pTarget))
                {
                    pTarget->OnVomitedUpon(pPlayer, false);
                    pVictim->m_bBiled = true;
                    pVictim->m_iSecondCheckFrame = 0;
                }
            }
        }
    }

    if (pPlayer->m_aTargetInfo.size() >= 1 && !pPlayer->m_bIsInCoolDown && z_boomer_vision_spin_on_vomit.GetBool())
    {
        if (pPlayer->m_nBileFrame[0] < pPlayer->m_aTargetInfo.size())
        {
            targetInfo_t targetInfo = pPlayer->m_aTargetInfo[pPlayer->m_nBileFrame[0]];
            CTerrorPlayer *pVictim = targetInfo.pPlayer;
            if (!pVictim || pVictim->IsDead() || !pVictim->IsInGame())
                return;

            vec_t flSpinAngle = targetInfo.flAngle;
            IPlayerInfo *pVictimInfo = pVictim->GetPlayerInfo();
            if (!pVictimInfo)
                return;

            QAngle vecAngles;
            Vector vecVictimPos = pVictimInfo->GetAbsOrigin();
            vec_t flHeight = vecSelfPos.z - vecVictimPos.z;
            UTIL_ComputeAimAngles(pPlayer, pVictim, &vecAngles, AimEye);

            if (z_boomer_vision_up_on_vomit.GetBool())
            {
                if (flHeight <= 0.0f)
                {
                    vecAngles.x -= (flTargetDist / (PLAYER_HEIGHT * 0.8));
                }
                else if (flHeight > 0.0f)
                {
                    vecAngles.x -= (flTargetDist / (PLAYER_HEIGHT * 1.5));
                }
            }

            pPlayer->Teleport(NULL, &vecAngles, NULL);

            int targetFrame = z_boomer_predict_pos.GetBool() ?
            (int)(flSpinAngle / TURN_ANGLE_DIVIDE) :
            z_boomer_spin_interval.GetInt();

            CTerrorBoomerVictim *pVictim2 = (CTerrorBoomerVictim *)pVictim;
            if (z_boomer_degree_force_bile.GetBool() && z_boomer_predict_pos.GetBool() && targetFrame == 0)
            {
                pVictim->OnVomitedUpon(pPlayer, false);
                pVictim2->m_bBiled = true;
                pVictim2->m_iSecondCheckFrame = 0;
                pPlayer->m_nBileFrame[0] += 1;
                return;
            }

            if (pPlayer->m_nBileFrame[1] < targetFrame)
            {
                if (z_boomer_degree_force_bile.GetBool())
                {
                    if (pVictim2->m_iSecondCheckFrame < targetFrame && !pVictim2->m_bBiled)
                    {
                        pVictim2->m_iSecondCheckFrame++;
                    }
                    else if (!pVictim2->m_bBiled && secondCheck(pPlayer, pVictim))
                    {
                        pVictim->OnVomitedUpon(pPlayer, false);
                        pVictim2->m_bBiled = true;
                        pVictim2->m_iSecondCheckFrame = 0;
                        pPlayer->m_nBileFrame[1] = targetFrame;
                    }
                    else if (pVictim2->m_bBiled)
                    {
                        pPlayer->m_nBileFrame[1] = pVictim2->m_iSecondCheckFrame = 0;
                        pPlayer->m_nBileFrame[0] += 1;
                        return;
                    }
                }

                pPlayer->m_nBileFrame[0] += 1;
            }
            else if (pPlayer->m_nBileFrame[0] >= pPlayer->m_aTargetInfo.size())
            {
                pPlayer->m_aTargetInfo.clear();
                pPlayer->m_nBileFrame[0] = pPlayer->m_nBileFrame[1] = 0;
            }
            else
            {
                pPlayer->m_nBileFrame[0] += 1;
                pPlayer->m_nBileFrame[1] = 0;
            }
        }
    }

    int flags = pPlayer->GetFlags();
    if ((flags & FL_ONGROUND) && pPlayer->HasVisibleThreats()
        && (flTargetDist <= (int)(0.8 * bridge->FindConVar("z_vomit_range")->GetFloat()))
        && !pPlayer->m_bIsInCoolDown && pPlayer->m_bCanBile)
    {
        CUserCmd *cmd = pPlayer->GetCurrentCommand();
        if (!cmd)
            return;

        cmd->buttons |= IN_ATTACK;
        cmd->buttons |= IN_FORWARD;

        if (pPlayer->m_bCanBile)
        {
            int index = pPlayer->entindex();
            g_hResetBileTimer = timersys->CreateTimer(&g_BoomerTimerEvent, (bridge->FindConVar("z_vomit_duration"))->GetFloat(), (void *)(intptr_t)index, 0);
        }

        pPlayer->m_bCanBile = false;
    }

    if ((pTarget->IsIncapped() || pTarget->GetSpecialInfectedDominatingMe()) && flTargetDist <= 80.0)
    {
        if (FloatAbs(vecSelfPos.z - vecTargetPos.z) > PLAYER_HEIGHT)
            return;

        bool bUnknown;
        Vector vecTargetEyePos = pTarget->GetEyeOrigin();
        if (!IsVisiableToPlayer(vecTargetEyePos, pPlayer, CTerrorPlayer::L4D2Teams_Survivor, CTerrorPlayer::L4D2Teams_Infected, 0.0, NULL, NULL, &bUnknown))
            return;
        
        CUserCmd *cmd = pPlayer->GetCurrentCommand();
        if (!cmd)
            return;

        cmd->buttons |= IN_ATTACK2;
    }

    CUserCmd *pCmd = pPlayer->GetCurrentCommand();
    if (z_boomer_force_bile.GetBool() && (pCmd && (pCmd->buttons & IN_ATTACK))
        && !pPlayer->m_bIsInCoolDown)
    {
        pPlayer->m_bIsInCoolDown = true;
        for (int i = 1; i <= gpGlobals->maxClients; i++)
        {
            CBaseEntity *pEntity = (CBaseEntity *)UTIL_PlayerByIndex(i);
            if (!pEntity)
                continue;

            CTerrorPlayer *pTerrorPlayer = (CTerrorPlayer *)pEntity;
            if (!pTerrorPlayer || !pTerrorPlayer->IsInGame() || !pTerrorPlayer->IsSurvivor() || pPlayer->IsDead())
                continue;

            Vector vecEyePos = pTerrorPlayer->GetEyeOrigin();
            if (vecSelfEyePos.DistTo(vecEyePos) > bridge->FindConVar("z_vomit_range")->GetFloat())
                continue;

            Ray_t ray;
            trace_t tr;
            ray.Init(vecSelfEyePos, vecEyePos);
            UTIL_TraceRay(ray, MASK_VISIBLE, NULL, COLLISION_GROUP_NONE, &tr, TR_VomitClientFilter, (void *)((uint8_t *)pPlayer->entindex()));
            if (!tr.DidHit() && tr.GetEntityIndex() == pTerrorPlayer->entindex())
            {
                pTerrorPlayer->OnVomitedUpon(pPlayer, false);
                ((CTerrorBoomerVictim *)pTerrorPlayer)->m_bBiled = true;
            }
        }

        g_hResetAbilityTimer = timersys->CreateTimer(&g_BoomerTimerEvent, bridge->FindConVar("z_vomit_interval")->GetFloat(), (void *)(intptr_t)pPlayer->entindex(), 0);
    }

    Vector vecVelocity;
    pPlayer->GetVelocity(&vecVelocity);
    vec_t flCurSpeed = sqrt(pow(vecVelocity.x, 2.0f) + pow(vecVelocity.y, 2.0f));

    if (z_boomer_bhop.GetBool() && pPlayer->HasVisibleThreats() 
        && (flags & FL_ONGROUND) && (0.5f * bridge->FindConVar("z_vomit_range")->GetFloat() < flTargetDist
        && flTargetDist < 1000.0f) && flCurSpeed > 160.0f)
    {
        Vector vecBuffer = UTIL_CaculateVel(vecSelfPos, vecTargetPos, z_boomer_bhop_speed.GetFloat());
        pCmd->buttons |= IN_JUMP;
        pCmd->buttons |= IN_DUCK;

        if (DoBhop(pPlayer, pCmd->buttons, vecBuffer))
            return;
    }

    if (pPlayer->GetMoveType() & MOVETYPE_LADDER)
    {
        pCmd->buttons &= ~IN_ATTACK;
        pCmd->buttons &= ~IN_ATTACK2;
        pCmd->buttons &= ~IN_JUMP;
        pCmd->buttons &= ~IN_DUCK;
    }
}

CTerrorPlayer *BossZombiePlayerBot::OnBoomerChooseVictim(CTerrorPlayer *pLasVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer)
{
    if (this->IsDead())
        return NULL;

    Vector vecEyePos = this->GetEyeOrigin();
    for (int i = 1; i <= gpGlobals->maxClients; i++)
    {
        CTerrorPlayer *pTerrorPlayer = (CTerrorPlayer *)UTIL_PlayerByIndex(i);
        if (!pTerrorPlayer)
            continue;

        if (!pTerrorPlayer->IsInGame() || !pTerrorPlayer->IsSurvivor() || pTerrorPlayer->IsDead())
            continue;

        if (!pTerrorPlayer->IsIncapped() && !pTerrorPlayer->GetSpecialInfectedDominatingMe())
        {
            Vector vecTargetEyePos = pTerrorPlayer->GetEyeOrigin();
            if (z_boomer_bile_find_range.GetFloat() > 0.0f && vecEyePos.DistTo(vecTargetEyePos) <= z_boomer_bile_find_range.GetFloat())
            {
                Ray_t ray;
                trace_t tr;
                ray.Init(vecEyePos, vecTargetEyePos);
                UTIL_TraceRay(ray, MASK_VISIBLE, NULL, COLLISION_GROUP_NONE, &tr, TR_VomitClientFilter, (void *)((uint8_t *)this->entindex()));
                if (!tr.DidHit() && tr.GetEntityIndex() == pTerrorPlayer->entindex())
                {
                    return pTerrorPlayer;
                }
            }

        }
    }

    return NULL;
}

void CTerrorPlayer::DTRCallBack_OnVomitedUpon(CBasePlayer *pAttacker, bool bIsExplodedByBoomer)
{
    CTerrorBoomerVictim *pBoomerVictim = (CTerrorBoomerVictim *)this;
    if (!pBoomerVictim)
        return;

    pBoomerVictim->m_bBiled = true;

    CBoomer *pPlayer = (CBoomer *)pAttacker;
    if (!pPlayer)
        return;

    if (!pPlayer->IsBoomer())
        return;

    if (pPlayer->m_aTargetInfo.size() > 1)
        return;

    Vector vecEyePos = pPlayer->GetEyeOrigin();
    for (int i = 1; i <= gpGlobals->maxClients; i++)
    {
        CTerrorBoomerVictim *pTerrorPlayer = (CTerrorBoomerVictim *)UTIL_PlayerByIndex(i);
        if (!pTerrorPlayer)
            continue;

        if (!pTerrorPlayer->IsInGame() || !pTerrorPlayer->IsSurvivor() || pTerrorPlayer->IsDead() || pTerrorPlayer->m_bBiled)
            continue;

        Vector vecTargetEyePos = pTerrorPlayer->GetEyeOrigin();
        if (vecEyePos.DistTo(vecTargetEyePos) > bridge->FindConVar("z_vomit_range")->GetFloat())
            continue;

        Ray_t ray;
        trace_t tr;
        ray.Init(vecEyePos, vecTargetEyePos);
        UTIL_TraceRay(ray, MASK_VISIBLE, NULL, COLLISION_GROUP_NONE, &tr, TR_VomitClientFilter, (void *)((uint8_t *)pPlayer->entindex()));

        if (!tr.DidHit() && tr.GetEntityIndex() == pTerrorPlayer->entindex())
        {
            vec_t flAngle = GetSelfTargetAngle(pPlayer, pTerrorPlayer);
            targetInfo_t targetInfo;
            targetInfo.pPlayer = pTerrorPlayer;
            targetInfo.flAngle = flAngle;
            pPlayer->m_aTargetInfo.push_back(targetInfo);
        }
    }

    if (pPlayer->m_aTargetInfo.size() > 1)
    {
        std::sort(pPlayer->m_aTargetInfo.begin(), pPlayer->m_aTargetInfo.end(), 
            [](const targetInfo_t &a, const targetInfo_t &b) {
                return a.flAngle < b.flAngle;
            }
        );
    }
}

static bool secondCheck(CBaseEntity *pPlayer, CBaseEntity *pTarget)
{
    IGamePlayer *pGamePlayer = ((CTerrorPlayer *)pPlayer)->GetGamePlayer();
    IGamePlayer *pGameTarget = ((CTerrorPlayer *)pTarget)->GetGamePlayer();
    if (!pGamePlayer || !pGameTarget)
        return;

    Vector vecSelfEyePos, vecTargetEyePos;
    serverClients->ClientEarPosition(pGamePlayer->GetEdict(), &vecSelfEyePos);
    serverClients->ClientEarPosition(pGameTarget->GetEdict(), &vecTargetEyePos);

    vec_t flDistance = vecSelfEyePos.DistTo(vecTargetEyePos);
    if (flDistance <= bridge->FindConVar("z_vomit_range")->GetFloat() || 
        UTIL_IsInAimOffset((CBasePlayer *)pPlayer, (CBasePlayer *)pTarget, z_boomer_degree_force_bile.GetFloat()) ||
        ((CTerrorBoomerVictim *)pTarget)->m_bBiled)
    {
        return false;
    }

    Ray_t ray;
    trace_t tr;
    ray.Init(vecSelfEyePos, vecTargetEyePos);
    UTIL_TraceRay(ray, MASK_VISIBLE, NULL, COLLISION_GROUP_NONE, &tr, TR_VomitClientFilter, (void *)((uint8_t *)pTarget->entindex()));
    
    if (!tr.DidHit() || tr.GetEntityIndex() == pTarget->entindex())
        return true;

    return false;
}

static bool TR_VomitClientFilter(IHandleEntity* pHandleEntity, int contentsMask, void *data)
{
    CBaseHandle EHandle = pHandleEntity->GetRefEHandle();
    if (!EHandle.IsValid())
        return false;

    edict_t *pEdict = gamehelpers->GetHandleEntity(EHandle);
    if (!pEdict)
        return false;

    CBaseEntity *pEntity = gameents->EdictToBaseEntity(pEdict);
    if (!pEntity)
        return false;

    int index = pEntity->entindex();
    if (index > 0 && index <= gpGlobals->maxClients && ((CTerrorBoomerVictim *)pEntity)->m_bBiled)
        return false;

    return index != *(uint8_t *)data;
}

static bool DoBhop(CBasePlayer* pPlayer, int buttons, Vector vec)
{
    if (buttons & IN_FORWARD || buttons & IN_MOVELEFT || buttons & IN_MOVERIGHT)
        return ClientPush(pPlayer, vec);
}

static inline bool ClientPush(CBasePlayer* pPlayer, Vector vec)
{
    Vector vecVelocity;
    pPlayer->GetVelocity(&vecVelocity);
    vecVelocity += vec;
    if (WillHitWallOrFall(pPlayer, vecVelocity))
    {
        if (vecVelocity.Length() <= 250.0f)
        {
            VectorNormalize(vecVelocity);
            VectorScale(vecVelocity, 251.0f, vecVelocity);
        }

        pPlayer->Teleport(NULL, NULL, &vecVelocity);
        return true;
    }

    return false;
}

// false means will, true otherwise.
static bool WillHitWallOrFall(CBasePlayer* pPlayer, Vector vec)
{
    CTerrorPlayer *pTerrorPlayer = (CTerrorPlayer *)pPlayer;
    Vector vecSelfPos = pTerrorPlayer->GetAbsOrigin();
    Vector vecResult = vecSelfPos + vec;

    Vector vecMins = pTerrorPlayer->GetPlayerMins();
    Vector vecMaxs = pTerrorPlayer->GetPlayerMaxs();

    vecSelfPos.z += NAV_MESH_HEIGHT;
    vecResult.z += NAV_MESH_HEIGHT;

    trace_t tr;
    UTIL_TraceHull(vecSelfPos, vecResult, vecMins, vecMaxs, MASK_NPCSOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr, TR_EntityFilter);

    bool bHullRayHit;
    if (tr.DidHit())
    {
        bHullRayHit = true;
        if (vecSelfPos.DistTo(tr.endpos) <= NAV_MESH_HEIGHT)
            return false;
    }

    vecResult.z -= NAV_MESH_HEIGHT;
    Vector vecDownHullRayStartPos;

    if (!bHullRayHit)
    {
        vecDownHullRayStartPos = vecResult;
    }

    Vector vecDownHullRayEndPos = vecDownHullRayStartPos;
    vecDownHullRayEndPos.z -= 100000.0f;

    trace_t tr2;
    UTIL_TraceHull(vecDownHullRayStartPos, vecDownHullRayEndPos, vecMins, vecMaxs, MASK_NPCSOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr2, TR_EntityFilter);

    if (tr2.DidHit())
    {
        Vector vecDownHullRayHitPos = tr2.endpos;
        if (FloatAbs(vecDownHullRayStartPos.z - vecDownHullRayHitPos.z) > FALL_DETECT_HEIGHT)
        {
            return false;
        }

        int index = tr2.GetEntityIndex();
        if (index <= 0 || index > gpGlobals->maxClients)
            return false;

        CBaseEntity *pEntity = (CBaseEntity *)UTIL_PlayerByIndex(index);
        if (!pEntity)
            return false;

        if (V_strcmp(pEntity->GetClassName(), "trigger_hurt") == 0)
            return false;

        return true;
    }

    return false;
}

static bool TR_EntityFilter(IHandleEntity *ignore, int contentsMask)
{
    if (!ignore)
        return false;

    CBaseEntity *pEntity = CTraceFilterSimple::EntityFromEntityHandle(ignore);
    if (!pEntity)
        return false;

    int index = pEntity->entindex();
    if (index > 0 && index <= gpGlobals->maxClients)
        return false;

    const char *classname = pEntity->GetClassName();
    
    if (V_strcmp(classname, "infected") == 0
    || V_strcmp(classname, "witch") == 0
    || V_strcmp(classname, "prop_physics") == 0
    || V_strcmp(classname, "tank_rock") == 0)
    {
        return false;
    }

    return true;
}