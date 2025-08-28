//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: HL2MP game stats tracking with audio feedback
//
//=============================================================================

#ifndef HL2MP_GAMESTATS_H
#define HL2MP_GAMESTATS_H
#ifdef _WIN32
#pragma once
#endif

#include "gamestats.h"

class CHL2MP_GameStats : public CBaseGameStats
{
	typedef CBaseGameStats BaseClass;

public:
	CHL2MP_GameStats( void );

	// Override gamestats events to add audio feedback
	virtual void Event_PlayerDamage( CBasePlayer *pBasePlayer, const CTakeDamageInfo &info );
	virtual void Event_WeaponHit( CBasePlayer *pShooter, bool bPrimary, char const *pchWeaponName, const CTakeDamageInfo &info );
	virtual void Event_PlayerKilledOther( CBasePlayer *pAttacker, CBaseEntity *pVictim, const CTakeDamageInfo &info );
};

#endif // HL2MP_GAMESTATS_H