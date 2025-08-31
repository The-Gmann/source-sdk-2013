//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"

#include "hl2mp_player_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( basehl2mpcombatweapon, CBaseHL2MPCombatWeapon );

IMPLEMENT_NETWORKCLASS_ALIASED( BaseHL2MPCombatWeapon , DT_BaseHL2MPCombatWeapon )

BEGIN_NETWORK_TABLE( CBaseHL2MPCombatWeapon , DT_BaseHL2MPCombatWeapon )
#if !defined( CLIENT_DLL )
//	SendPropInt( SENDINFO( m_bReflectViewModelAnimations ), 1, SPROP_UNSIGNED ),
#else
//	RecvPropInt( RECVINFO( m_bReflectViewModelAnimations ) ),
#endif
END_NETWORK_TABLE()


#if !defined( CLIENT_DLL )

#include "globalstate.h"

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CBaseHL2MPCombatWeapon )

	DEFINE_FIELD( m_bLowered,			FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flRaiseTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flHolsterTime,		FIELD_TIME ),

END_DATADESC()

#else

BEGIN_PREDICTION_DATA( CBaseHL2MPCombatWeapon )
	// misyl: We are not perfect at predicting these, and we also don't have to be.
	DEFINE_PRED_FIELD( m_flTimeWeaponIdle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flNextPrimaryAttack, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flNextSecondaryAttack, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()

#endif

extern ConVar sk_auto_reload_time;

CBaseHL2MPCombatWeapon::CBaseHL2MPCombatWeapon( void )
{

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHL2MPCombatWeapon::ItemHolsterFrame( void )
{
	BaseClass::ItemHolsterFrame();

	// Must be player held
	if ( GetOwner() && GetOwner()->IsPlayer() == false )
		return;

	// We can't be active
	if ( GetOwner()->GetActiveWeapon() == this )
		return;

	// If it's been longer than three seconds, reload
	if ( ( gpGlobals->curtime - m_flHolsterTime ) > sk_auto_reload_time.GetFloat() )
	{
		// Just load the clip with no animations
		FinishReload();
		m_flHolsterTime = gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Drops the weapon into a lowered pose
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHL2MPCombatWeapon::Lower( void )
{
	//Don't bother if we don't have the animation
	if ( SelectWeightedSequence( ACT_VM_IDLE_LOWERED ) == ACTIVITY_NOT_AVAILABLE )
		return false;

	m_bLowered = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Brings the weapon up to the ready position
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHL2MPCombatWeapon::Ready( void )
{
	//Don't bother if we don't have the animation
	if ( SelectWeightedSequence( ACT_VM_LOWERED_TO_IDLE ) == ACTIVITY_NOT_AVAILABLE )
		return false;

	m_bLowered = false;	
	m_flRaiseTime = gpGlobals->curtime + 0.5f;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHL2MPCombatWeapon::Deploy( void )
{
	// If we should be lowered, deploy in the lowered position
	// We have to ask the player if the last time it checked, the weapon was lowered
	if ( GetOwner() && GetOwner()->IsPlayer() )
	{
		CHL2MP_Player *pPlayer = assert_cast<CHL2MP_Player*>( GetOwner() );
		if ( pPlayer->IsWeaponLowered() )
		{
			if ( SelectWeightedSequence( ACT_VM_IDLE_LOWERED ) != ACTIVITY_NOT_AVAILABLE )
			{
				if ( DefaultDeploy( (char*)GetViewModel(), (char*)GetWorldModel(), ACT_VM_IDLE_LOWERED, (char*)GetAnimPrefix() ) )
				{
					m_bLowered = true;

					// Stomp the next attack time to fix the fact that the lower idles are long
					pPlayer->SetNextAttack( gpGlobals->curtime + 1.0 );
					m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
					m_flNextSecondaryAttack	= gpGlobals->curtime + 1.0;
					return true;
				}
			}
		}
	}

	m_bLowered = false;
	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHL2MPCombatWeapon::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( BaseClass::Holster( pSwitchingTo ) )
	{
		SetWeaponVisible( false );
		m_flHolsterTime = gpGlobals->curtime;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHL2MPCombatWeapon::WeaponShouldBeLowered( void )
{
	// Can't be in the middle of another animation
  	if ( GetIdealActivity() != ACT_VM_IDLE_LOWERED && GetIdealActivity() != ACT_VM_IDLE &&
		 GetIdealActivity() != ACT_VM_IDLE_TO_LOWERED && GetIdealActivity() != ACT_VM_LOWERED_TO_IDLE )
  		return false;

	if ( m_bLowered )
		return true;
	
#if !defined( CLIENT_DLL )

	if ( GlobalEntity_GetState( "friendly_encounter" ) == GLOBAL_ON )
		return true;

#endif

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Allows the weapon to choose proper weapon idle animation
//-----------------------------------------------------------------------------
void CBaseHL2MPCombatWeapon::WeaponIdle( void )
{
	//See if we should idle high or low
	if ( WeaponShouldBeLowered() )
	{
		// Move to lowered position if we're not there yet
		if ( GetActivity() != ACT_VM_IDLE_LOWERED && GetActivity() != ACT_VM_IDLE_TO_LOWERED 
			 && GetActivity() != ACT_TRANSITION )
		{
			SendWeaponAnim( ACT_VM_IDLE_LOWERED );
		}
		else if ( HasWeaponIdleTimeElapsed() )
		{
			// Keep idling low
			SendWeaponAnim( ACT_VM_IDLE_LOWERED );
		}
	}
	else
	{
		// See if we need to raise immediately
		if ( m_flRaiseTime < gpGlobals->curtime && GetActivity() == ACT_VM_IDLE_LOWERED ) 
		{
			SendWeaponAnim( ACT_VM_IDLE );
		}
		else if ( HasWeaponIdleTimeElapsed() ) 
		{
			SendWeaponAnim( ACT_VM_IDLE );
		}
	}
}

#if defined( CLIENT_DLL )

#define	HL2_BOB_CYCLE_MIN	1.0f
#define	HL2_BOB_CYCLE_MAX	0.45f
#define	HL2_BOB			0.002f
#define	HL2_BOB_UP		0.5f

extern float	g_lateralBob;
extern float	g_verticalBob;

// Enhanced bobbing system with rb_ CVars - defined here for HL2MP
ConVar rb_viewmodel_bob_enabled( "rb_viewmodel_bob_enabled", "1", FCVAR_ARCHIVE | FCVAR_CLIENTDLL, "Enable viewmodel bobbing" );
ConVar rb_viewmodel_bob_scale( "rb_viewmodel_bob_scale", "1.0", FCVAR_ARCHIVE | FCVAR_CLIENTDLL, "Viewmodel bobbing intensity scale", true, 0.0f, true, 3.0f );
ConVar rb_viewmodel_bob_air_scale( "rb_viewmodel_bob_air_scale", "1.5", FCVAR_ARCHIVE | FCVAR_CLIENTDLL, "Viewmodel bobbing scale when mid-air", true, 0.0f, true, 2.5f );
ConVar rb_viewmodel_bob_air_enabled( "rb_viewmodel_bob_air_enabled", "1", FCVAR_ARCHIVE | FCVAR_CLIENTDLL, "Enable enhanced air bobbing effects" );

// Landing bump system - REMOVED

// Advanced air behavior ConVars - hidden from typical users
static ConVar rb_viewmodel_bob_air_transition_in( "rb_viewmodel_bob_air_transition_in", "2.5", FCVAR_HIDDEN | FCVAR_CLIENTDLL, "Speed when transitioning to air state", true, 0.5f, true, 8.0f );
static ConVar rb_viewmodel_bob_air_transition_out( "rb_viewmodel_bob_air_transition_out", "1.5", FCVAR_HIDDEN | FCVAR_CLIENTDLL, "Speed when transitioning back to ground", true, 0.5f, true, 8.0f );
static ConVar rb_viewmodel_bob_air_smoothing( "rb_viewmodel_bob_air_smoothing", "1.2", FCVAR_HIDDEN | FCVAR_CLIENTDLL, "Air transition smoothing curve power (higher = smoother)", true, 0.5f, true, 3.0f );
static ConVar rb_viewmodel_bob_air_velocity_scale( "rb_viewmodel_bob_air_velocity_scale", "0.4", FCVAR_HIDDEN | FCVAR_CLIENTDLL, "How much air velocity affects bobbing", true, 0.0f, true, 1.5f );
static ConVar rb_viewmodel_bob_air_weight_effect( "rb_viewmodel_bob_air_weight_effect", "0.8", FCVAR_HIDDEN | FCVAR_CLIENTDLL, "Intensity of weight simulation in air", true, 0.0f, true, 2.0f );
static ConVar rb_viewmodel_bob_air_minimum_speed( "rb_viewmodel_bob_air_minimum_speed", "50.0", FCVAR_HIDDEN | FCVAR_CLIENTDLL, "Minimum effective speed for air bobbing", true, 0.0f, true, 200.0f );

// Hidden obsolete CVars - keeping them for compatibility but hiding them
static ConVar	cl_bobcycle( "cl_bobcycle","0.8", FCVAR_HIDDEN );
static ConVar	cl_bob( "cl_bob","0.002", FCVAR_HIDDEN );
static ConVar	cl_bobup( "cl_bobup","0.5", FCVAR_HIDDEN );

// Register these cvars if needed for easy tweaking
static ConVar	v_iyaw_cycle( "v_iyaw_cycle", "2", FCVAR_REPLICATED | FCVAR_CHEAT );
static ConVar	v_iroll_cycle( "v_iroll_cycle", "0.5", FCVAR_REPLICATED | FCVAR_CHEAT );
static ConVar	v_ipitch_cycle( "v_ipitch_cycle", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
static ConVar	v_iyaw_level( "v_iyaw_level", "0.3", FCVAR_REPLICATED | FCVAR_CHEAT );
static ConVar	v_iroll_level( "v_iroll_level", "0.1", FCVAR_REPLICATED | FCVAR_CHEAT );
static ConVar	v_ipitch_level( "v_ipitch_level", "0.3", FCVAR_REPLICATED | FCVAR_CHEAT );

// Shared air state data structure to synchronize between CalcViewmodelBob and AddViewmodelBob
struct ViewModelAirState
{
	float airTransitionBlend = 0.0f;
	float lastGroundState = 1.0f; // 1.0f = on ground, 0.0f = in air
	float airStartTime = 0.0f;
	float smoothedVerticalVel = 0.0f;
	float lastVerticalVel = 0.0f;
	float airborneTime = 0.0f;
	// Landing bump functionality removed
};

static ViewModelAirState s_airState;

//-----------------------------------------------------------------------------
// Purpose: Enhanced viewmodel bobbing with improved air behavior and smooth transitions
// Output : float
//-----------------------------------------------------------------------------
float CBaseHL2MPCombatWeapon::CalcViewmodelBob( void )
{
	static float bobtime;
	static float lastbobtime;
	float cycle;
	
	CBasePlayer *player = ToBasePlayer( GetOwner() );
	//Assert( player );

	// Check if bobbing is disabled or player is invalid
	if ( !rb_viewmodel_bob_enabled.GetBool() || !player )
	{
		g_verticalBob = 0.0f;
		g_lateralBob = 0.0f;
		// Reset air state when player is invalid
		if ( !player )
		{
			s_airState.lastGroundState = 1.0f;
			s_airState.airTransitionBlend = 0.0f;
			s_airState.airStartTime = 0.0f;
			s_airState.airborneTime = 0.0f;
			s_airState.smoothedVerticalVel = 0.0f;
		}
		return 0.0f;
	}

	if ( ( !gpGlobals->frametime ) || ( player == NULL ) )
	{
		//NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
		return 0.0f;// just use old value
	}

	//Find the speed of the player
	float speed = player->GetLocalVelocity().Length2D();

	//FIXME: This maximum speed value must come from the server.
	//		 MaxSpeed() is not sufficient for dealing with sprinting - jdw

	speed = clamp( speed, -320, 320 );

	// Enhanced air state tracking and velocity management
	bool bOnGround = (player->GetGroundEntity() != NULL);
	float targetGroundState = bOnGround ? 1.0f : 0.0f;
	float currentVerticalVel = player->GetAbsVelocity().z;
	
	// Handle air/ground state transitions
	if ( !bOnGround && s_airState.lastGroundState > 0.5f )
	{
		// Becoming airborne - initialize air tracking
		s_airState.airStartTime = gpGlobals->curtime;
		s_airState.airborneTime = 0.0f;
	}
	else if ( bOnGround && s_airState.lastGroundState < 0.5f )
	{
		// Just landed - landing bump functionality removed
	}
	else if ( bOnGround && s_airState.lastGroundState > 0.8f )
	{
		// Fully grounded - reset air tracking
		s_airState.airStartTime = 0.0f;
		s_airState.airborneTime = 0.0f;
	}
	else if ( s_airState.airStartTime > 0.0f )
	{
		// Update airborne time
		s_airState.airborneTime = gpGlobals->curtime - s_airState.airStartTime;
	}
	
	// Smooth vertical velocity tracking
	if ( !bOnGround )
	{
		// Active smoothing while airborne
		float smoothingFactor = 0.12f;
		s_airState.smoothedVerticalVel = Lerp( smoothingFactor, s_airState.smoothedVerticalVel, currentVerticalVel );
	}
	else if ( s_airState.lastGroundState > 0.8f )
	{
		// Gradual reset when grounded
		s_airState.smoothedVerticalVel = Lerp( 0.1f, s_airState.smoothedVerticalVel, 0.0f );
	}
	s_airState.lastVerticalVel = currentVerticalVel;
	
	// Smooth air-ground transition system
	if ( rb_viewmodel_bob_air_enabled.GetBool() )
	{
		// Calculate transition speeds based on direction
		float transitionSpeed = (targetGroundState < s_airState.lastGroundState) ? 
			rb_viewmodel_bob_air_transition_in.GetFloat() : rb_viewmodel_bob_air_transition_out.GetFloat();
		
		// Smooth state transition
		s_airState.lastGroundState = Approach( targetGroundState, s_airState.lastGroundState, gpGlobals->frametime * transitionSpeed );
		
		// Apply smoothing curve for natural feel
		float rawBlend = 1.0f - s_airState.lastGroundState;
		float smoothingPower = rb_viewmodel_bob_air_smoothing.GetFloat();
		float smoothedBlend = rawBlend * rawBlend * (3.0f - 2.0f * rawBlend); // Smoothstep
		s_airState.airTransitionBlend = pow( smoothedBlend, 1.0f / smoothingPower );
	}
	else
	{
		// Air effects disabled - reset to ground state
		s_airState.lastGroundState = 1.0f;
		s_airState.airTransitionBlend = 0.0f;
		s_airState.airStartTime = 0.0f;
		s_airState.airborneTime = 0.0f;
	}

	// Calculate bob scaling with air state considerations
	float bob_scale = rb_viewmodel_bob_scale.GetFloat();
	
	if ( s_airState.airTransitionBlend > 0.0f )
	{
		// Blend between ground and air bob scaling
		float airBobScale = bob_scale * rb_viewmodel_bob_air_scale.GetFloat();
		bob_scale = Lerp( s_airState.airTransitionBlend, bob_scale, airBobScale );
		
		// Adjust speed calculation for air movement
		float velocityInfluence = rb_viewmodel_bob_air_velocity_scale.GetFloat();
		float baseAirSpeed = MAX( speed * 0.4f, rb_viewmodel_bob_air_minimum_speed.GetFloat() );
		float verticalSpeedFactor = 1.0f + (fabs(s_airState.smoothedVerticalVel) / 800.0f) * velocityInfluence;
		verticalSpeedFactor = clamp( verticalSpeedFactor, 0.5f, 2.0f );
		
		speed = Lerp( s_airState.airTransitionBlend, speed, baseAirSpeed * verticalSpeedFactor );
	}

	float bob_offset = RemapVal( speed, 0, 320, 0.0f, 1.0f );
	
	bobtime += ( gpGlobals->curtime - lastbobtime ) * bob_offset;
	lastbobtime = gpGlobals->curtime;

	//Calculate the vertical bob
	cycle = bobtime - (int)(bobtime/HL2_BOB_CYCLE_MAX)*HL2_BOB_CYCLE_MAX;
	cycle /= HL2_BOB_CYCLE_MAX;

	if ( cycle < HL2_BOB_UP )
	{
		cycle = M_PI * cycle / HL2_BOB_UP;
	}
	else
	{
		cycle = M_PI + M_PI*(cycle-HL2_BOB_UP)/(1.0 - HL2_BOB_UP);
	}
	
	// Base vertical bob calculation
	g_verticalBob = speed*0.005f * bob_scale;
	g_verticalBob = g_verticalBob*0.3 + g_verticalBob*0.7*sin(cycle);

	// Enhanced air physics simulation
	float airRaiseAmount = 0.0f;
	if ( s_airState.airTransitionBlend > 0.0f && s_airState.airStartTime > 0.0f )
	{
		float airTime = clamp( s_airState.airborneTime, 0.0f, 6.0f );
		float velocityFactor = clamp( -s_airState.smoothedVerticalVel / 600.0f, -1.0f, 1.0f );
		float timeFactor = 1.0f - exp( -airTime * 1.0f );
		float weightEffect = rb_viewmodel_bob_air_weight_effect.GetFloat();
		
		// Weight simulation: falling = heavier, rising = lighter
		float weightFactor = 1.0f + (velocityFactor * 0.2f * weightEffect);
		weightFactor = clamp( weightFactor, 0.6f, 1.4f );
		
		// Combine factors for air movement effect
		float combinedFactor = velocityFactor * timeFactor * s_airState.airTransitionBlend * weightFactor;
		float maxRaise = 0.9f * weightEffect;
		airRaiseAmount = maxRaise * combinedFactor;
		
		// Add subtle oscillation for dynamic air feel
		float oscillationAmp = 0.12f * s_airState.airTransitionBlend * weightEffect;
		airRaiseAmount += sin( airTime * 0.6f ) * oscillationAmp;
	}
	
	// Landing bump effect removed

	// Add the air raise to vertical bob
	g_verticalBob += airRaiseAmount;

	g_verticalBob = clamp( g_verticalBob, -4.0f * bob_scale, (3.0f * bob_scale) + fabs(airRaiseAmount) );

	//Calculate the lateral bob
	cycle = bobtime - (int)(bobtime/HL2_BOB_CYCLE_MAX*2)*HL2_BOB_CYCLE_MAX*2;
	cycle /= HL2_BOB_CYCLE_MAX*2;

	if ( cycle < HL2_BOB_UP )
	{
		cycle = M_PI * cycle / HL2_BOB_UP;
	}
	else
	{
		cycle = M_PI + M_PI*(cycle-HL2_BOB_UP)/(1.0 - HL2_BOB_UP);
	}

	g_lateralBob = speed*0.005f * bob_scale;
	g_lateralBob = g_lateralBob*0.3 + g_lateralBob*0.7*sin(cycle);
	g_lateralBob = clamp( g_lateralBob, -7.0f * bob_scale, 4.0f * bob_scale );
	
	//NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&angles - 
//			viewmodelindex - 
//-----------------------------------------------------------------------------
void CBaseHL2MPCombatWeapon::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{
	Vector	forward, right;
	AngleVectors( angles, &forward, &right, NULL );

	CBasePlayer *player = ToBasePlayer( GetOwner() );
	
	// Safety check - if player is invalid (e.g., when dying), reset air state and return
	if ( !player )
	{
		// Reset air state when player becomes invalid
		s_airState.lastGroundState = 1.0f;
		s_airState.airTransitionBlend = 0.0f;
		s_airState.airStartTime = 0.0f;
		s_airState.airborneTime = 0.0f;
		s_airState.smoothedVerticalVel = 0.0f;
		return;
	}
	
	bool bOnGround = (player->GetGroundEntity() != NULL);
	float targetGroundState = bOnGround ? 1.0f : 0.0f;
	
	// Update shared air state (synchronized with CalcViewmodelBob)
	if ( !bOnGround && s_airState.lastGroundState > 0.5f )
	{
		// Just became airborne, record the time (if not already set)
		if ( s_airState.airStartTime <= 0.0f )
		{
			s_airState.airStartTime = gpGlobals->curtime;
			s_airState.airborneTime = 0.0f;
		}
	}
	else if ( bOnGround && s_airState.lastGroundState < 0.5f )
	{
		// Just landed - landing bump functionality removed
	}
	else if ( bOnGround && s_airState.lastGroundState > 0.8f )
	{
		// Fully grounded - safe to reset air time
		s_airState.airStartTime = 0.0f;
		s_airState.airborneTime = 0.0f;
	}
	
	// Ensure air state is synchronized (in case CalcViewmodelBob wasn't called)
	if ( rb_viewmodel_bob_air_enabled.GetBool() )
	{
		float transitionSpeed = (targetGroundState < s_airState.lastGroundState) ? 
			rb_viewmodel_bob_air_transition_in.GetFloat() : rb_viewmodel_bob_air_transition_out.GetFloat();
		
		s_airState.lastGroundState = Approach( targetGroundState, s_airState.lastGroundState, gpGlobals->frametime * transitionSpeed );
		
		float rawBlend = 1.0f - s_airState.lastGroundState;
		float smoothingPower = rb_viewmodel_bob_air_smoothing.GetFloat();
		// Use smoothstep-like curve for better transition feel
		float smoothedBlend = rawBlend * rawBlend * (3.0f - 2.0f * rawBlend);
		s_airState.airTransitionBlend = pow( smoothedBlend, 1.0f / smoothingPower );
	}
	else
	{
		s_airState.lastGroundState = 1.0f;
		s_airState.airTransitionBlend = 0.0f;
		s_airState.airStartTime = 0.0f;
		s_airState.airborneTime = 0.0f;
	}
	
	CalcViewmodelBob();

	// Apply bob, but scaled down to 40%
	VectorMA( origin, g_verticalBob * 0.1f, forward, origin );
	
	// Z bob a bit more
	origin[2] += g_verticalBob * 0.1f;
	
	// bob the angles
	angles[ ROLL ]	+= g_verticalBob * 0.5f;
	angles[ PITCH ]	-= g_verticalBob * 0.4f;

	angles[ YAW ]	-= g_lateralBob  * 0.3f;

	VectorMA( origin, g_lateralBob * 0.8f, right, origin );
	
	// Enhanced air tilt effects with optimized calculations
	if ( s_airState.airTransitionBlend > 0.0f && s_airState.airStartTime > 0.0f )
	{
		// Update airborne time if needed
		if ( s_airState.airborneTime <= 0.0f )
		{
			s_airState.airborneTime = gpGlobals->curtime - s_airState.airStartTime;
		}
		
		// Sync velocity tracking with CalcViewmodelBob
		float currentVerticalVel = player->GetAbsVelocity().z;
		if ( s_airState.smoothedVerticalVel == 0.0f )
		{
			s_airState.smoothedVerticalVel = currentVerticalVel;
		}
		else
		{
			s_airState.smoothedVerticalVel = Lerp( 0.15f, s_airState.smoothedVerticalVel, currentVerticalVel );
		}
		
		// Calculate air effects
		float airTime = clamp( s_airState.airborneTime, 0.0f, 6.0f );
		float velocityFactor = clamp( -s_airState.smoothedVerticalVel / 600.0f, -0.8f, 0.8f );
		float timeFactor = 1.0f - exp( -airTime * 0.8f );
		float weightEffect = rb_viewmodel_bob_air_weight_effect.GetFloat();
		
		// Weight modifier for tilt intensity
		float weightModifier = 1.0f + (velocityFactor > 0 ? velocityFactor * 0.2f * weightEffect : velocityFactor * 0.1f * weightEffect);
		weightModifier = clamp( weightModifier, 0.6f, 1.4f );
		
		// Air tilt effect
		float combinedFactor = velocityFactor * timeFactor * s_airState.airTransitionBlend * weightModifier;
		float maxTiltDegrees = 2.7f * weightEffect;
		float airTiltAmount = maxTiltDegrees * combinedFactor;
		angles[ PITCH ] -= airTiltAmount;
		
		// Horizontal movement roll effect
		Vector2D horizontalVel = Vector2D( player->GetAbsVelocity().x, player->GetAbsVelocity().y );
		float horizontalSpeed = horizontalVel.Length();
		if ( horizontalSpeed > 150.0f )
		{
			float rollInfluence = clamp( horizontalSpeed / 800.0f, 0.0f, 1.0f ) * 0.6f * s_airState.airTransitionBlend;
			angles[ ROLL ] += sin( airTime * 1.2f ) * rollInfluence;
		}
		
		// Extended air breathing effect
		if ( airTime > 1.5f )
		{
			float breathingAmp = 0.225f * s_airState.airTransitionBlend * weightEffect;
			angles[ PITCH ] += sin( airTime * 1.0f ) * breathingAmp;
		}
	}
}

//-----------------------------------------------------------------------------
Vector CBaseHL2MPCombatWeapon::GetBulletSpread( WeaponProficiency_t proficiency )
{
	return BaseClass::GetBulletSpread( proficiency );
}

//-----------------------------------------------------------------------------
float CBaseHL2MPCombatWeapon::GetSpreadBias( WeaponProficiency_t proficiency )
{
	return BaseClass::GetSpreadBias( proficiency );
}
//-----------------------------------------------------------------------------

const WeaponProficiencyInfo_t *CBaseHL2MPCombatWeapon::GetProficiencyValues()
{
	return NULL;
}

#else

// Server stubs
float CBaseHL2MPCombatWeapon::CalcViewmodelBob( void )
{
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&angles - 
//			viewmodelindex - 
//-----------------------------------------------------------------------------
void CBaseHL2MPCombatWeapon::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{
}


//-----------------------------------------------------------------------------
Vector CBaseHL2MPCombatWeapon::GetBulletSpread( WeaponProficiency_t proficiency )
{
	Vector baseSpread = BaseClass::GetBulletSpread( proficiency );

	const WeaponProficiencyInfo_t *pProficiencyValues = GetProficiencyValues();
	float flModifier = (pProficiencyValues)[ proficiency ].spreadscale;
	return ( baseSpread * flModifier );
}

//-----------------------------------------------------------------------------
float CBaseHL2MPCombatWeapon::GetSpreadBias( WeaponProficiency_t proficiency )
{
	const WeaponProficiencyInfo_t *pProficiencyValues = GetProficiencyValues();
	return (pProficiencyValues)[ proficiency ].bias;
}

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CBaseHL2MPCombatWeapon::GetProficiencyValues()
{
	return GetDefaultProficiencyValues();
}

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CBaseHL2MPCombatWeapon::GetDefaultProficiencyValues()
{
	// Weapon proficiency table. Keep this in sync with WeaponProficiency_t enum in the header!!
	static WeaponProficiencyInfo_t g_BaseWeaponProficiencyTable[] =
	{
		{ 2.50, 1.0	},
		{ 2.00, 1.0	},
		{ 1.50, 1.0	},
		{ 1.25, 1.0 },
		{ 1.00, 1.0	},
	};

	COMPILE_TIME_ASSERT( ARRAYSIZE(g_BaseWeaponProficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);

	return g_BaseWeaponProficiencyTable;
}

#endif
