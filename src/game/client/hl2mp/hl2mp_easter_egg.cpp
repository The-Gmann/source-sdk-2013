//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: HL2MP Easter Egg System - Client-side overlay for specific player
//
//=============================================================================

#include "cbase.h"
#include "hl2mp_easter_egg.h"
#include "vgui_controls/Panel.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "hud.h"
#include "hudelement.h"
#include "c_hl2mp_player.h"
#include "engine/IEngineSound.h"
#include "iclientmode.h"
#include "c_playerresource.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Kill Count Tracker System
//-----------------------------------------------------------------------------
class CHL2MPEasterEggManager : public CAutoGameSystem
{
public:
	CHL2MPEasterEggManager() : CAutoGameSystem("HL2MPEasterEggManager")
	{
		m_bEasterEggTriggered = false;
	}

	virtual void PostInit()
	{
		// Check if easter egg was already triggered for this player
		m_bEasterEggTriggered = HasEasterEggBeenTriggered();
	}

	virtual void LevelInitPreEntity()
	{
		// Re-check on level change
		m_bEasterEggTriggered = HasEasterEggBeenTriggered();
	}

	void CheckEasterEggTrigger();
	void TriggerEasterEgg();
	bool HasEasterEggBeenTriggered();
	void SaveEasterEggTriggered();

	public:
	bool m_bEasterEggTriggered;

	public:
	// Call this from HUD element to check trigger
	void CheckTrigger() { CheckEasterEggTrigger(); }
};

static CHL2MPEasterEggManager s_HL2MPEasterEggManager;

//-----------------------------------------------------------------------------
// Easter Egg HUD Element
//-----------------------------------------------------------------------------
class CHudEasterEgg : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE(CHudEasterEgg, vgui::Panel);

public:
	CHudEasterEgg(const char *pElementName);
	~CHudEasterEgg();

	virtual void Init();
	virtual void VidInit();
	virtual void Reset();
	virtual bool ShouldDraw();

	virtual void Paint();
	virtual void ApplySchemeSettings(vgui::IScheme *scheme);

	void ShowEasterEgg();
	void HideEasterEgg();

private:
	bool m_bShowEasterEgg;
	float m_flEasterEggStartTime;
	float m_flEasterEggDuration;
	
	int m_nTextureID;
};

DECLARE_HUDELEMENT(CHudEasterEgg);

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CHudEasterEgg::CHudEasterEgg(const char *pElementName) : CHudElement(pElementName), BaseClass(NULL, "HudEasterEgg")
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent(pParent);

	m_bShowEasterEgg = false;
	m_flEasterEggStartTime = 0.0f;
	m_flEasterEggDuration = 3.0f; // 3 seconds
	m_nTextureID = -1;

	SetHiddenBits(HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT);
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CHudEasterEgg::~CHudEasterEgg()
{
}

//-----------------------------------------------------------------------------
// Init
//-----------------------------------------------------------------------------
void CHudEasterEgg::Init()
{
	Reset();
}

//-----------------------------------------------------------------------------
// VidInit
//-----------------------------------------------------------------------------
void CHudEasterEgg::VidInit()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Reset
//-----------------------------------------------------------------------------
void CHudEasterEgg::Reset()
{
	m_bShowEasterEgg = false;
	m_flEasterEggStartTime = 0.0f;
}

//-----------------------------------------------------------------------------
// ShouldDraw
//-----------------------------------------------------------------------------
bool CHudEasterEgg::ShouldDraw()
{
	// Check for easter egg trigger regularly
	static float lastCheckTime = 0.0f;
	// Only check once per second to avoid performance issues
	if (gpGlobals->curtime - lastCheckTime > 1.0f)
	{
		lastCheckTime = gpGlobals->curtime;
		s_HL2MPEasterEggManager.CheckTrigger();
	}
	
	if (!m_bShowEasterEgg)
		return false;

	// Check if duration has expired
	if (gpGlobals->curtime > m_flEasterEggStartTime + m_flEasterEggDuration)
	{
		HideEasterEgg();
		return false;
	}

	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// ApplySchemeSettings
//-----------------------------------------------------------------------------
void CHudEasterEgg::ApplySchemeSettings(vgui::IScheme *scheme)
{
	BaseClass::ApplySchemeSettings(scheme);
	
	// Set panel to cover entire screen
	SetSize(ScreenWidth(), ScreenHeight());
	SetPos(0, 0);
	
	// Load the material
	if (m_nTextureID == -1)
	{
		m_nTextureID = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile(m_nTextureID, "easteregg/borya", true, false);
	}
}

//-----------------------------------------------------------------------------
// Paint
//-----------------------------------------------------------------------------
void CHudEasterEgg::Paint()
{
	if (!m_bShowEasterEgg || m_nTextureID == -1)
		return;

	// Get screen dimensions
	int screenWide, screenTall;
	vgui::surface()->GetScreenSize(screenWide, screenTall);

	// Calculate fade alpha based on time
	float timeElapsed = gpGlobals->curtime - m_flEasterEggStartTime;
	float alpha = 1.0f;
	
	// Fade in for first 0.5 seconds
	if (timeElapsed < 0.5f)
	{
		alpha = timeElapsed / 0.5f;
	}
	// Fade out for last 0.5 seconds
	else if (timeElapsed > m_flEasterEggDuration - 0.5f)
	{
		float fadeTime = m_flEasterEggDuration - timeElapsed;
		alpha = fadeTime / 0.5f;
	}

	alpha = clamp(alpha, 0.0f, 1.0f);

	// Set texture and color
	vgui::surface()->DrawSetTexture(m_nTextureID);
	vgui::surface()->DrawSetColor(255, 255, 255, (int)(255 * alpha));

	// Draw the texture scaled to screen size (2048x2048 -> 1920x1080)
	vgui::surface()->DrawTexturedRect(0, 0, screenWide, screenTall);
}

//-----------------------------------------------------------------------------
// ShowEasterEgg
//-----------------------------------------------------------------------------
void CHudEasterEgg::ShowEasterEgg()
{
	m_bShowEasterEgg = true;
	m_flEasterEggStartTime = gpGlobals->curtime;
	
	// Play the sound
	CLocalPlayerFilter filter;
	EmitSound_t soundParams;
	soundParams.m_pSoundName = "easteregg/borya.wav";
	soundParams.m_flVolume = 1.0f; // Maximum volume
	soundParams.m_nChannel = CHAN_STATIC;
	soundParams.m_SoundLevel = SNDLVL_GUNFIRE; // Very loud sound level
	soundParams.m_nPitch = PITCH_NORM;
	soundParams.m_nFlags = SND_CHANGE_VOL; // Allow volume changes
	
	C_BaseEntity::EmitSound(filter, SOUND_FROM_LOCAL_PLAYER, soundParams);
}

//-----------------------------------------------------------------------------
// HideEasterEgg
//-----------------------------------------------------------------------------
void CHudEasterEgg::HideEasterEgg()
{
	m_bShowEasterEgg = false;
}

//-----------------------------------------------------------------------------
// CHL2MPEasterEggManager Implementation
//-----------------------------------------------------------------------------
void CHL2MPEasterEggManager::CheckEasterEggTrigger()
{
	if (m_bEasterEggTriggered)
		return;

	// Check if easter egg was already triggered before
	if (HasEasterEggBeenTriggered())
	{
		m_bEasterEggTriggered = true;
		return;
	}

	// Check if local player is the target nickname (case insensitive)
	C_HL2MP_Player *pLocalPlayer = C_HL2MP_Player::GetLocalHL2MPPlayer();
	if (!pLocalPlayer)
		return;

	const char *playerName = pLocalPlayer->GetPlayerName();
	if (!playerName)
		return;

	// Check if nickname matches "ImgFrexYT" (case insensitive)
	if (Q_stricmp(playerName, "ImgFrexYT") != 0)
		return;

	// Get current kill count from player resource
	C_PlayerResource *pResource = dynamic_cast<C_PlayerResource*>(GameResources());
	if (!pResource)
		return;

	int playerIndex = pLocalPlayer->entindex();
	int killCount = pResource->GetPlayerScore(playerIndex);

	// Trigger easter egg on 5th kill
	if (killCount >= 5)
	{
		TriggerEasterEgg();
	}
}

void CHL2MPEasterEggManager::TriggerEasterEgg()
{
	m_bEasterEggTriggered = true;
	
	// Save that the easter egg has been triggered
	SaveEasterEggTriggered();
	
	CHudEasterEgg *pEasterEggHud = GET_HUDELEMENT(CHudEasterEgg);
	if (pEasterEggHud)
	{
		pEasterEggHud->ShowEasterEgg();
	}
}

//-----------------------------------------------------------------------------
// Check if easter egg file exists
//-----------------------------------------------------------------------------
bool CHL2MPEasterEggManager::HasEasterEggBeenTriggered()
{
	// Check if egg.txt exists in the scripts folder
	return filesystem->FileExists("scripts/egg.txt", "MOD");
}

//-----------------------------------------------------------------------------
// Save easter egg trigger state to file
//-----------------------------------------------------------------------------
void CHL2MPEasterEggManager::SaveEasterEggTriggered()
{
	// Create/write egg.txt in the scripts folder
	FileHandle_t file = filesystem->Open("scripts/egg.txt", "w", "MOD");
	if (file)
	{
		filesystem->Write("triggered\n", 10, file);
		filesystem->Close(file);
	}
}