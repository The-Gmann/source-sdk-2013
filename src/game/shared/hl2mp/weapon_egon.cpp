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
#include "AmmoDef.h"
#include "in_buttons.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "soundenvelope.h"
#include "sprite.h"

#ifdef CLIENT_DLL
    #include "c_hl2mp_player.h"
    #include "ClientEffectPrecacheSystem.h"
#else
    #include "hl2mp_player.h"
#endif

#ifdef CLIENT_DLL
    #define CWeaponEgon C_WeaponEgon
#endif

//----------------------------------------
// Definitions
//----------------------------------------
#define EGON_BEAM_SPRITE "sprites/xbeam1.vmt"
#define EGON_FLARE_SPRITE "sprites/xspark1.vmt"
#define EGON_BEAM_LENGTH 2048        // Lenght of the beam
#define EGON_DAMAGE_INTERVAL 0.1f    // Damage interval
#define EGON_AMMO_USE_RATE 0.2f     // 5 ammo per second
#define EGON_DMG_RADIUS 128.0f      // Damage radius

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------
// Client Effect Registration
//----------------------------------------
#ifdef CLIENT_DLL
    CLIENTEFFECT_REGISTER_BEGIN(PrecacheEffectEgon)
        CLIENTEFFECT_MATERIAL("sprites/xbeam1")
        CLIENTEFFECT_MATERIAL("sprites/xspark1")
    CLIENTEFFECT_REGISTER_END()
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

    CWeaponEgon(void);

    virtual void    Precache(void);
    virtual void    PrimaryAttack(void);
    virtual void    SecondaryAttack(void);
    virtual bool    Deploy(void);
    virtual bool    Holster(CBaseCombatWeapon *pSwitchingTo = NULL);
    virtual void    WeaponIdle(void);
    virtual void    ItemPostFrame(void);
    virtual bool    HasAmmo(void);

    void    Fire(const Vector &vecOrigin, const Vector &vecDir);
    void    StopFiring(void);
    void    CreateEffect(void);
    void    DestroyEffect(void);
    void    UpdateEffect(const Vector &startPoint, const Vector &endPoint);

    DECLARE_ACTTABLE();

enum EGON_FIRESTATE { FIRE_OFF, FIRE_STARTUP, FIRE_CHARGE };

protected:
    // Network variables
    CNetworkVar(float, m_flAmmoUseTime);
    CNetworkVar(float, m_flNextDamageTime);

    // Effect handles
    CNetworkHandle(CBeam, m_pBeam);
    CNetworkHandle(CBeam, m_pNoise);
    CNetworkHandle(CSprite, m_pSprite);

    // State variables
    EGON_FIRESTATE m_fireState;
    float m_flStartFireTime;
    float m_flShakeTime;
    float m_flStartSoundDuration;
};

//----------------------------------------
// ConVars
//----------------------------------------
ConVar sk_plr_dmg_egon("sk_plr_dmg_egon", "15", FCVAR_REPLICATED);

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Network Implementation
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

IMPLEMENT_NETWORKCLASS_ALIASED(WeaponEgon, DT_WeaponEgon)

BEGIN_NETWORK_TABLE(CWeaponEgon, DT_WeaponEgon)
    #ifdef CLIENT_DLL
        RecvPropFloat(RECVINFO(m_flAmmoUseTime)),
        RecvPropFloat(RECVINFO(m_flNextDamageTime)),
        RecvPropEHandle(RECVINFO(m_pBeam)),
        RecvPropEHandle(RECVINFO(m_pNoise)),
        RecvPropEHandle(RECVINFO(m_pSprite))
    #else
        SendPropFloat(SENDINFO(m_flAmmoUseTime)),
        SendPropFloat(SENDINFO(m_flNextDamageTime)),
        SendPropEHandle(SENDINFO(m_pBeam)),
        SendPropEHandle(SENDINFO(m_pNoise)),
        SendPropEHandle(SENDINFO(m_pSprite))
    #endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
    BEGIN_PREDICTION_DATA(CWeaponEgon)
        DEFINE_PRED_FIELD(m_flAmmoUseTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE),
        DEFINE_PRED_FIELD(m_flNextDamageTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE),
        DEFINE_PRED_FIELD(m_pBeam, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE),
        DEFINE_PRED_FIELD(m_pNoise, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE),
        DEFINE_PRED_FIELD(m_pSprite, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE)
    END_PREDICTION_DATA()
#endif

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Weapon Registration and Activity Table
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

LINK_ENTITY_TO_CLASS(weapon_egon, CWeaponEgon);
PRECACHE_WEAPON_REGISTER(weapon_egon);

acttable_t CWeaponEgon::m_acttable[] = 
{
    { ACT_HL2MP_IDLE,                    ACT_HL2MP_IDLE_AR2,                    false },
    { ACT_HL2MP_RUN,                     ACT_HL2MP_RUN_AR2,                     false },
    { ACT_HL2MP_IDLE_CROUCH,            ACT_HL2MP_IDLE_CROUCH_AR2,             false },
    { ACT_HL2MP_WALK_CROUCH,            ACT_HL2MP_WALK_CROUCH_AR2,             false },
    { ACT_HL2MP_GESTURE_RANGE_ATTACK,    ACT_HL2MP_GESTURE_RANGE_ATTACK_AR2,    false },
    { ACT_HL2MP_GESTURE_RELOAD,          ACT_HL2MP_GESTURE_RELOAD_AR2,          false },
    { ACT_HL2MP_JUMP,                    ACT_HL2MP_JUMP_AR2,                    false },
    { ACT_RANGE_ATTACK1,                 ACT_RANGE_ATTACK_AR2,                  false }
};

IMPLEMENT_ACTTABLE(CWeaponEgon);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponEgon::CWeaponEgon(void)
{
    m_flAmmoUseTime = 0;
    m_flNextDamageTime = 0;
    m_fireState = FIRE_OFF;
    m_flStartFireTime = 0;
    m_flShakeTime = 0;
#ifndef CLIENT_DLL
    m_pBeam = NULL;
    m_pNoise = NULL;
    m_pSprite = NULL;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Precache weapon assets
//-----------------------------------------------------------------------------
void CWeaponEgon::Precache(void)
{
    PrecacheModel(EGON_BEAM_SPRITE);
    PrecacheModel(EGON_FLARE_SPRITE);

    // Precache sounds
    const char *startSound = "Weapon_Gluon.Start";
    PrecacheScriptSound(startSound);
    PrecacheScriptSound("Weapon_Gluon.Run");
    PrecacheScriptSound("Weapon_Gluon.Off");

    // Get the actual wave file name from the script sound
    CSoundParameters params;
    if (GetParametersForSound(startSound, params, NULL))
    {
        m_flStartSoundDuration = enginesound->GetSoundDuration(params.soundname);
    }
    else
    {
        m_flStartSoundDuration = 2.93f; // Fallback to original value
    }

    BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Deploy weapon
//-----------------------------------------------------------------------------
bool CWeaponEgon::Deploy(void)
{
    m_fireState = FIRE_OFF;
    m_flAmmoUseTime = 0;
    m_flNextDamageTime = 0;

    return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: Check if weapon has ammo
//-----------------------------------------------------------------------------
bool CWeaponEgon::HasAmmo(void)
{
    CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
    if (!pPlayer)
        return false;

    if (pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
        return false;

    return true;
}

//-----------------------------------------------------------------------------
// Purpose: Primary attack - Start/continue beam
//-----------------------------------------------------------------------------
void CWeaponEgon::PrimaryAttack(void)
{
    CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
    if (!pPlayer)
        return;

    Vector vecAiming = pPlayer->GetAutoaimVector(0);
    Vector vecSrc = pPlayer->Weapon_ShootPosition();

    switch (m_fireState)
    {
        case FIRE_OFF:
        {
            if (!HasAmmo())
            {
                SendWeaponAnim(ACT_VM_DRYFIRE);  // Use dryfire animation when no ammo
                WeaponSound(EMPTY);
                m_flNextPrimaryAttack = gpGlobals->curtime + 0.25f;
                return;
            }

            m_flAmmoUseTime = gpGlobals->curtime;
            EmitSound("Weapon_Gluon.Start");
            
            SendWeaponAnim(ACT_VM_PULLBACK);  // Changed from ACT_VM_PRIMARYATTACK
            m_flStartFireTime = gpGlobals->curtime;
            
            m_fireState = FIRE_STARTUP;
			m_flNextDamageTime = gpGlobals->curtime + EGON_DAMAGE_INTERVAL;
        }
        break;

        case FIRE_STARTUP:
        {
            Fire(vecSrc, vecAiming);

            // Check if we've played the start sound for its full duration
            if (gpGlobals->curtime >= (m_flStartFireTime + m_flStartSoundDuration))
            {
                EmitSound("Weapon_Gluon.Run");
                m_fireState = FIRE_CHARGE;
            }

            if (!HasAmmo())
            {
                StopFiring();
                m_flNextPrimaryAttack = gpGlobals->curtime + 1.0f;
                return;
            }
        }
        break;

        case FIRE_CHARGE:
        {
            Fire(vecSrc, vecAiming);

            if (!HasAmmo())
            {
                StopFiring();
                m_flNextPrimaryAttack = gpGlobals->curtime + 1.0f;
                return;
            }
        }
        break;
    }
}

//-----------------------------------------------------------------------------
// Purpose: Secondary attack - do nothing
//-----------------------------------------------------------------------------
void CWeaponEgon::SecondaryAttack(void)
{
    // Do nothing
    return;
}

//-----------------------------------------------------------------------------
// Purpose: Stop firing the egon
//-----------------------------------------------------------------------------
void CWeaponEgon::StopFiring(void)
{
    if (m_fireState != FIRE_OFF)
    {
        StopSound("Weapon_Gluon.Run");
        EmitSound("Weapon_Gluon.Off");
        
        SendWeaponAnim(ACT_VM_IDLE);  // Return to idle animation
        DestroyEffect();
    }

    m_fireState = FIRE_OFF;
    m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;
    SetWeaponIdleTime(gpGlobals->curtime + 0.5f);
}

//-----------------------------------------------------------------------------
// Purpose: Handle holstering weapon
//-----------------------------------------------------------------------------
bool CWeaponEgon::Holster(CBaseCombatWeapon *pSwitchingTo)
{
    StopSound("Weapon_Gluon.Start");
    StopSound("Weapon_Gluon.Run");
    
    StopFiring();
    return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: Idle updates
//-----------------------------------------------------------------------------
void CWeaponEgon::WeaponIdle(void)
{
    if (!HasWeaponIdleTimeElapsed())
        return;

    if (m_fireState != FIRE_OFF)
        StopFiring();

    float flRand = random->RandomFloat(0, 1);
    if (flRand <= 0.5)
    {
        SendWeaponAnim(ACT_VM_IDLE);
        SetWeaponIdleTime(gpGlobals->curtime + random->RandomFloat(10, 15));
    }
    else
    {
        SendWeaponAnim(ACT_VM_FIDGET);
        SetWeaponIdleTime(gpGlobals->curtime + 3.0);
    }
}

//-----------------------------------------------------------------------------
// Purpose: Create the beam and sprite effects
//-----------------------------------------------------------------------------
void CWeaponEgon::CreateEffect(void)
{
#ifndef CLIENT_DLL
    CBasePlayer *pOwner = ToBasePlayer(GetOwner());
    if (!pOwner)
        return;

    DestroyEffect();

    m_pBeam = CBeam::BeamCreate(EGON_BEAM_SPRITE, 7.0f);
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

    m_pNoise = CBeam::BeamCreate(EGON_BEAM_SPRITE, 8.0f);
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

    // Create and parent sprite to beam
    m_pSprite = CSprite::SpriteCreate(EGON_FLARE_SPRITE, GetAbsOrigin(), false);
    if (m_pSprite && m_pBeam)
    {
        m_pSprite->SetScale(1.0f);
        m_pSprite->SetTransparency(kRenderGlow, 255, 255, 255, 255, kRenderFxNoDissipation);
        m_pSprite->AddSpawnFlags(SF_SPRITE_TEMPORARY);
        m_pSprite->SetOwnerEntity(pOwner);
        m_pSprite->SetParent(m_pBeam);
    }
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Update beam effect positions
//-----------------------------------------------------------------------------
void CWeaponEgon::UpdateEffect(const Vector &startPoint, const Vector &endPoint)
{
#ifndef CLIENT_DLL
    if (!m_pBeam)
    {
        CreateEffect();
    }

    if (m_pBeam)
    {
        m_pBeam->SetStartPos(endPoint);
    }

    if (m_pNoise)
    {
        m_pNoise->SetStartPos(endPoint);
    }

    // Only update sprite animation since position is handled by parenting
    if (m_pSprite)
    {
        m_pSprite->m_flFrame += 8 * gpGlobals->frametime;
        if (m_pSprite->m_flFrame > m_pSprite->Frames())
            m_pSprite->m_flFrame = 0;
    }
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Remove all beam effects
//-----------------------------------------------------------------------------
void CWeaponEgon::DestroyEffect(void)
{
#ifndef CLIENT_DLL
    if (m_pBeam)
    {
        UTIL_Remove(m_pBeam);
        m_pBeam = NULL;
    }
    if (m_pNoise)
    {
        UTIL_Remove(m_pNoise);
        m_pNoise = NULL;
    }
    if (m_pSprite)
    {
        UTIL_Remove(m_pSprite);
        m_pSprite = NULL;
    }
#else
    m_pBeam = NULL;
    m_pNoise = NULL;
    m_pSprite = NULL;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Fire the beam and apply damage
//-----------------------------------------------------------------------------
void CWeaponEgon::Fire(const Vector &vecOrigin, const Vector &vecDir)
{
    CBasePlayer *pOwner = ToBasePlayer(GetOwner());
    if (!pOwner)
        return;

    Vector vecEnd = vecOrigin + (vecDir * EGON_BEAM_LENGTH);
    trace_t tr;

    UTIL_TraceLine(vecOrigin, vecEnd, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr);

    // Create/update the beam effect
    if (!m_pBeam)
        CreateEffect();
    UpdateEffect(vecOrigin, tr.endpos);

    // Do damage if it's time
    if (gpGlobals->curtime >= m_flNextDamageTime)
    {
    #ifndef CLIENT_DLL
        // Direct damage to hit entity
        if (tr.m_pEnt && tr.m_pEnt->m_takedamage != DAMAGE_NO)
        {
            CTakeDamageInfo dmgInfo(this, pOwner, sk_plr_dmg_egon.GetFloat(), DMG_ENERGYBEAM | DMG_ALWAYSGIB);
            CalculateMeleeDamageForce(&dmgInfo, vecDir, tr.endpos);
            tr.m_pEnt->DispatchTraceAttack(dmgInfo, vecDir, &tr);
            ApplyMultiDamage();
        }

        // Radius damage - exactly 1/4 of direct damage as per HL1
        CTakeDamageInfo radiusDmgInfo(this, pOwner, sk_plr_dmg_egon.GetFloat() * 0.25f, 
            DMG_ENERGYBEAM | DMG_BLAST | DMG_ALWAYSGIB);
        RadiusDamage(radiusDmgInfo, tr.endpos, EGON_DMG_RADIUS, CLASS_NONE, NULL);

        // Screen shake
        if (m_flShakeTime < gpGlobals->curtime)
        {
            UTIL_ScreenShake(tr.endpos, 5.0f, 150.0f, 0.75f, 250.0f, SHAKE_START);
            m_flShakeTime = gpGlobals->curtime + 1.5f;
        }
    #endif
        m_flNextDamageTime = gpGlobals->curtime + EGON_DAMAGE_INTERVAL;
    }

    // Separate ammo consumption timing from damage timing
    if (gpGlobals->curtime >= m_flAmmoUseTime)
    {
    #ifndef CLIENT_DLL
        pOwner->RemoveAmmo(1, m_iPrimaryAmmoType);
    #endif
        m_flAmmoUseTime = gpGlobals->curtime + EGON_AMMO_USE_RATE;
    }

    // Impact effects
    if (tr.fraction != 1.0)
    {
        if ((tr.surface.flags & SURF_SKY) == 0)
        {
#ifndef CLIENT_DLL
            UTIL_ScreenShake(tr.endpos, 5.0f, 150.0f, 0.25f, 150.0f, SHAKE_START);
#endif
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose: Update weapon state each frame
//-----------------------------------------------------------------------------
void CWeaponEgon::ItemPostFrame(void)
{
    CBasePlayer *pOwner = ToBasePlayer(GetOwner());
    if (!pOwner)
        return;

    // If we're firing
    if (m_fireState != FIRE_OFF)
    {
        // Stop firing if button released or out of ammo
        if (!(pOwner->m_nButtons & IN_ATTACK))
        {
            StopFiring();
        }
        else if (!HasAmmo())
        {
            StopFiring();
        }
        else
        {
            // Continue firing
            PrimaryAttack();
        }
    }
    else
    {
        // Start firing if button is pressed
        if ((pOwner->m_nButtons & IN_ATTACK) && HasAmmo())
        {
            PrimaryAttack();
        }
    }

    BaseClass::ItemPostFrame();
}