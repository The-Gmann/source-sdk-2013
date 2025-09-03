(//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"

#ifdef CLIENT_DLL
#include "c_hl2mp_player.h"
#include "prediction.h"
#define CRecipientFilter C_RecipientFilter
#else
#include "hl2mp_player.h"
#endif

#include "engine/IEngineSound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "in_buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Stealth movement cvar - server-side control for silent walking/ducking
ConVar rbsv_stealth_movement( "rbsv_stealth_movement", "1", FCVAR_REPLICATED, "Enable stealth movement (silent walking and ducking). 0 = disabled, 1 = enabled" );

extern ConVar sv_footsteps;

const char *g_ppszPlayerSoundPrefixNames[PLAYER_SOUNDS_MAX] =
{
	"NPC_Citizen",
	"NPC_CombineS",
	"NPC_MetroPolice",
};

const char *CHL2MP_Player::GetPlayerModelSoundPrefix( void )
{
	return g_ppszPlayerSoundPrefixNames[m_iPlayerSoundType];
}

void CHL2MP_Player::PrecacheFootStepSounds( void )
{
	int iFootstepSounds = ARRAYSIZE(g_ppszPlayerSoundPrefixNames);
	int i;

	for (i = 0; i < iFootstepSounds; ++i)
	{
		char szFootStepName[128];

		Q_snprintf(szFootStepName, sizeof(szFootStepName), "%s.RunFootstepLeft", g_ppszPlayerSoundPrefixNames[i]);
		PrecacheScriptSound(szFootStepName);

		Q_snprintf(szFootStepName, sizeof(szFootStepName), "%s.RunFootstepRight", g_ppszPlayerSoundPrefixNames[i]);
		PrecacheScriptSound(szFootStepName);
	}
}

//-----------------------------------------------------------------------------
// Consider the weapon's built-in accuracy, this character's proficiency with
// the weapon, and the status of the target. Use this information to determine
// how accurately to shoot at the target.
//-----------------------------------------------------------------------------
Vector CHL2MP_Player::GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget )
{
	if ( pWeapon )
		return pWeapon->GetBulletSpread( WEAPON_PROFICIENCY_PERFECT );
	
	return VECTOR_CONE_15DEGREES;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : step - 
//			fvol - 
//			force - force sound to play
//-----------------------------------------------------------------------------
void CHL2MP_Player::PlayStepSound(Vector& vecOrigin, surfacedata_t* psurface, float fvol, bool force)
{
#if defined(CLIENT_DLL)
    // During prediction, play footstep sounds only once
    if (!prediction->IsFirstTimePredicted())
        return;
#endif

    // Check for stealth movement like Counter-Strike Source - both walking and ducking
    // Only if rb_stealth_movement is enabled
    if (rbsv_stealth_movement.GetBool())
    {
        // Walking stealth: Must check walk button, ground state, movement speed, and NOT shifting
        if (m_nButtons & IN_WALK)
        {
            // Only allow stealth walking when on the ground (prevents silent jumping/landing)
            if (GetFlags() & FL_ONGROUND)
            {
                // Must NOT be holding shift/speed key to prevent silent running
                if (!(m_nButtons & IN_SPEED))
                {
                    // Get player's current movement speed
                    Vector velocity = GetAbsVelocity();
                    float currentSpeed = velocity.Length2D();
                    
                    // Use threshold you determined works for your setup
                    const float walkSpeedThreshold = 225.0f;
                    
                    // Only grant stealth walking if actually moving at walking speed or slower
                    if (currentSpeed <= walkSpeedThreshold)
                    {
                        // Skip step sounds completely when actually walking for stealth - like Counter-Strike Source
                        return;
                    }
                }
            }
            // If holding walk but shifting, not on ground, or moving too fast, play normal footsteps
        }
        
        // Ducking/crouching stealth: Apply same mechanics when ducking
        if (GetFlags() & FL_DUCKING)
        {
            // Only allow stealth ducking when on the ground (prevents silent jumping/landing while crouched)
            if (GetFlags() & FL_ONGROUND)
            {
                // Must NOT be holding shift/speed key to prevent silent crouch-running
                if (!(m_nButtons & IN_SPEED))
                {
                    // Get player's current movement speed
                    Vector velocity = GetAbsVelocity();
                    float currentSpeed = velocity.Length2D();
                    
                    // Use same threshold for ducking stealth as walking
                    const float duckSpeedThreshold = 225.0f;
                    
                    // Only grant stealth ducking if moving slowly
                    if (currentSpeed <= duckSpeedThreshold)
                    {
                        // Skip step sounds completely when actually duck-walking for stealth
                        return;
                    }
                }
            }
            // If ducking but shifting, not on ground, or moving too fast, play normal footsteps
        }
    }

    // Always play the base surface-dependent footstep sound first
    BaseClass::PlayStepSound(vecOrigin, psurface, fvol, force);

    // Only play character-specific sounds for Combine Soldiers and Metro Police
    // Skip Citizens (m_iPlayerSoundType == 0)
    if (m_iPlayerSoundType == 1 || m_iPlayerSoundType == 2) // CombineS or MetroPolice
    {
        if (gpGlobals->maxClients > 1 && !sv_footsteps.GetFloat())
            return;

        // Note: Removed FL_DUCKING check here to allow ducking stealth mechanics

        char szCharacterStepSound[128];
        if (m_Local.m_nStepside)
        {
            Q_snprintf(szCharacterStepSound, sizeof(szCharacterStepSound), "%s.RunFootstepLeft", g_ppszPlayerSoundPrefixNames[m_iPlayerSoundType]);
        }
        else
        {
            Q_snprintf(szCharacterStepSound, sizeof(szCharacterStepSound), "%s.RunFootstepRight", g_ppszPlayerSoundPrefixNames[m_iPlayerSoundType]);
        }

        CSoundParameters params;
        if (GetParametersForSound(szCharacterStepSound, params, NULL))
        {
            CRecipientFilter filter;
            filter.AddRecipientsByPAS(vecOrigin);

#ifndef CLIENT_DLL
            if (gpGlobals->maxClients > 1)
                filter.RemoveRecipientsByPVS(vecOrigin);
#endif

            EmitSound_t ep;
            ep.m_nChannel = CHAN_STATIC;
            ep.m_pSoundName = params.soundname;
            ep.m_flVolume = fvol;
            ep.m_SoundLevel = params.soundlevel;
            ep.m_nFlags = 0;
            ep.m_nPitch = params.pitch;
            ep.m_pOrigin = &vecOrigin;

            EmitSound(filter, entindex(), ep);
        }
    }
}