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
#include "ammodef.h"

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
	
	// Low ammo and danger state detection
	bool IsAmmoLow( int ammo, int maxAmmo );
	bool IsAmmoEmpty( int ammo );
	Color GetAmmoDisplayColor( int ammo, int maxAmmo, bool isEmpty );
	Color GetAmmo2DisplayColor( int ammo2, int maxAmmo2, bool isEmpty );
	
private:
	CHandle< C_BaseCombatWeapon > m_hCurrentActiveWeapon;
	CHandle< C_BaseEntity > m_hCurrentVehicle;
	int		m_iAmmo;
	int		m_iAmmo2;
	CHudTexture *m_iconPrimaryAmmo;
	
	// Weapon change flash animation
	float	m_flWeaponChangeFlashTime;
	
	// Low ammo warning flash
	float	m_flLowAmmoWarningTime;
	float	m_flLowAmmo2WarningTime;
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
	
	// Initialize weapon change flash and low ammo warnings
	m_flWeaponChangeFlashTime = 0.0f;
	m_flLowAmmoWarningTime = 0.0f;
	m_flLowAmmo2WarningTime = 0.0f;
	
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
	
	// Reset weapon change flash and warning timers
	m_flWeaponChangeFlashTime = 0.0f;
	m_flLowAmmoWarningTime = 0.0f;
	m_flLowAmmo2WarningTime = 0.0f;

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

	// Check for weapon change first - trigger flash for ALL weapon changes
	bool weaponChanged = (wpn != m_hCurrentActiveWeapon);
	if ( weaponChanged )
	{
		m_hCurrentActiveWeapon = wpn;
		
		// Always trigger weapon change flash when changing weapons
		m_flWeaponChangeFlashTime = gpGlobals->curtime + 0.15f;
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponChanged");
	}

	if ( !wpn || !player || !wpn->UsesPrimaryAmmo() )
	{
		hudlcd->SetGlobalStat( "(ammo_primary)", "n/a" );
        hudlcd->SetGlobalStat( "(ammo_secondary)", "n/a" );

		SetPaintEnabled(false);
		SetPaintBackgroundEnabled(false);
		return;
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

	if (wpn == m_hCurrentActiveWeapon && !weaponChanged)
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
		if (wpn->UsesPrimaryAmmo() && wpn->UsesClipsForAmmo1())
		{
			SetShouldDisplaySecondaryValue(true);
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponUsesClips");
		}
		else
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponDoesNotUseClips");
			SetShouldDisplaySecondaryValue(false);
		}
	}

	hudlcd->SetGlobalStat( "(ammo_primary)", VarArgs( "%d", ammo1 ) );
	hudlcd->SetGlobalStat( "(ammo_secondary)", VarArgs( "%d", ammo2 ) );

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
// Purpose: Determine if ammo is considered low
//-----------------------------------------------------------------------------
bool CHudAmmo::IsAmmoLow( int ammo, int maxAmmo )
{
	if ( maxAmmo <= 0 ) return false;
	
	// Consider ammo low if less than 25% of max capacity
	float lowThreshold = maxAmmo * 0.25f;
	return ( ammo > 0 && ammo <= lowThreshold );
}

//-----------------------------------------------------------------------------
// Purpose: Determine if ammo is empty
//-----------------------------------------------------------------------------
bool CHudAmmo::IsAmmoEmpty( int ammo )
{
	return ( ammo <= 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Get display color for primary ammo based on state
//-----------------------------------------------------------------------------
Color CHudAmmo::GetAmmoDisplayColor( int ammo, int maxAmmo, bool isEmpty )
{
	if ( isEmpty )
	{
		// Empty ammo - use danger color
		return GetDangerColor();
	}
	else if ( IsAmmoLow( ammo, maxAmmo ) )
	{
		// Low ammo - interpolate between custom color and danger color
		Color customColor = GetCustomSchemeColor( "FgColor" );
		Color dangerColor = GetDangerColor();
		
		// Smooth transition based on how low the ammo is
		float lowThreshold = maxAmmo * 0.25f;
		float ratio = (float)ammo / lowThreshold; // 1.0 at threshold, 0.0 at empty
		ratio = clamp( ratio, 0.0f, 1.0f );
		
		// Interpolate colors
		int r = (int)(customColor.r() * ratio + dangerColor.r() * (1.0f - ratio));
		int g = (int)(customColor.g() * ratio + dangerColor.g() * (1.0f - ratio));
		int b = (int)(customColor.b() * ratio + dangerColor.b() * (1.0f - ratio));
		
		return Color( r, g, b, 255 );
	}
	else
	{
		// Normal ammo - use custom HUD color
		return GetCustomSchemeColor( "FgColor" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get display color for secondary ammo based on state
//-----------------------------------------------------------------------------
Color CHudAmmo::GetAmmo2DisplayColor( int ammo2, int maxAmmo2, bool isEmpty )
{
	if ( isEmpty )
	{
		// Empty ammo - use danger color
		return GetDangerColor();
	}
	else if ( IsAmmoLow( ammo2, maxAmmo2 ) )
	{
		// Low ammo - interpolate between custom color and danger color
		Color customColor = GetCustomSchemeColor( "FgColor" );
		Color dangerColor = GetDangerColor();
		
		// Smooth transition based on how low the ammo is
		float lowThreshold = maxAmmo2 * 0.25f;
		float ratio = (float)ammo2 / lowThreshold; // 1.0 at threshold, 0.0 at empty
		ratio = clamp( ratio, 0.0f, 1.0f );
		
		// Interpolate colors
		int r = (int)(customColor.r() * ratio + dangerColor.r() * (1.0f - ratio));
		int g = (int)(customColor.g() * ratio + dangerColor.g() * (1.0f - ratio));
		int b = (int)(customColor.b() * ratio + dangerColor.b() * (1.0f - ratio));
		
		return Color( r, g, b, 255 );
	}
	else
	{
		// Normal ammo - use custom HUD color
		return GetCustomSchemeColor( "FgColor" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates ammo display with enhanced danger state detection
//-----------------------------------------------------------------------------
void CHudAmmo::SetAmmo(int ammo, bool playAnimation)
{
	C_BaseCombatWeapon *pWeapon = GetActiveWeapon();
	int maxAmmo = 0;
	if ( pWeapon )
	{
		maxAmmo = GetAmmoDef()->MaxCarry( pWeapon->GetPrimaryAmmoType() );
	}
	
	bool isEmpty = (ammo == 0);
	bool wasLow = IsAmmoLow( m_iAmmo, maxAmmo );
	bool isLow = IsAmmoLow( ammo, maxAmmo );
	
	if (ammo != m_iAmmo)
	{
		if (playAnimation)
		{
			// Trigger appropriate ammo animations
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
			
			// Check for low ammo warning flash
			if ( !wasLow && isLow && !isEmpty )
			{
				// Just became low ammo - trigger warning flash
				m_flLowAmmoWarningTime = gpGlobals->curtime + 0.25f;
			}
		}
		
		// Update ammo color based on state
		Color ammoColor = GetAmmoDisplayColor( ammo, maxAmmo, isEmpty );
		SetFgColor( ammoColor );
		
		m_iAmmo = ammo;
	}
	
	SetDisplayValue(ammo);
}

//-----------------------------------------------------------------------------
// Purpose: Updates 2nd ammo display with enhanced danger state detection
//-----------------------------------------------------------------------------
void CHudAmmo::SetAmmo2(int ammo2, bool playAnimation)
{
	C_BaseCombatWeapon *pWeapon = GetActiveWeapon();
	int maxAmmo2 = 0;
	if ( pWeapon )
	{
		maxAmmo2 = GetAmmoDef()->MaxCarry( pWeapon->GetSecondaryAmmoType() );
	}
	
	bool isEmpty = (ammo2 == 0);
	bool wasLow = IsAmmoLow( m_iAmmo2, maxAmmo2 );
	bool isLow = IsAmmoLow( ammo2, maxAmmo2 );
	
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
			
			// Check for low ammo warning flash
			if ( !wasLow && isLow && !isEmpty )
			{
				// Just became low ammo - trigger warning flash
				m_flLowAmmo2WarningTime = gpGlobals->curtime + 0.25f;
			}
		}
		
		// Update ammo color based on state - apply to secondary value display
		Color ammo2Color = GetAmmo2DisplayColor( ammo2, maxAmmo2, isEmpty );
		// Note: SetSecondaryValue doesn't have color parameter, so we'll handle this in Paint
		
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
		
		// Get current weapon info for proper color determination
		C_BaseCombatWeapon *pWeapon = GetActiveWeapon();
		Color iconColor = GetCustomSchemeColor( "FgColor" ); // Default color
		
		if ( pWeapon )
		{
			int maxAmmo = GetAmmoDef()->MaxCarry( pWeapon->GetPrimaryAmmoType() );
			bool isEmpty = IsAmmoEmpty( m_iAmmo );
			
			// Use danger color for empty or low ammo, otherwise use normal color
			iconColor = GetAmmoDisplayColor( m_iAmmo, maxAmmo, isEmpty );
		}
		
		m_iconPrimaryAmmo->DrawSelf( x, y, iconColor );
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
			
			// Update secondary ammo color based on state
			if (wpn && wpn->UsesSecondaryAmmo())
			{
				int maxAmmo2 = GetAmmoDef()->MaxCarry( wpn->GetSecondaryAmmoType() );
				bool isEmpty = (ammo == 0);
				Color ammo2Color;
				
				if (isEmpty || (maxAmmo2 > 0 && ammo <= maxAmmo2 * 0.25f))
				{
					// Empty or low ammo - use danger color
					ammo2Color = GetDangerColor();
				}
				else
				{
					// Normal ammo - use custom HUD color
					ammo2Color = GetCustomSchemeColor( "FgColor" );
				}
				
				SetFgColor( ammo2Color );
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
			
			// Get current weapon info for proper color determination
			C_BaseCombatWeapon *wpn = GetActiveWeapon();
			Color iconColor = GetCustomSchemeColor( "FgColor" ); // Default color
			
			if ( wpn && wpn->UsesSecondaryAmmo() )
			{
				int maxAmmo2 = GetAmmoDef()->MaxCarry( wpn->GetSecondaryAmmoType() );
				bool isEmpty = (m_iAmmo == 0);
				
				if (isEmpty || (maxAmmo2 > 0 && m_iAmmo <= maxAmmo2 * 0.25f))
				{
					// Empty or low ammo - use danger color
					iconColor = GetDangerColor();
				}
				else
				{
					// Normal ammo - use custom HUD color
					iconColor = GetCustomSchemeColor( "FgColor" );
				}
			}

			m_iconSecondaryAmmo->DrawSelf( x, y, iconColor );
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

