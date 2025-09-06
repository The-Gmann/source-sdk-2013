//========= Copyright Valve Corporation, All rights reserved. ============//

#include "cbase.h"
#include <ctime>

#include "hl2mp_bot.h"
#include "hl2mp_bot_locomotion.h"
#include "particle_parse.h"
#include "hl2mp_gamerules.h"

extern ConVar falldamage;
extern ConVar hl2_normspeed;
extern ConVar hl2_sprintspeed;

// Debug ConVar for sprint tracking
ConVar bot_debug_sprint( "bot_debug_sprint", "0", FCVAR_CHEAT, "Show bot sprint debug messages with timestamps and duration" );

// Sprint behavior ConVars
ConVar bot_sprint_when_chasing( "bot_sprint_when_chasing", "1", FCVAR_CHEAT, "Bots sprint when chasing enemies" );
ConVar bot_sprint_when_fleeing( "bot_sprint_when_fleeing", "1", FCVAR_CHEAT, "Bots sprint when fleeing from enemies" );
ConVar bot_sprint_long_distance( "bot_sprint_long_distance", "300", FCVAR_CHEAT, "Distance threshold for sprinting to objectives" );
ConVar bot_sprint_low_health( "bot_sprint_low_health", "50", FCVAR_CHEAT, "Health threshold for emergency sprinting" );
ConVar bot_sprint_min_duration( "bot_sprint_min_duration", "2.0", FCVAR_CHEAT, "Minimum time to sprint before allowing state change" );
ConVar bot_nav_crouch( "bot_nav_crouch", "1", FCVAR_CHEAT );

//-----------------------------------------------------------------------------------------
void CHL2MPBotLocomotion::Update( void )
{
	BaseClass::Update();

	CHL2MPBot* me = ToHL2MPBot( GetBot()->GetEntity() );
	if ( !me )
	{
		return;
	}

	// Determine if bot should sprint based on tactical situation
	bool shouldSprint = ShouldSprint( me );
	
	// Add sprint state stability - prevent rapid switching
	if ( m_isSprinting )
	{
		// If currently sprinting, require minimum duration before allowing stop
		float sprintDuration = gpGlobals->curtime - m_sprintStartTime;
		if ( sprintDuration < bot_sprint_min_duration.GetFloat() )
		{
			// Force continue sprinting for minimum duration
			shouldSprint = true;
		}
	}
	
	// Enhanced debug output for sprint state analysis
	if ( bot_debug_sprint.GetBool() )
	{
		// Calculate current movement metrics
		Vector velocity = me->GetAbsVelocity();
		float currentSpeed = velocity.Length2D();
		float desiredSpeed = GetDesiredSpeed();
		bool isMoving = (desiredSpeed > 0.0f && currentSpeed > 10.0f);
		
		// Get sprint decision context
		const CKnownEntity *primaryThreat = me->GetVisionInterface()->GetPrimaryKnownThreat();
		bool hasVisibleThreat = (primaryThreat && primaryThreat->IsVisibleRecently());
		float threatRange = hasVisibleThreat ? me->GetRangeTo( primaryThreat->GetEntity() ) : -1.0f;
		
		// Determine movement state
		const char* movementState;
		if ( !isMoving )
		{
			movementState = "IDLE";
		}
		else if ( m_isSprinting )
		{
			movementState = "SPRINTING";
		}
		else if ( currentSpeed > hl2_normspeed.GetFloat() * 0.8f )
		{
			movementState = "RUNNING";
		}
		else
		{
			movementState = "WALKING";
		}
		
		// Get sprint decision reason
		const char* sprintReason = "None";
		if ( shouldSprint )
		{
			if ( me->GetHealth() < (bot_sprint_low_health.GetInt() / 2) )
			{
				sprintReason = "Emergency-LowHealth";
			}
			else if ( hasVisibleThreat && threatRange < 300.0f )
			{
				sprintReason = "Combat-Fleeing";
			}
			else if ( hasVisibleThreat && threatRange > 200.0f && threatRange < 800.0f )
			{
				sprintReason = "Combat-Chasing";
			}
			else
			{
				const PathFollower *path = me->GetCurrentPath();
				if ( path && path->IsValid() && path->GetLength() > bot_sprint_long_distance.GetFloat() )
				{
					sprintReason = "Navigation-LongDistance";
				}
				else
				{
					sprintReason = "Tactical";
				}
			}
		}
		else if ( hasVisibleThreat && threatRange < 200.0f )
		{
			sprintReason = "Stealth-Approach";
		}
		
		// Output detailed debug information
		if ( isMoving || m_isSprinting )
		{
			time_t rawtime = (time_t)gpGlobals->curtime;
			struct tm* timeinfo = localtime(&rawtime);
			
			float sprintDuration = m_isSprinting ? (gpGlobals->curtime - m_sprintStartTime) : 0.0f;
			float buttonRefreshAge = m_isSprinting ? (gpGlobals->curtime - m_lastSprintButtonPress) : 0.0f;
			
			DevMsg("[%02d:%02d:%02d] %s: %s | Speed: %.1f/%.1f (%.0f%%) | HP: %d | Threat: %s@%.0fu | Reason: %s",
				timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
				me->GetPlayerName(),
				movementState,
				currentSpeed,
				m_isSprinting ? hl2_sprintspeed.GetFloat() : hl2_normspeed.GetFloat(),
				m_isSprinting ? (currentSpeed / hl2_sprintspeed.GetFloat()) * 100.0f : (currentSpeed / hl2_normspeed.GetFloat()) * 100.0f,
				me->GetHealth(),
				hasVisibleThreat ? "YES" : "NO",
				threatRange,
				sprintReason);
			
			if ( m_isSprinting )
			{
				DevMsg(" | Duration: %.2fs | Refresh: %.2fs", sprintDuration, buttonRefreshAge);
			}
			
			// Add movement flags info
			if ( me->GetFlags() & FL_DUCKING )
			{
				DevMsg(" | CROUCHING");
			}
			if ( me->GetWaterLevel() > 0 )
			{
				DevMsg(" | WATER:%d", me->GetWaterLevel());
			}
			if ( !IsOnGround() )
			{
				DevMsg(" | AIRBORNE");
			}
			
			DevMsg("\n");
		}
	}

	// Apply sprint state changes - use continuous button management for proper sprint behavior
	if ( shouldSprint )
	{
		if ( !m_isSprinting )
		{
			// Start sprinting - press and hold the button for extended duration
			me->PressWalkButton( 5.0f ); // Hold for 5 seconds
			m_isSprinting = true;
			m_sprintStartTime = gpGlobals->curtime;
			m_lastSprintButtonPress = gpGlobals->curtime;
		}
		else
		{
			// Continue sprinting - refresh the button hold well before it expires
			if ( (gpGlobals->curtime - m_lastSprintButtonPress) >= 4.0f )
			{
				me->PressWalkButton( 5.0f ); // Refresh button hold
				m_lastSprintButtonPress = gpGlobals->curtime;
			}
		}
	}
	else
	{
		if ( m_isSprinting )
		{
			// Stop sprinting - explicitly release the button
			me->ReleaseWalkButton();
			m_isSprinting = false;
			m_sprintStartTime = 0.0f;
			m_lastSprintButtonPress = 0.0f;
		}
	}

	// Manage crouch-jump behavior - but don't interfere with sprinting
	if ( IsOnGround() )
	{
		me->ReleaseCrouchButton();

		if ( bot_nav_crouch.GetBool() )
		{
			const PathFollower *path = me->GetCurrentPath();
			if ( path && path->GetCurrentGoal() && path->GetCurrentGoal()->area )
			{
				if ( path->GetCurrentGoal()->area->GetAttributes() & NAV_MESH_CROUCH )
				{
					// moving through a crouch area
					me->PressCrouchButton( 0.3f );
				}
			}
		}
	}
	else
	{
		// In air - crouch for better jump control, but shorter duration when sprinting
		float crouchDuration = shouldSprint ? 0.1f : 0.3f;
		me->PressCrouchButton( crouchDuration );
	}
}


//-----------------------------------------------------------------------------------------
// Move directly towards the given position
void CHL2MPBotLocomotion::Approach( const Vector& pos, float goalWeight )
{
	BaseClass::Approach( pos, goalWeight );
}


//-----------------------------------------------------------------------------------------
// Distance at which we will die if we fall
float CHL2MPBotLocomotion::GetDeathDropHeight( void ) const
{
	CHL2MPBot* me = ( CHL2MPBot* )GetBot()->GetEntity();

	// misyl: Fall damage only deals 10 health otherwise. 
	if ( falldamage.GetInt() != 1 )
	{
		if ( me->GetHealth() > 10.0f )
			return MAX_COORD_FLOAT;

		// #define PLAYER_MAX_SAFE_FALL_SPEED	526.5f // approx 20 feet sqrt( 2 * gravity * 20 * 12 )
		return 240.0f;
	}
	else
	{
		return 1000.0f;
	}
}


//-----------------------------------------------------------------------------------------
// Get maximum running speed - now returns normal speed, sprinting handled by button press
float CHL2MPBotLocomotion::GetRunSpeed( void ) const
{
	// Return normal speed - sprinting is handled by pressing IN_SPEED button
	return hl2_normspeed.GetFloat();
}

//-----------------------------------------------------------------------------------------
// Determine if bot should sprint based on tactical situation
bool CHL2MPBotLocomotion::ShouldSprint( CHL2MPBot *me ) const
{
	if ( !me || !me->IsAlive() )
		return false;

	// Don't sprint if crouching
	if ( me->GetFlags() & FL_DUCKING )
		return false;

	// Don't sprint underwater
	if ( me->GetWaterLevel() >= 3 )
		return false;

	// Get current intention and known threats
	const CKnownEntity *primaryThreat = me->GetVisionInterface()->GetPrimaryKnownThreat();
	bool hasVisibleThreat = (primaryThreat && primaryThreat->IsVisibleRecently());
	
	// Check if we're in combat or fleeing (high priority sprinting)
	if ( hasVisibleThreat )
	{
		float threatRange = me->GetRangeTo( primaryThreat->GetEntity() );
		
		// Sprint when chasing enemies at medium-long range (reduced threshold)
		if ( bot_sprint_when_chasing.GetBool() && threatRange > 150.0f && threatRange < 1000.0f )
		{
			return true;
		}
		
		// Sprint when fleeing from close threats or low health (increased threshold)
		if ( bot_sprint_when_fleeing.GetBool() )
		{
			if ( threatRange < 400.0f || me->GetHealth() < bot_sprint_low_health.GetInt() )
			{
				return true;
			}
		}
	}
	
	// Sprint for long-distance navigation when no immediate threats
	if ( !hasVisibleThreat || primaryThreat->GetTimeSinceLastSeen() > 2.0f )
	{
		// Check if we have a long-distance goal
		const PathFollower *path = me->GetCurrentPath();
		if ( path && path->IsValid() )
		{
			float pathLength = path->GetLength();
			if ( pathLength > bot_sprint_long_distance.GetFloat() )
			{
				return true;
			}
		}
		
		// Sprint when moving at general high speed for more aggressive movement
		Vector velocity = me->GetAbsVelocity();
		if ( velocity.Length2D() > 100.0f )
		{
			return true;
		}
	}
	
	// Sprint when health is critically low (emergency situations)
	if ( me->GetHealth() < (bot_sprint_low_health.GetInt() / 2) )
	{
		return true;
	}
	
	// Don't sprint in stealth situations (sneaking up on enemies) - reduced threshold
	if ( hasVisibleThreat && primaryThreat->GetTimeSinceLastSeen() < 1.0f )
	{
		float threatRange = me->GetRangeTo( primaryThreat->GetEntity() );
		// Don't sprint when very close to unaware enemies (stealth approach)
		if ( threatRange < 100.0f )
		{
			return false;
		}
	}
	
	return false; // Default: don't sprint
}


//-----------------------------------------------------------------------------------------
// Return true if given area can be used for navigation
bool CHL2MPBotLocomotion::IsAreaTraversable( const CNavArea* area ) const
{
	CHL2MPBot* me = ( CHL2MPBot* )GetBot()->GetEntity();

	if ( area->IsBlocked( me->GetTeamNumber() ) )
	{
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------------------
bool CHL2MPBotLocomotion::IsEntityTraversable( CBaseEntity* obstacle, TraverseWhenType when ) const
{
	// assume all players are "traversable" in that they will move or can be killed
	if ( obstacle && obstacle->IsPlayer() )
	{
		return true;
	}

	// assume held objects will move.
	if ( obstacle && obstacle->VPhysicsGetObject() && ( obstacle->VPhysicsGetObject()->GetGameFlags() & FVPHYSICS_PLAYER_HELD ) )
	{
		return true;
	}

	if ( obstacle )
	{
		// misyl:
		// override the base brush logic here to work better for hl2mp (w/ solid flags)
		// changing this for TF would be scary.
		//
		// if we hit a clip brush, ignore it if it is not BRUSHSOLID_ALWAYS
		if ( FClassnameIs( obstacle, "func_brush" ) )
		{
			CFuncBrush* brush = ( CFuncBrush* )obstacle;

			switch ( brush->m_iSolidity )
			{
			case CFuncBrush::BRUSHSOLID_ALWAYS:
				return false;
			case CFuncBrush::BRUSHSOLID_NEVER:
				return true;
			case CFuncBrush::BRUSHSOLID_TOGGLE:
				return brush->GetSolidFlags() & FSOLID_NOT_SOLID;
			}
		}
	}

	return PlayerLocomotion::IsEntityTraversable( obstacle, when );
}
