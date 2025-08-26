//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Long Jump Module for HL2MP
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "player.h"
#include "gamerules.h"
#include "items.h"
#include "hl2mp_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CItemLongJump : public CItem
{
public:
	DECLARE_CLASS( CItemLongJump, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetModel( "models/w_longjump.mdl" );
		BaseClass::Spawn( );
	}
	
		void Precache( void )
	{
		PrecacheModel("models/w_longjump.mdl");
		PrecacheScriptSound("Player.LongJump");
		PrecacheScriptSound("Player.LongJumpPickup");
	}
	
	bool MyTouch( CBasePlayer *pPlayer )
	{
		CHL2MP_Player *pHL2Player = dynamic_cast<CHL2MP_Player*>(pPlayer);
		if ( !pHL2Player )
			return false;

		if ( pHL2Player->m_fLongJump )
		{
			return false;
		}

		if ( pHL2Player->IsSuitEquipped() )
		{
			pHL2Player->m_fLongJump = true; // player now has longjump module

			CSingleUserRecipientFilter user( pHL2Player );
			user.MakeReliable();

			UserMessageBegin( user, "ItemPickup" );
				WRITE_STRING( GetClassname() );
			MessageEnd();

			// Play the authentic HL1 longjump pickup sound (!HEV_A1)
			UTIL_EmitSoundSuit( pHL2Player->edict(), "Player.LongJumpPickup" );

			return true;		
		}
		return false;
	}
};

LINK_ENTITY_TO_CLASS( item_longjump, CItemLongJump );