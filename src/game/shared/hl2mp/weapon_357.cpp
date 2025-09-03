
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
	#include "c_hl2mp_player.h"
	#include <prediction.h>
    #include "c_te_effect_dispatch.h"
#else
	#include "hl2mp_player.h"
    #include "te_effect_dispatch.h"
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "zoom_shared.h" // Include for zoom functionality

// External declarations for cvars from other files
extern ConVar rbcl_smooth_zoom;

#ifdef CLIENT_DLL
#define CWeapon357 C_Weapon357
#endif

ConVar rbsv_357_zoom("rbsv_357_zoom", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Enable or disable 357 zoom functionality");
//-----------------------------------------------------------------------------
// CWeapon357
//-----------------------------------------------------------------------------

class CWeapon357 : public CBaseHL2MPCombatWeapon
{
	DECLARE_CLASS( CWeapon357, CBaseHL2MPCombatWeapon );
public:

	CWeapon357( void );
    
    void    Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator);
    void    PrimaryAttack( void );
    void    ItemPostFrame( void );
    bool    Holster(CBaseCombatWeapon *pSwitchingTo); // Declare the Holster method
    void    ToggleZoom( void ); // Declare the ToggleZoom method
    bool    Reload(void); // Declare the Reload method

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_ACTTABLE();

private:
	
	CWeapon357( const CWeapon357 & );

    CNetworkVar( bool, m_bInZoom ); // Networked variable to track zoom state
};

IMPLEMENT_NETWORKCLASS_ALIASED( Weapon357, DT_Weapon357 )

BEGIN_NETWORK_TABLE( CWeapon357, DT_Weapon357 )
#ifdef CLIENT_DLL
    RecvPropBool( RECVINFO( m_bInZoom ) ),
#else
    SendPropBool( SENDINFO( m_bInZoom ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeapon357 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_357, CWeapon357 );
PRECACHE_WEAPON_REGISTER( weapon_357 );


acttable_t CWeapon357::m_acttable[] = 
{
	{ ACT_MP_STAND_IDLE,				ACT_HL2MP_IDLE_PISTOL,					false },
	{ ACT_MP_CROUCH_IDLE,				ACT_HL2MP_IDLE_CROUCH_PISTOL,			false },

	{ ACT_MP_RUN,						ACT_HL2MP_RUN_PISTOL,					false },
	{ ACT_MP_CROUCHWALK,				ACT_HL2MP_WALK_CROUCH_PISTOL,			false },

	{ ACT_MP_ATTACK_STAND_PRIMARYFIRE,	ACT_HL2MP_GESTURE_RANGE_ATTACK_PISTOL,	false },
	{ ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,	ACT_HL2MP_GESTURE_RANGE_ATTACK_PISTOL,	false },

	{ ACT_MP_RELOAD_STAND,				ACT_HL2MP_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_MP_RELOAD_CROUCH,				ACT_HL2MP_GESTURE_RELOAD_PISTOL,		false },

	{ ACT_MP_JUMP,						ACT_HL2MP_JUMP_PISTOL,					false },
};

IMPLEMENT_ACTTABLE( CWeapon357 );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon357::CWeapon357( void )
{
	m_bReloadsSingly	= false;
	m_bFiresUnderwater	= false;
    m_bReloadsSingly    = false;
    m_bFiresUnderwater    = false;
    m_bInZoom            = false; // Initialize zoom state
}

//-----------------------------------------------------------------------------
// Purpose: Handles the weapon being holstered
//-----------------------------------------------------------------------------
bool CWeapon357::Holster(CBaseCombatWeapon *pSwitchingTo)
{
    if ( m_bInZoom )
    {
        ToggleZoom();
    }
    return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: Toggles the zoom state
//-----------------------------------------------------------------------------
void CWeapon357::ToggleZoom(void)
{
    if (!rbsv_357_zoom.GetBool())
        return;

    CBasePlayer *pPlayer = ToBasePlayer(GetOwner());

    if (pPlayer == NULL)
        return;

#ifndef CLIENT_DLL
    float zoomTransitionTime = rbcl_smooth_zoom.GetBool() ? 0.2f : 0.0f;

    if (m_bInZoom)
    {
        if (pPlayer->SetFOV(this, 0, zoomTransitionTime))
        {
            m_bInZoom = false;
        }
    }
    else
    {
        if (pPlayer->SetFOV(this, 40, zoomTransitionTime))
        {
            m_bInZoom = true;
        }
    }
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Handles the weapon reloading
//-----------------------------------------------------------------------------
bool CWeapon357::Reload(void)
{
    if ( m_bInZoom )
    {
        ToggleZoom();
    }
    return BaseClass::Reload();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon357::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast
	CHL2MP_Player *pPlayer = ToHL2MPPlayer( GetOwner() );

	if ( !pPlayer )
	{
		return;
	}

	if ( m_iClip1 <= 0 )
	{
		if ( !m_bFireOnEmpty )
		{
			Reload();
		}
		else
		{
			WeaponSound( EMPTY );
			m_flNextPrimaryAttack = 0.15;
		}

		return;
	}

	WeaponSound( SINGLE );
	pPlayer->DoMuzzleFlash();

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY );

	m_flNextPrimaryAttack = gpGlobals->curtime + 0.75;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.75;

	m_iClip1--;

	Vector vecSrc		= pPlayer->Weapon_ShootPosition();
	Vector vecAiming	= pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	FireBulletsInfo_t info( 1, vecSrc, vecAiming, vec3_origin, MAX_TRACE_LENGTH, m_iPrimaryAmmoType );
	info.m_pAttacker = pPlayer;

	// Fire the bullets, and force the first shot to be perfectly accuracy
	pPlayer->FireBullets( info );

#ifdef CLIENT_DLL
	//Disorient the player
	if ( prediction->IsFirstTimePredicted() )
	{
		QAngle angles;
		engine->GetViewAngles( angles );
		angles.x += random->RandomInt( -1, 1 );
		angles.y += random->RandomInt( -1, 1 );
		angles.z += 0.0f;
		engine->SetViewAngles( angles );
	}
#endif // CLIENT_DLL

	pPlayer->ViewPunch( QAngle( -8, random->RandomFloat( -2, 2 ), 0 ) );

	if ( !m_iClip1 && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 ); 
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon357::Operator_HandleAnimEvent(animevent_t* pEvent, CBaseCombatCharacter* pOperator)
{
    CBasePlayer* pOwner = ToBasePlayer(GetOwner());
    switch (pEvent->event)
    {
    case EVENT_WEAPON_RELOAD:
    {
        CEffectData data;

        // Emit six spent shells
        for (int i = 0; i < 6; i++)
        {
            IPredictionSystem::SuppressHostEvents(NULL);

            data.m_vOrigin = pOwner->WorldSpaceCenter() + RandomVector(-4, 4);
            data.m_vAngles = QAngle(90, random->RandomInt(0, 360), 0);

#ifdef CLIENT_DLL
            data.m_hEntity = GetOwner();
#else
            data.m_nEntIndex = entindex();
#endif

            DispatchEffect("ShellEject", data);
        }
        break;
    }
    }
}

//-----------------------------------------------------------------------------
// Purpose: Handles the item post frame
//-----------------------------------------------------------------------------
void CWeapon357::ItemPostFrame(void)
{
    CBasePlayer *pPlayer = ToBasePlayer(GetOwner());

    if (pPlayer == NULL)
        return;

    if (pPlayer->m_afButtonPressed & IN_ATTACK2)
    {
        ToggleZoom();
    }

    BaseClass::ItemPostFrame();
}
