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
#include "beam_flags.h"
#include "in_buttons.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "soundenvelope.h"
#include "sprite.h"

#ifdef CLIENT_DLL
    #include "c_hl2mp_player.h"
    #include "ClientEffectPrecacheSystem.h"
    #include "fx_line.h"
    #include "view.h"
    #include "materialsystem/imaterial.h"
    #include "materialsystem/imaterialsystem.h"
    #include "renderparm.h"
    #include "model_types.h"
    #include "tier0/vprof.h"
    #include "engine/ivdebugoverlay.h"
    #include "c_te_effect_dispatch.h"
    #include "dlight.h"
    #include "iefx.h"
    #include "prediction.h"
#else
    #include "hl2mp_player.h"
    #include "te_effect_dispatch.h"
    #include "ai_condition.h"
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
    static const char* BEAM_SPRITE_NODEPTH = "sprites/xbeam_nodepth.vmt";
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
    
    // Sound names (kept for reference, but using WeaponSound system now)
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
        CLIENTEFFECT_MATERIAL("sprites/xbeam_nodepth")
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

#ifdef CLIENT_DLL
    void ClientThink() override;
    void OnDataChanged(DataUpdateType_t updateType) override;
    void ViewModelDrawn(C_BaseViewModel *pBaseViewModel) override;
#endif

    // Public utility methods
    bool HasAmmo() const;
    
    // Bot-friendly firing behavior
    virtual float GetFireRate() { return 0.05f; }  // Fast continuous firing rate for bots
    virtual bool CanHolster() { return m_fireState == FIRE_OFF; } // Only holster when not firing
    virtual float GetMinRestTime() { return 0.1f; }  // Short rest time for continuous beam
    virtual float GetMaxRestTime() { return 0.3f; }  // Keep firing aggressively
    
#ifndef CLIENT_DLL
    virtual int WeaponRangeAttack1Condition(float flDot, float flDist);
#endif

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
    CNetworkVar(Vector, m_vecBeamEndPos);

#ifdef CLIENT_DLL
    // Client-only beam rendering
    CBeam *m_pClientBeam;
    CBeam *m_pClientNoise;
    CSprite *m_pClientSprite;
    Vector m_vecLastEndPos;
    float m_flNextBeamUpdateTime;
    dlight_t *m_pBeamGlow;
    bool m_bClientThinking;
    int m_nBeamDelayFrames;
#endif

    // State variables
    float m_flStartFireTime;
    float m_flShakeTime;
    float m_flStartSoundDuration;
    bool m_bTransitionedToCharge;
    bool m_bRunSoundPlaying; // Track if run sound is currently playing

    // Core weapon methods
    void StartFiring();
    void StopFiring();
    void Fire();
    
    // Effect management
#ifdef CLIENT_DLL
    void CreateClientBeams();
    void UpdateClientBeams();
    void DestroyClientBeams();
    Vector GetMuzzlePosition();
#endif
    
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
    RecvPropVector(RECVINFO(m_vecBeamEndPos)),
#else
    SendPropFloat(SENDINFO(m_flAmmoUseTime)),
    SendPropFloat(SENDINFO(m_flNextDamageTime)),
    SendPropInt(SENDINFO(m_fireState)),
    SendPropVector(SENDINFO(m_vecBeamEndPos)),
#endif
END_NETWORK_TABLE()

// Prediction table
#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA(CWeaponEgon)
    DEFINE_PRED_FIELD(m_flAmmoUseTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE),
    DEFINE_PRED_FIELD(m_flNextDamageTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE),
    DEFINE_PRED_FIELD(m_fireState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE),
    DEFINE_PRED_FIELD(m_vecBeamEndPos, FIELD_VECTOR, FTYPEDESC_INSENDTABLE),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS(weapon_egon, CWeaponEgon);
PRECACHE_WEAPON_REGISTER(weapon_egon);

// Activity table
acttable_t CWeaponEgon::m_acttable[] = 
{
    { ACT_HL2MP_IDLE,                    ACT_HL2MP_IDLE_AR2,                    false },
    { ACT_HL2MP_RUN,                     ACT_HL2MP_RUN_AR2,                     false },
    { ACT_HL2MP_IDLE_CROUCH,            ACT_HL2MP_IDLE_CROUCH_AR2,             false },
    { ACT_HL2MP_WALK_CROUCH,            ACT_HL2MP_WALK_CROUCH_AR2,             false },
    // Removed: ACT_HL2MP_GESTURE_RANGE_ATTACK to disable third-person attack animation
    { ACT_HL2MP_GESTURE_RELOAD,          ACT_HL2MP_GESTURE_RELOAD_AR2,          false },
    { ACT_HL2MP_JUMP,                    ACT_HL2MP_JUMP_AR2,                    false },
    // Removed: ACT_RANGE_ATTACK1 to disable NPC attack animation
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
    m_vecBeamEndPos = vec3_origin;
    m_flStartFireTime = 0.0f;
    m_flShakeTime = 0.0f;
    m_flStartSoundDuration = EgonConstants::DEFAULT_SOUND_DURATION;
    m_bTransitionedToCharge = false;
    m_bRunSoundPlaying = false;

    // Bot-friendly range configuration  
    m_fMinRange1 = 256.0f;  // Increased minimum range - bots should stay back and use range advantage
    m_fMaxRange1 = EgonConstants::BEAM_LENGTH; // Maximum range matches beam length
    m_fMinRange2 = 0.0f;    // No secondary attack
    m_fMaxRange2 = 0.0f;

#ifdef CLIENT_DLL
    m_pClientBeam = nullptr;
    m_pClientNoise = nullptr;
    m_pClientSprite = nullptr;
    m_vecLastEndPos = vec3_origin;
    m_flNextBeamUpdateTime = 0.0f;
    m_pBeamGlow = nullptr;
    m_bClientThinking = false;
    m_nBeamDelayFrames = 0;
#endif
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CWeaponEgon::~CWeaponEgon()
{
#ifdef CLIENT_DLL
    DestroyClientBeams();
#endif
}

//-----------------------------------------------------------------------------
// Precache assets
//-----------------------------------------------------------------------------
void CWeaponEgon::Precache()
{
    PrecacheModel(EgonConstants::BEAM_SPRITE);
    PrecacheModel(EgonConstants::BEAM_SPRITE_NODEPTH);
    PrecacheModel(EgonConstants::FLARE_SPRITE);

    // Precache sounds and get actual duration
    PrecacheScriptSound(EgonConstants::SOUND_START);
    PrecacheScriptSound(EgonConstants::SOUND_RUN);
    PrecacheScriptSound(EgonConstants::SOUND_OFF);

    // Get actual sound duration for timing from weapon script
    const char *startSound = GetShootSound(SINGLE);
    if (startSound && startSound[0])
    {
        CSoundParameters params;
        if (GetParametersForSound(startSound, params, nullptr))
        {
            m_flStartSoundDuration = enginesound->GetSoundDuration(params.soundname);
        }
    }

    BaseClass::Precache();
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Get muzzle position from attachment - handles both first and third person
//-----------------------------------------------------------------------------
Vector CWeaponEgon::GetMuzzlePosition()
{
    C_BasePlayer *pOwner = ToBasePlayer(GetOwner());
    if (!pOwner)
        return GetAbsOrigin();

    Vector muzzlePos;
    QAngle muzzleAng;
    
    // Check if we should use viewmodel (first person) or world model (third person)
    if (ShouldDrawUsingViewModel())
    {
        // First person - use viewmodel attachments
        C_BaseViewModel *pViewModel = pOwner->GetViewModel();
        if (pViewModel)
        {
            int attachment = pViewModel->LookupAttachment("muzzle");
            if (attachment == -1)
                attachment = pViewModel->LookupAttachment("0");
                
            if (attachment != -1 && pViewModel->GetAttachment(attachment, muzzlePos, muzzleAng))
            {
                return muzzlePos;
            }
        }
    }
    else
    {
        // Third person - use world model attachments
        int attachment = LookupAttachment("muzzle");
        if (attachment == -1)
            attachment = LookupAttachment("0");
            
        if (attachment != -1 && GetAttachment(attachment, muzzlePos, muzzleAng))
        {
            return muzzlePos;
        }
    }
    
    // Fallback to owner's shoot position
    return pOwner->Weapon_ShootPosition();
}

//-----------------------------------------------------------------------------
// Create client-side beams and sprite
//-----------------------------------------------------------------------------
void CWeaponEgon::CreateClientBeams()
{
    DestroyClientBeams();

    Vector startPos = GetMuzzlePosition();
    Vector endPos = m_vecBeamEndPos;

    // Wait for proper positioning data
    m_nBeamDelayFrames = 8;

    // Choose beam sprite based on viewmodel usage
    // Use xbeam1 for third-person (others), xbeam_nodepth for first-person (owner)
    const char* beamSprite = ShouldDrawUsingViewModel() ? 
                            EgonConstants::BEAM_SPRITE_NODEPTH : 
                            EgonConstants::BEAM_SPRITE;

    // Create primary beam
    m_pClientBeam = CBeam::BeamCreate(beamSprite, EgonConstants::BEAM_WIDTH);
    if (m_pClientBeam)
    {
        m_pClientBeam->SetStartPos(startPos);
        m_pClientBeam->SetEndPos(endPos);
        m_pClientBeam->SetBeamFlags(FBEAM_SINENOISE | FBEAM_FOREVER);
        m_pClientBeam->SetScrollRate(15);
        m_pClientBeam->SetBrightness(200);
        m_pClientBeam->SetColor(50, 215, 255);
        m_pClientBeam->SetNoise(0.2f);
        m_pClientBeam->SetRenderMode(kRenderTransAdd);
        m_pClientBeam->SetType(BEAM_POINTS);
        m_pClientBeam->PointsInit(startPos, endPos);
        m_pClientBeam->AddEffects(EF_NODRAW);
    }

    // Create noise beam
    m_pClientNoise = CBeam::BeamCreate(beamSprite, EgonConstants::NOISE_WIDTH);
    if (m_pClientNoise)
    {
        m_pClientNoise->SetStartPos(startPos);
        m_pClientNoise->SetEndPos(endPos);
        m_pClientNoise->SetBeamFlags(FBEAM_FOREVER);
        m_pClientNoise->SetScrollRate(10);
        m_pClientNoise->SetBrightness(150);
        m_pClientNoise->SetColor(50, 240, 255);
        m_pClientNoise->SetNoise(0.8f);
        m_pClientNoise->SetRenderMode(kRenderTransAdd);
        m_pClientNoise->SetType(BEAM_POINTS);
        m_pClientNoise->PointsInit(startPos, endPos);
        m_pClientNoise->AddEffects(EF_NODRAW);
    }

    // Create flare sprite
    m_pClientSprite = CSprite::SpriteCreate(EgonConstants::FLARE_SPRITE, endPos, false);
    if (m_pClientSprite)
    {
        m_pClientSprite->SetScale(EgonConstants::SPRITE_SCALE);
        m_pClientSprite->SetTransparency(kRenderGlow, 255, 255, 255, 255, kRenderFxNoDissipation);
        m_pClientSprite->AddSpawnFlags(SF_SPRITE_TEMPORARY);
        
        CBasePlayer *pOwner = ToBasePlayer(GetOwner());
        if (pOwner)
        {
            m_pClientSprite->SetOwnerEntity(pOwner);
        }
        
        m_pClientSprite->TurnOn();
        m_pClientSprite->AddEffects(EF_NODRAW);
        
        if (m_pClientSprite->GetClientHandle() == INVALID_CLIENTENTITY_HANDLE)
        {
            m_pClientSprite->Remove();
            m_pClientSprite = nullptr;
        }
    }

    m_pBeamGlow = nullptr;
}

//-----------------------------------------------------------------------------
// Update client beam positions
//-----------------------------------------------------------------------------
void CWeaponEgon::UpdateClientBeams()
{
    if (m_fireState == FIRE_OFF)
    {
        DestroyClientBeams();
        return;
    }

    Vector startPos = GetMuzzlePosition();
    Vector endPos = m_vecBeamEndPos;
    
    // Ensure minimum beam length
    Vector beamDir = endPos - startPos;
    float beamLength = beamDir.Length();
    
    if (beamLength < 16.0f)
    {
        beamDir.NormalizeInPlace();
        endPos = startPos + (beamDir * 16.0f);
    }

    // Handle frame delay for initial positioning
    if (m_nBeamDelayFrames > 0)
    {
        m_nBeamDelayFrames--;
        
        if (m_pClientBeam)
        {
            m_pClientBeam->SetStartPos(startPos);
            m_pClientBeam->SetEndPos(endPos);
            m_pClientBeam->PointsInit(startPos, endPos);
        }

        if (m_pClientNoise)
        {
            m_pClientNoise->SetStartPos(startPos);
            m_pClientNoise->SetEndPos(endPos);
            m_pClientNoise->PointsInit(startPos, endPos);
        }

        if (m_pClientSprite && m_pClientSprite->GetClientHandle() != INVALID_CLIENTENTITY_HANDLE)
        {
            m_pClientSprite->SetAbsOrigin(endPos);
        }
        
        m_vecLastEndPos = endPos;
        return;
    }

    // Create dynamic light
    if (!m_pBeamGlow)
    {
        m_pBeamGlow = effects->CL_AllocDlight(entindex());
        if (m_pBeamGlow)
        {
            m_pBeamGlow->origin = endPos;
            m_pBeamGlow->radius = 128.0f;
            m_pBeamGlow->color.r = 50;
            m_pBeamGlow->color.g = 215;
            m_pBeamGlow->color.b = 255;
            m_pBeamGlow->die = gpGlobals->curtime + 0.1f;
        }
    }

    // Smooth interpolation
    if (m_vecLastEndPos != vec3_origin)
    {
        float lerpFactor = gpGlobals->frametime * 25.0f;
        endPos = Lerp(clamp(lerpFactor, 0.0f, 1.0f), m_vecLastEndPos, endPos);
    }
    m_vecLastEndPos = endPos;

    // Create beams if they don't exist
    if (!m_pClientBeam || !m_pClientNoise || !m_pClientSprite)
    {
        CreateClientBeams();
        return;
    }

    // Show beams
    if (m_pClientBeam && m_pClientBeam->IsEffectActive(EF_NODRAW))
    {
        m_pClientBeam->RemoveEffects(EF_NODRAW);
    }
    
    if (m_pClientNoise && m_pClientNoise->IsEffectActive(EF_NODRAW))
    {
        m_pClientNoise->RemoveEffects(EF_NODRAW);
    }
    
    if (m_pClientSprite && m_pClientSprite->IsEffectActive(EF_NODRAW))
    {
        m_pClientSprite->RemoveEffects(EF_NODRAW);
    }

    // Update beam positions
    if (m_pClientBeam)
    {
        m_pClientBeam->SetStartPos(startPos);
        m_pClientBeam->SetEndPos(endPos);
        m_pClientBeam->PointsInit(startPos, endPos);
        m_pClientBeam->SetBeamFlags(m_pClientBeam->GetBeamFlags() | FBEAM_FOREVER);
    }

    if (m_pClientNoise)
    {
        m_pClientNoise->SetStartPos(startPos);
        m_pClientNoise->SetEndPos(endPos);
        m_pClientNoise->PointsInit(startPos, endPos);
        m_pClientNoise->SetBeamFlags(m_pClientNoise->GetBeamFlags() | FBEAM_FOREVER);
    }

    // Update sprite
    if (m_pClientSprite && m_pClientSprite->GetClientHandle() != INVALID_CLIENTENTITY_HANDLE)
    {
        m_pClientSprite->SetAbsOrigin(endPos);
        
        float currentFrame = m_pClientSprite->m_flFrame;
        currentFrame += 8.0f * gpGlobals->frametime;
        
        if (currentFrame > m_pClientSprite->Frames())
            currentFrame = 0.0f;
            
        m_pClientSprite->m_flFrame = currentFrame;
    }

    // Update dynamic light
    if (m_pBeamGlow)
    {
        m_pBeamGlow->origin = endPos;
        m_pBeamGlow->die = gpGlobals->curtime + 0.1f;
    }
}

//-----------------------------------------------------------------------------
// Destroy client beams
//-----------------------------------------------------------------------------
void CWeaponEgon::DestroyClientBeams()
{
    if (m_pClientBeam)
    {
        m_pClientBeam->Remove();
        m_pClientBeam = nullptr;
    }
    
    if (m_pClientNoise)
    {
        m_pClientNoise->Remove();
        m_pClientNoise = nullptr;
    }
    
    if (m_pClientSprite)
    {
        if (m_pClientSprite->GetClientHandle() != INVALID_CLIENTENTITY_HANDLE)
        {
            m_pClientSprite->Remove();
        }
        m_pClientSprite = nullptr;
    }
    
    m_pBeamGlow = nullptr;
    m_vecLastEndPos = vec3_origin;
    m_nBeamDelayFrames = 0;
}

//-----------------------------------------------------------------------------
// Client think
//-----------------------------------------------------------------------------
void CWeaponEgon::ClientThink()
{
    BaseClass::ClientThink();
    
    UpdateClientBeams();
    
    if (m_fireState != FIRE_OFF)
    {
        if (!m_bClientThinking)
        {
            SetNextClientThink(CLIENT_THINK_ALWAYS);
            m_bClientThinking = true;
        }
    }
    else
    {
        if (m_bClientThinking)
        {
            SetNextClientThink(CLIENT_THINK_NEVER);
            m_bClientThinking = false;
        }
    }
}

//-----------------------------------------------------------------------------
// Handle data changes from server
//-----------------------------------------------------------------------------
void CWeaponEgon::OnDataChanged(DataUpdateType_t updateType)
{
    BaseClass::OnDataChanged(updateType);
    
    if (m_fireState != FIRE_OFF && !m_bClientThinking)
    {
        SetNextClientThink(CLIENT_THINK_ALWAYS);
        m_bClientThinking = true;
    }
    else if (m_fireState == FIRE_OFF && m_bClientThinking)
    {
        SetNextClientThink(CLIENT_THINK_NEVER);
        m_bClientThinking = false;
        DestroyClientBeams();
    }
}

//-----------------------------------------------------------------------------
// Called when viewmodel is drawn
//-----------------------------------------------------------------------------
void CWeaponEgon::ViewModelDrawn(C_BaseViewModel *pBaseViewModel)
{
    BaseClass::ViewModelDrawn(pBaseViewModel);
    
    if (m_fireState != FIRE_OFF)
    {
        UpdateClientBeams();
    }
}
#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Deploy weapon
//-----------------------------------------------------------------------------
bool CWeaponEgon::Deploy()
{
    StopFiring();
    m_bRunSoundPlaying = false;
    return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Holster weapon
//-----------------------------------------------------------------------------
bool CWeaponEgon::Holster(CBaseCombatWeapon *pSwitchingTo)
{
    // Stop all sounds using proper weapon sound system
    StopWeaponSound(SINGLE);    // Stop start sound
    StopWeaponSound(SPECIAL1);  // Stop run sound
    
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

    // Audio and visual feedback using proper weapon sound system
    WeaponSound(SINGLE);  // Play start sound
    SendWeaponAnim(ACT_VM_PULLBACK);
    
    m_fireState = FIRE_STARTUP;
    
    // Set immediate next attack for continuous firing (important for bots)
    m_flNextPrimaryAttack = gpGlobals->curtime + 0.1f;

#ifdef CLIENT_DLL
    m_nBeamDelayFrames = 6;
    
    if (!m_bClientThinking)
    {
        SetNextClientThink(CLIENT_THINK_ALWAYS);
        m_bClientThinking = true;
    }
#endif
}

//-----------------------------------------------------------------------------
// Stop firing sequence
//-----------------------------------------------------------------------------
void CWeaponEgon::StopFiring()
{
    if (m_fireState == FIRE_OFF)
        return;

    // Stop all firing sounds and play off sound using proper weapon sound system
    StopWeaponSound(SINGLE);     // Stop start sound
    StopWeaponSound(SPECIAL1);   // Stop run sound
    WeaponSound(SPECIAL2);       // Play off sound
    
    m_bRunSoundPlaying = false;
    
    // Reset animation and effects
    SendWeaponAnim(ACT_VM_IDLE);
    
#ifdef CLIENT_DLL
    DestroyClientBeams();
    if (m_bClientThinking)
    {
        SetNextClientThink(CLIENT_THINK_NEVER);
        m_bClientThinking = false;
    }
#endif

    // Reset state
    m_fireState = FIRE_OFF;
    m_bTransitionedToCharge = false;
    m_flNextPrimaryAttack = gpGlobals->curtime + EgonConstants::STARTUP_DELAY;
    SetWeaponIdleTime(gpGlobals->curtime + EgonConstants::STARTUP_DELAY);
    
    m_vecBeamEndPos = vec3_origin;
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
        // Stop the start sound and play run sound using proper weapon sound system
        StopWeaponSound(SINGLE);     // Stop start sound
        
        if (!m_bRunSoundPlaying)
        {
            WeaponSound(SPECIAL1);   // Play run sound
            m_bRunSoundPlaying = true;
        }
        
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

    // Update beam end position for networking
    m_vecBeamEndPos = tr.endpos;

    // Set rapid fire rate for continuous beam (important for bot AI)
    m_flNextPrimaryAttack = gpGlobals->curtime + 0.05f;

#ifndef CLIENT_DLL
    // Process damage at intervals
    if (gpGlobals->curtime >= m_flNextDamageTime)
    {
        ProcessDamage(tr, vecAiming);
        m_flNextDamageTime = gpGlobals->curtime + EgonConstants::DAMAGE_INTERVAL;
    }

    // Impact effects for valid surfaces
    if (tr.fraction < 1.0f && !(tr.surface.flags & SURF_SKY))
    {
        UTIL_ScreenShake(tr.endpos, 5.0f, 150.0f, 0.25f, 150.0f, SHAKE_START);
    }
#endif

    // Consume ammo
    ProcessAmmoConsumption();
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

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Bot firing condition - when is it appropriate to use the egon?
//-----------------------------------------------------------------------------
int CWeaponEgon::WeaponRangeAttack1Condition(float flDot, float flDist)
{
    // Don't fire if no ammo
    if (!HasAmmo())
        return COND_NO_PRIMARY_AMMO;
    
    // Egon excels at medium to long range - encourage bots to keep distance
    if (flDist >= m_fMinRange1 && flDist <= m_fMaxRange1)
    {
        // More lenient aim requirement since it's a continuous beam
        if (flDot >= 0.5f) // ~60 degree cone - more forgiving
        {
            return COND_CAN_RANGE_ATTACK1;
        }
        else
        {
            return COND_NOT_FACING_ATTACK; // Need to turn toward target first
        }
    }
    
    // Too close - encourage bots to back up and use range advantage
    if (flDist < m_fMinRange1)
        return COND_TOO_CLOSE_TO_ATTACK;
    
    // Too far away
    return COND_TOO_FAR_TO_ATTACK;
}
#endif