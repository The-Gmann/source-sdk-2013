//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "in_buttons.h"
#include "ammodef.h"
#include "weapon_crossbow.h"
#include "zoom_shared.h"

#ifdef CLIENT_DLL
	#include "c_hl2mp_player.h"
	#include "c_te_effect_dispatch.h"
#else
	#include "hl2mp_player.h"
	#include "te_effect_dispatch.h"
	#include "IEffects.h"
	#include "Sprite.h"
	#include "SpriteTrail.h"
	#include "ilagcompensationmanager.h"
#endif

#include "weapon_hl2mpbasehlmpcombatweapon.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar rbsv_crossbow_sniperbolt("rbsv_crossbow_sniperbolt", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Toggle sniper bolt.");
ConVar rbsv_crossbow_explosivebolt("rbsv_crossbow_explosivebolt", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Toggle explosive bolt.");
ConVar rbcl_smooth_zoom("rbcl_smooth_zoom", "1", FCVAR_ARCHIVE | FCVAR_CLIENTDLL, "Enable or disable zoom transition smoothing");
ConVar rbcl_crossbow_scope("rbcl_crossbow_scope", "0", FCVAR_ARCHIVE | FCVAR_CLIENTDLL, "Enable or disable crossbow scope overlay");

#define BOLT_MODEL        "models/crossbow_bolt.mdl"

#define BOLT_AIR_VELOCITY    3500
#define BOLT_WATER_VELOCITY    1500
#define    BOLT_SKIN_NORMAL    0
#define BOLT_SKIN_GLOW        1

#ifndef CLIENT_DLL

extern ConVar sk_plr_dmg_crossbow;
extern ConVar sk_npc_dmg_crossbow;

void TE_StickyBolt( IRecipientFilter& filter, float delay,    Vector vecDirection, const Vector *origin );
void TE_Sparks( IRecipientFilter& filter, float delay, const Vector* pos, int nMagnitude, int nTrailLength, const Vector *pDir );

void ExplosionCreate(const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, int magnitude, int radius, bool doDamage)
{
    CBaseEntity *pExplosion = CBaseEntity::Create("env_explosion", vecOrigin, vecAngles, pOwner);
    if (pExplosion)
    {
        pExplosion->SetOwnerEntity(pOwner);
        pExplosion->SetAbsOrigin(vecOrigin);
        pExplosion->SetAbsAngles(vecAngles);
        pExplosion->KeyValue("iMagnitude", magnitude);
        pExplosion->KeyValue("iRadiusOverride", radius);
        pExplosion->KeyValue("spawnflags", "1"); // No fireball
        pExplosion->Spawn();
        pExplosion->Activate();
        
        // Trigger the explosion
        variant_t emptyVariant;
        pExplosion->AcceptInput("Explode", pOwner, pOwner, emptyVariant, 0.0f);
    }
}

//-----------------------------------------------------------------------------
// Crossbow Bolt
//-----------------------------------------------------------------------------
class CCrossbowBolt : public CBaseCombatCharacter
{
    DECLARE_CLASS( CCrossbowBolt, CBaseCombatCharacter );

public:
    CCrossbowBolt() : m_bIsSniperBolt(false), m_bExplosive(false) { };
    ~CCrossbowBolt();

    Class_T Classify( void ) { return CLASS_NONE; }

public:
    void Spawn( void );
    void Precache( void );
    void BubbleThink( void );
    void BoltTouch( CBaseEntity *pOther );
    bool CreateVPhysics( void );
    unsigned int PhysicsSolidMaskForEntity() const;
    static CCrossbowBolt *BoltCreate(const Vector &vecOrigin, const QAngle &angAngles, int iDamage, CBasePlayer *pentOwner = NULL, bool bIsSniperBolt = false);

    void SetSniperBolt(bool isSniperBolt) { m_bIsSniperBolt = isSniperBolt; }
    void SetExplosive(bool isExplosive) { m_bExplosive = isExplosive; }

protected:
    bool CreateSprites(void);

    CHandle<CSprite> m_pGlowSprite;
    CHandle<CSpriteTrail> m_pGlowTrail;

    int m_iDamage;
    bool m_bIsSniperBolt; // Add this flag
    bool m_bExplosive; // Add this flag

    void Explode(const trace_t &tr, CBaseEntity *pOther);

    DECLARE_DATADESC();
    DECLARE_SERVERCLASS();
};

LINK_ENTITY_TO_CLASS( crossbow_bolt, CCrossbowBolt );

BEGIN_DATADESC(CCrossbowBolt)
    // Function Pointers
    DEFINE_FUNCTION(BubbleThink),
    DEFINE_FUNCTION(BoltTouch),

    // These are recreated on reload, they don't need storage
    DEFINE_FIELD(m_pGlowSprite, FIELD_EHANDLE),
    DEFINE_FIELD(m_pGlowTrail, FIELD_EHANDLE),
    DEFINE_FIELD(m_bIsSniperBolt, FIELD_BOOLEAN), // Add this field

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CCrossbowBolt, DT_CrossbowBolt )
END_SEND_TABLE()

CCrossbowBolt *CCrossbowBolt::BoltCreate(const Vector &vecOrigin, const QAngle &angAngles, int iDamage, CBasePlayer *pentOwner, bool bIsSniperBolt)
{
    // Create a new entity with CCrossbowBolt private data
    CCrossbowBolt *pBolt = (CCrossbowBolt *)CreateEntityByName("crossbow_bolt");
    if (!pBolt)
    {
        Warning("Failed to create crossbow bolt entity\n");
        return NULL;
    }

    UTIL_SetOrigin(pBolt, vecOrigin);
    pBolt->SetAbsAngles(angAngles);
    
    // Set flags before spawning so CreateSprites can check them
    pBolt->m_iDamage = iDamage;
    pBolt->SetSniperBolt(bIsSniperBolt);
    pBolt->SetExplosive(!bIsSniperBolt && rbsv_crossbow_explosivebolt.GetBool());
    
    pBolt->Spawn();
    pBolt->SetOwnerEntity(pentOwner);

    return pBolt;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCrossbowBolt::~CCrossbowBolt( void )
{
    if ( m_pGlowSprite )
    {
        UTIL_Remove( m_pGlowSprite );
    }
    
    if ( m_pGlowTrail )
    {
        UTIL_Remove( m_pGlowTrail );
    }
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CCrossbowBolt::CreateVPhysics( void )
{
    // Create the object in the physics system
    VPhysicsInitNormal( SOLID_BBOX, FSOLID_NOT_STANDABLE, false );

    return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
unsigned int CCrossbowBolt::PhysicsSolidMaskForEntity() const
{
    return ( BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX ) & ~CONTENTS_GRATE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CCrossbowBolt::CreateSprites(void)
{
    // Start up the eye glow
    m_pGlowSprite = CSprite::SpriteCreate("sprites/light_glow02_noz.vmt", GetLocalOrigin(), false);

    if (m_pGlowSprite != NULL)
    {
        m_pGlowSprite->FollowEntity(this);
        m_pGlowSprite->SetTransparency(kRenderGlow, 255, 255, 255, 128, kRenderFxNoDissipation);
        m_pGlowSprite->SetScale(0.2f);
        m_pGlowSprite->TurnOff();
    }
    else
    {
        Warning("Failed to create glow sprite for crossbow bolt\n");
    }
    
    // Create orange trail for regular bolts (not sniper bolts)
    if (!m_bIsSniperBolt)
    {
        m_pGlowTrail = CSpriteTrail::SpriteTrailCreate("sprites/bluelaser1.vmt", GetLocalOrigin(), false);
        
        if (m_pGlowTrail != NULL)
        {
            m_pGlowTrail->FollowEntity(this);
            m_pGlowTrail->SetTransparency(kRenderTransAdd, 255, 50, 0, 200, kRenderFxNone); // Darker orange color
            m_pGlowTrail->SetStartWidth(3.0f); // 1.5x thicker (was 2.0f)
            m_pGlowTrail->SetEndWidth(0.15f); // 1.5x thicker (was 0.1f)
            m_pGlowTrail->SetLifeTime(0.75f); // Quick fade
        }
        else
        {
            Warning("Failed to create trail sprite for crossbow bolt\n");
        }
    }

    return (m_pGlowSprite != NULL);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCrossbowBolt::Spawn( void )
{
    Precache( );

    SetModel(BOLT_MODEL);
    SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
    UTIL_SetSize( this, -Vector(1,1,1), Vector(1,1,1) );
    SetSolid( SOLID_BBOX );
    SetGravity( 0.05f );
    
    // Make sure we're updated if we're underwater
    UpdateWaterState();

    SetTouch( &CCrossbowBolt::BoltTouch );

    SetThink( &CCrossbowBolt::BubbleThink );
    SetNextThink( gpGlobals->curtime + 0.1f );
    
    CreateSprites();

    // Make us glow until we've hit the wall
    m_nSkin = BOLT_SKIN_GLOW;
}

void CCrossbowBolt::Explode(const trace_t &tr, CBaseEntity *pOther)
{
    if (!m_bExplosive)
    {
        return;
    }

    // Create an explosion effect
    ExplosionCreate(GetAbsOrigin(), GetAbsAngles(), GetOwnerEntity(), 70, 128, true);

    // Deal splash damage
    CTakeDamageInfo info(this, GetOwnerEntity(), 70, DMG_BLAST);
    RadiusDamage(info, GetAbsOrigin(), 128, CLASS_NONE, NULL);

    // Remove the bolt
    UTIL_Remove(this);
}

void CCrossbowBolt::Precache( void )
{
    PrecacheModel( BOLT_MODEL );

    PrecacheModel( "sprites/light_glow02_noz.vmt" );
    PrecacheModel( "sprites/bluelaser1.vmt" ); // For the orange trail
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CCrossbowBolt::BoltTouch(CBaseEntity *pOther)
{
    if (!m_bIsSniperBolt && rbsv_crossbow_explosivebolt.GetBool())
    {
        trace_t tr;
        tr = BaseClass::GetTouchTrace();
        Explode(tr, pOther);
        return;
    }

    if (FClassnameIs(pOther, "prop_dynamic"))
    {
        trace_t    tr;
        tr = BaseClass::GetTouchTrace();
        UTIL_ImpactTrace(&tr, DMG_BULLET);
        UTIL_Remove(this);
        return;
    } 

    if (!pOther->IsSolid() || pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS))
        return;

    if (pOther->m_takedamage != DAMAGE_NO)
    {
        trace_t    tr;
        tr = BaseClass::GetTouchTrace();

        Vector    vecNormalizedVel = GetAbsVelocity();
        ClearMultiDamage();

        VectorNormalize(vecNormalizedVel);

        if (GetOwnerEntity() && GetOwnerEntity()->IsPlayer() && pOther->IsNPC())
        {
            CTakeDamageInfo    dmgInfo(this, GetOwnerEntity(), m_iDamage, DMG_NEVERGIB);
            dmgInfo.AdjustPlayerDamageInflictedForSkillLevel();
            CalculateMeleeDamageForce(&dmgInfo, vecNormalizedVel, tr.endpos, 0.7f);
            dmgInfo.SetDamagePosition(tr.endpos);
            pOther->DispatchTraceAttack(dmgInfo, vecNormalizedVel, &tr);
        }
        else
        {
            CTakeDamageInfo    dmgInfo(this, GetOwnerEntity(), m_iDamage, DMG_BULLET | DMG_NEVERGIB);
            CalculateMeleeDamageForce(&dmgInfo, vecNormalizedVel, tr.endpos, 0.7f);
            dmgInfo.SetDamagePosition(tr.endpos);
            pOther->DispatchTraceAttack(dmgInfo, vecNormalizedVel, &tr);
            // if what we hit is static architecture, can stay around for a while.
            Vector vecDir = GetAbsVelocity();
            float speed = VectorNormalize(vecDir);

            // See if we should reflect off this surface
            float hitDot = DotProduct(tr.plane.normal, -vecDir);

            if ((hitDot < 0.5f) && (speed > 100))
            {
                if (pOther->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS)
                    return;

                Vector vReflection = 2.0f * tr.plane.normal * hitDot + vecDir;

                QAngle reflectAngles;

                VectorAngles(vReflection, reflectAngles);

                SetLocalAngles(reflectAngles);

                SetAbsVelocity(vReflection * speed * 0.75f);
            }
            else
            {
                if (pOther->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS)
                    return;
                
                SetThink(&CCrossbowBolt::SUB_Remove);
                SetNextThink(gpGlobals->curtime + 2.0f);
                
                // Stick to what we've hit
                SetMoveType(MOVETYPE_NONE);
                SetParent(pOther); // Attach to the entity we hit
                
                Vector vForward;
                AngleVectors(GetAbsAngles(), &vForward);
                VectorNormalize(vForward);
                
                UTIL_ImpactTrace(&tr, DMG_BULLET);
                
                AddEffects(EF_NODRAW);
                SetTouch(NULL);
                SetThink(&CCrossbowBolt::SUB_Remove);
                SetNextThink(gpGlobals->curtime + 2.0f);
                
                if (m_pGlowSprite != NULL)
                {
                    m_pGlowSprite->TurnOff();
                }
                
                if (m_pGlowTrail != NULL)
                {
                    m_pGlowTrail->FadeAndDie(0.2f);
                }
            }
        }

        ApplyMultiDamage();
    }
    else
    {
        trace_t    tr;
        tr = BaseClass::GetTouchTrace();

        // See if we struck the world
        if (pOther->GetCollisionGroup() == COLLISION_GROUP_NONE && !(tr.surface.flags & SURF_SKY))
        {
            EmitSound("Weapon_Crossbow.BoltHitWorld");

            // if what we hit is static architecture, can stay around for a while.
            Vector vecDir = GetAbsVelocity();
            float speed = VectorNormalize(vecDir);

            // See if we should reflect off this surface
            float hitDot = DotProduct(tr.plane.normal, -vecDir);

            if ((hitDot < 0.5f) && (speed > 100))
            {
                Vector vReflection = 2.0f * tr.plane.normal * hitDot + vecDir;

                QAngle reflectAngles;

                VectorAngles(vReflection, reflectAngles);

                SetLocalAngles(reflectAngles);

                SetAbsVelocity(vReflection * speed * 0.75f);

                // Start to sink faster
                SetGravity(1.0f);
            }
            else
            {
                SetThink(&CCrossbowBolt::SUB_Remove);
                SetNextThink(gpGlobals->curtime + 2.0f);

                //FIXME: We actually want to stick (with hierarchy) to what we've hit
                SetMoveType(MOVETYPE_NONE);
                // ATTEMPT HOT BOLT FIX
                speed = 0;

                Vector vForward;

                AngleVectors(GetAbsAngles(), &vForward);
                VectorNormalize(vForward);

                CEffectData    data;

                data.m_vOrigin = tr.endpos;
                data.m_vNormal = vForward;
                data.m_nEntIndex = 0;

                DispatchEffect("BoltImpact", data);

                UTIL_ImpactTrace(&tr, DMG_BULLET);

                AddEffects(EF_NODRAW);
                SetTouch(NULL);
                SetThink(&CCrossbowBolt::SUB_Remove);
                // ATTEMPT HOT BOLT FIX
                SetNextThink(gpGlobals->curtime + 0.0f);

                if (m_pGlowSprite != NULL)
                {
                    m_pGlowSprite->TurnOn();
                    m_pGlowSprite->FadeAndDie(3.0f);
                }
                
                if (m_pGlowTrail != NULL)
                {
                    m_pGlowTrail->FadeAndDie(0.2f);
                }
            }

            // Shoot some sparks
            if (UTIL_PointContents(GetAbsOrigin()) != CONTENTS_WATER)
            {
                g_pEffects->Sparks(GetAbsOrigin());
            }
        }
        else
        {
            // Put a mark unless we've hit the sky
            if ((tr.surface.flags & SURF_SKY) == false)
            {
                UTIL_ImpactTrace(&tr, DMG_BULLET);
            }

            UTIL_Remove(this);
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCrossbowBolt::BubbleThink( void )
{
    AddEFlags( EFL_NO_WATER_VELOCITY_CHANGE );

    QAngle angNewAngles;

    VectorAngles( GetAbsVelocity(), angNewAngles );
    SetAbsAngles( angNewAngles );

    SetNextThink( gpGlobals->curtime + 0.1f );

    if ( GetWaterLevel()  == 0 )
        return;

    UTIL_BubbleTrail( GetAbsOrigin() - GetAbsVelocity() * 0.1f, GetAbsOrigin(), 5 );
}

#endif

//-----------------------------------------------------------------------------
// CWeaponCrossbow
//-----------------------------------------------------------------------------

#ifdef CLIENT_DLL
#define CWeaponCrossbow C_WeaponCrossbow
#endif

class CWeaponCrossbow : public CBaseHL2MPCombatWeapon
{
    DECLARE_CLASS( CWeaponCrossbow, CBaseHL2MPCombatWeapon );
public:

    CWeaponCrossbow( void );
    
    virtual void    Precache( void );
    virtual void    PrimaryAttack( void );
    virtual void    SecondaryAttack( void );
    virtual bool    Deploy( void );
    virtual bool    Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
    virtual bool    Reload( void );
    virtual void    ItemPostFrame( void );
    virtual void    ItemBusyFrame( void );
    virtual bool    SendWeaponAnim( int iActivity );

#ifndef CLIENT_DLL
    virtual void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
#endif

    DECLARE_NETWORKCLASS(); 
    DECLARE_PREDICTABLE();

private:
    
    void    SetSkin( int skinNum );
    void    CheckZoomToggle( void );
    void    FireBolt( void );
    void FireSniperBolt(void);
    void    SetBolt(int iSetting);
    void    ToggleZoom( void );
    
    // Various states for the crossbow's charger
    enum ChargerState_t
    {
        CHARGER_STATE_START_LOAD,
        CHARGER_STATE_START_CHARGE,
        CHARGER_STATE_READY,
        CHARGER_STATE_DISCHARGE,
        CHARGER_STATE_OFF,
    };

    void    CreateChargerEffects( void );
    void    SetChargerState( ChargerState_t state );
    void    DoLoadEffect( void );

    DECLARE_ACTTABLE();

private:
    
    // Charger effects
    ChargerState_t        m_nChargeState;

#ifndef CLIENT_DLL
    CHandle<CSprite> m_hChargerSprite;
    // REMOVED: m_hLastCosmeticBolt - no longer limiting sniper bolts to 1
#endif

    CNetworkVar( bool,    m_bInZoom );
    CNetworkVar( bool,    m_bMustReload );

    CWeaponCrossbow( const CWeaponCrossbow & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCrossbow, DT_WeaponCrossbow )

BEGIN_NETWORK_TABLE( CWeaponCrossbow, DT_WeaponCrossbow )
#ifdef CLIENT_DLL
    RecvPropBool( RECVINFO( m_bInZoom ) ),
    RecvPropBool( RECVINFO( m_bMustReload ) ),
#else
    SendPropBool( SENDINFO( m_bInZoom ) ),
    SendPropBool( SENDINFO( m_bMustReload ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponCrossbow )
    DEFINE_PRED_FIELD( m_bInZoom, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
    DEFINE_PRED_FIELD( m_bMustReload, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_crossbow, CWeaponCrossbow );

PRECACHE_WEAPON_REGISTER( weapon_crossbow );

acttable_t    CWeaponCrossbow::m_acttable[] = 
{
    { ACT_MP_STAND_IDLE,                ACT_HL2MP_IDLE_CROSSBOW,                    false },
    { ACT_MP_CROUCH_IDLE,                ACT_HL2MP_IDLE_CROUCH_CROSSBOW,                false },

    { ACT_MP_RUN,                        ACT_HL2MP_RUN_CROSSBOW,                        false },
    { ACT_MP_CROUCHWALK,                ACT_HL2MP_WALK_CROUCH_CROSSBOW,                false },

    { ACT_MP_ATTACK_STAND_PRIMARYFIRE,    ACT_HL2MP_GESTURE_RANGE_ATTACK_CROSSBOW,    false },
    { ACT_MP_ATTACK_CROUCH_PRIMARYFIRE,    ACT_HL2MP_GESTURE_RANGE_ATTACK_CROSSBOW,    false },

    { ACT_MP_RELOAD_STAND,                ACT_HL2MP_GESTURE_RANGE_ATTACK_CROSSBOW,            false },
    { ACT_MP_RELOAD_CROUCH,                ACT_HL2MP_GESTURE_RANGE_ATTACK_CROSSBOW,            false },

    { ACT_MP_JUMP,                        ACT_HL2MP_JUMP_CROSSBOW,                    false },
};

IMPLEMENT_ACTTABLE(CWeaponCrossbow);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponCrossbow::CWeaponCrossbow( void )
{
    m_bReloadsSingly    = true;
    m_bFiresUnderwater    = true;
    m_bInZoom            = false;
    m_bMustReload        = false;
}

#define    CROSSBOW_GLOW_SPRITE    "sprites/light_glow02_noz.vmt"
#define    CROSSBOW_GLOW_SPRITE2    "sprites/blueflare1.vmt"
#define    CROSSBOW_SCOPE_SPRITE    "scope.vmt"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::Precache( void )
{
#ifndef CLIENT_DLL
    UTIL_PrecacheOther( "crossbow_bolt" );
#endif

    PrecacheScriptSound( "Weapon_Crossbow.BoltHitBody" );
    PrecacheScriptSound( "Weapon_Crossbow.BoltHitWorld" );
    PrecacheScriptSound( "Weapon_Crossbow.BoltSkewer" );

    PrecacheModel( CROSSBOW_GLOW_SPRITE );
    PrecacheModel( CROSSBOW_GLOW_SPRITE2 );
    PrecacheModel( CROSSBOW_SCOPE_SPRITE ); // Precache the scope overlay

    BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponCrossbow::PrimaryAttack(void)
{
    if (rbsv_crossbow_sniperbolt.GetBool() && m_bInZoom && g_pGameRules->IsMultiplayer())
    {
        FireSniperBolt();
    }
    else
    {
        FireBolt();
    }

    // Signal a reload
    m_bMustReload = true;

    SetWeaponIdleTime(gpGlobals->curtime + SequenceDuration(ACT_VM_PRIMARYATTACK));
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponCrossbow::SecondaryAttack( void )
{
    //NOTENOTE: The zooming is handled by the post/busy frames
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponCrossbow::Reload( void )
{
    if ( BaseClass::Reload() )
    {
        m_bMustReload = false;
        return true;
    }

    return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::CheckZoomToggle( void )
{
    CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
    
    if ( pPlayer->m_afButtonPressed & IN_ATTACK2 )
    {
        ToggleZoom();
    }
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::ItemBusyFrame( void )
{
    // Allow zoom toggling even when we're reloading
    CheckZoomToggle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::ItemPostFrame( void )
{
    // Allow zoom toggling
    CheckZoomToggle();

    if ( m_bMustReload && HasWeaponIdleTimeElapsed() )
    {
        Reload();
    }

    BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::FireBolt(void)
{
    if (m_iClip1 <= 0)
    {
        if (!m_bFireOnEmpty)
        {
            Reload();
        }
        else
        {
            WeaponSound(EMPTY);
            m_flNextPrimaryAttack = 0.15;
        }

        return;
    }

    CBasePlayer* pOwner = ToBasePlayer(GetOwner());

    if (pOwner == NULL)
        return;

#ifndef CLIENT_DLL
    Vector vecAiming = pOwner->GetAutoaimVector(0);
    Vector vecSrc = pOwner->Weapon_ShootPosition();
    QAngle angAiming;
    VectorAngles(vecAiming, angAiming);

    CCrossbowBolt* pBolt = CCrossbowBolt::BoltCreate(vecSrc, angAiming, GetHL2MPWpnData().m_iPlayerDamage, pOwner);

    if (pOwner->GetWaterLevel() == 3)
    {
        pBolt->SetAbsVelocity(vecAiming * BOLT_WATER_VELOCITY);
    }
    else
    {
        pBolt->SetAbsVelocity(vecAiming * BOLT_AIR_VELOCITY);
    }
#endif

    m_iClip1--;

    SetBolt(1);

    pOwner->ViewPunch(QAngle(-2, 0, 0));

    WeaponSound(SINGLE);
    WeaponSound(SPECIAL2);

    SendWeaponAnim(ACT_VM_PRIMARYATTACK);

    if (!m_iClip1 && pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
    {
        // HEV suit - indicate out of ammo condition
        pOwner->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
    }

    m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + 0.75;

    SetChargerState(CHARGER_STATE_DISCHARGE);
}

//-----------------------------------------------------------------------------
// Purpose: Hitscan sniper bolt - instant damage with visual effects
//-----------------------------------------------------------------------------
void CWeaponCrossbow::FireSniperBolt(void)
{
    if (m_iClip1 <= 0)
    {
        if (!m_bFireOnEmpty)
        {
            Reload();
        }
        else
        {
            WeaponSound(EMPTY);
            m_flNextPrimaryAttack = 0.15;
        }
        return;
    }

    CBasePlayer* pOwner = ToBasePlayer(GetOwner());
    if (pOwner == NULL)
        return;

    // Standard weapon firing setup like other hitscan weapons
    WeaponSound(SINGLE);
    WeaponSound(SPECIAL2);
    // No muzzle flash for sniper bolt to avoid giving away position
    SendWeaponAnim(ACT_VM_PRIMARYATTACK);
    pOwner->SetAnimation(PLAYER_ATTACK1);

    Vector vecSrc = pOwner->Weapon_ShootPosition();
    Vector vecAiming = pOwner->GetAutoaimVector(AUTOAIM_5DEGREES);

#ifndef CLIENT_DLL
    // REMOVED: No longer clean up previous cosmetic bolt to allow multiple like regular bolts
    // Let each cosmetic bolt manage its own lifetime with the 30-second timer
    
    // IMPROVED FIX: Do a comprehensive trace to determine what we actually hit
    // This accounts for lag compensation by using the same trace that FireBullets would use
    trace_t actualTrace;
    
    // Use lag compensation manager for accurate hit detection
    lagcompensation->StartLagCompensation(pOwner, pOwner->GetCurrentCommand());
    
    // Perform the actual bullet trace with lag compensation active
    UTIL_TraceLine(vecSrc, vecSrc + vecAiming * MAX_TRACE_LENGTH, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &actualTrace);
    
    // Check what we actually hit
    bool hitLivingTarget = false;
    if (actualTrace.fraction < 1.0f && actualTrace.m_pEnt)
    {
        CBaseEntity *pHitEnt = actualTrace.m_pEnt;
        // Check if we hit a player or NPC (living target)
        if (pHitEnt->IsPlayer() || pHitEnt->IsNPC())
        {
            hitLivingTarget = true;
        }
    }
    
    // Restore lag compensation state
    lagcompensation->FinishLagCompensation(pOwner);
    
    // Only create cosmetic bolt if we didn't hit a living target
    if (!hitLivingTarget)
    {
        // Now do a world-only trace to find the final resting place
        trace_t worldTrace;
        UTIL_TraceLine(vecSrc, vecSrc + vecAiming * MAX_TRACE_LENGTH, MASK_SOLID_BRUSHONLY, pOwner, COLLISION_GROUP_NONE, &worldTrace);
        
        if (worldTrace.fraction < 1.0f && !(worldTrace.surface.flags & SURF_SKY) && worldTrace.DidHitWorld())
        {
            // Create the cosmetic bolt on world geometry
            Vector vecBoltPos = worldTrace.endpos - (vecAiming * 6.0f);
            QAngle angAiming;
            VectorAngles(vecAiming, angAiming);
            CBaseEntity *pBolt = CreateEntityByName("prop_dynamic");
            if (pBolt)
            {
                pBolt->SetModel(BOLT_MODEL);
                pBolt->SetAbsOrigin(vecBoltPos);
                pBolt->SetAbsAngles(angAiming);
                pBolt->SetOwnerEntity(pOwner);
                pBolt->Spawn();
                
                // Prevent cosmetic bolt from casting shadows
                pBolt->AddEffects(EF_NOSHADOW);
                
                // Set removal timer
                pBolt->SetThink(&CBaseEntity::SUB_Remove);
                pBolt->SetNextThink(gpGlobals->curtime + 30.0f);
                
                // REMOVED: No longer store reference to allow multiple bolts
                // Each bolt manages its own 30-second lifetime automatically
                
                // Create sparks when bolt hits the wall
                CBroadcastRecipientFilter filter;
                Vector sparkDir = worldTrace.plane.normal;
                TE_Sparks(filter, 0.0f, &worldTrace.endpos, 1, 1, &sparkDir);
                
                // Play the bolt hit world sound
                CPASAttenuationFilter soundFilter(worldTrace.endpos);
                EmitSound(soundFilter, 0, "Weapon_Crossbow.BoltHitWorld", &worldTrace.endpos);
            }
        }
    }
#endif

    // Fire the bullet using player FireBullets - handles lag compensation, damage, and impact effects
    FireBulletsInfo_t info(1, vecSrc, vecAiming, vec3_origin, MAX_TRACE_LENGTH, m_iPrimaryAmmoType);
    info.m_pAttacker = pOwner;
    pOwner->FireBullets(info);

    m_iClip1--;
    SetBolt(1);
    pOwner->ViewPunch(QAngle(-2, 0, 0));

    if (!m_iClip1 && pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
    {
        pOwner->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
    }

    m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + 0.75;
    SetChargerState(CHARGER_STATE_DISCHARGE);
}

//-----------------------------------------------------------------------------
// Purpose: Sets whether or not the bolt is visible
//-----------------------------------------------------------------------------
inline void CWeaponCrossbow::SetBolt(int iSetting)
{
    int iBody = FindBodygroupByName("bolt");
    if (iBody != -1)
    {
        SetBodygroup(iBody, iSetting);
    }
    else if (GetOwner() && GetOwner()->IsPlayer())
    {
        CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
        if (pPlayer)
        {
            CBaseViewModel *pViewModel = pPlayer->GetViewModel();
            if (pViewModel)
            {
                pViewModel->SetBodygroup(iBody, iSetting);
            }
        }
    }
    else
    {
        m_nSkin = iSetting;
    }
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponCrossbow::Deploy( void )
{
    if ( m_iClip1 <= 0 )
    {
        SetBolt(1);
        return DefaultDeploy( (char*)GetViewModel(), (char*)GetWorldModel(), ACT_CROSSBOW_DRAW_UNLOADED, (char*)GetAnimPrefix() );
    }

    SetSkin( BOLT_SKIN_GLOW );

    SetBolt(0);

    return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSwitchingTo - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponCrossbow::Holster( CBaseCombatWeapon *pSwitchingTo )
{
    if ( m_bInZoom )
    {
        ToggleZoom();
    }

    SetChargerState( CHARGER_STATE_OFF );

    return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::ToggleZoom(void)
{
    CBasePlayer *pPlayer = ToBasePlayer(GetOwner());

    if (pPlayer == NULL)
        return;

#ifndef CLIENT_DLL
    float zoomTransitionTime = rbcl_smooth_zoom.GetBool() ? 0.2f : 0.0f;

    if (m_bInZoom)
    {
        if (pPlayer->SetFOV(this, 0, zoomTransitionTime))
        {
            if (m_hChargerSprite)
            {
                m_hChargerSprite->SetBrightness(80, 1.0f);
            }
            m_bInZoom = false;

            // Send a message to hide the scope only if rbcl_crossbow_scope is 1
            if (rbcl_crossbow_scope.GetInt() == 1)
            {
                CSingleUserRecipientFilter filter(pPlayer);
                UserMessageBegin(filter, "ShowScope");
                WRITE_BYTE(0);
                MessageEnd();
            }
        }
    }
    else
    {
        if (m_hChargerSprite)
        {
            m_hChargerSprite->SetBrightness(0);
        }
        if (pPlayer->SetFOV(this, 20, zoomTransitionTime))
        {
            m_bInZoom = true;

            // Send a message to show the scope only if rbcl_crossbow_scope is 1
            if (rbcl_crossbow_scope.GetInt() == 1)
            {
                CSingleUserRecipientFilter filter(pPlayer);
                UserMessageBegin(filter, "ShowScope");
                WRITE_BYTE(1);
                MessageEnd();
            }
        }
    }
#endif
}

#define    BOLT_TIP_ATTACHMENT    2

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::CreateChargerEffects( void )
{
#ifndef CLIENT_DLL
    CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

    if ( m_hChargerSprite != NULL )
        return;

    m_hChargerSprite = CSprite::SpriteCreate( CROSSBOW_GLOW_SPRITE, GetAbsOrigin(), false );

    if ( m_hChargerSprite )
    {
        m_hChargerSprite->SetAttachment( pOwner->GetViewModel(), BOLT_TIP_ATTACHMENT );
        m_hChargerSprite->SetTransparency( kRenderTransAdd, 255, 128, 0, 255, kRenderFxNoDissipation );
        m_hChargerSprite->SetBrightness( 0 );
        m_hChargerSprite->SetScale( 0.1f );
        m_hChargerSprite->TurnOff();
    }
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : skinNum - 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::SetSkin( int skinNum )
{
    CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
    
    if ( pOwner == NULL )
        return;

    CBaseViewModel *pViewModel = pOwner->GetViewModel();

    if ( pViewModel == NULL )
        return;

    pViewModel->m_nSkin = skinNum;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::DoLoadEffect( void )
{
    CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

    if ( pOwner == NULL )
        return;
    
    //Tony; change this up a bit; on the server, dispatch an effect but don't send it to the client who fires
    //on the client, create an effect either in the view model, or on the world model if first person.
    CEffectData    data;

    data.m_nAttachmentIndex = 1;
    data.m_vOrigin = pOwner->GetAbsOrigin();

    CPASFilter filter( data.m_vOrigin );

#ifdef GAME_DLL
    filter.RemoveRecipient( pOwner );
    data.m_nEntIndex = entindex();
    DispatchEffect( "CrossbowLoad", data, filter );
#else
    CBaseViewModel *pViewModel = pOwner->GetViewModel();
    if ( ShouldDrawUsingViewModel() && pViewModel != NULL )
        data.m_hEntity = pViewModel->GetRefEHandle();
    else
        data.m_hEntity = GetRefEHandle();
    DispatchEffect( "CrossbowLoad", data );
#endif

#ifndef CLIENT_DLL
    CSprite *pBlast = CSprite::SpriteCreate( CROSSBOW_GLOW_SPRITE2, GetAbsOrigin(), false );

    if ( pBlast )
    {
        pBlast->SetAttachment( this, 1 );
        pBlast->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNone );
        pBlast->SetBrightness( 128 );
        pBlast->SetScale( 0.2f );
        pBlast->FadeOutFromSpawn();
    }
#endif
    
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::SetChargerState( ChargerState_t state )
{
    // Make sure we're setup
    CreateChargerEffects();

    // Don't do this twice
    if ( state == m_nChargeState )
        return;

    m_nChargeState = state;

    switch( m_nChargeState )
    {
    case CHARGER_STATE_START_LOAD:
    
        IPredictionSystem::SuppressHostEvents(NULL);
        WeaponSound( SPECIAL1 );
        
        // Shoot some sparks and draw a beam between the two outer points
        SetSkin( BOLT_SKIN_GLOW );
        SetBolt(0);
        DoLoadEffect();
        
        break;
#ifndef CLIENT_DLL
    case CHARGER_STATE_START_CHARGE:
        {
            if ( m_hChargerSprite == NULL )
                break;
            m_hChargerSprite->SetBrightness( 32, 0.5f );
            m_hChargerSprite->SetScale( 0.025f, 0.5f );
            m_hChargerSprite->TurnOn();
        }

        break;

    case CHARGER_STATE_READY:
        {
            // Get fully charged
            if ( m_hChargerSprite == NULL )
                break;
            m_hChargerSprite->SetBrightness( 80, 1.0f );
            m_hChargerSprite->SetScale( 0.1f, 0.5f );
            m_hChargerSprite->TurnOn();
        }

        break;

    case CHARGER_STATE_DISCHARGE:
        {
            SetSkin( BOLT_SKIN_NORMAL );
            
            if ( m_hChargerSprite == NULL )
                break;
            
            m_hChargerSprite->SetBrightness( 0 );
            m_hChargerSprite->TurnOff();
        }

        break;
#endif
    case CHARGER_STATE_OFF:
        {
            SetSkin( BOLT_SKIN_NORMAL );

#ifndef CLIENT_DLL
            if ( m_hChargerSprite == NULL )
                break;
            
            m_hChargerSprite->SetBrightness( 0 );
            m_hChargerSprite->TurnOff();
#endif
        }
        break;

    default:
        break;
    }
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CWeaponCrossbow::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
    switch( pEvent->event )
    {
    case EVENT_WEAPON_THROW:
        SetChargerState( CHARGER_STATE_START_LOAD );
        break;

    case EVENT_WEAPON_THROW2:
        SetChargerState( CHARGER_STATE_START_CHARGE );
        break;
    
    case EVENT_WEAPON_THROW3:
        SetChargerState( CHARGER_STATE_READY );
        break;

    default:
        BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
        break;
    }
}

#endif

//-----------------------------------------------------------------------------
// Purpose: Set the desired activity for the weapon and its viewmodel counterpart
// Input  : iActivity - activity to play
//-----------------------------------------------------------------------------
bool CWeaponCrossbow::SendWeaponAnim( int iActivity )
{
    int newActivity = iActivity;

    // The last shot needs a non-loaded activity
    if ( ( newActivity == ACT_VM_IDLE ) && ( m_iClip1 <= 0 ) )
    {
        newActivity = ACT_VM_FIDGET;
    }

    //For now, just set the ideal activity and be done with it
    return BaseClass::SendWeaponAnim( newActivity );
}