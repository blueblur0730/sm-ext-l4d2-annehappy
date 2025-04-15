#include "SI/boomer.h"
#include "utils.h"
#include "in_buttons.h"

CBoomerTimerEvent g_BoomerTimerEvent;

static ITimer *g_hResetBileTimer = NULL;
static ITimer *g_hResetAbilityTimer = NULL;
static ITimer *g_hResetBiledStateTimer = NULL;

std::unordered_map<int, boomerInfo_t> g_MapBoomerInfo;
std::unordered_map<int, boomerVictimInfo_t> g_MapBoomerVictimInfo;

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
    if (!strcmp(name, "player_shoved"))
    {
        OnPlayerShoved(event);
    }
    else if (!strcmp(name, "player_now_it"))
    {
        OnPlayerNowIt(event);
    }
    else if (!strcmp(name, "round_start"))
    {
        OnRoundStart(event);
    }
}

int CBoomerEventListner::GetEventDebugID(void)
{
    return EVENT_DEBUG_ID_INIT;
}

void CBoomerEventListner::OnPlayerShoved(IGameEvent *event)
{
    CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByUserIdExt(event->GetInt("userid"));

    int index = pPlayer->entindex();
    if (index <= 0 || index > gpGlobals->maxClients)
        return;

    if (!pPlayer->IsBoomer())
        return;

    if (!g_MapBoomerInfo.contains(index))
        return;

#ifdef _DEBUG
    rootconsole->ConsolePrint("### CBoomerEventListner::OnPlayerShoved");
#endif

    g_MapBoomerInfo[index].m_bCanBile = false;
    g_hResetBileTimer = timersys->CreateTimer(&g_BoomerTimerEvent, 1.5f, (void *)(intptr_t)index, 0);
}

void CBoomerEventListner::OnPlayerNowIt(IGameEvent *event)
{
    CTerrorPlayer *pAttacker = (CTerrorPlayer *)UTIL_PlayerByUserIdExt(event->GetInt("attacker"));
    if (!pAttacker || !pAttacker->IsBoomer())
        return;

    CTerrorPlayer *pVictim = (CTerrorPlayer *)UTIL_PlayerByUserIdExt(event->GetInt("userid"));
    if (!pAttacker)
        return;

    int index = pVictim->entindex();
    if (index <= 0 || index > gpGlobals->maxClients)
        return;

    if (!g_MapBoomerVictimInfo.contains(index))
        return;

#ifdef _DEBUG
    rootconsole->ConsolePrint("### CBoomerEventListner::OnPlayerNowIt");
#endif

    g_MapBoomerVictimInfo[index].m_bBiled = true;

    // FindVar actually can be replaced by ConVarRef, like: 
    // static ConVarRef sb_vomit_blind_time("sb_vomit_blind_time");
    // it is all the same, right?
    g_hResetBiledStateTimer = timersys->CreateTimer(&g_BoomerTimerEvent, g_pCVar->FindVar("sb_vomit_blind_time")->GetFloat(), (void *)(intptr_t)index, 0);
}

void CBoomerEventListner::OnRoundStart(IGameEvent *event)
{
#ifdef _DEBUG
    rootconsole->ConsolePrint("### CBoomerEventListner::OnRoundStart");
#endif

    for (int i = 1; i <= gpGlobals->maxClients; i++)
    {
        CTerrorPlayer *pPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(i);
        if (!pPlayer || !pPlayer->IsInGame() || !pPlayer->IsSurvivor())
            continue;

        if (!g_MapBoomerVictimInfo.contains(i))
            continue;

        g_MapBoomerVictimInfo[i].m_iSecondCheckFrame = 0;
    }
}

SourceMod::ResultType CBoomerTimerEvent::OnTimer(ITimer *pTimer, void *pData)
{
    if (pTimer == g_hResetBileTimer)
    {
        int client = (int)(intptr_t)pData;
        if (client <= 0 || client > gpGlobals->maxClients)
            return Pl_Stop;

        CTerrorPlayer *pBoomer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(client);
        if (!pBoomer->IsInGame() || !pBoomer->IsBoomer() || pBoomer->IsDead())
            return Pl_Stop;

        if (!g_MapBoomerInfo.contains(client))
            return Pl_Stop;

#ifdef _DEBUG
    rootconsole->ConsolePrint("### CBoomerTimerEvent::OnTimer, g_hResetBileTimer");
#endif
        g_MapBoomerInfo[client].m_bCanBile = false;
        g_MapBoomerInfo[client].m_bIsInCoolDown = true;

        g_hResetAbilityTimer = timersys->CreateTimer(&g_BoomerTimerEvent, g_pCVar->FindVar("z_vomit_interval")->GetFloat(), (void *)(intptr_t)client, 0);
        return Pl_Continue;
    }

    if (pTimer == g_hResetAbilityTimer)
    {
        int client = (int)(intptr_t)pData;
        if (client <= 0 || client > gpGlobals->maxClients)
            return Pl_Stop;

        CTerrorPlayer *pBoomer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(client);
        if (!pBoomer->IsInGame() || !pBoomer->IsBoomer() || pBoomer->IsDead())
            return Pl_Stop;
    
        if (!g_MapBoomerInfo.contains(client))
            return Pl_Stop;

#ifdef _DEBUG
    rootconsole->ConsolePrint("### CBoomerTimerEvent::OnTimer, g_hResetAbilityTimer");
#endif
        g_MapBoomerInfo[client].m_bCanBile = true;
        g_MapBoomerInfo[client].m_bIsInCoolDown = false;
    }

    if (pTimer == g_hResetBiledStateTimer)
    {
        int client = (int)(intptr_t)pData;
        if (client <= 0 || client > gpGlobals->maxClients)
            return Pl_Stop;

        CTerrorPlayer *pVictim = (CTerrorPlayer *)UTIL_PlayerByIndexExt(client);
        if (!pVictim || !pVictim->IsInGame() || !pVictim->IsSurvivor())
            return Pl_Stop;

        if (!g_MapBoomerVictimInfo.contains(client))
            return Pl_Stop;

#ifdef _DEBUG
    rootconsole->ConsolePrint("### CBoomerTimerEvent::OnTimer, g_hResetBiledStateTimer");
#endif
        g_MapBoomerVictimInfo[client].m_bBiled = false;
        g_MapBoomerVictimInfo[client].m_iSecondCheckFrame = 0;
    }

    return Pl_Stop;
}

void CBoomerTimerEvent::OnTimerEnd(ITimer *pTimer, void *pData)
{
    pTimer = nullptr;
}

void CBoomerCmdListner::OnPlayerRunCmd(CBaseEntity *pEntity, CUserCmd *pCmd)
{
    if (!pCmd)
        return;

    CTerrorPlayer *pPlayer = reinterpret_cast<CTerrorPlayer *>(pEntity);
    if (!pPlayer || !pPlayer->IsInGame() || !pPlayer->IsBoomer() || pPlayer->IsDead())
        return;

    int playerIndex = pPlayer->entindex();
    if (playerIndex <= 0 || playerIndex > gpGlobals->maxClients)
        return;

    if (!g_MapBoomerInfo.contains(playerIndex))
    {
        boomerInfo_t boomerInfo;
        boomerInfo.Init();
        g_MapBoomerInfo[playerIndex] = boomerInfo;
    }

    CTerrorPlayer *pTarget = (CTerrorPlayer *)UTIL_GetClosetSurvivor(pPlayer);
    if (!pTarget)
        return;

    int targetIndex = pTarget->entindex();
    if (targetIndex <= 0 || targetIndex > gpGlobals->maxClients)
        return;

    if (!g_MapBoomerVictimInfo.contains(targetIndex))
    {
        boomerVictimInfo_t victimInfo;
        victimInfo.Init();
        g_MapBoomerVictimInfo[targetIndex] = victimInfo;
    }

    CVomit *pAbility = reinterpret_cast<CVomit *>(pPlayer->GetAbility());
    if (!pAbility)
        return;

#ifdef _DEBUG
    rootconsole->ConsolePrint("### CBoomerCmdListner::OnPlayerRunCmd, player: %d, cmdButtons: %d", pEntity->entindex(), pCmd->buttons);
#endif

    Vector vecSelfPos = pPlayer->GetAbsOrigin();
    Vector vecTargetPos = pPlayer->GetAbsOrigin();
    Vector vecSelfEyePos = pPlayer->GetEyeOrigin();

    vec_t flTargetDist = vecSelfPos.DistTo(vecTargetPos);
    if (pPlayer->HasVisibleThreats() && !g_MapBoomerInfo[playerIndex].m_bIsInCoolDown && g_MapBoomerInfo[playerIndex].m_aTargetInfo.size() < 1)
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

            if (z_boomer_degree_force_bile.GetInt() > 0 && UTIL_IsInAimOffset(pPlayer, pTarget, z_boomer_degree_force_bile.GetFloat()) &&
                !g_MapBoomerVictimInfo[targetIndex].m_bBiled && pAbility->IsSpraying())
            {
                if (g_MapBoomerVictimInfo[targetIndex].m_iSecondCheckFrame < z_boomer_spin_interval.GetInt() && !g_MapBoomerVictimInfo[targetIndex].m_bBiled)
                {
                    g_MapBoomerVictimInfo[targetIndex].m_iSecondCheckFrame++;
                }
                else if (!g_MapBoomerVictimInfo[targetIndex].m_bBiled && secondCheck(pPlayer, pTarget))
                {
                    pTarget->OnVomitedUpon(pPlayer, false);
                    g_MapBoomerVictimInfo[targetIndex].m_bBiled = true;
                    g_MapBoomerVictimInfo[targetIndex].m_iSecondCheckFrame = 0;
                }
            }
        }
    }

    if (g_MapBoomerInfo[playerIndex].m_aTargetInfo.size() >= 1 && !g_MapBoomerInfo[playerIndex].m_bIsInCoolDown && z_boomer_vision_spin_on_vomit.GetBool())
    {
        if (g_MapBoomerInfo[playerIndex].m_nBileFrame[0] < g_MapBoomerInfo[playerIndex].m_aTargetInfo.size())
        {
            targetInfo_t targetInfo = g_MapBoomerInfo[playerIndex].m_aTargetInfo[g_MapBoomerInfo[playerIndex].m_nBileFrame[0]];
            CTerrorPlayer *pVictim = targetInfo.pPlayer;
            if (!pVictim || pVictim->IsDead() || !pVictim->IsInGame())
                return;

            int victimIndex = pVictim->entindex();
            if (victimIndex <= 0 || victimIndex > gpGlobals->maxClients)
                return;

            if (!g_MapBoomerVictimInfo.contains(victimIndex))
            {
                boomerVictimInfo_t victimInfo;
                victimInfo.Init();
                g_MapBoomerVictimInfo[victimIndex] = victimInfo;
            }
            
            vec_t flSpinAngle = targetInfo.flAngle;

            QAngle vecAngles;
            Vector vecVictimPos = pVictim->GetAbsOrigin();
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

            if (z_boomer_degree_force_bile.GetBool() && z_boomer_predict_pos.GetBool() && targetFrame == 0)
            {
                pVictim->OnVomitedUpon(pPlayer, false);
                g_MapBoomerVictimInfo[victimIndex].m_bBiled = true;
                g_MapBoomerVictimInfo[victimIndex].m_iSecondCheckFrame = 0;
                g_MapBoomerInfo[playerIndex].m_nBileFrame[0] += 1;
                return;
            }

            if (g_MapBoomerInfo[playerIndex].m_nBileFrame[1] < targetFrame)
            {
                if (z_boomer_degree_force_bile.GetBool())
                {
                    if (g_MapBoomerVictimInfo[victimIndex].m_iSecondCheckFrame < targetFrame && !g_MapBoomerVictimInfo[victimIndex].m_bBiled)
                    {
                        g_MapBoomerVictimInfo[victimIndex].m_iSecondCheckFrame++;
                    }
                    else if (!g_MapBoomerVictimInfo[victimIndex].m_bBiled && secondCheck(pPlayer, pVictim))
                    {
                        pVictim->OnVomitedUpon(pPlayer, false);
                        g_MapBoomerVictimInfo[victimIndex].m_bBiled = true;
                        g_MapBoomerVictimInfo[victimIndex].m_iSecondCheckFrame = 0;
                        g_MapBoomerInfo[playerIndex].m_nBileFrame[1] = targetFrame;
                    }
                    else if (g_MapBoomerVictimInfo[victimIndex].m_bBiled)
                    {
                        g_MapBoomerInfo[playerIndex].m_nBileFrame[1] = g_MapBoomerVictimInfo[victimIndex].m_iSecondCheckFrame = 0;
                        g_MapBoomerInfo[playerIndex].m_nBileFrame[0] += 1;
                        return;
                    }
                }

                g_MapBoomerInfo[playerIndex].m_nBileFrame[0] += 1;
            }
            else if (g_MapBoomerInfo[playerIndex].m_nBileFrame[0] >= g_MapBoomerInfo[playerIndex].m_aTargetInfo.size())
            {
                g_MapBoomerInfo[playerIndex].m_aTargetInfo.clear();
                g_MapBoomerInfo[playerIndex].m_nBileFrame[0] = g_MapBoomerInfo[playerIndex].m_nBileFrame[1] = 0;
            }
            else
            {
                g_MapBoomerInfo[playerIndex].m_nBileFrame[0] += 1;
                g_MapBoomerInfo[playerIndex].m_nBileFrame[1] = 0;
            }
        }
    }

    int flags = pPlayer->GetFlags();
    if ((flags & FL_ONGROUND) && pPlayer->HasVisibleThreats()
        && (flTargetDist <= (int)(0.8 * g_pCVar->FindVar("z_vomit_range")->GetFloat()))
        && !g_MapBoomerInfo[playerIndex].m_bIsInCoolDown && g_MapBoomerInfo[playerIndex].m_bCanBile)
    {
        pCmd->buttons |= IN_ATTACK;
        pCmd->buttons |= IN_FORWARD;

        if (g_MapBoomerInfo[playerIndex].m_bCanBile)
        {
            int index = pPlayer->entindex();
            g_hResetBileTimer = timersys->CreateTimer(&g_BoomerTimerEvent, (g_pCVar->FindVar("z_vomit_duration"))->GetFloat(), (void *)(intptr_t)index, 0);
        }

        g_MapBoomerInfo[playerIndex].m_bCanBile = false;
    }

    if ((pTarget->IsIncapped() || pTarget->GetSpecialInfectedDominatingMe()) && flTargetDist <= 80.0)
    {
        if (FloatAbs(vecSelfPos.z - vecTargetPos.z) > PLAYER_HEIGHT)
            return;

        bool bUnknown;
        Vector vecTargetEyePos = pTarget->GetEyeOrigin();
        if (!IsVisiableToPlayer(vecTargetEyePos, (CBasePlayer *)pPlayer, L4D2Teams_Survivor, L4D2Teams_Infected, 0.0, NULL, NULL, &bUnknown))
            return;

        pCmd->buttons |= IN_ATTACK2;
    }

    if (z_boomer_force_bile.GetBool() && (pCmd && (pCmd->buttons & IN_ATTACK))
        && !g_MapBoomerInfo[playerIndex].m_bIsInCoolDown)
    {
        g_MapBoomerInfo[playerIndex].m_bIsInCoolDown = true;
        for (int i = 1; i <= gpGlobals->maxClients; i++)
        {
            CBaseEntity *pEnt = (CBaseEntity *)UTIL_PlayerByIndexExt(i);
            if (!pEnt)
                continue;

            CTerrorPlayer *pTerrorPlayer = (CTerrorPlayer *)pEnt;
            if (!pTerrorPlayer || !pTerrorPlayer->IsInGame() || !pTerrorPlayer->IsSurvivor() || pPlayer->IsDead())
                continue;

            if (!g_MapBoomerVictimInfo.contains(i))
            {
                boomerVictimInfo_t victimInfo;
                victimInfo.Init();
                g_MapBoomerVictimInfo[i] = victimInfo;
            }

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
                g_MapBoomerVictimInfo[i].m_bBiled = true;
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

CTerrorPlayer* BossZombiePlayerBot::OnBoomerChooseVictim(CTerrorPlayer *pAttacker, CTerrorPlayer *pLastVictim, int targetScanFlags, CBasePlayer *pIgnorePlayer)
{
    if (!pAttacker->IsBoomer() || pAttacker->IsDead())
        return NULL;

    Vector vecEyePos = pAttacker->GetEyeOrigin();
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
                UTIL_TraceRay(ray, MASK_VISIBLE, NULL, COLLISION_GROUP_NONE, &tr, TR_VomitClientFilter, (void *)((uint8_t *)pAttacker->entindex()));

                if (!tr.DidHit() || (tr.m_pEnt && (tr.m_pEnt->entindex() == i)))
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
    CTerrorPlayer *pBoomerVictim = (CTerrorPlayer *)this;
    if (!pBoomerVictim)
        return;

    int victimIndex = pBoomerVictim->entindex();
    if (victimIndex <= 0 || victimIndex > gpGlobals->maxClients)
        return;

    if (!g_MapBoomerVictimInfo.contains(victimIndex))
    {
        boomerVictimInfo_t victimInfo;
        victimInfo.Init();
        g_MapBoomerVictimInfo[victimIndex] = victimInfo;
    }

    g_MapBoomerVictimInfo[victimIndex].m_bBiled = true;

    CTerrorPlayer *pPlayer = (CTerrorPlayer *)pAttacker;
    if (!pPlayer || !pPlayer->IsBoomer())
        return;

    int boomerIndex = pPlayer->entindex();
    if (boomerIndex <= 0 || boomerIndex > gpGlobals->maxClients)
        return;

    if (!g_MapBoomerInfo.contains(boomerIndex))
    {
        boomerInfo_t boomerInfo;
        boomerInfo.Init();
        g_MapBoomerInfo[boomerIndex] = boomerInfo;
    }

    if (g_MapBoomerInfo[boomerIndex].m_aTargetInfo.size() > 1)
        return;

    Vector vecEyePos = pPlayer->GetEyeOrigin();
    for (int i = 1; i <= gpGlobals->maxClients; i++)
    {
        CTerrorPlayer *pTerrorPlayer = (CTerrorPlayer *)UTIL_PlayerByIndexExt(i);
        if (!pTerrorPlayer)
            continue;

        if (!g_MapBoomerVictimInfo.contains(i))
        {
            boomerVictimInfo_t victimInfo;
            victimInfo.Init();
            g_MapBoomerVictimInfo[i] = victimInfo;
        }

        if (!pTerrorPlayer->IsInGame() || !pTerrorPlayer->IsSurvivor() || pTerrorPlayer->IsDead() || g_MapBoomerVictimInfo[i].m_bBiled)
            continue;

        Vector vecTargetEyePos = pTerrorPlayer->GetEyeOrigin();
        if (vecEyePos.DistTo(vecTargetEyePos) > g_pCVar->FindVar("z_vomit_range")->GetFloat())
            continue;

        Ray_t ray;
        trace_t tr;
        ray.Init(vecEyePos, vecTargetEyePos);
        UTIL_TraceRay(ray, MASK_VISIBLE, NULL, COLLISION_GROUP_NONE, &tr, TR_VomitClientFilter, (void *)((uint8_t *)pPlayer->entindex()));

        if (!tr.DidHit() || (tr.m_pEnt && (tr.m_pEnt->entindex() == i)))
        {
            vec_t flAngle = GetSelfTargetAngle(pPlayer, pTerrorPlayer);
            targetInfo_t targetInfo;
            targetInfo.pPlayer = pTerrorPlayer;
            targetInfo.flAngle = flAngle;
            g_MapBoomerInfo[boomerIndex].m_aTargetInfo.push_back(targetInfo);
        }
    }

    if (g_MapBoomerInfo[boomerIndex].m_aTargetInfo.size() > 1)
    {
        std::sort(g_MapBoomerInfo[boomerIndex].m_aTargetInfo.begin(), g_MapBoomerInfo[boomerIndex].m_aTargetInfo.end(), 
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

    int index = ((CTerrorPlayer *)pTarget)->entindex();
    if (index < 1 || index > gpGlobals->maxClients)
        return false;

    Vector vecSelfEyePos, vecTargetEyePos;
    serverClients->ClientEarPosition(pGamePlayer->GetEdict(), &vecSelfEyePos);
    serverClients->ClientEarPosition(pGameTarget->GetEdict(), &vecTargetEyePos);

    if (!g_MapBoomerVictimInfo.contains(index))
    {
        boomerVictimInfo_t victimInfo;
        victimInfo.Init();
        g_MapBoomerVictimInfo[index] = victimInfo;
    }

    vec_t flDistance = vecSelfEyePos.DistTo(vecTargetEyePos);
    if (flDistance <= g_pCVar->FindVar("z_vomit_range")->GetFloat() || 
        UTIL_IsInAimOffset((CBasePlayer *)pPlayer, (CBasePlayer *)pTarget, z_boomer_degree_force_bile.GetFloat()) ||
        g_MapBoomerVictimInfo[index].m_bBiled)
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
#ifdef _DEBUG
    rootconsole->ConsolePrint("### TR_VomitClientFilter: EHandle.IsValid(): %d.", EHandle.IsValid());
#endif
    if (!EHandle.IsValid())
        return false;

    edict_t *pEdict = gamehelpers->GetHandleEntity(EHandle);
#ifdef _DEBUG
    rootconsole->ConsolePrint("### TR_VomitClientFilter: pEdict: %d.", pEdict);
#endif
    if (!pEdict)
        return false;

    CBaseEntity *pEntity = gameents->EdictToBaseEntity(pEdict);
#ifdef _DEBUG
    rootconsole->ConsolePrint("### TR_VomitClientFilter: pEntity: %d.", pEntity);
#endif
    if (!pEntity)
        return false;

    int index = pEntity->entindex();
#ifdef _DEBUG
        rootconsole->ConsolePrint("### TR_VomitClientFilter: index: %d.", index);
#endif
    if (index < 1 || index > gpGlobals->maxClients)
        return false;

#ifdef _DEBUG
        rootconsole->ConsolePrint("### TR_VomitClientFilter: g_MapBoomerVictimInfo.contains(index): %d.", g_MapBoomerVictimInfo.contains(index));
#endif

    if (!g_MapBoomerVictimInfo.contains(index))
    {
        boomerVictimInfo_t victimInfo;
        victimInfo.Init();
        g_MapBoomerVictimInfo[index] = victimInfo;
    }
#ifdef _DEBUG
    rootconsole->ConsolePrint("### TR_VomitClientFilter: g_MapBoomerVictimInfo[index].m_bBiled: %d.", g_MapBoomerVictimInfo[index].m_bBiled);
#endif
    if (g_MapBoomerVictimInfo[index].m_bBiled)
        return false;
#ifdef _DEBUG
    rootconsole->ConsolePrint("### TR_VomitClientFilter: index != *(uint8_t *)data: %d.", index != *(uint8_t *)data);
 #endif
    return index != *(uint8_t *)data;
}

static bool DoBhop(CBasePlayer* pPlayer, int buttons, Vector vec)
{
    if (buttons & IN_FORWARD || buttons & IN_MOVELEFT || buttons & IN_MOVERIGHT)
        return ClientPush(pPlayer, vec);

    return false;
}