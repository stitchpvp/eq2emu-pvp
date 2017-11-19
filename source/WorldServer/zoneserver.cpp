/*  
EQ2Emulator:  Everquest II Server Emulator
Copyright (C) 2007  EQ2EMulator Development Team (http://www.eq2emulator.net)

This file is part of EQ2Emulator.
EQ2Emulator is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

EQ2Emulator is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with EQ2Emulator.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../common/debug.h"
#include <iostream>
using namespace std;
#include <string.h>
#include "../common/misc.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "Commands/Commands.h"

#ifdef WIN32
#include <windows.h>
#include <winsock.h>
#else
#include <sys/socket.h>
#ifdef FREEBSD //Timothy Whitman - January 7, 2003
#include <sys/types.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>

#include "../common/unix.h"

#define SOCKET_ERROR -1
#define INVALID_SOCKET -1

extern int errno;
#endif

#include "../common/servertalk.h"
#include "../common/packet_dump.h"
#include "WorldDatabase.h"
#include "races.h"
#include "classes.h"
#include "../common/seperator.h"
#include "../common/md5.h"
#include "../common/EQStream.h"
#include "../common/EQStreamFactory.h"
#include "../common/opcodemgr.h"
#include "zoneserver.h"
#include "client.h"
#include "LoginServer.h"
#include "World.h"
#include <string>
#include <assert.h>
#include "LuaInterface.h"
#include "Factions.h"
#include "VisualStates.h"
#include "ClientPacketFunctions.h"
#include "SpellProcess.h"
#include "../common/Log.h"
#include "Rules/Rules.h"
#include "Chat/Chat.h"
#include "Tradeskills/Tradeskills.h"
#include "RaceTypes/RaceTypes.h"
#include <algorithm>
#include "PVP.h"

#ifdef WIN32
#define snprintf	_snprintf
#define vsnprintf	_vsnprintf
#define strncasecmp	_strnicmp
#define strcasecmp  _stricmp
#endif

// int32 numplayers = 0;												// never used?
// extern bool GetZoneLongName(char* short_name, char** long_name);		// never used?
// extern bool holdzones;												// never used?
// extern volatile bool RunLoops;										// never used in the zone server?
// extern Classes classes;												// never used in the zone server?

extern WorldDatabase	database;
extern sint32			numzones;
extern ClientList		client_list;
extern LoginServer loginserver;
extern ZoneList zone_list;
extern World world;
extern ConfigReader configReader;
extern Commands commands;
extern LuaInterface* lua_interface;
extern MasterFactionList master_faction_list;
extern VisualStates visual_states;
extern RuleManager rule_manager;
extern Chat chat;
extern MasterRaceTypeList race_types_list;
extern MasterSpellList master_spell_list;		// temp - remove later
extern MasterSkillList master_skill_list;

int32 MinInstanceID = 1000;

// JA: Moved most default values to Rules and risky initializers to ZoneServer::Init() - 2012.12.07
ZoneServer::ZoneServer(const char* name) {
	depop_zone = false;
	repop_zone = false;
	respawns_allowed = true;
	instanceID = 0;
	strcpy(zone_name, name);
	zoneID = 0;
	rain = 0;
	cityzone = false;
	always_loaded = false;
	locked = false;	// JA: implementing /zone lock|unlock commands
	pNumPlayers = 0;
	LoadingData = true;
	zoneShuttingDown = false;
	++numzones;
	revive_points = 0;
	zone_motd = "";
	finished_depop = true;
	initial_spawn_threads_active = 0;
	minimumStatus = 0;
	minimumLevel = 0;
	maximumLevel = 0;
	minimumVersion = 0;
	weather_current_severity = 0;
	weather_signaled = false;
	xp_mod = 0;
	isDusk = false;
	dusk_hour = 0;
	dusk_minute = 0;
	dawn_hour = 0;
	dawn_minute = 0;
	reloading_spellprocess = false;
	MMasterZoneLock = new CriticalSection(MUTEX_ATTRIBUTE_RECURSIVE);
	weather_allowed = true;
	spawnthread_active = false;
	combatthread_active = false;
	client_thread_active = false;
	spellProcess = 0;
	tradeskillMgr = 0;
	InstanceType = PUBLIC_INSTANCE;
	isInstance = false;
	zone_file;

	reloading = true;
}

ZoneServer::~ZoneServer() {
	zoneShuttingDown = true;  //ensure other threads shut down too
	//allow other threads to properly shut down
	while (spawnthread_active || initial_spawn_threads_active > 0){
		if (spawnthread_active)
			LogWrite(ZONE__DEBUG, 7, "Zone", "Zone shutdown waiting on spawn thread");
		if (initial_spawn_threads_active > 0)
			LogWrite(ZONE__DEBUG, 7, "Zone", "Zone shutdown waiting on initial spawn thread");
		Sleep(10);
	}
	LogWrite(ZONE__INFO, 0, "Zone", "Initiating zone shutdown of '%s'", zone_name);
	changed_spawns.clear();
	transport_spawns.clear();
	safe_delete(spellProcess);
	safe_delete(tradeskillMgr);
	MMasterZoneLock->lock();
	MMasterSpawnLock.writelock(__FUNCTION__, __LINE__);
	DeleteData(true);
	RemoveLocationProximities();
	RemoveLocationGrids();
	DelayedSpawnRemoval(true);
	DeleteSpawns(true);
	
	DeleteGlobalSpawns();

	DeleteFlightPaths();

	MMasterSpawnLock.releasewritelock(__FUNCTION__, __LINE__);
	MMasterZoneLock->unlock();
	safe_delete(MMasterZoneLock);
	world.UpdateServerStatistic(STAT_SERVER_NUM_ACTIVE_ZONES, -1);

	// If lockout, public, tradeskill, or quest instance delete from db when zone shuts down
	if (InstanceType == SOLO_LOCKOUT_INSTANCE || InstanceType == GROUP_LOCKOUT_INSTANCE || InstanceType == RAID_LOCKOUT_INSTANCE ||
		InstanceType == PUBLIC_INSTANCE || InstanceType == TRADESKILL_INSTANCE || InstanceType == QUEST_INSTANCE) {
		LogWrite(INSTANCE__DEBUG, 0, "Instance",  "Non persistent instance shutting down, deleting instance");
		database.DeleteInstance(instanceID);
	}

	LogWrite(ZONE__INFO, 0, "Zone", "Completed zone shutdown of '%s'", zone_name);
	--numzones;
	UpdateWindowTitle(0);
	zone_list.Remove(this);
}

void ZoneServer::Init()
{
	LogWrite(ZONE__INFO, 0, "Zone", "Loading new Zone '%s'", zone_name);
	zone_list.Add(this);

	spellProcess = new SpellProcess();
	tradeskillMgr = new TradeskillMgr();

	/* Dynamic Timers */
	regenTimer.Start(rule_manager.GetGlobalRule(R_Zone, RegenTimer)->GetInt32());
	client_save.Start(rule_manager.GetGlobalRule(R_Zone, ClientSaveTimer)->GetInt32());
	shutdownTimer.Disable();
	spawn_range.Start(rule_manager.GetGlobalRule(R_Zone, CheckAttackPlayer)->GetInt32());
	aggro_timer.Start(rule_manager.GetGlobalRule(R_Zone, CheckAttackNPC)->GetInt32());

	/* Weather stuff */
	InitWeather();

	/* Static Timers */
	// JA - haven't decided yet if these should remain hard-coded. Changing them could break EQ2Emu functionality
	spawn_check_add.Start(1000);
	spawn_check_remove.Start(30000);
	spawn_expire_timer.Start(10000);
	respawn_timer.Start(10000);
	// there was never a starter for these?
	widget_timer.Start(5000);

	tracking_timer.Start(5000);

	movement_timer.Start(5);
	location_prox_timer.Start(1000);
	location_grid_timer.Start(1000);

	charsheet_changes.Start(500);

	// Send game time packet every in game hour (180 sec)
	sync_game_time_timer.Start(180000);

	// Get the dusk and dawn time from the rule manager and store it in the correct variables
	sscanf (rule_manager.GetGlobalRule(R_World, DuskTime)->GetString(), "%d:%d", &dusk_hour, &dusk_minute);
	sscanf (rule_manager.GetGlobalRule(R_World, DawnTime)->GetString(), "%d:%d", &dawn_hour, &dawn_minute);

	spawn_update.Start(rule_manager.GetGlobalRule(R_Zone, SpawnUpdateTimer)->GetInt16());
	LogWrite(ZONE__DEBUG, 0, "Zone", "SpawnUpdateTimer: %ims", rule_manager.GetGlobalRule(R_Zone, SpawnUpdateTimer)->GetInt16());

	spawn_delete_timer = rule_manager.GetGlobalRule(R_Zone, SpawnDeleteTimer)->GetInt32();
	LogWrite(ZONE__DEBUG, 0, "Zone", "SpawnDeleteTimer: %ums", spawn_delete_timer);

	LogWrite(ZONE__DEBUG, 0, "Zone", "Loading zone flight paths");
	database.LoadZoneFlightPaths(this);

	world.UpdateServerStatistic(STAT_SERVER_NUM_ACTIVE_ZONES, 1);
	UpdateWindowTitle(0);
	MMasterSpawnLock.SetName("ZoneServer::MMasterSpawnLock");
	m_npc_faction_list.SetName("ZoneServer::npc_faction_list");
	m_enemy_faction_list.SetName("ZoneServer::enemy_faction_list");
	m_reverse_enemy_faction_list.SetName("ZoneServer::reverse_enemy_faction_list");
	MDeadSpawns.SetName("ZoneServer::dead_spawns");
	MTransportSpawns.SetName("ZoneServer::transport_spawns");
	MSpawnList.SetName("ZoneServer::spawn_list");
	MTransporters.SetName("ZoneServer::m_transportMaps");
	MSpawnGroupAssociation.SetName("ZoneServer::spawn_group_associations");
	MSpawnGroupLocations.SetName("ZoneServer::spawn_group_locations");
	MSpawnLocationGroups.SetName("ZoneServer::spawn_location_groups");
	MSpawnGroupChances.SetName("ZoneServer::spawn_group_chances");
	MTransportLocations.SetName("ZoneServer::transporter_locations");
	MSpawnLocationList.SetName("ZoneServer::spawn_location_list");
	MSpawnDeleteList.SetName("ZoneServer::spawn_delete_list");
	MSpawnScriptTimers.SetName("ZoneServer::spawn_script_timers");
	MRemoveSpawnScriptTimersList.SetName("ZoneServer::remove_spawn_script_timers_list");
	MClientList.SetName("ZoneServer::clients");
#ifdef WIN32
	_beginthread(ZoneLoop, 0, this);
	_beginthread(SpawnLoop, 0, this);
#else
	pthread_t thread;
	pthread_create(&thread, NULL, ZoneLoop, this);
	pthread_detach(thread);
	pthread_t thread2;
	pthread_create(&thread2, NULL, SpawnLoop, this);
	pthread_detach(thread2);
#endif
}

void ZoneServer::InitWeather()
{
	weather_enabled = rule_manager.GetGlobalRule(R_Zone, WeatherEnabled)->GetBool();
	if( weather_enabled && isWeatherAllowed())
	{
		string tmp;
		// set up weather system when zone starts up
		weather_type				= rule_manager.GetGlobalRule(R_Zone, WeatherType)->GetInt8();
		switch(weather_type)
		{
		case 3: tmp = "Chaotic"; break;
		case 2: tmp = "Random"; break;
		case 1: tmp = "Dynamic"; break;
		default: tmp = "Normal"; break;
		}
		LogWrite(ZONE__DEBUG, 0, "Zone", "%s: Setting up '%s' weather", zone_name, tmp.c_str());

		weather_frequency			= rule_manager.GetGlobalRule(R_Zone, WeatherChangeFrequency)->GetInt32();
		LogWrite(ZONE__DEBUG, 1, "Zone", "%s: Change weather every %u seconds", zone_name, weather_frequency);

		weather_change_chance		= rule_manager.GetGlobalRule(R_Zone, WeatherChangeChance)->GetInt8();
		LogWrite(ZONE__DEBUG, 1, "Zone", "%s: Chance of weather change: %i%%", zone_name, weather_change_chance);

		weather_min_severity		= rule_manager.GetGlobalRule(R_Zone, MinWeatherSeverity)->GetFloat();
		weather_max_severity		= rule_manager.GetGlobalRule(R_Zone, MaxWeatherSeverity)->GetFloat();
		LogWrite(ZONE__DEBUG, 1, "Zone", "%s: Weather Severity min/max is %.2f - %.2f", zone_name, weather_min_severity, weather_max_severity);
		// Allow a random roll to determine if weather should start out severe or calm
		if( MakeRandomInt(1, 100) > 50)
		{
			weather_pattern				= 1; // default weather to increase in severity initially
			weather_current_severity	= weather_min_severity;
		}
		else
		{
			weather_pattern				= 0; // default weather to decrease in severity initially
			weather_current_severity	= weather_max_severity;
		}
		LogWrite(ZONE__DEBUG, 1, "Zone", "%s: Weather Severity set to %.2f, pattern: %i", zone_name, weather_current_severity, weather_pattern);

		weather_change_amount		= rule_manager.GetGlobalRule(R_Zone, WeatherChangePerInterval)->GetFloat();
		LogWrite(ZONE__DEBUG, 1, "Zone", "%s: Weather change by %.2f each interval", zone_name, weather_change_amount);

		if( weather_type > 0 )
		{
			weather_dynamic_offset		= rule_manager.GetGlobalRule(R_Zone, WeatherDynamicMaxOffset)->GetFloat();
			LogWrite(ZONE__DEBUG, 1, "Zone", "%s: Weather Max Offset changes no more than %.2f each interval", zone_name, weather_dynamic_offset);
		}
		else
			weather_dynamic_offset = 0;

		SetRain(weather_current_severity);
		weather_last_changed_time = Timer::GetUnixTimeStamp();
		weatherTimer.Start(rule_manager.GetGlobalRule(R_Zone, WeatherTimer)->GetInt32());
	}
}
void ZoneServer::DeleteSpellProcess(){
	//Just get a lock to make sure we aren't already looping the spawnprocess or clientprocess if this is different than the calling thread
	MMasterSpawnLock.writelock(__FUNCTION__, __LINE__);
	MMasterZoneLock->lock();
	reloading_spellprocess = true;

	// Remove spells from NPC's
	Spawn* spawn = 0;
	map<int32, Spawn*>::iterator itr;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
		spawn = itr->second;
		if(spawn && spawn->IsNPC())
			static_cast<NPC*>(spawn)->SetSpells(0);
	}
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);

	MMasterZoneLock->unlock();
	MMasterSpawnLock.releasewritelock(__FUNCTION__, __LINE__);
	DismissAllPets();
	safe_delete(spellProcess);
}

void ZoneServer::LoadSpellProcess(){
	if (spellProcess) safe_delete(spellProcess);

	spellProcess = new SpellProcess();
	reloading_spellprocess = false;

	// Reload NPC's spells
	Spawn* spawn = 0;
	NPC* npc = 0;
	map<int32, Spawn*>::iterator itr;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
		spawn = itr->second;
		if (spawn && spawn->IsNPC())
			npc = static_cast<NPC*>(spawn);
			if (npc)
				npc->SetSpells(GetNPCSpells(npc->GetPrimarySpellList(), npc->GetSecondarySpellList()));
	}
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::LockAllSpells(Player* player) {
	if (player && spellProcess) {
		Client* client = GetClientBySpawn(player);
		if (client)
			spellProcess->LockAllSpells(client);
	}
}
	
void ZoneServer::UnlockAllSpells(Player* player) {
	if (player && spellProcess) {
		Client* client = GetClientBySpawn(player);
		if (client)
			spellProcess->UnlockAllSpells(client);
	}
}

void ZoneServer::DeleteFactionLists() {
	map<int32, vector<int32> *>::iterator faction_itr;
	map<int32, vector<int32> *>::iterator spawn_itr;

	m_enemy_faction_list.writelock(__FUNCTION__, __LINE__);
	for (faction_itr = enemy_faction_list.begin(); faction_itr != enemy_faction_list.end(); faction_itr++)
		safe_delete(faction_itr->second);
	enemy_faction_list.clear();
	m_enemy_faction_list.releasewritelock(__FUNCTION__, __LINE__);

	m_reverse_enemy_faction_list.writelock(__FUNCTION__, __LINE__);
	for (faction_itr = reverse_enemy_faction_list.begin(); faction_itr != reverse_enemy_faction_list.end(); faction_itr++)
		safe_delete(faction_itr->second);
	reverse_enemy_faction_list.clear();
	m_reverse_enemy_faction_list.releasewritelock(__FUNCTION__, __LINE__);

	m_npc_faction_list.writelock(__FUNCTION__, __LINE__);
	for (spawn_itr = npc_faction_list.begin(); spawn_itr != npc_faction_list.end(); spawn_itr++)
		safe_delete(spawn_itr->second);
	npc_faction_list.clear();
	m_npc_faction_list.releasewritelock(__FUNCTION__, __LINE__);
}

void ZoneServer::DeleteData(bool boot_clients){
	Spawn* spawn = 0;
	vector<Spawn*> tmp_player_list; // changed to a vector from a MutexList as this is a local variable and don't need mutex stuff for the list

	// Clear spawn groups
	spawn_group_map.clear();

	// Loop through the spawn list and set the spawn for deletion
	map<int32, Spawn*>::iterator itr;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
		spawn = itr->second;
		if(spawn){
			if(!boot_clients && spawn->IsPlayer())
				tmp_player_list.push_back(spawn);
			else if(spawn->IsPlayer()){
				Client* client = GetClientBySpawn(spawn);
				if(client)
					client->Disconnect();
			}
			else{
				RemoveHeadingTimer(spawn); // called in RemoveSpawnSupportFunctions()
				RemoveSpawnSupportFunctions(spawn);
				AddPendingDelete(spawn);
			}
		}
	}
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);

	// Quick hack to prevent a deadlock, RemoveSpawnSupportFunctions() will cancel spells and result in zone->GetSpawnByID()
	// being called which read locks the spawn list and caused a dead lock as the above mutex's were write locked
	MSpawnList.writelock(__FUNCTION__, __LINE__);
	// Clear the spawn list, this was in the mutex above, moved it down so the above mutex could be a read lock
	spawn_list.clear();

	// Moved this up so we only read lock the list once in this list
	vector<Spawn*>::iterator spawn_iter2;
	for (spawn_iter2 = tmp_player_list.begin(); spawn_iter2 != tmp_player_list.end(); spawn_iter2++) {
		spawn_list[(*spawn_iter2)->GetID()] = (*spawn_iter2);
	}
	MSpawnList.releasewritelock(__FUNCTION__, __LINE__);

	// Clear player proximities
	RemovePlayerProximity(0, true);

	spawn_range_map.clear(true);
	if(boot_clients) {
		// Refactor
		vector<Client*>::iterator itr;

		MClientList.writelock(__FUNCTION__, __LINE__);
		for (itr = clients.begin(); itr != clients.end(); itr++)
			safe_delete(*itr);

		clients.clear();
		MClientList.releasewritelock(__FUNCTION__, __LINE__);
	}

	// Clear and delete spawn locations
	MSpawnLocationList.writelock(__FUNCTION__, __LINE__);
	map<int32, SpawnLocation*>::iterator spawn_location_iter;
	for (spawn_location_iter = spawn_location_list.begin(); spawn_location_iter != spawn_location_list.end(); spawn_location_iter++)
		safe_delete(spawn_location_iter->second);

	spawn_location_list.clear();
	MSpawnLocationList.releasewritelock(__FUNCTION__, __LINE__);

	// If we allow clients to stay in the zone we need to preserve the revive_points, otherwise if the player dies they will crash
	if(revive_points && boot_clients){
		vector<RevivePoint*>::iterator revive_iter;
		for(revive_iter=revive_points->begin(); revive_iter != revive_points->end(); revive_iter++){
			safe_delete(*revive_iter);
		}
		safe_delete(revive_points);
	}

	MSpawnGroupAssociation.writelock(__FUNCTION__, __LINE__);
	map<int32, set<int32>*>::iterator assoc_itr;
	for (assoc_itr = spawn_group_associations.begin(); assoc_itr != spawn_group_associations.end(); assoc_itr++)
		safe_delete(assoc_itr->second);

	spawn_group_associations.clear();
	MSpawnGroupAssociation.releasewritelock(__FUNCTION__, __LINE__);

	MSpawnGroupLocations.writelock(__FUNCTION__, __LINE__);
	map<int32, map<int32, int32>*>::iterator loc_itr;
	for (loc_itr = spawn_group_locations.begin(); loc_itr != spawn_group_locations.end(); loc_itr++)
		safe_delete(loc_itr->second);

	spawn_group_locations.clear();
	MSpawnGroupLocations.releasewritelock(__FUNCTION__, __LINE__);

	MSpawnLocationGroups.writelock(__FUNCTION__, __LINE__);
	map<int32, list<int32>*>::iterator group_itr;
	for (group_itr = spawn_location_groups.begin(); group_itr != spawn_location_groups.end(); group_itr++)
		safe_delete(group_itr->second);

	spawn_location_groups.clear();
	MSpawnLocationGroups.releasewritelock(__FUNCTION__, __LINE__);
	
	// Clear lists that need more then just a Clear()
	DeleteFactionLists();
	DeleteSpawnScriptTimers(0, true);
	DeleteSpawnScriptTimers();
	ClearDeadSpawns();

	// Clear lists
	heading_timers.clear();
	movement_spawns.clear();
	widget_timers.clear();
	respawn_timers.clear();
	transport_spawns.clear();
	quick_database_id_lookup.clear();

	map<int16, PacketStruct*>::iterator struct_itr;
	for (struct_itr = versioned_info_structs.begin(); struct_itr != versioned_info_structs.end(); struct_itr++)
		safe_delete(struct_itr->second);
	versioned_info_structs.clear();

	for (struct_itr = versioned_pos_structs.begin(); struct_itr != versioned_pos_structs.end(); struct_itr++)
		safe_delete(struct_itr->second);
	versioned_pos_structs.clear();

	for (struct_itr = versioned_vis_structs.begin(); struct_itr != versioned_vis_structs.end(); struct_itr++)
		safe_delete(struct_itr->second);
	versioned_vis_structs.clear();
}

void ZoneServer::RemoveLocationProximities() {
	MutexList<LocationProximity*>::iterator itr = location_proximities.begin();
	while(itr.Next()){
		safe_delete(itr->value);
	}
	location_proximities.clear();
}

RevivePoint* ZoneServer::GetRevivePoint(int32 id){
	vector<RevivePoint*>::iterator revive_iter;
	for(revive_iter=revive_points->begin(); revive_iter != revive_points->end(); revive_iter++){
		if((*revive_iter)->id == id)
			return *revive_iter;
	}
	return 0;
}

vector<RevivePoint*>* ZoneServer::GetRevivePoints(Client* client)
{
	vector<RevivePoint*>* points = new vector<RevivePoint*>;
	RevivePoint* closest_point = 0;

	// we should not check for revive points if this is null
	if ( revive_points != NULL )
	{
		LogWrite(ZONE__DEBUG, 0, "Zone", "Got revive point in %s!", __FUNCTION__);

		float closest = 100000;
		float test_closest = 0;
		RevivePoint* test_point = 0;
		vector<RevivePoint*>::iterator revive_iter;
		for(revive_iter=revive_points->begin(); revive_iter != revive_points->end(); revive_iter++)
		{
			test_point = *revive_iter;
			if(test_point)
			{
				LogWrite(ZONE__DEBUG, 0, "Zone", "Test Point Found!");
				test_closest = client->GetPlayer()->GetDistance(test_point->x, test_point->y, test_point->z);

				// should this be changed to list all revive points within max distance or just the closest
				if(test_closest < closest)
				{
					LogWrite(ZONE__DEBUG, 0, "Zone", "test_closest: %.2f, closest: %.2f", test_closest, closest);
					closest = test_closest;
					closest_point = test_point;
					if(closest <= MAX_REVIVEPOINT_DISTANCE)
						points->push_back(closest_point);
				}
			}
		}
	}

	if(closest_point && points->size() == 0 && closest_point->zone_id == GetZoneID())
	{
		LogWrite(ZONE__WARNING, 0, "Zone", "Nearest Revive Point too far away. Add another!");
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "The closest revive point is quite far away, you might want to ask the server admin for a closer one.");
		points->push_back(closest_point);
	}
	else if(points->size() == 0)
	{
		LogWrite(ZONE__WARNING, 0, "Zone", "No Revive Points set for zoneID %u. Add some!", GetZoneID());
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "There are no revive points for this zone, you might want to ask the server admin for one.");
		closest_point = new RevivePoint;
		closest_point->heading = GetSafeHeading();
		closest_point->id = 0xFFFFFFFF;
		closest_point->location_name = "Zone Safe Point";
		closest_point->zone_id = GetZoneID();
		closest_point->x = GetSafeX();
		closest_point->y = GetSafeY();
		closest_point->z = GetSafeZ();
		points->push_back(closest_point);
	}
	return points;
}

void ZoneServer::TriggerCharSheetTimer(){
	charsheet_changes.Trigger();
}

void ZoneServer::RegenUpdate(){
	if(damaged_spawns.size(true) == 0)
		return;

	Spawn* spawn = 0;
	MutexList<int32>::iterator spawn_iter = damaged_spawns.begin();
	while(spawn_iter.Next()){	
		spawn = GetSpawnByID(spawn_iter->value);
		if(spawn && (((spawn->GetHP() < spawn->GetTotalHP()) && spawn->GetHP()>0) ||  (spawn->GetPower() < spawn->GetTotalPower()))){
			if(spawn->IsEntity())
				((Entity*)spawn)->DoRegenUpdate();
			if(spawn->IsPlayer()){
				Client* client = GetClientBySpawn(spawn);
				if(client && client->IsConnected())
					client->QueuePacket(client->GetPlayer()->GetPlayerInfo()->serialize(client->GetVersion()));
			}
		}
		else
			RemoveDamagedSpawn(spawn);
		//Spawn no longer valid, remove it from the list
		if (!spawn)
			damaged_spawns.Remove(spawn_iter->value);
	}
}

void ZoneServer::ClearDeadSpawns(){
	MDeadSpawns.writelock(__FUNCTION__, __LINE__);
	dead_spawns.clear();
	MDeadSpawns.releasewritelock(__FUNCTION__, __LINE__);
}

void ZoneServer::ProcessDepop(bool respawns_allowed, bool repop) {
	vector<Client*>::iterator client_itr;
	Client* client = 0;
	Spawn* spawn = 0;
	PacketStruct* packet = 0;
	int16 packet_version = 0;
	spawn_expire_timers.clear();

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		if(!client)
			continue;
		client->GetPlayer()->SetTarget(0);
		if(repop)
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Zone Repop in progress...");
		else{
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Zone Depop in progress...");
			if(respawns_allowed)
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Spawns will respawn according to their respawn timers.");
		}
		if(!packet || packet_version != client->GetVersion()){
			safe_delete(packet);
			packet_version = client->GetVersion();
			packet = configReader.getStruct("WS_DestroyGhostCmd", packet_version);
		}
		map<int32, Spawn*>::iterator itr;
		MSpawnList.readlock(__FUNCTION__, __LINE__);
		for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
			spawn = itr->second;
			if(spawn && !spawn->IsPlayer()){
				SendRemoveSpawn(client, spawn, packet);
			}
		}
		MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);

	DeleteTransporters();
	safe_delete(packet);	
	if(!repop && respawns_allowed){
		spawn_range_map.clear(true);
		MutexList<Spawn*> tmp_player_list; // Local variable, never be on another thread so probably don't need the extra mutex code that comes with a MutexList
		ClearDeadSpawns();

		map<int32, Spawn*>::iterator itr;
		MSpawnList.writelock(__FUNCTION__, __LINE__);
		for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
			spawn = itr->second;
			if (spawn) {
				if(spawn->GetRespawnTime() > 0 && spawn->GetSpawnLocationID() > 0)
					respawn_timers.Put(spawn->GetSpawnLocationID(), Timer::GetCurrentTime2() + spawn->GetRespawnTime()*1000);
				if(spawn->IsPlayer())
					tmp_player_list.Add(spawn);
				else {
					RemoveSpawnSupportFunctions(spawn);	
					AddPendingDelete(spawn);
				}
			}
		}
		spawn_list.clear();
		//add back just the clients
		MutexList<Spawn*>::iterator spawn_iter2 = tmp_player_list.begin();
		while(spawn_iter2.Next()) {
			spawn_list[spawn_iter2->value->GetID()] = spawn_iter2->value;
		}
		MSpawnList.releasewritelock(__FUNCTION__, __LINE__);
	}
	else
		DeleteData(false);

	if(repop)
		LoadingData = true;
}

void ZoneServer::Depop(bool respawns, bool repop) {
	respawns_allowed = respawns;
	repop_zone = repop;
	finished_depop = false;
	depop_zone = true;
}

bool ZoneServer::AddCloseSpawnsToSpawnGroup(Spawn* spawn, float radius){
	if(!spawn)
		return false;

	Spawn* close_spawn = 0;
	bool ret = true;
	map<int32, Spawn*>::iterator itr;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
		close_spawn = itr->second;
		if(close_spawn && close_spawn != spawn && !close_spawn->IsPlayer() && close_spawn->GetDistance(spawn) <= radius){
			if((spawn->IsNPC() && close_spawn->IsNPC()) || (spawn->IsGroundSpawn() && close_spawn->IsGroundSpawn()) || (spawn->IsObject() && close_spawn->IsObject()) || (spawn->IsWidget() && close_spawn->IsWidget()) || (spawn->IsSign() && close_spawn->IsSign())){
				if(close_spawn->GetSpawnGroupID() == 0){
					spawn->AddSpawnToGroup(close_spawn);
					close_spawn->AddSpawnToGroup(spawn);
				}
				else
					ret = false;
			}
		} 
	}
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
	return ret;
}

void ZoneServer::RepopSpawns(Client* client, Spawn* in_spawn){
	vector<Spawn*>* spawns = in_spawn->GetSpawnGroup();
	PacketStruct* packet = configReader.getStruct("WS_DestroyGhostCmd", client->GetVersion());;
	if(spawns){
		if(!packet)
			return;

		Spawn* spawn = 0;
		vector<Spawn*>::iterator itr;
		for(itr = spawns->begin(); itr != spawns->end(); itr++){
			spawn = *itr;
			SendRemoveSpawn(client, spawn, packet);
		}
	}
	safe_delete(spawns);
	SendRemoveSpawn(client, in_spawn, packet);
	spawn_check_add.Trigger();
	safe_delete(packet);
}

bool ZoneServer::CheckNPCAttacks(NPC* npc, Spawn* victim, Client* client){
	if(!npc || !victim)
		return true;

	if(client){
		if(client->IsConnected() && client->GetPlayer()->GetFactions()->ShouldAttack(npc->GetFactionID()) && npc->AttackAllowed((Entity*)victim, false)) {
			if(!npc->EngagedInCombat() && client->GetPlayer()->GetArrowColor(npc->GetLevel()) != ARROW_COLOR_GRAY && !client->GetPlayer()->IsStealthed()) {
				if(victim->IsEntity()) {
					if (npc->HasSpawnGroup()) {
						vector<Spawn*>* groupVec = npc->GetSpawnGroup();
						for (int32 i = 0; i < groupVec->size(); i++) {
							Spawn* group_member = groupVec->at(i);
							if (group_member && !group_member->EngagedInCombat() && group_member->Alive()) {
								CallSpawnScript(group_member, SPAWN_SCRIPT_AGGRO, victim);
								((NPC*)group_member)->AddHate((Entity*)victim, 50);
							}
						}
						safe_delete(groupVec);
					}
					else
					{
						npc->AddHate((Entity*)victim, 50);
					}
				}
				else {
					if (npc->HasSpawnGroup()) {
						vector<Spawn*>* groupVec = npc->GetSpawnGroup();
						for (int32 i = 0; i < groupVec->size(); i++) {
							Spawn* group_member = groupVec->at(i);
							if (group_member && !group_member->EngagedInCombat() && group_member->Alive()) {
								CallSpawnScript(group_member, SPAWN_SCRIPT_AGGRO, victim);
								((NPC*)group_member)->InCombat(true);
							}
						}
						safe_delete(groupVec);
					}
					else
						npc->InCombat(true);
				}
			}
		}
	}
	else{
		if(!npc->EngagedInCombat()){
			if(victim->IsEntity()) {
				if (npc->HasSpawnGroup()) {
					vector<Spawn*>* groupVec = npc->GetSpawnGroup();
					for (int32 i = 0; i < groupVec->size(); i++) {
						Spawn* group_member = groupVec->at(i);
						if (group_member && !group_member->EngagedInCombat() && group_member->Alive()) {
							CallSpawnScript(group_member, SPAWN_SCRIPT_AGGRO, victim);
							((NPC*)group_member)->AddHate((Entity*)victim, 50);
						}
					}
					safe_delete(groupVec);
				}
				else
				{
					npc->AddHate((Entity*)victim, 50);
				}
			}
			else {
				if (npc->HasSpawnGroup()) {
					vector<Spawn*>* groupVec = npc->GetSpawnGroup();
					for (int32 i = 0; i < groupVec->size(); i++) {
						Spawn* group_member = groupVec->at(i);
						if (group_member && !group_member->EngagedInCombat() && group_member->Alive()) {
							CallSpawnScript(group_member, SPAWN_SCRIPT_AGGRO, victim);
							((NPC*)group_member)->InCombat(true);
						}
					}
				}
				else
				{
					npc->InCombat(true);
				}
			}
		}
	}
	return true;
}

bool ZoneServer::CheckEnemyList(NPC* npc) {
	vector<int32> *factions;
	vector<int32>::iterator faction_itr;
	vector<int32> *spawns;
	vector<int32>::iterator spawn_itr;
	map<float, Spawn*> attack_spawns;
	map<float, Spawn*> reverse_attack_spawns;	
	map<float, Spawn*>::iterator itr;
	int32 faction_id = npc->GetFactionID();
	float distance;

	if (faction_id == 0)
		return true;

	m_enemy_faction_list.readlock(__FUNCTION__, __LINE__);
	if (enemy_faction_list.count(faction_id) > 0) {
		factions = enemy_faction_list[faction_id];

		for (faction_itr = factions->begin(); faction_itr != factions->end(); faction_itr++) {
			m_npc_faction_list.readlock(__FUNCTION__, __LINE__);
			if (npc_faction_list.count(*faction_itr) > 0) {
				spawns = npc_faction_list[*faction_itr];
				spawn_itr = spawns->begin();

				for (spawn_itr = spawns->begin(); spawn_itr != spawns->end(); spawn_itr++) {
					Spawn* spawn = GetSpawnByID(*spawn_itr);
					if (spawn) {
						if ((distance = spawn->GetDistance(npc)) <= npc->GetAggroRadius())
							attack_spawns[distance] = spawn;
					}
				}
			}
			m_npc_faction_list.releasereadlock(__FUNCTION__, __LINE__);
		}
	}
	m_enemy_faction_list.releasereadlock(__FUNCTION__, __LINE__);

	m_reverse_enemy_faction_list.readlock(__FUNCTION__, __LINE__);
	if (reverse_enemy_faction_list.count(faction_id) > 0) {
		factions = reverse_enemy_faction_list[faction_id];

		for (faction_itr = factions->begin(); faction_itr != factions->end(); faction_itr++) {
			m_npc_faction_list.readlock(__FUNCTION__, __LINE__);
			if (npc_faction_list.count(*faction_itr) > 0) {
				spawns = npc_faction_list[*faction_itr];
				spawn_itr = spawns->begin();

				for (spawn_itr = spawns->begin(); spawn_itr != spawns->end(); spawn_itr++) {
					Spawn* spawn = GetSpawnByID(*spawn_itr);
					if (spawn) {
						if ((distance = spawn->GetDistance(npc)) <= npc->GetAggroRadius())
							reverse_attack_spawns[distance] = spawn;
					}
				}
			}
			m_npc_faction_list.releasereadlock(__FUNCTION__, __LINE__);
		}
	}
	m_reverse_enemy_faction_list.releasereadlock(__FUNCTION__, __LINE__);

	if (attack_spawns.size() > 0) {
		for (itr = attack_spawns.begin(); itr != attack_spawns.end(); itr++)
			CheckNPCAttacks(npc, itr->second);
	}
	if (reverse_attack_spawns.size() > 0) {
		for (itr = reverse_attack_spawns.begin(); itr != reverse_attack_spawns.end(); itr++)
			CheckNPCAttacks((NPC*)itr->second, npc);
	}

	return attack_spawns.size() == 0;
}

void ZoneServer::RemoveDeadEnemyList(Spawn *spawn) 
{
	int32 faction_id = spawn->GetFactionID();
	vector<int32> *spawns;
	vector<int32>::iterator itr;

	LogWrite(ZONE__DEBUG, 7, "Zone", "Processing RemoveDeadEnemyList...");

	m_npc_faction_list.writelock(__FUNCTION__, __LINE__);
	if (npc_faction_list.count(faction_id) > 0) {
		spawns = npc_faction_list[faction_id];

		for (itr = spawns->begin(); itr != spawns->end(); itr++) {
			if (*itr == spawn->GetID()) {
				spawns->erase(itr);
				break;
			}
		}
	}
	m_npc_faction_list.releasewritelock(__FUNCTION__, __LINE__);
}

void ZoneServer::AddEnemyList(NPC* npc){
	int32 faction_id = npc->GetFactionID();
	vector<int32> *hostile_factions;
	vector<int32>::iterator itr;

	if(faction_id <= 9)
		return;

	if(!rule_manager.GetGlobalRule(R_Faction, AllowFactionBasedCombat)->GetBool()) {
		LogWrite(FACTION__WARNING, 0, "Faction", "Faction Combat is DISABLED via R_Faction::AllowFactionBasedCombat rule!");
		return;
	}
	
	m_npc_faction_list.readlock(__FUNCTION__, __LINE__);
	if (npc_faction_list.count(faction_id) == 0) {
		if(faction_id > 10) {
			if ((hostile_factions = master_faction_list.GetHostileFactions(faction_id)) != NULL) {
				itr = hostile_factions->begin();

				for (itr = hostile_factions->begin(); itr != hostile_factions->end(); itr++) {
					m_enemy_faction_list.writelock(__FUNCTION__, __LINE__);
					if (enemy_faction_list.count(faction_id) == 0)
						enemy_faction_list[faction_id] = new vector<int32>;
					enemy_faction_list[faction_id]->push_back(*itr);
					m_enemy_faction_list.releasewritelock(__FUNCTION__, __LINE__);

					m_reverse_enemy_faction_list.writelock(__FUNCTION__, __LINE__);
					if(reverse_enemy_faction_list.count(*itr) == 0)
						reverse_enemy_faction_list[*itr] = new vector<int32>;
					reverse_enemy_faction_list[*itr]->push_back(faction_id);
					m_reverse_enemy_faction_list.releasewritelock(__FUNCTION__, __LINE__);
				}
			}
		}

		/*m_enemy_faction_list.writelock(__FUNCTION__, __LINE__);
		if(enemy_faction_list.count(1) == 0)
			enemy_faction_list[1] = new vector<int32>;
		enemy_faction_list[1]->push_back(faction_id);
		m_enemy_faction_list.releasewritelock(__FUNCTION__, __LINE__);*/
	}
	m_npc_faction_list.releasereadlock(__FUNCTION__, __LINE__);

	m_npc_faction_list.writelock(__FUNCTION__, __LINE__);
	if(npc_faction_list.count(faction_id) == 0)
		npc_faction_list[faction_id] = new vector<int32>;
	npc_faction_list[faction_id]->push_back(npc->GetID());
	m_npc_faction_list.releasewritelock(__FUNCTION__, __LINE__);
}

void ZoneServer::CheckSpawnRange(Client* client, Spawn* spawn, bool initial_login){
	if(client && spawn && (initial_login || client->IsConnected())) {
		if(spawn != client->GetPlayer()) {
			if(spawn_range_map.count(client) == 0)
				spawn_range_map.Put(client, new MutexMap<int32, float >());
			spawn_range_map.Get(client)->Put(spawn->GetID(), spawn->GetDistance(client->GetPlayer()));
			if (!initial_login && client && spawn->IsNPC() && spawn_range_map.Get(client)->Get(spawn->GetID()) <= ((NPC*)spawn)->GetAggroRadius() && !client->GetPlayer()->GetInvulnerable()) {
				CheckNPCAttacks((NPC*)spawn, client->GetPlayer(), client);
			}
		} 

		if(!initial_login && player_proximities.size() > 0 && player_proximities.count(spawn->GetID()) > 0)
			CheckPlayerProximity(spawn, client);
	}
}

void ZoneServer::CheckSpawnRange(Spawn* spawn){
	vector<Client*>::iterator client_itr;
	Client* client = 0;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		if (client && client->IsReadyForSpawns()) {
			CheckSpawnRange(client, spawn);
		}
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::PrepareSpawnID(Player* player, Spawn* spawn){
	player->spawn_id++;
	player->player_spawn_id_map[player->spawn_id] = spawn;
	player->player_spawn_reverse_id_map[spawn] = player->spawn_id;
}

void ZoneServer::CheckSendSpawnToClient(Client* client, bool initial_login) {
	if (!client) {
		LogWrite(ZONE__ERROR, 0, "Zone", "CheckSendSpawnToClient called with an invalid client");
		return;
	}

	if (initial_login){
		SetSpawnStructs(client);
	}
	else if (!client->GetInitialSpawnsSent())
		return;

	Spawn* spawn = 0;
	map<float, vector<Spawn*>* > closest_spawns;
	if(spawn_range_map.count(client) > 0) {
		if(initial_login || client->IsConnected()) {
			MutexMap<int32, float >::iterator spawn_iter = spawn_range_map.Get(client)->begin();
			while(spawn_iter.Next()) {
				spawn = GetSpawnByID(spawn_iter->first);
				if(spawn && spawn->GetPrivateQuestSpawn()) {
					if(!spawn->IsPrivateSpawn())
						spawn->AddAllowAccessSpawn(spawn);
					if(spawn->MeetsSpawnAccessRequirements(client->GetPlayer())) {
						if(spawn->IsPrivateSpawn() && !spawn->AllowedAccess(client->GetPlayer()))
							spawn->AddAllowAccessSpawn(client->GetPlayer());
					}
					else if(spawn->AllowedAccess(client->GetPlayer()))
						spawn->RemoveSpawnAccess(client->GetPlayer());
				}
				if(spawn && spawn != client->GetPlayer() && client->GetPlayer()->ShouldSendSpawn(spawn)) {
					if((!initial_login && spawn_iter->second <= SEND_SPAWN_DISTANCE) || (initial_login && (spawn_iter->second <= 40 || spawn->IsWidget()))) {
						if(closest_spawns.count(spawn_iter->second) == 0)
							closest_spawns[spawn_iter->second] = new vector<Spawn*>();
						closest_spawns[spawn_iter->second]->push_back(spawn);
						PrepareSpawnID(client->GetPlayer(), spawn);
					}
				}
			}
		}
		vector<Spawn*>::iterator spawn_iter2;
		map<float, vector<Spawn*>* >::iterator itr;
		for(itr = closest_spawns.begin(); itr != closest_spawns.end(); itr++) {
			for(spawn_iter2 = itr->second->begin(); spawn_iter2 != itr->second->end(); spawn_iter2++) {
				spawn = *spawn_iter2;
				client->GetPlayer()->ClearRemovedSpawn(spawn);
				SendSpawn(spawn, client);
				if(client->ShouldTarget() && client->GetCombineSpawn() == spawn)
					client->TargetSpawn(spawn);
			}
			delete itr->second;
		}
	}

	if (initial_login)
		client->SetInitialSpawnsSent(true);
}

void ZoneServer::CheckSendSpawnToClient(){
	vector<Client*>::iterator itr;
	Client* client = 0;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (itr = clients.begin(); itr != clients.end(); itr++) {
		client = *itr;
		if (client->IsReadyForSpawns()) {
			CheckSendSpawnToClient(client);
		}
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::CheckRemoveSpawnFromClient(Spawn* spawn) {
	vector<Client*>::iterator itr;
	Client* client = 0;
	PacketStruct* packet = 0;
	int16 packet_version = 0;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (itr = clients.begin(); itr != clients.end(); itr++) {
		client = *itr;
		if(client){
			if(!packet || packet_version != client->GetVersion()){
				safe_delete(packet);
				packet_version = client->GetVersion();
				packet = configReader.getStruct("WS_DestroyGhostCmd", packet_version);
			}

			if (spawn && spawn != client->GetPlayer() && client->GetPlayer()->WasSentSpawn(spawn->GetID()) && client->GetPlayer()->WasSpawnRemoved(spawn) == false) {
				if (spawn_range_map.Get(client)->Get(spawn->GetID()) > REMOVE_SPAWN_DISTANCE && !spawn->IsWidget()) {
					SendRemoveSpawn(client, spawn, packet);
				}
			}
		}
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
	safe_delete(packet);
}

bool ZoneServer::CombatProcess(Spawn* spawn) {
	bool ret = true;

	if (spawn && spawn->IsEntity())
		((Entity*)spawn)->ProcessCombat();

	return ret;
}

void ZoneServer::AddDelayedSpawnRemove(Spawn* spawn, int32 time) {
	// this prevents a crash when a zone shuts down with a client in it
	if (zoneShuttingDown)
		return;

	if (delayed_spawn_remove_list.count(spawn->GetID(), true) == 0)
		delayed_spawn_remove_list.Put(spawn->GetID(), Timer::GetCurrentTime2() + time);
}

void ZoneServer::RemoveDelayedSpawnRemove(Spawn* spawn) {
	if (delayed_spawn_remove_list.count(spawn->GetID(), true) > 0)
		delayed_spawn_remove_list.erase(spawn->GetID());
}

void ZoneServer::DelayedSpawnRemoval(bool force_delete_all) {
	if (delayed_spawn_remove_list.size(true) > 0) {
		MutexMap<int32, int32>::iterator itr = delayed_spawn_remove_list.begin();
		int32 current_time = Timer::GetCurrentTime2();
		Spawn* spawn = 0;
		// Cycle through all spawns scheduled for removal
		while (itr.Next()) {
			// If it is time for removal, or a force delete of all, then remove the spawn
			if (force_delete_all || current_time >= itr->second) {
				spawn = GetSpawnByID(itr->first);
				delayed_spawn_remove_list.erase(itr->first);
				if (spawn) {
					if (spawn->IsEntity()) {
						// Remove pets
						((Entity*)spawn)->DismissPet((NPC*)((Entity*)spawn)->GetPet());
						((Entity*)spawn)->DismissPet((NPC*)((Entity*)spawn)->GetCharmedPet());
						((Entity*)spawn)->DismissPet((NPC*)((Entity*)spawn)->GetDeityPet());
						((Entity*)spawn)->DismissPet((NPC*)((Entity*)spawn)->GetCosmeticPet());
					}

					if (spawn->IsPlayer())
						RemoveSpawn(spawn, false);
					else
						RemoveSpawn(spawn);
				}
			}
		}
	}
}

void ZoneServer::AddPendingDelete(Spawn* spawn) {
	MSpawnDeleteList.writelock(__FUNCTION__, __LINE__);
	if(spawn_delete_list.count(spawn) == 0)
		spawn_delete_list.insert(make_pair(spawn, Timer::GetCurrentTime2() + spawn_delete_timer)); //give other threads up to 30 seconds to stop using this spawn reference
	MSpawnDeleteList.releasewritelock(__FUNCTION__, __LINE__);
}

void ZoneServer::DeleteSpawns(bool delete_all) {
	MSpawnDeleteList.writelock(__FUNCTION__, __LINE__);
	if(spawn_delete_list.size() > 0){
		map<Spawn*, int32>::iterator itr;
		map<Spawn*, int32>::iterator erase_itr;
		int32 current_time = Timer::GetCurrentTime2();
		Spawn* spawn = 0;
		for (itr = spawn_delete_list.begin(); itr != spawn_delete_list.end(); ) {
			if (delete_all || current_time >= itr->second){
				spawn = itr->first;
				erase_itr = itr++;
				spawn_delete_list.erase(erase_itr);
				safe_delete(spawn);
			}
			else
				itr++;
		}
	}
	MSpawnDeleteList.releasewritelock(__FUNCTION__, __LINE__);
}

void ZoneServer::AddDamagedSpawn(Spawn* spawn){
	if (spawn)
		damaged_spawns.Add(spawn->GetID());
}

void ZoneServer::RemoveDamagedSpawn(Spawn* spawn){
	if (spawn)
		damaged_spawns.Remove(spawn->GetID());
}

bool ZoneServer::Process() 
{
	MMasterZoneLock->lock(); //Changing this back to a recursive lock to fix a possible /reload spells crash with multiple zones running - Foof
#ifndef NO_CATCH
	try
	{
#endif
		if(LoadingData){
			while(zoneID == 0){ //this is loaded by world
				Sleep(10);
			}

			if (reloading) {
				LogWrite(COMMAND__DEBUG, 0, "Command", "-Loading Entity Commands...");
				database.LoadEntityCommands(this);
				LogWrite(NPC__INFO, 0, "NPC", "-Loading NPC data...");
				database.LoadNPCs(this);
				LogWrite(NPC__INFO, 0, "NPC", "-Load NPC data complete!");

				LogWrite(OBJECT__INFO, 0, "Object", "-Loading Object data...");
				database.LoadObjects(this);
				LogWrite(OBJECT__INFO, 0, "Object", "-Load Object data complete!");

				LogWrite(SIGN__INFO, 0, "Sign", "-Loading Sign data...");
				database.LoadSigns(this);
				LogWrite(SIGN__INFO, 0, "Sign", "-Load Sign data complete!");

				LogWrite(WIDGET__INFO, 0, "Widget", "-Loading Widget data...");
				database.LoadWidgets(this);
				LogWrite(WIDGET__INFO, 0, "Widget", "-Load Widget data complete!");

				LogWrite(GROUNDSPAWN__INFO, 0, "GSpawn", "-Loading Groundspawn data...");
				database.LoadGroundSpawns(this);
				database.LoadGroundSpawnEntries(this);
				LogWrite(GROUNDSPAWN__INFO, 0, "GSpawn", "-Load Groundspawn data complete!");

				LogWrite(PET__INFO, 0, "Pet", "-Loading Pet data...");
				database.GetPetNames(this);	
				LogWrite(PET__INFO, 0, "Pet", "-Load Pet data complete!");

				LogWrite(LOOT__INFO, 0, "Loot", "-Loading Spawn loot data...");
				database.LoadLoot(this);
				LogWrite(LOOT__INFO, 0, "Loot", "-Loading Spawn loot data complete!");

				LogWrite(TRANSPORT__INFO, 0, "Transport", "-Loading Transporters...");
				database.LoadTransporters(this);
				LogWrite(TRANSPORT__INFO, 0, "Transport", "-Loading Transporters complete!");
				reloading = false;
			}

			MSpawnGroupAssociation.writelock(__FUNCTION__, __LINE__);
			spawn_group_associations.clear();
			MSpawnGroupAssociation.releasewritelock(__FUNCTION__, __LINE__);

			MSpawnGroupLocations.writelock(__FUNCTION__, __LINE__);
			spawn_group_locations.clear();
			MSpawnGroupLocations.releasewritelock(__FUNCTION__, __LINE__);

			MSpawnLocationGroups.writelock(__FUNCTION__, __LINE__);
			spawn_location_groups.clear();
			MSpawnLocationGroups.releasewritelock(__FUNCTION__, __LINE__);

			MSpawnGroupChances.writelock(__FUNCTION__, __LINE__);
			spawn_group_chances.clear();
			MSpawnGroupChances.releasewritelock(__FUNCTION__, __LINE__);

			DeleteTransporters();
			ReloadTransporters();

			database.LoadSpawns(this);
			ProcessSpawnLocations();

			if (!revive_points)
				revive_points = new vector<RevivePoint*>;
			else {
				while (!revive_points->empty()){
					safe_delete(revive_points->back());
					revive_points->pop_back();
				}
			}
			database.LoadRevivePoints(revive_points, GetZoneID());

			LoadingData = false;

			spawn_range.Trigger();
			spawn_check_add.Trigger();

			RemoveLocationGrids();
			database.LoadLocationGrids(this);

			const char* zone_script = world.GetZoneScript(this->GetZoneID());
			if(lua_interface && zone_script) {
				RemoveLocationProximities();
				lua_interface->RunZoneScript(zone_script, "init_zone_script", this);
			}
		}

		if(shutdownTimer.Enabled() && shutdownTimer.Check())
			zoneShuttingDown = true;

		if (reloading_spellprocess){
			MMasterZoneLock->unlock();
			return !zoneShuttingDown;
		}

		// Remove LD Player whos LD timer has expired
		if (!zoneShuttingDown)
			DelayedSpawnRemoval(false);

		// client loop
		if(charsheet_changes.Check())
			SendCharSheetChanges();

		// Client loop
		ClientProcess();

		if(spellProcess)
			spellProcess->Process();
		if (tradeskillMgr)
			tradeskillMgr->Process();
		
		// Client loop
		if(client_save.Check())
			SaveClients();

		// Possibility to do a client loop
		if(weather_enabled && weatherTimer.Check())
			ProcessWeather();

		// client related loop, move to main thread?
		if(!zoneShuttingDown)
			ProcessDrowning();

		// client than location_proximities loop, move to main thread
		if (location_prox_timer.Check() && !zoneShuttingDown)
			CheckLocationProximity();

		// client than location_grid loop, move to main thread
		if (location_grid_timer.Check() && !zoneShuttingDown)
			CheckLocationGrids();

		if (sync_game_time_timer.Check() && !zoneShuttingDown)
			SendTimeUpdateToAllClients();

		if(lua_interface) {
			lua_interface->Process();

			/* refactor this? */
			const char* zone_script = world.GetZoneScript(GetZoneID());
			if (zone_script) {
				
				int hour = world.GetWorldTimeStruct()->hour;
				int minute = world.GetWorldTimeStruct()->minute;
				
				if (!isDusk && ((hour > dusk_hour || hour < dawn_hour) || ((dusk_hour == hour && minute >= dusk_minute) || (hour == dawn_hour && minute < dawn_minute)))) {
					isDusk = true;
					lua_interface->RunZoneScript(zone_script, "dusk", this);
				}
				else if (isDusk && ((hour > dawn_hour && hour < dusk_hour) || ((hour == dawn_hour && minute >= dawn_minute) || (hour == dusk_hour && minute < dusk_minute)))) {
					isDusk = false;
					lua_interface->RunZoneScript(zone_script, "dawn", this);
				}
			}
		}

		// damaged spawns loop, spawn related, move to spawn thread?
		if(regenTimer.Check())
			RegenUpdate();

		// heading_timers loop
		if(!zoneShuttingDown)
			CheckHeadingTimers();

		// respawn_timers loop
		if(respawn_timer.Check() && !zoneShuttingDown)
			CheckRespawns();

		// spawn_expire_timers loop
		if (spawn_expire_timer.Check() && !zoneShuttingDown)
			CheckSpawnExpireTimers();

		// widget_timers loop
		if(widget_timer.Check() && !zoneShuttingDown)
			CheckWidgetTimers();

		// spawn_script_timers loop
		if(!zoneShuttingDown)
			CheckSpawnScriptTimers();

		// Check to see if a dead spawn needs to be removed
		CheckDeadSpawnRemoval();
#ifndef NO_CATCH
	}
	catch(...)
	{
		LogWrite(ZONE__ERROR, 0, "Zone", "Exception while running '%s'", GetZoneName());
		zoneShuttingDown = true;
		MMasterZoneLock->unlock();
		return false;
	}
#endif
	MMasterZoneLock->unlock();
	return (zoneShuttingDown == false);
}

bool ZoneServer::SpawnProcess(){
	if(depop_zone) {
		depop_zone = false;
		ProcessDepop(respawns_allowed, repop_zone);
		finished_depop = true;
	}

	MMasterSpawnLock.writelock(__FUNCTION__, __LINE__);
	// If the zone is loading data or shutting down don't do anything
	if(!LoadingData && !zoneShuttingDown && !reloading_spellprocess) {
		// send spawn changes, changed_spawns loop
		if(spawn_update.Check() && !zoneShuttingDown) { //check for changed spawns every {Rule:SpawnUpdateTimer} milliseconds (default: 200ms)
			SendSpawnChanges();
		}

		// Set some bool's for timers
		bool movement = movement_timer.Check();
		bool spawnRange = spawn_range.Check();
		bool checkRemove = spawn_check_remove.Check();
		bool aggroCheck = aggro_timer.Check();
		vector<int32> pending_spawn_list_remove;

		// Loop through the spawn list
		map<int32, Spawn*>::iterator itr;
		MSpawnList.readlock(__FUNCTION__, __LINE__);
		// Loop throught the list to set up spawns to be sent to clients
		for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
			// if zone is shutting down kill the loop
			if (zoneShuttingDown)
				break;

			Spawn* spawn = itr->second;
			if (spawn) {
				// Checks the range to all clients in the zone
				if (spawnRange)
					CheckSpawnRange(spawn);

				// Checks to see if the spawn needs to be removed from a client
				if (checkRemove)
					CheckRemoveSpawnFromClient(spawn);
			}
		}
		MSpawnList.releasereadlock(__FUNCTION__, __LINE__);

		// Broke the spawn loop into 2 so spawns are sent to the client faster, send the spawns to clients now then resume the spawn loop

		// client loop, move to main thread?
		// moved this back to the spawn thread as on the main thread during a depop, done on the spawn thread, spawns would start to pop again
		// might be an issue with other functions moved from the spawn thread to the main thread?
		if(spawn_check_add.Check() && !zoneShuttingDown)
			CheckSendSpawnToClient();

		MSpawnList.readlock(__FUNCTION__, __LINE__);
		for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
			// Break the loop if the zone is shutting down
			if (zoneShuttingDown)
				break;

			Spawn* spawn = itr->second;
			if (spawn) {
				// Process spawn movement
				if (movement) {
					spawn->ProcessMovement();

					// update last_movement_update for all spawns (used for time_step)
					spawn->last_movement_update = Timer::GetCurrentTime2();
				}

				// Makes NPC's KOS to other NPC's or players
				if (aggroCheck)
					ProcessAggroChecks(spawn);

				// Process combat for the spawn
				CombatProcess(spawn);
			}
			else {
				// unable to get a valid spawn, lets add the id to another list to remove from the spawn list after this loop is finished
				pending_spawn_list_remove.push_back(itr->first);
			}

		}
		MSpawnList.releasereadlock(__FUNCTION__, __LINE__);

		// Check to see if there are any spawn id's that need to be removed from the spawn list, if so remove them all
		if (pending_spawn_list_remove.size() > 0) {
			MSpawnList.writelock(__FUNCTION__, __LINE__);
			vector<int32>::iterator itr2;
			for (itr2 = pending_spawn_list_remove.begin(); itr2 != pending_spawn_list_remove.end(); itr2++) 
				spawn_list.erase(*itr2);

			pending_spawn_list_remove.clear();
			MSpawnList.releasewritelock(__FUNCTION__, __LINE__);
		}

		// Check to see if there are spawns waiting to be added to the spawn list, if so add them all
		if (pending_spawn_list_add.size() > 0) {
			MSpawnList.writelock(__FUNCTION__, __LINE__);
			MPendingSpawnListAdd.writelock(__FUNCTION__, __LINE__);
			list<Spawn*>::iterator itr2;
			for (itr2 = pending_spawn_list_add.begin(); itr2 != pending_spawn_list_add.end(); itr2++) {
				Spawn* spawn = *itr2;
				if (spawn)
					spawn_list[spawn->GetID()] = spawn;
			}

			pending_spawn_list_add.clear();
			MPendingSpawnListAdd.releasewritelock(__FUNCTION__, __LINE__);
			MSpawnList.releasewritelock(__FUNCTION__, __LINE__);
		}
		

		// Do other loops for spawns
		// tracking, client loop with spawn loop for each client that is tracking, change to a spawn_range_map loop instead of using the main spawn list?
		//if (tracking_timer.Check())
			//ProcessTracking(); // probably doesn't work as spawn loop iterator is never set


		// Delete unused spawns, do this last
		if(!zoneShuttingDown)
			DeleteSpawns(false);

		// Nothing should come after this


		//LogWrite(PLAYER__ERROR, 0, "Debug", "Spawn loop time %u", Timer::GetCurrentTime2() - time);
	}

	MMasterSpawnLock.releasewritelock(__FUNCTION__, __LINE__);
	
	return (zoneShuttingDown == false);
}

void ZoneServer::CheckDeadSpawnRemoval() {
	MDeadSpawns.writelock(__FUNCTION__, __LINE__);
		if(dead_spawns.size() > 0){
			vector<Spawn*> tmp_dead_list;			
			int32 current_time = Timer::GetCurrentTime2();
			Spawn* spawn = 0;
			map<int32, int32>::iterator itr = dead_spawns.begin();
			map<int32, int32>::iterator itr_delete;
			while (itr != dead_spawns.end()) {
				spawn = GetSpawnByID(itr->first);
				if (spawn) {
					if(current_time >= itr->second)
						tmp_dead_list.push_back(spawn);
					itr++;
				}
				else {
					itr_delete = itr++;
					dead_spawns.erase(itr_delete);
				}
			}
			for(int i=tmp_dead_list.size()-1;i>=0;i--){
				spawn = tmp_dead_list[i];
				if (!spawn->IsPlayer())
				{
					dead_spawns.erase(spawn->GetID());
					RemoveSpawn(spawn, true, true, false);
				}
			}
		}
		MDeadSpawns.releasewritelock(__FUNCTION__, __LINE__);
}

void ZoneServer::CheckRespawns(){	
	vector<int32> tmp_respawn_list;
	MutexMap<int32, int32>::iterator itr = respawn_timers.begin();
	while(itr.Next()){
		if(Timer::GetCurrentTime2() >= itr->second)
			tmp_respawn_list.push_back(itr->first);
	}
	for(int i=tmp_respawn_list.size()-1;i>=0;i--){
		if ( IsInstanceZone() )
		{
			if ( database.DeleteInstanceSpawnRemoved(GetInstanceID(),tmp_respawn_list[i]) )
			{
			}
			else
			{
			}
		}

		ProcessSpawnLocation(tmp_respawn_list[i], true);
		respawn_timers.erase(tmp_respawn_list[i]);
	}
}

void ZoneServer::CheckSpawnExpireTimers() {
	MutexMap<int32, int32>::iterator itr = spawn_expire_timers.begin();
	while (itr.Next()) {
		Spawn* spawn = GetSpawnByID(itr->first);
		if (spawn) {
			if (Timer::GetCurrentTime2() >= itr.second) {
				spawn_expire_timers.erase(itr.first);
				Despawn(spawn, spawn->GetRespawnTime());
			}
		}
		else
			spawn_expire_timers.erase(itr->first);
	}
}

void ZoneServer::AddSpawnExpireTimer(Spawn* spawn, int32 expire_time, int32 expire_offset) {
	if (spawn) {
		int32 actual_expire_time = expire_time;
		if (expire_offset > 0) {
			int32 low = expire_time;
			int32 high = expire_time + expire_offset;
			if (expire_offset < expire_time)
				low = expire_time - expire_offset;
			int32 range = (high - low) + 1;
			actual_expire_time = (low + (int32)((range * rand()) / (RAND_MAX + 1.0)));
		}
		actual_expire_time *= 1000;
		spawn_expire_timers.Put(spawn->GetID(), Timer::GetCurrentTime2() + actual_expire_time);
	}
}

void ZoneServer::SaveClient(Client* client){
	client->Save();
}

void ZoneServer::SaveClients(){
	vector<Client*>::iterator itr;
	Client* client = 0;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (itr = clients.begin(); itr != clients.end(); itr++) {
		client = *itr;
		if(client->IsConnected()){
			SaveClient(client);	
		}
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::SendSpawnVisualState(Spawn* spawn, int16 type){
	if(!spawn)
		return;

	vector<Client*>::iterator itr;
	spawn->SetTempVisualState(type);
	Client* client = 0;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (itr = clients.begin(); itr != clients.end(); itr++) {
		client = *itr;
		if(client && client->GetPlayer() != spawn)
			AddChangedSpawn(spawn);
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::ResendSpawns(Client* client) {
	Spawn* spawn = 0;

	if (spawn_range_map.count(client) > 0) {
		MutexMap<int32, float >::iterator itr = spawn_range_map.Get(client)->begin();

		while (itr.Next()) {
			spawn = GetSpawnByID(itr->first);
			if (spawn && client->GetPlayer()->GetFactions()->ShouldAttack(spawn->GetFactionID())) {
				SendSpawnChanges(spawn, client, false, true);
				Sleep(5);
			}
		}
	}
}

void ZoneServer::SendSpawnChanges(int32 spawn_id, Client* client, bool override_changes, bool override_vis_changes){
	Spawn* spawn = GetSpawnByDatabaseID(spawn_id);
	if(spawn && (spawn->changed || override_changes || override_vis_changes))
		SendSpawnChanges(spawn, client, override_changes, override_vis_changes);
}

void ZoneServer::SendSpawnChanges(Spawn* spawn, Client* client, bool override_changes, bool override_vis_changes){
	if(client && client->GetPlayer()->WasSentSpawn(spawn->GetID()) && !client->GetPlayer()->WasSpawnRemoved(spawn) && client->GetPlayer()->GetDistance(spawn) <REMOVE_SPAWN_DISTANCE){
		EQ2Packet* outapp = spawn->spawn_update_packet(client->GetPlayer(), client->GetVersion(), override_changes, override_vis_changes);
		if(outapp)
			client->QueuePacket(outapp);
	}
}

void ZoneServer::SendSpawnChanges(Spawn* spawn){
	if(spawn && spawn->changed){
		if(!spawn->IsPlayer() || (spawn->IsPlayer() && (spawn->info_changed || spawn->vis_changed))){
			vector<Client*>::iterator itr;
			Client* client = 0;

			MClientList.readlock(__FUNCTION__, __LINE__);
			for (itr = clients.begin(); itr != clients.end(); itr++) {
				client = *itr;

				if (spawn->IsPlayer() && client->GetPlayer()->CanAttackTarget((Player*)spawn) && ((Player*)spawn)->IsStealthed() && !client->GetPlayer()->stats[ITEM_STAT_SEESTEALTH]) {
					int16 packet_version = client->GetVersion();
					PacketStruct *packet = configReader.getStruct("WS_DestroyGhostCmd", packet_version);

					SendRemoveSpawn(client, (Player*)spawn, packet);

					safe_delete(packet);
				} else {
					SendSpawnChanges(spawn, client);
				}
			}
			MClientList.releasereadlock(__FUNCTION__, __LINE__);
		}
		spawn->changed = false;
		spawn->info_changed = false;
		if(spawn->IsPlayer() == false)
			spawn->position_changed = false;
		spawn->vis_changed = false;
	}
}

Spawn* ZoneServer::FindSpawn(Player* searcher, const char* name){
	if(!searcher || !name)
		return 0;

	Spawn* spawn = 0;
	vector<Spawn*> find_spawn_list;
	vector<Spawn*>::iterator fspawn_iter;
	int8 name_size = strlen(name);
	map<int32, Spawn*>::iterator itr;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
		spawn = itr->second;
		if(spawn && !strncasecmp(spawn->GetName(), name, name_size))
			find_spawn_list.push_back(spawn);
	}
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
	Spawn* closest = 0;
	float distance = 0;
	float test_distance = 0;
	for(fspawn_iter=find_spawn_list.begin(); fspawn_iter!=find_spawn_list.end(); fspawn_iter++){
		spawn = *fspawn_iter;
		if(spawn && ((((test_distance = searcher->GetDistance(spawn)) < distance)) || !closest)){
			distance = test_distance;
			closest = spawn;
		}
	}
	return closest;
}

void ZoneServer::AddChangedSpawn(Spawn* spawn){
	if(!spawn || (spawn->IsPlayer() && !spawn->info_changed && !spawn->vis_changed))
		return;
	if (changed_spawns.count(spawn->GetID()) == 0)
		changed_spawns.Add(spawn->GetID());
}

void ZoneServer::RemoveChangedSpawn(Spawn* spawn){
	if (spawn)
		changed_spawns.Remove(spawn->GetID());
}

void ZoneServer::AddDrowningVictim(Player* player){
	Client* client = GetClientBySpawn(player);
	if(client && drowning_victims.count(client) == 0)
		drowning_victims.Put(client, Timer::GetCurrentTime2());
}

void ZoneServer::RemoveDrowningVictim(Player* player){
	Client* client = GetClientBySpawn(player);
	if(client)
		drowning_victims.erase(client);
}

Client* ZoneServer::GetDrowningVictim(Player* player){
	Client* client = GetClientBySpawn(player);
	if(client && drowning_victims.count(client) > 0)
		return(client);
	return 0;
}

void ZoneServer::ProcessDrowning(){
	vector<Client*> dead_list;
	if(drowning_victims.size(true) > 0){
		sint32 damage = 0;
		int32 current_time = Timer::GetCurrentTime2();
		MutexMap<Client*, int32>::iterator itr = drowning_victims.begin();
		while(itr.Next()){
			if(current_time >= itr->second) {
				Client* client = itr->first;
				Player* player = client->GetPlayer();
				drowning_victims.Get(client) = Timer::GetCurrentTime2() + 2000;
				damage = player->GetTotalHP()/20 + player->GetHPRegen();
				player->TakeDamage(damage);
				if(player->GetHP() == 0)
					dead_list.push_back(client);
				player->SetCharSheetChanged(true);
				SendCharSheetChanges(client);
				SendDamagePacket(0, player, DAMAGE_PACKET_TYPE_SIMPLE_DAMAGE, DAMAGE_PACKET_RESULT_SUCCESSFUL, DAMAGE_PACKET_DAMAGE_TYPE_DROWN, damage, 0);
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are drowning!");
			}
		}
	}
	if(dead_list.size() > 0){
		vector<Client*>::iterator itr;
		for(itr = dead_list.begin(); itr != dead_list.end(); itr++){
			RemoveDrowningVictim((*itr)->GetPlayer());
			KillSpawn((*itr)->GetPlayer(), 0);
			(*itr)->SimpleMessage(CHANNEL_COLOR_WHITE, "You are sleeping with the fishes!  Glug, glug...");
		}
	}
}

void ZoneServer::SendSpawnChanges(){	
	Spawn* spawn = 0;
	MutexList<int32>::iterator spawn_iter = changed_spawns.begin();
	while(spawn_iter.Next()){		
		spawn = GetSpawnByID(spawn_iter->value);
		if(spawn && spawn->changed){
			if(!spawn->IsPlayer() || (spawn->IsPlayer() && (spawn->info_changed || spawn->vis_changed))){
				SendSpawnChanges(spawn);
			}
		}
		if (!spawn)
			changed_spawns.Remove(spawn_iter->value);
	}
	changed_spawns.clear();
}

void ZoneServer::SendPlayerPositionChanges(Player* player){
	if(player){
		player->position_changed = false;
		Client* client = 0;
		vector<Client*>::iterator client_itr;

		MClientList.readlock(__FUNCTION__, __LINE__);
		for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
			client = *client_itr;

			if (player != client->GetPlayer() && client->GetPlayer()->WasSentSpawn(player->GetID()) && client->GetPlayer()->WasSpawnRemoved(player) == false) {
				EQ2Packet* outapp = player->player_position_update_packet(client->GetPlayer(), client->GetVersion());
				if (outapp)
					client->QueuePacket(outapp);
			}
		}
		MClientList.releasereadlock(__FUNCTION__, __LINE__);
	}
}

void ZoneServer::SendCharSheetChanges(){
	vector<Client*>::iterator client_itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++)
		SendCharSheetChanges(*client_itr);
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::SendCharSheetChanges(Client* client){
	if(client && client->IsConnected() && client->GetPlayer()->GetCharSheetChanged()){
		client->GetPlayer()->SetCharSheetChanged(false);
		ClientPacketFunctions::SendCharacterSheet(client);
	}
}

int32 ZoneServer::CalculateSpawnGroup(SpawnLocation* spawnlocation, bool respawn)
{
	int32 group = 0;
	list<int32>* groups_at_location = GetSpawnGroupsByLocation(spawnlocation->placement_id);

	LogWrite(SPAWN__TRACE, 0, "Spawn", "Enter %s", __FUNCTION__);

	if(groups_at_location){
		list<int32>::iterator group_location_itr;
		float chance = 0;
		float total_chance = 0;
		map<int32, float> tmp_chances;		
		set<int32>* associated_groups = 0;
		for (group_location_itr = groups_at_location->begin(); group_location_itr != groups_at_location->end(); group_location_itr++) {
			if(tmp_chances.count(*group_location_itr) > 0)
				continue;
			associated_groups = GetAssociatedGroups(*group_location_itr);
			if(associated_groups){
				set<int32>::iterator group_itr;
				for (group_itr = associated_groups->begin(); group_itr != associated_groups->end(); group_itr++) {
					chance = GetSpawnGroupChance(*group_itr);
					if(chance > 0){
						total_chance += chance;
						tmp_chances[*group_itr] = chance;
					}
					else
						tmp_chances[*group_itr] = 0;
				}
			}
			else{ //single group, no associations
				chance = GetSpawnGroupChance(*group_location_itr);
				total_chance += chance;
				tmp_chances[*group_location_itr] = chance;				
			}
		}
		if(tmp_chances.size() > 1){
			//set the default for any chances not set
			map<int32, float>::iterator itr2;
			for(itr2 = tmp_chances.begin(); itr2 != tmp_chances.end(); itr2++){
				if(itr2->second == 0){
					total_chance += 100/tmp_chances.size();
					tmp_chances[itr2->first] = 100/tmp_chances.size();
				}
			}
		}
		if(tmp_chances.size() > 1){
			float roll = (float)(rand()%((int32)total_chance));
			map<int32, float>::iterator itr3;
			for (itr3 = tmp_chances.begin(); itr3 != tmp_chances.end(); itr3++){
				if(itr3->second >= roll){
					group = itr3->first;
					break;
				}
				else
					roll -= itr3->second;
			}
		}
		else if(tmp_chances.size() == 1)
			group = tmp_chances.begin()->first;
	}
	if(group > 0){
		map<int32, int32>* locations = GetSpawnLocationsByGroup(group);
		if(locations){
			map<int32, int32>::iterator itr;
			Spawn* spawn = 0;
			Spawn* leader = 0;

			MSpawnLocationList.readlock(__FUNCTION__, __LINE__);
			for (itr = locations->begin(); itr != locations->end(); itr++) {
				if(spawn_location_list.count(itr->second) > 0){
					spawn = ProcessSpawnLocation(spawn_location_list[itr->second], respawn);
					if(!leader && spawn)
						leader = spawn;
					if(leader)
						leader->AddSpawnToGroup(spawn);
					if(spawn){						
						//if(spawn_group_map.count(group) == 0)
						//	spawn_group_map.Put(group, new MutexList<Spawn*>());
						MutexList<int32>* groupList = &spawn_group_map.Get(group);
						groupList->Add(spawn->GetID());
						spawn->SetSpawnGroupID(group);
					}
				}
			}
			MSpawnLocationList.releasereadlock(__FUNCTION__, __LINE__);
		}
	}

	LogWrite(SPAWN__TRACE, 0, "Spawn", "Exit %s", __FUNCTION__);

	return group;
}

void ZoneServer::ProcessSpawnLocation(int32 location_id, bool respawn)
{
	LogWrite(SPAWN__TRACE, 0, "Spawn", "Enter %s", __FUNCTION__);

	MSpawnLocationList.readlock(__FUNCTION__, __LINE__);

	if(spawn_location_list.count(location_id) > 0)
	{
		if(respawn) //see if there are any spawns still in game associated with this spawn's group, if so, dont spawn this
		{ 
			list<int32>* groups = GetSpawnGroupsByLocation(spawn_location_list[location_id]->placement_id);

			if(groups)
			{
				set<int32>* associated_groups = 0;
				bool should_spawn = true;
				list<int32>::iterator itr;
				for (itr = groups->begin(); itr != groups->end(); itr++) {
					associated_groups = GetAssociatedGroups(*itr);

					if(associated_groups)
					{
						set<int32>::iterator assoc_itr;
						for (assoc_itr = associated_groups->begin(); assoc_itr != associated_groups->end(); assoc_itr++) {
							if(spawn_group_map.count(*assoc_itr) > 0 && spawn_group_map.Get(*assoc_itr).size() > 0)
								should_spawn = false;
						}
					}
				}

				if(should_spawn)
					CalculateSpawnGroup(spawn_location_list[location_id]);

				LogWrite(SPAWN__TRACE, 0, "Spawn", "Exit %s", __FUNCTION__);
				return;
			}
		}

		LogWrite(SPAWN__TRACE, 0, "Spawn", "Exit %s", __FUNCTION__);

		ProcessSpawnLocation(spawn_location_list[location_id], respawn);
	}
	MSpawnLocationList.releasereadlock(__FUNCTION__, __LINE__);
}

Spawn* ZoneServer::ProcessSpawnLocation(SpawnLocation* spawnlocation, bool respawn)
{
	LogWrite(SPAWN__TRACE, 0, "Spawn", "Enter %s", __FUNCTION__);

	if(!spawnlocation)
		return 0;

	Spawn* spawn = 0;
	float rand_number = MakeRandomFloat(0, spawnlocation->total_percentage);

	for(int32 i=0;i<spawnlocation->entities.size();i++)
	{
		if(spawnlocation->entities[i]->spawn_percentage == 0)
			continue;

		if(spawnlocation->entities[i]->spawn_percentage >= rand_number){
			if(spawnlocation->entities[i]->spawn_type == SPAWN_ENTRY_TYPE_NPC)
				spawn = AddNPCSpawn(spawnlocation, spawnlocation->entities[i]);
			else if(spawnlocation->entities[i]->spawn_type == SPAWN_ENTRY_TYPE_GROUNDSPAWN)
				spawn = AddGroundSpawn(spawnlocation, spawnlocation->entities[i]);
			else if(spawnlocation->entities[i]->spawn_type == SPAWN_ENTRY_TYPE_OBJECT)
				spawn = AddObjectSpawn(spawnlocation, spawnlocation->entities[i]);
			else if(spawnlocation->entities[i]->spawn_type == SPAWN_ENTRY_TYPE_WIDGET)
				spawn = AddWidgetSpawn(spawnlocation, spawnlocation->entities[i]);
			else if(spawnlocation->entities[i]->spawn_type == SPAWN_ENTRY_TYPE_SIGN)
				spawn = AddSignSpawn(spawnlocation, spawnlocation->entities[i]);

			if (!spawn) 
			{
				LogWrite(ZONE__ERROR, 0, "Zone", "Error adding spawn to zone");
				safe_delete(spawn);
				continue;
			}

			const char* script = 0;

			for(int x=0;x<3;x++)
			{
				switch(x)
				{
					case 0:
						script = world.GetSpawnEntryScript(spawnlocation->entities[i]->spawn_entry_id);
						break;
					case 1:
						script = world.GetSpawnLocationScript(spawnlocation->entities[i]->spawn_location_id);
						break;
					case 2:
						script = world.GetSpawnScript(spawnlocation->entities[i]->spawn_id);
						break;
				}

				if(script && lua_interface && lua_interface->GetSpawnScript(script) != 0)
				{
					spawn->SetSpawnScript(string(script));
					break;
				}
			}

			if (spawn) 
			{
				if(respawn)
					CallSpawnScript(spawn, SPAWN_SCRIPT_RESPAWN);
				else	
					CallSpawnScript(spawn, SPAWN_SCRIPT_SPAWN);
			}
			break;
		}
		else
			rand_number -= spawnlocation->entities[i]->spawn_percentage;
	}

	LogWrite(SPAWN__TRACE, 0, "Spawn", "Exit %s", __FUNCTION__);

	return spawn;
}


Spawn* ZoneServer::ProcessInstanceSpawnLocation(SpawnLocation* spawnlocation, map<int32,int32>* instNPCs, map<int32,int32>* instGroundSpawns, map<int32,int32>* instObjSpawns, map<int32,int32>* instWidgetSpawns, map<int32,int32>* instSignSpawns, bool respawn)
{
	if(!spawnlocation)
		return 0;

	LogWrite(SPAWN__TRACE, 0, "Spawn", "Enter %s", __FUNCTION__);

	Spawn* spawn = 0;
	float rand_number = MakeRandomFloat(0, spawnlocation->total_percentage);

	for(int32 i=0;i<spawnlocation->entities.size();i++)
	{
		if(spawnlocation->entities[i]->spawn_percentage == 0)
			continue;

		int32 spawnTime = 0;

		if(spawnlocation->entities[i]->spawn_percentage >= rand_number)
		{
			if(spawnlocation->entities[i]->spawn_type == SPAWN_ENTRY_TYPE_NPC && 
				(spawnTime = database.CheckSpawnRemoveInfo(instNPCs,spawnlocation->entities[i]->spawn_location_id) > 0))
				spawn = AddNPCSpawn(spawnlocation, spawnlocation->entities[i]);
			else if(spawnlocation->entities[i]->spawn_type == SPAWN_ENTRY_TYPE_GROUNDSPAWN && 
				(spawnTime = database.CheckSpawnRemoveInfo(instGroundSpawns,spawnlocation->entities[i]->spawn_location_id) > 0))
				spawn = AddGroundSpawn(spawnlocation, spawnlocation->entities[i]);
			else if(spawnlocation->entities[i]->spawn_type == SPAWN_ENTRY_TYPE_OBJECT && 
				(spawnTime = database.CheckSpawnRemoveInfo(instObjSpawns,spawnlocation->entities[i]->spawn_location_id) > 0))
				spawn = AddObjectSpawn(spawnlocation, spawnlocation->entities[i]);
			else if(spawnlocation->entities[i]->spawn_type == SPAWN_ENTRY_TYPE_WIDGET && 
				(spawnTime = database.CheckSpawnRemoveInfo(instWidgetSpawns,spawnlocation->entities[i]->spawn_location_id) > 0))
				spawn = AddWidgetSpawn(spawnlocation, spawnlocation->entities[i]);
			else if(spawnlocation->entities[i]->spawn_type == SPAWN_ENTRY_TYPE_SIGN && 
				(spawnTime = database.CheckSpawnRemoveInfo(instSignSpawns,spawnlocation->entities[i]->spawn_location_id) > 0))
				spawn = AddSignSpawn(spawnlocation, spawnlocation->entities[i]);

			const char* script = 0;

			for(int x=0;x<3;x++)
			{
				switch(x)
				{
					case 0:
						script = world.GetSpawnEntryScript(spawnlocation->entities[i]->spawn_entry_id);
						break;
					case 1:
						script = world.GetSpawnLocationScript(spawnlocation->entities[i]->spawn_location_id);
						break;
					case 2:
						script = world.GetSpawnScript(spawnlocation->entities[i]->spawn_id);
						break;
				}

				if(spawn && script && lua_interface->GetSpawnScript(script) != 0)
				{
					spawn->SetSpawnScript(string(script));
					break;
				}
			}

			if(spawn)
			{
				if (respawn)
					CallSpawnScript(spawn, SPAWN_SCRIPT_RESPAWN);
				else
					CallSpawnScript(spawn, SPAWN_SCRIPT_SPAWN);

				if ( spawnTime > 1 )
				{
					spawn->SetRespawnTime(spawnTime);
				}
			}
			break;
		}
		else
			rand_number -= spawnlocation->entities[i]->spawn_percentage;
	}

	LogWrite(SPAWN__TRACE, 0, "Spawn", "Exit %s", __FUNCTION__);

	return spawn;
}

void ZoneServer::ProcessSpawnLocations()
{
	LogWrite(SPAWN__TRACE, 0, "Spawn", "Enter %s", __FUNCTION__);

	map<int32,int32>* instNPCs = NULL;
	map<int32,int32>* instGroundSpawns = NULL;
	map<int32,int32>* instObjSpawns = NULL;
	map<int32,int32>* instWidgetSpawns = NULL;
	map<int32,int32>* instSignSpawns = NULL;

	if ( this->IsInstanceZone() )
	{
		LogWrite(SPAWN__DEBUG, 0, "Spawn", "Processing Instance Removed Spawns...");
		instNPCs = database.GetInstanceRemovedSpawns(this->GetInstanceID() , SPAWN_ENTRY_TYPE_NPC );
		instGroundSpawns = database.GetInstanceRemovedSpawns(this->GetInstanceID() , SPAWN_ENTRY_TYPE_GROUNDSPAWN );
		instObjSpawns = database.GetInstanceRemovedSpawns(this->GetInstanceID() , SPAWN_ENTRY_TYPE_OBJECT );
		instWidgetSpawns = database.GetInstanceRemovedSpawns(this->GetInstanceID() , SPAWN_ENTRY_TYPE_WIDGET );
		instSignSpawns = database.GetInstanceRemovedSpawns(this->GetInstanceID() , SPAWN_ENTRY_TYPE_SIGN );
	}

	map<int32, bool> processed_spawn_locations;
	map<int32, SpawnLocation*>::iterator itr;
	MSpawnLocationList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_location_list.begin(); itr != spawn_location_list.end(); itr++) {
		LogWrite(SPAWN__TRACE, 0, "Spawn", "while spawn_location_list itr (#%u)", spawn_location_list.size());

		if(itr->second && processed_spawn_locations.count(itr->second->placement_id) > 0) //if we processed one spawn in a spawn group, we processed them all for that group
			continue;

		if(itr->second && spawn_location_groups.count(itr->second->placement_id) > 0)
		{
			int32 group_id = CalculateSpawnGroup(itr->second);

			if(group_id)
			{
				LogWrite(SPAWN__TRACE, 0, "Spawn", "is group_id");
				set<int32>* associated_groups = GetAssociatedGroups(group_id);

				if(associated_groups)
				{
					LogWrite(SPAWN__TRACE, 0, "Spawn", "is associated_groups");
					vector<int32>* associated_locations = GetAssociatedLocations(associated_groups);

					if(associated_locations)
					{
						LogWrite(SPAWN__TRACE, 0, "Spawn", "is associated_locations");
						for(int32 i=0;i<associated_locations->size();i++)
						{
							LogWrite(SPAWN__DEBUG, 5, "Spawn", "Loading processed_spawn_locations...");
							processed_spawn_locations[associated_locations->at(i)] = true;
						}

						safe_delete(associated_locations);
					}
				}
			}
		}
		else
		{
			if ( this->IsInstanceZone() )
			{
				LogWrite(SPAWN__DEBUG, 5, "Spawn", "ProcessInstanceSpawnLocation (%u)...", itr->second->placement_id);
				ProcessInstanceSpawnLocation(itr->second,instNPCs,instGroundSpawns,instObjSpawns,instWidgetSpawns,instSignSpawns);
			}
			else
			{
				LogWrite(SPAWN__DEBUG, 5, "Spawn", "ProcessSpawnLocation (%u)...", itr->second->placement_id);
				ProcessSpawnLocation(itr->second);
			}
		}
	}
	MSpawnLocationList.releasereadlock(__FUNCTION__, __LINE__);

	safe_delete(instNPCs);
	safe_delete(instGroundSpawns);
	safe_delete(instObjSpawns);
	safe_delete(instWidgetSpawns);
	safe_delete(instSignSpawns);

	LogWrite(SPAWN__TRACE, 0, "Spawn", "Exit %s", __FUNCTION__);
}

void ZoneServer::AddLoot(NPC* npc){
	vector<int32> loot_tables = GetSpawnLootList(npc->GetDatabaseID(), GetZoneID(), npc->GetLevel(), race_types_list.GetRaceType(npc->GetModelType()));
	if(loot_tables.size() > 0){
		vector<LootDrop*>* loot_drops = 0;
		vector<LootDrop*>::iterator loot_drop_itr;
		LootTable* table = 0;
		vector<int32>::iterator loot_list_itr;
		float chance = 0;
		for(loot_list_itr = loot_tables.begin(); loot_list_itr != loot_tables.end(); loot_list_itr++){
			table = GetLootTable(*loot_list_itr);
			if(table && table->maxcoin > 0){
				chance = rand()%100;
				if(table->coin_probability >= chance){
					if(table->maxcoin > table->mincoin)
						npc->AddLootCoins(table->mincoin + rand()%(table->maxcoin - table->mincoin));
				}
			}
			chance = rand()%100;
			if(table && (table->lootdrop_probability < 100 || table->lootdrop_probability >= chance)){
				loot_drops = GetLootDrops(*loot_list_itr);
				if(loot_drops){
					LootDrop* drop = 0;
					int16 count = 0;
					for(loot_drop_itr = loot_drops->begin(); loot_drop_itr != loot_drops->end(); loot_drop_itr++){
						drop = *loot_drop_itr;
						chance = rand()%100;
						if(drop->probability >= chance){
							count++;
							npc->AddLootItem(drop->item_id, drop->item_charges);

							LogWrite(MISC__TODO, 1, "TODO", "Auto-Equip new looted items\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
							//if(drop->equip_item) 

						}
						if(table->maxlootitems > 0 && count >= table->maxlootitems)
							break;
					}
				}
			}
		}
	}
}

void ZoneServer::DeterminePosition(SpawnLocation* spawnlocation, Spawn* spawn){
	if(!spawn || !spawnlocation)
		return;

	int offset = 0;
	if(spawnlocation->x_offset > 0){
		//since rand() deals with integers only, we are going to divide by 1000 later so that we can use fractions of integers
		offset = (int)((spawnlocation->x_offset*1000)+1); 
		spawn->SetX(spawnlocation->x + ((float)(rand()%offset - rand()%offset))/1000);
	}
	else
		spawn->SetX(spawnlocation->x);
	if(spawnlocation->y_offset > 0){
		//since rand() deals with integers only, we are going to divide by 1000 later so that we can use fractions of integers
		offset = (int)((spawnlocation->y_offset*1000)+1); 
		spawn->SetY(spawnlocation->y + ((float)(rand()%offset - rand()%offset))/1000);
	}
	else
		spawn->SetY(spawnlocation->y);
	if(spawnlocation->z_offset > 0){
		//since rand() deals with integers only, we are going to divide by 1000 later so that we can use fractions of integers
		offset = (int)((spawnlocation->z_offset*1000)+1); 
		spawn->SetZ(spawnlocation->z + ((float)(rand()%offset - rand()%offset))/1000);
	}
	else
		spawn->SetZ(spawnlocation->z);
	spawn->SetHeading(spawnlocation->heading);
	spawn->SetPitch(spawnlocation->pitch);
	spawn->SetRoll(spawnlocation->roll);
	spawn->SetSpawnOrigX(spawn->GetX());
	spawn->SetSpawnOrigY(spawn->GetY());
	spawn->SetSpawnOrigZ(spawn->GetZ());
	spawn->SetSpawnOrigHeading(spawn->GetHeading());
	spawn->SetSpawnOrigPitch(spawnlocation->pitch);
	spawn->SetSpawnOrigRoll(spawnlocation->roll);
	spawn->appearance.pos.grid_id = spawnlocation->grid_id;
	spawn->SetSpawnLocationPlacementID(spawnlocation->placement_id);
}

NPC* ZoneServer::AddNPCSpawn(SpawnLocation* spawnlocation, SpawnEntry* spawnentry){
	LogWrite(SPAWN__TRACE, 1, "Spawn", "Enter %s", __FUNCTION__);
	NPC* npc = GetNewNPC(spawnentry->spawn_id);
	if(npc){
		DeterminePosition(spawnlocation, npc);
		npc->SetSpawnLocationID(spawnentry->spawn_location_id);
		npc->SetSpawnEntryID(spawnentry->spawn_entry_id);
		npc->SetRespawnTime(spawnentry->respawn);
		npc->SetExpireTime(spawnentry->expire_time);
		if (spawnentry->expire_time > 0)
			AddSpawnExpireTimer(npc, spawnentry->expire_time, spawnentry->expire_offset);
		AddLoot(npc);
		AddSpawn(npc);
	}
	LogWrite(SPAWN__TRACE, 1, "Spawn", "Exit %s", __FUNCTION__);
	return npc;
}

vector<int32>* ZoneServer::GetAssociatedLocations(set<int32>* groups){
	vector<int32>* ret = 0;
	LogWrite(SPAWN__TRACE, 1, "Spawn", "Enter %s", __FUNCTION__);
	if(groups){
		int32 group_id = 0;		
		set<int32>::iterator group_itr;
		for (group_itr = groups->begin(); group_itr != groups->end(); group_itr++) {
			if(!ret)
				ret = new vector<int32>();
			group_id = *group_itr;

			MSpawnGroupLocations.readlock(__FUNCTION__, __LINE__);
			if(spawn_group_locations.count(group_id) > 0){
				map<int32, int32>::iterator itr;
				for (itr = spawn_group_locations[group_id]->begin(); itr != spawn_group_locations[group_id]->end(); itr++) {
					ret->push_back(itr->first);
				}
			}
			MSpawnGroupLocations.releasereadlock(__FUNCTION__, __LINE__);
		}
	}
	LogWrite(SPAWN__TRACE, 1, "Spawn", "Exit %s", __FUNCTION__);
	return ret;
}

set<int32>* ZoneServer::GetAssociatedGroups(int32 group_id) {
	set<int32>* ret = 0;
	MSpawnGroupAssociation.readlock(__FUNCTION__, __LINE__);
	LogWrite(SPAWN__TRACE, 1, "Spawn", "Enter %s", __FUNCTION__);
	if(spawn_group_associations.count(group_id) > 0)
		ret = spawn_group_associations[group_id];
	LogWrite(SPAWN__TRACE, 1, "Spawn", "Exit %s", __FUNCTION__);
	MSpawnGroupAssociation.releasereadlock(__FUNCTION__, __LINE__);
	return ret;
}

map<int32, int32>* ZoneServer::GetSpawnLocationsByGroup(int32 group_id) {
	map<int32, int32>* ret = 0;

	MSpawnGroupLocations.readlock(__FUNCTION__, __LINE__);
	if(spawn_group_locations.count(group_id) > 0)
		ret = spawn_group_locations[group_id];
	MSpawnGroupLocations.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}

list<int32>* ZoneServer::GetSpawnGroupsByLocation(int32 location_id){
	list<int32>* ret = 0;

	MSpawnLocationGroups.readlock(__FUNCTION__, __LINE__);
	if(spawn_location_groups.count(location_id) > 0)
		ret = spawn_location_groups[location_id];
	MSpawnLocationGroups.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}

float ZoneServer::GetSpawnGroupChance(int32 group_id){
	float ret = -1;

	MSpawnGroupChances.readlock(__FUNCTION__, __LINE__);
	if(spawn_group_chances.count(group_id) > 0)
		ret = spawn_group_chances[group_id];
	MSpawnGroupChances.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}

void ZoneServer::AddSpawnGroupChance(int32 group_id, float percent){
	MSpawnGroupChances.writelock(__FUNCTION__, __LINE__);
	spawn_group_chances[group_id] = percent;
	MSpawnGroupChances.releasewritelock(__FUNCTION__, __LINE__);
}

void ZoneServer::AddSpawnGroupAssociation(int32 group_id1, int32 group_id2) {
	MSpawnGroupAssociation.writelock(__FUNCTION__, __LINE__);
	//Check if we already have containers for these group ids, if not create them
	if (spawn_group_associations.count(group_id1) == 0)
		spawn_group_associations[group_id1] = new set<int32>;
	if (spawn_group_associations.count(group_id2) == 0)
		spawn_group_associations[group_id2] = new set<int32>;

	//Associate groups 1 and 2 now
	set<int32>* group_1 = spawn_group_associations.find(group_id1)->second;
	set<int32>* group_2 = spawn_group_associations.find(group_id2)->second;
	group_1->insert(group_id2);
	group_2->insert(group_id1);
	
	//Associate the remaining groups together
	set<int32>::iterator itr;
	for (itr = group_1->begin(); itr != group_1->end(); itr++){
		group_2->insert(*itr);
		map<int32, set<int32>*>::iterator assoc_itr = spawn_group_associations.find(*itr);
		if (assoc_itr != spawn_group_associations.end())
			assoc_itr->second->insert(group_id2);
		else {
			set<int32>* new_set = new set<int32>;
			spawn_group_associations[*itr] = new_set;
			new_set->insert(group_id2);
		}
	}
	for (itr = group_2->begin(); itr != group_2->end(); itr++){
		group_1->insert(*itr);
		map<int32, set<int32>*>::iterator assoc_itr = spawn_group_associations.find(*itr);
		if (assoc_itr != spawn_group_associations.end())
			assoc_itr->second->insert(group_id1);
		else {
			set<int32>* new_set = new set<int32>;
			spawn_group_associations[*itr] = new_set;
			new_set->insert(group_id1);
		}
	}
	MSpawnGroupAssociation.releasewritelock(__FUNCTION__, __LINE__);
}

void ZoneServer::AddSpawnGroupLocation(int32 group_id, int32 location_id, int32 spawn_location_id) {
	MSpawnGroupLocations.writelock(__FUNCTION__, __LINE__);
	if(spawn_group_locations.count(group_id) == 0)
		spawn_group_locations[group_id] =  new map<int32, int32>();
	(*spawn_group_locations[group_id])[location_id] = spawn_location_id;
	MSpawnGroupLocations.releasewritelock(__FUNCTION__, __LINE__);

	MSpawnLocationGroups.writelock(__FUNCTION__, __LINE__);
	if(spawn_location_groups.count(location_id) == 0)
		spawn_location_groups[location_id] = new list<int32>();
	spawn_location_groups[location_id]->push_back(group_id);
	MSpawnLocationGroups.releasewritelock(__FUNCTION__, __LINE__);

	MSpawnGroupAssociation.writelock(__FUNCTION__, __LINE__);
	if(spawn_group_associations.count(group_id) == 0)
		spawn_group_associations[group_id] = new set<int32>();
	spawn_group_associations[group_id]->insert(group_id);
	MSpawnGroupAssociation.releasewritelock(__FUNCTION__, __LINE__);
}

void ZoneServer::CallSpawnScript(Spawn* npc, int8 type, Spawn* spawn, const char* message){
	LogWrite(SPAWN__TRACE, 0, "Spawn", "Enter %s", __FUNCTION__);
	if(!npc)
		return;

	const char* script = npc->GetSpawnScript();
	if(lua_interface && script){
		switch(type){
			case SPAWN_SCRIPT_SPAWN:{
				lua_interface->RunSpawnScript(script, "spawn", npc);
				break;
									}
			case SPAWN_SCRIPT_RESPAWN:{
				lua_interface->RunSpawnScript(script, "respawn", npc);
				break;
									  }
			case SPAWN_SCRIPT_ATTACKED:{
				lua_interface->RunSpawnScript(script, "attacked", npc, spawn);
				break;
									   }
			case SPAWN_SCRIPT_TARGETED:{
				lua_interface->RunSpawnScript(script, "targeted", npc, spawn);
				break;
									   }
			case SPAWN_SCRIPT_HAILED:{
				lua_interface->RunSpawnScript(script, "hailed", npc, spawn);
				break;
									 }
			case SPAWN_SCRIPT_HAILED_BUSY:{
				lua_interface->RunSpawnScript(script, "hailed_busy", npc, spawn);
				break;
										  }
			case SPAWN_SCRIPT_DEATH:{
				lua_interface->RunSpawnScript(script, "death", npc, spawn);
				break;
									}
			case SPAWN_SCRIPT_KILLED:{
				lua_interface->RunSpawnScript(script, "killed", npc, spawn);
				break;
									 }
			case SPAWN_SCRIPT_AGGRO:{
				lua_interface->RunSpawnScript(script, "aggro", npc, spawn);
				break;
									}
			case SPAWN_SCRIPT_HEALTHCHANGED:{
				lua_interface->RunSpawnScript(script, "healthchanged", npc, spawn);
				break;
											}
			case SPAWN_SCRIPT_RANDOMCHAT:{
				lua_interface->RunSpawnScript(script, "randomchat", npc, 0, message);
				break;
										 }
			case SPAWN_SCRIPT_CUSTOM:
			case SPAWN_SCRIPT_TIMER:
			case SPAWN_SCRIPT_CONVERSATION:{
				lua_interface->RunSpawnScript(script, message, npc, spawn);
				break;
										   }
			case SPAWN_SCRIPT_CASTED_ON: {
				lua_interface->RunSpawnScript(script, "casted_on", npc, spawn, message);
				break;
										 }
			case SPAWN_SCRIPT_AUTO_ATTACK_TICK: {
				lua_interface->RunSpawnScript(script, "auto_attack_tick", npc, spawn);
				break;
												 }
			case SPAWN_SCRIPT_COMBAT_RESET: {
				lua_interface->RunSpawnScript(script, "CombatReset", npc);
				break;
											}
			case SPAWN_SCRIPT_GROUP_DEAD: {
				lua_interface->RunSpawnScript(script, "group_dead", npc, spawn);
				break;
										  }
			case SPAWN_SCRIPT_HEAR_SAY: {
				lua_interface->RunSpawnScript(script, "hear_say", npc, spawn, message);
				break;
			}
		}
	}
	LogWrite(SPAWN__TRACE, 0, "Spawn", "Exit %s", __FUNCTION__);
}

void ZoneServer::DeleteTransporters() {
	MTransportLocations.writelock(__FUNCTION__, __LINE__);
	transporter_locations.clear(); //world takes care of actually deleting the data
	MTransportLocations.releasewritelock(__FUNCTION__, __LINE__);
}

void ZoneServer::ReloadTransporters(){
	MutexList<LocationTransportDestination*>* locations = GetLocationTransporters(GetZoneID());
	if(locations){
		MutexList<LocationTransportDestination*>::iterator itr = locations->begin();
		while(itr.Next())
			AddTransporter(itr->value);
	}
}

void ZoneServer::CheckTransporters(Client* client) {
	MTransportLocations.readlock(__FUNCTION__, __LINE__);
	if(transporter_locations.size() > 0){
		LocationTransportDestination* loc = 0;
		list<LocationTransportDestination*>::iterator itr;
		for (itr = transporter_locations.begin(); itr != transporter_locations.end(); itr++) {
			loc = *itr;
			if(client->GetPlayer()->GetDistance(loc->trigger_x, loc->trigger_y, loc->trigger_z) <= loc->trigger_radius){
				if(loc->destination_zone_id == 0 || loc->destination_zone_id == GetZoneID()){
					EQ2Packet* packet = client->GetPlayer()->Move(loc->destination_x, loc->destination_y, loc->destination_z, client->GetVersion());
					if(packet)
						client->QueuePacket(packet);
				}
				else{
					ZoneServer* new_zone = zone_list.Get(loc->destination_zone_id);
					if(new_zone){
						client->GetPlayer()->SetX(loc->destination_x);
						client->GetPlayer()->SetY(loc->destination_y);
						client->GetPlayer()->SetZ(loc->destination_z);
						client->GetPlayer()->SetHeading(loc->destination_heading);
						client->Zone(new_zone, false);
					}
				}
				break;
			}
		}
	}
	MTransportLocations.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::AddTransporter(LocationTransportDestination* loc) {
	MTransportLocations.writelock(__FUNCTION__, __LINE__);
	transporter_locations.push_back(loc);
	MTransportLocations.releasewritelock(__FUNCTION__, __LINE__);
}

Sign* ZoneServer::AddSignSpawn(SpawnLocation* spawnlocation, SpawnEntry* spawnentry){
	LogWrite(SPAWN__TRACE, 0, "Spawn", "Enter %s", __FUNCTION__);
	Sign* sign = GetNewSign(spawnentry->spawn_id);
	if(sign){
		DeterminePosition(spawnlocation, sign);
		sign->SetSpawnLocationID(spawnentry->spawn_location_id);
		sign->SetSpawnEntryID(spawnentry->spawn_entry_id);
		sign->SetRespawnTime(spawnentry->respawn);
		sign->SetExpireTime(spawnentry->expire_time);
		if (spawnentry->expire_time > 0)
			AddSpawnExpireTimer(sign, spawnentry->expire_time, spawnentry->expire_offset);
		AddSpawn(sign);
	}
	LogWrite(SPAWN__TRACE, 0, "Spawn", "Exit %s", __FUNCTION__);
	return sign;
}

Widget* ZoneServer::AddWidgetSpawn(SpawnLocation* spawnlocation, SpawnEntry* spawnentry){
	LogWrite(SPAWN__TRACE, 0, "Spawn", "Enter %s", __FUNCTION__);
	Widget* widget = GetNewWidget(spawnentry->spawn_id);
	if(widget){
		DeterminePosition(spawnlocation, widget);
		widget->SetSpawnLocationID(spawnentry->spawn_location_id);
		widget->SetSpawnEntryID(spawnentry->spawn_entry_id);
		if(!widget->GetIncludeLocation()){
			widget->SetX(widget->GetWidgetX());
			if(widget->GetCloseY() != 0)
				widget->SetY(widget->GetCloseY());
			widget->SetZ(widget->GetWidgetZ());
		}
		widget->SetRespawnTime(spawnentry->respawn);
		widget->SetExpireTime(spawnentry->expire_time);
		widget->SetSpawnOrigHeading(widget->GetHeading());
		if (spawnentry->expire_time > 0)
			AddSpawnExpireTimer(widget, spawnentry->expire_time, spawnentry->expire_offset);
		AddSpawn(widget);
	}
	LogWrite(SPAWN__TRACE, 0, "Spawn", "Exit %s", __FUNCTION__);
	return widget;
}

Object* ZoneServer::AddObjectSpawn(SpawnLocation* spawnlocation, SpawnEntry* spawnentry){
	LogWrite(SPAWN__TRACE, 0, "Spawn", "Enter %s", __FUNCTION__);
	Object* object = GetNewObject(spawnentry->spawn_id);
	if(object){
		DeterminePosition(spawnlocation, object);
		object->SetSpawnLocationID(spawnentry->spawn_location_id);
		object->SetSpawnEntryID(spawnentry->spawn_entry_id);
		object->SetRespawnTime(spawnentry->respawn);
		object->SetExpireTime(spawnentry->expire_time);
		if (spawnentry->expire_time > 0)
			AddSpawnExpireTimer(object, spawnentry->expire_time, spawnentry->expire_offset);
		AddSpawn(object);
	}
	LogWrite(SPAWN__TRACE, 0, "Spawn", "Exit %s", __FUNCTION__);
	return object;
}

GroundSpawn* ZoneServer::AddGroundSpawn(SpawnLocation* spawnlocation, SpawnEntry* spawnentry){
	LogWrite(SPAWN__TRACE, 0, "Spawn", "Enter %s", __FUNCTION__);
	GroundSpawn* spawn = GetNewGroundSpawn(spawnentry->spawn_id);
	if(spawn){
		DeterminePosition(spawnlocation, spawn);
		spawn->SetSpawnLocationID(spawnentry->spawn_location_id);
		spawn->SetSpawnEntryID(spawnentry->spawn_entry_id);
		spawn->SetRespawnTime(spawnentry->respawn);
		spawn->SetExpireTime(spawnentry->expire_time);
		if (spawnentry->expire_time > 0)
			AddSpawnExpireTimer(spawn, spawnentry->expire_time, spawnentry->expire_offset);
		AddSpawn(spawn);
	}
	LogWrite(SPAWN__TRACE, 0, "Spawn", "Exit %s", __FUNCTION__);
	return spawn;
}

void ZoneServer::AddSpawn(Spawn* spawn) {
	spawn->SetZone(this);
	spawn->position_changed = false;
	spawn->info_changed = false;
	spawn->vis_changed = false;
	spawn->changed = false;
	if(!spawn->IsPlayer() || (spawn->IsPlayer() && !((Player*)spawn)->IsReturningFromLD())) {
		// Write locking the spawn list here will cause deadlocks, so instead add it to a temp list that the
		// main spawn thread will put into the spawn_list when ever it has a chance.
		MPendingSpawnListAdd.writelock(__FUNCTION__, __LINE__);
		pending_spawn_list_add.push_back(spawn);
		MPendingSpawnListAdd.releasewritelock(__FUNCTION__, __LINE__);
	}
	else
		((Player*)spawn)->SetReturningFromLD(false);
	spawn_range.Trigger();
	spawn_check_add.Trigger();
	if(spawn->IsNPC())
		AddEnemyList((NPC*)spawn);
	if(spawn->IsPlayer() && ((Player*)spawn)->GetGroupMemberInfo())
		world.GetGroupManager()->SendGroupUpdate(((Player*)spawn)->GetGroupMemberInfo()->group_id, GetClientBySpawn(spawn));
	if (spawn->IsPlayer()) {
		((Player*)spawn)->GetInfoStruct()->rain = rain;
		((Player*)spawn)->SetCharSheetChanged(true);
	}

}

void ZoneServer::AddClient(Client* client){
	MClientList.writelock(__FUNCTION__, __LINE__);
	clients.push_back(client);
	MClientList.releasewritelock(__FUNCTION__, __LINE__);

	connected_clients.Add(client);
}

void ZoneServer::RemoveClient(Client* client)
{
	Guild *guild;

	if(client)
	{
		client->Disconnect();
		
		LogWrite(ZONE__DEBUG, 0, "Zone", "Sending login equipment appearance updates...");
		loginserver.SendImmediateEquipmentUpdatesForChar(client->GetPlayer()->GetCharacterID());

		if (!client->IsZoning()) 
		{
			if ((guild = client->GetPlayer()->GetGuild()) != NULL)
				guild->GuildMemberLogoff(client->GetPlayer());

			chat.LeaveAllChannels(client);
		}
		
		int32 LD_Timer = rule_manager.GetGlobalRule(R_World, LinkDeadTimer)->GetInt32();
		int32 DisconnectClientTimer = rule_manager.GetGlobalRule(R_World, RemoveDisconnectedClientsTimer)->GetInt32();

		if(!zoneShuttingDown && !client->IsZoning())
		{
			GroupMemberInfo* gmi = client->GetPlayer()->GetGroupMemberInfo();
			if (gmi) {
				int32 size = world.GetGroupManager()->GetGroupSize(gmi->group_id);
				if (size > 1) {
					bool send_left_message = size > 2;
					world.GetGroupManager()->RemoveGroupMember(gmi->group_id, client->GetPlayer());
					if (send_left_message)
						world.GetGroupManager()->GroupMessage(gmi->group_id, "%s has left the group.", client->GetPlayer()->GetName());
				}
			}
			if( (client->GetPlayer()->GetActivityStatus() & ACTIVITY_STATUS_LINKDEAD) > 0)
			{
				LogWrite(ZONE__DEBUG, 0, "Zone", "Removing client '%s' (%u) due to LD/Exit...", client->GetPlayer()->GetName(), client->GetPlayer()->GetCharacterID());
				//AddDelayedSpawnRemove(client->GetPlayer(), LD_Timer);
				//connected_clients.Remove(client, true, LD_Timer + DisconnectClientTimer);
			}
			else
			{
				LogWrite(ZONE__DEBUG, 0, "Zone", "Removing client '%s' (%u) due to Camp/Quit...", client->GetPlayer()->GetName(), client->GetPlayer()->GetCharacterID());
			}

			GetSpellProcess()->RemoveSpellTimersFromSpawn(client->GetPlayer(), true);
				
			client->GetPlayer()->DismissPet((NPC*)client->GetPlayer()->GetPet());
			client->GetPlayer()->DismissPet((NPC*)client->GetPlayer()->GetCharmedPet());
			client->GetPlayer()->DismissPet((NPC*)client->GetPlayer()->GetDeityPet());
			client->GetPlayer()->DismissPet((NPC*)client->GetPlayer()->GetCosmeticPet());

			RemoveSpawn(client->GetPlayer(), false);
		}

		connected_clients.Remove(client, true, DisconnectClientTimer);

		client_spawn_map.Put(client->GetPlayer(), 0);

		LogWrite(ZONE__DEBUG, 0, "Zone", "Calling clients.Remove(client)...");
		MClientList.writelock(__FUNCTION__, __LINE__);
		clients.erase(find(clients.begin(), clients.end(), client));
		MClientList.releasewritelock(__FUNCTION__, __LINE__);

		LogWrite(ZONE__INFO, 0, "Zone", "Scheduling client '%s' for removal.", client->GetPlayer()->GetName());
		LogWrite(MISC__TODO, 1, "TODO", "Put Player Online Status updates in a timer eventually\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
		database.ToggleCharacterOnline(client, 0);
	}
}

void ZoneServer::RemoveClientImmediately(Client* client) {
	Guild *guild;

	if(client) 
	{
		loginserver.SendImmediateEquipmentUpdatesForChar(client->GetPlayer()->GetCharacterID());
		if(connected_clients.count(client) > 0)
		{
			if (!client->IsZoning() && (guild = client->GetPlayer()->GetGuild()))
				guild->GuildMemberLogoff(client->GetPlayer());

			MClientList.writelock(__FUNCTION__, __LINE__);
			clients.erase(find(clients.begin(), clients.end(), client));
			MClientList.releasewritelock(__FUNCTION__, __LINE__);
			//clients.Remove(client);
			LogWrite(ZONE__INFO, 0, "Zone", "Removing connection for client '%s'.", client->GetPlayer()->GetName());
			connected_clients.Remove(client, true);
		}
		else
		{
			MClientList.writelock(__FUNCTION__, __LINE__);
			clients.erase(find(clients.begin(), clients.end(), client));
			MClientList.releasewritelock(__FUNCTION__, __LINE__);
			//clients.Remove(client, true);
		}

		LogWrite(MISC__TODO, 1, "TODO", "Put Player Online Status updates in a timer eventually\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
		database.ToggleCharacterOnline(client, 0);
	}
}

void ZoneServer::ClientProcess()
{
	if(connected_clients.size(true) == 0)
	{
		if(!IsCityZone() && !AlwaysLoaded() && !shutdownTimer.Enabled())
		{
			LogWrite(ZONE__INFO, 0, "Zone", "Starting zone shutdown timers...");
			shutdownTimer.Start();
		}
		return;
	}

	shutdownTimer.Disable();	
	Client* client = 0;		
	MutexList<Client*>::iterator iterator = connected_clients.begin();

	while(iterator.Next())
	{
		client = iterator->value;
#ifndef NO_CATCH
		try
		{
#endif
			if(zoneShuttingDown || !client->Process(true))
			{
				if(!zoneShuttingDown && !client->IsZoning())
				{
					LogWrite(ZONE__DEBUG, 0, "Zone", "Client is disconnecting in %s (camping = %s)", __FUNCTION__, (client->GetPlayer()->GetActivityStatus() & ACTIVITY_STATUS_CAMPING) == 0 ? "false" : "true");

					if( (client->GetPlayer()->GetActivityStatus() & ACTIVITY_STATUS_CAMPING) == 0 )
					{
						 //only set LD flag if we're disconnecting but not camping/quitting
						client->GetPlayer()->SetActivityStatus(client->GetPlayer()->GetActivityStatus() + ACTIVITY_STATUS_LINKDEAD);
						if(client->GetPlayer()->GetGroupMemberInfo())
						{
							LogWrite(ZONE__DEBUG, 0, "Zone", "Telling client's group they are disconnecting");
							world.GetGroupManager()->GroupMessage(client->GetPlayer()->GetGroupMemberInfo()->group_id, "%s has gone Linkdead.", client->GetPlayer()->GetName());
						}
					}

				}

				RemoveClient(client);
			}
#ifndef NO_CATCH
		}
		catch(...)
		{
			LogWrite(ZONE__ERROR, 0, "Zone", "Exception caught when in ZoneServer::ClientProcess() for zone '%s'!\n%s, %i", GetZoneName(), __FUNCTION__, __LINE__);
			try{
				if(!client->IsZoning())
				{
					if( (client->GetPlayer()->GetActivityStatus() & ACTIVITY_STATUS_CAMPING) == 0 )
					{
						client->GetPlayer()->SetActivityStatus(client->GetPlayer()->GetActivityStatus() + ACTIVITY_STATUS_LINKDEAD);
						if(client->GetPlayer()->GetGroupMemberInfo())
							world.GetGroupManager()->GroupMessage(client->GetPlayer()->GetGroupMemberInfo()->group_id, "%s has gone Linkdead.", client->GetPlayer()->GetName());
					}
				}

				RemoveClient(client);
			}
			catch(...){
				LogWrite(ZONE__ERROR, 0, "Zone", "Exception caught when in ZoneServer::ClientProcess(), second try\n%s, %i", __FUNCTION__, __LINE__);
			}
		}
#endif
	}
}

void ZoneServer::SimpleMessage(int8 type, const char* message, Spawn* from, float distance){
	Client* client = 0;
	vector<Client*>::iterator client_itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		if(from && client && client->IsConnected() && from->GetDistance(client->GetPlayer()) <= distance){
			client->SimpleMessage(type, message);
		}
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::HandleChatMessage(Client* client, Spawn* from, const char* to, int16 channel, const char* message, float distance, const char* channel_name, bool show_bubble, int32 language){
	if((!distance || from->GetDistance(client->GetPlayer()) <= distance) && (!from || !client->GetPlayer()->IsIgnored(from->GetName()))){
		PacketStruct* packet = configReader.getStruct("WS_HearChat", client->GetVersion());
		if(packet){
			if(from)
				packet->setMediumStringByName("from", from->GetName());
			if(client->GetPlayer() != from)
				packet->setMediumStringByName("to", client->GetPlayer()->GetName());
			packet->setDataByName("channel", channel);
			if(from && ((from == client->GetPlayer()) || (client->GetPlayer()->WasSentSpawn(from->GetID()) && !client->GetPlayer()->WasSpawnRemoved(from))))
				packet->setDataByName("from_spawn_id", client->GetPlayer()->GetIDWithPlayerSpawn(from));
			else
				packet->setDataByName("from_spawn_id", 0xFFFFFFFF);
			packet->setDataByName("to_spawn_id", 0xFFFFFFFF);
			packet->setMediumStringByName("message", message);
			packet->setDataByName("language", language);

			if (language > 0 && !client->GetPlayer()->HasLanguage(language))
				packet->setDataByName("understood", 0);
			else
				packet->setDataByName("understood", 1);

			show_bubble == true ? packet->setDataByName("show_bubble", 1) : packet->setDataByName("show_bubble", 0);
			if(channel_name)
				packet->setMediumStringByName("channel_name", channel_name);
			client->QueuePacket(packet->serialize());
			safe_delete(packet);
		}
	}
}

void ZoneServer::HandleChatMessage(Spawn* from, const char* to, int16 channel, const char* message, float distance, const char* channel_name, bool show_bubble, int32 language){
	vector<Client*>::iterator client_itr;
	Client* client = 0;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		if(client && client->IsConnected())
			HandleChatMessage(client, from, to, channel, message, distance, channel_name, show_bubble, language);
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::HandleBroadcast(const char* message) {
	vector<Client*>::iterator client_itr;
	Client* client = 0;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		if(client && client->IsConnected())
			client->SimpleMessage(CHANNEL_BROADCAST, message);
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::HandleAnnouncement(const char* message) {
	vector<Client*>::iterator client_itr;
	Client* client = 0;
	int32 words = ::CountWordsInString(message);
	if (words < 5)
		words = 5;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.begin(); client_itr++) {
		client = *client_itr;
		if(client && client->IsConnected()) {
			client->SimpleMessage(CHANNEL_BROADCAST, message);
			client->SendPopupMessage(10, message, "ui_harvest_normal", words, 0xFF, 0xFF, 0x00);
		}
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::SendTimeUpdate(Client* client){
	if(client){
		PacketStruct* packet = world.GetWorldTime(client->GetVersion());
		if(packet){
			client->QueuePacket(packet->serialize());
			safe_delete(packet);
		} 
	}
}

void ZoneServer::SendTimeUpdateToAllClients(){
	Client* client = 0;
	vector<Client*>::iterator client_itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		if(client && client->IsConnected())
			SendTimeUpdate(client);
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::UpdateVitality(float amount){
	Client* client = 0;
	vector<Client*>::iterator client_itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		if(client && client->GetPlayer()->GetInfoStruct()->xp_vitality < 100){
			if((client->GetPlayer()->GetInfoStruct()->xp_vitality + amount) > 100)
				client->GetPlayer()->GetInfoStruct()->xp_vitality = 100;
			else
				client->GetPlayer()->GetInfoStruct()->xp_vitality += amount;
			client->GetPlayer()->SetCharSheetChanged(true);
		}
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::SendSpawn(Spawn* spawn, Client* client){
	EQ2Packet* outapp = spawn->serialize(client->GetPlayer(), client->GetVersion());

	if(outapp)
		client->QueuePacket(outapp);
	/*
	vis flags:
	2 = show icon
	4 = targetable
	16 = show name
	32 = show level/border
	activity_status:
	4 - linkdead
	8 - camping
	16 - LFG
	32 - LFW
	2048 - mentoring
	4096 - displays shield
	8192 - immunity gained
	16384 - immunity remaining
	attackable_status
	1 - no_hp_bar
	4 - not attackable
	npc_con
	-4 = scowls
	-3 = threatening
	-2 = dubiously
	-1 = apprehensively
	0 = indifferent
	1 = amiably
	2 = kindly
	3 = warmly
	4 = ally
	quest_flag
	1 = new quest
	2 = update and new quest
	3 = update
	*/
}

Client*	ZoneServer::GetClientBySpawn(Spawn* spawn){
	return client_spawn_map.Get(spawn);
}

Client*	ZoneServer::GetClientByName(char* name) {
	Client* ret = 0;
	vector<Client*>::iterator itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (itr = clients.begin(); itr != clients.end(); itr++) {
		if ((*itr)->GetPlayer()) {
			if (strncmp((*itr)->GetPlayer()->GetName(), name, strlen(name)) == 0) {
				ret = *itr;
				break;
			}
		}
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
	return ret;
}

void ZoneServer::AddMovementNPC(Spawn* spawn){
	if (spawn)
		movement_spawns.Put(spawn->GetID(), 1);
}

void ZoneServer::RemoveMovementNPC(Spawn* spawn){
	if (spawn)
		remove_movement_spawns.Add(spawn->GetID());
}

void ZoneServer::ProcessMovement(){	
	Spawn* spawn = 0;
	MutexMap<int32, int32>::iterator itr = movement_spawns.begin();
	while(itr.Next()){
		spawn = GetSpawnByID(itr->first);
		if(spawn) {
			if(spawn->IsNPC())
				spawn->ProcessMovement();
		}
		else
			movement_spawns.erase(itr->first);
	}
	MutexList<int32>::iterator remove_itr = remove_movement_spawns.begin();
	while(remove_itr.Next()){
		movement_spawns.erase(remove_itr->value);
	}
	remove_movement_spawns.clear();
}

void ZoneServer::PlayFlavor(Client* client, Spawn* spawn, const char* mp3, const char* text, const char* emote, int32 key1, int32 key2, int8 language){
	if(!client || !spawn)
		return;

	PacketStruct* packet = configReader.getStruct("WS_PlayFlavor", client->GetVersion());
	if(packet){
		packet->setDataByName("spawn_id", client->GetPlayer()->GetIDWithPlayerSpawn(spawn));
		packet->setDataByName("unknown1", 0xFFFFFFFF);
		packet->setDataByName("unknown5", 1, 1);
		packet->setDataByName("unknown5", 1, 6);
		if(mp3){
			packet->setMediumStringByName("mp3", mp3);
			packet->setDataByName("key", key1);
			packet->setDataByName("key", key2, 1);
		}
		packet->setMediumStringByName("name", spawn->GetName());
		if(text)
			packet->setMediumStringByName("text", text);
		if(emote)
			packet->setMediumStringByName("emote", emote);
		if (language != 0)
			packet->setDataByName("language", language);

		//We should probably add Common = language id 0 or 0xFF so admins can customize more..
		if (language == 0 || client->GetPlayer()->HasLanguage(language))
			packet->setDataByName("understood", 1);

		EQ2Packet* app = packet->serialize();
		//DumpPacket(app);
		client->QueuePacket(app);
		safe_delete(packet);
	}
}

void ZoneServer::PlayVoice(Client* client, Spawn* spawn, const char* mp3, int32 key1, int32 key2){
	if(!client || !spawn)
		return;

	PacketStruct* packet = configReader.getStruct("WS_PlayVoice", client->GetVersion());
	if(packet){
		packet->setDataByName("spawn_id", client->GetPlayer()->GetIDWithPlayerSpawn(spawn));
		packet->setMediumStringByName("mp3", mp3);
		packet->setDataByName("key", key1);
		packet->setDataByName("key", key2, 1);
		client->QueuePacket(packet->serialize());
		safe_delete(packet);
	}
}

void ZoneServer::PlayFlavor(Spawn* spawn, const char* mp3, const char* text, const char* emote, int32 key1, int32 key2, int8 language){
	if(!spawn)
		return;

	Client* client = 0;
	vector<Client*>::iterator client_itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		if(!client || !client->IsConnected() || !client->GetPlayer()->WasSentSpawn(spawn->GetID()) || client->GetPlayer()->WasSpawnRemoved(spawn) || client->GetPlayer()->GetDistance(spawn) > 30)
			continue;
		PlayFlavor(client, spawn, mp3, text, emote, key1, key2, language);
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::PlayVoice(Spawn* spawn, const char* mp3, int32 key1, int32 key2){
	if(!spawn || !mp3)
		return;

	Client* client = 0;
	vector<Client*>::iterator client_itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		if(!client || !client->IsConnected() || !client->GetPlayer()->WasSentSpawn(spawn->GetID()) || client->GetPlayer()->WasSpawnRemoved(spawn))
			continue;
		PlayVoice(client, spawn, mp3, key1, key2);
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::PlaySoundFile(Client* client, const char* name, float origin_x, float origin_y, float origin_z){
	if(!name)
		return;

	PacketStruct* packet = 0;
	if(client){
		packet = configReader.getStruct("WS_Play3DSound", client->GetVersion());
		if(packet){
			packet->setMediumStringByName("name", name);
			packet->setDataByName("x", origin_x);
			packet->setDataByName("y", origin_y);
			packet->setDataByName("z", origin_z);
			packet->setDataByName("unknown1", 1);
			packet->setDataByName("unknown2", 2.5);
			packet->setDataByName("unknown3", 15);
			client->QueuePacket(packet->serialize());
			safe_delete(packet);
		}
	}
	else{
		EQ2Packet* outapp = 0;
		int16 packet_version = 0;
		vector<Client*>::iterator client_itr;

		MClientList.readlock(__FUNCTION__, __LINE__);
		for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
			client = *client_itr;
			if(client && (!packet || packet_version != client->GetVersion())){
				safe_delete(packet);
				safe_delete(outapp);
				packet_version = client->GetVersion();
				packet = configReader.getStruct("WS_Play3DSound", packet_version);
				if(packet){
					packet->setMediumStringByName("name", name);
					packet->setDataByName("x", origin_x);
					packet->setDataByName("y", origin_y);
					packet->setDataByName("z", origin_z);
					packet->setDataByName("unknown1", 1);
					packet->setDataByName("unknown2", 2.5);
					packet->setDataByName("unknown3", 15);
					outapp = packet->serialize();
				}
			}
			if(outapp && client && client->IsConnected())
				client->QueuePacket(outapp->Copy());
		}
		MClientList.releasereadlock(__FUNCTION__, __LINE__);
		safe_delete(packet);
		safe_delete(outapp);
	}
}

void ZoneServer::AddHeadingTimer(Spawn* spawn){
	heading_timers.Put(spawn, Timer::GetCurrentTime2() + 30000);
}

void ZoneServer::RemoveHeadingTimer(Spawn* spawn){
	heading_timers.erase(spawn);
}

void ZoneServer::CheckHeadingTimers(){
	if(heading_timers.size() > 0){
		MutexMap<Spawn*, int32>::iterator itr = heading_timers.begin();
		Spawn* spawn = 0;
		int32 current_time = Timer::GetCurrentTime2();
		while(itr.Next()){
			if(current_time >= itr->second){
				spawn = itr->first;
				spawn->SetHeading(spawn->GetSpawnOrigHeading());
				spawn->SetTempActionState(-1);
				heading_timers.erase(itr->first);
			}
		}
	}
}

bool ZoneServer::HasWidgetTimer(Spawn* widget){	
	bool ret = false;
	if (widget) {
		int32 id = widget->GetID();
		MutexMap<int32, int32>::iterator itr = widget_timers.begin();
		while(itr.Next()){
			if(itr->first == id){
				ret = true;
				break;
			}
		}
	}
	return ret;
}

void ZoneServer::CheckWidgetTimers(){
	vector<int32> remove_list;
	MutexMap<int32, int32>::iterator itr = widget_timers.begin();
	while(itr.Next()){
		if(Timer::GetCurrentTime2() >= itr->second){
			Spawn* widget = GetSpawnByID(itr->first);
			if (widget && widget->IsWidget())
				((Widget*)widget)->HandleTimerUpdate();

			remove_list.push_back(itr->first);
		}
	}
	for(int32 i=0;i<remove_list.size(); i++)
		widget_timers.erase(remove_list[i]);
}

void ZoneServer::AddWidgetTimer(Spawn* widget, float time){
	if(widget && widget->IsWidget())
		widget_timers.Put(widget->GetID(), (int32)(time*1000) + Timer::GetCurrentTime2());
}

Spawn*	ZoneServer::GetSpawnGroup(int32 id){
	Spawn* ret = 0;
	Spawn* spawn = 0;
	map<int32, Spawn*>::iterator itr;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
		spawn = itr->second;
		if(spawn){
			if(spawn->GetSpawnGroupID() == id){
				ret = spawn;
				break;
			}
		}
	}
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
	return ret;
}

Spawn* ZoneServer::GetSpawnByLocationID(int32 location_id) {
	Spawn* ret = 0;
	Spawn* current_spawn = 0;
	map<int32, Spawn*>::iterator itr;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
		current_spawn = itr->second;
		if (current_spawn && current_spawn->GetSpawnLocationID() == location_id) {
			ret = current_spawn;
			break;
		}
	}
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
	return ret;
}

Spawn*	ZoneServer::GetSpawnByDatabaseID(int32 id){
	Spawn* ret = 0;
	if(quick_database_id_lookup.count(id) > 0)
		ret = GetSpawnByID(quick_database_id_lookup.Get(id));
	else{
		Spawn* spawn = 0;
		map<int32, Spawn*>::iterator itr;
		MSpawnList.readlock(__FUNCTION__, __LINE__);
		for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++){
			spawn = itr->second;
			if(spawn){
				if(spawn->GetDatabaseID() == id){
					quick_database_id_lookup.Put(id, spawn->GetID());
					ret = spawn;
					break;
				}
			}
		}
		MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
	}
	return ret;
}

Spawn* ZoneServer::GetSpawnByID(int32 id) {
	Spawn* ret = 0;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	if (spawn_list.count(id) > 0)
		ret = spawn_list[id];
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
	return ret;
}

bool ZoneServer::SendRemoveSpawn(Client* client, Spawn* spawn, PacketStruct* packet, bool delete_spawn)
{
	if(!client || !spawn)
		return false;

	int16 index = client->GetPlayer()->GetIndexForSpawn(spawn);
	int32 cur_id = client->GetPlayer()->GetIDWithPlayerSpawn(spawn);
	if(packet && index > 0 && client->GetPlayer()->WasSpawnRemoved(spawn) == false)
	{
		LogWrite(ZONE__DEBUG, 7, "Zone", "Processing SendRemoveSpawn for spawn index %u...", index);
		packet->setDataByName("spawn_index", index);
		client->GetPlayer()->GetPlayerSpawnMap()->erase(index);
		client->GetPlayer()->GetPlayerSpawnIndexMap()->erase(spawn);
		client->GetPlayer()->player_spawn_id_map.erase(cur_id);
		client->GetPlayer()->player_spawn_reverse_id_map.erase(spawn);
		client->GetPlayer()->RemoveSpawn(spawn); // sets it as removed

		if(delete_spawn)
			packet->setDataByName("delete", 1);	
		
		client->QueuePacket(packet->serialize());
		return true;
	}
	return false;
}

void ZoneServer::SetSpawnCommand(Spawn* spawn, int8 type, char* value, Client* client){
	//commands
	LogWrite(MISC__TODO, 1, "TODO", "%s does nothing!\n%s, %i", __FUNCTION__, __FILE__, __LINE__);
}

void ZoneServer::SetSpawnCommand(int32 spawn_id, int8 type, char* value, Client* client){
	LogWrite(MISC__TODO, 1, "TODO", "%s does nothing!\n%s, %i", __FUNCTION__, __FILE__, __LINE__);
}

void ZoneServer::ApplySetSpawnCommand(Client* client, Spawn* target, int8 type, char* value){
	// This will apply the /spawn set command to all the spawns in the zone with the same DB ID, we do not want to set
	// location values (x, y, z, heading, grid) for all spawns in the zone with the same DB ID, only the targeted spawn
	if(type == SPAWN_SET_VALUE_SPAWNENTRY_SCRIPT || type == SPAWN_SET_VALUE_SPAWNLOCATION_SCRIPT || (type >= SPAWN_SET_VALUE_X && type <= SPAWN_SET_VALUE_LOCATION) ||
		type == SPAWN_SET_VALUE_PITCH || type == SPAWN_SET_VALUE_ROLL)
		return;

	Spawn* tmp = 0;
	if(target->IsNPC())
		tmp = GetNPC(target->GetDatabaseID());
	else if(target->IsObject())
		tmp = GetObject(target->GetDatabaseID());
	else if(target->IsGroundSpawn())
		tmp = GetGroundSpawn(target->GetDatabaseID());
	else if(target->IsSign())
		tmp = GetSign(target->GetDatabaseID());
	else if(target->IsWidget())
		tmp = GetWidget(target->GetDatabaseID());
	if(tmp && type == SPAWN_SET_VALUE_SPAWN_SCRIPT)
		tmp->SetSpawnScript(value);
	else if(tmp)
		commands.SetSpawnCommand(client, tmp, type, value); // set the master spawn
	Spawn* spawn = 0;

	// this check needs to be here otherwise every spawn with 0 will be set
	if ( target->GetDatabaseID ( ) > 0 )
	{
		map<int32, Spawn*>::iterator itr;
		MSpawnList.readlock(__FUNCTION__, __LINE__);
		for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
			spawn = itr->second;
			if(spawn && spawn->GetDatabaseID() == target->GetDatabaseID()){
				if(type == SPAWN_SET_VALUE_SPAWN_SCRIPT)
					spawn->SetSpawnScript(value);
				else
					commands.SetSpawnCommand(client, spawn, type, value);
			}
		}
		MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
	}
}

void ZoneServer::DeleteSpawnScriptTimers(Spawn* spawn, bool all){
	MSpawnScriptTimers.writelock(__FUNCTION__, __LINE__);
	MRemoveSpawnScriptTimersList.writelock(__FUNCTION__, __LINE__);
	if(spawn_script_timers.size() > 0){
		set<SpawnScriptTimer*>::iterator itr;
		SpawnScriptTimer* timer = 0;
		for (itr = spawn_script_timers.begin(); itr != spawn_script_timers.end(); itr++) {
			timer = *itr;
			if((all || timer->spawn == spawn->GetID()) && remove_spawn_script_timers_list.count(timer) == 0)
				remove_spawn_script_timers_list.insert(timer);
		}

		if(all)
			spawn_script_timers.clear();
	}
	MRemoveSpawnScriptTimersList.releasewritelock(__FUNCTION__, __LINE__);
	MSpawnScriptTimers.releasewritelock(__FUNCTION__, __LINE__);
}

void ZoneServer::DeleteSpawnScriptTimers() {
	MSpawnScriptTimers.writelock(__FUNCTION__, __LINE__);
	MRemoveSpawnScriptTimersList.writelock(__FUNCTION__, __LINE__);
	if(remove_spawn_script_timers_list.size() > 0){
		set<SpawnScriptTimer*>::iterator itr;
		SpawnScriptTimer* timer = 0;
		
		for (itr = remove_spawn_script_timers_list.begin(); itr != remove_spawn_script_timers_list.end(); itr++) {
			timer = *itr;
			spawn_script_timers.erase(timer);
			safe_delete(timer);	
		}
		remove_spawn_script_timers_list.clear();
	}
	MRemoveSpawnScriptTimersList.releasewritelock(__FUNCTION__, __LINE__);
	MSpawnScriptTimers.releasewritelock(__FUNCTION__, __LINE__);
}

void ZoneServer::CheckSpawnScriptTimers(){
	DeleteSpawnScriptTimers();
	SpawnScriptTimer* timer = 0;
	vector<SpawnScriptTimer*> call_timers;
	MSpawnScriptTimers.readlock(__FUNCTION__, __LINE__);
	MRemoveSpawnScriptTimersList.writelock(__FUNCTION__, __LINE__);
	if(spawn_script_timers.size() > 0){
		int32 current_time = Timer::GetCurrentTime2();
		set<SpawnScriptTimer*>::iterator itr;
		for (itr = spawn_script_timers.begin(); itr != spawn_script_timers.end(); itr++) {
			timer = *itr;
			if(timer->current_count < timer->max_count && current_time >= timer->timer){
				timer->current_count++;	
				call_timers.push_back(timer);
			}
			if(timer->current_count >= timer->max_count && remove_spawn_script_timers_list.count(timer) == 0)
				remove_spawn_script_timers_list.insert(timer);
		}
	}
	MRemoveSpawnScriptTimersList.releasewritelock(__FUNCTION__, __LINE__);
	MSpawnScriptTimers.releasereadlock(__FUNCTION__, __LINE__);
	if(call_timers.size() > 0){
		vector<SpawnScriptTimer*>::iterator itr;
		for(itr = call_timers.begin(); itr != call_timers.end(); itr++){
			timer = *itr;
			CallSpawnScript(GetSpawnByID(timer->spawn), SPAWN_SCRIPT_TIMER, timer->player > 0 ? GetSpawnByID(timer->player) : 0, timer->function.c_str());
		}
	}
}

void ZoneServer::KillSpawnByDistance(Spawn* spawn, float max_distance, bool include_players, bool send_packet){
	Spawn* test_spawn = 0;
	map<int32, Spawn*>::iterator itr;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
		test_spawn = itr->second;
		if(test_spawn && test_spawn->IsEntity() && test_spawn != spawn && (!test_spawn->IsPlayer() || include_players)){
			if(test_spawn->GetDistance(spawn) < max_distance)
				KillSpawn(test_spawn, spawn, send_packet);
		}
	}
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::SpawnSetByDistance(Spawn* spawn, float max_distance, string field, string value){
	Spawn* test_spawn = 0;
	int32 type = commands.GetSpawnSetType(field);
	if(type == 0xFFFFFFFF)
		return;

	map<int32, Spawn*>::iterator itr;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
		test_spawn = itr->second;
		if(test_spawn && test_spawn != spawn && !test_spawn->IsPlayer()){
			if(test_spawn->GetDistance(spawn) < max_distance){
				commands.SetSpawnCommand(0, test_spawn, type, value.c_str());
			}
		}
	}
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::AddSpawnScriptTimer(SpawnScriptTimer* timer){
	MSpawnScriptTimers.writelock(__FUNCTION__, __LINE__);
	spawn_script_timers.insert(timer);
	MSpawnScriptTimers.releasewritelock(__FUNCTION__, __LINE__);
}

/*
void ZoneServer::RemoveFromRangeMap(Client* client){
	spawn_range_map.erase(client);
}
*/

void ZoneServer::RemoveSpawn(Spawn* spawn, bool delete_spawn, bool respawn, bool lock) 
{
	LogWrite(ZONE__DEBUG, 3, "Zone", "Processing RemoveSpawn function...");

	RemoveSpawnSupportFunctions(spawn);
	RemoveDeadEnemyList(spawn);
	if (lock)
		MDeadSpawns.writelock(__FUNCTION__, __LINE__);
	if (dead_spawns.count(spawn->GetID()) > 0)
		dead_spawns.erase(spawn->GetID());
	if (lock)
		MDeadSpawns.releasewritelock(__FUNCTION__, __LINE__);;

	if (spawn_expire_timers.count(spawn->GetID()) > 0)
		spawn_expire_timers.erase(spawn->GetID());
	
	RemoveDelayedSpawnRemove(spawn);

	if(respawn && !spawn->IsPlayer() && spawn->GetRespawnTime() > 0 && spawn->GetSpawnLocationID() > 0)
	{
		LogWrite(ZONE__DEBUG, 3, "Zone", "Handle NPC Respawn for '%s'.", spawn->GetName());

		// handle instance spawn db info
		// we don't care if a NPC or a client kills the spawn, we could have events that cause NPCs to kill NPCs.
		if(spawn->GetZone()->GetInstanceID() > 0 && spawn->GetSpawnLocationID() > 0)
		{
			LogWrite(ZONE__DEBUG, 3, "Zone", "Handle NPC Respawn in an Instance.");
			// use respawn time to either insert/update entry (likely insert in this situation)
			if ( spawn->IsNPC() )
			{
				database.CreateInstanceSpawnRemoved(spawn->GetSpawnLocationID(),SPAWN_ENTRY_TYPE_NPC, 
				spawn->GetRespawnTime(),spawn->GetZone()->GetInstanceID());
			}
			else if ( spawn->IsObject ( ) )
			{
				database.CreateInstanceSpawnRemoved(spawn->GetSpawnLocationID(),SPAWN_ENTRY_TYPE_OBJECT, 
				spawn->GetRespawnTime(),spawn->GetZone()->GetInstanceID());
			}
		}
		else
			respawn_timers.Put(spawn->GetSpawnLocationID(), Timer::GetCurrentTime2() + spawn->GetRespawnTime()*1000);
	}

	PacketStruct* packet = 0;
	int16 packet_version = 0;
	Client* client = 0;

	// Clear the pointer in the spawn list, spawn thread will remove the key
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	spawn_list[spawn->GetID()] = 0;
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
	
	vector<Client*>::iterator client_itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;

		if (client) {
			if(client->IsConnected() && (!packet || packet_version != client->GetVersion()))
			{
				safe_delete(packet);
				packet_version = client->GetVersion();
				packet = configReader.getStruct("WS_DestroyGhostCmd", packet_version);
			}

			if(client->GetPlayer()->HasTarget() && client->GetPlayer()->GetTarget() == spawn)
				client->GetPlayer()->SetTarget(0);

			SendRemoveSpawn(client, spawn, packet, delete_spawn);
			if(spawn_range_map.count(client) > 0)
				spawn_range_map.Get(client)->erase(spawn->GetID());
		}
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);

	safe_delete(packet);

	// Do we really need the mutex locks and check to dead_spawns as we remove it from dead spawns at the start of this function
	if (lock)
		MDeadSpawns.readlock(__FUNCTION__, __LINE__);
	if(delete_spawn && dead_spawns.count(spawn->GetID()) == 0)
		AddPendingDelete(spawn);
	if (lock)
		MDeadSpawns.releasereadlock(__FUNCTION__, __LINE__);

	LogWrite(ZONE__DEBUG, 3, "Zone", "Done processing RemoveSpawn function...");
}

void ZoneServer::RemoveSpawnFromClient(Spawn* spawn) {
	Client* client = 0;
	PacketStruct* packet = 0;
	int16 packet_version = 0;

	vector<Client*>::iterator client_itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;

		if (client) {
			if (!client->IsConnected())
				continue;

			if (client->GetPlayer() == spawn)
				continue;

			if (!packet || packet_version != client->GetVersion())
			{
				safe_delete(packet);
				packet = configReader.getStruct("WS_DestroyGhostCmd", client->GetVersion());
			}

			if (client->GetPlayer()->HasTarget() && client->GetPlayer()->GetTarget() == spawn)
				client->GetPlayer()->SetTarget(0);

			SendRemoveSpawn(client, spawn, packet);

			if (spawn_range_map.count(client) > 0)
				spawn_range_map.Get(client)->erase(spawn->GetID());
		}
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

Spawn* ZoneServer::GetClosestSpawn(Spawn* spawn, int32 spawn_id){
	Spawn* closest_spawn = 0;
	Spawn* test_spawn = 0;
	float closest_distance = 1000000;
	float test_distance = 0;

	map<int32, Spawn*>::iterator itr;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
		test_spawn = itr->second;
		if(test_spawn && test_spawn->GetDatabaseID() == spawn_id){
			test_distance = test_spawn->GetDistance(spawn);
			if(test_distance < closest_distance){
				closest_distance = test_distance;
				closest_spawn = test_spawn;
			}
		}
	}
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);

	return closest_spawn;
}

int32 ZoneServer::GetClosestLocation(Spawn* spawn){
	Spawn* closest_spawn = 0;
	Spawn* test_spawn = 0;
	float closest_distance = 1000000;
	float test_distance = 0;

	map<int32, Spawn*>::iterator itr;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
		test_spawn = itr->second;
		if(test_spawn){
			test_distance = test_spawn->GetDistance(spawn);
			if(test_distance < closest_distance){
				closest_distance = test_distance;
				closest_spawn = test_spawn;
				if(closest_distance < 10)
					break;
			}
		}
	}
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);

	if(closest_spawn)
		return closest_spawn->GetLocation();
	return 0;
}

void ZoneServer::SendQuestUpdates(Client* client, Spawn* spawn){
	if(!client)
		return;

	if(spawn){
		if(client->GetPlayer()->WasSentSpawn(spawn->GetID()) && !client->GetPlayer()->WasSpawnRemoved(spawn))
			SendSpawnChanges(spawn, client, false, true);
	}
	else{
		map<int32, Spawn*>::iterator itr;
		MSpawnList.readlock(__FUNCTION__, __LINE__);
		for (itr = spawn_list.begin(); itr != spawn_list.end(); ++itr) {
			spawn = itr->second;

			if (spawn) {
				spawn->m_requiredQuests.readlock(__FUNCTION__, __LINE__);
				if (client->GetPlayer()->WasSentSpawn(spawn->GetID()) && !client->GetPlayer()->WasSpawnRemoved(spawn) && (client->GetPlayer()->CheckQuestRemoveFlag(spawn) || client->GetPlayer()->CheckQuestFlag(spawn) != 0 || (spawn->GetQuestsRequired()->size() > 0 && client->GetPlayer()->CheckQuestRequired(spawn))))
					SendSpawnChanges(spawn, client, false, true);
				spawn->m_requiredQuests.releasereadlock(__FUNCTION__, __LINE__);
			}
		}
		MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
	}
}

void ZoneServer::SendAllSpawnsForLevelChange(Client* client){
	Spawn* spawn = 0;
	if(spawn_range_map.count(client) > 0) {
		MutexMap<int32, float >::iterator itr = spawn_range_map.Get(client)->begin();
		while(itr.Next()) {
			spawn = GetSpawnByID(itr->first);
			if(spawn && client->GetPlayer()->WasSentSpawn(spawn->GetID()) && !client->GetPlayer()->WasSpawnRemoved(spawn)) {
				SendSpawnChanges(spawn, client, false, true);
				// Attempt to slow down the packet spam sent to the client
				Sleep(5);
			}
		}
	}
}

void ZoneServer::StartZoneSpawnsForLevelThread(Client* client){
	if(zoneShuttingDown)
		return;

#ifdef WIN32
	_beginthread(SendLevelChangedSpawns, 0, client);
#else
	pthread_t thread;
	pthread_create(&thread, NULL, SendLevelChangedSpawns, client);
	pthread_detach(thread);
#endif
}

void ZoneServer::StartZoneSpawnsForAggroThread(Client* client) {
	if (zoneShuttingDown)
		return;

#ifdef WIN32
	_beginthread(SendAggroChangedSpawns, 0, client);
#else
	pthread_t thread;
	pthread_create(&thread, NULL, SendAggroChangedSpawns, client);
	pthread_detach(thread);
#endif
}

void ZoneServer::ReloadClientQuests(){
	Client* client = 0;
	vector<Client*>::iterator client_itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		if(client)
			client->ReloadQuests();
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::SendCalculatedXP(Player* player, Spawn* victim){
	if (player && victim) {
		if (player->GetGroupMemberInfo()) {
			world.GetGroupManager()->GroupLock(__FUNCTION__, __LINE__);

			deque<GroupMemberInfo*>* members = world.GetGroupManager()->GetGroupMembers(player->GetGroupMemberInfo()->group_id);
			deque<GroupMemberInfo*>::iterator itr;
			for (itr = members->begin(); itr != members->end(); itr++) {
				GroupMemberInfo* gmi = *itr;
				if (gmi->client) {
					Player* group_member = gmi->client->GetPlayer();
					float xp = group_member->CalculateXP(victim) / members->size();
					if (xp > 0) {
						int16 level = group_member->GetLevel();
						if (group_member->AddXP((int32)xp)) {
							gmi->client->Message(CHANNEL_COLOR_EXP, "You gain %u experience!", (int32)xp);
							LogWrite(PLAYER__DEBUG, 0, "Player", "Player: %s earned %u experience (GroupID %u)", group_member->GetName(), (int32)xp, player->GetGroupMemberInfo()->group_id);
							if(group_member->GetLevel() != level)
								gmi->client->ChangeLevel(level, group_member->GetLevel());
							group_member->SetCharSheetChanged(true);
						}
					}
				}
			}

			world.GetGroupManager()->ReleaseGroupLock(__FUNCTION__, __LINE__);
		}
		else {
			float xp = player->CalculateXP(victim);
			if (xp > 0) {
				Client* client = GetClientBySpawn(player);
				if(!client)
					return;
				int16 level = player->GetLevel();
				if (player->AddXP((int32)xp)) {
					client->Message(CHANNEL_COLOR_EXP, "You gain %u XP!", (int32)xp);
					LogWrite(PLAYER__DEBUG, 0, "Player", "Player: %s earned %u experience.", player->GetName(), (int32)xp);
					if(player->GetLevel() != level)
						client->ChangeLevel(level, player->GetLevel());
					player->SetCharSheetChanged(true);
				}
			}
		}
	}
}

void ZoneServer::ProcessFaction(Spawn* spawn, Client* client)
{
	Player* player = client->GetPlayer();

	if(client && !spawn->IsPlayer() && spawn->GetFactionID() > 10)
	{
		bool update_result = false;
		Faction* faction = 0;
		vector<int32>* factions = 0;

		if(player->GetFactions()->ShouldDecrease(spawn->GetFactionID()))
		{
			update_result = player->GetFactions()->DecreaseFaction(spawn->GetFactionID());
			faction = master_faction_list.GetFaction(spawn->GetFactionID());

			if(faction && update_result)
				client->Message(CHANNEL_COLOR_FACTION, "Your faction standing with %s got worse.", faction->name.c_str());
			else if(faction)
				client->Message(CHANNEL_COLOR_FACTION, "Your faction standing with %s could not possibly get any worse.", faction->name.c_str());

			factions = master_faction_list.GetHostileFactions(spawn->GetFactionID());

			if(factions)
			{
				vector<int32>::iterator itr;

				for(itr = factions->begin(); itr != factions->end(); itr++)
				{
					if(player->GetFactions()->ShouldIncrease(*itr))
					{
						update_result = player->GetFactions()->IncreaseFaction(*itr);
						faction = master_faction_list.GetFaction(*itr);

						if(faction && update_result)
							client->Message(CHANNEL_COLOR_FACTION, "Your faction standing with %s got better.", faction->name.c_str());
						else if(faction)
							client->Message(CHANNEL_COLOR_FACTION, "Your faction standing with %s could not possibly get any better.", faction->name.c_str());
					}
				}
			}
		}

		factions = master_faction_list.GetFriendlyFactions(spawn->GetFactionID());

		if(factions)
		{
			vector<int32>::iterator itr;

			for(itr = factions->begin(); itr != factions->end(); itr++)
			{
				if(player->GetFactions()->ShouldDecrease(*itr))
				{
					update_result = player->GetFactions()->DecreaseFaction(*itr);
					faction = master_faction_list.GetFaction(*itr);

					if(faction && update_result)
						client->Message(CHANNEL_COLOR_FACTION, "Your faction standing with %s got worse.", faction->name.c_str());
					else if(faction)
						client->Message(CHANNEL_COLOR_FACTION, "Your faction standing with %s could not possibly get any worse.", faction->name.c_str());
				}
			}
		}

		EQ2Packet* outapp = client->GetPlayer()->GetFactions()->FactionUpdate(client->GetVersion());

		if(outapp)
			client->QueuePacket(outapp);
	}
}

void ZoneServer::Despawn(Spawn* spawn, int32 timer){
	if(!spawn || spawn->IsPlayer())
		return;

	RemoveSpawnSupportFunctions(spawn);
	if(spawn->IsEntity())
		((Entity*)spawn)->InCombat(false);
	if(timer == 0)
		timer = 1;
	AddDeadSpawn(spawn, timer);
}

void ZoneServer::KillSpawn(Spawn* dead, Spawn* killer, bool send_packet, int8 damage_type, int16 kill_blow_type)
{
	MDeadSpawns.readlock(__FUNCTION__, __LINE__);
	if(!dead || this->dead_spawns.count(dead->GetID()) > 0) {
		MDeadSpawns.releasereadlock(__FUNCTION__, __LINE__);
		return;
	}
	MDeadSpawns.releasereadlock(__FUNCTION__, __LINE__);

	PacketStruct* packet = 0;
	Client* client = 0;
	vector<int32>* encounter = 0;
	bool killer_in_encounter = false;

	if(dead->IsEntity())
	{
		// Check kill and death procs
		if (killer && dead != killer){
			if (dead->IsEntity())
				((Entity*)dead)->CheckProcs(PROC_TYPE_DEATH, killer);
			if (killer->IsEntity())
				((Entity*)killer)->CheckProcs(PROC_TYPE_KILL, dead);

			//Check if caster is alive after death proc called, incase of deathsave
			if (dead->Alive())
				return;
		}

		if (killer && killer->IsPlayer()) {
			((Player*)killer)->InCombat(false);
			((Player*)killer)->SetRangeAttack(false);
			((Player*)killer)->SetMeleeAttack(false);

			if (dead->IsPlayer() && PVP::IsEnabled()) {
				Player* dead_player = static_cast<Player*>(dead);
				Player* killer_player = static_cast<Player*>(killer);
				int dead_rank = PVP::GetRankIndex(dead_player);
				int killer_rank = PVP::GetRankIndex(killer_player);
				int ranking_difference = dead_rank - killer_rank;
				
				if (ranking_difference >= 1 && ranking_difference <= 2) {
					dead_player->SetFame(dead_player->GetFame() - 10);
					killer_player->SetFame(killer_player->GetFame() + 10);
					GetClientBySpawn(dead_player)->SimpleMessage(CHANNEL_COLOR_YELLOW, "Your death has decreased your fame.");
					GetClientBySpawn(killer_player)->SimpleMessage(CHANNEL_COLOR_YELLOW, "Your victory has increased your fame.");
				} else if (ranking_difference <= 0 && ranking_difference >= -2) {
					dead_player->SetFame(dead_player->GetFame() - 5);
					killer_player->SetFame(killer_player->GetFame() + 5);
					GetClientBySpawn(dead_player)->SimpleMessage(CHANNEL_COLOR_YELLOW, "Your death has decreased your fame.");
					GetClientBySpawn(killer_player)->SimpleMessage(CHANNEL_COLOR_YELLOW, "Your victory has increased your fame.");
				}

				if (dead_rank != PVP::GetRankIndex(dead_player)) {
					int rank = PVP::GetRankIndex(dead_player);
					if (rank == 0) {
						GetClientBySpawn(dead_player)->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are no longer ranked.");
						GetClientBySpawn(dead_player)->SendPopupMessage(10, "Your are no longer ranked.", "", 2, 0xFF, 0xFF, 0xFF);
					} else if (rank < dead_rank) {
						char message[37];
						sprintf(message, "Your rank has dropped to %s.", PVP::GetRank(dead_player).c_str());
						GetClientBySpawn(dead_player)->SimpleMessage(CHANNEL_COLOR_YELLOW, message);
						GetClientBySpawn(dead_player)->SendPopupMessage(10, message, "", 2, 0xFF, 0xFF, 0xFF);
					}
					GetClientBySpawn(dead_player)->SendTitleUpdate();
				}

				if (PVP::GetRankIndex(killer_player) > killer_rank) {
					char message[42];
					sprintf(message, "You have obtained the rank of %s.", PVP::GetRank(killer_player).c_str());
					GetClientBySpawn(killer_player)->SimpleMessage(CHANNEL_COLOR_YELLOW, message);
					GetClientBySpawn(killer_player)->SendPopupMessage(10, message, "", 2, 0xFF, 0xFF, 0xFF);
					GetClientBySpawn(killer_player)->SendTitleUpdate();
				}

				dead_player->UpdatePlayerStatistic(STAT_PLAYER_TOTAL_PVP_DEATHS, 1);
				killer_player->UpdatePlayerStatistic(STAT_PLAYER_TOTAL_PVP_KILLS, 1);
			}
		}

		if (dead->IsPlayer()) {
			((Player*)dead)->InCombat(false);
			((Player*)dead)->SetRangeAttack(false);
			((Player*)dead)->SetMeleeAttack(false);
		} else {
			((Entity*)dead)->InCombat(false);
		}

		dead->SetInitialState(16512, false); // This will make aerial npc's fall after death
		dead->SetHP(0);
		dead->SetSpawnType(3);
		dead->appearance.attackable = 0;

		// Remove hate towards dead from all npc's in the zone
		ClearHate((Entity*)dead);

		RemoveSpellTimersFromSpawn(dead, true, !dead->IsPlayer());

		if(dead->IsPlayer()) 
		{
			((Player*)dead)->UpdatePlayerStatistic(STAT_PLAYER_TOTAL_DEATHS, 1);
			client = GetClientBySpawn(dead);

			if(client) {
				if(client->GetPlayer()->DamageEquippedItems(10, client))
					client->QueuePacket(client->GetPlayer()->GetEquipmentList()->serialize(client->GetVersion()));

				client->DisplayDeadWindow();
			}
		}
		else if (dead->IsNPC()) {
			encounter = ((NPC*)dead)->Brain()->GetEncounter();
		}

	}

	dead->SetActionState(0);
	dead->SetTempActionState(0);

	// If dead is an npc get the encounter and loop through it giving out the rewards, no rewards for pets
	if (dead->IsNPC() && !dead->IsPet()) {
		Spawn* spawn = 0;
		int8 size = encounter->size();
		// Needs npc to have access to the encounter list for who is allowed to loot
		NPC* chest = 0;

		if (!((NPC*)dead)->Brain()->PlayerInEncounter()) {
			((NPC*)dead)->SetLootCoins(0);
			((NPC*)dead)->GetLootItems()->clear();
		}

		// If dead has loot attempt to drop a chest
		if (((NPC*)dead)->HasLoot()) {
			chest = ((NPC*)dead)->DropChest();
		}


		for (int8 i = 0; i < encounter->size(); i++) {
			spawn = GetSpawnByID(encounter->at(i));
			// set a flag to let us know if the killer is in the encounter
			if (!killer_in_encounter && spawn == killer)
				killer_in_encounter = true;

			if (spawn && spawn->IsPlayer()) {
				// Update players total kill count
				((Player*)spawn)->UpdatePlayerStatistic(STAT_PLAYER_TOTAL_NPC_KILLS, 1);

				// If this was an epic mob kill send the announcement for this player
				if (dead->GetEncounterLevel() >= 10)
					SendEpicMobDeathToGuild((Player*)spawn, dead);

				// Clear hostile spells from the players spell queue
				spellProcess->RemoveSpellFromQueue((Player*)spawn, true);

				// Get the client of the player
				client = GetClientBySpawn(spawn);
				// valid client?
				if(client) {
					// Check for quest kill updates
					client->CheckPlayerQuestsKillUpdate(dead);
					// If the dead mob is not a player and if it had a faction with an ID greater or equal to 10 the send faction changes
					if(!dead->IsPlayer() && dead->GetFactionID() > 10)
						ProcessFaction(dead, client);

					// Send xp...this is currently wrong fix it
					if (spawn != dead && ((Player*)spawn)->GetArrowColor(dead->GetLevel()) >= ARROW_COLOR_GREEN) {
						//SendCalculatedXP((Player*)spawn, dead);

						float xp = ((Player*)spawn)->CalculateXP(dead) / size;
						if (xp > 0) {
							int16 level = spawn->GetLevel();
							if (((Player*)spawn)->AddXP((int32)xp)) {
								client->Message(CHANNEL_COLOR_EXP, "You gain %u XP!", (int32)xp);
								LogWrite(PLAYER__DEBUG, 0, "Player", "Player: %s earned %u experience.", spawn->GetName(), (int32)xp);
								if(spawn->GetLevel() != level)
									client->ChangeLevel(level, spawn->GetLevel());
								((Player*)spawn)->SetCharSheetChanged(true);
							}
						}
					}
				}
			}

			// If a chest is being dropped add this spawn to the chest's encounter so they can loot it
			if (chest)
				chest->Brain()->AddToEncounter((Entity*)spawn);
		}

		// If a chest is being dropped add it to the world and set the timer to remove it.
		if (chest) {
			AddSpawn(chest);
			AddDeadSpawn(chest, 0xFFFFFFFF);
			LogWrite(LOOT__DEBUG, 0, "Loot", "Adding a chest to the world...");
		}
	}

	// Reset client pointer
	client = 0;

	// Killer was not in the encounter, give them the faction hit but no xp
	if (!killer_in_encounter) {
		// make sure the killer is a player and the dead spawn had a faction and wasn't a player
		if (killer && killer->IsPlayer()) {
			if (!dead->IsPlayer() && dead->GetFactionID() > 10) {
			client = GetClientBySpawn(killer);
			if (client)
				ProcessFaction(dead, client);
			}

			// Clear hostile spells from the killers spell queue
			spellProcess->RemoveSpellFromQueue((Player*)killer, true);
		}
	}

	// Reset client pointer
	client = 0;	


	vector<Spawn*>* group = dead->GetSpawnGroup();
	if (group && group->size() == 1)
		CallSpawnScript(dead, SPAWN_SCRIPT_GROUP_DEAD, killer);
	safe_delete(group);


	// Remove the support functions for the dead spawn
	RemoveSpawnSupportFunctions(dead);

	// Erase the expire timer if it has one
	if (spawn_expire_timers.count(dead->GetID()) > 0)
		spawn_expire_timers.erase(dead->GetID());

	// If dead is an npc or object call the spawn scrip and handle instance stuff
	if(dead->IsNPC() || dead->IsObject())
	{
		// handle instance spawn db info
		// we don't care if a NPC or a client kills the spawn, we could have events that cause NPCs to kill NPCs.
		if(dead->GetZone()->GetInstanceID() > 0 && dead->GetSpawnLocationID() > 0)
		{
			// use respawn time to either insert/update entry (likely insert in this situation)
			if(dead->IsNPC())
				database.CreateInstanceSpawnRemoved(dead->GetSpawnLocationID(),SPAWN_ENTRY_TYPE_NPC, dead->GetRespawnTime(),dead->GetZone()->GetInstanceID());
			else if ( dead->IsObject ( ) )
				database.CreateInstanceSpawnRemoved(dead->GetSpawnLocationID(),SPAWN_ENTRY_TYPE_OBJECT, dead->GetRespawnTime(),dead->GetZone()->GetInstanceID());
		}

		// Call the spawn scripts death() function
		CallSpawnScript(dead, SPAWN_SCRIPT_DEATH, killer);
	}
	
	int32 victim_id = dead->GetID();
	int32 attacker_id = 0xFFFFFFFF;

	if(killer)
		attacker_id = killer->GetID();

	if(send_packet)
	{
		vector<Client*>::iterator client_itr;
		MClientList.readlock(__FUNCTION__, __LINE__);
		for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
			client = *client_itr;
			if(!client->GetPlayer()->WasSentSpawn(victim_id) || (attacker_id != 0xFFFFFFFF && !client->GetPlayer()->WasSentSpawn(attacker_id)) )
				continue;
			else if(client->GetPlayer()->WasSpawnRemoved(dead) || (attacker_id != 0xFFFFFFFF && client->GetPlayer()->WasSpawnRemoved(killer)))
				continue;
			else if(killer && killer->GetDistance(client->GetPlayer()) > HEAR_SPAWN_DISTANCE)
				continue;

			packet = configReader.getStruct("WS_HearDeath", client->GetVersion());
			if(packet)
			{
				if(killer)
					packet->setDataByName("attacker", client->GetPlayer()->GetIDWithPlayerSpawn(killer));
				else
					packet->setDataByName("attacker", 0xFFFFFFFF);

				packet->setDataByName("defender", client->GetPlayer()->GetIDWithPlayerSpawn(dead));
				packet->setDataByName("damage_type", damage_type);
				packet->setDataByName("blow_type", kill_blow_type);

				client->QueuePacket(packet->serialize());
				LogWrite(COMBAT__DEBUG, 0, "Combat", "Zone Killing '%s'", dead->GetName());
				safe_delete(packet);
			}
		}
		MClientList.releasereadlock(__FUNCTION__, __LINE__);
	}


	int32 pop_timer = 0xFFFFFFFF;
	if(killer && killer->IsNPC())
	{
		// Call the spawn scripts killed() function
		CallSpawnScript(killer, SPAWN_SCRIPT_KILLED, dead);		

		if(!dead->IsPlayer())
		{
			LogWrite(MISC__TODO, 1, "TODO", "Whenever pets are added, check for pet kills\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
			// Set the time for the corpse to linger to 5 sec
			//pop_timer = 5000;
			// commented out the timer so in the event the killer is not a player (pet, guard, something else i haven't thought of)
			// the corpse doesn't vanish sooner then it should if it had loot for the players.  AddDeadSpawn() will set timers based on if
			// the corpse has loot or not if the timer value (pop_timer) is 0xFFFFFFFF
		}
	}
	
	// If the dead spawns was not a player add it to the dead spawn list
	if (!dead->IsPlayer())
		AddDeadSpawn(dead, pop_timer);

	// if dead was a player clear hostile spells from its spell queue
	if (dead->IsPlayer())
		spellProcess->RemoveSpellFromQueue((Player*)dead, true);



	// ResetPetInfo() is called in DismissPet(), might not need to be here

	// Players pet is killed, clear the pet info from char sheet
	/*if (dead->IsNPC() && ((NPC*)dead)->IsPet() && ((NPC*)dead)->GetOwner()->IsPlayer())
		((Player*)((NPC*)dead)->GetOwner())->ResetPetInfo();*/

	safe_delete(encounter);
}

void ZoneServer::SendDamagePacket(Spawn* attacker, Spawn* victim, int8 type1, int8 type2, int8 damage_type, int16 damage, const char* spell_name){
	//Scat: was set but never being used anywhere. i see references to 0xFFFFFFFF below so could be old code not used anymore
	//int32 attacker_id = 0xFFFFFFFF;
	//if(attacker)
	//	attacker_id = attacker->GetID();
	PacketStruct* packet = 0;
	Client* client = 0;
	if(attacker && victim->IsPlayer() && victim->GetTarget() == 0){
		client = GetClientBySpawn(victim);
		if(client)
			client->TargetSpawn(attacker);
	}

	vector<Client*>::iterator client_itr;
	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		if(!client || (client->GetPlayer() != attacker && client->GetPlayer() != victim && ((attacker && client->GetPlayer()->WasSentSpawn(attacker->GetID()) == false) || (victim && client->GetPlayer()->WasSentSpawn(victim->GetID()) == false))))
			continue;
		if((attacker && client->GetPlayer()->WasSpawnRemoved(attacker)) || (victim && client->GetPlayer()->WasSpawnRemoved(victim)))
			continue;
		if(attacker && attacker->GetDistance(client->GetPlayer()) > 50)
			continue;
		if(victim && victim->GetDistance(client->GetPlayer()) > 50)
			continue;
		switch(type1){
			case DAMAGE_PACKET_TYPE_SIPHON_SPELL:
			case DAMAGE_PACKET_TYPE_SIPHON_SPELL2:
				packet = configReader.getStruct("WS_HearSiphonSpellDamage", client->GetVersion());
				break;
			case DAMAGE_PACKET_TYPE_MULTIPLE_DAMAGE:
				packet = configReader.getStruct("WS_HearMultipleDamage", client->GetVersion());
				break;
			case DAMAGE_PACKET_TYPE_SIMPLE_CRIT_DMG:
			case DAMAGE_PACKET_TYPE_SIMPLE_DAMAGE:
				packet = configReader.getStruct("WS_HearSimpleDamage", client->GetVersion());
				break;
			case DAMAGE_PACKET_TYPE_SPELL_DAMAGE2:
			case DAMAGE_PACKET_TYPE_SPELL_DAMAGE3:
			case DAMAGE_PACKET_TYPE_SPELL_CRIT_DMG:
			case DAMAGE_PACKET_TYPE_SPELL_DAMAGE:
				packet = configReader.getStruct("WS_HearSpellDamage", client->GetVersion());
				if (packet)
					packet->setSubstructDataByName("header", "unknown", 5);
				break;
			case DAMAGE_PACKET_TYPE_RANGE_DAMAGE:
				packet = configReader.getStruct("WS_HearRangeDamage", client->GetVersion());
				break;
			case DAMAGE_PACKET_TYPE_RANGE_SPELL_DMG:
			case DAMAGE_PACKET_TYPE_RANGE_SPELL_DMG2:
				packet = configReader.getStruct("WS_HearRangeDamage", client->GetVersion());
				break;
			default:
				LogWrite(ZONE__ERROR, 0, "Zone", "Unknown Damage Packet type: %i in ZoneServer::SendDamagePacket.", type1);
				MClientList.releasereadlock(__FUNCTION__, __LINE__);
				return;
		}

		if(packet){
			packet->setSubstructDataByName("header", "packet_type", type1);
			packet->setSubstructDataByName("header", "result_type", type2);
			if(!attacker)
				packet->setSubstructDataByName("header", "attacker", 0xFFFFFFFF);
			else
				packet->setSubstructDataByName("header", "attacker", client->GetPlayer()->GetIDWithPlayerSpawn(attacker));
			packet->setSubstructDataByName("header", "defender", client->GetPlayer()->GetIDWithPlayerSpawn(victim));			
			packet->setDataByName("damage_type", damage_type);
			packet->setDataByName("damage", damage);
			if(spell_name)
				packet->setSmallStringByName("spell_name", spell_name);
			EQ2Packet* app = packet->serialize();
		    //DumpPacket(app);
			client->QueuePacket(app);
			safe_delete(packet);
			packet = 0;
		}
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::SendHealPacket(Spawn* caster, Spawn* target, int16 heal_type, int32 heal_amt, const char* spell_name){
	Client* client = 0;
	vector<Client*>::iterator client_itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		if(!client || (client->GetPlayer() != caster && ((caster && client->GetPlayer()->WasSentSpawn(caster->GetID()) == false) || (target && client->GetPlayer()->WasSentSpawn(target->GetID()) == false))))
			continue;
		if((caster && client->GetPlayer()->WasSpawnRemoved(caster)) || (caster && client->GetPlayer()->WasSpawnRemoved(target)))
			continue;
		if(caster && caster->GetDistance(client->GetPlayer()) > 50)
			continue;
		if(target && target->GetDistance(client->GetPlayer()) > 50)
			continue;
		

		PacketStruct* packet = configReader.getStruct("WS_HearHeal", client->GetVersion());
		if (packet) {
			packet->setDataByName("caster", client->GetPlayer()->GetIDWithPlayerSpawn(caster));
			packet->setDataByName("target", client->GetPlayer()->GetIDWithPlayerSpawn(target));
			packet->setDataByName("heal_amt", heal_amt);
			packet->setDataByName("spellname", spell_name);
			packet->setDataByName("type", heal_type);
			packet->setDataByName("unknown2", 1);
			EQ2Packet* app = packet->serialize();
			client->QueuePacket(app);
			safe_delete(packet);
		}
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::SendThreatPacket(Spawn* caster, Spawn* target, int32 threat_amt, const char* spell_name) {
	Client* client = 0;

	vector<Client*>::iterator client_itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		if(!client || (client->GetPlayer() != caster && ((caster && client->GetPlayer()->WasSentSpawn(caster->GetID()) == false) || (target && client->GetPlayer()->WasSentSpawn(target->GetID()) == false))))
			continue;
		if((caster && client->GetPlayer()->WasSpawnRemoved(caster)) || (caster && client->GetPlayer()->WasSpawnRemoved(target)))
			continue;
		if(caster && caster->GetDistance(client->GetPlayer()) > 50)
			continue;
		if(target && target->GetDistance(client->GetPlayer()) > 50)
			continue;
		
		PacketStruct* packet = configReader.getStruct("WS_HearThreatCmd", client->GetVersion());
		if (packet) {
			packet->setDataByName("spell_name", spell_name);
			packet->setDataByName("spawn_id", client->GetPlayer()->GetIDWithPlayerSpawn(caster));
			packet->setDataByName("target", client->GetPlayer()->GetIDWithPlayerSpawn(target));
			packet->setDataByName("threat_amount", threat_amt);
			
			client->QueuePacket(packet->serialize());
		}
		safe_delete(packet);
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::SendSpellFailedPacket(Client* client, int16 error){
	if(!client)
		return;

	PacketStruct* packet = configReader.getStruct("WS_DisplaySpellFailed", client->GetVersion());
	if(packet){
		/*		Temp solution, need to modify the error code before this function and while we still have access to the spell/combat art		*/
		error = master_spell_list.GetSpellErrorValue(client->GetVersion(), error);

		packet->setDataByName("error_code", error);
		//packet->PrintPacket();
		client->QueuePacket(packet->serialize());
		safe_delete(packet);
	}
}

void ZoneServer::SendInterruptPacket(Spawn* interrupted, LuaSpell* spell){
	if(!interrupted || !spell)
		return;
	
	EQ2Packet* outapp = 0;
	PacketStruct* packet = 0;
	Client* client = 0;
	vector<Client*>::iterator client_itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		if(!client || !client->GetPlayer()->WasSentSpawn(interrupted->GetID()) || client->GetPlayer()->WasSpawnRemoved(interrupted))
			continue;
		packet = configReader.getStruct("WS_Interrupt", client->GetVersion());
		if(packet){
			packet->setDataByName("spawn_id", client->GetPlayer()->GetIDWithPlayerSpawn(interrupted));
			packet->setArrayLengthByName("num_targets", spell->targets.size());
			for (int32 i = 0; i < spell->targets.size(); i++)
				packet->setArrayDataByName("target_id", client->GetPlayer()->GetIDWithPlayerSpawn(client->GetPlayer()->GetZone()->GetSpawnByID(spell->targets[i])), i);
			packet->setDataByName("spell_id", spell->spell->GetSpellID());
			outapp = packet->serialize();
			client->QueuePacket(outapp);
			safe_delete(packet);
		}
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
	safe_delete(packet);
}

void ZoneServer::SendCastSpellPacket(LuaSpell* spell, Entity* caster){
	EQ2Packet* outapp = 0;
	PacketStruct* packet = 0;
	Client* client = 0;
	if(!caster || !spell || !spell->spell || spell->interrupted)
		return;

	vector<Client*>::iterator client_itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		if(!client)
			continue;
		packet = configReader.getStruct("WS_HearCastSpell", client->GetVersion());
		if(packet){
			packet->setDataByName("spawn_id", client->GetPlayer()->GetIDWithPlayerSpawn(caster));
			packet->setArrayLengthByName("num_targets", spell->targets.size());
			for (int32 i = 0; i < spell->targets.size(); i++)
				packet->setArrayDataByName("target", client->GetPlayer()->GetIDWithPlayerSpawn(spell->caster->GetZone()->GetSpawnByID(spell->targets[i])), i);
			packet->setDataByName("spell_visual", spell->spell->GetSpellData()->spell_visual); //result
			packet->setDataByName("cast_time", spell->spell->GetSpellData()->cast_time*.01); //delay
			packet->setDataByName("spell_id", spell->spell->GetSpellID());
			packet->setDataByName("spell_level", 1);
			packet->setDataByName("spell_tier", spell->spell->GetSpellData()->tier);
			outapp = packet->serialize();
			client->QueuePacket(outapp);
			safe_delete(packet);
		}
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
	safe_delete(packet);
}

void ZoneServer::SendCastSpellPacket(int32 spell_visual, Spawn* target, Spawn* caster) {
	if (target) {
		vector<Client*>::iterator client_itr;
		EQ2Packet* outapp = 0;
		PacketStruct* packet = 0;

		MClientList.readlock(__FUNCTION__, __LINE__);
		for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
			Client* client = *client_itr;
			if (!client)
				continue;
			packet = configReader.getStruct("WS_HearCastSpell", client->GetVersion());
			if (packet) {
				int32 caster_id = 0xFFFFFFFF;
				if (caster) {
					caster_id = client->GetPlayer()->GetIDWithPlayerSpawn(caster);
				}

				packet->setDataByName("spawn_id", caster_id);
				packet->setArrayLengthByName("num_targets", 1);
				packet->setArrayDataByName("target", client->GetPlayer()->GetIDWithPlayerSpawn(target));
				packet->setDataByName("spell_id", 0xFFFFFFFF);
				packet->setDataByName("spell_visual", spell_visual);
				packet->setDataByName("cast_time", 0);
				packet->setDataByName("spell_id", 0);
				packet->setDataByName("spell_level", 0);
				packet->setDataByName("spell_tier", 0);
				outapp = packet->serialize();
				client->QueuePacket(outapp);
				safe_delete(packet);
			}
		}
		MClientList.releasereadlock(__FUNCTION__, __LINE__);
		safe_delete(packet);
	}
}

void ZoneServer::SendCastEntityCommandPacket(EntityCommand* entity_command, int32 spawn_id, int32 target_id) {
	if (entity_command) {
		Spawn* spawn = GetSpawnByID(spawn_id);
		Spawn* target = GetSpawnByID(target_id);
		if (!spawn || !target)
			return;

		Client* client = 0;
		vector<Client*>::iterator client_itr;

		MClientList.readlock(__FUNCTION__, __LINE__);
		for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
			client = *client_itr;
			if (!client || !client->GetPlayer()->WasSentSpawn(spawn_id) || client->GetPlayer()->WasSpawnRemoved(spawn) || !client->GetPlayer()->WasSentSpawn(target_id) || client->GetPlayer()->WasSpawnRemoved(target))
				continue;
			PacketStruct* packet = configReader.getStruct("WS_HearCastSpell", client->GetVersion());
			if (packet) {
				packet->setDataByName("spawn_id", client->GetPlayer()->GetIDWithPlayerSpawn(spawn));
				packet->setArrayLengthByName("num_targets", 1);
				packet->setArrayDataByName("target", client->GetPlayer()->GetIDWithPlayerSpawn(target));
				packet->setDataByName("num_targets", 1);
				packet->setDataByName("spell_visual", entity_command->spell_visual); //result
				packet->setDataByName("cast_time", entity_command->cast_time * 0.01); //delay
				packet->setDataByName("spell_id", 1);
				packet->setDataByName("spell_level", 1);
				packet->setDataByName("spell_tier", 1);
				EQ2Packet* outapp = packet->serialize();
				client->QueuePacket(outapp);
				safe_delete(packet);
			}
		}
		MClientList.releasereadlock(__FUNCTION__, __LINE__);
	}
}

void ZoneServer::StartZoneInitialSpawnThread(Client* client){
	if(zoneShuttingDown)
		return;

#ifdef WIN32
	_beginthread(SendInitialSpawns, 0, client);
#else
	pthread_t thread;
	pthread_create(&thread, NULL, SendInitialSpawns, client);
	pthread_detach(thread);
#endif
}

void ZoneServer::SendZoneSpawns(Client* client){
	initial_spawn_threads_active++;

	map<int32, Spawn*>::iterator itr;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
		Spawn* spawn = itr->second;
		if (spawn) {
			CheckSpawnRange(client, spawn, true);
		}
	}
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
	
	CheckSendSpawnToClient(client, true);
	client->SetConnected(true);
	ClientPacketFunctions::SendFinishedEntitiesList(client);
	initial_spawn_threads_active--;
}

vector<Entity*> ZoneServer::GetPlayers(){
	vector<Entity*> ret;
	Client* client = 0;
	vector<Client*>::iterator client_itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		ret.push_back(client->GetPlayer());
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}

Player*	ZoneServer::GetPlayerByID(int32 id) {
	Player* ret = nullptr;
	Client* client = nullptr;

	MutexList<Client*>::iterator client_itr = connected_clients.begin();

	while (client_itr.Next()) {
		client = client_itr->value;

		if (client && client->GetPlayer() && client->GetPlayer()->GetCharacterID() == id) {
			ret = client->GetPlayer();
			break;
		}
	}

	return ret;
}

int16 ZoneServer::SetSpawnTargetable(Spawn* spawn, float distance){
	Spawn* test_spawn = 0;
	int16 ret_val = 0;
	map<int32, Spawn*>::iterator itr;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
		test_spawn = itr->second;
		if(test_spawn){
			if(test_spawn->GetDistance(spawn) <= distance){
				test_spawn->SetTargetable(1);
				ret_val++;
			}
		}
	}
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
	return ret_val;
}

int16 ZoneServer::SetSpawnTargetable(int32 spawn_id){
	Spawn* spawn = 0;
	int16 ret_val = 0;
	map<int32, Spawn*>::iterator itr;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
		spawn = itr->second;
		if(spawn){
			if(spawn->GetDatabaseID() == spawn_id){
				spawn->SetTargetable(1);
				ret_val++;
			}
		}
	}
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
	return ret_val;
}

EQ2Packet* ZoneServer::GetZoneInfoPacket(Client* client){
	client_spawn_map.Put(client->GetPlayer(), client);
	PacketStruct* packet = configReader.getStruct("WS_ZoneInfo", client->GetVersion());
	packet->setSmallStringByName("server1",net.GetWorldName());
	packet->setSmallStringByName("server2",net.GetWorldName());
	packet->setSmallStringByName("auction_website", "eq2emulator.net");
	packet->setDataByName("auction_port", 80);
	packet->setSmallStringByName("upload_page", "test_upload.m");
	packet->setSmallStringByName("upload_key", "dsya987yda9");
	packet->setSmallStringByName("zone", GetZoneFile());
	packet->setSmallStringByName("zone2", GetZoneName());
	packet->setSmallStringByName("zone_desc", GetZoneDescription());
	packet->setSmallStringByName("char_name", client->GetPlayer()->GetName());
	//packet->setDataByName("unknown1", 1, 1);
	int32 expansions = EXPANSION_UNKNOWN + EXPANSION_DOF + EXPANSION_KOS + EXPANSION_EOF + EXPANSION_ROK + EXPANSION_TSO + EXPANSION_DOV;
	packet->setDataByName("expansions_enabled", expansions);
	
	packet->setDataByName("x", client->GetPlayer()->GetX());
	packet->setDataByName("y", client->GetPlayer()->GetY());
	packet->setDataByName("z", client->GetPlayer()->GetZ());
	packet->setDataByName("unknown1", 1, 1);
	// unknown3 can prevent screen shots from being taken if
	packet->setDataByName("unknown3", 2094661567, 1);			// Screenshots allowed with this value
	//packet->setDataByName("unknown3", 3815767999, 1);			// Screenshots disabled with this value
	packet->setDataByName("unknown3", 1, 2);

	if (client->GetVersion() <= 63181){
		packet->setDataByName("unknown3", 872447025, 0);//63181 
		packet->setDataByName("unknown3", 3085434875, 1);// 63181 
		packet->setDataByName("unknown3", 2147483633, 2);// 63181 
	}
	else{
		packet->setDataByName("unknown3a", 73821356);//63182 
		packet->setDataByName("unknown3b", 3991404383);// 63182
		packet->setDataByName("unknown3c", 4278189967);// 63182
		packet->setDataByName("unknown2a", 8);// 63182
		packet->setDataByName("unknown2b", 8);// 63182
	}

	//packet->setDataByName("unknown5", 100798094);
	packet->setDataByName("unknown5", 50859661);
	packet->setDataByName("unknown6", 19);
	packet->setDataByName("unknown6", 58, 1);
	packet->setDataByName("unknown6", 6, 2);
	packet->setDataByName("unknown7", 1);
	packet->setDataByName("unknown7", 1, 1);
	packet->setDataByName("unknown9", 13);
	//packet->setDataByName("unknown10", 25188959);
	packet->setDataByName("unknown10", 25190239);
	packet->setDataByName("unknown12", 0xFFFFFFFF);

	LogWrite(MISC__TODO, 0, "TODO", "Put cl_ client commands back in variables (not Rules) so they can be dynamically maintained");
	vector<Variable*>* variables = world.GetClientVariables();
	packet->setArrayLengthByName("num_client_setup", variables->size());
	for(int i=variables->size()-1;i>=0;i--)
		packet->setArrayDataByName("client_cmds", variables->at(i)->GetNameValuePair().c_str(), i);
	safe_delete(variables);
	//packet->setDataByName("unknown8", ); story?
	// AA Tabs for 1193+ clients
	if (client->GetVersion() >= 1193) {
		packet->setArrayLengthByName("tab_count", 48);
		int8 i = 0;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":es24a58bd8fcaac8c2:All", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c727bd47a6:Racial Innate", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c75a96e23c:Tradeskill Advancement", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c744f1fd99:Focus Effects", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c71edd2a66:Heroic", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c76ee6239f:Shadows", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7e678b977:Prestige", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c77ee422d7:Animalist", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7f165af77:Bard", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7421b9375:Brawler", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7a03ae7d1:Cleric", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7c9605e9f:Crusader", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7f9424168:Druid", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c79cb9556c:Enchanter", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c70c8b6aa4:Predator", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c73a43b6dd:Rogue", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c759fe7d15:Sorcerer", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7ad610aca:Summoner", i);
		
		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c71e056728:Warrior", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7ba864c0b:Assassin", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7b8116aad:Beastlord", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7f53feb7b:Berserker", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c73d8a70e2:Brigand", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c770c766d6:Bruiser", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c79226984b:Coercer", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c70c58bb30:Conjurer", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c73dfe68d0:Defiler", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c792919a6b:Dirge", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7062e5f55:Fury", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c762c1fdfc:Guardian", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c78addfbf4:Illusionist", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7ece054a7:Inquisitor", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7d550d2e7:Monk", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c743cfeaa2:Mystic", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7f63c9c8c:Necromancer", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c70c5de0ae:Paladin", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c79bc97b3a:Ranger", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c78fbd2256:Shadowknight", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7781cc625:Shaman", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c77eecdcdb:Swashbuckler", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7648d181e:Templar", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c78df47d77:Troubador", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7c78ce0b8:Warden", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c76290dcfa:Warlock", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7d1d52cf5:Wizard", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c71c8f6f4d:Shaper", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c72f6e354d:Channeler", i);

		i++;
		packet->setArrayDataByName("tab_index", i, i);
		packet->setArrayDataByName("tab_name", ":410385c7df8bd37d:Dragon", i);
		}

	EQ2Packet* outapp = packet->serialize();
	//packet->PrintPacket();
	//DumpPacket(outapp);
	safe_delete(packet);
	return outapp;
}

void ZoneServer::SendUpdateDefaultCommand(Spawn* spawn, const char* command, float distance){
	Client* client = 0;
	PacketStruct* packet = 0;
	vector<Client*>::iterator client_itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		if(client && client->GetPlayer()->WasSentSpawn(spawn->GetID()) && client->GetPlayer()->WasSpawnRemoved(spawn) == false){
			packet = configReader.getStruct("WS_SetDefaultCommand", client->GetVersion());
			if(packet){
				packet->setDataByName("spawn_id", client->GetPlayer()->GetIDWithPlayerSpawn(spawn));
				packet->setMediumStringByName("command_name", command);
				packet->setDataByName("distance", distance);
				client->QueuePacket(packet->serialize());
				safe_delete(packet);
			}
		}
		spawn->SetPrimaryCommand(command, command, distance);
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::CheckPlayerProximity(Spawn* spawn, Client* client){
	if(player_proximities.count(spawn->GetID()) > 0){
		PlayerProximity* prox = player_proximities.Get(spawn->GetID());
		if(prox->clients_in_proximity.count(client) == 0 && spawn_range_map.count(client) > 0 && spawn_range_map.Get(client)->count(spawn->GetID()) > 0 && spawn_range_map.Get(client)->Get(spawn->GetID()) < prox->distance){
			prox->clients_in_proximity[client] = true;
			CallSpawnScript(spawn, SPAWN_SCRIPT_CUSTOM, client->GetPlayer(), prox->in_range_lua_function.c_str());
		}
		else if(prox->clients_in_proximity.count(client) > 0 && spawn_range_map.count(client) > 0 && spawn_range_map.Get(client)->count(spawn->GetID()) > 0 && spawn_range_map.Get(client)->Get(spawn->GetID()) > prox->distance){
			if(prox->leaving_range_lua_function.length() > 0)
				CallSpawnScript(spawn, SPAWN_SCRIPT_CUSTOM, client->GetPlayer(), prox->leaving_range_lua_function.c_str());
			prox->clients_in_proximity.erase(client);
		}
	}
}

void ZoneServer::AddPlayerProximity(Spawn* spawn, float distance, string in_range_function, string leaving_range_function){
	RemovePlayerProximity(spawn);
	PlayerProximity* prox = new PlayerProximity;
	prox->distance = distance;
	prox->in_range_lua_function = in_range_function;
	prox->leaving_range_lua_function = leaving_range_function;
	player_proximities.Put(spawn->GetID(), prox);
}

void ZoneServer::RemovePlayerProximity(Client* client){	
	PlayerProximity* prox = 0;
	MutexMap<int32, PlayerProximity*>::iterator itr = player_proximities.begin();
	while(itr.Next()){
		prox = itr->second;
		if(prox->clients_in_proximity.count(client) > 0)
			prox->clients_in_proximity.erase(client);
	}
}

void ZoneServer::RemovePlayerProximity(Spawn* spawn, bool all){
	if(all){
		MutexMap<int32, PlayerProximity*>::iterator itr = player_proximities.begin();
		while(itr.Next()){
			player_proximities.erase(itr->first, false, true, 10000);
		}
	}
	else if(player_proximities.count(spawn->GetID()) > 0){
		player_proximities.erase(spawn->GetID(), false, true, 10000);
	}
}

void ZoneServer::AddLocationProximity(float x, float y, float z, float max_variation, string in_range_function, string leaving_range_function) {
	LocationProximity* prox = new LocationProximity;
	prox->x = x;
	prox->y = y;
	prox->z = z;
	prox->max_variation = max_variation;
	prox->in_range_lua_function = in_range_function;
	prox->leaving_range_lua_function = leaving_range_function;
	location_proximities.Add(prox);
}

void ZoneServer::CheckLocationProximity() {
	const char* zone_script = world.GetZoneScript(this->GetZoneID());
	if (!zone_script)
		return;

	if (location_proximities.size() > 0 && connected_clients.size() > 0) {
		Client* client = 0;
		MutexList<Client*>::iterator iterator = connected_clients.begin();
		while(iterator.Next()){
			client = iterator->value;
			if (client->IsConnected() && client->IsReadyForSpawns() && !client->IsZoning()) {
				try {
					MutexList<LocationProximity*>::iterator itr = location_proximities.begin();
					LocationProximity* prox = 0;
					while(itr.Next()){
						prox = itr->value;
						bool in_range = false;
						float char_x = client->GetPlayer()->GetX();
						float char_y = client->GetPlayer()->GetY();
						float char_z = client->GetPlayer()->GetZ();
						float x = prox->x;
						float y = prox->y;
						float z = prox->z;
						float max_variation = prox->max_variation;
						float total_diff = 0;
						float diff = x - char_x; //Check X
						if(diff < 0)
							diff *= -1;
						if(diff <=  max_variation) {
							total_diff += diff;
							diff = z - char_z; //Check Z (we check Z first because it is far more likely to be a much greater variation than y)
							if(diff < 0)
								diff *= -1;
							if(diff <=  max_variation) {
								total_diff += diff;
								if(total_diff <=  max_variation) { //Check Total
									diff = y - char_y; //Check Y
									if(diff < 0)
										diff *= -1;
									if(diff <=  max_variation) {
										total_diff += diff;
										if(total_diff <= max_variation) {
											in_range = true;
											if(lua_interface && prox->in_range_lua_function.length() > 0 && prox->clients_in_proximity.count(client) == 0) { //Check Total
												prox->clients_in_proximity[client] = true;
												lua_interface->RunZoneScript(zone_script, prox->in_range_lua_function.c_str(), this, client->GetPlayer());
											}
										}
									}
								}
							}
						}
						if (!in_range) {
							if(lua_interface && prox->leaving_range_lua_function.length() > 0 && prox->clients_in_proximity.count(client) > 0) {
								lua_interface->RunZoneScript(zone_script, prox->leaving_range_lua_function.c_str(), this, client->GetPlayer());
								prox->clients_in_proximity.erase(client);
							}
						}
					}
				}
				catch (...) {
					LogWrite(ZONE__ERROR, 0, "Zone", "Except caught in ZoneServer::CheckLocationProximity");
					return;
				}
			}
		}
	}
}

void ZoneServer::CheckLocationGrids() {
	if (connected_clients.size() > 0 && location_grids.size() > 0) {
		MutexList<Client*>::iterator client_itr = connected_clients.begin();
		while (client_itr.Next()) {
			Client* client = client_itr.value;
			if (!client)
				continue;
			Player* player = client->GetPlayer();
			float x = player->GetX();
			float y = player->GetY();
			float z = player->GetZ();
			int32 grid_id = player->appearance.pos.grid_id;
			MutexList<LocationGrid*>::iterator location_grid_itr = location_grids.begin();
			while (location_grid_itr.Next()) {
				LocationGrid* grid = location_grid_itr.value;
				if (grid->locations.size() > 0 || /*(grid->grid_id == grid_id ||*/ grid->players.count(player) > 0) {
					float x_small = 0;
					float x_large = 0;
					float y_small = 0;
					float y_large = 0;
					float z_small = 0;
					float z_large = 0;
					bool first = true;
					bool in_grid = false;
					MutexList<Location*>::iterator location_itr = grid->locations.begin();
					while (location_itr.Next()) {
						Location* location = location_itr.value;
						if (first) {
							x_small = location->x;
							x_large = location->x;
							if (grid->include_y) {
								y_small = location->y;
								y_large = location->y;
							}
							z_small = location->z;
							z_large = location->z;
							first = false;
						}
						else {
							if (location->x < x_small)
								x_small = location->x;
							else if (location->x > x_large)
								x_large = location->x;
							if (grid->include_y) {
								if (location->y < y_small)
									y_small = location->y;
								else if (location->y > y_large)
									y_large = location->y;
							}
							if (location->z < z_small)
								z_small = location->z;
							else if (location->z > z_large)
								z_large = location->z;
						}
					}
					if (grid->include_y && (x >= x_small && x <= x_large && y >= y_small && y <= y_large && z >= z_small && z <= z_large))
						in_grid = true;
					else if (x >= x_small && x <= x_large && z >= z_small && z <= z_large)
						in_grid = true;
					if (in_grid && grid->players.count(player) == 0) {
						grid->players.Put(player, true);

						bool show_enter_location_popup = true;
						bool discovery_enabled = rule_manager.GetGlobalRule(R_World, EnablePOIDiscovery)->GetBool();

						if( grid->discovery && discovery_enabled && !player->DiscoveredLocation(grid->id) )
						{
							// check if player has already discovered this location

							// if not, process new discovery
							char tmp[200] = {0};
							sprintf(tmp, "\\#FFE400You have discovered\12\\#FFF283%s", grid->name.c_str());
							client->SendPopupMessage(11, tmp, "ui_discovery", 2.25, 0xFF, 0xFF, 0xFF);
							LogWrite(ZONE__DEBUG, 0, "Zone", "Player '%s' discovered location '%s' (%u)", player->GetName(), grid->name.c_str(), grid->id);

							player->UpdatePlayerHistory(HISTORY_TYPE_DISCOVERY, HISTORY_SUBTYPE_LOCATION, grid->id);
							show_enter_location_popup = false;

							// else, print standard location entry
						}

						if( show_enter_location_popup )
						{
							LogWrite(ZONE__DEBUG, 0, "Zone", "Player '%s' entering location '%s' (%u)", player->GetName(), grid->name.c_str(), grid->id);
							client->SendPopupMessage(10, grid->name.c_str(), 0, 2.5, 255, 255, 0);
						}
					}
					else if (!in_grid && grid->players.count(player) > 0) {
						LogWrite(ZONE__DEBUG, 0, "Zone", "Player '%s' leaving location '%s' (%u)", player->GetName(), grid->name.c_str(), grid->id);
						grid->players.erase(player);
					}
				}
			}
		}
	}
}

// Called from a command (client, main zone thread) and the main zone thread
// so no need for a mutex container
void ZoneServer::AddLocationGrid(LocationGrid* grid) {
	if (grid)
		location_grids.Add(grid);
}

void ZoneServer::RemoveLocationGrids() {
	MutexList<LocationGrid*>::iterator itr = location_grids.begin();
	while (itr.Next())
		itr.value->locations.clear(true);
	location_grids.clear(true);
}

void ZoneServer::RemoveSpellTimersFromSpawn(Spawn* spawn, bool remove_all, bool delete_recast){
	if(spellProcess)
		spellProcess->RemoveSpellTimersFromSpawn(spawn, remove_all, delete_recast);
}

void ZoneServer::Interrupted(Entity* caster, Spawn* interruptor, int16 error_code, bool cancel, bool from_movement){
	if(spellProcess)
		spellProcess->Interrupted(caster, interruptor, error_code, cancel, from_movement);
}

Spell* ZoneServer::GetSpell(Entity* caster){
	Spell* spell = 0;
	if(spellProcess)
		spell = spellProcess->GetSpell(caster);
	return spell;
}

void ZoneServer::ProcessSpell(Spell* spell, Entity* caster, Spawn* target, bool lock, bool harvest_spell){
	if(spellProcess)
		spellProcess->ProcessSpell(this, spell, caster, target, lock, harvest_spell);
}

void ZoneServer::ProcessEntityCommand(EntityCommand* entity_command, Entity* caster, Spawn* target, bool lock) {
	if (spellProcess)
		spellProcess->ProcessEntityCommand(this, entity_command, caster, target, lock);
}

void ZoneServer::RemoveSpawnSupportFunctions(Spawn* spawn) {
	if(!spawn)
		return;	

	LogWrite(ZONE__DEBUG, 7, "Zone", "Processing RemoveSpawnSupportFunctions...");

	if(spawn->IsEntity())
		RemoveSpellTimersFromSpawn((Entity*)spawn, true);

	RemoveDamagedSpawn(spawn);
	spawn->SendSpawnChanges(false);
	RemoveChangedSpawn(spawn);
	
	// Everything inside this if will be nuked during a reload in other spots, no need to do it twice
	if (!reloading) {
		RemoveDeadEnemyList(spawn);

		spawn->changed = true;
		spawn->info_changed = true;
		spawn->vis_changed = true;
		spawn->position_changed = true;
		SendSpawnChanges(spawn);

		if (spawn->GetSpawnGroupID() > 0) {
			int32 group_id = spawn->GetSpawnGroupID();
			spawn->RemoveSpawnFromGroup();
			if (spawn_group_map.count(group_id) > 0)
				spawn_group_map.Get(group_id).Remove(spawn->GetID());
		}

		if (!spawn->IsPlayer()) {
			if(quick_database_id_lookup.count(spawn->GetDatabaseID()) > 0)
				quick_database_id_lookup.erase(spawn->GetDatabaseID());
		}

		RemoveHeadingTimer(spawn);
		DeleteSpawnScriptTimers(spawn);
		RemovePlayerProximity(spawn);
	}

	// We don't use RemoveMovementNPC() here as it caused a hell of a delay during reloads
	// instead we remove it from the list directly
	if (spawn->IsNPC())
		movement_spawns.erase(spawn->GetID());
}

void ZoneServer::HandleEmote(Client* originator, string name) {
	if (!originator) {
		LogWrite(ZONE__ERROR, 0, "Zone", "HandleEmote called with an invalid client");
		return;
	}

	Client* client = 0;
	Emote* emote = visual_states.FindEmote(name);
	if(!emote){
		originator->Message(CHANNEL_COLOR_YELLOW, "Unable to find emote '%s'.  If this should be a valid emote be sure to submit a /bug report.", name.c_str());
		return;
	}
	PacketStruct* packet = 0;
	char* emoteResponse = 0;
	vector<Client*>::iterator client_itr;
	
	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		if(!client || client->GetPlayer()->IsIgnored(originator->GetPlayer()->GetName()))
			continue;
		packet = configReader.getStruct("WS_CannedEmote", client->GetVersion());
		if(packet){
			packet->setDataByName("spawn_id" , client->GetPlayer()->GetIDWithPlayerSpawn(originator->GetPlayer()));
			if(!emoteResponse){
				string message;
				if(originator->GetPlayer()->GetTarget() && originator->GetPlayer()->GetTarget()->GetID() != originator->GetPlayer()->GetID()){
					message = emote->GetTargetedMessageString();
					if(message.find("%t") < 0xFFFFFFFF)
						message.replace(message.find("%t"), 2, originator->GetPlayer()->GetTarget()->GetName());
				}
				if(message.length() == 0)
					message = emote->GetMessageString();
				if(message.find("%g1") < 0xFFFFFFFF){
					if(originator->GetPlayer()->GetGender() == 1)
						message.replace(message.find("%g1"), 3, "his");
					else
						message.replace(message.find("%g1"), 3, "her");
				}
				if(message.find("%g2") < 0xFFFFFFFF){
					if(originator->GetPlayer()->GetGender() == 1)
						message.replace(message.find("%g2"), 3, "him");
					else
						message.replace(message.find("%g2"), 3, "her");
				}
				if(message.find("%g3") < 0xFFFFFFFF){
					if(originator->GetPlayer()->GetGender() == 1)
						message.replace(message.find("%g3"), 3, "he");
					else
						message.replace(message.find("%g3"), 3, "she");
				}
				if(message.length() > 0){
					emoteResponse = new char[message.length() + strlen(originator->GetPlayer()->GetName()) + 10];
					sprintf(emoteResponse,"%s %s", originator->GetPlayer()->GetName(), message.c_str());
				}
				else{
					originator->Message(CHANNEL_COLOR_YELLOW, "%s is not properly configured, be sure to submit a /bug report.", name.c_str());
					safe_delete(packet);
					break;
				}
			}
			packet->setMediumStringByName("emote_msg", emoteResponse);
			packet->setDataByName("anim_type", emote->GetVisualState());
			client->QueuePacket(packet->serialize());
			safe_delete(packet);
			safe_delete_array(emoteResponse);
		}	
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}


void ZoneServer::SetupInstance(int32 createdInstanceID) {
	if ( createdInstanceID == 0 ) // if this happens that isn't good!
		instanceID = ++MinInstanceID;
	else // db should pass the good ID
		instanceID = createdInstanceID;
}

void ZoneServer::RemoveDeadSpawn(Spawn* spawn){
	AddDeadSpawn(spawn, 0);
}

void ZoneServer::AddDeadSpawn(Spawn* spawn, int32 timer){
	MDeadSpawns.writelock(__FUNCTION__, __LINE__);
	if (dead_spawns.count(spawn->GetID()) > 0)
		dead_spawns[spawn->GetID()] = Timer::GetCurrentTime2() + timer;
	else if(timer != 0xFFFFFFFF)
		dead_spawns.insert(make_pair(spawn->GetID(), Timer::GetCurrentTime2() + timer));
	else{
		if(spawn->IsEntity() && ((Entity*)spawn)->HasLoot()){
			dead_spawns.insert(make_pair(spawn->GetID(), Timer::GetCurrentTime2() + (15000 * spawn->GetLevel() + 240000)));
			SendUpdateDefaultCommand(spawn, "loot", 10);
		}
		else
			dead_spawns.insert(make_pair(spawn->GetID(), Timer::GetCurrentTime2() + 10000));
	}
	MDeadSpawns.releasewritelock(__FUNCTION__, __LINE__);
}

void ZoneServer::WritePlayerStatistics() {
	MutexList<Client*>::iterator client_itr = connected_clients.begin();
	while(client_itr.Next())
		client_itr->value->GetPlayer()->WritePlayerStatistics();
}

void ZoneServer::AddPlayerTracking(Player* player) {
	if (player && !player->GetIsTracking() && players_tracking.count(player->GetDatabaseID()) == 0) {
		Client* client = GetClientBySpawn(player);
		if (client) {
			PacketStruct* packet = configReader.getStruct("WS_TrackingUpdate", client->GetVersion());
			if (packet) {
				player->SetIsTracking(true);
				players_tracking.Put(client->GetCharacterID(), player);
				packet->setDataByName("mode", TRACKING_START);
				packet->setDataByName("type", TRACKING_TYPE_ENTITIES);
				client->QueuePacket(packet->serialize());
				safe_delete(packet);
			}
		}
	}
}

void ZoneServer::RemovePlayerTracking(Player* player, int8 mode) {
	if (player && player->GetIsTracking()) {
		Client* client = GetClientBySpawn(player);
		if (client) {
			PacketStruct* packet = configReader.getStruct("WS_TrackingUpdate", client->GetVersion());
			if (packet) {
				player->SetIsTracking(false);
				players_tracking.erase(client->GetCharacterID());
				packet->setDataByName("mode", mode);
				packet->setDataByName("type", TRACKING_TYPE_ENTITIES);
				client->QueuePacket(packet->serialize());
				safe_delete(packet);
			}
		}
	}
}

void ZoneServer::ProcessTracking() {
	MutexMap<int32, Player*>::iterator itr = players_tracking.begin();
	while (itr.Next())
		ProcessTracking(GetClientBySpawn(itr->second));
}

void ZoneServer::ProcessTracking(Client* client) {
	if (!client)
		return;

	Player* player = client->GetPlayer();
	if (player && player->GetIsTracking()) {
		MutexMap<int32, Spawn*>::iterator spawn_itr;
		PacketStruct* packet = configReader.getStruct("WS_TrackingUpdate", client->GetVersion());
		if (packet) {
			packet->setDataByName("mode", TRACKING_UPDATE);
			packet->setDataByName("type", TRACKING_TYPE_ENTITIES);
			vector<TrackedSpawn*> spawns_tracked;
			while (spawn_itr.Next()) {
				Spawn* spawn = spawn_itr->second;
				float distance = player->GetDistance(spawn);
				if (spawn->IsEntity() && distance <= 80 && spawn != player) {
					TrackedSpawn* ts = new TrackedSpawn;
					ts->spawn = spawn;
					ts->distance = distance;

					/* Add spawns in ascending order from closest to furthest */
					if (spawns_tracked.empty())
						spawns_tracked.push_back(ts);
					else {
						vector<TrackedSpawn*>::iterator tracked_itr;
						bool added = false;
						for (tracked_itr = spawns_tracked.begin(); tracked_itr != spawns_tracked.end(); tracked_itr++) {
							TrackedSpawn* cur_ts = *tracked_itr;
							if (ts->distance <= cur_ts->distance) {
								spawns_tracked.insert(tracked_itr, ts);
								added = true;
								break;
							}
						}
						if (!added)
							spawns_tracked.push_back(ts);
					}
				}
			}
			packet->setArrayLengthByName("num_spawns", spawns_tracked.size());
			for (int32 i = 0; i < spawns_tracked.size(); i++) {
				TrackedSpawn* ts = spawns_tracked[i];

				LogWrite(ZONE__DEBUG, 0, "Zone", "%s (%f)", ts->spawn->GetName(), ts->distance);

				packet->setArrayDataByName("spawn_id", player->GetIDWithPlayerSpawn(ts->spawn), i);
				packet->setArrayDataByName("spawn_name", ts->spawn->GetName(), i);
				if (ts->spawn->IsPlayer())
					packet->setArrayDataByName("spawn_type", TRACKING_SPAWN_TYPE_PC, i);
				else
					packet->setArrayDataByName("spawn_type", TRACKING_SPAWN_TYPE_NPC, i);
				packet->setArrayDataByName("spawn_con_color", player->GetArrowColor(ts->spawn->GetLevel()), i);
			}
			packet->setArrayLengthByName("num_array1", 0);
			//for (int32 i = 0; i < spawns_tracked.size(); i++) {
			//}
			packet->setArrayLengthByName("num_spawns2", spawns_tracked.size());
			for (int32 i = 0; i < spawns_tracked.size(); i++) {
				TrackedSpawn* ts = spawns_tracked[i];
				packet->setArrayDataByName("list_spawn_id", player->GetIDWithPlayerSpawn(ts->spawn), i);
				packet->setArrayDataByName("list_number", i, i);
			}
			client->QueuePacket(packet->serialize());
			safe_delete(packet);
			for (int32 i = 0; i < spawns_tracked.size(); i++)
				safe_delete(spawns_tracked[i]);
		}
	}
}

void ZoneServer::SendEpicMobDeathToGuild(Player* killer, Spawn* victim) {
	if (killer && victim) {

		LogWrite(MISC__TODO, 1, "TODO" , "Check if player is in raid\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);

		if (killer->GetGroupMemberInfo()) {
			world.GetGroupManager()->GroupLock(__FUNCTION__, __LINE__);

			deque<GroupMemberInfo*>::iterator itr;
			deque<GroupMemberInfo*>* members = world.GetGroupManager()->GetGroupMembers(killer->GetGroupMemberInfo()->group_id);
			for (itr = members->begin(); itr != members->end(); itr++) {
				GroupMemberInfo* gmi = *itr;
				if (gmi->client) {
					Player* group_member = gmi->client->GetPlayer();
					if (group_member->GetGuild()) {
						Guild* guild = group_member->GetGuild();
						string message = Guild::GetEpicMobDeathMessage(group_member->GetName(), victim->GetName());
						guild->AddNewGuildEvent(GUILD_EVENT_KILLS_EPIC_MONSTER, message.c_str(), Timer::GetUnixTimeStamp());
						guild->SendMessageToGuild(GUILD_EVENT_KILLS_EPIC_MONSTER, message.c_str());
					}
				}
			}

			world.GetGroupManager()->ReleaseGroupLock(__FUNCTION__, __LINE__);
		}
		else if (killer->GetGuild()) {
			Guild* guild = killer->GetGuild();
			string message = Guild::GetEpicMobDeathMessage(killer->GetName(), victim->GetName());
			guild->AddNewGuildEvent(GUILD_EVENT_KILLS_EPIC_MONSTER, message.c_str(), Timer::GetUnixTimeStamp());
			guild->SendMessageToGuild(GUILD_EVENT_KILLS_EPIC_MONSTER, message.c_str());
		}
	}
}

void ZoneServer::ProcessAggroChecks(Spawn* spawn) {
	// If faction based combat is not allowed then no need to run the loops so just return out
	if(!rule_manager.GetGlobalRule(R_Faction, AllowFactionBasedCombat)->GetBool())
		return;

	if (spawn && spawn->IsNPC() && spawn->Alive())
		CheckEnemyList((NPC*)spawn);
}

void ZoneServer::SendUpdateTitles(Client *client, Title *suffix, Title *prefix) {
	assert(client);
	SendUpdateTitles(client->GetPlayer(), suffix, prefix);
}

void ZoneServer::SendUpdateTitles(Spawn *spawn, Title *suffix, Title *prefix) {
	if (!spawn)
		return;

	vector<Client*>::iterator itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (itr = clients.begin(); itr != clients.end(); ++itr) {
		Client* current_client = *itr;
		PacketStruct *packet;

		if (!(packet = configReader.getStruct("WS_UpdateTitle", current_client->GetVersion())))
			break;

		packet->setDataByName("player_id", current_client->GetPlayer()->GetIDWithPlayerSpawn(spawn));
		packet->setDataByName("player_name", spawn->GetName());
		packet->setDataByName("unknown1", 1, 1);
		if(suffix)
			packet->setDataByName("suffix_title", suffix->GetName());
		else
			packet->setDataByName("suffix_title", spawn->GetSuffixTitle());

		string prefix_title;
		if (prefix)
			prefix_title = prefix->GetName();
		else
			prefix_title = spawn->GetPrefixTitle();
		if (spawn->IsPlayer()) {
			string pvp_title = PVP::GetRank(static_cast<Player*>(spawn));
			if (pvp_title.length() > 0) {
				if (prefix_title.length() > 0)
					prefix_title = pvp_title + " " + prefix_title;
				else
					prefix_title = pvp_title;
			}
		}
		packet->setDataByName("prefix_title", prefix_title.c_str());
		packet->setDataByName("last_name", spawn->GetLastName());
		packet->setDataByName("sub_title", spawn->GetSubTitle());
		current_client->QueuePacket(packet->serialize());
		safe_delete(packet);
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::AddTransportSpawn(Spawn* spawn){
	if(!spawn)
		return;
	MTransportSpawns.writelock(__FUNCTION__, __LINE__);
	transport_spawns.push_back(spawn->GetID());
	MTransportSpawns.releasewritelock(__FUNCTION__, __LINE__);
}

Spawn* ZoneServer::GetClosestTransportSpawn(float x, float y, float z){
	Spawn* spawn = 0;
	Spawn* closest_spawn = 0;
	float closest_distance = 0.0;
	MTransportSpawns.writelock(__FUNCTION__, __LINE__);
	vector<int32>::iterator itr = transport_spawns.begin();
	while(itr != transport_spawns.end()){
		spawn = GetSpawnByID(*itr);
		if(spawn){
			if(closest_distance == 0.0){
				closest_spawn = spawn;
				closest_distance = spawn->GetDistance(x, y, z);
			}
			else if(spawn->GetDistance(x, y, z) < closest_distance){
				closest_spawn = spawn;
				closest_distance = spawn->GetDistance(x, y, z);
			}
			itr++;
		}
		else
			itr = transport_spawns.erase(itr);
	}
	MTransportSpawns.releasewritelock(__FUNCTION__, __LINE__);

	return closest_spawn;
}

void ZoneServer::SetRain(float val) {
	rain = val;
	vector<Client*>::iterator itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (itr = clients.begin(); itr != clients.end(); itr++) {
		Client* client = *itr;
		client->GetPlayer()->GetInfoStruct()->rain = val;
		client->GetPlayer()->SetCharSheetChanged(true);
		if( val >= 0.75 && !weather_signaled )
		{
			weather_signaled = true;
			client->SimpleMessage(CHANNEL_COLOR_WHITE, "It starts to rain.");
		}
		else if( val < 0.75 && weather_signaled ) 
		{
			weather_signaled = false;
			client->SimpleMessage(CHANNEL_COLOR_WHITE, "It stops raining.");
		}
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::SetWind(float val) {
	vector<Client*>::iterator itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (itr = clients.begin(); itr != clients.end(); itr++) {
		Client* client = *itr;
		client->GetPlayer()->GetInfoStruct()->wind = val;
		client->GetPlayer()->SetCharSheetChanged(true);
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::ProcessWeather()
{
	// if the global rule to disable weather is set, or if the `weather_allowed` field in the zone record == 0, do not process weather
	if( !weather_enabled || !isWeatherAllowed() )
		return;

	LogWrite(ZONE__DEBUG, 1, "Zone", "%s: Processing weather changes", zone_name);
	float new_weather = 0;
	float weather_offset = 0;
	bool change_weather = false;

	// check to see if it is time to change the weather according to weather_frequency (time between changes)
	if( weather_last_changed_time <= (Timer::GetUnixTimeStamp() - weather_frequency) )
	{
		LogWrite(ZONE__DEBUG, 2, "Zone", "%s: Checking for weather changes", zone_name);
		// reset last changed time (frequency check)
		weather_last_changed_time = Timer::GetUnixTimeStamp();

		// this is the chance a weather change occurs at all at the expired interval
		int8 weather_random = MakeRandomInt(1, 100);
		LogWrite(ZONE__DEBUG, 2, "Zone", "%s: Chance to change weather: %i%%, rolled: %i%% - Change weather: %s", zone_name, weather_change_chance, weather_random, weather_random <= weather_change_chance ? "True" : "False");

		if( weather_random <= weather_change_chance )
		{
			change_weather = true;
			weather_offset = weather_change_amount;

			if( weather_type == 3 ) // chaotic weather patterns, random weather between min/max
			{
				new_weather = MakeRandomFloat(weather_min_severity, weather_max_severity);
				LogWrite(ZONE__DEBUG, 3, "Zone", "%s: Chaotic weather severity changed to %.2f", zone_name, new_weather);
				weather_pattern = 2;
			}
			else if( weather_type == 2 ) // random weather patterns, combination of normal + dynamic + max_offset
			{
				weather_offset = MakeRandomFloat(weather_change_amount, weather_dynamic_offset);
				LogWrite(ZONE__DEBUG, 3, "Zone", "%s: Random weather severity changed by %.2f", zone_name, weather_offset);

				int8 weather_alter = weather_change_chance / 10;	// the divide is to prevent too many direction changes in a cycle
				weather_random = MakeRandomInt(1, 100);				// chance that the weather changes direction (weather_pattern)

				if( weather_random <= weather_alter )
					weather_pattern = ( weather_pattern == 0 ) ? 1 : 0;
			}
			else if( weather_type == 1 ) // dynamic weather patterns, weather may not reach min/max
			{
				int8 weather_alter = weather_change_chance / 10;	// the divide is to prevent too many direction changes in a cycle
				weather_random = MakeRandomInt(1, 100);				// chance that the weather changes direction (weather_pattern)

				if( weather_random <= weather_alter )
				{
					weather_pattern = ( weather_pattern == 0 ) ? 1 : 0;
					LogWrite(ZONE__DEBUG, 3, "Zone", "%s: Dynamic weather pattern changed to %i", zone_name, weather_pattern);
				}
			}
			else // normal weather patterns, weather starts at min, goes to max, then back down again
			{
				// do nothing (processed below)
				LogWrite(ZONE__DEBUG, 3, "Zone", "%s: Normal weather severity changed by %.2f", zone_name, weather_offset);
			}

			// when all done, change the weather
			if( change_weather )
			{
				if( weather_pattern == 1 )
				{
					// weather is getting worse, til it reaches weather_max_severity
					new_weather = ( weather_current_severity <= weather_max_severity ) ? weather_current_severity + weather_offset : weather_max_severity;
					LogWrite(ZONE__DEBUG, 3, "Zone", "%s: Increased weather severity by %.2f", zone_name, weather_offset);

					if(new_weather > weather_max_severity)
					{
 						new_weather = weather_max_severity - weather_offset;
						weather_pattern = 0;
					}
				}
				else if( weather_pattern == 0 )
				{
					// weather is clearing up, til it reaches weather_min_severity
					new_weather = ( weather_current_severity >= weather_min_severity ) ? weather_current_severity - weather_offset : weather_min_severity;
					LogWrite(ZONE__DEBUG, 3, "Zone", "%s: Decreased weather severity by %.2f", zone_name, weather_offset);

					if(new_weather < weather_min_severity)
					{
						new_weather = weather_min_severity + weather_offset;
						weather_pattern = 1;
					}
				}				

				LogWrite(ZONE__DEBUG, 1, "Zone", "%s: Weather change triggered from %.2f to %.2f", zone_name, weather_current_severity, new_weather);
				this->SetRain(new_weather);
				weather_current_severity = new_weather;
			}
		}
	}
	else
		LogWrite(ZONE__DEBUG, 1, "Zone", "%s: Not time to change weather yet", zone_name);
}

void ZoneServer::HidePrivateSpawn(Spawn* spawn) {
	if (!spawn->IsPrivateSpawn())
		return;

	Client* client = 0;	
	Player* player = 0;
	PacketStruct* packet = 0;
	MutexList<Client*>::iterator itr = connected_clients.begin();
	while (itr->Next()) {
		client = itr->value;
		player = client->GetPlayer();
		if (player->WasSentSpawn(spawn->GetID()) && !player->WasSpawnRemoved(spawn)) {
			packet = configReader.getStruct("WS_DestroyGhostCmd", client->GetVersion());
			SendRemoveSpawn(client, spawn, packet);
			if(spawn_range_map.count(client) > 0)
				spawn_range_map.Get(client)->erase(spawn->GetID());

			if(player->GetTarget() == spawn)
				player->SetTarget(0);
		}
	}
}

SpawnLocation* ZoneServer::GetSpawnLocation(int32 id) {
	SpawnLocation* ret = 0;
	MSpawnLocationList.readlock(__FUNCTION__, __LINE__);
	if (spawn_location_list.count(id) > 0)
		ret = spawn_location_list[id];
	MSpawnLocationList.releasereadlock(__FUNCTION__, __LINE__);
	return ret;
}

void ZoneServer::PlayAnimation(Spawn* spawn, int32 visual_state, Spawn* spawn2, int8 hide_type){
	Client* client = 0;
	PacketStruct* packet = 0;
	Spawn* exclude_spawn = 0;
	if (!spawn)
		return;
	if (spawn2){
		if(hide_type == 1){
			client = GetClientBySpawn(spawn2);
			if(client){
				packet = configReader.getStruct("WS_CannedEmote", client->GetVersion());
				packet->setDataByName("spawn_id", client->GetPlayer()->GetIDWithPlayerSpawn(spawn));
				packet->setDataByName("anim_type", visual_state);
				client->QueuePacket(packet->serialize());
			}
			return;
		}
		if(hide_type == 2)
			exclude_spawn = spawn2;
	}

	vector<Client*>::iterator client_itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++) {
		client = *client_itr;
		if(spawn->GetDistance(client->GetPlayer()) > 50)
			continue;
		if(exclude_spawn == client->GetPlayer())
			continue;

		packet = configReader.getStruct("WS_CannedEmote", client->GetVersion());
		if (packet) {
			packet->setDataByName("spawn_id", client->GetPlayer()->GetIDWithPlayerSpawn(spawn));
			packet->setDataByName("anim_type", visual_state);
			client->QueuePacket(packet->serialize());
			safe_delete(packet);
		}
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

vector<Spawn*> ZoneServer::GetSpawnsByID(int32 id) {
	vector<Spawn*> tmp_list;
	Spawn* spawn;

	map<int32, Spawn*>::iterator itr;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
		spawn = itr->second;
		if (spawn && (spawn->GetDatabaseID() == id))
			tmp_list.push_back(spawn);
	}
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);

	return tmp_list;
}

vector<Spawn*> ZoneServer::GetAttackableSpawnsByDistance(Spawn* caster, float distance) {
	vector<Spawn*> ret;
	Spawn* spawn = 0;
	map<int32, Spawn*>::iterator itr;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
		spawn = itr->second;

		if (!spawn || !spawn->Alive() || spawn == caster)
			continue;

		if (spawn->IsPet() && ((NPC*)spawn)->GetOwner() && ((NPC*)spawn)->GetOwner() == caster)
			continue;

		if (caster->IsPet() && ((NPC*)caster)->GetOwner() && ((NPC*)caster)->GetOwner() == ((NPC*)spawn))
			continue;

		if (((spawn->IsNPC() && spawn->appearance.attackable > 0) || (spawn->IsPlayer() && ((Player*)caster)->CanAttackTarget((Player*)spawn))) && spawn->GetDistance(caster, true) <= distance)
			ret.push_back(spawn);
	}
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
	return ret;
}

void ZoneServer::ResurrectSpawn(Spawn* spawn, Client* client) {
	if(!client || !spawn)
		return;
	PendingResurrection* rez = client->GetCurrentRez();
	if(!rez || !rez->caster)
		return;

	PacketStruct* packet = 0;
	float power_perc = rez->mp_perc;
	float health_perc = rez->hp_perc;
	Spawn* caster_spawn = rez->caster;
	sint32 heal_amt = 0;
	sint32 power_amt = 0;
	bool no_calcs = rez->no_calcs;
	int8 crit_mod = rez->crit_mod;
	Entity* caster = 0;
	InfoStruct* info = 0;
	bool crit = false;
	string heal_spell = rez->heal_name;
	int16 heal_packet_type = 0;
	int16 power_packet_type = 0;

	//Calculations for how much to heal the spawn
	if(health_perc > 0)
		heal_amt = (spawn->GetTotalHP() * (health_perc / 100));
	if(power_perc > 0)
		power_amt = (spawn->GetTotalPower() * (power_perc / 100));

	if(caster_spawn->IsEntity()){
		caster = ((Entity*)caster_spawn);
		info = caster->GetInfoStruct();
	}

	if(!no_calcs && caster){
		//Potency Mod
		heal_amt *=  ((caster->stats[ITEM_STAT_POTENCY] / 100) + 1);
		power_amt *= ((caster->stats[ITEM_STAT_POTENCY] / 100) + 1);

		//Ability Mod
		heal_amt += (int32)min((int32)info->ability_modifier, (int32)(heal_amt / 2));
		power_amt += (int32)min((int32)info->ability_modifier, (int32)(power_amt / 2));

		if(!crit_mod || crit_mod == 1){
			if(crit_mod == 1) 
				crit = true;
			else {
				// Crit Roll
				float chance = (float)max((float)0, (float)info->crit_chance);
				crit = (MakeRandomFloat(0, 100) <= chance); 
			}
			if(crit){
				//Apply total crit multiplier with crit bonus
				heal_amt *= ((info->crit_bonus / 100) + 1.3);
				power_amt *= ((info->crit_bonus / 100) + 1.3);
			}
		}
	}

	//Set this rez as a crit to be passed to subspell (not yet used)
	rez->crit = true;

	//Set Heal amt to 1 if 0 now so the player has health
	if(heal_amt == 0)
		heal_amt = 1;

	if(heal_amt > spawn->GetTotalHP())
		heal_amt = spawn->GetTotalHP();
	if(power_amt > spawn->GetTotalPower())
		power_amt = spawn->GetTotalPower();

	spawn->SetHP(heal_amt);
	if(power_amt > 0)
		spawn->SetPower(power_amt);

	if(client && caster){
		EQ2Packet* move = ((Player*)spawn)->Move(caster->GetX(), caster->GetY(), caster->GetZ(), client->GetVersion());
		if(move)
			client->QueuePacket(move);
	}

	if(crit){
		power_packet_type = HEAL_PACKET_TYPE_CRIT_MANA;
		heal_packet_type = HEAL_PACKET_TYPE_CRIT_HEAL;
	}
	else {
		power_packet_type = HEAL_PACKET_TYPE_SIMPLE_MANA;
		heal_packet_type = HEAL_PACKET_TYPE_SIMPLE_HEAL;
	}

	SendHealPacket(caster, spawn, heal_packet_type, heal_amt, heal_spell.c_str());
	if(power_amt > 0)
		SendHealPacket(caster, spawn, power_packet_type, power_amt, heal_spell.c_str());

	//The following code sets the spawn as alive
	if(dead_spawns.count(spawn->GetID()) > 0)
		dead_spawns.erase(spawn->GetID());

	if(spawn->IsPlayer()){
		spawn->SetSpawnType(4);
		client = GetClientBySpawn(spawn);
		if(client){
			packet = configReader.getStruct("WS_Resurrected", client->GetVersion());
			if(packet){
				client->QueuePacket(packet->serialize());
			}
			packet = configReader.getStruct("WS_ServerControlFlags", client->GetVersion());
			if(packet)
			{
				packet->setDataByName("parameter1", 8);
				client->QueuePacket(packet->serialize());
				packet->setDataByName("parameter1", 16);
				client->QueuePacket(packet->serialize());
			}

			safe_delete(packet);
			client->SimpleMessage(CHANNEL_COLOR_REVIVE, "You regain consciousness!");
		}
	}
	spawn->SendSpawnChanges(true);
	spawn->SetTempActionState(-1);
	spawn->appearance.attackable = 1;

	caster->GetZone()->GetSpellProcess()->DeleteCasterSpell(rez->spell, true);
}

void ZoneServer::SendDispellPacket(Entity* caster, Spawn* target, string dispell_name, string spell_name, int8 dispell_type){
	if(!caster || !target)
		return;

	Client* client = 0;
	Player* player = 0;
	PacketStruct* packet = 0;
	vector<Client*>::iterator client_itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (client_itr = clients.begin(); client_itr != clients.end(); client_itr++){
		client = *client_itr;
		if(!client || !(player = client->GetPlayer()) || (player != caster && ((caster && player->WasSentSpawn(caster->GetID()) == false) || (target && player->WasSentSpawn(target->GetID()) == false))))
			continue;
		if((caster && player->WasSpawnRemoved(caster)) || (caster && player->WasSpawnRemoved(target)))
			continue;
		if(caster && caster->GetDistance(player) > 50)
			continue;
		if(target && target->GetDistance(player) > 50)
			continue;

		packet = configReader.getStruct("WS_HearDispell", client->GetVersion());
		if(packet){
			packet->setDataByName("spell_name", spell_name.c_str());
			packet->setDataByName("dispell_name", dispell_name.c_str());
			packet->setDataByName("caster", player->GetIDWithPlayerSpawn(caster));
			packet->setDataByName("target", player->GetIDWithPlayerSpawn(target));
			packet->setDataByName("type", dispell_type);
			client->QueuePacket(packet->serialize());
		}
		safe_delete(packet);
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::DismissAllPets() {
	Spawn* spawn = 0;
	map<int32, Spawn*>::iterator itr;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
		spawn = itr->second;
		if (spawn && spawn->IsPet() && ((NPC*)spawn)->GetOwner())
			((NPC*)spawn)->GetOwner()->DismissPet((NPC*)spawn);
	}
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::RemoveTargetFromSpell(LuaSpell* spell, Spawn* target){
	if (spellProcess)
		spellProcess->RemoveTargetFromSpell(spell, target);
}

void ZoneServer::ClearHate(Entity* entity) {
	Spawn* spawn = 0;
	map<int32, Spawn*>::iterator itr;
	MSpawnList.readlock(__FUNCTION__, __LINE__);
	for (itr = spawn_list.begin(); itr != spawn_list.end(); itr++) {
		spawn = itr->second;
		if (spawn && spawn->IsNPC())
			((NPC*)spawn)->Brain()->ClearHate(entity);
	}
	MSpawnList.releasereadlock(__FUNCTION__, __LINE__);
}

ThreadReturnType ZoneLoop(void* tmp) {
#ifdef WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
#endif
	if (tmp == 0) {
		ThrowError("ZoneLoop(): tmp = 0!");
		THREAD_RETURN(NULL);
	}
	ZoneServer* zs = (ZoneServer*) tmp;
	while (zs->Process()) {
		if(zs->GetClientCount() == 0)
			Sleep(1000);
		else
			Sleep(10);
	}
	zs->Process(); //run loop once more to clean up some functions
	safe_delete(zs);
	THREAD_RETURN(NULL);
}

ThreadReturnType SpawnLoop(void* tmp) {
#ifdef WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
#endif
	if (tmp == 0) {
		ThrowError("SpawnLoop(): tmp = 0!");
		THREAD_RETURN(NULL);
	}
	ZoneServer* zs = (ZoneServer*) tmp;
#ifndef NO_CATCH
	try {
#endif
		zs->spawnthread_active = true;
		while (zs->SpawnProcess()) {
			if(zs->GetClientCount() == 0)
				Sleep(1000);
			else
				Sleep(20);
		}
		zs->spawnthread_active = false;
#ifndef NO_CATCH
	}
	catch(...) {
		LogWrite(ZONE__ERROR, 0, "Zone",  "Error Processing SpawnLoop, shutting down zone '%s'...", zs->GetZoneName());
		try{
			zs->Shutdown();
		}
		catch(...){
			LogWrite(ZONE__ERROR, 0, "Zone",  "Error Processing SpawnLoop while shutting down zone '%s'...", zs->GetZoneName());
		}
	}
#endif
	THREAD_RETURN(NULL);
}

ThreadReturnType SendInitialSpawns(void* tmp) {
#ifdef WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
#endif
	if (tmp == 0) {
		ThrowError("SendInitialSpawns(): tmp = 0!");
		THREAD_RETURN(NULL);
	}
	Client* client = (Client*) tmp;
	client->GetCurrentZone()->SendZoneSpawns(client);
	THREAD_RETURN(NULL);
}

ThreadReturnType SendLevelChangedSpawns(void* tmp) {
#ifdef WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
#endif
	if (tmp == 0) {
		ThrowError("SendLevelChangedSpawns(): tmp = 0!");
		THREAD_RETURN(NULL);
	}
	Client* client = (Client*)tmp;
	client->GetCurrentZone()->SendAllSpawnsForLevelChange(client);
	THREAD_RETURN(NULL);
}

ThreadReturnType SendAggroChangedSpawns(void* tmp) {
#ifdef WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
#endif
	if (tmp == 0) {
		ThrowError("SendAggroChangedSpawns(): tmp = 0!");
		THREAD_RETURN(NULL);
	}
	Client* client = (Client*)tmp;
	client->GetCurrentZone()->ResendSpawns(client);
	THREAD_RETURN(NULL);
}

void ZoneServer::SetSpawnStructs(Client* client) {
	int16 client_ver = client->GetVersion();
	Player* player = client->GetPlayer();

	//Save a copy of the correct spawn substructs for the client's player, save here a copy if we don't have one
	PacketStruct* pos = configReader.getStruct("Substruct_SpawnPositionStruct", client_ver);
	player->SetSpawnPosStruct(pos);
	if (versioned_pos_structs.count(pos->GetVersion()) == 0)
		versioned_pos_structs[pos->GetVersion()] = new PacketStruct(pos, true);

	PacketStruct* vis = configReader.getStruct("Substruct_SpawnVisualizationInfoStruct", client_ver);
	player->SetSpawnVisStruct(vis);
	if (versioned_vis_structs.count(vis->GetVersion()) == 0)
		versioned_vis_structs[vis->GetVersion()] = new PacketStruct(vis, true);

	PacketStruct* info = configReader.getStruct("Substruct_SpawnInfoStruct", client_ver);
	player->SetSpawnInfoStruct(info);
	if (versioned_info_structs.count(info->GetVersion()) == 0)
		versioned_info_structs[info->GetVersion()] = new PacketStruct(info, true);

	PacketStruct* header = configReader.getStruct("WS_SpawnStruct_Header", client_ver);
	player->SetSpawnHeaderStruct(header);

	PacketStruct* footer = configReader.getStruct("WS_SpawnStruct_Footer", client_ver);
	player->SetSpawnFooterStruct(footer);

	PacketStruct* sfooter = configReader.getStruct("WS_SignWidgetSpawnStruct_Footer", client_ver);
	player->SetSignFooterStruct(sfooter);

	PacketStruct* wfooter = configReader.getStruct("WS_WidgetSpawnStruct_Footer", client_ver);
	player->SetWidgetFooterStruct(wfooter);
}

Spawn* ZoneServer::GetSpawn(int32 id){
	Spawn* ret = 0;

	if(GetNPC(id))
		ret = GetNewNPC(id);
	else if(this->GetObject(id))
		ret = GetNewObject(id);
	else if(GetWidget(id))
		ret = GetNewWidget(id);
	else if(GetSign(id))
		ret = GetNewSign(id);
	else if(GetGroundSpawn(id))
		ret = GetNewGroundSpawn(id);
	// Unable to find the spawn in the list lets attempt to add it if we are not currently reloading
	else if (!reloading && database.LoadNPC(this, id)) {
		if (GetNPC(id))
			ret = GetNewNPC(id);
		else
			LogWrite(NPC__ERROR, 0, "NPC", "Database inserted npc (%u) but was still unable to retrieve it!", id);
	}
	else if (!reloading && database.LoadObject(this, id)) {
		if (this->GetObject(id))
			ret = GetNewObject(id);
		else
			LogWrite(OBJECT__ERROR, 0, "Object", "Database inserted object (%u) but was still unable to retrieve it!", id);
	}
	else if (!reloading && database.LoadWidget(this, id)) {
		if (GetWidget(id))
			ret = GetNewWidget(id);
		else
			LogWrite(WIDGET__ERROR, 0, "Widget", "Database inserted widget (%u) but was still unable to retrieve it!", id);
	}
	else if (!reloading && database.LoadSign(this, id)) {
		if (GetSign(id))
			ret = GetNewSign(id);
		else
			LogWrite(SIGN__ERROR, 0, "Sign", "Database inserted sign (%u) but was still unable to retrieve it!", id);
	}
	else if (!reloading && database.LoadGroundSpawn(this, id)) {
		if (GetGroundSpawn(id))
			ret = GetNewGroundSpawn(id);
		else
			LogWrite(GROUNDSPAWN__ERROR, 0, "GSpawn", "Database inserted ground spawn (%u) but was still unable to retrieve it!", id);
	}

	if(ret)
		ret->SetID(Spawn::NextID());
	return ret;
}




vector<EntityCommand*>* ZoneServer::GetEntityCommandList(int32 id){
	if(entity_command_list.count(id) > 0)
		return entity_command_list[id];
	else
		return 0;
}

void ZoneServer::SetEntityCommandList(int32 id, EntityCommand* command) {
	if (entity_command_list.count(id) == 0)
		entity_command_list[id] = new vector<EntityCommand*>;

	entity_command_list[id]->push_back(command);
}

EntityCommand* ZoneServer::GetEntityCommand(int32 id, string name) {
	EntityCommand* ret = 0;
	if (entity_command_list.count(id) == 0)
		return ret;

	vector<EntityCommand*>::iterator itr;
	for (itr = entity_command_list[id]->begin(); itr != entity_command_list[id]->end(); itr++) {
		if ((*itr)->name == name) {
			ret = (*itr);
			break;
		}
	}

	return ret;
}

void ZoneServer::ClearEntityCommands() {
	if (entity_command_list.size() > 0) {
		map<int32, vector<EntityCommand*>* >::iterator itr;
		for (itr = entity_command_list.begin(); itr != entity_command_list.end(); itr++) {
			vector<EntityCommand*>* entity_commands = itr->second;
			if (entity_commands && entity_commands->size() > 0) {
				vector<EntityCommand*>::iterator v_itr;
				for (v_itr = entity_commands->begin(); v_itr != entity_commands->end(); v_itr++)
					safe_delete(*v_itr);
				entity_commands->clear();
			}
			safe_delete(entity_commands);
		}
		entity_command_list.clear();
	}
}

void ZoneServer::AddNPCSpell(int32 list_id, int32 spell_id, int8 tier){
	npc_spell_list[list_id][spell_id] = tier;
}

vector<Spell*>* ZoneServer::GetNPCSpells(int32 primary_list, int32 secondary_list){
	vector<Spell*>* ret = 0;
	if(npc_spell_list.count(primary_list) > 0){
		ret = new vector<Spell*>();
		map<int32, int8>::iterator itr;
		Spell* tmpSpell = 0;
		for(itr = npc_spell_list[primary_list].begin(); itr != npc_spell_list[primary_list].end(); itr++){
			tmpSpell = master_spell_list.GetSpell(itr->first, itr->second);
			if(tmpSpell)
				ret->push_back(tmpSpell);
		}
	}
	if(npc_spell_list.count(secondary_list) > 0){
		if(!ret)
			ret = new vector<Spell*>();
		map<int32, int8>::iterator itr;
		Spell* tmpSpell = 0;
		for(itr = npc_spell_list[secondary_list].begin(); itr != npc_spell_list[secondary_list].end(); itr++){
			tmpSpell = master_spell_list.GetSpell(itr->first, itr->second);
			if(tmpSpell)
				ret->push_back(tmpSpell);
		}
	}
	if(ret && ret->size() == 0){
		safe_delete(ret);
		ret = 0;
	}
	return ret;
}

void ZoneServer::AddNPCSkill(int32 list_id, int32 skill_id, int16 value){
	npc_skill_list[list_id][skill_id] = value;
}

map<string, Skill*>* ZoneServer::GetNPCSkills(int32 primary_list, int32 secondary_list){
	map<string, Skill*>* ret = 0;
	if(npc_skill_list.count(primary_list) > 0){
		ret = new map<string, Skill*>();
		map<int32, int16>::iterator itr;
		Skill* tmpSkill = 0;
		for(itr = npc_skill_list[primary_list].begin(); itr != npc_skill_list[primary_list].end(); itr++){
			tmpSkill = master_skill_list.GetSkill(itr->first);
			if(tmpSkill){
				tmpSkill = new Skill(tmpSkill);
				tmpSkill->current_val = itr->second;
				tmpSkill->max_val = tmpSkill->current_val+5;
				(*ret)[tmpSkill->name.data] = tmpSkill;
			}
		}
	}
	if(npc_skill_list.count(secondary_list) > 0){
		if(!ret)
			ret = new map<string, Skill*>();
		map<int32, int16>::iterator itr;
		Skill* tmpSkill = 0;
		for(itr = npc_skill_list[secondary_list].begin(); itr != npc_skill_list[secondary_list].end(); itr++){
			tmpSkill = master_skill_list.GetSkill(itr->first);
			if(tmpSkill){
				tmpSkill = new Skill(tmpSkill);
				tmpSkill->current_val = itr->second;
				tmpSkill->max_val = tmpSkill->current_val+5;
				(*ret)[tmpSkill->name.data] = tmpSkill;
			}
		}
	}
	if(ret && ret->size() == 0){
		safe_delete(ret);
		ret = 0;
	}
	return ret;
}

void ZoneServer::AddNPCEquipment(int32 list_id, int32 item_id){
	npc_equipment_list[list_id].push_back(item_id);
}

void ZoneServer::SetNPCEquipment(NPC* npc) {
	if(npc_equipment_list.count(npc->GetEquipmentListID()) > 0){
		Item* tmpItem = 0;
		int8 slot = 0;
		vector<int32>::iterator itr;
		for(itr = npc_equipment_list[npc->GetEquipmentListID()].begin(); itr != npc_equipment_list[npc->GetEquipmentListID()].end(); itr++){
			tmpItem = master_item_list.GetItem(*itr);
			if(tmpItem){
				slot = npc->GetEquipmentList()->GetFreeSlot(tmpItem);
				if(slot < 255){
					tmpItem = new Item(tmpItem);
					npc->GetEquipmentList()->SetItem(slot, tmpItem);
				}
			}
		}
	}
}

void ZoneServer::AddNPC(int32 id, NPC* npc) {
	npc_list[id] = npc;
}

void ZoneServer::AddWidget(int32 id, Widget* widget) {
	widget_list[id] = widget;
}

Widget* ZoneServer::GetWidget(int32 id, bool override_loading) { 
	if((!reloading || override_loading) && widget_list.count(id) > 0)
		return widget_list[id]; 
	else
		return 0;
}

Widget* ZoneServer::GetNewWidget(int32 id) {
	if(!reloading && widget_list.count(id) > 0)
		return widget_list[id]->Copy(); 
	else
		return 0;
}


void ZoneServer::LoadGroundSpawnEntries(){
	MGroundSpawnItems.lock();
	database.LoadGroundSpawnEntries(this);
	MGroundSpawnItems.unlock();
}

void ZoneServer::LoadGroundSpawnItems() {
}

void ZoneServer::AddGroundSpawnEntry(int32 groundspawn_id, int16 min_skill_level, int16 min_adventure_level, int8 bonus_table, float harvest1, float harvest3, float harvest5, float harvest_imbue, float harvest_rare, float harvest10, int32 harvest_coin) {
	GroundSpawnEntry* entry = new GroundSpawnEntry;
	entry->min_skill_level = min_skill_level;
	entry->min_adventure_level = min_adventure_level;
	entry->bonus_table = bonus_table;
	entry->harvest1 = harvest1;
	entry->harvest3 = harvest3;
	entry->harvest5 = harvest5;
	entry->harvest_imbue = harvest_imbue;
	entry->harvest_rare = harvest_rare;
	entry->harvest10 = harvest10;
	entry->harvest_coin = harvest_coin;
	groundspawn_entries[groundspawn_id].push_back(entry);
}

void ZoneServer::AddGroundSpawnItem(int32 groundspawn_id, int32 item_id, int8 is_rare, int32 grid_id) {
	GroundSpawnEntryItem* entry = new GroundSpawnEntryItem;
	entry->item_id = item_id;
	entry->is_rare = is_rare;
	entry->grid_id = grid_id;
	groundspawn_items[groundspawn_id].push_back(entry);

}

vector<GroundSpawnEntry*>* ZoneServer::GetGroundSpawnEntries(int32 id){
	vector<GroundSpawnEntry*>* ret = 0;
	MGroundSpawnItems.lock();
	if(groundspawn_entries.count(id) > 0)
		ret = &groundspawn_entries[id];
	MGroundSpawnItems.unlock();
	return ret;
}

vector<GroundSpawnEntryItem*>* ZoneServer::GetGroundSpawnEntryItems(int32 id){
	vector<GroundSpawnEntryItem*>* ret = 0;
	if(groundspawn_items.count(id) > 0)
		ret = &groundspawn_items[id];
	return ret;
}

// TODO - mis-named, should be DeleteGroundSpawnEntries() but this is ok for now :)
void ZoneServer::DeleteGroundSpawnItems()
{
	MGroundSpawnItems.lock();

	map<int32, vector<GroundSpawnEntry*> >::iterator groundspawnentry_map_itr;
	vector<GroundSpawnEntry*>::iterator groundspawnentry_itr;
	for(groundspawnentry_map_itr = groundspawn_entries.begin(); groundspawnentry_map_itr != groundspawn_entries.end(); groundspawnentry_map_itr++)
	{
		for(groundspawnentry_itr = groundspawnentry_map_itr->second.begin(); groundspawnentry_itr != groundspawnentry_map_itr->second.end(); groundspawnentry_itr++)
		{
			safe_delete(*groundspawnentry_itr);
		}
	}
	groundspawn_entries.clear();

	map<int32, vector<GroundSpawnEntryItem*> >::iterator groundspawnitem_map_itr;
	vector<GroundSpawnEntryItem*>::iterator groundspawnitem_itr;
	for(groundspawnitem_map_itr = groundspawn_items.begin(); groundspawnitem_map_itr != groundspawn_items.end(); groundspawnitem_map_itr++)
	{
		for(groundspawnitem_itr = groundspawnitem_map_itr->second.begin(); groundspawnitem_itr != groundspawnitem_map_itr->second.end(); groundspawnitem_itr++)
		{
			safe_delete(*groundspawnitem_itr);
		}
	}
	groundspawn_items.clear();

	MGroundSpawnItems.unlock();
}

void ZoneServer::AddGroundSpawn(int32 id, GroundSpawn* spawn) {
	groundspawn_list[id] = spawn;
}

GroundSpawn* ZoneServer::GetGroundSpawn(int32 id, bool override_loading) { 
	if((!reloading || override_loading) && groundspawn_list.count(id) > 0)
		return groundspawn_list[id]; 
	else
		return 0;
}

GroundSpawn* ZoneServer::GetNewGroundSpawn(int32 id) {
	if(!reloading && groundspawn_list.count(id) > 0)
		return groundspawn_list[id]->Copy(); 
	else
		return 0;
}

void ZoneServer::AddLootTable(int32 id, LootTable* table){
	loot_tables[id] = table;
}

void ZoneServer::AddLootDrop(int32 id, LootDrop* drop){
	loot_drops[id].push_back(drop);
}

void ZoneServer::AddSpawnLootList(int32 spawn_id, int32 id){
	spawn_loot_list[spawn_id].push_back(id);
}

void ZoneServer::AddLevelLootList(int8 level, int32 id) {
	level_loot_list[level].push_back(id);
}

void ZoneServer::AddRacialLootList(int16 racial_id, int32 id) {
	racial_loot_list[racial_id].push_back(id);
}

void ZoneServer::AddZoneLootList(int32 zone, ZoneLoot* loot) {
	zone_loot_list[zone].push_back(loot);
}

void ZoneServer::ClearLootTables(){
	map<int32,LootTable*>::iterator table_itr;
	for(table_itr = loot_tables.begin(); table_itr != loot_tables.end(); table_itr++){
		safe_delete(table_itr->second);
	}

	map<int32, vector<LootDrop*> >::iterator drop_itr;
	vector<LootDrop*>::iterator drop_itr2;
	for(drop_itr = loot_drops.begin(); drop_itr != loot_drops.end(); drop_itr++){
		for(drop_itr2 = drop_itr->second.begin(); drop_itr2 != drop_itr->second.end(); drop_itr2++){
			safe_delete(*drop_itr2);
		}
	}

	map<int32, vector<ZoneLoot*> >::iterator zone_itr;
	vector<ZoneLoot*>::iterator zone_itr2;
	for(zone_itr = zone_loot_list.begin(); zone_itr != zone_loot_list.end(); zone_itr++) {
		for (zone_itr2 = zone_itr->second.begin(); zone_itr2 != zone_itr->second.end(); zone_itr2++) {
			safe_delete(*zone_itr2);
		}
	}

	loot_tables.clear();
	loot_drops.clear();
	spawn_loot_list.clear();
	level_loot_list.clear();
	racial_loot_list.clear();
	zone_loot_list.clear();
}

vector<int32> ZoneServer::GetSpawnLootList(int32 spawn_id, int32 zone_id, int8 spawn_level, int16 racial_id) {
	vector<int32> ret;
	if(reloading)
		return ret;

	if (spawn_loot_list.count(spawn_id) > 0)
		ret.insert(ret.end(), spawn_loot_list[spawn_id].begin(), spawn_loot_list[spawn_id].end());

	if (level_loot_list.count(spawn_level) > 0)
		ret.insert(ret.end(), level_loot_list[spawn_level].begin(), level_loot_list[spawn_level].end());

	if (racial_loot_list.count(racial_id) > 0)
		ret.insert(ret.end(), racial_loot_list[racial_id].begin(), racial_loot_list[racial_id].end());

	if (zone_loot_list.count(zone_id) > 0) {
		vector<ZoneLoot*>::iterator itr;
		for (itr = zone_loot_list[zone_id].begin(); itr != zone_loot_list[zone_id].end(); itr++) {
			ZoneLoot* loot = *itr;
			if (loot->minLevel == 0 && loot->maxLevel == 0)
				ret.push_back(loot->table_id);
			else {
				if (spawn_level >= loot->minLevel && spawn_level <= loot->maxLevel)
					ret.push_back(loot->table_id);
			}
		}
	}

	return ret;
}

vector<LootDrop*>* ZoneServer::GetLootDrops(int32 table_id){
	if(!reloading && loot_drops.count(table_id) > 0)
		return &(loot_drops[table_id]);
	else
		return 0;
}

LootTable* ZoneServer::GetLootTable(int32 table_id){
	return loot_tables[table_id];
}

void ZoneServer::AddLocationTransporter(int32 zone_id, string message, float trigger_x, float trigger_y, float trigger_z, float trigger_radius, int32 destination_zone_id, float destination_x, float destination_y, float destination_z, float destination_heading, int32 cost, int32 unique_id){
	LocationTransportDestination* loc = new LocationTransportDestination;
	loc->message = message;
	loc->trigger_x = trigger_x;
	loc->trigger_y = trigger_y;
	loc->trigger_z = trigger_z;
	loc->trigger_radius = trigger_radius;
	loc->destination_zone_id = destination_zone_id;
	loc->destination_x = destination_x;
	loc->destination_y = destination_y;
	loc->destination_z = destination_z;
	loc->destination_heading = destination_heading;
	loc->cost = cost;
	loc->unique_id = unique_id;
	MTransporters.lock();
	if(location_transporters.count(zone_id) == 0)
		location_transporters[zone_id] = new MutexList<LocationTransportDestination*>();
	location_transporters[zone_id]->Add(loc);
	MTransporters.unlock();
}

void ZoneServer::AddTransporter(int32 transport_id, int8 type, string name, string message, int32 destination_zone_id, float destination_x, float destination_y, float destination_z, float destination_heading, int32 cost, int32 unique_id, int8 min_level, int8 max_level, int32 quest_req, int16 quest_step_req, int32 quest_complete, int32 map_x, int32 map_y){
	TransportDestination* transport = new TransportDestination;
	transport->type = type;
	transport->display_name = name;
	transport->message = message;
	transport->destination_zone_id = destination_zone_id;
	transport->destination_x = destination_x;
	transport->destination_y = destination_y;
	transport->destination_z = destination_z;
	transport->destination_heading = destination_heading;
	transport->cost = cost;
	transport->unique_id = unique_id;

	transport->min_level = min_level;
	transport->max_level = max_level;
	transport->req_quest = quest_req;
	transport->req_quest_step = quest_step_req;
	transport->req_quest_complete = quest_complete;

	transport->map_x = map_x;
	transport->map_y = map_y;

	MTransporters.lock();
	transporters[transport_id].push_back(transport);
	MTransporters.unlock();
}

vector<TransportDestination*>* ZoneServer::GetTransporters(int32 transport_id){
	vector<TransportDestination*>* ret = 0;
	MTransporters.lock();
	if(transporters.count(transport_id) > 0)
		ret = &transporters[transport_id];
	MTransporters.unlock();
	return ret;
}

MutexList<LocationTransportDestination*>* ZoneServer::GetLocationTransporters(int32 zone_id){
	MutexList<LocationTransportDestination*>* ret = 0;
	MTransporters.lock();
	if(location_transporters.count(zone_id) > 0)
		ret = location_transporters[zone_id];
	MTransporters.unlock();
	return ret;
}

void ZoneServer::DeleteGlobalTransporters(){
	MTransporters.lock();
	map<int32, vector<TransportDestination*> >::iterator itr;
	vector<TransportDestination*>::iterator transport_vector_itr;
	for(itr = transporters.begin(); itr != transporters.end(); itr++){
		for(transport_vector_itr = itr->second.begin(); transport_vector_itr != itr->second.end(); transport_vector_itr++){
			safe_delete(*transport_vector_itr);
		}
	}
	map<int32, MutexList<LocationTransportDestination*>* >::iterator itr2;
	for(itr2 = location_transporters.begin(); itr2 != location_transporters.end(); itr2++){
		itr2->second->clear(true);
		delete itr2->second;
	}
	transporters.clear();
	location_transporters.clear();
	MTransporters.unlock();
}

void ZoneServer::DeleteGlobalSpawns() {
	ClearLootTables();
	
	map<int32, NPC*>::iterator npc_list_iter;
	for(npc_list_iter=npc_list.begin();npc_list_iter!=npc_list.end();npc_list_iter++) {
		safe_delete(npc_list_iter->second);
	} 
	npc_list.clear();
	map<int32, Object*>::iterator object_list_iter;
	for(object_list_iter=object_list.begin();object_list_iter!=object_list.end();object_list_iter++) {
		safe_delete(object_list_iter->second);
	}
	object_list.clear();
	map<int32, GroundSpawn*>::iterator groundspawn_list_iter;
	for(groundspawn_list_iter=groundspawn_list.begin();groundspawn_list_iter!=groundspawn_list.end();groundspawn_list_iter++) {
		safe_delete(groundspawn_list_iter->second);
	}
	groundspawn_list.clear();
	map<int32, Widget*>::iterator widget_list_iter;
	for(widget_list_iter=widget_list.begin();widget_list_iter!=widget_list.end();widget_list_iter++) {
		safe_delete(widget_list_iter->second);
	}
	widget_list.clear();
	map<int32, Sign*>::iterator sign_list_iter;
	for(sign_list_iter=sign_list.begin();sign_list_iter!=sign_list.end();sign_list_iter++) {
		safe_delete(sign_list_iter->second);
	}
	sign_list.clear();

	/*map<int32, AppearanceData*>::iterator appearance_list_iter;
	for(appearance_list_iter=npc_appearance_list.begin();appearance_list_iter!=npc_appearance_list.end();appearance_list_iter++) {
		safe_delete(appearance_list_iter->second);
	}
	npc_appearance_list.clear();*/

	
	ClearEntityCommands();

	DeleteGroundSpawnItems();
	DeleteGlobalTransporters();
	DeleteTransporterMaps();
}

void ZoneServer::AddTransportMap(int32 id, string name) {
	MTransportMaps.writelock(__FUNCTION__, __LINE__);
	m_transportMaps[id] = name;
	MTransportMaps.releasewritelock(__FUNCTION__, __LINE__);
}

bool ZoneServer::TransportHasMap(int32 id) {
	MTransportMaps.readlock(__FUNCTION__, __LINE__);
	bool ret = m_transportMaps.count(id) > 0;
	MTransportMaps.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}

string ZoneServer::GetTransportMap(int32 id) {
	string ret;

	MTransportMaps.readlock(__FUNCTION__, __LINE__);
	if (m_transportMaps.count(id) > 0)
		ret = m_transportMaps[id];
	MTransportMaps.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}

void ZoneServer::DeleteTransporterMaps() {
	MTransportMaps.writelock(__FUNCTION__, __LINE__);
	m_transportMaps.clear();
	MTransportMaps.releasewritelock(__FUNCTION__, __LINE__);
}

void ZoneServer::ReloadSpawns() {
	if (reloading)
		return;

	reloading = true;
	// Let every one in the zone know what is happening
	HandleBroadcast("Reloading all spawns for this zone.");
	DeleteGlobalSpawns();
	Depop(false, true);
}

void ZoneServer::SendStateCommand(Spawn* spawn, int32 state) {
	vector<Client*>::iterator itr;

	MClientList.readlock(__FUNCTION__, __LINE__);
	for (itr = clients.begin(); itr != clients.end(); itr++) {
		Client* client = *itr;
		if (client && client->GetPlayer()->WasSentSpawn(spawn->GetID()) && !client->GetPlayer()->WasSpawnRemoved(spawn))
			ClientPacketFunctions::SendStateCommand(client, client->GetPlayer()->GetIDWithPlayerSpawn(spawn), state);
	}
	MClientList.releasereadlock(__FUNCTION__, __LINE__);
}

void ZoneServer::AddFlightPath(int32 id, FlightPathInfo* info) {
	if (m_flightPaths.count(id) > 0) {
		LogWrite(ZONE__ERROR, 0, "Zone", "Duplicate flight path (%u)", id);
		safe_delete(info);
		return;
	}

	m_flightPaths[id] = info;
}

void ZoneServer::AddFlightPathLocation(int32 id, FlightPathLocation* location) {
	if (m_flightPaths.count(id) == 0) {
		LogWrite(ZONE__ERROR, 0, "Zone", "There is no flight info for this route (%u)", id);
		safe_delete(location);
		return;
	}

	m_flightPathRoutes[id].push_back(location);
}

void ZoneServer::DeleteFlightPaths() {
	map<int32, vector<FlightPathLocation*> >::iterator itr;
	vector<FlightPathLocation*>::iterator itr2;
	map<int32, FlightPathInfo*>::iterator itr3;

	for (itr = m_flightPathRoutes.begin(); itr != m_flightPathRoutes.end(); itr++) {
		for (itr2 = itr->second.begin(); itr2 != itr->second.end(); itr2++) {
			safe_delete(*itr2);
		}

		itr->second.clear();
	}
	m_flightPathRoutes.clear();

	for (itr3 = m_flightPaths.begin(); itr3 != m_flightPaths.end(); itr3++) {
		safe_delete(itr3->second);
	}
	m_flightPaths.clear();
}

void ZoneServer::SendFlightPathsPackets(Client* client) {
	// Only send a packet if there are flight paths
	if (m_flightPathRoutes.size() > 0) {
		PacketStruct* packet = configReader.getStruct("WS_FlightPathsMsg", client->GetVersion());
		if (packet) {

			int32 num_routes = m_flightPaths.size();
			packet->setArrayLengthByName("number_of_routes", num_routes);
			packet->setArrayLengthByName("number_of_routes2", num_routes);
			packet->setArrayLengthByName("number_of_routes3", num_routes);
			packet->setArrayLengthByName("number_of_routes4", num_routes);

			map<int32, FlightPathInfo*>::iterator itr;
			int32 i = 0;
			for (itr = m_flightPaths.begin(); itr != m_flightPaths.end(); itr++, i++) {
				packet->setArrayDataByName("route_length", m_flightPathRoutes[itr->first].size(), i);
				packet->setArrayDataByName("ground_mount", itr->second->flying ? 0 : 1, i);
				packet->setArrayDataByName("allow_dismount", itr->second->dismount ? 1 : 0, i);


				packet->setSubArrayLengthByName("route_length2", m_flightPathRoutes[itr->first].size(), i);
				vector<FlightPathLocation*>::iterator itr2;
				int32 j = 0;
				for (itr2 = m_flightPathRoutes[itr->first].begin(); itr2 != m_flightPathRoutes[itr->first].end(); itr2++, j++) {
					packet->setSubArrayDataByName("x", (*itr2)->X, i, j);
					packet->setSubArrayDataByName("y", (*itr2)->Y, i, j);
					packet->setSubArrayDataByName("z", (*itr2)->Z, i, j);
				}
			}

			client->QueuePacket(packet->serialize());
			safe_delete(packet);
		}
	}
}

int32 ZoneServer::GetFlightPathIndex(int32 id) {
	int32 index = 0;
	map<int32, FlightPathInfo*>::iterator itr;
	for (itr = m_flightPaths.begin(); itr != m_flightPaths.end(); itr++, index++) {
		if (itr->first == id)
			return index;
	}

	return -1;
}

float ZoneServer::GetFlightPathSpeed(int32 id) {
	float speed = 1;

	if (m_flightPaths.count(id) > 0)
		speed = m_flightPaths[id]->speed;

	return speed;
}