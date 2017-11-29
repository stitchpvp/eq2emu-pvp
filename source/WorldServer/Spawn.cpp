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
#include "Spawn.h"
#include <stdio.h>
#include "../common/timer.h"
#include <time.h>
#include <math.h>
#include "Entity.h"
#include "Widget.h"
#include "Sign.h"
#include "../common/MiscFunctions.h"
#include "../common/Log.h"
#include "Rules/Rules.h"
#include "World.h"
#include "LuaInterface.h"
#include "PVP.h"
#include "Zone/SPGrid.h"

extern ConfigReader configReader;
extern RuleManager rule_manager;
extern World world;

Spawn::Spawn(){ 
	group_id = 0;
	size_offset = 0;
	merchant_id = 0;
	memset(&appearance, 0, sizeof(appearance)); 
	memset(&basic_info, 0, sizeof(BasicInfoStruct)); 
	appearance.encounter_level =6;
	size = 32;
	appearance.pos.collision_radius = 32;
	id = Spawn::NextID();
	oversized_packet = 0xFF;
	zone = 0;
	spawn_location_id = 0;
	spawn_entry_id = 0;
	spawn_location_spawns_id = 0;
	respawn = 0;
	expire_time = 0;
	expire_offset = 0;
	x_offset = 0;
	y_offset = 0;
	z_offset = 0;
	database_id = 0;
	packet_num = 1;
	changed = false;
	vis_changed = false;
	position_changed = false;
	send_spawn_changes = true;
	info_changed = false;
	appearance.pos.Speed1 = 0;
	last_attacker = 0;	
	faction_id = 0;
	running_to = 0;
	tmp_visual_state = -1;
	tmp_action_state = -1;
	transporter_id = 0;
	invulnerable = false;
	spawn_group_list = 0;
	MSpawnGroup = 0;
	movement_locations = 0;
	MMovementLocations = 0;
	target = 0;
	primary_command_list_id = 0;
	secondary_command_list_id = 0;
	is_pet = false;
	m_followTarget = 0;
	following = false;
	req_quests_continued_access = false;
	req_quests_override = 0;
	req_quests_private = false;
	m_illusionModel = 0;
	Cell_Info.CurrentCell = nullptr;
	Cell_Info.CellListIndex = -1;
	m_Update.SetName("Spawn::m_Update");
	m_requiredHistory.SetName("Spawn::m_requiredHistory");
	m_requiredQuests.SetName("Spawn::m_requiredQuests");
}

Spawn::~Spawn(){
	for(int32 i=0;i<primary_command_list.size();i++){
		safe_delete(primary_command_list[i]);
	}
	for(int32 i=0;i<secondary_command_list.size();i++){
		safe_delete(secondary_command_list[i]);
	}
	RemoveSpawnFromGroup();
	if (MMovementLocations)
		MMovementLocations->writelock(__FUNCTION__, __LINE__);
	if(movement_locations){
		while(movement_locations->size()){
			safe_delete(movement_locations->front());
			movement_locations->pop_front();
		}
		safe_delete(movement_locations);
	}
	if (MMovementLocations)
		MMovementLocations->releasewritelock(__FUNCTION__, __LINE__);
	safe_delete(MMovementLocations);

	MMovementLoop.lock();
	for (int32 i = 0; i < movement_loop.size(); i++)
		safe_delete(movement_loop.at(i));
	MMovementLoop.unlock();

	map<int32, vector<int16>* >::iterator rq_itr;
	m_requiredQuests.writelock(__FUNCTION__, __LINE__);
	for (rq_itr = required_quests.begin(); rq_itr != required_quests.end(); rq_itr++){
		safe_delete(rq_itr->second);
	}
	m_requiredQuests.releasewritelock(__FUNCTION__, __LINE__);
}

void Spawn::InitializeHeaderPacketData(Player* player, PacketStruct* header, int16 index) {
	header->setDataByName("index", index);

	if (primary_command_list.size() > 0) {
		if (primary_command_list.size() > 1) {
			header->setArrayLengthByName("command_list", primary_command_list.size());
			for (int32 i = 0; i < primary_command_list.size(); i++) {
				header->setArrayDataByName("command_list_name", primary_command_list[i]->name.c_str(), i);
				header->setArrayDataByName("command_list_max_distance", primary_command_list[i]->distance, i);
				header->setArrayDataByName("command_list_error", primary_command_list[i]->error_text.c_str(), i);
				header->setArrayDataByName("command_list_command", primary_command_list[i]->command.c_str(), i);
			}
		}
		header->setMediumStringByName("default_command", primary_command_list[0]->command.c_str());
		header->setDataByName("max_distance", primary_command_list[0]->distance);
	}

	if (IsPlayer() && player->CanAttackTarget(this)) {
		header->setMediumStringByName("default_command", "attack");
		header->setDataByName("max_distance", 10000.0);
	}

	if (spawn_group_list && MSpawnGroup){
		MSpawnGroup->readlock(__FUNCTION__, __LINE__);
		header->setArrayLengthByName("group_size", spawn_group_list->size());
		vector<Spawn*>::iterator itr;
		int i = 0;
		for (itr = spawn_group_list->begin(); itr != spawn_group_list->end(); itr++, i++){
			header->setArrayDataByName("group_spawn_id", player->GetIDWithPlayerSpawn((*itr)), i);
		}
		MSpawnGroup->releasereadlock(__FUNCTION__, __LINE__);
	}

	if (header->GetVersion() >= 57080)
		header->setDataByName("unknown", 0xFFFFFFFF);
	else
		header->setDataByName("unknown", 0xFFFF);
	header->setDataByName("spawn_id", player->GetIDWithPlayerSpawn(this));
	header->setDataByName("crc", id);
	header->setDataByName("time_stamp", Timer::GetCurrentTime2());
}

void Spawn::InitializeVisPacketData(Player* player, PacketStruct* vis_packet) {
	int16 version = vis_packet->GetVersion();

	if (IsPlayer())
		appearance.pos.grid_id = 0xFFFFFFFF;
	if (appearance.targetable == 1 || appearance.show_level == 1 || appearance.display_name == 1){
		if (!IsGroundSpawn()){
			int8 arrow_color = ARROW_COLOR_WHITE;
			sint8 npc_con = player->GetFactions()->GetCon(faction_id);
			if (appearance.attackable == 1)
				arrow_color = player->GetArrowColor(GetLevel());

			if (IsPlayer() && player->CanAttackTarget((Player*)this)) {
				arrow_color = player->GetArrowColor(GetLevel());
				npc_con = -4;
			}

			vis_packet->setDataByName("arrow_color", arrow_color);
			vis_packet->setDataByName("locked_no_loot", appearance.locked_no_loot);
			if (IsNPC() && player->GetArrowColor(GetLevel()) == ARROW_COLOR_GRAY || player->IsStealthed() || player->IsInvis())
				if (npc_con == -4)
					npc_con = -3;
			vis_packet->setDataByName("npc_con", npc_con);
			if (appearance.attackable == 1 && IsNPC() && (player->GetFactions()->GetCon(faction_id) <= -4 || ((NPC*)this)->Brain()->GetHate(player) > 1))
				vis_packet->setDataByName("npc_hate", ((NPC*)this)->Brain()->GetHatePercentage(player));
			int8 quest_flag = player->CheckQuestFlag(this);
			if (version < 1188 && quest_flag >= 16)
				quest_flag = 1;
			vis_packet->setDataByName("quest_flag", quest_flag);
		}
	}

	int8 vis_flags = 0;
	if (MeetsSpawnAccessRequirements(player)){
		if (appearance.attackable == 1 || (IsPlayer() && player->CanAttackTarget((Player*)this)))
			vis_flags += 64; //attackable icon
		if (appearance.show_level == 1)
			vis_flags += 32;
		if (appearance.display_name == 1)
			vis_flags += 16;
		if (IsPlayer() || appearance.targetable == 1)
			vis_flags += 4;
		if (appearance.show_command_icon == 1)
			vis_flags += 2;
	} else if (req_quests_override > 0) {
			vis_flags = req_quests_override & 0xFF;
	}

	vis_packet->setDataByName("vis_flags", vis_flags);

	if (MeetsSpawnAccessRequirements(player)) {
		vis_packet->setDataByName("hand_flag", appearance.display_hand_icon);
	} else if ((req_quests_override & 256) > 0) {
			vis_packet->setDataByName("hand_flag", 1);
	}
}

void Spawn::InitializeFooterPacketData(Player* player, PacketStruct* footer) {
	if (IsWidget()){
		Widget* widget = (Widget*)this;
		if (widget->GetMultiFloorLift()) {
			footer->setDataByName("widget_x", widget->GetX());
			footer->setDataByName("widget_y", widget->GetY());
			footer->setDataByName("widget_z", widget->GetZ());
		}
		else {
			footer->setDataByName("widget_x", widget->GetWidgetX());
			footer->setDataByName("widget_y", widget->GetWidgetY());
			footer->setDataByName("widget_z", widget->GetWidgetZ());
		}
		footer->setDataByName("widget_id", widget->GetWidgetID());
		footer->setDataByName("unknown3c", 6);
	}
	else if (IsSign()){
		Sign* sign = (Sign*)this;
		footer->setDataByName("widget_id", sign->GetWidgetID());
		footer->setDataByName("widget_x", sign->GetWidgetX());
		footer->setDataByName("widget_y", sign->GetWidgetY());
		footer->setDataByName("widget_z", sign->GetWidgetZ());
		footer->setDataByName("unknown2b", 6);
		if (sign->GetSignTitle())
			footer->setMediumStringByName("title", sign->GetSignTitle());
		if (sign->GetSignDescription())
			footer->setMediumStringByName("description", sign->GetSignDescription());
		footer->setDataByName("sign_distance", sign->GetSignDistance());
		footer->setDataByName("show", 1);
	}

	footer->setMediumStringByName("name", appearance.name);
	footer->setMediumStringByName("guild", appearance.sub_title);
	if (IsPlayer()) {
		string prefix_title = GetPrefixTitle();
		string pvp_title = PVP::GetRank(static_cast<Player*>(this));
		if (pvp_title.length() > 0) {
			if (prefix_title.length() > 0)
				prefix_title = pvp_title + " " + prefix_title;
			else
				prefix_title = pvp_title;
		}
		footer->setMediumStringByName("prefix", prefix_title.c_str());
	} else {
		footer->setMediumStringByName("prefix", appearance.prefix_title);
	}
	footer->setMediumStringByName("suffix", appearance.suffix_title);
	footer->setMediumStringByName("last_name", appearance.last_name);

	if (appearance.attackable == 0 && GetLevel() > 0) {
		footer->setDataByName("spawn_type", 1);
	} else if (appearance.attackable == 0) {
		footer->setDataByName("spawn_type", 6);
	} else {
		footer->setDataByName("spawn_type", 3);
	}
}

EQ2Packet* Spawn::spawn_serialize(Player* player, int16 version){
	// If spawn is NPC AND is pet && owner is a player && owner is the player passed to this function && player's char sheet pet id is 0
	if (IsNPC() && ((NPC*)this)->IsPet() && ((NPC*)this)->GetOwner()->IsPlayer() && player == ((NPC*)this)->GetOwner() && player->GetInfoStruct()->pet_id == 0) {
		((Player*)((NPC*)this)->GetOwner())->GetInfoStruct()->pet_id = player->spawn_id;
		player->SetCharSheetChanged(true);
	}

	int16 index;
	if (player->player_spawn_index_map.count(this) > 0) {
		index = player->player_spawn_index_map[this];
		player->player_spawn_map[index] = this;
	}
	else {
		player->spawn_index++;
		if(player->spawn_index == 255)
			player->spawn_index++; //just so we dont have to worry about overloading
		index = player->spawn_index;
		player->player_spawn_index_map[this] = index;
		player->player_spawn_map[index] = this;
	}

	// Jabantiz - [Bug] Client Crash on Revive
	if (player->player_spawn_reverse_id_map.count(this) == 0) {
		int32 spawn_id = ++player->spawn_id;
		player->player_spawn_id_map[spawn_id] = this;
		player->player_spawn_reverse_id_map[this] = spawn_id;
	}

	PacketStruct* header = player->GetSpawnHeaderStruct();
	header->ResetData();
	InitializeHeaderPacketData(player, header, index);

	PacketStruct* footer;
	if(IsWidget())
		footer = player->GetWidgetFooterStruct();
	else if(IsSign())
		footer = player->GetSignFooterStruct();
	else
		footer = player->GetSpawnFooterStruct();

	footer->ResetData();
	InitializeFooterPacketData(player, footer);

	PacketStruct* vis_struct = player->GetSpawnVisStruct();
	PacketStruct* info_struct = player->GetSpawnInfoStruct();
	PacketStruct* pos_struct = player->GetSpawnPosStruct();

	player->vis_mutex.writelock(__FUNCTION__, __LINE__);
	vis_struct->ResetData();
	InitializeVisPacketData(player, vis_struct);

	player->info_mutex.writelock(__FUNCTION__, __LINE__);
	info_struct->ResetData();
	InitializeInfoPacketData(player, info_struct);

	player->pos_mutex.writelock(__FUNCTION__, __LINE__);
	pos_struct->ResetData();
	InitializePosPacketData(player, pos_struct);

	string* vis_data= vis_struct->serializeString();
	string* pos_data = pos_struct->serializeString();
	string* info_data = info_struct->serializeString();

	int16 part2_size = pos_data->length() + vis_data->length() + info_data->length();
	uchar* part2 = new uchar[part2_size];

	player->AddSpawnPosPacketForXOR(id, (uchar*)pos_data->c_str(), pos_data->length());
	player->AddSpawnVisPacketForXOR(id, (uchar*)vis_data->c_str(), vis_data->length());
	player->AddSpawnInfoPacketForXOR(id, (uchar*)info_data->c_str(), info_data->length());
	
	uchar* ptr = part2;
	memcpy(ptr, pos_data->c_str(), pos_data->length());
	ptr += pos_data->length();
	memcpy(ptr, vis_data->c_str(), vis_data->length());
	ptr += vis_data->length();
	memcpy(ptr, info_data->c_str(), info_data->length());
	player->pos_mutex.releasewritelock(__FUNCTION__, __LINE__);
	player->info_mutex.releasewritelock(__FUNCTION__, __LINE__);
	player->vis_mutex.releasewritelock(__FUNCTION__, __LINE__);

	string* part1 = header->serializeString();
	string* part3 = footer->serializeString();

	uchar tmp[900];
	part2_size = Pack(tmp, part2, part2_size, 900, version);
	int32 total_size = part1->length() + part2_size + part3->length() + 3;


	uchar* final_packet = new uchar[total_size + 4];
	ptr = final_packet;
	memcpy(ptr, &total_size, sizeof(total_size));
	ptr += sizeof(total_size);

	memcpy(ptr, &oversized_packet, sizeof(oversized_packet));
	ptr += sizeof(oversized_packet);

	memcpy(ptr, &opcode, sizeof(opcode));
	ptr += sizeof(opcode);
	
	memcpy(ptr, part1->c_str(), part1->length());
	ptr += part1->length();

	memcpy(ptr, tmp, part2_size);
	ptr += part2_size;
	
	memcpy(ptr, part3->c_str(), part3->length());
	delete[] part2;

	EQ2Packet* ret = new EQ2Packet(OP_ClientCmdMsg, final_packet, total_size + 4);
	delete[] final_packet;

  	return ret;
}

uchar* Spawn::spawn_info_changes(Player* player, int16 version){
	int16 index = player->player_spawn_index_map[this];

	PacketStruct* packet = player->GetSpawnInfoStruct();

	player->info_mutex.writelock(__FUNCTION__, __LINE__);
	packet->ResetData();
	InitializeInfoPacketData(player, packet);
	string* data = packet->serializeString();
	int32 size = data->length();
	uchar* xor_info_packet = player->GetTempInfoPacketForXOR();
	if (!xor_info_packet)
		xor_info_packet = player->SetTempInfoPacketForXOR(size);
	uchar* orig_packet = player->GetSpawnInfoPacketForXOR(id);
	if(orig_packet){
		memcpy(xor_info_packet, (uchar*)data->c_str(), size);
 		Encode(xor_info_packet, orig_packet, size);
	}
	uchar* tmp = new uchar[size + 10];
	size = Pack(tmp, xor_info_packet, size, size, version);
	player->info_mutex.releasewritelock(__FUNCTION__, __LINE__);

	int32 orig_size = size;
	size-=sizeof(int32);
	size+=CheckOverLoadSize(index);
	info_packet_size = size + CheckOverLoadSize(size);

	uchar* tmp2 = new uchar[info_packet_size];
	uchar* ptr = tmp2;
	ptr += DoOverLoad(size, ptr);
	ptr += DoOverLoad(index, ptr);
	memcpy(ptr, tmp+sizeof(int32), orig_size - sizeof(int32));
	delete[] tmp;
	return tmp2;
}

uchar* Spawn::spawn_vis_changes(Player* player, int16 version){
	PacketStruct* vis_struct = player->GetSpawnVisStruct();
	int16 index = player->player_spawn_index_map[this];

	player->vis_mutex.writelock(__FUNCTION__, __LINE__);
	uchar* orig_packet = player->GetSpawnVisPacketForXOR(id);
	vis_struct->ResetData();
	InitializeVisPacketData(player, vis_struct);
	string* data = vis_struct->serializeString();
	int32 size = data->length();
	uchar* xor_vis_packet = player->GetTempVisPacketForXOR();
	if (!xor_vis_packet)
		xor_vis_packet = player->SetTempVisPacketForXOR(size);
	if(orig_packet){
		memcpy(xor_vis_packet, (uchar*)data->c_str(), size);
		Encode(xor_vis_packet, orig_packet, size);
	}
	uchar* tmp = new uchar[size + 10];
	size = Pack(tmp, xor_vis_packet, size, size, version);
	player->vis_mutex.releasewritelock(__FUNCTION__, __LINE__);

	int32 orig_size = size;
	size-=sizeof(int32);
	size+=CheckOverLoadSize(index);
	vis_packet_size = size + CheckOverLoadSize(size);
	uchar* tmp2 = new uchar[vis_packet_size];
	uchar* ptr = tmp2;
	ptr += DoOverLoad(size, ptr);
	ptr += DoOverLoad(index, ptr);
	memcpy(ptr, tmp+sizeof(int32), orig_size - sizeof(int32));
	delete[] tmp;
	return tmp2;
}

uchar* Spawn::spawn_pos_changes(Player* player, int16 version){
	int16 index = player->GetIndexForSpawn(this);
	
	PacketStruct* packet = player->GetSpawnPosStruct();

	player->pos_mutex.writelock(__FUNCTION__, __LINE__);
	uchar* orig_packet = player->GetSpawnPosPacketForXOR(id);
	packet->ResetData();
	InitializePosPacketData(player, packet);
	string* data = packet->serializeString();
	int32 size = data->length();
	uchar* xor_pos_packet = player->GetTempPosPacketForXOR();
	if (!xor_pos_packet)
		xor_pos_packet = player->SetTempPosPacketForXOR(size);
	if(orig_packet){
		memcpy(xor_pos_packet, (uchar*)data->c_str(), size);
		Encode(xor_pos_packet, orig_packet, size);
	}
	uchar* tmp;
	if (IsPlayer())
		tmp = new uchar[size + 14];
	else
		tmp = new uchar[size + 10];
	size = Pack(tmp, xor_pos_packet, size, size, version);
	player->pos_mutex.releasewritelock(__FUNCTION__, __LINE__);


	int32 orig_size = size;
	// Needed for CoE+ clients
	if (version >= 1188)
		size += 1;

	if(IsPlayer())
		size += 4;
	size-=sizeof(int32);
	size+=CheckOverLoadSize(index);

	pos_packet_size = size + CheckOverLoadSize(size);
	uchar* tmp2 = new uchar[pos_packet_size];
	uchar* ptr = tmp2;
	ptr += DoOverLoad(size, ptr);
	ptr += DoOverLoad(index, ptr);

	// extra byte in coe+ clients, 0 for NPC's 1 for Players
	int8 x = 0;
	if(IsPlayer()){
		if (version >= 1188) {
			// set x to 1 and add it to the packet
			x = 1;
			memcpy(ptr, &x, sizeof(int8));
			ptr += sizeof(int8);
		}
		int32 now = Timer::GetCurrentTime2();
		memcpy(ptr, &now, sizeof(int32));
		ptr += sizeof(int32);
	}
	else if (version >= 1188) {
		// add x to packet
		memcpy(ptr, &x, sizeof(int8));
		ptr += sizeof(int8);
	}
	memcpy(ptr, tmp+sizeof(int32), orig_size - sizeof(int32));
	delete[] tmp;
	return tmp2;
}

EQ2Packet* Spawn::player_position_update_packet(Player* player, int16 version){
	if(!player || player->IsPlayer() == false){
		LogWrite(SPAWN__ERROR, 0, "Spawn", "Error: Called player_position_update_packet without player!");
		return 0;
	}
	else if(IsPlayer() == false){
		LogWrite(SPAWN__ERROR, 0, "Spawn", "Error: Called player_position_update_packet from spawn!");
		return 0;
	}

	static const int8 info_size = 1;
	static const int8 vis_size = 1;
	m_Update.writelock(__FUNCTION__, __LINE__);
	uchar* pos_changes = spawn_pos_changes(player, version);
	int32 tmp_pos_packet_size = pos_packet_size;
	m_Update.releasewritelock(__FUNCTION__, __LINE__);

	int32 size = info_size + tmp_pos_packet_size + vis_size + 11;
	static const int8 oversized = 255;
	int16 opcode_val = EQOpcodeManager[GetOpcodeVersion(version)]->EmuToEQ(OP_EqUpdateGhostCmd);
	uchar* tmp = new uchar[size];
	memset(tmp, 0, size);
	uchar* ptr = tmp;
	size -=4;
	memcpy(ptr, &size, sizeof(int32));
	size +=4;
	ptr += sizeof(int32);
	memcpy(ptr, &oversized, sizeof(int8));
	ptr += sizeof(int8);
	memcpy(ptr, &opcode_val, sizeof(int16));
	ptr += sizeof(int16);
	ptr += sizeof(int32);
	ptr += info_size;
	memcpy(ptr, pos_changes, tmp_pos_packet_size);
	EQ2Packet* ret_packet = new EQ2Packet(OP_ClientCmdMsg, tmp, size);
	delete[] tmp;
	delete[] pos_changes;
	return ret_packet;
}

EQ2Packet* Spawn::spawn_update_packet(Player* player, int16 version, bool override_changes, bool override_vis_changes){
	if(!player || player->IsPlayer() == false){
		LogWrite(SPAWN__ERROR, 0, "Spawn", "Error: Called spawn_update_packet without player!");
		return 0;
	}
	else if((IsPlayer() && info_changed == false && vis_changed == false) || (info_changed == false && vis_changed == false && position_changed == false)){
		if(!override_changes && !override_vis_changes)
			return 0;
	}

	static const uchar null_byte = 0;

	uchar* info_changes = 0;
	uchar* pos_changes = 0;
	uchar* vis_changes = 0;
	static const int8 oversized = 255;
	int16 opcode_val = EQOpcodeManager[GetOpcodeVersion(version)]->EmuToEQ(OP_EqUpdateGhostCmd);

	int16 tmp_info_packet_size;
	int16 tmp_vis_packet_size;
	int16 tmp_pos_packet_size;
	//We need to lock these variables up to make this thread safe
	m_Update.writelock(__FUNCTION__, __LINE__);
	//These variables are set in the spawn_info_changes, pos and vis changes functions
	info_packet_size = 1;
	pos_packet_size = 1;
	vis_packet_size = 1;

	if (info_changed || override_changes)
		info_changes = spawn_info_changes(player, version);
	if ((position_changed || override_changes) && IsPlayer() == false)
		pos_changes = spawn_pos_changes(player, version);
	if (vis_changed || override_changes || override_vis_changes)
		vis_changes = spawn_vis_changes(player, version);

	tmp_info_packet_size = info_packet_size;
	tmp_pos_packet_size = pos_packet_size;
	tmp_vis_packet_size = vis_packet_size;
	m_Update.releasewritelock(__FUNCTION__, __LINE__);

	int32 size = info_packet_size + pos_packet_size + vis_packet_size + 11;
	uchar* tmp = new uchar[size];
	memset(tmp, 0, size);
	uchar* ptr = tmp;
	size -=4;
	memcpy(ptr, &size, sizeof(int32));
	size +=4;
	ptr += sizeof(int32);
	memcpy(ptr, &oversized, sizeof(int8));
	ptr += sizeof(int8);
	memcpy(ptr, &opcode_val, sizeof(int16));
	ptr += sizeof(int16);
	if (IsPlayer() == false){ //this isnt sent for player updates, it is sent on position update
		//int32 time = Timer::GetCurrentTime2();
		packet_num = Timer::GetCurrentTime2();
		memcpy(ptr, &packet_num, sizeof(int32));
	}
	ptr += sizeof(int32);

	memcpy(ptr, info_changes ? info_changes : &null_byte, tmp_info_packet_size);
	ptr += info_packet_size;
	memcpy(ptr, pos_changes ? pos_changes : &null_byte, tmp_pos_packet_size);
	ptr += pos_packet_size;
	memcpy(ptr, vis_changes ? vis_changes : &null_byte, tmp_vis_packet_size);

	EQ2Packet* ret_packet = new EQ2Packet(OP_ClientCmdMsg, tmp, size);
	delete[] tmp;
	safe_delete_array(info_changes);
	safe_delete_array(vis_changes);
	safe_delete_array(pos_changes);
	return ret_packet;
}

EQ2Packet* Spawn::serialize(Player* player, int16 version){
	return 0;
}

Spawn* Spawn::GetTarget(){
	Spawn* ret = 0;
	
	// only attempt to get a spawn if we had a target stored
	if (target != 0)
	{
		ret = GetZone()->GetSpawnByID(target);
	
		if (!ret)
			target = 0;
	}

	return ret;
}

void Spawn::SetTarget(Spawn* spawn){
	SetInfo(&target, spawn ? spawn->GetID() : 0);
}

Spawn* Spawn::GetLastAttacker() {
	Spawn* ret = 0;
	ret = GetZone()->GetSpawnByID(last_attacker);
	if (!ret)
		last_attacker = 0;
	return ret;
}

void Spawn::SetLastAttacker(Spawn* spawn){
	last_attacker = spawn->GetID();
}

void Spawn::SetInvulnerable(bool val){
	invulnerable = val;
}

bool Spawn::GetInvulnerable(){
	return invulnerable;
}

bool Spawn::TakeDamage(int32 damage){
	if(invulnerable)
		return false;
	if (IsEntity()) {
		if (((Entity*)this)->IsMezzed())
			((Entity*)this)->RemoveAllMezSpells();

		if (damage == 0)
			return true;
	}

	int32 hp = GetHP();
	if(damage >= hp) {
		SetHP(0);
	} else {
		SetHP(hp - damage);
	}
	return true;
}

void Spawn::TakeDamage(Spawn* attacker, int32 damage){
	if (TakeDamage(damage))
		GetZone()->CallSpawnScript(this, SPAWN_SCRIPT_HEALTHCHANGED, attacker);
	SetLastAttacker(attacker);
}

ZoneServer*	Spawn::GetZone(){
	return zone;
}

void Spawn::SetZone(ZoneServer* in_zone){
	zone = in_zone;
}


/*** HIT POINT ***/
void Spawn::SetHP(sint32 new_val, bool setUpdateFlags){
	if(new_val == 0){
		ClearRunningLocations();
		CalculateRunningLocation(true);
	}
	if(new_val > basic_info.max_hp)
		SetInfo(&basic_info.max_hp, new_val, setUpdateFlags);
	SetInfo(&basic_info.cur_hp, new_val, setUpdateFlags);
	if(/*IsPlayer() &&*/ GetZone() && basic_info.cur_hp > 0 && basic_info.cur_hp < basic_info.max_hp)
		GetZone()->AddDamagedSpawn(this);

	if (IsEntity() && ((Entity*)this)->GetGroupMemberInfo()) {
		((Entity*)this)->UpdateGroupMemberInfo();
		if (IsPlayer())
			world.GetGroupManager()->SendGroupUpdate(((Entity*)this)->GetGroupMemberInfo()->group_id, GetZone()->GetClientBySpawn(this));
		else
			world.GetGroupManager()->SendGroupUpdate(((Entity*)this)->GetGroupMemberInfo()->group_id);
	}

	if (IsNPC() && ((NPC*)this)->IsPet() && ((NPC*)this)->GetOwner()->IsPlayer()) {
		Player* player = (Player*)((NPC*)this)->GetOwner();
		if (player->GetPet() && player->GetCharmedPet()) {
			if (this == player->GetPet()) {
				player->GetInfoStruct()->pet_health_pct = (float)basic_info.cur_hp / (float)basic_info.max_hp;
				player->SetCharSheetChanged(true);
			}
		}
		else {
			player->GetInfoStruct()->pet_health_pct = (float)basic_info.cur_hp / (float)basic_info.max_hp;
			player->SetCharSheetChanged(true);
		}
	}

	if (IsPlayer())
		((Player*)this)->SetCharSheetChanged(true);
}
void Spawn::SetTotalHP(sint32 new_val){
	if(basic_info.hp_base == 0)
		SetTotalHPBase(new_val);
	SetInfo(&basic_info.max_hp, new_val);

	if(GetZone() && basic_info.cur_hp > 0 && basic_info.cur_hp < basic_info.max_hp)
		GetZone()->AddDamagedSpawn(this);

	if (IsEntity() && ((Entity*)this)->GetGroupMemberInfo()) {
		((Entity*)this)->UpdateGroupMemberInfo();
		if (IsPlayer())
			world.GetGroupManager()->SendGroupUpdate(((Entity*)this)->GetGroupMemberInfo()->group_id, GetZone()->GetClientBySpawn(this));
		else
			world.GetGroupManager()->SendGroupUpdate(((Entity*)this)->GetGroupMemberInfo()->group_id);
	}

	if (IsNPC() && ((NPC*)this)->IsPet() && ((NPC*)this)->GetOwner()->IsPlayer()) {
		Player* player = (Player*)((NPC*)this)->GetOwner();
		if (player->GetPet() && player->GetCharmedPet()) {
			if (this == player->GetPet()) {
				player->GetInfoStruct()->pet_health_pct = (float)basic_info.cur_hp / (float)basic_info.max_hp;
				player->SetCharSheetChanged(true);
			}
		}
		else {
			player->GetInfoStruct()->pet_health_pct = (float)basic_info.cur_hp / (float)basic_info.max_hp;
			player->SetCharSheetChanged(true);
		}
	}
}
void Spawn::SetTotalHPBase(sint32 new_val)
{
	SetInfo(&basic_info.hp_base, new_val);

	if(GetZone() && basic_info.cur_hp > 0 && basic_info.cur_hp < basic_info.max_hp)
		GetZone()->AddDamagedSpawn(this);

	if (IsEntity() && ((Entity*)this)->GetGroupMemberInfo()) {
		((Entity*)this)->UpdateGroupMemberInfo();
		if (IsPlayer())
			world.GetGroupManager()->SendGroupUpdate(((Entity*)this)->GetGroupMemberInfo()->group_id, GetZone()->GetClientBySpawn(this));
		else
			world.GetGroupManager()->SendGroupUpdate(((Entity*)this)->GetGroupMemberInfo()->group_id);
	}
}
sint32 Spawn::GetHP()
{
	return basic_info.cur_hp;
}
sint32 Spawn::GetTotalHP()
{
	return basic_info.max_hp;
}
sint32 Spawn::GetTotalHPBase()
{
	return basic_info.hp_base;
}


/*** POWER ***/
void Spawn::SetPower(sint32 power, bool setUpdateFlags){
	if(power > basic_info.max_power)
		SetInfo(&basic_info.max_power, power, setUpdateFlags);
	SetInfo(&basic_info.cur_power, power, setUpdateFlags);
	if(/*IsPlayer() &&*/ GetZone() && basic_info.cur_power < basic_info.max_power)
		GetZone()->AddDamagedSpawn(this);

	if (IsEntity() && ((Entity*)this)->GetGroupMemberInfo()) {
		((Entity*)this)->UpdateGroupMemberInfo();
		if (IsPlayer())
			world.GetGroupManager()->SendGroupUpdate(((Entity*)this)->GetGroupMemberInfo()->group_id, GetZone()->GetClientBySpawn(this));
		else
			world.GetGroupManager()->SendGroupUpdate(((Entity*)this)->GetGroupMemberInfo()->group_id);
	}

	if (IsNPC() && ((NPC*)this)->IsPet() && ((NPC*)this)->GetOwner()->IsPlayer()) {
		Player* player = (Player*)((NPC*)this)->GetOwner();
		if (player->GetPet() && player->GetCharmedPet()) {
			if (this == player->GetPet()) {
				player->GetInfoStruct()->pet_power_pct = (float)basic_info.cur_power / (float)basic_info.max_power;
				player->SetCharSheetChanged(true);
			}
		}
		else {
			player->GetInfoStruct()->pet_power_pct = (float)basic_info.cur_power / (float)basic_info.max_power;
			player->SetCharSheetChanged(true);
		}
	}
}
void Spawn::SetTotalPower(sint32 new_val)
{
	if(basic_info.power_base == 0)
		SetTotalPowerBase(new_val);
	SetInfo(&basic_info.max_power, new_val);

	if(GetZone() && basic_info.cur_power < basic_info.max_power)
		GetZone()->AddDamagedSpawn(this);

	if (IsEntity() && ((Entity*)this)->GetGroupMemberInfo()) {
		((Entity*)this)->UpdateGroupMemberInfo();
		if (IsPlayer())
			world.GetGroupManager()->SendGroupUpdate(((Entity*)this)->GetGroupMemberInfo()->group_id, GetZone()->GetClientBySpawn(this));
		else
			world.GetGroupManager()->SendGroupUpdate(((Entity*)this)->GetGroupMemberInfo()->group_id);
	}

	if (IsNPC() && ((NPC*)this)->IsPet() && ((NPC*)this)->GetOwner()->IsPlayer()) {
		Player* player = (Player*)((NPC*)this)->GetOwner();
		if (player->GetPet() && player->GetCharmedPet()) {
			if (this == player->GetPet()) {
				player->GetInfoStruct()->pet_power_pct = (float)basic_info.cur_power / (float)basic_info.max_power;
				player->SetCharSheetChanged(true);
			}
		}
		else {
			player->GetInfoStruct()->pet_power_pct = (float)basic_info.cur_power / (float)basic_info.max_power;
			player->SetCharSheetChanged(true);
		}
	}
}
void Spawn::SetTotalPowerBase(sint32 new_val)
{
	SetInfo(&basic_info.power_base, new_val);

	if(GetZone() && basic_info.cur_power < basic_info.max_power)
		GetZone()->AddDamagedSpawn(this);

	if (IsEntity() && ((Entity*)this)->GetGroupMemberInfo()) {
		((Entity*)this)->UpdateGroupMemberInfo();
		if (IsPlayer())
			world.GetGroupManager()->SendGroupUpdate(((Entity*)this)->GetGroupMemberInfo()->group_id, GetZone()->GetClientBySpawn(this));
		else
			world.GetGroupManager()->SendGroupUpdate(((Entity*)this)->GetGroupMemberInfo()->group_id);
	}
}
sint32 Spawn::GetPower()
{
	return basic_info.cur_power;
}
sint32 Spawn::GetTotalPower(){
	return basic_info.max_power;
}
sint32 Spawn::GetTotalPowerBase()
{
	return basic_info.power_base;
}


/*** SAVAGERY ***/
void Spawn::SetSavagery(sint32 savagery, bool setUpdateFlags)
{
	/* JA: extremely limited functionality until we better understand Savagery */
	if(savagery > basic_info.max_savagery)
		SetInfo(&basic_info.max_savagery, savagery, setUpdateFlags);

	SetInfo(&basic_info.cur_savagery, savagery, setUpdateFlags);
}
void Spawn::SetTotalSavagery(sint32 new_val)
{
	/* JA: extremely limited functionality until we better understand Savagery */
	if(basic_info.savagery_base == 0)
		SetTotalSavageryBase(new_val);

	SetInfo(&basic_info.max_savagery, new_val);
}
void Spawn::SetTotalSavageryBase(sint32 new_val){
	SetInfo(&basic_info.savagery_base, new_val);

	if (IsEntity() && ((Entity*)this)->GetGroupMemberInfo()) {
		((Entity*)this)->UpdateGroupMemberInfo();
		if (IsPlayer())
			world.GetGroupManager()->SendGroupUpdate(((Entity*)this)->GetGroupMemberInfo()->group_id, GetZone()->GetClientBySpawn(this));
		else
			world.GetGroupManager()->SendGroupUpdate(((Entity*)this)->GetGroupMemberInfo()->group_id);
	}
}
sint32 Spawn::GetTotalSavagery()
{
	return basic_info.max_savagery;
}
sint32 Spawn::GetSavagery()
{
	return basic_info.cur_savagery;
}


/*** DISSONANCE ***/
void Spawn::SetDissonance(sint32 dissonance, bool setUpdateFlags)
{
	/* JA: extremely limited functionality until we better understand Dissonance */
	if(dissonance > basic_info.max_dissonance)
		SetInfo(&basic_info.max_dissonance, dissonance, setUpdateFlags);

	SetInfo(&basic_info.cur_dissonance, dissonance, setUpdateFlags);
}
void Spawn::SetTotalDissonance(sint32 new_val)
{
	/* JA: extremely limited functionality until we better understand Dissonance */
	if(basic_info.dissonance_base == 0)
		SetTotalDissonanceBase(new_val);

	SetInfo(&basic_info.max_dissonance, new_val);

}
void Spawn::SetTotalDissonanceBase(sint32 new_val)
{
	SetInfo(&basic_info.dissonance_base, new_val);

	if (IsEntity() && ((Entity*)this)->GetGroupMemberInfo()) {
		((Entity*)this)->UpdateGroupMemberInfo();
		if (IsPlayer())
			world.GetGroupManager()->SendGroupUpdate(((Entity*)this)->GetGroupMemberInfo()->group_id, GetZone()->GetClientBySpawn(this));
		else
			world.GetGroupManager()->SendGroupUpdate(((Entity*)this)->GetGroupMemberInfo()->group_id);
	}
}
sint32 Spawn::GetTotalDissonance()
{
	return basic_info.max_dissonance;
}
sint32 Spawn::GetDissonance()
{
	return basic_info.cur_dissonance;
}

void Spawn::ScalePet() {
	if (!IsPet() || !IsEntity()) return;

	double base = pow(GetLevel(), 2) * 2 + 40;

	SetTotalHP(static_cast<sint32>(base * 1.5));
	SetTotalPower(static_cast<sint32>(base * 1.5));
	SetHP(GetTotalHP());
	SetPower(GetTotalPower());

	Entity* entity = static_cast<Entity*>(this);
	entity->ChangePrimaryWeapon();
	entity->ChangeSecondaryWeapon();
}

/* --< Alternate Advancement Points >-- */
void Spawn::SetAssignedAA(sint16 new_val)
{
	SetInfo(&basic_info.assigned_aa, new_val);
}

void Spawn::SetUnassignedAA(sint16 new_val)
{
	SetInfo(&basic_info.unassigned_aa, new_val);
}

void Spawn::SetTradeskillAA(sint16 new_val)
{
	SetInfo(&basic_info.tradeskill_aa, new_val);
}

void Spawn::SetUnassignedTradeskillAA(sint16 new_val)
{
	SetInfo(&basic_info.unassigned_tradeskill_aa, new_val);
}

void Spawn::SetPrestigeAA(sint16 new_val)
{
	SetInfo(&basic_info.prestige_aa, new_val);
}

void Spawn::SetUnassignedPrestigeAA(sint16 new_val)
{
	SetInfo(&basic_info.unassigned_prestige_aa, new_val);
}

void Spawn::SetTradeskillPrestigeAA(sint16 new_val)
{
	SetInfo(&basic_info.tradeskill_prestige_aa, new_val);
}

void Spawn::SetUnassignedTradeskillPrestigeAA(sint16 new_val)
{
	SetInfo(&basic_info.unassigned_tradeskill_prestige_aa, new_val);
}


sint16 Spawn::GetAssignedAA()
{
	return basic_info.assigned_aa;
}

sint16 Spawn::GetUnassignedAA()
{
	return basic_info.unassigned_aa;
}

sint16 Spawn::GetTradeskillAA()
{
	return basic_info.tradeskill_aa;
}

sint16 Spawn::GetUnassignedTradeskillAA()
{
	return basic_info.unassigned_tradeskill_aa;
}

sint16 Spawn::GetPrestigeAA()
{
	return basic_info.prestige_aa;
}

sint16 Spawn::GetUnassignedPretigeAA()
{
	return basic_info.unassigned_prestige_aa;
}

sint16 Spawn::GetTradeskillPrestigeAA()
{
	return basic_info.tradeskill_prestige_aa;
}

sint16 Spawn::GetUnassignedTradeskillPrestigeAA()
{
	return basic_info.unassigned_tradeskill_prestige_aa;
}

float Spawn::GetDistance(float x1, float y1, float z1, float x2, float y2, float z2){
	x1 = x1 - x2;
	y1 = y1 - y2;
	z1 = z1 - z2;
	return sqrt(x1*x1 + y1*y1 + z1*z1); 
}

float Spawn::GetDistance(float x, float y, float z, bool ignore_y){
	if(ignore_y)
		return GetDistance(x, y, z, GetX(), y, GetZ());
	else
		return GetDistance(x, y, z, GetX(), GetY(), GetZ());
}

float Spawn::GetDistance(Spawn* spawn, bool ignore_y){
	float ret = 0;
	if(spawn)
		ret = GetDistance(spawn->GetX(), spawn->GetY(), spawn->GetZ(), ignore_y);
	return ret;
}

int32 Spawn::GetRespawnTime(){
	return respawn;
}

void Spawn::SetRespawnTime(int32 time){
	respawn = time;
}

int32 Spawn::GetExpireOffsetTime(){
	return expire_offset;
}

void Spawn::SetExpireOffsetTime(int32 time){
	expire_offset = time;
}

int32 Spawn::GetSpawnLocationID(){
	return spawn_location_id;
}

void Spawn::SetSpawnLocationID(int32 id){
	spawn_location_id = id;
}

int32 Spawn::GetSpawnEntryID(){
	return spawn_entry_id;
}

void Spawn::SetSpawnEntryID(int32 id){
	spawn_entry_id = id;
}

int32 Spawn::GetSpawnLocationPlacementID(){
	return spawn_location_spawns_id;
}

void Spawn::SetSpawnLocationPlacementID(int32 id){
	spawn_location_spawns_id = id;
}

const char* Spawn::GetSpawnScript(){
	if(spawn_script.length() > 0)
		return spawn_script.c_str();
	else
		return 0;
}

void Spawn::SetSpawnScript(string name){
	spawn_script = name;
}

void Spawn::SetPrimaryCommand(const char* name, const char* command, float distance){
	EntityCommand* entity_command = CreateEntityCommand(name, distance, command, "", 0, 0);
	if(primary_command_list.size() > 0 && primary_command_list[0]){
		safe_delete(primary_command_list[0]);
		primary_command_list[0] = entity_command;
	}
	else
		primary_command_list.push_back(entity_command);
}

void Spawn::SetSecondaryCommands(vector<EntityCommand*>* commands){
	if(commands && commands->size() > 0){
		vector<EntityCommand*>::iterator itr;
		if(secondary_command_list.size() > 0){
			for(itr = secondary_command_list.begin(); itr != secondary_command_list.end(); itr++){
				safe_delete(*itr);
			}
			secondary_command_list.clear();
		}
		EntityCommand* command = 0;
		for(itr = commands->begin(); itr != commands->end(); itr++){
			command = CreateEntityCommand(*itr);
			secondary_command_list.push_back(command);
		}
	}
}

void Spawn::SetPrimaryCommands(vector<EntityCommand*>* commands){
	if(commands && commands->size() > 0){
		vector<EntityCommand*>::iterator itr;
		if(primary_command_list.size() > 0){
			for(itr = primary_command_list.begin(); itr != primary_command_list.end(); itr++){
				safe_delete(*itr);
			}
			primary_command_list.clear();
		}
		EntityCommand* command = 0;
		for(itr = commands->begin(); itr != commands->end(); itr++){
			command = CreateEntityCommand(*itr);
			primary_command_list.push_back(command);
		}
	}
}

EntityCommand* Spawn::FindEntityCommand(string command) {
	EntityCommand* entity_command = 0;
	if (primary_command_list.size() > 0) {
		vector<EntityCommand*>::iterator itr;
		for (itr = primary_command_list.begin(); itr != primary_command_list.end(); itr++) {
			if ((*itr)->command == command) {
				entity_command = *itr;
				break;
			}
		}
	}
	if (!entity_command && secondary_command_list.size() > 0) {
		vector<EntityCommand*>::iterator itr;
		for (itr = secondary_command_list.begin(); itr != secondary_command_list.end(); itr++) {
			if ((*itr)->command == command) {
				entity_command = *itr;
				break;
			}
		}
	}
	return entity_command;
}

void Spawn::SetSizeOffset(int8 offset){
	size_offset = offset;
}

int8 Spawn::GetSizeOffset(){
	return size_offset;
}

void Spawn::SetMerchantID(int32 val){
	merchant_id = val;
}

int32 Spawn::GetMerchantID(){
	return merchant_id;
}

void Spawn::SetMerchantType(int8 val){
	merchant_type = val;
}

int8 Spawn::GetMerchantType(){
	return merchant_type;
}

void Spawn::SetQuestsRequired(map<int32, vector<int16>* >* quests){
	if(quests){
		map<int32, vector<int16>* >::iterator itr;
		for(itr = quests->begin(); itr != quests->end(); itr++){
			vector<int16>* quest_steps = itr->second;
			for (int32 i = 0; i < quest_steps->size(); i++)
				SetQuestsRequired(itr->first, quest_steps->at(i));
		}
	}
}

void Spawn::SetQuestsRequired(int32 quest_id, int16 quest_step){
	m_requiredQuests.writelock(__FUNCTION__, __LINE__);
	if (required_quests.count(quest_id) == 0)
		required_quests[quest_id] = new vector<int16>;
	else{
		for (int32 i = 0; i < required_quests[quest_id]->size(); i++){
			if (required_quests[quest_id]->at(i) == quest_step){
				m_requiredQuests.releasewritelock(__FUNCTION__, __LINE__);
				return;
			}
		}
	}
	required_quests[quest_id]->push_back(quest_step);
	m_requiredQuests.releasewritelock(__FUNCTION__, __LINE__);
}

void Spawn::SetRequiredHistory(int32 event_id, int32 value1, int32 value2){
	LUAHistory set_value;
	set_value.Value = value1;
	set_value.Value2 = value2;
	set_value.SaveNeeded = false;
	m_requiredHistory.writelock(__FUNCTION__, __LINE__);
	required_history[event_id] = set_value;
	m_requiredHistory.releasewritelock(__FUNCTION__, __LINE__);
}

map<int32, vector<int16>* >* Spawn::GetQuestsRequired(){
	return &required_quests;
}

void Spawn::SetTransporterID(int32 id){
	transporter_id = id;
}

int32 Spawn::GetTransporterID(){
	return transporter_id;
}

void Spawn::InitializePosPacketData(Player* player, PacketStruct* packet){
	int16 version = packet->GetVersion();
	packet->setDataByName("pos_grid_id", appearance.pos.grid_id);
	bool include_heading = true;
	if (IsWidget() && ((Widget*)this)->GetIncludeHeading() == false)
		include_heading = false;
	else if (IsSign() && ((Sign*)this)->GetIncludeHeading() == false)
		include_heading = false;
	else if (IsGroundSpawn())
		include_heading = false;

	if (include_heading){
		packet->setDataByName("pos_heading1", appearance.pos.Dir1);
		packet->setDataByName("pos_heading2", appearance.pos.Dir2);
	}

	packet->setDataByName("pos_collision_radius", appearance.pos.collision_radius > 0 ? appearance.pos.collision_radius : 32);
	if (version <= 910){
		packet->setDataByName("pos_size", size > 0 ? size : 32);
		packet->setDataByName("pos_size_multiplier", 32); //32 is normal
	}
	else{
		packet->setDataByName("pos_size_ratio", size > 0 ? (((float)size) / 32) : 1);
		packet->setDataByName("pos_size_multiplier_ratio", 1);
	}
	packet->setDataByName("pos_state", appearance.pos.state);
	bool include_location = true;
	if (IsWidget() && ((Widget*)this)->GetIncludeLocation() == false)
		include_location = false;
	else if (IsSign() && ((Sign*)this)->GetIncludeLocation() == false)
		include_location = false;

	if (include_location){
		if (IsWidget() && ((Widget*)this)->GetMultiFloorLift()) {
			Widget* widget = (Widget*)this;
			float x = appearance.pos.X - widget->GetWidgetX();
			float y = appearance.pos.Y - widget->GetWidgetY();
			float z = appearance.pos.Z - widget->GetWidgetZ();

			packet->setDataByName("pos_x", x);
			packet->setDataByName("pos_y", y);
			packet->setDataByName("pos_z", z);
		}
		else {
			packet->setDataByName("pos_x", appearance.pos.X);
			packet->setDataByName("pos_y", appearance.pos.Y);
			packet->setDataByName("pos_z", appearance.pos.Z);

			if (IsPlayer()) {
				if (GetSpeedX() > 0)
					packet->setDataByName("pos_next_x", appearance.pos.X + (GetSpeedX() * 2.5));

				if (GetSpeedY() > 0)
					packet->setDataByName("pos_next_y", appearance.pos.Y + (GetSpeedY() * 2.5));

				if (GetSpeedZ() > 0)
					packet->setDataByName("pos_next_z", appearance.pos.Z + (GetSpeedZ() * 2.5));
			}
		}
		if (IsSign())
			packet->setDataByName("pos_unknown6", 3, 2);
	}

	if (IsWidget() && ((Widget*)this)->GetMultiFloorLift()) {
		Widget* widget = (Widget*)this;

		float x;
		float y;
		float z;

		if (IsRunning()){
			x = appearance.pos.X2 - widget->GetWidgetX();
			y = appearance.pos.Y2 - widget->GetWidgetY();
			z = appearance.pos.Z2- widget->GetWidgetZ();
		}
		else {
			x = appearance.pos.X - widget->GetWidgetX();
			y = appearance.pos.Y - widget->GetWidgetY();
			z = appearance.pos.Z - widget->GetWidgetZ();
		}

		packet->setDataByName("pos_next_x", x);
		packet->setDataByName("pos_next_y", y);
		packet->setDataByName("pos_next_z", z);

		packet->setDataByName("pos_x3", x);
		packet->setDataByName("pos_y3", y);
		packet->setDataByName("pos_z3", z);
	}
	else {
		packet->setDataByName("pos_next_x", appearance.pos.X2);
		packet->setDataByName("pos_next_y", appearance.pos.Y2);
		packet->setDataByName("pos_next_z", appearance.pos.Z2);

		packet->setDataByName("pos_x3", appearance.pos.X3);
		packet->setDataByName("pos_y3", appearance.pos.Y3);
		packet->setDataByName("pos_z3", appearance.pos.Z3);
	}
	packet->setDataByName("pos_unknown2", 4, 2);

	int16 speed_multiplier = rule_manager.GetGlobalRule(R_Spawn, SpeedMultiplier)->GetInt16(); // was 1280, 600 and now 300... investigating why
	float speed_ratio = rule_manager.GetGlobalRule(R_Spawn, SpeedRatio)->GetFloat(); // was 7.5 for 1280 and 600, then 0 for 300... investigating why
	LogWrite(SPAWN__DEBUG, 5, "Spawn", "Speed: %.2f (from LUA), Multiplier: %i, Ratio: %.2f, Final: %.2f", GetSpeed(), speed_multiplier, speed_ratio, GetSpeed()*((float)speed_multiplier/speed_ratio));
	if (speed_ratio > 0){
		if (IsPlayer())
			packet->setDataByName("pos_speed", ((Player*)this)->GetPosPacketSpeed()*((float)speed_multiplier / speed_ratio));
		else
			packet->setDataByName("pos_speed", GetSpeed()*((float)speed_multiplier / speed_ratio));
	}
	else{
		if (IsPlayer())
			packet->setDataByName("pos_speed", ((Player*)this)->GetPosPacketSpeed()*speed_multiplier);
		else
			packet->setDataByName("pos_speed", GetSpeed()*speed_multiplier);
	}

	if(IsNPC() || IsWidget() || IsSign()){
		packet->setDataByName("pos_move_type", 11);
		packet->setDataByName("pos_movement_mode", 2);
	}
	else if(IsGroundSpawn()){
		packet->setDataByName("pos_move_type", 16);
		packet->setDataByName("pos_movement_mode", 2);
	}

	if(version <= 910)
		packet->setDataByName("pos_unknown10", 0xFFFF, 1);
	else
		packet->setDataByName("pos_unknown10", 0xFFFF);
	if(version <= 910)
		packet->setDataByName("pos_unknown10", 0xFFFF, 2);
	else
		packet->setDataByName("pos_unknown10", 0XFFFF, 1);
	packet->setDataByName("pos_pitch1", appearance.pos.Pitch1);
	packet->setDataByName("pos_pitch2", appearance.pos.Pitch2);
	packet->setDataByName("pos_roll", appearance.pos.Roll);
}

void Spawn::InitializeInfoPacketData(Player* spawn, PacketStruct* packet){
	int16 version = packet->GetVersion();
	if(appearance.targetable == 1 || appearance.show_level == 1 || appearance.display_name == 1){
		appearance.locked_no_loot = 1; //for now
		if(!IsObject() && !IsGroundSpawn() && !IsWidget() && !IsSign()){
			int8 percent = 0;
			if(GetHP() > 0)
				percent = (int8)(((float)GetHP()/GetTotalHP()) * 100);
			if(percent < 100){
 				packet->setDataByName("hp_remaining", 100 ^ percent);
			}
			else
				packet->setDataByName("hp_remaining", 0);
			if(GetTotalPower() > 0){
				percent = (int8)(((float)GetPower()/GetTotalPower()) * 100);
				if(percent > 0)
					packet->setDataByName("power_percent", percent);
				else
					packet->setDataByName("power_percent", 0);
			}
		}
	}
	packet->setDataByName("level", (int8)GetLevel());
	packet->setDataByName("unknown4", (int8)GetLevel());
	packet->setDataByName("difficulty", appearance.encounter_level); //6);
	packet->setDataByName("heroic_flag", appearance.heroic_flag);
	if (!IsObject() && !IsGroundSpawn() && !IsWidget() && !IsSign())
		packet->setDataByName("interaction_flag", 12); //this makes NPCs head turn to look at you

	if (IsPlayer() && Alive()) {
		if (spawn->CanAttackTarget(this)) {
			packet->setDataByName("spawn_type", 6);
		} else if (version >= 1188) {
			packet->setDataByName("spawn_type", 0);
		} else {
			packet->setDataByName("spawn_type", spawn_type);
		}
	} else {
		packet->setDataByName("spawn_type", spawn_type);
	}

	packet->setDataByName("class", appearance.adventure_class);

	int16 model_type = appearance.model_type;
	if (GetIllusionModel() != 0) {
		if (IsPlayer()) {
			if (((Player*)this)->get_character_flag(CF_SHOW_ILLUSION)) {
				model_type = GetIllusionModel();
			}
		}
		else
			model_type = GetIllusionModel();
	}

	packet->setDataByName("model_type", model_type);
	if(appearance.soga_model_type == 0)
		packet->setDataByName("soga_model_type", model_type);
	else
		packet->setDataByName("soga_model_type", appearance.soga_model_type);

	if(GetTempActionState() >= 0)
		packet->setDataByName("action_state", GetTempActionState());
	else
		packet->setDataByName("action_state", appearance.action_state);

	if (GetTempVisualState() >= 0) {
		if (this != spawn || (GetTempVisualState() != 290 && GetTempVisualState() != 11757 && GetTempVisualState() != 11758))
			packet->setDataByName("visual_state", GetTempVisualState());
	} else {
		packet->setDataByName("visual_state", appearance.visual_state);
	}
	packet->setDataByName("emote_state", appearance.emote_state);
	packet->setDataByName("mood_state", appearance.mood_state);
	packet->setDataByName("gender", appearance.gender);
	packet->setDataByName("race", appearance.race);
	packet->setDataByName("gender", appearance.gender);
	if(IsEntity()){
		Entity* entity = ((Entity*)this);
		packet->setDataByName("combat_voice", entity->GetCombatVoice());
		packet->setDataByName("emote_voice", entity->GetEmoteVoice());
		for(int i=0;i<25;i++){
			if(i == 2){ //don't send helm if hidden flag
				if(IsPlayer()){
					if(((Player*)this)->get_character_flag(CF_HIDE_HELM)){
						packet->setDataByName("equipment_types", 0, i);
						packet->setColorByName("equipment_colors", 0, i);
						packet->setColorByName("equipment_highlights", 0, i);
						continue;
					}
				}
			}
			else if(i == 19){ //don't send cloak if hidden
				if(IsPlayer()){
					if(!((Player*)this)->get_character_flag(CF_SHOW_CLOAK)){
						packet->setDataByName("equipment_types", 0, i);
						packet->setColorByName("equipment_colors", 0, i);
						packet->setColorByName("equipment_highlights", 0, i);
						continue;
					}
				}
			}
			packet->setDataByName("equipment_types", entity->equipment.equip_id[i], i);
			packet->setColorByName("equipment_colors", entity->equipment.color[i], i);
			packet->setColorByName("equipment_highlights", entity->equipment.highlight[i], i);
		}
		packet->setDataByName("mount_type", entity->GetMount());

		// find the visual flags
		int8 vis_flag = 0;
		//Invis + crouch flag check
		if (entity->IsStealthed())
			vis_flag += (INFO_VIS_FLAG_INVIS + INFO_VIS_FLAG_CROUCH);
		//Invis flag check
		else if (entity->IsInvis())
			vis_flag += INFO_VIS_FLAG_INVIS;
		//Mount flag check
		if (entity->GetMount() > 0)
			vis_flag += INFO_VIS_FLAG_MOUNTED;
		//Hide hood check
		if ((IsPlayer() && ((Player*)this)->get_character_flag(CF_HIDE_HOOD)) || appearance.hide_hood)
			vis_flag += INFO_VIS_FLAG_HIDE_HOOD;

		packet->setDataByName("visual_flag", vis_flag);
		packet->setColorByName("mount_saddle_color", entity->GetMountSaddleColor());
		packet->setColorByName("mount_color", entity->GetMountColor());
		packet->setDataByName("hair_type_id", entity->features.hair_type);
		packet->setDataByName("chest_type_id", entity->features.chest_type);
		packet->setDataByName("wing_type_id", entity->features.wing_type);
		packet->setDataByName("legs_type_id", entity->features.legs_type);
		packet->setDataByName("soga_hair_type_id", entity->features.soga_hair_type);
		packet->setDataByName("facial_hair_type_id", entity->features.hair_face_type);
		packet->setDataByName("soga_facial_hair_type_id", entity->features.soga_hair_face_type);		
		for(int i=0;i<3;i++){
			packet->setDataByName("eye_type", entity->features.eye_type[i], i);
			packet->setDataByName("ear_type", entity->features.ear_type[i], i);
			packet->setDataByName("eye_brow_type", entity->features.eye_brow_type[i], i);
			packet->setDataByName("cheek_type", entity->features.cheek_type[i], i);
			packet->setDataByName("lip_type", entity->features.lip_type[i], i);
			packet->setDataByName("chin_type", entity->features.chin_type[i], i);
			packet->setDataByName("nose_type", entity->features.nose_type[i], i);
			packet->setDataByName("soga_eye_type", entity->features.soga_eye_type[i], i);
			packet->setDataByName("soga_ear_type", entity->features.soga_ear_type[i], i);
			packet->setDataByName("soga_eye_brow_type", entity->features.soga_eye_brow_type[i], i);
			packet->setDataByName("soga_cheek_type", entity->features.soga_cheek_type[i], i);
			packet->setDataByName("soga_lip_type", entity->features.soga_lip_type[i], i);
			packet->setDataByName("soga_chin_type", entity->features.soga_chin_type[i], i);
			packet->setDataByName("soga_nose_type", entity->features.soga_nose_type[i], i);
		}
		packet->setColorByName("skin_color", entity->features.skin_color);
		packet->setColorByName("eye_color", entity->features.eye_color);		
		packet->setColorByName("hair_type_color", entity->features.hair_type_color);
		packet->setColorByName("hair_type_highlight_color", entity->features.hair_type_highlight_color);
		packet->setColorByName("hair_face_color", entity->features.hair_face_color);
		packet->setColorByName("hair_face_highlight_color", entity->features.hair_face_highlight_color);
		packet->setColorByName("hair_highlight", entity->features.hair_highlight_color);
		packet->setColorByName("wing_color1", entity->features.wing_color1);
		packet->setColorByName("wing_color2", entity->features.wing_color2);
		packet->setColorByName("hair_color1", entity->features.hair_color1);
		packet->setColorByName("hair_color2", entity->features.hair_color2);
		packet->setColorByName("soga_skin_color", entity->features.soga_skin_color);
		packet->setColorByName("soga_eye_color", entity->features.soga_eye_color);
		packet->setColorByName("soga_hair_color1", entity->features.soga_hair_color1);
		packet->setColorByName("soga_hair_color2", entity->features.soga_hair_color2);
		packet->setColorByName("soga_hair_type_color", entity->features.soga_hair_type_color);
		packet->setColorByName("soga_hair_type_highlight_color", entity->features.soga_hair_type_highlight_color);
		packet->setColorByName("soga_hair_face_color", entity->features.soga_hair_face_color);
		packet->setColorByName("soga_hair_face_highlight_color", entity->features.soga_hair_face_highlight_color);
		packet->setColorByName("soga_hair_highlight", entity->features.soga_hair_highlight_color);
	}
	else{
		EQ2_Color empty;
		empty.red = 255;
		empty.blue = 255;
		empty.green = 255;
		packet->setColorByName("skin_color", empty);
		packet->setColorByName("eye_color", empty);
		packet->setColorByName("soga_skin_color", empty);
		packet->setColorByName("soga_eye_color", empty);
	}

	if (appearance.icon == 0) {
		if (appearance.attackable == 1 || (IsPlayer() && spawn->CanAttackTarget((Player*)this))) {
			appearance.icon = 0;
		}
		else if (appearance.encounter_level > 0) {
			appearance.icon = 4;
		}
		else {
			appearance.icon = 6;
		}
	}
	
	// If Coe+ clients modify the values before we send
	// if not then just send the value we have.
	int8 temp_icon = appearance.icon;

	//Check if we need to add the hand icon..
	if ((temp_icon & 6) != 6 && appearance.display_hand_icon) {
		temp_icon |= 6;
	}

	//Icon value 28 for boats, set this without modifying the value
	if (version >= 1188 && temp_icon != 28) {
		if ((temp_icon & 64) > 0) {
			temp_icon -= 64;   // remove the DoV value;
			temp_icon += 128; // add the CoE value
		}
		if ((temp_icon & 32) > 0) {
			temp_icon -= 32;   // remove the DoV value;
			temp_icon += 64; // add the CoE value
		}
		if ((temp_icon & 4) > 0) {
			temp_icon -= 4;   // remove DoV value
			temp_icon += 8;   // add the CoE icon
		}
		if ((temp_icon & 6) > 0) {
			temp_icon -= 10;   // remove DoV value
			temp_icon += 12;   // add the CoE icon
		}
	}
	packet->setDataByName("icon", temp_icon);//appearance.icon);

	int16 temp_activity_status = 0;
	if (version >= 1188) {
		if ((appearance.activity_status & ACTIVITY_STATUS_ROLEPLAYING) > 0)
			temp_activity_status += ACTIVITY_STATUS_ROLEPLAYING_1188;

		if ((appearance.activity_status & ACTIVITY_STATUS_ANONYMOUS) > 0)
			temp_activity_status += ACTIVITY_STATUS_ANONYMOUS_1188;

		if ((appearance.activity_status & ACTIVITY_STATUS_LINKDEAD) > 0)
			temp_activity_status += ACTIVITY_STATUS_LINKDEAD_1188;

		if ((appearance.activity_status & ACTIVITY_STATUS_CAMPING) > 0)
			temp_activity_status += ACTIVITY_STATUS_CAMPING_1188;

		if ((appearance.activity_status & ACTIVITY_STATUS_LFG) > 0)
			temp_activity_status += ACTIVITY_STATUS_LFG_1188;

		if ((appearance.activity_status & ACTIVITY_STATUS_LFW) > 0)
			temp_activity_status += ACTIVITY_STATUS_LFW_1188;

		if ((appearance.activity_status & ACTIVITY_STATUS_SOLID) > 0)
			temp_activity_status += ACTIVITY_STATUS_SOLID_1188;

		if ((appearance.activity_status & ACTIVITY_STATUS_IMMUNITY_GAINED) > 0)
			temp_activity_status += ACTIVITY_STATUS_IMMUNITY_GAINED_1188;

		if ((appearance.activity_status & ACTIVITY_STATUS_IMMUNITY_REMAINING) > 0)
			temp_activity_status += ACTIVITY_STATUS_IMMUNITY_REMAINING_1188;
	}
	else
		temp_activity_status = appearance.activity_status;

	packet->setDataByName("activity_status", temp_activity_status); //appearance.activity_status);

	// If player and player has a follow target
	if (IsPlayer()) {
		if (((Player*)this)->GetFollowTarget())
			packet->setDataByName("follow_target", ((((Player*)this)->GetIDWithPlayerSpawn(((Player*)this)->GetFollowTarget()) * -1) - 1));
		else
			packet->setDataByName("follow_target", 0);
	}
	if (GetTarget() && GetTarget()->GetTargetable())
		packet->setDataByName("target_id", ((spawn->GetIDWithPlayerSpawn(GetTarget()) * -1) - 1));
	else
		packet->setDataByName("target_id", 0);

	//Send spell effects for target window
	if(IsEntity()){
		InfoStruct* info = ((Entity*)this)->GetInfoStruct();
		int8 i = 0;
		int16 backdrop = 0;
		int16 spell_icon = 0;
		int32 spell_id = 0;
		LuaSpell* spell = 0;
		((Entity*)this)->GetSpellEffectMutex()->readlock(__FUNCTION__, __LINE__);
		while(i < 30){
			//Change value of spell id for this packet if spell exists
			spell_id = info->spell_effects[i].spell_id;
			if(spell_id > 0)
				spell_id = 0xFFFFFFFF - spell_id;
			else
				spell_id = 0;
			packet->setSubstructDataByName("spell_effects", "spell_id", spell_id, i);

			//Change value of spell icon for this packet if spell exists
			spell_icon = info->spell_effects[i].icon;
			if(spell_icon > 0){
				if(!(spell_icon == 0xFFFF))
					spell_icon = 0xFFFF - spell_icon;
			}
			else
				spell_icon = 0;
			packet->setSubstructDataByName("spell_effects", "spell_icon", spell_icon, i);

			//Change backdrop values to match values in this packet
			backdrop = info->spell_effects[i].icon_backdrop;
			switch(backdrop){
				case 312:
					backdrop = 33080;
					break;
				case 313:
					backdrop = 33081;
					break;
				case 314:
					backdrop = 33082;
					break;
				case 315:
					backdrop = 33083;
					break;
				case 316:
					backdrop = 33084;
					break;
				case 317:
					backdrop = 33085;
					break;
				case (318 || 319):
					backdrop = 33086;
					break;
				default:
					break;
			}


			packet->setSubstructDataByName("spell_effects", "spell_icon_backdrop", backdrop, i);
			spell = info->spell_effects[i].spell;
			if (spell)
				packet->setSubstructDataByName("spell_effects", "spell_triggercount", spell->num_triggers, i);
			i++;
		}
		((Entity*)this)->GetSpellEffectMutex()->releasereadlock(__FUNCTION__, __LINE__);
	}
}

void Spawn::MoveToLocation(Spawn* spawn, float distance, bool immediate){
	if(!spawn)
		return;
	SetRunningTo(spawn);
	FaceTarget(spawn);
	if(immediate)
		ClearRunningLocations();	
	AddRunningLocation(spawn->GetX(), spawn->GetY(), spawn->GetZ(), GetSpeed(), distance);
}

void Spawn::ProcessMovement(){
	if(IsPlayer()){
		//Check if player is riding a boat, if so update pos (boat's current location + XYZ offsets)
		Player* player = ((Player*)this);
		int32 boat_id = player->GetBoatSpawn();
		Spawn* boat = 0;
		if(boat_id > 0)
			boat = GetZone()->GetSpawnByID(boat_id);
		if(boat){
			SetX(boat->GetX() + player->GetBoatX());
			SetY(boat->GetY() + player->GetBoatY());
			SetZ(boat->GetZ() + player->GetBoatZ());
		}
		return;
	}

	if (GetHP() <= 0 && !IsWidget())
		return;

	MMovementLoop.lock();
	Spawn* followTarget = GetZone()->GetSpawnByID(m_followTarget);
	if (!followTarget && m_followTarget > 0)
		m_followTarget = 0;
	if (following && followTarget) {

		// Need to clear m_followTarget before the zoneserver deletes it
		if (followTarget->GetHP() <= 0) {
			followTarget = 0;
			MMovementLoop.unlock();
			return;
		}

		if (!IsEntity() || (!((Entity*)this)->IsCasting() && !((Entity*)this)->IsMezzedOrStunned() && !((Entity*)this)->IsRooted())) {
			if (GetBaseSpeed() > 0) {
				CalculateRunningLocation();
			}
			else {
				float speed = 4.0f;
				if (IsEntity())
					speed = ((Entity*)this)->GetMaxSpeed();
				SetSpeed(speed);
			}
			MovementLocation* loc = GetCurrentRunningLocation();
			if (GetDistance(followTarget) <= MAX_COMBAT_RANGE || (loc && loc->x == GetX() && loc->y == GetY() && loc->z == GetZ())) {
				ClearRunningLocations();
				CalculateRunningLocation(true);
			}
			else if (loc) {
				float distance = GetDistance(loc->x, loc->y, loc->z, followTarget->GetX(), followTarget->GetY(), followTarget->GetZ());
				if (distance > MAX_COMBAT_RANGE) {
					MoveToLocation(followTarget, MAX_COMBAT_RANGE);
					CalculateRunningLocation();
				}
			}
			else {
				MoveToLocation(followTarget, MAX_COMBAT_RANGE);
				CalculateRunningLocation();
			}
		}
	}

	// Movement loop is only for scripted paths
	else if(!MovementInterrupted() && !EngagedInCombat() && !NeedsToResumeMovement() && movement_loop.size() > 0 && movement_index < movement_loop.size() && (!IsNPC() || !((NPC*)this)->m_runningBack)){
		// Get the target location
		MovementData* data = movement_loop[movement_index];
		// need to resume our movement
		if(resume_movement){
			if (movement_locations){
				while (movement_locations->size()){
					safe_delete(movement_locations->front());
					movement_locations->pop_front();
				}
				movement_locations->clear();
			}

			data = movement_loop[movement_index];
			SetSpeed(data->speed);
			if(!IsWidget())
				FaceTarget(data->x, data->z);
			// 0 delay at target location, need to set multiple locations
			if(data->delay == 0 && movement_loop.size() > 0) {
				int16 tmp_index = movement_index+1;
				MovementData* data2 = 0;
				if(tmp_index < movement_loop.size()) 
					data2 = movement_loop[tmp_index];
				else
					data2 = movement_loop[0];
				AddRunningLocation(data->x, data->y, data->z, data->speed, 0, true, false);				
				AddRunningLocation(data2->x, data2->y, data2->z, data2->speed);
			}
			// delay at target location, only need to set 1 location
			else
				AddRunningLocation(data->x, data->y, data->z, data->speed);
			movement_start_time = 0;
			resume_movement = false;
		}
		// If we are not moving or we have arrived at our destination
		else if(!IsRunning() || (data && data->x == GetX() && data->y == GetY() && data->z == GetZ())){
			// If we were moving remove the last running location (the point we just arrived at)
			if(IsRunning())
				RemoveRunningLocation();

			// If this waypoint has a delay and we just arrived here (movement_start_time == 0)
			if(data->delay > 0 && movement_start_time == 0){
				// Set the current time
				movement_start_time = Timer::GetCurrentTime2();
				// If this waypoint had a lua function then call it
				if(data->lua_function.length() > 0)
					GetZone()->CallSpawnScript(this, SPAWN_SCRIPT_CUSTOM, 0, data->lua_function.c_str());
			}
			// If this waypoint has no delay or we have waited the required time (current time >= delay + movement_start_time)
			else if(data->delay == 0 || (data->delay > 0 && Timer::GetCurrentTime2() >= (data->delay+movement_start_time))) {
				// if no delay at this waypoint but a lua function for it then call the function
				if(data->delay == 0 && data->lua_function.length() > 0)
					GetZone()->CallSpawnScript(this, SPAWN_SCRIPT_CUSTOM, 0, data->lua_function.c_str());
				// Advance the current movement loop index
				if((int16)(movement_index+1) < movement_loop.size())
					movement_index++;
				else
					movement_index = 0;
				// Get the next target location
				data = movement_loop[movement_index];
				// set the speed for that location
				SetSpeed(data->speed);

				if(!IsWidget())
				// turn towards the location
					FaceTarget(data->x, data->z);
				// If 0 delay at location get and set data for the point after it
				if(data->delay == 0 && movement_loop.size() > 0){
					while (movement_locations->size()){
						safe_delete(movement_locations->front());
						movement_locations->pop_front();
					}
					// clear current target locations
					movement_locations->clear();
					// get the data for the location after out new location
					int16 tmp_index = movement_index+1;
					MovementData* data2 = 0;
					if(tmp_index < movement_loop.size()) 
						data2 = movement_loop[tmp_index];
					else
						data2 = movement_loop[0];
					// set the first location (adds it to movement_locations that we just cleared)
					AddRunningLocation(data->x, data->y, data->z, data->speed, 0, true, false);
					// set the location after that
					AddRunningLocation(data2->x, data2->y, data2->z, data2->speed);
				}
				// there is a delay at the next location so we only need to set it
				else
					AddRunningLocation(data->x, data->y, data->z, data->speed);

				// reset this timer to 0 now that we are moving again
				movement_start_time = 0;
			}
		}
		// moving and not at target location yet
		else if(GetBaseSpeed() > 0)
			CalculateRunningLocation();
		// not moving, have a target location but not at it yet
		else if (data) {
			SetSpeed(data->speed);
			AddRunningLocation(data->x, data->y, data->z, data->speed);
		}
	}
	else if (!MovementInterrupted() && IsRunning()) {
		CalculateRunningLocation();
	}
	else if (IsNPC() && !MovementInterrupted() && !IsRunning() && !EngagedInCombat() && ((NPC*)this)->GetRunbackLocation()) {
		// Is an npc that is not moving and not engaged in combat but has a run back location set then clear the runback location
		LogWrite(NPC_AI__DEBUG, 7, "NPC_AI", "Clear runback location for %s", GetName());
		SetPos(&appearance.pos.Dir1, ((NPC*)this)->m_runbackHeading, false);
		SetPos(&appearance.pos.Dir2, ((NPC*)this)->m_runbackHeading, true);
		((NPC*)this)->ClearRunback();
		resume_movement = true;
		NeedsToResumeMovement(false);
	}
	MMovementLoop.unlock();
}

void Spawn::ResetMovement(){
	MMovementLoop.lock();
	vector<MovementData*>::iterator itr;
	for(itr = movement_loop.begin(); itr != movement_loop.end(); itr++){
		safe_delete(*itr);
	}
	MMovementLoop.unlock();
	resume_movement = true;
	movement_index = 0;
}

void Spawn::AddMovementLocation(float x, float y, float z, float speed, int16 delay, const char* lua_function){

	LogWrite(LUA__DEBUG, 5, "LUA", "AddMovementLocation: x: %.2f, y: %.2f, z: %.2f, speed: %.2f, delay: %i, lua: %s",
		x, y, z, speed, delay, string(lua_function).c_str());

	MovementData* data = new MovementData;
	data->x = x;
	data->y = y;
	data->z = z;
	data->speed = speed;
	data->delay = delay*1000;
	if(lua_function)
		data->lua_function = string(lua_function);
	MMovementLoop.lock();
	movement_loop.push_back(data);
	MMovementLoop.unlock();
}

bool Spawn::IsRunning(){
	if(movement_locations && movement_locations->size() > 0)
		return true;
	else
		return false;
}

void Spawn::RunToLocation(float x, float y, float z, float following_x, float following_y, float following_z){
	if(!IsWidget())
		FaceTarget(x, z);
	SetPos(&appearance.pos.X2, x);
	SetPos(&appearance.pos.Y2, y);
	SetPos(&appearance.pos.Z2, z);
	if(following_x == 0 && following_y == 0 && following_z == 0){
		SetPos(&appearance.pos.X3, x);
		SetPos(&appearance.pos.Y3, y);
		SetPos(&appearance.pos.Z3, z);
	}
	else{
		SetPos(&appearance.pos.X3, following_x);
		SetPos(&appearance.pos.Y3, following_y);
		SetPos(&appearance.pos.Z3, following_z);
	}
}

MovementLocation* Spawn::GetCurrentRunningLocation(){
	MovementLocation* ret = 0;
	if(movement_locations && movement_locations->size() > 0){
		MMovementLocations->readlock(__FUNCTION__, __LINE__);
		ret = movement_locations->front();
		MMovementLocations->releasereadlock(__FUNCTION__, __LINE__);
	}
	return ret;
}

MovementLocation* Spawn::GetLastRunningLocation(){
	MovementLocation* ret = 0;
	if(movement_locations && movement_locations->size() > 0){
		MMovementLocations->readlock(__FUNCTION__, __LINE__);
		ret = movement_locations->back();
		MMovementLocations->releasereadlock(__FUNCTION__, __LINE__);
	}
	return ret;
}

void Spawn::AddRunningLocation(float x, float y, float z, float speed, float distance_away, bool attackable, bool finished_adding_locations, string lua_function){
	if(speed == 0)
		return;
	MovementLocation* current_location = 0;
	float distance = GetDistance(x, y, z, distance_away != 0);
	if(distance_away != 0){
		distance -= distance_away;
		x = x - (GetX() - x)*distance_away/distance;
		z = z - (GetZ() - z)*distance_away/distance;
	}
	if(!movement_locations){
		movement_locations = new deque<MovementLocation*>();
		MMovementLocations = new Mutex();
	}
	MovementLocation* data = new MovementLocation;
	data->x = x;
	data->y = y;
	data->z = z;
	data->speed = speed;
	data->attackable = attackable;
	data->lua_function = lua_function;
	MMovementLocations->writelock(__FUNCTION__, __LINE__);
	if(movement_locations->size() > 0)
		current_location = movement_locations->back();
	if(!current_location){
		SetSpawnOrigX(GetX());
		SetSpawnOrigY(GetY());
		SetSpawnOrigZ(GetZ());
		SetSpawnOrigHeading(GetHeading());
	}
	movement_locations->push_back(data);	
	if(finished_adding_locations){
		current_location = movement_locations->front();
		SetSpeed(current_location->speed);
		if(movement_locations->size() > 1){		
			data = movement_locations->at(1);
			RunToLocation(current_location->x, current_location->y, current_location->z, data->x, data->y, data->z);
		}
		else
			RunToLocation(current_location->x, current_location->y, current_location->z, 0, 0, 0);
	}
	MMovementLocations->releasewritelock(__FUNCTION__, __LINE__);
}

bool Spawn::RemoveRunningLocation(){
	bool ret = false;
	if(movement_locations){
		MMovementLocations->writelock(__FUNCTION__, __LINE__);
		if(movement_locations->size() > 0){
			delete movement_locations->front();
			movement_locations->pop_front();
			ret = true;
		}
		MMovementLocations->releasewritelock(__FUNCTION__, __LINE__);
	}
	return ret;
}

void Spawn::ClearRunningLocations(){
	while(RemoveRunningLocation()){}
}

bool Spawn::CalculateChange(){
	bool remove_needed = false;
	if(movement_locations && MMovementLocations){		
		MovementLocation* data = 0;
		MMovementLocations->readlock(__FUNCTION__, __LINE__);
		if(movement_locations->size() > 0){
			// Target location
			data = movement_locations->front();
			// If no target or we are at the target location need to remove this point
			if(!data || (data->x == GetX() && data->y == GetY() && data->z == GetZ()))
				remove_needed = true;
			if(data){					
				if(NeedsToResumeMovement()){
					resume_movement = true;
					NeedsToResumeMovement(false);
				}
				if(!data->attackable)
					SetHeading(GetSpawnOrigHeading());
			}
		}
		MMovementLocations->releasereadlock(__FUNCTION__, __LINE__);
		if(remove_needed) {
			if (data && data->lua_function.length() > 0)
				GetZone()->CallSpawnScript(this, SPAWN_SCRIPT_CUSTOM, 0, data->lua_function.c_str());

			RemoveRunningLocation();
			CalculateChange();
		}
		else if(data){
			// Speed is per second so we need a time_step (amount of time since the last update) to modify movement by
			float time_step = ((float)(Timer::GetCurrentTime2() - last_movement_update))/1000;

			// Code taken from EQEmu and modified to work for us
			float nx = GetX();
			float ny = GetY();
			float nz = GetZ();

			float tar_vx = data->x - nx;
			float tar_vy = data->y - ny;
			float tar_vz = data->z - nz;

			//float test_vector=sqrtf (data->x*data->x + data->y*data->y + data->z*data->z);
			// Goto http://www.eq2emulator.net/phpBB3/viewtopic.php?f=13&t=3180#p24256
			// for detailed info on how I got the speed equation (1.1043506061 * GetSpeed() + 0.1832966667)
			float tar_vector = (1.1043506061 * GetSpeed() + 0.1832966667) / sqrtf (tar_vx*tar_vx + tar_vy*tar_vy + tar_vz*tar_vz);

			// Distance less then 0.5 just set the npc to the target location
			if (GetDistance(data->x, data->y, data->z, IsWidget() ? false : true) <= 0.5f) {
				SetX(data->x, false);
				SetY(data->y, false);
				SetZ(data->z, false);
			}
			else {
				SetX(GetX() + ((tar_vx*tar_vector)*time_step), false);
				SetY(GetY() + ((tar_vy*tar_vector)*time_step), false);
				SetZ(GetZ() + ((tar_vz*tar_vector)*time_step), false);
			}

			if (GetZone()->Grid != nullptr) {
				Cell* newCell = GetZone()->Grid->GetCell(GetX(), GetZ());
				if (newCell != Cell_Info.CurrentCell) {
					GetZone()->Grid->RemoveSpawnFromCell(this);
					GetZone()->Grid->AddSpawn(this, newCell);
		}

				int32 newGrid = GetZone()->Grid->GetGridID(this);
				if (newGrid != appearance.pos.grid_id)
					SetPos(&(appearance.pos.grid_id), newGrid);
	}
		}
	}
	return remove_needed;
}

void Spawn::CalculateRunningLocation(bool stop){
	CalculateChange();
	if(stop) {
		//following = false;
		SetPos(&appearance.pos.X2, GetX());
		SetPos(&appearance.pos.Y2, GetY());
		SetPos(&appearance.pos.Z2, GetZ());
		SetPos(&appearance.pos.X3, GetX());
		SetPos(&appearance.pos.Y3, GetY());
		SetPos(&appearance.pos.Z3, GetZ());
	}
}

void Spawn::FaceTarget(float x, float z){
	float angle;
	double diff_x=x - GetX();
	double diff_z=z - GetZ();
	if(diff_z==0){
	   if(diff_x > 0)
		   angle = 90;
	   else
		   angle = 270;
	}
	else
		angle = ((atan(diff_x / diff_z)) * 180) / 3.14159265358979323846;
	if(angle < 0)
		angle = angle + 360;
	else
		angle = angle + 180;
	if(diff_x < 0)
		angle = angle + 180;
	SetHeading(angle);
}

void Spawn::FaceTarget(Spawn* target){
	if(!target)
		return;
	FaceTarget(target->GetX(), target->GetZ());
	if(GetHP() > 0 && target->IsPlayer() && !EngagedInCombat()){
		GetZone()->AddHeadingTimer(this);
		SetTempActionState(0);
	}
}

bool Spawn::MeetsSpawnAccessRequirements(Player* player){
	bool ret = false;
	Quest* quest = 0;
	//Check if we meet all quest requirements first..
	m_requiredQuests.readlock(__FUNCTION__, __LINE__);
	if (player && required_quests.size() > 0){
		map<int32, vector<int16>* >::iterator itr;
		for (itr = required_quests.begin(); itr != required_quests.end(); itr++){
			ret = false;
			player->AddQuestRequiredSpawn(this, itr->first);
			vector<int16>* quest_steps = itr->second;
			for (int32 i = 0; i < quest_steps->size(); i++) {
				quest = player->GetQuest(itr->first);
				if (req_quests_continued_access){
					if (quest){
						if (quest->GetQuestStepCompleted(quest_steps->at(i))){
							ret = true;
							break;
						}
					}
					else if (player->GetCompletedQuest(itr->first)){
						ret = true;
						break;
					}
				}
				if (quest && quest->QuestStepIsActive(quest_steps->at(i))) {
					ret = true;
					break;
				}
			}
		}
	}
	else
		ret = true;
	m_requiredQuests.releasereadlock(__FUNCTION__, __LINE__);
	if (!ret)
		return ret;

	//Now check if the player meets all history requirements
	m_requiredHistory.readlock(__FUNCTION__, __LINE__);
	if (required_history.size() > 0){
		map<int32, LUAHistory>::iterator itr;
		for (itr = required_history.begin(); itr != required_history.end(); itr++){
			player->AddHistoryRequiredSpawn(this, itr->first);
			LUAHistory* player_history = player->GetLUAHistory(itr->first);
			if (player_history){
				if (player_history->Value != itr->second.Value || player_history->Value2 != itr->second.Value2)
					ret = false;
			}
			else
				ret = false;
			if (!ret)
				break;
		}
	}
	m_requiredHistory.releasereadlock(__FUNCTION__, __LINE__);
	return ret;
}

vector<Spawn*>* Spawn::GetSpawnGroup(){
	vector<Spawn*>* ret_list = 0;
	if(spawn_group_list){
		ret_list = new vector<Spawn*>();
		if(MSpawnGroup)
			MSpawnGroup->readlock(__FUNCTION__, __LINE__);
		ret_list->insert(ret_list->begin(), spawn_group_list->begin(), spawn_group_list->end());
		if(MSpawnGroup)
			MSpawnGroup->releasereadlock(__FUNCTION__, __LINE__);
	}
	return ret_list;
}

bool Spawn::HasSpawnGroup() {
	return spawn_group_list && spawn_group_list->size() > 0;
}

bool Spawn::IsInSpawnGroup(Spawn* spawn) {
	bool ret = false;
	if (HasSpawnGroup() && spawn) {
		vector<Spawn*>::iterator itr;
		for (itr = spawn_group_list->begin(); itr != spawn_group_list->end(); itr++) {
			if ((*itr) == spawn) {
				ret = true;
				break;
			}
		}
	}
	return ret;
}

void Spawn::AddSpawnToGroup(Spawn* spawn){
	if(!spawn)
		return;
	if(!spawn_group_list){
		spawn_group_list = new vector<Spawn*>();
		spawn_group_list->push_back(this);
		safe_delete(MSpawnGroup);
		MSpawnGroup = new Mutex();
		MSpawnGroup->SetName("Spawn::MSpawnGroup");
	}
	vector<Spawn*>::iterator itr;
	MSpawnGroup->writelock(__FUNCTION__, __LINE__);
	for(itr = spawn_group_list->begin(); itr != spawn_group_list->end(); itr++){
		if((*itr) == spawn){
			MSpawnGroup->releasewritelock(__FUNCTION__, __LINE__);
			return;
		}
	}
	spawn_group_list->push_back(spawn);
	spawn->SetSpawnGroupList(spawn_group_list, MSpawnGroup);
	MSpawnGroup->releasewritelock(__FUNCTION__, __LINE__);
}


void Spawn::SetSpawnGroupList(vector<Spawn*>* list, Mutex* mutex){
	spawn_group_list = list;
	MSpawnGroup = mutex;
}

void Spawn::RemoveSpawnFromGroup(bool erase_all){
	SetSpawnGroupID(0);
	bool del = false;
	if(MSpawnGroup){
		MSpawnGroup->writelock(__FUNCTION__, __LINE__);
		if(spawn_group_list){
			vector<Spawn*>::iterator itr;
			Spawn* spawn = 0;
			if(spawn_group_list->size() == 1)
				erase_all = true;
			for(itr = spawn_group_list->begin(); itr != spawn_group_list->end(); itr++){
				spawn = *itr;
				if (spawn) {
					if(!erase_all){
						if(spawn == this){
							spawn_group_list->erase(itr);
							MSpawnGroup->releasewritelock(__FUNCTION__, __LINE__);
							spawn_group_list = 0;						
							MSpawnGroup = 0;
							return;
						}
					}
					else{
						if (spawn != this)
							spawn->SetSpawnGroupList(0, 0);
					}
				}
			}
			if (erase_all)
				spawn_group_list->clear();
			del = (spawn_group_list->size() == 0);
		}
		MSpawnGroup->releasewritelock(__FUNCTION__, __LINE__);
		if (del){
			safe_delete(MSpawnGroup);
			safe_delete(spawn_group_list);
		}
	}
}

void Spawn::SetSpawnGroupID(int32 id){
	group_id = id;
}

int32 Spawn::GetSpawnGroupID(){
	return group_id;
}

void Spawn::AddChangedZoneSpawn(){
	if(send_spawn_changes && GetZone())
		GetZone()->AddChangedSpawn(this);
}

void Spawn::RemoveSpawnAccess(Spawn* spawn) {
	if (allowed_access.count(spawn->GetID()) > 0) {
		allowed_access.erase(spawn->GetID());
		GetZone()->HidePrivateSpawn(this);
	}
}

void Spawn::SetFollowTarget(Spawn* spawn) {
	if (spawn && spawn != this) {
		m_followTarget = spawn->GetID();
	}
	else {
		m_followTarget = 0;
		if (following)
			following = false;
	}
}

void Spawn::AddTempVariable(string var, string val) {
	m_tempVariableTypes[var] = 5;
	m_tempVariables[var] = val;
}

void Spawn::AddTempVariable(string var, Spawn* val) {
	m_tempVariableTypes[var] = 1;
	m_tempVariableSpawn[var] = val->GetID();
}

void Spawn::AddTempVariable(string var, ZoneServer* val) {
	m_tempVariableTypes[var] = 2;
	m_tempVariableZone[var] = val;
}

void Spawn::AddTempVariable(string var, Item* val) {
	m_tempVariableTypes[var] = 3;
	m_tempVariableItem[var] = val;
}

void Spawn::AddTempVariable(string var, Quest* val) {
	m_tempVariableTypes[var] = 4;
	m_tempVariableQuest[var] = val;
}

string Spawn::GetTempVariable(string var) {
	string ret = "";

	if (m_tempVariables.count(var) > 0)
		ret = m_tempVariables[var];

	return ret;
}

Spawn* Spawn::GetTempVariableSpawn(string var) {
	Spawn* ret = 0;
	
	if (m_tempVariableSpawn.count(var) > 0)
		ret = GetZone()->GetSpawnByID(m_tempVariableSpawn[var]);

	return ret;
}

ZoneServer* Spawn::GetTempVariableZone(string var) {
	ZoneServer* ret = 0;

	if (m_tempVariableZone.count(var) > 0)
		ret = m_tempVariableZone[var];

	return ret;
}

Item* Spawn::GetTempVariableItem(string var) {
	Item* ret = 0;

	if (m_tempVariableItem.count(var) > 0)
		ret = m_tempVariableItem[var];

	return ret;
}

Quest* Spawn::GetTempVariableQuest(string var) {
	Quest* ret = 0;

	if (m_tempVariableQuest.count(var) > 0)
		ret = m_tempVariableQuest[var];

	return ret;
}

int8 Spawn::GetTempVariableType(string var) {
	int8 ret = 0;

	if (m_tempVariableTypes.count(var) > 0)
		ret = m_tempVariableTypes[var];

	return ret;
}

void Spawn::DeleteTempVariable(string var) {
	int8 type = GetTempVariableType(var);

	switch (type) {
	case 1:
		m_tempVariableSpawn.erase(var);
		break;
	case 2:
		m_tempVariableZone.erase(var);
		break;
	case 3:
		m_tempVariableItem.erase(var);
		break;
	case 4:
		m_tempVariableQuest.erase(var);
		break;
	case 5:
		m_tempVariables.erase(var);
		break;
	}

	m_tempVariableTypes.erase(var);
}

Spawn* Spawn::GetRunningTo() {
	return GetZone()->GetSpawnByID(running_to);
}

Spawn* Spawn::GetFollowTarget() {
	return GetZone()->GetSpawnByID(m_followTarget);
}

void Spawn::CopySpawnAppearance(Spawn* spawn){
	if (!spawn)
		return;

	//This function copies the appearace of the provided spawn to this one
	if (spawn->IsEntity() && IsEntity()){
		memcpy(&((Entity*)this)->features, &((Entity*)spawn)->features, sizeof(CharFeatures));
		memcpy(&((Entity*)this)->equipment, &((Entity*)spawn)->equipment, sizeof(EQ2_Equipment));
	}

	SetSize(spawn->GetSize());
	SetModelType(spawn->GetModelType());
}