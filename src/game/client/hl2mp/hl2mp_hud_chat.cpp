//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include <cstdio>
#include "hl2mp_hud_chat.h"
#include "hud_macros.h"
#include "text_message.h"
#include "vguicenterprint.h"
#include "vgui/ILocalize.h"
#include <vgui_controls/TextEntry.h>
#include "c_team.h"
#include "c_playerresource.h"
#include "c_hl2mp_player.h"
#include "hl2mp_gamerules.h"
#include "ihudlcd.h"

// Forward declaration of our custom color function
extern Color GetCustomSchemeColor( const char *colorName );



DECLARE_HUDELEMENT( CHudChat );

DECLARE_HUD_MESSAGE( CHudChat, SayText );
DECLARE_HUD_MESSAGE( CHudChat, SayText2 );
DECLARE_HUD_MESSAGE( CHudChat, TextMsg );


//=====================
//CHudChatLine
//=====================

void CHudChatLine::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );
}

//=====================
//CHudChatInputLine
//=====================

void CHudChatInputLine::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	
	// Update colors dynamically when scheme is applied
	UpdateChatColors();
}

void CHudChatInputLine::UpdateChatColors()
{
	// Read rb_hud_color dynamically for real-time updates
	extern ConVar rbcl_hud_color;
	int r = 255, g = 255, b = 255;
	sscanf( rbcl_hud_color.GetString(), "%d %d %d", &r, &g, &b );
	Color hudColor(r, g, b, 255);
	
	// Cast to TextEntry to access selection color methods
	vgui::TextEntry *pTextEntry = dynamic_cast<vgui::TextEntry*>(m_pInput);
	if (pTextEntry)
	{
		pTextEntry->SetFgColor( hudColor );
		pTextEntry->SetSelectionBgColor( hudColor );
		pTextEntry->SetSelectionTextColor( Color( 0, 0, 0, 255 ) );
	}
}

//=====================
//CHudChat
//=====================

CHudChat::CHudChat( const char *pElementName ) : BaseClass( pElementName )
{
	
}

void CHudChat::CreateChatInputLine( void )
{
	m_pChatInput = new CHudChatInputLine( this, "ChatInputLine" );
	m_pChatInput->SetVisible( false );
}

void CHudChat::CreateChatLines( void )
{
	m_ChatLine = new CHudChatLine( this, "ChatLine1" );
	m_ChatLine->SetVisible( false );	
}

void CHudChat::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
}


void CHudChat::Init( void )
{
	BaseClass::Init();

	HOOK_HUD_MESSAGE( CHudChat, SayText );
	HOOK_HUD_MESSAGE( CHudChat, SayText2 );
	HOOK_HUD_MESSAGE( CHudChat, TextMsg );
}

//-----------------------------------------------------------------------------
// Purpose: Overrides base reset to not cancel chat at round restart
//-----------------------------------------------------------------------------
void CHudChat::Reset( void )
{
}

int CHudChat::GetChatInputOffset( void )
{
	if ( m_pChatInput->IsVisible() )
	{
		return m_iFontHeight;
	}
	else
		return 0;
}

Color CHudChat::GetClientColor( int clientIndex )
{
	if ( clientIndex == 0 ) // console msg
	{
		extern ConVar rbcl_hud_color;
		Color hudColor(255, 255, 255, 255);
		int r, g, b;
		if (sscanf(rbcl_hud_color.GetString(), "%d %d %d", &r, &g, &b) == 3)
		{
			hudColor = Color(r, g, b, 255);
		}
		return hudColor;
	}
	else if( g_PR )
	{
		switch ( g_PR->GetTeam( clientIndex ) )
		{
		case TEAM_COMBINE	: return g_ColorBlue;
		case TEAM_REBELS	: return g_ColorRed;
		default	: 
			{
				extern ConVar rbcl_hud_color;
				Color hudColor(255, 255, 255, 255);
				int r, g, b;
				if (sscanf(rbcl_hud_color.GetString(), "%d %d %d", &r, &g, &b) == 3)
				{
					hudColor = Color(r, g, b, 255);
				}
				return hudColor;
			}
		}
	}

	extern ConVar rbcl_hud_color;
	Color hudColor(255, 255, 255, 255);
	int r, g, b;
	if (sscanf(rbcl_hud_color.GetString(), "%d %d %d", &r, &g, &b) == 3)
	{
		hudColor = Color(r, g, b, 255);
	}
	return hudColor;
}