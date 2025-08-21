//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//
// EGON/GLUON GUN IMPLEMENTATION
// Half-Life 2: Deathmatch Reborn
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//----------------------------------------
// Includes
//----------------------------------------
#include "cbase.h"
#include "beam_shared.h"
#include "in_buttons.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "soundenvelope.h"
#include "sprite.h"

#ifdef CLIENT_DLL
    #include "c_hl2mp_player.h"
    #include "ClientEffectPrecacheSystem.h"
#else
    #include "hl2mp_player.h"
    #include "te_effect_dispatch.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------
// Constants
//----------------------------------------
namespace EgonConstants
{
    // Sprites and models
    static const char* BEAM_SPRITE = "sprites/xbeam1.vmt";
    static const char* FLARE_SPRITE = "sprites/xspark1.vmt";
    
    // Beam properties
    static constexpr float BEAM_LENGTH = 2048.0f;
    static constexpr float BEAM_WIDTH = 7.0f;
    static constexpr float NOISE_WIDTH = 8.0f;
    static constexpr float SPRITE_SCALE = 1.0f;
    
    // Timing constants
    static constexpr float DAMAGE_INTERVAL = 0.1f;
    static constexpr float AMMO_USE_RATE = 0.2f;
    static constexpr float STARTUP_DELAY = 0.5f;
    static constexpr float SHAKE_COOLDOWN = 1.5f;
    static constexpr float DEFAULT_SOUND_DURATION = 2.93f;
    
    // Damage properties
    static constexpr float DMG_RADIUS = 128.0f;
    static constexpr float RADIUS_DAMAGE_MULTIPLIER = 0.25f;
    
    // Sound names
    static const char* SOUND_START = "Weapon_Gluon.Start";
    static const char* SOUND_RUN = "Weapon_Gluon.Run";
    static const char* SOUND_OFF = "Weapon_Gluon.Off";
}

//----------------------------------------
// ConVars
//----------------------------------------
ConVar sk_plr_dmg_egon("sk_plr_dmg_egon", "15", FCVAR_REPLICATED, "Egon weapon damage");

//----------------------------------------
// Client Effect Registration
//----------------------------------------
#ifdef CLIENT_DLL
    CLIENTEFFECT_REGISTER_BEGIN(PrecacheEffectEgon)
        CLIENTEFFECT_MATERIAL("sprites/xbeam1")
        CLIENTEFFECT_MATERIAL("sprites/xspark1")
    CLIENTEFFECT_REGISTER_END()

    #define CWeaponEgon C_WeaponEgon
#endif

//----------------------------------------
// Weapon Class Declaration
//----------------------------------------
class CWeaponEgon : public CBaseHL2MPCombatWeapon
{
public:
    DECLARE_CLASS(CWeaponEgon, CBaseHL2MPCombatWeapon);
    DECLARE_NETWORKCLASS();
    DECLARE_PREDICTABLE();
    DECLARE_ACTTABLE();

    CWeaponEgon();
    virtual ~CWeaponEgon();

    // Base weapon overrides
    void Precache() override;
    void PrimaryAttack() override;
    void SecondaryAttack() override;
    bool Deploy() override;
    bool Holster(CBaseCombatWeapon *pSwitchingTo = nullptr) override;
    void WeaponIdle() override;
    void ItemPostFrame() override;

    // Public utility methods
    bool HasAmmo() const;

private:
    enum EFireState 
    { 
        FIRE_OFF, 
        FIRE_STARTUP, 
        FIRE_CHARGE 
    };

    // Network variables
    CNetworkVar(float, m_flAmmoUseTime);
    CNetworkVar(float, m_flNextDamageTime);
    CNetworkVar(EFireState, m_fireState);

    // Effect handles
    CNetworkHandle(CBeam, m_pBeam);
    CNetworkHandle(CBeam, m_pNoise);
    CNetworkHandle(CSprite, m_pSprite);

    // State variables
    float m_flStartFireTime;
    float m_flShakeTime;
    float m_flStartSoundDuration;
    bool m_bTransitionedToCharge;

    // Core weapon methods
    void StartFiring();
    void StopFiring();
    void Fire();
    
    // Effect management
    void CreateEffects();
    void UpdateEffects(const Vector &startPoint, const Vector &endPoint);
    void DestroyEffects();
    
    // Damage and utility
    void ProcessDamage(const trace_t &tr, const Vector &direction);
    void ProcessAmmoConsumption();
    void HandleFireStateTransition();
    
    // Validation helpers
    bool IsValidOwner() const;
    CBasePlayer* GetPlayerOwner() const;
};

// Network table implementation
IMPLEMENT_NETWORKCLASS_ALIASED(WeaponEgon, DT_WeaponEgon)

BEGIN_NETWORK_TABLE(CWeaponEgon, DT_WeaponEgon)
#ifdef CLIENT_DLL
    RecvPropFloat(RECVINFO(m_flAmmoUseTime)),
    RecvPropFloat(RECVINFO(m_flNextDamageTime)),
    RecvPropInt(RECVINFO(m_fireState)),
    RecvPropEHandle(RECVINFO(m_pBeam)),
    RecvPropEHandle(RECVINFO(m_pNoise)),
    RecvPropEHandle(RECVINFO(m_pSprite)),
#else
    SendPropFloat(SENDINFO(m_flAmmoUseTime)),
    SendPropFloat(SENDINFO(m_flNextDamageTime)),
    SendPropInt(SENDINFO(m_fireState)),
    SendPropEHandle(SENDINFO(m_pBeam)),
    SendPropEHandle(SENDINFO(m_pNoise)),
    SendPropEHandle(SENDINFO(m_pSprite)),
#endif
END_NETWORK_TABLE()

// Prediction table
#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA(CWeaponEgon)
    DEFINE_PRED_FIELD(m_flAmmoUseTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE),
    DEFINE_PRED_FIELD(m_flNextDamageTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE),
    DEFINE_PRED_FIELD(m_fireState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS(weapon_egon, CWeaponEgon);
PRECACHE_WEAPON_REGISTER(weapon_egon);

// Activity table
acttable_t CWeaponEgon::m_acttable[] =
{
    { ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_PHYSGUN,					false },
    { ACT_HL2MP_RUN,					ACT_HL2MP_RUN_PHYSGUN,					false },
    { ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_PHYSGUN,			false },
    { ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_PHYSGUN,			false },
    { ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_PHYSGUN,	false },
    { ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_PHYSGUN,		false },
    { ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_PHYSGUN,					false },
};

IMPLEMENT_ACTTABLE(CWeaponEgon);

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CWeaponEgon::CWeaponEgon()
{
    m_flAmmoUseTime = 0.0f;
    m_flNextDamageTime = 0.0f;
    m_fireState = FIRE_OFF;
    m_flStartFireTime = 0.0f;
    m_flShakeTime = 0.0f;
    m_flStartSoundDuration = EgonConstants::DEFAULT_SOUND_DURATION;
    m_bTransitionedToCharge = false;

#ifndef CLIENT_DLL
    m_pBeam = nullptr;
    m_pNoise = nullptr;
    m_pSprite = nullptr;
#endif
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CWeaponEgon::~CWeaponEgon()
{
    DestroyEffects();
}

//-----------------------------------------------------------------------------
// Precache assets
//-----------------------------------------------------------------------------
void CWeaponEgon::Precache()
{
    PrecacheModel(EgonConstants::BEAM_SPRITE);
    PrecacheModel(EgonConstants::FLARE_SPRITE);

    // Precache sounds and get actual duration
    PrecacheScriptSound(EgonConstants::SOUND_START);
    PrecacheScriptSound(EgonConstants::SOUND_RUN);
    PrecacheScriptSound(EgonConstants::SOUND_OFF);

    // Get actual sound duration for timing
    CSoundParameters params;
    if (GetParametersForSound(EgonConstants::SOUND_START, params, nullptr))
    {
        m_flStartSoundDuration = enginesound->GetSoundDuration(params.soundname);
    }

    BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Deploy weapon
//-----------------------------------------------------------------------------
bool CWeaponEgon::Deploy()
{
    StopFiring();
    return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Holster weapon
//-----------------------------------------------------------------------------
bool CWeaponEgon::Holster(CBaseCombatWeapon *pSwitchingTo)
{
    StopSound(EgonConstants::SOUND_START);
    StopSound(EgonConstants::SOUND_RUN);
    StopFiring();
    
    return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Check ammo availability
//-----------------------------------------------------------------------------
bool CWeaponEgon::HasAmmo() const
{
    const CBasePlayer *pPlayer = GetPlayerOwner();
    return pPlayer && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) > 0;
}

//-----------------------------------------------------------------------------
// Validate owner
//-----------------------------------------------------------------------------
bool CWeaponEgon::IsValidOwner() const
{
    return GetPlayerOwner() != nullptr;
}

//-----------------------------------------------------------------------------
// Get player owner safely
//-----------------------------------------------------------------------------
CBasePlayer* CWeaponEgon::GetPlayerOwner() const
{
    return ToBasePlayer(GetOwner());
}

//-----------------------------------------------------------------------------
// Primary attack handler
//-----------------------------------------------------------------------------
void CWeaponEgon::PrimaryAttack()
{
    if (!IsValidOwner())
        return;

    switch (m_fireState)
    {
        case FIRE_OFF:
            StartFiring();
            break;

        case FIRE_STARTUP:
        case FIRE_CHARGE:
            if (!HasAmmo())
            {
                StopFiring();
                m_flNextPrimaryAttack = gpGlobals->curtime + 1.0f;
                return;
            }

            Fire();
            HandleFireStateTransition();
            break;
    }
}

//-----------------------------------------------------------------------------
// Secondary attack (unused)
//-----------------------------------------------------------------------------
void CWeaponEgon::SecondaryAttack()
{
    // Intentionally empty - no secondary fire for Egon
}

//-----------------------------------------------------------------------------
// Start firing sequence
//-----------------------------------------------------------------------------
void CWeaponEgon::StartFiring()
{
    if (!HasAmmo())
    {
        SendWeaponAnim(ACT_VM_DRYFIRE);
        WeaponSound(EMPTY);
        m_flNextPrimaryAttack = gpGlobals->curtime + 0.25f;
        return;
    }

    // Initialize timing
    const float currentTime = gpGlobals->curtime;
    m_flAmmoUseTime = currentTime;
    m_flStartFireTime = currentTime;
    m_flNextDamageTime = currentTime + EgonConstants::DAMAGE_INTERVAL;
    m_bTransitionedToCharge = false;

    // Audio and visual feedback
    EmitSound(EgonConstants::SOUND_START);
    SendWeaponAnim(ACT_VM_PULLBACK);
    
    m_fireState = FIRE_STARTUP;
}

//-----------------------------------------------------------------------------
// Stop firing sequence
//-----------------------------------------------------------------------------
void CWeaponEgon::StopFiring()
{
    if (m_fireState == FIRE_OFF)
        return;

    // Stop sounds and play off sound
    StopSound(EgonConstants::SOUND_RUN);
    EmitSound(EgonConstants::SOUND_OFF);
    
    // Reset animation and effects
    SendWeaponAnim(ACT_VM_IDLE);
    DestroyEffects();

    // Reset state
    m_fireState = FIRE_OFF;
    m_bTransitionedToCharge = false;
    m_flNextPrimaryAttack = gpGlobals->curtime + EgonConstants::STARTUP_DELAY;
    SetWeaponIdleTime(gpGlobals->curtime + EgonConstants::STARTUP_DELAY);
}

//-----------------------------------------------------------------------------
// Handle fire state transitions
//-----------------------------------------------------------------------------
void CWeaponEgon::HandleFireStateTransition()
{
    // Only transition once from startup to charge
    if (m_fireState == FIRE_STARTUP && 
        !m_bTransitionedToCharge &&
        gpGlobals->curtime >= (m_flStartFireTime + m_flStartSoundDuration))
    {
        EmitSound(EgonConstants::SOUND_RUN);
        m_fireState = FIRE_CHARGE;
        m_bTransitionedToCharge = true;
    }
}

//-----------------------------------------------------------------------------
// Main firing logic
//-----------------------------------------------------------------------------
void CWeaponEgon::Fire()
{
    CBasePlayer *pOwner = GetPlayerOwner();
    if (!pOwner)
        return;

    // Get aim direction and source
    Vector vecAiming = pOwner->GetAutoaimVector(0);
    Vector vecSrc = pOwner->Weapon_ShootPosition();
    Vector vecEnd = vecSrc + (vecAiming * EgonConstants::BEAM_LENGTH);

    // Perform trace
    trace_t tr;
    UTIL_TraceLine(vecSrc, vecEnd, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr);

    // Update visual effects
    UpdateEffects(vecSrc, tr.endpos);

    // Process damage at intervals
    if (gpGlobals->curtime >= m_flNextDamageTime)
    {
        ProcessDamage(tr, vecAiming);
        m_flNextDamageTime = gpGlobals->curtime + EgonConstants::DAMAGE_INTERVAL;
    }

    // Consume ammo
    ProcessAmmoConsumption();

    // Impact effects for valid surfaces
    if (tr.fraction < 1.0f && !(tr.surface.flags & SURF_SKY))
    {
#ifndef CLIENT_DLL
        UTIL_ScreenShake(tr.endpos, 5.0f, 150.0f, 0.25f, 150.0f, SHAKE_START);
#endif
    }
}

//-----------------------------------------------------------------------------
// Create beam and sprite effects
//-----------------------------------------------------------------------------
void CWeaponEgon::CreateEffects()
{
#ifndef CLIENT_DLL
    CBasePlayer *pOwner = GetPlayerOwner();
    if (!pOwner)
        return;

    DestroyEffects(); // Clean up first

    // Create primary beam
    m_pBeam = CBeam::BeamCreate(EgonConstants::BEAM_SPRITE, EgonConstants::BEAM_WIDTH);
    if (m_pBeam)
    {
        m_pBeam->PointEntInit(GetAbsOrigin(), this);
        m_pBeam->SetBeamFlags(FBEAM_SINENOISE);
        m_pBeam->SetEndAttachment(1);
        m_pBeam->AddSpawnFlags(SF_BEAM_TEMPORARY);
        m_pBeam->SetOwnerEntity(pOwner);
        m_pBeam->SetScrollRate(50);
        m_pBeam->SetBrightness(200);
        m_pBeam->SetColor(50, 215, 255);
        m_pBeam->SetNoise(0.2f);
    }

    // Create noise beam
    m_pNoise = CBeam::BeamCreate(EgonConstants::BEAM_SPRITE, EgonConstants::NOISE_WIDTH);
    if (m_pNoise)
    {
        m_pNoise->PointEntInit(GetAbsOrigin(), this);
        m_pNoise->SetEndAttachment(1);
        m_pNoise->AddSpawnFlags(SF_BEAM_TEMPORARY);
        m_pNoise->SetOwnerEntity(pOwner);
        m_pNoise->SetScrollRate(35);
        m_pNoise->SetBrightness(200);
        m_pNoise->SetColor(50, 240, 255);
        m_pNoise->SetNoise(0.8f);
    }

    // Create end sprite
    m_pSprite = CSprite::SpriteCreate(EgonConstants::FLARE_SPRITE, GetAbsOrigin(), false);
    if (m_pSprite)
    {
        m_pSprite->SetScale(EgonConstants::SPRITE_SCALE);
        m_pSprite->SetTransparency(kRenderGlow, 255, 255, 255, 255, kRenderFxNoDissipation);
        m_pSprite->AddSpawnFlags(SF_SPRITE_TEMPORARY);
        m_pSprite->SetOwnerEntity(pOwner);
        
        if (m_pBeam)
            m_pSprite->SetParent(m_pBeam);
    }
#endif
}

//-----------------------------------------------------------------------------
// Update effect positions
//-----------------------------------------------------------------------------
void CWeaponEgon::UpdateEffects(const Vector &startPoint, const Vector &endPoint)
{
#ifndef CLIENT_DLL
    if (!m_pBeam)
        CreateEffects();

    if (m_pBeam)
        m_pBeam->SetStartPos(endPoint);

    if (m_pNoise)
        m_pNoise->SetStartPos(endPoint);

    // Animate sprite
    if (m_pSprite)
    {
        m_pSprite->m_flFrame += 8.0f * gpGlobals->frametime;
        if (m_pSprite->m_flFrame > m_pSprite->Frames())
            m_pSprite->m_flFrame = 0.0f;
    }
#endif
}

//-----------------------------------------------------------------------------
// Clean up all effects
//-----------------------------------------------------------------------------
void CWeaponEgon::DestroyEffects()
{
#ifndef CLIENT_DLL
    if (m_pBeam)
    {
        UTIL_Remove(m_pBeam);
        m_pBeam = nullptr;
    }
    if (m_pNoise)
    {
        UTIL_Remove(m_pNoise);
        m_pNoise = nullptr;
    }
    if (m_pSprite)
    {
        UTIL_Remove(m_pSprite);
        m_pSprite = nullptr;
    }
#else
    m_pBeam = nullptr;
    m_pNoise = nullptr;
    m_pSprite = nullptr;
#endif
}

//-----------------------------------------------------------------------------
// Process damage to targets
//-----------------------------------------------------------------------------
void CWeaponEgon::ProcessDamage(const trace_t &tr, const Vector &direction)
{
#ifndef CLIENT_DLL
    CBasePlayer *pOwner = GetPlayerOwner();
    if (!pOwner)
        return;

    const float baseDamage = sk_plr_dmg_egon.GetFloat();

    // Direct damage to hit entity
    if (tr.m_pEnt && tr.m_pEnt->m_takedamage != DAMAGE_NO)
    {
        CTakeDamageInfo directDmg(this, pOwner, baseDamage, DMG_ENERGYBEAM | DMG_ALWAYSGIB);
        CalculateMeleeDamageForce(&directDmg, direction, tr.endpos);
        
        // Cast away const since DispatchTraceAttack doesn't modify the trace meaningfully
        tr.m_pEnt->DispatchTraceAttack(directDmg, direction, const_cast<trace_t*>(&tr));
        ApplyMultiDamage();
    }

    // Radius damage
    const float radiusDamage = baseDamage * EgonConstants::RADIUS_DAMAGE_MULTIPLIER;
    CTakeDamageInfo radiusDmgInfo(this, pOwner, radiusDamage, 
                                 DMG_ENERGYBEAM | DMG_BLAST | DMG_ALWAYSGIB);
    RadiusDamage(radiusDmgInfo, tr.endpos, EgonConstants::DMG_RADIUS, CLASS_NONE, nullptr);

    // Screen shake effect (rate limited)
    if (m_flShakeTime < gpGlobals->curtime)
    {
        UTIL_ScreenShake(tr.endpos, 5.0f, 150.0f, 0.75f, 250.0f, SHAKE_START);
        m_flShakeTime = gpGlobals->curtime + EgonConstants::SHAKE_COOLDOWN;
    }
#endif
}

//-----------------------------------------------------------------------------
// Handle ammo consumption
//-----------------------------------------------------------------------------
void CWeaponEgon::ProcessAmmoConsumption()
{
    if (gpGlobals->curtime >= m_flAmmoUseTime)
    {
#ifndef CLIENT_DLL
        CBasePlayer *pOwner = GetPlayerOwner();
        if (pOwner)
        {
            pOwner->RemoveAmmo(1, m_iPrimaryAmmoType);
        }
#endif
        m_flAmmoUseTime = gpGlobals->curtime + EgonConstants::AMMO_USE_RATE;
    }
}

//-----------------------------------------------------------------------------
// Weapon idle behavior
//-----------------------------------------------------------------------------
void CWeaponEgon::WeaponIdle()
{
    if (!HasWeaponIdleTimeElapsed())
        return;

    if (m_fireState != FIRE_OFF)
    {
        StopFiring();
        return;
    }

    // Randomized idle animations
    const bool useBasicIdle = random->RandomFloat(0.0f, 1.0f) <= 0.5f;
    
    if (useBasicIdle)
    {
        SendWeaponAnim(ACT_VM_IDLE);
        SetWeaponIdleTime(gpGlobals->curtime + random->RandomFloat(10.0f, 15.0f));
    }
    else
    {
        SendWeaponAnim(ACT_VM_FIDGET);
        SetWeaponIdleTime(gpGlobals->curtime + 3.0f);
    }
}

//-----------------------------------------------------------------------------
// Frame update logic
//-----------------------------------------------------------------------------
void CWeaponEgon::ItemPostFrame()
{
    if (!IsValidOwner())
        return;

    CBasePlayer *pOwner = GetPlayerOwner();
    const bool attackPressed = !!(pOwner->m_nButtons & IN_ATTACK);

    if (m_fireState != FIRE_OFF)
    {
        if (!attackPressed || !HasAmmo())
        {
            StopFiring();
        }
        else
        {
            PrimaryAttack();
        }
    }
    else if (attackPressed && HasAmmo())
    {
        PrimaryAttack();
    }

    BaseClass::ItemPostFrame();
}