//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: HL2MP client-side audio feedback user message handlers
//
//=============================================================================

#include "cbase.h"
#include "usermessages.h"
#include "engine/IEngineSound.h"
#include "hud_macros.h"
#include "igamesystem.h"

// ConVar definitions for client-side audio feedback settings
static ConVar rb_hitsound_enabled( "rb_hitsound_enabled", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Enable hitsound feedback" );
static ConVar rb_hitsound_volume( "rb_hitsound_volume", "0.55", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Hitsound volume", true, 0.0f, true, 1.0f );
static ConVar rb_hitsound_pitch_min( "rb_hitsound_pitch_min", "100", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Minimum pitch for low damage hitsounds", true, 70.0f, true, 150.0f );
static ConVar rb_hitsound_pitch_max( "rb_hitsound_pitch_max", "150", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Maximum pitch for high damage hitsounds", true, 100.0f, true, 180.0f );
static ConVar rb_hitsound_fixed_pitch( "rb_hitsound_fixed_pitch", "0", FCVAR_CLIENTDLL | FCVAR_HIDDEN, "Fixed pitch for hitsounds (0 = use damage-based pitch, 50-200 = fixed pitch)", true, 0.0f, true, 200.0f );
static ConVar rb_killsound_enabled( "rb_killsound_enabled", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Enable killsound feedback" );
static ConVar rb_killsound_volume( "rb_killsound_volume", "1.0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Killsound volume", true, 0.0f, true, 1.0f );

//-----------------------------------------------------------------------------
// Purpose: Handle HitSound user message from server
//-----------------------------------------------------------------------------
void __MsgFunc_HitSound( bf_read &msg )
{
	// Check if hitsounds are enabled
	if ( !rb_hitsound_enabled.GetBool() )
		return;

	// Read the damage amount and pitch from the message
	float damage = msg.ReadFloat();	

	// Check if we should use fixed pitch or recalculate using client ConVars
	int pitch;
	int fixedPitch = rb_hitsound_fixed_pitch.GetInt();
	if ( fixedPitch > 0 )
	{
		// Use fixed pitch from ConVar
		pitch = fixedPitch;
	}
	else
	{
		// Recalculate pitch using client's ConVar settings
		float pitchMin = rb_hitsound_pitch_min.GetFloat();
		float pitchMax = rb_hitsound_pitch_max.GetFloat();
		float maxDamage = 100.0f;
		
		// Clamp damage and calculate pitch using client values
		float clampedDamage = clamp( damage, 0.0f, maxDamage );
		float pitchRange = pitchMax - pitchMin;
		float calculatedPitch = pitchMin + (pitchRange * (clampedDamage / maxDamage));
		// Use ConVar bounds for clamping instead of hardcoded 50-200
		pitch = (int)clamp( calculatedPitch, pitchMin, pitchMax );
	}

	// Get volume setting
	float volume = rb_hitsound_volume.GetFloat();
	if ( volume <= 0.0f )
		return;

	// Play the hitsound with calculated pitch
	CLocalPlayerFilter filter;
	
	EmitSound_t soundParams;
	soundParams.m_pSoundName = "player/hitsound.wav";
	soundParams.m_flVolume = volume;
	soundParams.m_nChannel = CHAN_STATIC;
	soundParams.m_SoundLevel = SNDLVL_NONE;
	soundParams.m_nPitch = pitch;
	
	C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, soundParams );
}

//-----------------------------------------------------------------------------
// Purpose: Handle KillSound user message from server
//-----------------------------------------------------------------------------
void __MsgFunc_KillSound( bf_read &msg )
{
	// Check if killsounds are enabled
	if ( !rb_killsound_enabled.GetBool() )
		return;

	// Read the trigger byte (not used for anything, just protocol compliance)
	msg.ReadByte();

	// Get volume setting
	float volume = rb_killsound_volume.GetFloat();
	if ( volume <= 0.0f )
		return;

	// Play the killsound
	CLocalPlayerFilter filter;
	
	EmitSound_t soundParams;
	soundParams.m_pSoundName = "player/killsound.wav";
	soundParams.m_flVolume = volume;
	soundParams.m_nChannel = CHAN_STATIC;
	soundParams.m_SoundLevel = SNDLVL_NONE;
	soundParams.m_nPitch = PITCH_NORM;
	
	C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, soundParams );
}

//-----------------------------------------------------------------------------
// Purpose: Hook the user message handlers
//-----------------------------------------------------------------------------
void HookAudioFeedbackMessages()
{
	HOOK_MESSAGE( HitSound );
	HOOK_MESSAGE( KillSound );
}

//-----------------------------------------------------------------------------
// Purpose: Auto-register using the Source Engine's initialization system
//-----------------------------------------------------------------------------
class CAudioFeedbackInit : public CAutoGameSystem
{
public:
	virtual bool Init()
	{
		HookAudioFeedbackMessages();
		return true;
	}
};

static CAudioFeedbackInit g_AudioFeedbackInit;