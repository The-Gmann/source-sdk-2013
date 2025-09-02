//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hl2mp_cvars.h"

ConVar mp_noweapons(
	"mp_noweapons",
	"0",
	FCVAR_GAMEDLL,
	"If non-zero, game will not give player default weapons and ammo");

// Ready restart
ConVar mp_readyrestart(
	"mp_readyrestart", 
	"0", 
	FCVAR_GAMEDLL,
	"If non-zero, game will restart once each player gives the ready signal");

ConVar sv_game_description(
	"sv_game_description",
	"Classic Deathmatch",
	FCVAR_GAMEDLL,
	"Sets the game description");

// Ready signal
ConVar mp_ready_signal(
	"mp_ready_signal",
	"ready",
	FCVAR_GAMEDLL,
	"Text that each player must speak for the match to begin");

// Ear ringing - now handled client-side, this is just for server default
// Individual clients control this via rbcl_ear_ringing
ConVar rbsv_ear_ringing_default(
	"rbsv_ear_ringing_default",
	"1",
	FCVAR_GAMEDLL,
	"Default ear ringing setting for clients who haven't set rbcl_ear_ringing");