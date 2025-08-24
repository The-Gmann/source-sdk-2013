//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//
// GAUSS GUN IMPLEMENTATION
// Half-Life 2: Deathmatch Reborn
// Credit to SIOSPHERE/Sub-Zero
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
#include "effect_dispatch_data.h"

#ifdef CLIENT_DLL
    #include "c_hl2mp_player.h"
    #include "ClientEffectPrecacheSystem.h"
    #include "c_te_effect_dispatch.h"
#else
    #include "hl2mp_player.h"
    #include "te_effect_dispatch.h"
    #include "ilagcompensationmanager.h"
#endif

#ifdef CLIENT_DLL
    #define CWeaponGauss C_WeaponGauss
#endif

//----------------------------------------
// Definitions
//----------------------------------------
#define GAUSS_BEAM_SPRITE              "sprites/laserbeam.vmt"
#define GAUSS_CHARGE_TIME              0.3f
#define MAX_GAUSS_CHARGE               16.0f
#define MAX_GAUSS_CHARGE_TIME          3.0f
#define DANGER_GAUSS_CHARGE_TIME       10.0f

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------
// Client Effect Registration
//----------------------------------------
#ifdef CLIENT_DLL
    CLIENTEFFECT_REGISTER_BEGIN(PrecacheEffectGauss)
        CLIENTEFFECT_MATERIAL("sprites/laserbeam")
    CLIENTEFFECT_REGISTER_END()
#endif

//----------------------------------------
// Weapon Class Declaration
//----------------------------------------
class CWeaponGauss : public CBaseHL2MPCombatWeapon
{
public:
    DECLARE_CLASS(CWeaponGauss, CBaseHL2MPCombatWeapon);
    DECLARE_NETWORKCLASS();
    DECLARE_PREDICTABLE();

    CWeaponGauss(void);

    // Core weapon functions
    void    Spawn(void);
    void    Precache(void);
    bool    Holster(CBaseCombatWeapon *pSwitchingTo = NULL);
    void    ItemPostFrame(void);
    float   GetFireRate(void) { return 0.2f; }
    virtual void Drop(const Vector &vecVelocity);
    virtual void Equip(CBaseCombatCharacter *pOwner);

    // Attack functions
    void    PrimaryAttack(void);
    void    SecondaryAttack(void);
    void    Fire(void);
    void    ChargedFire(void);
    void    ChargedFireFirstBeam(void);
    void    IncreaseCharge(void);

    // Effect functions
    void    AddViewKick(void);
    void    DrawBeam(const Vector &startPos, const Vector &endPos, float width, bool useMuzzle = true);
    void    DoWallBreak(Vector startPos, Vector endPos, Vector aimDir, trace_t *ptr, CBasePlayer *pOwner, bool m_bBreakAll);
    bool    DidPunchThrough(trace_t *tr);
    void    DoImpactEffect(trace_t &tr, int nDamageType);

    // Sound functions
    void    StopChargeSound(void);
    void    PlayAfterShock(void);

    DECLARE_ACTTABLE();

protected:
    CSoundPatch    *m_sndCharge;

    // Network variables
    CNetworkVar(bool, m_bCharging);
    CNetworkVar(float, m_flNextChargeTime);
    CNetworkVar(float, m_flChargeStartTime);
    CNetworkVar(float, m_flCoilMaxVelocity);
    CNetworkVar(float, m_flCoilVelocity);
    CNetworkVar(float, m_flCoilAngle);
    CNetworkVar(float, m_flPlayAftershock);
    CNetworkVar(float, m_flNextAftershock);  // For periodic aftershock sounds during charging

    CNetworkHandle(CBeam, m_pBeam);

	int m_iSavedAmmo;  // Ammo preservation
};

//----------------------------------------
// ConVars
//----------------------------------------
ConVar sk_plr_dmg_gauss("sk_plr_dmg_gauss", "20", FCVAR_REPLICATED);
ConVar sk_plr_max_dmg_gauss("sk_plr_max_dmg_gauss", "200", FCVAR_REPLICATED);
ConVar rb_selfgauss("rb_selfgauss", "1", FCVAR_REPLICATED, "Enable self damage from gauss gun reflections and explosions (1=enabled, 0=disabled)");

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Network Implementation
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

IMPLEMENT_NETWORKCLASS_ALIASED(WeaponGauss, DT_WeaponGauss)

BEGIN_NETWORK_TABLE(CWeaponGauss, DT_WeaponGauss)
    #ifdef CLIENT_DLL
        RecvPropBool(RECVINFO(m_bCharging)),
        RecvPropFloat(RECVINFO(m_flNextChargeTime)),
        RecvPropFloat(RECVINFO(m_flChargeStartTime)),
        RecvPropFloat(RECVINFO(m_flCoilMaxVelocity)),
        RecvPropFloat(RECVINFO(m_flCoilVelocity)),
        RecvPropFloat(RECVINFO(m_flCoilAngle)),
        RecvPropFloat(RECVINFO(m_flPlayAftershock)),
        RecvPropEHandle(RECVINFO(m_pBeam))
    #else
        SendPropBool(SENDINFO(m_bCharging)),
        SendPropFloat(SENDINFO(m_flNextChargeTime)),
        SendPropFloat(SENDINFO(m_flChargeStartTime)),
        SendPropFloat(SENDINFO(m_flCoilMaxVelocity)),
        SendPropFloat(SENDINFO(m_flCoilVelocity)),
        SendPropFloat(SENDINFO(m_flCoilAngle)),
        SendPropFloat(SENDINFO(m_flPlayAftershock)),
        SendPropEHandle(SENDINFO(m_pBeam))
    #endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
    BEGIN_PREDICTION_DATA(CWeaponGauss)
        DEFINE_PRED_FIELD(m_pBeam, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE)
    END_PREDICTION_DATA()
#endif

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Weapon Registration and Activity Table
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

LINK_ENTITY_TO_CLASS(weapon_gauss, CWeaponGauss);
PRECACHE_WEAPON_REGISTER(weapon_gauss);

acttable_t CWeaponGauss::m_acttable[] = 
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

IMPLEMENT_ACTTABLE(CWeaponGauss);

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Constructor & Core Functions
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponGauss::CWeaponGauss(void)
{
    m_flNextChargeTime   = 0;
    m_flChargeStartTime  = 0;
    m_sndCharge         = NULL;
    m_bCharging         = false;
    m_flPlayAftershock  = 0;
    m_flNextAftershock  = 0;
    m_iSavedAmmo = -1;
}

//-----------------------------------------------------------------------------
// Purpose: Precache weapon assets
//-----------------------------------------------------------------------------
void CWeaponGauss::Precache(void)
{
    PrecacheModel(GAUSS_BEAM_SPRITE);

    #ifndef CLIENT_DLL
        PrecacheScriptSound("Weapon_Gauss.ChargeLoop");
        PrecacheScriptSound("Weapon_Gauss.Electro1");
        PrecacheScriptSound("Weapon_Gauss.Electro2");
        PrecacheScriptSound("Weapon_Gauss.Electro3");
        PrecacheScriptSound("Weapon_Gauss.Single");
    #endif

    BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Spawn the weapon
//-----------------------------------------------------------------------------
void CWeaponGauss::Spawn(void)
{
    BaseClass::Spawn();
    SetActivity(ACT_HL2MP_IDLE);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Attack Functions
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//-----------------------------------------------------------------------------
// Purpose: Primary fire attack
//-----------------------------------------------------------------------------
void CWeaponGauss::PrimaryAttack(void)
{
    CBasePlayer *pOwner = ToBasePlayer(GetOwner());
    if (!pOwner)
        return;

    // Check for minimum ammo requirement (2 ammo)
    if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) < 2)
    {
        WeaponSound(EMPTY);
        m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;
        return;
    }

    // Play fire sound - NO aftershock for primary fire
    WeaponSound(SINGLE);
    pOwner->DoMuzzleFlash();

    SendWeaponAnim(ACT_VM_PRIMARYATTACK);

    m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
    pOwner->RemoveAmmo(2, m_iPrimaryAmmoType); // Use 2 ammo for primary fire

    Fire();

    m_flCoilMaxVelocity = 0.0f;
    m_flCoilVelocity = 1000.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Secondary fire attack - Charge weapon
//-----------------------------------------------------------------------------
void CWeaponGauss::SecondaryAttack(void)
{
    CBasePlayer *pOwner = ToBasePlayer(GetOwner());
    if (!pOwner || !pOwner->IsAlive())
    {
        if (m_bCharging)
        {
            #ifndef CLIENT_DLL
                DevMsg("[GAUSS SECONDARY] Player died/invalid while charging - stopping charge\n");
            #endif
            StopChargeSound();
            m_bCharging = false;
        }
        return;
    }

    if (!m_bCharging)
    {
        if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
        {
            #ifndef CLIENT_DLL
                DevMsg("[GAUSS SECONDARY] Cannot start charging - no ammo available\n");
            #endif
            return;
        }

        #ifndef CLIENT_DLL
            DevMsg("[GAUSS SECONDARY] Starting charge with %d ammo\n", pOwner->GetAmmoCount(m_iPrimaryAmmoType));
        #endif

        // Start looping animation
        SendWeaponAnim(ACT_VM_PULLBACK);
        
        // Start looping sound
        if (!m_sndCharge)
        {
            #ifndef CLIENT_DLL
                CPASAttenuationFilter filter(this);
                m_sndCharge = (CSoundEnvelopeController::GetController()).SoundCreate(
                    filter, entindex(), CHAN_STATIC, "Weapon_Gauss.ChargeLoop", ATTN_NORM);

                if (m_sndCharge)
                {
                    (CSoundEnvelopeController::GetController()).Play(m_sndCharge, 1.0f, 50);
                    (CSoundEnvelopeController::GetController()).SoundChangePitch(m_sndCharge, 250, 3.0f);
                }
            #endif
        }

        m_flChargeStartTime = gpGlobals->curtime;
        m_bCharging = true;
        m_flNextChargeTime = gpGlobals->curtime + GAUSS_CHARGE_TIME;
        m_flNextAftershock = 0; // Initialize - will be set when fully charged

        // Decrement initial power
        pOwner->RemoveAmmo(1, m_iPrimaryAmmoType);
        
        #ifndef CLIENT_DLL
            DevMsg("[GAUSS SECONDARY] Charge started - ammo now %d\n", pOwner->GetAmmoCount(m_iPrimaryAmmoType));
        #endif
    }

    // Don't call IncreaseCharge here - it will be called in ItemPostFrame
}

//-----------------------------------------------------------------------------
// Purpose: Handle weapon charge increase
//-----------------------------------------------------------------------------
void CWeaponGauss::IncreaseCharge(void)
{
    CBasePlayer *pOwner = ToBasePlayer(GetOwner());
    if (!pOwner || !pOwner->IsAlive())
    {
        if (m_bCharging)
        {
            StopChargeSound();
            m_bCharging = false;
        }
        return;
    }

    // Check for overcharge damage FIRST - before any other logic
    if ((gpGlobals->curtime - m_flChargeStartTime) > DANGER_GAUSS_CHARGE_TIME)
    {
        #ifndef CLIENT_DLL
            // Play overcharge sound
            EmitSound("Weapon_Gauss.Electro2");
            
            // Deal damage to player only if self damage is enabled
            if (rb_selfgauss.GetBool())
            {
                // Create damage info with proper attacker (use the player as both inflictor and attacker)
                CTakeDamageInfo dmgInfo(pOwner, pOwner, 50, DMG_SHOCK);
                dmgInfo.SetDamagePosition(pOwner->GetAbsOrigin());
                pOwner->TakeDamage(dmgInfo);
            }
            
            // Screen flash effect (always show regardless of damage setting)
            color32 gaussDamage = {255, 128, 0, 128};
            UTIL_ScreenFade(pOwner, gaussDamage, 0.2f, 0.2f, FFADE_IN);
        #endif

        // Stop charging and reset
        StopChargeSound();
        m_bCharging = false;
        SendWeaponAnim(ACT_VM_IDLE);
        m_flNextSecondaryAttack = gpGlobals->curtime + 1.0f;
        m_flNextPrimaryAttack = gpGlobals->curtime + 1.0f;
        return;
    }

    // Play periodic aftershock sounds after full charge + 5 seconds
    float chargeTime = gpGlobals->curtime - m_flChargeStartTime;
    float aftershockStartTime = MAX_GAUSS_CHARGE_TIME + 5.0f; // Full charge (3s) + 5 seconds = 8s total
    
    if (chargeTime >= aftershockStartTime)
    {
        // Initialize aftershock timer if not set
        if (m_flNextAftershock == 0)
        {
            m_flNextAftershock = gpGlobals->curtime + 1.0f; // Start aftershocks in 1 second
        }
        
        // Play aftershock sounds at fixed 1.5 second intervals
        if (gpGlobals->curtime >= m_flNextAftershock)
        {
            #ifndef CLIENT_DLL
                // Play random aftershock sound
                switch (random->RandomInt(0, 2))
                {
                    case 0: EmitSound("Weapon_Gauss.Electro1"); break;
                    case 1: EmitSound("Weapon_Gauss.Electro2"); break;
                    case 2: EmitSound("Weapon_Gauss.Electro3"); break;
                }
                
                m_flNextAftershock = gpGlobals->curtime + 1.5f; // Fixed 1.5 second interval
            #endif
        }
    }

    // Don't increase charge if we're at max
    if (m_flNextChargeTime == 1000)
        return;

    // If it's time to consume ammo
    if (m_bCharging && gpGlobals->curtime >= m_flNextChargeTime)
    {
        // If we're out of ammo, fire now
        if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
        {
            #ifndef CLIENT_DLL
                float currentChargeTime = gpGlobals->curtime - m_flChargeStartTime;
                DevMsg("[GAUSS SECONDARY] Out of ammo - firing with charge time %.2fs\n", currentChargeTime);
            #endif
            ChargedFire();
            return;
        }

        // Otherwise consume ammo and continue charging
        pOwner->RemoveAmmo(1, m_iPrimaryAmmoType);
        m_flNextChargeTime = gpGlobals->curtime + GAUSS_CHARGE_TIME;
        
        #ifndef CLIENT_DLL
            float currentChargeTime = gpGlobals->curtime - m_flChargeStartTime;
            DevMsg("[GAUSS SECONDARY] Consumed ammo - charge time: %.2fs, ammo remaining: %d\n", 
                   currentChargeTime, pOwner->GetAmmoCount(m_iPrimaryAmmoType));
        #endif

        // Check if we've reached max charge
        if ((gpGlobals->curtime - m_flChargeStartTime) >= MAX_GAUSS_CHARGE_TIME)
        {
            m_flNextChargeTime = 1000;  // Stop consuming ammo
            #ifndef CLIENT_DLL
                DevMsg("[GAUSS SECONDARY] Reached maximum charge time - no longer consuming ammo\n");
            #endif
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose: Fire the weapon normally
//-----------------------------------------------------------------------------
void CWeaponGauss::Fire(void)
{
    CBasePlayer *pOwner = ToBasePlayer(GetOwner());
    if (!pOwner)
        return;

    m_bCharging = false;

#ifndef CLIENT_DLL
    // Move other players back to history positions based on local player's lag
    lagcompensation->StartLagCompensation( pOwner, pOwner->GetCurrentCommand() );
#endif

    Vector startPos = pOwner->Weapon_ShootPosition();
    Vector aimDir = pOwner->GetAutoaimVector(AUTOAIM_5DEGREES);

    // Calculate spread
    Vector vecUp, vecRight;
    VectorVectors(aimDir, vecRight, vecUp);

    float x, y, z;
    do {
        x = random->RandomFloat(-0.5f, 0.5f) + random->RandomFloat(-0.5f, 0.5f);
        y = random->RandomFloat(-0.5f, 0.5f) + random->RandomFloat(-0.5f, 0.5f);
        z = x*x + y*y;
    } while (z > 1);

    Vector endPos = startPos + (aimDir * MAX_TRACE_LENGTH);
    float flDamage = sk_plr_dmg_gauss.GetFloat();
    int nMaxHits = 10;
    bool firstBeam = true;
    CBaseEntity* pentIgnore = pOwner;
    Vector lastHitPos = startPos;  // Track last hit position for beam connections

    #ifndef CLIENT_DLL
        ClearMultiDamage();
    #endif

    while (flDamage > 10 && nMaxHits > 0)
    {
        nMaxHits--;

        trace_t tr;
        UTIL_TraceLine(startPos, endPos, MASK_SHOT, pentIgnore, COLLISION_GROUP_NONE, &tr);

        if (tr.allsolid)
            break;

        // Draw beam from last hit to current hit
        DrawBeam(lastHitPos, tr.endpos, firstBeam ? 1.6f : 0.4f, firstBeam);
        lastHitPos = tr.endpos;  // Update last hit position

        // Handle hit entity
        if (tr.m_pEnt)
        {
            #ifndef CLIENT_DLL
                CTakeDamageInfo dmgInfo(this, pOwner, flDamage, DMG_SHOCK | DMG_BULLET);
                Vector force = aimDir * flDamage * 500.0f;
                dmgInfo.SetDamageForce(force);
                dmgInfo.SetDamagePosition(tr.endpos);
                tr.m_pEnt->DispatchTraceAttack(dmgInfo, aimDir, &tr);
                
                // Debug output for damage dealt - only show for non-worldspawn entities
                const char* className = tr.m_pEnt->GetClassname();
                if (className && Q_stricmp(className, "worldspawn") != 0)
                {
                    DevMsg("Gauss hit %s for %.1f damage\n", className, flDamage);
                }
            #endif
        }

        // Add impact effects
        CPVSFilter filter(tr.endpos);
        te->GaussExplosion(filter, 0.0f, tr.endpos, tr.plane.normal, 0);
        UTIL_ImpactTrace(&tr, GetAmmoDef()->DamageType(m_iPrimaryAmmoType), "ImpactGauss");

        // Stop if we hit sky
        if (tr.surface.flags & SURF_SKY)
            break;

        // Handle reflections
        if (tr.DidHitWorld())
        {
            float hitAngle = -DotProduct(tr.plane.normal, aimDir);

            if (hitAngle < 0.5f)
            {
                // Calculate reflection
                Vector vReflection = 2.0f * tr.plane.normal * hitAngle + aimDir;
                
                // Update for next trace with adjusted start position
                Vector offsetPos = tr.endpos + (tr.plane.normal * 0.2f);  // Small offset from wall
                startPos = offsetPos + (vReflection * 1.0f);  // Small offset in reflection direction
                endPos = startPos + (vReflection * MAX_TRACE_LENGTH);
                aimDir = vReflection;

                // Reduce damage based on reflection angle
                if (hitAngle == 0) hitAngle = 0.1f;
                flDamage = flDamage * (1.0f - hitAngle);

                #ifndef CLIENT_DLL
                    // Create radius damage info
                    CTakeDamageInfo radiusDmgInfo(this, pOwner, flDamage * hitAngle, DMG_SHOCK);
                    
                    // If self damage is disabled, exclude the owner from radius damage
                    CBaseEntity *pIgnoreEntity = rb_selfgauss.GetBool() ? NULL : pOwner;
                    
                    RadiusDamage(radiusDmgInfo, tr.endpos, 64.0f, CLASS_NONE, pIgnoreEntity);
                    
                    // Debug output for reflection explosion damage
                    DevMsg("Gauss reflection explosion: %.1f damage, radius 64.0\n", 
                           flDamage * hitAngle);
                #endif

                firstBeam = false;
                pentIgnore = NULL;
                continue;
            }
        }
        break;
    }

    #ifndef CLIENT_DLL
        ApplyMultiDamage();
        
        // Debug: Show health/armor status for all players that took damage in primary attack
        for (int i = 1; i <= gpGlobals->maxClients; i++)
        {
            CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
            if (pPlayer && pPlayer->IsAlive() && pPlayer != pOwner)
            {
                // Check if this player was likely hit (health < 100 or has armor damage)
                if (pPlayer->GetHealth() < 100 || pPlayer->ArmorValue() < 100)
                {
                    DevMsg("Gauss primary: Player %d health now %d (armor: %d)\n", 
                           i, pPlayer->GetHealth(), pPlayer->ArmorValue());
                }
            }
        }
        
        // Finish lag compensation
        lagcompensation->FinishLagCompensation( pOwner );
    #endif

    m_flNextSecondaryAttack = gpGlobals->curtime + 1.0f;
    AddViewKick();
}

//-----------------------------------------------------------------------------
// Purpose: Fire charged shot with reflections
//-----------------------------------------------------------------------------
void CWeaponGauss::ChargedFire(void)
{
    CBasePlayer *pOwner = ToBasePlayer(GetOwner());
    if (!pOwner)
        return;

    // Stop charge sound before anything else
    StopChargeSound();
    m_bCharging = false;
    m_flNextAftershock = 0;  // Reset aftershock timer

#ifndef CLIENT_DLL
    // Move other players back to history positions based on local player's lag
    lagcompensation->StartLagCompensation( pOwner, pOwner->GetCurrentCommand() );
#endif

    // Play fire sound and queue aftershock for charged shot
    WeaponSound(SINGLE);
    m_flPlayAftershock = gpGlobals->curtime + random->RandomFloat(0.3f, 0.8f);
    pOwner->DoMuzzleFlash();

    SendWeaponAnim(ACT_VM_SECONDARYATTACK);

    // Reset charge state
    m_flNextSecondaryAttack = gpGlobals->curtime + 1.0f;
	m_flNextPrimaryAttack = gpGlobals->curtime + 1.0f; 

    // Calculate shot trajectory
    Vector startPos = pOwner->Weapon_ShootPosition();
    Vector aimDir = pOwner->GetAutoaimVector(AUTOAIM_5DEGREES);

    // Validate positions before proceeding
    if (!startPos.IsValid() || !aimDir.IsValid())
        return;

    // Calculate damage based on charge time - ALWAYS fire, even with tiny charge
    float chargeTime = gpGlobals->curtime - m_flChargeStartTime;
    float flDamage = 50.0f; // Minimum damage for any charge
    
    if (chargeTime >= MAX_GAUSS_CHARGE_TIME)
    {
        flDamage = 200;
    }
    else if (chargeTime > 0)
    {
        // Scale from minimum 50 to maximum 200 based on charge time
        flDamage = 50 + (150 * (chargeTime / MAX_GAUSS_CHARGE_TIME));
    }
    
    #ifndef CLIENT_DLL
        // Debug: Show charge time and calculated damage
        DevMsg("[GAUSS SECONDARY] Charge time: %.2fs (max: %.2fs), Initial damage: %.1f\n", 
               chargeTime, MAX_GAUSS_CHARGE_TIME, flDamage);
        
        // Debug: Show knockback calculation
        Vector originalVelocity = pOwner->GetAbsVelocity();
        Vector knockbackForce = aimDir * flDamage * 5;
        DevMsg("[GAUSS SECONDARY] Player velocity before: (%.1f, %.1f, %.1f), knockback force: (%.1f, %.1f, %.1f)\n",
               originalVelocity.x, originalVelocity.y, originalVelocity.z,
               knockbackForce.x, knockbackForce.y, knockbackForce.z);
    #endif

    Vector endPos = startPos + (aimDir * MAX_TRACE_LENGTH);
    int nMaxHits = 10;
    bool firstBeam = true;
    CBaseEntity* pentIgnore = pOwner;
    bool penetrated = false;
    Vector lastHitPos = startPos; // Track the last position for beam segments

    #ifndef CLIENT_DLL
        // Apply knockback like HL1
        Vector vecVelocity = pOwner->GetAbsVelocity();
        vecVelocity = vecVelocity - aimDir * flDamage * 5;
        pOwner->SetAbsVelocity(vecVelocity);
        
        // Determine if we should ignore the owner for explosion damage
        CBaseEntity *pIgnoreEntity = rb_selfgauss.GetBool() ? NULL : pOwner;
        
        ClearMultiDamage();
    #endif

    // Main firing loop with reflections like primary fire
    while (flDamage > 10 && nMaxHits > 0)
    {
        nMaxHits--;

        trace_t tr;
        UTIL_TraceLine(startPos, endPos, MASK_SHOT, pentIgnore, COLLISION_GROUP_NONE, &tr);

        if (tr.allsolid)
            break;

        // Draw beam segment from last hit position to current hit position
        float beamWidth = firstBeam ? 9.6f : 4.8f;
        DrawBeam(lastHitPos, tr.endpos, beamWidth, firstBeam);
        lastHitPos = tr.endpos; // Update last hit position

        // Handle hit entity
        if (tr.m_pEnt)
        {
            #ifndef CLIENT_DLL
                CTakeDamageInfo dmgInfo(this, pOwner, flDamage, DMG_SHOCK | DMG_BULLET);
                Vector force = aimDir * flDamage * 500.0f;
                dmgInfo.SetDamageForce(force);
                dmgInfo.SetDamagePosition(tr.endpos);
                tr.m_pEnt->DispatchTraceAttack(dmgInfo, aimDir, &tr);
                
                // Debug output for damage dealt - only show for non-worldspawn entities
                const char* className = tr.m_pEnt->GetClassname();
                if (className && Q_stricmp(className, "worldspawn") != 0)
                {
                    DevMsg("[GAUSS SECONDARY] Hit %s (entindex: %d) for %.1f damage at (%.1f, %.1f, %.1f)\n", 
                           className, tr.m_pEnt->entindex(), flDamage, 
                           tr.endpos.x, tr.endpos.y, tr.endpos.z);
                    
                    // Show health status for players
                    if (tr.m_pEnt->IsPlayer())
                    {
                        CBasePlayer* pHitPlayer = ToBasePlayer(tr.m_pEnt);
                        if (pHitPlayer)
                        {
                            DevMsg("[GAUSS SECONDARY] Player %d health before hit: %d (armor: %d)\n",
                                   pHitPlayer->entindex(), pHitPlayer->GetHealth(), pHitPlayer->ArmorValue());
                        }
                    }
                }
            #endif
        }

        // Add impact effects
        CPVSFilter filter(tr.endpos);
        te->GaussExplosion(filter, 0.0f, tr.endpos, tr.plane.normal, 0);
        UTIL_ImpactTrace(&tr, GetAmmoDef()->DamageType(m_iPrimaryAmmoType), "ImpactGauss");
        DoImpactEffect(tr, GetAmmoDef()->DamageType(m_iPrimaryAmmoType));

        #ifndef CLIENT_DLL
            // Add radius damage for area effect - exclude owner if self damage disabled
            RadiusDamage(CTakeDamageInfo(this, pOwner, flDamage * 0.5f, DMG_SHOCK | DMG_BULLET), 
                        tr.endpos, 64.0f, CLASS_NONE, pIgnoreEntity);
                        
            // Debug output for area effect damage
            DevMsg("[GAUSS SECONDARY] Area effect at (%.1f, %.1f, %.1f): %.1f damage, radius 64.0\n", 
                   tr.endpos.x, tr.endpos.y, tr.endpos.z, flDamage * 0.5f);
        #endif

        // Stop if we hit sky
        if (tr.surface.flags & SURF_SKY)
            break;

        // Handle world impacts - try penetration first, then reflection
        if (tr.DidHitWorld())
        {
            float hitAngle = -DotProduct(tr.plane.normal, aimDir);

            // Try penetration for charged shots (only if enough damage)
            if (flDamage > 100.0f && !penetrated)
            {
                Vector testPos = tr.endpos + (aimDir * 128.0f);
                trace_t penetrationTrace;
                UTIL_TraceLine(testPos, tr.endpos, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &penetrationTrace);

                if (!penetrationTrace.allsolid)
                {
                    // Penetration successful
                    penetrated = true;
                    
                    // Draw penetration beam from wall exit to penetration hit
                    Vector penetrationStart = tr.endpos + (aimDir * 64.0f);
                    DrawBeam(penetrationStart, penetrationTrace.endpos, beamWidth * 0.8f, false);
                    
                    // Update positions for continued tracing
                    startPos = penetrationStart;
                    lastHitPos = penetrationTrace.endpos; // Update last hit for next segment
                    endPos = startPos + (aimDir * MAX_TRACE_LENGTH);
                    flDamage = flDamage * 0.8f; // Reduce damage for penetration
                    
                    #ifndef CLIENT_DLL
                        // Handle penetration damage
                        if (penetrationTrace.m_pEnt)
                        {
                            CTakeDamageInfo dmgInfo(this, pOwner, flDamage, DMG_SHOCK | DMG_BULLET);
                            Vector force = aimDir * flDamage * 500.0f;
                            dmgInfo.SetDamageForce(force);
                            dmgInfo.SetDamagePosition(penetrationTrace.endpos);
                            penetrationTrace.m_pEnt->DispatchTraceAttack(dmgInfo, aimDir, &penetrationTrace);
                            
                            // Debug output for penetration damage
                            DevMsg("[GAUSS SECONDARY] Penetration hit %s (entindex: %d) for %.1f damage at (%.1f, %.1f, %.1f)\n", 
                                   penetrationTrace.m_pEnt->GetClassname(), penetrationTrace.m_pEnt->entindex(), 
                                   flDamage, penetrationTrace.endpos.x, penetrationTrace.endpos.y, penetrationTrace.endpos.z);
                            
                            // Show health status for players hit by penetration
                            if (penetrationTrace.m_pEnt->IsPlayer())
                            {
                                CBasePlayer* pHitPlayer = ToBasePlayer(penetrationTrace.m_pEnt);
                                if (pHitPlayer)
                                {
                                    DevMsg("[GAUSS SECONDARY] Penetration - Player %d health before hit: %d (armor: %d)\n",
                                           pHitPlayer->entindex(), pHitPlayer->GetHealth(), pHitPlayer->ArmorValue());
                                }
                            }
                        }
                        
                        // Penetration explosion - exclude owner if self damage disabled
                        RadiusDamage(CTakeDamageInfo(this, pOwner, flDamage, DMG_SHOCK | DMG_BLAST), 
                                    penetrationTrace.endpos, flDamage * 1.75f, CLASS_NONE, pIgnoreEntity);
                        
                        // Debug output for penetration explosion damage
                        DevMsg("[GAUSS SECONDARY] Penetration explosion at (%.1f, %.1f, %.1f): %.1f damage, radius %.1f\n", 
                               penetrationTrace.endpos.x, penetrationTrace.endpos.y, penetrationTrace.endpos.z,
                               flDamage, flDamage * 1.75f);
                    #endif
                    
                    firstBeam = false;
                    pentIgnore = NULL;
                    continue; // Continue with penetration
                }
            }

            // Try reflection if penetration failed or not applicable
            if (hitAngle < 0.5f)
            {
                // Calculate reflection
                Vector vReflection = 2.0f * tr.plane.normal * hitAngle + aimDir;
                
                // Update for next trace with adjusted start position
                Vector offsetPos = tr.endpos + (tr.plane.normal * 0.2f);  // Small offset from wall
                startPos = offsetPos + (vReflection * 1.0f);  // Small offset in reflection direction
                endPos = startPos + (vReflection * MAX_TRACE_LENGTH);
                aimDir = vReflection;

                // Reduce damage based on reflection angle
                if (hitAngle == 0) hitAngle = 0.1f;
                flDamage = flDamage * (1.0f - hitAngle);

                #ifndef CLIENT_DLL
                    // Reflection explosion - exclude owner if self damage disabled
                    RadiusDamage(CTakeDamageInfo(this, pOwner, flDamage * hitAngle, DMG_SHOCK), 
                                tr.endpos, 64.0f, CLASS_NONE, pIgnoreEntity);
                                
                    // Debug output for reflection explosion damage
                    DevMsg("[GAUSS SECONDARY] Reflection explosion at (%.1f, %.1f, %.1f): %.1f damage, radius 64.0, hit angle: %.2f\n", 
                           tr.endpos.x, tr.endpos.y, tr.endpos.z, flDamage * hitAngle, hitAngle);
                #endif

                firstBeam = false;
                pentIgnore = NULL;
                continue; // Continue with reflection
            }
        }
        
        // If we get here, we hit something that stops the beam
        break;
    }

    #ifndef CLIENT_DLL
        ApplyMultiDamage();
        
        // Debug: Show final health/armor status after all damage for secondary attack
        DevMsg("[GAUSS SECONDARY] === Final damage summary ===\n");
        
        // Show all players' status after damage
        for (int i = 1; i <= gpGlobals->maxClients; i++)
        {
            CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
            if (pPlayer && pPlayer->IsAlive())
            {
                if (pPlayer == pOwner)
                {
                    DevMsg("[GAUSS SECONDARY] Shooter (Player %d) final health: %d (armor: %d)\n", 
                           i, pPlayer->GetHealth(), pPlayer->ArmorValue());
                }
                else if (pPlayer->GetHealth() < 100 || pPlayer->ArmorValue() < 100)
                {
                    DevMsg("[GAUSS SECONDARY] Hit target (Player %d) final health: %d (armor: %d)\n", 
                           i, pPlayer->GetHealth(), pPlayer->ArmorValue());
                }
            }
        }
        
        // Show final velocity of shooter after knockback
        Vector finalVelocity = pOwner->GetAbsVelocity();
        DevMsg("[GAUSS SECONDARY] Player velocity after knockback: (%.1f, %.1f, %.1f)\n",
               finalVelocity.x, finalVelocity.y, finalVelocity.z);
        DevMsg("[GAUSS SECONDARY] === End damage summary ===\n");
        
        // Finish lag compensation
        lagcompensation->FinishLagCompensation( pOwner );
    #endif

    // Add view punch effect based on damage
    QAngle viewPunch;
    float punchMultiplier = flDamage / 200.0f; // Scale punch based on charge
    viewPunch.x = random->RandomFloat(-2.0f * punchMultiplier, -4.0f * punchMultiplier);
    viewPunch.y = random->RandomFloat(-0.25f, 0.25f);
    viewPunch.z = 0;
    pOwner->ViewPunch(viewPunch);
}

//-----------------------------------------------------------------------------
// Purpose: Initial beam for charged attack (now unused since integrated into ChargedFire)
//-----------------------------------------------------------------------------
void CWeaponGauss::ChargedFireFirstBeam(void)
{
    // This function is now handled within ChargedFire() for better integration
    // Keeping it for compatibility but it doesn't need to do anything
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Utility Functions
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//-----------------------------------------------------------------------------
// Purpose: Check if beam punched through surface
// Input  : Trace result pointer
// Output : Returns true if punch-through occurred
//-----------------------------------------------------------------------------
bool CWeaponGauss::DidPunchThrough(trace_t *tr)
{
    // Valid punch-through if:
    // - Hit world
    // - Not sky texture
    // - Either not start solid, or was start solid but not all solid
    if (tr->DidHitWorld() && 
        tr->surface.flags != SURF_SKY && 
        ((tr->startsolid == false) || (tr->startsolid == true && tr->allsolid == false)))
    {
        return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
// Purpose: Handle wall break effects and penetration
// Input  : Start and end positions, aim direction, trace info, owner, and break flag
//-----------------------------------------------------------------------------
void CWeaponGauss::DoWallBreak(Vector startPos, Vector endPos, Vector aimDir, 
                              trace_t *ptr, CBasePlayer *pOwner, bool m_bBreakAll)
{
    trace_t *temp = ptr;

    if (m_bBreakAll)
    {
        Vector tempPos = endPos;
        Vector beamStart = startPos;
        int x = 0;

        // Continue breaking walls while we can punch through
        while (DidPunchThrough(ptr))
        {
            temp = ptr;

            // Alternate trace directions
            if (x == 0)
            {
                UTIL_TraceLine(startPos, tempPos, MASK_SHOT, 
                             pOwner, COLLISION_GROUP_NONE, ptr);
                x = 1;
            }
            else
            {
                UTIL_TraceLine(endPos, startPos, MASK_SHOT, 
                             pOwner, COLLISION_GROUP_NONE, ptr);
                x = 0;
            }

            // Add impact effects if we hit world
            if (ptr->DidHitWorld() && ptr->surface.flags != SURF_SKY)
            {
                UTIL_ImpactTrace(ptr, GetAmmoDef()->DamageType(m_iPrimaryAmmoType), "ImpactGauss");
                DoImpactEffect(*ptr, GetAmmoDef()->DamageType(m_iPrimaryAmmoType));
            }

            // Update positions for next trace
            startPos = ptr->endpos;
            tempPos = ptr->endpos + (aimDir * MAX_TRACE_LENGTH + aimDir * 128.0f);
        }
    }
    else
    {
        // Single trace from start to end
        UTIL_TraceLine(startPos, endPos, MASK_SHOT, pOwner, 
                      COLLISION_GROUP_NONE, ptr);
    }

    // Restore original trace if no punch-through
    if (!DidPunchThrough(ptr))
    {
        ptr = temp;
    }
}

//-----------------------------------------------------------------------------
// Purpose: Draw beam effect between two points
// Input  : Start and end positions, beam width, muzzle flag
//-----------------------------------------------------------------------------
void CWeaponGauss::DrawBeam(const Vector &startPos, const Vector &endPos, float width, bool useMuzzle)
{
    #ifndef CLIENT_DLL
        // Validate positions to prevent collision assertion errors
        if (startPos.IsValid() == false || endPos.IsValid() == false)
            return;
            
        // Check for extreme coordinates
        if (startPos.LengthSqr() > (MAX_TRACE_LENGTH * MAX_TRACE_LENGTH) ||
            endPos.LengthSqr() > (MAX_TRACE_LENGTH * MAX_TRACE_LENGTH))
            return;

        // Make sure we have a reasonable distance
        float distance = startPos.DistTo(endPos);
        if (distance < 1.0f || distance > MAX_TRACE_LENGTH)
            return;

        // Create main beam
        CBeam *pMainBeam = CBeam::BeamCreate(GAUSS_BEAM_SPRITE, width);
        if (!pMainBeam)
            return;

        // Setup beam properties based on muzzle flag
        if (useMuzzle && GetOwner())
        {
            pMainBeam->PointEntInit(endPos, this);
            int muzzleAttachment = LookupAttachment("Muzzle");
            if (muzzleAttachment > 0)
            {
                pMainBeam->SetEndAttachment(muzzleAttachment);
            }
            else
            {
                // Fallback if no muzzle attachment
                pMainBeam->SetStartPos(startPos);
                pMainBeam->SetEndPos(endPos);
            }
            pMainBeam->SetWidth(width / 4.0f);
            pMainBeam->SetEndWidth(width);
        }
        else
        {
            pMainBeam->SetStartPos(startPos);
            pMainBeam->SetEndPos(endPos);
            pMainBeam->SetWidth(width);
            pMainBeam->SetEndWidth(width / 4.0f);
        }

        // Set common beam properties
        pMainBeam->SetBrightness(255);
        pMainBeam->SetColor(255, 145 + random->RandomInt(-16, 16), 0);
        pMainBeam->LiveForTime(0.1f);
        pMainBeam->RelinkBeam();

        // Create electric bolt effects along beam
        for (int i = 0; i < 3; i++)
        {
            CBeam *pBoltBeam = CBeam::BeamCreate(GAUSS_BEAM_SPRITE, (width/2.0f) + i);
            if (!pBoltBeam)
                continue;

            // Use muzzle attachment for bolt effects if specified
            if (useMuzzle && GetOwner())
            {
                pBoltBeam->PointEntInit(endPos, this);
                int muzzleAttachment = LookupAttachment("Muzzle");
                if (muzzleAttachment > 0)
                {
                    pBoltBeam->SetEndAttachment(muzzleAttachment);
                }
                else
                {
                    // Fallback if no muzzle attachment
                    pBoltBeam->SetStartPos(startPos);
                    pBoltBeam->SetEndPos(endPos);
                }
            }
            else
            {
                pBoltBeam->SetStartPos(startPos);
                pBoltBeam->SetEndPos(endPos);
            }

            // Set bolt-specific properties
            pBoltBeam->SetBrightness(random->RandomInt(64, 255));
            pBoltBeam->SetColor(255, 255, 150 + random->RandomInt(0, 64));
            pBoltBeam->LiveForTime(0.1f);
            pBoltBeam->SetNoise(1.6f * i);
            pBoltBeam->SetEndWidth(0.1f);
            pBoltBeam->RelinkBeam();
        }
    #endif
}

//-----------------------------------------------------------------------------
// Purpose: Add view kick effect
//-----------------------------------------------------------------------------
void CWeaponGauss::AddViewKick(void)
{
    CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
    if (!pPlayer)
        return;

    QAngle viewPunch;
    viewPunch.x = random->RandomFloat(-2.0f, -1.0f);
    viewPunch.y = random->RandomFloat(-0.5f, 0.5f);
    viewPunch.z = 0;

    pPlayer->ViewPunch(viewPunch);
}

//-----------------------------------------------------------------------------
// Purpose: Play aftershock sound effect
//-----------------------------------------------------------------------------
void CWeaponGauss::PlayAfterShock(void)
{
    if (m_flPlayAftershock && m_flPlayAftershock < gpGlobals->curtime)
    {
        // Randomly select from three aftershock sounds
        switch (random->RandomInt(0, 3))
        {
            case 0: EmitSound("Weapon_Gauss.Electro1"); break;
            case 1: EmitSound("Weapon_Gauss.Electro2"); break;
            case 2: EmitSound("Weapon_Gauss.Electro3"); break;
			case 3: break;
        }
        m_flPlayAftershock = 0.0f;
    }
}

//-----------------------------------------------------------------------------
// Purpose: Stop charge sound
//-----------------------------------------------------------------------------
void CWeaponGauss::StopChargeSound(void)
{
    #ifndef CLIENT_DLL
        if (m_sndCharge != NULL)
        {
            (CSoundEnvelopeController::GetController()).SoundFadeOut(m_sndCharge, 0.1f);
            m_sndCharge = NULL;
        }
    #endif
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Frame and State Functions
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//-----------------------------------------------------------------------------
// Purpose: Handle weapon state changes and updates
//-----------------------------------------------------------------------------
void CWeaponGauss::ItemPostFrame(void)
{
    CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
    if (!pPlayer)
        return;

    // If player is dead and charging, just stop the charge without firing
    if (!pPlayer->IsAlive() && m_bCharging)
    {
        StopChargeSound();
        m_bCharging = false;
        return;
    }

    // Handle charging - call IncreaseCharge every frame while charging
    if (m_bCharging && pPlayer->IsAlive())
    {
        IncreaseCharge();
    }

    // Handle charge release only if player is alive
    if (pPlayer->IsAlive() && (pPlayer->m_afButtonReleased & IN_ATTACK2))
    {
        if (m_bCharging)
        {
            #ifndef CLIENT_DLL
                float chargeTime = gpGlobals->curtime - m_flChargeStartTime;
                DevMsg("[GAUSS SECONDARY] Button released - firing with charge time %.2fs\n", chargeTime);
            #endif
            ChargedFire();
        }
    }

    // Play aftershock sounds (only for charged shots)
    PlayAfterShock();

    BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
// Purpose: Handle weapon holstering
// Input  : Weapon being switched to
// Output : Returns true on success, false on failure
//-----------------------------------------------------------------------------
bool CWeaponGauss::Holster(CBaseCombatWeapon *pSwitchingTo)
{
    StopChargeSound();
    m_bCharging = false;
    m_flNextAftershock = 0; // Reset aftershock timer

    if (m_sndCharge != NULL)
    {
        (CSoundEnvelopeController::GetController()).SoundFadeOut(m_sndCharge, 0.1f);
    }
    m_sndCharge = NULL;

    return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: Save ammo count when weapon is dropped
//-----------------------------------------------------------------------------
void CWeaponGauss::Drop(const Vector &vecVelocity)
{
    CBasePlayer* pOwner = ToBasePlayer(GetOwner());
    if (pOwner)
    {
        // Save ammo count before dropping
        m_iSavedAmmo = pOwner->GetAmmoCount(m_iPrimaryAmmoType);

		StopChargeSound();
        // If we're charging, stop everything without firing
        if (m_bCharging)
        {
            // Stop the sound first
            if (m_sndCharge)
            {
                (CSoundEnvelopeController::GetController()).SoundDestroy(m_sndCharge);
                m_sndCharge = NULL;
            }

            // Reset charge state
            m_bCharging = false;
            m_flNextChargeTime = 0;
            m_flNextAftershock = 0; // Reset aftershock timer
        }
    }

    BaseClass::Drop(vecVelocity);
}

//-----------------------------------------------------------------------------
// Purpose: Restore saved ammo count when weapon is picked up
//-----------------------------------------------------------------------------
void CWeaponGauss::Equip(CBaseCombatCharacter *pOwner)
{
    BaseClass::Equip(pOwner);
    if (m_iSavedAmmo != -1)
    {
        CBasePlayer* pPlayer = ToBasePlayer(pOwner);
        if (pPlayer)
        {
            pPlayer->SetAmmoCount(m_iSavedAmmo, m_iPrimaryAmmoType);
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose: Charged fire impact glow
//-----------------------------------------------------------------------------
void CWeaponGauss::DoImpactEffect(trace_t &tr, int nDamageType)
{
    CEffectData data;
    data.m_vOrigin = tr.endpos + (tr.plane.normal * 1.0f);
    data.m_vNormal = tr.plane.normal;

    if (tr.fraction != 1.0 && !(tr.surface.flags & SURF_SKY))
    {
        DispatchEffect("GaussImpact", data);
    }

    BaseClass::DoImpactEffect(tr, nDamageType);
}