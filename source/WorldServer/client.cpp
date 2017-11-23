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
#include "../common/Log.h"
#include <iostream>
#include <iomanip>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <zlib.h>
#include <assert.h>
#include <algorithm>
#include "Player.h"
#include "Commands/Commands.h"
#include "ClientPacketFunctions.h"
#include "../common/ConfigReader.h"
#include "Guilds/Guild.h"
#include "Variables.h"
#include "Rules/Rules.h"
#include "Titles.h"
#include "IRC/IRC.h"
#include "Chat/Chat.h"

//#include "Quests.h"

#ifdef WIN32
#include <windows.h>
#include <winsock.h>
#define snprintf	_snprintf
#define vsnprintf	_vsnprintf
#define strncasecmp	_strnicmp
#define strcasecmp  _stricmp
#else
#include <sys/socket.h>
#ifdef FREEBSD
#include <sys/types.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include "client.h"
#include "../common/emu_opcodes.h"
#include "../common/packet_dump.h"
#include "WorldDatabase.h"
#include "races.h"
#include "classes.h"
#include "LoginServer.h"
#include "World.h"
#include "../common/EQ2_Common_Structs.h"
#include "net.h"
#include "../common/MiscFunctions.h"
#include "Items/Items.h"
#include "Skills.h"
#include "LuaInterface.h"
#include "Quests.h"
#include "Collections/Collections.h"
#include "Achievements/Achievements.h"
#include "Traits/Traits.h"
#include "Recipes/Recipe.h"
#include "Tradeskills/Tradeskills.h"
#include "AltAdvancement/AltAdvancement.h"
#include "zoneserver.h"
#include "SpellProcess.h"
extern WorldDatabase database;
extern const char* ZONE_NAME;
extern LoginServer loginserver;
extern sint32 numclients;
extern NetConnection net;
extern Commands commands;
extern ClientList client_list;
extern ZoneList zone_list;
extern ZoneAuth zone_auth;
extern MasterItemList master_item_list;
extern MasterSkillList master_skill_list;
extern MasterSpellList master_spell_list;
extern MasterTraitList master_trait_list;
extern MasterQuestList master_quest_list;
extern MasterFactionList master_faction_list;
extern MasterRecipeList master_recipe_list;
extern volatile bool RunLoops;
extern ConfigReader configReader;
extern LuaInterface* lua_interface;
extern World world;
extern Variables variables;
extern Classes classes;
extern Races races;
extern GuildList guild_list;
extern MasterCollectionList master_collection_list;
extern MasterAchievementList master_achievement_list;
extern RuleManager rule_manager;
extern IRC irc;
extern Chat chat;
extern MasterAAList master_aa_list;
extern MasterAAList master_tree_nodes;

using namespace std;

Client::Client(EQStream* ieqs) : pos_update(1), quest_pos_timer(2000), lua_debug_timer(30000){
	eqs = ieqs;
	ip = eqs->GetrIP();
	port = ntohs(eqs->GetrPort());
	merchant_transaction = 0;
	SetMailTransaction(0);
	timestamp_flag = 0;
	current_quest_id = 0;
	last_update_time = 0;
	quest_updates = false;

	//autobootup_timeout = new Timer(10000);
	//autobootup_timeout->Disable();

	CLE_keepalive_timer = new Timer(15000);
	connect = new Timer(1000);
	connect->Disable();
	zoneID = 0;
	account_name[0] = 0;
	account_id = 0;
	pwaitingforbootup = 0;
	current_zone = 0;
	connected_to_zone = false;
	connected = false;
	camp_timer = 0;
	disconnect_timer = 0;
	client_zoning = false;
	player_pos_changed = false;
	++numclients;
	if (world.GetServerStatisticValue(STAT_SERVER_MOST_CONNECTIONS) < numclients)
		world.UpdateServerStatistic(STAT_SERVER_MOST_CONNECTIONS, numclients, true);
	remove_from_list = false;
	new_client_login = false;
	UpdateWindowTitle(0);
	num_active_failures = 0;
	player = new Player();
	combine_spawn = 0;
	lua_debug = false;
	ready_for_spawns = false;
	lua_debug_timer.Disable();
	transport_spawn = 0;
	MBuyBack.SetName("Client::MBuyBack");
	MPendingQuestAccept.SetName("Client::MPendingQuestAccept");
	MDeletePlayer.SetName("Client::MDeletePlayer");
	MQuestPendingUpdates.SetName("Client::MQuestPendingUpdates");
	search_items = 0;
	version = 0;
	next_conversation_id = 0;
	pending_guild_invite.guild = 0;
	pending_guild_invite.invited_by = 0;
	m_recipeListSent = false;
	m_resurrect.SetName("Client::m_resurrect");
	current_rez.expire_timer = 0;
	current_rez.should_delete = true;
	pending_last_name = 0;
	should_target = false;
	initial_spawns_sent = false;
	MQuestTimers.SetName("Client::quest_timers");
	memset(&incoming_paperdoll, 0, sizeof(incoming_paperdoll));
	on_auto_mount = false;
	should_load_spells = true;
}

Client::~Client() {
	if(current_zone && player){
		if(player->GetGroupMemberInfo() && (player->GetActivityStatus() & ACTIVITY_STATUS_LINKDEAD) > 0)
			world.GetGroupManager()->RemoveGroupMember(player->GetGroupMemberInfo()->group_id, player);
		world.GetGroupManager()->ClearPendingInvite(player);
	}
	if(lua_interface)
		lua_interface->RemoveDebugClients(this);

	if (player)
		zone_list.RemoveClientFromMap(player->GetName(), this);

	//let the stream factory know were done with this stream
	if(eqs){
		eqs->Close();
		try{
			eqs->ReleaseFromUse();
		}
		catch(...){}
	}
	eqs = NULL;

	//safe_delete(autobootup_timeout);

	safe_delete(disconnect_timer);
	safe_delete(camp_timer);
	safe_delete(CLE_keepalive_timer);
	safe_delete(connect);
	--numclients;

	MDeletePlayer.writelock(__FUNCTION__, __LINE__);
	if (player && !player->GetPendingDeletion())
		safe_delete(player);
	MDeletePlayer.releasewritelock(__FUNCTION__, __LINE__);

	safe_delete(search_items);
	if(current_rez.expire_timer)
		safe_delete(current_rez.expire_timer);
	if (pending_last_name)
		safe_delete(pending_last_name);
	safe_delete_array(incoming_paperdoll.image_bytes);

	UpdateWindowTitle(0);
}

void Client::QueuePacket(EQ2Packet* app){
	if(eqs){
		if(!eqs->CheckActive()){
			client_list.Remove(this);
			eqs = 0;
		}
	}
	if(app && eqs && version > 0)
		eqs->EQ2QueuePacket(app);
	else{
		safe_delete(app);
	}

}

void Client::PopulateSkillMap(){
	EQ2Packet* app = master_skill_list.GetPopulateSkillsPacket(GetVersion());
	if(app)
		QueuePacket(app);
	else{
		LogWrite(WORLD__ERROR, 0, "World", "Unable to send populate skills packet for version: %i!", GetVersion());
		Disconnect(); //the client cant proceed without the skill packet, might as well kick it now
	}

}

void Client::SendLoginInfo() {
	if(firstlogin) {
		LogWrite(WORLD__DEBUG, 0, "World", "Increment Server_Accepted_Connection + 1");
		world.UpdateServerStatistic(STAT_SERVER_ACCEPTED_CONNECTION, 1);
		LogWrite(CCLIENT__DEBUG, 0, "Client", "Populate Skill Map...");
		PopulateSkillMap();

		// JA: Check client version and move player to valid zone if current client does not support last saved zone (loading SF client on DoV saved zone) IT CAN HAPPEN!
		LogWrite(MISC__TODO, 1, "TODO", "Check client version at login, move char if invalid zone file");
	}

	LogWrite(CCLIENT__DEBUG, 0, "Client", "Toggle Character Online...");
	database.ToggleCharacterOnline(this, 1);

	LogWrite(CCLIENT__DEBUG, 0, "Client", "Loading Character Skills for player '%s'...", player->GetName());
	int32 count = database.LoadCharacterSkills(GetCharacterID(), player);
	if(count == 0) {
		LogWrite(CCLIENT__WARNING, 0, "Client", "No character skills found!");
		database.UpdateStartingSkills(GetCharacterID(), player->GetAdventureClass(), player->GetRace());
		database.LoadCharacterSkills(GetCharacterID(), player);
	}

	count = database.LoadCharacterTitles(GetCharacterID(), player);
	if(count == 0) {
		LogWrite(CCLIENT__DEBUG, 0, "Client", "No character titles found!");
		LogWrite(CCLIENT__DEBUG, 0, "Client", "Initializing starting values - Titles");
		database.UpdateStartingTitles(GetCharacterID(), player->GetAdventureClass(), player->GetRace(), player->GetGender());
	}

	count = database.LoadCharacterLanguages(GetCharacterID(), player);
	if(count == 0)
		LogWrite(CCLIENT__DEBUG, 0, "Client", "No character languages loaded!");

	count = database.LoadCharacterSpells(GetCharacterID(), player);
	if(count == 0) {
		LogWrite(CCLIENT__DEBUG, 0, "Client", "No character spells found!");
		database.UpdateStartingSpells(GetCharacterID(), player->GetAdventureClass(), player->GetRace());
		database.LoadCharacterSpells(GetCharacterID(), player);
	}
	count = database.LoadPlayerRecipeBooks(GetCharacterID(), player);
	if(count == 0)
		LogWrite(CCLIENT__DEBUG, 0, "Client", "No character recipe books found!");

	ClientPacketFunctions::SendLoginAccepted( this );

	//ClientPacketFunctions::SendAbilities ( this );
	ClientPacketFunctions::SendCommandNamePacket ( this );

	ClientPacketFunctions::SendQuickBarInit ( this );

	// we only need to send the MOTD if it is the first time the person is logging in.
	if(firstlogin){
		ClientPacketFunctions::SendMOTD ( this );
		ClientPacketFunctions::SendCharacterMacros(this);
		zone_list.CheckFriendList(this);
	}
	database.LoadPlayerFactions(this);
	database.LoadCharacterQuests(this);
	database.LoadPlayerMail(this);
	LogWrite(CCLIENT__DEBUG, 0, "Client", "Send Quest Journal...");
	SendQuestJournal(true);
	SendCollectionList();
	SendBiography();

	map<int32, Quest*>::iterator itr;
	for (itr = player->player_quests.begin(); itr != player->player_quests.end(); ++itr) {
		Quest* quest = itr->second;
		if (quest->IsTracked()) {
			quest->SetTracked(false);
			QueuePacket(itr->second->QuestJournalReply(version, GetNameCRC(), player));
			quest->SetTracked(true);
			QueuePacket(itr->second->QuestJournalReply(version, GetNameCRC(), player));
		}
	}
	//SendAchievementsList();

	/*Guild* guild = player->GetGuild();
	if (guild) {
		guild->UpdateGuildMemberInfo(GetPlayer());
		if (firstlogin)
			guild->SendGuildMOTD(this);
		guild->SendGuildUpdate(this);
		guild->SendGuildMember(GetPlayer(), firstlogin);
		guild->SendGuildEventList(this);
		guild->SendGuildBankEventList(this);
		guild->SendAllGuildEvents(this);
		guild->SendGuildMemberList(this);
	}*/

	LogWrite(CCLIENT__DEBUG, 0, "Client", "Loading Faction Updates...");
	EQ2Packet* outapp = player->GetFactions()->FactionUpdate(GetVersion());
	if(outapp){
		LogWrite(CCLIENT__PACKET, 0, "Client", "Dump/Print Packet in func: %s, line: %i", __FUNCTION__, __LINE__);
		//DumpPacket(outapp);
		QueuePacket(outapp);
	}

	LogWrite(CCLIENT__DEBUG, 0, "Client", "Send Command List...");
	ClientPacketFunctions::SendCommandList( this );

	LogWrite(CCLIENT__DEBUG, 0, "Client", "Send Language Updates...");
	SendLanguagesUpdate(database.GetCharacterCurrentLang(GetCharacterID(), player));

	ClientPacketFunctions::SendInstanceList(this);

	SendZoneInfo();
	/*Spell* spell = 0;
	vector<QuickBarItem*>::iterator itr;
	for(itr = player->GetQuickbar()->begin(); itr != player->GetQuickbar()->end(); itr++){
	if((*itr)->type == 1){
	spell =  master_spell_list.GetSpell((*itr)->id);
	if(spell)
	QueuePacket(spell->serialize(this, false, 0x20));
	}
	}*/
}

void Client::SendPlayerDeathWindow()
{
	LogWrite(CCLIENT__DEBUG, 0, "Client", "SendPlayerDeathWindow");
	vector<RevivePoint*>* results=GetCurrentZone()->GetRevivePoints(this);
	vector<RevivePoint*>::iterator itr;

	if (results && results->size()>0)
	{
		PacketStruct* packet=configReader.getStruct("WS_DeathWindow", GetVersion());
		if (packet)
		{
			packet->setArrayLengthByName("location_count", results->size());
			int32 i=0;

			for (itr=results->begin(); itr!=results->end(); ++itr, ++i)
			{
				RevivePoint* point = *itr;
				if(point)
				{
					packet->setArrayDataByName("location_id", point->id, i);
					//zone_name = database.GetZoneName(point->zone_id);
					string zone_name = database.GetZoneDescription(point->zone_id);
					if(zone_name.length() > 0)
						packet->setArrayDataByName("zone_name", zone_name.c_str(), i);
					packet->setArrayDataByName("location_name", point->location_name.c_str(), i);	
					packet->setArrayDataByName("distance", GetPlayer()->GetDistance(point->x, point->y, point->z), i);
				}

				if(point->id == 0xFFFFFFFF)//tmp location
					safe_delete(point);
			}
			// done with the revive points so lets free up the pointer
			safe_delete(results);
#if EQDEBUG >= 3
			LogWrite(CCLIENT__DEBUG, 0, "Client", "WS_DeathWindow Packet:");
			packet->PrintPacket();
#endif
			EQ2Packet* outapp = packet->serialize();
			QueuePacket(outapp);
			safe_delete(packet);
		}
	}

}

void Client::DisplayDeadWindow()
{
	LogWrite(ZONE__DEBUG, 0, "Zone", "DisplayDeadWindow()");

	player->SetHP(0);
	player->SetPower(0);
	GetCurrentZone()->TriggerCharSheetTimer();

	PacketStruct* packet = configReader.getStruct("WS_ServerControlFlags", GetVersion());
	if(packet)
	{
		packet->setDataByName("parameter1", 8);
		packet->setDataByName("value", 1);
		QueuePacket(packet->serialize());
		packet->setDataByName("parameter1", 16);
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

	packet = configReader.getStruct("WS_ServerUpdateTarget", GetVersion());
	if(packet)
	{
		packet->setDataByName("spawn_id", 0xFFFFFFFF);
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

	SendPlayerDeathWindow();

}

void Client::HandlePlayerRevive(int32 point_id)
{
	PacketStruct* packet = configReader.getStruct("WS_ServerControlFlags", GetVersion());
	if(packet)
	{
		packet->setDataByName("parameter1", 8);
		QueuePacket(packet->serialize());
		packet->setDataByName("parameter1", 16);
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

	SimpleMessage(CHANNEL_COLOR_REVIVE, "You regain consciousness!");
	packet = configReader.getStruct("WS_Resurrected", GetVersion());
	if(packet)
	{
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

	float x, y, z, heading;
	RevivePoint* revive_point = 0;
	if(point_id != 0xFFFFFFFF)
		revive_point = GetCurrentZone()->GetRevivePoint(point_id);

	string zone_desc;
	const char* location_name = "Unknown";

	//revive at zone safe coords
	if(!revive_point)
	{
		LogWrite(CCLIENT__WARNING, 0, "Client", "No Revive Point! Spawning player at safe coordinates!");
		x = GetCurrentZone()->GetSafeX();
		y = GetCurrentZone()->GetSafeY();
		z = GetCurrentZone()->GetSafeZ();
		heading = GetCurrentZone()->GetSafeHeading();
		zone_desc = GetCurrentZone()->GetZoneDescription();
		location_name = "Zone Safe Point";
		Zone(GetCurrentZone()->GetZoneName(), false);
	}
	else
	{
		LogWrite(CCLIENT__DEBUG, 0, "Client", "Sending player to chosen Revive Point.");
		x = revive_point->x;
		y = revive_point->y;
		z = revive_point->z;
		heading = revive_point->heading;
		zone_desc = database.GetZoneDescription(revive_point->zone_id);
		location_name = revive_point->location_name.c_str();
		Zone(GetCurrentZone()->GetZoneName(), false);
	}

	player->SetResurrecting(true);
	player->SetX(x);
	player->SetY(y);
	player->SetZ(z);
	player->SetHeading(heading);
	player->SetHP(player->GetTotalHP());
	player->SetPower(1);

	LogWrite(CCLIENT__DEBUG, 0, "Client", "Attempt Revive @ %s, %.2f, %.2f, %.2f, %.2f, HP: %i, Pow: %i, %s", 
		zone_desc.c_str(),
		player->GetX(),
		player->GetY(),
		player->GetZ(),
		player->GetHeading(),
		player->GetHP(),
		player->GetPower(),
		location_name);

	//player->ClearEverything();
	Save();

	if(revive_point && revive_point->zone_id != GetCurrentZone()->GetZoneID() && revive_point->zone_id != 0)
	{
		string* zone_name = database.GetZoneName(revive_point->zone_id);
		if(!zone_name || zone_name->length() == 0)
		{
			LogWrite(CCLIENT__WARNING, 0, "Client", "Unable to zone player to revive zone ID '%u', using current zone's safe coords.", revive_point->zone_id);
			x = GetCurrentZone()->GetSafeX();
			y = GetCurrentZone()->GetSafeY();
			z = GetCurrentZone()->GetSafeZ();
			heading = GetCurrentZone()->GetSafeHeading();
			location_name = "Zone Safe Point";
		} 
		else 
		{
			LogWrite(CCLIENT__DEBUG, 0, "Client", "Sending player to revive zone ID '%u', using current zone's safe coords.", revive_point->zone_id);
			location_name = revive_point->location_name.c_str();
			player->ClearEverything();
			Zone(zone_name->c_str(), false);
		}
		safe_delete(zone_name);
	}

	zone_desc = GetCurrentZone()->GetZoneDescription();
	Message(CHANNEL_COLOR_REVIVE, "Reviving in %s at %s.", zone_desc.c_str(), location_name);
	player->SetSpawnType(4);
	packet = configReader.getStruct("WS_CancelMoveObjectMode", GetVersion());
	if(packet)
	{
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

	packet = configReader.getStruct("WS_TeleportWithinZone", GetVersion());
	if(packet)
	{
		packet->setDataByName("x", x);
		packet->setDataByName("y", y);
		packet->setDataByName("z", z);
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

	packet = configReader.getStruct("WS_SetControlGhost", GetVersion());
	if(packet)
	{
		packet->setDataByName("spawn_id", 0xFFFFFFFF);
		packet->setDataByName("unknown2", 255);
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

	packet = configReader.getStruct("WS_SetPOVGhostCmd", GetVersion());
	if(packet)
	{
		packet->setDataByName("spawn_id", 0xFFFFFFFF);
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

	GetCurrentZone()->RemoveSpawn(player, false);
	
	m_resurrect.writelock(__FUNCTION__, __LINE__);
	if(current_rez.active)
		current_rez.should_delete = true;
	m_resurrect.releasewritelock(__FUNCTION__, __LINE__);
}

void Client::SendCharInfo(){
	EQ2Packet* app;

	player->SetEquippedItemAppearances();

	ClientPacketFunctions::SendCharacterData ( this );

	SendCharPOVGhost();

	PacketStruct* packet = configReader.getStruct("WS_SetControlGhost",GetVersion());
	if(packet){
		packet->setDataByName("spawn_id", player->GetIDWithPlayerSpawn(player));
		packet->setDataByName("size", 0.56);
		packet->setDataByName("unknown2", 255);
		EQ2Packet* app = packet->serialize();
		QueuePacket(app);
		safe_delete(packet);
	}

	//SendAchievementsList();
	ClientPacketFunctions::SendCharacterSheet ( this );
	ClientPacketFunctions::SendTraitList(this);
	//ClientPacketFunctions::SendAbilities(this);
	master_aa_list.DisplayAA(this);
	ClientPacketFunctions::SendSkillBook(this);
	ClientPacketFunctions::SendLoginCommandMessages(this);

	GetCurrentZone()->AddSpawn(player);

	player->ClearProcs();

	if (!player->IsResurrecting()) {
		ClientPacketFunctions::SendUpdateSpellBook(this);
	} else {
		player->SetResurrecting(false);
	}

	//SendCollectionList();
	Guild* guild = player->GetGuild();
	if (guild)
		guild->GuildMemberLogin(this, firstlogin);

	app = player->item_list.serialize(GetPlayer(), GetVersion());
	if(app){
		LogWrite(CCLIENT__PACKET, 0, "Client", "Dump/Print Packet in func: %s, line: %i", __FUNCTION__, __LINE__);
		//DumpPacket(app);
		QueuePacket(app);
	}
	app = player->GetEquipmentList()->serialize(GetVersion());
	if(app){
		QueuePacket(app);
	}

	vector<Item*>* items = player->item_list.GetItemsFromBagID(-3); // bank items
	if(items && items->size() > 0){
		for(int32 i=0;i<items->size();i++){
			EQ2Packet* outapp = items->at(i)->serialize(GetVersion(), false, GetPlayer());

			LogWrite(CCLIENT__PACKET, 0, "Client", "Dump/Print Packet in func: %s, line: %i", __FUNCTION__, __LINE__);
			//DumpPacket(outapp);
			QueuePacket(outapp);
		}
	}

	if (firstlogin && (app = chat.GetWorldChannelList(this)) != NULL)
		QueuePacket(app);

	safe_delete(items);
	items = player->item_list.GetItemsFromBagID(-4); //shared bank items
	if(items && items->size() > 0){
		for(int32 i=0;i<items->size();i++)
			QueuePacket(items->at(i)->serialize(GetVersion(), false, GetPlayer()));
	}
	safe_delete(items);
	SendTitleUpdate();

	GetPlayer()->ChangePrimaryWeapon();
	GetPlayer()->ChangeSecondaryWeapon();
	GetPlayer()->ChangeRangedWeapon();
	database.LoadBuyBacks(this);

	string zone_motd = GetCurrentZone()->GetZoneMOTD();
	if (zone_motd.length() > 0 && zone_motd[0] != ' ') {
		string zone_motd_send = "Zone MOTD: " + zone_motd;
		SimpleMessage(CHANNEL_COLOR_WHITE, zone_motd_send.c_str());
	}
	const char* zone_script = world.GetZoneScript(GetCurrentZone()->GetZoneID());
	if (zone_script && lua_interface)
		lua_interface->RunZoneScript(zone_script, "player_entry", GetCurrentZone(), GetPlayer());
	this->client_zoning = false;
	if(player->GetHP() < player->GetTotalHP() || player->GetPower() < player->GetTotalPower())
		GetCurrentZone()->AddDamagedSpawn(player);

	if (firstlogin && rule_manager.GetGlobalRule(R_World, IRCGlobalEnabled)->GetBool())
		Message(CHANNEL_BROADCAST, "This server has a global IRC channel, join the world channel %s to use", rule_manager.GetGlobalRule(R_World, IRCChan)->GetString());

	if (firstlogin)
		firstlogin = false;

	items = player->GetEquippedItemList();
	if (items && items->size() > 0) {
		for(int32 i = 0; i < items->size(); i++) {
			Item* item = items->at(i);
			if (item && item->GetItemScript() && lua_interface)
				lua_interface->RunItemScript(item->GetItemScript(), "equipped", item, player);
		}
	}

	//Allow this player to change their last name if they meet the level requirement
	if (!player->get_character_flag(CF_ENABLE_CHANGE_LASTNAME) && player->GetLevel() >= rule_manager.GetGlobalRule(R_Player, MinLastNameLevel)->GetInt8())
		player->set_character_flag(CF_ENABLE_CHANGE_LASTNAME);

	safe_delete(items);

	if (!player->Alive())
		DisplayDeadWindow();
}

void Client::SendZoneSpawns(){
	//Allows us to place spawns almost anywhere
	uchar blah[] ={0x00,0x3C,0x1C,0x46,0x00,0x3C,0x1C,0x46,0x00,0x3C,0x1C,0x46};
	EQ2Packet* app = new EQ2Packet(OP_MoveableObjectPlacementCriteri, blah, sizeof(blah));
	QueuePacket(app);

	ClientPacketFunctions::SendSkillSlotMappings(this);
	ClientPacketFunctions::SendGameWorldTime ( this );
	GetCurrentZone()->StartZoneInitialSpawnThread(this);
}

void Client::SendCharPOVGhost(){
	PacketStruct* set_pov = configReader.getStruct("WS_SetPOVGhostCmd",GetVersion());
	if(set_pov){
		set_pov->setDataByName("spawn_id", player->GetIDWithPlayerSpawn(player));
		EQ2Packet* app_pov = set_pov->serialize();
		QueuePacket(app_pov);
		safe_delete(set_pov);
	}

}

void Client::SendZoneInfo(){
	ZoneServer* zone = GetCurrentZone();
	if(zone){
		EQ2Packet* packet = zone->GetZoneInfoPacket(this);
		QueuePacket(packet);
		PacketStruct* fog_packet = configReader.getStruct("WS_FogInit", GetVersion());

		LogWrite(CCLIENT__PACKET, 0, "Client", "Dump/Print Packet in func: %s, line: %i", __FUNCTION__, __LINE__);
#if EQDEBUG >= 9
		fog_packet->PrintPacket();
#endif

		if(fog_packet){
			database.LoadFogInit(zone->GetZoneFile(), fog_packet);
			QueuePacket(fog_packet->serialize());
			safe_delete(fog_packet);
		}

		zone->SendFlightPathsPackets(this);
	}
	/*
	uchar blah[] ={0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0x00,0x01,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF
	,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
	,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x10,0x49,0x2B,0x62,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00};
	EQ2Packet* appA = new EQ2Packet(OP_GuildUpdateMsg, blah, sizeof(blah));
	QueuePacket(appA);

	uchar blahA[] ={0x45,0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00
	,0x00,0x10,0xE2,0x10,0x6C,0x00,0x00,0x00,0x00};
	EQ2Packet* appB = new EQ2Packet(OP_KeymapDataMsg, blahA, sizeof(blahA));
	QueuePacket(appB);
	*/

	LogWrite(CCLIENT__DEBUG, 0, "Client", "SendFriendList");
	SendFriendList();
	LogWrite(CCLIENT__DEBUG, 0, "Client", "SendIgnoreList");
	SendIgnoreList();
}

void Client::SendDefaultGroupOptions(){
/*
0 - loot method
1 - loot items rarity
2 - Auto split coin
4 - default yell method
6 - group autolock
7 - solo autolock
*/
	uchar blah7[] = {0x01,0x01,0x01,0x01,0x00,0x00,0x00};
	EQ2Packet* app7 = new EQ2Packet(OP_DefaultGroupOptionsMsg, blah7, sizeof(blah7));
	QueuePacket(app7);

}

bool Client::HandlePacket(EQApplicationPacket *app) {
	bool ret = true;

	LogWrite(PACKET__DEBUG, 9, "Packet", "[EQDEBUG] Received Packet:");
#if EQDEBUG >= 9
	DumpPacket(app, true);
#endif

	EmuOpcode opcode = app->GetOpcode();

#if EQDEBUG >= 9
	const char* name = app->GetOpcodeName();
	if(name)
	cout << name;
	else
	cout << "Unknown";
	cout << " Packet: OPCode: 0x" << hex << setw(2) << setfill('0') << app->GetOpcode() << dec << ", size: " << setw(5) << setfill(' ') << app->Size() << endl;
	DumpPacket(app);
#endif

		switch(opcode){
		case OP_LoginByNumRequestMsg:{
			LogWrite(OPCODE__DEBUG, 0, "Opcode", "Opcode 0x%X (%i): OP_LoginByNumRequestMsg", opcode, opcode);
			
			PacketStruct* request;
			request = configReader.getStruct("LoginByNumRequest", 1);
			if (request && request->LoadPacketData(app->pBuffer,app->size)) {
				// test the original location of Version for clients older than 1212
				version = request->getType_int16_ByName("version");

				if (EQOpcodeManager.count(GetOpcodeVersion(version)) == 0) {
					// must be new client data version method, re-fetch the packet
					safe_delete(request);
					request = configReader.getStruct("LoginByNumRequest", 1212);

					if (request && request->LoadPacketData(app->pBuffer, app->size)) {
						// Xinux suggests using an INT16 here. Our first new version = 57000
						version = request->getType_int16_ByName("version");
					}
					else {
						LogWrite(LOGIN__ERROR, 0, "Login", "Nasty Horrible things happening. Tell a dev asap! Version: %i", version);
						break;
					}
				}

				if (EQOpcodeManager.count(GetOpcodeVersion(version)) == 0) {
					LogWrite(WORLD__ERROR, 0, "World", "Incompatible version: %i", version);
					ClientPacketFunctions::SendLoginDenied( this );
					return false;
				}

				int32 account_id = request->getType_int32_ByName("account_id");
				int32 access_code = request->getType_int32_ByName("access_code");

				ZoneAuthRequest* zar = zone_auth.GetAuth(account_id, access_code);

				if(zar)
				{
					firstlogin = zar->isFirstLogin ( );
					LogWrite(ZONE__INFO, 0, "ZoneAuth", "Access Key: %u, Character Name: %s, Account ID: %u", zar->GetAccessKey(), zar->GetCharacterName(), zar->GetAccountID());
					if(database.loadCharacter(zar->GetCharacterName(), zar->GetAccountID(), this)){
						version = request->getType_int16_ByName("version");
						MDeletePlayer.writelock(__FUNCTION__, __LINE__);
						Client* client = zone_list.GetClientByCharName(player->GetName());
						if(client){
							if(client->getConnection())
								client->getConnection()->SendDisconnect();

							if(client->GetCurrentZone() && !client->IsZoning()){
								//swap players, allowing the client to resume his LD'd player (ONLY if same version of the client)
								if(client->GetVersion() == version){
									Player* current_player = GetPlayer();
									SetPlayer(client->GetPlayer());
									client->SetPlayer(current_player);
									client->GetPlayer()->SetPendingDeletion(true);

									GetPlayer()->SetPendingDeletion(false);
									GetPlayer()->ResetSavedSpawns();
									GetPlayer()->SetReturningFromLD(true);
									should_load_spells = false;
									GetPlayer()->GetZone()->RemoveDelayedSpawnRemove(GetPlayer());
								}
								client->GetCurrentZone()->RemoveClientImmediately(client);
							}

							client->GetPlayer()->SetResurrecting(true);

							GroupMemberInfo* info = 0;
							if (info = client->GetPlayer()->GetGroupMemberInfo()) {
								info->client = this;
								info->member = GetPlayer();

								client->GetPlayer()->SetGroupMemberInfo(0);
								GetPlayer()->SetGroupMemberInfo(info);

								GetPlayer()->UpdateGroupMemberInfo();
								world.GetGroupManager()->SendGroupUpdate(GetPlayer()->GetGroupMemberInfo()->group_id, this);
							}
						}
						MDeletePlayer.releasewritelock(__FUNCTION__, __LINE__);						
						if(!GetCurrentZone()){
							LogWrite(ZONE__ERROR, 0, "Zone", "Error loading zone for character: %s", player->GetName());
							ClientPacketFunctions::SendLoginDenied( this );
						}
						else if(EQOpcodeManager.count(GetOpcodeVersion(version)) > 0 && getConnection()){
							getConnection()->SetClientVersion(version);
							connected_to_zone = true;
							client_list.Remove(this); //remove from master client list
							new_client_login = true;
							GetCurrentZone()->AddClient(this); //add to zones client list
							zone_list.AddClientToMap(player->GetName(), this);
						}
						else{
							LogWrite(WORLD__ERROR, 0, "World", "Incompatible version: %i", version);
							ClientPacketFunctions::SendLoginDenied( this );

													return false;
						}
					}
					else{
						LogWrite(WORLD__ERROR, 0, "World", "Could not load character '%s' with account id of: %u", zar->GetCharacterName(), zar->GetAccountID());
						ClientPacketFunctions::SendLoginDenied( this );
					}
					zone_auth.RemoveAuth(zar);
				}
				else
				{
					LogWrite(WORLD__ERROR, 0, "World", "Invalid ZoneAuthRequest, disconnecting client.");
					Disconnect();
				}
			}
			safe_delete(request);
			break;
		}
		case OP_MapRequest:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_MapRequest", opcode, opcode);
			PacketStruct* packet = configReader.getStruct("WS_MapRequest", GetVersion());
			if(packet && app->size > 2 && GetCurrentZone()){
				packet->LoadPacketData(app->pBuffer, app->size);
				PacketStruct* fog_packet = configReader.getStruct("WS_FogInit", GetVersion());
				if(fog_packet){
					LogWrite(PACKET__DEBUG, 0, "Packet", "In OP_MapRequest: Fog Packet");
					database.LoadFogInit(packet->getType_EQ2_16BitString_ByName("zone").data, fog_packet);
					fog_packet->setDataByName("unknown1", 1);
					fog_packet->setDataByName("unknown3", 1);

					LogWrite(CCLIENT__PACKET, 0, "Client", "Dump/Print Packet in func: %s, line: %i", __FUNCTION__, __LINE__);
					//fog_packet->PrintPacket();
					QueuePacket(fog_packet->serialize());
					safe_delete(fog_packet);
				}
				safe_delete(packet);
			}
			break;
		}
		case OP_RequestCampMsg:
			{
				LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_RequestCampMsg", opcode, opcode);

				PacketStruct* request = configReader.getStruct("WS_RequestCamp",GetVersion());
				if (request && request->LoadPacketData(app->pBuffer,app->size))
				{
					LogWrite(CCLIENT__DEBUG, 0, "CClient", "Client '%s' (%u) is camping...", GetPlayer()->GetName(), GetPlayer()->GetCharacterID());
					LogWrite(CCLIENT__DEBUG, 0, "CClient", "WS_RequestCamp - quit: %i, camp_desktop: %i, camp_char_select: %i, (to) char_name: %s", 
						request->getType_int8_ByName("quit"),
						request->getType_int8_ByName("camp_desktop"),
						request->getType_int16_ByName("camp_char_select"),
						( request->getType_EQ2_16BitString_ByName("char_name").data.length() > 0 ) ? request->getType_EQ2_16BitString_ByName("char_name").data.c_str() : "");

					//DumpPacket(app->pBuffer, app->size);
					//request->PrintPacket();

					if (!camp_timer)
					{
						int16 camp_time = 20; // default if rule cannot be found
						if(GetAdminStatus() >= 100)
							camp_time = rule_manager.GetGlobalRule(R_World, GMCampTimer)->GetInt16();
						else
							camp_time = rule_manager.GetGlobalRule(R_World, PlayerCampTimer)->GetInt16();

						PacketStruct* response = configReader.getStruct("WS_Camp",GetVersion());
						if(response)
						{
							bool disconnect = false;
							if(request->getType_int8_ByName("camp_desktop") == 1 && request->getType_int8_ByName("quit") == 1)
							{
								// Command: /camp desktop
								// Command: /quit
								response->setDataByName("camp_desktop", 1);
								disconnect = true;
							}
							else
							{
								// Command: /camp 
								response->setDataByName("camp_desktop", request->getType_int8_ByName("camp_desktop"));
								response->setDataByName("camp_char_select", request->getType_int16_ByName("camp_char_select"));
								response->setDataByName("seconds", camp_time);
							}

							camp_timer = new Timer(camp_time * 1000);
							camp_timer->Enable();

							if(request->getType_EQ2_16BitString_ByName("char_name").data.length() > 0)
							{
								// /camp {char_name}
								response->setDataByName("char_name", request->getType_EQ2_16BitString_ByName("char_name").data.c_str());
							}
							else if(request->getType_int8_ByName("camp_desktop") == 0 && request->getType_int16_ByName("camp_char_select") == 0)
							{ 
								// /camp  (go back to char selection screen)
								response->setDataByName("char_name", " ");
								response->setDataByName("camp_char_select", 1);
							}

							LogWrite(CCLIENT__DEBUG, 0, "CClient", "WS_Camp - seconds: %i, camp_desktop: %i, camp_char_select: %i, (to) char_name: %s", 
								response->getType_int8_ByName("seconds"),
								response->getType_int8_ByName("camp_desktop"),
								response->getType_int8_ByName("camp_char_select"),
								( response->getType_EQ2_16BitString_ByName("char_name").data.length() > 0 ) ? response->getType_EQ2_16BitString_ByName("char_name").data.c_str() : "");

							// JA: trying to recognize /camp vs LD (see ZoneServer::ClientProcess())
							if( (player->GetActivityStatus() & ACTIVITY_STATUS_CAMPING) == 0 )
								player->SetActivityStatus(player->GetActivityStatus() + ACTIVITY_STATUS_CAMPING);
							//response->PrintPacket();
							QueuePacket(response->serialize());
							safe_delete(response);
							if(disconnect)
								Disconnect();
						}
					}
				}
				safe_delete(request);
				break;
			}
		case OP_StoodMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_StoodMsg", opcode, opcode);
			if(camp_timer)
			{
				// JA: clear camping flag
				if( (player->GetActivityStatus() & ACTIVITY_STATUS_CAMPING) > 0 )
					player->SetActivityStatus(player->GetActivityStatus() - ACTIVITY_STATUS_CAMPING);
				safe_delete(camp_timer);
				EQ2Packet* outapp = new EQ2Packet(OP_CampAbortedMsg, 0, 0);
				QueuePacket(outapp);
			}
			player->SetTempVisualState(0);
			break;
						 }
		case OP_StandMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_StandMsg", opcode, opcode);
			if(camp_timer)
			{
				// JA: clear camping flag
				if( (player->GetActivityStatus() & ACTIVITY_STATUS_CAMPING) > 0 )
					player->SetActivityStatus(player->GetActivityStatus() - ACTIVITY_STATUS_CAMPING);
				safe_delete(camp_timer);
				EQ2Packet* outapp = new EQ2Packet(OP_CampAbortedMsg, 0, 0);
				QueuePacket(outapp);
			}
			player->SetTempVisualState(539);
			break;
						 } 
		case OP_SitMsg: {
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_SitMsg", opcode, opcode);
			player->SetTempVisualState(538);
			break;
						}
		case OP_SatMsg: {
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_SatMsg", opcode, opcode);
			player->SetTempVisualState(540);
			break;
						}
		case OP_QuestJournalOpenMsg:
		case OP_QuestJournalInspectMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_QuestJournalOpenMsg, OP_QuestJournalInspectMsg", opcode, opcode);
			if(app->size < sizeof(int32))
				break;
			int32 quest_id = 0;
			memcpy(&quest_id, app->pBuffer, sizeof(int32));
			Quest* quest = GetActiveQuest(quest_id);
			if(quest)
				QueuePacket(quest->QuestJournalReply(GetVersion(), GetNameCRC(), player));
			else{
				quest = player->GetCompletedQuest(quest_id);
				if(quest)
					QueuePacket(quest->QuestJournalReply(GetVersion(), GetNameCRC(), player, 0, 1, true));
			}
			break;
									   }
		case OP_QuestJournalSetVisibleMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_QuestJournalSetVisibleMsg", opcode, opcode);
			PacketStruct* packet = configReader.getStruct("WS_QuestJournalVisible", GetVersion());
			if (packet && packet->LoadPacketData(app->pBuffer, app->size)) {
				int32 quest_id = packet->getType_int32_ByName("quest_id");
				bool hidden = packet->getType_int8_ByName("visible") == 1 ? false : true;
				map<int32, Quest*>* player_quests = player->GetPlayerQuests();
				if (player_quests) {
					if (player_quests->count(quest_id) > 0)
						player_quests->at(quest_id)->SetHidden(hidden);
					else
						LogWrite(CCLIENT__ERROR, 0, "Client", "OP_QuestJournalSetVisibleMsg error: Player does not have quest with id of %u", quest_id);
				}
				else
					LogWrite(CCLIENT__ERROR, 0, "Client", "OP_QuestJournalSetVisibleMsg error: Unable to get player(%s) quests", player->GetName());

				safe_delete(packet);
			}
			break;
										  }
		case OP_MacroUpdateMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_MacroUpdateMsg", opcode, opcode);
			PacketStruct* macro_update=configReader.getStruct("WS_MacroUpdate",GetVersion());
			if(macro_update && macro_update->LoadPacketData(app->pBuffer, app->size)){
				vector<string>* update = new vector<string>;
				int8 number = macro_update->getType_int8_ByName("number");
				int16 icon = macro_update->getType_int16_ByName("icon");
				string name = macro_update->getType_EQ2_8BitString_ByName("name").data;
				int8 count = macro_update->getType_int8_ByName("macro_count");
				for(int8 i=0;i<count;i++){
					char tmp_command[15] = {0};
					sprintf(tmp_command, "command_%i", i);
					update->push_back(macro_update->getType_EQ2_16BitString_ByName(tmp_command).data);
				}
				if(name.length() == 0)
					database.UpdateCharacterMacro(GetCharacterID(), number, 0, icon, update);
				else
					database.UpdateCharacterMacro(GetCharacterID(), number, name.c_str(), icon, update);
				safe_delete(update);
				safe_delete(macro_update);
			}
			break;
							   }
		case OP_DialogSelectMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_DialogSelectMsg", opcode, opcode);
			PacketStruct* packet = configReader.getStruct("WS_DialogSelect", GetVersion());
			if(packet){
				packet->LoadPacketData(app->pBuffer, app->size);
				int32 conversation_id = packet->getType_int32_ByName("conversation_id");
				int32 response_index = packet->getType_int32_ByName("response");
				if(GetCurrentZone()){
					Spawn* spawn = conversation_spawns[conversation_id];
					Item* item = conversation_items[conversation_id];
					if(conversation_map.count(conversation_id) > 0 && conversation_map[conversation_id].count(response_index) > 0){					
						if(spawn)
							GetCurrentZone()->CallSpawnScript(spawn, SPAWN_SCRIPT_CONVERSATION, player, conversation_map[conversation_id][response_index].c_str());
						else if(item && lua_interface && item->GetItemScript())
							lua_interface->RunItemScript(item->GetItemScript(), conversation_map[conversation_id][response_index].c_str(), item, player);
					}
				}
				safe_delete(packet);
			}
			break;
								}
		case OP_PositionMoveableObject:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_PositionMoveableObject", opcode, opcode);
			PacketStruct* place_object=configReader.getStruct("WS_PlaceMoveableObject",GetVersion());
			if(place_object && place_object->LoadPacketData(app->pBuffer, app->size)){
				Spawn* spawn = GetPlayer()->GetSpawnWithPlayerID(place_object->getType_int32_ByName("spawn_id"));
				if(!spawn){
					SimpleMessage(CHANNEL_COLOR_YELLOW, "Unable to find spawn.");
					break;
				}
				spawn->SetX(place_object->getType_float_ByName("x"));
				spawn->SetY(place_object->getType_float_ByName("y"));
				spawn->SetZ(place_object->getType_float_ByName("z"));
				spawn->SetHeading(place_object->getType_float_ByName("heading") + 180);
				spawn->SetSpawnOrigX(spawn->GetX());
				spawn->SetSpawnOrigY(spawn->GetY());
				spawn->SetSpawnOrigZ(spawn->GetZ());
				spawn->SetSpawnOrigHeading(spawn->GetHeading());

				if(spawn->GetSpawnLocationID() > 0 && database.UpdateSpawnLocationSpawns(spawn))
					SimpleMessage(CHANNEL_COLOR_YELLOW, "Successfully saved spawn information.");
				else if(spawn->GetSpawnLocationID() > 0)
					SimpleMessage(CHANNEL_COLOR_YELLOW, "Error saving spawn information, see console window for details.");
				safe_delete(place_object);
			}
			break;
									   }
		case OP_CampAbortedMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_CampAbortedMsg", opcode, opcode);
			if(camp_timer)
			{
				// JA: clear camping flag
				if( (player->GetActivityStatus() & ACTIVITY_STATUS_CAMPING) > 0 )
					player->SetActivityStatus(player->GetActivityStatus() - ACTIVITY_STATUS_CAMPING);
				safe_delete(camp_timer);
				EQ2Packet* outapp = new EQ2Packet(OP_CampAbortedMsg, 0, 0);
				QueuePacket(outapp);
			}
			break;
							   }
		case OP_DoneLoadingZoneResourcesMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_DoneLoadingZoneResourcesMsg", opcode, opcode);
			SendZoneSpawns();
			break;
											}
		case OP_DefaultGroupOptionsRequestMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_DefaultGroupOptionsRequestMsg", opcode, opcode);
			SendDefaultGroupOptions();
			break;
											  }
		case OP_DoneLoadingEntityResourcesMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_DoneLoadingEntityResourcesMsg", opcode, opcode);
			if(!IsReadyForSpawns())
				SetReadyForSpawns(true);
			SendCharInfo();
			pos_update.Start();
			spawn_vis_update.Start(1000);
			quest_pos_timer.Start();
			break;
											  }
		case OP_LootItemsRequestMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_LootItemsRequestMsg", opcode, opcode);
			HandleLoot(app);
			break;
									}
		case OP_KnowledgeWindowSlotMappingMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_KnowledgeWindowSlotMappingMsg", opcode, opcode);
			PacketStruct* packet = configReader.getStruct("WS_SpellSlotMapping", GetVersion());
			if(packet){
				if(packet->LoadPacketData(app->pBuffer, app->size)){
					int num_updates = packet->getType_int16_ByName("spell_count");
					int16 slot_id = 0;
					char tmp_spell_id[15];
					char tmp_slot[15];
					for(int i=0;i<num_updates;i++){
						memset(tmp_spell_id, 0, 15);
						memset(tmp_slot, 0, 15);
						sprintf(tmp_spell_id, "spell_id_%i", i);
						sprintf(tmp_slot, "slot_id_%i", i);
						int32 spell_id = packet->getType_int32_ByName(tmp_spell_id);
						if(spell_id > 0){
							slot_id = packet->getType_int16_ByName(tmp_slot);
							SpellBookEntry* spell = player->GetSpellBookSpell(spell_id);
							if(spell && spell->slot != slot_id){
								spell->slot = slot_id;
								spell->save_needed = true;
							}
						}
					}
				}
				safe_delete(packet);
			}
			//SendKnowledgeWindowSlot();
			break;
		}
		case OP_ReadyToZoneMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_ReadyToZoneMsg", opcode, opcode);
			if(client_zoning)
				LogWrite(WORLD__INFO, 0, "World", "OP_ReadyToZone: Player %s zoning to %s", player->GetName(), GetCurrentZone()->GetZoneName());
			else
				LogWrite(WORLD__ERROR, 0, "World", "OP_ReadyToZone: Player %s attempting to zone without server authorization.", player->GetName());
			Disconnect();
			break;
							   }
		case OP_ClientFellMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_ClientFellMsg (ouch!)", opcode, opcode);
			PacketStruct* request = configReader.getStruct("WS_ClientFell", GetVersion());
			if(request && request->LoadPacketData(app->pBuffer, app->size)){
				float height = request->getType_float_ByName("height");
				/*int32 spawn_id = request->getType_int32_ByName("spawn_id");
				if(GetPlayer()->GetSpawnWithPlayerID(spawn_id) != GetPlayer()){
				cout << "Error: " << GetPlayer()->GetName() << " called ClientFell with an invalid ID of: " << spawn_id << endl;
				break;
				}*/
				float safe_height = 13.0f;
				Skill* skill = GetPlayer()->GetSkillByName("Safe Fall", true);
				if(skill)
					safe_height += (skill->current_val + 1)/5;

				if (height > safe_height) {
					int16 damage = (int16)ceil((height-safe_height)*25);
					if(height >= 80)
						damage = 30000;
					//cout << "Detected fall height:" << height << " damage:" << damage << endl;
					if (damage > 0){
						GetPlayer()->TakeDamage(damage);
						if (GetPlayer()->GetPlayerStatisticValue(STAT_PLAYER_HIGHEST_FALLING_HIT) < damage)
							GetPlayer()->UpdatePlayerStatistic(STAT_PLAYER_HIGHEST_FALLING_HIT, damage, true);
						if(!GetPlayer()->GetInvulnerable())
							GetPlayer()->SetCharSheetChanged(true);
						GetCurrentZone()->SendDamagePacket(0, GetPlayer(), DAMAGE_PACKET_TYPE_SIMPLE_DAMAGE, GetPlayer()->GetInvulnerable() ? DAMAGE_PACKET_RESULT_INVULNERABLE : DAMAGE_PACKET_RESULT_SUCCESSFUL, DAMAGE_PACKET_DAMAGE_TYPE_FALLING, damage, 0);
						if(GetPlayer()->GetHP() == 0) {
							GetCurrentZone()->KillSpawn(GetPlayer(), 0);
						}
					}
				}
				safe_delete(request);
			}
			break;
							  }
		case OP_MapFogDataUpdateMsg:{

			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_MapFogDataUpdateMsg", opcode, opcode);
			LogWrite(MISC__TODO, 3, "TODO", "Handle (OP_MapFogDataUpdateMsg), ignoring it for now\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
			break;
		}
		case OP_SelectZoneTeleporterDestinatio:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_SelectZoneTeleporterDestinatio", opcode, opcode);
			ProcessTeleportLocation(app);
			break;
		}
		case OP_SendLatestRequestMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_SendLatestRequestMsg", opcode, opcode);
			uchar blah25[] = {0x01};
			EQ2Packet* app25 = new EQ2Packet(OP_ClearDataMsg, blah25, sizeof(blah25));
			QueuePacket(app25);
			break;
									 }
		case OP_ShowCreateFromRecipeUIMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_ShowCreateFromRecipeUIMsg", opcode, opcode);
			break;
			/*uchar blah[] ={0x09,0x0e,0x00,0x51,0x75,0x65,0x65,0x6e,0x27,0x73,0x20,0x43,0x6f,0x6c,0x6f,0x6e
			,0x79,0x00,0x00,0x00,0x00,0x40,0x40,0xff,0xff,0xff};
			EQ2Packet* app = new EQ2Packet(OP_EncounterBrokenMsg, blah, sizeof(blah));
			QueuePacket(app);

			uchar blah2[] = {0x00,0x00,0xff,0xff,0xff,0xff};
			app = new EQ2Packet(OP_CreateCharFromCBBRequestMsg, blah2, sizeof(blah2));
			QueuePacket(app);

			uchar blah3[] ={0x09,0x17,0x00,0x5c,0x23,0x46,0x46,0x45,0x34,0x30,0x30,0x20,0x51,0x75,0x65,0x65
			,0x6e,0x27,0x73,0x20,0x43,0x6f,0x6c,0x6f,0x6e,0x79,0x00,0x00,0x00,0x00,0xa0,0x40
			,0xff,0xff,0xff};
			app = new EQ2Packet(OP_CreateCharFromCBBRequestMsg, blah3, sizeof(blah3));
			QueuePacket(app);

			uchar blah4[] ={0x0b,0x00,0x21,0x00,0x00,0x00,0x1d,0x81,0x42,0x17,0x81,0x42,0x17,0x81,0x42,0x17
			,0x81,0x42,0x17,0x81,0x42,0x17,0x81,0x42,0x17,0x81,0x42,0x17,0x81,0x42,0x17,0x81
			,0x42,0x17,0x81,0x42,0x17,0x81,0x42};
			app = new EQ2Packet(OP_UpdateSpellBookMsg, blah4, sizeof(blah4));
			QueuePacket(app);
			uchar blah5[] ={0x00,0x00};
			app = new EQ2Packet(OP_RecipeDetailsMsg, blah5, sizeof(blah5));
			QueuePacket(app);
			break;*/
			//player->GetPlayerInfo()->GetInfo()->cur_power = 100;
			//EQ2Packet* app = player->GetPlayerInfo()->serialize(1);
			//QueuePacket(app);
										  }
		case OP_BeginItemCreationMsg: {
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_BeginItemCreationMsg", opcode, opcode);
			//DumpPacket(app->pBuffer, app->size);
			PacketStruct* packet = configReader.getStruct("WS_BeginItemCreation", GetVersion());
			if (packet) {
				packet->LoadPacketData(app->pBuffer, app->size);
				int32 item = 0;
				vector<int32> items;
				char tmp_item_id[20];
				item = packet->getType_int32_ByName("primary_component_id");
				if (item > 0)
					items.push_back(item);
				item = 0;
				int8 build_components = packet->getType_int8_ByName("num_build_components");
				for (int8 i = 0; i < build_components; i++) {
					memset(tmp_item_id, 0, 20);
					sprintf(tmp_item_id, "component_id_%i", i);
					item = packet->getType_int32_ByName(tmp_item_id);
					if (item > 0)
						items.push_back(item);

					item = 0;
				}

				item = packet->getType_int32_ByName("fuel_id");
				if (item > 0)
					items.push_back(item);

				GetCurrentZone()->GetTradeskillMgr()->BeginCrafting(this, items);

				safe_delete(packet);
			}
			break;
									  }
		case OP_StopItemCreationMsg: {
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_StopItemCreationMsg", opcode, opcode);
			DumpPacket(app->pBuffer, app->size);
			GetCurrentZone()->GetTradeskillMgr()->StopCrafting(this);
			break;
										  }
		//case OP_SignalMsg:{
		//	LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_SignalMsg", opcode, opcode);
		//	//if(app->size == 25 && app->pBuffer[0] == 0x17 && app->pBuffer[24] == 0x79)
		//	//	GetPlayer()->ModifySpellStatus(0, 66, true);
		//	if(connected && player->GetHP() == 0)
		//		DisplayDeadWindow();
		//	break;
		//				  }
		case OP_EntityVerbsRequestMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_EntityVerbsRequestMsg", opcode, opcode);
			HandleVerbRequest(app);
			break;
									  }
		case OP_EntityVerbsVerbMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_EntityVerbsVerbMsg", opcode, opcode);
			PacketStruct* packet = configReader.getStruct("WS_EntityVerbsVerb", GetVersion());
			if(packet){
				packet->LoadPacketData(app->pBuffer, app->size);
				int32 spawn_id = packet->getType_int32_ByName("spawn_id");
				player->SetTarget(player->GetSpawnWithPlayerID(spawn_id));
				Spawn* spawn = player->GetTarget();
				if (spawn && !spawn->IsNPC() && !spawn->IsPlayer()){
					string command = packet->getType_EQ2_16BitString_ByName("command").data;
					if (EntityCommandPrecheck(spawn, command.c_str())){
						if (spawn->IsGroundSpawn())
							((GroundSpawn*)spawn)->HandleUse(this, command);
						else if (spawn->IsObject())
							((Object*)spawn)->HandleUse(this, command);
						else if (spawn->IsWidget())
							((Widget*)spawn)->HandleUse(this, command);
						else if (spawn->IsSign())
							((Sign*)spawn)->HandleUse(this, command);
					}
				}
				else{
					EQ2_16BitString command = packet->getType_EQ2_16BitString_ByName("command");
					if(command.size > 0){
						string command_name = command.data;
						if(command_name.find(" ") < 0xFFFFFFFF)
							command_name = command_name.substr(0, command_name.find(" "));
						int32 handler = commands.GetCommandHandler(command_name.c_str());
						if(handler != 0xFFFFFFFF){
							if(command.data == command_name){
								command.data = "";
								command.size = 0;
							}
							else{
								command.data = command.data.substr(command.data.find(" ")+1);
								command.size = command.data.length();
							}
							commands.Process(handler, &command, this);
						}
						else{
							if(spawn && spawn->IsNPC()){
								if (EntityCommandPrecheck(spawn, command.data.c_str())){
									if (!((NPC*)spawn)->HandleUse(this, command.data)){
										LogWrite(WORLD__ERROR, 0, "World", "Unhandled command in OP_EntityVerbsVerbMsg: %s", command.data.c_str());
									}
								}
							}
							else
								LogWrite(WORLD__ERROR, 0, "World", "Unknown command in OP_EntityVerbsVerbMsg: %s", command.data.c_str());
						}
					}
				}
				safe_delete(packet);
			}
			break;
								   }
		case OP_SkillInfoRequest:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_SkillInfoRequest", opcode, opcode);
			HandleSkillInfoRequest(app);
			break;
								 }
		case OP_UpdateTargetMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_UpdateTargetMsg", opcode, opcode);
			int16 index = 0;
			memcpy(&index, app->pBuffer, sizeof(int16));
			if (index == 0xFFFF)
				GetPlayer()->SetTarget(0);
			else
				GetPlayer()->SetTarget(GetPlayer()->GetSpawnByIndex(index));
			if(GetPlayer()->GetTarget())
				GetCurrentZone()->CallSpawnScript(GetPlayer()->GetTarget(), SPAWN_SCRIPT_TARGETED, GetPlayer());
			//player->SetTarget((int16*)app->pBuffer);
			break;
								}
		case OP_ExamineInfoRequestMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_ExamineInfoRequestMsg", opcode, opcode);
			HandleExamineInfoRequest(app);
			break;
									  }
		case OP_QuickbarUpdateMsg:
		//case OP_QuickbarAddMsg:
			{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_QuickbarUpdateMsg, OP_QuickbarAddMsg", opcode, opcode);
			HandleQuickbarUpdateRequest(app);
			break;
							   }
		case OP_PredictionUpdateMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_PredictionUpdateMsg", opcode, opcode);
			EQ2Packet* app = new EQ2Packet(OP_PredictionUpdateMsg, 0, 0);
			QueuePacket(app);
			break;
		}
		case OP_RemoteCmdMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_RemoteCmdMsg", opcode, opcode);
			if(app->size > 0){
				EQ2_CommandString remote(app->pBuffer, app->size);

				LogWrite(PACKET__DEBUG, 1, "Packet", "RemoteCmdMsg Packet dump:");
#if EQDEBUG >= 9
				DumpPacket(app);
#endif
				commands.Process(remote.handler, &remote.command, this);
			}
			else //bad client, disconnect
				Disconnect();
			break;
							 }
		case OP_CancelSpellCast:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_CancelSpellCast", opcode, opcode);
			current_zone->Interrupted(player, 0, 0, true);
			SimpleMessage(CHANNEL_COLOR_SPELL_INTERRUPT, "You stop casting.");
			break;
								}
		case OP_UpdatePositionMsg:{
			LogWrite(OPCODE__DEBUG, 7, "Opcode", "Opcode 0x%X (%i): OP_UpdatePositionMsg", opcode, opcode); 
			int8 offset = 13;
			if(app->pBuffer[0]==0xFF)
				offset +=2;
			if (app->size>offset) {
				if(player->IsCasting()){
					float distance = 0;
					float x = player->GetX();
					float y = player->GetY();
					float z = player->GetZ();
					player->PrepareIncomingMovementPacket(app->size - offset, app->pBuffer + offset, version);
					distance = player->GetDistance(x, y, z, false);
					if(distance > .5)
						current_zone->Interrupted(player, 0, SPELL_ERROR_INTERRUPTED, false, true);
				}
				else
					player->PrepareIncomingMovementPacket(app->size - offset, app->pBuffer + offset, version);
				player_pos_changed = true;
				LogWrite(CCLIENT__PACKET, 0, "Client", "Dump/Print Packet in func: %s, line: %i", __FUNCTION__, __LINE__);
				//DumpPacket(app);
			}
			break;
								  }
		case OP_MailSendMessageMsg: {
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_MailSendMessageMsg", opcode, opcode);
			HandleSentMail(app);
			break;
									}
		case OP_StopTrackingMsg: {
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_StopTrackingMsg", opcode, opcode);
			player->GetZone()->RemovePlayerTracking(player, TRACKING_STOP);
			break;
								 }
		case OP_BeginTrackingMsg: {
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_BeginTrackingMsg", opcode, opcode);
			PacketStruct* packet = configReader.getStruct("WS_BeginTracking", GetVersion());
			if (packet) {
				packet->LoadPacketData(app->pBuffer, app->size);
				int32 spawn_id = packet->getType_int32_ByName("spawn_id");
				Spawn* spawn = player->GetSpawnWithPlayerID(spawn_id);
				if (spawn) {
					AddWaypoint(spawn->GetName(), WAYPOINT_CATEGORY_TRACKING, spawn_id);
					BeginWaypoint(spawn->GetName(), spawn->GetX(), spawn->GetY(), spawn->GetZ());
					player->GetZone()->RemovePlayerTracking(player, TRACKING_CLOSE_WINDOW);
				}
				safe_delete(packet);
			}
			break;
								  }
		case OP_BioUpdateMsg: {
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_BioUpdateMsg", opcode, opcode);
			PacketStruct* packet = configReader.getStruct("WS_BioUpdate", GetVersion());
			if (packet) {
				packet->LoadPacketData(app->pBuffer, app->size);
				player->SetBiography(packet->getType_EQ2_16BitString_ByName("biography").data);
				safe_delete(packet);
			}
			break;
							  }
		case OP_RewardPackMsg: {
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_RewardPackMsg", opcode, opcode);

			/* This logging is still here because I remember another system using this packet and just want to make sure we can figure out that it's being sent
			   when we come across it (scatman) */
			const char* name = app->GetOpcodeName();
			if(name)
				LogWrite(WORLD__DEBUG, 0, "World", "%s Received OP_RewardPackMsg %04i", name, app->GetRawOpcode());
			else
				LogWrite(WORLD__DEBUG, 0, "World", "Received OP_RewardPackMsg %04i", app->GetRawOpcode());
			//DumpPacket(app);
			PacketStruct* packet = configReader.getStruct("WS_RewardPackMsg", GetVersion());
			if (packet) {
				packet->LoadPacketData(app->pBuffer, app->size);
				string recruiter_name = packet->getType_EQ2_16BitString_ByName("recruiter_name").data;

				/* Player has contacted a guild recruiter */
				if (recruiter_name.length() > 0) {
					Guild* guild = guild_list.GetGuild(packet->getType_int32_ByName("guild_id"));
					Client* recruiter = zone_list.GetClientByCharName(recruiter_name);
					if (recruiter && guild) {
						Message(CHANNEL_COLOR_GUILD_EVENT, "Contact request sent to %s of %s.", recruiter->GetPlayer()->GetName(), guild->GetName());
						recruiter->Message(CHANNEL_COLOR_GUILD_EVENT, "%s [%u %s], [0 Unskilled] (%s) is requesting to speak to YOU about joining the guild.", player->GetName(), player->GetLevel(), classes.GetClassNameCase(player->GetAdventureClass()).c_str(), races.GetRaceNameCase(player->GetRace()));
						recruiter->PlaySound("ui_guild_page");
					}
				}
				/* New picture taken for guild recruiting */
				else {
					//DumpPacket(app->pBuffer, app->size);
					int32 guild_id = 0;
					int16 picture_data_size = 0;
					unsigned char* recruiter_picture_data = 0;
					memcpy(&guild_id, app->pBuffer + 4, sizeof(int32));
					memcpy(&picture_data_size, app->pBuffer + 15, sizeof(int16));
					Guild* guild = guild_list.GetGuild(guild_id);
					if (guild) {
						GuildMember* gm = guild->GetGuildMember(player);
						if (gm) {
							if (gm->recruiter_picture_data)
								safe_delete_array(gm->recruiter_picture_data);
							recruiter_picture_data = new unsigned char[picture_data_size];
							for (int16 i = 0; i < picture_data_size; i++)
								memcpy(recruiter_picture_data + i, app->pBuffer + 17 + i, 2);
							gm->recruiter_picture_data = recruiter_picture_data;
							gm->recruiter_picture_data_size = picture_data_size;
							guild->SetMemberSaveNeeded(true);
						}
					}
				}
				safe_delete(packet);
			}
			break;
							   }
		case OP_PetOptions: {
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_PetOptions", opcode, opcode);
			Spawn* target = player->GetTarget();
			PacketStruct* packet = configReader.getStruct("WS_PetOptions", GetVersion());
			if (packet && target && (target == player->GetPet() || target == player->GetCharmedPet() || target == player->GetDeityPet() || target == player->GetCosmeticPet())) {
				bool change = false;
				packet->LoadPacketData(app->pBuffer, app->size);

				string name = packet->getType_EQ2_16BitString_ByName("pet_name").data;
				if (strlen(name.c_str()) != 0) {
					target->SetName(name.c_str());
					strcpy(player->GetInfoStruct()->pet_name, name.c_str());
					GetCurrentZone()->SendUpdateTitles(target);
					change = true;
				}

				// Check protect self setting and update if needed
				if (packet->getType_int8_ByName("protect_self") == 1) {
					if ((player->GetInfoStruct()->pet_behavior & 2) == 0) {
						player->GetInfoStruct()->pet_behavior += 2;
						change = true;
					}
				}
				else {
					if ((player->GetInfoStruct()->pet_behavior & 2) != 0) {
						player->GetInfoStruct()->pet_behavior -= 2;
						change = true;
					}
				}

				// Check protect master setting and update if needed
				if (packet->getType_int8_ByName("protect_master") == 1) {
					if ((player->GetInfoStruct()->pet_behavior & 1) == 0) {
						player->GetInfoStruct()->pet_behavior += 1;
						change = true;
					}
				}
				else {
					if ((player->GetInfoStruct()->pet_behavior & 1) != 0) {
						player->GetInfoStruct()->pet_behavior -= 1;
						change = true;
					}
				}

				// Check stay/follow setting and update if needed
				if (packet->getType_int8_ByName("stay_follow_toggle") == 1) {
					if (player->GetInfoStruct()->pet_movement != 2) {
						player->GetInfoStruct()->pet_movement = 2;
						change = true;
					}
				}
				else {
					if (player->GetInfoStruct()->pet_movement != 1) {
						player->GetInfoStruct()->pet_movement = 1;
						change = true;
					}
				}

				// Ranged/Melee settings are not implemented yet

				if (change)					
					player->SetCharSheetChanged(true);

				safe_delete(packet);
			}
			break;
							}
		case OP_RecipeBook:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_RecipeBook", opcode, opcode);
			SendRecipeList();
			break;
						   }
		case OP_BuyPlayerHouseMsg: {
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_BuyPlayerHouseMsg", opcode, opcode);
			DumpPacket(app);

			PacketStruct* packet = configReader.getStruct("WS_BuyHouse", GetVersion());
			if (packet) {
				packet->LoadPacketData(app->pBuffer, app->size);
				HouseZone* hz = world.GetHouseZone(packet->getType_int64_ByName("house_id"));
				if (hz) {
					ZoneServer* instance_zone = zone_list.GetByInstanceID(0,hz->zone_id);
					int32 upkeep_due = Timer::GetUnixTimeStamp() + 604800; // 604800 = 7 days
					int64 unique_id = database.AddPlayerHouse(GetPlayer()->GetCharacterID(), hz->id, instance_zone->GetInstanceID(), upkeep_due);
					world.AddPlayerHouse(GetPlayer()->GetCharacterID(), hz->id, unique_id, instance_zone->GetInstanceID(), upkeep_due, 0, 0, GetPlayer()->GetName());
				}
			}

			safe_delete(packet);
			break;
								   }
		case OP_EnterHouseMsg: {
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_EnterHouseMsg", opcode, opcode);
			DumpPacket(app);
			PacketStruct* packet = configReader.getStruct("WS_EnterHouse", GetVersion());
			if (packet) {
				packet->LoadPacketData(app->pBuffer, app->size);
				PlayerHouse* ph = world.GetPlayerHouseByUniqueID(packet->getType_int64_ByName("house_id"));				
				if (ph) {
					HouseZone* hz = world.GetHouseZone(ph->house_id);
					if (hz) {
						ZoneServer* house = zone_list.GetByInstanceID(ph->instance_id, hz->zone_id);
						if (house) {
							Zone(house, true);
						}
					}
				}
			}

			safe_delete(packet);
			break;
							   }
		case OP_PayHouseUpkeepMsg: {
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_PayHouseUpkeepMsg", opcode, opcode);
			DumpPacket(app);
			PacketStruct* packet = configReader.getStruct("WS_PayUpkeep", GetVersion());
			if (packet) {
				packet->LoadPacketData(app->pBuffer, app->size);

			}

			safe_delete(packet);
			break;
								   }
		case OP_ExitHouseMsg: {
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_ExitHouseMsg", opcode, opcode);
			int32 instance_id = GetCurrentZone()->GetInstanceID();
			if (instance_id > 0) {
				PlayerHouse* ph = world.GetPlayerHouseByInstanceID(instance_id);
				if (ph) {
					HouseZone* hz = world.GetHouseZone(ph->house_id);
					if (hz) {
						ZoneServer* new_zone = zone_list.Get(hz->exit_zone_id);

						// determine if this is an instanced zone that already exists
						ZoneServer* instance_zone = GetPlayer()->GetGroupMemberInZone(hz->exit_zone_id);
						if(instance_zone || new_zone){
							GetPlayer()->SetX(hz->exit_x);
							GetPlayer()->SetY(hz->exit_y);
							GetPlayer()->SetZ(hz->exit_z);
							GetPlayer()->SetHeading(hz->exit_heading);
							if ( instance_zone )
								Zone(instance_zone->GetInstanceID(),false,true);
							else
								Zone(new_zone, false);
						}
					}
				}
			}
			DumpPacket(app);
			break;
							  }
		case OP_QuestJournalWaypointMsg: {
			//DumpPacket(app);
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_QuestJournalWaypointMsg", opcode, opcode);
			PacketStruct* packet = configReader.getStruct("WS_QuestJournalWaypoint", GetVersion());
			if (packet) {
				packet->LoadPacketData(app->pBuffer, app->size);
				int32 quests = packet->getType_int32_ByName("num_quests");
				
				if( quests > 100 ) // just picking a number higher than max allowed
				{
					LogWrite(CCLIENT__ERROR, 0, "Client", "num_quests = %u - quantity too high, aborting load.", quests);
					break;
				}

				LogWrite(CCLIENT__DEBUG, 0, "Client", "num_quests = %u", quests);

				for (int32 i = 0; i < quests; i++) {
					int32 id = packet->getType_int32_ByName("quest_id_0", i);
					if( id == 0 )
						continue;
					LogWrite(CCLIENT__DEBUG, 5, "Client", "quest_id = %u", id);
					bool tracked = packet->getType_int8_ByName("quest_tracked_0", i) == 1 ? true : false;
					if (player->player_quests.count(id) > 0) {
						player->player_quests[id]->SetTracked(tracked);
						player->player_quests[id]->SetSaveNeeded(true);
					}
				}

				safe_delete(packet);
			}

			break;
										 }
		case OP_PaperdollImage: {
			PacketStruct* packet = configReader.getStruct("WS_PaperdollImage", version);
			if (packet && packet->LoadPacketData(app->pBuffer, app->size)) {

				//First check if this is a new image... delete an existing partial image if one exists
				int8 packet_index = packet->getType_int8_ByName("packetIndex");
				if (packet_index == 0){
					safe_delete_array(incoming_paperdoll.image_bytes);
					incoming_paperdoll.last_received_packet_index = 0;
					incoming_paperdoll.current_size_bytes = 0;
				}
				//return if this packet is not the one we are expecting...
				else if (packet_index != incoming_paperdoll.last_received_packet_index + 1) {
					safe_delete(packet);
					break;
				}

				//Check how many packets we're supposed to be receiving for this/these images
				incoming_paperdoll.image_num_packets = packet->getType_int8_ByName("totalNumPackets");

				//Check the image type, if this is a new type in the same series of packets we have a new image
				int8 img_type = packet->getType_int8_ByName("image_type");
				if (packet_index != 0 && img_type != incoming_paperdoll.image_type){
					//We have a new image. Save the old data and clear before continuing
					SavePlayerImages();
				}
				incoming_paperdoll.image_type = img_type;

				//Get the size of the image data in this packet
				sint64 image_size = packet->getType_int32_ByName("imageSize");
				if (image_size <= 0 || image_size > 1048576) {
					//If this packet is saying that the array is size <= 0 or > 1 MiB return out... it shouldn't be those sizes ever
					safe_delete(packet);
					break;
				}

				//Create a new array
				int32 new_image_size = image_size;
				uchar* new_image = new uchar[incoming_paperdoll.current_size_bytes + new_image_size];
				if (incoming_paperdoll.image_bytes){
					memcpy(new_image, incoming_paperdoll.image_bytes, incoming_paperdoll.current_size_bytes);
					safe_delete_array(incoming_paperdoll.image_bytes);
				}

				//variable i should be the index in the packet of the first PNG file byte
				vector<DataStruct*>* d_structs = packet->getStructs();
				vector<DataStruct*>::iterator itr;
				int32 i = 0;
				for (itr = d_structs->begin(); itr != d_structs->end(); ++itr){
					DataStruct* ds = (*itr);
					if (strcmp(ds->GetName(), "pngData_0") != 0)
						i += ds->GetDataSizeInBytes();
					else
						break;
				}

				//Return if this packet is bad and we would read out of bounds
				if (app->size - i < new_image_size) {
					safe_delete(packet);
					safe_delete_array(new_image);
					break;
				}

				uchar* tmp = new_image + incoming_paperdoll.current_size_bytes;
				memcpy(tmp, app->pBuffer + i, new_image_size);

				incoming_paperdoll.current_size_bytes += new_image_size;
				incoming_paperdoll.image_bytes = new_image;

				//Check if this is the last packet we're expecting for this image. Create a final image if so
				if (incoming_paperdoll.image_num_packets == 1 ||
					incoming_paperdoll.last_received_packet_index + 2 == incoming_paperdoll.image_num_packets){
					SavePlayerImages();
				}

				incoming_paperdoll.last_received_packet_index = packet_index;
			}
			safe_delete(packet);

			break;
		}

		case OP_ReadyForTakeOffMsg:
		{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_ReadyForTakeOffMsg", opcode, opcode);

			int32 index = GetCurrentZone()->GetFlightPathIndex(GetPendingFlightPath());
			if (GetPendingFlightPath() > 0) {
				if (index != -1) {
					PacketStruct* packet = configReader.getStruct("WS_ClearForTakeOff", GetVersion());
					if (packet) {
						packet->setDataByName("spawn_id", GetPlayer()->GetIDWithPlayerSpawn(GetPlayer()));
						packet->setDataByName("path_id", index);
						packet->setDataByName("speed", GetCurrentZone()->GetFlightPathSpeed(GetPendingFlightPath()));
						QueuePacket(packet->serialize());
						safe_delete(packet);

						on_auto_mount = true;
					}
				}
				else {
					LogWrite(CCLIENT__ERROR, 0, "Client", "OP_ReadyForTakeOffMsg recieved but unable to get an index for path (%u) in zone (%u)", GetPendingFlightPath(), GetCurrentZone()->GetZoneID());
					Message(CHANNEL_ERROR, "Unable to get index for path (%u) in zone (%u)", GetPendingFlightPath(), GetCurrentZone()->GetZoneID());
					EndAutoMount();
				}

				SetPendingFlightPath(0);
				
			}
			else
				LogWrite(CCLIENT__ERROR, 0, "Client", "OP_ReadyForTakeOffMsg recieved but there is no pending flight path...");

			break;
		}

		case OP_EarlyLandingRequestMsg:
		{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): OP_ReadyForLandingMsg", opcode, opcode);

			EndAutoMount();
			
			break;
		}

		default:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): Unknown in %s", opcode, opcode, __FILE__);
			const char* name = app->GetOpcodeName();
			if(name)
				LogWrite(OPCODE__DEBUG, 1, "Opcode", "%s Received %04X (%i)", name, app->GetRawOpcode(), app->GetRawOpcode());
			else
				LogWrite(OPCODE__DEBUG, 1, "Opcode", "Received %04X (%i)", app->GetRawOpcode(), app->GetRawOpcode());

			//DumpPacket(app);

		}
	}
	if (!eqs || !eqs->CheckActive()) {
			return false;
	}

	return ret;
}

bool Client::HandleLootItem(Entity* entity, Item* item){
	if(!item){
		SimpleMessage(CHANNEL_COLOR_YELLOW, "Unable to find item to loot!");
			return false;
	}
	if(player->item_list.HasFreeSlot() || player->item_list.CanStack(item)){
		if(player->item_list.AssignItemToFreeSlot(item)){
			int8 type = CHANNEL_COLOR_LOOT;
			if(GetVersion() >= 973 && GetVersion() <= 1000)
				type = CHANNEL_COLOR_NEW_LOOT;
			else if(GetVersion() >= 973)
				type = CHANNEL_COLOR_NEWEST_LOOT;			
			if(entity)
				Message(type, "You loot \\aITEM %u 0:%s\\/a from the corpse of %s", item->details.item_id, item->name.c_str(), entity->GetName());
			else
				Message(type, "You found a \\aITEM %u 0:%s\\/a", item->details.item_id, item->name.c_str());
			Guild* guild = player->GetGuild();
			if (guild && item->details.tier >= ITEM_TAG_LEGENDARY) {
				char adjective[32];
				int8 type;
				memset(adjective, 0, sizeof(adjective));
				if (item->details.tier >= ITEM_TAG_MYTHICAL) {
					strncpy(adjective, "Mythical", sizeof(adjective) - 1);
					type = GUILD_EVENT_LOOTS_MYTHICAL_ITEM;
				}
				else if (item->details.tier >= ITEM_TAG_FABLED) {
					strncpy(adjective, "Fabled", sizeof(adjective) - 1);
					type = GUILD_EVENT_LOOTS_FABELED_ITEM;
				}
				else {
					strncpy(adjective, "Legendary", sizeof(adjective) - 1);
					type = GUILD_EVENT_LOOTS_LEGENDARY_ITEM;
				}
				guild->AddNewGuildEvent(type, "%s has looted the %s \\aITEM %u 0:%s\\/a", Timer::GetUnixTimeStamp(), true, player->GetName(), adjective, item->details.item_id, item->name.c_str());
				guild->SendMessageToGuild(type, "%s has looted the %s \\aITEM %u 0:%s\\/a", player->GetName(), adjective, item->details.item_id, item->name.c_str());
			}
			CheckPlayerQuestsItemUpdate(item);
					return true;
		}
		else
			SimpleMessage(CHANNEL_COLOR_RED, "Could not find free slot to place item.");	
	}
	else
		SimpleMessage(CHANNEL_COLOR_YELLOW, "Unable to loot item: Inventory is FULL.");

	return false;
}

bool Client::HandleLootItem(Entity* entity, int32 item_id){
	if(!entity) {
			return false;
	}

	return HandleLootItem(entity, entity->LootItem(item_id));
}

void Client::HandleLoot(EQApplicationPacket* app){
	PacketStruct* packet = configReader.getStruct("WS_LootType", GetVersion());
	if(packet){
		packet->LoadPacketData(app->pBuffer, app->size);
		int32 loot_id = packet->getType_int32_ByName("loot_id");
		bool loot_all = (packet->getType_int8_ByName("loot_all") == 1);
		safe_delete(packet);
		int32 item_id = 0;
		Spawn* spawn = GetCurrentZone()->GetSpawnByID(loot_id);
		if(player->HasPendingLootItems(loot_id)){
			Item* master_item = 0;
			Item* item = 0;
			if(loot_all){
				vector<Item*>* items = player->GetPendingLootItems(loot_id);
				if(items){
					for(int32 i=0;loot_all && i<items->size();i++){
						master_item = items->at(i);
						if(master_item){
							item = new Item(master_item);
							if(item){
								loot_all = HandleLootItem(0, item);
								if(loot_all)
									player->RemovePendingLootItem(loot_id, item->details.item_id);
							}
						}
					}
					safe_delete(items);
				}
			}
			else{
				packet = configReader.getStruct("WS_LootItem", GetVersion());
				if(packet){
					packet->LoadPacketData(app->pBuffer, app->size);
					item_id = packet->getType_int32_ByName("item_id");
					vector<Item*>* items = player->GetPendingLootItems(loot_id);
					if(items){
						for(int32 i=0;i<items->size();i++){
							master_item = items->at(i);
							if(master_item && master_item->details.item_id == item_id){
								item = new Item(master_item);
								if(item && HandleLootItem(0, item))
									player->RemovePendingLootItem(loot_id, item->details.item_id);
								break;
							}
						}
						safe_delete(items);
					}
					safe_delete(packet);
				}
			}
			EQ2Packet* outapp = player->SendInventoryUpdate(GetVersion());
			if(outapp)
				QueuePacket(outapp);
			Loot(0, player->GetPendingLootItems(loot_id), (Entity*)spawn);
		}
		else{
			if(spawn && spawn->IsNPC() && ((NPC*)spawn)->Brain()->CheckLootAllowed(player)){
				if(loot_all){
					while(loot_all && ((item_id = ((Entity*)spawn)->GetLootItemID()) > 0)){
						loot_all = HandleLootItem((Entity*)spawn, item_id);
					}
				}
				else{
					packet = configReader.getStruct("WS_LootItem", GetVersion());
					if(packet){
						packet->LoadPacketData(app->pBuffer, app->size);
						item_id = packet->getType_int32_ByName("item_id");
						HandleLootItem((Entity*)spawn, item_id);
						safe_delete(packet);
					}
				}
				EQ2Packet* outapp = player->SendInventoryUpdate(GetVersion());
				if(outapp)
					QueuePacket(outapp);
				Loot((Entity*)spawn);
				if(!((Entity*)spawn)->HasLoot()){
					CloseLoot();
					if(((Entity*)spawn)->IsNPC())
						GetCurrentZone()->RemoveDeadSpawn(spawn);
				}
			}
			else{
				if(!spawn){
					LogWrite(WORLD__ERROR, 0, "World", "Unknown id of %u when looting!", loot_id);
					SimpleMessage(CHANNEL_COLOR_YELLOW, "Unable to find spawn to loot!");
				}
				else
					SimpleMessage(CHANNEL_COLOR_YELLOW, "You are not unable to loot that at this time.");
			}
		}
	}

}

void Client::HandleSkillInfoRequest(EQApplicationPacket* app){
	PacketStruct* request = 0;
	//	cout << "Request: \n";
	//	DumpPacket(app);
	switch(app->pBuffer[0]){
		case 0:{ //items
			request = configReader.getStruct("WS_SkillInfoItemRequest", GetVersion());
			if(request){
				request->LoadPacketData(app->pBuffer, app->size);
				Item* item = GetPlayer()->GetEquipmentList()->GetItemFromUniqueID(request->getType_int32_ByName("unique_id"));
				if(!item)
					item = GetPlayer()->item_list.GetItemFromUniqueID(request->getType_int32_ByName("unique_id"), true);
				if(item){
					PacketStruct* response = configReader.getStruct("WS_SkillInfoItemResponse", GetVersion());
					if(response){
						response->setDataByName("request_type", request->getType_int32_ByName("request_type"));
						response->setDataByName("unique_id", request->getType_int32_ByName("unique_id"));
						response->setSmallStringByName("name", item->name.c_str());
						EQ2Packet* app2 = response->serialize();
						//DumpPacket(app2);
						QueuePacket(app2);
						safe_delete(response);
					}
				}
			}
			break;
		}
		case 2:{//spells
			request = configReader.getStruct("WS_SkillInfoSpellRequest", GetVersion());
			if(request){
				request->LoadPacketData(app->pBuffer, app->size);
				int32 id = request->getType_int32_ByName("id");
				int8 tier = request->getType_int32_ByName("unique_id"); //on live this is really unique_id, but I'm going to make it tier instead :)
				Spell* spell = master_spell_list.GetSpell(id, tier);
				PacketStruct* response = configReader.getStruct("WS_SkillInfoResponse", GetVersion());
				if(response){
					response->setDataByName("request_type", 2);
					response->setDataByName("unique_id", tier);
					response->setDataByName("id", id);
					if(spell)
						response->setSmallStringByName("name", spell->GetName());
					else
						response->setSmallStringByName("name", "Unknown Spell");
					EQ2Packet* app2 = response->serialize();
					//				DumpPacket(app2);
					QueuePacket(app2);
					safe_delete(response);
				}
			}
			break;
		}
		default:{
			LogWrite(WORLD__ERROR, 0, "World", "Unknown SkillInfoRequest type of %i", (int)app->pBuffer[0]);
				}
	}
	safe_delete(request);

}

void Client::HandleExamineInfoRequest(EQApplicationPacket* app){
	PacketStruct* request = 0;

	//LogWrite(CCLIENT__DEBUG, 0, "Client", "Request2:");
	//DumpPacket(app);

	int8 type = app->pBuffer[0];
	if (type == 3) {
		Spell* spell = 0;
		bool trait_display;
		request = configReader.getStruct("WS_ExamineInfoRequest_type3", GetVersion());
		if (!request) {
			return;
		}
		request->LoadPacketData(app->pBuffer, app->size);
		int32 id = request->getType_int32_ByName("id");
		int32 tier = request->getType_int32_ByName("tier");
		int32 trait_tier = request->getType_int32_ByName("trait_tier");
		//printf("Type: (%i) Tier: (%u) Unknown ID: (%u) Item ID: (%u)\n",type,tier,trait_tier,id);
		if (trait_tier != 0xFFFFFFFF) {
			spell = master_spell_list.GetSpell(id, trait_tier);
			trait_display = true;
		} else {
			spell = master_spell_list.GetSpell(id, tier);
			trait_display = false;
		}

		if (spell && sent_spell_details.count(id) == 0) {
			sent_spell_details[id] = true;
			EQ2Packet* app = spell->SerializeSpell(this, false, trait_display);
			//DumpPacket(app);
			QueuePacket(app);
		}
	}
	else if(type == 0){
		request = configReader.getStruct("WS_ExamineInfoItemRequest", GetVersion());
		if(!request) {
					return;
		}
		request->LoadPacketData(app->pBuffer, app->size);
		int32 id = request->getType_int32_ByName("id");
		Item* item = GetPlayer()->item_list.GetItemFromUniqueID(id, true);
		if(!item)
			item = GetPlayer()->GetEquipmentList()->GetItemFromUniqueID(id);
		if(!item)
			item = master_item_list.GetItem(id);
		if(item){// && sent_item_details.count(id) == 0){
			sent_item_details[id] = true;
			EQ2Packet* app = item->serialize(GetVersion(), false, GetPlayer());
			//DumpPacket(app);
			QueuePacket(app);
		}
		else{
			LogWrite(WORLD__ERROR, 0, "World", "HandleExamineInfoRequest: Unknown Item ID = %u", id);
			DumpPacket(app);
		}
	}
	else if(type == 1){
		request = configReader.getStruct("WS_ExamineInfoItemRequest", GetVersion());
		if(!request) {
					return;
		}
		request->LoadPacketData(app->pBuffer, app->size);
		int32 id = request->getType_int32_ByName("id");
		Item* item = master_item_list.GetItem(id);
		if(item){// && sent_item_details.count(id) == 0){
			sent_item_details[id] = true;
			EQ2Packet* app = item->serialize(GetVersion(), false, GetPlayer());
			//DumpPacket(app);
			QueuePacket(app);
		}
		else{
			LogWrite(WORLD__ERROR, 0, "World", "HandleExamineInfoRequest: Unknown Item ID = %u", id);
			DumpPacket(app);
		}
	}
	else if(type == 2){
		request = configReader.getStruct("WS_ExamineInfoItemLinkRequest", GetVersion());
		if(!request) {
					return;
		}
		request->LoadPacketData(app->pBuffer, app->size);
		int32 id = request->getType_int32_ByName("item_id");
		//int32 unknown_0 = request->getType_int32_ByName("unknown",0);
		//int32 unknown_1 = request->getType_int32_ByName("unknown",1);
		//int8 unknown2 = request->getType_int8_ByName("unknown2");
		//int32 unique_id = request->getType_int32_ByName("unique_id");
		//int16 unknown5 = request->getType_sint16_ByName("unknown5");
		//printf("Type: (%i) Unknown_0: (%u) Unknown_1: (%u) Unknown2: (%i) Unique ID: (%u) Unknown5: (%i) Item ID: (%u)\n",type,unknown_0,unknown_1,unknown2,unique_id,unknown5,id);
		Item* item = master_item_list.GetItem(id);
		if(item){
			//only display popup for non merchant links
			EQ2Packet* app = item->serialize(GetVersion(), (request->getType_int32_ByName("unique_id") != 0xFFFFFFFF), GetPlayer(), true, 0, 0, true);
			//DumpPacket(app);
			QueuePacket(app);
		}
		else{
			LogWrite(WORLD__ERROR, 0, "World", "HandleExamineInfoRequest: Unknown Item ID = %u", id);
			DumpPacket(app);
		}
	}
	else if(type == 4){ //spell effect
		request = configReader.getStruct("WS_ExamineSpellEffectRequest", GetVersion());
		if(!request) {
					return;
		}
		request->LoadPacketData(app->pBuffer, app->size);
		int32 id = request->getType_int32_ByName("id");
		//int16 unknown5 = request->getType_sint16_ByName("unknown5");
		SpellEffects* effect = player->GetSpellEffect(id);
		//printf("Type: (%i) Unknown5: (%i) Item ID: (%u)\n",type,unknown5,id);
		if(effect){
			int8 tier = effect->tier;
			Spell* spell = master_spell_list.GetSpell(id, tier);
			if(spell && sent_spell_details.count(id) == 0){
				sent_spell_details[id] = true;
				EQ2Packet* app = spell->SerializeSpecialSpell(this, false, 0x00, 0x81);
				//DumpPacket(app);
				QueuePacket(app);
			}
		}
	}
	else if(type == 5){ // recipe info
		request = configReader.getStruct("WS_ExamineInfoRequest", GetVersion());
		if(!request)
			return;
		request->LoadPacketData(app->pBuffer, app->size);
		int32 id = request->getType_int32_ByName("unknown_id");
		Recipe *recipe = master_recipe_list.GetRecipe(id);
		if(recipe){
			EQ2Packet* app = recipe->SerializeRecipe(this, recipe, false, GetItemPacketType(GetVersion()), 0x02);
			//DumpPacket(app);
			QueuePacket(app);
		}
		
	}
	else if(type == 6){ // AA spell info
		Spell* spell = 0;
		AltAdvanceData* data = 0;
		request = configReader.getStruct("WS_ExamineInfoRequest", GetVersion());
		if(!request)
			return;
		request->LoadPacketData(app->pBuffer, app->size);
		int32 id = request->getType_int32_ByName("id");
		int32 tier = request->getType_int32_ByName("unique_id");
		LogWrite(WORLD__INFO, 0, "World", "Examine Info Request->Unique ID: %u Tab ID: %u ", id, tier);
		data = master_aa_list.GetAltAdvancement(id);
		LogWrite(WORLD__INFO, 0, "World", "SOE Spell CRC: %u", data->spell_crc);
		spell = master_spell_list.GetSpellByCRC(data->spell_crc);
		//spell = master_spell_list.GetSpell(id, 1);
		if (spell && sent_spell_details.count(spell->GetSpellID()) == 0) {
			sent_spell_details[spell->GetSpellID()] = true;
			EQ2Packet* app = spell->SerializeAASpell(this, data, false, GetItemPacketType(GetVersion()), 0x04);
			DumpPacket(app);
			LogWrite(WORLD__INFO, 0, "WORLD", "Examine Info Request-> Spell ID: %u", spell->GetSpellID());
			QueuePacket(app);
		}
	}
	else{
		LogWrite(WORLD__ERROR, 0, "World", "Unknown examine request: %i", (int)type);
		DumpPacket(app);
	}
	safe_delete(request);

}

void Client::HandleQuickbarUpdateRequest(EQApplicationPacket* app){
	PacketStruct* request = configReader.getStruct("WS_QuickBarUpdateRequest", GetVersion());
	if(request){
		request->LoadPacketData(app->pBuffer, app->size);
		int32 id = request->getType_int32_ByName("id");
		int32 bar = request->getType_int32_ByName("hotbar_number");
		int32 slot = request->getType_int32_ByName("hotkey_slot");
		int32 type = request->getType_int32_ByName("type");
		int8 tier = request->getType_int32_ByName("unique_id");
		EQ2_16BitString text = request->getType_EQ2_16BitString_ByName("text");
		if (type == 0xFFFFFFFF) {
			GetPlayer()->RemoveQuickbarItem(bar, slot);
		} else {
			if (type == QUICKBAR_NORMAL) {
				Spell* spell = master_spell_list.GetSpell(id, tier);
				if (spell)
					GetPlayer()->AddQuickbarItem(bar, slot, type, spell->GetSpellIcon(), spell->GetSpellIconBackdrop(), id, tier, 0, text.data.c_str());
			} else {
				GetPlayer()->AddQuickbarItem(bar, slot, type, 0, 0, id, 0, 0, text.data.c_str());
			}
		}
		safe_delete(request);
	}

}

bool Client::Process(bool zone_process) {
	if(!eqs) {
			return false;
	}
	if((connected_to_zone && !zone_process) || (!connected_to_zone && zone_process)) {
			return true;
	}

	if(new_client_login){
		LogWrite(CCLIENT__DEBUG, 0, "Client", "SendLoginInfo to new client...");
		SendLoginInfo();
		new_client_login = false;
	}
	bool ret = true;
	sockaddr_in to;

	memset((char *) &to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = port;
	to.sin_addr.s_addr = ip;

	/************ Get all packets from packet manager out queue and process them ************/
	EQApplicationPacket *app = 0;
	if(eqs && !eqs->CheckActive()){
		num_active_failures++;

		LogWrite(CCLIENT__DEBUG, 7, "Client", "%s, num_active_failures = %i", __FUNCTION__, num_active_failures);

		if(num_active_failures > 100) {
					return false;
		}
			return true;
	}
	while(ret && eqs && (app = eqs->PopPacket())) {
		ret = HandlePacket(app);

		LogWrite(CCLIENT__DEBUG, 5, "Client", "Func: %s, Line: %i, Opcode: '%s'", __FUNCTION__, __LINE__, app->GetOpcodeName());

		delete app;
	}

	if (GetCurrentZone() && GetCurrentZone()->GetSpawnByID(GetPlayer()->GetID()) && should_load_spells) {
		player->ApplyPassiveSpells();
		database.LoadCharacterActiveSpells(player);

		should_load_spells = false;
	}

	if(quest_updates) {
		LogWrite(CCLIENT__DEBUG, 1, "Client", "%s, ProcessQuestUpdates", __FUNCTION__, __LINE__);
		ProcessQuestUpdates();
	}
	if(last_update_time > 0 && last_update_time < (Timer::GetCurrentTime2() - 300))	{
		LogWrite(CCLIENT__DEBUG, 1, "Client", "%s, CheckQuestQueue", __FUNCTION__, __LINE__);
		CheckQuestQueue();
	}
	if(pos_update.Check() && player_pos_changed){
		//GetPlayer()->CalculateLocation();
		GetCurrentZone()->SendPlayerPositionChanges(GetPlayer());
		player_pos_changed = false;
		GetCurrentZone()->CheckTransporters(this);
	}
	if (spawn_vis_update.Check() && GetPlayer()->GetResendSpawns()) {
		GetCurrentZone()->StartZoneSpawnsForAggroThread(this);
		GetPlayer()->SetResendSpawns(false);
	}
	if(lua_interface && lua_debug && lua_debug_timer.Check())
		lua_interface->UpdateDebugClients(this);
	if(quest_pos_timer.Check())
		CheckPlayerQuestsLocationUpdate();
	if(camp_timer && camp_timer->Check() && getConnection()){
		getConnection()->SendDisconnect(false);
		safe_delete(camp_timer);
		disconnect_timer = new Timer(2000);
		disconnect_timer->Start();
	}
	if(player->GetSkills()->HasSkillUpdates()){
		vector<Skill*>* skills = player->GetSkills()->GetSkillUpdates();
		if(skills){
			vector<Skill*>::iterator itr;
			for(itr = skills->begin(); itr != skills->end(); ++itr){
				Skill* skill = *itr;
				SkillChanged(skill, skill->previous_val, skill->current_val);
			}
			safe_delete(skills);
		}
	}
	if(disconnect_timer && disconnect_timer->Check()){
		safe_delete(disconnect_timer);
		ret = false;
	}
	m_resurrect.writelock(__FUNCTION__, __LINE__);
	if(current_rez.should_delete || (current_rez.expire_timer && current_rez.expire_timer->Check(false))){
		if (GetCurrentZone() && GetCurrentZone()->GetSpellProcess() && current_rez.spell)
			GetCurrentZone()->GetSpellProcess()->DeleteCasterSpell(current_rez.spell, false);

		if(current_rez.expire_timer)
			safe_delete(current_rez.expire_timer);
		current_rez.expire_timer = 0;
		current_rez.active = false;
		current_rez.caster = 0;
		current_rez.crit = false;
		current_rez.crit_mod = 0;
		current_rez.expire_timer = 0;
		current_rez.heal_name = "";
		current_rez.hp_perc = 0;
		current_rez.mp_perc = 0;
		current_rez.no_calcs = false;
		current_rez.range = 0;
		current_rez.should_delete = false;
		current_rez.spell_name = "";
		current_rez.spell_visual = 0;
		current_rez.subspell = 0;
	}
	m_resurrect.releasewritelock(__FUNCTION__, __LINE__);

	// Quest timers
	Quest* failed_step = 0;
	MQuestTimers.writelock(__FUNCTION__, __LINE__);
	if (quest_timers.size() > 0) {
		vector<int32>::iterator itr;
		map<int32, Quest*>* player_quests = player->GetPlayerQuests();
		for (itr = quest_timers.begin(); itr != quest_timers.end(); ++itr) {
			if (player_quests->count(*itr) > 0 && player_quests->at(*itr)->GetStepTimer() != 0) {
				Quest* quest = player_quests->at(*itr);
				if (Timer::GetUnixTimeStamp() >= quest->GetStepTimer()) {
					failed_step = quest;
					break;
				}
			}
			else {
				quest_timers.erase(itr);
				break;
			}
		}
	}
	MQuestTimers.releasewritelock(__FUNCTION__, __LINE__);

	if (failed_step)
		failed_step->StepFailed(failed_step->GetTimerStep());

	if (player->ControlFlagsChanged())
		player->SendControlFlagUpdates(this);

	if (!eqs || !eqs->CheckActive())
		ret = false;

	if(!ret)
		Save();

	return ret;
}

ClientList::ClientList() {
	MClients.SetName("ClientList::MClients");

}

ClientList::~ClientList() {
}

void ClientList::ReloadQuests() {
	list<Client*>::iterator client_iter;
	MClients.readlock(__FUNCTION__, __LINE__);
	for(client_iter=client_list.begin(); client_iter!=client_list.end(); ++client_iter){
		Client* client = *client_iter;
		if(client)
			client->ReloadQuests();
	}
	MClients.releasereadlock(__FUNCTION__, __LINE__);

}

int32 ClientList::Count(){
	return client_list.size();
}

void ClientList::Add(Client* client) {
	MClients.writelock(__FUNCTION__, __LINE__);
	client_list.push_back(client);
	MClients.releasewritelock(__FUNCTION__, __LINE__);

}

Client* ClientList::FindByAccountID(int32 account_id) {
	list<Client*>::iterator client_iter;
	Client* client = 0;
	Client* ret = 0;
	MClients.readlock(__FUNCTION__, __LINE__);
	for(client_iter=client_list.begin(); client_list.size() > 0 && client_iter!=client_list.end(); ++client_iter){
		client = *client_iter;
		if (client->GetAccountID() == account_id) {
			ret =  client;
			break;
		}
	}
	MClients.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}

Client* ClientList::FindByName(char* charName) {
	list<Client*>::iterator client_iter;
	Client* client = 0;
	Client* ret = 0;
	MClients.readlock(__FUNCTION__, __LINE__);
	for(client_iter=client_list.begin(); client_list.size() > 0 && client_iter!=client_list.end(); ++client_iter){
		client = *client_iter;
		if (!client || !client->GetPlayer())
			continue;

		if (!strncmp(client->GetPlayer()->GetName(),charName,strlen(charName))) {
			ret =  client;
			break;
		}
	}
	MClients.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}

Client* ClientList::Get(int32 ip, int16 port) {
	list<Client*>::iterator client_iter;
	Client* client = 0;
	Client* ret = 0;
	MClients.readlock(__FUNCTION__, __LINE__);
	for(client_iter=client_list.begin(); client_list.size() > 0 && client_iter!=client_list.end(); ++client_iter){
		client = *client_iter;
		if(client->GetIP() == ip && client->GetPort() == port){
			ret = client;
			break;
		}
	}
	MClients.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}

void ClientList::Process() {

	list<Client*>::iterator client_iter;
	list<Client*>::iterator erase_iter;
	Client* client = 0;
	MClients.readlock(__FUNCTION__, __LINE__);
	erase_iter = client_list.end();
	for(client_iter=client_list.begin(); client_iter!=client_list.end(); ++client_iter){
		client = *client_iter;
		// have a sanity check because the client list can sometimes obtain null client pointers
		if (!client || (!client->Process() || client->remove_from_list)) {
			erase_iter = client_iter;
			break;
		}
	}
	MClients.releasereadlock(__FUNCTION__, __LINE__);
	if(erase_iter != client_list.end()){
		client = *erase_iter;
		MClients.writelock(__FUNCTION__, __LINE__);
		client_list.erase(erase_iter);
		MClients.releasewritelock(__FUNCTION__, __LINE__);
		if(client && !client->remove_from_list){
			struct in_addr  in;
			in.s_addr = client->GetIP();
			LogWrite(WORLD__INFO, 0, "World", "Removing client from ip: %s port: %i", inet_ntoa(in), client->GetPort());
			safe_delete(client);
		}
	}

}

void ClientList::RemoveConnection(EQStream* eqs){
	if(eqs){
		list<Client*>::iterator client_iter;
		MClients.readlock(__FUNCTION__, __LINE__);
		for(client_iter=client_list.begin(); client_iter!=client_list.end(); ++client_iter){
			Client* client = *client_iter;
			if(client->getConnection() == eqs)
				client->Disconnect(false);
		}
		MClients.releasereadlock(__FUNCTION__, __LINE__);
	}

}

bool ClientList::ContainsStream(EQStream* eqs){
	if(!eqs) {
			return false;
	}
	list<Client*>::iterator client_iter;
	bool ret = false;
	MClients.readlock(__FUNCTION__, __LINE__);
	for(client_iter=client_list.begin(); client_iter!=client_list.end(); ++client_iter){
		if((*client_iter)->getConnection() && (*client_iter)->getConnection()->GetRemotePort() == eqs->GetRemotePort() && (*client_iter)->getConnection()->GetRemoteIP() == eqs->GetRemoteIP()){
			ret = true;
			break;
		}
	}
	MClients.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}

void ClientList::Remove(Client* client, bool remove_data) {
	client->remove_from_list = true;
	if(remove_data){
		safe_delete(client);
	}

}

void Client::SetCurrentZone(int32 id){
	if(current_zone){
		//current_zone->GetCombat()->RemoveHate(player);
		current_zone->RemoveSpawn(player, false);
	}
	SetCurrentZone(zone_list.Get(id));

}

void Client::SetCurrentZoneByInstanceID(int32 id,int32 zoneid){
	if(current_zone){
		//current_zone->GetCombat()->RemoveHate(player);
		current_zone->RemoveSpawn(player, false);
	}
	SetCurrentZone(zone_list.GetByInstanceID(id,zoneid));

}

ZoneServer* Client::GetCurrentZone(){
	return current_zone;
}

void Client::SimpleMessage(int8 color, const char* message){
	PacketStruct* command_packet = configReader.getStruct("WS_DisplayText", GetVersion());
	if(command_packet){
		command_packet->setDataByName("color", color);
		command_packet->setMediumStringByName("text", message);
		command_packet->setDataByName("unknown02", 0x00ff);
		EQ2Packet* outapp = command_packet->serialize();
		QueuePacket(outapp);
		safe_delete(command_packet);
	}

}

void Client::SendSpellUpdate(Spell* spell){
	PacketStruct* packet = configReader.getStruct("WS_SpellGainedMsg", GetVersion());
	if(packet){
		packet->setDataByName("spell_name", spell->GetName());
		packet->setDataByName("spell_type", spell->GetSpellData()->type);
		packet->setDataByName("display_spell_tier", 1);
		packet->setDataByName("unknown3", 1);
		packet->setDataByName("tier", spell->GetSpellTier());
		packet->setDataByName("unknown5", 0xFFFFFFFF);
		packet->setDataByName("spell_id", spell->GetSpellID());
		packet->setDataByName("unique_id", spell->GetSpellTier());
		packet->setDataByName("icon", spell->GetSpellIcon());
		packet->setDataByName("icon_type", spell->GetSpellIconBackdrop());
		EQ2Packet* outapp = packet->serialize();
		//DumpPacket(outapp);
		QueuePacket(outapp);
		safe_delete(packet);
	}

}

void Client::Message(int8 type, const char* message, ...) {
	va_list argptr;
	char buffer[4096];

	va_start(argptr, message);
	vsnprintf(buffer, sizeof(buffer), message, argptr);
	va_end(argptr);
	SimpleMessage(type, buffer);

}

void Client::Disconnect(bool send_disconnect)
{
	LogWrite(CCLIENT__DEBUG, 0, "CClient", "Client Disconnect...");

	SetConnected(false);

	if(send_disconnect && getConnection())
		getConnection()->SendDisconnect(true);

	this->Save();
	this->GetPlayer()->WritePlayerStatistics();

	eqs = 0;
}

bool Client::Summon(const char* search_name){
	Spawn* target = 0;
	if(search_name || GetPlayer()->GetTarget()){
		Client* search_client = 0;
		if(search_name){
			target = GetCurrentZone()->FindSpawn(GetPlayer(), search_name);
			if(target && target->IsPlayer())
				search_client = GetCurrentZone()->GetClientBySpawn(target);
			if(!target){
				search_client = zone_list.GetClientByCharName(string(search_name));
				if(search_client)
					target = search_client->GetPlayer();
			}
		}
		else
			target = GetPlayer()->GetTarget();
		if(target && target != GetPlayer()){
			target->SetX(GetPlayer()->GetX());
			target->SetY(GetPlayer()->GetY());
			target->SetZ(GetPlayer()->GetZ());
			target->SetHeading(GetPlayer()->GetHeading());
			if(!target->IsPlayer()){
				target->SetSpawnOrigX(target->GetX());
				target->SetSpawnOrigY(target->GetY());
				target->SetSpawnOrigZ(target->GetZ());
				target->SetSpawnOrigHeading(target->GetHeading());
			}
			target->SetLocation(GetPlayer()->GetLocation());
		}
		else if(target)
			Message(CHANNEL_COLOR_RED,"Error: You cannot summon yourself!");
		if(search_client && search_client != this){
			search_client->Message(CHANNEL_COLOR_YELLOW, "You have been summoned by '%s'!", GetPlayer()->GetName());
			Message(CHANNEL_COLOR_YELLOW, "Summoning '%s'...", search_client->GetPlayer()->GetName());
			if(search_client->GetCurrentZone() != GetCurrentZone())
				search_client->Zone(GetCurrentZone()->GetZoneName(), false);
			else{
				EQ2Packet* app = search_client->GetPlayer()->Move(GetPlayer()->GetX(), GetPlayer()->GetY(), GetPlayer()->GetZ(), search_client->GetVersion());
				if(app)
					search_client->QueuePacket(app);
			}
		}
	}

	if(!target)
		return false;
	else
		return true;
}

bool Client::TryZoneInstance(int32 zoneID, bool zone_coords_valid) {
	// determine if this is a group instanced zone that already exists
	ZoneServer* instance_zone = GetPlayer()->GetGroupMemberInZone(zoneID);

	int8 instanceType = 0;

	if ( instance_zone != NULL )
		Zone(instance_zone->GetInstanceID(),zone_coords_valid, true);
	else if ( (instanceType = database.GetInstanceTypeByZoneID(zoneID) ) > 0 )
	{
		// best to check if we already have our own instance!
		InstanceData* data = GetPlayer()->GetCharacterInstances()->FindInstanceByZoneID(zoneID);
		if (data) {
			// If lockout instances check to see if we are locked out
			if (instanceType == SOLO_LOCKOUT_INSTANCE || instanceType == GROUP_LOCKOUT_INSTANCE || instanceType == RAID_LOCKOUT_INSTANCE) {
				int32 time = 0;
				// Check success timer
				if (data->last_success_timestamp > 0) {
					if (Timer::GetUnixTimeStamp() < data->last_success_timestamp + data->success_lockout_time) {
						// Timer has not expired yet can't re enter
						LogWrite(INSTANCE__DEBUG, 0, "Instance", "Success lockout not expired for character %s in zone %u", GetPlayer()->GetName(), zoneID);
						time = (data->last_success_timestamp + data->success_lockout_time) - Timer::GetUnixTimeStamp();
					}
				}

				// Check failure timer
				if (data->last_failure_timestamp > 0) {
					if (Timer::GetUnixTimeStamp() < data->last_failure_timestamp + data->failure_lockout_time) {
						// Timer has not expired yet
						LogWrite(INSTANCE__DEBUG, 0, "Instance", "Failure lockout not expired for character %s in zone %u", GetPlayer()->GetName(), zoneID);
						time = (data->last_failure_timestamp + data->failure_lockout_time) - Timer::GetUnixTimeStamp();
					}
				}

				// Time > 0 then we are locked out, make the message and send it and return true
				if (time > 0) {
					string time_msg = "";
					int16 hour;
					int8 min;
					int8 sec;
					hour = time / 3600;
					time = time % 3600;
					min = time / 60;
					time = time % 60;
					sec = time;

					if (hour > 0) {
						char temp[10];
						sprintf(temp, " %i", hour);
						time_msg.append(temp);
						time_msg.append(" hour");
						time_msg.append((hour > 1) ? "s" : "");
					}
					if (min > 0) {
						char temp[5];
						sprintf(temp, " %i", min);
						time_msg.append(temp);
						time_msg.append(" minute");
						time_msg.append((min > 1) ? "s" : "");
					}
					// Only add seconds if minutes and hours are 0
					if (hour == 0 && min == 0 && sec > 0) {
						char temp[5];
						sprintf(temp, " %i", sec);
						time_msg.append(temp);
						time_msg.append(" second");
						time_msg.append((sec > 1) ? "s" : "");
					}

					Message(CHANNEL_COLOR_YELLOW, "You may not enter again for%s.", time_msg.c_str());
					return true;
				}
			}

			// Need to update `character_instances` table with new timestamps (for persistent) and instance id's
			instance_zone = zone_list.GetByInstanceID(data->instance_id,zoneID);

			// if we got an instance_zone and the instance_id from the data is 0 or data instance id is not the same as the zone instance id then update values
			if (instance_zone && (data->instance_id == 0 || data->instance_id != instance_zone->GetInstanceID())) {
				if (instanceType == SOLO_PERSIST_INSTANCE || instanceType == GROUP_PERSIST_INSTANCE || instanceType == RAID_PERSIST_INSTANCE) {
					database.UpdateCharacterInstance(GetCharacterID(), string(instance_zone->GetZoneName()), instance_zone->GetInstanceID(), 1, Timer::GetUnixTimeStamp());
					data->last_success_timestamp = Timer::GetUnixTimeStamp();
				}
				else
					database.UpdateCharacterInstance(GetCharacterID(), string(instance_zone->GetZoneName()), instance_zone->GetInstanceID());

				data->instance_id = instance_zone->GetInstanceID();
			}
		}
		else {
			switch(instanceType)
			{
			case SOLO_LOCKOUT_INSTANCE:
			case GROUP_LOCKOUT_INSTANCE:
			case RAID_LOCKOUT_INSTANCE:
				{
					instance_zone = zone_list.GetByInstanceID(0,zoneID);
					if (instance_zone) {
						// once lockout instance zone shuts down you can't renenter if you have a lockout or if you don't you get a new zone
						// so delete `instances` entry for the zone when it shuts down.
						int32 db_id = database.AddCharacterInstance(GetPlayer()->GetCharacterID(), instance_zone->GetInstanceID(), string(instance_zone->GetZoneName()), instance_zone->GetInstanceType(), 0, 0, instance_zone->GetDefaultLockoutTime(), instance_zone->GetDefaultReenterTime());

						if (db_id > 0)
							GetPlayer()->GetCharacterInstances()->AddInstance(db_id, instance_zone->GetInstanceID(), 0, 0, instance_zone->GetDefaultLockoutTime(), instance_zone->GetDefaultReenterTime(), zoneID, instance_zone->GetInstanceType(), string(instance_zone->GetZoneName()));
					}
					break;
				}
			case SOLO_PERSIST_INSTANCE:
			case GROUP_PERSIST_INSTANCE:
			case RAID_PERSIST_INSTANCE:
				{
					instance_zone = zone_list.GetByInstanceID(0,zoneID);
					if (instance_zone) {
						int32 db_id = database.AddCharacterInstance(GetPlayer()->GetCharacterID(), instance_zone->GetInstanceID(), string(instance_zone->GetZoneName()), instance_zone->GetInstanceType(), Timer::GetUnixTimeStamp(), 0, instance_zone->GetDefaultLockoutTime(), instance_zone->GetDefaultReenterTime());

						if (db_id > 0)
							GetPlayer()->GetCharacterInstances()->AddInstance(db_id, instance_zone->GetInstanceID(), Timer::GetUnixTimeStamp(), 0, instance_zone->GetDefaultLockoutTime(), instance_zone->GetDefaultReenterTime(), zoneID, instance_zone->GetInstanceType(), string(instance_zone->GetZoneName()));
					}
					break;
				}
			case PUBLIC_INSTANCE:
			case TRADESKILL_INSTANCE:
				{
					// if its public/tradeskill, look for a public already setup
					instance_zone = zone_list.GetByLowestPopulation(zoneID);
					if (instance_zone) {
						// Check the current population against the max population, if greater or equal start a new version
						if (instance_zone->GetClientCount() >= rule_manager.GetGlobalRule(R_Zone, MaxPlayers)->GetInt32())
							instance_zone = zone_list.GetByInstanceID(0, zoneID);
					}
					else
						instance_zone = zone_list.GetByInstanceID(0, zoneID);

					break;
				}

			case PERSONAL_HOUSE_INSTANCE:
			case GUILD_HOUSE_INSTANCE:
				{
					// Because of the way housing works (need to load a specific instance id supplied in a packet) we can't
					// use this function without some rework, so it will all be handled in Client::HandlePacket()
					// with the OP_EnterHouseMsg opcode
					break;
				}

			case QUEST_INSTANCE:
				{
					instance_zone = zone_list.GetByInstanceID(0, zoneID);
					break;
					/*
					ALTER TABLE `zones` CHANGE COLUMN `instance_type` `instance_type` ENUM('NONE','GROUP_LOCKOUT_INSTANCE','GROUP_PERSIST_INSTANCE','RAID_LOCKOUT_INSTANCE','RAID_PERSIST_INSTANCE','SOLO_LOCKOUT_INSTANCE','SOLO_PERSIST_INSTANCE','TRADESKILL_INSTANCE','PUBLIC_INSTANCE','PERSONAL_HOUSE_INSTANCE','GUILD_HOUSE_INSTANCE','QUEST_INSTANCE') NOT NULL DEFAULT 'NONE' COLLATE 'latin1_general_ci' AFTER `start_zone`;
					*/
				}

			default:
				{
					// NONE
				}

			}

		}

		if ( instance_zone != NULL )
			Zone(instance_zone, zone_coords_valid );
	}


	if ( instance_zone != NULL )
		return true;
	else
		return false;
}

bool Client::GotoSpawn(const char* search_name){
	Spawn* target = 0;
	if(search_name || GetPlayer()->GetTarget()){
		Client* search_client = 0;
		if(search_name){
			target = GetCurrentZone()->FindSpawn(GetPlayer(), search_name);
			if(!target){
				search_client = zone_list.GetClientByCharName(search_name);
				if(search_client)
					target = search_client->GetPlayer();
			}
		}
		else
			target = GetPlayer()->GetTarget();
		if(target && target != GetPlayer()){
			GetPlayer()->SetX(target->GetX());
			GetPlayer()->SetY(target->GetY());
			GetPlayer()->SetZ(target->GetZ());
			GetPlayer()->SetHeading(target->GetHeading());
			GetPlayer()->SetLocation(target->GetLocation());
			Message(CHANNEL_COLOR_YELLOW, "Warping to '%s'", target->GetName());
		}
		else if(target)
			Message(CHANNEL_COLOR_RED,"Error: You cannot goto yourself!");
		if(search_client && search_client->GetCurrentZone() != GetCurrentZone())
			Zone(search_client->GetCurrentZone()->GetZoneName(), false);
		else if(target){
			EQ2Packet* app = GetPlayer()->Move(target->GetX(), target->GetY(), target->GetZ(), GetVersion());
			if(app)
				QueuePacket(app);
		}
	}

	if(!target)
		return false;
	else
		return true;
}

bool Client::CheckZoneAccess(const char* zoneName) {

	LogWrite(CCLIENT__DEBUG, 0, "Client", "Zone access check for %s (%u), client: %u",zoneName, database.GetZoneID(zoneName), GetVersion());

	ZoneServer* zone = zone_list.Get(zoneName,false);

	// JA: implemented /zone lock|unlock commands (2012.07.28)
	if( zone && zone->GetZoneLockState() )
	{
		LogWrite(CCLIENT__DEBUG, 0, "Client", "Zone currently LOCKED: '%s' (%ul)", zoneName, zone->GetZoneID());
		Message(CHANNEL_COLOR_RED, "This zone is locked, and you don't have the key! (%s).", zoneName);
		return false;
	}

	sint16 zoneMinStatus = 0;
	int16 zoneMinLevel = 0;
	int16 zoneMaxLevel = 0;
	int16 zoneMinVersion = 0;
	if ( !zone )
	{
		LogWrite(CCLIENT__DEBUG, 0, "Client", "Grabbing zone requirements for %s", zoneName);
		bool success = database.GetZoneRequirements(zoneName, &zoneMinStatus, &zoneMinLevel, &zoneMaxLevel, &zoneMinVersion);

		if ( !success ) { // couldn't even find the zone, this shouldn't happen though..
			return true;
		}
	}
	else
	{
		zoneMinStatus = zone->GetMinimumStatus();
		zoneMinLevel = zone->GetMinimumLevel();
		zoneMaxLevel = zone->GetMaximumLevel();
		zoneMinVersion = zone->GetMinimumVersion();
	}

	LogWrite(CCLIENT__DEBUG, 0, "Client", "Access Requirements: status %i, level %i - %i, req >= %i version", zoneMinStatus, zoneMinLevel, zoneMaxLevel, zoneMinVersion);

	// use ZoneLevelOverrideStatus in both min_level and max_level checks
	sint16 ZoneLevelOverrideStatus = rule_manager.GetGlobalRule(R_Zone, MinZoneLevelOverrideStatus)->GetSInt16();

	if ( (zoneMinVersion > 0) && (GetVersion() < zoneMinVersion) )
	{
		LogWrite(CCLIENT__DEBUG, 0, "Client", "Zone MinVersion of %i challenge...", zoneMinVersion);
		Message(CHANNEL_COLOR_RED, "You do not have the required expansion pack to enter here (%s).", database.GetExpansionIDByVersion(zoneMinVersion).c_str());
		LogWrite(CCLIENT__DEBUG, 0, "Client", "Client denied access to zone '%s' (version req: %i)", zoneName, zoneMinVersion);
		return false;
	}


	if ( (zoneMinLevel > 1) && (player->GetLevel() < zoneMinLevel) )
	{
		if( ZoneLevelOverrideStatus && ZoneLevelOverrideStatus > GetAdminStatus() )
		{
			LogWrite(CCLIENT__DEBUG, 0, "Client", "Player denied access to zone '%s' (level req: %i)", zoneName, player->GetLevel());
			Message(CHANNEL_COLOR_RED, "Your level is too low to enter here (%s)", zoneMinLevel);
			return false;
		}
	}

	if ( (zoneMaxLevel > 1) && (player->GetLevel() > zoneMaxLevel) )
	{
		if( ZoneLevelOverrideStatus && ZoneLevelOverrideStatus > GetAdminStatus() )
		{
			LogWrite(CCLIENT__DEBUG, 0, "Client", "Player denied access to zone '%s' (level req: %i)", zoneName, player->GetLevel());
			Message(CHANNEL_COLOR_RED, "Your level is too high to enter here (%s)", zoneMaxLevel);
			return false;
		}
	}

	if ( (zoneMinStatus > 0) && (GetAdminStatus() < zoneMinStatus) )
	{
		LogWrite(CCLIENT__DEBUG, 0, "Client", "Zone MinStatus of %i challenge...", zoneMinStatus);

		sint16 ZoneAccessOverrideStatus = rule_manager.GetGlobalRule(R_Zone, MinZoneAccessOverrideStatus)->GetSInt16();
		if( ZoneAccessOverrideStatus && ZoneAccessOverrideStatus > GetAdminStatus() )
		{
			LogWrite(CCLIENT__DEBUG, 0, "Client", "Player denied access to zone '%s' (status req: %i)", zoneName, GetAdminStatus());
			Message(CHANNEL_COLOR_RED, "You do not have permission to enter here (%i).", zoneMinStatus);
			return false;
		}
	}
	return true;
}

void Client::Zone(int32 instanceid, bool set_coords, bool byInstanceID) {
	Zone(zone_list.GetByInstanceID(instanceid), set_coords);

}

void Client::Zone(ZoneServer* new_zone, bool set_coords){
	if(!new_zone) {
		LogWrite(CCLIENT__DEBUG, 0, "Client", "Zone Request Denied! No 'new_zone' value");
		return;
	}

	client_zoning = true;
	LogWrite(CCLIENT__DEBUG, 0, "Client", "%s: Setting player Resurrecting to 'true'", __FUNCTION__);
	player->SetResurrecting(true);

	LogWrite(CCLIENT__DEBUG, 0, "Client", "%s: Removing player from fighting...", __FUNCTION__);
	//GetCurrentZone()->GetCombat()->RemoveHate(player);
	
	database.SavePlayerActiveSpells(this);

	// Remove players pet from zone if there is one
	player->DismissPet((NPC*)player->GetPet());
	player->DismissPet((NPC*)player->GetCharmedPet());
	player->DismissPet((NPC*)player->GetDeityPet());
	player->DismissPet((NPC*)player->GetCosmeticPet());

	LogWrite(CCLIENT__DEBUG, 0, "Client", "%s: Removing player from current zone...", __FUNCTION__);
	GetCurrentZone()->RemoveSpawn(player, false);

	LogWrite(CCLIENT__DEBUG, 0, "Client", "%s: Setting zone to '%s'...", __FUNCTION__, new_zone->GetZoneName());
	SetCurrentZone(new_zone);

	// Do smoething with group to show that the person is ozning

	UpdateTimeStampFlag(ZONE_UPDATE_FLAG);

	if(set_coords)
	{
		LogWrite(CCLIENT__DEBUG, 0, "Client", "%s: Zoning player to coordinates x: %2f, y: %2f, z: %2f, heading: %2f in zone '%s'...", 
			__FUNCTION__, 
			GetCurrentZone()->GetSafeX(),
			GetCurrentZone()->GetSafeY(),
			GetCurrentZone()->GetSafeZ(),
			GetCurrentZone()->GetSafeHeading(), 
			new_zone->GetZoneName()
			);
		player->SetX(GetCurrentZone()->GetSafeX());
		player->SetY(GetCurrentZone()->GetSafeY());
		player->SetZ(GetCurrentZone()->GetSafeZ());
		player->SetHeading(GetCurrentZone()->GetSafeHeading());
	}

	LogWrite(CCLIENT__DEBUG, 0, "Client", "%s: Saving Player info...", __FUNCTION__);
	Save();

	char* new_zone_ip = 0;
	struct in_addr in;
	in.s_addr = this->GetIP();
	if(strncmp(inet_ntoa(in), "192.168",7)==0 && strlen(net.GetInternalWorldAddress()) > 0)
		new_zone_ip = net.GetInternalWorldAddress();
	else
		new_zone_ip = net.GetWorldAddress();
	LogWrite(CCLIENT__DEBUG, 0, "Client", "%s: New Zone IP '%s'...", __FUNCTION__, new_zone_ip);

	int32 key = Timer::GetUnixTimeStamp();
	LogWrite(CCLIENT__DEBUG, 0, "Client", "%s: Sending ZoneChangeMsg...", __FUNCTION__);
	ClientPacketFunctions::SendZoneChange(this, new_zone_ip, net.GetWorldPort(), key);

	LogWrite(CCLIENT__DEBUG, 0, "Client", "%s: Sending to zone_auth.AddAuth...", __FUNCTION__);
	zone_auth.AddAuth(new ZoneAuthRequest(GetAccountID(), player->GetName(), key));

}

void Client::Zone(const char* new_zone, bool set_coords)
{
	LogWrite(CCLIENT__DEBUG, 0, "Client", "Zone Request to '%s'", new_zone);
	Zone(zone_list.Get(new_zone), set_coords);
}

float Client::DistanceFrom(Client* client){
	float ret = 0;
	if(client && client != this){
		ret = pow(player->GetX() - client->player->GetX(), 2) + pow(player->GetY() - client->player->GetY(), 2) + pow(player->GetZ() - client->player->GetZ(), 2);
		ret = sqrt(ret);
	}

	return ret;
}

void Client::DetermineCharacterUpdates ( ) {
	ServerPacket* outpack = new ServerPacket(ServerOP_BasicCharUpdate, sizeof(CharDataUpdate_Struct));
	CharDataUpdate_Struct* cdu = (CharDataUpdate_Struct*)outpack->pBuffer;
	cdu->account_id = GetAccountID ( );
	cdu->char_id = GetCharacterID ( );
	int32 timestamp = Timer::GetUnixTimeStamp();
	int8 flag = GetTimeStampFlag( );
	if(flag&LEVEL_UPDATE_FLAG)
	{
		cdu->update_field = LEVEL_UPDATE_FLAG;
		cdu->update_data = player->GetLevel ( );
		loginserver.SendPacket(outpack);
	}
	if(flag&CLASS_UPDATE_FLAG)
	{
		cdu->update_field = CLASS_UPDATE_FLAG;
		cdu->update_data = player->GetAdventureClass();
		loginserver.SendPacket(outpack);
	}
	if(flag&GENDER_UPDATE_FLAG)
	{
		cdu->update_field = GENDER_UPDATE_FLAG;
		cdu->update_data = player->GetGender ( );
		loginserver.SendPacket(outpack);
	}
	if(flag&DELETE_UPDATE_FLAG) {
		LogWrite(MISC__TODO, 1, "TODO", "Delete update req in func: %s, line: %i", __FUNCTION__, __LINE__);
	}

	safe_delete(outpack); // Zone, armor and name use a different structure

	if(flag&RACE_UPDATE_FLAG)
	{
		outpack = new ServerPacket(ServerOP_RaceUpdate, sizeof(RaceUpdate_Struct));
		RaceUpdate_Struct* ru = (RaceUpdate_Struct*)outpack->pBuffer;
		ru->account_id = GetAccountID ( );
		ru->char_id = GetCharacterID ( );
		ru->race = player->GetRace ( );
		ru->model_type = player->GetModelType ( );
		loginserver.SendPacket ( outpack );
		safe_delete(outpack);
	}

	if(flag&ZONE_UPDATE_FLAG){
		ServerPacket* outpack = new ServerPacket(ServerOP_ZoneUpdate, CHARZONESTRUCT_MAXSIZE);
		memset(outpack->pBuffer, 0, CHARZONESTRUCT_MAXSIZE);
		CharZoneUpdate_Struct* czu = (CharZoneUpdate_Struct*)outpack->pBuffer;
		czu->account_id = GetAccountID();
		czu->char_id = GetCharacterID();
		czu->zone_id = GetCurrentZone()->GetZoneID();
		const char* zone_file = GetCurrentZone()->GetZoneFile();
		czu->zone_length = strlen(zone_file);
		if(czu->zone_length > 64)
			czu->zone_length = 64;
		strncpy(czu->new_zone, zone_file, czu->zone_length);
		loginserver.SendPacket(outpack);
		safe_delete(outpack);
	}
	if(flag&ARMOR_UPDATE_FLAG) {
		LogWrite(MISC__TODO, 1, "TODO", "Armor update req in func: %s, line: %i", __FUNCTION__, __LINE__);
	}
	if(flag&NAME_UPDATE_FLAG) {
		LogWrite(MISC__TODO, 1, "TODO", "Name update req in func: %s, line: %i", __FUNCTION__, __LINE__);
	}

	database.UpdateCharacterTimeStamp(GetAccountID ( ),GetCharacterID ( ),timestamp);

}

void Client::Save(){
	if(current_zone){
		DetermineCharacterUpdates();

		UpdateCharacterInstances();

		database.Save(this);
		if(GetPlayer()->UpdateQuickbarNeeded()){
			database.SaveQuickBar(GetCharacterID(), GetPlayer()->GetQuickbar());
			GetPlayer()->ResetQuickbarNeeded();
		}
		database.SaveItems(this);
		database.SaveBuyBacks(this);

		GetPlayer()->SaveHistory();
		GetPlayer()->SaveLUAHistory();
	}

}

void Client::UpdateCharacterInstances() {
	if (GetPlayer() != NULL)
		GetPlayer()->GetCharacterInstances()->ProcessInstanceTimers(GetPlayer());

	/*if ( GetPlayer() != NULL )
	{
	// determine the last timestamp then get a new one, determine the difference in the timestamp
	// to use for applying the update to each instances timer
	int32 lastSaveTS = GetLastSavedTimeStamp();
	int32 newSaveTS = Timer::GetUnixTimeStamp();
	int32 diffTS = newSaveTS - lastSaveTS;

	// update instance timers
	GetPlayer()->GetCharacterInstances().ProcessInstanceTimers(GetPlayer(),diffTS);

	// update with the new timestamp and save the db
	this->SetLastSavedTimeStamp(newSaveTS);
	}*/

}

void Client::HandleVerbRequest(EQApplicationPacket* app){
	PacketStruct* packet = configReader.getStruct("WS_EntityVerbsRequest", GetVersion());
	if(packet){
		packet->LoadPacketData(app->pBuffer, app->size);
		int32 spawn_id = packet->getType_int32_ByName("spawn_id");
		PacketStruct* out = configReader.getStruct("WS_EntityVerbsResponse", GetVersion());
		Spawn* spawn = GetPlayer()->GetSpawnWithPlayerID(spawn_id);
		vector<EntityCommand*> commands;
		vector<EntityCommand*> delete_commands;
		if(out && spawn) {
			for(int32 i=0;i<spawn->primary_command_list.size();i++)
				commands.push_back(spawn->primary_command_list[i]);

			for(int32 i=0;i<spawn->secondary_command_list.size();i++)
				commands.push_back(spawn->secondary_command_list[i]);

			if (spawn->IsPlayer()) {
				if (!player->CanAttackTarget((Player*)spawn)) {
					delete_commands.push_back(player->CreateEntityCommand("Inspect", 10000, "inspect_player", "", 0, 0));
					delete_commands.push_back(player->CreateEntityCommand("Who", 10000, "who", "", 0, 0));

					if (player->IsFriend(spawn->GetName())) {
						delete_commands.push_back(player->CreateEntityCommand("remove from friends list", 10000, "friend_remove", "", 0, 0));
					}	else {
						delete_commands.push_back(player->CreateEntityCommand("add to friends list", 10000, "friend_add", "", 0, 0));
					}

					if (player->IsIgnored(spawn->GetName())) {
						delete_commands.push_back(player->CreateEntityCommand("remove from ignore list", 10000, "ignore_remove", "", 0, 0));
					} else {
						delete_commands.push_back(player->CreateEntityCommand("add to ignore list", 10000, "ignore_add", "", 0, 0));
					}

					if (((Player *)spawn)->GetGroupMemberInfo()) {
						if (player->IsGroupMember((Player *)spawn) && player->GetGroupMemberInfo()->leader) { //group leader
							delete_commands.push_back(player->CreateEntityCommand("kick from group", 10000, "kickfromgroup", "", 0, 0));
							delete_commands.push_back(player->CreateEntityCommand("make group leader", 10000, "makeleader", "", 0, 0));
						}
					} else if (!player->GetGroupMemberInfo() || (player->GetGroupMemberInfo()->leader && world.GetGroupManager()->GetGroupSize(player->GetGroupMemberInfo()->group_id) < 6)) {
						delete_commands.push_back(player->CreateEntityCommand("invite to group", 10000, "invite", "", 0, 0));
					}
				} else {
					delete_commands.push_back(player->CreateEntityCommand("attack", 10000, "attack", "", 0, 0));
				}
			}

			commands.insert(commands.end(), delete_commands.begin(), delete_commands.end());

			out->setDataByName("spawn_id", spawn_id);
			out->setArrayLengthByName("num_verbs", commands.size());
			
			for(int32 i=0;i<commands.size();i++){
				out->setArrayDataByName("command", commands[i]->command.c_str(), i);
				out->setArrayDataByName("distance", commands[i]->distance, i);
				if(commands[i]->error_text.length() == 0)
					out->setArrayAddToPacketByName("error", false, i);
				else{
					out->setArrayDataByName("display_error", 1, i);
					out->setArrayDataByName("error", commands[i]->error_text.c_str(), i);
				}
				out->setArrayDataByName("display_text", commands[i]->name.c_str(), i);
			}
			EQ2Packet* outapp = out->serialize();
			//DumpPacket(outapp);
			QueuePacket(outapp);
			safe_delete(out);
			for(int32 i=0;i<delete_commands.size();i++){
				safe_delete(delete_commands[i]);
			}
		}
		safe_delete(packet);
	}

}

void Client::SkillChanged(Skill* skill, int16 previous_value, int16 new_value){
	Message(CHANNEL_COLOR_SKILL, "You get %s at %s (%i/%i).", new_value > previous_value ? "better" : "worse", skill->name.data.c_str(), new_value, skill->max_val);
	char tmp[200] = {0};
	sprintf(tmp, "\\#6EFF6EYou get %s at \12\\#C8FFC8%s\\#6EFF6E! (%i/%i)", new_value > previous_value ? "better" : "worse", skill->name.data.c_str(), new_value, skill->max_val);
	SendPopupMessage(6, tmp, new_value > previous_value ? "skill_up" : "skill_down", 2.75, 0xFF, 0xFF, 0xFF);
	EQ2Packet* app = GetPlayer()->skill_list.GetSkillPacket(GetVersion());
	if(app)
		QueuePacket(app);

}

void Client::SendPopupMessage(int8 unknown, const char* text, const char* type, float size, int8 red, int8 green, int8 blue)
{
	/* JA notes on the unknown:
		2 = ding glimmer
		16 = Achievement Unlocked
		6 no longer does anything
	*/

	PacketStruct* packet = configReader.getStruct("WS_OnScreenMsg", GetVersion());
	if(packet){
		packet->setDataByName("unknown", unknown);
		packet->setMediumStringByName("text", text);
		if (type && strlen(type) > 0)
			packet->setMediumStringByName("message_type", type);
		packet->setDataByName("size", size);
		packet->setDataByName("red", red);
		packet->setDataByName("green", green);
		packet->setDataByName("blue", blue);
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

}

void Client::ChangeLevel(int16 old_level, int16 new_level){
	if (new_level < 1) {
		SimpleMessage(CHANNEL_COLOR_RED, "You cannot be lower than level 1!");
		return;
	}

	if(player->GetLevel() != new_level){
		player->SetLevel(new_level);
		if (player->GetGroupMemberInfo()) {
			player->UpdateGroupMemberInfo();
			world.GetGroupManager()->SendGroupUpdate(player->GetGroupMemberInfo()->group_id);
		}
	}

	if (new_level > old_level)
		player->UpdatePlayerHistory(HISTORY_TYPE_XP, HISTORY_SUBTYPE_ADVENTURE, new_level, player->GetAdventureClass());

	if (player->GetPet()) {
		NPC* pet = (NPC*)player->GetPet();
		if (pet->GetMaxPetLevel() == 0 || new_level <= pet->GetMaxPetLevel()) {
			pet->SetLevel(new_level);
			pet->ScalePet();
			PacketStruct* command_packet=configReader.getStruct("WS_CannedEmote", GetVersion());
			if (command_packet){
				command_packet->setDataByName("spawn_id", player->GetIDWithPlayerSpawn(pet));
				command_packet->setDataByName("anim_type",1753);	
				QueuePacket(command_packet->serialize());
				safe_delete(command_packet);
			}
		}
	}

	PacketStruct* level_update = configReader.getStruct("WS_LevelChanged", GetVersion());
	if(level_update){
		level_update->setDataByName("old_level", old_level);
		level_update->setDataByName("new_level", new_level);
		QueuePacket(level_update->serialize());
		safe_delete(level_update);
		GetCurrentZone()->StartZoneSpawnsForLevelThread(this);
	}

	PacketStruct* command_packet=configReader.getStruct("WS_CannedEmote", GetVersion());
	if (command_packet){
		command_packet->setDataByName("spawn_id", player->GetIDWithPlayerSpawn(player));
		command_packet->setDataByName("anim_type",1753);	
		QueuePacket(command_packet->serialize());
		safe_delete(command_packet);
	}

	if (!player->get_character_flag(CF_ENABLE_CHANGE_LASTNAME) && new_level >= rule_manager.GetGlobalRule(R_Player, MinLastNameLevel)->GetInt8())
		player->set_character_flag(CF_ENABLE_CHANGE_LASTNAME);

	player->GetSkills()->IncreaseAllSkillCaps(5 * (new_level - old_level));
	SendNewSpells(player->GetAdventureClass());
	SendNewSpells(classes.GetBaseClass(player->GetAdventureClass()));
	SendNewSpells(classes.GetSecondaryBaseClass(player->GetAdventureClass()));
	

	GetPlayer()->ChangePrimaryWeapon();
	GetPlayer()->ChangeSecondaryWeapon();
	GetPlayer()->ChangeRangedWeapon();
	GetPlayer()->GetInfoStruct()->level = new_level;

	LogWrite(MISC__TODO, 1, "TODO", "Get new HP/POWER/stat based on default values from DB\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);

	GetPlayer()->SetTotalHPBase(new_level*new_level*2+40);
	GetPlayer()->SetTotalPowerBase((sint32)(new_level*new_level*2.1+45));
	GetPlayer()->CalculateBonuses();
	GetPlayer()->SetHP(GetPlayer()->GetTotalHP());
	GetPlayer()->SetPower(GetPlayer()->GetTotalPower());
	GetPlayer()->GetInfoStruct()->agi_base = new_level*2+15;
	GetPlayer()->GetInfoStruct()->intel_base = new_level*2+15;
	GetPlayer()->GetInfoStruct()->wis_base = new_level*2+15;
	GetPlayer()->GetInfoStruct()->str_base = new_level*2+15;
	GetPlayer()->GetInfoStruct()->sta_base = new_level*2+15;
	GetPlayer()->GetInfoStruct()->cold_base = (int16)(new_level*1.5+10);
	GetPlayer()->GetInfoStruct()->heat_base = (int16)(new_level*1.5+10);
	GetPlayer()->GetInfoStruct()->disease_base = (int16)(new_level*1.5+10);
	GetPlayer()->GetInfoStruct()->mental_base = (int16)(new_level*1.5+10);
	GetPlayer()->GetInfoStruct()->magic_base = (int16)(new_level*1.5+10);
	GetPlayer()->GetInfoStruct()->divine_base = (int16)(new_level*1.5+10);
	GetPlayer()->GetInfoStruct()->poison_base = (int16)(new_level*1.5+10);
	GetPlayer()->SetHPRegen((int)(new_level*.75)+(int)(new_level/10)+3);
	GetPlayer()->SetPowerRegen(new_level+(int)(new_level/10)+4);
	GetPlayer()->GetInfoStruct()->poison_base = (int16)(new_level*1.5+10);
	UpdateTimeStampFlag ( LEVEL_UPDATE_FLAG );
	GetPlayer()->SetCharSheetChanged(true);

	Message(CHANNEL_COLOR_EXP,"You are now level %i!", new_level);
	LogWrite(WORLD__DEBUG, 0, "World", "Player: %s leveled from %u to %u", GetPlayer()->GetName(), old_level, new_level);
	GetPlayer()->GetSkills()->SetSkillCapsByType(1, 5*new_level);
	GetPlayer()->GetSkills()->SetSkillCapsByType(3, 5*new_level);
	GetPlayer()->GetSkills()->SetSkillCapsByType(6, 5*new_level);
	GetPlayer()->GetSkills()->SetSkillCapsByType(13, 5*new_level);

	Guild* guild = GetPlayer()->GetGuild();
	if (guild) {
		int8 event_type = 0; 
		if (new_level < 10)
			event_type = GUILD_EVENT_GAINS_ADV_LEVEL_1_10;
		else if (new_level == 10)
			event_type = GUILD_EVENT_GAINS_ADV_LEVEL_10;
		else if (new_level < 20)
			event_type = GUILD_EVENT_GAINS_ADV_LEVEL_11_20;
		else if (new_level == 20)
			event_type = GUILD_EVENT_GAINS_ADV_LEVEL_20;
		else if (new_level < 30)
			event_type = GUILD_EVENT_GAINS_ADV_LEVEL_21_30;
		else if (new_level == 30)
			event_type = GUILD_EVENT_GAINS_ADV_LEVEL_30;
		else if (new_level < 40)
			event_type = GUILD_EVENT_GAINS_ADV_LEVEL_31_40;
		else if (new_level == 40)
			event_type = GUILD_EVENT_GAINS_ADV_LEVEL_40;
		else if (new_level < 50)
			event_type = GUILD_EVENT_GAINS_ADV_LEVEL_41_50;
		else if (new_level == 50)
			event_type = GUILD_EVENT_GAINS_ADV_LEVEL_50;
		else if (new_level < 60)
			event_type = GUILD_EVENT_GAINS_ADV_LEVEL_51_60;
		else if (new_level == 60)
			event_type = GUILD_EVENT_GAINS_ADV_LEVEL_60;
		else if (new_level < 70)
			event_type = GUILD_EVENT_GAINS_ADV_LEVEL_61_70;
		else if (new_level == 70)
			event_type = GUILD_EVENT_GAINS_ADV_LEVEL_70;
		else if (new_level < 80)
			event_type = GUILD_EVENT_GAINS_ADV_LEVEL_71_80;
		else if (new_level == 80)
			event_type = GUILD_EVENT_GAINS_ADV_LEVEL_80;
		guild->AddNewGuildEvent(event_type, "%s has gained an adventure level and is now a level %u %s.", Timer::GetUnixTimeStamp(), true, GetPlayer()->GetName(), new_level, classes.GetClassNameCase(GetPlayer()->GetAdventureClass()).c_str());
		guild->SendMessageToGuild(event_type, "%s has gained an adventure level and is now a level %u %s.", GetPlayer()->GetName(), new_level, classes.GetClassNameCase(GetPlayer()->GetAdventureClass()).c_str());
		guild->UpdateGuildMemberInfo(GetPlayer());
		guild->SendGuildMember(GetPlayer());
		guild->SendGuildMemberList();	
	}

	// Need to send the trait list every time the players level changes
	// Also need to force the char sheet update or else there can be a large delay from when you level
	// to when you are actually able to select traits.
	QueuePacket(GetPlayer()->GetPlayerInfo()->serialize(GetVersion()));
	QueuePacket(master_trait_list.GetTraitListPacket(this));
}

void Client::ChangeTSLevel(int16 old_level, int16 new_level){
	if (new_level < 1) {
		SimpleMessage(CHANNEL_COLOR_RED, "You cannot be lower than level 1!");
			return;
	}
	if ((player->GetTSLevel() >= 9 && player->GetTradeskillClass() == 1) || (player->GetTSLevel() >= 19 && (player->GetTradeskillClass() == 1 || player->GetTradeskillClass() == 2 || player->GetTradeskillClass() == 6 || player->GetTradeskillClass() == 10))) {
		SimpleMessage(CHANNEL_COLOR_YELLOW, "You can not gain levels until you select your next class!");
		return;
	}

	if (new_level > old_level)
		player->UpdatePlayerHistory(HISTORY_TYPE_XP, HISTORY_SUBTYPE_TRADESKILL, new_level, player->GetTradeskillClass());

	if(player->GetTSLevel() != new_level){
		player->SetTSLevel(new_level);
		if (player->GetGroupMemberInfo()) {
			player->UpdateGroupMemberInfo();
			world.GetGroupManager()->SendGroupUpdate(player->GetGroupMemberInfo()->group_id);
		}
	}
	// Only tradeskill skills should increace, and then only those related to your class
	// player->GetSkills()->IncreaseAllSkillCaps(5 * (new_level - old_level));
	PacketStruct* level_update = configReader.getStruct("WS_LevelChanged", GetVersion());
	if(level_update){
		level_update->setDataByName("old_level", old_level);
		level_update->setDataByName("new_level", new_level);
		level_update->setDataByName("type", 1);
		QueuePacket(level_update->serialize());
		safe_delete(level_update);
	}
	// Need to make tradeskill versions of the following
	//SendNewSpells(player->GetAdventureClass());
	//SendNewSpells(classes.GetBaseClass(player->GetAdventureClass()));
	//SendNewSpells(classes.GetSecondaryBaseClass(player->GetAdventureClass()));
	PacketStruct* command_packet=configReader.getStruct("WS_CannedEmote", GetVersion());
	if (command_packet){
		command_packet->setDataByName("spawn_id", GetPlayer()->GetIDWithPlayerSpawn(GetPlayer()));
		command_packet->setDataByName("anim_type",1753);	
		QueuePacket(command_packet->serialize());
		safe_delete(command_packet);
	}
	GetPlayer()->GetInfoStruct()->tradeskill_level = new_level;
	GetPlayer()->SetTSLevel(new_level);

	
	UpdateTimeStampFlag ( LEVEL_UPDATE_FLAG );
	GetPlayer()->SetCharSheetChanged(true);
	Message(CHANNEL_COLOR_WHITE,"Your tradeskill level is now %i!", new_level);
	LogWrite(WORLD__DEBUG, 0, "World", "Player: %s leveled from %u to %u", GetPlayer()->GetName(), old_level, new_level);
	//GetPlayer()->GetSkills()->SetSkillCapsByType(1, 5*new_level);
	//GetPlayer()->GetSkills()->SetSkillCapsByType(3, 5*new_level);
	//GetPlayer()->GetSkills()->SetSkillCapsByType(6, 5*new_level);
	//GetPlayer()->GetSkills()->SetSkillCapsByType(13, 5*new_level);
	Guild* guild = GetPlayer()->GetGuild();
	if (guild) {
		int8 event_type = 0; 
		if (new_level < 10)
			event_type = GUILD_EVENT_GAINS_TS_LEVEL_1_10;
		else if (new_level == 10)
			event_type = GUILD_EVENT_GAINS_TS_LEVEL_10;
		else if (new_level < 20)
			event_type = GUILD_EVENT_GAINS_TS_LEVEL_11_20;
		else if (new_level == 20)
			event_type = GUILD_EVENT_GAINS_TS_LEVEL_20;
		else if (new_level < 30)
			event_type = GUILD_EVENT_GAINS_TS_LEVEL_21_30;
		else if (new_level == 30)
			event_type = GUILD_EVENT_GAINS_TS_LEVEL_30;
		else if (new_level < 40)
			event_type = GUILD_EVENT_GAINS_TS_LEVEL_31_40;
		else if (new_level == 40)
			event_type = GUILD_EVENT_GAINS_TS_LEVEL_40;
		else if (new_level < 50)
			event_type = GUILD_EVENT_GAINS_TS_LEVEL_41_50;
		else if (new_level == 50)
			event_type = GUILD_EVENT_GAINS_TS_LEVEL_50;
		else if (new_level < 60)
			event_type = GUILD_EVENT_GAINS_TS_LEVEL_51_60;
		else if (new_level == 60)
			event_type = GUILD_EVENT_GAINS_TS_LEVEL_60;
		else if (new_level < 70)
			event_type = GUILD_EVENT_GAINS_TS_LEVEL_61_70;
		else if (new_level == 70)
			event_type = GUILD_EVENT_GAINS_TS_LEVEL_70;
		else if (new_level < 80)
			event_type = GUILD_EVENT_GAINS_TS_LEVEL_71_80;
		else if (new_level == 80)
			event_type = GUILD_EVENT_GAINS_TS_LEVEL_80;
		guild->AddNewGuildEvent(event_type, "%s has gained a tradeskill level and is now a level %u %s.", Timer::GetUnixTimeStamp(), true, GetPlayer()->GetName(), new_level, classes.GetClassNameCase(GetPlayer()->GetTradeskillClass() + 42).c_str());
		guild->SendMessageToGuild(event_type, "%s has gained a tradeskill level and is now a level %u %s.", GetPlayer()->GetName(), new_level, classes.GetClassNameCase(GetPlayer()->GetTradeskillClass() + 42).c_str());
		guild->UpdateGuildMemberInfo(GetPlayer());
		guild->SendGuildMember(GetPlayer());
		guild->SendGuildMemberList();	
	}

	// Need to send the trait list every time the players level changes
	// Also need to force the char sheet update or else there can be a large delay from when you level
	// to when you are actually able to select traits.
	QueuePacket(GetPlayer()->GetPlayerInfo()->serialize(GetVersion()));
	QueuePacket(master_trait_list.GetTraitListPacket(this));
}

void Client::SendPendingLoot(int32 total_coins, Entity* entity){
	if(entity)
		Loot(total_coins, player->GetPendingLootItems(entity->GetID()), entity);
}

void Client::CloseLoot(){
	PacketStruct* packet = configReader.getStruct("WS_CloseWindow", GetVersion());
	packet->setDataByName("window_id", 4);
	EQ2Packet* outapp = packet->serialize();
	//DumpPacket(outapp);
	QueuePacket(outapp);
	safe_delete(packet);
}

string Client::GetCoinMessage(int32 total_coins){
	if(total_coins == 0) {
			return " 0 Copper";
	}
	char tmp[64] = {0};
	string message = "";
	int32 val = 0;
	if(total_coins >= 1000000){
		val = total_coins / 1000000;
		total_coins -= 1000000 * val;
		sprintf(tmp, " %u Platinum", val);
		message.append(tmp);
		memset(tmp, 0, 64);
	}
	if(total_coins >= 10000){
		val = total_coins / 10000;
		total_coins -= 10000 * val;
		sprintf(tmp, " %u Gold", val);
		message.append(tmp);
		memset(tmp, 0, 64);
	}
	if(total_coins >= 100){
		val = total_coins / 100;
		total_coins -= 100 * val;
		sprintf(tmp, " %u Silver", val);
		message.append(tmp);
		memset(tmp, 0, 64);
	}
	if(total_coins > 0){
		sprintf(tmp, " %u Copper", (int32)total_coins);
		message.append(tmp);
	}

	return message;
}

void Client::Loot(int32 total_coins, vector<Item*>* items, Entity* entity){
	if(!entity){
		CloseLoot();
			return;
	}
	if(total_coins > 0){
		player->AddCoins(total_coins);
		//PlaySound("coin_cha_ching");
		string message = "";
		if(entity->GetHP() == 0){
			message = "You loot ";
			entity->SetLootCoins(0);
		}
		else
			message = "You receive ";
		message.append(GetCoinMessage(total_coins));
		if(entity->GetHP() == 0)
			message.append(" from the corpse of ").append(entity->GetName());
		int8 type = CHANNEL_COLOR_LOOT;
		if (GetVersion() >= 1208)
			type = CHANNEL_LOOT;
		else if(GetVersion() >= 973)
			type = CHANNEL_COLOR_NEW_LOOT;
		
		
		SimpleMessage(type, message.c_str());
	}
	if(!items || items->size() == 0)
		CloseLoot();
	PacketStruct* packet = configReader.getStruct("WS_UpdateLoot", GetVersion());
	if(packet){
		vector<Item*>::iterator itr;
		int32 packet_size = 0;
		if(items && items->size() > 0){
			packet->setDataByName("loot_count", items->size());
			packet->setDataByName("display", 1);
		}
		packet->setDataByName("unknown2", 1);
		if (version >= 1096)
			packet->setDataByName("unknown3", 0x78);
		else
			packet->setDataByName("unknown3", 0x3C);

		packet->setDataByName("loot_id", entity->GetID());
		EQ2Packet* tmpPacket = packet->serialize();
		packet_size += tmpPacket->size;
		uchar* data = 0;
		if(items && items->size() > 0){
			data = new uchar[items->size()*1000 + packet_size];
			memset(data, 0, items->size()*1000 + packet_size);
		}
		else{
			data = new uchar[packet_size];
			memset(data, 0, packet_size);
		}
		uchar* ptr = data;
		memcpy(ptr, tmpPacket->pBuffer, tmpPacket->size);
		ptr += tmpPacket->size;
		safe_delete(tmpPacket);
		Item* item = 0;
		if(items && items->size() > 0){
			for(itr = items->begin(); itr != items->end(); ++itr){
				item = *itr;
				memcpy(ptr, &item->details.item_id, sizeof(int32));
				ptr += sizeof(int32);
				packet_size += sizeof(int32);

				tmpPacket = item->serialize(GetVersion(), true, GetPlayer(), false, 1, 0, false, true);

				int8 offset = 0;
				if(GetVersion() >= 860){
					offset = 11;
					/*memcpy(ptr, tmpPacket->pBuffer + 11, tmpPacket->size - 11);
					ptr += tmpPacket->size - 11;
					packet_size += tmpPacket->size - 11;*/
				}
				else{
					offset = 10;
					/*memcpy(ptr, tmpPacket->pBuffer + 10, tmpPacket->size - 10);
					ptr += tmpPacket->size - 10;
					packet_size += tmpPacket->size - 10;*/
				}

				memcpy(ptr, tmpPacket->pBuffer + offset, tmpPacket->size - offset);
				ptr += tmpPacket->size - offset;
				packet_size += tmpPacket->size - offset;

				safe_delete(tmpPacket);
			}
		}
		packet_size -= sizeof(int32);
		memcpy(data, &packet_size, sizeof(int32));
		packet_size += sizeof(int32);
		EQ2Packet* outapp = new EQ2Packet(OP_ClientCmdMsg, data, packet_size);
		//DumpPacket(outapp);
		QueuePacket(outapp);
		safe_delete_array(data);
		safe_delete(packet);
	}

}

void Client::Loot(Entity* entity){
	if(entity->IsNPC() && ((NPC*)entity)->Brain()->CheckLootAllowed(GetPlayer())){
		int32 total_coins = entity->GetLootCoins();
		entity->LockLoot();
		Loot(total_coins, entity->GetLootItems(), entity);
		entity->UnlockLoot();

		int32 state = 0;
		// Check for the chest and set the action state
		/*4034 = small chest | 5864 = treasure chest | 5865 = ornate treasure chest | 4015 = exquisite chest*/
		if (entity->GetModelType() == 4034) {			
			// small chest, open with copper coins
			state = 11899;
		}
		else if (entity->GetModelType() == 5864) {
			// treasure chest, open with silver coins
			state = 11901;
		}
		else if (entity->GetModelType() == 5865) {
			// ornate chest, open with gold coins
			state = 11900;
		}
		else if (entity->GetModelType() == 4015) {
			// exquisite chest, open with gold coins and jewels as well as a glow effect
			state = 11898;
		}

		// We set the visual state with out updating so those not in range will see it opened when it is finally sent to them,
		// for those in range the SendStateCommand will cause it to animate open.
		entity->SetVisualState(state, false);
		GetCurrentZone()->SendStateCommand(entity, state);
	}
	else
		SimpleMessage(CHANNEL_COLOR_YELLOW, "You are not allowed to loot at this time.");

}

Spawn* Client::GetBanker(){
	return banker;
}

void Client::SetBanker(Spawn* in_banker){
	banker = in_banker;

}

void Client::Bank(Spawn* banker, bool cancel){
	if(banker && banker->primary_command_list.size() > 0 && banker->primary_command_list[0]->command == "bank"){
		if(!cancel)
			SetBanker(banker);
		else
			SetBanker(0);
		PacketStruct* packet = configReader.getStruct("WS_UpdateBank", GetVersion());
		if(packet){
			packet->setDataByName("spawn_id", GetPlayer()->GetIDWithPlayerSpawn(banker));
			int64 coins = GetPlayer()->GetInfoStruct()->bank_coin_copper + GetPlayer()->GetInfoStruct()->bank_coin_silver * 100 +
				GetPlayer()->GetInfoStruct()->bank_coin_gold * 10000 + (int64)GetPlayer()->GetInfoStruct()->bank_coin_plat * 1000000;
			int32 coins1, coins2;
			coins1 = ((int32*)&coins)[0];
			coins2 = ((int32*)&coins)[1];
			packet->setDataByName("bank_coins", coins1);
			packet->setDataByName("bank_coins2", coins2);
			packet->setDataByName("copper", GetPlayer()->GetInfoStruct()->coin_copper);
			packet->setDataByName("silver", GetPlayer()->GetInfoStruct()->coin_silver);
			packet->setDataByName("gold", GetPlayer()->GetInfoStruct()->coin_gold);
			packet->setDataByName("plat", GetPlayer()->GetInfoStruct()->coin_plat);
			if(!cancel)
				packet->setDataByName("display", 1);
			QueuePacket(packet->serialize());
			safe_delete(packet);
		}
	}

}

void Client::BankWithdrawal(int64 amount){
	if (GetBanker() && amount > 0) {
		bool cheater = false;
		string withdrawal = "";
		char withdrawal_data[512] = {0};
		int32 tmp = 0;
		if(amount >= 1000000){
			tmp = amount/1000000;
			if(tmp > GetPlayer()->GetBankCoinsPlat())
				cheater = true;
			else{
				GetPlayer()->GetInfoStruct()->bank_coin_plat -= tmp;
				GetPlayer()->GetInfoStruct()->coin_plat += tmp;
				amount -= (int64)tmp *1000000;
				sprintf(withdrawal_data, "%u Platinum ", tmp);
				withdrawal.append(withdrawal_data);
				memset(withdrawal_data, 0, 64);
			}
		}
		if(!cheater && amount >= 10000){
			tmp = amount/10000;
			if(tmp > GetPlayer()->GetBankCoinsGold())
				cheater = true;
			else{
				GetPlayer()->GetInfoStruct()->bank_coin_gold -= tmp;
				if ((GetPlayer()->GetInfoStruct()->coin_gold + tmp) > 100) {
					GetPlayer()->GetInfoStruct()->coin_gold = (GetPlayer()->GetInfoStruct()->coin_gold + tmp) - 100;
					GetPlayer()->GetInfoStruct()->coin_plat += 1;
				}
				else
					GetPlayer()->GetInfoStruct()->coin_gold += tmp;
				amount -= tmp *10000;
				sprintf(withdrawal_data, "%u Gold ", tmp);
				withdrawal.append(withdrawal_data);
				memset(withdrawal_data, 0, 64);
			}
		}
		if(!cheater && amount >= 100){
			tmp = amount/100;
			if(tmp > GetPlayer()->GetBankCoinsSilver())
				cheater = true;
			else{
				GetPlayer()->GetInfoStruct()->bank_coin_silver -= tmp;
				if ((GetPlayer()->GetInfoStruct()->coin_silver + tmp) > 100) {
					GetPlayer()->GetInfoStruct()->coin_silver = (GetPlayer()->GetInfoStruct()->coin_silver + tmp) - 100;
					GetPlayer()->GetInfoStruct()->coin_gold += 1;
				}
				else
					GetPlayer()->GetInfoStruct()->coin_silver += tmp;
				amount -= tmp *100;
				sprintf(withdrawal_data, "%u Silver ", tmp);
				withdrawal.append(withdrawal_data);
				memset(withdrawal_data, 0, 64);
			}
		}
		if(!cheater){
			if(amount > 0){
				sprintf(withdrawal_data, "%u Copper ", (int32)amount);
				withdrawal.append(withdrawal_data);
				GetPlayer()->GetInfoStruct()->bank_coin_copper -= amount;
				if ((GetPlayer()->GetInfoStruct()->coin_copper + amount) > 100) {
					GetPlayer()->GetInfoStruct()->coin_copper = (GetPlayer()->GetInfoStruct()->coin_copper + amount) - 100;
					GetPlayer()->GetInfoStruct()->coin_silver += 1;
				}
				else
					GetPlayer()->GetInfoStruct()->coin_copper += amount;
			}
			if(withdrawal.length() > 0){
				withdrawal.append("withdrawn ");
				sprintf(withdrawal_data, "(%u Plat %u Gold %u Silver %u Copper in the bank now.)", GetPlayer()->GetInfoStruct()->bank_coin_plat,
					GetPlayer()->GetInfoStruct()->bank_coin_gold, GetPlayer()->GetInfoStruct()->bank_coin_silver, GetPlayer()->GetInfoStruct()->bank_coin_copper);
				withdrawal.append(withdrawal_data);
				SimpleMessage(CHANNEL_COLOR_REVIVE, withdrawal.c_str());
			}
		}
		else
			Message(CHANNEL_COLOR_RED, "Stop trying to cheat!");
		player->SetCharSheetChanged(true);
		Bank(banker);
	}

}

void Client::BankDeposit(int64 amount){
	if (GetBanker() && amount > 0) {
		bool cheater = false;
		int32 tmp = 0;
		char deposit_data[512] = {0};
		string deposit = "";
		if(amount >= 1000000){
			tmp = amount/1000000;
			if(tmp > GetPlayer()->GetCoinsPlat())
				cheater = true;
			else{
				GetPlayer()->GetInfoStruct()->bank_coin_plat += tmp;
				GetPlayer()->GetInfoStruct()->coin_plat -= tmp;
				amount -= (int64)tmp *1000000;
				sprintf(deposit_data, "%u Platinum ", tmp);
				deposit.append(deposit_data);
				memset(deposit_data, 0, 64);
			}
		}
		if(!cheater && amount >= 10000){
			tmp = amount/10000;
			if(tmp > GetPlayer()->GetCoinsGold())
				cheater = true;
			else{
				if ((GetPlayer()->GetInfoStruct()->bank_coin_gold + tmp) > 100) {
					GetPlayer()->GetInfoStruct()->bank_coin_gold = (GetPlayer()->GetInfoStruct()->bank_coin_gold + tmp) - 100;
					GetPlayer()->GetInfoStruct()->bank_coin_plat += 1;
				}
				else
					GetPlayer()->GetInfoStruct()->bank_coin_gold += tmp;
				GetPlayer()->GetInfoStruct()->coin_gold -= tmp;
				amount -= tmp *10000;
				sprintf(deposit_data, "%u Gold ", tmp);
				deposit.append(deposit_data);
				memset(deposit_data, 0, 64);
			}
		}
		if(!cheater && amount >= 100){
			tmp = amount/100;
			if(tmp > GetPlayer()->GetCoinsSilver())
				cheater = true;
			else{
				if ((GetPlayer()->GetInfoStruct()->bank_coin_silver + tmp) > 100) {
					GetPlayer()->GetInfoStruct()->bank_coin_silver = (GetPlayer()->GetInfoStruct()->bank_coin_silver + tmp) - 100;
					GetPlayer()->GetInfoStruct()->bank_coin_gold += 1;
				}
				else
					GetPlayer()->GetInfoStruct()->bank_coin_silver += tmp;
				GetPlayer()->GetInfoStruct()->coin_silver -= tmp;
				amount -= tmp *100;
				sprintf(deposit_data, "%u Silver ", tmp);
				deposit.append(deposit_data);
				memset(deposit_data, 0, 64);
			}
		}
		if(!cheater){
			if(amount > 0){
				sprintf(deposit_data, "%u Copper ", (int32)amount);
				deposit.append(deposit_data);
				if ((GetPlayer()->GetInfoStruct()->bank_coin_copper + amount) > 100) {
					GetPlayer()->GetInfoStruct()->bank_coin_copper = (GetPlayer()->GetInfoStruct()->bank_coin_copper + amount) - 100;
					GetPlayer()->GetInfoStruct()->bank_coin_silver += 1;
				}
				else
					GetPlayer()->GetInfoStruct()->bank_coin_copper += amount;
				GetPlayer()->GetInfoStruct()->coin_copper -= amount;
			}
			if(deposit.length() > 0){
				deposit.append("deposited ");
				sprintf(deposit_data, "(%u Plat %u Gold %u Silver %u Copper in the bank now.)", GetPlayer()->GetInfoStruct()->bank_coin_plat,
					GetPlayer()->GetInfoStruct()->bank_coin_gold, GetPlayer()->GetInfoStruct()->bank_coin_silver, GetPlayer()->GetInfoStruct()->bank_coin_copper);
				deposit.append(deposit_data);
				SimpleMessage(CHANNEL_COLOR_REVIVE, deposit.c_str());
			}
		}
		else
			Message(CHANNEL_COLOR_RED, "Stop trying to cheat!");
		player->SetCharSheetChanged(true);
		Bank(banker);
	}

}

void Client::AddPendingQuestReward(Quest* quest){
	MQuestPendingUpdates.writelock(__FUNCTION__, __LINE__);
	quest_pending_reward.push_back(quest);
	quest_updates = true;
	MQuestPendingUpdates.releasewritelock(__FUNCTION__, __LINE__);

}

void Client::AddPendingQuestUpdate(int32 quest_id, int32 step_id, int32 progress){
	MQuestPendingUpdates.writelock(__FUNCTION__, __LINE__);
	quest_pending_updates[quest_id][step_id] = progress;
	quest_updates = true;
	MQuestPendingUpdates.releasewritelock(__FUNCTION__, __LINE__);

}

void Client::ProcessQuestUpdates(){
	if(quest_pending_updates.size() > 0){
		map<int32, map<int32, int32> > tmp_quest_updates;
		MQuestPendingUpdates.writelock(__FUNCTION__, __LINE__);
		tmp_quest_updates.insert(quest_pending_updates.begin(), quest_pending_updates.end());
		quest_pending_updates.clear();
		MQuestPendingUpdates.releasewritelock(__FUNCTION__, __LINE__);
		map<int32, map<int32, int32> >::iterator quest_itr;
		map<int32, int32>::iterator step_itr;
		for(quest_itr = tmp_quest_updates.begin(); quest_itr != tmp_quest_updates.end(); ++quest_itr){
			for(step_itr = quest_itr->second.begin(); step_itr != quest_itr->second.end(); ++step_itr){
				if(step_itr->second == 0xFFFFFFFF){
					SetStepComplete(quest_itr->first, step_itr->first);
					player->SendQuestRequiredSpawns(quest_itr->first);
				}
				else
					AddStepProgress(quest_itr->first, step_itr->first, step_itr->second);
			}
		}		
	}
	if(quest_pending_reward.size() > 0){
		vector<Quest*>::iterator itr;
		vector<Quest*> tmp_quest_rewards;
		MQuestPendingUpdates.writelock(__FUNCTION__, __LINE__);
		tmp_quest_rewards.insert(tmp_quest_rewards.begin(), quest_pending_reward.begin(), quest_pending_reward.end());
		quest_pending_reward.clear();
		MQuestPendingUpdates.releasewritelock(__FUNCTION__, __LINE__);
		for(itr = tmp_quest_rewards.begin(); itr != tmp_quest_rewards.end(); ++itr){
			GiveQuestReward(*itr);
		}		
	}
	quest_updates = false;

}

void Client::CheckQuestQueue(){	
	MQuestQueue.writelock();
	last_update_time = 0;
	vector<QueuedQuest*>::iterator itr;
	for(itr = quest_queue.begin(); itr != quest_queue.end(); ++itr){
		QueuedQuest* queued_quest = *itr;
		SendQuestUpdateStepImmediately(queued_quest->quest, queued_quest->step, queued_quest->display_quest_helper);
		safe_delete(queued_quest);
	}
	quest_queue.clear();
	MQuestQueue.releasewritelock();

}

void Client::SetStepComplete(int32 quest_id, int32 step){
	Quest* quest = player->SetStepComplete(quest_id, step);
	if(quest){
		SendQuestUpdate(quest);
		GetCurrentZone()->SendQuestUpdates(this);
	}

}

void Client::AddStepProgress(int32 quest_id, int32 step, int32 progress) {
	Quest* quest = player->AddStepProgress(quest_id, step, progress);
	if (quest) {
		SendQuestUpdate(quest);
		GetCurrentZone()->SendQuestUpdates(this);
	}
}

void Client::CheckPlayerQuestsKillUpdate(Spawn* spawn){
	vector<Quest*>* quest_updates = player->CheckQuestsKillUpdate(spawn);
	if(quest_updates){
		for(int32 i=0;i<quest_updates->size();i++)
			SendQuestUpdate(quest_updates->at(i));
	}
	safe_delete(quest_updates);
	vector<Quest*>* quest_failures = player->CheckQuestsFailures();
	if(quest_failures){
		for(int32 i=0;i<quest_failures->size();i++)
			SendQuestFailure(quest_failures->at(i));
	}
	safe_delete(quest_failures);

}

void Client::CheckPlayerQuestsChatUpdate(Spawn* spawn){
	vector<Quest*>* quest_updates = player->CheckQuestsChatUpdate(spawn);
	if(quest_updates){
		for(int32 i=0;i<quest_updates->size();i++)
			SendQuestUpdate(quest_updates->at(i));
		GetCurrentZone()->SendQuestUpdates(this);
	}
	safe_delete(quest_updates);
}

void Client::CheckPlayerQuestsItemUpdate(Item* item){
	vector<Quest*>* quest_updates = player->CheckQuestsItemUpdate(item);
	if(quest_updates){
		for(int32 i=0;i<quest_updates->size();i++)
			SendQuestUpdate(quest_updates->at(i));
	}
	safe_delete(quest_updates);
	vector<Quest*>* quest_failures = player->CheckQuestsFailures();
	if(quest_failures){
		for(int32 i=0;i<quest_failures->size();i++)
			SendQuestFailure(quest_failures->at(i));
	}
	safe_delete(quest_failures);
}

void Client::CheckPlayerQuestsLocationUpdate(){
	vector<Quest*>* quest_updates = player->CheckQuestsLocationUpdate();
	if(quest_updates){
		for(int32 i=0;i<quest_updates->size();i++)
			SendQuestUpdate(quest_updates->at(i));
	}
	safe_delete(quest_updates);
}

void Client::CheckPlayerQuestsSpellUpdate(Spell* spell){
	vector<Quest*>* quest_updates = player->CheckQuestsSpellUpdate(spell);
	if(quest_updates) {
		for(int32 i = 0; i < quest_updates->size(); i++)
			SendQuestUpdate(quest_updates->at(i));
	}
	safe_delete(quest_updates);
	vector<Quest*>* quest_failures = player->CheckQuestsFailures();
	if(quest_failures){
		for(int32 i = 0; i< quest_failures->size(); i++)
			SendQuestFailure(quest_failures->at(i));
	}
	safe_delete(quest_failures);
}

void Client::AddPendingQuest(Quest* quest){
	player->pending_quests[quest->GetQuestID()] = quest;
	EQ2Packet* outapp = quest->OfferQuest(GetVersion(), player);
	//DumpPacket(outapp);
	QueuePacket(outapp);
}

Quest* Client::GetActiveQuest(int32 quest_id){
	if(player->player_quests.count(quest_id) > 0) {
		LogWrite(CCLIENT__DEBUG, 0, "Client", "Found %u active quests for char_id: %u", player->player_quests.count(quest_id), player->GetCharacterID());

			return player->player_quests[quest_id];
	}

	return 0;
}

Quest* Client::GetPendingQuest(int32 id){
	if(player->pending_quests.count(id) > 0) {
		LogWrite(CCLIENT__DEBUG, 0, "Client", "Found %u pending quests for char_id: %u", player->pending_quests.count(id), player->GetCharacterID());

			return player->pending_quests[id];
	}

	return 0;
}

void Client::RemovePendingQuest(Quest* quest){
	player->pending_quests.erase(quest->GetQuestID());

}

void Client::SetPlayerQuest(Quest* quest, map<int32, int32>* progress){
	if(!quest || !progress) {
			return;
	}
	map<int32, int32>::iterator itr;
	player->SetZone(GetCurrentZone());
	for(itr = progress->begin(); itr != progress->end(); ++itr){
		QuestStep* step = quest->GetQuestStep(itr->first);
		if(step && itr->second > 0){
			step->SetStepProgress(itr->second);
			if(lua_interface && step->GetQuestCurrentQuantity() >= step->GetQuestNeededQuantity())
				lua_interface->CallQuestFunction(quest, "Reload", player, step->GetStepID());
		}
	}

}

void Client::AddPlayerQuest(Quest* quest, bool call_accepted, bool send_packets){
	if(player->player_quests.count(quest->GetQuestID()) > 0) {
		if (player->player_quests[quest->GetQuestID()]->GetQuestFlags() > 0)
			quest->SetQuestFlags(player->player_quests[quest->GetQuestID()]->GetQuestFlags());

		RemovePlayerQuest(quest->GetQuestID(), false, false);
	}
	player->player_quests[quest->GetQuestID()] = quest;
	quest->SetPlayer(player);
	current_quest_id = quest->GetQuestID();
	if(send_packets && quest->GetQuestGiver() > 0)
		GetCurrentZone()->SendSpawnChanges(quest->GetQuestGiver(), this, false, true);
	if(lua_interface && call_accepted)
		lua_interface->CallQuestFunction(quest, "Accepted", player);
	if(send_packets) {
		LogWrite(CCLIENT__DEBUG, 0, "Client", "Send Quest Journal...");
		//SendQuestJournal();
		SendQuestJournalUpdate(quest);

		// sent twice to match live
		quest->SetTracked(false);
		QueuePacket(quest->QuestJournalReply(GetVersion(), GetNameCRC(), player));
		quest->SetTracked(true);
		QueuePacket(quest->QuestJournalReply(GetVersion(), GetNameCRC(), player));
	}
	//This isn't during a load screen, so update spawns with required quests
	if(call_accepted)
		player->SendQuestRequiredSpawns(quest->GetQuestID());

}

void Client::RemovePlayerQuest(int32 id, bool send_update, bool delete_quest){
	if(current_quest_id == id)
		current_quest_id = 0;
	if(player->player_quests.count(id) > 0){
		if(delete_quest){
			player->player_quests[id]->SetDeleted(true);
			database.DeleteCharacterQuest(id, GetCharacterID(), player->GetCompletedPlayerQuests()->count(id) > 0);
		}
		if(send_update && player->player_quests[id]->GetQuestGiver() > 0)
			GetCurrentZone()->SendSpawnChanges(player->player_quests[id]->GetQuestGiver(), this, false, true);
		if(send_update) {
			LogWrite(CCLIENT__DEBUG, 0, "Client", "Send Quest Journal...");
			SendQuestJournal();
		}
		player->RemoveQuest(id, delete_quest);
		if(send_update) {
			LogWrite(CCLIENT__DEBUG, 0, "Client", "Send Quest Journal...");
			SendQuestJournal();
		}
	}

}

void Client::SendQuestUpdateStepImmediately(Quest* quest, int32 step, bool display_quest_helper){
	if(quest){
		QuestStep* quest_step = quest->GetQuestStep(step);
		if(quest_step){
			QueuePacket(quest->QuestJournalReply(GetVersion(), GetNameCRC(), player, quest_step, 1, false, false, display_quest_helper));
			quest_step->WasUpdated(false);
		}
	}

}

void Client::SendQuestUpdateStep(Quest* quest, int32 step, bool display_quest_helper){
	QueuedQuest* item = new QueuedQuest;
	item->quest = quest;
	item->step = step;
	item->display_quest_helper = display_quest_helper;
	MQuestQueue.writelock();
	quest_queue.push_back(item);
	last_update_time = Timer::GetCurrentTime2();
	MQuestQueue.releasewritelock();

}

void Client::SendQuestFailure(Quest* quest){
	vector<QuestStep*>* failures = quest->GetQuestFailures();
	if (failures) {
		for(int32 i=0;i<failures->size();i++){
			QuestStep* step = failures->at(i);
			QueuePacket(quest->QuestJournalReply(GetVersion(), GetNameCRC(), player, step, 1, false, true));
			LogWrite(CCLIENT__DEBUG, 0, "Client", "Send Quest Journal...");
			SendQuestJournal();
		}
		failures->clear();
	}

}

void Client::SendQuestUpdate(Quest* quest){
	vector<QuestStep*>* updates = quest->GetQuestUpdates();
	if(updates){
		for(int32 i=0;i<updates->size();i++){
			QuestStep* step = updates->at(i);
			if(lua_interface && step->Complete() && quest->GetCompleteAction(step->GetStepID()))
				lua_interface->CallQuestFunction(quest, quest->GetCompleteAction(step->GetStepID()), player);
			if(step->WasUpdated()){
				SendQuestJournal();
				QueuePacket(quest->QuestJournalReply(GetVersion(), GetNameCRC(), player, step));
			}
			LogWrite(CCLIENT__DEBUG, 0, "Client", "Send Quest Journal...");
			
		}
		if(lua_interface && quest->GetCompleted() && quest->GetCompleteAction())
			lua_interface->CallQuestFunction(quest, quest->GetCompleteAction(), player);
		if(quest->GetCompleted()){
			if (quest->GetQuestReturnNPC() > 0)
				GetCurrentZone()->SendSpawnChanges(quest->GetQuestReturnNPC(), this, false, true);
			if (quest->GetCompletedFlag())
				quest->SetCompletedFlag(false);
		}

		updates->clear();
	}

}

void Client::SendQuestJournal(bool all_quests, Client* client){
	if(!client)
		client = this;
	PacketStruct* packet = player->GetQuestJournalPacket(all_quests, GetVersion(), GetNameCRC(), current_quest_id);
	if(packet){
		EQ2Packet* outapp = packet->serialize();
		//DumpPacket(outapp);
		client->QueuePacket(outapp);
		safe_delete(packet);
	}
}

void Client::SendQuestJournalUpdate(Quest* quest) {
	PacketStruct* packet = player->GetQuestJournalPacket(quest, GetVersion(), GetNameCRC());
	if (packet) {
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}
}

void Client::ReloadQuests(){
	vector<int32> ids = player->GetQuestIDs();
	for(int32 i=0;i<ids.size();i++){
		Quest* quest = master_quest_list.GetQuest(ids[i]);
		if(quest)
			AddPlayerQuest(quest, false);
		else
			RemovePlayerQuest(ids[i]);
	}

}

Quest*	Client::GetPendingQuestAcceptance(int32 item_id){
	bool found_quest = false;
	vector<Quest*>::iterator itr;
	Quest* quest = 0;
	MPendingQuestAccept.lock();
	for(itr = pending_quest_accept.begin(); itr != pending_quest_accept.end(); ++itr){
		quest = *itr;
		vector<Item*>* items = quest->GetSelectableRewardItems();
		if(items && items->size() > 0){
			for(int32 i=0;i<items->size();i++){
				if(items->at(i)->details.item_id == item_id){
					found_quest = true;
					break;
				}
			}
		}
		else if(item_id == 0)
			found_quest = true;
		if(found_quest){
			pending_quest_accept.erase(itr);
			break;
		}
	}
	MPendingQuestAccept.unlock();

	if(found_quest)
		return quest;
	return 0;
}

void Client::AcceptQuestReward(Quest* quest, int32 item_id){
	int8 num_slots_needed = 0;
	int16 free_slots = player->item_list.GetNumberOfFreeSlots();
	Item* master_item = 0;
	if(item_id > 0){
		num_slots_needed++;
		master_item  = master_item_list.GetItem(item_id);
	}
	vector<Item*>* items = quest->GetRewardItems();
	if(items && items->size() > 0)
		num_slots_needed += items->size();
	if(free_slots >= num_slots_needed || (player->item_list.HasFreeBagSlot() && master_item && master_item->IsBag() && master_item->bag_info->num_slots >= items->size())){
		if(master_item)
			AddItem(item_id);
		if(items && items->size() > 0){
			for(int32 i=0;i<items->size();i++)
				AddItem(new Item(items->at(i)));
		}
		EQ2Packet* outapp = player->SendInventoryUpdate(GetVersion());
		if(outapp)
			QueuePacket(outapp);
		map<int32, sint32>* reward_factions = quest->GetRewardFactions();
		map<int32, sint32>::iterator itr;
		for (itr = reward_factions->begin(); itr != reward_factions->end(); ++itr) {
			int32 faction_id = itr->first;
			sint32 amount = itr->second;
			if (amount > 0)
				player->GetFactions()->IncreaseFaction(faction_id, amount);
			else
				player->GetFactions()->DecreaseFaction(faction_id, (amount * -1));
		}
	}
	else{
		MPendingQuestAccept.lock();
		pending_quest_accept.push_back(quest);
		MPendingQuestAccept.unlock();
		SimpleMessage(CHANNEL_COLOR_RED, "You do not have enough free slots!  Free some slots and try again.");
		DisplayQuestComplete(quest);
	}

}

void Client::DisplayQuestComplete(Quest* quest){
	PacketStruct* packet = configReader.getStruct("WS_QuestComplete", GetVersion());
	if(packet){
		packet->setDataByName("title", "Quest Reward!");
		packet->setDataByName("name", quest->GetName());
		packet->setDataByName("description", quest->GetDescription());
		packet->setDataByName("level", quest->GetLevel());
		packet->setDataByName("encounter_level", quest->GetEncounterLevel());
		int8 difficulty = 0;
		if((string)quest->GetType() == "Tradeskill")
			difficulty = player->GetTSArrowColor(quest->GetLevel());
		else
			difficulty = player->GetArrowColor(quest->GetLevel());
		packet->setDataByName("difficulty", difficulty);
		int64 rewarded_coin = 0;
		if (quest->GetCoinsReward() > 0){
			if (quest->GetCoinsRewardMax() > 0)
				rewarded_coin = MakeRandomInt(quest->GetCoinsReward(), quest->GetCoinsRewardMax());
			else
				rewarded_coin = quest->GetCoinsReward();
		}
		quest->SetGeneratedCoin(rewarded_coin);
		packet->setDataByName("max_coin", rewarded_coin);
		packet->setDataByName("min_coin", rewarded_coin);
		packet->setDataByName("status_points", quest->GetStatusPoints());
		vector<Item*>* items2 = quest->GetSelectableRewardItems();
		vector<Item*>* items = quest->GetRewardItems();
		if(items){
			packet->setArrayLengthByName("num_rewards", items->size());
			for(int32 i=0;i<items->size();i++){
				packet->setArrayDataByName("reward_id", items->at(i)->details.item_id, i);
				if(version < 860)
					packet->setItemArrayDataByName("item", items->at(i), player, i, 0, -1);
				else if (version < 1193)
					packet->setItemArrayDataByName("item", items->at(i), player, i);
				else
					packet->setItemArrayDataByName("item", items->at(i), player, i, 0, 2);
			}
		}
		if(items2 && items2->size() > 0){
			packet->setArrayLengthByName("num_select_rewards", items2->size());
			for(int32 i=0;i<items2->size();i++){
				packet->setArrayDataByName("select_reward_id", items2->at(i)->details.item_id, i);
				if(version < 860)
					packet->setItemArrayDataByName("select_item", items2->at(i), player, i, 0, -1);
				else if (version < 1193)
					packet->setItemArrayDataByName("select_item", items2->at(i), player, i);
				else
					packet->setItemArrayDataByName("select_item", items2->at(i), player, i, 0, 2);
			}
		}
		map<int32, sint32>* reward_factions = quest->GetRewardFactions();
		if (reward_factions && reward_factions->size() > 0) {
			packet->setArrayLengthByName("num_factions", reward_factions->size());
			map<int32, sint32>::iterator itr;
			int16 index = 0;
			for (itr = reward_factions->begin(); itr != reward_factions->end(); ++itr) {
				int32 faction_id = itr->first;
				sint32 amount = itr->second;
				const char* faction_name = master_faction_list.GetFactionNameByID(faction_id);
				if (faction_name) {
					packet->setArrayDataByName("faction_name", const_cast<char*>(faction_name), index);
					packet->setArrayDataByName("amount", amount, index);
				}
				index++;
			}
		}
		EQ2Packet* outapp = packet->serialize();
	//			DumpPacket(outapp);
		QueuePacket(outapp);
		safe_delete(packet);
	}

}

void Client::DisplayRandomizeFeatures(int32 flags) {
	SimpleMessage(CHANNEL_COLOR_WHITE, "Showing Active Randomize Features:");
	if(flags > 0) {
		if (flags & RANDOMIZE_GENDER)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Gender");
		if (flags & RANDOMIZE_RACE)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Race");
		if (flags & RANDOMIZE_MODEL_TYPE)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Model");
		if (flags & RANDOMIZE_FACIAL_HAIR_TYPE)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Facial Hair");
		if (flags & RANDOMIZE_HAIR_TYPE)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Hair");
		if (flags & RANDOMIZE_WING_TYPE)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Wing");
		if (flags & RANDOMIZE_CHEEK_TYPE)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Cheek");
		if (flags & RANDOMIZE_CHIN_TYPE)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Chin");
		if (flags & RANDOMIZE_EAR_TYPE)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Ear");
		if (flags & RANDOMIZE_EYE_BROW_TYPE)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Eyebrow");
		if (flags & RANDOMIZE_EYE_TYPE)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Eye");
		if (flags & RANDOMIZE_LIP_TYPE)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Lip");
		if (flags & RANDOMIZE_NOSE_TYPE)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Nose");
		if (flags & RANDOMIZE_EYE_COLOR)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Eye Color");
		if (flags & RANDOMIZE_HAIR_COLOR1)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Hair Color1");
		if (flags & RANDOMIZE_HAIR_COLOR2)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Hair Color2");
		if (flags & RANDOMIZE_HAIR_HIGHLIGHT)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Hair Color Highlights");
		if (flags & RANDOMIZE_HAIR_FACE_COLOR)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Facial Hair Color");
		if (flags & RANDOMIZE_HAIR_FACE_HIGHLIGHT_COLOR)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Facial Hair Color Highlights");
		if (flags & RANDOMIZE_HAIR_TYPE_COLOR)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Hair Type Color");
		if (flags & RANDOMIZE_HAIR_TYPE_HIGHLIGHT_COLOR)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Hair Type Highlights");
		if (flags & RANDOMIZE_SKIN_COLOR)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Skin Color");
		if (flags & RANDOMIZE_WING_COLOR1)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Wing Color1");
		if (flags & RANDOMIZE_WING_COLOR2)
			SimpleMessage(CHANNEL_COLOR_WHITE, "- Wing Color2");
	}
	else
	{
		SimpleMessage(CHANNEL_COLOR_WHITE, "- No Randomization Set.");
	}

}

void Client::GiveQuestReward(Quest* quest){
	current_quest_id = 0;
	MPendingQuestAccept.lock();
	pending_quest_accept.push_back(quest);
	MPendingQuestAccept.unlock();

	quest->IncrementCompleteCount();
	player->AddCompletedQuest(quest);

	LogWrite(CCLIENT__DEBUG, 0, "Client", "Send Quest Journal...");
	SendQuestJournal();
	player->RemoveQuest(quest->GetQuestID(), false);
	DisplayQuestComplete(quest);
	if(quest->GetExpReward() > 0){
		int16 level = player->GetLevel();
		if (player->AddXP(quest->GetExpReward())) {
			SimpleMessage(CHANNEL_COLOR_EXP, "You gain experience!");
			if(player->GetLevel() != level)
				ChangeLevel(level, player->GetLevel());
			player->SetCharSheetChanged(true);
		}
	}
	if(quest->GetTSExpReward() > 0){
		int8 ts_level = player->GetTSLevel();
		if (player->AddTSXP(quest->GetTSExpReward())) {
			SimpleMessage(CHANNEL_COLOR_EXP, "You gain tradeskill experience!");
			if(player->GetTSLevel() != ts_level)
				ChangeTSLevel(ts_level, player->GetTSLevel());
			player->SetCharSheetChanged(true);
		}
	}
	int64 total_coins = quest->GetGeneratedCoin();
	if(total_coins > 0){
		player->AddCoins(total_coins);
		PlaySound("coin_cha_ching");
		char tmp[64] = {0};
		string message = "You receive ";
		int32 val = 0;
		if(total_coins >= 1000000){
			val = total_coins / 1000000;
			total_coins -= 1000000 * val;
			sprintf(tmp, "%u Platinum ", val);
			message.append(tmp);
			memset(tmp, 0, 64);
		}
		if(total_coins >= 10000){
			val = total_coins / 10000;
			total_coins -= 10000 * val;
			sprintf(tmp, "%u Gold ", val);
			message.append(tmp);
			memset(tmp, 0, 64);
		}
		if(total_coins >= 100){
			val = total_coins / 100;
			total_coins -= 100 * val;
			sprintf(tmp, "%u Silver ", val);
			message.append(tmp);
			memset(tmp, 0, 64);
		}
		if(total_coins > 0){
			sprintf(tmp, "%u Copper ", (int32)total_coins);
			message.append(tmp);
		}
		message.append("for completing ").append(quest->GetName());
		int8 type = CHANNEL_COLOR_LOOT;
		if(GetVersion() >= 973)
			type = CHANNEL_COLOR_NEW_LOOT;
		SimpleMessage(type, message.c_str());
	}
	if(quest->GetQuestGiver() > 0)
		GetCurrentZone()->SendSpawnChanges(quest->GetQuestGiver(), this, false, true);
	RemovePlayerQuest(quest->GetQuestID(), true, false);

}

void Client::DisplayConversation(int32 conversation_id, int32 spawn_id, vector<ConversationOption>* conversations, const char* text, const char* mp3, int32 key1, int32 key2){
	PacketStruct* packet = configReader.getStruct("WS_DialogOpen", GetVersion());
	if(packet){				
		packet->setDataByName("conversation_id", conversation_id);
		packet->setDataByName("text", text);
		packet->setDataByName("unknown2", 1);
		conversation_map[conversation_id].clear();
		if (conversations) {
			packet->setArrayLengthByName("num_responses", conversations->size());
			for(int32 i=0;i<conversations->size();i++){
				packet->setArrayDataByName("response", conversations->at(i).option.c_str(), i);
				if(conversations->at(i).function.length() > 0)
					conversation_map[conversation_id][i] = conversations->at(i).function;
			}
		}
		packet->setDataByName("spawn_id", spawn_id);
		if(mp3){
			packet->setDataByName("voice", mp3);
			packet->setDataByName("key1", key1);
			packet->setDataByName("key2", key2);
		}
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

}

void Client::DisplayConversation(Item* item, vector<ConversationOption>* conversations, const char* text, int8 type, const char* mp3, int32 key1, int32 key2){
	if(!item || !text || !conversations || conversations->size() == 0) {
			return;
	}
	int32 conversation_id = GetConversationID(0, item);
	if(conversation_id == 0){
		next_conversation_id++;
		conversation_id = next_conversation_id;
	}
	conversation_items[conversation_id] = item;
	if (type == 4)
		DisplayConversation(conversation_id, player->GetIDWithPlayerSpawn(player), conversations, text, mp3, key1, key2);
	else
		DisplayConversation(conversation_id, 0xFFFFFFFF, conversations, text, mp3, key1, key2);

}

void Client::DisplayConversation(Entity* npc, int8 type, vector<ConversationOption>* conversations, const char* text, const char* mp3, int32 key1, int32 key2){
	if(!npc || !(type == 1 || type == 2 || type == 3) || !text /*|| !conversations || conversations->size() == 0*/) {
			return;
	}
	int32 conversation_id = GetConversationID(npc, 0);
	if(conversation_id == 0){
		next_conversation_id++;
		conversation_id = next_conversation_id;
	}
	conversation_spawns[conversation_id] = npc;

	/* NPCs can start two different types of conversations.
	 * Type 1: The chat type with bubbles.
	 * Type 2: The dialog type with the blue box. */
	if (type == 1)
		DisplayConversation(conversation_id, player->GetIDWithPlayerSpawn(npc), conversations, text, mp3, key1, key2);
	else if (type == 2)
		DisplayConversation(conversation_id, 0xFFFFFFFF, conversations, text, mp3, key1, key2);
	else if (type == 3)
		DisplayConversation(conversation_id, player->GetIDWithPlayerSpawn(player), conversations, text, mp3, key1, key2);

}

void Client::CloseDialog(int32 conversation_id){
	PacketStruct* packet = configReader.getStruct("WS_ServerDialogClose", GetVersion());
	if(packet){
		packet->setDataByName("conversation_id", conversation_id);
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}
	conversation_items.erase(conversation_id);
	conversation_spawns.erase(conversation_id);

}

int32 Client::GetConversationID(Spawn* spawn, Item* item){
	int32 conversation_id = 0;
	if(spawn){
		map<int32, Spawn*>::iterator itr;
		for(itr = conversation_spawns.begin(); itr != conversation_spawns.end(); ++itr){
			if(itr->second == spawn){
				conversation_id = itr->first;
				break;
			}
		}
	}
	else if(item){
		map<int32, Item*>::iterator itr;
		for(itr = conversation_items.begin(); itr != conversation_items.end(); ++itr){
			if(itr->second == item){
				conversation_id = itr->first;
				break;
			}
		}
	}

	return conversation_id;
}

Spawn* Client::GetCombineSpawn(){
	return combine_spawn;
}

bool Client::ShouldTarget(){
	return should_target;
}

void Client::TargetSpawn(Spawn* spawn){
	should_target = false;
	PacketStruct* packet = configReader.getStruct("WS_ServerUpdateTarget", GetVersion());
	if(packet){
		packet->setDataByName("spawn_id", GetPlayer()->GetIDWithPlayerSpawn(spawn));
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

}

void Client::CombineSpawns(float radius, Spawn* spawn){
	combine_spawn = spawn;
	spawn->RemoveSpawnFromGroup(true);
	if(!GetCurrentZone()->AddCloseSpawnsToSpawnGroup(combine_spawn, radius))
		SimpleMessage(CHANNEL_COLOR_YELLOW, "One or more spawns are in a spawn group and cannot be combined until they are removed from their group.");
	GetCurrentZone()->RepopSpawns(this, combine_spawn);
	should_target = true;

}

void Client::AddCombineSpawn(Spawn* spawn){
	if(combine_spawn && combine_spawn != spawn && spawn){
		combine_spawn->AddSpawnToGroup(spawn);
		spawn->AddSpawnToGroup(combine_spawn);
		GetCurrentZone()->RepopSpawns(this, combine_spawn);
	}
	else if(spawn)
		combine_spawn = spawn;
	should_target = true;

}

void Client::RemoveCombineSpawn(Spawn* spawn){
	if(combine_spawn && spawn)
		spawn->RemoveSpawnFromGroup();
	if(combine_spawn == spawn)
		combine_spawn->RemoveSpawnFromGroup(true);
	GetCurrentZone()->RepopSpawns(this, combine_spawn);
	if(combine_spawn == spawn)
		combine_spawn = 0;

}

void Client::SaveCombineSpawns(const char* name){
	if(!combine_spawn) {
			return;
	}
	vector<Spawn*>* spawns = combine_spawn->GetSpawnGroup();
	if(!spawns) {
			return;
	}
	int32 count = spawns->size();
	safe_delete(spawns);
	if(count == 1)
		SimpleMessage(CHANNEL_COLOR_YELLOW, "Error: You only have a single Spawn in the group!");
	else if(database.SaveCombinedSpawnLocation(GetCurrentZone(), combine_spawn, name)){
		Message(CHANNEL_COLOR_YELLOW, "Successfully combined %u spawns into spawn location: %u", count, combine_spawn->GetSpawnLocationID());
		GetCurrentZone()->RemoveSpawn(combine_spawn);
	}
	else
		SimpleMessage(CHANNEL_COLOR_YELLOW, "Error saving spawn group, check console for details.");
	combine_spawn = 0;

}

bool Client::AddItem(int32 item_id, int8 quantity){
	Item* master_item = master_item_list.GetItem(item_id);
	Item* item = 0;
	if(master_item)
		item = new Item(master_item);
	if(item){
		if(quantity > 0 )
			item->details.count = quantity;
			return AddItem(item);
	}
	else
		Message(CHANNEL_COLOR_RED, "Could not find item with id of: %i", item_id);

	return false;
}

bool Client::AddItem(Item* item){
	if(!item) {
			return false;
	}
	if(item->IsBag())
		item->details.bag_id = item->details.unique_id;
	if(player->AddItem(item)){
		EQ2Packet* outapp = player->SendInventoryUpdate(GetVersion());
		if(outapp){
			QueuePacket(outapp);
			//resend bag desc with new item name added	
			outapp = player->SendBagUpdate(item->details.inv_slot_id, GetVersion());
			if(outapp)
				QueuePacket(outapp);
			/*EQ2Packet* app = item->serialize(client->GetVersion(), false);
			DumpPacket(app);
			client->QueuePacket(app);
			*/
		}
		CheckPlayerQuestsItemUpdate(item);
		if(item->GetItemScript() && lua_interface)
			lua_interface->RunItemScript(item->GetItemScript(), "obtained", item, player);
	}
	else{
		SimpleMessage(CHANNEL_COLOR_RED, "Could not find free slot to place item.");
		safe_delete(item);
			return false;
	}

	return true;
}

bool Client::RemoveItem(Item *item, int8 quantity) {
	EQ2Packet *outapp;
	bool delete_item = false;

	assert(item);

	if (quantity > 0 && !item->IsBag() && item->details.count > quantity) {
		item->details.count -= quantity;
		item->save_needed = true;
	}
	else {
		database.DeleteItem(character_id, item, 0);
		player->GetPlayerItemList()->RemoveItem(item, false);
		delete_item = true;
	}
	
	if ((outapp = player->SendInventoryUpdate(version))) {
		QueuePacket(outapp);
		if (item->GetItemScript() && lua_interface)
			lua_interface->RunItemScript(item->GetItemScript(), "removed", item, player);
		if (delete_item)
			safe_delete(item);
		return true;
	}

	return false;
}

void Client::SetLuaDebugClient(bool val){ 
	if(val)
		lua_debug_timer.Start();
	lua_debug = val; 
	if(lua_interface && !val){
		lua_interface->RemoveDebugClients(this);
		lua_debug_timer.Disable();
	}

}

void Client::SetMerchantTransaction(Spawn* spawn){
	merchant_transaction = spawn;

}

Spawn* Client::GetMerchantTransaction(){
	return merchant_transaction;
}

void Client::SetMailTransaction(Spawn* spawn) {
	mail_transaction = spawn;
	mail_window.coin_copper = 0;
	mail_window.coin_silver = 0;
	mail_window.coin_gold = 0;
	mail_window.coin_plat = 0;
	mail_window.char_item_id = 0;

}

Spawn* Client::GetMailTransaction() {
	return mail_transaction;
}

void Client::PlaySound(const char* name){
	if(name){
		PacketStruct* packet = configReader.getStruct("WS_PlaySound", GetVersion());
		if(packet){
			packet->setMediumStringByName("name", name);
			QueuePacket(packet->serialize());
			safe_delete(packet);
		}
	}

}

float Client::CalculateBuyMultiplier(int32 merchant_id){
	/*MerchantFactionMultiplier* multiplier = world.GetMerchantMultiplier(merchant_id);
	if(multiplier){
		sint32 faction_val = player->GetFactions()->GetFactionValue(multiplier->faction_id);
		float diff_low = faction_val - multiplier->faction_min;
		if(diff_low < 0)
			diff_low*=-1;
		float total_diff = multiplier->faction_max - multiplier->faction_min;
		if(total_diff < 0)
			total_diff*=-1;
		float buy_multiplier = multiplier->high_buy_multiplier - multiplier->low_buy_multiplier;
		float total1 = (diff_low/total_diff);
		float final_buy_multiplier = total1*buy_multiplier + total1*multiplier->low_buy_multiplier;
		return final_buy_multiplier;
	}*/

	return 1;
}

float Client::CalculateSellMultiplier(int32 merchant_id){
	/*MerchantFactionMultiplier* multiplier = world.GetMerchantMultiplier(merchant_id);
	if(multiplier){
		sint32 faction_val = player->GetFactions()->GetFactionValue(multiplier->faction_id);
		float diff_low = faction_val - multiplier->faction_min;
		if(diff_low < 0)
			diff_low*=-1;
		float total_diff = multiplier->faction_max - multiplier->faction_min;
		if(total_diff < 0)
			total_diff*=-1;
		float sell_multiplier = multiplier->high_sell_multiplier - multiplier->low_sell_multiplier;
		float total1 = (diff_low/total_diff);
		float final_sell_multiplier = total1*sell_multiplier + total1*multiplier->low_sell_multiplier;
		return final_sell_multiplier;
	}*/

	return 1;
}

void Client::SellItem(int32 item_id, int8 quantity, int32 unique_id){
	Spawn* spawn = GetMerchantTransaction();
	if(spawn && spawn->GetMerchantID() > 0){
		float multiplier = CalculateBuyMultiplier(spawn->GetMerchantID());
		Item* master_item = master_item_list.GetItem(item_id);
		Item* item = 0;
		if (unique_id == 0)
			item = player->item_list.GetItemFromID(item_id, quantity);
		else
			item = player->item_list.GetItemFromUniqueID(unique_id);

		if(!item)
			player->item_list.GetItemFromID(item_id);
		if(item && master_item){
			int32 sell_price = (int32)(master_item->sell_price * multiplier);
			if(sell_price > item->sell_price)
				sell_price = item->sell_price;
			if(quantity > item->details.count)
				quantity = item->details.count;
			int32 total_sell_price = sell_price * quantity;
			if(quantity > 1)
				Message(CHANNEL_COLOR_MERCHANT, "You sell %i \\aITEM %u 0:%s\\/a to %s for %s.", quantity, master_item->details.item_id, master_item->name.c_str(), spawn->GetName(), GetCoinMessage(total_sell_price).c_str());
			else
				Message(CHANNEL_COLOR_MERCHANT, "You sell \\aITEM %u 0:%s\\/a to %s for %s.", master_item->details.item_id, master_item->name.c_str(), spawn->GetName(), GetCoinMessage(total_sell_price).c_str());
			player->AddCoins(total_sell_price);
			AddBuyBack(unique_id, item_id, quantity, sell_price);
			if(quantity >= item->details.count) {
				database.DeleteItem(GetCharacterID(), item, 0);
				player->item_list.DestroyItem(item->details.index);
			}
			else{
				item->details.count -= quantity;
				item->save_needed = true;
			}
			EQ2Packet* outapp = player->SendInventoryUpdate(GetVersion());
			if(outapp)
				QueuePacket(outapp);
			if(!(spawn->GetMerchantType() & MERCHANT_TYPE_NO_BUY))
				SendSellMerchantList();
			if(!(spawn->GetMerchantType() & MERCHANT_TYPE_NO_BUY_BACK))
				SendBuyBackList();
		}
	}

}

void Client::BuyBack(int32 item_id, int8 quantity){
	Spawn* spawn = GetMerchantTransaction();
	if(spawn && spawn->GetMerchantID() > 0){
		deque<BuyBackItem*>::iterator itr;
		BuyBackItem* buyback = 0;
		BuyBackItem* closest = 0;
		MBuyBack.readlock(__FUNCTION__, __LINE__);
		for(itr = buy_back_items.begin(); itr != buy_back_items.end(); ++itr){
			buyback = *itr;
			if(buyback->unique_id == item_id){
				closest = buyback;
				quantity = buyback->quantity;
				break;
			}
		}
		MBuyBack.releasereadlock(__FUNCTION__, __LINE__);
		if(closest){
			Item* item = 0;
			Item* master_item = master_item_list.GetItem(closest->item_id);
			if(master_item){
				item = new Item(master_item);
				if(closest->quantity >= quantity)
					item->details.count = quantity;
				else
					item->details.count = closest->quantity;
			}
			if(!player->item_list.HasFreeSlot() && !player->item_list.CanStack(item))
				SimpleMessage(CHANNEL_COLOR_RED, "You do not have any slots available for this item.");
			else if(player->RemoveCoins(closest->quantity*closest->price)){
				bool removed = false;
				if(closest->quantity == quantity){
					MBuyBack.writelock(__FUNCTION__, __LINE__);
					for(itr = buy_back_items.begin(); itr != buy_back_items.end(); ++itr){
						if(*itr == closest){
							buy_back_items.erase(itr);
							removed = true;
							break;
						}
					}
					MBuyBack.releasewritelock(__FUNCTION__, __LINE__);
				}
				else{
					closest->quantity -= quantity;
					closest->save_needed = true;
				}
				AddItem(item);
				//EQ2Packet* outapp = player->SendInventoryUpdate(GetVersion());
				//if(outapp)
					//QueuePacket(outapp);
				if(removed){
					database.DeleteBuyBack(GetCharacterID(), closest->item_id, closest->quantity, closest->price);
					safe_delete(closest);
				}
				if(!(spawn->GetMerchantType() & MERCHANT_TYPE_NO_BUY))
					SendSellMerchantList();
				if(!(spawn->GetMerchantType() & MERCHANT_TYPE_NO_BUY_BACK))
					SendBuyBackList();
			}
			else
				SimpleMessage(CHANNEL_COLOR_RED, "You cannot afford this item.");
		}
	}

}

void Client::BuyItem(int32 item_id, int8 quantity){
	// Get the merchant we are buying from
	Spawn* spawn = GetMerchantTransaction();
	// Make sure the spawn has a merchant list
	if(spawn && spawn->GetMerchantID() > 0){
		float multiplier = CalculateBuyMultiplier(spawn->GetMerchantID());
		Item* master_item = master_item_list.GetItem(item_id);

		vector<MerchantItemInfo>* temp;
		vector<MerchantItemInfo>::iterator itr;
		MerchantItemInfo* ItemInfo = 0;
		temp = world.GetMerchantList(spawn->GetMerchantID());
		
		for (itr = temp->begin(); itr != temp->end(); ++itr) {
			if ((*itr).item_id == item_id) {
				ItemInfo = &(*itr);
				break;
			}
		}

		if(master_item && ItemInfo){
			int32 sell_price = 0;
			int16 total_available = 0;

			if (spawn->GetMerchantType() & MERCHANT_TYPE_LOTTO) {
				quantity = 1;
				total_available = 0xFFFF;
				sell_price = master_item->sell_price;
			}
			else {
				total_available = world.GetMerchantItemQuantity(spawn->GetMerchantID(), item_id);
				sell_price = (int32)(master_item->sell_price * multiplier);
				if(quantity > total_available)
					quantity = total_available;
			}
			int32 total_buy_price = sell_price * quantity;
			Item* item = new Item(master_item);
			item->details.count = quantity;
			if(!player->item_list.HasFreeSlot() && !player->item_list.CanStack(item)){
				SimpleMessage(CHANNEL_COLOR_RED, "You do not have any slots available for this item.");
				safe_delete(item);
			}
			else {
				// Price not set in the merchant_inventory table, use the old method
				if (ItemInfo->price_item_id == 0 && ItemInfo->price_item2_id == 0 && ItemInfo->price_status == 0 && ItemInfo->price_stationcash == 0 && ItemInfo->price_coins == 0) {
					if(player->RemoveCoins(total_buy_price)){
						item->SetMaxSellValue(sell_price);
						if(quantity > 1)
							Message(CHANNEL_COLOR_MERCHANT, "You buy %i \\aITEM %u 0:%s\\/a from %s for%s.", quantity, master_item->details.item_id, master_item->name.c_str(), spawn->GetName(), GetCoinMessage(total_buy_price).c_str());
						else
							Message(CHANNEL_COLOR_MERCHANT, "You buy \\aITEM %u 0:%s\\/a from %s for%s.", master_item->details.item_id, master_item->name.c_str(), spawn->GetName(), GetCoinMessage(total_buy_price).c_str());				
						AddItem(item);
						CheckPlayerQuestsItemUpdate(item);
						if(item && total_available < 0xFFFF){
							world.DecreaseMerchantQuantity(spawn->GetMerchantID(), item_id, quantity);
							SendBuyMerchantList();
						}
						//EQ2Packet* outapp = player->SendInventoryUpdate(GetVersion());
						//if(outapp)
						//	QueuePacket(outapp);
						if(!(spawn->GetMerchantType() & MERCHANT_TYPE_NO_BUY) && !(spawn->GetMerchantType() & MERCHANT_TYPE_LOTTO))
							SendSellMerchantList();
						if (spawn->GetMerchantType() & MERCHANT_TYPE_LOTTO)
							PlayLotto(total_buy_price, item->details.item_id);
					}
					else{
						Message(CHANNEL_COLOR_RED, "You do not have enough coin to purchase \\aITEM %u 0:%s\\/a.", master_item->details.item_id, master_item->name.c_str());
						GetCurrentZone()->SendSpellFailedPacket(this, SPELL_ERROR_NOT_ENOUGH_COIN);
						PlaySound("buy_failed");
					}
				}
				else {
					// Price set in merchant_inventory table

					// Check if the player has enough status, coins and staion cash to buy the item before checking the items
					// TODO: need to add support for station cash
					if (player->GetInfoStruct()->status_points >= ItemInfo->price_status && player->HasCoins(ItemInfo->price_coins)) {
						// Check items

						int16 item_quantity = 0;
						// Default these to true in case price_item_id or price_item2_id was never set
						bool hasItem1 = true;
						bool hasItem2 = true;
						Item* tempItem1 = 0;
						Item* tempItem2 = 0;
						if (ItemInfo->price_item_id != 0) {
							// Same item for whatever reason lets add the quantities together
							if (ItemInfo->price_item_id == ItemInfo->price_item2_id)
								item_quantity = ItemInfo->price_item_qty + ItemInfo->price_item2_qty;
							else
								item_quantity = ItemInfo->price_item_qty;

							tempItem1 = player->item_list.GetItemFromID(ItemInfo->price_item_id);
							if (tempItem1) {
								if (tempItem1->details.count < item_quantity)
									hasItem1 = false;
							}
							else {
								hasItem1 = false;
							}
						}

						// Check item2, if item_quantity is greater then item1 quantity then item2 is the same item
						// as item1 and we already checked for it so we can skip this check
						if (ItemInfo->price_item2_id != 0 && item_quantity <= ItemInfo->price_item_qty) {
							tempItem2 = player->item_list.GetItemFromID(ItemInfo->price_item2_id);
							if (tempItem2) {
								if (tempItem2->details.count < ItemInfo->price_item2_qty)
									hasItem2 = false;
							}
							else {
								hasItem2 = false;
							}
						}
						// if we have every thing then remove the price and give the item
						if (hasItem1 && hasItem2) {
							player->GetInfoStruct()->status_points -= ItemInfo->price_status;
							// TODO: station cash

							// The update that would normally be sent after modifing the players inventory is automatically sent in AddItem wich is called later
							// so there is no need to send it more then that one time
							if (tempItem1) {
								if (tempItem1->details.count > item_quantity) {
									tempItem1->details.count -= item_quantity;
									tempItem1->save_needed = true;
								}
								else {
									database.DeleteItem(GetCharacterID(), tempItem1, 0);
									player->item_list.DestroyItem(tempItem1->details.index);
								}
							}
							if (tempItem2) {
								if (tempItem2->details.count > ItemInfo->price_item2_qty) {
									tempItem2->details.count -= ItemInfo->price_item2_qty;
									tempItem2->save_needed = true;
								}
								else {
									database.DeleteItem(GetCharacterID(), tempItem2, 0);
									player->item_list.DestroyItem(tempItem2->details.index);
								}
							}


							// Checked to see if we had enough coins already so don't need to check the return type on RemoveCoins as it will always be true
							player->RemoveCoins(ItemInfo->price_coins);
							item->SetMaxSellValue(sell_price);
							if(quantity > 1)
								Message(CHANNEL_COLOR_MERCHANT, "You buy %i \\aITEM %u 0:%s\\/a from %s for%s.", quantity, master_item->details.item_id, master_item->name.c_str(), spawn->GetName(), GetCoinMessage(total_buy_price).c_str());
							else
								Message(CHANNEL_COLOR_MERCHANT, "You buy \\aITEM %u 0:%s\\/a from %s for%s.", master_item->details.item_id, master_item->name.c_str(), spawn->GetName(), GetCoinMessage(total_buy_price).c_str());				
							AddItem(item);
							CheckPlayerQuestsItemUpdate(item);
							if(item && total_available < 0xFFFF){
								world.DecreaseMerchantQuantity(spawn->GetMerchantID(), item_id, quantity);
								SendBuyMerchantList();
							}
							if(!(spawn->GetMerchantType() & MERCHANT_TYPE_NO_BUY) && !(spawn->GetMerchantType() & MERCHANT_TYPE_LOTTO))
								SendSellMerchantList();
							if (spawn->GetMerchantType() & MERCHANT_TYPE_LOTTO)
								PlayLotto(total_buy_price, item->details.item_id);

						}
						else{
							Message(CHANNEL_COLOR_RED, "You do not have enough coin to purchase \\aITEM %u 0:%s\\/a.", master_item->details.item_id, master_item->name.c_str());
							GetCurrentZone()->SendSpellFailedPacket(this, SPELL_ERROR_NOT_ENOUGH_COIN);
							PlaySound("buy_failed");
						}
					}
					else{
						Message(CHANNEL_COLOR_RED, "You do not have enough coin to purchase \\aITEM %u 0:%s\\/a.", master_item->details.item_id, master_item->name.c_str());
						GetCurrentZone()->SendSpellFailedPacket(this, SPELL_ERROR_NOT_ENOUGH_COIN);
						PlaySound("buy_failed");
					}
				}
			}
		}
	}

}

void Client::RepairItem(int32 item_id) {
	Spawn* spawn = GetMerchantTransaction();
	if (spawn) {
		Item* item = player->item_list.GetItemFromID(item_id);
		if (!item)
			item = player->GetEquipmentList()->GetItemFromItemID(item_id);
		if (item) {
			int32 repair_cost = item->CalculateRepairCost();
			if (player->RemoveCoins((int32)repair_cost)) {
				item->generic_info.condition = 100;
				item->save_needed = true;
				QueuePacket(player->GetEquipmentList()->serialize(GetVersion()));
				QueuePacket(player->SendInventoryUpdate(GetVersion()));
				QueuePacket(item->serialize(version, false, player));
				Message(CHANNEL_COLOR_MERCHANT, "You give %s %s to repair your \\aITEM %u 0:%s\\/a.", spawn->GetName(), GetCoinMessage(repair_cost).c_str(), item->details.item_id, item->name.c_str());
				PlaySound("coin_cha_ching");
				if (spawn->GetMerchantType() & MERCHANT_TYPE_REPAIR)
					SendRepairList();
			}
			else {
				LogWrite(MISC__TODO, 1, "TODO", " Send pop up that says 'Not enough coin.'\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
				Message(CHANNEL_COLOR_MERCHANT, "You do not have enough coin to repair \\aITEM %u 0:%s\\/a.", item->details.item_id, item->name.c_str());
				PlaySound("buy_failed");
			}
		}
	}

}

void Client::RepairAllItems() {
	Spawn* spawn = GetMerchantTransaction();
	if (spawn) {
		vector<Item*>* repairable_items = GetRepairableItems();
		if (repairable_items && repairable_items->size() > 0) {
			vector<Item*>::iterator itr;
			int64 total_cost = 0;
			for (itr = repairable_items->begin(); itr != repairable_items->end(); ++itr)
					total_cost += (*itr)->CalculateRepairCost();
			if (player->RemoveCoins((int32)total_cost)) {
				Message(CHANNEL_COLOR_MERCHANT, "You give %s to repair all of your items.", GetCoinMessage((int32)total_cost).c_str());
				for (itr = repairable_items->begin(); itr != repairable_items->end(); ++itr) {
					Item* item = *itr;
					if (item) {
						item->generic_info.condition = 100;
						item->save_needed = true;
						QueuePacket(item->serialize(version, false, player));
						Message(CHANNEL_COLOR_YELLOW, "Repaired: \\aITEM %u 0:%s\\/a.", item->details.item_id, item->name.c_str());
					}
				}
				QueuePacket(player->GetEquipmentList()->serialize(GetVersion()));
				QueuePacket(player->SendInventoryUpdate(GetVersion()));
				PlaySound("coin_cha_ching");
				if (spawn->GetMerchantType() & MERCHANT_TYPE_REPAIR)
					SendRepairList();
			}
			else {
				LogWrite(MISC__TODO, 1, "TODO", "Send pop up that says 'Not enough coin.'\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
				SimpleMessage(CHANNEL_COLOR_MERCHANT, "You do not have enough coin to repair all of your items.");
				PlaySound("buy_failed");
			}
		}
		safe_delete(repairable_items);
	}

}

void Client::SendAchievementsList()
{
	/*map<int32, Achievement *> *achievements = player->GetAchievementList()->GetAchievements();
	map<int32, Achievement *>::iterator itr;
	Achievement *achievement;
	vector<AchievementRequirements *> *requirements = 0;
	vector<AchievementRequirements *>::iterator itr2;
	AchievementRequirements *requirement;
	vector<AchievementRewards *> *rewards = 0;
	vector<AchievementRewards *>::iterator itr3;
	AchievementRewards *reward;
	PacketStruct *packet;
	int16 i = 0;
	int16 j = 0;
	int16 k = 0;

	if (!(packet = configReader.getStruct("WS_CharacterAchievements", version))) {
		return;
	}

	packet->setArrayLengthByName("num_achievements" , achievements->size());
	for (itr = achievements->begin(); itr != achievements->end(); itr++) {
		achievement = itr->second;
		packet->setArrayDataByName("achievement_id", achievement->GetID(), i);
		packet->setArrayDataByName("title", achievement->GetTitle(), i);
		packet->setArrayDataByName("uncompleted_text", achievement->GetUncompletedText(), i);
		packet->setArrayDataByName("completed_text", achievement->GetCompletedText(), i);
		packet->setArrayDataByName("category", achievement->GetCategory(), i);
		packet->setArrayDataByName("expansion", achievement->GetExpansion(), i);
		packet->setArrayDataByName("icon", achievement->GetIcon(), i);
		packet->setArrayDataByName("point_value", achievement->GetPointValue(), i);
		packet->setArrayDataByName("qty_req", achievement->GetQtyReq(), i);
		packet->setArrayDataByName("hide_achievement", achievement->GetHide(), i);
		packet->setArrayDataByName("unknown3", achievement->GetUnknown3a(), i);
		packet->setArrayDataByName("unknown3", achievement->GetUnknown3b(), i);
		requirements = achievement->GetRequirements();
		rewards = achievement->GetRewards();
		j = 0;
		k = 0;
		packet->setSubArrayLengthByName("num_items", requirements->size(), i, j);
		for (itr2 = requirements->begin(); itr2 != requirements->end(); itr2++) {
			requirement = *itr2;
			packet->setSubArrayDataByName("item_name", requirement->name.c_str(), i, j);
			packet->setSubArrayDataByName("item_qty_req", requirement->qty_req, i, j);
			j++;
		}
		packet->setSubArrayLengthByName("num_rewards", achievement->GetRewards()->size(), i, k);
		for (itr3 = rewards->begin(); itr3 != rewards->end(); itr3++) {
			reward = *itr3;
			packet->setSubArrayDataByName("reward_item", reward->reward.c_str(), i, k);
			k++;
		}
		i++;
	}

	//packet->PrintPacket();
	EQ2Packet* data = packet->serialize();
	EQ2Packet* app = new EQ2Packet(OP_ClientCmdMsg, data->pBuffer, data->size);
	safe_delete(packet);
	safe_delete(data);
	//DumpPacket(app);
	QueuePacket(app);*/
	QueuePacket(master_achievement_list.GetAchievementPacket());
	SendAchievementUpdate(true);
}

void Client::SendAchievementUpdate(bool first_login) {
	map<int32, AchievementUpdate *> *updates = player->GetAchievementUpdateList()->GetAchievementUpdates();
	map<int32, AchievementUpdate *>::iterator itr;
	AchievementUpdate *update;
	vector<AchievementUpdateItems *> *update_items = 0;
	vector<AchievementUpdateItems *>::iterator itr2;
	AchievementUpdateItems *update_item;

	int16 i = 0;

	PacketStruct *packet;
	
	if (!(packet = configReader.getStruct("WS_AchievementUpdate", version))) {
		return;
	}
	
	packet->setDataByName("unknown1", first_login ? 1 : 0);
	packet->setArrayLengthByName("num_achievements", updates->size());
	for (itr = updates->begin(); itr != updates->end(); ++itr) {
		int16 j = 0;
		update = itr->second;
		packet->setArrayDataByName("achievement_id", update->GetID(), i);
		packet->setArrayDataByName("completed_date", update->GetCompletedDate(), i);
		update_items = update->GetUpdateItems();
		packet->setSubArrayLengthByName("num_items", update_items->size(), i);
		for (itr2 = update_items->begin(); itr2 != update_items->end(); ++itr2) {
			update_item = *itr2;
			packet->setSubArrayDataByName("item_update", update_item->item_update, i, j);
			j++;
		}
	i++;
	}

	//packet->PrintPacket();
	EQ2Packet* data = packet->serialize();
	EQ2Packet* app = new EQ2Packet(OP_ClientCmdMsg, data->pBuffer, data->size);
	safe_delete(packet);
	safe_delete(data);
	//DumpPacket(app);
	QueuePacket(app);
}

void Client::SendBuyMerchantList(bool sell){
	Spawn* spawn = GetMerchantTransaction();
	if(spawn && spawn->GetMerchantID() > 0){
		vector<MerchantItemInfo>* items = world.GetMerchantItemList(spawn->GetMerchantID(), spawn->GetMerchantType(), player);
		if(items){
			PacketStruct* packet = configReader.getStruct("WS_UpdateMerchant", GetVersion());
			if(packet){
				float multiplier = CalculateBuyMultiplier(spawn->GetMerchantID());
				packet->setDataByName("spawn_id", player->GetIDWithPlayerSpawn(spawn));
				packet->setArrayLengthByName("num_items", items->size());
				vector<MerchantItemInfo>::iterator itr;
				sint8 item_difficulty = 0;
				int32 sell_price = 0;
				int i = 0;
				int tmp_level = 0;
				for(itr = items->begin(); itr != items->end(); ++itr, ++i){
					MerchantItemInfo ItemInfo = *itr;
					Item* item = master_item_list.GetItem(ItemInfo.item_id);
					if (!item)
						continue;
					
					packet->setArrayDataByName("item_name", item->name.c_str(), i);
					packet->setArrayDataByName("item_id", item->details.item_id, i);
					packet->setArrayDataByName("stack_size", item->stack_count, i);
					packet->setArrayDataByName("icon", item->details.icon, i);
					if(item->generic_info.adventure_default_level > 0)
						tmp_level = item->generic_info.adventure_default_level;
					else
						tmp_level = item->generic_info.tradeskill_default_level;
					packet->setArrayDataByName("level", tmp_level, i);
					packet->setArrayDataByName("tier", item->details.tier, i);
					packet->setArrayDataByName("item_id2", item->details.item_id, i);
					item_difficulty = player->GetArrowColor(tmp_level);
					if(item_difficulty != ARROW_COLOR_WHITE && item_difficulty != ARROW_COLOR_RED && item_difficulty != ARROW_COLOR_GRAY)
						item_difficulty = ARROW_COLOR_WHITE;
					item_difficulty -= 6;
					if(item_difficulty < 0)
						item_difficulty*=-1;
					packet->setArrayDataByName("item_difficulty", item_difficulty, i);
					packet->setArrayDataByName("quantity", ItemInfo.quantity, i);
					packet->setArrayDataByName("unknown5", 255, i);
					packet->setArrayDataByName("stack_size2", item->stack_count, i);

					if (GetVersion() <= 1096)
						packet->setArrayDataByName("description", item->description.c_str(), i);
					
					// If no price set in the merchant_inventory table then use the old method
					if (ItemInfo.price_item_id == 0 && ItemInfo.price_item2_id == 0 && ItemInfo.price_coins == 0 && ItemInfo.price_status == 0 && ItemInfo.price_stationcash == 0) {
						sell_price = (int32)(item->sell_price * multiplier);
						packet->setArrayDataByName("price", sell_price, i);
					}
					else {
						int8 count = 0;
						if (ItemInfo.price_item_id != 0 && ItemInfo.price_item_qty != 0)
							count++;
						if (ItemInfo.price_item2_id != 0 && ItemInfo.price_item2_qty != 0)
							count++;
						if (count != 0) {
							packet->setSubArrayLengthByName("num_tokens", count, i);
							int8 index = 0;
							Item* token = 0;
							if (ItemInfo.price_item_id != 0) {
								token = master_item_list.GetItem(ItemInfo.price_item_id);
								if (item) {
									packet->setSubArrayDataByName("token_icon", token->details.icon, i, index);
									packet->setSubArrayDataByName("token_qty", ItemInfo.price_item_qty, i, index);
									packet->setSubArrayDataByName("token_id", ItemInfo.price_item_id, i, index);
									packet->setSubArrayDataByName("token_name", token->name.c_str(), i, index);
								}
								token = 0;
								index++;
							}
							if (ItemInfo.price_item2_id != 0) {
								token = master_item_list.GetItem(ItemInfo.price_item2_id);
								if (item) {
									packet->setSubArrayDataByName("token_icon", token->details.icon, i, index);
									packet->setSubArrayDataByName("token_qty", ItemInfo.price_item2_qty, i, index);
									packet->setSubArrayDataByName("token_id", ItemInfo.price_item2_id, i, index);
									packet->setSubArrayDataByName("token_name", token->name.c_str(), i, index);
								}
							}
						}
						packet->setArrayDataByName("price", ItemInfo.price_coins, i);
						packet->setArrayDataByName("status", ItemInfo.price_status, i);
						packet->setArrayDataByName("station_cash", ItemInfo.price_stationcash, i);
					}
				}
				if (sell)
					packet->setDataByName("type", 130);
				else
					packet->setDataByName("type", 2);
				EQ2Packet* outapp = packet->serialize();
				//	DumpPacket(outapp);
				QueuePacket(outapp);
				safe_delete(packet);
			}
			safe_delete(items);
		}
		else {
			// Need to send an empty packet in the event there is no item list, otherwise the
			// last item list sent to the player will show for this merchant
			PacketStruct* packet = configReader.getStruct("WS_UpdateMerchant", GetVersion());
			if (packet) {
				packet->setDataByName("spawn_id", player->GetIDWithPlayerSpawn(spawn));

				if (sell)
					packet->setDataByName("type", 130);
				else
					packet->setDataByName("type", 2);

				EQ2Packet* outapp = packet->serialize();
				QueuePacket(outapp);
				safe_delete(packet);
			}
		}
	}

}

void Client::SendSellMerchantList(bool sell){
	Spawn* spawn = GetMerchantTransaction();
	if(spawn && spawn->GetMerchantID() > 0){
		map<int32, Item*>* items = player->GetItemList();
		if(items){
			PacketStruct* packet = configReader.getStruct("WS_UpdateMerchant", GetVersion());
			if(packet){
				vector<Item*> sellable_items;
				map<int32, Item*>::iterator test_itr;
				for(test_itr = items->begin(); test_itr != items->end(); ++test_itr){
					if(test_itr->second && !test_itr->second->CheckFlag(NO_VALUE))
						sellable_items.push_back(test_itr->second);
				}
				packet->setDataByName("spawn_id", player->GetIDWithPlayerSpawn(spawn));
				packet->setArrayLengthByName("num_items", sellable_items.size());
				vector<Item*>::iterator itr;
				Item* item = 0;
				float multiplier = CalculateSellMultiplier(spawn->GetMerchantID());
				int32 sell_price = 0;
				int i = 0;
				int tmp_level = 0;
				for(itr = sellable_items.begin(); itr != sellable_items.end(); ++itr, ++i){
					item = *itr;
					Item* master_item = master_item_list.GetItem(item->details.item_id);
					if(master_item)
						sell_price = (int32)(master_item->sell_price * multiplier);
					else
						sell_price = 0;
					if(sell_price > item->sell_price)
						sell_price = item->sell_price;
					packet->setArrayDataByName("item_name", item->name.c_str(), i);
					packet->setArrayDataByName("price", sell_price, i);
					packet->setArrayDataByName("item_id", item->details.item_id, i);
					packet->setArrayDataByName("unique_item_id", item->details.unique_id, i);
					packet->setArrayDataByName("stack_size", item->details.count, i);
					packet->setArrayDataByName("icon", item->details.icon, i);
					if(item->generic_info.adventure_default_level > 0)
						tmp_level = item->generic_info.adventure_default_level;
					else
						tmp_level = item->generic_info.tradeskill_default_level;
					packet->setArrayDataByName("level", tmp_level, i);
					packet->setArrayDataByName("tier", item->details.tier, i);
					packet->setArrayDataByName("item_id2", item->details.item_id, i);
					sint8 item_difficulty = player->GetArrowColor(tmp_level);
					if(item_difficulty != ARROW_COLOR_WHITE && item_difficulty != ARROW_COLOR_RED && item_difficulty != ARROW_COLOR_GRAY)
						item_difficulty = ARROW_COLOR_WHITE;
					item_difficulty -= 6;
					if(item_difficulty < 0)
						item_difficulty*=-1;
					packet->setArrayDataByName("item_difficulty", item_difficulty, i);
					if(item->details.count == 1)
						packet->setArrayDataByName("quantity", 0xFFFF, i);
					else
						packet->setArrayDataByName("quantity", item->details.count, i);
					packet->setArrayDataByName("stack_size2", item->details.count, i);
					if (GetVersion() <= 1096) {
						packet->setArrayDataByName("description", item->description.c_str(), i);
					}
				}
				if (sell)
					packet->setDataByName("type", 129);
				else
					packet->setDataByName("type", 1);
				EQ2Packet* outapp = packet->serialize();
				//DumpPacket(outapp);
				QueuePacket(outapp);
				safe_delete(packet);
			}
			safe_delete(items);
		}
	}

}

void Client::SendBuyBackList(bool sell){
	Spawn* spawn = GetMerchantTransaction();
	if(spawn && spawn->GetMerchantID() > 0){
		deque<BuyBackItem*>::iterator itr;
		Item* master_item = 0;
		PacketStruct* packet = configReader.getStruct("WS_UpdateMerchant", GetVersion());
		if(packet){
			packet->setDataByName("spawn_id", player->GetIDWithPlayerSpawn(spawn));
			packet->setArrayLengthByName("num_items", buy_back_items.size());
			sint8 item_difficulty = 0;
			MBuyBack.readlock(__FUNCTION__, __LINE__);
			int tmp_level = 0;
			int i = 0;
			for(itr = buy_back_items.begin(); itr != buy_back_items.end(); ++itr, ++i){
				BuyBackItem* buyback = *itr;
				master_item = master_item_list.GetItem(buyback->item_id);
				if(master_item){
					packet->setArrayDataByName("item_name", master_item->name.c_str(), i);
					packet->setArrayDataByName("price", buyback->price, i);
					packet->setArrayDataByName("item_id", master_item->details.item_id, i);
					packet->setArrayDataByName("unique_item_id", buyback->unique_id, i);
					packet->setArrayDataByName("stack_size", buyback->quantity, i);
					packet->setArrayDataByName("icon", master_item->details.icon, i);
					if(master_item->generic_info.adventure_default_level > 0)
						tmp_level = master_item->generic_info.adventure_default_level;
					else
						tmp_level = master_item->generic_info.tradeskill_default_level;
					packet->setArrayDataByName("level", tmp_level, i);
					packet->setArrayDataByName("tier", master_item->details.tier, i);
					packet->setArrayDataByName("item_id2", master_item->details.item_id, i);
					item_difficulty = player->GetArrowColor(tmp_level);
					if(item_difficulty != ARROW_COLOR_WHITE && item_difficulty != ARROW_COLOR_RED && item_difficulty != ARROW_COLOR_GRAY)
						item_difficulty = ARROW_COLOR_WHITE;
					item_difficulty -= 6;
					if(item_difficulty < 0)
						item_difficulty*=-1;
					packet->setArrayDataByName("item_difficulty", item_difficulty, i);
					if(buyback->quantity == 1)
						packet->setArrayDataByName("quantity", 0xFFFF, i);
					else
						packet->setArrayDataByName("quantity", buyback->quantity, i);
					packet->setArrayDataByName("stack_size2", buyback->quantity, i);
					if (GetVersion() <= 1096) {
						packet->setArrayDataByName("description", master_item->description.c_str(), i);
					}
				}
			}
			MBuyBack.releasereadlock(__FUNCTION__, __LINE__);
			if (sell)
				packet->setDataByName("type", 640);
			else
				packet->setDataByName("type", 512);
			EQ2Packet* outapp = packet->serialize();
			//	DumpPacket(outapp);
			QueuePacket(outapp);
			safe_delete(packet);
		}
	}

}

void Client::SendRepairList() {
	Spawn* spawn = GetMerchantTransaction();
	if (spawn) {
		vector<Item*>* repairable_items = GetRepairableItems();
		PacketStruct* packet = configReader.getStruct("WS_UpdateMerchant", GetVersion());
		if (packet) {
			packet->setDataByName("spawn_id", player->GetIDWithPlayerSpawn(spawn));
			packet->setArrayLengthByName("num_items", repairable_items->size());
			int32 i = 0;
			vector<Item*>::iterator itr;
			for (itr = repairable_items->begin(); itr != repairable_items->end(); ++itr, ++i) {
				Item* item = *itr;
				packet->setArrayDataByName("item_name", item->name.c_str(), i);
				packet->setArrayDataByName("price", item->CalculateRepairCost(), i);
				packet->setArrayDataByName("item_id", item->details.item_id, i);
				packet->setArrayDataByName("stack_size", item->details.count, i);
				packet->setArrayDataByName("icon", item->details.icon, i);
				/*if (item->generic_info.adventure_default_level > 0)
					tmp_level = item->generic_info.adventure_default_level;
				else
					tmp_level = item->generic_info.tradeskill_default_level;
				packet->setArrayDataByName("level", tmp_level, i);*/
				packet->setArrayDataByName("level", item->generic_info.adventure_default_level, i);
				packet->setArrayDataByName("tier", item->details.tier, i);
				packet->setArrayDataByName("item_id2", item->details.item_id, i);
				sint8 item_difficulty = player->GetArrowColor(item->generic_info.adventure_default_level);
				if(item_difficulty != ARROW_COLOR_WHITE && item_difficulty != ARROW_COLOR_RED && item_difficulty != ARROW_COLOR_GRAY)
					item_difficulty = ARROW_COLOR_WHITE;
				item_difficulty -= 6;
				if(item_difficulty < 0)
					item_difficulty*=-1;
				packet->setArrayDataByName("item_difficulty", item_difficulty, i);
				if(item->details.count == 1)
					packet->setArrayDataByName("quantity", 0xFFFF, i);
				else
					packet->setArrayDataByName("quantity", item->details.count, i);
				packet->setArrayDataByName("stack_size2", item->details.count, i);

				if (GetVersion() <= 1096) {
					packet->setArrayDataByName("description", item->description.c_str(), i);
				}
			}
			packet->setDataByName("type", 96);
			EQ2Packet* outapp = packet->serialize();
			QueuePacket(outapp);
			safe_delete(packet);
		}
		safe_delete(repairable_items);
	}

}

void Client::ShowLottoWindow() {
	Spawn* spawn = GetMerchantTransaction();
	if (spawn) {

		int32 item_id = rule_manager.GetGlobalRule(R_World, GamblingTokenItemID)->GetInt32();
		if( !item_id )
		{
			LogWrite(WORLD__ERROR, 0, "World", "No GamblingTokenItemID rule set!");
			SimpleMessage(CHANNEL_COLOR_RED, "The server admin has not setup a lotto item ticket.");

					return;
		}
		else if( item_id == 0 )
		{
			LogWrite(WORLD__ERROR, 0, "World", "Error! Invalid GamblingTokenItemID value!");

					return;
		}

		Item* item = master_item_list.GetItem(item_id);
		if (!item) {
			LogWrite(WORLD__ERROR, 0, "World", "The 'GamblingTokenItemID' rule value %u is not a valid item id.", item_id);

					return;
		}

		LogWrite(WORLD__DEBUG, 0, "World", "GamblingTokenItemID = '%s' (%u)", item->name.c_str(), item_id);

		PacketStruct* packet = configReader.getStruct("WS_UpdateMerchant", GetVersion());
		if (packet) {
			packet->setDataByName("spawn_id", player->GetIDWithPlayerSpawn(spawn));
			packet->setArrayLengthByName("num_items", 1);
			packet->setArrayDataByName("item_name", item->name.c_str());
			packet->setArrayDataByName("price", item->sell_price);
			packet->setArrayDataByName("item_id", item->details.item_id);
			packet->setArrayDataByName("stack_size", item->details.count);
			packet->setArrayDataByName("icon", item->details.icon);
			packet->setArrayDataByName("level", item->generic_info.adventure_default_level);
			packet->setArrayDataByName("tier", item->details.tier);
			packet->setArrayDataByName("item_id2", item->details.item_id);
			int8 item_difficulty = player->GetArrowColor(item->generic_info.adventure_default_level);
			if(item_difficulty != ARROW_COLOR_WHITE && item_difficulty != ARROW_COLOR_RED && item_difficulty != ARROW_COLOR_GRAY)
				item_difficulty = ARROW_COLOR_WHITE;
			item_difficulty -= 6;
			if(item_difficulty < 0)
				item_difficulty*=-1;
			packet->setArrayDataByName("item_difficulty", item_difficulty);
			//if(item->details.count == 1)
				packet->setArrayDataByName("quantity", 0xFFFF);
			//else
			//	packet->setArrayDataByName("quantity", item->details.count);
			packet->setArrayDataByName("stack_size2", item->details.count);
			packet->setArrayDataByName("description", item->description.c_str());
			packet->setDataByName("type", 0x00000102);
			QueuePacket(packet->serialize());
			safe_delete(packet);
		}
	}

}

void Client::PlayLotto(int32 price, int32 ticket_item_id) {
	PacketStruct* packet = configReader.getStruct("WS_Lottery", GetVersion());
	if (packet) {
		world.AddLottoPlayer(GetCharacterID(), Timer::GetCurrentTime2() + 4500);
		int32 rolls[6] = {0};
		int32 lottery_digits[6] = {0};
		int8 num_matches = 0;
		int64 jackpot = 0;
		Item* item = GetPlayer()->item_list.GetItemFromID(ticket_item_id);
		if (!item) {
					return;
		}
		database.DeleteItem(GetCharacterID(), item, 0);
		GetPlayer()->item_list.RemoveItem(item, true);
		QueuePacket(GetPlayer()->SendInventoryUpdate(GetVersion()));
		Variable* winning_numbers = variables.FindVariable("gambling_winning_numbers");
		if (!winning_numbers) {
			winning_numbers = new Variable("gambling_winning_numbers", "231205182236", "Current Gigglegibber Gambling Game winning numbers");
			variables.AddVariable(winning_numbers);
			database.SaveVariable(winning_numbers->GetName(), winning_numbers->GetValue(), winning_numbers->GetComment());
		}
		if (strlen(winning_numbers->GetValue()) != 12) {
			winning_numbers->SetValue("231205182236");
			database.SaveVariable(winning_numbers->GetName(), winning_numbers->GetValue(), winning_numbers->GetComment());
		}
		try {
			for (int32 i = 0; i < 12; i+=2) {
				char num[4];
				strncpy(num, winning_numbers->GetValue() + i, 2);
				lottery_digits[i / 2] = atoi(num);
			}
		}
		catch (...) {
			LogWrite(WORLD__ERROR, 0, "World", "Error parsing 'gambling_winning_numbers' variable");

					return;
		}
		Variable* jackpot_var = variables.FindVariable("gambling_current_jackpot");
		if (!jackpot_var) {
			jackpot_var = new Variable("gambling_current_jackpot", "10000", "Current Gigglegibber Gambling Game Jackpot");
			variables.AddVariable(jackpot_var);
			database.SaveVariable(jackpot_var->GetName(), jackpot_var->GetValue(), jackpot_var->GetComment());
		}
		try {
			jackpot = atoul(jackpot_var->GetValue());
			if (jackpot < 10000)
				jackpot = 10000;
		}
		catch (...) {
			jackpot = 10000;
		}
		char new_jackpot[128] = {0};
		sprintf(new_jackpot, "%llu", jackpot + price);
		jackpot_var->SetValue(new_jackpot);
		database.SaveVariable(jackpot_var->GetName(), jackpot_var->GetValue(), jackpot_var->GetComment());
		world.PickRandomLottoDigits(rolls);
		packet->setDataByName("roll_digit1", rolls[0]);
		packet->setDataByName("roll_digit2", rolls[1]);
		packet->setDataByName("roll_digit3", rolls[2]);
		packet->setDataByName("roll_digit4", rolls[3]);
		packet->setDataByName("roll_digit5", rolls[4]);
		packet->setDataByName("roll_digit6", rolls[5]);
		packet->setDataByName("lottery_digit1", lottery_digits[0]);
		packet->setDataByName("lottery_digit2", lottery_digits[1]);
		packet->setDataByName("lottery_digit3", lottery_digits[2]);
		packet->setDataByName("lottery_digit4", lottery_digits[3]);
		packet->setDataByName("lottery_digit5", lottery_digits[4]);
		packet->setDataByName("lottery_digit6", lottery_digits[5]);
		QueuePacket(packet->serialize());
		safe_delete(packet);
		for (int32 i = 0; i < 6; i++) {
			for (int32 j = 0; j < 6; j++) {
				if (rolls[i] == lottery_digits[j]) {
					num_matches++;
					break;
				}
			}
		}
		char new_jackpot_str[16];
		memset(new_jackpot_str, 0, sizeof(new_jackpot_str));
		world.SetLottoPlayerNumMatches(GetCharacterID(), num_matches);
		if (num_matches == 6) {
			world.PickRandomLottoDigits(lottery_digits);
			for (int32 i = 0; i < 12; i+=2)
				sprintf(new_jackpot_str + i, "%02u", lottery_digits[i / 2]);
			winning_numbers->SetValue(new_jackpot_str);
			jackpot_var->SetValue("10000");
			database.SaveVariable(winning_numbers->GetName(), winning_numbers->GetValue(), winning_numbers->GetComment());
			database.SaveVariable(jackpot_var->GetName(), jackpot_var->GetValue(), jackpot_var->GetComment());
		}
	}

}

void Client::SendGuildCreateWindow() {
	Spawn* spawn = GetPlayer()->GetTarget();
	if (spawn) {
		PacketStruct* packet = configReader.getStruct("WS_UpdateMerchant", GetVersion());
		if (packet) {
			packet->setDataByName("spawn_id", player->GetIDWithPlayerSpawn(spawn));
			packet->setArrayLengthByName("num_items", 0);
			packet->setDataByName("type", 0x00008000);
			QueuePacket(packet->serialize());
			safe_delete(packet);
		}
	}

}

void Client::AddBuyBack(int32 unique_id, int32 item_id, int8 quantity, int32 price, bool save_needed){
	BuyBackItem* item = new BuyBackItem;
	item->item_id = item_id;
	item->unique_id = unique_id;
	item->price = price;
	item->quantity = quantity;
	item->save_needed = save_needed;
	MBuyBack.writelock(__FUNCTION__, __LINE__);
	buy_back_items.push_back(item);
	if(buy_back_items.size() > 10){
		safe_delete(buy_back_items.front());
		buy_back_items.pop_front();
	}
	MBuyBack.releasewritelock(__FUNCTION__, __LINE__);

}

deque<BuyBackItem*>*	Client::GetBuyBacks(){
	return &buy_back_items;
}

vector<Item*>* Client::GetRepairableItems() {
	vector<Item*>* repairable_items = new vector<Item*>;
	vector<Item*>* equipped_items = player->GetEquipmentList()->GetAllEquippedItems();
	map<int32, Item*>* items = player->GetItemList();
	if (equipped_items && equipped_items->size() > 0) {
		for (int32 i = 0; i < equipped_items->size(); i++) {
			Item* item = equipped_items->at(i);
			if (item && item->generic_info.condition < 100)
				repairable_items->push_back(item);
		}
	}
	if (items && items->size() > 0) {
		map<int32, Item*>::iterator itr;
		for (itr = items->begin(); itr != items->end(); ++itr) {
			Item* item = itr->second;
			if (item && item->generic_info.condition < 100)
				repairable_items->push_back(item);
		}
	}
	safe_delete(equipped_items);
	safe_delete(items);

	return repairable_items;
}

void Client::SendMailList() {
	int32 kiosk_id = player->GetIDWithPlayerSpawn(GetMailTransaction());
	if (kiosk_id > 0) {
		PacketStruct* p = configReader.getStruct("WS_GetMailHeader", GetVersion());
		if (p) {
			MutexMap<int32, Mail*>* mail_list = player->GetMail();
			MutexMap<int32, Mail*>::iterator itr = mail_list->begin();
			int16 i = 0;
			p->setDataByName("kiosk_id", kiosk_id);
			p->setArrayLengthByName("num_messages", (int16)mail_list->size());
			while (itr.Next()) {
				Mail* mail = itr->second;
				p->setArrayDataByName("mail_id", mail->mail_id, i);
				p->setArrayDataByName("player_to_id", mail->player_to_id, i);
				p->setArrayDataByName("player_from", mail->player_from.c_str(), i);
				p->setArrayDataByName("subject", mail->subject.c_str(), i);
				p->setArrayDataByName("unknown1", 0x0000, i);
				p->setArrayDataByName("already_read", mail->already_read, i);
				p->setArrayDataByName("mail_deletion", mail->expire_time - mail->time_sent, i);
				p->setArrayDataByName("mail_type", mail->mail_type, i);
				p->setArrayDataByName("mail_expire", 0xFFFFFFFF, i);
				p->setArrayDataByName("unknown1a", 0xFFFFFFFF, i);
				p->setArrayDataByName("coin_copper", mail->coin_copper, i);
				p->setArrayDataByName("coin_silver", mail->coin_silver, i);
				p->setArrayDataByName("coin_gold", mail->coin_gold, i);
				p->setArrayDataByName("coin_plat", mail->coin_plat, i);
				p->setArrayDataByName("num_items", mail->stack, i);
				p->setArrayDataByName("packettype", GetItemPacketType(GetVersion()));
				p->setArrayDataByName("packetsubtype", 0xFF, i);

				p->setArrayDataByName("unknown2", 0, i);
				//p->setArrayDataByName("item_id", 5, i, 0);
				//Item* item = master_item_list.GetItem(5);
				//if (item)
				//	printf("%s\n", item->name.c_str());
				//else
				//	printf("no item\n");
				//p->setItemArrayDataByName("item", master_item_list.GetItem(5), i, 0);
				i++;
			}

			// GMs send mail for free!
			if( GetAdminStatus() > 0 )
			{
				p->setDataByName("postage_cost", 0);
				p->setDataByName("attachment_cost", 0);
			}
			else
			{
				p->setDataByName("postage_cost", 10);
				p->setDataByName("attachment_cost", 50);
			}
			p->setDataByName("unknown3", 0x01F4);
			p->setDataByName("unknown4", 0x01000000);
			QueuePacket(p->serialize());
			safe_delete(p);
		}
	}
	else
		SimpleMessage(CHANNEL_COLOR_MAIL, "You are currently not in a mail transaction.");

}

void Client::DisplayMailMessage(int32 mail_id) {
	Mail* mail = player->GetMail(mail_id);
	if (mail) {
		int32 kiosk_id = player->GetIDWithPlayerSpawn(GetMailTransaction());
		if (kiosk_id > 0) {
			PacketStruct* update = configReader.getStruct("WS_UpdatePlayerMail", GetVersion());
			if (update) {
				update->setDataByName("action", 0x03);
				update->setDataByName("packettype", GetItemPacketType(GetVersion()));
				update->setDataByName("packetsubtype", 0xFF);
				QueuePacket(update->serialize());
				safe_delete(update);
			}
			PacketStruct* packet = configReader.getStruct("WS_MailGetMessage", GetVersion());
			if (packet) {
				packet->setDataByName("kiosk_id", kiosk_id);
				packet->setDataByName("mail_id", mail->mail_id);
				packet->setDataByName("player_to_id", mail->player_to_id);
				packet->setDataByName("player_from", mail->player_from.c_str());
				packet->setDataByName("subject", mail->subject.c_str());
				packet->setDataByName("mail_body", mail->mail_body.c_str());
				packet->setDataByName("unknown1", 1);
				packet->setDataByName("unknown2", 0);
				packet->setDataByName("lock_report_button", 1);
				packet->setDataByName("unknown3", 0xFFFFFFFF);
				packet->setDataByName("unknown3a", 0xFFFFFFFF);
				packet->setDataByName("coin_copper", mail->coin_copper);
				packet->setDataByName("coin_silver", mail->coin_silver);
				packet->setDataByName("coin_gold", mail->coin_gold);
				packet->setDataByName("coin_plat", mail->coin_plat);
				packet->setDataByName("stack", mail->stack);
				packet->setDataByName("packettype", GetItemPacketType(GetVersion()));
				packet->setDataByName("packetsubtype", 0xFF);
				packet->setDataByName("unknown4", 0);
				packet->setDataByName("unknown5", 0);
				packet->setDataByName("unknown6", 0);
				packet->setDataByName("unknown7", 0);
				mail->already_read = true;
				mail->save_needed = true;
				QueuePacket(packet->serialize());
				safe_delete(packet);
				//SendMailList();
			}
		}
		else
			SimpleMessage(CHANNEL_COLOR_MAIL, "You are currently not in a mail transaction.");
	}

}

/* This is called when the client sends a mail message.  This determines whether or not the mail can be sent and must send the reply
   packet back to the client before the mail actually sent. */
void Client::HandleSentMail(EQApplicationPacket* app) {
	PacketStruct* packet = configReader.getStruct("WS_MailSendMessage", GetVersion());
	if (packet) {
		packet->LoadPacketData(app->pBuffer, app->size);
		string player_to = packet->getType_EQ2_16BitString_ByName("player_to").data;
		PacketStruct* reply_packet = configReader.getStruct("WS_MailSendMessageReply", GetVersion());
		vector<int32>* ids = 0;
		if (reply_packet) {
			int8 reply_type = MAIL_SEND_RESULT_UNKNOWN_ERROR;
			if (player_to.length() == 0)
				reply_type = MAIL_SEND_RESULT_EMPTY_TO_LIST;
			else if (player_to.compare(string(GetPlayer()->GetName())) == 0)
				reply_type = MAIL_SEND_RESULT_CANNOT_SEND_TO_SELF;
			else if (GetAdminStatus() == 0 && !player->RemoveCoins(10))
				reply_type = MAIL_SEND_RESULT_NOT_ENOUGH_COIN;
			else {
				if (player_to.compare("<all>") == 0) {
					if (mail_window.coin_copper + mail_window.coin_silver + mail_window.coin_gold + mail_window.coin_plat == 0)
						ids = database.GetAllPlayerIDs();
					else
						SimpleMessage(CHANNEL_COLOR_MAIL, "You may not mail gifts to multiple players.");
				}
				else {
					ids = new vector<int32>;
					ids->push_back(database.GetCharacterID(player_to.c_str()));
				}
				if (ids) {
					for (int32 i = 0; i < ids->size(); i++) {
						int32 player_to_id = ids->at(i);
						if (player_to_id > 0) {
							reply_type = MAIL_SEND_RESULT_SUCCESS;
							Mail* mail = new Mail;
							mail->mail_id = 0;
							mail->player_to_id = player_to_id;
							mail->player_from = string(GetPlayer()->GetName());
							mail->subject = packet->getType_EQ2_16BitString_ByName("subject").data;
							mail->mail_body = packet->getType_EQ2_16BitString_ByName("mail_body").data;
							mail->already_read = 0;
							mail->mail_type = MAIL_TYPE_REGULAR;
							mail->coin_copper = mail_window.coin_copper;
							mail->coin_silver = mail_window.coin_silver;
							mail->coin_gold = mail_window.coin_gold;
							mail->coin_plat = mail_window.coin_plat;
							mail->stack = 0;

							// GM's send mail for free!
							if( GetAdminStatus() > 0 )
							{
								mail->postage_cost = 0;
								mail->attachment_cost = 0;
							}
							else
							{
								mail->postage_cost = 10;
								mail->attachment_cost = 50;
							}
							mail->char_item_id = 0;
							mail->time_sent = Timer::GetUnixTimeStamp();
							mail->expire_time = mail->time_sent + 2592000;	//30 days in seconds
							//int16 packettype = packet->getType_int16_ByName("packettype");
							//int8 packetsubtype = packet->getType_int8_ByName("packetsubtype");
							/*int32 postage_cost = packet->getType_int32_ByName("postage_cost");
							int32 attachment_cost = packet->getType_int32_ByName("attachment_cost");
							if (postage_cost > 0 || attachment_cost > 0)
								PlaySoundA("coin_cha_ching");*/
							mail->save_needed = false;
							Client* to_client = zone_list.GetClientByCharID(player_to_id);
							if (to_client) {
								to_client->GetPlayer()->AddMail(mail);
								to_client->SimpleMessage(CHANNEL_COLOR_MAIL, "You've got mail! :)");
							}
							database.SavePlayerMail(mail);
							mail_window.coin_copper = 0;
							mail_window.coin_silver = 0;
							mail_window.coin_gold = 0;
							mail_window.coin_plat = 0;
						}
						else
							reply_type = MAIL_SEND_RESULT_UNKNOWN_PLAYER;
					}
				}
			}
			string players_to = "";
			if (ids) {
				for (int32 i = 0; i < ids->size(); i++) {
					if (ids->at(i) != 0)
						players_to.append(database.GetCharacterName(ids->at(i)));
					if (i < (ids->size() - 1))
						players_to.append(", ");
				}
			}
			reply_packet->setDataByName("player_to", players_to.c_str());
			reply_packet->setDataByName("reply_type", reply_type);
			QueuePacket(reply_packet->serialize());
			safe_delete(reply_packet);
			safe_delete(ids);
		}
		safe_delete(packet);
	}

}

void Client::DeleteMail(int32 mail_id, bool from_database) {
	
	player->DeleteMail(mail_id, from_database);

}

bool Client::AddMailCoin(int32 copper, int32 silver, int32 gold, int32 plat) {
	
	bool ret = false;
	if (GetMailTransaction()) {
		PacketStruct* packet = configReader.getStruct("WS_UpdatePlayerMail", GetVersion());
		if (packet) {
			if (copper > 0) {
				if (player->RemoveCoins(copper)) {
					mail_window.coin_copper += copper;
					Message(CHANNEL_COLOR_MAIL, "You add %u copper to the mail window.", copper);
					ret = true;
				}	
			}
			else if (silver > 0) {
				if (player->RemoveCoins(silver * 100)) {
					mail_window.coin_silver += silver;
					Message(CHANNEL_COLOR_MAIL, "You add %u silver to the mail window.", silver);
					ret = true;
				}	
			}
			else if (gold > 0) {
				if (player->RemoveCoins(gold * 10000)) {
					mail_window.coin_gold += gold;
					Message(CHANNEL_COLOR_MAIL, "You add %u gold to the mail window.", gold);
					ret = true;
				}	
			}
			else if (plat > 0) {
				if (player->RemoveCoins(plat * 1000000)) {
					mail_window.coin_plat += plat;
					Message(CHANNEL_COLOR_MAIL, "You add %u platinum to the mail window.", plat);
					ret = true;
				}	
			}
			if (ret) {
				packet->setDataByName("coin_copper", mail_window.coin_copper);
				packet->setDataByName("coin_silver", mail_window.coin_silver);
				packet->setDataByName("coin_gold", mail_window.coin_gold);
				packet->setDataByName("coin_plat", mail_window.coin_plat);
				packet->setDataByName("stack", 0);
				packet->setDataByName("packettype", GetItemPacketType(version));
				packet->setDataByName("packetsubtype", 0xFF);
				packet->setDataByName("unknown2", 0);
				//packet->PrintPacket();
				QueuePacket(packet->serialize());
			}
			else
				SimpleMessage(CHANNEL_COLOR_MAIL, "You don't have that much money.");
			safe_delete(packet);
		}
	}
	else
		SimpleMessage(CHANNEL_COLOR_MAIL, "You are currently not in a mail transaction.");

	return ret;
}

bool Client::RemoveMailCoin(int32 copper, int32 silver, int32 gold, int32 plat) {
	bool ret = false;
	if (GetMailTransaction()) {
		PacketStruct* packet = configReader.getStruct("WS_UpdatePlayerMail", GetVersion());
		if (packet) {
			if (copper > 0) {
				player->AddCoins(copper);
				mail_window.coin_copper -= copper;
				Message(CHANNEL_COLOR_MAIL, "You remove %u copper from the mail window.", copper);
				ret = true;
			}
			else if (silver > 0) {
				player->AddCoins(silver * 100);
				mail_window.coin_silver -= silver;
				Message(CHANNEL_COLOR_MAIL, "You remove %u silver from the mail window.", silver);
				ret = true;
			}
			else if (gold > 0) {
				player->AddCoins(gold * 10000);
				mail_window.coin_gold -= gold;
				Message(CHANNEL_COLOR_MAIL, "You remove %u gold from the mail window.", gold);
				ret = true;	
			}
			else if (plat > 0) {
				player->AddCoins(plat * 1000000);
				mail_window.coin_plat -= plat;
				Message(CHANNEL_COLOR_MAIL, "You remove %u platinum from the mail window.", plat);
				ret = true;	
			}
			if (ret) {
				packet->setDataByName("coin_copper", mail_window.coin_copper);
				packet->setDataByName("coin_silver", mail_window.coin_silver);
				packet->setDataByName("coin_gold", mail_window.coin_gold);
				packet->setDataByName("coin_plat", mail_window.coin_plat);
				packet->setDataByName("stack", 0);
				packet->setDataByName("packettype", 0x2BFE);
				packet->setDataByName("packetsubtype", 0xFF);
				packet->setDataByName("unknown2", 0);
				QueuePacket(packet->serialize());
			}
			safe_delete(packet);
		}
	}
	else
		SimpleMessage(CHANNEL_COLOR_MAIL, "You are currently not in a mail transaction.");

	return ret;
}

void Client::TakeMailAttachments(int32 mail_id) {
	if (GetMailTransaction()) {
		Mail* mail = player->GetMail(mail_id);
		if (mail) {
			int64 amount = 0;
			if (mail->coin_copper > 0) {
				amount += mail->coin_copper;
				mail->coin_copper = 0;
			}
			if (mail->coin_silver > 0) {
				amount += mail->coin_silver * 100;
				mail->coin_silver = 0;
			}
			if (mail->coin_gold > 0) {
				amount += mail->coin_gold * 10000;
				mail->coin_gold = 0;
			}
			if (mail->coin_plat > 0) {
				amount += mail->coin_plat * 1000000;
				mail->coin_plat = 0;
			}
			if (mail->char_item_id > 0) {
				LogWrite(MISC__TODO, 1, "TODO", "Items by Mail\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
			}
			/*	Can't find the right packet to send to update the player's mail.  This packet below updates the mail the player is sending, not
			    the mail the player is getting attachments from.  There is an opcode OP_MailRemoveAttachFromMailMsg with opcode 328 but i can't
				find it in any packet logs.*/
			/*PacketStruct* packet = configReader.getStruct("WS_UpdatePlayerMail", GetVersion());
			if (packet) {
				packet->setDataByName("unknown", 0x03);
				packet->setDataByName("coin_copper", mail->coin_copper);
				packet->setDataByName("coin_silver", mail->coin_silver);
				packet->setDataByName("coin_gold", mail->coin_gold);
				packet->setDataByName("coin_plat", mail->coin_plat);
				packet->setDataByName("stack", 0);
				packet->setDataByName("packettype", 0x2BFE);
				packet->setDataByName("packetsubtype", 0xFF);
				packet->setDataByName("unknown2", 0);
				packet->setDataByName("unknown3", 0x00000001);//0x00000016
				DumpPacket(packet->serialize());
				QueuePacket(packet->serialize());
				safe_delete(packet);
			}*/
			database.SavePlayerMail(mail);
			if (amount > 0)
				player->AddCoins(amount);
			SendMailList();
		}
	}
	else
		SimpleMessage(CHANNEL_COLOR_MAIL, "You are currently not in a mail transaction.");

}

void Client::CancelSendMail() {
	SimpleMessage(CHANNEL_COLOR_MAIL, "You cancel sending a letter.");
	player->AddCoins(mail_window.coin_copper + (mail_window.coin_silver * 100) + (mail_window.coin_gold * 10000) + (mail_window.coin_plat * 1000000));
	mail_window.coin_copper = 0;
	mail_window.coin_silver = 0;
	mail_window.coin_gold = 0;
	mail_window.coin_plat = 0;
	mail_window.char_item_id = 0;

}

bool Client::GateAllowed(){
	ZoneServer* zone = zone_list.Get(player->GetPlayerInfo()->GetBindZoneID());

	LogWrite(MISC__TODO, 1, "TODO", "possibly add a check here to see if a player is allowed to gate from this spot, allow for now\nfile: %s, func: %s, line: %i", __FILE__, __FUNCTION__, __LINE__);

	if(zone)
		return true;
	return false;
}

bool Client::BindAllowed(){

	LogWrite(MISC__TODO, 1, "TODO", "possibly add a check here to see if a player is allowed to bind in this spot, allow anywhere for now\nfile: %s, func: %s, line: %i", __FILE__, __FUNCTION__, __LINE__);

	return true;
}

bool Client::Bind(){
	player->GetPlayerInfo()->SetBindZone(GetCurrentZone()->GetZoneID());
	player->GetPlayerInfo()->SetBindX(player->GetX());
	player->GetPlayerInfo()->SetBindY(player->GetY());
	player->GetPlayerInfo()->SetBindZ(player->GetZ());
	player->GetPlayerInfo()->SetBindHeading(player->GetHeading());

	return true;
}

bool Client::Gate(){
	if (player->GetPlayerInfo()->GetBindZoneID() == 0)
		return false;

	ZoneServer* zone = zone_list.Get(player->GetPlayerInfo()->GetBindZoneID());
	if(zone){
		player->SetX(player->GetPlayerInfo()->GetBindZoneX());
		player->SetY(player->GetPlayerInfo()->GetBindZoneY());
		player->SetZ(player->GetPlayerInfo()->GetBindZoneZ());
		player->SetHeading(player->GetPlayerInfo()->GetBindZoneHeading());
		Zone(zone, false);

			return true;
	}

	return false;
}

void Client::ProcessTeleport(Spawn* spawn, vector<TransportDestination*>* destinations, int32 transport_id){
	if(!destinations || !spawn) {
			return;
	}

	bool has_map = false;
	if (transport_id > 0)
		has_map = GetCurrentZone()->TransportHasMap(transport_id);

	transport_spawn = spawn;
	vector<TransportDestination*> transport_list;
	vector<TransportDestination*>::iterator itr;
	TransportDestination* destination = 0;
	for(itr = destinations->begin(); itr != destinations->end(); ++itr){
		destination = *itr;
		if(has_map || (destination->type == TRANSPORT_TYPE_ZONE && ((destination->destination_zone_id != GetCurrentZone()->GetZoneID()) || GetPlayer()->GetDistance(destination->destination_x, destination->destination_y, destination->destination_z) > 100)))
			transport_list.push_back(destination);
	}
	if(transport_list.size() == 0 && destination){
		if(destination->destination_zone_id == 0 || destination->destination_zone_id == GetCurrentZone()->GetZoneID()){
			EQ2Packet* app = GetPlayer()->Move(destination->destination_x, destination->destination_y, destination->destination_z, GetVersion());
			if(app)
				QueuePacket(app);
		}
		else{
			ZoneServer* new_zone = zone_list.Get(destination->destination_zone_id);

			// determine if this is an instanced zone that already exists
			ZoneServer* instance_zone = GetPlayer()->GetGroupMemberInZone(destination->destination_zone_id);

			if(instance_zone || new_zone){
				GetPlayer()->SetX(destination->destination_x);
				GetPlayer()->SetY(destination->destination_y);
				GetPlayer()->SetZ(destination->destination_z);
				GetPlayer()->SetHeading(destination->destination_heading);
				if ( instance_zone )
					Zone(instance_zone->GetInstanceID(),false,true);
				else
					Zone(new_zone, false);
			}
		}
		if(destination->message.length() > 0)
			SimpleMessage(CHANNEL_COLOR_YELLOW, destination->message.c_str());
	}
	else if(transport_list.size() > 0){
		PlaySound("mariner_bell");

		PacketStruct* packet = configReader.getStruct("WS_TeleportList", GetVersion());
		if(packet){
			packet->setDataByName("spawn_id", GetPlayer()->GetIDWithPlayerSpawn(spawn));

			// Put all the destinations the player can go in a new vector
			vector<TransportDestination*> destinations;
			for (int32 i = 0; i < transport_list.size(); i++) {
				destination = transport_list.at(i);

				// Check min level
				if (destination->min_level > 0 && GetPlayer()->GetLevel() < destination->min_level)
					continue;
				// Check max level
				if (destination->max_level > 0 && GetPlayer()->GetLevel() > destination->max_level)
					continue;
				// Check quest complete
				if (destination->req_quest_complete > 0 && GetPlayer()->GetCompletedQuest(destination->req_quest_complete) == 0)
					continue;
				// Check req quest and step
				if (destination->req_quest > 0 && destination->req_quest_step > 0 && GetPlayer()->GetQuestStep(destination->req_quest) != destination->req_quest_step)
					continue;
				// If we have a map and our current location is the same as the detination and player is within 20 units from the transport set the "current" elements but don't addt to the destination list
				if (has_map && (destination->destination_zone_id == GetCurrentZone()->GetZoneID() && GetPlayer()->GetDistance(destination->destination_x, destination->destination_y, destination->destination_z) < 20)) {
					packet->setDataByName("current_zone", destination->display_name.c_str());
					packet->setDataByName("current_map_x", destination->map_x);
					packet->setDataByName("current_map_y", destination->map_y);
				}
				else {
					destinations.push_back(destination);
				}
			}

			// Use the new vector to create the packet
			destination = 0;
			packet->setArrayLengthByName("num_destinations", destinations.size());
			for(int32 i = 0; i < destinations.size(); i++) {
				destination = destinations.at(i);				

				packet->setArrayDataByName("unique_id", destination->unique_id, i);
				packet->setArrayDataByName("display_name", destination->display_name.c_str(), i);
				packet->setArrayDataByName("zone_name", destination->display_name.c_str(), i);
				packet->setArrayDataByName("zone_file_name", destination->display_name.c_str(), i);
				packet->setArrayDataByName("cost", destination->cost, i);
				
				if (has_map) {
					packet->setArrayDataByName("map_x", destination->map_x, i);
					packet->setArrayDataByName("map_y", destination->map_y, i);
				}

			}

			if (has_map)
				packet->setDataByName("map_name", GetCurrentZone()->GetTransportMap(transport_id).c_str());
			EQ2Packet* app = packet->serialize();
			//DumpPacket(app);
			if (destinations.size() > 0)
				QueuePacket(app);
			safe_delete(packet);
		}
	}

}	

void Client::ProcessTeleportLocation(EQApplicationPacket* app){
	PacketStruct* packet = configReader.getStruct("WS_TeleportDestination", GetVersion());
	if(packet){
		if(packet->LoadPacketData(app->pBuffer, app->size)){
			Spawn* spawn = GetPlayer()->GetSpawnWithPlayerID(packet->getType_int32_ByName("spawn_id"));
			int32 unique_id = packet->getType_int32_ByName("unique_id");
			string zone_name = packet->getType_EQ2_16BitString_ByName("zone_name").data;
			int32 cost = packet->getType_int32_ByName("cost");
			vector<TransportDestination*>* destinations = 0;
			TransportDestination* destination = 0;
			if(spawn && spawn == transport_spawn && spawn->GetTransporterID() > 0)
				destinations = GetCurrentZone()->GetTransporters(spawn->GetTransporterID());
			if(destinations){
				vector<TransportDestination*>::iterator itr;
				for(itr = destinations->begin(); itr != destinations->end(); ++itr){
					if((*itr)->unique_id == unique_id && (*itr)->display_name == zone_name && (*itr)->cost == cost){
						destination=*itr;
						break;
					}
				}
			}
			if(!destination)
				SimpleMessage(CHANNEL_COLOR_RED, "Error processing transport.");
			else{
				if(cost == 0 || player->RemoveCoins(cost)){
					if(destination->destination_zone_id == 0 || destination->destination_zone_id == GetCurrentZone()->GetZoneID()){
						EQ2Packet* outapp = GetPlayer()->Move(destination->destination_x, destination->destination_y, destination->destination_z, GetVersion());
						if(outapp)
							QueuePacket(outapp);
					}
					else{
						GetPlayer()->SetX(destination->destination_x);
						GetPlayer()->SetY(destination->destination_y);
						GetPlayer()->SetZ(destination->destination_z);
						GetPlayer()->SetHeading(destination->destination_heading);

						// Test if where we're going is an Instanced zone
						if (!TryZoneInstance(destination->destination_zone_id, false)) {
							LogWrite(INSTANCE__DEBUG, 0, "Instance", "Attempting to zone normally");
							ZoneServer* new_zone = zone_list.Get(destination->destination_zone_id);
							Zone(new_zone, false);
						}
					}
					if(destination->message.length() > 0)
						SimpleMessage(CHANNEL_COLOR_YELLOW, destination->message.c_str());
				}
				else
					SimpleMessage(CHANNEL_COLOR_RED, "You do not have enough money to use this transport.");
			}
		}
		safe_delete(packet);
	}

}

void Client::SendNewSpells(int8 class_id){
	if(class_id > 0){
		vector<Spell*>* spells = master_spell_list.GetSpellListByAdventureClass(class_id, player->GetLevel(), 1);
		vector<Spell*>::iterator itr;
		bool send_updates = false;
		for(itr = spells->begin(); itr != spells->end(); ++itr){
			Spell* spell = *itr;
			if(spell && !player->HasSpell(spell->GetSpellID(), spell->GetSpellTier(), true) && spell->GetSpellData()->lua_script.length() > 0){
				send_updates = true;
				SendSpellUpdate(spell);
				player->AddSpellBookEntry(spell->GetSpellID(), spell->GetSpellTier(), player->GetFreeSpellBookSlot(spell->GetSpellData()->spell_book_type), spell->GetSpellData()->spell_book_type, spell->GetSpellData()->linked_timer, true);
				player->UnlockSpell(spell);
			}
		}
		if(send_updates){
			EQ2Packet* outapp = player->GetSpellBookUpdatePacket(GetVersion());
			if(outapp)
				QueuePacket(outapp);
		}
		safe_delete(spells);
	}

}

void Client::SetItemSearch(vector<Item*>* items){
	if(items){
		if(search_items){
			safe_delete(search_items);
		}
		search_items = items;
	}

}

vector<Item*>* Client::GetSearchItems(){
	return search_items;
}

void Client::SearchStore(int32 page){
	if(search_items){
		PacketStruct* packet = configReader.getStruct("WS_BrokerItems", GetVersion());
		if(packet){
			int32 x = page*8;
			if(search_items->size() > 8){
				if((search_items->size() - x) > 8)
					packet->setArrayLengthByName("num_items", 8);
				else
					packet->setArrayLengthByName("num_items", search_items->size() - x);
			}
			else
				packet->setArrayLengthByName("num_items", search_items->size());
			if(search_items->size() > 0){
				packet->setArrayLengthByName("num_sellers", 1);
				packet->setArrayDataByName("seller_seller_id", 1);
				packet->setArrayDataByName("seller_name", "EQ2EmuDevs");
				packet->setDataByName("per_page", 8);
				packet->setDataByName("num_pages", search_items->size()/8+1);
				packet->setDataByName("page", page);
				int32 limit = search_items->size() > 8 ? 8 : search_items->size();
				for(int32 i=0;i<limit;i++, x++){
					if(x >= search_items->size())
						break;
					Item* item = search_items->at(x);
					packet->setArrayDataByName("item_id", item->details.item_id, i);
					packet->setArrayDataByName("item_id2", item->details.item_id, i);
					packet->setArrayDataByName("icon", item->details.icon, i);
					//packet->setArrayDataByName("unknown2b", i, i);
					packet->setArrayDataByName("item_seller_id", 1, i);
					if(item->stack_count == 0)
						packet->setArrayDataByName("quantity", 1, i);
					else
						packet->setArrayDataByName("quantity", item->stack_count, i);
					packet->setArrayDataByName("stack_size", item->stack_count, i);
					packet->setArrayDataByName("item_name", item->name.c_str(), i);
					packet->setArrayDataByName("req_level", item->generic_info.adventure_default_level, i);
					//QueuePacket(item->serialize(GetVersion(), false, GetPlayer()));
				}
			}
			/*EQ2Packet* outapp = packet->serialize();
			DumpPacket(outapp);
			QueuePacket(outapp);*/
			QueuePacket(packet->serialize());
			safe_delete(packet);
		}
	}

}

void Client::SetReadyForSpawns(bool val){ 
	ready_for_spawns = val; 
	if(GetPlayer()->GetActivityStatus() > 0){
		GetPlayer()->SetActivityStatus(0);
		if (GetPlayer()->GetGroupMemberInfo()) {
			world.GetGroupManager()->GroupMessage(GetPlayer()->GetGroupMemberInfo()->group_id, "%s has returned from Linkdead.", GetPlayer()->GetName());
		}
	}
	zone_list.CheckFriendZoned(this);

}

void Client::SendChatRelationship(int8 type, const char* name){
	if(!name) {
			return;
	}
	PacketStruct* packet = configReader.getStruct("WS_ChatRelationship", GetVersion());
	if(packet){
		packet->setDataByName("account_id", GetAccountID());
		packet->setDataByName("type", type);
		packet->setArrayLengthByName("num_names", 1);
		packet->setArrayDataByName("name", name);
		if(type == 0){
			Client* client = zone_list.GetClientByCharName(name);
			if(client){
				packet->setArrayDataByName("location", client->GetCurrentZone()->GetZoneName());
				packet->setArrayDataByName("class_name", classes.GetClassName(client->GetPlayer()->GetAdventureClass()));
			}
		}
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

}

void Client::SendFriendList(){
	PacketStruct* packet = configReader.getStruct("WS_ChatRelationship", GetVersion());
	if(packet){
		packet->setDataByName("account_id", GetAccountID());
		map<string, int8>::iterator itr;
		map<string, int8>* friends = player->GetFriends();
		if(friends && friends->size() > 0){		
			vector<string> names;
			for(itr = friends->begin(); itr != friends->end(); ++itr){
				if(itr->second == 2)
					continue;
				names.push_back(itr->first);
			}
			packet->setArrayLengthByName("num_names", names.size());
			for(int32 i=0;i<names.size();i++){
				Client* client = zone_list.GetClientByCharName(names[i]);
				packet->setArrayDataByName("name", names[i].c_str(), i);
				if(client){
					packet->setArrayDataByName("location", client->GetCurrentZone()->GetZoneName(), i);
					packet->setArrayDataByName("class_name", classes.GetClassName(client->GetPlayer()->GetAdventureClass()), i);
				}
			}
		}
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

}

void Client::SendIgnoreList(){
	PacketStruct* packet = configReader.getStruct("WS_ChatRelationship", GetVersion());
	if(packet){
		packet->setDataByName("account_id", GetAccountID());
		packet->setDataByName("type", 2);
		map<string, int8>::iterator itr;
		map<string, int8>* ignored = player->GetIgnoredPlayers();
		if(ignored && ignored->size() > 0){
			vector<string> names;
			for(itr = ignored->begin(); itr != ignored->end(); ++itr){
				if(itr->second == 2)
					continue;
				names.push_back(itr->first);
			}
			packet->setArrayLengthByName("num_names", names.size());
			for(int32 i=0;i<names.size();i++)
				packet->setArrayDataByName("name", names[i].c_str(), i);
		}
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

}

void Client::AddWaypoint(const char* waypoint_name, int8 waypoint_category, int32 spawn_id) {
	if (waypoint_name) {
		PacketStruct* packet = configReader.getStruct("WS_WaypointUpdate", GetVersion());
		if (packet) {
			packet->setArrayLengthByName("num_updates", 1);
			packet->setArrayDataByName("waypoint_name", waypoint_name, 0);
			packet->setArrayDataByName("waypoint_category", waypoint_category, 0);
			packet->setArrayDataByName("spawn_id", spawn_id, 0);
			packet->setArrayDataByName("waypoint_category2", waypoint_category, 0);
			packet->setArrayDataByName("spawn_id2", spawn_id, 0);
			QueuePacket(packet->serialize());
			safe_delete(packet);
		}
	}

}

void Client::BeginWaypoint(const char* waypoint_name, float x, float y, float z) {
	if (waypoint_name) {
		PacketStruct* packet = configReader.getStruct("WS_GlowPath", GetVersion());
		if (packet) {
			packet->setArrayLengthByName("num_points", 1);
			packet->setArrayDataByName("x", x, 0);
			packet->setArrayDataByName("y", y, 0);
			packet->setArrayDataByName("z", z, 0);
			packet->setDataByName("waypoint_x", x);
			packet->setDataByName("waypoint_y", y);
			packet->setDataByName("waypoint_z", z);
			packet->setDataByName("waypoint_name", waypoint_name);
			packet->setDataByName("unknown", 0);
			QueuePacket(packet->serialize());
			safe_delete(packet);
		}
	}

}

void Client::InspectPlayer(Player* player_to_inspect) {
	if (player_to_inspect) {
		PacketStruct* packet = configReader.getStruct("WS_InspectPlayer", GetVersion());
		if (packet) {
			packet->setDataByName("unknown", 0);
			packet->setSmallStringByName("name", player_to_inspect->GetName());
			packet->setDataByName("race", player_to_inspect->GetRace());
			packet->setDataByName("gender", player_to_inspect->GetGender());
			packet->setDataByName("adventure_level", player_to_inspect->GetLevel());

			LogWrite(MISC__TODO, 1, "TODO", "Put mentored level here (adventure_level_effective)\nfile: %s, func: %s, line: %i", __FILE__, __FUNCTION__, __LINE__);

			packet->setDataByName("adventure_level_effective", player_to_inspect->GetLevel());
			packet->setDataByName("adventure_class", player_to_inspect->GetAdventureClass());
			packet->setDataByName("tradeskill_level", player_to_inspect->GetTSLevel());
			packet->setDataByName("tradeskill_class", player_to_inspect->GetTradeskillClass());
			packet->setDataByName("health", player_to_inspect->GetHP());
			packet->setDataByName("health_max", player_to_inspect->GetTotalHP());
			packet->setDataByName("health_base", player_to_inspect->GetTotalHPBase());
			packet->setDataByName("power", player_to_inspect->GetPower());
			packet->setDataByName("power_max", player_to_inspect->GetTotalPower());
			packet->setDataByName("power_base", player_to_inspect->GetTotalPowerBase());
			packet->setDataByName("mitigation", 0);
			packet->setDataByName("unknown1", 0);
			packet->setDataByName("avoidance", 0);
			packet->setDataByName("unknown2", 0);
			packet->setDataByName("mitigation_percentage", 0);
			packet->setDataByName("strength", player_to_inspect->GetStr());
			packet->setDataByName("strength_base", player_to_inspect->GetStrBase());
			packet->setDataByName("stamina", player_to_inspect->GetSta());
			packet->setDataByName("stamina_base", player_to_inspect->GetStaBase());
			packet->setDataByName("agility", player_to_inspect->GetAgi());
			packet->setDataByName("agility_base", player_to_inspect->GetAgiBase());
			packet->setDataByName("wisdom", player_to_inspect->GetWis());
			packet->setDataByName("wisdom_base", player_to_inspect->GetWisBase());
			packet->setDataByName("intelligence", player_to_inspect->GetInt());
			packet->setDataByName("intelligence_base", player_to_inspect->GetIntBase());
			packet->setDataByName("unknown4", 0);
			packet->setDataByName("unknown5", 0);
			packet->setDataByName("unknown6", 0);
			packet->setDataByName("unknown7", 0);
			packet->setDataByName("unknown8", 0);
			packet->setDataByName("unknown9", 0);
			packet->setDataByName("unknown10", 0);
			packet->setDataByName("unknown11", 0);
			packet->setDataByName("unknown12", 0);
			packet->setDataByName("heat_resist", player_to_inspect->GetHeatResistance());
			packet->setDataByName("heat_resist_base", player_to_inspect->GetHeatResistanceBase());
			packet->setDataByName("heat_resist_percentage", 0);
			packet->setDataByName("cold_resist", player_to_inspect->GetColdResistance());
			packet->setDataByName("cold_resist_base", player_to_inspect->GetColdResistanceBase());
			packet->setDataByName("cold_resist_percentage", 0);
			packet->setDataByName("magic_resist", player_to_inspect->GetMagicResistance());
			packet->setDataByName("magic_resist_base", player_to_inspect->GetMagicResistanceBase());
			packet->setDataByName("magic_resist_percentage", 0);
			packet->setDataByName("mental_resist", player_to_inspect->GetMentalResistance());
			packet->setDataByName("mental_resist_base", player_to_inspect->GetMentalResistanceBase());
			packet->setDataByName("mental_resist_percentage", 0);
			packet->setDataByName("divine_resist", player_to_inspect->GetDivineResistance());
			packet->setDataByName("divine_resist_base", player_to_inspect->GetDivineResistanceBase());
			packet->setDataByName("divine_resist_percentage", 0);
			packet->setDataByName("disease_resist", player_to_inspect->GetDiseaseResistance());
			packet->setDataByName("disease_resist_base", player_to_inspect->GetDiseaseResistanceBase());
			packet->setDataByName("disease_resist_percentage", 0);
			packet->setDataByName("poison_resist", player_to_inspect->GetPoisonResistance());
			packet->setDataByName("poison_resist_base", player_to_inspect->GetPoisonResistanceBase());
			packet->setDataByName("poison_resist_percentage", 0);
			packet->setArrayLengthByName("num_chars", 0x01FF);
			string biography = player_to_inspect->GetBiography();
			for (size_t i = 0; i < biography.length(); i++)
				packet->setArrayDataByName("biography_char", biography[i], i);
	
			LogWrite(MISC__TODO, 0, "TODO", "Why is inspect player weapons commented out? in func: %s, line: %i", __FUNCTION__, __LINE__);
			/*Item* pw = player_to_inspect->GetEquipmentList()->GetItem(0);
			if (pw)
				packet->setItemByName("primary", pw, player_to_inspect, 0);
			Item* sw = player_to_inspect->GetEquipmentList()->GetItem(1);
			if (sw)
				packet->setItemByName("secondary", sw, player_to_inspect, 0);*/
			//DumpPacket(packet->serialize());
			QueuePacket(packet->serialize());
			safe_delete(packet);
		}
	}

}

void Client::SetPendingGuildInvite(Guild* guild, Player* invited_by) {
	pending_guild_invite.guild = guild;
	pending_guild_invite.invited_by = invited_by;
}

void Client::ShowClaimWindow() {
	PacketStruct* packet = configReader.getStruct("WS_PromoFlagsDetails", GetVersion());
	if (packet) {
		map<int32, Item*>* items = &master_item_list.items;
		map<int32, Item*>::iterator itr;
		int32 i = 0;
		if (items->size() > 10)
			packet->setArrayLengthByName("num_claim_items", 10);
		else
			packet->setArrayLengthByName("num_claim_items", items->size());
		for (itr = items->begin(); itr != items->end(); ++itr) {
			if (i == 10)
				break;
			Item* item = itr->second;
			packet->setArrayDataByName("id", i, i);
			packet->setArrayDataByName("not_yet_claimed", 1, i);
			packet->setArrayDataByName("num_remaining", 5, i);
			packet->setArrayDataByName("one_per_character", 1, i);
			packet->setArrayDataByName("claimed_on_this_char", 0, i);
			packet->setArrayDataByName("item_name", item->name.c_str(), i);
			packet->setArrayDataByName("text", "If you ever see this text, let Scatman know, thanks! :)", i);
			packet->setArrayDataByName("category", "Scott's Shit", i);
			packet->setArrayDataByName("icon", item->details.icon, i);
			packet->setArrayDataByName("item_id", item->details.item_id, i);
			i++;
		}
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

}

void Client::ShowGuildSearchWindow() {
	PacketStruct* packet = configReader.getStruct("WS_GuildRecruiting", GetVersion());
	if (packet) {
		MutexMap<int32, Guild*>* guilds = guild_list.GetGuilds();
		MutexMap<int32, Guild*>::iterator itr = guilds->begin();
		packet->setArrayLengthByName("num_guilds", guilds->size());
		int32 i = 0;
		while (itr.Next()) {
			Guild* guild = itr.second;
			packet->setArrayDataByName("guild_id", guild->GetID(), i);
			packet->setArrayDataByName("guild_name", guild->GetName(), i);
			packet->setArrayDataByName("recruiting_short_description", guild->GetRecruitingShortDesc().c_str(), i);
			packet->setArrayDataByName("descriptive_tag1", guild->GetRecruitingDescTag(0), i);
			packet->setArrayDataByName("descriptive_tag2", guild->GetRecruitingDescTag(1), i);
			packet->setArrayDataByName("descriptive_tag3", guild->GetRecruitingDescTag(2), i);
			packet->setArrayDataByName("descriptive_tag4", guild->GetRecruitingDescTag(3), i);
			packet->setArrayDataByName("playstyle", guild->GetRecruitingPlayStyle(), i);
			packet->setArrayDataByName("looking_for", guild->GetRecruitingLookingForPacketValue(), i); //tradeskillers, fighters, new
			packet->setArrayDataByName("unknown7", 0x02, i);
			packet->setArrayDataByName("min_level", guild->GetRecruitingMinLevel(), i);
			i++;
		}
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

}

void Client::ShowDressingRoom(Item *item, sint32 crc) {
	PacketStruct *packet;
	vector<int8> *slot_data;
	vector<int8>::iterator itr;
	int32 slots = 0;

	assert(item);

	if (!(packet = configReader.getStruct("WS_DressingRoom", GetVersion()))) {
			return;
	}

	slot_data = &item->slot_data;
	for (itr = slot_data->begin(); itr != slot_data->end(); ++itr) {
		if (version >= 1188)
			slots = *itr;
		else
			slots += (int8)pow(2.0, *itr);
	}

	packet->setDataByName("slot", slots);
	packet->setDataByName("appearance_id", item->generic_info.appearance_id);
	if(slots == 2){
		packet->setDataByName("rgb", item->generic_info.appearance_red, 2);
		packet->setDataByName("rgb", item->generic_info.appearance_blue, 0);
	}
	else{
		packet->setDataByName("rgb", item->generic_info.appearance_red, 0);
		packet->setDataByName("rgb", item->generic_info.appearance_blue, 2);
	}
	packet->setDataByName("rgb", item->generic_info.appearance_green, 1);
	if(slots == 4){
		packet->setDataByName("highlight_rgb", item->generic_info.appearance_highlight_red, 2);
		packet->setDataByName("highlight_rgb", item->generic_info.appearance_highlight_green, 1);
		packet->setDataByName("highlight_rgb", item->generic_info.appearance_highlight_blue, 0);
	}
	else if(slots == 7){
		packet->setDataByName("highlight_rgb", item->generic_info.appearance_highlight_red, 1);
		packet->setDataByName("highlight_rgb", item->generic_info.appearance_highlight_green, 0);
		packet->setDataByName("highlight_rgb", item->generic_info.appearance_highlight_blue, 2);
	}
	else{
		packet->setDataByName("highlight_rgb", item->generic_info.appearance_highlight_red, 0);
		packet->setDataByName("highlight_rgb", item->generic_info.appearance_highlight_green, 1);
		packet->setDataByName("highlight_rgb", item->generic_info.appearance_highlight_blue, 2);
	}
	packet->setDataByName("icon", item->details.icon);
	packet->setDataByName("item_id", item->details.item_id);
	packet->setDataByName("item_crc", crc);
	packet->setDataByName("unknown2", 0xFFFFFFFF);
	packet->setDataByName("unknown4", 0xFFFFFFFF);
	packet->setDataByName("unknown5", 0xFF, 9);
	packet->setDataByName("unknown5", 0xFF, 10);
	QueuePacket(packet->serialize());
	safe_delete(packet);

}

void Client::SendCollectionList() {
	map<int32, Collection *> *collections = player->GetCollectionList()->GetCollections();
	map<int32, Collection *>::iterator itr;
	vector<struct CollectionItem *> *collection_items;
	Collection *collection;
	struct CollectionItem *collection_item;
	PacketStruct *packet = 0;
	int16 i = 0, j;

	if (!(packet = configReader.getStruct("WS_CollectionUpdate", version)))
		return;

	packet->setArrayLengthByName("num_collections", collections->size());
	for (itr = collections->begin(); itr != collections->end(); ++itr) {
		collection = itr->second;
		collection_items = collection->GetCollectionItems();
		
		packet->setArrayDataByName("unknown", 1, i);
		packet->setArrayDataByName("collection_name", collection->GetName(), i);
		packet->setArrayDataByName("collection_category", collection->GetCategory(), i);
		packet->setArrayDataByName("completed", collection->GetCompleted(), i);
		packet->setArrayDataByName("collection_id", collection->GetID(), i);
		packet->setArrayDataByName("level", collection->GetLevel(), i);
		packet->setArrayDataByName("ready_to_turn_in", collection->GetIsReadyToTurnIn(), i);
		packet->setSubArrayLengthByName("num_items", collection_items->size(), i);

		for (j = 0; j < collection_items->size(); j++) {
			collection_item = collection_items->at(j);
			packet->setSubArrayDataByName("item_icon", collection_item->item->details.icon, i, j);
			if (version < 955)
				packet->setSubArrayDataByName("item_name", collection_item->item->name.c_str(), i, j);
			else
				packet->setSubArrayDataByName("item_id", collection_item->item->details.item_id, i, j);
			packet->setSubArrayDataByName("item_flag", collection_item->found, i, j);
		}
		i++;
	}

	packet->setDataByName("new_collection_flag", 1);

	QueuePacket(packet->serialize());
	safe_delete(packet);

}

bool Client::SendCollectionsForItem(Item *item) {
	map<int32, Collection *> collections_to_send;
	map<int32, Collection *> *collections;
	map<int32, Collection *>::iterator itr;
	vector<struct CollectionItem *> *collection_items;
	Collection *collection;
	struct CollectionItem *collection_item;
	PacketStruct *packet;
	int16 i;

	assert(item);

	/* get any collections required by this item that the player already has */
	collections = player->GetCollectionList()->GetCollections();
	for (itr = collections->begin(); itr != collections->end(); ++itr) {
		collection = itr->second;
		if (!collection->GetCompleted() && !collection->GetIsReadyToTurnIn() && collection->NeedsItem(item)) {
			LogWrite(COLLECTION__DEBUG, 0, "Collect", "Adding collection from player list %u\n", collection->GetID());
			collections_to_send[collection->GetID()] = collection;
		}
	}

	/* get any collections required by this item that the player does not have and send it to the client */
	collections = master_collection_list.GetCollections();
	for (itr = collections->begin(); itr != collections->end(); ++itr) {
		collection = itr->second;
		if (collection->NeedsItem(item) && !player->GetCollectionList()->GetCollection(collection->GetID())) {
			if (!(packet = configReader.getStruct("WS_CollectionUpdate", version))) {
							return false;
			}

			packet->setArrayLengthByName("num_collections", 1);
			packet->setArrayDataByName("unknown", 1, 0);
			packet->setArrayDataByName("collection_name", collection->GetName(), 0);
			packet->setArrayDataByName("collection_category", collection->GetCategory(), 0);
			packet->setArrayDataByName("completed", 0, 0);
			packet->setArrayDataByName("collection_id", collection->GetID(), 0);
			packet->setArrayDataByName("level", collection->GetLevel(), 0);
			packet->setArrayDataByName("ready_to_turn_in", 0, 0);
			packet->setArrayDataByName("unknown3", 0x28, 0);

			collection_items = collection->GetCollectionItems();
			packet->setSubArrayLengthByName("num_items", collection_items->size(), 0);
			for (i = 0; i < collection_items->size(); i++) {
				collection_item = collection_items->at(i);
				packet->setSubArrayDataByName("item_icon", collection_item->item->details.icon, 0, i);
				if (version < 955)
					packet->setSubArrayDataByName("item_name", collection_item->item->name.c_str(), 0, i);
				else
					packet->setSubArrayDataByName("item_id", collection_item->item->details.item_id, 0, i);
				packet->setSubArrayDataByName("item_flag", collection_item->found, 0, i);
			}
			packet->setDataByName("new_collection_flag", 0);

			QueuePacket(packet->serialize());
			safe_delete(packet);

			LogWrite(COLLECTION__DEBUG, 0, "Collect", "Adding collection from master list %u\n", collection->GetID());
			collections_to_send[collection->GetID()] = collection;
		}
	}

	/* send the client a list of collections that should be filtered for this item */
	if (collections_to_send.size() > 0) {
		if (!(packet = configReader.getStruct("WS_CollectionFilter", version))) {
					return false;
		}

		i = 0;
		packet->setArrayLengthByName("num_filters", collections_to_send.size());
		for (itr = collections_to_send.begin(); itr != collections_to_send.end(); ++itr) {
			collection = itr->second;

			packet->setArrayDataByName("collection_id", collection->GetID(), i);
			packet->setArrayDataByName("collection_item_num", collection->GetCollectionItemByItemID(item->details.item_id)->index, i);
			i++;
		}

		packet->setDataByName("item_icon", item->details.icon);
		packet->setDataByName("item_name", item->name.c_str());
		packet->setDataByName("item_id", item->details.item_id);
		packet->setDataByName("unknown3", player->GetCollectionList()->Size());

		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

	return collections_to_send.size() > 0;
}

void Client::HandleCollectionAddItem(int32 collection_id, Item *item) {
	Collection *collection;
	struct CollectionItem *collection_item;
	PacketStruct *packet;
	char tmp[200] = {0};

	assert(item);

	/* first try to get the collection from the player's collection list. if it's not found, get it from the master list */
	if ((collection = player->GetCollectionList()->GetCollection(collection_id))) {

		/* get the item struct that represents the item for this collection */
		if (!(collection_item = collection->GetCollectionItemByItemID(item->details.item_id))) {
			SendCollectionList();
			LogWrite(COLLECTION__ERROR, 0, "Collect", "Error in Client::HandleCollectionAddItem. Could not find item '%s' required by collection '%s'", item->name.c_str(), collection->GetName());
			return;
		}

		/* sanity check */
		if (collection_item->found) {
			SendCollectionList();
			LogWrite(COLLECTION__ERROR, 0, "Collect", "Error in Client::HandleCollectionAddItem. Player '%s' has already found item '%s' for collection '%s'", player->GetName(), item->name.c_str(), collection->GetName());
			return;
		}
	}
	else if ((collection = master_collection_list.GetCollection(collection_id))) {
		collection = new Collection(collection);

		/* get the item struct that represents the item for this collection */
		if (!(collection_item = collection->GetCollectionItemByItemID(item->details.item_id))) {
			SendCollectionList();
			LogWrite(COLLECTION__ERROR, 0, "Collect", "Error in Client::HandleCollectionAddItem. Could not find item '%s' required by collection '%s'", item->name.c_str(), collection->GetName());
			safe_delete(collection);
			return;
		}

		/* add the new collection to the player's collection list */
		if (!player->GetCollectionList()->AddCollection(collection)) {
			SendCollectionList();
			LogWrite(COLLECTION__ERROR, 0, "Collect", "Error in Client::HandleCollectionAddItem. Player '%s' already has collection '%s'", player->GetName(), collection->GetName());
			safe_delete(collection);
			return;
		}
	}
	else {
		LogWrite(COLLECTION__ERROR, 0, "Collect", "Error in Client::HandleCollectionAddItem. Could not find collection with id %u", collection_id);
		return;
	}

	collection_item->found = 1;
	collection->SetSaveNeeded(true);

	if (!(packet = configReader.getStruct("WS_CollectionItem", version))) {
		SendCollectionList();
		LogWrite(COLLECTION__ERROR, 0, "Collect", "Error in Client::HandleCollectionAddItem. Could not find struct 'WS_CollectionItem'");
		return;
	}

	packet->setDataByName("collection_id", collection_id);
	packet->setDataByName("collection_item_num", collection_item->index);
	packet->setDataByName("add", 1);
	QueuePacket(packet->serialize());
	Message(CHANNEL_COLOR_YELLOW,"You added: %s to %s", collection_item->item->name.c_str(), collection->GetName());
	sprintf(tmp, "You added: %s to %s", collection_item->item->name.c_str(), collection->GetName());
	SendPopupMessage(5,tmp, "quest_item", 3.5, 0x64, 0xFF, 0xFF);
	safe_delete(packet);

	RemoveItem(item, 1);
	SendCollectionList();

}

void Client::DisplayCollectionComplete(Collection *collection) {
	vector<struct CollectionRewardItem *> *reward_items;
	vector<struct CollectionRewardItem *> *selectable_reward_items;
	struct CollectionRewardItem *reward_item;
	PacketStruct *packet;
	int32 i;

	assert(collection);

	if (!(packet = configReader.getStruct("WS_QuestComplete", version))) {
			return;
	}

	reward_items = collection->GetRewardItems();
	selectable_reward_items = collection->GetSelectableRewardItems();

	packet->setDataByName("title", "Quest Reward!");
	packet->setDataByName("name", collection->GetName());
	packet->setDataByName("description", collection->GetCategory());
	packet->setDataByName("level", collection->GetLevel());
	packet->setDataByName("max_coin", collection->GetRewardCoin());
	packet->setDataByName("min_coin", collection->GetRewardCoin());
	//packet->setDataByName("status_points", quest->GetStatusPoints());
	packet->setArrayLengthByName("num_rewards", reward_items->size());
	for (i = 0; i < reward_items->size(); i++) {
		reward_item = reward_items->at(i);
		packet->setArrayDataByName("reward_id", reward_item->item->details.item_id, i);
		if (version < 860)
			packet->setItemArrayDataByName("item",reward_item->item, player, i, 0, -1);
		else if (version < 1193)
			packet->setItemArrayDataByName("item",reward_item->item, player, i);
		else
			packet->setItemArrayDataByName("item",reward_item->item, player, i, 0, 2);
	}
	packet->setArrayLengthByName("num_select_rewards", selectable_reward_items->size());
	for(i = 0; i < selectable_reward_items->size(); i++) {
		reward_item = selectable_reward_items->at(i);
		packet->setArrayDataByName("select_reward_id", reward_item->item->details.item_id, i);
		if(version < 860)
			packet->setItemArrayDataByName("select_item", reward_item->item, player, i, 0, -1);
		else if (version < 1193)
			packet->setItemArrayDataByName("select_item", reward_item->item, player, i);
		else
			packet->setItemArrayDataByName("select_item", reward_item->item, player, i, 0, 2);
	}

	QueuePacket(packet->serialize());
	safe_delete(packet);

}

void Client::HandInCollections() {
	map<int32, Collection *> *collections;
	map<int32, Collection *>::iterator itr;

	/* only show 1 collection reward dialog at a time */
	if (player->GetPendingCollectionReward()) {
			return;
	}

	collections = player->GetCollectionList()->GetCollections();

	/* we only want to display 1 collection reward dialog at a time. so once we find one to display, send it and return. once the player accepts the reward
	for this collection, this function gets called again and the process repeats until there are no more collections to hand in */
	for (itr = collections->begin(); itr != collections->end(); ++itr) {
		Collection* collection = itr->second;
		if (collection->GetIsReadyToTurnIn()) {
			player->SetPendingCollectionReward(collection);
			DisplayCollectionComplete(collection);

					return;
		}
	}

}

void Client::AcceptCollectionRewards(Collection *collection, int32 selectable_item_id) {
	vector<struct CollectionRewardItem *> *reward_items;
	vector<struct CollectionRewardItem *>::iterator itr;
	struct CollectionRewardItem *reward_item;
	int16 num_slots_needed;
	int16 num_slots;

	assert(collection);

	reward_items = collection->GetRewardItems();
	num_slots_needed = (int16)reward_items->size();
	if (selectable_item_id > 0)
		num_slots_needed++;

	num_slots = player->GetPlayerItemList()->GetNumberOfFreeSlots();
	if (num_slots < num_slots_needed) {
		SimpleMessage(CHANNEL_COLOR_RED, "You do not have enough free slots. Free up some slots and try again");
		HandInCollections();

			return;
	}

	/* add manditory items */
	for (itr = reward_items->begin(); itr != reward_items->end(); ++itr) {
		reward_item = *itr;
		AddItem(reward_item->item->details.item_id, reward_item->quantity);
	}

	/* find and add the selectable item if there's one */
	if (selectable_item_id > 0) {
		reward_items = collection->GetSelectableRewardItems();
		for (itr = reward_items->begin(); itr != reward_items->end(); ++itr) {
			reward_item = *itr;
			if (reward_item->item->details.item_id == selectable_item_id) {
				AddItem(reward_item->item->details.item_id, reward_item->quantity);
				break;
			}
		}
	}

	if (collection->GetRewardXP() > 0) {
		player->AddXP((int32)collection->GetRewardXP());
		SimpleMessage(CHANNEL_COLOR_YELLOW, "You gain experience!");
	}
	if (collection->GetRewardCoin() > 0) {
		player->AddCoins(collection->GetRewardCoin());
		Message(CHANNEL_COLOR_YELLOW, "You receive %s", GetCoinMessage(collection->GetRewardCoin()).c_str());
	}

	collection->SetCompleted(true);
	collection->SetSaveNeeded(true);
	SendCollectionList();

	/* reset the pending collection reward and check for my collections that the player needs to hand in */
	player->SetPendingCollectionReward(0);
	HandInCollections();

}

void Client::SendRecipeList() {
	PacketStruct* packet = 0;
	if (GetRecipeListSent()) {
		return;
	}

	if (!(packet = configReader.getStruct("WS_RecipeList", version))) {
		return;
	}
	map<int32, Recipe *> *recipes = player->GetRecipeList()->GetRecipes();
	map<int32, Recipe *>::iterator itr;
	int16 i = 0;

	packet->setArrayLengthByName("num_recipes", recipes->size());
	for (itr = recipes->begin(); itr != recipes->end(); ++itr){
		Recipe* recipe = itr->second;
		packet->setArrayDataByName("id", recipe->GetID(), i);
		packet->setArrayDataByName("tier", recipe->GetTier(), i);
		packet->setArrayDataByName("level", recipe->GetLevel(), i);
		packet->setArrayDataByName("icon", recipe->GetIcon(), i);
		packet->setArrayDataByName("classes", recipe->GetClasses(), i);
		packet->setArrayDataByName("skill", recipe->GetSkill(), i);
		packet->setArrayDataByName("technique", recipe->GetTechnique(), i);
		packet->setArrayDataByName("knowledge", recipe->GetKnowledge(), i);
		packet->setArrayDataByName("unknown2", recipe->GetUnknown2(), i);
		packet->setArrayDataByName("recipe_name", recipe->GetName(), i);
		packet->setArrayDataByName("recipe_book", recipe->GetBook(), i);
		packet->setArrayDataByName("unknown3", recipe->GetUnknown3(), i);
		i++;
	}
	//packet->PrintPacket();
	//DumpPacket(packet->serialize());
	QueuePacket(packet->serialize());
	safe_delete(packet);

	SetRecipeListSent(true);	
}

void Client::ShowRecipeBook() {
	PacketStruct* packet = 0;
	Spawn* target = 0;
	if (!(target = player->GetTarget())) {
		SimpleMessage(CHANNEL_COLOR_YELLOW, "You do not have a tradeskill device targeted");
		return;
	}
	if (!target->IsObject()) {
		return;
	}

	if (!(packet = configReader.getStruct("WS_ShowRecipeBook", version))) {
		return;
	}

	packet->setDataByName("device", target->GetName());
	packet->setDataByName("unknown1", 1);

	if (((Object*)target)->GetDeviceID() > 0) {
		int32 deviceID = (int32)pow(2.0, (double)((Object*)target)->GetDeviceID());
		//LogWrite(TRADESKILL__ERROR, 0, "Tradeskills", "GetDeviceID() = %u, deviceID = %u", ((Object*)target)->GetDeviceID(), deviceID);
		packet->setDataByName("unknown2", 8);
		packet->setDataByName("unknown3", deviceID);
	}	
	
	//packet->PrintPacket();
	QueuePacket(packet->serialize());
	safe_delete(packet);
}

void Client::SendTitleUpdate(){
	list<Title*> *titles = player->GetPlayerTitles()->GetAllTitles();
	list<Title*>::iterator itr;
	sint16 prefix_index = database.GetCharPrefixIndex(GetCharacterID(), player);
	sint16 suffix_index = database.GetCharSuffixIndex(GetCharacterID(), player);
	PacketStruct* packet = configReader.getStruct("WS_TitleUpdate", GetVersion());
	if(packet){
		int16 i = 0;
		packet->setArrayLengthByName("num_titles", titles->size());
		for(itr = titles->begin(); itr != titles->end(); ++itr){
			Title* title = *itr;
			packet->setArrayDataByName("title", title->GetName(), i);
			packet->setArrayDataByName("prefix", title->GetPrefix(), i);
			i++;
		}
		packet->setDataByName("current_prefix", prefix_index);
		packet->setDataByName("current_suffix", suffix_index);

		LogWrite(CCLIENT__PACKET, 0, "Client", "Dump/Print Packet in func: %s, line: %i", __FUNCTION__, __LINE__);
#if EQDEBUG >= 9
		packet->PrintPacket();
#endif

		QueuePacket(packet->serialize());
		safe_delete(packet);
		SendUpdateTitles(prefix_index, suffix_index);
	}
}

void Client::SendUpdateTitles(sint16 prefix, sint16 suffix){
	Title* suffix_title = 0;
	Title* prefix_title = 0;
	if (suffix != -1){
		suffix_title = player->GetPlayerTitles()->GetTitle(suffix);
		strcpy(player->appearance.suffix_title, suffix_title->GetName());
	}
	else
		memset(player->appearance.suffix_title, 0, strlen(player->appearance.suffix_title));
	if (prefix != -1){
		prefix_title = player->GetPlayerTitles()->GetTitle(prefix);
		strcpy(player->appearance.prefix_title, prefix_title->GetName());
	}
	else
		memset(player->appearance.prefix_title, 0, strlen(player->appearance.prefix_title));

	current_zone->SendUpdateTitles(this, suffix_title, prefix_title);
}

void Client::SendLanguagesUpdate(int32 id){
	list<Language*> *languages = player->GetPlayerLanguages()->GetAllLanguages();
	list<Language*>::iterator itr;
	PacketStruct* packet = configReader.getStruct("WS_Languages", GetVersion());
	if(packet){
		int32 i = 0;
		packet->setArrayLengthByName("num_languages", languages->size());
		for(itr = languages->begin(); itr != languages->end(); ++itr){
			Language* language = *itr;
			packet->setArrayDataByName("language_id", language->GetID(), i);
			i++;
		}
		packet->setDataByName("current_language", id);

		LogWrite(CCLIENT__PACKET, 0, "Client", "Dump/Print Packet in func: %s, line: %i", __FUNCTION__, __LINE__);
#if EQDEBUG >= 9
		packet->PrintPacket();
#endif
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}
}

void Client::SendPetOptionsWindow(const char* pet_name, int8 type) {
	PacketStruct* packet = configReader.getStruct("WS_PetOptions", GetVersion());
	if (packet) {
		if (pet_name)
			packet->setDataByName("pet_name", pet_name);
		if (player->GetInfoStruct()->pet_behavior & 1)
			packet->setDataByName("protect_master", 1);
		if (player->GetInfoStruct()->pet_behavior & 2)
			packet->setDataByName("protect_self", 1);
		if (player->GetInfoStruct()->pet_movement == 2)
			packet->setDataByName("stay_follow_toggle", 1);

		packet->setDataByName("pet_type", type);

		QueuePacket(packet->serialize());
	}

	safe_delete(packet);
}

bool Client::IsCrafting() {
	return current_zone->GetTradeskillMgr()->IsClientCrafting(this);
}

void Client::SendBiography() {
	PacketStruct* packet = configReader.getStruct("WS_BioUpdate", GetVersion());
	if (packet) {
		if	(strlen(player->GetInfoStruct()->biography) > 0)
			packet->setDataByName("biography",player->GetInfoStruct()->biography);

		QueuePacket(packet->serialize());
	}

	safe_delete(packet);
}

PendingResurrection* Client::GetCurrentRez() {
	return &current_rez;
}

Mutex* Client::GetResurrectMutex() {
	return &m_resurrect;
}

void Client::SendResurrectionWindow() {
	Spawn* caster = current_rez.caster;
	if(!caster || !player)
		return;

	PacketStruct* packet = configReader.getStruct("WS_ChoiceWindow", GetVersion());
	if(!packet)
		return;

	char* tmp = new char[80];
	sprintf(tmp, "%s would like to cast '%s' on you. Do you accept?", caster->GetName(), current_rez.spell_name.c_str());

	packet->setMediumStringByName("text", tmp);
	packet->setMediumStringByName("accept_text", "Yes");
	packet->setMediumStringByName("cancel_text", "No");
	
	sprintf(tmp, "accept_resurrection %u", player->GetID());
	packet->setMediumStringByName("accept_command", tmp);

	sprintf(tmp, "decline_resurrection %u", player->GetID());
	packet->setMediumStringByName("cancel_command", tmp);
	packet->setDataByName("time", current_rez.expire_timer->GetRemainingTime() / 1000);
	QueuePacket(packet->serialize());
	safe_delete(packet);
	safe_delete_array(tmp);
}

void Client::AcceptResurrection() {
	Spawn* caster = current_rez.caster;
	if(!player || !caster)
		return;

	if(player->Alive())
		return;

	if((caster->GetZone() != player->GetZone()) || (current_rez.range > 0 && player->GetDistance(caster) > current_rez.range)){
		SimpleMessage(CHANNEL_COLOR_YELLOW, "The caster must be nearby to complete the spell.");
		SendResurrectionWindow();
		return;
	}

	player->GetZone()->ResurrectSpawn(player, this);
	current_rez.should_delete = true;
}

void Client::SetPendingLastName(string last_name) {
	pending_last_name = new string;
	pending_last_name->assign(last_name);
}

string* Client::GetPendingLastName() {
	return pending_last_name;
}

void Client::RemovePendingLastName() {
	safe_delete(pending_last_name);
}

void Client::SendLastNameConfirmation() {
	if (!pending_last_name)
		return;

	PacketStruct* packet = configReader.getStruct("WS_ChoiceWindow", GetVersion());
	if (packet){
		char* text = new char[128];
		sprintf(text, "Are you sure you want your last name to be \"%s\"?" , pending_last_name->c_str());
		packet->setDataByName("text", text);
		packet->setDataByName("accept_text", "Yes");
		packet->setDataByName("accept_command", "confirmedlastname");
		packet->setDataByName("cancel_text", "No");
		packet->setDataByName("unknown2", 50);
		packet->setDataByName("unknown4", 1);
		packet->setDataByName("unknown5", 1);
		QueuePacket(packet->serialize());
		safe_delete(packet);
		safe_delete_array(text);
	}
}

void Client::AddQuestTimer(int32 quest_id) {
	MQuestTimers.writelock(__FUNCTION__, __LINE__);
	quest_timers.push_back(quest_id);
	MQuestTimers.releasewritelock(__FUNCTION__, __LINE__);
}

void Client::RemoveQuestTimer(int32 quest_id) {
	MQuestTimers.writelock(__FUNCTION__, __LINE__);
	vector<int32>::iterator itr;
	for (itr = quest_timers.begin(); itr != quest_timers.end(); ++itr) {
		if ((*itr) == quest_id) {
			quest_timers.erase(itr);
			break;
		}
	}
	MQuestTimers.releasewritelock(__FUNCTION__, __LINE__);
}

void Client::SavePlayerImages() {
	LogWrite(CCLIENT__DEBUG, 0, "Client", "Saving %s image for player %s (%u)", (incoming_paperdoll.image_type == PAPERDOLL_TYPE_FULL ? "paperdoll" : "headshot"), GetPlayer()->GetName(), GetCharacterID());

	// Save the paperdoll image if the server allows it
	if (incoming_paperdoll.image_type == PAPERDOLL_TYPE_FULL && rule_manager.GetGlobalRule(R_World, SavePaperdollImage)->GetBool())
		database.SaveCharacterPicture(GetCharacterID(), incoming_paperdoll.image_type, incoming_paperdoll.image_bytes, incoming_paperdoll.current_size_bytes);

	if (incoming_paperdoll.image_type == PAPERDOLL_TYPE_HEAD) {
		// Save the head shot if the server allows it
		if (rule_manager.GetGlobalRule(R_World, SaveHeadshotImage)->GetBool())
			database.SaveCharacterPicture(GetCharacterID(), incoming_paperdoll.image_type, incoming_paperdoll.image_bytes, incoming_paperdoll.current_size_bytes);
	
		// Send the head shot to the login server
		if (rule_manager.GetGlobalRule(R_World, SendPaperdollImagesToLogin)->GetBool()) {
			int32 size = incoming_paperdoll.current_size_bytes + CHARPICSTRUCT_MINSIZE;
			ServerPacket* packet = new ServerPacket(ServerOP_CharacterPicture, size);
			memset(packet->pBuffer, 0, size);

			CharPictureUpdate_Struct* pic = (CharPictureUpdate_Struct*)packet->pBuffer;
			pic->account_id = GetAccountID();
			pic->char_id = GetCharacterID();
			pic->pic_size = (int16)incoming_paperdoll.current_size_bytes;
			memcpy(pic->pic, incoming_paperdoll.image_bytes, incoming_paperdoll.current_size_bytes);

			loginserver.SendPacket(packet);
			safe_delete(packet);
		}
	}
	safe_delete_array(incoming_paperdoll.image_bytes);
	incoming_paperdoll.current_size_bytes = 0;
}

void Client::EndAutoMount() {
	PacketStruct* packet = configReader.getStruct("WS_ServerControlFlags", GetVersion());
	if (packet) {
		packet->setDataByName("parameter1", 128);
		packet->setDataByName("parameter2", 64);
		packet->setDataByName("value", 1);
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

	packet = configReader.getStruct("WS_ServerControlFlags", GetVersion());
	if (packet) {
		packet->setDataByName("parameter3", 4);
		packet->setDataByName("parameter5", 2);
		packet->setDataByName("value", 0);
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

	packet = configReader.getStruct("WS_ClearForLanding", GetVersion());
	if (packet) {
		packet->setDataByName("spawn_id", GetPlayer()->GetIDWithPlayerSpawn(GetPlayer()));
		QueuePacket(packet->serialize());
		safe_delete(packet);
	}

	on_auto_mount = false;

	player->SetMount(((Player*)player)->GetTempMount());
	EQ2_Color mount_color = player->GetTempMountColor();
	EQ2_Color saddle_color = player->GetTempMountSaddleColor();
	player->SetMountColor(&mount_color);
	player->SetMountSaddleColor(&saddle_color);
	player->SetTempMount(0);
}

bool Client::EntityCommandPrecheck(Spawn* spawn, const char* command){
	const char* spawn_script = spawn->GetSpawnScript();
	bool should_use_spawn = true;
	if (spawn_script){
		lua_State* state = lua_interface->GetSpawnScript(spawn_script);
		if (state){
			Mutex* state_mutex = lua_interface->GetSpawnScriptMutex(spawn_script);
			if (state_mutex)
				state_mutex->writelock(__FUNCTION__, __LINE__);
			lua_getglobal(state, "can_use_command");
			if (lua_isfunction(state, -1)){
				lua_interface->SetSpawnValue(state, spawn);
				lua_interface->SetSpawnValue(state, GetPlayer());
				lua_interface->SetStringValue(state, command ? command : "");
				if (lua_pcall(state, 3, 1, 0) == 0){
					should_use_spawn = lua_interface->GetBooleanValue(state, 1);
				}
			}
			lua_interface->ResetFunctionStack(state);
			if (state_mutex)
				state_mutex->releasewritelock(__FUNCTION__, __LINE__);
		}
	}
	return should_use_spawn;
}