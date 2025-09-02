//========= Copyright Valve Corporation, All rights reserved. ============//

#include "cbase.h"
#include "hl2mp_gamerules.h"
#include "bot/hl2mp_bot.h"
#include "item_healthkit.h"
#include "bot/behavior/hl2mp_bot_get_health.h"
#include "hl2/func_recharge.h"
#include "hl2/item_healthkit.h"

extern ConVar bot_path_lookahead_range;

ConVar bot_health_critical_ratio( "bot_health_critical_ratio", "0.3", FCVAR_CHEAT );
ConVar bot_health_ok_ratio( "bot_health_ok_ratio", "0.65", FCVAR_CHEAT );
ConVar bot_health_search_near_range( "bot_health_search_near_range", "1000", FCVAR_CHEAT );
ConVar bot_health_search_far_range( "bot_health_search_far_range", "2000", FCVAR_CHEAT );
ConVar bot_health_charger_weave( "bot_health_charger_weave", "1", FCVAR_CHEAT );

ConVar bot_debug_health_scavenging( "bot_debug_health_scavenging", "0", FCVAR_CHEAT );
ConVar bot_debug_armor_scavenging( "bot_debug_armor_scavenging", "0", FCVAR_CHEAT );

//---------------------------------------------------------------------------------------------
// Helper function to check if an entity is a special megacharger
//---------------------------------------------------------------------------------------------
bool IsMegacharger( CBaseEntity *pEntity )
{
	return ( pEntity && FStrEq( STRING( pEntity->GetEntityName() ), "megacharger" ) );
}

//---------------------------------------------------------------------------------------------
// Helper function to get the maximum armor value for a given charger type
//---------------------------------------------------------------------------------------------
int GetMaxArmorForCharger( CBaseEntity *pCharger )
{
	if ( IsMegacharger( pCharger ) )
	{
		return 200; // Megachargers allow overcharge to 200
	}
	return 100; // Standard chargers max at 100
}

//---------------------------------------------------------------------------------------------
// Helper function to check if a charger can provide health
//---------------------------------------------------------------------------------------------
bool CanChargerProvideHealth( CBaseEntity *pCharger )
{
	// Megachargers can provide both health and armor
	if ( IsMegacharger( pCharger ) )
		return true;
	
	// Health chargers provide health
	if ( pCharger->ClassMatches( "item_healthcharger" ) || pCharger->ClassMatches( "func_healthcharger" ) )
		return true;
	
	return false;
}

//---------------------------------------------------------------------------------------------
// Helper function to check if a charger can provide armor
//---------------------------------------------------------------------------------------------
bool CanChargerProvideArmor( CBaseEntity *pCharger )
{
	// Megachargers can provide both health and armor
	if ( IsMegacharger( pCharger ) )
		return true;
	
	// Suit chargers and recharge stations provide armor
	if ( pCharger->ClassMatches( "*suitcharger" ) || pCharger->ClassMatches( "*recharge" ) )
		return true;
	
	return false;
}

//---------------------------------------------------------------------------------------------
class CHealthFilter : public INextBotFilter
{
public:
	CHealthFilter( CHL2MPBot *me, bool bFindChargers = true )
	{
		m_me = me;
		m_bFindChargers = bFindChargers;
	}

	bool IsSelected( const CBaseEntity *constCandidate ) const
	{
		if ( !constCandidate )
			return false;

		CBaseEntity *candidate = const_cast< CBaseEntity * >( constCandidate );

		CClosestHL2MPPlayer close( candidate );
		ForEachPlayer( close );

		// if the closest player to this candidate object is an enemy, don't use it
		if ( close.m_closePlayer && m_me->IsEnemy( close.m_closePlayer ) )
			return false;

		// ignore non-existent ammo to ensure we collect nearby existing ammo
		if ( candidate->IsEffectActive( EF_NODRAW ) )
			return false;

		if ( candidate->ClassMatches( "item_healthkit" ) )
		{
			// Only return healthkits if we actually need health
			return m_me->GetHealth() < m_me->GetMaxHealth();
		}

		if ( candidate->ClassMatches( "item_healthvial" ) )
		{
			// Only return healthvials if we actually need health
			return m_me->GetHealth() < m_me->GetMaxHealth();
		}

		// Add support for item_battery (suit batteries)
		if ( candidate->ClassMatches( "item_battery" ) )
		{
			// Check if bot needs armor (prioritize when under 50)
			CHL2MP_Player *pPlayer = ToHL2MPPlayer( m_me );
			if ( pPlayer && pPlayer->IsSuitEquipped() && pPlayer->ArmorValue() < 100 )
			{
				return true;
			}
			return false;
		}

		if ( m_bFindChargers )
		{
			// Check for megachargers first (special case)
			if ( IsMegacharger( candidate ) )
			{
				CHL2MP_Player *pPlayer = ToHL2MPPlayer( m_me );
				if ( !pPlayer )
					return false;
				
				bool needsHealth = pPlayer->GetHealth() < pPlayer->GetMaxHealth();
				bool needsArmor = pPlayer->IsSuitEquipped() && pPlayer->ArmorValue() < GetMaxArmorForCharger( candidate );
				
				// Megachargers are useful if bot needs either health or armor
				if ( needsHealth || needsArmor )
				{
					// Check if it has juice (assuming it's an item_suitcharger with special name)
					CNewRecharge* pMegaCharger = dynamic_cast< CNewRecharge* >( candidate );
					if ( pMegaCharger && pMegaCharger->GetJuice() > 0 )
					{
						return true;
					}
				}
				return false;
			}
			if ( candidate->ClassMatches( "item_healthcharger" ) )
			{
				// needs to have juice
				CNewWallHealth* pNewWallHealth = dynamic_cast< CNewWallHealth* >( candidate );
				if ( pNewWallHealth && pNewWallHealth->GetJuice() == 0 )
					return false;

				return true;
			}

			if ( candidate->ClassMatches( "func_healthcharger" ) )
			{
				// needs to have juice
				CWallHealth* pWallHealth = dynamic_cast< CWallHealth* >( candidate );
				if ( pWallHealth && pWallHealth->GetJuice() == 0 )
					return false;

				return true;
			}
			
			// Add suit charger support for bots
			if ( candidate->ClassMatches( "item_suitcharger" ) )
			{
				// Check if bot needs armor and suit charger has juice
				CHL2MP_Player *pPlayer = ToHL2MPPlayer( m_me );
				int maxArmor = GetMaxArmorForCharger( candidate );
				if ( pPlayer && pPlayer->IsSuitEquipped() && pPlayer->ArmorValue() < maxArmor )
				{
					CNewRecharge* pSuitCharger = dynamic_cast< CNewRecharge* >( candidate );
					if ( pSuitCharger && pSuitCharger->GetJuice() > 0 )
					{
						return true;
					}
				}
				return false;
			}
			
			if ( candidate->ClassMatches( "func_recharge" ) )
			{
				// Check if bot needs armor and suit charger has juice
				CHL2MP_Player *pPlayer = ToHL2MPPlayer( m_me );
				int maxArmor = GetMaxArmorForCharger( candidate );
				if ( pPlayer && pPlayer->IsSuitEquipped() && pPlayer->ArmorValue() < maxArmor )
				{
					CRecharge* pSuitCharger = dynamic_cast< CRecharge* >( candidate );
					if ( pSuitCharger && pSuitCharger->GetJuice() > 0 )
					{
						return true;
					}
				}
				return false;
			}
		}

		return false;
	}

	CHL2MPBot *m_me;
	bool m_bFindChargers;
};


//---------------------------------------------------------------------------------------------
Vector CHL2MPBotGetHealth::GetHealthKitPathOrigin( CBaseEntity *pHealthKit, bool bIsCharger )
{
	Vector target = pHealthKit->WorldSpaceCenter();
	if ( bIsCharger )
	{
		// chargers are usually on walls, so move in front of it instead
		if ( pHealthKit->ClassMatches( "func*" ) )
		{
			// brushes typically don't have angles, so we have to compromise by finding the nearest area
			CNavArea *pNearestArea = TheNavMesh->GetNearestNavArea( pHealthKit );

			Vector dir = (pNearestArea->GetCenter() - pHealthKit->WorldSpaceCenter());
			dir.z = 0;
			VectorNormalize( dir );

			target += (dir * pHealthKit->BoundingRadius()) + (dir * HalfHumanWidth);
		}
		else
		{
			Vector dir;
			pHealthKit->GetVectors( &dir, NULL, NULL );

			target += (dir * pHealthKit->BoundingRadius()) + (dir * HalfHumanWidth);
		}

		if ( bot_debug_health_scavenging.GetBool() )
		{
			NDebugOverlay::Cross3D( target, 5.0f, 0, 255, 0, true, 10.0f );
		}
	}

	return target;
}

//---------------------------------------------------------------------------------------------
// Version of CCollectReachableObjects which uses GetHealthKitPathOrigin()
//---------------------------------------------------------------------------------------------
class CCollectReachableHealthKits : public ISearchSurroundingAreasFunctor
{
public:
	CCollectReachableHealthKits( const CHL2MPBot *me, float maxRange, const CUtlVector< CHandle< CBaseEntity > > &potentialVector, CUtlVector< CHandle< CBaseEntity > > *collectionVector ) : m_potentialVector( potentialVector )
	{
		m_me = me;
		m_maxRange = maxRange;
		m_collectionVector = collectionVector;
	}

	virtual bool operator() ( CNavArea *area, CNavArea *priorArea, float travelDistanceSoFar )
	{
		// do any of the potential objects overlap this area?
		FOR_EACH_VEC( m_potentialVector, it )
		{
			CBaseEntity *obj = m_potentialVector[ it ];

			if ( obj && area->Contains( CHL2MPBotGetHealth::GetHealthKitPathOrigin( obj, V_strstr( obj->GetClassname(), "charger" ) || IsMegacharger( obj ) ) ) )
			{
				// reachable - keep it
				if ( !m_collectionVector->HasElement( obj ) )
				{
					m_collectionVector->AddToTail( obj );
				}
			}
		}
		return true;
	}

	virtual bool ShouldSearch( CNavArea *adjArea, CNavArea *currentArea, float travelDistanceSoFar )
	{
		if ( adjArea->IsBlocked( m_me->GetTeamNumber() ) )
		{
			return false;
		}

		if ( travelDistanceSoFar > m_maxRange )
		{
			// too far away
			return false;
		}

		return currentArea->IsContiguous( adjArea );
	}

	const CHL2MPBot *m_me;
	float m_maxRange;
	const CUtlVector< CHandle< CBaseEntity > > &m_potentialVector;
	CUtlVector< CHandle< CBaseEntity > > *m_collectionVector;
};


//---------------------------------------------------------------------------------------------
static CHL2MPBot *s_possibleBot = NULL;
static CHandle< CBaseEntity > s_possibleHealth = NULL;
static bool s_possibleIsCharger = NULL;
static int s_possibleFrame = 0;


//---------------------------------------------------------------------------------------------
/** 
 * Return true if this Action has what it needs to perform right now
 */
bool CHL2MPBotGetHealth::IsPossible( CHL2MPBot *me )
{
	VPROF_BUDGET( "CHL2MPBotGetHealth::IsPossible", "NextBot" );

	float healthRatio = (float)me->GetHealth() / (float)me->GetMaxHealth();
	CHL2MP_Player *pPlayer = ToHL2MPPlayer( me );
	
	// Simple armor check
	int currentArmor = (pPlayer && pPlayer->IsSuitEquipped()) ? pPlayer->ArmorValue() : 100;
	bool needsHealth = healthRatio < bot_health_ok_ratio.GetFloat() || (me->GetFlags() & FL_ONFIRE);
	bool needsArmor = currentArmor < 75;
	
	if ( !needsHealth && !needsArmor )
		return false;

	// Calculate search range based on urgency
	float urgency = MIN( healthRatio, currentArmor / 100.0f );
	if ( me->GetFlags() & FL_ONFIRE )
		urgency = 0.0f;

	
	// the more we are hurt, the farther we'll travel to get health
	float t = clamp( 1.0f - urgency, 0.0f, 1.0f );
	float searchRange = bot_health_search_far_range.GetFloat() + t * ( bot_health_search_near_range.GetFloat() - bot_health_search_far_range.GetFloat() );

	// Collect all health/armor items
	CUtlVector< CHandle< CBaseEntity > > hHealthKits;
	CBaseEntity* item = NULL;
	while ( ( item = gEntList.FindEntityByClassname( item, "item_health*" ) ) != NULL )
		hHealthKits.AddToTail( item );
	while ( ( item = gEntList.FindEntityByClassname( item, "func_health*" ) ) != NULL )
		hHealthKits.AddToTail( item );
	while ( ( item = gEntList.FindEntityByClassname( item, "item_battery" ) ) != NULL )
		hHealthKits.AddToTail( item );
	while ( ( item = gEntList.FindEntityByClassname( item, "item_suitcharger" ) ) != NULL )
		hHealthKits.AddToTail( item );
	while ( ( item = gEntList.FindEntityByClassname( item, "func_recharge" ) ) != NULL )
		hHealthKits.AddToTail( item );

	bool findChargers = true;
	if ( !needsHealth && !needsArmor && me->GetVisionInterface()->GetPrimaryKnownThreat( true ) )
		findChargers = false;

	CHealthFilter healthFilter( me, findChargers );
	CUtlVector< CHandle< CBaseEntity > > hReachableHealthKits;

	// can't call CHL2MPBot::SelectReachableObjects() because chargers don't use their worldspace center in pathfinding
	if ( me->GetLastKnownArea() )
	{		
		// filter candidate objects
		CUtlVector< CHandle< CBaseEntity > > filteredObjectVector;
		for( int i=0; i<hHealthKits.Count(); ++i )
		{
			if ( healthFilter.IsSelected( hHealthKits[i] ) )
				filteredObjectVector.AddToTail( hHealthKits[i] );
		}

		// only keep those that are reachable by us
		CCollectReachableHealthKits collector( me, searchRange, filteredObjectVector, &hReachableHealthKits );
		SearchSurroundingAreas( me->GetLastKnownArea(), collector );
	}

	// Simple priority selection: megachargers > health chargers > suit chargers > health kits > batteries
	CBaseEntity* closestHealth = NULL;
	for( int i = 0; i < hReachableHealthKits.Count(); ++i )
	{
		CBaseEntity* item = hReachableHealthKits[i];
		if ( !item ) continue;
		
		// Priority order based on immediate utility
		if ( IsMegacharger( item ) ) {
			closestHealth = item;
			break; // Highest priority
		}
		
		if ( !closestHealth ) {
			if ( needsHealth && item->ClassMatches( "*healthcharger" ) )
				closestHealth = item;
			else if ( needsArmor && (item->ClassMatches( "*suitcharger" ) || item->ClassMatches( "*recharge" )) )
				closestHealth = item;
			else if ( needsHealth && (item->ClassMatches( "item_healthkit" ) || item->ClassMatches( "item_healthvial" )) )
				closestHealth = item;
			else if ( needsArmor && item->ClassMatches( "item_battery" ) )
				closestHealth = item;
		}
	}

	if ( !closestHealth )
	{
		if ( me->IsDebugging( NEXTBOT_BEHAVIOR ) )
			Warning( "%3.2f: No health/armor items nearby\n", gpGlobals->curtime );
		return false;
	}

	s_possibleIsCharger = V_strstr( closestHealth->GetClassname(), "charger" ) || IsMegacharger( closestHealth );

	CHL2MPBotPathCost cost( me, FASTEST_ROUTE );
	PathFollower path;
	if ( !path.Compute( me, GetHealthKitPathOrigin( closestHealth, s_possibleIsCharger ), cost ) || !path.IsValid() || path.GetResult() != Path::COMPLETE_PATH )
	{
		if ( me->IsDebugging( NEXTBOT_BEHAVIOR ) )
			Warning( "%3.2f: No path to health!\n", gpGlobals->curtime );
		return false;
	}

	s_possibleBot = me;
	s_possibleHealth = closestHealth;
	s_possibleFrame = gpGlobals->framecount;

	return true;
}

//---------------------------------------------------------------------------------------------
ActionResult< CHL2MPBot >	CHL2MPBotGetHealth::OnStart( CHL2MPBot *me, Action< CHL2MPBot > *priorAction )
{
	VPROF_BUDGET( "CHL2MPBotGetHealth::OnStart", "NextBot" );

	m_path.SetMinLookAheadDistance( me->GetDesiredPathLookAheadRange() );

	// if IsPossible() has already been called, use its cached data
	if ( s_possibleFrame != gpGlobals->framecount || s_possibleBot != me )
	{
		if ( !IsPossible( me ) || s_possibleHealth == NULL )
		{
			return Done( "Can't get health" );
		}
	}

	m_healthKit = s_possibleHealth;
	m_isGoalCharger = s_possibleIsCharger;

	CHL2MPBotPathCost cost( me, SAFEST_ROUTE );
	if ( !m_path.Compute( me, GetHealthKitPathOrigin( m_healthKit, m_isGoalCharger ), cost ) )
	{
		return Done( "No path to health!" );
	}

	return Continue();
}


//---------------------------------------------------------------------------------------------
ActionResult< CHL2MPBot >	CHL2MPBotGetHealth::Update( CHL2MPBot *me, float interval )
{
	if ( m_healthKit == NULL || ( m_healthKit->IsEffectActive( EF_NODRAW ) ) )
	{
		return Done( "Health kit I was going for has been taken" );
	}

	// Declare pPlayer once at the beginning to avoid redefinition
	CHL2MP_Player *pPlayer = ToHL2MPPlayer( me );

	if ( me->GetHealth() >= me->GetMaxHealth() )
	{
		// Check if we also need armor - don't finish if we still need armor and current item provides it
		int maxArmor = GetMaxArmorForCharger( m_healthKit );
		if ( pPlayer && pPlayer->IsSuitEquipped() && pPlayer->ArmorValue() < maxArmor )
		{
			// Still need armor - only continue if current item can provide armor
			if ( m_healthKit && (m_healthKit->ClassMatches( "item_battery" ) || 
								  CanChargerProvideArmor( m_healthKit )) )
			{
				// Continue to get armor
			}
			else
			{
				return Done( "Health full but still need armor from different item" );
			}
		}
		else
		{
			return Done( "I've been healed" );
		}
	}

	// If going for a health item (not charger/battery) and health is full, stop
	if ( !m_isGoalCharger && !m_healthKit->ClassMatches( "item_battery" ) )
	{
		if ( me->GetHealth() >= me->GetMaxHealth() )
			return Done( "Health is full, no need for health items" );
	}

	if ( HL2MPRules()->IsTeamplay() )
	{
		// if the closest player to the item we're after is an enemy, give up

		CClosestHL2MPPlayer close( m_healthKit );
		ForEachPlayer( close );
		if ( close.m_closePlayer && me->IsEnemy( close.m_closePlayer ) )
			return Done( "An enemy is closer to it" );
	}

	if ( m_isGoalCharger )
	{
		// we need to get near and wait, not try to run over
		const float nearRange = PLAYER_USE_RADIUS;
		Vector vecDir = (me->EyePosition() - m_healthKit->WorldSpaceCenter());
		if ( vecDir.IsLengthLessThan( nearRange ) )
		{
			// if we're carrying a prop, drop it
			if ( me->Physcannon_GetHeldProp() )
				me->PressAltFireButton( 0.1f );

			if ( me->GetVisionInterface()->IsLineOfSightClearToEntity( m_healthKit ) )
			{
				int maxArmor = GetMaxArmorForCharger( m_healthKit );
				bool healthFull = (me->GetHealth() >= me->GetMaxHealth());
				bool armorFull = !pPlayer || !pPlayer->IsSuitEquipped() || (pPlayer->ArmorValue() >= maxArmor);
				
				// Check if we're done - stop when we can't get any more from this charger
				bool canGetHealth = CanChargerProvideHealth( m_healthKit ) && !healthFull;
				bool canGetArmor = CanChargerProvideArmor( m_healthKit ) && !armorFull;
				
				if ( !canGetHealth && !canGetArmor )
					return Done( "Can't get any more from this charger" );
				
				// Check if this charger can't provide what we need
				if ( !healthFull && !CanChargerProvideHealth( m_healthKit ) && armorFull )
					return Done( "Need health but this charger only provides armor" );
				if ( !armorFull && !CanChargerProvideArmor( m_healthKit ) && healthFull )
					return Done( "Need armor but this charger only provides health" );

				// Check if charger is empty
				CNewWallHealth* pNewWallHealth = dynamic_cast< CNewWallHealth* >( m_healthKit.Get() );
				if ( pNewWallHealth && pNewWallHealth->GetJuice() == 0 )
					return Done( "Charger is out of juice!" );

				CWallHealth* pWallHealth = dynamic_cast< CWallHealth* >( m_healthKit.Get() );
				if ( pWallHealth && pWallHealth->GetJuice() == 0 )
					return Done( "Charger is out of juice!" );
				
				CNewRecharge* pNewRecharge = dynamic_cast< CNewRecharge* >( m_healthKit.Get() );
				if ( pNewRecharge && pNewRecharge->GetJuice() == 0 )
					return Done( "Suit Charger is out of juice!" );
				
				CRecharge* pRecharge = dynamic_cast< CRecharge* >( m_healthKit.Get() );
				if ( pRecharge && pRecharge->GetJuice() == 0 )
					return Done( "Suit Charger is out of juice!" );

				// Don't wait in combat unless critically low
				float healthRatio = (float)me->GetHealth() / (float)me->GetMaxHealth();
				bool criticalHealth = healthRatio < 0.3f;
				bool criticalArmor = pPlayer && pPlayer->IsSuitEquipped() && pPlayer->ArmorValue() < 50;
				
				const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
				if ( !criticalHealth && !criticalArmor && threat && me->IsLineOfSightClear( threat->GetEntity(), CHL2MPBot::IGNORE_ACTORS ) )
					return Done( "No time to wait for more health/armor, I must fight" );
				
				if ( !m_usingCharger )
				{
					m_usingCharger = true;
					me->ReleaseUseButton();
					return Continue();
				}

				if ( me->GetDifficulty() >= CHL2MPBot::EXPERT && bot_health_charger_weave.GetBool() )
				{
					// expert bots go back and forth to avoid getting hit
					Vector right;
					m_healthKit->GetVectors( NULL, &right, NULL );

					Vector2D toMe = (me->GetAbsOrigin() - m_healthKit->GetAbsOrigin()).AsVector2D();
					Vector2DNormalize( toMe );

					// go towards the center
					float dot = right.AsVector2D().Dot( toMe );
					if ( dot < 0 )
						me->PressLeftButton( 0.1f );
					else
						me->PressRightButton( 0.1f );
				}

				// begin looking at the charger, then +USE it
				me->GetBodyInterface()->AimHeadTowards( m_healthKit, IBody::CRITICAL, 0.5f, NULL, "Using health charger" );
				if ( me->GetBodyInterface()->IsHeadAimingOnTarget() )
				{
					// Check again if we're done while using charger
					bool canGetHealth = CanChargerProvideHealth( m_healthKit ) && (me->GetHealth() < me->GetMaxHealth());
					bool canGetArmor = CanChargerProvideArmor( m_healthKit ) && pPlayer && pPlayer->IsSuitEquipped() && (pPlayer->ArmorValue() < maxArmor);
					
					if ( !canGetHealth && !canGetArmor )
					{
						me->ReleaseUseButton();
						return Done( "Charging complete" );
					}
					
					me->PressUseButton( 0.5f );
				}
				else
					me->ReleaseUseButton();

				return Continue();
			}
		}

		if ( m_usingCharger )
		{
			// something may have obstructed or moved us away
			m_usingCharger = false;
			me->ReleaseUseButton();
		}

		// Don't keep going if in combat unless critically low
		float healthRatio = (float)me->GetHealth() / (float)me->GetMaxHealth();
		bool criticalHealth = healthRatio < 0.3f;
		bool criticalArmor = pPlayer && pPlayer->IsSuitEquipped() && pPlayer->ArmorValue() < 50;

		if ( !criticalHealth && !criticalArmor && me->GetVisionInterface()->GetPrimaryKnownThreat( true ) )
			return Done( "No time to reach charger, I must fight" );
	}

	if ( !m_path.IsValid() )
	{
		// this can occur if we overshoot the health kit's location
		// because it is momentarily gone
		CHL2MPBotPathCost cost( me, SAFEST_ROUTE );
		if ( !m_path.Compute( me, GetHealthKitPathOrigin( m_healthKit, m_isGoalCharger ), cost ) )
		{
			return Done( "No path to health!" );
		}
	}

	m_path.Update( me );

	// may need to switch weapons (ie: engineer holding toolbox now needs to heal and defend himself)
	const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
	me->EquipBestWeaponForThreat( threat );

	return Continue();
}


//---------------------------------------------------------------------------------------------
ActionResult< CHL2MPBot >	CHL2MPBotGetHealth::OnSuspend( CHL2MPBot* me, Action< CHL2MPBot >* interruptingAction )
{
	if ( m_usingCharger )
	{
		m_usingCharger = false;
		me->ReleaseUseButton();
	}

	return Continue();
}


//---------------------------------------------------------------------------------------------
void CHL2MPBotGetHealth::OnEnd( CHL2MPBot* me, Action< CHL2MPBot >* nextAction )
{
	if ( m_usingCharger )
	{
		m_usingCharger = false;
		me->ReleaseUseButton();
	}
}


//---------------------------------------------------------------------------------------------
EventDesiredResult< CHL2MPBot > CHL2MPBotGetHealth::OnStuck( CHL2MPBot *me )
{
	return TryDone( RESULT_CRITICAL, "Stuck trying to reach health kit" );
}


//---------------------------------------------------------------------------------------------
EventDesiredResult< CHL2MPBot > CHL2MPBotGetHealth::OnMoveToSuccess( CHL2MPBot *me, const Path *path )
{
	return TryContinue();
}


//---------------------------------------------------------------------------------------------
EventDesiredResult< CHL2MPBot > CHL2MPBotGetHealth::OnMoveToFailure( CHL2MPBot *me, const Path *path, MoveToFailureType reason )
{
	return TryDone( RESULT_CRITICAL, "Failed to reach health kit" );
}


//---------------------------------------------------------------------------------------------
// We are always hurrying if we need to collect health
QueryResultType CHL2MPBotGetHealth::ShouldHurry( const INextBot *me ) const
{
	return ANSWER_YES;
}
