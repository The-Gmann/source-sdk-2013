//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "hud_numericdisplay.h"
#include "iclientmode.h"
#include "iclientvehicle.h"
#include <vgui_controls/AnimationController.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "ihudlcd.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// Forward declaration of our custom color function
extern Color GetCustomSchemeColor( const char *colorName );

//-----------------------------------------------------------------------------
// Purpose: Displays current ammunition level
//-----------------------------------------------------------------------------
class CHudAmmo : public CHudNumericDisplay, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CHudAmmo, CHudNumericDisplay );

public:
	CHudAmmo( const char *pElementName );
	void Init( void );
	void VidInit( void );
	void Reset();

	void SetAmmo(int ammo, bool playAnimation);
	void SetAmmo2(int ammo2, bool playAnimation);
	virtual void Paint( void );
	virtual void PostChildPaint( void );
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );

protected:
	virtual void OnThink();

	void UpdateAmmoDisplays();
	void UpdatePlayerAmmo( C_BasePlayer *player );
	void UpdateVehicleAmmo( C_BasePlayer *player, IClientVehicle *pVehicle );
	
private:
	CHandle< C_BaseCombatWeapon > m_hCurrentActiveWeapon;
	CHandle< C_BaseEntity > m_hCurrentVehicle;
	int		m_iAmmo;
	int		m_iAmmo2;
	CHudTexture *m_iconPrimaryAmmo;
	
	// Weapon change flash animation
	float	m_flWeaponChangeFlashTime;
};

DECLARE_HUDELEMENT( CHudAmmo );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudAmmo::CHudAmmo( const char *pElementName ) : BaseClass(NULL, "HudAmmo"), CHudElement( pElementName )
{
	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT | HIDEHUD_WEAPONSELECTION );

	hudlcd->SetGlobalStat( "(ammo_primary)", "0" );
	hudlcd->SetGlobalStat( "(ammo_secondary)", "0" );
	hudlcd->SetGlobalStat( "(weapon_print_name)", "" );
	hudlcd->SetGlobalStat( "(weapon_name)", "" );
	
	// Initialize weapon change flash
	m_flWeaponChangeFlashTime = 0.0f;
	
	// Enable PostChildPaint for flash overlay
	SetPostChildPaintEnabled( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudAmmo::Init( void )
{
	m_iAmmo		= -1;
	m_iAmmo2	= -1;
	
	m_iconPrimaryAmmo = NULL;

	wchar_t *tempString = g_pVGuiLocalize->Find("#Valve_Hud_AMMO");
	if (tempString)
	{
		SetLabelText(tempString);
	}
	else
	{
		SetLabelText(L"AMMO");
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudAmmo::VidInit( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Apply scheme settings and use custom HUD colors
//-----------------------------------------------------------------------------
void CHudAmmo::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	
	// Override with custom HUD colors
	SetFgColor( GetCustomSchemeColor( "FgColor" ) );
}

//-----------------------------------------------------------------------------
// Purpose: Resets hud after save/restore
//-----------------------------------------------------------------------------
void CHudAmmo::Reset()
{
	BaseClass::Reset();

	m_hCurrentActiveWeapon = NULL;
	m_hCurrentVehicle = NULL;
	m_iAmmo = 0;
	m_iAmmo2 = 0;
	
	// Reset weapon change flash
	m_flWeaponChangeFlashTime = 0.0f;

	UpdateAmmoDisplays();
}

//-----------------------------------------------------------------------------
// Purpose: called every frame to get ammo info from the weapon
//-----------------------------------------------------------------------------
void CHudAmmo::UpdatePlayerAmmo( C_BasePlayer *player )
{
	// Clear out the vehicle entity
	m_hCurrentVehicle = NULL;

	C_BaseCombatWeapon *wpn = GetActiveWeapon();

	hudlcd->SetGlobalStat( "(weapon_print_name)", wpn ? wpn->GetPrintName() : " " );
	hudlcd->SetGlobalStat( "(weapon_name)", wpn ? wpn->GetName() : " " );

	// Check for weapon change first, but only trigger flash for weapons with ammo displays
	bool weaponChanged = (wpn != m_hCurrentActiveWeapon);
	if ( weaponChanged )
	{
		m_hCurrentActiveWeapon = wpn;
	}

	if ( !wpn || !player || !wpn->UsesPrimaryAmmo() )
	{
		hudlcd->SetGlobalStat( "(ammo_primary)", "n/a" );
        hudlcd->SetGlobalStat( "(ammo_secondary)", "n/a" );

		SetPaintEnabled(false);
		SetPaintBackgroundEnabled(false);
		return;
	}

	// Only trigger flash when switching TO a weapon that has an ammo display
	if ( weaponChanged )
	{
		// Trigger weapon change flash for weapons with ammo counters
		m_flWeaponChangeFlashTime = gpGlobals->curtime + 0.15f;
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponChanged");
	}

	SetPaintEnabled(true);
	SetPaintBackgroundEnabled(true);

	// Get our icons for the ammo types
	m_iconPrimaryAmmo = gWR.GetAmmoIconFromWeapon( wpn->GetPrimaryAmmoType() );

	// get the ammo in our clip
	int ammo1 = wpn->Clip1();
	int ammo2;
	if (ammo1 < 0)
	{
		// we don't use clip ammo, just use the total ammo count
		ammo1 = player->GetAmmoCount(wpn->GetPrimaryAmmoType());
		ammo2 = 0;
	}
	else
	{
		// we use clip ammo, so the second ammo is the total ammo
		ammo2 = player->GetAmmoCount(wpn->GetPrimaryAmmoType());
	}

	hudlcd->SetGlobalStat( "(ammo_primary)", VarArgs( "%d", ammo1 ) );
	hudlcd->SetGlobalStat( "(ammo_secondary)", VarArgs( "%d", ammo2 ) );

	if (!weaponChanged)
	{
		// same weapon, just update counts
		SetAmmo(ammo1, true);
		SetAmmo2(ammo2, true);
	}
	else
	{
		// different weapon, change without triggering ammo animations
		SetAmmo(ammo1, false);
		SetAmmo2(ammo2, false);

		// update whether or not we show the total ammo display
		if (wpn->UsesClipsForAmmo1())
		{
			SetShouldDisplaySecondaryValue(true);
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponUsesClips");
		}
		else
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponDoesNotUseClips");
			SetShouldDisplaySecondaryValue(false);
		}

		// Flash and weapon change already handled above
	}
}

void CHudAmmo::UpdateVehicleAmmo( C_BasePlayer *player, IClientVehicle *pVehicle )
{
	m_hCurrentActiveWeapon = NULL;
	CBaseEntity *pVehicleEnt = pVehicle->GetVehicleEnt();

	if ( !pVehicleEnt || pVehicle->GetPrimaryAmmoType() < 0 )
	{
		SetPaintEnabled(false);
		SetPaintBackgroundEnabled(false);
		return;
	}

	SetPaintEnabled(true);
	SetPaintBackgroundEnabled(true);

	// get the ammo in our clip
	int ammo1 = pVehicle->GetPrimaryAmmoClip();
	int ammo2;
	if (ammo1 < 0)
	{
		// we don't use clip ammo, just use the total ammo count
		ammo1 = pVehicle->GetPrimaryAmmoCount();
		ammo2 = 0;
	}
	else
	{
		// we use clip ammo, so the second ammo is the total ammo
		ammo2 = pVehicle->GetPrimaryAmmoCount();
	}

	if (pVehicleEnt == m_hCurrentVehicle)
	{
		// same weapon, just update counts
		SetAmmo(ammo1, true);
		SetAmmo2(ammo2, true);
	}
	else
	{
		// diferent weapon, change without triggering
		SetAmmo(ammo1, false);
		SetAmmo2(ammo2, false);

		// update whether or not we show the total ammo display
		if (pVehicle->PrimaryAmmoUsesClips())
		{
			SetShouldDisplaySecondaryValue(true);
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponUsesClips");
		}
		else
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponDoesNotUseClips");
			SetShouldDisplaySecondaryValue(false);
		}

		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponChanged");
		m_hCurrentVehicle = pVehicleEnt;
	}
}

//-----------------------------------------------------------------------------
// Purpose: called every frame to get ammo info from the weapon
//-----------------------------------------------------------------------------
void CHudAmmo::OnThink()
{
	// Update weapon change flash effect
	if ( m_flWeaponChangeFlashTime > 0.0f && gpGlobals->curtime <= m_flWeaponChangeFlashTime )
	{
		// Flash is active - calculation handled in Paint()
	}
	else if ( m_flWeaponChangeFlashTime > 0.0f )
	{
		// Flash finished, reset
		m_flWeaponChangeFlashTime = 0.0f;
		SetFgColor( GetCustomSchemeColor( "FgColor" ) );
	}
	
	UpdateAmmoDisplays();
}

//-----------------------------------------------------------------------------
// Purpose: updates the ammo display counts
//-----------------------------------------------------------------------------
void CHudAmmo::UpdateAmmoDisplays()
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	IClientVehicle *pVehicle = player ? player->GetVehicle() : NULL;

	if ( !pVehicle )
	{
		UpdatePlayerAmmo( player );
	}
	else
	{
		UpdateVehicleAmmo( player, pVehicle );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates ammo display
//-----------------------------------------------------------------------------
void CHudAmmo::SetAmmo(int ammo, bool playAnimation)
{
	if (ammo != m_iAmmo)
	{
		if (playAnimation)
		{
			if (ammo == 0)
			{
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("AmmoEmpty");
			}
			else if (ammo < m_iAmmo)
			{
				// ammo has decreased
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("AmmoDecreased");
			}
			else
			{
				// ammunition has increased
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("AmmoIncreased");
			}
		}

		m_iAmmo = ammo;
	}

	SetDisplayValue(ammo);
}

//-----------------------------------------------------------------------------
// Purpose: Updates 2nd ammo display
//-----------------------------------------------------------------------------
void CHudAmmo::SetAmmo2(int ammo2, bool playAnimation)
{
	if (ammo2 != m_iAmmo2)
	{
		if (playAnimation)
		{
			if (ammo2 == 0)
			{
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("Ammo2Empty");
			}
			else if (ammo2 < m_iAmmo2)
			{
				// ammo has decreased
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("Ammo2Decreased");
			}
			else
			{
				// ammunition has increased
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("Ammo2Increased");
			}
		}

		m_iAmmo2 = ammo2;
	}

	SetSecondaryValue(ammo2);
}

//-----------------------------------------------------------------------------
// Purpose: We add an icon into the 
//-----------------------------------------------------------------------------
void CHudAmmo::Paint( void )
{
	BaseClass::Paint();

	if ( m_hCurrentVehicle == NULL && m_iconPrimaryAmmo )
	{
		int nLabelHeight;
		int nLabelWidth;
		surface()->GetTextSize( m_hTextFont, m_LabelText, nLabelWidth, nLabelHeight );

		// Figure out where we're going to put this
		int x = text_xpos + ( nLabelWidth - m_iconPrimaryAmmo->Width() ) / 2;
		int y = text_ypos - ( nLabelHeight + ( m_iconPrimaryAmmo->Height() / 2 ) );
		
		// Use normal custom HUD color for icon
		m_iconPrimaryAmmo->DrawSelf( x, y, GetCustomSchemeColor( "FgColor" ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw flash overlay after all other painting is complete
//-----------------------------------------------------------------------------
void CHudAmmo::PostChildPaint( void )
{
	// Draw weapon change flash overlay after everything else
	if ( m_flWeaponChangeFlashTime > 0.0f && gpGlobals->curtime <= m_flWeaponChangeFlashTime )
	{
		// Calculate flash intensity (1.0 at start, 0.0 at end)
		float flFlashTime = 0.15f; // Total flash duration
		float flElapsed = flFlashTime - (m_flWeaponChangeFlashTime - gpGlobals->curtime);
		float flIntensity = 1.0f - (flElapsed / flFlashTime);
		
		// Create flash overlay color with max 50% opacity
		Color customColor = GetCustomSchemeColor( "FgColor" );
		int flashAlpha = (int)(127 * flIntensity); // 127 = 50% of 255
		Color flashColor( customColor.r(), customColor.g(), customColor.b(), flashAlpha );
		
		// Draw rounded flash overlay that matches panel background
		int panelWide, panelTall;
		GetSize( panelWide, panelTall );
		
		// Use DrawBox to create rounded rectangle overlay
		DrawBox( 0, 0, panelWide, panelTall, flashColor, 1.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Displays the secondary ammunition level
//-----------------------------------------------------------------------------
class CHudSecondaryAmmo : public CHudNumericDisplay, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CHudSecondaryAmmo, CHudNumericDisplay );

public:
	CHudSecondaryAmmo( const char *pElementName ) : BaseClass( NULL, "HudAmmoSecondary" ), CHudElement( pElementName )
	{
		m_iAmmo = -1;
		m_flWeaponChangeFlashTime = 0.0f;
		m_iconSecondaryAmmo = NULL;

		SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_WEAPONSELECTION | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
		
		// Enable PostChildPaint for flash overlay
		SetPostChildPaintEnabled( true );
	}

	void Init( void )
	{
		wchar_t *tempString = g_pVGuiLocalize->Find("#Valve_Hud_AMMO_ALT");
		if (tempString)
		{
			SetLabelText(tempString);
		}
		else
		{
			SetLabelText(L"ALT");
		}
	}

	void VidInit( void )
	{
	}

	virtual void ApplySchemeSettings( vgui::IScheme *scheme )
	{
		BaseClass::ApplySchemeSettings( scheme );
		
		// Override with custom HUD colors
		SetFgColor( GetCustomSchemeColor( "FgColor" ) );
	}

	void SetAmmo( int ammo )
	{
		if (ammo != m_iAmmo)
		{
			// Check if we should play animations (only if ammo value actually changed and we have a valid weapon)
			C_BaseCombatWeapon *wpn = GetActiveWeapon();
			C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
			bool shouldAnimate = (player && wpn && wpn->UsesSecondaryAmmo());
			
			if (shouldAnimate)
			{
				if (ammo == 0)
				{
					g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("AmmoSecondaryEmpty");
				}
				else if (ammo < m_iAmmo)
				{
					// ammo has decreased
					g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("AmmoSecondaryDecreased");
				}
				else
				{
					// ammunition has increased
					g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("AmmoSecondaryIncreased");
				}
			}

			m_iAmmo = ammo;
		}
		SetDisplayValue( ammo );
	}

	void Reset()
	{
		// hud reset, update ammo state
		BaseClass::Reset();
		m_iAmmo = 0;
		m_hCurrentActiveWeapon = NULL;
		m_iconSecondaryAmmo = NULL;
		m_flWeaponChangeFlashTime = 0.0f;
		SetAlpha( 0 );
		UpdateAmmoState();
	}

	virtual void Paint( void )
	{
		BaseClass::Paint();

		if ( m_iconSecondaryAmmo )
		{
			int nLabelHeight;
			int nLabelWidth;
			surface()->GetTextSize( m_hTextFont, m_LabelText, nLabelWidth, nLabelHeight );

			// Figure out where we're going to put this
			int x = text_xpos + ( nLabelWidth - m_iconSecondaryAmmo->Width() ) / 2;
			int y = text_ypos - ( nLabelHeight + ( m_iconSecondaryAmmo->Height() / 2 ) );

			m_iconSecondaryAmmo->DrawSelf( x, y, GetCustomSchemeColor( "FgColor" ) );
		}
	}

	virtual void PostChildPaint( void )
	{
		// Draw weapon change flash overlay after everything else
		if ( m_flWeaponChangeFlashTime > 0.0f && gpGlobals->curtime <= m_flWeaponChangeFlashTime )
		{
			// Calculate flash intensity (1.0 at start, 0.0 at end)
			float flFlashTime = 0.15f; // Total flash duration
			float flElapsed = flFlashTime - (m_flWeaponChangeFlashTime - gpGlobals->curtime);
			float flIntensity = 1.0f - (flElapsed / flFlashTime);
			
			// Create flash overlay color with max 50% opacity
			Color customColor = GetCustomSchemeColor( "FgColor" );
			int flashAlpha = (int)(127 * flIntensity); // 127 = 50% of 255
			Color flashColor( customColor.r(), customColor.g(), customColor.b(), flashAlpha );
			
			// Draw rounded flash overlay that matches panel background
			int panelWide, panelTall;
			GetSize( panelWide, panelTall );
			
			// Use DrawBox to create rounded rectangle overlay
			DrawBox( 0, 0, panelWide, panelTall, flashColor, 1.0f );
		}
	}

protected:

	virtual void OnThink()
	{
		// Update weapon change flash effect
		if ( m_flWeaponChangeFlashTime > 0.0f && gpGlobals->curtime <= m_flWeaponChangeFlashTime )
		{
			// Flash is active - calculation handled in Paint()
		}
		else if ( m_flWeaponChangeFlashTime > 0.0f )
		{
			// Flash finished, reset
			m_flWeaponChangeFlashTime = 0.0f;
			SetFgColor( GetCustomSchemeColor( "FgColor" ) );
		}
		
		// set whether or not the panel draws based on if we have a weapon that supports secondary ammo
		C_BaseCombatWeapon *wpn = GetActiveWeapon();
		C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
		IClientVehicle *pVehicle = player ? player->GetVehicle() : NULL;
		if (!wpn || !player || pVehicle)
		{
			m_hCurrentActiveWeapon = NULL;
			SetPaintEnabled(false);
			SetPaintBackgroundEnabled(false);
			return;
		}
		else
		{
			SetPaintEnabled(true);
			SetPaintBackgroundEnabled(true);
		}

		UpdateAmmoState();
	}

	void UpdateAmmoState()
	{
		C_BaseCombatWeapon *wpn = GetActiveWeapon();
		C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();

		if (player && wpn && wpn->UsesSecondaryAmmo())
		{
			SetAmmo(player->GetAmmoCount(wpn->GetSecondaryAmmoType()));
		}

		if ( m_hCurrentActiveWeapon != wpn )
		{
			m_hCurrentActiveWeapon = wpn;
			
			if ( wpn && wpn->UsesSecondaryAmmo() )
			{
				// Only trigger flash when switching TO a weapon that uses secondary ammo
				m_flWeaponChangeFlashTime = gpGlobals->curtime + 0.15f;
				
				// we've changed to a weapon that uses secondary ammo
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponUsesSecondaryAmmo");
				
				// Get the icon we should be displaying
				m_iconSecondaryAmmo = gWR.GetAmmoIconFromWeapon( wpn->GetSecondaryAmmoType() );
			}
			else 
			{
				// we've changed away from a weapon that uses secondary ammo
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponDoesNotUseSecondaryAmmo");
				
				// Clear the icon since this weapon doesn't use secondary ammo
				m_iconSecondaryAmmo = NULL;
			}
		}
	}
	
private:
	CHandle< C_BaseCombatWeapon > m_hCurrentActiveWeapon;
	CHudTexture *m_iconSecondaryAmmo;
	int		m_iAmmo;
	
	// Weapon change flash animation
	float	m_flWeaponChangeFlashTime;
};

DECLARE_HUDELEMENT( CHudSecondaryAmmo );

