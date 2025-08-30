//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "model_types.h"
#include "vcollide.h"
#include "vcollide_parse.h"
#include "solidsetdefaults.h"
#include "bone_setup.h"
#include "engine/ivmodelinfo.h"
#include "physics.h"
#include "c_breakableprop.h"
#include "view.h"
#include "dlight.h"
#include "iefx.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar rbcl_dlight_barrel;

IMPLEMENT_CLIENTCLASS_DT(C_BreakableProp, DT_BreakableProp, CBreakableProp)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_BreakableProp::C_BreakableProp( void )
{
	m_takedamage = DAMAGE_YES;
	m_pBarrelDLight = NULL;
	m_dlightKey = -1;
	m_bWasOnFireLastFrame = false;
}

//-----------------------------------------------------------------------------
// Purpose: Clean up dlight when destroyed
//-----------------------------------------------------------------------------
C_BreakableProp::~C_BreakableProp( void )
{
	if ( m_pBarrelDLight )
	{
		m_pBarrelDLight->die = gpGlobals->curtime;
		m_pBarrelDLight = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame to update dynamic light for burning barrels
//-----------------------------------------------------------------------------
void C_BreakableProp::ClientThink( void )
{
	BaseClass::ClientThink();
	
	// Early out if dynamic lights are disabled
	if ( !rbcl_dlight_barrel.GetBool() )
	{
		if ( m_pBarrelDLight )
		{
			// Kill existing light if cvar is disabled
			m_pBarrelDLight->die = gpGlobals->curtime;
			m_pBarrelDLight = NULL;
		}
		m_bWasOnFireLastFrame = false;
		return;
	}
	
	bool bIsOnFire = IsOnFire();
	
	// Check if fire state changed
	if ( bIsOnFire != m_bWasOnFireLastFrame )
	{
		if ( bIsOnFire )
		{
			// Just caught fire, start thinking to create dlight
			m_dlightKey = entindex();
			SetNextClientThink( gpGlobals->curtime );
		}
		else
		{
			// Fire went out, kill the dlight
			if ( m_pBarrelDLight )
			{
				m_pBarrelDLight->die = gpGlobals->curtime;
				m_pBarrelDLight = NULL;
			}
			SetNextClientThink( CLIENT_THINK_NEVER );
		}
		m_bWasOnFireLastFrame = bIsOnFire;
	}
	
	// Update dlight if on fire
	if ( bIsOnFire )
	{
		UpdateBarrelDLight();
		SetNextClientThink( gpGlobals->curtime + 0.1f ); // Update every 0.1 seconds
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create and update dynamic light for burning barrel
//-----------------------------------------------------------------------------
void C_BreakableProp::UpdateBarrelDLight( void )
{
	if ( !m_pBarrelDLight )
	{
		m_pBarrelDLight = effects->CL_AllocDlight( m_dlightKey );
		if ( !m_pBarrelDLight )
		{
			return;
		}
	}
	
	// Set light properties
	m_pBarrelDLight->origin = GetAbsOrigin() + Vector( 0, 0, 32 ); // Slightly above the barrel
	m_pBarrelDLight->radius = 150.0f;
	m_pBarrelDLight->die = gpGlobals->curtime + 0.2f;
	
	// Fire colors - orange/red glow
	m_pBarrelDLight->color.r = 255;
	m_pBarrelDLight->color.g = 100;
	m_pBarrelDLight->color.b = 0;
	m_pBarrelDLight->color.exponent = 2;
	m_pBarrelDLight->style = 0;
	
	// Add slight flickering by varying the radius and color slightly
	float flicker = sin( gpGlobals->curtime * 10.0f ) * 0.1f + 1.0f;
	m_pBarrelDLight->radius *= flicker;
	m_pBarrelDLight->color.r = (int)(255 * flicker);
	m_pBarrelDLight->color.g = (int)(100 * flicker);
}

//-----------------------------------------------------------------------------
// Purpose: Override OnDataChanged to start thinking when needed
//-----------------------------------------------------------------------------
void C_BreakableProp::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );
	
	if ( type == DATA_UPDATE_CREATED )
	{
		// Start thinking if this barrel is already on fire when created
		if ( IsOnFire() && rbcl_dlight_barrel.GetBool() )
		{
			m_dlightKey = entindex();
			m_bWasOnFireLastFrame = true;
			SetNextClientThink( gpGlobals->curtime );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BreakableProp::SetFadeMinMax( float fademin, float fademax )
{
	m_fadeMinDist = fademin;
	m_fadeMaxDist = fademax;
}

//-----------------------------------------------------------------------------
// Copy fade from another breakable prop
//-----------------------------------------------------------------------------
void C_BreakableProp::CopyFadeFrom( C_BreakableProp *pSource )
{
	m_flFadeScale = pSource->m_flFadeScale;
}
