//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hud_suitpower.h"
#include "hud_macros.h"
#include "c_basehlplayer.h"
#include "iclientmode.h"
#include <vgui_controls/AnimationController.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <cstdio>

using namespace vgui;

// Access to the infinite aux power ConVar
extern ConVar rb_infinite_aux_power;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Forward declaration of our custom color function
extern Color GetCustomSchemeColor( const char *colorName );
extern Color GetDangerColor();
extern Color GetTransitionedColor( Color normalColor, Color dangerColor, float transitionProgress );

DECLARE_HUDELEMENT( CHudSuitPower );

// Suit power color transition duration constant
const float CHudSuitPower::SUITPOWER_COLOR_TRANSITION_DURATION = 0.5f;

#define SUITPOWER_INIT -1

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudSuitPower::CHudSuitPower( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudSuitPower" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	// Initialize color transition system
	m_bSuitPowerInDangerState = false;
	m_flSuitPowerColorTransitionTime = 0.0f;

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudSuitPower::Init( void )
{
	m_flSuitPower = SUITPOWER_INIT;
	m_nSuitPowerLow = -1;
	m_iActiveSuitDevices = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudSuitPower::Reset( void )
{
	Init();
	
	// Reset color transition system
	m_bSuitPowerInDangerState = false;
	m_flSuitPowerColorTransitionTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Apply scheme settings and use custom HUD colors
//-----------------------------------------------------------------------------
void CHudSuitPower::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	
	// Don't cache rb_hud_color values here - we'll read them dynamically in Paint method
	// to ensure real-time color updates
}

//-----------------------------------------------------------------------------
// Purpose: Save CPU cycles by letting the HUD system early cull
// costly traversal.  Called per frame, return true if thinking and 
// painting need to occur.
//-----------------------------------------------------------------------------
bool CHudSuitPower::ShouldDraw()
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	// Only draw if suit power is not at max or if any devices are active
	bool bNeedsDraw = ( pPlayer->m_HL2Local.m_flSuitPower < 100.0f ) || ( m_iActiveSuitDevices > 0 );

	return ( bNeedsDraw && CHudElement::ShouldDraw() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudSuitPower::OnThink( void )
{
	float flCurrentPower = 0;
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	flCurrentPower = pPlayer->m_HL2Local.m_flSuitPower;

	// get the suit power and send it to the hud
	if ( flCurrentPower != m_flSuitPower )
	{
		if ( flCurrentPower >= 20.0f && m_nSuitPowerLow )
		{
			m_nSuitPowerLow = false;
		}

		m_flSuitPower = flCurrentPower;
	}

	int newSuitDevices = 0;

	if ( pPlayer->IsFlashlightActive() )
	{
		newSuitDevices |= 0x00000001;
	}

	if ( pPlayer->IsSprinting() )
	{
		newSuitDevices |= 0x00000002;
	}

	if ( pPlayer->IsBreatherActive() )
	{
		newSuitDevices |= 0x00000004;
	}

	if ( newSuitDevices != m_iActiveSuitDevices )
	{
		m_iActiveSuitDevices = newSuitDevices;

		// count the number of active devices
		int numActiveDevices = 0;
		for ( int i = 0; i < 3; i++ )
		{
			if ( m_iActiveSuitDevices & (1 << i) )
			{
				numActiveDevices++;
			}
		}

		// animate the aux power panel to the right size/position
		switch ( numActiveDevices )
		{
		case 0:
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SuitAuxPowerNoItemsActive");
			break;
		case 1:
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SuitAuxPowerOneItemActive");
			break;
		case 2:
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SuitAuxPowerTwoItemsActive");
			break;
		default:
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SuitAuxPowerThreeItemsActive");
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: draws the power bar
//-----------------------------------------------------------------------------
void CHudSuitPower::Paint()
{
	// Don't show aux power HUD when infinite aux power is enabled
	if ( rb_infinite_aux_power.GetBool() )
		return;

	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	// Check if suit power is at 20% or below for danger color (2 bars out of 10)
	bool isLowPower = (m_flSuitPower <= 20.0f);
	
	// Handle color transition for aux power
	if ( isLowPower != m_bSuitPowerInDangerState )
	{
		// State changed, start transition
		m_bSuitPowerInDangerState = isLowPower;
		m_flSuitPowerColorTransitionTime = gpGlobals->curtime + SUITPOWER_COLOR_TRANSITION_DURATION;
	}
	
	// Calculate transition progress
	float transitionProgress = 0.0f;
	if ( m_flSuitPowerColorTransitionTime > gpGlobals->curtime )
	{
		// Transition in progress
		float timeRemaining = m_flSuitPowerColorTransitionTime - gpGlobals->curtime;
		transitionProgress = 1.0f - (timeRemaining / SUITPOWER_COLOR_TRANSITION_DURATION);
		
		if ( !m_bSuitPowerInDangerState )
		{
			// Transitioning back to normal, reverse the progress
			transitionProgress = 1.0f - transitionProgress;
		}
	}
	else
	{
		// Transition complete
		transitionProgress = m_bSuitPowerInDangerState ? 1.0f : 0.0f;
	}
	
	// Get the appropriate color with smooth transition
	Color normalColor = GetCustomSchemeColor( "FgColor" );
	Color dangerColor = GetDangerColor();
	Color auxPowerColor = GetTransitionedColor( normalColor, dangerColor, transitionProgress );

	// get bar chunks
	int chunkCount = m_flBarWidth / (m_flBarChunkWidth + m_flBarChunkGap);
	int enabledChunks = (int)((float)chunkCount * (m_flSuitPower * 1.0f/100.0f) + 0.5f );

	// draw the suit power bar
	surface()->DrawSetColor( auxPowerColor );
	int xpos = m_flBarInsetX, ypos = m_flBarInsetY;
	for (int i = 0; i < enabledChunks; i++)
	{
		surface()->DrawFilledRect( xpos, ypos, xpos + m_flBarChunkWidth, ypos + m_flBarHeight );
		xpos += (m_flBarChunkWidth + m_flBarChunkGap);
	}
	// draw the exhausted portion of the bar.
	surface()->DrawSetColor( Color( auxPowerColor[0], auxPowerColor[1], auxPowerColor[2], m_iAuxPowerDisabledAlpha ) );
	for (int i = enabledChunks; i < chunkCount; i++)
	{
		surface()->DrawFilledRect( xpos, ypos, xpos + m_flBarChunkWidth, ypos + m_flBarHeight );
		xpos += (m_flBarChunkWidth + m_flBarChunkGap);
	}

	// draw our name
	surface()->DrawSetTextFont(m_hTextFont);
	surface()->DrawSetTextColor(auxPowerColor);
	surface()->DrawSetTextPos(text_xpos, text_ypos);

	wchar_t *tempString = g_pVGuiLocalize->Find("#Valve_Hud_AUX_POWER");

	if (tempString)
	{
		surface()->DrawPrintText(tempString, wcslen(tempString));
	}
	else
	{
		surface()->DrawPrintText(L"AUX POWER", wcslen(L"AUX POWER"));
	}

	if ( m_iActiveSuitDevices )
	{
		// draw the additional text
		int ypos = text2_ypos;

		if (pPlayer->IsBreatherActive())
		{
			tempString = g_pVGuiLocalize->Find("#Valve_Hud_OXYGEN");

			surface()->DrawSetTextPos(text2_xpos, ypos);

			if (tempString)
			{
				surface()->DrawPrintText(tempString, wcslen(tempString));
			}
			else
			{
				surface()->DrawPrintText(L"OXYGEN", wcslen(L"OXYGEN"));
			}
			ypos += text2_gap;
		}

		if (pPlayer->IsFlashlightActive())
		{
			tempString = g_pVGuiLocalize->Find("#Valve_Hud_FLASHLIGHT");

			surface()->DrawSetTextPos(text2_xpos, ypos);

			if (tempString)
			{
				surface()->DrawPrintText(tempString, wcslen(tempString));
			}
			else
			{
				surface()->DrawPrintText(L"FLASHLIGHT", wcslen(L"FLASHLIGHT"));
			}
			ypos += text2_gap;
		}

		if (pPlayer->IsSprinting())
		{
			tempString = g_pVGuiLocalize->Find("#Valve_Hud_SPRINT");

			surface()->DrawSetTextPos(text2_xpos, ypos);

			if (tempString)
			{
				surface()->DrawPrintText(tempString, wcslen(tempString));
			}
			else
			{
				surface()->DrawPrintText(L"SPRINT", wcslen(L"SPRINT"));
			}
			ypos += text2_gap;
		}
	}
}


