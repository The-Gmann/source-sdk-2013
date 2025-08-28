//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: HL2MP game stats tracking with audio feedback
//
//=============================================================================

#include "cbase.h"
#include "hl2mp_gamestats.h"
#include "hl2mp_player.h"

static CHL2MP_GameStats s_HL2MP_GameStats;

CHL2MP_GameStats::CHL2MP_GameStats( void )
{
	gamestats = &s_HL2MP_GameStats;
}

//-----------------------------------------------------------------------------
// Purpose: Called when a player takes damage - check if attacker should get hitsound feedback
//-----------------------------------------------------------------------------
void CHL2MP_GameStats::Event_PlayerDamage( CBasePlayer *pBasePlayer, const CTakeDamageInfo &info )
{
	// Call base implementation first
	BaseClass::Event_PlayerDamage( pBasePlayer, info );

	// Check if there's an attacker and they're an HL2MP player
	CBaseEntity *pAttacker = info.GetAttacker();
	if ( pAttacker && pAttacker->IsPlayer() && pAttacker != pBasePlayer )
	{
		CHL2MP_Player *pHL2MPAttacker = dynamic_cast<CHL2MP_Player*>( pAttacker );
		if ( pHL2MPAttacker )
		{
			// Trigger hitsound feedback - attacker hit the victim (pBasePlayer)
			pHL2MPAttacker->OnDamageDealt( pBasePlayer, info );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when a weapon hits a target - triggers hitsound feedback
//-----------------------------------------------------------------------------
void CHL2MP_GameStats::Event_WeaponHit( CBasePlayer *pShooter, bool bPrimary, char const *pchWeaponName, const CTakeDamageInfo &info )
{
	// Call base implementation first
	BaseClass::Event_WeaponHit( pShooter, bPrimary, pchWeaponName, info );

	// Note: We're now using Event_PlayerDamage for hitsound tracking instead of Event_WeaponHit
	// because Event_PlayerDamage gives us direct access to both attacker and victim
}

//-----------------------------------------------------------------------------
// Purpose: Called when a player kills another entity - triggers killsound feedback
//-----------------------------------------------------------------------------
void CHL2MP_GameStats::Event_PlayerKilledOther( CBasePlayer *pAttacker, CBaseEntity *pVictim, const CTakeDamageInfo &info )
{
	// Call base implementation first
	BaseClass::Event_PlayerKilledOther( pAttacker, pVictim, info );

	// Trigger killsound feedback for HL2MP players
	CHL2MP_Player *pHL2MPAttacker = dynamic_cast<CHL2MP_Player*>( pAttacker );
	if ( pHL2MPAttacker )
	{
		pHL2MPAttacker->OnKilledOther( pVictim, info );
	}
}