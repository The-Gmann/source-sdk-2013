//========= Copyright Valve Corporation, All rights reserved. ============//

#include "cbase.h"
#include "dlight.h"
#include "iefx.h"
#include "basegrenade_shared.h"

// These should match the server values
#define FRAG_GRENADE_BLIP_FREQUENCY         1.0f
#define FRAG_GRENADE_BLIP_FAST_FREQUENCY    0.2f

extern ConVar rbcl_dlight_grenade;

class C_GrenadeFragLight : public CBaseGrenade
{
public:
    DECLARE_CLASS(C_GrenadeFragLight, CBaseGrenade);
    DECLARE_CLIENTCLASS();

    C_GrenadeFragLight();
    ~C_GrenadeFragLight();

    virtual void OnDataChanged(DataUpdateType_t updateType);
    virtual void ClientThink();

private:
    dlight_t* m_pDLight;
    int m_dlightKey;
    
    float m_flNextBlipTime;
    bool m_bFastBlink;
    
    void UpdateDLight();
};

IMPLEMENT_CLIENTCLASS_DT(C_GrenadeFragLight, DT_GrenadeFragLight, CGrenadeFrag)
    RecvPropFloat(RECVINFO(m_flNextBlipTime)),
    RecvPropBool(RECVINFO(m_bFastBlink)),
END_RECV_TABLE()

C_GrenadeFragLight::C_GrenadeFragLight()
{
    m_pDLight = nullptr;
    m_dlightKey = -1;
    m_flNextBlipTime = 0.0f;
    m_bFastBlink = false;
}

C_GrenadeFragLight::~C_GrenadeFragLight()
{
	if (m_pDLight)
	{
		m_pDLight->die = gpGlobals->curtime;
		m_pDLight = nullptr;
	}
}

void C_GrenadeFragLight::OnDataChanged(DataUpdateType_t updateType)
{
    BaseClass::OnDataChanged(updateType);

    if (updateType == DATA_UPDATE_CREATED)
    {
        m_dlightKey = entindex(); // Use entity index as the light key
        SetNextClientThink(gpGlobals->curtime); // Start thinking immediately
    }
}

void C_GrenadeFragLight::UpdateDLight()
{
	// Early out if dynamic lights are disabled
	extern ConVar rbcl_dlight_grenade;
	if (!rbcl_dlight_grenade.GetBool())
	{
		if (m_pDLight)
		{
			// Kill existing light if cvar is disabled
			m_pDLight->die = gpGlobals->curtime;
			m_pDLight = nullptr;
		}
		return;
	}

	if (!m_pDLight)
	{
		m_pDLight = effects->CL_AllocDlight(m_dlightKey);
		if (!m_pDLight)
		{
			return;
		}
	}

	float flBlinkDuration = m_bFastBlink ? FRAG_GRENADE_BLIP_FAST_FREQUENCY : FRAG_GRENADE_BLIP_FREQUENCY;
	float flTimeSinceLastBlip = gpGlobals->curtime - (m_flNextBlipTime - flBlinkDuration);
	float flBlinkProgress = flTimeSinceLastBlip / flBlinkDuration;

	// Calculate light intensity based on blink progress
	float flIntensity = 1.0f - fabs(2.0f * flBlinkProgress - 1.0f);

	// Base radius - increased by 2x
	float baseRadius = 80.0f;
	// Modulate radius by intensity with a factor of 0.5
	float modulatedRadius = baseRadius * (1.0f + 0.5f * (flIntensity - 1.0f));

	m_pDLight->origin = GetAbsOrigin();
	m_pDLight->radius = modulatedRadius;
	m_pDLight->die = gpGlobals->curtime + 0.2f;
	// Brightness increased by 2x (but clamped to max 255)
	m_pDLight->color.r = MIN( 255, 360 * flIntensity );
	m_pDLight->color.g = 0 * flIntensity;
	m_pDLight->color.b = 0 * flIntensity;
	m_pDLight->color.exponent = 1;
	m_pDLight->style = 0;
}

void C_GrenadeFragLight::ClientThink()
{
	// Only update dlight if the cvar is enabled
	extern ConVar rbcl_dlight_grenade;
	if (rbcl_dlight_grenade.GetBool())
	{
		UpdateDLight();
	}
	else if (m_pDLight)
	{
		// Kill the light if it exists and cvar is disabled
		m_pDLight->die = gpGlobals->curtime;
		m_pDLight = nullptr;
	}

	SetNextClientThink(gpGlobals->curtime);
	BaseClass::ClientThink();
}