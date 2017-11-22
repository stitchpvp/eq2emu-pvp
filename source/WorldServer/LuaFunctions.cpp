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
#include "LuaFunctions.h"
#include "Spawn.h"
#include "WorldDatabase.h"
#include "LuaInterface.h"
#include "../common/ConfigReader.h"
#include "client.h"
#include "World.h"
#include "Commands/Commands.h"
#include "races.h"
#include "classes.h"
#include "Variables.h"
#include "SpellProcess.h"
#include "../common/Log.h"
#include <math.h>
#include "HeroicOp/HeroicOp.h"
#include "RaceTypes/RaceTypes.h"
#include "ClientPacketFunctions.h"

extern WorldDatabase database;
extern LuaInterface* lua_interface;
extern ConfigReader configReader;
extern MasterQuestList master_quest_list;
extern MasterItemList master_item_list;
extern MasterSpellList master_spell_list;
extern World world;
extern Commands commands;
extern ZoneList zone_list;
extern Races races;
extern Classes classes;
extern Variables variables;
extern MasterSkillList master_skill_list;
extern MasterHeroicOPList master_ho_list;
extern MasterRaceTypeList race_types_list;
extern MasterLanguagesList master_languages_list;

int EQ2Emu_lua_PlayFlavor(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	string mp3_string = lua_interface->GetStringValue(state, 2);
	string text_string = lua_interface->GetStringValue(state, 3);
	string emote_string = lua_interface->GetStringValue(state, 4);
	int32 key1 = lua_interface->GetInt32Value(state, 5);
	int32 key2 = lua_interface->GetInt32Value(state, 6);
	Spawn* player = lua_interface->GetSpawn(state, 7);
	int8 language = lua_interface->GetInt8Value(state, 8);
	if(spawn){
		const char* mp3 = 0;
		const char* text = 0;
		const char* emote = 0;
		if(mp3_string.length() > 0)
			mp3 = mp3_string.c_str();
		if(text_string.length() > 0)
			text = text_string.c_str();
		if(emote_string.length() > 0)
			emote = emote_string.c_str();
		Client* client = 0;
		if(player && player->IsPlayer())
			client = spawn->GetZone()->GetClientBySpawn(player);
		if(client){
			if(static_cast<Player*>(player)->WasSentSpawn(spawn->GetID()) && !static_cast<Player*>(player)->WasSpawnRemoved(spawn))
				spawn->GetZone()->PlayFlavor(client, spawn, mp3, text, emote, key1, key2, language);
		}
		else
			spawn->GetZone()->PlayFlavor(spawn, mp3, text, emote, key1, key2, language);
	}
	return 0;
}

int EQ2Emu_lua_PlaySound(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	string sound_string = lua_interface->GetStringValue(state, 2);
	float x = lua_interface->GetFloatValue(state, 3);
	float y = lua_interface->GetFloatValue(state, 4);
	float z = lua_interface->GetFloatValue(state, 5);
	Spawn* player = lua_interface->GetSpawn(state, 6);
	if(spawn && sound_string.length() > 0){
		Client* client = 0;
		if(player && player->IsPlayer())
			client = spawn->GetZone()->GetClientBySpawn(player);
		if(client)
			spawn->GetZone()->PlaySoundFile(client, sound_string.c_str(), x, y, z);
		else
			spawn->GetZone()->PlaySoundFile(0, sound_string.c_str(), x, y, z);

	}
	return 0;
}
int EQ2Emu_lua_SetRequiredQuest(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int32 quest_id = lua_interface->GetInt32Value(state, 2);
	int32 quest_step = lua_interface->GetInt32Value(state, 3);
	bool private_spawn = (lua_interface->GetInt8Value(state, 4) == 1);
	bool continued_access = (lua_interface->GetInt8Value(state, 5) == 1);
	int16 flag_override = lua_interface->GetInt16Value(state, 6);

	if (!spawn){
		lua_interface->LogError("LUA SetRequiredQuest command error: spawn is not valid");
		return 0;
	}

	if(quest_id > 0){
		//Add this quest to the list of required quests for this spawn
		spawn->SetQuestsRequired(quest_id, quest_step);
		//If private spawn value set
		if(private_spawn){
			//Set the spawn to be private when not granted access through this quest
			spawn->AddAllowAccessSpawn(spawn);
			spawn->SetPrivateQuestSpawn(true);
		}
		//This value allows access after a quest step, or the whole quest has been completed
		if(continued_access)
			spawn->SetQuestsRequiredContinuedAccess(true);
		//This value will override vis_flags in the vis packet
		if(flag_override > 0)
			spawn->SetQuestsRequiredOverride(flag_override);
	}
	return 0;
}

int EQ2Emu_lua_SpawnSetByDistance(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	float max_distance = lua_interface->GetFloatValue(state, 2);
	string variable = lua_interface->GetStringValue(state, 3);
	string value = lua_interface->GetStringValue(state, 4);
	if(max_distance > 0 && spawn && value.length() > 0 && variable.length() > 0 && spawn->GetZone())
		spawn->GetZone()->SpawnSetByDistance(spawn, max_distance, variable, value); 
	return 0;
}

int EQ2Emu_lua_KillSpawn(lua_State* state) {
	if(!lua_interface)
		return 0;
	Spawn* dead = lua_interface->GetSpawn(state);
	Spawn* killer = lua_interface->GetSpawn(state, 2);
	bool send_packet = (lua_interface->GetInt8Value(state, 3) == 1);
	if(dead && dead->Alive() && dead->GetZone())
		dead->GetZone()->KillSpawn(dead, killer, send_packet);
	return 0;
} 

int EQ2Emu_lua_KillSpawnByDistance(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	float max_distance = lua_interface->GetFloatValue(state, 2);
	bool include_players = lua_interface->GetInt8Value(state, 3);
	bool send_packet = (lua_interface->GetInt8Value(state, 4) == 1);
	if(max_distance > 0 && spawn && spawn->GetZone())
		spawn->GetZone()->KillSpawnByDistance(spawn, max_distance, include_players, send_packet);
	return 0;
}

int EQ2Emu_lua_Despawn(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int32 delay = lua_interface->GetInt32Value(state, 2);
	if(spawn && spawn->GetZone())
		spawn->GetZone()->Despawn(spawn, delay);
	return 0;
}

int EQ2Emu_lua_SpawnSet(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	string variable = lua_interface->GetStringValue(state, 2);
	string value = lua_interface->GetStringValue(state, 3);
	int32 type = commands.GetSpawnSetType(variable);
	if(type != 0xFFFFFFFF && value.length() > 0 && spawn)
		commands.SetSpawnCommand(0, spawn, type, value.c_str());
	return 0;
}

int EQ2Emu_lua_GetSpawn(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int32 spawn_id = lua_interface->GetInt32Value(state, 2);
	if(spawn && spawn_id > 0){
		Spawn* closest_spawn = spawn->GetZone()->GetClosestSpawn(spawn, spawn_id);
		if(closest_spawn){
			lua_interface->SetSpawnValue(state, closest_spawn);
			return 1;
		}
	}
	return 0;
}

int EQ2Emu_lua_GetVariableValue(lua_State* state) {
	if (!lua_interface)
		return 0;
	string variable_name = lua_interface->GetStringValue(state);
	Variable* var = variables.FindVariable(variable_name);
	if (var) {
		lua_interface->SetStringValue(state, var->GetValue());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetCoinMessage(lua_State* state) {
	if (!lua_interface)
		return 0;
	int32 total_coins = lua_interface->GetInt32Value(state);
	if (total_coins == 0) {
		lua_interface->SetStringValue(state, "0 copper");
		return 1;
	}
	char tmp[64] = {0};
	string message = "";
	int32 val = 0;
	if (total_coins >= 1000000) {
		val = total_coins / 1000000;
		total_coins -= 1000000 * val;
		sprintf(tmp, " %u Platinum", val);
		message.append(tmp);
		memset(tmp, 0, 64);
	}
	if (total_coins >= 10000) {
		val = total_coins / 10000;
		total_coins -= 10000 * val;
		sprintf(tmp, " %u Gold", val);
		message.append(tmp);
		memset(tmp, 0, 64);
	}
	if (total_coins >= 100) {
		val = total_coins / 100;
		total_coins -= 100 * val;
		sprintf(tmp, " %u Silver", val);
		message.append(tmp);
		memset(tmp, 0, 64);
	}
	if (total_coins > 0) {
		sprintf(tmp, " %u Copper", (int32)total_coins);
		message.append(tmp);
	}
	lua_interface->SetStringValue(state, message.c_str());
	return 1;
}

int EQ2Emu_lua_GetSpawnByGroupID(lua_State* state) {
	ZoneServer* zone = lua_interface->GetZone(state);
	int32 group_id = lua_interface->GetInt32Value(state, 2);
	Spawn* spawn = zone->GetSpawnGroup(group_id);
	if (zone && spawn) {
		lua_interface->SetSpawnValue(state, spawn);
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetSpawnByLocationID(lua_State* state) {
	ZoneServer* zone = lua_interface->GetZone(state);
	int32 location_id = lua_interface->GetInt32Value(state, 2);
	if (zone) {
		Spawn* spawn = zone->GetSpawnByLocationID(location_id);
		if (spawn) {
			lua_interface->SetSpawnValue(state, spawn);
			return 1;
		}
	}
	return 0;
}

int EQ2Emu_lua_GetSpawnID(lua_State* state) {
	Spawn* spawn = lua_interface->GetSpawn(state);
	if (spawn) {
		lua_interface->SetInt32Value(state, spawn->GetDatabaseID());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetSpawnGroupID(lua_State* state) {
	Spawn* spawn = lua_interface->GetSpawn(state);
	if (spawn) {
		lua_interface->SetInt32Value(state, spawn->GetSpawnGroupID());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetSpawnLocationID(lua_State* state) {
	Spawn* spawn = lua_interface->GetSpawn(state);
	if (spawn) {
		lua_interface->SetInt32Value(state, spawn->GetSpawnLocationID());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetSpawnLocationPlacementID(lua_State* state) {
	Spawn* spawn = lua_interface->GetSpawn(state);
	if (spawn) {
		lua_interface->SetInt32Value(state, spawn->GetSpawnLocationPlacementID());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetFactionAmount(lua_State* state) {
	Player* player = static_cast<Player*>(lua_interface->GetSpawn(state));
	int32 faction_id = lua_interface->GetInt32Value(state, 2);
	if (player && player->IsPlayer() && faction_id > 0) {
		lua_interface->SetInt32Value(state, player->GetFactions()->GetFactionValue(faction_id));
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetGender(lua_State* state) {
	Spawn* spawn = lua_interface->GetSpawn(state);
	if (spawn) {
		lua_interface->SetInt32Value(state, spawn->GetGender());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetTarget(lua_State* state) {
	Spawn* spawn = lua_interface->GetSpawn(state);
	if (spawn && spawn->IsEntity() && static_cast<Entity*>(spawn)->GetTarget()) {
		lua_interface->SetSpawnValue(state, static_cast<Entity*>(spawn)->GetTarget());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_PlayVoice(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	string mp3_string = lua_interface->GetStringValue(state, 2);
	int32 key1 = lua_interface->GetInt32Value(state, 3);
	int32 key2 = lua_interface->GetInt32Value(state, 4);
	Spawn* player = lua_interface->GetSpawn(state, 5);
	if(spawn && mp3_string.length() > 0){
		Client* client = 0;
		if(player && player->IsPlayer())
			client = spawn->GetZone()->GetClientBySpawn(player);
		if(client){
			if(static_cast<Player*>(player)->WasSentSpawn(spawn->GetID()) && !static_cast<Player*>(player)->WasSpawnRemoved(spawn))
				spawn->GetZone()->PlayVoice(client, spawn, mp3_string.c_str(), key1, key2);
		}
		else
			spawn->GetZone()->PlayVoice(spawn, mp3_string.c_str(), key1, key2);
	}
	return 0;
}

int EQ2Emu_lua_GetCurrentZoneSafeLocation(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		lua_interface->SetFloatValue(state, spawn->GetZone()->GetSafeX());
		lua_interface->SetFloatValue(state, spawn->GetZone()->GetSafeY());
		lua_interface->SetFloatValue(state, spawn->GetZone()->GetSafeZ());
		return 3;
	}
	return 0;
}

int EQ2Emu_lua_AddLootItem(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn && spawn->IsEntity()){
		int32 item_id = lua_interface->GetInt32Value(state, 2);
		int16 charges = lua_interface->GetInt16Value(state, 3);
		if(charges == 0)
			charges = 1;
		static_cast<Entity*>(spawn)->AddLootItem(item_id, charges);
	}
	return 0;
}

int EQ2Emu_lua_RemoveLootItem(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn && spawn->IsEntity()){
		int32 item_id = lua_interface->GetInt32Value(state, 2);
		static_cast<Entity*>(spawn)->LootItem(item_id);
	}
	return 0;
}

int EQ2Emu_lua_AddLootCoin(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn && spawn->IsEntity()){
		int32 val = lua_interface->GetInt32Value(state, 2);
		static_cast<Entity*>(spawn)->AddLootCoins(val);
	}
	return 0;
}

int EQ2Emu_lua_GiveLoot(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* entity = lua_interface->GetSpawn(state);
	Spawn* player = lua_interface->GetSpawn(state, 2);
	if(entity && entity->IsEntity() && player && player->IsPlayer()){
		int32 coins = lua_interface->GetInt32Value(state, 3);
		vector<Item*>* items = 0;
		int i=0;
		int32 item_id = 0;
		while((item_id = lua_interface->GetInt32Value(state, 4+i))){
			if(items == 0)
				items = new vector<Item*>;
			if(master_item_list.GetItem(item_id))
				items->push_back(master_item_list.GetItem(item_id));
			i++;
		}
		Client* client = 0;
		client = entity->GetZone()->GetClientBySpawn(player);
		if(client){
			static_cast<Player*>(player)->AddPendingLootItems(entity->GetID(), items);
			client->Loot(coins, static_cast<Player*>(player)->GetPendingLootItems(entity->GetID()), static_cast<Entity*>(entity));
		}
		safe_delete(items);
	}
	return 0;
}

int EQ2Emu_lua_HasPendingLootItem(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* entity = lua_interface->GetSpawn(state);
	Spawn* player = lua_interface->GetSpawn(state, 2);
	int32 item_id = lua_interface->GetInt32Value(state, 3);
	if(entity && entity->IsEntity() && player && player->IsPlayer() && item_id > 0){
		lua_interface->SetBooleanValue(state, static_cast<Player*>(player)->HasPendingLootItem(entity->GetID(), item_id));
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_HasPendingLoot(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* entity = lua_interface->GetSpawn(state);
	Spawn* player = lua_interface->GetSpawn(state, 2);
	if(entity && entity->IsEntity() && player && player->IsPlayer()){
		lua_interface->SetBooleanValue(state, static_cast<Player*>(player)->HasPendingLootItems(entity->GetID()));
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_CreateConversation(lua_State* state){
	if(!lua_interface)
		return 0;
	vector<ConversationOption>* conversation = new vector<ConversationOption>();
	lua_interface->SetConversationValue(state, conversation);
	return 1;
}

int EQ2Emu_lua_AddConversationOption(lua_State* state){
	if(!lua_interface)
		return 0;
	vector<ConversationOption>* conversation = lua_interface->GetConversation(state);
	if(conversation){
		ConversationOption conv_option;
		conv_option.option = lua_interface->GetStringValue(state, 2);
		conv_option.function = lua_interface->GetStringValue(state, 3);
		if(conv_option.option.length() > 0)
			conversation->push_back(conv_option);
	}
	return 0;
}

int EQ2Emu_lua_CloseConversation(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* npc = lua_interface->GetSpawn(state);
	Spawn* player = lua_interface->GetSpawn(state, 2);
	if(npc && player && player->IsPlayer() && player->GetZone()){
		Client* client = player->GetZone()->GetClientBySpawn(player);
		if(client){
			int32 conversation_id = client->GetConversationID(npc, 0);
			client->CloseDialog(conversation_id);
		}
	}
	return 0;
}

int EQ2Emu_lua_CloseItemConversation(lua_State* state){
	if(!lua_interface)
		return 0;
	Item* item = lua_interface->GetItem(state);
	Spawn* player = lua_interface->GetSpawn(state, 2);
	if(item && player && player->IsPlayer() && player->GetZone()){
		Client* client = player->GetZone()->GetClientBySpawn(player);
		if(client){
			int32 conversation_id = client->GetConversationID(0, item);
			client->CloseDialog(conversation_id);
		}
	}
	return 0;
}

int EQ2Emu_lua_StartDialogConversation(lua_State* state) {
	if (!lua_interface)
		return 0;
	vector<ConversationOption>* conversation = lua_interface->GetConversation(state);
	Spawn* spawn = 0;
	Item* item = 0;
	int8 type = lua_interface->GetInt8Value(state, 2);
	if (type == 1 || type == 3)
		spawn = lua_interface->GetSpawn(state, 3);
	else if (type == 2 || type == 4)
		item = lua_interface->GetItem(state, 3);
	Spawn* player = lua_interface->GetSpawn(state, 4);
	string text = lua_interface->GetStringValue(state, 5);
	string mp3 = lua_interface->GetStringValue(state, 6);
	int32 key1 = lua_interface->GetInt32Value(state, 7);
	int32 key2 = lua_interface->GetInt32Value(state, 8);
	if (conversation && text.length() > 0 && (spawn || item) && player && player->IsPlayer()) {
		Client* client = player->GetZone()->GetClientBySpawn(player);
		if (client) {
			if (spawn) {
				// Need to do this so the function works the same as it did before
				if (type == 1)
					type++;

				if (mp3.length() > 0)
					client->DisplayConversation(static_cast<Entity*>(spawn), type, conversation, const_cast<char*>(text.c_str()), mp3.c_str(), key1, key2);
				else
					client->DisplayConversation(static_cast<Entity*>(spawn), type, conversation, const_cast<char*>(text.c_str()));
			}
			else {
				if (mp3.length() > 0)
					client->DisplayConversation(item, conversation, const_cast<char*>(text.c_str()), type, mp3.c_str(), key1, key2);
				else
					client->DisplayConversation(item, conversation, const_cast<char*>(text.c_str()), type);
			}
		}
	}
	safe_delete(conversation);
	return 0;
}

/*int EQ2Emu_lua_StartItemConversation(lua_State* state){
	if(!lua_interface)
		return 0;
	vector<ConversationOption>* conversation = lua_interface->GetConversation(state);
	Item* item = lua_interface->GetItem(state, 2);
	Spawn* player = lua_interface->GetSpawn(state, 3);
	string text = lua_interface->GetStringValue(state, 4);
	string mp3 = lua_interface->GetStringValue(state, 5);
	int32 key1 = lua_interface->GetInt32Value(state, 6);
	int32 key2 = lua_interface->GetInt32Value(state, 7);
	if(conversation && text.length() > 0 && item && player && player->IsPlayer()){
		Client* client = player->GetZone()->GetClientBySpawn(player);
		if(client){
			if(mp3.length() > 0)
				client->DisplayConversation(item, conversation, (char*)text.c_str(), mp3.c_str(), key1, key2);
			else
				client->DisplayConversation(item, conversation, (char*)text.c_str());
		}
		safe_delete(conversation);
	}
	return 0;
}*/

int EQ2Emu_lua_StartConversation(lua_State* state){
	if(!lua_interface)
		return 0;
	vector<ConversationOption>* conversation = lua_interface->GetConversation(state);
	Spawn* npc = lua_interface->GetSpawn(state, 2);
	Spawn* player = lua_interface->GetSpawn(state, 3);
	string text = lua_interface->GetStringValue(state, 4);
	string mp3 = lua_interface->GetStringValue(state, 5);
	int32 key1 = lua_interface->GetInt32Value(state, 6);
	int32 key2 = lua_interface->GetInt32Value(state, 7);
	if(conversation && text.length() > 0 && npc && npc->IsEntity() && player && player->IsPlayer()){
		Client* client = npc->GetZone()->GetClientBySpawn(player);
		if(mp3.length() > 0)
			client->DisplayConversation(static_cast<Entity*>(npc), 1, conversation, text.c_str(), mp3.c_str(), key1, key2);
		else
			client->DisplayConversation(static_cast<Entity*>(npc), 1, conversation, text.c_str());
		safe_delete(conversation);
	}
	return 0;
}

int EQ2Emu_lua_SetPlayerProximityFunction(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	float distance = lua_interface->GetFloatValue(state, 2);
	string in_range_function = lua_interface->GetStringValue(state, 3);
	string leaving_range_function = lua_interface->GetStringValue(state, 4);
	if(spawn && !spawn->IsPlayer() && distance > 0 && in_range_function.length() > 0)
		spawn->GetZone()->AddPlayerProximity(spawn, distance, in_range_function, leaving_range_function);
	return 0;
}

int EQ2Emu_lua_SetLocationProximityFunction(lua_State* state) {
	ZoneServer* zone = lua_interface->GetZone(state);
	float x = lua_interface->GetFloatValue(state, 2);
	float y = lua_interface->GetFloatValue(state, 3);
	float z = lua_interface->GetFloatValue(state, 4);
	float max_variation = lua_interface->GetFloatValue(state, 5);
	string in_range_function = lua_interface->GetStringValue(state, 6);
	string leaving_range_function = lua_interface->GetStringValue(state, 7);
	if (zone && in_range_function.length() > 0)
		zone->AddLocationProximity(x, y, z, max_variation, in_range_function, leaving_range_function);
	return 0;
}

int EQ2Emu_lua_SetLootCoin(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn && spawn->IsEntity()){
		int32 val = lua_interface->GetInt32Value(state, 2);
		static_cast<Entity*>(spawn)->SetLootCoins(val);
	}
	return 0;
}

int EQ2Emu_lua_GetLootCoin(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn && spawn->IsEntity()){
		lua_interface->SetInt32Value(state, static_cast<Entity*>(spawn)->GetLootCoins());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_MovementLoopAdd(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	float x = lua_interface->GetFloatValue(state, 2);
	float y = lua_interface->GetFloatValue(state, 3);
	float z = lua_interface->GetFloatValue(state, 4);
	float speed = lua_interface->GetFloatValue(state, 5);
	int32 delay = lua_interface->GetInt32Value(state, 6); //this should be given as seconds, as it is converted to ms later
	string function = lua_interface->GetStringValue(state, 7);
	if(spawn){
		spawn->AddMovementLocation(x, y, z, speed, delay, function.c_str());
		spawn->GetZone()->AddMovementNPC(spawn);
	}
	lua_interface->ResetFunctionStack(state);
	return 0;
}

int EQ2Emu_lua_IsPlayer(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		lua_interface->SetBooleanValue(state, spawn->IsPlayer());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_FaceTarget(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	Spawn* target = lua_interface->GetSpawn(state, 2);
	if(spawn && target){
		if(spawn->IsEntity())
			static_cast<Entity*>(spawn)->FaceTarget(target);
	}
	lua_interface->ResetFunctionStack(state);
	return 0;
}

int EQ2Emu_lua_MoveToLocation(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	float x = lua_interface->GetFloatValue(state, 2);
	float y = lua_interface->GetFloatValue(state, 3);
	float z = lua_interface->GetFloatValue(state, 4);
	float speed = lua_interface->GetFloatValue(state, 5);
	string lua_function = lua_interface->GetStringValue(state, 6);

	if (speed == 0)
		speed = spawn->GetSpeed();

	if(spawn){
		spawn->AddRunningLocation(x, y, z, speed, 0.0f, true, true, lua_function);
	}
	lua_interface->ResetFunctionStack(state);
	return 0;
}

int EQ2Emu_lua_Say(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	string message = lua_interface->GetStringValue(state, 2);
	Spawn* player = lua_interface->GetSpawn(state, 3);
	int32 language = lua_interface->GetInt32Value(state, 4);
	if(spawn && message.length() > 0){
		Client* client = 0;
		if(player && player->IsPlayer())
			client = spawn->GetZone()->GetClientBySpawn(player);
		if(client)
			spawn->GetZone()->HandleChatMessage(client, spawn, 0, CHANNEL_SAY, message.c_str(), 30, 0, true, language);
		else
			spawn->GetZone()->HandleChatMessage(spawn, 0, CHANNEL_SAY, message.c_str(), 30, 0, true, language);
	}
	lua_interface->ResetFunctionStack(state);
	return 0;
}

int EQ2Emu_lua_Shout(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	string message = lua_interface->GetStringValue(state, 2);
	Spawn* player = lua_interface->GetSpawn(state, 3);
	if(spawn && message.length() > 0){
		Client* client = 0;
		if(player && player->IsPlayer())
			client = spawn->GetZone()->GetClientBySpawn(player);
		if(client)
			spawn->GetZone()->HandleChatMessage(client, spawn, 0, CHANNEL_SHOUT, message.c_str(), 30);
		else
			spawn->GetZone()->HandleChatMessage(spawn, 0, CHANNEL_SHOUT, message.c_str(), 30);
	}
	lua_interface->ResetFunctionStack(state);
	return 0;
}

int EQ2Emu_lua_SayOOC(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	string message = lua_interface->GetStringValue(state, 2);
	Spawn* player = lua_interface->GetSpawn(state, 3);
	if(spawn && message.length() > 0){
		Client* client = 0;
		if(player && player->IsPlayer())
			client = spawn->GetZone()->GetClientBySpawn(player);
		if(client)
			spawn->GetZone()->HandleChatMessage(client, spawn, 0, CHANNEL_OOC, message.c_str(), 30);
		else
			spawn->GetZone()->HandleChatMessage(spawn, 0, CHANNEL_OOC, message.c_str(), 30);
	}
	lua_interface->ResetFunctionStack(state);
	return 0;
}

int EQ2Emu_lua_Emote(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	string message = lua_interface->GetStringValue(state, 2);
	Spawn* spawn2 = lua_interface->GetSpawn(state, 3);
	Spawn* player = lua_interface->GetSpawn(state, 4);
	char* to = 0;
	if(spawn2)
		to = spawn2->GetName();
	if(spawn && message.length() > 0){
		Client* client = 0;
		if(player && player->IsPlayer())
			client = spawn->GetZone()->GetClientBySpawn(player);
		if(client)
			spawn->GetZone()->HandleChatMessage(client, spawn, to, CHANNEL_EMOTE, message.c_str(), 30);
		else
			spawn->GetZone()->HandleChatMessage(spawn, to, CHANNEL_EMOTE, message.c_str(), 30);
	}
	lua_interface->ResetFunctionStack(state);
	return 0;
}

int EQ2Emu_lua_SpellHeal(lua_State* state){
   if(!lua_interface)
      return 0;

   LuaSpell* luaspell = lua_interface->GetCurrentSpell(state);

   if(!luaspell)
      return 0;

   Spawn* caster = luaspell->caster;
   Spawn* target = lua_interface->GetSpawn(state);
   string heal_type = lua_interface->GetStringValue(state, 2);
   int32 min_heal = lua_interface->GetInt32Value(state, 3);
   int32 max_heal = lua_interface->GetInt32Value(state, 4);
   int8 crit_mod = lua_interface->GetInt32Value(state, 5);
   bool no_calcs = lua_interface->GetInt32Value(state, 6) == 1;

   lua_interface->ResetFunctionStack(state);

   if(caster && caster->IsEntity()){
	  bool success = false;

      if (target) {
         float distance = caster->GetDistance(target, true);
         if (static_cast<Entity*>(caster)->SpellHeal(target, distance, luaspell, heal_type, min_heal, max_heal, crit_mod, no_calcs))
            success = true;
      }

      if (success) {
         if(caster->GetZone())
            caster->GetZone()->TriggerCharSheetTimer();
      }
   }
   return 0;
}


int EQ2Emu_lua_SummonItem(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int32 item_id = lua_interface->GetInt32Value(state, 2);
	bool send_messages = (lua_interface->GetInt8Value(state, 3) == 1);
	if (spawn && spawn->IsPlayer()){
		Client* client = spawn->GetZone()->GetClientBySpawn(spawn);
		if (client && item_id > 0){
			lua_interface->SetBooleanValue(state, client->AddItem(item_id, 1));
			if (send_messages) {
				Item* item = master_item_list.GetItem(item_id);
				if (item) {
					client->Message(CHANNEL_COLOR_YELLOW, "You receive \\aITEM %u 0:%s\\/a.", item->details.item_id, item->name.c_str());
					string popup_text = "You receive " + item->name;
					client->SendPopupMessage(10, popup_text.c_str(), "ui_harvested_normal", 3, 0xFF, 0xFF, 0xFF);
				}
			}
			return 1;
		}
	}
	lua_interface->SetBooleanValue(state, false);
	return 1;
}

int EQ2Emu_lua_RemoveItem(lua_State* state) {
	Spawn* spawn = lua_interface->GetSpawn(state);
	int32 item_id = lua_interface->GetInt32Value(state, 2);
	Client *client;

	if (spawn && spawn->IsPlayer() && item_id > 0) {
		if ((client = spawn->GetZone()->GetClientBySpawn(spawn))) {
			Item *item = client->GetPlayer()->item_list.GetItemFromID(item_id);
			if (item) {
				if (client->RemoveItem(item, 1)) {
					lua_interface->SetBooleanValue(state, true);
					return 1;
				}
			}
		}
	}

	lua_interface->SetBooleanValue(state, false);
	return 1;
}

int EQ2Emu_lua_HasItem(lua_State* state) {
	Spawn* player = lua_interface->GetSpawn(state);
	int32 item_id = lua_interface->GetInt32Value(state, 2);
	bool include_bank = lua_interface->GetInt8Value(state, 3);
	if (player && player->IsPlayer()) {
		bool hasItem = static_cast<Player*>(player)->item_list.HasItem(item_id, include_bank);
		if (!hasItem)
			hasItem = static_cast<Player*>(player)->GetEquipmentList()->HasItem(item_id);
		lua_interface->SetBooleanValue(state, hasItem);
		return 1;
	}
	lua_interface->SetBooleanValue(state, false);
	return 1;
}

int EQ2Emu_lua_Spawn(lua_State* state){
	if(!lua_interface)
		return 0;
	ZoneServer* zone = lua_interface->GetZone(state);
	int32 spawn_id = lua_interface->GetInt32Value(state, 2);
	bool restricted_npc = (lua_interface->GetInt8Value(state, 3) == 1);
	float x = lua_interface->GetFloatValue(state, 4);
	float y = lua_interface->GetFloatValue(state, 5);
	float z = lua_interface->GetFloatValue(state, 6);
	float heading = lua_interface->GetFloatValue(state, 7);
	if(zone && spawn_id > 0 && (x != 0 || y != 0 || z != 0)){
		Spawn* spawn = zone->GetSpawn(spawn_id);
		if(!spawn)
			lua_interface->LogError("LUA Spawn command error: Could not find spawn with id of %u.", spawn_id);
		else{
			spawn->SetX(x);
			spawn->SetY(y);
			spawn->SetZ(z);
			spawn->SetLocation(zone->GetClosestLocation(spawn));
			spawn->SetHeading(heading);
			if(restricted_npc)
				spawn->AddAllowAccessSpawn(spawn);
			zone->AddSpawn(spawn);
			const char* spawn_script = world.GetSpawnScript(spawn_id);	
			if(spawn_script && lua_interface->GetSpawnScript(spawn_script) != 0){
				spawn->SetSpawnScript(string(spawn_script));
				zone->CallSpawnScript(spawn, SPAWN_SCRIPT_SPAWN);
			}
			lua_interface->SetSpawnValue(state, spawn);
			return 1;
		}
	}
	else{
		string output = "Invalid paramaters to LUA Spawn command: \n";
		if(!zone)
			output = output.append("\t").append("Missing zone reference. \n");
		if(spawn_id == 0)
			output = output.append("\t").append("Missing spawn_id.");
		lua_interface->LogError("Error in EQ2Emu_lua_Zone - %s", output.c_str());
	}
	return 0;
}

int EQ2Emu_lua_GetZoneName(lua_State* state){
	if(!lua_interface)
		return 0;
	ZoneServer* zone = lua_interface->GetZone(state);
	if(zone){
		lua_interface->SetStringValue(state, zone->GetZoneName());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetZoneID(lua_State* state){
	if(!lua_interface)
		return 0;
	ZoneServer* zone = lua_interface->GetZone(state);
	if(zone){
		lua_interface->SetInt32Value(state, zone->GetZoneID());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetZone(lua_State* state){
	if(!lua_interface)
		return 0;
	int32 zone_id = lua_interface->GetInt32Value(state);
	ZoneServer* zone = 0;
	if(zone_id > 0)
		zone = zone_list.Get(zone_id);
	else{
		string zone_name = lua_interface->GetStringValue(state);
		if(zone_name.length() > 0){
			zone = zone_list.Get(zone_name.c_str());
		}
		else{
			Spawn* spawn = lua_interface->GetSpawn(state);
			if(spawn)
				zone = spawn->GetZone();
		}
	}
	if(zone){
		lua_interface->SetZoneValue(state, zone);
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_AddHate(lua_State* state) {
	Spawn* entity = lua_interface->GetSpawn(state);
	Spawn* npc = lua_interface->GetSpawn(state, 2);
	sint32 amount = lua_interface->GetSInt32Value(state, 3);
	bool send_packet = lua_interface->GetInt8Value(state, 4) == 1 ? true : false;
	LuaSpell* luaspell = lua_interface->GetCurrentSpell(state);
	if (entity && entity->IsEntity() && amount != 0) {
		if (luaspell && send_packet)
			entity->GetZone()->SendThreatPacket(entity, npc, amount, luaspell->spell->GetName());
		
		if (npc && npc->IsNPC() && npc->GetZone())
			static_cast<NPC*>(npc)->AddHate(static_cast<Entity*>(entity), amount);
	}
	return 0;
} 


int EQ2Emu_lua_Zone(lua_State* state){
	if(!lua_interface)
		return 0;
	ZoneServer* zone = lua_interface->GetZone(state);
	Spawn* player = lua_interface->GetSpawn(state, 2);
	Client* client = 0;
	if(player && player->IsPlayer())
		client = player->GetZone()->GetClientBySpawn(player);
	float x = lua_interface->GetFloatValue(state, 3);
	float y = lua_interface->GetFloatValue(state, 4);
	float z = lua_interface->GetFloatValue(state, 5);
	float heading = lua_interface->GetFloatValue(state, 6);

	LogWrite(LUA__DEBUG, 0, "LUA", "LUA Zone Request by Player: '%s' (%u)", player->GetName(), player->GetID());
	LogWrite(LUA__DEBUG, 5, "LUA", "\tTo Zone: '%s' (%u)", zone->GetZoneName(), zone->GetZoneID());
	
	if(zone && client){

		if ( !client->CheckZoneAccess(zone->GetZoneName()) )
		{
			LogWrite(LUA__WARNING, 0, "LUA", "CheckZoneAccess() FAILED! LUA Zone Request Denied!");
			return 0;
		}

		if(x != 0 || y != 0 || z != 0){
			LogWrite(LUA__DEBUG, 5, "LUA", "\tTo Coordinates: %2f, %2f, %2f, %2f", x, y, z, heading);
			player->SetX(x);
			player->SetY(y);
			player->SetZ(z);
			player->SetHeading(heading);
			client->Zone(zone->GetZoneName(), false);
		}
		else
			client->Zone(zone->GetZoneName());
	}
	else
		lua_interface->SimpleLogError("Error in EQ2Emu_lua_Zone: invalid zone or spawn input.");
	return 0;
}

int EQ2Emu_lua_AddSpawnAccess(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	Spawn* spawn2 = lua_interface->GetSpawn(state, 2);
	if(spawn && spawn2)
		spawn->AddAllowAccessSpawn(spawn2);
	return 0;
}

int EQ2Emu_lua_CastSpell(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* target = lua_interface->GetSpawn(state);
	int32 spell_id = lua_interface->GetInt32Value(state, 2);
	int8 spell_tier = lua_interface->GetInt8Value(state, 3);
	Spawn* caster = lua_interface->GetSpawn(state, 4);

	if (!target) {
		lua_interface->LogError("LUA CastSpell command error: target is not a valid spawn");
		return 0;
	}

	if (!target->IsEntity()) {
		lua_interface->LogError("LUA CastSpell command error: target (%s) is not an entity", target->GetName());
		return 0;
	}

	if (spell_id <= 0) {
		lua_interface->LogError("LUA CastSpell command error: spell id is not valid");
		return 0;
	}

	if (caster && !caster->IsEntity()) {
		lua_interface->LogError("LUA CastSpell command error: caster (%s) is not an entity", caster->GetName());
		return 0;
	}

	if (spell_tier == 0)
		spell_tier = 1;

	if (!caster)
		caster = target;

	target->GetZone()->ProcessSpell(master_spell_list.GetSpell(spell_id, spell_tier), static_cast<Entity*>(caster), static_cast<Entity*>(target));
	return 0;
}

int EQ2Emu_lua_SpellDamage(lua_State* state){
	if(!lua_interface)
		return 0;

	Spawn* target = lua_interface->GetSpawn(state);
	LuaSpell* luaspell = lua_interface->GetCurrentSpell(state);

	if(!luaspell)
		return 0;

	Spawn* caster = luaspell->caster;
	sint32 type = lua_interface->GetSInt32Value(state, 2);
	int32 min_damage = lua_interface->GetInt32Value(state, 3);
	int32 max_damage = lua_interface->GetInt32Value(state, 4);
	int8 crit_mod = lua_interface->GetInt32Value(state, 5);
	bool no_calcs = lua_interface->GetInt32Value(state, 6) == 1;

	lua_interface->ResetFunctionStack(state);

	if(caster && caster->IsEntity()){		
		if (target) {
			float distance = caster->GetDistance(target, true);
			static_cast<Entity*>(caster)->SpellAttack(target, distance, luaspell, type, min_damage, max_damage, crit_mod, no_calcs);
		}
	}

	return 0;
}

int EQ2Emu_lua_ModifyPower(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	sint32 value = lua_interface->GetSInt32Value(state, 2);
	lua_interface->ResetFunctionStack(state);
	if(spawn && value != 0){
		if (spawn->GetPower() + value > spawn->GetTotalPower())
			spawn->SetPower(spawn->GetTotalPower());
		else
			spawn->SetPower(spawn->GetPower() + value);
	}
	return 0;
}
int EQ2Emu_lua_ModifyHP(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	sint32 value = lua_interface->GetSInt32Value(state, 2);
	lua_interface->ResetFunctionStack(state);
	if(spawn && value != 0){
		if (spawn->GetHP() + value > spawn->GetTotalHP())
			spawn->SetHP(spawn->GetTotalHP());
		else
			spawn->SetHP(spawn->GetHP() + value);
	}
	return 0;
}
int EQ2Emu_lua_ModifyMaxPower(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	sint32 value = lua_interface->GetSInt32Value(state, 2);
	lua_interface->ResetFunctionStack(state);
	if(spawn && value != 0){
		spawn->SetPower(spawn->GetPower() + value);
		if(value > spawn->GetTotalHPBase())
			spawn->SetTotalPower(spawn->GetTotalPower() + value);
	}
	return 0;
}
int EQ2Emu_lua_ModifyMaxHP(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	sint32 value = lua_interface->GetSInt32Value(state, 2);
	lua_interface->ResetFunctionStack(state);
	if(spawn && value != 0){
		spawn->SetHP(spawn->GetHP() + value);
		if(value > spawn->GetTotalHPBase())
			spawn->SetTotalHP(spawn->GetTotalHP() + value);
	}
	return 0;
}
int EQ2Emu_lua_SetCurrentHP(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	sint32 value = lua_interface->GetSInt32Value(state, 2);
	lua_interface->ResetFunctionStack(state);
	if(spawn && value > 0){
		spawn->SetHP(value);
		if(value > spawn->GetTotalHPBase())
			spawn->SetTotalHP(value);
	}
	return 0;
}
int EQ2Emu_lua_SetMaxHP(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	sint32 value = lua_interface->GetSInt32Value(state, 2);
	lua_interface->ResetFunctionStack(state);
	if (spawn && spawn->IsEntity() && value > 0)
		static_cast<Entity*>(spawn)->AddSpellBonus(0, ITEM_STAT_HEALTH, value - spawn->GetTotalHP());
	return 0;
}

int EQ2Emu_lua_SetMaxHPBase(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	sint32 value = lua_interface->GetSInt32Value(state, 2);
	lua_interface->ResetFunctionStack(state);
	if(spawn && spawn->IsEntity() && value > 0)
		static_cast<Entity*>(spawn)->SetTotalHPBase(value);
	return 0;
}

int EQ2Emu_lua_SetCurrentPower(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	sint32 value = lua_interface->GetSInt32Value(state, 2);
	lua_interface->ResetFunctionStack(state);
	if(spawn && value > 0){
		spawn->SetPower(value);
		if(value > spawn->GetTotalPowerBase())
			spawn->SetTotalPower(value);
	}
	return 0;
}
int EQ2Emu_lua_SetMaxPower(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	sint32 value = lua_interface->GetSInt32Value(state, 2);
	lua_interface->ResetFunctionStack(state);
	if(spawn && spawn->IsEntity() && value > 0)
		static_cast<Entity*>(spawn)->AddSpellBonus(0, ITEM_STAT_POWER, value - spawn->GetTotalPower());
	return 0;
}
int EQ2Emu_lua_SetMaxPowerBase(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	sint32 value = lua_interface->GetSInt32Value(state, 2);
	lua_interface->ResetFunctionStack(state);
	if(spawn && spawn->IsEntity() && value > 0)
		static_cast<Entity*>(spawn)->SetTotalPowerBase(value);
	return 0;
}
int EQ2Emu_lua_SetPosition(lua_State* state){
	if(!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	float x = lua_interface->GetFloatValue(state, 2);
	float y = lua_interface->GetFloatValue(state, 3);
	float z = lua_interface->GetFloatValue(state, 4);
	float heading = lua_interface->GetFloatValue(state, 5);
	bool should_zone = lua_interface->GetBooleanValue(state, 6);

	lua_interface->ResetFunctionStack(state);

	if(spawn){
		spawn->SetX(x);
		spawn->SetY(y);
		spawn->SetZ(z);

		if (heading != 0)
			spawn->SetHeading(heading);

		spawn->SetSpawnOrigX(spawn->GetX());
		spawn->SetSpawnOrigY(spawn->GetY());
		spawn->SetSpawnOrigZ(spawn->GetZ());
		spawn->SetSpawnOrigHeading(spawn->GetHeading());

		if(spawn->IsPlayer()){
			Client* client = spawn->GetZone()->GetClientBySpawn(spawn);

			if (client) {
				if (should_zone) {
					PacketStruct* packet = configReader.getStruct("WS_TeleportWithinZone", client->GetVersion());
					if (packet)
					{
						packet->setDataByName("x", x);
						packet->setDataByName("y", y);
						packet->setDataByName("z", z);
						client->QueuePacket(packet->serialize());
					}
					safe_delete(packet);

					client->GetCurrentZone()->RemoveSpawnFromClient(spawn);
				} else {
					EQ2Packet* packet = client->GetPlayer()->Move(x, y, z, client->GetVersion(), (heading == 0 ? -1.0f : (heading + 180.0f)));
					client->QueuePacket(packet);
					safe_delete(packet);
				}
			}
		}
		
	}
	return 0;
}
int EQ2Emu_lua_SetHeading(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	float value = lua_interface->GetFloatValue(state, 2);
	lua_interface->ResetFunctionStack(state);
	if (spawn) {
		spawn->SetHeading(value);
		if (spawn->IsPlayer()) {
			Client* client = spawn->GetZone()->GetClientBySpawn(spawn);
			if (client) {
				EQ2Packet* packet = client->GetPlayer()->Move(spawn->GetX(), spawn->GetY(), spawn->GetZ(), client->GetVersion(), value + 180.0f);
				client->QueuePacket(packet);
			}
		}
	}
	return 0;
}
int EQ2Emu_lua_SetModelType(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int16 value = lua_interface->GetInt16Value(state, 2);
	lua_interface->ResetFunctionStack(state);
	if(spawn)
		spawn->SetModelType(value);
	return 0;
}
int EQ2Emu_lua_SetAdventureClass(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int8 value = lua_interface->GetInt8Value(state, 2);
	lua_interface->ResetFunctionStack(state);
	if(spawn){		
		if(spawn->IsPlayer())
			static_cast<Player*>(spawn)->SetPlayerAdventureClass(value);
		else
			spawn->SetAdventureClass(value);
	}
	return 0;
}
int EQ2Emu_lua_SetTradeskillClass(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int8 value = lua_interface->GetInt8Value(state, 2);
	lua_interface->ResetFunctionStack(state);
	if(spawn) {
		spawn->SetTradeskillClass(value);
		if (spawn->IsEntity()) {
			static_cast<Entity*>(spawn)->GetInfoStruct()->tradeskill_class1 = classes.GetTSBaseClass(spawn->GetTradeskillClass());
			static_cast<Entity*>(spawn)->GetInfoStruct()->tradeskill_class2 = classes.GetSecondaryTSBaseClass(spawn->GetTradeskillClass());
			static_cast<Entity*>(spawn)->GetInfoStruct()->tradeskill_class3 = spawn->GetTradeskillClass();
		}
		if (spawn->IsPlayer())
			static_cast<Player*>(spawn)->SetCharSheetChanged(true);
	}
	return 0;
}
int EQ2Emu_lua_SetMount(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int16 value = lua_interface->GetInt16Value(state, 2);
	if(spawn && spawn->IsEntity()){
		static_cast<Entity*>(spawn)->SetMount(value);
		EQ2_Color color;
		color.red = 255;
		color.green = 255;
		color.blue = 255;
		static_cast<Entity*>(spawn)->SetMountColor(&color);
		static_cast<Entity*>(spawn)->SetMountSaddleColor(&color);
	}
	return 0;
}
int EQ2Emu_lua_SetMountColor(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	EQ2_Color mount_color;
	EQ2_Color saddle_color;
	mount_color.red = lua_interface->GetInt8Value(state, 2);
	mount_color.green = lua_interface->GetInt8Value(state, 3);
	mount_color.blue = lua_interface->GetInt8Value(state, 4);
	saddle_color.red = lua_interface->GetInt8Value(state, 5);
	saddle_color.green = lua_interface->GetInt8Value(state, 6);
	saddle_color.blue = lua_interface->GetInt8Value(state, 7);
	if(spawn && spawn->IsEntity()){
		static_cast<Entity*>(spawn)->SetMountColor(&mount_color);
		static_cast<Entity*>(spawn)->SetMountSaddleColor(&saddle_color);
	}
	return 0;
}
int EQ2Emu_lua_GetMount(lua_State* state){
	if(!lua_interface)
		return 0;
   Spawn* spawn = lua_interface->GetSpawn(state);
   if(spawn && spawn->IsEntity()){
      lua_interface->SetInt32Value(state, static_cast<Entity*>(spawn)->GetMount());
      return 1;
   }
   return 0;
}
int EQ2Emu_lua_GetRace(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn)
	{
		LogWrite(LUA__DEBUG, 0, "LUA", "%s - Race: %i", __FUNCTION__, spawn->GetRace());
		lua_interface->SetInt32Value(state, spawn->GetRace());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetRaceName(lua_State* state){
	if(!lua_interface)
		return 0;
   Spawn* spawn = lua_interface->GetSpawn(state);
   if(spawn) {
	   lua_interface->SetStringValue(state, races.GetRaceName(spawn->GetRace()));
	   return 1;
   }
   return 0;
}

int EQ2Emu_lua_GetClass(lua_State* state) {
	Spawn* spawn = lua_interface->GetSpawn(state);
	if (spawn) {
		lua_interface->SetInt32Value(state, spawn->GetAdventureClass());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetClassName(lua_State* state) {
	Spawn* spawn = lua_interface->GetSpawn(state);
	if (spawn) {
		lua_interface->SetStringValue(state, classes.GetClassName(spawn->GetAdventureClass()));
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_SetSpeed(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	float value = lua_interface->GetFloatValue(state, 2);
	lua_interface->ResetFunctionStack(state);
	if(spawn){
		spawn->SetSpeed(value);
		if(spawn->IsPlayer()){
			Client* client = spawn->GetZone()->GetClientBySpawn(spawn);
			if(client){
				PacketStruct* packet = configReader.getStruct("WS_SetControlGhost", client->GetVersion());
				if(packet){
					packet->setDataByName("spawn_id", 0xFFFFFFFF);
					packet->setDataByName("speed", value);
					packet->setDataByName("size", 0.51);
					EQ2Packet* app = packet->serialize();
					client->QueuePacket(app);
					safe_delete(packet);
				}
			}
		}
	}
	return 0;
}

int EQ2Emu_lua_AddSpellBonus(lua_State* state){
	if(!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	int16 type = lua_interface->GetInt16Value(state, 2);
	sint32 value = lua_interface->GetSInt32Value(state, 3);
	LuaSpell* luaspell = lua_interface->GetCurrentSpell(state);

	int64 class_req = 0;
	int32 class_id = 0;
	int32 i = 0;

	while((class_id = lua_interface->GetInt32Value(state, 4 + i))) {
		class_req += pow(2.0, (double)(class_id - 1));
		i++;
	}

	if (value != 0 && type >= 0) {
		if (luaspell) {
			if (!(luaspell->effect_bitmask & EFFECT_FLAG_SPELLBONUS))
				luaspell->effect_bitmask += EFFECT_FLAG_SPELLBONUS;
		}
			
		if (spawn && spawn->IsEntity()) {
			static_cast<Entity*>(spawn)->AddSpellBonus(luaspell, type, value, class_req);

			if (spawn->IsPlayer()) {
				if (static_cast<Player*>(spawn)->GetGroupMemberInfo())
					static_cast<Player*>(spawn)->UpdateGroupMemberInfo();

				static_cast<Player*>(spawn)->SetCharSheetChanged(true);
			}
		}
		else
			lua_interface->SimpleLogError("Unable to apply spell bonus in AddSpellBonus.");
	}
	else
		lua_interface->SimpleLogError("Invalid parameters for AddSpellBonus.");

	return 0;
}

int EQ2Emu_lua_AddSpawnSpellBonus(lua_State* state){
	if(!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	int16 type = lua_interface->GetInt16Value(state, 2);
	sint32 value = lua_interface->GetSInt32Value(state, 3);
	LuaSpell* luaspell = lua_interface->GetCurrentSpell(state);

	if (!spawn) {
		lua_interface->LogError("LUA AddSpawnSpellBonus command error: spawn is not valid");
		return 0;
	}

	if (!spawn->IsEntity()) {
		lua_interface->LogError("LUA AddSpawnSpellBonus command error: spawn is not an entity");
		return 0;
	}

	if (value == 0) {
		lua_interface->LogError("LUA AddSpawnSpellBonus command error: value must be set");
		return 0;
	}

	if (type < 0) {
		lua_interface->LogError("LUA AddSpawnSpellBonus command error: type is not valid");
		return 0;
	}

	if (!luaspell || !luaspell->spell) {
		lua_interface->LogError("LUA AddSpawnSpellBonus command error: can only be used in a spell script");
		return 0;
	}

	int32 class_req = 0;
	int32 class_id = 0;
	int32 i = 0;
	while((class_id = lua_interface->GetInt32Value(state, 4 + i))) {
		class_req += pow(2.0, (double)(class_id - 1));
		i++;
	}

	static_cast<Entity*>(spawn)->AddSpellBonus(luaspell, type, value, class_req);
	if (!(luaspell->effect_bitmask & EFFECT_FLAG_SPELLBONUS))
		luaspell->effect_bitmask += EFFECT_FLAG_SPELLBONUS;
	if(spawn->IsPlayer())
		static_cast<Player*>(spawn)->SetCharSheetChanged(true);

	return 0;
}

int EQ2Emu_lua_RemoveSpellBonus(lua_State* state){
	if(!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	LuaSpell* luaspell = lua_interface->GetCurrentSpell(state);

	if (spawn && spawn->IsEntity()) {
		static_cast<Entity*>(spawn)->RemoveSpellBonus(luaspell);

		if (spawn->IsPlayer())
			static_cast<Player*>(spawn)->SetCharSheetChanged(true);
	}

	return 0;
}

int EQ2Emu_lua_AddSkillBonus(lua_State* state) {
	if(!lua_interface)
		return 0;

	Spawn* target = lua_interface->GetSpawn(state);
	int32 skill_id = lua_interface->GetInt32Value(state, 2);
	float value = lua_interface->GetFloatValue(state, 3);
	LuaSpell* luaspell = lua_interface->GetCurrentSpell(state);

	if (value != 0) {
		if (luaspell && luaspell->spell && luaspell->caster) {
			int32 spell_id = luaspell->spell->GetSpellID();

			if (target && target->Alive()) {
				if (!(luaspell->effect_bitmask & EFFECT_FLAG_SKILLBONUS))
					luaspell->effect_bitmask += EFFECT_FLAG_SKILLBONUS;

				if (target->IsPlayer()) {
					static_cast<Player*>(target)->AddSkillBonus(spell_id, skill_id, value);

					Client* client = target->GetZone()->GetClientBySpawn(target);

					if (client) {
						EQ2Packet* packet = static_cast<Player*>(target)->GetSkills()->GetSkillPacket(client->GetVersion());
						if (packet)
							client->QueuePacket(packet);
					}
				}
				else if (target->IsNPC()) {
					static_cast<NPC*>(target)->AddSkillBonus(spell_id, skill_id, value);
				}
				else
					LogWrite(LUA__ERROR, 0, "LUA", "Error applying bonus buff on '%s'.  Not a NPC or player.", target->GetName());
			}
		}
	} else
		lua_interface->SimpleLogError("Invalid parameters for AddSkillBonus.");

	return 0;
}

int EQ2Emu_lua_RemoveSkillBonus(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	LuaSpell* luaspell = lua_interface->GetCurrentSpell(state);
	
	if (spawn) {
		if (spawn->IsPlayer()) {
			static_cast<Player*>(spawn)->RemoveSkillBonus(luaspell->spell->GetSpellID());

			Client* client = spawn->GetZone()->GetClientBySpawn(spawn);

			if (client) {
				EQ2Packet* packet = static_cast<Player*>(spawn)->GetSkills()->GetSkillPacket(client->GetVersion());
				if (packet)
					client->QueuePacket(packet);
			}
		}
		else if (spawn->IsNPC())
			static_cast<NPC*>(spawn)->RemoveSkillBonus(luaspell->spell->GetSpellID());
		else
			LogWrite(LUA__ERROR, 0, "LUA", "Error removing skill bonus on '%s'.  Not a NPC or player.", spawn->GetName());
	}

	return 0;
}

int EQ2Emu_lua_AddControlEffect(lua_State* state) {
	if(!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	int8 type = lua_interface->GetInt32Value(state, 2);

	LuaSpell* luaspell = lua_interface->GetCurrentSpell(state);
	
	if (spawn && spawn->IsEntity()) {
		if (type == CONTROL_EFFECT_TYPE_MEZ){
			static_cast<Entity*>(spawn)->AddMezSpell(luaspell);
			if (!(luaspell->effect_bitmask & EFFECT_FLAG_MEZ))
				luaspell->effect_bitmask += EFFECT_FLAG_MEZ;
		}
		else if (type == CONTROL_EFFECT_TYPE_STIFLE){
			static_cast<Entity*>(spawn)->AddStifleSpell(luaspell);
			if (!(luaspell->effect_bitmask & EFFECT_FLAG_STIFLE))
				luaspell->effect_bitmask += EFFECT_FLAG_STIFLE;
		}
		else if (type == CONTROL_EFFECT_TYPE_DAZE){
			static_cast<Entity*>(spawn)->AddDazeSpell(luaspell);
			if (!(luaspell->effect_bitmask & EFFECT_FLAG_DAZE))
				luaspell->effect_bitmask += EFFECT_FLAG_DAZE;
		}
		else if (type == CONTROL_EFFECT_TYPE_STUN){
			static_cast<Entity*>(spawn)->AddStunSpell(luaspell);
			if (!(luaspell->effect_bitmask & EFFECT_FLAG_STUN))
				luaspell->effect_bitmask += EFFECT_FLAG_STUN;
		}
		else if (type == CONTROL_EFFECT_TYPE_ROOT){
			static_cast<Entity*>(spawn)->AddRootSpell(luaspell);
			if (!(luaspell->effect_bitmask & EFFECT_FLAG_ROOT))
				luaspell->effect_bitmask += EFFECT_FLAG_ROOT;
		}
		else if (type == CONTROL_EFFECT_TYPE_FEAR){
			static_cast<Entity*>(spawn)->AddFearSpell(luaspell);
			if (!(luaspell->effect_bitmask & EFFECT_FLAG_FEAR))
				luaspell->effect_bitmask += EFFECT_FLAG_FEAR;
		}
		else if (type == CONTROL_EFFECT_TYPE_WALKUNDERWATER){
			static_cast<Entity*>(spawn)->AddWaterwalkSpell(luaspell);
			if (!(luaspell->effect_bitmask & EFFECT_FLAG_WATERWALK))
				luaspell->effect_bitmask += EFFECT_FLAG_WATERWALK;
		}
		else if (type == CONTROL_EFFECT_TYPE_JUMPUNDERWATER){
			static_cast<Entity*>(spawn)->AddWaterjumpSpell(luaspell);
			if (!(luaspell->effect_bitmask & EFFECT_FLAG_WATERJUMP))
				luaspell->effect_bitmask += EFFECT_FLAG_WATERJUMP;
		}
		else if (type == CONTROL_EFFECT_TYPE_SNARE){
			static_cast<Entity*>(spawn)->AddSnareSpell(luaspell);
			if (!(luaspell->effect_bitmask & EFFECT_FLAG_SNARE))
				luaspell->effect_bitmask += EFFECT_FLAG_SNARE;
		}
		else if (type == CONTROL_EFFECT_TYPE_FLIGHT){
			static_cast<Entity*>(spawn)->AddFlightSpell(luaspell);
			if (!(luaspell->effect_bitmask & EFFECT_FLAG_FLIGHT))
				luaspell->effect_bitmask += EFFECT_FLAG_FLIGHT;
		}
		else if (type == CONTROL_EFFECT_TYPE_GLIDE){
			static_cast<Entity*>(spawn)->AddGlideSpell(luaspell);
			if (!(luaspell->effect_bitmask & EFFECT_FLAG_GLIDE))
				luaspell->effect_bitmask += EFFECT_FLAG_GLIDE;
		}
		else if (type == CONTROL_EFFECT_TYPE_SAFEFALL){
			static_cast<Entity*>(spawn)->AddSafefallSpell(luaspell);
			if (!(luaspell->effect_bitmask & EFFECT_FLAG_SAFEFALL))
				luaspell->effect_bitmask += EFFECT_FLAG_SAFEFALL;
		}
		else
			lua_interface->LogError("Unhandled control effect type of %u.", type);
	}
	else
		lua_interface->LogError("Error applying control effect on non entity '%s'.", spawn->GetName());
	return 0;
}

int EQ2Emu_lua_RemoveControlEffect(lua_State* state) {
	if(!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	int8 type = lua_interface->GetInt8Value(state, 2);

	LuaSpell* luaspell = lua_interface->GetCurrentSpell(state);

	if (spawn && spawn->IsEntity()) {

		if (type == CONTROL_EFFECT_TYPE_MEZ)
			static_cast<Entity*>(spawn)->RemoveMezSpell(luaspell);
		else if (type == CONTROL_EFFECT_TYPE_STIFLE)
			static_cast<Entity*>(spawn)->RemoveStifleSpell(luaspell);
		else if (type == CONTROL_EFFECT_TYPE_DAZE)
			static_cast<Entity*>(spawn)->RemoveDazeSpell(luaspell);
		else if (type == CONTROL_EFFECT_TYPE_STUN)
			static_cast<Entity*>(spawn)->RemoveStunSpell(luaspell);
		else if (type == CONTROL_EFFECT_TYPE_ROOT)
			static_cast<Entity*>(spawn)->RemoveRootSpell(luaspell);
		else if (type == CONTROL_EFFECT_TYPE_FEAR)
			static_cast<Entity*>(spawn)->RemoveFearSpell(luaspell);
		else if (type == CONTROL_EFFECT_TYPE_WALKUNDERWATER)
			static_cast<Entity*>(spawn)->RemoveWaterwalkSpell(luaspell);
		else if (type == CONTROL_EFFECT_TYPE_JUMPUNDERWATER)
			static_cast<Entity*>(spawn)->RemoveWaterjumpSpell(luaspell);
		else if (type == CONTROL_EFFECT_TYPE_SNARE)
			static_cast<Entity*>(spawn)->RemoveSnareSpell(luaspell);
		else if (type == CONTROL_EFFECT_TYPE_FLIGHT)
			static_cast<Entity*>(spawn)->RemoveFlightSpell(luaspell);
		else if (type == CONTROL_EFFECT_TYPE_GLIDE)
			static_cast<Entity*>(spawn)->RemoveGlideSpell(luaspell);
		else if (type == CONTROL_EFFECT_TYPE_SAFEFALL)
			static_cast<Entity*>(spawn)->RemoveSafefallSpell(luaspell);
		else
			lua_interface->LogError("Unhandled control effect type of %u.", type);
	}
	return 0;
}

int EQ2Emu_lua_SetIntBase(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int16 value = lua_interface->GetInt16Value(state, 2);
	if(spawn && spawn->IsEntity()){
		static_cast<Entity*>(spawn)->GetInfoStruct()->intel_base = value;
		if(spawn->IsPlayer())
			static_cast<Player*>(spawn)->SetCharSheetChanged(true);
	}
	return 0;
}

int EQ2Emu_lua_SetAgiBase(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int16 value = lua_interface->GetInt16Value(state, 2);
	if(spawn && spawn->IsEntity()){
		static_cast<Entity*>(spawn)->GetInfoStruct()->agi_base = value;
		if(spawn->IsPlayer())
			static_cast<Player*>(spawn)->SetCharSheetChanged(true);
	}
	return 0;
}

int EQ2Emu_lua_SetWisBase(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int16 value = lua_interface->GetInt16Value(state, 2);
	if(spawn && spawn->IsEntity()){
		static_cast<Entity*>(spawn)->GetInfoStruct()->wis_base = value;
		if(spawn->IsPlayer())
			static_cast<Player*>(spawn)->SetCharSheetChanged(true);
	}
	return 0;
}

int EQ2Emu_lua_SetStaBase(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int16 value = lua_interface->GetInt16Value(state, 2);
	if(spawn && spawn->IsEntity()){
		static_cast<Entity*>(spawn)->GetInfoStruct()->sta_base = value;
		if(spawn->IsPlayer())
			static_cast<Player*>(spawn)->SetCharSheetChanged(true);
	}
	return 0;
}

int EQ2Emu_lua_SetStrBase(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int16 value = lua_interface->GetInt16Value(state, 2);
	if(spawn && spawn->IsEntity()){
		static_cast<Entity*>(spawn)->GetInfoStruct()->str_base = value;
		if(spawn->IsPlayer())
			static_cast<Player*>(spawn)->SetCharSheetChanged(true);
	}
	return 0;
}

int EQ2Emu_lua_SetInt(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	sint32 value = lua_interface->GetSInt32Value(state, 2);
	if(spawn && spawn->IsEntity()){
		static_cast<Entity*>(spawn)->AddSpellBonus(0, ITEM_STAT_INT, value);
		if(spawn->IsPlayer())
			static_cast<Player*>(spawn)->SetCharSheetChanged(true);
	}
	return 0;
}

int EQ2Emu_lua_SetWis(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	sint32 value = lua_interface->GetSInt32Value(state, 2);
	if(spawn && spawn->IsEntity()){
		static_cast<Entity*>(spawn)->AddSpellBonus(0, ITEM_STAT_WIS, value);
		if(spawn->IsPlayer())
			static_cast<Player*>(spawn)->SetCharSheetChanged(true);
	}
	return 0;
}
int EQ2Emu_lua_SetSta(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	sint32 value = lua_interface->GetSInt32Value(state, 2);
	if(spawn && spawn->IsEntity()){
		static_cast<Entity*>(spawn)->AddSpellBonus(0, ITEM_STAT_STA, value);
		if(spawn->IsPlayer())
			static_cast<Player*>(spawn)->SetCharSheetChanged(true);
	}
	return 0;
}
int EQ2Emu_lua_SetStr(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	sint32 value = lua_interface->GetSInt32Value(state, 2);
	if(spawn && spawn->IsEntity()){
		static_cast<Entity*>(spawn)->AddSpellBonus(0, ITEM_STAT_STR, value);
		if(spawn->IsPlayer())
			static_cast<Player*>(spawn)->SetCharSheetChanged(true);
	}
	return 0;
}
int EQ2Emu_lua_SetAgi(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	sint32 value = lua_interface->GetSInt32Value(state, 2);
	if(spawn && spawn->IsEntity()){
		static_cast<Entity*>(spawn)->AddSpellBonus(0, ITEM_STAT_AGI, value);
		if(spawn->IsPlayer())
			static_cast<Player*>(spawn)->SetCharSheetChanged(true);
	}
	return 0;
}

int EQ2Emu_lua_GetCurrentHP(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		lua_interface->SetInt32Value(state, spawn->GetHP());
		return 1;
	}
	return 0;
}
int EQ2Emu_lua_GetMaxHP(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		lua_interface->SetInt32Value(state, spawn->GetTotalHP());
		return 1;
	}
	return 0;
}
int EQ2Emu_lua_GetMaxHPBase(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		lua_interface->SetInt32Value(state, spawn->GetTotalHPBase());
		return 1;
	}
	return 0;
}
int EQ2Emu_lua_GetName(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		lua_interface->SetStringValue(state, spawn->GetName());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetLevel(lua_State* state) {
	Spawn* spawn = lua_interface->GetSpawn(state);
	if (spawn) {
		lua_interface->SetInt32Value(state, spawn->GetLevel());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetCurrentPower(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		lua_interface->SetInt32Value(state, spawn->GetPower());
		return 1;
	}
	return 0;
}
int EQ2Emu_lua_GetMaxPower(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		lua_interface->SetInt32Value(state, spawn->GetTotalPower());
		return 1;
	}
	return 0;
}
int EQ2Emu_lua_GetMaxPowerBase(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		lua_interface->SetInt32Value(state, spawn->GetTotalPowerBase());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetDistance(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	Spawn* spawn2 = lua_interface->GetSpawn(state, 2);
	bool include_radius = lua_interface->GetInt8Value(state, 3) == 1;
	if(spawn && spawn2){
		float distance = spawn->GetDistance(spawn2);
		if (include_radius) {
			distance -= spawn->appearance.pos.collision_radius / 10;
			distance -= spawn2->appearance.pos.collision_radius / 10;
		}

		lua_interface->SetFloatValue(state, distance);
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetX(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		lua_interface->SetFloatValue(state, spawn->GetX());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetY(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		lua_interface->SetFloatValue(state, spawn->GetY());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetZ(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		lua_interface->SetFloatValue(state, spawn->GetZ());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetHeading(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		lua_interface->SetFloatValue(state, spawn->GetHeading());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetModelType(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		lua_interface->SetInt32Value(state, spawn->GetModelType());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetSpeed(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		lua_interface->SetFloatValue(state, spawn->GetSpeed());
		return 1;
	}
	return 0;
}
int EQ2Emu_lua_HasMoved(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn && spawn->IsEntity()){
		lua_interface->SetBooleanValue(state, static_cast<Entity*>(spawn)->HasMoved(false));
		return 1;
	}
	return 0;
}
int EQ2Emu_lua_GetInt(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn && spawn->IsEntity()){
		lua_interface->SetInt32Value(state, static_cast<Entity*>(spawn)->GetInt());
		return 1;
	}
	return 0;
}
int EQ2Emu_lua_GetWis(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn && spawn->IsEntity()){
		lua_interface->SetInt32Value(state, static_cast<Entity*>(spawn)->GetWis());
		return 1;
	}
	return 0;
}
int EQ2Emu_lua_GetSta(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn && spawn->IsEntity()){
		lua_interface->SetInt32Value(state, static_cast<Entity*>(spawn)->GetSta());
		return 1;
	}
	return 0;
}
int EQ2Emu_lua_GetStr(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn && spawn->IsEntity()){
		lua_interface->SetInt32Value(state, static_cast<Entity*>(spawn)->GetStr());
		return 1;
	}
	return 0;
}
int EQ2Emu_lua_GetAgi(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn && spawn->IsEntity()){
		lua_interface->SetInt32Value(state, static_cast<Entity*>(spawn)->GetAgi());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetIntBase(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn && spawn->IsEntity()){
		lua_interface->SetInt32Value(state, static_cast<Entity*>(spawn)->GetIntBase());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetWisBase(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn && spawn->IsEntity()){
		lua_interface->SetInt32Value(state, static_cast<Entity*>(spawn)->GetWisBase());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetStaBase(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn && spawn->IsEntity()){
		lua_interface->SetInt32Value(state, static_cast<Entity*>(spawn)->GetStaBase());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetStrBase(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn && spawn->IsEntity()){
		lua_interface->SetInt32Value(state, static_cast<Entity*>(spawn)->GetStrBase());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetAgiBase(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn && spawn->IsEntity()){
		lua_interface->SetInt32Value(state, static_cast<Entity*>(spawn)->GetAgiBase());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_SetStepComplete(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* player = lua_interface->GetSpawn(state);
	int32 quest_id = lua_interface->GetInt32Value(state, 2);
	int32 step = lua_interface->GetInt32Value(state, 3);
	if(player && player->IsPlayer() && quest_id > 0 && step > 0){
		Client* client = player->GetZone()->GetClientBySpawn(player);
		if(client)
			client->AddPendingQuestUpdate(quest_id, step);
	}
	return 0;
}

int EQ2Emu_lua_AddStepProgress(lua_State* state) {
	Spawn* player = lua_interface->GetSpawn(state);
	int32 quest_id = lua_interface->GetInt32Value(state, 2);
	int32 step = lua_interface->GetInt32Value(state, 3);
	int32 progress = lua_interface->GetInt32Value(state, 4); 
	if (player && player->IsPlayer() && quest_id > 0 && step > 0 && progress > 0) {
		Client* client = player->GetZone()->GetClientBySpawn(player);
		if (client)
			client->AddPendingQuestUpdate(quest_id, step, progress);
	}
	return 0;
}

int EQ2Emu_lua_GetTaskGroupStep(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* player = lua_interface->GetSpawn(state);
	int32 quest_id = lua_interface->GetInt32Value(state, 2);
	if(player && player->IsPlayer() && quest_id > 0){
		lua_interface->SetInt32Value(state, static_cast<Player*>(player)->GetTaskGroupStep(quest_id));
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_QuestStepIsComplete(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* player = lua_interface->GetSpawn(state);
	int32 quest_id = lua_interface->GetInt32Value(state, 2);
	int32 step_id = lua_interface->GetInt32Value(state, 3);
	if(player && player->IsPlayer() && quest_id > 0){
		lua_interface->SetBooleanValue(state, static_cast<Player*>(player)->GetQuestStepComplete(quest_id, step_id));
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_GetQuestStep(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* player = lua_interface->GetSpawn(state);
	int32 quest_id = lua_interface->GetInt32Value(state, 2);
	if(player && player->IsPlayer() && quest_id > 0){
		lua_interface->SetInt32Value(state, static_cast<Player*>(player)->GetQuestStep(quest_id));
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_RegisterQuest(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	string name = lua_interface->GetStringValue(state, 2);
	string type = lua_interface->GetStringValue(state, 3);
	string zone = lua_interface->GetStringValue(state, 4);
	int16 level = lua_interface->GetInt16Value(state, 5);
	string description = lua_interface->GetStringValue(state, 6);
	bool load = true;
	if(!quest){
		lua_interface->SimpleLogError("Quest not given in RegisterQuest!");
		load = false;
	}
	if(load && name.length() == 0){
		lua_interface->SimpleLogError("Name not given in RegisterQuest!");
		load = false;
	}
	if(load && type.length() == 0){
		lua_interface->LogError("Type not given in RegisterQuest for '%s'!", name.c_str());
		load = false;
	}
	if(load && zone.length() == 0){
		lua_interface->LogError("Zone not given in RegisterQuest for '%s'!", name.c_str());
		load = false;
	}
	if(load && description.length() == 0){
		lua_interface->LogError("Description not given in RegisterQuest for '%s'!", name.c_str());
		load = false;
	}
	if(load && level == 0){
		lua_interface->LogError("Level not given in RegisterQuest for '%s'!", name.c_str());
		load = false;
	}
	if(load)
		quest->RegisterQuest(name, type, zone, level, description);
	return 0;
}

int EQ2Emu_lua_SetQuestPrereqLevel(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int8 level = lua_interface->GetInt16Value(state, 2);
		quest->SetPrereqLevel(level);
	}
	return 0;
}

int EQ2Emu_lua_AddQuestPrereqQuest(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int32 quest_id = lua_interface->GetInt32Value(state, 2);
		quest->AddPrereqQuest(quest_id);
	}
	return 0;
}

int EQ2Emu_lua_AddQuestPrereqItem(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int32 item_id = lua_interface->GetInt32Value(state, 2);
		int8 quantity = lua_interface->GetInt32Value(state, 3);
		if(quantity == 0)
			quantity = 1;
		Item* master_item = master_item_list.GetItem(item_id);
		if(master_item){
			Item* item = new Item(master_item);
			item->details.count = quantity;
			quest->AddPrereqItem(item);
		}
	}
	return 0;
}
int EQ2Emu_lua_HasQuest(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* player = lua_interface->GetSpawn(state);
	int32 quest_id = lua_interface->GetInt32Value(state, 2);
	if(player && player->IsPlayer() && quest_id > 0){
		lua_interface->SetBooleanValue(state, (static_cast<Player*>(player)->player_quests.count(quest_id) > 0));
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_QuestReturnNPC(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	int32 spawn_id = lua_interface->GetInt32Value(state, 2);
	if(quest && spawn_id > 0)
		quest->SetQuestReturnNPC(spawn_id);
	return 0;
}

int EQ2Emu_lua_AddTimer(lua_State* state){
	if(!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	if (!spawn) {
		lua_interface->LogError("LUA AddTimer command error: spawn is not valid");
		return 0;
	}

	int32 time = lua_interface->GetInt32Value(state, 2);
	if (time <= 0) {
		lua_interface->LogError("LUA AddTimer command error: time is not set");
		return 0;
	}

	string function = lua_interface->GetStringValue(state, 3);
	if (function.length() == 0) {
		lua_interface->LogError("LUA AddTimer command error: function is not set");
		return 0;
	}

	int32 max_count = lua_interface->GetInt32Value(state, 4);
	Spawn* player = lua_interface->GetSpawn(state, 5);

	SpawnScriptTimer* timer = new SpawnScriptTimer;
	timer->timer = Timer::GetCurrentTime2() + time;
	timer->function = function;
	timer->spawn = spawn->GetID();
	timer->player = player ? player->GetID() : 0;
	if(max_count == 0)
		max_count = 1;
	timer->max_count = max_count;
	timer->current_count = 0;
	spawn->GetZone()->AddSpawnScriptTimer(timer);

	return 0;
}

int EQ2Emu_lua_GetQuest(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* player = lua_interface->GetSpawn(state);
	int32 quest_id = lua_interface->GetInt32Value(state, 2);
	if(player && player->IsPlayer() && quest_id > 0){
		lua_interface->SetQuestValue(state, static_cast<Player*>(player)->player_quests[quest_id]);
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_QuestIsComplete(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* player = lua_interface->GetSpawn(state);
	int32 quest_id = lua_interface->GetInt32Value(state, 2);
	if(player && player->IsPlayer() && quest_id > 0){
		Quest* quest = static_cast<Player*>(player)->player_quests[quest_id];
		if(quest)
			lua_interface->SetBooleanValue(state, quest->GetCompleted());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_HasCompletedQuest(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* player = lua_interface->GetSpawn(state);
	int32 quest_id = lua_interface->GetInt32Value(state, 2);
	if(player && player->IsPlayer() && quest_id > 0){
		lua_interface->SetBooleanValue(state, (static_cast<Player*>(player)->GetCompletedQuest(quest_id) != 0));
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_ProvidesQuest(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* npc = lua_interface->GetSpawn(state);
	int32 quest_id = lua_interface->GetInt32Value(state, 2);
	if(npc && !npc->IsPlayer() && quest_id > 0)
		npc->AddProvidedQuest(quest_id);
	return 0;
}

int EQ2Emu_lua_OfferQuest(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* npc = lua_interface->GetSpawn(state);
	Spawn* player = lua_interface->GetSpawn(state, 2);
	int32 quest_id = lua_interface->GetInt32Value(state, 3);

	/* NPC is allowed to be null */
	if (player && player->IsPlayer() && quest_id > 0){
		Quest* master_quest = master_quest_list.GetQuest(quest_id);
		if(master_quest){
			Client* client = player->GetZone()->GetClientBySpawn(player);
			Quest* quest = new Quest(master_quest);
			if(client && quest){
				client->AddPendingQuest(quest);
				if (npc)
					quest->SetQuestGiver(npc->GetDatabaseID());
				else
					quest->SetQuestGiver(0);
			}
		}
	}
	return 0;
}

int EQ2Emu_lua_AddQuestPrereqClass(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int8 class_id = lua_interface->GetInt8Value(state, 2);
		quest->AddPrereqClass(class_id);
	}
	return 0;
}

int EQ2Emu_lua_AddQuestPrereqRace(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int8 race = lua_interface->GetInt8Value(state, 2);
		quest->AddPrereqRace(race);
	}
	return 0;
}

int EQ2Emu_lua_AddQuestPrereqModelType(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int16 model_type = lua_interface->GetInt16Value(state, 2);
		quest->AddPrereqModelType(model_type);
	}
	return 0;
}

int EQ2Emu_lua_AddQuestPrereqTradeskillLevel(lua_State* state){
	if(!lua_interface)
		return 0;

	Quest* quest = lua_interface->GetQuest(state);
	if (!quest) {
		lua_interface->LogError("LUA AddQuestPrereqTradeskillLevel command error: quest is not valid");
			return 0;
	}

	int8 level = lua_interface->GetInt8Value(state, 2);
	quest->SetPrereqTSLevel(level);
	return 0;
}

int EQ2Emu_lua_AddQuestPrereqTradeskillClass(lua_State* state){
	if(!lua_interface)
		return 0;

	Quest* quest = lua_interface->GetQuest(state);
	if (!quest) {
		lua_interface->LogError("LUA AddQuestPrereqTradeskillClass command error: quest is not valid");
		return 0;
	}

	int8 class_id = lua_interface->GetInt8Value(state, 2);
	quest->AddPrereqTradeskillClass(class_id);
	return 0;
}

int EQ2Emu_lua_AddQuestPrereqFaction(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int32 faction_id = lua_interface->GetInt32Value(state, 2);
		sint32 min = lua_interface->GetSInt32Value(state, 3);
		sint32 max = lua_interface->GetSInt32Value(state, 4);
		quest->AddPrereqFaction(faction_id, min, max);
	}
	return 0;
}

int EQ2Emu_lua_AddQuestSelectableRewardItem(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int32 item_id = lua_interface->GetInt32Value(state, 2);
		int8 quantity = lua_interface->GetInt8Value(state, 3);
		if(quantity == 0)
			quantity = 1;
		Item* master_item = master_item_list.GetItem(item_id);
		if(master_item){
			Item* item = new Item(master_item);
			item->details.count = quantity;
			quest->AddSelectableRewardItem(item);
		}
	}
	return 0;
}

int EQ2Emu_lua_AddQuestRewardItem(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int32 item_id = lua_interface->GetInt32Value(state, 2);
		int8 quantity = lua_interface->GetInt32Value(state, 3);
		if(quantity == 0)
			quantity = 1;
		Item* master_item = master_item_list.GetItem(item_id);
		if(master_item){
			Item* item = new Item(master_item);
			item->details.count = quantity;
			quest->AddRewardItem(item);
		}
	}
	return 0;
}

int EQ2Emu_lua_AddQuestRewardCoin(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int32 copper = lua_interface->GetInt32Value(state, 2);
		int32 silver = lua_interface->GetInt32Value(state, 3);
		int32 gold = lua_interface->GetInt32Value(state, 4);
		int32 plat = lua_interface->GetInt32Value(state, 5);
		quest->AddRewardCoins(copper, silver, gold, plat);
	}
	return 0;
}

int EQ2Emu_lua_AddQuestRewardFaction(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	int32 faction_id = lua_interface->GetInt32Value(state, 2);
	sint32 amount = lua_interface->GetSInt32Value(state, 3);
	if (quest && faction_id > 0 && amount != 0)
		quest->AddRewardFaction(faction_id, amount);
	return 0;
}

int EQ2Emu_lua_SetQuestRewardStatus(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int32 status = lua_interface->GetInt32Value(state, 2);
		quest->SetRewardStatus(status);
	}
	return 0;
}

int EQ2Emu_lua_SetQuestRewardComment(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		string comment = lua_interface->GetStringValue(state, 2);
		quest->SetRewardComment(comment);
	}
	return 0;
}

int EQ2Emu_lua_SetQuestRewardExp(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int32 exp = lua_interface->GetInt32Value(state, 2);
		quest->SetRewardXP(exp);
	}
	return 0;
}

int EQ2Emu_lua_AddQuestStep(lua_State* state) {
	Quest* quest = lua_interface->GetQuest(state);
	if (quest) {
		int32 step = lua_interface->GetInt32Value(state, 2);
		string description = lua_interface->GetStringValue(state, 3);
		int32 quantity = lua_interface->GetInt32Value(state, 4);
		float percentage = lua_interface->GetFloatValue(state, 5);
		string str_taskgroup = lua_interface->GetStringValue(state, 6);
		int16 icon = lua_interface->GetInt16Value(state, 7);
		const char* taskgroup = 0;
		if(str_taskgroup.length() > 0)
			taskgroup = str_taskgroup.c_str();
		QuestStep* quest_step = quest->AddQuestStep(step, QUEST_STEP_TYPE_NORMAL, description, 0, quantity, taskgroup, 0, 0, percentage);
		if (quest_step && icon && quantity > 0)
			quest_step->SetIcon(icon);
	}
	return 0;
}

int EQ2Emu_lua_AddQuestStepKill(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int32 step = lua_interface->GetInt32Value(state, 2);
		string description = lua_interface->GetStringValue(state, 3);
		int32 quantity = lua_interface->GetInt32Value(state, 4);
		float percentage = lua_interface->GetFloatValue(state, 5);
		string str_taskgroup = lua_interface->GetStringValue(state, 6);
		int16 icon = lua_interface->GetInt16Value(state, 7);
		const char* taskgroup = 0;
		if(str_taskgroup.length() > 0)
			taskgroup = str_taskgroup.c_str();
		int32 npc_id = 0;
		vector<int32>* ids = 0;
		int i=0;
		while((npc_id = lua_interface->GetInt32Value(state, 8+i))){
			if(ids == 0)
				ids = new vector<int32>;
			ids->push_back(npc_id);
			i++;
		}
		QuestStep* quest_step = quest->AddQuestStep(step, QUEST_STEP_TYPE_KILL, description, ids, quantity, taskgroup, 0, 0, percentage);
		if(quest_step && icon > 0 && quantity > 0)
				quest_step->SetIcon(icon);
	}
	return 0;
}

int EQ2Emu_lua_AddQuestStepChat(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int32 step = lua_interface->GetInt32Value(state, 2);
		string description = lua_interface->GetStringValue(state, 3);
		int32 quantity = lua_interface->GetInt32Value(state, 4);
		string str_taskgroup = lua_interface->GetStringValue(state, 5);
		int16 icon = lua_interface->GetInt16Value(state, 6);
		const char* taskgroup = 0;
		if(str_taskgroup.length() > 0)
			taskgroup = str_taskgroup.c_str();
		int32 npc_id = 0;
		vector<int32>* ids = 0;
		int i=0;
		while((npc_id = lua_interface->GetInt32Value(state, 7+i))){
			if(ids == 0)
				ids = new vector<int32>;
			ids->push_back(npc_id);
			i++;
		}
		QuestStep* quest_step = quest->AddQuestStep(step, QUEST_STEP_TYPE_CHAT, description, ids, quantity, taskgroup);
		if (quest_step && icon > 0)
			quest_step->SetIcon(icon);
		if(quest->GetPlayer()){
			Client* client = quest->GetPlayer()->GetZone()->GetClientBySpawn(quest->GetPlayer());
			quest->GetPlayer()->GetZone()->SendQuestUpdates(client);
		}
	}
	return 0;
}

int EQ2Emu_lua_AddQuestStepObtainItem(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int32 step = lua_interface->GetInt32Value(state, 2);
		string description = lua_interface->GetStringValue(state, 3);
		int32 quantity = lua_interface->GetInt32Value(state, 4);
		float percentage = lua_interface->GetFloatValue(state, 5);
		string str_taskgroup = lua_interface->GetStringValue(state, 6);
		int16 icon = lua_interface->GetInt16Value(state, 7);
		const char* taskgroup = 0;
		if(str_taskgroup.length() > 0)
			taskgroup = str_taskgroup.c_str();
		int32 item_id = 0;
		vector<int32>* ids = 0;
		int i=0;
		while((item_id = lua_interface->GetInt32Value(state, 8+i))){
			if(ids == 0)
				ids = new vector<int32>;
			ids->push_back(item_id);
			i++;
		}
		QuestStep* quest_step = quest->AddQuestStep(step, QUEST_STEP_TYPE_OBTAIN_ITEM, description, ids, quantity, taskgroup, 0, 0, percentage);
		if (quest_step && icon > 0 && quantity > 0)
			quest_step->SetIcon(icon);
	}
	return 0;
}

int EQ2Emu_lua_AddQuestStepLocation(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int32 step = lua_interface->GetInt32Value(state, 2);
		string description = lua_interface->GetStringValue(state, 3);
		float max_variation = lua_interface->GetFloatValue(state, 4);
		string str_taskgroup = lua_interface->GetStringValue(state, 5);
		int16 icon = lua_interface->GetInt16Value(state, 6);
		const char* taskgroup = 0;
		if(str_taskgroup.length() > 0)
			taskgroup = str_taskgroup.c_str();
		vector<Location>* locations = 0;
		int i = 7;
		while(true) {
			Location loc;
			loc.id = 0;
			loc.x = lua_interface->GetFloatValue(state, i);
			loc.y = lua_interface->GetFloatValue(state, i + 1);
			loc.z = lua_interface->GetFloatValue(state, i + 2);
			if(loc.x == 0 && loc.y == 0 && loc.z == 0)
				break;
			if(locations == 0) 
				locations = new vector<Location>;
			locations->push_back(loc);
			i += 3;
		}
		QuestStep* quest_step = quest->AddQuestStep(step, QUEST_STEP_TYPE_LOCATION, description, 0, 1, taskgroup, locations, max_variation);
		if (quest_step && icon > 0)
			quest_step->SetIcon(icon);
	}
	return 0;
}

int EQ2Emu_lua_AddQuestStepSpell(lua_State* state) {
	Quest* quest = lua_interface->GetQuest(state);
	if(quest) {
		int32 step = lua_interface->GetInt32Value(state, 2);
		string description = lua_interface->GetStringValue(state, 3);
		int32 quantity = lua_interface->GetInt32Value(state, 4);
		float percentage = lua_interface->GetFloatValue(state, 5);
		string str_taskgroup = lua_interface->GetStringValue(state, 6);
		int16 icon = lua_interface->GetInt16Value(state, 7);
		const char* taskgroup = 0;
		if (str_taskgroup.length() > 0)
			taskgroup = str_taskgroup.c_str();
		int32 spell_id = 0;
		vector<int32>* ids = 0;
		int i = 0;
		while ((spell_id = lua_interface->GetInt32Value(state, 8 + i))) {
			if (ids == 0)
				ids = new vector<int32>;
			ids->push_back(spell_id);
			i++;
		}
		QuestStep* quest_step = quest->AddQuestStep(step, QUEST_STEP_TYPE_SPELL, description, ids, quantity, taskgroup, 0, 0, percentage);
		if(quest_step && icon > 0 && quantity > 0)
				quest_step->SetIcon(icon);
	}
	return 0;
}

int EQ2Emu_lua_AddQuestStepCraft(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int32 step = lua_interface->GetInt32Value(state, 2);
		string description = lua_interface->GetStringValue(state, 3);
		int32 quantity = lua_interface->GetInt32Value(state, 4);
		float percentage = lua_interface->GetFloatValue(state, 5);
		string str_taskgroup = lua_interface->GetStringValue(state, 6);
		int16 icon = lua_interface->GetInt16Value(state, 7);
		const char* taskgroup = 0;
		if(str_taskgroup.length() > 0)
			taskgroup = str_taskgroup.c_str();
		int32 item_id = 0;
		vector<int32>* ids = 0;
		int i=0;
		while((item_id = lua_interface->GetInt32Value(state, 8+i))){
			if(ids == 0)
				ids = new vector<int32>;
			ids->push_back(item_id);
			i++;
		}
		QuestStep* quest_step = quest->AddQuestStep(step, QUEST_STEP_TYPE_CRAFT, description, ids, quantity, taskgroup, 0, 0, percentage);
		if (quest_step && icon > 0 && quantity > 0)
			quest_step->SetIcon(icon);
	}
	return 0;
}

int EQ2Emu_lua_AddQuestStepHarvest(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int32 step = lua_interface->GetInt32Value(state, 2);
		string description = lua_interface->GetStringValue(state, 3);
		int32 quantity = lua_interface->GetInt32Value(state, 4);
		float percentage = lua_interface->GetFloatValue(state, 5);
		string str_taskgroup = lua_interface->GetStringValue(state, 6);
		int16 icon = lua_interface->GetInt16Value(state, 7);
		const char* taskgroup = 0;
		if(str_taskgroup.length() > 0)
			taskgroup = str_taskgroup.c_str();
		int32 item_id = 0;
		vector<int32>* ids = 0;
		int i=0;
		while((item_id = lua_interface->GetInt32Value(state, 8+i))){
			if(ids == 0)
				ids = new vector<int32>;
			ids->push_back(item_id);
			i++;
		}
		QuestStep* quest_step = quest->AddQuestStep(step, QUEST_STEP_TYPE_HARVEST, description, ids, quantity, taskgroup, 0, 0, percentage);
		if (quest_step && icon > 0 && quantity > 0)
			quest_step->SetIcon(icon);
	}
	return 0;
}

int EQ2Emu_lua_SetQuestCompleteAction(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		string action = lua_interface->GetStringValue(state, 2);
		if(action.length() > 0)
			quest->SetCompleteAction(action);
	}
	return 0;
}

int EQ2Emu_lua_AddQuestStepCompleteAction(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int32 step = lua_interface->GetInt32Value(state, 2);
		string action = lua_interface->GetStringValue(state, 3);
		if(step > 0 && action.length() > 0)
			quest->AddCompleteAction(step, action);
	}
	return 0;
}

int EQ2Emu_lua_AddQuestStepProgressAction(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if(quest){
		int32 step = lua_interface->GetInt32Value(state, 2);
		string action = lua_interface->GetStringValue(state, 3);
		if(step > 0 && action.length() > 0)
			quest->AddProgressAction(step, action);
	}
	return 0;
}

int EQ2Emu_lua_UpdateQuestDescription(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	string description = lua_interface->GetStringValue(state, 2);
	if(quest && description.length() > 0)
		quest->SetDescription(description);
	return 0;
}

int EQ2Emu_lua_SetCompletedDescription(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	string description = lua_interface->GetStringValue(state, 2);
	if(quest && description.length() > 0)
		quest->SetCompletedDescription(description);
	return 0;
}

int EQ2Emu_lua_UpdateQuestTaskGroupDescription(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	int32 step = lua_interface->GetInt32Value(state, 2);
	string description = lua_interface->GetStringValue(state, 3);
	bool display_bullets = (lua_interface->GetInt8Value(state, 4) == 1);
	if(quest && step > 0 && description.length() > 0){
		quest->SetTaskGroupDescription(step, description, display_bullets);
		if(quest->GetPlayer()){
			Client* client = quest->GetPlayer()->GetZone()->GetClientBySpawn(quest->GetPlayer());
			if(client)
				client->SendQuestUpdateStep(quest, step, false);
		}
	}
	return 0;
}

int EQ2Emu_lua_UpdateQuestStepDescription(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	int32 step = lua_interface->GetInt32Value(state, 2);
	string description = lua_interface->GetStringValue(state, 3);
	if(quest && step > 0 && description.length() > 0){
		quest->SetStepDescription(step, description);
		if(quest->GetPlayer()){
			Client* client = quest->GetPlayer()->GetZone()->GetClientBySpawn(quest->GetPlayer());
			if(client)
				client->SendQuestUpdateStep(quest, step);
		}
	}
	return 0;
}

int EQ2Emu_lua_UpdateQuestZone(lua_State* state) {
	Quest* quest = lua_interface->GetQuest(state);
	string zone = lua_interface->GetStringValue(state, 2);
	if (quest && zone.length() > 0)
		quest->SetZone(zone);
	return 0;
}

int EQ2Emu_lua_GiveQuestReward(lua_State* state){
	if(!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	Spawn* spawn = lua_interface->GetSpawn(state, 2);
	if(quest && spawn){
		if(spawn->IsPlayer()){
			Client* client = spawn->GetZone()->GetClientBySpawn(spawn);
			if(client)
				client->AddPendingQuestReward(quest);
		}
	}
	return 0;
}

int EQ2Emu_lua_Harvest(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* player = lua_interface->GetSpawn(state);
	Spawn* node = lua_interface->GetSpawn(state, 2);
	if(player && node && player->IsPlayer() && node->IsGroundSpawn()){
		Client* client = player->GetZone()->GetClientBySpawn(player);

		if(client){
			LogWrite(MISC__TODO, 1, "TODO", "Cancel harvest if skill insufficient; Func: %s, Line: %i", __FUNCTION__, __LINE__);
			static_cast<GroundSpawn*>(node)->ProcessHarvest(client);
			if(static_cast<GroundSpawn*>(node)->GetNumberHarvests() == 0)
				player->GetZone()->RemoveSpawn(node, true);
		}
	}
	else if(player && player->IsPlayer()){
		Client* client = player->GetZone()->GetClientBySpawn(player);
		if(client)
			client->Message(CHANNEL_COLOR_RED, "Invalid target for this spell.");
	}
	return 0;
}

int EQ2Emu_lua_Bind(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		if(spawn->IsPlayer()){
			Client* client = spawn->GetZone()->GetClientBySpawn(spawn);
			if(client){
				if(!client->Bind())
					client->SimpleMessage(CHANNEL_COLOR_RED, "Unable to set bind point.");
			}
		}
	}
	return 0;
}

int EQ2Emu_lua_Gate(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		if(spawn->IsPlayer()){
			Client* client = spawn->GetZone()->GetClientBySpawn(spawn);
			if(client){
				if(!client->Gate())
					client->SimpleMessage(CHANNEL_COLOR_RED, "Unable to gate.");
			}
		}
	}
	return 0;
}

int EQ2Emu_lua_IsBindAllowed(lua_State* state){
	if(!lua_interface)
		return 0;
	bool ret = false;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		if(spawn->IsPlayer()){
			Client* client = spawn->GetZone()->GetClientBySpawn(spawn);
			if(client)
				ret = client->BindAllowed();
		}
	}
	lua_interface->SetBooleanValue(state, ret);
	return 1;
}

int EQ2Emu_lua_IsGateAllowed(lua_State* state){
	if(!lua_interface)
		return 0;
	bool ret = false;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn){
		if(spawn->IsPlayer()){
			Client* client = spawn->GetZone()->GetClientBySpawn(spawn);
			if(client)
				ret = client->GateAllowed();
		}
	}
	lua_interface->SetBooleanValue(state, ret);
	return 1;
}

int EQ2Emu_lua_IsAlive(lua_State* state) {
	Spawn* spawn = lua_interface->GetSpawn(state);
	if (spawn) {
		lua_interface->SetBooleanValue(state, spawn->Alive());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_IsInCombat(lua_State* state) {
	if (!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if (spawn && spawn->IsEntity()) {
		lua_interface->SetBooleanValue(state, static_cast<Entity*>(spawn)->EngagedInCombat());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_SendMessage(lua_State* state) {
	Spawn* spawn = lua_interface->GetSpawn(state);
	string message = lua_interface->GetStringValue(state, 2);
	string color_str = lua_interface->GetStringValue(state, 3);
	if (spawn && spawn->IsPlayer() && message.length() > 0) {
		Client* client = spawn->GetZone()->GetClientBySpawn(spawn);
		if (client) {
			int8 color = CHANNEL_COLOR_WHITE;
			if (color_str.length() > 0) {
				if (strncasecmp(color_str.c_str(), "red", 3) == 0)
					color = CHANNEL_COLOR_RED;
				else if (strncasecmp(color_str.c_str(), "yellow", 6) == 0)
					color = CHANNEL_COLOR_YELLOW;
			}
			client->SimpleMessage(color, message.c_str());
		}
	}
	return 0;
}

int EQ2Emu_lua_SendPopUpMessage(lua_State* state) {
	Spawn* spawn = lua_interface->GetSpawn(state);
	string message = lua_interface->GetStringValue(state, 2);
	int8 red = lua_interface->GetInt8Value(state, 3);
	int8 green = lua_interface->GetInt8Value(state, 4);
	int8 blue = lua_interface->GetInt8Value(state, 5);
	int32 words = ::CountWordsInString(message.c_str());
	if (words < 5)
		words = 5;
	Client* client = spawn->GetZone()->GetClientBySpawn(spawn);
	if (client)
		client->SendPopupMessage(10, message.c_str(), "ui_harvested_normal", words, red, green, blue);
	return 0;
}

int EQ2Emu_lua_SetServerControlFlag(lua_State* state) {
	Spawn* spawn = lua_interface->GetSpawn(state);
	int8 param = lua_interface->GetInt8Value(state, 2);
	int8 param_value = lua_interface->GetInt8Value(state, 3);
	int8 value = lua_interface->GetInt8Value(state, 4);
	if (spawn && spawn->IsPlayer() && (param >= 1 && param <= 5)) {
		Client* client = spawn->GetZone()->GetClientBySpawn(spawn);
		if (client) {
			PacketStruct* packet = configReader.getStruct("WS_ServerControlFlags", client->GetVersion());
			switch (param) {
				case 1: {
					packet->setDataByName("parameter1", param_value);
					break;
				}
				case 2: {
					packet->setDataByName("parameter2", param_value);
					break;
				}
				case 3: {
					packet->setDataByName("parameter3", param_value);
					break;
				}
				case 4: {
					packet->setDataByName("parameter4", param_value);
					break;
				}
				case 5: {
					packet->setDataByName("parameter5", param_value);
					break;
				}
			}
			packet->setDataByName("value", value);
			client->QueuePacket(packet->serialize());
			safe_delete(packet);
		}
	}
	return 0;
}

int EQ2Emu_lua_ToggleTracking(lua_State* state) {
	Spawn* spawn = lua_interface->GetSpawn(state);
	if (spawn && spawn->IsPlayer()) {
		if (static_cast<Player*>(spawn)->GetIsTracking())
			spawn->GetZone()->AddPlayerTracking(static_cast<Player*>(spawn));
		else
			spawn->GetZone()->RemovePlayerTracking(static_cast<Player*>(spawn), TRACKING_STOP);
	}
	return 0;
}

int EQ2Emu_lua_AddPrimaryEntityCommand(lua_State* state) {
	Spawn* player = lua_interface->GetSpawn(state);
	Spawn* spawn = lua_interface->GetSpawn(state, 2);
	string name = lua_interface->GetStringValue(state, 3);
	float distance = lua_interface->GetFloatValue(state, 4);
	string command = lua_interface->GetStringValue(state, 5);
	string error_text = lua_interface->GetStringValue(state, 6);
	int16 cast_time = lua_interface->GetInt16Value(state, 7);
	int32 spell_visual = lua_interface->GetInt32Value(state, 8);
	if (spawn && player && player->IsPlayer() && name.length() > 0) {
		if (distance == 0)
			distance = 10.0f;
		if (command.length() == 0)
			command = name;
		spawn->AddPrimaryEntityCommand(name.c_str(), distance, command.c_str(), error_text.c_str(), cast_time, spell_visual);
		player->GetZone()->SendUpdateDefaultCommand(spawn, command.c_str(), distance);
	}
	return 0;
}

int EQ2Emu_lua_AddSpellBookEntry(lua_State* state) {
	if(!lua_interface)
		return 0;
	Spawn* player = lua_interface->GetSpawn(state);
	int16 spellid = lua_interface->GetInt16Value(state, 2);
	int16 tier = lua_interface->GetInt16Value(state, 3);
	Spell* spell = master_spell_list.GetSpell(spellid, tier);
	if (player && spell && player->IsPlayer()) {
		Client* client = player->GetZone()->GetClientBySpawn(player);
		if (client ) {
			static_cast<Player*>(player)->AddSpellBookEntry(spell->GetSpellID(), spell->GetSpellTier(), static_cast<Player*>(player)->GetFreeSpellBookSlot(spell->GetSpellData()->spell_book_type), spell->GetSpellData()->spell_book_type, spell->GetSpellData()->linked_timer, true);
			EQ2Packet* outapp = static_cast<Player*>(player)->GetSpellBookUpdatePacket(client->GetVersion());
			if (outapp)
				client->QueuePacket(outapp);
		}
	}
	return 0;
}

int EQ2Emu_lua_HasFreeSlot(lua_State* state) {
	if(!lua_interface)
		return 0;
	Spawn* player = lua_interface->GetSpawn(state);
	if(player && player->IsPlayer()) {
		lua_interface->SetBooleanValue(state, static_cast<Player*>(player)->item_list.HasFreeSlot());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_Attack(lua_State* state) {
	if (lua_interface) {
		Spawn* npc = lua_interface->GetSpawn(state);
		Spawn* player = lua_interface->GetSpawn(state, 2);
		if (npc && player && npc->IsNPC() && player->IsPlayer())
			static_cast<NPC*>(npc)->AddHate(static_cast<Entity*>(player), 100);
	}
	return 0;
}

int EQ2Emu_lua_ApplySpellVisual(lua_State* state) {
	if (lua_interface) {
		Spawn* target = lua_interface->GetSpawn(state);
		int32 spell_visual = lua_interface->GetInt32Value(state, 2);
		Spawn* caster = lua_interface->GetSpawn(state, 3);
		if (target && target->GetZone())
			target->GetZone()->SendCastSpellPacket(spell_visual, target, caster);
	}
	return 0;
}

int EQ2Emu_lua_HasCollectionsToHandIn(lua_State *state) {
	if (lua_interface) {
		Spawn *player = lua_interface->GetSpawn(state);
		if (player && player->IsPlayer()) {
			lua_interface->SetBooleanValue(state, static_cast<Player*>(player)->GetCollectionList()->HasCollectionsToHandIn());
			return 1;
		}
	}

	return 0;
}

int EQ2Emu_lua_HandInCollections(lua_State *state) {
	Spawn *player;

	if (lua_interface) {
		player = lua_interface->GetSpawn(state);
		if (player && static_cast<Player*>(player)->IsPlayer() && static_cast<Player*>(player)->GetCollectionList()->HasCollectionsToHandIn()) {
			Client *client = player->GetZone()->GetClientBySpawn(player);
			if (client)
				client->HandInCollections();
		}
	}

	return 0;
}

int EQ2Emu_lua_UseWidget(lua_State* state) {
	if (lua_interface) {
		Spawn* widget = lua_interface->GetSpawn(state);
		if (widget && widget->IsWidget())
			static_cast<Widget*>(widget)->HandleUse(NULL, "");
	}

	return 0;
}

int EQ2Emu_lua_SetSpellList(lua_State* state) {
	if (lua_interface) {
		Spawn* spawn = lua_interface->GetSpawn(state);
		int32 primary_list = lua_interface->GetInt32Value(state, 2);
		int32 secondary_list = lua_interface->GetInt32Value(state, 3);

		if (!spawn || !spawn->IsNPC()) {
			lua_interface->LogError("LUA SetSpellList command error: Spawn was not a valid NPC");
			return 0;
		}

		NPC* npc = static_cast<NPC*>(spawn);
		npc->SetPrimarySpellList(primary_list);
		npc->SetSecondarySpellList(secondary_list);
		npc->SetSpells(npc->GetZone()->GetNPCSpells(npc->GetPrimarySpellList(), npc->GetSecondarySpellList()));
	}
	return 0;
}

int EQ2Emu_lua_GetPet(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn) {
		if (spawn->IsEntity() && static_cast<Entity*>(spawn)->GetPet()) {
			lua_interface->SetSpawnValue(state, static_cast<Entity*>(spawn)->GetPet());
			return 1;
		}
	}
	return 0;
}

int EQ2Emu_lua_GetCharmedPet(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn) {
		if (spawn->IsEntity() && static_cast<Entity*>(spawn)->GetCharmedPet()) {
			lua_interface->SetSpawnValue(state, static_cast<Entity*>(spawn)->GetCharmedPet());
			return 1;
		}
	}
	return 0;
}

int EQ2Emu_lua_GetDeityPet(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn) {
		if (spawn->IsEntity() && static_cast<Entity*>(spawn)->GetDeityPet()) {
			lua_interface->SetSpawnValue(state, static_cast<Entity*>(spawn)->GetDeityPet());
			return 1;
		}
	}
	return 0;
}

int EQ2Emu_lua_GetCosmeticPet(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(spawn) {
		if (spawn->IsEntity() && static_cast<Entity*>(spawn)->GetCosmeticPet()) {
			lua_interface->SetSpawnValue(state, static_cast<Entity*>(spawn)->GetCosmeticPet());
			return 1;
		}
	}
	return 0;
}
int EQ2Emu_lua_Charm(lua_State* state){
	if (!lua_interface)
		return 0;
	Spawn* owner = lua_interface->GetSpawn(state);
	Spawn* pet = lua_interface->GetSpawn(state, 2);
	LuaSpell* luaspell = lua_interface->GetCurrentSpell(state);

	if (owner && pet && owner->IsEntity() && pet->IsNPC()) {
		static_cast<Entity*>(owner)->SetCharmedPet(static_cast<Entity*>(pet));
		pet->SetPet(true);
		static_cast<NPC*>(pet)->SetPetType(PET_TYPE_CHARMED);
		static_cast<NPC*>(pet)->SetOwner(static_cast<Entity*>(owner));
		// If owner is player and player does not have a summoned pet set the players charsheet
		if (owner->IsPlayer() && !static_cast<Entity*>(owner)->GetPet()) {
			Player* player = static_cast<Player*>(owner);
				player->GetInfoStruct()->pet_id = player->GetIDWithPlayerSpawn(pet);
				strcpy(player->GetInfoStruct()->pet_name, pet->GetName());
				player->GetInfoStruct()->pet_movement = 2;
				player->GetInfoStruct()->pet_behavior = 3;
				player->GetInfoStruct()->pet_health_pct = 1.0f;
				player->GetInfoStruct()->pet_power_pct = 1.0f;
				// Make sure the values get sent to the client
				player->SetCharSheetChanged(true);
		}
		// Clear the spawns script so the charmed mob doesn't try to do anything like random walks
		pet->SetSpawnScript("");
		// Set faction to the same as the owner
		pet->SetFactionID(owner->GetFactionID());

		static_cast<NPC*>(pet)->SetPetSpellID(luaspell->spell->GetSpellData()->id);
		static_cast<NPC*>(pet)->SetPetSpellTier(luaspell->spell->GetSpellData()->tier);

		// Clear hate list
		static_cast<NPC*>(pet)->Brain()->ClearHate();

		// Set the brain to a pet brain
		static_cast<NPC*>(pet)->SetBrain(new CombatPetBrain(static_cast<NPC*>(pet)));
	}
	return 0;
}

int EQ2Emu_lua_GetGroup(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	if (!spawn) {
		lua_interface->LogError("LUA GetGroup command error: spawn is not valid");
		return 0;
	}

	vector<Spawn*> groupMembers;
	if (!spawn->IsPlayer() && spawn->HasSpawnGroup()) {
		groupMembers = *spawn->GetSpawnGroup();
	}
	else if (spawn->IsPlayer() && static_cast<Player*>(spawn)->GetGroupMemberInfo()) {
		world.GetGroupManager()->GroupLock(__FUNCTION__, __LINE__);

		deque<GroupMemberInfo*>::iterator itr;
		deque<GroupMemberInfo*>* members = world.GetGroupManager()->GetGroupMembers(static_cast<Player*>(spawn)->GetGroupMemberInfo()->group_id);
		for(itr = members->begin(); itr != members->end(); ++itr){
			GroupMemberInfo* info = *itr;
			if(info && info->client)
				groupMembers.push_back(info->client->GetPlayer());
		}

		world.GetGroupManager()->ReleaseGroupLock(__FUNCTION__, __LINE__);
	}
	else
		return 0;

	lua_createtable(state, groupMembers.size(), 0); 
	int newTable = lua_gettop(state); 
	for(int32 i=0; i < groupMembers.size(); i++) { 
		lua_interface->SetSpawnValue(state, groupMembers.at(i));
		lua_rawseti(state, newTable, i + 1); 
	} 
	return 1; 

}

int EQ2Emu_lua_CreateOptionWindow(lua_State* state) {
	if (!lua_interface)
		return 0;
	vector<OptionWindowOption>* option_window = new vector<OptionWindowOption>();
	lua_interface->SetOptionWindowValue(state, option_window);
	return 1;
}

int EQ2Emu_lua_AddOptionWindowOption(lua_State* state) {
	if (!lua_interface)
		return 0;
	vector<OptionWindowOption>* option_window = lua_interface->GetOptionWindow(state);
	if (option_window) {
		OptionWindowOption option_window_option;
		option_window_option.optionName = lua_interface->GetStringValue(state, 2);
		option_window_option.optionDescription = lua_interface->GetStringValue(state, 3);
		option_window_option.optionIconSheet = lua_interface->GetInt32Value(state, 4);
		option_window_option.optionIconID = lua_interface->GetInt16Value(state, 5);
		option_window_option.optionCommand = lua_interface->GetStringValue(state, 6);
		option_window_option.optionConfirmTitle = lua_interface->GetStringValue(state, 7);
		if (option_window_option.optionName.length() > 0 && option_window_option.optionDescription.length() > 0)
			option_window->push_back(option_window_option);
	}
	return 0;
}

int EQ2Emu_lua_SendOptionWindow(lua_State* state) {
	if (!lua_interface)
		return 0;

	vector<OptionWindowOption>* option_window = lua_interface->GetOptionWindow(state);
	Spawn* player = lua_interface->GetSpawn(state, 2);
	string window_title = lua_interface->GetStringValue(state, 3);
	string cancel_command = lua_interface->GetStringValue(state, 4);
	Client* client = player->GetZone()->GetClientBySpawn(player);
	if (option_window && window_title.length() > 0 && client) {
		PacketStruct* packet = configReader.getStruct("WS_SelectTradeskill", client->GetVersion());
		if (!packet)
			return 0;

		packet->setDataByName("title_text", window_title.c_str());
		if (cancel_command.length() > 0)
			packet->setDataByName("command_text_cancel", cancel_command.c_str());

		packet->setArrayLengthByName("num_selections", option_window->size());
		vector<OptionWindowOption>::iterator itr;
		int8 i = 0;
		for (itr = option_window->begin(); itr != option_window->end(); ++itr) {
			OptionWindowOption opt = *itr;
			packet->setArrayDataByName("tradeskill_name", opt.optionName.c_str(), i);
			packet->setArrayDataByName("tradeskill_description", opt.optionDescription.c_str(), i);
			packet->setArrayDataByName("icon_sheet", opt.optionIconSheet, i);
			packet->setArrayDataByName("icon_id", opt.optionIconID, i);
			if (opt.optionCommand.length() > 0)
				packet->setArrayDataByName("command_text", opt.optionCommand.c_str(), i);
			if (opt.optionConfirmTitle.length() > 0)
				packet->setArrayDataByName("confirm_window_title", opt.optionConfirmTitle.c_str(), i);

			i++;
		}
		client->QueuePacket(packet->serialize());
		safe_delete(option_window);
		safe_delete(packet);
	}
	return 0;
}

int EQ2Emu_lua_GetTradeskillClass(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);

	if (spawn) {
		lua_interface->SetInt32Value(state, spawn->GetTradeskillClass());
		return 1;
	}
	else
		lua_interface->LogError("LUA GetTradeskillClass command error: Spawn was not valid");

	return 0;
}

int EQ2Emu_lua_GetTradeskillLevel(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	if (spawn) {
		lua_interface->SetInt32Value(state, spawn->GetTSLevel());
		return 1;
	}
	else
		lua_interface->LogError("LUA GetTradeskillLevel command error: Spawns was not valid");
	return 0;
}

int EQ2Emu_lua_GetTradeskillClassName(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
		if (spawn) {
			int8 class_id = spawn->GetTradeskillClass();
			// Need to add 42 for the offset in the array
			class_id += 44;
			lua_interface->SetStringValue(state, classes.GetClassNameCase(class_id).c_str());
			return 1;
		}
		else
			lua_interface->LogError("LUA GetTradeskillClassName command error: Spawn was not valid");
	return 0;
}

int EQ2Emu_lua_SetTradeskillLevel(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	int16 level = lua_interface->GetInt8Value(state, 2);
	if (spawn) {
		if (spawn->IsPlayer())
			spawn->GetZone()->GetClientBySpawn(spawn)->ChangeTSLevel(spawn->GetTSLevel(), level);
		else
			spawn->SetTSLevel(level);
	}
	else
		lua_interface->LogError("LUA SetTradeskillLevel command error: Spawn was not valid");
	return 0;
}

int EQ2Emu_lua_SummonPet(lua_State* state) {
	// Check to see if we have a valid lua_interface
	if(!lua_interface)
		return 0;

	// Get the spawn that is getting the pet
	Spawn* spawn = lua_interface->GetSpawn(state);
	// Get the DB ID of the pet
	int32 pet_id = lua_interface->GetInt32Value(state, 2);
	// The max level the pet can gain
	int8 max_level = lua_interface->GetInt8Value(state, 3);
	// Get the spell that this command was called from
	LuaSpell* luaspell = lua_interface->GetCurrentSpell(state);
	
	// Check to make sure the spawn pointer is valid
	if (!spawn) {
		lua_interface->LogError("LUA SummonPet command error: Spawn is not valid");
		return 0;
	}

	// Check to make sure the spawn is an entity
	if (!spawn->IsEntity()) {
		lua_interface->LogError("LUA SummonPet command error: Spawn is not an entity");
		return 0;
	}

	// Check to make sure the spawn doesn't already have a pet of this type
	if (static_cast<Entity*>(spawn)->GetPet()) {
		if (spawn->IsPlayer()) {
			Client* client = spawn->GetZone()->GetClientBySpawn(spawn);
			if (client)
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You already have a pet.");
		}

		lua_interface->LogError("LUA SummonPet command error: spawn already has a pet of this type");
		return 0;
	}
	
	// Check to see if the DB ID for the pet is set
	if (pet_id == 0) {
		lua_interface->LogError("LUA SummonPet command error: pet_id can not be set to 0");
		return 0;
	}

	// Check to see if the pointer to the spell is valid
	if (!luaspell) {
		lua_interface->LogError("LUA SummonPet command error: valid spell not found, SummonPet can only be used in spell scripts");
		return 0;
	}

	// Get a pointer to a spawn with the given DB ID and check if the pointer is valid
	Spawn* pet = spawn->GetZone()->GetSpawn(pet_id);
	if(!pet) {
		lua_interface->LogError("LUA SummonPet command error: Could not find spawn with id of %u.", pet_id);
		return 0;
	}

	// Check to make sure the pet is an npc
	if (!pet->IsNPC()) {
		lua_interface->LogError("LUA SummonPet command error: id (%u) did not point to a npc", pet_id);
		return 0;
	}

	// Spawn the pet at the same location as the owner
	pet->SetX(spawn->GetX());
	pet->SetY(spawn->GetY());
	pet->SetZ(spawn->GetZ());
	pet->SetLocation(spawn->GetLocation());
	pet->SetHeading(spawn->GetHeading());			
	spawn->GetZone()->AddSpawn(pet);

	/*
	const char* spawn_script = world.GetSpawnScript(pet_id);	
	if(spawn_script && lua_interface->GetSpawnScript(spawn_script) != 0){
		spawn->SetSpawnScript(string(spawn_script));
		zone->CallSpawnScript(spawn, SPAWN_SCRIPT_SPAWN);
	}*/

	// Get a random pet name
	string random_pet_name;

	if (spawn->IsPlayer()) {
		random_pet_name = string(spawn->GetName());
	} else {
		int16 rand_index = MakeRandomInt(0, spawn->GetZone()->pet_names.size() - 1);
		random_pet_name = spawn->GetZone()->pet_names.at(rand_index);
		LogWrite(PET__DEBUG, 0, "Pets", "Randomize Pet Name: '%s' (rand: %i)", random_pet_name.c_str(), rand_index);
	}

	// If player set various values for the char sheet (pet window)
	if (spawn->IsPlayer()) {
		Player* player = static_cast<Player*>(spawn);
		player->GetInfoStruct()->pet_id = player->GetIDWithPlayerSpawn(pet);
		strcpy(player->GetInfoStruct()->pet_name, random_pet_name.c_str());
		player->GetInfoStruct()->pet_movement = 2;
		player->GetInfoStruct()->pet_behavior = 3;
		player->GetInfoStruct()->pet_health_pct = 1.0f;
		player->GetInfoStruct()->pet_power_pct = 1.0f;
		// Make sure the values get sent to the client
		player->SetCharSheetChanged(true);
	}

	// Set the pets name
	pet->SetName(random_pet_name.c_str());
	// Set the level of the pet to the owners level or max level(if set) if owners level is greater
	if (max_level > 0)
		pet->SetLevel(spawn->GetLevel() >= max_level ? max_level : spawn->GetLevel());
	else
		pet->SetLevel(spawn->GetLevel());
	// Set the max level this pet can reach
	static_cast<NPC*>(pet)->SetMaxPetLevel(max_level);
	// Set the faction of the pet to the same faction as the owner
	pet->SetFactionID(spawn->GetFactionID());
	// Set the spawn as a pet
	pet->SetPet(true);
	// Give a pointer of the owner to the pet
	static_cast<NPC*>(pet)->SetOwner(static_cast<Entity*>(spawn));
	// Give a pointer of the pet to the owner
	static_cast<Entity*>(spawn)->SetCombatPet(static_cast<Entity*>(pet));
	// Set the pet type
	(static_cast<NPC*>(pet))->SetPetType(PET_TYPE_COMBAT);
	// Set the spell id used to create this pet
	(static_cast<NPC*>(pet))->SetPetSpellID(luaspell->spell->GetSpellData()->id);
	// Set the spell tier used to create this pet
	(static_cast<NPC*>(pet))->SetPetSpellTier(luaspell->spell->GetSpellData()->tier);
	// Set the pets spawn type to 6
	pet->SetSpawnType(6);
	// Set the pets brain
	static_cast<NPC*>(pet)->SetBrain(new CombatPetBrain(static_cast<NPC*>(pet)));
	// Check to see if the pet has a subtitle
	if (strlen(pet->GetSubTitle()) > 0) {
		// Add the players name to the front of the sub title
		string pet_subtitle;
		pet_subtitle.append(spawn->GetName()).append("'s ").append(pet->GetSubTitle());
		LogWrite(PET__DEBUG, 0, "Pets", "Pet Subtitle: '%s'", pet_subtitle.c_str());
		// Set the pets subtitle to the new one
		pet->SetSubTitle(pet_subtitle.c_str());
	}
	// Add the "Pet Options" entity command to the pet
	pet->AddSecondaryEntityCommand("Pet Options", 10.0f, "petoptions", "", 0, 0);
	// Set the pet as the return value for this function
	lua_interface->SetSpawnValue(state, pet);
	return 1;
}

int EQ2Emu_lua_SummonDeityPet(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int32 pet_id = lua_interface->GetInt32Value(state, 2);
	LuaSpell* luaspell = lua_interface->GetCurrentSpell(state);
	
	if (!spawn) {
		lua_interface->LogError("LUA SummonDeityPet command error: Spawn is not valid");
		return 0;
	}

	if (!spawn->IsEntity()) {
		lua_interface->LogError("LUA SummonDeityPet command error: Spawn is not an entity");
		return 0;
	}

	if (static_cast<Entity*>(spawn)->GetDeityPet()) {
		if (spawn->IsPlayer()) {
			Client* client = spawn->GetZone()->GetClientBySpawn(spawn);
			if (client)
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You already have a deity pet.");
		}

		lua_interface->LogError("LUA SummonDeityPet command error: spawn already has a pet of this type");
		return 0;
	}
	
	if (pet_id == 0) {
		lua_interface->LogError("LUA SummonDeityPet command error: pet_id can not be set to 0");
		return 0;
	}

	if (!luaspell) {
		lua_interface->LogError("LUA SummonDeityPet command error: valid spell not found, SummonDeityPet can only be used in spell scripts");
		return 0;
	}

	Spawn* pet = spawn->GetZone()->GetSpawn(pet_id);
	if(!pet) {
		lua_interface->LogError("LUA SummonDeityPet command error: Could not find spawn with id of %u.", pet_id);
		return 0;
	}

	if (!pet->IsNPC()) {
		lua_interface->LogError("LUA SummonDeityPet command error: spawn with id of %u is not a npc", pet_id);
		return 0;
	}

	pet->SetX(spawn->GetX());
	pet->SetY(spawn->GetY());
	pet->SetZ(spawn->GetZ());
	pet->SetLocation(spawn->GetLocation());
	pet->SetHeading(spawn->GetHeading());			
	spawn->GetZone()->AddSpawn(pet);

	string random_pet_name;
	int16 rand_index = MakeRandomInt(0, spawn->GetZone()->pet_names.size() - 1);
	random_pet_name = spawn->GetZone()->pet_names.at(rand_index);
	LogWrite(PET__DEBUG, 0, "Pets", "Randomize Pet Name: '%s' (rand: %i)", random_pet_name.c_str(), rand_index);

	pet->SetName(random_pet_name.c_str());
	pet->SetLevel(spawn->GetLevel());
	pet->SetFactionID(spawn->GetFactionID());
	pet->SetPet(true);
	static_cast<NPC*>(pet)->SetPetType(PET_TYPE_DEITY);
	static_cast<NPC*>(pet)->SetOwner(static_cast<Entity*>(spawn));
	static_cast<Entity*>(spawn)->SetDeityPet(static_cast<Entity*>(pet));
	pet->SetSpawnType(6);
	static_cast<NPC*>(pet)->SetBrain(new NonCombatPetBrain(static_cast<NPC*>(pet)));
	static_cast<NPC*>(pet)->SetPetSpellID(luaspell->spell->GetSpellData()->id);
	static_cast<NPC*>(pet)->SetPetSpellTier(luaspell->spell->GetSpellData()->tier);
	
	if (strlen(pet->GetSubTitle()) > 0) {
		string pet_subtitle;
		pet_subtitle.append(spawn->GetName()).append("'s ").append(pet->GetSubTitle());
		LogWrite(PET__DEBUG, 0, "Pets", "Pet Subtitle: '%s'", pet_subtitle.c_str());
		pet->SetSubTitle(pet_subtitle.c_str());
	}

	// deity and cosmetic pets are not attackable
	pet->SetAttackable(false);
	pet->AddSecondaryEntityCommand("Pet Options", 10.0f, "petoptions", "", 0, 0);
	lua_interface->SetSpawnValue(state, pet);
	return 1;
}

int EQ2Emu_lua_SummonCosmeticPet(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int32 pet_id = lua_interface->GetInt32Value(state, 2);
	LuaSpell* luaspell = lua_interface->GetCurrentSpell(state);
	
	if (!spawn) {
		lua_interface->LogError("LUA SummonCosmeticPet command error: Spawn is not valid");
		return 0;
	}

	if (!spawn->IsEntity()) {
		lua_interface->LogError("LUA SummonCosmeticPet command error: Spawn is not an entity");
		return 0;
	}

	if (static_cast<Entity*>(spawn)->GetCosmeticPet()) {
		if (spawn->IsPlayer()) {
			Client* client = spawn->GetZone()->GetClientBySpawn(spawn);
			if (client)
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You already have a cosmetic pet.");
		}

		lua_interface->LogError("LUA SummonCosmeticPet command error: spawn already has a pet of this type");
		return 0;
	}
	
	if (pet_id == 0) {
		lua_interface->LogError("LUA SummonCosmeticPet command error: pet_id can not be set to 0");
		return 0;
	}

	if (!luaspell) {
		lua_interface->LogError("LUA SummonCosmeticPet command error: valid spell not found, SummonCosmeticPet can only be used in spell scripts");
		return 0;
	}

	Spawn* pet = spawn->GetZone()->GetSpawn(pet_id);
	if(!pet) {
		lua_interface->LogError("LUA SummonCosmeticPet command error: Could not find spawn with id of %u.", pet_id);
		return 0;
	}

	if (!pet->IsNPC()) {
		lua_interface->LogError("LUA SummonCosmeticPet command error: spawn with id of %u is not a npc", pet_id);
		return 0;
	}

	pet->SetX(spawn->GetX());
	pet->SetY(spawn->GetY());
	pet->SetZ(spawn->GetZ());
	pet->SetLocation(spawn->GetLocation());
	pet->SetHeading(spawn->GetHeading());			
	spawn->GetZone()->AddSpawn(pet);

	string random_pet_name;
	int16 rand_index = MakeRandomInt(0, spawn->GetZone()->pet_names.size() - 1);
	random_pet_name = spawn->GetZone()->pet_names.at(rand_index);
	LogWrite(PET__DEBUG, 0, "Pets", "Randomize Pet Name: '%s' (rand: %i)", random_pet_name.c_str(), rand_index);

	pet->SetName(random_pet_name.c_str());
	pet->SetLevel(spawn->GetLevel());
	pet->SetFactionID(spawn->GetFactionID());
	pet->SetPet(true);
	static_cast<NPC*>(pet)->SetPetType(PET_TYPE_COSMETIC);
	static_cast<NPC*>(pet)->SetOwner(static_cast<Entity*>(spawn));
	static_cast<Entity*>(spawn)->SetCosmeticPet(static_cast<Entity*>(pet));
	pet->SetSpawnType(6);
	static_cast<NPC*>(pet)->SetBrain(new NonCombatPetBrain(static_cast<NPC*>(pet)));
	static_cast<NPC*>(pet)->SetPetSpellID(luaspell->spell->GetSpellData()->id);
	static_cast<NPC*>(pet)->SetPetSpellTier(luaspell->spell->GetSpellData()->tier);

	if (strlen(pet->GetSubTitle()) > 0) {
		string pet_subtitle;
		pet_subtitle.append(spawn->GetName()).append("'s ").append(pet->GetSubTitle());
		LogWrite(PET__DEBUG, 0, "Pets", "Pet Subtitle: '%s'", pet_subtitle.c_str());
		pet->SetSubTitle(pet_subtitle.c_str());
	}

	pet->SetAttackable(false);
	pet->AddSecondaryEntityCommand("Pet Options", 10.0f, "petoptions", "", 0, 0);
	lua_interface->SetSpawnValue(state, pet);
	return 1;
}

int EQ2Emu_lua_DismissPet(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);

	if (!spawn) {
		lua_interface->LogError("LUA DismissPet command error: spawn is not valid");
		return 0;
	}

	if (!spawn->IsPet()) {
		lua_interface->LogError("LUA DismissPet command error: spawn is not a pet");
		return 0;
	}

	if (!static_cast<NPC*>(spawn)->IsDismissing())
		static_cast<NPC*>(spawn)->GetOwner()->DismissPet(static_cast<NPC*>(spawn));

	return 0;
}

int EQ2Emu_lua_SetQuestFeatherColor(lua_State* state){
	if(!lua_interface)
		return 0;
	
	Quest* quest = lua_interface->GetQuest(state);

	if (!quest) {
		lua_interface->LogError("LUA SetQuestFeatherColor command error: valid quest not found, SetQuestFeatherColor can only be called from a quest script");
		return 0;
	}
	
	int8 feather_color = lua_interface->GetInt8Value(state, 2);
	if(feather_color > 0)
		quest->SetFeatherColor(feather_color);
	
	return 0;
}

int EQ2Emu_lua_RemoveSpawnAccess(lua_State* state){
	if(!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	Spawn* spawn2 = lua_interface->GetSpawn(state, 2);

	if (!spawn) {
		lua_interface->LogError("LUA RemoveSpawnAccess command error: first spawn is not valid");
		return 0;
	}

	if (!spawn2) {
		lua_interface->LogError("LUA RemoveSpawnAccess command error: second spawn is not valid");
		return 0;
	}

	spawn->RemoveSpawnAccess(spawn2);
	return 0;
}

int EQ2Emu_lua_SpawnByLocationID(lua_State* state) {
	if (!lua_interface)
		return 0;

	ZoneServer* zone = lua_interface->GetZone(state);
	int32 location_id = lua_interface->GetInt32Value(state, 2);

	if (!zone) {
		lua_interface->LogError("LUA SpawnByLocationID command error: zone is not valid");
		return 0;
	}
	
	if (location_id == 0) {
		lua_interface->LogError("LUA SpawnByLocationID command error: location id can not be 0");
		return 0;
	}

	SpawnLocation* location = zone->GetSpawnLocation(location_id);
	if (!location) {
		lua_interface->LogError("LUA SpawnByLocationID command error: no location found for the given ID (%u)", location_id);
		return 0;
	}

	if (location->entities[0]) {
		Spawn* spawn = 0;
		if(location->entities[0]->spawn_type == SPAWN_ENTRY_TYPE_NPC)
			spawn = zone->AddNPCSpawn(location, location->entities[0]);
		else if(location->entities[0]->spawn_type == SPAWN_ENTRY_TYPE_GROUNDSPAWN)
			spawn = zone->AddGroundSpawn(location, location->entities[0]);
		else if(location->entities[0]->spawn_type == SPAWN_ENTRY_TYPE_OBJECT)
			spawn = zone->AddObjectSpawn(location, location->entities[0]);
		else if(location->entities[0]->spawn_type == SPAWN_ENTRY_TYPE_WIDGET)
			spawn = zone->AddWidgetSpawn(location, location->entities[0]);
		else if(location->entities[0]->spawn_type == SPAWN_ENTRY_TYPE_SIGN)
			spawn = zone->AddSignSpawn(location, location->entities[0]);
		if (!spawn) {
			LogWrite(ZONE__ERROR, 0, "Zone", "Error adding spawn to zone");
			safe_delete(spawn);
		}

		const char* script = 0;
		for(int x=0;x<3;x++) {
			switch(x) {
				case 0:
					script = world.GetSpawnEntryScript(location->entities[0]->spawn_entry_id);
					break;
				case 1:
					script = world.GetSpawnLocationScript(location->entities[0]->spawn_location_id);
					break;
				case 2:
					script = world.GetSpawnScript(location->entities[0]->spawn_id);
					break;
			}
			if(script && lua_interface->GetSpawnScript(script) != 0) {
				spawn->SetSpawnScript(string(script));
				break;
			}
		}
		if (spawn) {
			zone->CallSpawnScript(spawn, SPAWN_SCRIPT_SPAWN);
			lua_interface->SetSpawnValue(state, spawn);
			return 1;
		}
	}

	return 0;
}

int EQ2Emu_lua_CastEntityCommand(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* caster = lua_interface->GetSpawn(state);
	Spawn* target = lua_interface->GetSpawn(state, 2);
	int32 id = lua_interface->GetInt32Value(state, 3);
	string command = lua_interface->GetStringValue(state, 4);

	if (!caster) {
		lua_interface->LogError("LUA CastEntityCommand command error: caster is not valid");
		return 0;
	}

	if (!target) {
		lua_interface->LogError("LUA CastEntityCommand command error: target is not valid");
		return 0;
	}

	if (!caster->IsPlayer()) {
		lua_interface->LogError("LUA CastEntityCommand command error: caster is not a player");
		return 0;
	}

	EntityCommand* entity_command = caster->GetZone()->GetEntityCommand(id, command);
	if (!entity_command) {
		lua_interface->LogError("LUA CastEntityCommand command error: unable to get a valid EntityCommand with the given ID (%u) and name (%s)", id, command.c_str());
		return 0;
	}

	Client* client = caster->GetZone()->GetClientBySpawn(caster);
	if (!client) {
		lua_interface->LogError("LUA CastEntityCommand command error: unable to get a valid client for the given caster");
		return 0;
	}

	client->GetCurrentZone()->ProcessEntityCommand(entity_command, static_cast<Player*>(caster), target);

	return 0;
}

int EQ2Emu_lua_SetLuaBrain(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);

	if (!spawn) {
		lua_interface->LogError("LUA SetLuaBrain command error: spawn is not valid");
		return 0;
	}

	if (!spawn->IsNPC()) {
		lua_interface->LogError("LUA SetLuaBrain command error: spawn is not a npc");
		return 0;
	}

	static_cast<NPC*>(spawn)->SetBrain(new LuaBrain(static_cast<NPC*>(spawn)));

	return 0;
}

int EQ2Emu_lua_SetBrainTick(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	int16 tick = lua_interface->GetInt16Value(state, 2);

	if (!spawn) {
		lua_interface->LogError("LUA SetBrainTick command error: spawn is not valid");
		return 0;
	}

	if (!spawn->IsNPC()) {
		lua_interface->LogError("LUA SetBrainTick command error: spawn is not a valid npc");
		return 0;
	}

	if (tick < 20) {
		lua_interface->LogError("LUA SetBrainTick command error: tick can not be set below 20 milliseconds");
		return 0;
	}

	static_cast<NPC*>(spawn)->Brain()->SetTick(tick);

	return 0;
}

int EQ2Emu_lua_SetFollowTarget(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	Spawn* target = lua_interface->GetSpawn(state, 2);

	if (!spawn) {
		lua_interface->LogError("LUA SetFollowTarget command error: spawn is not valid");
		return 0;
	}

	// Target can be null, setting follow target to 0 clears it and will cancel follow, so no need to check it

	spawn->SetFollowTarget(target);
	return 0;
}

int EQ2Emu_lua_GetFollowTarget(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	if (!spawn) {
		lua_interface->LogError("LUA GetFollowTarget command error: spawn is not valid");
		return 0;
	}
	Spawn* target = spawn->GetFollowTarget();
	if (target) {
		lua_interface->SetSpawnValue(state, target);
		return 1;
	}

	return 0;
}

int EQ2Emu_lua_ToggleFollow(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	if (!spawn) {
		lua_interface->LogError("LUA ToggleFollow command error: spawn is not valid");
		return 0;
	}

	if (spawn->following)
		spawn->following = false;
	else
		spawn->following = true;

	return 0;
}

int EQ2Emu_lua_IsFollowing(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	if (!spawn) {
		lua_interface->LogError("LUA IsFollowing command error: spawn is not valid");
		return 0;
	}

	lua_interface->SetBooleanValue(state, spawn->following);
	return 1;
}

int EQ2Emu_lua_SetTempVariable(lua_State* state) {
	// As this is unique among the rest of our lua functions as the 3rd param can be of multiple types
	// I will attempt to explain how this function works for future refrence

	// Fist lets make sure lua_interface is valid, if not return out
	if (!lua_interface)
		return 0;

	// Next we grab the first 2 params same as we usually would
	Spawn* spawn = lua_interface->GetSpawn(state);
	string var = lua_interface->GetStringValue(state, 2);

	// DataType will let us know the value type so we can handle it correctly, we set these ourself so the values I used are made up
	// 1 = Spawn
	// 2 = Zone
	// 3 = Item
	// 4 = Quest
	// 5 = String
	// 6 = nil (null)
	int8 dataType = 0;

	// Define pointers for each potential type
	Spawn* spawnVal = 0;
	ZoneServer* zone = 0;
	Item* item = 0;
	Quest* quest = 0;
	string val;

	// Finally we get to grabbing the third param, we will first check to see if it is light user data
	// which is custom data types, in this case it can be Spawn, Zone, Item, or Quest.  Conversation and
	// options window are also light user data be we do not handle those.
	// We check with lua_islightuserdata(lua_State*, index)
	if (lua_islightuserdata(state, 3)) {
		// It is light user data so we will grab the param with lua_touserdata(lua_State*, index)
		// and convert it to LUAUserData*
		LUAUserData* data = static_cast<LUAUserData*>(lua_touserdata(state, 3));
		// Check to make sure the data we got is valid, if not give an error
		if(!data || !data->IsCorrectlyInitialized()){
			lua_interface->LogError("LUA SetTempVariable command error while processing %s", lua_tostring(state, -1));
		}
		// Check if data is a Spawn, if so set our Spawn pointer and the dataType variable
		else if(data->IsSpawn()) {
			spawnVal = data->spawn;
			dataType = 1;
		}
		// Check if data is a Zone, if so set our Zone pointer and the dataType variable
		else if (data->IsZone()) {
			zone = data->zone;
			dataType = 2;
		}
		// Check if data is a Item, if so set our Item pointer and the dataType variable
		else if (data->IsItem()) {
			item = data->item;
			dataType = 3;
		}
		// Check if data is a Ques, if so set our Quest pointer and the dataType variable
		else if (data->IsQuest()) {
			quest = data->quest;
			dataType = 4;
		}
	}
	// Wasn't light user data, check if it is nil(null)
	else if (lua_isnil(state, 3)) {
		// It is nil (null) set the dataType variable, no need to set a pointer in this case
		dataType = 6;
	}
	// Wasn't light user data or nil (null), must be a string
	else {
		// Set the string and dataType variable
		val = lua_interface->GetStringValue(state, 3);
		dataType = 5;
	}

	// We now have all the params, lets check to make sure they are valid
	if (!spawn) {
		lua_interface->LogError("LUA SetTempVariable command error: spawn is not valid");
		return 0;
	}

	if (var.length() == 0) {
		lua_interface->LogError("LUA SetTempVariable command error: var must be set");
		return 0;
	}

	if (dataType == 0) {
		lua_interface->LogError("LUA SetTempVariable command error: unknown data type");
		return 0;
	}

	// All params are valid, lets set the spawns temp variable, this is where dataType variable comes in.
	// AddTempVariable has overloads for all the types of data we support, we need to make sure the third
	// param gets sent to the correct list so we check the value of dataType to know where it should go.
	switch (dataType) {
	case 1:
		// 1 = Spawn
		spawn->AddTempVariable(var, spawnVal);
		break;
	case 2:
		// 2 = Zone
		spawn->AddTempVariable(var, zone);
		break;
	case 3:
		// 3 = Item
		spawn->AddTempVariable(var, item);
		break;
	case 4:
		// 4 = Quest
		spawn->AddTempVariable(var, quest);
		break;
	case 5:
		// 5 = String
		spawn->AddTempVariable(var, val);
		break;
	case 6:
		// 6 = nil (null) so the variable is no longer set, lets remove it from the spawn
		spawn->DeleteTempVariable(var);
		break;
	}

	// And we are done so return out
	return 0;
}

int EQ2Emu_lua_GetTempVariable(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	string var = lua_interface->GetStringValue(state, 2);

	if (!spawn) {
		lua_interface->LogError("LUA GetTempVariable command error: spawn is not valid");
		return 0;
	}

	if (var.length() == 0) {
		lua_interface->LogError("LUA GetTempVariable command error: var must be set");
		return 0;
	}

	// This will tell us the type of data this variable contains, uses the same values as the previous function
	int8 type = spawn->GetTempVariableType(var);

	Spawn* spawn2 = 0;
	ZoneServer* zone = 0;
	Item* item = 0;
	Quest* quest = 0;

	// Set the lua function return value based on the type of data the variable contains
	switch(type) {
	case 1:
		spawn2 = spawn->GetTempVariableSpawn(var);
		if (!spawn2)
			return 0;
		lua_interface->SetSpawnValue(state, spawn2);
		break;
	case 2:
		zone = spawn->GetTempVariableZone(var);
		if (!zone)
			return 0;
		lua_interface->SetZoneValue(state, zone);
		break;
	case 3:
		item = spawn->GetTempVariableItem(var);
		if (!item)
			return 0;
		lua_interface->SetItemValue(state, item);
		break;
	case 4:
		quest = spawn->GetTempVariableQuest(var);
		if (!quest)
			return 0;
		lua_interface->SetQuestValue(state, quest);
		break;
	case 5:
		lua_interface->SetStringValue(state, spawn->GetTempVariable(var).c_str());
		break;
	default:
		// Not a valid type then the variable was not set so return out
		return 0;
	}

	// Return value was set so return out
	return 1;
}

int EQ2Emu_lua_GiveQuestItem(lua_State* state)
{
	if (!lua_interface)
		return 0;

	Quest* quest = lua_interface->GetQuest(state);
	Spawn* spawn = lua_interface->GetSpawn(state, 2);
	string description = lua_interface->GetStringValue(state, 3);
	int32 item_id = lua_interface->GetInt32Value(state, 4);

	if (!quest) {
		lua_interface->LogError("LUA GiveQuestItem command error: quest is not valid");
		lua_interface->SetBooleanValue(state, false);
		return 1;
	}

	if (!spawn) {
		lua_interface->LogError("LUA GiveQuestItem command error: spawn is not valid");
		lua_interface->SetBooleanValue(state, false);
		return 1;
	}

	if (!spawn->IsPlayer()) {
		lua_interface->LogError("LUA GiveQuestItem command error: spawn must be a player");
		lua_interface->SetBooleanValue(state, false);
		return 1;
	}

	if (item_id == 0) {
		lua_interface->LogError("LUA GiveQuestItem command error: item_id is not valid");
		lua_interface->SetBooleanValue(state, false);
		return 1;
	}

	Client* client = spawn->GetZone()->GetClientBySpawn(spawn);
	if (!client) {
		lua_interface->LogError("LUA GiveQuestItem command error: unable to get a valid client from the given spawn");
		lua_interface->SetBooleanValue(state, false);
		return 1;
	}

	Item* item = master_item_list.GetItem(item_id);
	if (!item) {
		lua_interface->LogError("LUA GiveQuestItem command error: unable to get an item from the given id (%u)", item_id);
		lua_interface->SetBooleanValue(state, false);
		return 1;
	}

	PacketStruct* packet = configReader.getStruct("WS_QuestComplete", client->GetVersion());
	if (packet) {
		packet->setDataByName("title", "Quest Reward!");
		packet->setDataByName("name", quest->GetName());
		packet->setDataByName("description", description.c_str());
		packet->setDataByName("level", quest->GetLevel());
		packet->setArrayLengthByName("num_rewards", 1);
		packet->setArrayDataByName("reward_id", item->details.item_id);
		
		if (client->GetVersion() < 860)
			packet->setItemArrayDataByName("item", item, static_cast<Player*>(spawn), 0, 0, -1);
		else if (client->GetVersion() < 1193)
			packet->setItemArrayDataByName("item", item, static_cast<Player*>(spawn));
		else
			packet->setItemArrayDataByName("item", item, static_cast<Player*>(spawn), 0, 0, 2);

		client->QueuePacket(packet->serialize());
		safe_delete(packet);

		lua_interface->SetBooleanValue(state, client->AddItem(item_id, 1));
		client->Message(CHANNEL_COLOR_YELLOW, "You receive \\aITEM %u 0:%s\\/a.", item->details.item_id, item->name.c_str());
		return 1;
	}

	lua_interface->SetBooleanValue(state, false);
	return 1;
}

int EQ2Emu_lua_SetQuestRepeatable(lua_State* state) {
	if (!lua_interface)
		return 0;

	Quest* quest = lua_interface->GetQuest(state);
	if (!quest) {
		lua_interface->LogError("LUA SetQuestRepeatable command error: quest is not valid");
		return 0;
	}

	quest->SetRepeatable(true);
	return 0;
}

int EQ2Emu_lua_GetArchetypeName(lua_State* state) {

	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	if (!spawn) {
		lua_interface->LogError("LUA GetArchetypeName command error: spawn is not valid");
		return 0;
	}
	int8 base_class = classes.GetBaseClass(spawn->GetAdventureClass());
	string ret = classes.GetClassNameCase(base_class);
	if(ret.length() > 0){
		lua_interface->SetStringValue(state, ret.c_str());
		return 1;
	}

	return 0;
}

int EQ2Emu_lua_AddWard(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* target = lua_interface->GetSpawn(state);
	int32 damage = lua_interface->GetInt32Value(state, 2);
	bool keepWard = (lua_interface->GetInt8Value(state, 3) == 1);
	int8 wardType = lua_interface->GetInt8Value(state, 4);
	int8 damageTypes = lua_interface->GetInt8Value(state, 5);

	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	if (!target)
		return 0;

	if (target->IsEntity()) {
		// If the ward is already active remove it
		if (static_cast<Entity*>(target)->GetWard(spell))
			static_cast<Entity*>(target)->RemoveWard(spell);

		// Create new ward info
		WardInfo* ward = new WardInfo;
		ward->Spell = spell;
		ward->BaseDamage = damage;
		ward->DamageLeft = damage;
		ward->keepWard = keepWard;
		ward->WardType = wardType;

		if (wardType == WARD_TYPE_MAGICAL)
			ward->DamageType = damageTypes;

		// Add the ward to the entity
		static_cast<Entity*>(target)->AddWard(spell, ward);
	}

	return 0;
}

int EQ2Emu_lua_AddToWard(lua_State* state) {
	if (!lua_interface)
		return 0;

	int32 amount = lua_interface->GetInt32Value(state);
	LuaSpell* spell = lua_interface->GetCurrentSpell(state);
	WardInfo* ward = 0;

	ZoneServer* zone = spell->caster->GetZone();
	spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
	if (zone->GetSpawnByID(spell->targets.at(0))->IsEntity()) {
		Entity* target = static_cast<Entity*>(zone->GetSpawnByID(spell->targets.at(0)));
		ward = target->GetWard(spell);
		if (ward) {
			ward->DamageLeft += amount;
			if (ward->DamageLeft > ward->BaseDamage)
				ward->DamageLeft = ward->BaseDamage;

			for (int32 i = 0; i < spell->targets.size(); i++) {
				if (Spawn* spawn = zone->GetSpawnByID(spell->targets.at(i)))
					zone->SendHealPacket(ward->Spell->caster, spawn, HEAL_PACKET_TYPE_REGEN_ABSORB, amount, ward->Spell->spell->GetName());
			}
		}
	}
	spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);

	if (ward && spell->caster->IsPlayer())
		ClientPacketFunctions::SendMaintainedExamineUpdate(spell->caster->GetZone()->GetClientBySpawn(spell->caster), spell->slot_pos, ward->DamageLeft, 1);

	return 0;
}

int EQ2Emu_lua_GetWardAmountLeft(lua_State* state) {
	if (!lua_interface)
		return 0;

	LuaSpell* spell = lua_interface->GetCurrentSpell(state);
	if (!spell) {
		lua_interface->LogError("LUA GetWardAmountLeft command error: this command can only be used in a spell script");
		return 0;
	}

	Spawn* target = lua_interface->GetSpawn(state);

	if (target->IsEntity()) {
		WardInfo* ward = static_cast<Entity*>(target)->GetWard(spell);
		if (ward) {
			lua_interface->SetInt32Value(state, ward->DamageLeft);
			return 1;
		}
	}
	return 0;
}

int EQ2Emu_lua_RemoveWard(lua_State* state) {
	if (!lua_interface)
		return 0;

	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	Spawn* target = lua_interface->GetSpawn(state);

	if (target && target->IsEntity()) {
		static_cast<Entity*>(target)->RemoveWard(spell);
	}

	return 0;
}

int EQ2Emu_lua_Interrupt(lua_State* state) 
{
    if (!lua_interface)
		return 0;

	Spawn* caster = lua_interface->GetSpawn(state); // Second param in lua_interface->get functions defaults to 1
	Spawn* target = lua_interface->GetSpawn(state, 2);

	if (!caster)
	{
	    lua_interface->LogError("LUA Interrupt command error: caster is not a valid spawn");
		return 0;
	}

	if(!target) 
	{
	    lua_interface->LogError("LUA Interrupt command error: target is not a valid spawn");
		return 0;
	}

	if (!target->IsEntity())
	{
		lua_interface->LogError("LUA Interrupt command error: Target is not an entity");
		return 0;
	}

	caster->GetZone()->GetSpellProcess()->Interrupted(static_cast<Entity*>(target), caster, SPELL_ERROR_INTERRUPTED);
	
	return 0;
}

int EQ2Emu_lua_Stealth(lua_State* state){
	if(!lua_interface)
		return 0;

	int8 type = lua_interface->GetInt8Value(state);
	Spawn* spawn = lua_interface->GetSpawn(state, 2);
	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	if (!spell){
		lua_interface->LogError("LUA Stealth command error: must be used from spell script");
		return 0;
	}

	ZoneServer* zone = spell->caster->GetZone();

	if (spawn){
		if (spawn->IsEntity()){
			if (type == 1){
				static_cast<Entity*>(spawn)->AddStealthSpell(spell);
				if (!(spell->effect_bitmask & EFFECT_FLAG_STEALTH))
					spell->effect_bitmask += EFFECT_FLAG_STEALTH;
				}
			else if (type == 2){
				static_cast<Entity*>(spawn)->AddInvisSpell(spell);
				if (!(spell->effect_bitmask & EFFECT_FLAG_INVIS))
					spell->effect_bitmask += EFFECT_FLAG_INVIS;
			}
			return 0;
		}
		else{
			lua_interface->LogError("LUA Stealth command error: target override is not Entity");
			return 0;
		}
	}
	else{
		spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
		for (int32 i = 0; i < spell->targets.size(); i++){
			spawn = zone->GetSpawnByID(spell->targets.at(i));
			if (!spawn || !spawn->IsEntity())
				continue;

			if (type == 1){
				static_cast<Entity*>(spawn)->AddStealthSpell(spell);
				if (!(spell->effect_bitmask & EFFECT_FLAG_STEALTH))
					spell->effect_bitmask += EFFECT_FLAG_STEALTH;
			}
			else if (type == 2){
				static_cast<Entity*>(spawn)->AddInvisSpell(spell);
				if (!(spell->effect_bitmask & EFFECT_FLAG_INVIS))
					spell->effect_bitmask += EFFECT_FLAG_INVIS;
			}
			else{
				lua_interface->LogError("LUA Stealth command error: invalid stealth type given");
				break;
			}
		}
		spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);
	}
	return 0;
}

int EQ2Emu_lua_IsStealthed(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if (!spawn){
		lua_interface->LogError("LUA IsStealthed command error: spawn is not valid");
		return 0;}

	if(spawn->IsEntity()){
		lua_interface->SetBooleanValue(state, static_cast<Entity*>(spawn)->IsStealthed());
		return 1;
	}
	else
		lua_interface->LogError("LUA IsStealthed command error: spawn is not entity");
	return 0;
}

int EQ2Emu_lua_IsInvis(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if (!spawn){
		lua_interface->LogError("LUA IsInvis command error: spawn is not valid");
		return 0;}

	if(spawn->IsEntity()){
		lua_interface->SetBooleanValue(state, static_cast<Entity*>(spawn)->IsInvis());
		return 1;
	}
	else
		lua_interface->LogError("LUA IsInvis command error: spawn is not entity");
	return 0;
}

int EQ2Emu_lua_HasItemEquipped(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* player = lua_interface->GetSpawn(state);
	int32 item_id = lua_interface->GetInt32Value(state, 2);
	if (!player->IsPlayer()){
		lua_interface->LogError("LUA HasItemEquipped command error: spawn is not player");
		return 0;
	}
	lua_interface->SetBooleanValue(state, static_cast<Player*>(player)->GetEquipmentList()->HasItem(item_id));
	return 1;
}

int	EQ2Emu_lua_GetEquippedItemBySlot(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* player = lua_interface->GetSpawn(state);
	int8 slot = lua_interface->GetInt8Value(state, 2);

	if(!player){
		lua_interface->LogError("LUA GetEquippedItemBySlot command error: spawn is not valid");
		return 0;
	}
	if(!player->IsPlayer()){
		lua_interface->LogError("LUA GetEquippedItemBySlot command error: spawn is not player");
		return 0;
	}
	Item* item = static_cast<Player*>(player)->GetEquipmentList()->GetItem(slot);
	if(!item){
		lua_interface->LogError("LUA GetEquippedItemBySlot command error: item was not found in slot");
		return 0;
	}
	lua_interface->SetItemValue(state, item);
	return 1;
}

int	EQ2Emu_lua_GetEquippedItemByID(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* player = lua_interface->GetSpawn(state);
	int32 id = lua_interface->GetInt32Value(state, 2);

	if(!player){
		lua_interface->LogError("LUA GetEquippedItemByID command error: spawn is not valid");
		return 0;
	}
	if(!player->IsPlayer()){
		lua_interface->LogError("LUA GetEquippedItemByID command error: spawn is not player");
		return 0;
	}
	Item* item = static_cast<Player*>(player)->GetEquipmentList()->GetItemFromItemID(id);
	if(!item){
		lua_interface->LogError("LUA GetEquippedItemByID command error: equipped item with used id not found");
		return 0;
	}
	lua_interface->SetItemValue(state, item);
	return 1;
}

int	EQ2Emu_lua_GetItemByID(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* player = lua_interface->GetSpawn(state);
	int32 id = lua_interface->GetInt32Value(state, 2);
	int8 count = lua_interface->GetInt8Value(state, 3);
	bool include_bank = lua_interface->GetInt8Value(state, 4);

	if(!player){
		lua_interface->LogError("LUA GetItemByID command error: spawn is not valid");
		return 0;
	}
	if(!player->IsPlayer()){
		lua_interface->LogError("LUA GetItemByID command error: spawn is not player");
		return 0;
	}
	if(!count)
		count = 1;
	Item* item = static_cast<Player*>(player)->GetPlayerItemList()->GetItemFromID(id, count, include_bank);
	if(!item){
		lua_interface->LogError("LUA GetItemByID command error: item with used id not found");
		return 0;
	}
	lua_interface->SetItemValue(state, item);
	return 1;
}

int EQ2Emu_lua_PlayAnimation(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int32 anim = lua_interface->GetInt32Value(state, 2);
	Spawn* spawn2 = lua_interface->GetSpawn(state, 3);
	int8 type = lua_interface->GetInt8Value(state, 4);

	if(!spawn){
		lua_interface->LogError("LUA PlayAnimation command error: spawn is not valid");
		return 0;
	}
	if(anim < 0){
		lua_interface->LogError("LUA PlayAnimation command error: visual state is not valid");
		return 0;
	}
	if(spawn2){
		if (spawn2->IsPlayer()){
			if(type != 1 && type != 2)
				spawn->GetZone()->PlayAnimation(spawn, anim, spawn2);
			else
				spawn->GetZone()->PlayAnimation(spawn, anim, spawn2, type);
			return 0;
		}
		else{
			lua_interface->LogError("LUA PlayAnimation command error: second spawn not a player");
			return 0;
		}
	}
	else
		spawn->GetZone()->PlayAnimation(spawn, anim);
	return 0;
}

int EQ2Emu_lua_IsPet(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(!spawn){
		lua_interface->LogError("LUA IsPet command error: spawn is not valid");
		return 0;
	}
	lua_interface->SetBooleanValue(state, spawn->IsPet());
	return 1;
}

int EQ2Emu_lua_GetOwner(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(!spawn){
		lua_interface->LogError("LUA GetOwner command error: spawn is not valid");
		return 0;
	}
	if(!spawn->IsNPC()){
		lua_interface->LogError("LUA GetOwner command error: spawn is not a NPC");
		return 0;
	}
	if(static_cast<NPC*>(spawn)->GetOwner()){
		lua_interface->SetSpawnValue(state, static_cast<NPC*>(spawn)->GetOwner());
		return 1;
	}
	return 0;
}

int EQ2Emu_lua_SetTarget(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	Spawn* target = lua_interface->GetSpawn(state, 2);
	if(!spawn){
		lua_interface->LogError("LUA SetTarget command error: spawn is not valid");
		return 0;
	}

	if (spawn->IsPlayer()) {
		static_cast<Player*>(spawn)->GetZone()->GetClientBySpawn(spawn)->TargetSpawn(target);
	} else {
		spawn->SetTarget(target);
	}
	return 0;
}

int EQ2Emu_lua_SetInCombat(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	bool val = lua_interface->GetBooleanValue(state, 2);
	if(!spawn){
		lua_interface->LogError("LUA SetInCombat command error: spawn is not valid");
		return 0;
	}
	if(!spawn->IsEntity()){
		lua_interface->LogError("LUA SetInCombat command error: spawn is not an entity");
		return 0;
	}
	static_cast<Entity*>(spawn)->InCombat(val);

	if (val) {
		spawn->ClearRunningLocations();
		spawn->CalculateRunningLocation(true);
	}
	return 0;
}

int EQ2Emu_lua_CompareSpawns(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn1 = lua_interface->GetSpawn(state);
	Spawn* spawn2 = lua_interface->GetSpawn(state, 2);
	if(!spawn1){
		lua_interface->LogError("LUA CompareSpawns command error: first spawn is not valid");
		return 0;
	}
	if(!spawn2){
		lua_interface->LogError("LUA CompareSpawns command error: second spawn is not valid");
		return 0;
	}
	lua_interface->SetBooleanValue(state, (spawn1 == spawn2));
	return 1;
}

int EQ2Emu_lua_Runback(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(!spawn){
		lua_interface->LogError("LUA Runback command error: spawn is not valid");
		return 0;
	}
	if(!spawn->IsNPC()){
		lua_interface->LogError("LUA Runback command error: spawn is not an NPC");
		return 0;
	}
	static_cast<NPC*>(spawn)->Runback();
	return 0;
}

int EQ2Emu_lua_GetRunbackDistance(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(!spawn){
		lua_interface->LogError("LUA GetRunbackDistance command error: spawn is not valid");
		return 0;
	}
	if(!spawn->IsNPC()){
		lua_interface->LogError("LUA GetRunbackDistance command error: spawn is not an NPC");
		return 0;
	}
	lua_interface->SetFloatValue(state, static_cast<NPC*>(spawn)->GetRunbackDistance());
	return 1;
}

int EQ2Emu_lua_IsCasting(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(!spawn){
		lua_interface->LogError("LUA IsCasting command error: spawn is not valid");
		return 0;
	}
	if(!spawn->IsEntity()){
		lua_interface->LogError("LUA IsCasting command error: spawn is not an entity");
		return 0;
	}
	lua_interface->SetBooleanValue(state, static_cast<Entity*>(spawn)->IsCasting());
	return 1;
}

int EQ2Emu_lua_IsMezzed(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(!spawn){
		lua_interface->LogError("LUA IsMezzed command error: spawn is not valid");
		return 0;
	}
	if(!spawn->IsEntity()){
		lua_interface->LogError("LUA IsMezzed command error: spawn is not an entity");
		return 0;
	}
	lua_interface->SetBooleanValue(state, static_cast<Entity*>(spawn)->IsMezzed());
	return 1;
}

int EQ2Emu_lua_IsStunned(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(!spawn){
		lua_interface->LogError("LUA IsStunned command error: spawn is not valid");
		return 0;
	}
	if(!spawn->IsEntity()){
		lua_interface->LogError("LUA IsStunned command error: spawn is not an entity");
		return 0;
	}
	lua_interface->SetBooleanValue(state, static_cast<Entity*>(spawn)->IsStunned());
	return 1;
}

int EQ2Emu_lua_IsMezzedOrStunned(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(!spawn){
		lua_interface->LogError("LUA IsMezzedOrStunned command error: spawn is not valid");
		return 0;
	}
	if(!spawn->IsEntity()){
		lua_interface->LogError("LUA IsMezzedOrStunned command error: spawn is not an entity");
		return 0;
	}
	lua_interface->SetBooleanValue(state, static_cast<Entity*>(spawn)->IsMezzedOrStunned());
	return 1;
}

int EQ2Emu_lua_ProcessSpell(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	Spawn* target = lua_interface->GetSpawn(state, 2);
	float distance = lua_interface->GetFloatValue(state, 3);
	if(!spawn){
		lua_interface->LogError("LUA ProcessSpell command error: spawn is not valid");
		return 0;
	}
	if(!target){
		lua_interface->LogError("LUA ProcessSpell command error: spawn is not an entity");
		return 0;
	}
	if(!spawn->IsNPC()){
		lua_interface->LogError("LUA ProcessSpell command error: spawn is not an NPC");
		return 0;
	}
	if(!target->IsEntity()){
		lua_interface->LogError("LUA ProcessSpell command error: target is not an entity");
		return 0;
	}
	lua_interface->SetBooleanValue(state, static_cast<NPC*>(spawn)->Brain()->ProcessSpell(static_cast<Entity*>(target), distance));
	return 1;
}

int EQ2Emu_lua_ProcessMelee(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	Spawn* target = lua_interface->GetSpawn(state, 2);
	float distance = lua_interface->GetFloatValue(state, 3);
	if(!spawn){
		lua_interface->LogError("LUA ProcessMelee command error: spawn is not valid");
		return 0;
	}
	if(!target){
		lua_interface->LogError("LUA ProcessMelee command error: target is not valid");
		return 0;
	}
	if(!spawn->IsNPC()){
		lua_interface->LogError("LUA ProcessMelee command error: spawn is not an NPC");
		return 0;
	}
	if(!target->IsEntity()){
		lua_interface->LogError("LUA ProcessMelee command error: target is not an entity");
		return 0;
	}
	static_cast<NPC*>(spawn)->Brain()->ProcessMelee(static_cast<Entity*>(target), distance);
	return 0;
}

int EQ2Emu_lua_HasRecovered(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(!spawn){
		lua_interface->LogError("LUA HasRecovered command error: spawn is not valid");
		return 0;
	}
	if(!spawn->IsNPC()){
		lua_interface->LogError("LUA HasRecovered command error: spawn is not an NPC");
		return 0;
	}
	lua_interface->SetBooleanValue(state, static_cast<NPC*>(spawn)->Brain()->HasRecovered());
	return 1;
}

int EQ2Emu_lua_GetEncounterSize(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(!spawn){
		lua_interface->LogError("LUA GetEncounterSize command error: spawn is not valid");
		return 0;
	}
	if(!spawn->IsNPC()){
		lua_interface->LogError("LUA GetEncounterSize command error: spawn is not an NPC");
		return 0;
	}
	lua_interface->SetInt32Value(state, static_cast<NPC*>(spawn)->Brain()->GetEncounterSize());
	return 1;
}

int EQ2Emu_lua_GetMostHated(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(!spawn){
		lua_interface->LogError("LUA GetMostHated command error: spawn is not valid");
		return 0;
	}
	if(!spawn->IsNPC()){
		lua_interface->LogError("LUA GetMostHated command error: spawn is not an NPC");
		return 0;
	}

	Entity* hated = static_cast<NPC*>(spawn)->Brain()->GetMostHated();
	if (hated) {
		lua_interface->SetSpawnValue(state, hated);
		return 1;
	}

	return 0;
}

int EQ2Emu_lua_ClearHate(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	Spawn* hated = lua_interface->GetSpawn(state, 2);
	if(!spawn){
		lua_interface->LogError("LUA ClearHate command error: spawn is not valid");
		return 0;
	}
	if(!spawn->IsNPC()){
		lua_interface->LogError("LUA ClearHate command error: spawn is not NPC");
		return 0;
	}
	if(!hated){
		static_cast<NPC*>(spawn)->Brain()->ClearHate();
		return 0;
	}
	else
	{
		if(!hated->IsEntity()){
			lua_interface->LogError("LUA ClearHate command error: second param is not entity");
			return 0;
		}
		static_cast<NPC*>(spawn)->Brain()->ClearHate(static_cast<Entity*>(hated));
		return 0;
	}
	return 0;
}

int EQ2Emu_lua_ClearEncounter(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	if(!spawn){
		lua_interface->LogError("LUA ClearEncounter command error: spawn is not valid");
		return 0;
	}
	if(!spawn->IsNPC()){
		lua_interface->LogError("LUA ClearEncounter command error: spawn is not an NPC");
		return 0;
	}
	static_cast<NPC*>(spawn)->Brain()->ClearEncounter();
	return 0;
}

int EQ2Emu_lua_GetEncounter(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	if (!spawn) {
		lua_interface->LogError("LUA GetEncounter command error: Spawn is not valid");
		return 0;
	}

	if (!spawn->IsNPC()) {
		lua_interface->LogError("LUA GetEncounter command error: spawn is not a NPC");
		return 0;
	}

	// Temp list to store hate list
	vector<int32>* encounterList = static_cast<NPC*>(spawn)->Brain()->GetEncounter();

	if (encounterList->size() == 0) {
		safe_delete(encounterList);
		return 0;
	}

	lua_createtable(state, encounterList->size(), 0); 
	int newTable = lua_gettop(state); 
	for(int32 i = 0; i < encounterList->size(); i++) {
		Spawn* temp = spawn->GetZone()->GetSpawnByID(encounterList->at(i));
		if (temp)
			lua_interface->SetSpawnValue(state, temp);
		lua_rawseti(state, newTable, i + 1); 
	} 

	safe_delete(encounterList);
	return 1;
}

int EQ2Emu_lua_GetHateList(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	if (!spawn) {
		lua_interface->LogError("LUA GetHateList command error: spawn is not valid");
		return 0;
	}

	if (!spawn->IsNPC()) {
		lua_interface->LogError("LUA GetHateList command error: spawn is not a NPC");
		return 0;
	}

	// Temp list to store hate list
	vector<Entity*>* hateList = static_cast<NPC*>(spawn)->Brain()->GetHateList();

	if (hateList->size() == 0) {
		safe_delete(hateList);
		return 0;
	}

	lua_createtable(state, hateList->size(), 0);
	int newTable = lua_gettop(state);
	for(int32 i = 0; i < hateList->size(); i++) {
		lua_interface->SetSpawnValue(state, hateList->at(i));
		lua_rawseti(state, newTable, i + 1);
	}

	safe_delete(hateList);
	return 1;
}

int EQ2Emu_lua_HasGroup(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	if (!spawn) {
		lua_interface->LogError("LUA HasGroup command error: spawn is not valid");
		return 0;
	}

	if (spawn->IsPlayer()) {
		if (static_cast<Player*>(spawn)->GetGroupMemberInfo() && world.GetGroupManager()->GetGroupSize(static_cast<Player*>(spawn)->GetGroupMemberInfo()->group_id) > 1)
			lua_interface->SetBooleanValue(state, true);
		else
			lua_interface->SetBooleanValue(state, false);

		return 1;
	}
	else {
		lua_interface->SetBooleanValue(state, spawn->HasSpawnGroup());
		return 1;
	}
}

int EQ2Emu_lua_SetCompleteFlag(lua_State* state) {
	if (!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);

	if (!quest) {
		lua_interface->LogError("LUA SetCompleteFlag command error: quest is not valid");
		return 0;
	}

	quest->SetCompletedFlag(true);
	return 0;
}

int EQ2Emu_lua_HasSpellEffect(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	int32 spellID = lua_interface->GetInt32Value(state, 2);
	int8 tier = lua_interface->GetInt8Value(state, 3);

	if (!spawn) {
		lua_interface->LogError("LUA HasSpellEffect command error: spawn is not valid");
		return 0;
	}

	if (!spawn->IsEntity()) {
		lua_interface->LogError("LUA HasSpellEffect command error: spawn is not an entity");
		return 0;
	}

	if (spellID == 0) {
		lua_interface->LogError("LUA HasSpellEffect command error: spell id is not valid");
		return 0;
	}

	SpellEffects* effect = static_cast<Entity*>(spawn)->GetSpellEffect(spellID);
	if (effect) {
		if (tier > 0) {
			// If a tier was passed chec to see if it is the same as the effect
			if (tier == effect->tier)
				lua_interface->SetBooleanValue(state, true);
			else
				lua_interface->SetBooleanValue(state, false);

			return 1;
		}
		else {
			// Have an effect but no tier was passed so return true
			lua_interface->SetBooleanValue(state, true);
		}

		return 1;
	}

	// no effect so return false
	lua_interface->SetBooleanValue(state, false);
	return 1;
}

int EQ2Emu_lua_AddSpawnIDAccess(lua_State* state){
	if (!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int32 id = lua_interface->GetInt32Value(state, 2);
	ZoneServer* zone = lua_interface->GetZone(state, 3);
	vector<Spawn*> list;

	if(!spawn){
		lua_interface->LogError("LUA AddSpawnIDAccess command error: spawn is not valid");
		return 0;
	}
	//If zone not provided, use spawn's zone
	if(!zone)
		zone = spawn->GetZone();

	list = zone->GetSpawnsByID(id);
	if(list.size() == 0){
		lua_interface->LogError("LUA AddSpawnIDAccess command error: GetSpawnsByID returned no spawns");
		return 0;
	}

	vector<Spawn*>::iterator itr = list.begin();
	for (int8 i=0; i < list.size(); i++){
		Spawn* spawn2 = itr[i];
		if(spawn2)
			spawn2->AddAllowAccessSpawn(spawn);
	}

	return 0;
}

int EQ2Emu_lua_RemoveSpawnIDAccess(lua_State* state){
	if (!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int32 id = lua_interface->GetInt32Value(state, 2);
	ZoneServer* zone = lua_interface->GetZone(state, 3);

	if(!spawn){
		lua_interface->LogError("LUA RemoveSpawnIDAccess command error: spawn is not valid");
		return 0;
	}
	//If zone not provided, use spawn's zone
	if(!zone)
		zone = spawn->GetZone();

	vector<Spawn*> list = zone->GetSpawnsByID(id);
	vector<Spawn*>::iterator itr = list.begin();
	if(list.size() == 0){
		lua_interface->LogError("LUA RemoveSpawnIDAccess command error: GetSpawnsByID returned no spawns");
		return 0;
	}
	for (int8 i=0; i < list.size(); i++){
		Spawn* spawn2 = itr[i];
		if(spawn2)
			spawn2->RemoveSpawnAccess(spawn);
	}

	return 0;
}

int EQ2Emu_lua_SetQuestYellow(lua_State* state){
	if (!lua_interface)
		return 0;
	Quest* quest = lua_interface->GetQuest(state);
	if (!quest){
		lua_interface->LogError("LUA SetQuestYellow command error: quest is not valid");
		return 0;
	}
	quest->SetYellowName(true);
	return 0;
}

int EQ2Emu_lua_CanReceiveQuest(lua_State* state){
	if (!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	int32 quest_id = lua_interface->GetInt32Value(state, 2);
	if (!spawn){
		lua_interface->LogError("LUA CanReceieveQuest command error: spawn is not valid");
		return 0;
	}
	if (!spawn->IsPlayer()){
		lua_interface->LogError("LUA CanReceieveQuest command error: spawn is not player");
		return 0;
	}
	lua_interface->SetBooleanValue(state, static_cast<Player*>(spawn)->CanReceiveQuest(quest_id));
	return 1;
}

int EQ2Emu_lua_SetSuccessTimer(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);

	if (!spawn) {
		lua_interface->LogError("LUA SetSuccessTimer command error: spawn is not valid");
		return 0;
	}
	if (!spawn->IsPlayer()) {
		lua_interface->LogError("LUA SetSuccessTimer command error: spawn is not a player");
		return 0;
	}

	ZoneServer* zone = spawn->GetZone();
	if (!zone) {
		lua_interface->LogError("LUA SetSuccessTimer command error: unable to get a valid zone for the given spawn");
		return 0;
	}

	Instance_Type iType = zone->GetInstanceType();
	if (iType == SOLO_LOCKOUT_INSTANCE ||
		iType == GROUP_LOCKOUT_INSTANCE ||
		iType == RAID_LOCKOUT_INSTANCE ||
		iType == SOLO_PERSIST_INSTANCE ||
		iType == GROUP_PERSIST_INSTANCE ||
		iType == RAID_PERSIST_INSTANCE) {
			InstanceData* data = static_cast<Player*>(spawn)->GetCharacterInstances()->FindInstanceByZoneID(spawn->GetZone()->GetZoneID());
			if (data) {
				// Check to see if the timer has already been set, if it has return out.
				if (Timer::GetUnixTimeStamp() <= data->last_success_timestamp + data->success_lockout_time)
					return 0;


				database.UpdateCharacterInstance(static_cast<Player*>(spawn)->GetCharacterID(), string(spawn->GetZone()->GetZoneName()), spawn->GetZone()->GetInstanceID(), 1, Timer::GetUnixTimeStamp());
				data->last_success_timestamp = Timer::GetUnixTimeStamp();
				Client* client = spawn->GetZone()->GetClientBySpawn(spawn);
				if (client) {
					string time_msg = "";
					int32 time = data->success_lockout_time;
					int16 hour;
					int8 min;
					int8 sec;
					hour = time/3600;
					time = time%3600;
					min = time/60;
					time = time%60;
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

					client->Message(CHANNEL_COLOR_YELLOW, "The success zone reuse timer for %s has been set.  You can return in%s.", data->zone_name.c_str(), time_msg.c_str());
				}
			}
			else
				lua_interface->LogError("LUA SetSuccessTimer command error: unable to get instance data for instance %u for player %s", spawn->GetZone()->GetInstanceID(), spawn->GetName());
	}
	else
		lua_interface->LogError("LUA SetSuccessTimer command error: current zone for given spawn is not a lockout or persistent instance");


	return 0;
}

int EQ2Emu_lua_SetFailureTimer(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);

	if (!spawn) {
		lua_interface->LogError("LUA SetFailureTimer command error: spawn is not valid");
		return 0;
	}
	if (!spawn->IsPlayer()) {
		lua_interface->LogError("LUA SetFailureTimer command error: spawn is not a player");
		return 0;
	}

	ZoneServer* zone = spawn->GetZone();
	if (!zone) {
		lua_interface->LogError("LUA SetFailureTimer command error: unable to get a valid zone for the given spawn");
		return 0;
	}

	Instance_Type iType = zone->GetInstanceType();
	if (iType == SOLO_LOCKOUT_INSTANCE ||
		iType == GROUP_LOCKOUT_INSTANCE ||
		iType == RAID_LOCKOUT_INSTANCE ||
		iType == SOLO_PERSIST_INSTANCE ||
		iType == GROUP_PERSIST_INSTANCE ||
		iType == RAID_PERSIST_INSTANCE) {
			InstanceData* data = static_cast<Player*>(spawn)->GetCharacterInstances()->FindInstanceByZoneID(spawn->GetZone()->GetZoneID());
			if (data) {
				// Check to see if the timer has already been set, if it has return out.
				if (Timer::GetUnixTimeStamp() <= data->last_failure_timestamp + data->failure_lockout_time)
					return 0;

				database.UpdateCharacterInstance(static_cast<Player*>(spawn)->GetCharacterID(), string(spawn->GetZone()->GetZoneName()), spawn->GetZone()->GetInstanceID(), 2, Timer::GetUnixTimeStamp());
				data->last_failure_timestamp = Timer::GetUnixTimeStamp();
				Client* client = spawn->GetZone()->GetClientBySpawn(spawn);
				if (client) {
					string time_msg = "";
					int32 time = data->failure_lockout_time;
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

					client->Message(CHANNEL_COLOR_YELLOW, "The failure zone reuse timer for %s has been set. You can return in%s", data->zone_name.c_str(), time_msg.c_str());
				}
			}
			else
				lua_interface->LogError("LUA SetFailureTimer command error: unable to get instance data for instance %u for player %s", spawn->GetZone()->GetInstanceID(), spawn->GetName());
	}
	else
		lua_interface->LogError("LUA SetFailureTimer command error: current zone for given spawn is not a lockout or persistent instance");

	return 0;
}

int EQ2Emu_lua_IsGroundSpawn(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	if (!spawn) {
		lua_interface->LogError("LUA IsGroundSpawn command error: not a valid spawn");
		return 0;
	}

	lua_interface->SetBooleanValue(state, spawn->IsGroundSpawn());
	return 1;
}

int EQ2Emu_lua_CanHarvest(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* player = lua_interface->GetSpawn(state);
	Spawn* ground = lua_interface->GetSpawn(state, 2);

	if (!player) {
		lua_interface->LogError("LUA CanHarvest command error: not a valid spawn");
		return 0;
	}

	if (!player->IsPlayer()) {
		lua_interface->LogError("LUA CanHarvest command error: spawn is not a player");
		return 0;
	}

	if (!ground) {
		lua_interface->LogError("LUA CanHarvest command error: not a valid spawn");
		return 0;
	}

	if (!ground->IsGroundSpawn()) {
		lua_interface->LogError("LUA CanHarvest command error: spawn is not a ground spawn");
		return 0;
	}

	vector<GroundSpawnEntry*>* groundspawn_entries = player->GetZone()->GetGroundSpawnEntries(static_cast<GroundSpawn*>(ground)->GetGroundSpawnEntryID());

	if(!groundspawn_entries) {
		lua_interface->LogError("LUA CanHarvest command error: No groundspawn entries assigned to groundspawn id: %u", static_cast<GroundSpawn*>(ground)->GetGroundSpawnEntryID());
		return 0;
	}

	Skill* skill = 0;
	string collection_skill = string(static_cast<GroundSpawn*>(ground)->GetCollectionSkill());
	if (collection_skill == "Collecting")
		skill = static_cast<Player*>(player)->GetSkillByName("Gathering");
	else
		skill = static_cast<Player*>(player)->GetSkillByName(collection_skill.c_str());
	
	if(!skill) {
		lua_interface->LogError("LUA CanHarvest command error: Player '%s' lacks the skill: '%s'", player->GetName(), collection_skill.c_str());
		return 0;
	}

	

	vector<GroundSpawnEntry*>::iterator itr;
	GroundSpawnEntry* entry = 0;
	bool can_harvest = false;
	sint32 min_skill = -1;

	// first, iterate through groundspawn_entries, discard tables player cannot use
	for(itr = groundspawn_entries->begin(); itr != groundspawn_entries->end(); ++itr)
	{
		entry = *itr;

		if (min_skill == -1 || entry->min_skill_level < min_skill)
			min_skill = entry->min_skill_level;
		// if player lacks skill, skip table
		if( entry->min_skill_level > skill->current_val )
			continue;
		// if bonus, but player lacks level, skip table
		if(entry->bonus_table && (player->GetLevel() < entry->min_adventure_level))
			continue;
		
		can_harvest = true;
		break;
	}

	lua_interface->SetBooleanValue(state, can_harvest);

	// If false, send the message to the client
	if (!can_harvest) {
		Client* client = player->GetZone()->GetClientBySpawn(player);
		if (client) {
			string msg = "You do not have enough skill to ";
			if(collection_skill == "Gathering" || collection_skill == "Collecting")
				msg.append("gather");
			else if(collection_skill == "Mining")
				msg.append("mine");
			else if(collection_skill == "Trapping")
				msg.append("trap");
			else if(collection_skill == "Foresting")
				msg.append("forest");
			else if(collection_skill == "Fishing")
				msg.append("catch");

			msg.append(" the %s. It requires %i %s skill, and your skill is %i.");
			client->Message(CHANNEL_HARVESTING_WARNINGS, msg.c_str(), ground->GetName(), min_skill, skill->name.data.c_str(), skill->current_val);
			// You do not have enough skill to catch the band of fish.  It requires 20 Fishing skill, and your skill is 12.
		}
	}
	return 1;
}

int EQ2Emu_lua_HasRecipeBook(lua_State* state){
	if (!lua_interface)
		return 0;

	Spawn* player = lua_interface->GetSpawn(state);
	int32 recipe_id = lua_interface->GetInt32Value(state, 2);

	if(!player){
		lua_interface->LogError("LUA HasRecipeBook command error, Spawn is not valid");
		return 0;
	}
	if(!player->IsPlayer()){
		lua_interface->LogError("LUA HasRecipeBook command error, Spawn is not a player");
		return 0;
	}
	
	bool ret = static_cast<Player*>(player)->HasRecipeBook(recipe_id);
	lua_interface->SetBooleanValue(state, ret);
	return 1;
}

int EQ2Emu_lua_SummonDumbFirePet(lua_State* state) {
	// Check to see if we have a valid lua_interface
	if(!lua_interface)
		return 0;

	// Get the spawn that is getting the pet
	Spawn* spawn = lua_interface->GetSpawn(state);
	Spawn* target = lua_interface->GetSpawn(state, 2);
	// Get the DB ID of the pet
	int32 pet_id = lua_interface->GetInt32Value(state, 3);

	float x = lua_interface->GetFloatValue(state, 4);
	float y = lua_interface->GetFloatValue(state, 5);
	float z = lua_interface->GetFloatValue(state, 6);
	// Get the spell that this command was called from
	LuaSpell* luaspell = lua_interface->GetCurrentSpell(state);
	
	// Check to make sure the spawn pointer is valid
	if (!spawn) {
		lua_interface->LogError("LUA SummonDumbFirePet command error: Spawn is not valid");
		return 0;
	}

	// Check to make sure the spawn is an entity
	if (!spawn->IsEntity()) {
		lua_interface->LogError("LUA SummonDumbFirePet command error: Spawn is not an entity");
		return 0;
	}

	if (!target) {
		lua_interface->LogError("LUA SummonDumbFirePet command error: target is not valid");
		return 0;
	}

	if (!target->IsEntity()) {
		lua_interface->LogError("LUA SummonDumbFirePet command error: target is not an entity");
		return 0;
	}
	
	// Check to see if the DB ID for the pet is set
	if (pet_id == 0) {
		lua_interface->LogError("LUA SummonDumbFirePet command error: pet_id can not be set to 0");
		return 0;
	}

	// Check to see if the pointer to the spell is valid
	if (!luaspell) {
		lua_interface->LogError("LUA SummonDumbFirePet command error: valid spell not found, SummonPet can only be used in spell scripts");
		return 0;
	}

	// Get a pointer to a spawn with the given DB ID and check if the pointer is valid
	Spawn* pet = spawn->GetZone()->GetSpawn(pet_id);
	if(!pet) {
		lua_interface->LogError("LUA SummonDumbFirePet command error: Could not find spawn with id of %u.", pet_id);
		return 0;
	}

	// Check to make sure the pet is an npc
	if (!pet->IsNPC()) {
		lua_interface->LogError("LUA SummonDumbFirePet command error: id (%u) did not point to a npc", pet_id);
		return 0;
	}

	if (x == 0)
		x = spawn->GetX();

	if (y == 0)
		y = spawn->GetY();

	if (z == 0)
		z = spawn->GetZ();

	// Spawn the pet at the same location as the owner
	pet->SetX(x);
	pet->SetY(y);
	pet->SetZ(z);
	pet->SetLocation(spawn->GetLocation());
	pet->SetHeading(spawn->GetHeading());			
	spawn->GetZone()->AddSpawn(pet);

	/*
	const char* spawn_script = world.GetSpawnScript(pet_id);	
	if(spawn_script && lua_interface->GetSpawnScript(spawn_script) != 0){
		spawn->SetSpawnScript(string(spawn_script));
		zone->CallSpawnScript(spawn, SPAWN_SCRIPT_SPAWN);
	}*/

	// Get a random pet name
	string random_pet_name;
	int16 rand_index = MakeRandomInt(0, spawn->GetZone()->pet_names.size() - 1);
	random_pet_name = spawn->GetZone()->pet_names.at(rand_index);
	LogWrite(PET__DEBUG, 0, "Pets", "Randomize Pet Name: '%s' (rand: %i)", random_pet_name.c_str(), rand_index);

	// Set the pets name
	pet->SetName(random_pet_name.c_str());
	// Set the level of the pet to the owners level
	pet->SetLevel(spawn->GetLevel());
	// Set the faction of the pet to the same faction as the owner
	pet->SetFactionID(spawn->GetFactionID());
	// Set the spawn as a pet
	pet->SetPet(true);
	// Give a pointer of the owner to the pet
	static_cast<NPC*>(pet)->SetOwner(static_cast<Entity*>(spawn));
	
	// Set the pet type
	static_cast<NPC*>(pet)->SetPetType(PET_TYPE_DUMBFIRE);
	// Set the spell id used to create this pet
	static_cast<NPC*>(pet)->SetPetSpellID(luaspell->spell->GetSpellData()->id);
	// Set the spell tier used to create this pet
	static_cast<NPC*>(pet)->SetPetSpellTier(luaspell->spell->GetSpellData()->tier);
	// Set the pets spawn type to 6
	pet->SetSpawnType(6);
	// Set the pets brain
	static_cast<NPC*>(pet)->SetBrain(new DumbFirePetBrain(static_cast<NPC*>(pet), static_cast<Entity*>(target), luaspell->spell->GetSpellDuration() * 100));
	// Check to see if the pet has a subtitle
	if (strlen(pet->GetSubTitle()) > 0) {
		// Add the players name to the front of the sub title
		string pet_subtitle;
		pet_subtitle.append(spawn->GetName()).append("'s ").append(pet->GetSubTitle());
		LogWrite(PET__DEBUG, 0, "Pets", "Pet Subtitle: '%s'", pet_subtitle.c_str());
		// Set the pets subtitle to the new one
		pet->SetSubTitle(pet_subtitle.c_str());
	}

	// Set the pet as the return value for this function
	lua_interface->SetSpawnValue(state, pet);
	return 1;
}

int EQ2Emu_lua_SpawnMove(lua_State* state){
	if(!lua_interface)
		return 0;
	Spawn* spawn = lua_interface->GetSpawn(state);
	Spawn* player = lua_interface->GetSpawn(state, 2);
	float max_distance = lua_interface->GetFloatValue(state, 3);
	string type = lua_interface->GetStringValue(state, 4);

	if(!spawn || spawn->IsPlayer()) {
		lua_interface->LogError("LUA SpawnMove command error: first param spawn is not valid or is player");
		return 0;
	}

	if(!player || !player->IsPlayer()) {
		lua_interface->LogError("LUA SpawnMove command error: second param is not player");
		return 0;
	}

	Client* client = 0;
	if(player->GetZone())
		client = player->GetZone()->GetClientBySpawn(player);

	if(!client){
		lua_interface->LogError("LUA SpawnMove command error: could not find client");
		return 0;
	}

	//Set max_distance to default if not set or not proper value
	if(max_distance <= 0)
		max_distance = 500;
	PacketStruct* packet = configReader.getStruct("WS_MoveObjectMode", client->GetVersion());
	if(packet){
		float unknown2_3 = 0;
		int8 placement_mode = 0;
		if(type == "wall"){
			placement_mode = 2;
			unknown2_3 = 150;
		}
		else if(type == "ceiling")
			placement_mode = 1;
		packet->setDataByName("placement_mode", placement_mode);
		packet->setDataByName("spawn_id", static_cast<Player*>(player)->GetIDWithPlayerSpawn(spawn));
		packet->setDataByName("model_type", spawn->GetModelType());
		packet->setDataByName("unknown", 1); //size
		packet->setDataByName("unknown2", 1); //size 2
		packet->setDataByName("unknown2", .5, 1); //size 3
		packet->setDataByName("unknown2", 3, 2);
		packet->setDataByName("unknown2", unknown2_3, 3);
		packet->setDataByName("max_distance", max_distance);
		packet->setDataByName("CoEunknown", 0xFFFFFFFF);
		client->QueuePacket(packet->serialize());
		safe_delete(packet);
	}
	return 0;
}

int EQ2Emu_lua_GetItemType(lua_State* state){
	if(!lua_interface)
		return 0;
	Item* item = lua_interface->GetItem(state);

	if(!item){
		lua_interface->LogError("LUA GetItemType command error: item pointer is not valid");
		return 0;
	}

	lua_interface->SetInt32Value(state, item->generic_info.item_type);
	return 1;
}

int EQ2Emu_lua_AddTransportSpawn(lua_State* state){
	if(!lua_interface)
		return 0;
	
	Spawn* spawn = lua_interface->GetSpawn(state);

	if(!spawn){
		lua_interface->LogError("LUA AddTransportSpawn command error: spawn is not valid");
		return 0;
	}

	if(spawn->GetZone())
		spawn->GetZone()->AddTransportSpawn(spawn);

	return 0;
}

int EQ2Emu_lua_GetSkillValue(lua_State* state) {
	if (!lua_interface)
		return 0;

	Skill* skill = lua_interface->GetSkill(state);
	if (!skill) {
		lua_interface->LogError("LUA GetSkillValue command error: skill is not valid");
		return 0;
	}

	lua_interface->SetInt32Value(state, skill->current_val);
	return 1;
}

int EQ2Emu_lua_GetSkillMaxValue(lua_State* state) {
	if (!lua_interface)
		return 0;

	Skill* skill = lua_interface->GetSkill(state);
	if (!skill) {
		lua_interface->LogError("LUA GetSkillMaxValue command error: skill is not valid");
		return 0;
	}

	lua_interface->SetInt32Value(state, skill->max_val);
	return 1;
}

int EQ2Emu_lua_GetSkillName(lua_State* state) {
	if (!lua_interface)
		return 0;

	Skill* skill = lua_interface->GetSkill(state);
	if (!skill) {
		lua_interface->LogError("LUA GetSkillName command error: skill is not valid");
		return 0;
	}

	lua_interface->SetStringValue(state, skill->name.data.c_str());
	return 1;
}

int EQ2Emu_lua_SetSkillMaxValue(lua_State* state) {
	if (!lua_interface)
		return 0;

	Skill* skill = lua_interface->GetSkill(state);
	int16 value = lua_interface->GetInt16Value(state, 2);
	if (!skill) {
		lua_interface->LogError("LUA SetSkillMaxValue command error: skill is not valid");
		return 0;
	}

	skill->max_val = value;
	if (skill->max_val < skill->current_val)
		skill->current_val = skill->max_val;

	return 0;
}

int EQ2Emu_lua_SetSkillValue(lua_State* state) {
	if (!lua_interface)
		return 0;

	Skill* skill = lua_interface->GetSkill(state);
	int16 value = lua_interface->GetInt16Value(state, 2);
	if (!skill) {
		lua_interface->LogError("LUA SetSkillValue command error: skill is not valid");
		return 0;
	}

	if (value > skill->max_val)
		skill->current_val = skill->max_val;
	else
		skill->current_val = value;

	return 0;
}

int EQ2Emu_lua_GetSkill(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	string name = lua_interface->GetStringValue(state, 2);

	if (!spawn) {
		lua_interface->LogError("LUA GetSkill command error: spawn is not valid");
		return 0;
	}

	if (!spawn->IsEntity()) {
		lua_interface->LogError("LUA GetSkill command error: spawn is not a valid entity");
		return 0;
	}

	Skill* skill = static_cast<Entity*>(spawn)->GetSkillByName(name.c_str());
	if (skill) {
		lua_interface->SetSkillValue(state, skill);
		return 1;
	}

	return 0;
}

int EQ2Emu_lua_AddProc(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	int8 type = lua_interface->GetInt8Value(state, 2);
	float chance = lua_interface->GetFloatValue(state, 3);
	Item* item = lua_interface->GetItem(state, 4);
	LuaSpell* spell = 0;

	if (!spawn || !spawn->IsEntity()) {
		lua_interface->LogError("LUA AddProc command error: spawn is not a valid entity");
		return 0;
	}

	if (!item)
		spell = lua_interface->GetCurrentSpell(state);

	if (!item && !spell) {
		lua_interface->LogError("LUA AddProc command error: can only use with an item provided or inside a spell script");
		return 0;
	}

	static_cast<Entity*>(spawn)->AddProc(type, chance, item, spell);

	return 0;
}

int EQ2Emu_lua_RemoveProc(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	Item* item = lua_interface->GetItem(state, 2);
	LuaSpell* spell = 0;

	if (!spawn) {
		lua_interface->LogError("LUA RemoveProc command error: spawn is not valid");
		return 0;
	}

	if (!spawn->IsEntity()) {
		lua_interface->LogError("LUA RemoveProc command error: spawn is not a valid entity");
		return 0;
	}

	if (!item)
		spell = lua_interface->GetCurrentSpell(state);

	if (!item && !spell) {
		lua_interface->LogError("LUA RemoveProc command error: can only use with an item provided or inside a spell script");
		return 0;
	}

	static_cast<Entity*>(spawn)->RemoveProc(item, spell);

	return 0;
}

int EQ2Emu_lua_Knockback(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* target_spawn = lua_interface->GetSpawn(state);
	Spawn* spawn = lua_interface->GetSpawn(state, 2);
	float vertical = lua_interface->GetFloatValue(state, 4);
	float horizontal = lua_interface->GetFloatValue(state, 5);
	bool use_heading = lua_interface->GetInt8Value(state, 6) == 1 ? true : false;

	if (!target_spawn) {
		lua_interface->LogError("LUA Knockback command error: target_spawn is not valid");
		return 0;
	}

	if (!spawn) {
		lua_interface->LogError("LUA Knockback command error: spawn is not valid");
		return 0;
	}

	if (spawn->IsPlayer() && (vertical != 0 || horizontal != 0)) {
		Client* client = spawn->GetZone()->GetClientBySpawn(spawn);
		PacketStruct* packet = configReader.getStruct("WS_PlayerKnockback", client->GetVersion());
		if (packet) {
			packet->setDataByName("target_x", target_spawn->GetX());
			packet->setDataByName("target_y", target_spawn->GetY());
			packet->setDataByName("target_z", target_spawn->GetZ());
			packet->setDataByName("vertical_movement", vertical);
			packet->setDataByName("horizontal_movement", horizontal);
			if (use_heading)
				packet->setDataByName("use_player_heading", 1);

			client->QueuePacket(packet->serialize());
		}
		safe_delete(packet);
	}

	return 0;
}

int EQ2Emu_lua_IsEpic(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	if (!spawn) {
		lua_interface->LogError("LUA IsEpic command error: spawn is not valid");
		return 0;
	}

	lua_interface->SetBooleanValue(state, (spawn->GetHeroic() >= 2));
	return 1;
}

int EQ2Emu_lua_ProcDamage(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* caster = lua_interface->GetSpawn(state);
	Spawn* target = lua_interface->GetSpawn(state, 2);
	string name = lua_interface->GetStringValue(state, 3);
	int8 dmg_type = lua_interface->GetInt8Value(state, 4);	
	int32 low_damage = lua_interface->GetInt32Value(state, 5);
	int32 high_damage = lua_interface->GetInt32Value(state, 6);
	string success_msg = lua_interface->GetStringValue(state, 7);
	string effect_msg = lua_interface->GetStringValue(state, 8);
	
	if (!caster) {
		lua_interface->LogError("LUA ProcDamage command error: caster is not a valid spawn");
		return 0;
	}

	if (!caster->IsEntity()) {
		lua_interface->LogError("LUA ProcDamage command error: caster is not an entity");
		return 0;
	}

	if (!target) {
		lua_interface->LogError("LUA ProcDamage command error: target is not a valid spawn");
		return 0;
	}

	if (!target->IsEntity()) {
		lua_interface->LogError("LUA ProcDamage command error: target is not an entity");
		return 0;
	}

	if (name.length() == 0) {
		lua_interface->LogError("LUA ProcDamage command error: name is empty");
		return 0;
	}

	static_cast<Entity*>(caster)->ProcAttack(target, dmg_type, low_damage, high_damage, name, success_msg, effect_msg);
	return 0;
}

int EQ2Emu_lua_ProcHeal(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* caster = lua_interface->GetSpawn(state);
	Spawn* target = lua_interface->GetSpawn(state, 2);
	string name = lua_interface->GetStringValue(state, 3);
	string heal_type = lua_interface->GetStringValue(state, 4);
	int32 low_heal = lua_interface->GetInt32Value(state, 5);
	int32 high_heal = lua_interface->GetInt32Value(state, 6);

	if (!caster) {
		lua_interface->LogError("LUA ProcHeal command error: caster is not a valid spawn");
		return 0;
	}

	if (!caster->IsEntity()) {
		lua_interface->LogError("LUA ProcHeal command error: caster is not an entity");
		return 0;
	}

	if (!target) {
		lua_interface->LogError("LUA ProcHeal command error: target is not a valid spawn");
		return 0;
	}

	if (!target->IsEntity()) {
		lua_interface->LogError("LUA ProcHeal command error: target is not an entity");
		return 0;
	}

	if (name.length() == 0) {
		lua_interface->LogError("LUA ProcHeal command error: name is empty");
		return 0;
	}

	static_cast<Entity*>(caster)->ProcHeal(target, heal_type, low_heal, high_heal, name);
	return 0;
}

int EQ2Emu_lua_GetSkillIDByName(lua_State* state) {
	if(!lua_interface)
		return 0;

	string name = lua_interface->GetStringValue(state);
	
	if(name.length() == 0){
		lua_interface->LogError("LUA GetSkillIDByName command error: name param was not set");
		return 0;
	}

	Skill* skill = master_skill_list.GetSkillByName(name.c_str());
	if(!skill){
		lua_interface->LogError("LUA GetSkillIDByName command error: skill with name of %s not found", name.c_str());
		return 0;
	}

	lua_interface->SetInt32Value(state, skill->skill_id);
	return 1;
}

int EQ2Emu_lua_IsHeroic(lua_State* state) {
	if(!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);

	if(!spawn){
		lua_interface->LogError("LUA IsHeroic command error: spawn is not valid");
		return 0;
	}

	lua_interface->SetBooleanValue(state, spawn->GetHeroic() == 1);
	return 1;
}

int EQ2Emu_lua_LastSpellAttackHit(lua_State* state) {
	if(!lua_interface)
		return 0;

	LuaSpell* luaspell = lua_interface->GetCurrentSpell(state);

	if(!luaspell){
		lua_interface->LogError("LUA LastSpellAttackHit command error: this must be called from a spellscript");
		return 0;
	}

	lua_interface->SetBooleanValue(state, luaspell->last_spellattack_hit);
	return 1;
}
int EQ2Emu_lua_IsBehind(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	Spawn* target = lua_interface->GetSpawn(state, 2);

	if (!spawn) {
		lua_interface->LogError("LUA IsBehind command error: spawn is not valid");
		return 0;
	}

	if (!spawn->IsEntity()) {
		lua_interface->LogError("LUA IsBehind command error: spawn is not an entity");
		return 0;
	}

	if (!target) {
		lua_interface->LogError("LUA IsBehind command error: target is not valid");
		return 0;
	}

	lua_interface->SetBooleanValue(state, static_cast<Entity*>(spawn)->BehindTarget(target));
	return 1;
}

int EQ2Emu_lua_IsFlanking(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	Spawn* target = lua_interface->GetSpawn(state, 2);

	if (!spawn) {
		lua_interface->LogError("LUA IsFlanking command error: spawn is not valid");
		return 0;
	}

	if (!spawn->IsEntity()) {
		lua_interface->LogError("LUA IsFlanking command error: spawn is not an entity");
		return 0;
	}

	if (!target) {
		lua_interface->LogError("LUA IsFlanking command error: target is not valid");
		return 0;
	}

	lua_interface->SetBooleanValue(state, static_cast<Entity*>(spawn)->FlankingTarget(target));
	return 1;
}

int EQ2Emu_lua_GetItemCount(lua_State* state) {
	if(!lua_interface)
		return 0;

	Item* item = lua_interface->GetItem(state);

	if(!item){
		lua_interface->LogError("LUA GetItemCount command error: item not valid");
		return 0;
	}

	lua_interface->SetInt32Value(state, item->details.count);
	return 1;
}

int EQ2Emu_lua_SetItemCount(lua_State* state) {
	if(!lua_interface)
		return 0;

	Item* item = lua_interface->GetItem(state);
	Spawn* owner = lua_interface->GetSpawn(state, 2);
	int16 new_count = lua_interface->GetInt32Value(state, 3);

	if(!item){
		lua_interface->LogError("LUA SetItemCount command error: item not valid");
		return 0;
	}
	
	if(!owner){
		lua_interface->LogError("LUA SetItemCount command error: spawn not valid");
		return 0;
	}

	if(!owner->IsPlayer()){
		lua_interface->LogError("LUA SetItemCount command error: spawn is not a player");
		return 0;
	}

	if(item->stack_count < new_count){
		lua_interface->LogError("LUA SetItemCount command error: new item count cannot be more than max stack count");
		return 0;
	}

	if(new_count > 0){
		item->details.count = new_count;
		item->save_needed = true;
	}
	else if(static_cast<Player*>(owner)->GetEquipmentList()->GetItem(item->details.slot_id) == item)
		static_cast<Player*>(owner)->GetEquipmentList()->RemoveItem(item->details.slot_id, true);
	else if(static_cast<Player*>(owner)->GetPlayerItemList()->GetItemFromUniqueID(item->details.unique_id) == item)
		static_cast<Player*>(owner)->GetPlayerItemList()->RemoveItem(item, true);
	else
	{
		lua_interface->LogError("LUA SetItemCount command error: could not remove item from player");
		return 0;
	}

	Client* client = owner->GetZone()->GetClientBySpawn(owner);

	if(!client)
		return 0;

	static_cast<Player*>(owner)->SendInventoryUpdate(client->GetVersion());

	EQ2Packet* app = static_cast<Player*>(owner)->GetEquipmentList()->serialize(client->GetVersion());
	if(app)
		client->QueuePacket(app);

	return 0;
}

int EQ2Emu_lua_AddSpellTimer(lua_State* state) {
	if(!lua_interface)
		return 0;

	int32 time = lua_interface->GetInt32Value(state);
	string function = lua_interface->GetStringValue(state, 2);
	Spawn* caster = lua_interface->GetSpawn(state, 3);
	Spawn* target = lua_interface->GetSpawn(state, 4);

	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	if (time == 0) {
		lua_interface->LogError("LUA AddSpellTimer command error: time must be set");
		return 0;
	}

	if (function.length() == 0) {
		lua_interface->LogError("LUA AddSpellTimer command error: function name must be set");
		return 0;
	}

	if (!spell) {
		lua_interface->LogError("LUA AddSpellTimer command error: spell not found, AddSpellTimer must be used in a spell script");
		return 0;
	}

	SpellScriptTimer* timer = new SpellScriptTimer;

	/* //Google tells me memsetting a string is bad, manually setting just in case - Foof
	#ifdef WIN32
		ZeroMemory(timer, sizeof(SpellScriptTimer));
	#else
		bzero(timer, sizeof(SpellScriptTimer));
	#endif*/

	timer->caster = 0;
	timer->deleteWhenDone = false;
	timer->target = 0;
	
	timer->time = Timer::GetCurrentTime2() + time;
	timer->customFunction = function;
	timer->spell = spell;
	if (caster)
		timer->caster = caster->GetID();
	if (target)
		timer->target = target->GetID();

	spell->caster->GetZone()->GetSpellProcess()->AddSpellScriptTimer(timer);
	return 0;
}

int EQ2Emu_lua_Resurrect(lua_State* state) {
	if(!lua_interface)
		return 0;

	float hp_perc = lua_interface->GetFloatValue(state);
	float power_perc = lua_interface->GetFloatValue(state, 2);
	bool send_window = lua_interface->GetInt32Value(state, 3) == 1;
	Spawn* target = lua_interface->GetSpawn(state, 4);
	string heal_name = lua_interface->GetStringValue(state, 5);
	int8 crit_mod = lua_interface->GetInt32Value(state, 6);
	bool no_calcs = lua_interface->GetInt32Value(state, 7) == 1;

	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	if (!spell) {
		lua_interface->LogError("LUA command error: this function must be used in a spellscript");
		return 0;
	}

	Entity* caster = spell->caster;

	if(!caster){
		lua_interface->LogError("LUA command error: could not find caster");
		return 0;
	}

	Client* client = 0;
	PendingResurrection* rez = 0;
	ZoneServer* zone = spell->caster->GetZone();
	if(!target){
		spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
		if(spell->targets.size() > 0){
			vector<int32> spell_targets = spell->targets;
			for(int8 i=0; i < spell_targets.size(); i++){
				target = zone->GetSpawnByID(spell_targets.at(i));
				if(!target)
					continue;
				if(!target->IsPlayer())
					continue;

				client = target->GetZone()->GetClientBySpawn(target);

				if(!client)
					continue;
				
				rez = client->GetCurrentRez();
				if(rez->active)
					continue;

				client->GetResurrectMutex()->writelock(__FUNCTION__, __LINE__);
				rez->active = true;
				rez->caster = caster;
				rez->expire_timer = new Timer;
				int32 duration = spell->spell->GetSpellDuration();
				rez->expire_timer->Start(duration * 100);
				rez->hp_perc = hp_perc;
				rez->mp_perc = power_perc;
				rez->range = spell->spell->GetSpellData()->range;
				rez->spell_name = spell->spell->GetName();
				if(heal_name.length() > 0)
					rez->heal_name = heal_name;
				else
					rez->heal_name = rez->spell_name;
				rez->no_calcs = no_calcs;
				rez->crit_mod = crit_mod;
				rez->spell_visual = spell->spell->GetSpellData()->spell_visual;
				rez->spell = spell;
				if(send_window)
					client->SendResurrectionWindow();
				else{
					target->GetZone()->ResurrectSpawn(target, client);
					rez->should_delete = true;
				}
				client->GetResurrectMutex()->releasewritelock(__FUNCTION__, __LINE__);
			}
		}
		spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);
	}
	else {
		client = target->GetZone()->GetClientBySpawn(target);

		if(!client)
			return 0;
				
		rez = client->GetCurrentRez();
		if(rez->active)
			return 0;

		client->GetResurrectMutex()->writelock(__FUNCTION__, __LINE__);
		rez->active = true;
		rez->caster = caster;
		rez->expire_timer =  new Timer;
		int32 duration = spell->spell->GetSpellDuration();
		rez->expire_timer->Start(duration * 100);
		rez->hp_perc = hp_perc;
		rez->mp_perc = power_perc;
		rez->range = spell->spell->GetSpellData()->range;
		rez->spell_name = spell->spell->GetName();
		if(heal_name.length() > 0)
			rez->heal_name = heal_name;
		else
			rez->heal_name = rez->spell_name;
		rez->no_calcs = no_calcs;
		rez->crit_mod = crit_mod;
		rez->spell_visual = spell->spell->GetSpellData()->spell_visual;
		rez->spell = spell;
		if(send_window)
			client->SendResurrectionWindow();
		else{
			target->GetZone()->ResurrectSpawn(target, client);
			rez->should_delete = true;
		}
		client->GetResurrectMutex()->releasewritelock(__FUNCTION__, __LINE__);
	}
	return 0;
}

int EQ2Emu_lua_SetVision(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	int8 vision = lua_interface->GetInt8Value(state, 2);
	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	if (!spawn) {
		lua_interface->LogError("LUA SetVision command error: spawn is not valid");
		return 0;
	}

	if (!spawn->IsEntity()) {
		lua_interface->LogError("LUA SetVision command error: spawn is not an entity");
		return 0;
	}

	if (spell && spell->targets.size() > 0) {
		ZoneServer* zone = spell->caster->GetZone();
		for (int8 i = 0; i < spell->targets.size(); i++) {
			Spawn* target = zone->GetSpawnByID(spell->targets.at(i));
			if (target->IsEntity()) {
				static_cast<Entity*>(target)->GetInfoStruct()->vision = vision;
				if (target->IsPlayer())
					static_cast<Player*>(target)->SetCharSheetChanged(true);
			}
		}
	}
	else {
		static_cast<Entity*>(spawn)->GetInfoStruct()->vision = vision;
		if (spawn->IsPlayer())
			static_cast<Player*>(spawn)->SetCharSheetChanged(true);
	}
	return 0;
}

int EQ2Emu_lua_BlurVision(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	float intensity = lua_interface->GetFloatValue(state, 2);
	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	if (!spawn) {
		lua_interface->LogError("LUA BlurVision command error: spawn is not valid");
		return 0;
	}

	if (!spawn->IsEntity()) {
		lua_interface->LogError("LUA BlurVision command error: spawn is not an entity");
		return 0;
	}

	if (spell && spell->targets.size() > 0) {
		ZoneServer* zone = spell->caster->GetZone();
		for (int8 i = 0; i < spell->targets.size(); i++) {
			Spawn* target = zone->GetSpawnByID(spell->targets.at(i));
			if (target && target->IsEntity()) {
				static_cast<Entity*>(target)->GetInfoStruct()->drunk = intensity;
				if (target->IsPlayer())
					static_cast<Player*>(target)->SetCharSheetChanged(true);
			}
		}
	}
	else {
		static_cast<Entity*>(spawn)->GetInfoStruct()->drunk = intensity;
		if (spawn->IsPlayer())
			static_cast<Player*>(spawn)->SetCharSheetChanged(true);
	}
	return 0;
}

int EQ2Emu_lua_BreatheUnderwater(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	bool breatheUnderwater = lua_interface->GetBooleanValue(state, 2);
	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	if (!spawn) {
		lua_interface->LogError("LUA BreathUnderwater command error: spawn is not valid");
		return 0;
	}

	if (!spawn->IsEntity()) {
		lua_interface->LogError("LUA BreathUnderwater command error: spawn is not en entity");
		return 0;
	}

	if (spell && spell->targets.size() > 0) {
		ZoneServer* zone = spell->caster->GetZone();
		for (int8 i = 0; i < spell->targets.size(); i++) {
			Spawn* target = zone->GetSpawnByID(spell->targets.at(i));
			if (target->IsEntity()) {
				static_cast<Entity*>(target)->GetInfoStruct()->breathe_underwater = breatheUnderwater;
				if (target->IsPlayer())
					static_cast<Player*>(target)->SetCharSheetChanged(true);
			}
		}
	}
	else {
		static_cast<Entity*>(spawn)->GetInfoStruct()->breathe_underwater = breatheUnderwater;
		if (spawn->IsPlayer())
			static_cast<Player*>(spawn)->SetCharSheetChanged(true);
	}
	return 0;
}

int EQ2Emu_lua_GetItemSkillReq(lua_State* state) {
	if(!lua_interface)
		return 0;

	Item* item = lua_interface->GetItem(state);
	int8 type = lua_interface->GetInt32Value(state, 2);
	if(!item){
		lua_interface->LogError("LUA GetItemSkillReq command error: item not valid");
		return 0;
	}

	if(type == 1)
		lua_interface->SetInt32Value(state, item->generic_info.skill_req1);
	else if (type == 2)
		lua_interface->SetInt32Value(state, item->generic_info.skill_req2);

	return 1;
}

int EQ2Emu_lua_SetSpeeedMultiplier(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* target = lua_interface->GetSpawn(state);
	float val = lua_interface->GetFloatValue(state, 2);
	LuaSpell* spell = lua_interface->GetCurrentSpell(state);
	// Added from Gangrenous post
	if (spell && spell->resisted)
		return 0;

	if (spell && spell->spell && spell->targets.size() > 0) {
		ZoneServer* zone = spell->caster->GetZone();
		for (int32 i = 0; i != spell->targets.size(); i++) {
			Spawn* spawn = zone->GetSpawnByID(spell->targets.at(i));
			if (spawn && spawn->IsEntity()) {
				static_cast<Entity*>(spawn)->SetSpeedMultiplier(val);
				if (spawn->IsPlayer())
					static_cast<Player*>(spawn)->SetCharSheetChanged(true);
			}
		}
	}
	else {
		if (target && target->IsEntity()) {
			static_cast<Entity*>(target)->SetSpeedMultiplier(val);
			if (target->IsPlayer())
				static_cast<Player*>(target)->SetCharSheetChanged(true);
		}
	}

	return 0;
}

int EQ2Emu_lua_SetIllusion(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	int16 model = lua_interface->GetInt16Value(state, 2);
	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	if (spell && spell->spell && spell->targets.size() > 0) {
		ZoneServer* zone = spell->caster->GetZone();
		for (int32 i = 0; i < spell->targets.size(); i++) {
			Spawn* target = zone->GetSpawnByID(spell->targets.at(i));
			if (target)
				target->SetIllusionModel(model);
		}
	}
	else {
		if (!spawn) {
			lua_interface->LogError("LUA SetIllusion command error: spawn is not valid");
			return 0;
		}

		spawn->SetIllusionModel(model);
	}

	return 0;
}

int EQ2Emu_lua_ResetIllusion(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	if (spell && spell->spell && spell->targets.size() > 0) {
		ZoneServer* zone = spell->caster->GetZone();
		for (int32 i = 0; i < spell->targets.size(); i++) {
			Spawn* target = zone->GetSpawnByID(spell->targets.at(i));
			if (target)
				target->SetIllusionModel(0);
		}
	}
	else {
		if (!spawn) {
			lua_interface->LogError("LUA ResetIllusion command error: spawn is not valid");
			return 0;
		}

		spawn->SetIllusionModel(0);
	}

	return 0;
}

int EQ2Emu_lua_AddThreatTransfer(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* caster = lua_interface->GetSpawn(state);
	Spawn* target = lua_interface->GetSpawn(state, 2);
	float chance = lua_interface->GetFloatValue(state, 3);
	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	if (!caster) {
		lua_interface->LogError("LUA AddThreatTransfer command error: caster is not a valid spawn");
		return 0;
	}

	if (!caster->IsEntity()) {
		lua_interface->LogError("LUA AddThreatTransfer command error: caster is not an entity");
		return 0;
	}

	if (!target) {
		lua_interface->LogError("LUA AddThreatTransfer command error: target is not a valid spawn");
		return 0;
	}

	if (!target->IsEntity()) {
		lua_interface->LogError("LUA AddThreatTransfer command error: target is not an entity");
		return 0;
	}

	if (chance <= 0) {
		lua_interface->LogError("LUA AddThreatTransfer command error: chance must be greater then 0%");
		return 0;
	}

	if (!spell) {
		lua_interface->LogError("LUA AddThreatTransfer command error: can only be used in a spell script");
		return 0;
	}

	if (static_cast<Entity*>(caster)->GetThreatTransfer()) {
		return 0;
	}

	ThreatTransfer* transfer = new ThreatTransfer;
	transfer->Target = target->GetID();
	transfer->Amount = chance;
	transfer->Spell = spell;

	static_cast<Entity*>(caster)->SetThreatTransfer(transfer);
	return 0;
}

int EQ2Emu_lua_RemoveThreatTransfer(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	if (!spawn) {
		lua_interface->LogError("LUA RemoveThreatTransfer command error: spawn is not valid");
		return 0;
	}

	if (!spell) {
		lua_interface->LogError("LUA RemoveThreatTransfer command error: can only be used in a spell script");
		return 0;
	}

	if (static_cast<Entity*>(spawn)->GetThreatTransfer() && static_cast<Entity*>(spawn)->GetThreatTransfer()->Spell == spell) {
		ThreatTransfer* transfer = static_cast<Entity*>(spawn)->GetThreatTransfer();
		safe_delete(transfer);
		static_cast<Entity*>(spawn)->SetThreatTransfer(0);
	}

	return 0;
}

int EQ2Emu_lua_CureByType(lua_State* state) {
	if(!lua_interface)
		return 0;

	LuaSpell* spell = lua_interface->GetCurrentSpell(state);
	if(!spell){
		lua_interface->LogError("LUA CureByType command error: can only be used in a spell script");
		return 0;
	}

	int8 cure_count = lua_interface->GetInt8Value(state);
	int8 cure_type = lua_interface->GetInt8Value(state, 2);
	string cure_name = lua_interface->GetStringValue(state, 3);
	int8 cure_level = lua_interface->GetInt8Value(state, 4);
	Spawn* target = lua_interface->GetSpawn(state, 5);

	if(target){
		if(!target->IsEntity()){
			lua_interface->LogError("LUA CureByType command error: spawn override must be entity if used");
			return 0;
		}

		if(static_cast<Entity*>(target)->GetDetTypeCount(cure_type) > 0)
			static_cast<Entity*>(target)->CureDetrimentByType(cure_count, cure_type, cure_name.length() > 0 ? cure_name : static_cast<string>(spell->spell->GetName()), spell->caster, cure_level);
	}
	else {
		ZoneServer* zone = spell->caster->GetZone();
		vector<int32> targets = spell->targets;
		
		spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
		for(int8 i=0; i<targets.size(); i++){
			target = zone->GetSpawnByID(targets.at(i));

			if(!target || !target->IsEntity())
				continue;

			if(static_cast<Entity*>(target)->GetDetTypeCount(cure_type) > 0)
				static_cast<Entity*>(target)->CureDetrimentByType(cure_count, cure_type, cure_name.length() > 0 ? cure_name : static_cast<string>(spell->spell->GetName()), spell->caster, cure_level);
		}
		spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);
	}
	return 0;
}

int EQ2Emu_lua_CureByControlEffect(lua_State* state) {
	if(!lua_interface)
		return 0;

	LuaSpell* spell = lua_interface->GetCurrentSpell(state);
	if(!spell){
		lua_interface->LogError("LUA CureByControlEffect command error: can only be used in a spell script");
		return 0;
	}

	int8 cure_count = lua_interface->GetInt8Value(state);
	int8 cure_type = lua_interface->GetInt8Value(state, 2);
	string cure_name = lua_interface->GetStringValue(state, 3);
	int8 cure_level = lua_interface->GetInt8Value(state, 4);
	Spawn* target = lua_interface->GetSpawn(state, 5);

	if(target){
		if(!target->IsEntity()){
			lua_interface->LogError("LUA CureByControlEffect command error: spawn override must be entity if used");
			return 0;
		}

		if(static_cast<Entity*>(target)->GetDetCount() > 0)
			static_cast<Entity*>(target)->CureDetrimentByControlEffect(cure_count, cure_type, cure_name.length() > 0 ? cure_name : static_cast<string>(spell->spell->GetName()), spell->caster, cure_level);
	}
	else {
		ZoneServer* zone = spell->caster->GetZone();
		vector<int32> targets = spell->targets;
		
		spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
		for(int8 i=0; i<targets.size(); i++){
			target = zone->GetSpawnByID(targets.at(i));

			if(!target || !target->IsEntity())
				continue;

			if(static_cast<Entity*>(target)->GetDetCount() > 0)
				static_cast<Entity*>(target)->CureDetrimentByControlEffect(cure_count, cure_type, cure_name.length() > 0 ? cure_name : static_cast<string>(spell->spell->GetName()), spell->caster, cure_level);
		}
		spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);
	}
	return 0;
}

int EQ2Emu_lua_CancelSpell(lua_State* state) {
	if (!lua_interface)
		return 0;

	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	if (!spell) {
		lua_interface->LogError("LUA CancelSpell command error: can only be use in a spell script");
		return 0;
	}

	if (!spell->caster) {
		lua_interface->LogError("LUA CancelSpell command error: unable to get the caster of the spell");
		return 0;
	}

	if (!spell->caster->GetZone()) {
		lua_interface->LogError("LUA CancelSpell command error: unable to get the zone of the caster");
		return 0;
	}

	spell->caster->GetZone()->GetSpellProcess()->AddSpellCancel(spell);

	return 0;
}

int EQ2Emu_lua_RemoveStealth(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	if (!spell){
		lua_interface->LogError("LUA RemoveStealth command error: must be used from spell script");
		return 0;
	}

	if (spawn && spawn->IsEntity())
		static_cast<Entity*>(spawn)->RemoveStealthSpell(spell);
	else{
		ZoneServer* zone = spell->caster->GetZone();
		spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
		for (int32 i = 0; i < spell->targets.size(); i++){
			spawn = zone->GetSpawnByID(spell->targets.at(i));
			if (!spawn || !spawn->IsEntity())
				continue;

			static_cast<Entity*>(spawn)->RemoveStealthSpell(spell);
		}
		spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);
	}
	return 0;
}

int EQ2Emu_lua_RemoveInvis(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	if (!spell){
		lua_interface->LogError("LUA RemoveInvis command error: must be used from spell script");
		return 0;
	}

	if (spawn && spawn->IsEntity())
		static_cast<Entity*>(spawn)->RemoveInvisSpell(spell);
	else{
		ZoneServer* zone = spell->caster->GetZone();
		spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
		for (int32 i = 0; i < spell->targets.size(); i++){
			spawn = zone->GetSpawnByID(spell->targets.at(i));
			if (!spawn || !spawn->IsEntity())
				continue;

			static_cast<Entity*>(spawn)->RemoveInvisSpell(spell);
		}
		spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);
	}
	return 0;
}

int EQ2Emu_lua_StartHeroicOpportunity(lua_State* state) {
	if (!lua_interface)
		return 0;
	
	Spawn* caster = lua_interface->GetSpawn(state);
	Spawn* target = caster->GetTarget();//lua_interface->GetSpawn(state, 2);
	int8 class_id = lua_interface->GetInt8Value(state, 2);
	
	if (!caster) {
		lua_interface->LogError("LUA StartHeroicOpportunity command error: caster is not valid");
		return 0;
	}

	if (!caster->IsPlayer()) {
		lua_interface->LogError("LUA StartHeroicOpportunity command error: caster must be a player");
		return 0;
	}

	if (!target) {
		lua_interface->LogError("LUA StartHeroicOpportunity command error: target is not valid");
		return 0;
	}

	Client* client = caster->GetZone()->GetClientBySpawn(caster);
	if (!client) {
		lua_interface->LogError("LUA StartHeroicOpportunity command error: unable to get a client for the given caster");
		return 0;
	}

	HeroicOP* ho = master_ho_list.GetHeroicOP(class_id);
	if (ho) {
		ho->SetTarget(target->GetID());
		LogWrite(SPELL__ERROR, 0, "HO", "caster: %u", caster->GetID());
		LogWrite(SPELL__ERROR, 0, "HO", "target: %u", target->GetID());
		if (static_cast<Entity*>(caster)->GetGroupMemberInfo()) {
			if (caster->GetZone()->GetSpellProcess()->AddHO(client, ho)) {
				world.GetGroupManager()->GroupLock(__FUNCTION__, __LINE__);

				deque<GroupMemberInfo*>::iterator itr;
				deque<GroupMemberInfo*>* members = world.GetGroupManager()->GetGroupMembers(static_cast<Entity*>(caster)->GetGroupMemberInfo()->group_id);
				for (itr = members->begin(); itr != members->end(); ++itr) {
					if ((*itr)->client)
						ClientPacketFunctions::SendHeroicOPUpdate((*itr)->client, ho);
				}

				world.GetGroupManager()->ReleaseGroupLock(__FUNCTION__, __LINE__);
			}
			else
				safe_delete(ho);
		}
		else {
			if (caster->GetZone()->GetSpellProcess()->AddHO(client, ho)) {
				ClientPacketFunctions::SendHeroicOPUpdate(client, ho);
			}
			else
				safe_delete(ho);
		}
	}

	return 0;
}

int EQ2Emu_lua_SetSpellTriggerCount(lua_State* state){
	if (!lua_interface)
		return 0;

	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	if (!spell){
		lua_interface->LogError("LUA SetSpellTriggerCount command error: you must use this function in a spellscript!");
		return 0;
	}

	int16 triggerCount = lua_interface->GetInt16Value(state);
	bool cancel_after_triggers = (lua_interface->GetInt8Value(state, 2) == 1);

	if (!triggerCount){
		lua_interface->LogError("LUA SetSpellTriggerCount command error: used trigger value equals zero!");
		return 0;
	}

	spell->num_triggers = triggerCount;
	spell->had_triggers = true;
	spell->cancel_after_all_triggers = cancel_after_triggers;

	return 0;
}

int EQ2Emu_lua_GetSpellTriggerCount(lua_State* state){
	if (!lua_interface)
		return 0;

	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	if (!spell){
		lua_interface->LogError("LUA GetSpellTriggerCount command error: you must use this function in a spellscript!");
		return 0;
	}

	lua_interface->SetInt32Value(state, spell->num_triggers);

	return 1;
}

int EQ2Emu_lua_RemoveTriggerFromSpell(lua_State* state){
	if (!lua_interface)
		return 0;

	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	if (!spell){
		lua_interface->LogError("LUA RemoveTriggerFromSpell command error: you must use this function in a spellscript!");
		return 0;
	}

	int16 remove_count = lua_interface->GetInt16Value(state);

	if (!remove_count)
		remove_count = 1;

	if (remove_count >= spell->num_triggers){
		spell->num_triggers = 0;
		if (spell->cancel_after_all_triggers)
			spell->caster->GetZone()->GetSpellProcess()->AddSpellCancel(spell);
	}
	else{
		spell->num_triggers -= remove_count;
		ClientPacketFunctions::SendMaintainedExamineUpdate(spell->caster->GetZone()->GetClientBySpawn(spell->caster), spell->slot_pos, spell->num_triggers, 0);
	}
	return 0;
}

int EQ2Emu_lua_CopySpawnAppearance(lua_State* state){
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	Spawn* copy_spawn = lua_interface->GetSpawn(state, 2);

	if (!spawn){
		lua_interface->LogError("LUA CopySpawnAppearance command error: the first spawn used was not valid!");
		return 0;
	}
	
	if (!copy_spawn){
		lua_interface->LogError("LUA CopySpawnAppearance command error: the second spawn used was not valid!");
		return 0;
	}

	spawn->CopySpawnAppearance(copy_spawn);
	return 0;
}

int EQ2Emu_lua_AddImmunitySpell(lua_State* state){
	if (!lua_interface)
		return 0;

	LuaSpell* spell = lua_interface->GetCurrentSpell(state);
	int8 type = lua_interface->GetInt8Value(state);
	Spawn* spawn = lua_interface->GetSpawn(state, 2);

	if (!spell){
		lua_interface->LogError("LUA AddImmunitySpell command error: This must be used in a spellscript");
		return 0;
	}

	if (spawn){
		if (!spawn->IsEntity()){
			lua_interface->LogError("LUA AddImmunitySpell command error: The spawn provided is not an entity");
			return 0;
		}
		Entity* entity = static_cast<Entity*>(spawn);
		switch (type){
		case IMMUNITY_TYPE_AOE:
			entity->AddAOEImmunity(spell);
			if (!(spell->effect_bitmask & EFFECT_FLAG_AOE_IMMUNE))
				spell->effect_bitmask += EFFECT_FLAG_AOE_IMMUNE;
			break;
		case IMMUNITY_TYPE_STUN:
			entity->AddStunImmunity(spell);
			if (!(spell->effect_bitmask & EFFECT_FLAG_STUN_IMMUNE))
				spell->effect_bitmask += EFFECT_FLAG_STUN_IMMUNE;
			break;
		case IMMUNITY_TYPE_ROOT:
			entity->AddRootImmunity(spell);
			if (!(spell->effect_bitmask & EFFECT_FLAG_ROOT_IMMUNE))
				spell->effect_bitmask += EFFECT_FLAG_ROOT_IMMUNE;
			break;
		case IMMUNITY_TYPE_DAZE:
			entity->AddDazeImmunity(spell);
			if (!(spell->effect_bitmask & EFFECT_FLAG_DAZE_IMMUNE))
				spell->effect_bitmask += EFFECT_FLAG_DAZE_IMMUNE;
			break;
		case IMMUNITY_TYPE_FEAR:
			entity->AddFearImmunity(spell);
			if (!(spell->effect_bitmask & EFFECT_FLAG_FEAR_IMMUNE))
				spell->effect_bitmask += EFFECT_FLAG_FEAR_IMMUNE;
			break;
		case IMMUNITY_TYPE_MEZ:
			entity->AddMezImmunity(spell);
			if (!(spell->effect_bitmask & EFFECT_FLAG_MEZ_IMMUNE))
				spell->effect_bitmask += EFFECT_FLAG_MEZ_IMMUNE;
			break;
		case IMMUNITY_TYPE_STIFLE:
			entity->AddStifleImmunity(spell);
			if (!(spell->effect_bitmask & EFFECT_FLAG_STIFLE_IMMUNE))
				spell->effect_bitmask += EFFECT_FLAG_STIFLE_IMMUNE;
			break;
		default:
			lua_interface->LogError("LUA AddImmunitySpell command error: invalid immunity type");
		}
	}
	else {
		bool should_break = false;
		spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
		for (int8 i = 0; i < spell->targets.size(); i++){
			spawn = spell->caster->GetZone()->GetSpawnByID(spell->targets.at(i));
			if (!spawn || !spawn->IsEntity())
				continue;
			Entity* entity = static_cast<Entity*>(spawn);
			switch (type){
			case IMMUNITY_TYPE_AOE:
				entity->AddAOEImmunity(spell);
				if (!(spell->effect_bitmask & EFFECT_FLAG_AOE_IMMUNE))
					spell->effect_bitmask += EFFECT_FLAG_AOE_IMMUNE;
				break;
			case IMMUNITY_TYPE_STUN:
				entity->AddStunImmunity(spell);
				if (!(spell->effect_bitmask & EFFECT_FLAG_STUN_IMMUNE))
					spell->effect_bitmask += EFFECT_FLAG_STUN_IMMUNE;
				break;
			case IMMUNITY_TYPE_ROOT:
				entity->AddRootImmunity(spell);
				if (!(spell->effect_bitmask & EFFECT_FLAG_ROOT_IMMUNE))
					spell->effect_bitmask += EFFECT_FLAG_ROOT_IMMUNE;
				break;
			case IMMUNITY_TYPE_DAZE:
				entity->AddDazeImmunity(spell);
				if (!(spell->effect_bitmask & EFFECT_FLAG_DAZE_IMMUNE))
					spell->effect_bitmask += EFFECT_FLAG_DAZE_IMMUNE;
				break;
			case IMMUNITY_TYPE_FEAR:
				entity->AddFearImmunity(spell);
				if (!(spell->effect_bitmask & EFFECT_FLAG_FEAR_IMMUNE))
					spell->effect_bitmask += EFFECT_FLAG_FEAR_IMMUNE;
				break;
			case IMMUNITY_TYPE_MEZ:
				entity->AddMezImmunity(spell);
				if (!(spell->effect_bitmask & EFFECT_FLAG_MEZ_IMMUNE))
					spell->effect_bitmask += EFFECT_FLAG_MEZ_IMMUNE;
				break;
			case IMMUNITY_TYPE_STIFLE:
				entity->AddStifleImmunity(spell);
				if (!(spell->effect_bitmask & EFFECT_FLAG_STIFLE_IMMUNE))
					spell->effect_bitmask += EFFECT_FLAG_STIFLE_IMMUNE;
				break;
			default:
				lua_interface->LogError("LUA AddImmunitySpell command error: invalid immunity type");
				should_break = true;
			}
			if (should_break)
				break;
		}
		spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);
	}

	return 0;
}

int EQ2Emu_lua_RemoveImmunitySpell(lua_State* state){
	if (!lua_interface)
		return 0;

	LuaSpell* spell = lua_interface->GetCurrentSpell(state);
	int8 type = lua_interface->GetInt8Value(state);
	Spawn* spawn = lua_interface->GetSpawn(state, 2);

	if (!spell){
		lua_interface->LogError("LUA RemoveImmunitySpell command error: This must be used in a spellscript");
		return 0;
	}

	if (spawn){
		if (!spawn->IsEntity()){
			lua_interface->LogError("LUA RemoveImmunitySpell command error: The spawn provided is not an entity");
			return 0;
		}
		Entity* entity = static_cast<Entity*>(spawn);
		switch (type){
		case IMMUNITY_TYPE_AOE:
			entity->RemoveAOEImmunity(spell);
			break;
		case IMMUNITY_TYPE_STUN:
			entity->RemoveStunImmunity(spell);
			break;
		case IMMUNITY_TYPE_ROOT:
			entity->RemoveRootImmunity(spell);
			break;
		case IMMUNITY_TYPE_DAZE:
			entity->RemoveDazeImmunity(spell);
			break;
		case IMMUNITY_TYPE_FEAR:
			entity->RemoveFearImmunity(spell);
			break;
		case IMMUNITY_TYPE_MEZ:
			entity->RemoveMezImmunity(spell);
			break;
		case IMMUNITY_TYPE_STIFLE:
			entity->RemoveStifleImmunity(spell);
			break;
		default:
			lua_interface->LogError("LUA RemoveImmunitySpell command error: invalid immunity type");
		}
	}
	else {
		bool should_break = false;
		spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
		for (int8 i = 0; i < spell->targets.size(); i++){
			spawn = spell->caster->GetZone()->GetSpawnByID(spell->targets.at(i));
			if (!spawn || !spawn->IsEntity())
				continue;
			Entity* entity = static_cast<Entity*>(spawn);
			switch (type){
			case IMMUNITY_TYPE_AOE:
				entity->RemoveAOEImmunity(spell);
				break;
			case IMMUNITY_TYPE_STUN:
				entity->RemoveStunImmunity(spell);
				break;
			case IMMUNITY_TYPE_ROOT:
				entity->RemoveRootImmunity(spell);
				break;
			case IMMUNITY_TYPE_DAZE:
				entity->RemoveDazeImmunity(spell);
				break;
			case IMMUNITY_TYPE_FEAR:
				entity->RemoveFearImmunity(spell);
				break;
			case IMMUNITY_TYPE_MEZ:
				entity->RemoveMezImmunity(spell);
				break;
			case IMMUNITY_TYPE_STIFLE:
				entity->RemoveStifleImmunity(spell);
				break;
			default:
				lua_interface->LogError("LUA RemoveImmunitySpell command error: invalid immunity type");
				should_break = true;
			}
			if (should_break)
				break;
		}
		spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);
	}

	return 0;
}

int EQ2Emu_lua_SetSpellSnareValue(lua_State* state) {
	if (!lua_interface)
		return 0;

	LuaSpell* spell = lua_interface->GetCurrentSpell(state);
	if (!spell) {
		lua_interface->LogError("LUA SetSpellSnareValue command error: This can only be used in a spell script!");
		return 0;
	}

	float snare = lua_interface->GetFloatValue(state);
	Spawn* spawn = lua_interface->GetSpawn(state, 2);

	// convert the val to the speed multipler value (100 - val)
	float val = 100.0 - snare;
	val /= 100.0;

	if (spawn) {
		if (!spawn->IsEntity()) {
			lua_interface->LogError("LUA SetSpellSnareValue command error: spawn must be an entity.");
			return 0;
		}

		static_cast<Entity*>(spawn)->SetSnareValue(spell, val);
	}
	else {
		spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
		for (int8 i = 0; i < spell->targets.size(); i++){
			spawn = spell->caster->GetZone()->GetSpawnByID(spell->targets.at(i));
			if (!spawn || !spawn->IsEntity())
				continue;

			static_cast<Entity*>(spawn)->SetSnareValue(spell, val);
		}
		spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);
	}

	return 0;
}

int EQ2Emu_lua_CheckRaceType(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	int16 race_id = lua_interface->GetInt16Value(state, 2);

	if (!spawn) {
		lua_interface->LogError("LUA CheckRaceType command error: spawn is not valid");
		return 0;
	}

	if (race_id == 0) {
		lua_interface->LogError("LUA CheckRaceType command error: race id must be set");
		return 0;
	}

	lua_interface->SetBooleanValue(state, (race_id == race_types_list.GetRaceType(spawn->GetModelType()) || race_id == race_types_list.GetRaceBaseType(spawn->GetModelType())));
	return 1;
}

int EQ2Emu_lua_GetRaceType(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);

	if (!spawn) {
		lua_interface->LogError("LUA GetRaceType command error: spawn is not valid");
		return 0;
	}

	lua_interface->SetInt32Value(state, race_types_list.GetRaceType(spawn->GetModelType()));
	return 1;
}

int EQ2Emu_lua_GetRaceBaseType(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);

	if (!spawn) {
		lua_interface->LogError("LUA GetRaceBaseType command error: spawn is not valid");
		return 0;
	}

	lua_interface->SetInt32Value(state, race_types_list.GetRaceBaseType(spawn->GetModelType()));
	return 1;
}

int EQ2Emu_lua_GetSpellName(lua_State* state) {
	if (!lua_interface)
		return 0;

	LuaSpell* spell = lua_interface->GetCurrentSpell(state);

	if (!spell){
		lua_interface->LogError("LUA GetSpellName command error: this function must be used from a spell script!");
		return 0;
	}

	lua_interface->SetStringValue(state, spell->spell->GetName());
	return 1;
}

int EQ2Emu_lua_GetQuestFlags(lua_State* state) {
	if (!lua_interface)
		return 0;

	Quest* quest = lua_interface->GetQuest(state);
	if (!quest) {
		lua_interface->LogError("LUA GetQuestFlags command error: quest is not valid.");
		return 0;
	}

	lua_interface->SetInt32Value(state, quest->GetQuestFlags());
	return 1;
}

int EQ2Emu_lua_SetQuestFlags(lua_State* state) {
	if (!lua_interface)
		return 0;

	Quest* quest = lua_interface->GetQuest(state);
	int32 flags = lua_interface->GetInt32Value(state, 2);

	if (!quest) {
		lua_interface->LogError("LUA SetQuestFlags command error: quest is not valid.");
		return 0;
	}

	quest->SetQuestFlags(flags);
	return 0;
}

int EQ2Emu_lua_SetQuestTimer(lua_State* state) {
	if (!lua_interface)
		return 0;

	Quest* quest = lua_interface->GetQuest(state);
	Spawn* player = lua_interface->GetSpawn(state, 2);
	int32 step = lua_interface->GetInt32Value(state, 3);
	int32 duration = lua_interface->GetInt32Value(state, 4);
	string action = lua_interface->GetStringValue(state, 5);

	if (!quest) {
		lua_interface->LogError("LUA SetQuestTimer command error: quest is not valid.");
		return 0;
	}

	if (!player) {
		lua_interface->LogError("LUA SetQuestTimer command error: player is not a valid spawn.");
		return 0;
	}

	if (!player->IsPlayer()) {
		lua_interface->LogError("LUA SetQuestTimer command error: player is not a valid player.");
		return 0;
	}

	if (step == 0) {
		lua_interface->LogError("LUA SetQuestTimer command error: step must be set.");
		return 0;
	}

	if (duration == 0) {
		lua_interface->LogError("LUA SetQuestTimer command error: duration must be set.");
		return 0;
	}

	if (action.length() == 0) {
		lua_interface->LogError("LUA SetQuestTimer command error: failed action must be set.");
		return 0;
	}
	
	Client* client = player->GetZone()->GetClientBySpawn(player);
	if (!client) {
		lua_interface->LogError("LUA SetQuestTimer command error: failed to get a valid client pointer for the given player");
		return 0;
	}

	quest->SetTimerStep(step);
	quest->AddFailedAction(step, action);
	quest->SetStepTimer(duration);
	client->AddQuestTimer(quest->GetQuestID());

	return 0;
}

int EQ2Emu_lua_SetQuestTimerComplete(lua_State* state) {
	if (!lua_interface)
		return 0;

	Quest* quest = lua_interface->GetQuest(state);
	Spawn* player = lua_interface->GetSpawn(state, 2);

	if (!quest) {
		lua_interface->LogError("LUA SetQuestTimerComplete command error: quest is not valid.");
		return 0;
	}

	if (!player) {
		lua_interface->LogError("LUA SetQuestTimerComplete command error: player is not a valid spawn.");
		return 0;
	}

	if (!player->IsPlayer()) {
		lua_interface->LogError("LUA SetQuestTimerComplete command error: player is not a valid player.");
		return 0;
	}

	Client* client = player->GetZone()->GetClientBySpawn(player);
	if (!client) {
		lua_interface->LogError("LUA SetQuestTimerComplete command error: failed to get a valid client pointer for the given player");
		return 0;
	}

	quest->SetTimerStep(0);
	quest->SetStepTimer(0);
	client->RemoveQuestTimer(quest->GetQuestID());

	return 0;
}

int EQ2Emu_lua_RemoveQuestStep(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* player = lua_interface->GetSpawn(state);
	Quest* quest = lua_interface->GetQuest(state, 2);
	int32 step = lua_interface->GetInt32Value(state, 3);

	if (!player) {
		lua_interface->LogError("LUA RemoveQuestStep command error: player is not a valid spawn.");
		return 0;
	}

	if (!player->IsPlayer()) {
		lua_interface->LogError("LUA RemoveQuestStep command error: player is not a valid player.");
		return 0;
	}

	if (!quest) {
		lua_interface->LogError("LUA RemoveQuestStep command error: quest is not valid.");
		return 0;
	}

	if (step == 0) {
		lua_interface->LogError("LUA RemoveQuestStep command error: step must be set.");
		return 0;
	}

	Client* client = player->GetZone()->GetClientBySpawn(player);
	if (!client) {
		lua_interface->LogError("LUA RemoveQuestStep command error: unable to get a valid client pointer from the given player.");
		return 0;
	}

	if (quest->RemoveQuestStep(step, client)) {
		client->QueuePacket(quest->QuestJournalReply(client->GetVersion(), client->GetNameCRC(), static_cast<Player*>(player), 0, 0, 0, true));
		client->GetCurrentZone()->SendQuestUpdates(client);
	}
	else
		lua_interface->LogError("LUA RemoveQuestStep command error: unable to remove the step (%u) from the quest (%s).", step, quest->GetName());
	
	return 0;
}

int EQ2Emu_lua_ResetQuestStep(lua_State* state) {
	if (!lua_interface)
		return 0;

	Quest* quest = lua_interface->GetQuest(state, 1);
	int32 step = lua_interface->GetInt32Value(state, 2);
	string desc = lua_interface->GetStringValue(state, 3);
	string task_group = lua_interface->GetStringValue(state, 4);

	if (!quest) {
		lua_interface->LogError("LUA ResetQuestStep command error: quest is not valid.");
		return 0;
	}

	if (step == 0) {
		lua_interface->LogError("LUA ResetQuestStep command error: step must be set.");
		return 0;
	}

	QuestStep* quest_step = quest->GetQuestStep(step);
	if (!quest_step) {
		lua_interface->LogError("LUA ResetQuestStep command error: unable to get a valid quest step.");
		return 0;
	}

	quest_step->SetStepProgress(0);
	quest_step->SetTaskGroup(task_group);
	quest_step->SetDescription(desc);

	return 0;
}

int EQ2Emu_lua_AddQuestStepFailureAction(lua_State* state) {
	if (!lua_interface)
		return 0;

	Quest* quest = lua_interface->GetQuest(state);
	int32 step = lua_interface->GetInt32Value(state, 2);
	string action = lua_interface->GetStringValue(state, 3);

	if (!quest) {
		lua_interface->LogError("LUA AddQuestStepFailureAction command error: quest is not valid.");
		return 0;
	}

	if (step == 0) {
		lua_interface->LogError("LUA AddQuestStepFailureAction command error: step must be set.");
		return 0;
	}

	if (action.length() == 0) {
		lua_interface->LogError("LUA AddQuestStepFailureAction command error: action must be set.");
		return 0;
	}

	quest->AddFailedAction(step, action);
	return 0;
}

int EQ2Emu_lua_SetStepFailed(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* player = lua_interface->GetSpawn(state);
	int32 quest_id = lua_interface->GetInt32Value(state, 2);
	int32 step = lua_interface->GetInt32Value(state, 3);

	if (!player) {
		lua_interface->LogError("LUA SetStepFailed command error: player is not a valid spawn.");
		return 0;
	}

	if (!player->IsPlayer()) {
		lua_interface->LogError("LUA SetStepFailed command error: player is not a valid player.");
		return 0;
	}

	if (quest_id == 0) {
		lua_interface->LogError("LUA SetStepFailed command error: quest_id must be set.");
		return 0;
	}

	if (step == 0) {
		lua_interface->LogError("LUA SetStepFailed command error: step must be set.");
		return 0;
	}

	Quest* quest = static_cast<Player*>(player)->GetQuest(quest_id);
	if (!quest) {
		lua_interface->LogError("LUA SetStepFailed command error: unable to get a valid quest from the given id (%u).", quest_id);
		return 0;
	}

	quest->StepFailed(step);
	return 0;
}

int EQ2Emu_lua_GetQuestCompleteCount(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* player = lua_interface->GetSpawn(state);
	int32 quest_id = lua_interface->GetInt32Value(state, 2);
	
	if (!player) {
		lua_interface->LogError("LUA GetQuestCompleteCount command error: player is not a valid spawn");
		return 0;
	}

	if (!player->IsPlayer()) {
		lua_interface->LogError("LUA GetQuestCompleteCount command error: player is not a valid player");
		return 0;
	}

	if (quest_id == 0) {
		lua_interface->LogError("LUA GetQuestCompleteCount command error: quest id is not valid");
		return 0;
	}

	Quest* quest = static_cast<Player*>(player)->GetCompletedQuest(quest_id);
	if (!quest) {
		lua_interface->SetInt32Value(state, 0);
		return 1;
	}

	lua_interface->SetInt32Value(state, quest->GetCompleteCount());
	return 1;
}

int EQ2Emu_lua_SetServerVariable(lua_State* state) {
	if (!lua_interface)
		return 0;

	string name = lua_interface->GetStringValue(state);
	string value = lua_interface->GetStringValue(state, 2);
	string comment = lua_interface->GetStringValue(state, 3);

	if (name.length() == 0) {
		lua_interface->LogError("LUA SetServerVariable command error: name is not valid");
		return 0;
	}

	if (value.length() == 0) {
		lua_interface->LogError("LUA SetServerVariable command error: value is not valid");
		return 0;
	}

	string varname = string("lua_").append(name);
	Variable* var = variables.FindVariable(varname);
	if (var)
		var->SetValue(value.c_str());
	else {
		var = new Variable(varname.c_str(), value.c_str(), comment.c_str());
		variables.AddVariable(var);
	}

	database.SaveVariable(var->GetName(), var->GetValue(), var->GetComment());
	return 0;
}

int EQ2Emu_lua_GetServerVariable(lua_State* state) {
	if (!lua_interface)
		return 0;

	string name = lua_interface->GetStringValue(state);
	if (name.length() == 0) {
		lua_interface->LogError("LUA GetServerVariable command error: name is not valid");
		return 0;
	}

	string varname = string("lua_").append(name);
	Variable* var = variables.FindVariable(varname);
	if (var)
		lua_interface->SetStringValue(state, var->GetValue());
	else
		lua_interface->SetStringValue(state, "NULL");

	return 1;
}

int EQ2Emu_lua_HasLanguage(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* player = lua_interface->GetSpawn(state);
	int32 language_id = lua_interface->GetInt32Value(state, 2);

	if (!player) {
		lua_interface->LogError("LUA HasLanguage command error: player is not valid");
		return 0;
	}

	if (!player->IsPlayer()) {
		lua_interface->LogError("LUA HasLanguage command error: player is not a valid player");
		return 0;
	}

	lua_interface->SetBooleanValue(state, static_cast<Player*>(player)->HasLanguage(language_id));
	return 1;
}

int EQ2Emu_lua_AddLanguage(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* player = lua_interface->GetSpawn(state);
	int32 language_id = lua_interface->GetInt32Value(state, 2);

	if (!player) {
		lua_interface->LogError("LUA AddLanguage command error: player is not valid");
		return 0;
	}

	if (!player->IsPlayer()) {
		lua_interface->LogError("LUA AddLanguage command error: player is not a valid player");
		return 0;
	}

	Language* language = master_languages_list.GetLanguage(language_id);
	if (language)
		static_cast<Player*>(player)->AddLanguage(language->GetID(), language->GetName(), true);

	return 0;
}

int EQ2Emu_lua_IsNight(lua_State* state) {
	if (!lua_interface)
		return 0;

	ZoneServer* zone = lua_interface->GetZone(state);
	if (!zone) {
		lua_interface->LogError("LUA IsNight command error: zone is not valid");
		return 0;
	}

	lua_interface->SetBooleanValue(state, zone->IsDusk());
	return 1;
}

int EQ2Emu_lua_AddMultiFloorLift(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);

	if (!spawn) {
		lua_interface->LogError("LUA AddMultiFloorLift command error: spawn is not valid");
		return 0;
	}

	if (!spawn->IsWidget()) {
		lua_interface->LogError("LUA AddMultiFloorLift command error: spawn is not a widget");
		return 0;
	}

	static_cast<Widget*>(spawn)->SetMultiFloorLift(true);

	if (spawn->GetZone())
		spawn->GetZone()->AddTransportSpawn(spawn);

	return 0;
}

int EQ2Emu_lua_StartAutoMount(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* player = lua_interface->GetSpawn(state);
	int32 path = lua_interface->GetInt32Value(state, 2);

	if (!player) {
		lua_interface->LogError("LUA StartAutoMount command error: spawn is not valid");
		return 0;
	}

	if (!player->IsPlayer()) {
		lua_interface->LogError("LUA StartAutoMount command error: spawn is not a player");
		return 0;
	}

	if (path == 0) {
		lua_interface->LogError("LUA StartAutoMount command error: path must be greater then zero");
		return 0;
	}

	Client* client = player->GetZone()->GetClientBySpawn(player);
	if (!client) {
		lua_interface->LogError("LUA StartAutoMount command error: unable to get a client from the given player");
		return 0;
	}

	client->SetPendingFlightPath(path);

	static_cast<Player*>(player)->SetTempMount(static_cast<Entity*>(player)->GetMount());
	static_cast<Player*>(player)->SetTempMountColor(static_cast<Entity*>(player)->GetMountColor());
	static_cast<Player*>(player)->SetTempMountSaddleColor(static_cast<Entity*>(player)->GetMountSaddleColor());

	PacketStruct* packet = configReader.getStruct("WS_ReadyForTakeOff", client->GetVersion());
	if (packet) {
		client->QueuePacket(packet->serialize());
		safe_delete(packet);
	}

	return 0;
}

int EQ2Emu_lua_EndAutoMount(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* player = lua_interface->GetSpawn(state);
	if (!player) {
		lua_interface->LogError("LUA EndAutoMount command error: spawn is not valid");
		return 0;
	}

	if (!player->IsPlayer()) {
		lua_interface->LogError("LUA EndAutoMount command error: spawn is not a player");
		return 0;
	}

	Client* client = player->GetZone()->GetClientBySpawn(player);
	if (!client) {
		lua_interface->LogError("LUA EndAutoMount command error: unable to get a client from the given player");
		return 0;
	}
	client->EndAutoMount();

	return 0;
}

int EQ2Emu_lua_IsOnAutoMount(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* player = lua_interface->GetSpawn(state);
	if (!player) {
		lua_interface->LogError("LUA IsOnAutoMount command error: spawn in not valid");
		return 0;
	}

	if (!player->IsPlayer()) {
		lua_interface->LogError("LUA IsOnAutoMount command error: spawn is not a player");
		return 0;
	}

	Client* client = player->GetZone()->GetClientBySpawn(player);
	if (!client) {
		lua_interface->LogError("LUA IsOnAutoMount command error: unable to get a client from the given player");
		return 0;
	}

	lua_interface->SetBooleanValue(state, client->GetOnAutoMount());
	return 1;
}

int EQ2Emu_lua_SetPlayerHistory(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* player = lua_interface->GetSpawn(state);
	int32 event_id = lua_interface->GetInt32Value(state, 2);
	int32 value = lua_interface->GetInt32Value(state, 3);
	int32 value2 = lua_interface->GetInt32Value(state, 4);

	if (!player) {
		lua_interface->LogError("LUA SetPlayerHistory command error: spawn is not valid");
		return 0;
	}

	if (!player->IsPlayer()) {
		lua_interface->LogError("LUA SetPlayerHistory command error: spawn is not a player");
		return 0;
	}

	static_cast<Player*>(player)->UpdateLUAHistory(event_id, value, value2);
	return 0;
}

int EQ2Emu_lua_GetPlayerHistory(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* player = lua_interface->GetSpawn(state);
	int32 event_id = lua_interface->GetInt32Value(state, 2);

	if (!player) {
		lua_interface->LogError("LUA GetPlayerHistory command error: spawn is not valid");
		return 0;
	}

	if (!player->IsPlayer()) {
		lua_interface->LogError("LUA GetPlayerHistory command error: spawn is not a player");
		return 0;
	}

	LUAHistory* hd = static_cast<Player*>(player)->GetLUAHistory(event_id);
	if (!hd)
		return 0;

	lua_interface->SetInt32Value(state, hd->Value);
	lua_interface->SetInt32Value(state, hd->Value2);
	return 2;
}

int EQ2Emu_lua_SetGridID(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	int32 grid = lua_interface->GetInt32Value(state, 2);

	if (!spawn) {
		lua_interface->LogError("LUA SetGridID command error: spawn is not valid");
		return 0;
	}

	if (grid == 0) {
		lua_interface->LogError("LUA SetGridID command error: grid is not valid");
		return 0;
	}

	spawn->SetPos(&(spawn->appearance.pos.grid_id), grid);
	return 0;
}

int EQ2Emu_lua_SetRequiredHistory(lua_State* state){
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);
	int32 event_id = lua_interface->GetInt32Value(state, 2);
	int32 value1 = lua_interface->GetInt32Value(state, 3);
	int32 value2 = lua_interface->GetInt32Value(state, 4);
	bool private_spawn = (lua_interface->GetInt8Value(state, 5) == 1);
	int16 flag_override = lua_interface->GetInt16Value(state, 6);

	if (!spawn){
		lua_interface->LogError("LUA SetRequiredHistory command error: spawn is not valid");
		return 0;
	}

	//Add this quest to the list of required quests for this spawn
	spawn->SetRequiredHistory(event_id, value1, value2);
	//If private spawn value set
	if (private_spawn){
		//Set the spawn to be private when not granted access via history
		spawn->AddAllowAccessSpawn(spawn);
		spawn->SetPrivateQuestSpawn(true);
	}
	//This value will override vis_flags in the vis packet
	if (flag_override > 0)
		spawn->SetQuestsRequiredOverride(flag_override);
	return 0;
}

int EQ2Emu_lua_GetQuestStepProgress(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* player = lua_interface->GetSpawn(state);
	int32 quest_id = lua_interface->GetInt32Value(state, 2);
	int32 step_id = lua_interface->GetInt32Value(state, 3);

	if (!player) {
		lua_interface->LogError("LUA GetQuestStepProgress command error: spawn is not valid");
		return 0;
	}

	if (!player->IsPlayer()) {
		lua_interface->LogError("LUA GetQuestStepProgress command error: spawn is not a player");
		return 0;
	}

	lua_interface->SetInt32Value(state, static_cast<Player*>(player)->GetStepProgress(quest_id, step_id));

	return 1;
}

int EQ2Emu_lua_SetPlayerLevel(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* player = lua_interface->GetSpawn(state);
	int8 level = lua_interface->GetInt8Value(state, 2);

	if (!player) {
		lua_interface->LogError("LUA SetPlayerLevel command error: spawn is not valid");
		return 0;
	}

	if (!player->IsPlayer()) {
		lua_interface->LogError("LUA SetPlayerLevel command error: spawn is not a player");
		return 0;
	}

	if (level == 0) {
		lua_interface->LogError("LUA SetPlayerLevel command error: new level can't be 0");
		return 0;
	}

	Client* client = player->GetZone()->GetClientBySpawn(player);
	if (!client) {
		lua_interface->LogError("LUA SetPlayerLevel command error: unable to get a client from the given spawn");
		return 0;
	}

	client->ChangeLevel(client->GetPlayer()->GetLevel(), level);
	client->GetPlayer()->SetXP(1);
	client->GetPlayer()->SetNeededXP();

	return 0;
}

int EQ2Emu_lua_AddCoin(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* player = lua_interface->GetSpawn(state);
	int32 amount = lua_interface->GetInt32Value(state, 2);

	if (!player) {
		lua_interface->LogError("LUA AddCoin command error: spawn is not valid");
		return 0;
	}

	if (!player->IsPlayer()){
		lua_interface->LogError("LUA AddCoin command error: spawn is not a player");
		return 0;
	}

	if (amount == 0) {
		lua_interface->LogError("LUA AddCoin command error: amount must be greater then 0");
		return 0;
	}

	static_cast<Player*>(player)->AddCoins(amount);
	return 0;
}

int EQ2Emu_lua_RemoveCoin(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* player = lua_interface->GetSpawn(state);
	int32 amount = lua_interface->GetInt32Value(state, 2);

	if (!player) {
		lua_interface->LogError("LUA RemoveCoin command error: spawn is not valid");
		return 0;
	}

	if (!player->IsPlayer()){
		lua_interface->LogError("LUA RemoveCoin command error: spawn is not a player");
		return 0;
	}

	if (amount == 0) {
		lua_interface->LogError("LUA RemoveCoin command error: amount must be greater then 0");
		return 0;
	}

	lua_interface->SetBooleanValue(state, static_cast<Player*>(player)->RemoveCoins(amount));
	return 1;
}

int EQ2Emu_lua_GetPlayersInZone(lua_State* state) {
	if (!lua_interface)
		return 0;

	ZoneServer* zone = lua_interface->GetZone(state);
	if (!zone) {
		lua_interface->LogError("LUA GetPlayersInZone command error: zone is not valid");
		return 0;
	}

	vector<Entity*> players = zone->GetPlayers();
	if (players.size() == 0)
		return 0;

	lua_createtable(state, players.size(), 0);
	int newTable = lua_gettop(state);
	for (int32 i = 0; i < players.size(); i++) {
		lua_interface->SetSpawnValue(state, players.at(i));
		lua_rawseti(state, newTable, i + 1);
	}

	return 1;
}

int EQ2Emu_lua_GetWeaponDamageType(lua_State * state)
{
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);

	if (spawn) {
		if (spawn->IsEntity()) {
			lua_interface->SetInt32Value(state, static_cast<Entity*>(spawn)->GetPrimaryWeaponType());
			return 1;
		}
	}

	return 0;
}

int EQ2Emu_lua_PauseMovement(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);

	if (spawn) {
		if (spawn->IsNPC()) {
			static_cast<NPC*>(spawn)->MovementInterrupted(true);
			static_cast<NPC*>(spawn)->ClearRunningLocations();
			static_cast<NPC*>(spawn)->CalculateRunningLocation(true);
			return 1;
		} else {
			lua_interface->LogError("LUA PauseMovement command error: spawn is not an NPC");
			return 0;
		}
	} else {
		lua_interface->LogError("LUA PauseMovement command error: spawn is not valid");
		return 0;
	}
}

int EQ2Emu_lua_ResumeMovement(lua_State* state) {
	if (!lua_interface)
		return 0;

	Spawn* spawn = lua_interface->GetSpawn(state);

	if (spawn) {
		if (spawn->IsNPC()) {
			static_cast<NPC*>(spawn)->MovementInterrupted(false);
			return 1;
		} else {
			lua_interface->LogError("LUA PauseMovement command error: spawn is not an NPC");
			return 0;
		}
	} else {
		lua_interface->LogError("LUA PauseMovement command error: spawn is not valid");
		return 0;
	}
}

int EQ2Emu_lua_GetProcPercentageForWeapon(lua_State* state) {
	if (!lua_interface)
		return 0;

	Item* item = lua_interface->GetItem(state);

	if (!item || !item->IsWeapon()) {
		lua_interface->LogError("LUA GetProcPercentageForWeapon command error: item is not valid or is not weapon");
		return 0;
	}

	float times_per_minute = lua_interface->GetFloatValue(state, 2);

	lua_interface->SetFloatValue(state, times_per_minute / (60 / (item->weapon_info->delay / 10.0)) * 100);
	return 1;
}

int EQ2Emu_lua_SpawnGroupByID(lua_State* state){
	if (!lua_interface)
		return 0;

	ZoneServer* zone = lua_interface->GetZone(state, 1);
	if (!zone) {
		lua_interface->LogError("LUA GetPlayersInZone command error: zone is not valid");
		return 0;
	}

	int32 group_id = lua_interface->GetInt32Value(state, 2);

	//Map of <placement_id, location_id>
	map<int32, int32>* locs = zone->GetSpawnLocationsByGroup(group_id);
	map<int32, int32>::iterator itr;

	vector<Spawn*> group;

	for (itr = locs->begin(); itr != locs->end(); ++itr){
		SpawnLocation* location = zone->GetSpawnLocation(itr->second);
		if (!location) {
			lua_interface->LogError("LUA SpawnByLocationID command error: no location found for the given ID (%u)", itr->second);
			return 0;
		}

		if (location->entities[0]) {
			Spawn* spawn = 0;
			if (location->entities[0]->spawn_type == SPAWN_ENTRY_TYPE_NPC)
				spawn = zone->AddNPCSpawn(location, location->entities[0]);
			else if (location->entities[0]->spawn_type == SPAWN_ENTRY_TYPE_GROUNDSPAWN)
				spawn = zone->AddGroundSpawn(location, location->entities[0]);
			else if (location->entities[0]->spawn_type == SPAWN_ENTRY_TYPE_OBJECT)
				spawn = zone->AddObjectSpawn(location, location->entities[0]);
			else if (location->entities[0]->spawn_type == SPAWN_ENTRY_TYPE_WIDGET)
				spawn = zone->AddWidgetSpawn(location, location->entities[0]);
			else if (location->entities[0]->spawn_type == SPAWN_ENTRY_TYPE_SIGN)
				spawn = zone->AddSignSpawn(location, location->entities[0]);
			if (!spawn) {
				LogWrite(ZONE__ERROR, 0, "Zone", "Error adding spawn to zone");
				safe_delete(spawn);
			}

			const char* script = 0;
			for (int x = 0; x<3; x++) {
				switch (x) {
				case 0:
					script = world.GetSpawnEntryScript(location->entities[0]->spawn_entry_id);
					break;
				case 1:
					script = world.GetSpawnLocationScript(location->entities[0]->spawn_location_id);
					break;
				case 2:
					script = world.GetSpawnScript(location->entities[0]->spawn_id);
					break;
				}
				if (script && lua_interface->GetSpawnScript(script) != 0) {
					spawn->SetSpawnScript(string(script));
					break;
				}
			}
			if (spawn) {
				zone->CallSpawnScript(spawn, SPAWN_SCRIPT_SPAWN);
				lua_interface->SetSpawnValue(state, spawn);
				group.push_back(spawn);
			}
		}
	}

	if (!group.empty()){
		lua_createtable(state, group.size(), 0);
		int newTable = lua_gettop(state);
		for (int32 i = 0; i < group.size(); i++) {
			lua_interface->SetSpawnValue(state, group[i]);
			lua_rawseti(state, newTable, i + 1);
		}
	}
	else
		lua_pushnil(state);

	return 1;
}