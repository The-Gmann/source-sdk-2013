//========= Copyright (C) 2021, CSProMod Team, All rights reserved. =========//
//
// Purpose: Provide world light-related functions to the client
//
// Written: November 2011
// Author: Saul Rennison
//
//===========================================================================//

#ifndef WORLDLIGHT_H
#define WORLDLIGHT_H
#ifdef _WIN32
#pragma once
#endif

#include "igamesystem.h" // CAutoGameSystem

class Vector;
struct dworldlight_t;
class ConVar;

// External ConVar declarations
extern ConVar r_worldlight_mincastintensity;

//-----------------------------------------------------------------------------
// Purpose: Light cache entry for spatial optimization
//-----------------------------------------------------------------------------
struct LightCacheEntry_t
{
	Vector		vecPosition;		// Cached world position
	Vector		vecLightPos;		// Best light position found
	Vector		vecLightBrightness;	// Best light brightness found
	int			nCacheFrame;		// Frame when cached
	bool		bValidResult;		// Whether cache contains valid light result
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWorldLights : public CAutoGameSystem
{
public:
	CWorldLights();
	~CWorldLights() { Clear(); }

	bool GetBrightestLightSource( const Vector &vecPosition, Vector &vecLightPos, Vector &vecLightBrightness );

	// CAutoGameSystem overrides
	bool Init() OVERRIDE;
	void LevelInitPreEntity() OVERRIDE;
	void LevelShutdownPostEntity() OVERRIDE { Clear(); }

private:
	void Clear();
	
	// Light cache optimization functions
	int GetLightCacheIndex( const Vector &vecPosition );
	bool GetCachedLightResult( const Vector &vecPosition, Vector &vecLightPos, Vector &vecLightBrightness );
	void CacheLightResult( const Vector &vecPosition, const Vector &vecLightPos, const Vector &vecLightBrightness, bool bValidResult );
	void ClearLightCache();

private:
	int m_nWorldLights;
	dworldlight_t *m_pWorldLights;
	
	// Spatial light cache for performance optimization
	static const int LIGHT_CACHE_SIZE = 256;
	static const float LIGHT_CACHE_GRID_SIZE;
	LightCacheEntry_t m_LightCache[LIGHT_CACHE_SIZE];
	int m_nCacheFrame;
};

// Singleton accessor
extern CWorldLights *g_pWorldLights;

#endif // WORLDLIGHT_H