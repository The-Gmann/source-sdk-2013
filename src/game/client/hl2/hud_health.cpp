//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
// Health.cpp
//
// implementation of CHudHealth class
//
#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "view.h"

#include "iclientmode.h"

#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>

#include <vgui/ILocalize.h>
#include <cstdio>

using namespace vgui;

#include "hudelement.h"
#include "hud_numericdisplay.h"

#include "convar.h"

// Forward declaration of our custom color functions
extern Color GetCustomSchemeColor( const char *colorName );
extern Color GetDangerColor();
extern Color GetTransitionedColor( Color normalColor, Color dangerColor, float transitionProgress );

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define INIT_HEALTH -1

//-----------------------------------------------------------------------------
// Purpose: Health panel
//-----------------------------------------------------------------------------
class CHudHealth : public CHudElement, public CHudNumericDisplay
{
	DECLARE_CLASS_SIMPLE( CHudHealth, CHudNumericDisplay );

public:
	CHudHealth( const char *pElementName );
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual void OnThink();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void PostChildPaint( void );
			void MsgFunc_Damage( bf_read &msg );

protected:
	// Health danger state detection
	bool IsHealthLow( int health );
	bool IsHealthCritical( int health );
	Color GetHealthDisplayColor( int health );
	void SetHealth( int health, bool playAnimation );

private:
	// old variables
	int		m_iHealth;
	
	int		m_bitsDamage;
	
	// Low health warning flash
	float	m_flLowHealthWarningTime;
	
	// Color transition system
	bool	m_bHealthInDangerState;
	float	m_flHealthColorTransitionTime;
	static const float HEALTH_COLOR_TRANSITION_DURATION; // 0.5 seconds
};	

DECLARE_HUDELEMENT( CHudHealth );
DECLARE_HUD_MESSAGE( CHudHealth, Damage );

// Health color transition duration constant
const float CHudHealth::HEALTH_COLOR_TRANSITION_DURATION = 0.5f;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudHealth::CHudHealth( const char *pElementName ) : CHudElement( pElementName ), CHudNumericDisplay(NULL, "HudHealth")
{
	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
	
	// Initialize low health warning flash
	m_flLowHealthWarningTime = 0.0f;
	
	// Initialize color transition system
	m_bHealthInDangerState = false;
	m_flHealthColorTransitionTime = 0.0f;
	
	// Enable PostChildPaint for flash overlay
	SetPostChildPaintEnabled( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHealth::Init()
{
	HOOK_HUD_MESSAGE( CHudHealth, Damage );
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHealth::Reset()
{
	m_iHealth		= INIT_HEALTH;
	m_bitsDamage	= 0;
	
	// Reset low health warning flash timer
	m_flLowHealthWarningTime = 0.0f;
	
	// Reset color transition system
	m_bHealthInDangerState = false;
	m_flHealthColorTransitionTime = 0.0f;

	wchar_t *tempString = g_pVGuiLocalize->Find("#Valve_Hud_HEALTH");

	if (tempString)
	{
		SetLabelText(tempString);
	}
	else
	{
		SetLabelText(L"HEALTH");
	}
	SetDisplayValue(m_iHealth);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHealth::VidInit()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: Apply scheme settings and use custom HUD colors
//-----------------------------------------------------------------------------
void CHudHealth::ApplySchemeSettings( vgui::IScheme *scheme )
{
	// Call the vgui Panel base class directly to skip CHudNumericDisplay's color setting
	vgui::Panel::ApplySchemeSettings( scheme );
	
	// Only set background color from scheme, foreground color is handled manually
	SetBgColor( scheme->GetColor( "BgColor", GetBgColor() ) );
	
	// Don't set FgColor here - we handle it manually in OnThink/SetHealth
	// to ensure real-time color updates and avoid animation conflicts
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHealth::OnThink()
{
	int newHealth = 0;
	C_BasePlayer *local = C_BasePlayer::GetLocalPlayer();
	if ( local )
	{
		// Never below zero
		newHealth = MAX( local->GetHealth(), 0 );
	}

	// Check if we should be in danger state (low health or dead)
	bool shouldShowDanger = (newHealth <= 0) || IsHealthLow( newHealth );
	
	// Handle color transition for health
	if ( shouldShowDanger != m_bHealthInDangerState )
	{
		// State changed, start transition
		m_bHealthInDangerState = shouldShowDanger;
		m_flHealthColorTransitionTime = gpGlobals->curtime + HEALTH_COLOR_TRANSITION_DURATION;
	}
	
	// Calculate transition progress
	float transitionProgress = 0.0f;
	if ( m_flHealthColorTransitionTime > gpGlobals->curtime )
	{
		// Transition in progress
		float timeRemaining = m_flHealthColorTransitionTime - gpGlobals->curtime;
		transitionProgress = 1.0f - (timeRemaining / HEALTH_COLOR_TRANSITION_DURATION);
		
		if ( !m_bHealthInDangerState )
		{
			// Transitioning back to normal, reverse the progress
			transitionProgress = 1.0f - transitionProgress;
		}
	}
	else
	{
		// Transition complete
		transitionProgress = m_bHealthInDangerState ? 1.0f : 0.0f;
	}
	
	// Get the appropriate color with smooth transition
	Color normalColor = GetCustomSchemeColor( "FgColor" );
	Color dangerColor = GetDangerColor();
	Color healthColor = GetTransitionedColor( normalColor, dangerColor, transitionProgress );
	SetFgColor( healthColor );

	// Only update the fade if we've changed health
	if ( newHealth == m_iHealth )
	{
		// Update low health warning flash effect
		if ( m_flLowHealthWarningTime > 0.0f && gpGlobals->curtime <= m_flLowHealthWarningTime )
		{
			// Flash is active - calculation handled in PostChildPaint()
		}
		else if ( m_flLowHealthWarningTime > 0.0f )
		{
			// Flash finished, reset to proper color
			m_flLowHealthWarningTime = 0.0f;
			SetFgColor( healthColor ); // Ensure correct color after flash
		}
		return;
	}

	SetHealth( newHealth, true );
}

//-----------------------------------------------------------------------------
// Purpose: Determine if health is considered low (starts transition at 19 health)
//-----------------------------------------------------------------------------
bool CHudHealth::IsHealthLow( int health )
{
	// Low health threshold is 19 health - starts becoming fully red
	return ( health > 0 && health <= 19 );
}

//-----------------------------------------------------------------------------
// Purpose: Determine if health is critical (very low)
//-----------------------------------------------------------------------------
bool CHudHealth::IsHealthCritical( int health )
{
	// Critical health is 5 health or less
	return ( health > 0 && health <= 5 );
}

//-----------------------------------------------------------------------------
// Purpose: Get display color for health based on current health value
//-----------------------------------------------------------------------------
Color CHudHealth::GetHealthDisplayColor( int health )
{
	if ( health <= 0 )
	{
		// Dead - use danger color
		return GetDangerColor();
	}
	else if ( IsHealthLow( health ) )
	{
		// Low health - use danger color immediately (like aux power)
		return GetDangerColor();
	}
	else
	{
		// Normal health - read rb_hud_color dynamically (like suit power does)
		extern ConVar rb_hud_color;
		int r = 255, g = 255, b = 255;
		sscanf( rb_hud_color.GetString(), "%d %d %d", &r, &g, &b );
		return Color(r, g, b, 255);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates health display with enhanced danger state detection
//-----------------------------------------------------------------------------
void CHudHealth::SetHealth( int health, bool playAnimation )
{
	bool wasLow = IsHealthLow( m_iHealth );
	bool isLow = IsHealthLow( health );
	
	if ( health != m_iHealth )
	{
		if ( playAnimation )
		{
			// Only trigger blur and alpha animations, not color animations
			if ( health >= 20 )
			{
				// Use a custom animation that only does blur, not color
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("HealthIncreasedAbove20_NoColor");
			}
			else if ( health > 0 )
			{
				// Use custom animations that only do blur, not color
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("HealthIncreasedBelow20_NoColor");
				// Don't call HealthLow as it sets colors - we'll handle that manually
			}
			
			// Check for low health warning flash
			if ( !wasLow && isLow && health > 0 )
			{
				// Just became low health - trigger warning flash
				m_flLowHealthWarningTime = gpGlobals->curtime + 0.25f;
			}
		}
		
		// Update health color based on state - always set manually
		Color healthColor = GetHealthDisplayColor( health );
		SetFgColor( healthColor );
		
		m_iHealth = health;
	}
	
	SetDisplayValue( health );
}

//-----------------------------------------------------------------------------
// Purpose: Draw flash overlay after all other painting is complete
//-----------------------------------------------------------------------------
void CHudHealth::PostChildPaint( void )
{
	// Draw low health warning flash overlay after everything else
	if ( m_flLowHealthWarningTime > 0.0f && gpGlobals->curtime <= m_flLowHealthWarningTime )
	{
		// Calculate flash intensity (1.0 at start, 0.0 at end)
		float flFlashTime = 0.25f; // Total flash duration
		float flElapsed = flFlashTime - (m_flLowHealthWarningTime - gpGlobals->curtime);
		float flIntensity = 1.0f - (flElapsed / flFlashTime);
		
		// Create flash overlay color with max 60% opacity
		Color dangerColor = GetDangerColor();
		int flashAlpha = (int)(153 * flIntensity); // 153 = 60% of 255
		Color flashColor( dangerColor.r(), dangerColor.g(), dangerColor.b(), flashAlpha );
		
		// Draw rounded flash overlay that matches panel background
		int panelWide, panelTall;
		GetSize( panelWide, panelTall );
		
		// Use DrawBox to create rounded rectangle overlay
		DrawBox( 0, 0, panelWide, panelTall, flashColor, 1.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHealth::MsgFunc_Damage( bf_read &msg )
{

	int armor = msg.ReadByte();	// armor
	int damageTaken = msg.ReadByte();	// health
	long bitsDamage = msg.ReadLong(); // damage bits
	bitsDamage; // variable still sent but not used

	Vector vecFrom;

	vecFrom.x = msg.ReadBitCoord();
	vecFrom.y = msg.ReadBitCoord();
	vecFrom.z = msg.ReadBitCoord();

	// Actually took damage?
	if ( damageTaken > 0 || armor > 0 )
	{
		if ( damageTaken > 0 )
		{
			// Start blur animation only, handle colors manually
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("HealthDamageTaken_NoColor");
			
			// Set color manually based on current health state
			Color healthColor = GetHealthDisplayColor( m_iHealth );
			SetFgColor( healthColor );
		}
	}
}