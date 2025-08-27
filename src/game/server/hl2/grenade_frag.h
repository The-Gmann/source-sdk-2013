//========= Copyright Valve Corporation, All rights reserved. ============//

#ifndef GRENADE_FRAG_H
#define GRENADE_FRAG_H
#pragma once

#include "basegrenade_shared.h"

class CSprite;
class CSpriteTrail;

class CGrenadeFrag : public CBaseGrenade
{
    DECLARE_CLASS(CGrenadeFrag, CBaseGrenade);
    DECLARE_SERVERCLASS();
    DECLARE_DATADESC();

public:
    CGrenadeFrag();
    ~CGrenadeFrag();

    void Spawn(void);
    void OnRestore(void);
    void Precache(void);
    bool CreateVPhysics(void);
    void CreateEffects(void);
    void SetTimer(float detonateDelay, float warnDelay);
    void SetVelocity(const Vector &velocity, const AngularImpulse &angVelocity);
    int OnTakeDamage(const CTakeDamageInfo &inputInfo);
    void DelayThink();
    void VPhysicsUpdate(IPhysicsObject *pPhysics);
    void OnPhysGunPickup(CBasePlayer *pPhysGunUser, PhysGunPickup_t reason);
    void SetCombineSpawned(bool combineSpawned) { m_combineSpawned = combineSpawned; }
    bool IsCombineSpawned(void) const { return m_combineSpawned; }
    void SetPunted(bool punt) { m_punted = punt; }
    bool WasPunted(void) const { return m_punted; }

    void InputSetTimer(inputdata_t &inputdata);

protected:
    CHandle<CSprite>      m_pMainGlow;
    CHandle<CSpriteTrail> m_pGlowTrail;

    CNetworkVar(float, m_flNextBlipTime);
    CNetworkVar(bool, m_bFastBlink);

    bool   m_inSolid;
    bool   m_combineSpawned;
    bool   m_punted;
    float  m_flGlowTransparency;
    float  m_flGlowTrailTransparency;

private:
    void BlipSound() { EmitSound("Grenade.Blip"); }
};

// Free functions
CBaseGrenade *Fraggrenade_Create(const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, float timer, bool combineSpawned);
bool Fraggrenade_WasPunted(const CBaseEntity *pEntity);
bool Fraggrenade_WasCreatedByCombine(const CBaseEntity *pEntity);

#endif // GRENADE_FRAG_H