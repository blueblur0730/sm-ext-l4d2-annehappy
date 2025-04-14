#include "SI/boomer.h"
#include "utils.h"
#include "in_buttons.h"

CBoomerTimerEvent g_BoomerTimerEvent;

static ITimer *g_hResetBileTimer = NULL;
static ITimer *g_hResetAbilityTimer = NULL;
static ITimer *g_hResetBiledStateTimer = NULL;

ConVar z_boomer_bhop("z_boomer_bhop", "1", FCVAR_NOTIFY | FCVAR_CHEAT, "Enable boomer bhop.", true, 0.0f, true, 1.0f);
ConVar z_boomer_bhop_speed("z_boomer_bhop_speed", "150.0", FCVAR_NOTIFY | FCVAR_CHEAT, "Boomer bhop speed.", true, 0.0f, false, 0.0f);
ConVar z_boomer_vision_up_on_vomit("z_boomer_vision_up_on_vomit", "1", FCVAR_NOTIFY | FCVAR_CHEAT, "Boomer vision will turn up when vomitting.", true, 0.0f, true, 1.0f);
ConVar z_boomer_vision_spin_on_vomit("z_boomer_vision_spin_on_vomit", "1", FCVAR_NOTIFY | FCVAR_CHEAT, "Boomer vision will spin when vomitting.", true, 0.0f, true, 1.0f);
ConVar z_boomer_force_bile("z_boomer_force_bile", "1", FCVAR_NOTIFY | FCVAR_CHEAT, "Boomer will force to vomit when survivors are in the range of vomitting.", true, 0.0f, true, 1.0f);
ConVar z_boomer_bile_find_range("z_boomer_bile_find_range", "300.0", FCVAR_NOTIFY | FCVAR_CHEAT, "Boomer will vomit to the incapacited survivors in this range first. 0 = disabled.", true, 0.0f, false, 0.0f);
ConVar z_boomer_spin_interval("z_boomer_spin_interval", "15", FCVAR_NOTIFY | FCVAR_CHEAT, "Boomer will spin to another survivors in every this many frames.", true, 0.0f, false, 0.0f);
ConVar z_boomer_degree_force_bile("z_boomer_degree_force_bile", "10", FCVAR_NOTIFY | FCVAR_CHEAT, "Boomer will force to vomit to the survivors whoes pos with boomer's eye is on this degree, 0 = disable.", true, 0.0f, false, 0.0f);
ConVar z_boomer_predict_pos("z_boomer_predict_pos", "1", FCVAR_NOTIFY | FCVAR_CHEAT, "Boomer will predict the frame of the next target's vision according to the target's angle.", true, 0.0f, true, 1.0f);


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

int CBoomerEventListner::GetEventDebugID(void)
{
    return 0;
}

void CBoomerEventListner::OnPlayerSpawned(IGameEvent *event)
{
    CBoomer *pBoomer = (CBoomer *)UTIL_PlayerByUserIdExt(event->GetInt("userid"));
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
    CBoomer *pPlayer = (CBoomer *)UTIL_PlayerByUserIdExt(event->GetInt("userid"));

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
    CBoomer *pAttacker = (CBoomer *)UTIL_PlayerByUserIdExt(event->GetInt("attacker"));
    if (!pAttacker || !pAttacker->IsBoomer())
        return;

    CTerrorBoomerVictim *pVictim = (CTerrorBoomerVictim *)UTIL_PlayerByUserIdExt(event->GetInt("userid"));

    int index = pVictim->entindex();
    if (index <= 0 || index > gpGlobals->maxClients)
        return;

    pVictim->m_bBiled = true;

    // FindVar actually can be replaced by ConVarRef, like: 
    // static ConVarRef sb_vomit_blind_time("sb_vomit_blind_time");
    // it is all the same, right?
    g_hResetBiledStateTimer = timersys->CreateTimer(&g_BoomerTimerEvent, g_pCVar->FindVar("sb_vomit_blind_time")->GetFloat(), (void *)(intptr_t)index, 0);
}

void CBoomerEventListner::OnRoundStart(IGameEvent *event)
{
    for (int i = 1; i <= gpGlobals->maxClients; i++)
    {
        CTerrorBoomerVictim *pPlayer = (CTerrorBoomerVictim *)UTIL_PlayerByIndexExt(i);
        if (!pPlayer)
            continue;

        pPlayer->m_iSecondCheckFrame = 0;
    }
}

SourceMod::ResultType CBoomerTimerEvent::OnTimer(ITimer *pTimer, void *pData)
{
    if (pTimer == g_hResetBileTimer)
    {
        int client = (int)(intptr_t)pData;
        if (client <= 0 || client > gpGlobals->maxClients)
            return Pl_Stop;

        CBoomer *pBoomer = (CBoomer *)UTIL_PlayerByIndexExt(client);
        if (!pBoomer->IsBoomer() || pBoomer->IsDead())
            return Pl_Stop;

        pBoomer->m_bCanBile = false;
        pBoomer->m_bIsInCoolDown = true;
        g_hResetAbilityTimer = timersys->CreateTimer(&g_BoomerTimerEvent, g_pCVar->FindVar("z_vomit_interval")->GetFloat(), (void *)(intptr_t)client, 0);
        return Pl_Continue;
    }

    if (pTimer == g_hResetAbilityTimer)
    {
        int client = (int)(intptr_t)pData;
        if (client <= 0 || client > gpGlobals->maxClients)
            return Pl_Stop;

        CBoomer *pBoomer = (CBoomer *)UTIL_PlayerByIndexExt(client);
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

        CTerrorBoomerVictim *pVictim = (CTerrorBoomerVictim *)UTIL_PlayerByIndexExt(client);
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

    CTerrorPlayer *pTarget = (CTerrorPlayer *)UTIL_GetClosetSurvivor(pPlayer);
    if (!pTarget)
        return;

    CVomit *pAbility = (CVomit *)pPlayer->GetAbility();
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
        if (flTargetDist <= g_pCVar->FindVar("z_vomit_range")->GetFloat())
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

            pPlayer->Teleport(NULL, &vecAngles, NULL);

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
        && (flTargetDist <= (int)(0.8 * g_pCVar->FindVar("z_vomit_range")->GetFloat()))
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
            g_hResetBileTimer = timersys->CreateTimer(&g_BoomerTimerEvent, (g_pCVar->FindVar("z_vomit_duration"))->GetFloat(), (void *)(intptr_t)index, 0);
        }

        pPlayer->m_bCanBile = false;
    }

    if ((pTarget->IsIncapped() || pTarget->GetSpecialInfectedDominatingMe()) && flTargetDist <= 80.0)
    {
        if (FloatAbs(vecSelfPos.z - vecTargetPos.z) > PLAYER_HEIGHT)
            return;

        bool bUnknown;
        Vector vecTargetEyePos = pTarget->GetEyeOrigin();
        if (!IsVisiableToPlayer(vecTargetEyePos, (CBasePlayer *)pPlayer, L4D2Teams_Survivor, L4D2Teams_Infected, 0.0, NULL, NULL, &bUnknown))
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
            CBaseEntity *pEntity = (CBaseEntity *)UTIL_PlayerByIndexExt(i);
            if (!pEntity)
                continue;

            CTerrorPlayer *pTerrorPlayer = (CTerrorPlayer *)pEntity;
            if (!pTerrorPlayer || !pTerrorPlayer->IsInGame() || !pTerrorPlayer->IsSurvivor() || pPlayer->IsDead())
                continue;

            Vector vecEyePos = pTerrorPlayer->GetEyeOrigin();
            if (vecSelfEyePos.DistTo(vecEyePos) > g_pCVar->FindVar("z_vomit_range")->GetFloat())
                continue;

            Ray_t ray;
            trace_t tr;
            ray.Init(vecSelfEyePos, vecEyePos);
            UTIL_TraceRay(ray, MASK_VISIBLE, NULL, COLLISION_GROUP_NONE, &tr, TR_VomitClientFilter, (void *)((uint8_t *)pPlayer->entindex()));
            if (!tr.DidHit() && (tr.m_pEnt && (tr.m_pEnt->entindex() == pTerrorPlayer->entindex())))
            {
                pTerrorPlayer->OnVomitedUpon(pPlayer, false);
                ((CTerrorBoomerVictim *)pTerrorPlayer)->m_bBiled = true;
            }
        }

        g_hResetAbilityTimer = timersys->CreateTimer(&g_BoomerTimerEvent, g_pCVar->FindVar("z_vomit_interval")->GetFloat(), (void *)(intptr_t)pPlayer->entindex(), 0);
    }

    Vector vecVelocity;
    pPlayer->GetVelocity(&vecVelocity, NULL);
    vec_t flCurSpeed = (vec_t)FastSqrt(vecVelocity.x * vecVelocity.x + vecVelocity.y * vecVelocity.y);

    if (z_boomer_bhop.GetBool() && pPlayer->HasVisibleThreats() 
        && (flags & FL_ONGROUND) && (0.5f * g_pCVar->FindVar("z_vomit_range")->GetFloat() < flTargetDist
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

CTerrorPlayer* BossZombiePlayerBot::OnBoomerChooseVictim(CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer)
{
    if (!this->IsBoomer() || this->IsDead())
        return NULL;

    Vector vecEyePos = this->GetEyeOrigin();
    for (int i = 1; i <= gpGlobals->maxClients; i++)
    {
        CTerrorPlayer *pTerrorPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(i);
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
                if (!tr.DidHit() && (tr.m_pEnt && (tr.m_pEnt->entindex() == pTerrorPlayer->entindex())))
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
        CTerrorBoomerVictim *pTerrorPlayer = (CTerrorBoomerVictim *)UTIL_PlayerByIndexExt(i);
        if (!pTerrorPlayer)
            continue;

        if (!pTerrorPlayer->IsInGame() || !pTerrorPlayer->IsSurvivor() || pTerrorPlayer->IsDead() || pTerrorPlayer->m_bBiled)
            continue;

        Vector vecTargetEyePos = pTerrorPlayer->GetEyeOrigin();
        if (vecEyePos.DistTo(vecTargetEyePos) > g_pCVar->FindVar("z_vomit_range")->GetFloat())
            continue;

        Ray_t ray;
        trace_t tr;
        ray.Init(vecEyePos, vecTargetEyePos);
        UTIL_TraceRay(ray, MASK_VISIBLE, NULL, COLLISION_GROUP_NONE, &tr, TR_VomitClientFilter, (void *)((uint8_t *)pPlayer->entindex()));

        if (!tr.DidHit() && (tr.m_pEnt && (tr.m_pEnt->entindex() == pTerrorPlayer->entindex())))
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
        return false;

    Vector vecSelfEyePos, vecTargetEyePos;
    serverClients->ClientEarPosition(pGamePlayer->GetEdict(), &vecSelfEyePos);
    serverClients->ClientEarPosition(pGameTarget->GetEdict(), &vecTargetEyePos);

    vec_t flDistance = vecSelfEyePos.DistTo(vecTargetEyePos);
    if (flDistance <= g_pCVar->FindVar("z_vomit_range")->GetFloat() || 
        UTIL_IsInAimOffset((CBasePlayer *)pPlayer, (CBasePlayer *)pTarget, z_boomer_degree_force_bile.GetFloat()) ||
        ((CTerrorBoomerVictim *)pTarget)->m_bBiled)
    {
        return false;
    }

    Ray_t ray;
    trace_t tr;
    ray.Init(vecSelfEyePos, vecTargetEyePos);
    UTIL_TraceRay(ray, MASK_VISIBLE, NULL, COLLISION_GROUP_NONE, &tr, TR_VomitClientFilter, (void *)((uint8_t *)pTarget->entindex()));
    
    if (!tr.DidHit() || (tr.m_pEnt && (tr.m_pEnt->entindex() == pTarget->entindex())))
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

    int index = gamehelpers->EntityToBCompatRef(pEntity);
    if (index > 0 && index <= gpGlobals->maxClients && ((CTerrorBoomerVictim *)pEntity)->m_bBiled)
        return false;

    return index != *(uint8_t *)data;
}

static bool DoBhop(CBasePlayer* pPlayer, int buttons, Vector vec)
{
    if (buttons & IN_FORWARD || buttons & IN_MOVELEFT || buttons & IN_MOVERIGHT)
        return ClientPush(pPlayer, vec);

    return false;
}