//========= Copyright Valve Corporation, All rights reserved. ============//
//----------------------------------------------------------------------------------------------------------------

#include "cbase.h"
#include "hl2mp_bot_manager.h"
#include "filesystem.h"

#include "Player/NextBotPlayer.h"
#include "team.h"
#include "hl2mp_bot.h"
#include "hl2mp_gamerules.h"


//----------------------------------------------------------------------------------------------------------------

// Creates and sets CHL2MPBotManager as the NextBotManager singleton
static CHL2MPBotManager sHL2MPBotManager;

ConVar bot_difficulty( "bot_difficulty", "2", FCVAR_NONE, "Defines the skill of bots joining the game.  Values are: 0=easy, 1=normal, 2=hard, 3=expert." );
ConVar bot_quota( "bot_quota", "0", FCVAR_NONE, "Determines the total number of bots in the game." );
ConVar bot_quota_mode( "bot_quota_mode", "normal", FCVAR_NONE, "Determines the type of quota.\nAllowed values: 'normal', 'fill', and 'match'.\nIf 'fill', the server will adjust bots to keep N players in the game, where N is bot_quota.\nIf 'match', the server will maintain a 1:N ratio of humans to bots, where N is bot_quota." );
ConVar bot_join_after_player( "bot_join_after_player", "1", FCVAR_NONE, "If nonzero, bots wait until a player joins before entering the game." );
ConVar bot_auto_vacate( "bot_auto_vacate", "1", FCVAR_NONE, "If nonzero, bots will automatically leave to make room for human players." );
ConVar bot_offline_practice( "bot_offline_practice", "0", FCVAR_NONE, "Tells the server that it is in offline practice mode." );
ConVar bot_melee_only( "bot_melee_only", "0", FCVAR_GAMEDLL, "If nonzero, bots will only use melee weapons" );
ConVar bot_gravgun_only( "bot_gravgun_only", "0", FCVAR_GAMEDLL, "If nonzero, bots will only use gravity gun weapon" );
ConVar bot_quota_debug( "bot_quota_debug", "0", FCVAR_CHEAT, "Enable debug output for bot quota system" );

extern const char *GetRandomBotName( void );
extern void CreateBotName( int iTeam, CHL2MPBot::DifficultyType skill, char* pBuffer, int iBufferSize );
extern void ReleaseBotName(const char* name);

// Dynamic bot names list loaded from scripts/bot_names.cfg
static CUtlVector<CUtlString> g_BotNamesList;
static CUtlVector<int> g_AvailableNameIndices; // Tracks which names haven't been used yet

// Fallback bot names if file cannot be loaded
static const char *g_ppszFallbackBotNames[] = 
{
	// HL2 Main Characters
	"Gordon Freeman", "Alyx Vance", "Eli Vance", "Isaac Kleiner", "Barney Calhoun",
	"Judith Mossman", "Wallace Breen", "Dog", "Father Grigori", "Arne Magnusson",
	
	// Citizens and Rebels
	"Odessa Cubbage", "Winston", "Sheckley", "Griggs", "Lars", "Lazlo",
	"Captain Vance", "Vortigaunt", "The G-Man", "Civil Protection",
	
	// Half-Life Mods & Extended Universe
	"Adrian Shephard", "Otis Laurey", "Gina Cross", "Colette Green", "Damien Reeves",
	"Walter Bennet", "Ivan the Space Biker", "Kate", "Richard Keller", "Dr. Rosenberg",
	"Nick", "Pit Drone Wrangler", "Race X Survivor", "HECU Marine", "Black Ops Agent",
	"Freeman's Mind", "Civil Protection 01", "Metrocop Beta", "Rebel Medic", "City Scanner",
	
	// Opposing Force References
	"Corporal Shephard", "Sergeant Major", "Drill Instructor", "Bootcamp Survivor",
	"Military Police", "Hazmat Specialist", "Gene Worm Hunter", "Portal Storm Witness",
	"Displacer Cannon User", "Spore Launcher Expert", "Shock Trooper", "Alien Grunt Slayer",
	
	// Blue Shift & Decay References
	"Security Guard Calhoun", "Dr. Cross", "Dr. Green", "Maintenance Worker",
	"Sector C Guard", "Anomalous Materials", "Lambda Complex", "Xen Borderworld",
	"Resonance Cascade", "Black Mesa East", "Nova Prospekt", "The Citadel",
	
	// Community Mod References
	"Sweet Half-Life", "Azure Sheep", "Point of View", "They Hunger",
	"USS Darkstar", "Wanted!", "Half-Quake", "Poke646", "Science and Industry",
	"Sven Coop", "Natural Selection", "Day of Defeat", "Ricochet", "Deathmatch Classic",
	
	// HL2 Episode Characters
	"Advisor Victim", "Strider Pilot", "Hunter Synth", "Combine Elite",
	"Zombie Torso", "Antlion Worker", "Vortigaunt Elder", "Resistance Fighter",
	"White Forest Base", "Episode Three", "Borealis Crew", "Aperture Scientist"
};

//----------------------------------------------------------------------------------------------------------------
// Load bot names from scripts/bot_names.cfg or fall back to hardcoded names
void LoadBotNamesFromFile()
{
	g_BotNamesList.Purge();
	
	// Try to load from scripts/bot_names.cfg first
	FileHandle_t file = filesystem->Open("scripts/bot_names.cfg", "r", "MOD");
	if (file == FILESYSTEM_INVALID_HANDLE)
	{
		// Try .txt extension if .cfg doesn't exist
		file = filesystem->Open("scripts/bot_names.txt", "r", "MOD");
	}
	
	if (file != FILESYSTEM_INVALID_HANDLE)
	{
		// File found, read line by line
		char line[256];
		while (filesystem->ReadLine(line, sizeof(line), file))
		{
			// Remove newline and whitespace
			int len = V_strlen(line);
			while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r' || line[len-1] == ' ' || line[len-1] == '\t'))
			{
				line[--len] = 0;
			}
			
			// Skip empty lines and comments
			if (len > 0 && line[0] != '/' && line[0] != '#')
			{
				g_BotNamesList.AddToTail(CUtlString(line));
			}
		}
		filesystem->Close(file);
		
		if (g_BotNamesList.Count() > 0)
		{
			DevMsg("[BOT_NAMES] Loaded %d bot names from file\n", g_BotNamesList.Count());
			return;
		}
	}
	
	// Fallback to hardcoded names if file loading failed
	DevMsg("[BOT_NAMES] Using fallback hardcoded bot names\n");
	for (int i = 0; i < ARRAYSIZE(g_ppszFallbackBotNames); i++)
	{
		g_BotNamesList.AddToTail(CUtlString(g_ppszFallbackBotNames[i]));
	}
}

//----------------------------------------------------------------------------------------------------------------
// Get a random bot name from our list, avoiding duplicates until all names are used
const char* GetUniqueBotName()
{
	// Load names on first use
	if (g_BotNamesList.Count() == 0)
	{
		LoadBotNamesFromFile();
	}
	
	if (g_BotNamesList.Count() == 0)
	{
		// Emergency fallback
		return "Bot";
	}
	
	// Initialize available indices if empty or if we've used all names
	if (g_AvailableNameIndices.Count() == 0)
	{
		// Refill the available names list with all indices
		for (int i = 0; i < g_BotNamesList.Count(); i++)
		{
			g_AvailableNameIndices.AddToTail(i);
		}
		
		if (g_AvailableNameIndices.Count() > 0)
		{
			DevMsg("[BOT_NAMES] Refreshed available names pool with %d names\n", g_AvailableNameIndices.Count());
		}
	}
	
	if (g_AvailableNameIndices.Count() == 0)
	{
		// This shouldn't happen, but just in case
		return "Bot";
	}
	
	// Pick a random index from the available names
	int randomIndex = RandomInt(0, g_AvailableNameIndices.Count() - 1);
	int nameIndex = g_AvailableNameIndices[randomIndex];
	
	// Get the name and remove this index from available list
	const char* name = g_BotNamesList[nameIndex].String();
	g_AvailableNameIndices.Remove(randomIndex);
	
	return name;
}

//----------------------------------------------------------------------------------------------------------------
// Make a bot name available again when a bot disconnects
void ReleaseBotName(const char* name)
{
	if (!name)
		return;
	
	// Find the index of this name in our list
	for (int i = 0; i < g_BotNamesList.Count(); i++)
	{
		if (V_strcmp(g_BotNamesList[i].String(), name) == 0)
		{
			// Check if this index is already available
			bool alreadyAvailable = false;
			for (int j = 0; j < g_AvailableNameIndices.Count(); j++)
			{
				if (g_AvailableNameIndices[j] == i)
				{
					alreadyAvailable = true;
					break;
				}
			}
			
			// Add it back to available list if not already there
			if (!alreadyAvailable)
			{
				g_AvailableNameIndices.AddToTail(i);
				DevMsg("[BOT_NAMES] Released name '%s' back to available pool\n", name);
			}
			break;
		}
	}
}

static bool UTIL_KickBotFromTeam( int kickTeam )
{
	int i;

	// try to kick a dead bot first
	for ( i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		CHL2MPBot* pBot = dynamic_cast<CHL2MPBot*>(pPlayer);

		if (pBot == NULL)
			continue;

		if ( pBot->HasAttribute( CHL2MPBot::QUOTA_MANANGED ) == false )
			continue;

		if ( ( pPlayer->GetFlags() & FL_FAKECLIENT ) == 0 )
			continue;

		if ( !pPlayer->IsAlive() && pPlayer->GetTeamNumber() == kickTeam )
		{
			// Release the bot's name back to the available pool
			ReleaseBotName( pPlayer->GetPlayerName() );
			
			// its a bot on the right team - kick it
			engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", pPlayer->GetUserID() ) );

			return true;
		}
	}

	// no dead bots, kick any bot on the given team
	for ( i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		CHL2MPBot* pBot = dynamic_cast<CHL2MPBot*>(pPlayer);

		if (pBot == NULL)
			continue;

		if ( pBot->HasAttribute( CHL2MPBot::QUOTA_MANANGED ) == false )
			continue;

		if ( ( pPlayer->GetFlags() & FL_FAKECLIENT ) == 0 )
			continue;

		if (pPlayer->GetTeamNumber() == kickTeam)
		{
			// Release the bot's name back to the available pool
			ReleaseBotName( pPlayer->GetPlayerName() );
			
			// its a bot on the right team - kick it
			engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", pPlayer->GetUserID() ) );

			return true;
		}
	}

	return false;
}

//----------------------------------------------------------------------------------------------------------------

CHL2MPBotManager::CHL2MPBotManager()
	: NextBotManager()
	, m_flNextPeriodicThink( 0 )
{
	NextBotManager::SetInstance( this );
}


//----------------------------------------------------------------------------------------------------------------
CHL2MPBotManager::~CHL2MPBotManager()
{
	NextBotManager::SetInstance( NULL );
}


//----------------------------------------------------------------------------------------------------------------
void CHL2MPBotManager::OnMapLoaded( void )
{
	NextBotManager::OnMapLoaded();

	ClearStuckBotData();
	
	// Check bot quota immediately on map load
	// This ensures bots are added if quota was already set before server start
	m_flNextPeriodicThink = gpGlobals->curtime + 1.0f; // Give server 1 second to initialize
}


//----------------------------------------------------------------------------------------------------------------
void CHL2MPBotManager::Update()
{
	// Check if it's time for the initial bot quota check
	if ( m_flNextPeriodicThink > 0.0f && gpGlobals->curtime >= m_flNextPeriodicThink )
	{
		m_flNextPeriodicThink = 0.0f; // Reset so we don't do this again
		if ( bot_quota_debug.GetBool() )
		{
			DevMsg( "[BOT_QUOTA] Initial bot quota check on map load\n" );
		}
	}
	
	MaintainBotQuota();

	DrawStuckBotData();

	NextBotManager::Update();
}


//----------------------------------------------------------------------------------------------------------------
bool CHL2MPBotManager::RemoveBotFromTeamAndKick( int nTeam )
{
	CUtlVector< CHL2MP_Player* > vecCandidates;

	// Gather potential candidates
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );

		if ( pPlayer == NULL )
			continue;

		if ( FNullEnt( pPlayer->edict() ) )
			continue;

		if ( !pPlayer->IsConnected() )
			continue;

		CHL2MPBot* pBot = dynamic_cast<CHL2MPBot*>( pPlayer );
		if ( pBot && pBot->HasAttribute( CHL2MPBot::QUOTA_MANANGED ) )
		{
			if ( pBot->GetTeamNumber() == nTeam )
			{
				vecCandidates.AddToTail( pPlayer );
			}
		}
	}
	
	CHL2MP_Player *pVictim = NULL;
	if ( vecCandidates.Count() > 0 )
	{
		// first look for bots that are currently dead
		FOR_EACH_VEC( vecCandidates, i )
		{
			CHL2MP_Player *pPlayer = vecCandidates[i];
			if ( pPlayer && !pPlayer->IsAlive() )
			{
				pVictim = pPlayer;
				break;
			}
		}

		// if we didn't fine one, try to kick anyone on the team
		if ( !pVictim )
		{
			FOR_EACH_VEC( vecCandidates, i )
			{
				CHL2MP_Player *pPlayer = vecCandidates[i];
				if ( pPlayer )
				{
					pVictim = pPlayer;
					break;
				}
			}
		}
	}

	if ( pVictim )
	{
		if ( pVictim->IsAlive() )
		{
			pVictim->CommitSuicide();
		}
		UTIL_KickBotFromTeam( TEAM_UNASSIGNED );
		return true;
	}

	return false;
}

//----------------------------------------------------------------------------------------------------------------
void CHL2MPBotManager::MaintainBotQuota()
{
	if ( TheNavMesh->IsGenerating() )
		return;

	if ( g_fGameOver )
		return;

	// new players can't spawn immediately after the round has been going for some time
	if ( !GameRules() )
		return;

	// if it is not time to do anything...
	if ( gpGlobals->curtime < m_flNextPeriodicThink )
		return;

	// think every quarter second
	m_flNextPeriodicThink = gpGlobals->curtime + 0.25f;

	// don't add bots until local player has been registered, to make sure he's player ID #1
	if ( !engine->IsDedicatedServer() )
	{
		CBasePlayer *pPlayer = UTIL_GetListenServerHost();
		if ( !pPlayer )
			return;
	}

	// Count current players and bots with proper team assignments
	int nConnectedClients = 0;
	int nQuotaManagedBots = 0;
	int nQuotaManagedBotsOnGameTeams = 0;
	int nHumanPlayersOnGameTeams = 0;
	int nSpectators = 0;

	// Track team populations for balance
	int nRebelPlayers = 0;
	int nCombinePlayers = 0;
	int nRebelBots = 0;
	int nCombineBots = 0;
	int nUnassignedBots = 0;  // Track unassigned bots for deathmatch
	int nUnassignedPlayers = 0;  // Track unassigned players for deathmatch

	bool bIsTeamplay = HL2MPRules()->IsTeamplay();

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );

		if ( pPlayer == NULL || FNullEnt( pPlayer->edict() ) || !pPlayer->IsConnected() )
			continue;

		nConnectedClients++;

		CHL2MPBot* pBot = dynamic_cast<CHL2MPBot*>( pPlayer );
		bool isQuotaManagedBot = (pBot && pBot->HasAttribute( CHL2MPBot::QUOTA_MANANGED ));

		int teamNumber = pPlayer->GetTeamNumber();

		if ( isQuotaManagedBot )
		{
			nQuotaManagedBots++;
			
			// In teamplay mode, only rebels/combine count as "game teams"
			// In deathmatch mode, unassigned also counts as a "game team"
			if ( bIsTeamplay )
			{
				if ( teamNumber == TEAM_REBELS || teamNumber == TEAM_COMBINE )
				{
					nQuotaManagedBotsOnGameTeams++;
				}
			}
			else
			{
				// Deathmatch mode - unassigned is a valid game team
				if ( teamNumber == TEAM_UNASSIGNED || teamNumber == TEAM_REBELS || teamNumber == TEAM_COMBINE )
				{
					nQuotaManagedBotsOnGameTeams++;
				}
			}

			// Track bot team populations
			if ( teamNumber == TEAM_REBELS )
				nRebelBots++;
			else if ( teamNumber == TEAM_COMBINE )
				nCombineBots++;
			else if ( teamNumber == TEAM_UNASSIGNED )
				nUnassignedBots++;
		}
		else
		{
			// Human player
			if ( bIsTeamplay )
			{
				if ( teamNumber == TEAM_REBELS || teamNumber == TEAM_COMBINE )
				{
					nHumanPlayersOnGameTeams++;
				}
			}
			else
			{
				// Deathmatch mode - unassigned is a valid game team
				if ( teamNumber == TEAM_UNASSIGNED || teamNumber == TEAM_REBELS || teamNumber == TEAM_COMBINE )
				{
					nHumanPlayersOnGameTeams++;
				}
			}
			
			if ( teamNumber == TEAM_SPECTATOR )
			{
				nSpectators++;
			}

			// Track human team populations
			if ( teamNumber == TEAM_REBELS )
				nRebelPlayers++;
			else if ( teamNumber == TEAM_COMBINE )
				nCombinePlayers++;
			else if ( teamNumber == TEAM_UNASSIGNED )
				nUnassignedPlayers++;
		}
	}

	// Calculate desired bot count based on quota mode
	int desiredBotCount = bot_quota.GetInt();
	int nTotalHumans = nConnectedClients - nQuotaManagedBots;

	if ( bot_quota_debug.GetBool() )
	{
		DevMsg( "[BOT_QUOTA] === Bot Quota Debug Info ===\n" );
		DevMsg( "[BOT_QUOTA] Teamplay Mode: %s\n", bIsTeamplay ? "YES" : "NO" );
		DevMsg( "[BOT_QUOTA] Connected Clients: %d, Quota Managed Bots: %d, Bots On Game Teams: %d\n", 
			nConnectedClients, nQuotaManagedBots, nQuotaManagedBotsOnGameTeams );
		DevMsg( "[BOT_QUOTA] Human Players On Game Teams: %d, Spectators: %d\n", 
			nHumanPlayersOnGameTeams, nSpectators );
		DevMsg( "[BOT_QUOTA] Team Populations - Rebels: %d+%d, Combine: %d+%d, Unassigned: %d+%d (Human+Bot)\n",
			nRebelPlayers, nRebelBots, nCombinePlayers, nCombineBots, nUnassignedPlayers, nUnassignedBots );
		DevMsg( "[BOT_QUOTA] Raw Desired Bot Count: %d, Quota Mode: %s\n", 
			desiredBotCount, bot_quota_mode.GetString() );
	}

	if ( FStrEq( bot_quota_mode.GetString(), "fill" ) )
	{
		// Fill mode: maintain N total players (bots + humans)
		desiredBotCount = MAX( 0, desiredBotCount - nHumanPlayersOnGameTeams );
		if ( bot_quota_debug.GetBool() )
			DevMsg( "[BOT_QUOTA] Fill mode: Adjusted desired count = %d\n", desiredBotCount );
	}
	else if ( FStrEq( bot_quota_mode.GetString(), "match" ) )
	{
		// Match mode: maintain ratio of bots to humans
		desiredBotCount = (int)MAX( 0, bot_quota.GetFloat() * nHumanPlayersOnGameTeams );
		if ( bot_quota_debug.GetBool() )
			DevMsg( "[BOT_QUOTA] Match mode: Adjusted desired count = %d\n", desiredBotCount );
	}

	// Wait for a player to join, if necessary - but be more lenient in deathmatch
	if ( bot_join_after_player.GetBool() )
	{
		if ( bIsTeamplay )
		{
			// In teamplay, wait for humans on game teams
			if ( ( nHumanPlayersOnGameTeams == 0 ) && ( nSpectators == 0 ) )
			{
				desiredBotCount = 0;
				if ( bot_quota_debug.GetBool() )
					DevMsg( "[BOT_QUOTA] Teamplay: Waiting for human players, setting desired count to 0\n" );
			}
		}
		else
		{
			// In deathmatch, only wait if there are absolutely no humans connected
			if ( nTotalHumans == 0 )
			{
				desiredBotCount = 0;
				if ( bot_quota_debug.GetBool() )
					DevMsg( "[BOT_QUOTA] Deathmatch: No humans connected, setting desired count to 0\n" );
			}
		}
	}

	// Reserve slots for humans if auto-vacate is enabled
	if ( bot_auto_vacate.GetBool() )
	{
		desiredBotCount = MIN( desiredBotCount, gpGlobals->maxClients - nTotalHumans - 1 );
		if ( bot_quota_debug.GetBool() )
			DevMsg( "[BOT_QUOTA] Auto-vacate enabled: Adjusted desired count = %d\n", desiredBotCount );
	}
	else
	{
		desiredBotCount = MIN( desiredBotCount, gpGlobals->maxClients - nTotalHumans );
	}

	if ( bot_quota_debug.GetBool() )
	{
		DevMsg( "[BOT_QUOTA] Final desired bot count: %d, Current bots on game teams: %d\n", 
			desiredBotCount, nQuotaManagedBotsOnGameTeams );
	}

	// Add bots if necessary
	if ( desiredBotCount > nQuotaManagedBotsOnGameTeams )
	{
		if ( bot_quota_debug.GetBool() )
		{
			DevMsg( "[BOT_QUOTA] Need to add bots: %d desired, %d current\n", 
				desiredBotCount, nQuotaManagedBotsOnGameTeams );
		}

		// Determine which team needs more bots for balance
		int targetTeam = TEAM_UNASSIGNED;
		if ( bIsTeamplay )
		{
			int totalRebels = nRebelPlayers + nRebelBots;
			int totalCombine = nCombinePlayers + nCombineBots;

			if ( totalRebels < totalCombine )
			{
				targetTeam = TEAM_REBELS;
			}
			else if ( totalCombine < totalRebels )
			{
				targetTeam = TEAM_COMBINE;
			}
			else
			{
				// Teams are balanced, randomly assign
				targetTeam = (RandomInt( 0, 1 ) == 0) ? TEAM_REBELS : TEAM_COMBINE;
			}
			if ( bot_quota_debug.GetBool() )
			{
				DevMsg( "[BOT_QUOTA] Teamplay mode: Assigning bot to team %d (Rebels=%d, Combine=%d)\n", 
					targetTeam, totalRebels, totalCombine );
			}
		}
		else
		{
			// Deathmatch mode - use unassigned team
			targetTeam = TEAM_UNASSIGNED;
			if ( bot_quota_debug.GetBool() )
			{
				DevMsg( "[BOT_QUOTA] Deathmatch mode: Assigning bot to TEAM_UNASSIGNED\n" );
			}
		}

		// Try to get a bot from the pool first (but be more careful about reuse)
		CHL2MPBot *pBot = GetAvailableBotFromPool();
		if ( pBot == NULL )
		{
			// Create a new bot
			if ( bot_quota_debug.GetBool() )
				DevMsg( "[BOT_QUOTA] Creating new bot from scratch\n" );
			pBot = NextBotCreatePlayerBot< CHL2MPBot >( GetUniqueBotName() );
		}
		else
		{
			if ( bot_quota_debug.GetBool() )
				DevMsg( "[BOT_QUOTA] Reusing bot from pool\n" );
		}

		if ( pBot )
		{
			if ( bot_quota_debug.GetBool() )
				DevMsg( "[BOT_QUOTA] Bot created successfully, configuring...\n" );
			// Set quota management flag
			pBot->SetAttribute( CHL2MPBot::QUOTA_MANANGED );

			// Assign team (ensure we never leave bots unassigned in teamplay)
			int iTeam = targetTeam;
			if ( iTeam == TEAM_UNASSIGNED && bIsTeamplay )
			{
				// Fallback safety - should not happen with new logic above
				if ( bot_quota_debug.GetBool() )
					DevMsg( "[BOT_QUOTA] WARNING: Fallback team assignment in teamplay mode!\n" );
				iTeam = TEAM_REBELS;
			}

			if ( bot_quota_debug.GetBool() )
				DevMsg( "[BOT_QUOTA] Assigning bot to team %d\n", iTeam );

			// Set appropriate model based on team
			const char* pszModel = "";
			if ( iTeam == TEAM_UNASSIGNED )
			{
				pszModel = g_ppszRandomModels[RandomInt( 0, ARRAYSIZE( g_ppszRandomModels ) )];
			}
			else if ( iTeam == TEAM_COMBINE )
			{
				pszModel = g_ppszRandomCombineModels[RandomInt( 0, ARRAYSIZE( g_ppszRandomCombineModels ) )];
			}
			else
			{
				pszModel = g_ppszRandomCitizenModels[RandomInt( 0, ARRAYSIZE( g_ppszRandomCitizenModels ) )];
			}

			// Generate unique bot name
			const char* uniqueName = GetUniqueBotName();
			
			if ( bot_quota_debug.GetBool() )
				DevMsg( "[BOT_QUOTA] Bot configured - Name: %s, Model: %s, Team: %d\n", uniqueName, pszModel, iTeam );

			// Configure bot
			engine->SetFakeClientConVarValue( pBot->edict(), "cl_playermodel", pszModel );
			engine->SetFakeClientConVarValue( pBot->edict(), "name", uniqueName );

			// Join team - this is critical for proper spawning
			if ( bot_quota_debug.GetBool() )
				DevMsg( "[BOT_QUOTA] Joining bot to team %d...\n", iTeam );
			pBot->HandleCommand_JoinTeam( iTeam );
			pBot->ChangeTeam( iTeam );

			// Force spawn if the bot doesn't spawn automatically
			if ( !pBot->IsAlive() )
			{
				if ( bot_quota_debug.GetBool() )
					DevMsg( "[BOT_QUOTA] Bot not alive, forcing respawn...\n" );
				pBot->ForceRespawn();
			}
			else
			{
				if ( bot_quota_debug.GetBool() )
					DevMsg( "[BOT_QUOTA] Bot spawned successfully\n" );
			}
		}
		else
		{
			if ( bot_quota_debug.GetBool() )
				DevWarning( "[BOT_QUOTA] Failed to create bot!\n" );
		}
	}
	else if ( desiredBotCount < nQuotaManagedBotsOnGameTeams )
	{
		if ( bot_quota_debug.GetBool() )
		{
			DevMsg( "[BOT_QUOTA] Need to remove bots: %d desired, %d current\n", 
				desiredBotCount, nQuotaManagedBotsOnGameTeams );
		}
		
		// Remove excess bots
		// First try to remove unassigned bots (cleanup)
		if ( UTIL_KickBotFromTeam( TEAM_UNASSIGNED ) )
		{
			if ( bot_quota_debug.GetBool() )
				DevMsg( "[BOT_QUOTA] Removed unassigned bot\n" );
			return;
		}

		// Determine which team to remove from for balance
		int kickTeam;
		int totalRebels = nRebelPlayers + nRebelBots;
		int totalCombine = nCombinePlayers + nCombineBots;

		CTeam *pRebels = GetGlobalTeam( TEAM_REBELS );
		CTeam *pCombine = GetGlobalTeam( TEAM_COMBINE );

		if ( pCombine && pRebels )
		{
			// Remove from the team that has more players
			if ( totalCombine > totalRebels )
			{
				kickTeam = TEAM_COMBINE;
			}
			else if ( totalRebels > totalCombine )
			{
				kickTeam = TEAM_REBELS;
			}
			// Remove from the team that's winning
			else if ( pCombine->GetScore() > pRebels->GetScore() )
			{
				kickTeam = TEAM_COMBINE;
			}
			else if ( pRebels->GetScore() > pCombine->GetScore() )
			{
				kickTeam = TEAM_REBELS;
			}
			else
			{
				// Teams and scores are equal, pick randomly
				kickTeam = (RandomInt( 0, 1 ) == 0) ? TEAM_COMBINE : TEAM_REBELS;
			}
		}
		else
		{
			// Fallback to rebels if team objects aren't valid
			kickTeam = TEAM_REBELS;
		}

		if ( bot_quota_debug.GetBool() )
			DevMsg( "[BOT_QUOTA] Attempting to kick bot from team %d\n", kickTeam );

		// Attempt to kick a bot from the chosen team
		if ( UTIL_KickBotFromTeam( kickTeam ) )
		{
			if ( bot_quota_debug.GetBool() )
				DevMsg( "[BOT_QUOTA] Successfully kicked bot from team %d\n", kickTeam );
			return;
		}

		// If no bots on that team, try the other team
		int otherTeam = kickTeam == TEAM_COMBINE ? TEAM_REBELS : TEAM_COMBINE;
		if ( bot_quota_debug.GetBool() )
			DevMsg( "[BOT_QUOTA] No bots on team %d, trying team %d\n", kickTeam, otherTeam );
		UTIL_KickBotFromTeam( otherTeam );
	}
	else
	{
		if ( bot_quota_debug.GetBool() )
		{
			DevMsg( "[BOT_QUOTA] Bot quota satisfied: %d bots (desired: %d)\n", 
				nQuotaManagedBotsOnGameTeams, desiredBotCount );
		}
	}
}


//----------------------------------------------------------------------------------------------------------------
bool CHL2MPBotManager::IsAllBotTeam( int iTeam )
{
	CTeam *pTeam = GetGlobalTeam( iTeam );
	if ( pTeam == NULL )
	{
		return false;
	}

	// check to see if any players on the team are humans
	for ( int i = 0, n = pTeam->GetNumPlayers(); i < n; ++i )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( pTeam->GetPlayer( i ) );
		if ( pPlayer == NULL )
		{
			continue;
		}
		if ( pPlayer->IsBot() == false )
		{
			return false;
		}
	}

	// if we made it this far, then they must all be bots!
	if ( pTeam->GetNumPlayers() != 0 )
	{
		return true;
	}

	return true;
}


//----------------------------------------------------------------------------------------------------------------
void CHL2MPBotManager::SetIsInOfflinePractice(bool bIsInOfflinePractice)
{
	bot_offline_practice.SetValue( bIsInOfflinePractice ? 1 : 0 );
}


//----------------------------------------------------------------------------------------------------------------
bool CHL2MPBotManager::IsInOfflinePractice() const
{
	return bot_offline_practice.GetInt() != 0;
}


//----------------------------------------------------------------------------------------------------------------
bool CHL2MPBotManager::IsMeleeOnly() const
{
	return bot_melee_only.GetBool();
}


//----------------------------------------------------------------------------------------------------------------
bool CHL2MPBotManager::IsGravGunOnly() const
{
	return bot_gravgun_only.GetBool();
}


//----------------------------------------------------------------------------------------------------------------
void CHL2MPBotManager::RevertOfflinePracticeConvars()
{
	bot_quota.Revert();
	bot_quota_mode.Revert();
	bot_auto_vacate.Revert();
	bot_difficulty.Revert();
	bot_offline_practice.Revert();
}


//----------------------------------------------------------------------------------------------------------------
void CHL2MPBotManager::LevelShutdown()
{
	m_flNextPeriodicThink = 0.0f;
	if ( IsInOfflinePractice() )
	{
		RevertOfflinePracticeConvars();
		SetIsInOfflinePractice( false );
	}		
}


//----------------------------------------------------------------------------------------------------------------
CHL2MPBot* CHL2MPBotManager::GetAvailableBotFromPool()
{
	// Look for truly disconnected or kicked bots that can be reused
	// Avoid reusing bots that are just in spectator/unassigned as they may be transitioning
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CHL2MP_Player *pPlayer = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		CHL2MPBot* pBot = dynamic_cast<CHL2MPBot*>(pPlayer);

		if (pBot == NULL)
			continue;

		if ( ( pBot->GetFlags() & FL_FAKECLIENT ) == 0 )
			continue;

		// Only reuse bots that are actually disconnected or in a safe reusable state
		if ( !pBot->IsConnected() /*|| pBot->IsKicked()*/ )
		{
			pBot->ClearAttribute( CHL2MPBot::QUOTA_MANANGED );
			return pBot;
		}
	}

	// Don't reuse bots from spectator/unassigned teams to avoid slot conflicts
	return NULL;
}


//----------------------------------------------------------------------------------------------------------------
void CHL2MPBotManager::OnForceAddedBots( int iNumAdded )
{
	bot_quota.SetValue( bot_quota.GetInt() + iNumAdded );
	m_flNextPeriodicThink = gpGlobals->curtime + 1.0f;
}


//----------------------------------------------------------------------------------------------------------------
void CHL2MPBotManager::OnForceKickedBots( int iNumKicked )
{
	bot_quota.SetValue( MAX( bot_quota.GetInt() - iNumKicked, 0 ) );
	// allow time for the bots to be kicked
	m_flNextPeriodicThink = gpGlobals->curtime + 2.0f;
}


//----------------------------------------------------------------------------------------------------------------
CHL2MPBotManager &TheHL2MPBots( void )
{
	return static_cast<CHL2MPBotManager&>( TheNextBots() );
}



//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------
CON_COMMAND_F( bot_debug_stuck_log, "Given a server logfile, visually display bot stuck locations.", FCVAR_GAMEDLL | FCVAR_CHEAT )
{
	// Listenserver host or rcon access only!
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( args.ArgC() < 2 )
	{
		DevMsg( "%s <logfilename>\n", args.Arg(0) );
		return;
	}

	FileHandle_t file = filesystem->Open( args.Arg(1), "r", "GAME" );

	const int maxBufferSize = 1024;
	char buffer[ maxBufferSize ];

	char logMapName[ maxBufferSize ];
	logMapName[0] = '\000';

	TheHL2MPBots().ClearStuckBotData();

	if ( file )
	{
		int line = 0;
		while( !filesystem->EndOfFile( file ) )
		{
			filesystem->ReadLine( buffer, maxBufferSize, file );
			++line;

			strtok( buffer, ":" );
			strtok( NULL, ":" );
			strtok( NULL, ":" );
			char *first = strtok( NULL, " " );

			if ( !first )
				continue;

			if ( !strcmp( first, "Loading" ) )
			{
				// L 08/08/2012 - 15:10:47: Loading map "mvm_coaltown"
				strtok( NULL, " " );
				char *mapname = strtok( NULL, "\"" );

				if ( mapname )
				{
					strcpy( logMapName, mapname );
					Warning( "*** Log file from map '%s'\n", mapname );
				}
			}
			else if ( first[0] == '\"' )
			{
				// might be a player ID

				char *playerClassname = &first[1];

				char *nameEnd = playerClassname;
				while( *nameEnd != '\000' && *nameEnd != '<' )
					++nameEnd;
				*nameEnd = '\000';

				char *botIDString = ++nameEnd;
				char *IDEnd = botIDString;
				while( *IDEnd != '\000' && *IDEnd != '>' )
					++IDEnd;
				*IDEnd = '\000';

				int botID = atoi( botIDString );

				char *second = strtok( NULL, " " );
				if ( second && !strcmp( second, "stuck" ) )
				{
					CStuckBot *stuckBot = TheHL2MPBots().FindOrCreateStuckBot( botID, playerClassname );

					CStuckBotEvent *stuckEvent = new CStuckBotEvent;


					// L 08/08/2012 - 15:15:05: "Scout<53><BOT><Blue>" stuck (position "-180.61 2471.29 216.04") (duration "2.52") L 08/08/2012 - 15:15:05:    path_goal ( "-180.61 2471.29 216.04" )
					strtok( NULL, " (\"" );	// (position

					stuckEvent->m_stuckSpot.x = (float)atof( strtok( NULL, " )\"" ) );
					stuckEvent->m_stuckSpot.y = (float)atof( strtok( NULL, " )\"" ) );
					stuckEvent->m_stuckSpot.z = (float)atof( strtok( NULL, " )\"" ) );

					strtok( NULL, ") (\"" );
					stuckEvent->m_stuckDuration = (float)atof( strtok( NULL, "\"" ) );

					strtok( NULL, ") (\"-L0123456789/:" );	// path_goal

					char *goal = strtok( NULL, ") (\"" );

					if ( goal && strcmp( goal, "NULL" ) )
					{
						stuckEvent->m_isGoalValid = true;

						stuckEvent->m_goalSpot.x = (float)atof( goal );
						stuckEvent->m_goalSpot.y = (float)atof( strtok( NULL, ") (\"" ) );
						stuckEvent->m_goalSpot.z = (float)atof( strtok( NULL, ") (\"" ) );
					}
					else
					{
						stuckEvent->m_isGoalValid = false;
					}

					stuckBot->m_stuckEventVector.AddToTail( stuckEvent );
				}
			}
		}

		filesystem->Close( file );
	}
	else
	{
		Warning( "Can't open file '%s'\n", args.Arg(1) );
	}

	//TheHL2MPBots().DrawStuckBotData();
}


//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------
CON_COMMAND_F( bot_debug_stuck_log_clear, "Clear currently loaded bot stuck data", FCVAR_GAMEDLL | FCVAR_CHEAT )
{
	// Listenserver host or rcon access only!
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	TheHL2MPBots().ClearStuckBotData();
}


//----------------------------------------------------------------------------------------------------------------
// for parsing and debugging stuck bot server logs
void CHL2MPBotManager::ClearStuckBotData()
{
	m_stuckBotVector.PurgeAndDeleteElements();
}


//----------------------------------------------------------------------------------------------------------------
// for parsing and debugging stuck bot server logs
CStuckBot *CHL2MPBotManager::FindOrCreateStuckBot( int id, const char *playerClass )
{
	for( int i=0; i<m_stuckBotVector.Count(); ++i )
	{
		CStuckBot *stuckBot = m_stuckBotVector[i];

		if ( stuckBot->IsMatch( id, playerClass ) )
		{
			return stuckBot;
		}
	}

	// new instance of a stuck bot
	CStuckBot *newStuckBot = new CStuckBot( id, playerClass );
	m_stuckBotVector.AddToHead( newStuckBot );

	return newStuckBot;
}


//----------------------------------------------------------------------------------------------------------------
void CHL2MPBotManager::DrawStuckBotData( float deltaT )
{
	if ( engine->IsDedicatedServer() )
		return;

	if ( !m_stuckDisplayTimer.IsElapsed() )
		return;

	m_stuckDisplayTimer.Start( deltaT );

	CBasePlayer *player = UTIL_GetListenServerHost();
	if ( player == NULL )
		return;

// 	Vector forward;
// 	AngleVectors( player->EyeAngles(), &forward );

	for( int i=0; i<m_stuckBotVector.Count(); ++i )
	{
		for( int j=0; j<m_stuckBotVector[i]->m_stuckEventVector.Count(); ++j )
		{
			m_stuckBotVector[i]->m_stuckEventVector[j]->Draw( deltaT );
		}

		for( int j=0; j<m_stuckBotVector[i]->m_stuckEventVector.Count()-1; ++j )
		{
			NDebugOverlay::HorzArrow( m_stuckBotVector[i]->m_stuckEventVector[j]->m_stuckSpot, 
									  m_stuckBotVector[i]->m_stuckEventVector[j+1]->m_stuckSpot,
									  3, 100, 0, 255, 255, true, deltaT );
		}

		NDebugOverlay::Text( m_stuckBotVector[i]->m_stuckEventVector[0]->m_stuckSpot, CFmtStr( "%s(#%d)", m_stuckBotVector[i]->m_name, m_stuckBotVector[i]->m_id ), false, deltaT );
	}
}


