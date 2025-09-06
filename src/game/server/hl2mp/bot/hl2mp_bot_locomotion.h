//========= Copyright Valve Corporation, All rights reserved. ============//

#ifndef HL2MP_BOT_LOCOMOTION_H
#define HL2MP_BOT_LOCOMOTION_H

#include "NextBot/Player/NextBotPlayerLocomotion.h"

// Forward declarations
class CHL2MPBot;

//----------------------------------------------------------------------------
class CHL2MPBotLocomotion : public PlayerLocomotion
{
public:
	DECLARE_CLASS( CHL2MPBotLocomotion, PlayerLocomotion );

	CHL2MPBotLocomotion( INextBot *bot ) : PlayerLocomotion( bot )
	{
		// Initialize sprint tracking variables
		m_isSprinting = false;
		m_sprintStartTime = 0.0f;
		m_lastSprintButtonPress = 0.0f;
		
		// Initialize oscillation prevention variables
		m_lastLadderZ = 0.0f;
		m_ladderDirectionChangeTime = 0.0f;
		m_ladderDirectionChanges = 0;
	}

	virtual ~CHL2MPBotLocomotion() { }

	virtual void Update( void );								// (EXTEND) update internal state

	virtual void Approach( const Vector &pos, float goalWeight = 1.0f );	// move directly towards the given position

	virtual float GetMaxJumpHeight( void ) const;				// return maximum height of a jump
	virtual float GetDeathDropHeight( void ) const;			// distance at which we will die if we fall

	virtual float GetRunSpeed( void ) const;				// get maximum running speed

	virtual bool IsAreaTraversable( const CNavArea *baseArea ) const;	// return true if given area can be used for navigation
	virtual bool IsEntityTraversable( CBaseEntity *obstacle, TraverseWhenType when = EVENTUALLY ) const;

	// Ladder navigation overrides
	virtual void ClimbLadder( const CNavLadder *ladder, const CNavArea *dismountGoal );		// climb the given ladder to the top and dismount
	virtual void DescendLadder( const CNavLadder *ladder, const CNavArea *dismountGoal );	// descend the given ladder to the bottom and dismount
	virtual bool IsUsingLadder( void ) const;				// we are moving to get on, ascending/descending, and/or dismounting a ladder
	virtual bool IsAscendingOrDescendingLadder( void ) const;	// we are actually on the ladder right now, either climbing up or down

	// Sprint decision making
	bool ShouldSprint( CHL2MPBot *me ) const;			// determine if bot should sprint based on tactical situation
	
	// Ladder movement helper
	bool HandleLadderMovement( CHL2MPBot *me );			// handle HL2 ladder movement system

protected:
	virtual void AdjustPosture( const Vector &moveGoal ) { }	// never crouch to navigate

	// Sprint state tracking variables
	bool m_isSprinting;
	float m_sprintStartTime;
	float m_lastSprintButtonPress;
	
	// Ladder state tracking variables (from PlayerLocomotion base class)
	const CNavLadder *m_ladderInfo;
	const CNavArea *m_ladderDismountGoal;
	bool m_isGoingUpLadder;
	CountdownTimer m_ladderTimer;
	float m_lastLadderZ;				// Track last Z position to prevent oscillation
	float m_ladderDirectionChangeTime;	// Time of last direction change
	int m_ladderDirectionChanges;		// Count direction changes to detect oscillation
	enum LadderState
	{
		NO_LADDER = 0,
		APPROACHING_ASCENDING_LADDER,
		APPROACHING_DESCENDING_LADDER,
		ASCENDING_LADDER,
		DESCENDING_LADDER,
		DISMOUNTING_LADDER_TOP,
		DISMOUNTING_LADDER_BOTTOM
	};
	LadderState m_ladderState;
};

inline float CHL2MPBotLocomotion::GetMaxJumpHeight( void ) const
{
	// https://developer.valvesoftware.com/wiki/Dimensions_(Half-Life_2_and_Counter-Strike:_Source)#Jumping
	return 56.0f;
}

#endif // HL2MP_BOT_LOCOMOTION_H
