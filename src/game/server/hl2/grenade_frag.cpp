//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "basegrenade_shared.h"
#include "grenade_frag.h"
#include "Sprite.h"
#include "SpriteTrail.h"
#include "soundent.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define FRAG_GRENADE_BLIP_FREQUENCY         1.0f
#define FRAG_GRENADE_BLIP_FAST_FREQUENCY    0.2f

#define FRAG_GRENADE_GRACE_TIME_AFTER_PICKUP 1.5f
#define FRAG_GRENADE_WARN_TIME 1.5f

const float GRENADE_COEFFICIENT_OF_RESTITUTION = 0.2f;

ConVar sk_plr_dmg_fraggrenade("sk_plr_dmg_fraggrenade", "0");
ConVar sk_npc_dmg_fraggrenade("sk_npc_dmg_fraggrenade", "0");
ConVar sk_fraggrenade_radius("sk_fraggrenade_radius", "0");
ConVar rb_grenade_yoyo("rb_grenade_yoyo", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Toggle Grenade timer resetting with the Gravity Gun");

#define GRENADE_MODEL "models/Weapons/w_grenade.mdl"

LINK_ENTITY_TO_CLASS(npc_grenade_frag, CGrenadeFrag);

// Add the network table here
IMPLEMENT_SERVERCLASS_ST(CGrenadeFrag, DT_GrenadeFragLight)
    SendPropFloat(SENDINFO(m_flNextBlipTime)),
    SendPropBool(SENDINFO(m_bFastBlink)),
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGrenadeFrag::CGrenadeFrag(void) : BaseClass()
{
    m_bFastBlink = false;
    m_flNextBlipTime = 0.0f;
    m_pMainGlow = NULL;
    m_pGlowTrail = NULL;
    m_inSolid = false;
    m_combineSpawned = false;
    m_punted = false;
    m_flGlowTransparency = 200.0f;
    m_flGlowTrailTransparency = 255.0f;
}

BEGIN_DATADESC(CGrenadeFrag)

    // Fields
    DEFINE_FIELD(m_pMainGlow, FIELD_EHANDLE),
    DEFINE_FIELD(m_pGlowTrail, FIELD_EHANDLE),
    DEFINE_FIELD(m_flNextBlipTime, FIELD_TIME),
    DEFINE_FIELD(m_inSolid, FIELD_BOOLEAN),
    DEFINE_FIELD(m_combineSpawned, FIELD_BOOLEAN),
    DEFINE_FIELD(m_punted, FIELD_BOOLEAN),

    // Function Pointers
    DEFINE_THINKFUNC(DelayThink),

    // Inputs
    DEFINE_INPUTFUNC(FIELD_FLOAT, "SetTimer", InputSetTimer),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGrenadeFrag::~CGrenadeFrag(void)
{
    if (m_pMainGlow)
    {
        UTIL_Remove(m_pMainGlow);
        m_pMainGlow = NULL;
    }

    if (m_pGlowTrail)
    {
        UTIL_Remove(m_pGlowTrail);
        m_pGlowTrail = NULL;
    }
}

//-----------------------------------------------------------------------------
// Purpose: Spawn the grenade
//-----------------------------------------------------------------------------
void CGrenadeFrag::Spawn(void)
{
    Precache();

    SetModel(GRENADE_MODEL);

    if (GetOwnerEntity() && GetOwnerEntity()->IsPlayer())
    {
        m_flDamage = sk_plr_dmg_fraggrenade.GetFloat();
        m_DmgRadius = sk_fraggrenade_radius.GetFloat();
    }
    else
    {
        m_flDamage = sk_npc_dmg_fraggrenade.GetFloat();
        m_DmgRadius = sk_fraggrenade_radius.GetFloat();
    }

    m_takedamage = DAMAGE_YES;
    m_iHealth = 1;
    m_flGlowTransparency = 200.0f; // Initial transparency value
    m_flGlowTrailTransparency = 255.0f; // Initial transparency value

    SetSize(-Vector(4, 4, 4), Vector(4, 4, 4));
    SetCollisionGroup(COLLISION_GROUP_WEAPON);
    if (!CreateVPhysics())
    {
        UTIL_Remove(this);
        return;
    }

    BlipSound();
    m_flNextBlipTime = gpGlobals->curtime + FRAG_GRENADE_BLIP_FREQUENCY;

    AddSolidFlags(FSOLID_NOT_STANDABLE);

    m_combineSpawned = false;
    m_punted = false;

    IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
    if (pPhysicsObject != nullptr)
    {
        pPhysicsObject->SetMaterialIndex(physprops->GetSurfaceIndex("grenade"));
    }

    BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: Restore the grenade
//-----------------------------------------------------------------------------
void CGrenadeFrag::OnRestore(void)
{
    // If we were primed and ready to detonate, put FX on us.
    if (m_flDetonateTime > 0)
        CreateEffects();

    BaseClass::OnRestore();
}

//-----------------------------------------------------------------------------
// Purpose: Create visual effects for the grenade
//-----------------------------------------------------------------------------
void CGrenadeFrag::CreateEffects(void)
{
    // Start up the eye glow
    if (m_pMainGlow == nullptr)
        m_pMainGlow = CSprite::SpriteCreate("sprites/redglow1.vmt", GetLocalOrigin(), false);

    int nAttachment = LookupAttachment("fuse");

    if (m_pMainGlow != nullptr)
    {
        m_pMainGlow->FollowEntity(this);
        m_pMainGlow->SetAttachment(this, nAttachment);
        m_pMainGlow->SetTransparency(kRenderGlow, 255, 255, 255, 200, kRenderFxNoDissipation);
        m_pMainGlow->SetScale(0.2f);
        m_pMainGlow->SetGlowProxySize(4.0f);
    }

    // Start up the eye trail 
    if (m_pGlowTrail == nullptr)
        m_pGlowTrail = CSpriteTrail::SpriteTrailCreate("sprites/bluelaser1.vmt", GetLocalOrigin(), false);

    if (m_pGlowTrail != nullptr)
    {
        m_pGlowTrail->FollowEntity(this);
        m_pGlowTrail->SetAttachment(this, nAttachment);
        m_pGlowTrail->SetTransparency(kRenderTransAdd, 255, 0, 0, 255, kRenderFxNone);
        m_pGlowTrail->SetStartWidth(8.0f);
        m_pGlowTrail->SetEndWidth(1.0f);
        m_pGlowTrail->SetLifeTime(0.5f);
    }
}

//-----------------------------------------------------------------------------
// Purpose: Create VPhysics for the grenade
//-----------------------------------------------------------------------------
bool CGrenadeFrag::CreateVPhysics()
{
    // Create the object in the physics system
    IPhysicsObject *pPhysicsObject = VPhysicsInitNormal(SOLID_BBOX, 0, false);
    return (pPhysicsObject != nullptr);
}

//-----------------------------------------------------------------------------
// Purpose: Trace filter for collision group delta
//-----------------------------------------------------------------------------
class CTraceFilterCollisionGroupDelta : public CTraceFilterEntitiesOnly
{
public:
    DECLARE_CLASS_NOBASE(CTraceFilterCollisionGroupDelta);

    CTraceFilterCollisionGroupDelta(const IHandleEntity *passentity, int collisionGroupAlreadyChecked, int newCollisionGroup)
        : m_pPassEnt(passentity), m_collisionGroupAlreadyChecked(collisionGroupAlreadyChecked), m_newCollisionGroup(newCollisionGroup)
    {
    }

    virtual bool ShouldHitEntity(IHandleEntity *pHandleEntity, int contentsMask)
    {
        if (!PassServerEntityFilter(pHandleEntity, m_pPassEnt))
            return false;
        CBaseEntity *pEntity = EntityFromEntityHandle(pHandleEntity);

        if (pEntity != nullptr)
        {
            if (g_pGameRules->ShouldCollide(m_collisionGroupAlreadyChecked, pEntity->GetCollisionGroup()))
                return false;
            if (g_pGameRules->ShouldCollide(m_newCollisionGroup, pEntity->GetCollisionGroup()))
                return true;
        }

        return false;
    }

protected:
    const IHandleEntity *m_pPassEnt;
    int m_collisionGroupAlreadyChecked;
    int m_newCollisionGroup;
};

//-----------------------------------------------------------------------------
// Purpose: Update VPhysics for the grenade
//-----------------------------------------------------------------------------
void CGrenadeFrag::VPhysicsUpdate(IPhysicsObject *pPhysics)
{
    BaseClass::VPhysicsUpdate(pPhysics);
    Vector vel;
    AngularImpulse angVel;
    pPhysics->GetVelocity(&vel, &angVel);

    Vector start = GetAbsOrigin();
    // Find all entities that my collision group wouldn't hit, but COLLISION_GROUP_NONE would and bounce off of them as a ray cast
    CTraceFilterCollisionGroupDelta filter(this, GetCollisionGroup(), COLLISION_GROUP_NONE);
    trace_t tr;

    // Trace line to detect collisions
    UTIL_TraceLine(start, start + vel * gpGlobals->frametime, CONTENTS_HITBOX | CONTENTS_MONSTER | CONTENTS_SOLID, &filter, &tr);

    if (tr.startsolid)
    {
        if (!m_inSolid)
        {
            // Bounce backwards
            vel *= -GRENADE_COEFFICIENT_OF_RESTITUTION;
            pPhysics->SetVelocity(&vel, nullptr);
        }
        m_inSolid = true;
        return;
    }
    m_inSolid = false;
    if (tr.DidHit())
    {
        Vector dir = vel;
        VectorNormalize(dir);
        // Send a tiny amount of damage so the character will react to getting bonked
        CTakeDamageInfo info(this, GetThrower(), pPhysics->GetMass() * vel, GetAbsOrigin(), 0.1f, DMG_CRUSH);
        tr.m_pEnt->TakeDamage(info);

        // Reflect velocity around normal
        vel = -2.0f * tr.plane.normal * DotProduct(vel, tr.plane.normal) + vel;

        // Absorb 80% in impact
        vel *= GRENADE_COEFFICIENT_OF_RESTITUTION;
        angVel *= -0.5f;
        pPhysics->SetVelocity(&vel, &angVel);
    }
}

//-----------------------------------------------------------------------------
// Purpose: Precache the grenade
//-----------------------------------------------------------------------------
void CGrenadeFrag::Precache(void)
{
    PrecacheModel(GRENADE_MODEL);

    PrecacheScriptSound("Grenade.Blip");

    PrecacheModel("sprites/redglow1.vmt");
    PrecacheModel("sprites/bluelaser1.vmt");

    BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Set the timer for the grenade
//-----------------------------------------------------------------------------
void CGrenadeFrag::SetTimer(float detonateDelay, float warnDelay)
{
    m_flDetonateTime = gpGlobals->curtime + detonateDelay;
    m_flWarnAITime = gpGlobals->curtime + warnDelay;
    SetThink(&CGrenadeFrag::DelayThink);
    SetNextThink(gpGlobals->curtime);

    CreateEffects();
}

//-----------------------------------------------------------------------------
// Purpose: Handle PhysGun pickup
//-----------------------------------------------------------------------------
void CGrenadeFrag::OnPhysGunPickup(CBasePlayer *pPhysGunUser, PhysGunPickup_t reason)
{
    if (rb_grenade_yoyo.GetBool())
    {
        SetThrower(pPhysGunUser);

        SetTimer(FRAG_GRENADE_GRACE_TIME_AFTER_PICKUP, FRAG_GRENADE_GRACE_TIME_AFTER_PICKUP / 2);

        BlipSound();
        m_flNextBlipTime = gpGlobals->curtime + FRAG_GRENADE_BLIP_FAST_FREQUENCY;
        m_bHasWarnedAI = true;

        SetPunted(true);
    }

    BaseClass::OnPhysGunPickup(pPhysGunUser, reason);
}

//-----------------------------------------------------------------------------
// Purpose: Handle delay think
//-----------------------------------------------------------------------------
void CGrenadeFrag::DelayThink()
{
    if (gpGlobals->curtime > m_flDetonateTime)
    {
        Detonate();
        return;
    }

    if (!m_bHasWarnedAI && gpGlobals->curtime >= m_flWarnAITime)
    {
#if !defined(CLIENT_DLL)
        CSoundEnt::InsertSound(SOUND_DANGER, GetAbsOrigin(), 400, 1.5, this);
#endif
        m_bHasWarnedAI = true;
    }

    // Check if it's time to play the blip sound and set the next blip time
    if (gpGlobals->curtime > m_flNextBlipTime)
    {
        BlipSound();

        if (m_bHasWarnedAI)
        {
            m_flNextBlipTime = gpGlobals->curtime + FRAG_GRENADE_BLIP_FAST_FREQUENCY;
        }
        else
        {
            m_flNextBlipTime = gpGlobals->curtime + FRAG_GRENADE_BLIP_FREQUENCY;
        }
    }

    // Calculate the new transparency values based on the blip frequency
    float flBlinkDuration = m_bHasWarnedAI ? FRAG_GRENADE_BLIP_FAST_FREQUENCY : FRAG_GRENADE_BLIP_FREQUENCY;
    float flTimeSinceLastBlip = gpGlobals->curtime - (m_flNextBlipTime - flBlinkDuration);
    float flBlinkProgress = flTimeSinceLastBlip / flBlinkDuration;

    // Smoothly transition the transparency values
    m_flGlowTransparency = 200.0f * (1.0f - fabs(2.0f * flBlinkProgress - 1.0f));
    m_flGlowTrailTransparency = 255.0f * (1.0f - fabs(2.0f * flBlinkProgress - 1.0f));

    // Update the transparency of the glow and the glow trail
    if (m_pMainGlow)
    {
        m_pMainGlow->SetTransparency(kRenderGlow, 255, 255, 255, m_flGlowTransparency, kRenderFxNoDissipation);
    }

    if (m_pGlowTrail)
    {
        m_pGlowTrail->SetTransparency(kRenderTransAdd, 255, 0, 0, m_flGlowTrailTransparency, kRenderFxNone);
    }

    SetNextThink(gpGlobals->curtime + 0.1);
}

//-----------------------------------------------------------------------------
// Purpose: Set the velocity of the grenade
//-----------------------------------------------------------------------------
void CGrenadeFrag::SetVelocity(const Vector &velocity, const AngularImpulse &angVelocity)
{
    IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
    if (pPhysicsObject != nullptr)
    {
        pPhysicsObject->AddVelocity(&velocity, &angVelocity);
    }
}

//-----------------------------------------------------------------------------
// Purpose: Handle damage taken by the grenade
//-----------------------------------------------------------------------------
int CGrenadeFrag::OnTakeDamage(const CTakeDamageInfo &inputInfo)
{
    // Manually apply vphysics because BaseCombatCharacter takedamage doesn't call back to CBaseEntity OnTakeDamage
    VPhysicsTakeDamage(inputInfo);

    // Grenades only suffer blast damage and burn damage.
    if (!(inputInfo.GetDamageType() & (DMG_BLAST | DMG_BURN)))
        return 0;

    return BaseClass::OnTakeDamage(inputInfo);
}

//-----------------------------------------------------------------------------
// Purpose: Handle input to set the timer
//-----------------------------------------------------------------------------
void CGrenadeFrag::InputSetTimer(inputdata_t &inputdata)
{
    SetTimer(inputdata.value.Float(), inputdata.value.Float() - FRAG_GRENADE_WARN_TIME);
}

//-----------------------------------------------------------------------------
// Purpose: Create a frag grenade
//-----------------------------------------------------------------------------
CBaseGrenade *Fraggrenade_Create(const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, float timer, bool combineSpawned)
{
    // Don't set the owner here, or the player can't interact with grenades he's thrown
    CGrenadeFrag *pGrenade = (CGrenadeFrag *)CBaseEntity::Create("npc_grenade_frag", position, angles, pOwner);

    pGrenade->SetTimer(timer, timer - FRAG_GRENADE_WARN_TIME);
    pGrenade->SetVelocity(velocity, angVelocity);
    pGrenade->SetThrower(ToBaseCombatCharacter(pOwner));
    pGrenade->m_takedamage = DAMAGE_EVENTS_ONLY;
    pGrenade->SetCombineSpawned(combineSpawned);

    return pGrenade;
}

//-----------------------------------------------------------------------------
// Purpose: Check if the grenade was punted
//-----------------------------------------------------------------------------
bool Fraggrenade_WasPunted(const CBaseEntity *pEntity)
{
    const CGrenadeFrag *pFrag = dynamic_cast<const CGrenadeFrag *>(pEntity);
    if (pFrag != nullptr)
    {
        return pFrag->WasPunted();
    }

    return false;
}

//-----------------------------------------------------------------------------
// Purpose: Check if the grenade was created by Combine
//-----------------------------------------------------------------------------
bool Fraggrenade_WasCreatedByCombine(const CBaseEntity *pEntity)
{
    const CGrenadeFrag *pFrag = dynamic_cast<const CGrenadeFrag *>(pEntity);
    if (pFrag != nullptr)
    {
        return pFrag->IsCombineSpawned();
    }

    return false;
}