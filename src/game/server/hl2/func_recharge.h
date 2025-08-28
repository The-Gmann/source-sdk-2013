//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Header file for recharge entities (func_recharge and item_suitcharger)
//
//=============================================================================

#ifndef FUNC_RECHARGE_H
#define FUNC_RECHARGE_H
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"
#include "baseanimating.h"

//-----------------------------------------------------------------------------
// Purpose: Suit recharge station (func_recharge)
//-----------------------------------------------------------------------------
class CRecharge : public CBaseToggle
{
public:
	DECLARE_CLASS( CRecharge, CBaseToggle );

	void Spawn( );
	bool CreateVPhysics();
	int DrawDebugTextOverlays(void);
	void Off(void);
	void Recharge(void);
	bool KeyValue( const char *szKeyName, const char *szValue );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int	ObjectCaps( void ) { return (BaseClass::ObjectCaps() | FCAP_CONTINUOUS_USE); }
	
	// For bot access
	int GetJuice() const { return m_iJuice; }

private:
	void InputRecharge( inputdata_t &inputdata );
	
	float MaxJuice() const;
	void UpdateJuice( int newJuice );

	DECLARE_DATADESC();

	float	m_flNextCharge; 
	int		m_iReactivate ; // DeathMatch Delay until reactvated
	int		m_iJuice;
	int		m_iOn;			// 0 = off, 1 = startup, 2 = going
	float   m_flSoundTime;
	
	int		m_nState;
	
	COutputFloat m_OutRemainingCharge;
	COutputEvent m_OnHalfEmpty;
	COutputEvent m_OnEmpty;
	COutputEvent m_OnFull;
	COutputEvent m_OnPlayerUse;
};

//-----------------------------------------------------------------------------
// Purpose: Animated suit recharge station (item_suitcharger)
//-----------------------------------------------------------------------------
class CNewRecharge : public CBaseAnimating
{
public:
	DECLARE_CLASS( CNewRecharge, CBaseAnimating );

	void Spawn( );
	bool CreateVPhysics();
	int DrawDebugTextOverlays(void);
	void Off(void);
	void Recharge(void);
	bool KeyValue( const char *szKeyName, const char *szValue );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int	ObjectCaps( void ) { return (BaseClass::ObjectCaps() | m_iCaps ); }

	void SetInitialCharge( void );
	
	// For bot access
	int GetJuice() const { return m_iJuice; }

private:
	void InputRecharge( inputdata_t &inputdata );
	void InputSetCharge( inputdata_t &inputdata );
	float MaxJuice() const;
	void UpdateJuice( int newJuice );
	void Precache( void );

	DECLARE_DATADESC();

	float	m_flNextCharge; 
	int		m_iReactivate ; // DeathMatch Delay until reactvated
	int		m_iJuice;
	int		m_iOn;			// 0 = off, 1 = startup, 2 = going
	float   m_flSoundTime;
	
	int		m_nState;
	int		m_iCaps;
	int		m_iMaxJuice;
	
	COutputFloat m_OutRemainingCharge;
	COutputEvent m_OnHalfEmpty;
	COutputEvent m_OnEmpty;
	COutputEvent m_OnFull;
	COutputEvent m_OnPlayerUse;

	virtual void StudioFrameAdvance ( void );
	float m_flJuice;
};

#endif // FUNC_RECHARGE_H