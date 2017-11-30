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
#include "LuaInterface.h"
#include "LuaFunctions.h"
#include "WorldDatabase.h"
#include "../common/Log.h"

#ifndef WIN32
    #include <stdio.h>
    #include <sys/types.h>
	#include <sys/stat.h>
    #include <dirent.h>
    #include <pthread.h>
#else
	#include <process.h>
#endif

extern WorldDatabase database;


LuaInterface::LuaInterface() {
	shutting_down = false;
	spawn_scripts_reloading = false;
	MDebugClients.SetName("LuaInterface::MDebugClients");
	MSpells.SetName("LuaInterface::MSpells");
	MSpawnScripts.SetName("LuaInterface::MSpawnScripts");
	MZoneScripts.SetName("LuaInterface::MZoneScripts");
	MQuests.SetName("LuaInterface::MQuests");
	MLUAUserData.SetName("LuaInterface::MLUAUserData");
	MLUAMain.SetName("LuaInterface::MLUAMain");
	MItemScripts.SetName("LuaInterface::MItemScripts");
	user_data_timer = new Timer(20000);
	user_data_timer->Start();
	spell_delete_timer = new Timer(5000);
	spell_delete_timer->Start();
}
#ifdef WIN32
vector<string>* LuaInterface::GetDirectoryListing(const char* directory) {
	vector<string>* ret = new vector<string>;
    WIN32_FIND_DATA fdata;
    HANDLE dhandle;
    char buf[MAX_PATH];
    snprintf(buf, sizeof(buf), "%s\\*", directory);
    if((dhandle = FindFirstFile(buf, &fdata)) == INVALID_HANDLE_VALUE) {
		safe_delete(ret);
		return 0;
    }

    ret->push_back(string(fdata.cFileName));

    while(1) {
        if(FindNextFile(dhandle, &fdata)) {
			ret->push_back(string(fdata.cFileName));
        } 
		else{
            if(GetLastError() == ERROR_NO_MORE_FILES) {
                break;
            } else {
				safe_delete(ret);
                FindClose(dhandle);
                return 0;
            }
        }
    }

    if(FindClose(dhandle) == 0) {
		safe_delete(ret);
        return 0;
    }
	return ret;
}
#else
vector<string>* LuaInterface::GetDirectoryListing(const char* directory) {
	vector<string>* ret = new vector<string>;
	DIR *dp;
	struct dirent *ep;
	dp = opendir (directory);
	if (dp != NULL){
		while ((ep = readdir (dp)))
			ret->push_back(string(ep->d_name));
		(void) closedir (dp);
    }
	else {
		safe_delete(ret);
		return 0;
	}

	return ret;
}
#endif

LuaInterface::~LuaInterface() {
	shutting_down = true;
	MLUAMain.lock();
	DestroySpells();
	DestroySpawnScripts();
	DestroyQuests();
	DestroyItemScripts();
	DestroyZoneScripts();
	DeleteUserDataPtrs(true);
	DeletePendingSpells(true);
	safe_delete(user_data_timer);
	safe_delete(spell_delete_timer);
}

void LuaInterface::Process() {
	if(shutting_down)
		return;
	MLUAMain.lock();
	if(user_data_timer && user_data_timer->Check())
		DeleteUserDataPtrs(false);
	if(spell_delete_timer && spell_delete_timer->Check())
		DeletePendingSpells(false);
	MLUAMain.unlock();
}

void LuaInterface::DestroySpells() {
	map<string, LuaSpell*>::iterator itr;
	MSpells.lock();
	for(itr = spells.begin(); itr != spells.end(); itr++){
		lua_close(itr->second->state);
		safe_delete(itr->second);
	}
	spells.clear();
	MSpells.unlock();
}

void LuaInterface::DestroyQuests(bool reload) {
	map<int32, lua_State*>::iterator itr;
	MQuests.lock();
	for(itr = quest_states.begin(); itr != quest_states.end(); itr++){
		safe_delete(quests[itr->first]);
		lua_close(itr->second);
	}
	quests.clear();
	quest_states.clear();
	map<int32, Mutex*>::iterator mutex_itr;
	for(mutex_itr = quests_mutex.begin(); mutex_itr != quests_mutex.end(); mutex_itr++){
		safe_delete(mutex_itr->second);
	}
	quests_mutex.clear();
	if(reload)
		database.LoadQuests();
	MQuests.unlock();
}

void LuaInterface::DestroyItemScripts() {
	map<string, map<lua_State*, bool> >::iterator itr;
	map<lua_State*, bool>::iterator state_itr;
	Mutex* mutex = 0;
	MItemScripts.writelock(__FUNCTION__, __LINE__);
	for(itr = item_scripts.begin(); itr != item_scripts.end(); itr++){
		mutex = GetItemScriptMutex(itr->first.c_str());
		mutex->writelock(__FUNCTION__, __LINE__);
		for(state_itr = itr->second.begin(); state_itr != itr->second.end(); state_itr++)
			lua_close(state_itr->first);
		mutex->releasewritelock(__FUNCTION__, __LINE__);
		safe_delete(mutex);
	}
	item_scripts.clear();
	item_scripts_mutex.clear();
	MItemScripts.releasewritelock(__FUNCTION__, __LINE__);
}

void LuaInterface::DestroySpawnScripts() {
	map<string, map<lua_State*, bool> >::iterator itr;
	map<lua_State*, bool>::iterator state_itr;
	Mutex* mutex = 0;
	MSpawnScripts.writelock(__FUNCTION__, __LINE__);
	for(itr = spawn_scripts.begin(); itr != spawn_scripts.end(); itr++){
		mutex = GetSpawnScriptMutex(itr->first.c_str());
		mutex->writelock(__FUNCTION__, __LINE__);
		for(state_itr = itr->second.begin(); state_itr != itr->second.end(); state_itr++)
			lua_close(state_itr->first);
		mutex->releasewritelock(__FUNCTION__, __LINE__);
		safe_delete(mutex);
	}
	spawn_scripts.clear();
	spawn_scripts_mutex.clear();
	MSpawnScripts.releasewritelock(__FUNCTION__, __LINE__);
}

void LuaInterface::DestroyZoneScripts()  {
	map<string, map<lua_State*, bool> >::iterator itr;
	map<lua_State*, bool>::iterator state_itr;
	Mutex* mutex = 0;
	MZoneScripts.writelock(__FUNCTION__, __LINE__);
	for (itr = zone_scripts.begin(); itr != zone_scripts.end(); itr++){
		mutex = GetZoneScriptMutex(itr->first.c_str());
		mutex->writelock(__FUNCTION__, __LINE__);
		for(state_itr = itr->second.begin(); state_itr != itr->second.end(); state_itr++)
			lua_close(state_itr->first);
		mutex->releasewritelock(__FUNCTION__, __LINE__);
		safe_delete(mutex);
	}
	zone_scripts.clear();
	zone_scripts_mutex.clear();
	MZoneScripts.releasewritelock(__FUNCTION__, __LINE__);
}

void LuaInterface::ReloadSpells() {
	DestroySpells();
	database.LoadSpellScriptData();
}

bool LuaInterface::LoadLuaSpell(const char* name) {
	LuaSpell* spell = 0;
	string lua_script = string(name);
	if (lua_script.find(".lua") == string::npos)
		lua_script.append(".lua");
	lua_State* state = LoadLuaFile(lua_script.c_str());
	if(state){
		spell = new LuaSpell;
		spell->file_name = lua_script;
		spell->state = state;
		spell->spell = 0;
		spell->caster = 0;
		spell->initial_target = 0;
		spell->resisted = false;
		spell->interrupted = false;
		spell->last_spellattack_hit = false;
		spell->crit = false;
		spell->MSpellTargets.SetName("LuaSpell.MSpellTargets");
		spell->cancel_after_all_triggers = false;
		spell->num_triggers = 0;
		spell->had_triggers = false;
		spell->had_dmg_remaining = false;
		spell->slot_pos = 0;
		spell->damage_remaining = 0;
		spell->effect_bitmask = 0;

		MSpells.lock();
		if (spells.count(lua_script) > 0) {
			lua_close(spells[lua_script]->state);
			safe_delete(spells[lua_script]);
		}
		spells[lua_script] = spell;
		MSpells.unlock();

		return true;
	}
	return false;
}

bool LuaInterface::LoadLuaSpell(string name) {
	return LoadLuaSpell(name.c_str());
}

bool LuaInterface::LoadItemScript(string name) {
	return LoadItemScript(name.c_str());
}

bool LuaInterface::LoadItemScript(const char* name) {
	bool ret = false;
	if(name){
		lua_State* state = LoadLuaFile(name);
		if(state){
			MItemScripts.writelock(__FUNCTION__, __LINE__);
			item_scripts[name][state] = false;
			MItemScripts.releasewritelock(__FUNCTION__, __LINE__);
			ret = true;
		}
	}
	return ret;
}

bool LuaInterface::LoadSpawnScript(const char* name) {
	bool ret = false;
	if(name){
		lua_State* state = LoadLuaFile(name);
		if(state){
			MSpawnScripts.writelock(__FUNCTION__, __LINE__);
			spawn_scripts[name][state] = false;
			MSpawnScripts.releasewritelock(__FUNCTION__, __LINE__);
			ret = true;
		}
	}
	return ret;
}

bool LuaInterface::LoadZoneScript(const char* name)  {
	bool ret = false;
	if (name) {
		lua_State* state = LoadLuaFile(name);
		if (state) {
			MZoneScripts.writelock(__FUNCTION__, __LINE__);
			zone_scripts[name][state] = false;
			MZoneScripts.releasewritelock(__FUNCTION__, __LINE__);
			ret = true;
		}
	}
	return ret;
}

void LuaInterface::ProcessErrorMessage(const char* message) {
	MDebugClients.lock();
	vector<Client*> delete_clients;
	map<Client*, int32>::iterator itr;
	for(itr = debug_clients.begin(); itr != debug_clients.end(); itr++){
		if((Timer::GetCurrentTime2() - itr->second) > 60000)
			delete_clients.push_back(itr->first);
		else
			itr->first->Message(CHANNEL_COLOR_RED, "LUA Error: %s", message);
	}
	for(int32 i=0;i<delete_clients.size();i++)
		debug_clients.erase(delete_clients[i]);
	MDebugClients.unlock();
}

void LuaInterface::RemoveDebugClients(Client* client) {
	MDebugClients.lock();
	debug_clients.erase(client);
	MDebugClients.unlock();
}

void LuaInterface::UpdateDebugClients(Client* client) {
	MDebugClients.lock();
	debug_clients[client] = Timer::GetCurrentTime2();
	MDebugClients.unlock();
}

Mutex*  LuaInterface::GetQuestMutex(Quest* quest) {
	Mutex* ret = 0;
	MQuests.lock();
	if(quests_mutex.count(quest->GetQuestID()) == 0){
		ret = new Mutex();
		quests_mutex[quest->GetQuestID()] = ret;
		ret->SetName(string("Quest::").append(quest->GetName()));
	}
	else
		ret = quests_mutex[quest->GetQuestID()];
	MQuests.unlock();
	return ret;
}

void LuaInterface::CallQuestFunction(Quest* quest, const char* function, Spawn* player, int32 step_id) {
	if(shutting_down)
		return;
	lua_State* state = 0;
	if(quest){
		LogWrite(LUA__DEBUG, 0, "LUA", "Quest: %s, function: %s", quest->GetName(), function);
		Mutex* mutex = GetQuestMutex(quest);
		mutex->lock();
		if(quest_states.count(quest->GetQuestID()) > 0)
			state = quest_states[quest->GetQuestID()];
		if(state){
			int8 arg_count = 3;
			lua_getglobal(state, function);
			SetQuestValue(state, quest);
			Spawn* spawn = player->GetZone()->GetSpawnByDatabaseID(quest->GetQuestGiver());
			SetSpawnValue(state, spawn);
			SetSpawnValue(state, player);
			if(step_id != 0xFFFFFFFF){
				SetInt32Value(state, step_id);
				arg_count++;
			}
			if(lua_pcall(state, arg_count, 0, 0) != 0){
				LogError("Error processing quest function '%s': %s ", function, lua_tostring(state, -1));
				lua_pop(state, 1);
				mutex->unlock();
				return;
			}
		}
		mutex->unlock();
		LogWrite(LUA__DEBUG, 0, "LUA", "Done!");
	}
}

Quest* LuaInterface::LoadQuest(int32 id, const char* name, const char* type, const char* zone, int8 level, const char* description, char* script_name) {
	if(shutting_down)
		return 0;
	lua_State* state = LoadLuaFile(script_name);
	Quest* quest = 0;
	if(state){
		quest = new Quest(id);
		if (name)
			quest->SetName(string(name));
		if (type)
			quest->SetType(string(type));
		if (zone)
			quest->SetZone(string(zone));
		quest->SetLevel(level);
		if (description)
			quest->SetDescription(string(description));
		lua_getglobal(state, "Init");
		SetQuestValue(state, quest);
		if(lua_pcall(state, 1, 0, 0) != 0){
			LogError("Error processing Quest \"%s\" (%u): %s", name ? name : "unknown", id, lua_tostring(state, -1));
			lua_pop(state, 1);
			safe_delete(quest);
			return 0;
		}
		if(!quest->GetName()){
			safe_delete(quest);	
			return 0;
		}
		quest_states[id] = state;
		quests[id] = quest;
	}
	return quest;
}

bool LuaInterface::LoadSpawnScript(string name) {
	return LoadSpawnScript(name.c_str());
}

bool LuaInterface::LoadZoneScript(string name) {
	return LoadZoneScript(name.c_str());
}

void LuaInterface::AddSpawnPointers(LuaSpell* spell, bool first_cast, bool precast, const char* function, SpellScriptTimer* timer) {
	if (function)
		lua_getglobal(spell->state, function);
	else if (precast)
		lua_getglobal(spell->state, "precast");
	else if(first_cast)
		lua_getglobal(spell->state, "cast");
	else
		lua_getglobal(spell->state, "tick");

	Spawn* temp_spawn = 0;
	if (timer && timer->caster)
		temp_spawn = spell->caster->GetZone()->GetSpawnByID(timer->caster);

	if (temp_spawn)
		SetSpawnValue(spell->state, temp_spawn);
	else
		SetSpawnValue(spell->state, spell->caster);

	if (spell->initial_target && precast)
		SetSpawnValue(spell->state, spell->caster->GetZone()->GetSpawnByID(spell->initial_target));

	/*temp_spawn = 0;

	if (timer && timer->target)
		temp_spawn = spell->caster->GetZone()->GetSpawnByID(timer->target);

	if (temp_spawn)
		SetSpawnValue(spell->state, temp_spawn);
	else {
		if(spell->caster && spell->initial_target)
			SetSpawnValue(spell->state, spell->caster->GetZone()->GetSpawnByID(spell->initial_target));
		else if(spell->caster && spell->caster->GetTarget())
			SetSpawnValue(spell->state, spell->caster->GetTarget());
		else
			SetSpawnValue(spell->state, 0);
	}(*/
}

LuaSpell* LuaInterface::GetCurrentSpell(lua_State* state) {
	if(current_spells.count(state) > 0)
		return current_spells[state];
	return 0;
}

bool LuaInterface::CallSpellProcess(LuaSpell* spell, int8 num_parameters) {
	if(shutting_down || !spell || !spell->caster)
		return false;
	current_spells[spell->state] = spell;
	if(lua_pcall(spell->state, num_parameters, 0, 0) != 0){
		LogError("Error running %s", lua_tostring(spell->state, -1));
		lua_pop(spell->state, 1);
		RemoveSpell(spell, false);
		return false;
	}
	return true;
}

void LuaInterface::RemoveSpawnScript(const char* name) {
	lua_State* state = 0;
	Mutex* mutex = GetSpawnScriptMutex(name);
	while((state = GetSpawnScript(name, false))){
		mutex->writelock(__FUNCTION__, __LINE__);
		lua_close(state);
		spawn_scripts[name].erase(state);
		mutex->releasewritelock(__FUNCTION__, __LINE__);
	}
	MSpawnScripts.writelock(__FUNCTION__, __LINE__);
	spawn_scripts.erase(name);
	MSpawnScripts.releasewritelock(__FUNCTION__, __LINE__);
}

bool LuaInterface::CallItemScript(lua_State* state, int8 num_parameters) {
	if(shutting_down)
		return false;
	if(!state || lua_pcall(state, num_parameters, 0, 0) != 0){
		if (state){
			const char* err = lua_tostring(state, -1);
			LogError(err);
			lua_pop(state, 1);
		}
		return false;
	}
	return true;
}

bool LuaInterface::CallSpawnScript(lua_State* state, int8 num_parameters) {
	if(shutting_down)
		return false;
	if(!state || lua_pcall(state, num_parameters, 0, 0) != 0){
		if (state){
			const char* err = lua_tostring(state, -1);
			LogError(err);
			lua_pop(state, 1);
		}
		return false;
	}
	return true;
}

bool LuaInterface::CallZoneScript(lua_State* state, int8 num_parameters) {
	if(shutting_down)
		return false;
	if (!state || lua_pcall(state, num_parameters, 0, 0) != 0) {
		if (state){
			const char* err = lua_tostring(state, -1);
			LogError(err);
			lua_pop(state, 1);
		}
		return false;
	}
	return true;
}

lua_State* LuaInterface::LoadLuaFile(const char* name) {
	if(shutting_down)
		return 0;
	lua_State* state = luaL_newstate();
	luaL_openlibs(state);
	if(luaL_dofile(state, name) == 0){
		RegisterFunctions(state);
		return state;
	}
	else{
		LogError("Error loading %s (file name: '%s')", lua_tostring(state, -1), name);
		lua_pop(state, 1);
		lua_close(state);
	}
	return 0;
}

void LuaInterface::RemoveSpell(LuaSpell* spell, Spawn* spawn, bool call_remove_function, bool can_delete) {
	if(shutting_down)
		return;

	if(call_remove_function){
		lua_getglobal(spell->state, "remove");

		current_spells[spell->state] = spell;

		LUASpawnWrapper* spawn_wrapper = new LUASpawnWrapper();
		spawn_wrapper->spawn = spell->caster;
		AddUserDataPtr(spawn_wrapper);
		lua_pushlightuserdata(spell->state, spawn_wrapper);

		if(spawn) {
			spawn_wrapper = new LUASpawnWrapper();
			spawn_wrapper->spawn = spawn;
			AddUserDataPtr(spawn_wrapper);
			lua_pushlightuserdata(spell->state, spawn_wrapper);
		} else {
			lua_pushlightuserdata(spell->state, 0);
		}

		vector<LUAData*>* data = spell->spell->GetLUAData();
		for (int32 i = 0; i<data->size(); i++) {
			switch (data->at(i)->type) {
			case 0:
				lua_pushinteger(spell->state, data->at(i)->int_value);
				break;
			case 1:
				lua_pushnumber(spell->state, data->at(i)->float_value);
				break;
			case 2:
				lua_pushboolean(spell->state, data->at(i)->bool_value);
				break;
			case 3:
				lua_pushstring(spell->state, data->at(i)->string_value.c_str());
				break;
			}
		}

		current_spells[spell->state] = spell;
		lua_pcall(spell->state, data->size() + 2, 0, 0);
	}

	if (can_delete) {
		MSpellDelete.lock();
		spells_pending_delete[spell] = Timer::GetCurrentTime2() + 10000;
		MSpellDelete.unlock();
	}
}

void LuaInterface::RegisterFunctions(lua_State* state) {
	lua_register(state, "SetHP", EQ2Emu_lua_SetCurrentHP);
	lua_register(state, "SetMaxHP", EQ2Emu_lua_SetMaxHP);
	lua_register(state, "SetMaxHPBase", EQ2Emu_lua_SetMaxHPBase);
	lua_register(state, "SetPower", EQ2Emu_lua_SetCurrentPower);
	lua_register(state, "SetMaxPower", EQ2Emu_lua_SetMaxPower);
	lua_register(state, "SetMaxPowerBase", EQ2Emu_lua_SetMaxPowerBase);
	lua_register(state, "SetPosition", EQ2Emu_lua_SetPosition);
	lua_register(state, "SetHeading", EQ2Emu_lua_SetHeading);
	lua_register(state, "SetModelType", EQ2Emu_lua_SetModelType);
	lua_register(state, "SetAdventureClass", EQ2Emu_lua_SetAdventureClass);
	lua_register(state, "SetTradeskillClass", EQ2Emu_lua_SetTradeskillClass);
	lua_register(state, "SetMount", EQ2Emu_lua_SetMount);
	lua_register(state, "SetMountColor", EQ2Emu_lua_SetMountColor);
	lua_register(state, "GetMount", EQ2Emu_lua_GetMount);
	lua_register(state, "GetRace", EQ2Emu_lua_GetRace);
	lua_register(state, "GetRaceName", EQ2Emu_lua_GetRaceName);
	lua_register(state, "GetClass", EQ2Emu_lua_GetClass);
	lua_register(state, "GetClassName", EQ2Emu_lua_GetClassName);
	lua_register(state, "GetArchetypeName", EQ2Emu_lua_GetArchetypeName);
	lua_register(state, "SetSpeed", EQ2Emu_lua_SetSpeed);
	lua_register(state, "ModifyPower", EQ2Emu_lua_ModifyPower);
	lua_register(state, "ModifyHP", EQ2Emu_lua_ModifyHP);

	lua_register(state, "GetDistance", EQ2Emu_lua_GetDistance);
	lua_register(state, "GetHeading", EQ2Emu_lua_GetHeading);
	lua_register(state, "GetLevel", EQ2Emu_lua_GetLevel);
	lua_register(state, "GetHP", EQ2Emu_lua_GetCurrentHP);
	lua_register(state, "GetMaxHP", EQ2Emu_lua_GetMaxHP);
	lua_register(state, "GetMaxHPBase", EQ2Emu_lua_GetMaxHPBase);
	lua_register(state, "GetMaxPower", EQ2Emu_lua_GetMaxPower);
	lua_register(state, "GetMaxPowerBase", EQ2Emu_lua_GetMaxPowerBase);
	lua_register(state, "GetName", EQ2Emu_lua_GetName);
	lua_register(state, "GetPower", EQ2Emu_lua_GetCurrentPower);
	lua_register(state, "GetX", EQ2Emu_lua_GetX);
	lua_register(state, "GetY", EQ2Emu_lua_GetY);
	lua_register(state, "GetZ", EQ2Emu_lua_GetZ);
	lua_register(state, "GetSpawnID", EQ2Emu_lua_GetSpawnID);
	lua_register(state, "GetSpawnGroupID", EQ2Emu_lua_GetSpawnGroupID);
	lua_register(state, "GetSpawnLocationID", EQ2Emu_lua_GetSpawnLocationID);
	lua_register(state, "GetSpawnLocationPlacementID", EQ2Emu_lua_GetSpawnLocationPlacementID);
	lua_register(state, "GetFactionAmount", EQ2Emu_lua_GetFactionAmount);
	lua_register(state, "GetGender", EQ2Emu_lua_GetGender);
	lua_register(state, "GetTarget", EQ2Emu_lua_GetTarget);
	lua_register(state, "HasFreeSlot", EQ2Emu_lua_HasFreeSlot);
	lua_register(state, "HasItemEquipped", EQ2Emu_lua_HasItemEquipped);
	lua_register(state, "GetEquippedItemByID", EQ2Emu_lua_GetEquippedItemByID);
	lua_register(state, "GetEquippedItemBySlot", EQ2Emu_lua_GetEquippedItemBySlot);
	lua_register(state, "GetItemByID", EQ2Emu_lua_GetItemByID);
	lua_register(state, "GetItemType", EQ2Emu_lua_GetItemType);
	lua_register(state, "GetSpellName", EQ2Emu_lua_GetSpellName);
	
	lua_register(state, "GetModelType", EQ2Emu_lua_GetModelType);
	lua_register(state, "GetSpeed", EQ2Emu_lua_GetSpeed);
	lua_register(state, "HasMoved", EQ2Emu_lua_HasMoved);
	lua_register(state, "SpellDamage", EQ2Emu_lua_SpellDamage);
	lua_register(state, "CastSpell", EQ2Emu_lua_CastSpell);	
	lua_register(state, "SpellHeal", EQ2Emu_lua_SpellHeal);
	lua_register(state, "SummonItem", EQ2Emu_lua_SummonItem);
	lua_register(state, "RemoveItem", EQ2Emu_lua_RemoveItem);
	lua_register(state, "HasItem", EQ2Emu_lua_HasItem);
	lua_register(state, "SpawnMob", EQ2Emu_lua_Spawn);
	lua_register(state, "SummonPet", EQ2Emu_lua_SummonPet);
	lua_register(state, "AddSpawnAccess", EQ2Emu_lua_AddSpawnAccess);
	lua_register(state, "GetZone", EQ2Emu_lua_GetZone);
	lua_register(state, "GetZoneName", EQ2Emu_lua_GetZoneName);
	lua_register(state, "GetZoneID", EQ2Emu_lua_GetZoneID);
	lua_register(state, "Zone", EQ2Emu_lua_Zone);
	lua_register(state, "AddHate", EQ2Emu_lua_AddHate);
	lua_register(state, "IsAlive", EQ2Emu_lua_IsAlive);
	lua_register(state, "IsInCombat", EQ2Emu_lua_IsInCombat);
	lua_register(state, "Attack", EQ2Emu_lua_Attack);
	lua_register(state, "ApplySpellVisual", EQ2Emu_lua_ApplySpellVisual);
	
	lua_register(state, "IsPlayer", EQ2Emu_lua_IsPlayer);
	lua_register(state, "FaceTarget", EQ2Emu_lua_FaceTarget);
	lua_register(state, "MoveToLocation", EQ2Emu_lua_MoveToLocation);
	lua_register(state, "Shout", EQ2Emu_lua_Shout);
	lua_register(state, "Say", EQ2Emu_lua_Say);
	lua_register(state, "SayOOC", EQ2Emu_lua_SayOOC);
	lua_register(state, "Emote", EQ2Emu_lua_Emote);
	lua_register(state, "MovementLoopAddLocation", EQ2Emu_lua_MovementLoopAdd);
	lua_register(state, "GetCurrentZoneSafeLocation", EQ2Emu_lua_GetCurrentZoneSafeLocation);
	lua_register(state, "AddTimer", EQ2Emu_lua_AddTimer);
	lua_register(state, "Harvest", EQ2Emu_lua_Harvest);
	
	lua_register(state, "AddSpellBonus", EQ2Emu_lua_AddSpellBonus);
	lua_register(state, "RemoveSpellBonus", EQ2Emu_lua_RemoveSpellBonus);
	lua_register(state, "AddSkillBonus", EQ2Emu_lua_AddSkillBonus);
	lua_register(state, "RemoveSkillBonus", EQ2Emu_lua_RemoveSkillBonus);
	lua_register(state, "AddControlEffect", EQ2Emu_lua_AddControlEffect);
	lua_register(state, "RemoveControlEffect", EQ2Emu_lua_RemoveControlEffect);
	lua_register(state, "GetCurrentZoneSafeLocation", EQ2Emu_lua_GetCurrentZoneSafeLocation);
	lua_register(state, "GetInt", EQ2Emu_lua_GetInt);
	lua_register(state, "GetWis", EQ2Emu_lua_GetWis);
	lua_register(state, "GetSta", EQ2Emu_lua_GetSta);
	lua_register(state, "GetStr", EQ2Emu_lua_GetStr);
	lua_register(state, "GetAgi", EQ2Emu_lua_GetAgi);
	lua_register(state, "SetInt", EQ2Emu_lua_SetInt);
	lua_register(state, "SetWis", EQ2Emu_lua_SetWis);
	lua_register(state, "SetSta", EQ2Emu_lua_SetSta);
	lua_register(state, "SetStr", EQ2Emu_lua_SetStr);
	lua_register(state, "SetAgi", EQ2Emu_lua_SetAgi);
	lua_register(state, "GetIntBase", EQ2Emu_lua_GetIntBase);
	lua_register(state, "GetWisBase", EQ2Emu_lua_GetWisBase);
	lua_register(state, "GetStaBase", EQ2Emu_lua_GetStaBase);
	lua_register(state, "GetStrBase", EQ2Emu_lua_GetStrBase);
	lua_register(state, "GetAgiBase", EQ2Emu_lua_GetAgiBase);
	lua_register(state, "SetIntBase", EQ2Emu_lua_SetIntBase);
	lua_register(state, "SetWisBase", EQ2Emu_lua_SetWisBase);
	lua_register(state, "SetStaBase", EQ2Emu_lua_SetStaBase);
	lua_register(state, "SetStrBase", EQ2Emu_lua_SetStrBase);
	lua_register(state, "SetAgiBase", EQ2Emu_lua_SetAgiBase);
	lua_register(state, "GetSpawn", EQ2Emu_lua_GetSpawn);
	lua_register(state, "GetVariableValue", EQ2Emu_lua_GetVariableValue);
	lua_register(state, "GetCoinMessage", EQ2Emu_lua_GetCoinMessage);
	lua_register(state, "GetSpawnByGroupID", EQ2Emu_lua_GetSpawnByGroupID);
	lua_register(state, "GetSpawnByLocationID", EQ2Emu_lua_GetSpawnByLocationID);
	lua_register(state, "PlayFlavor", EQ2Emu_lua_PlayFlavor);
	lua_register(state, "PlaySound", EQ2Emu_lua_PlaySound);
	lua_register(state, "PlayVoice", EQ2Emu_lua_PlayVoice);
	lua_register(state, "PlayAnimation", EQ2Emu_lua_PlayAnimation);
	lua_register(state, "AddLootItem", EQ2Emu_lua_AddLootItem);
	lua_register(state, "RemoveLootItem", EQ2Emu_lua_RemoveLootItem);
	lua_register(state, "AddLootCoin", EQ2Emu_lua_AddLootCoin);
	lua_register(state, "GiveLoot", EQ2Emu_lua_GiveLoot);
	lua_register(state, "HasPendingLootItem", EQ2Emu_lua_HasPendingLootItem);
	lua_register(state, "HasPendingLoot", EQ2Emu_lua_HasPendingLoot);
	lua_register(state, "SetLootCoin", EQ2Emu_lua_SetLootCoin);
	lua_register(state, "GetLootCoin", EQ2Emu_lua_GetLootCoin);
	lua_register(state, "SetPlayerProximityFunction", EQ2Emu_lua_SetPlayerProximityFunction);
	lua_register(state, "SetLocationProximityFunction", EQ2Emu_lua_SetLocationProximityFunction);
	lua_register(state, "CreateConversation", EQ2Emu_lua_CreateConversation);
	lua_register(state, "AddConversationOption", EQ2Emu_lua_AddConversationOption);
	lua_register(state, "StartConversation", EQ2Emu_lua_StartConversation);
	lua_register(state, "CloseConversation", EQ2Emu_lua_CloseConversation);
	lua_register(state, "CloseItemConversation", EQ2Emu_lua_CloseItemConversation);
	//lua_register(state, "StartItemConversation", EQ2Emu_lua_StartItemConversation);
	lua_register(state, "StartDialogConversation", EQ2Emu_lua_StartDialogConversation);
	lua_register(state, "SpawnSet", EQ2Emu_lua_SpawnSet);
	lua_register(state, "SpawnSetByDistance", EQ2Emu_lua_SpawnSetByDistance);
	lua_register(state, "SpawnMove", EQ2Emu_lua_SpawnMove);
	lua_register(state, "KillSpawn", EQ2Emu_lua_KillSpawn); 
	lua_register(state, "KillSpawnByDistance", EQ2Emu_lua_KillSpawnByDistance);
	lua_register(state, "Despawn", EQ2Emu_lua_Despawn);
	lua_register(state, "IsBindAllowed", EQ2Emu_lua_IsBindAllowed);
	lua_register(state, "IsGateAllowed", EQ2Emu_lua_IsGateAllowed);
	lua_register(state, "Bind", EQ2Emu_lua_Bind);
	lua_register(state, "Gate", EQ2Emu_lua_Gate);
	lua_register(state, "SendMessage", EQ2Emu_lua_SendMessage);
	lua_register(state, "SendPopUpMessage", EQ2Emu_lua_SendPopUpMessage);
	lua_register(state, "SetServerControlFlag", EQ2Emu_lua_SetServerControlFlag);
	lua_register(state, "ToggleTracking", EQ2Emu_lua_ToggleTracking);
	lua_register(state, "AddPrimaryEntityCommand", EQ2Emu_lua_AddPrimaryEntityCommand);
	lua_register(state, "AddSpellBookEntry", EQ2Emu_lua_AddSpellBookEntry);
	lua_register(state, "Interrupt", EQ2Emu_lua_Interrupt);
	lua_register(state, "Stealth", EQ2Emu_lua_Stealth);
	lua_register(state, "IsInvis", EQ2Emu_lua_IsInvis);
	lua_register(state, "IsStealthed", EQ2Emu_lua_IsStealthed);
	lua_register(state, "AddSpawnIDAccess", EQ2Emu_lua_AddSpawnIDAccess);
	lua_register(state, "RemoveSpawnIDAccess", EQ2Emu_lua_RemoveSpawnIDAccess);
	lua_register(state, "HasRecipeBook", EQ2Emu_lua_HasRecipeBook);

	lua_register(state, "SetRequiredQuest", EQ2Emu_lua_SetRequiredQuest);
	lua_register(state, "SetRequiredHistory", EQ2Emu_lua_SetRequiredHistory);
	lua_register(state, "SetStepComplete", EQ2Emu_lua_SetStepComplete);
	lua_register(state, "AddStepProgress", EQ2Emu_lua_AddStepProgress);
	lua_register(state, "UpdateQuestTaskGroupDescription", EQ2Emu_lua_UpdateQuestTaskGroupDescription);
	lua_register(state, "GetTaskGroupStep", EQ2Emu_lua_GetTaskGroupStep);
	lua_register(state, "GetQuestStep", EQ2Emu_lua_GetQuestStep);
	lua_register(state, "QuestStepIsComplete", EQ2Emu_lua_QuestStepIsComplete);
	lua_register(state, "RegisterQuest", EQ2Emu_lua_RegisterQuest);
	lua_register(state, "SetQuestPrereqLevel", EQ2Emu_lua_SetQuestPrereqLevel);
	lua_register(state, "AddQuestPrereqQuest", EQ2Emu_lua_AddQuestPrereqQuest);
	lua_register(state, "AddQuestPrereqItem", EQ2Emu_lua_AddQuestPrereqItem);
	lua_register(state, "AddQuestPrereqFaction", EQ2Emu_lua_AddQuestPrereqFaction);
	lua_register(state, "AddQuestPrereqRace", EQ2Emu_lua_AddQuestPrereqRace);
	lua_register(state, "AddQuestPrereqModelType", EQ2Emu_lua_AddQuestPrereqModelType);
	lua_register(state, "AddQuestPrereqClass", EQ2Emu_lua_AddQuestPrereqClass);
	lua_register(state, "AddQuestPrereqTradeskillLevel", EQ2Emu_lua_AddQuestPrereqTradeskillLevel);
	lua_register(state, "AddQuestPrereqTradeskillClass", EQ2Emu_lua_AddQuestPrereqTradeskillClass);
	lua_register(state, "AddQuestSelectableRewardItem", EQ2Emu_lua_AddQuestSelectableRewardItem);
	lua_register(state, "AddQuestRewardItem", EQ2Emu_lua_AddQuestRewardItem);
	lua_register(state, "AddQuestRewardCoin", EQ2Emu_lua_AddQuestRewardCoin);
	lua_register(state, "AddQuestRewardFaction", EQ2Emu_lua_AddQuestRewardFaction);
	lua_register(state, "SetQuestRewardStatus", EQ2Emu_lua_SetQuestRewardStatus);
	lua_register(state, "SetQuestRewardComment", EQ2Emu_lua_SetQuestRewardComment);
	lua_register(state, "SetQuestRewardExp", EQ2Emu_lua_SetQuestRewardExp);
	lua_register(state, "AddQuestStepKill", EQ2Emu_lua_AddQuestStepKill);
	lua_register(state, "AddQuestStep", EQ2Emu_lua_AddQuestStep);
	lua_register(state, "AddQuestStepChat", EQ2Emu_lua_AddQuestStepChat);
	lua_register(state, "AddQuestStepObtainItem", EQ2Emu_lua_AddQuestStepObtainItem);
	lua_register(state, "AddQuestStepLocation", EQ2Emu_lua_AddQuestStepLocation);
	lua_register(state, "AddQuestStepSpell", EQ2Emu_lua_AddQuestStepSpell);
	lua_register(state, "AddQuestStepCraft", EQ2Emu_lua_AddQuestStepCraft);
	lua_register(state, "AddQuestStepHarvest", EQ2Emu_lua_AddQuestStepHarvest);
	lua_register(state, "AddQuestStepCompleteAction", EQ2Emu_lua_AddQuestStepCompleteAction);
	lua_register(state, "AddQuestStepProgressAction", EQ2Emu_lua_AddQuestStepProgressAction);
	lua_register(state, "SetQuestCompleteAction", EQ2Emu_lua_SetQuestCompleteAction);
	lua_register(state, "GiveQuestReward", EQ2Emu_lua_GiveQuestReward);
	lua_register(state, "UpdateQuestStepDescription", EQ2Emu_lua_UpdateQuestStepDescription);
	lua_register(state, "UpdateQuestDescription", EQ2Emu_lua_UpdateQuestDescription);
	lua_register(state, "UpdateQuestZone", EQ2Emu_lua_UpdateQuestZone);
	lua_register(state, "SetCompletedDescription", EQ2Emu_lua_SetCompletedDescription);
	lua_register(state, "OfferQuest", EQ2Emu_lua_OfferQuest);
	lua_register(state, "ProvidesQuest", EQ2Emu_lua_ProvidesQuest);	
	lua_register(state, "HasQuest", EQ2Emu_lua_HasQuest);	
	lua_register(state, "HasCompletedQuest", EQ2Emu_lua_HasCompletedQuest);	
	lua_register(state, "QuestIsComplete", EQ2Emu_lua_QuestIsComplete);	
	lua_register(state, "QuestReturnNPC", EQ2Emu_lua_QuestReturnNPC);		
	lua_register(state, "GetQuest", EQ2Emu_lua_GetQuest);
	lua_register(state, "HasCollectionsToHandIn", EQ2Emu_lua_HasCollectionsToHandIn);
	lua_register(state, "HandInCollections", EQ2Emu_lua_HandInCollections);
	lua_register(state, "UseWidget", EQ2Emu_lua_UseWidget);
	lua_register(state, "SetSpellList", EQ2Emu_lua_SetSpellList);
	lua_register(state, "GetPet", EQ2Emu_lua_GetPet);
	lua_register(state, "Charm", EQ2Emu_lua_Charm);
	lua_register(state, "GetGroup", EQ2Emu_lua_GetGroup);
	lua_register(state, "SetCompleteFlag", EQ2Emu_lua_SetCompleteFlag);
	lua_register(state, "SetQuestYellow", EQ2Emu_lua_SetQuestYellow);
	lua_register(state, "CanReceiveQuest", EQ2Emu_lua_CanReceiveQuest);
	lua_register(state, "AddTransportSpawn", EQ2Emu_lua_AddTransportSpawn);

	// Option window
	lua_register(state, "CreateOptionWindow", EQ2Emu_lua_CreateOptionWindow);
	lua_register(state, "AddOptionWindowOption", EQ2Emu_lua_AddOptionWindowOption);
	lua_register(state, "SendOptionWindow", EQ2Emu_lua_SendOptionWindow);

	lua_register(state, "GetTradeskillClass", EQ2Emu_lua_GetTradeskillClass);
	lua_register(state, "GetTradeskillLevel", EQ2Emu_lua_GetTradeskillLevel);
	lua_register(state, "GetTradeskillClassName", EQ2Emu_lua_GetTradeskillClassName);
	lua_register(state, "SetTradeskillLevel", EQ2Emu_lua_SetTradeskillLevel);

	lua_register(state, "SummonDeityPet", EQ2Emu_lua_SummonDeityPet);
	lua_register(state, "SummonCosmeticPet", EQ2Emu_lua_SummonCosmeticPet);
	lua_register(state, "DismissPet", EQ2Emu_lua_DismissPet);

	lua_register(state, "GetCharmedPet", EQ2Emu_lua_GetCharmedPet);
	lua_register(state, "GetDeityPet", EQ2Emu_lua_GetDeityPet);
	lua_register(state, "GetCosmeticPet", EQ2Emu_lua_GetCosmeticPet);

	lua_register(state, "SetQuestFeatherColor", EQ2Emu_lua_SetQuestFeatherColor);
	lua_register(state, "RemoveSpawnAccess", EQ2Emu_lua_RemoveSpawnAccess);
	lua_register(state, "SpawnByLocationID", EQ2Emu_lua_SpawnByLocationID);
	lua_register(state, "CastEntityCommand", EQ2Emu_lua_CastEntityCommand);
	lua_register(state, "SetLuaBrain", EQ2Emu_lua_SetLuaBrain);
	lua_register(state, "SetBrainTick", EQ2Emu_lua_SetBrainTick);
	lua_register(state, "SetFollowTarget", EQ2Emu_lua_SetFollowTarget);
	lua_register(state, "GetFollowTarget", EQ2Emu_lua_GetFollowTarget);
	lua_register(state, "ToggleFollow", EQ2Emu_lua_ToggleFollow);
	lua_register(state, "IsFollowing", EQ2Emu_lua_IsFollowing);
	lua_register(state, "SetTempVariable", EQ2Emu_lua_SetTempVariable);
	lua_register(state, "GetTempVariable", EQ2Emu_lua_GetTempVariable);
	lua_register(state, "GiveQuestItem", EQ2Emu_lua_GiveQuestItem);
	lua_register(state, "SetQuestRepeatable", EQ2Emu_lua_SetQuestRepeatable);

	lua_register(state, "AddWard", EQ2Emu_lua_AddWard);
	lua_register(state, "AddToWard", EQ2Emu_lua_AddToWard);
	lua_register(state, "RemoveWard", EQ2Emu_lua_RemoveWard);
	lua_register(state, "GetWardAmountLeft", EQ2Emu_lua_GetWardAmountLeft);

	lua_register(state, "SetTarget", EQ2Emu_lua_SetTarget);
	lua_register(state, "IsPet", EQ2Emu_lua_IsPet);
	lua_register(state, "GetOwner", EQ2Emu_lua_GetOwner);
	lua_register(state, "SetInCombat", EQ2Emu_lua_SetInCombat);
	lua_register(state, "CompareSpawns", EQ2Emu_lua_CompareSpawns);
	lua_register(state, "Runback", EQ2Emu_lua_Runback);
	lua_register(state, "GetRunbackDistance", EQ2Emu_lua_GetRunbackDistance);
	lua_register(state, "IsCasting", EQ2Emu_lua_IsCasting);
	lua_register(state, "IsMezzed", EQ2Emu_lua_IsMezzed);
	lua_register(state, "IsStunned", EQ2Emu_lua_IsStunned);
	lua_register(state, "IsMezzedOrStunned", EQ2Emu_lua_IsMezzedOrStunned);
	lua_register(state, "ProcessSpell", EQ2Emu_lua_ProcessSpell);
	lua_register(state, "ProcessMelee", EQ2Emu_lua_ProcessMelee);
	lua_register(state, "HasRecovered", EQ2Emu_lua_HasRecovered);
	lua_register(state, "GetEncounterSize", EQ2Emu_lua_GetEncounterSize);
	lua_register(state, "GetMostHated", EQ2Emu_lua_GetMostHated);
	lua_register(state, "ClearHate", EQ2Emu_lua_ClearHate);
	lua_register(state, "ClearEncounter", EQ2Emu_lua_ClearEncounter);
	lua_register(state, "GetEncounter", EQ2Emu_lua_GetEncounter);
	lua_register(state, "GetHateList", EQ2Emu_lua_GetHateList);
	lua_register(state, "HasGroup", EQ2Emu_lua_HasGroup);
	lua_register(state, "HasSpellEffect", EQ2Emu_lua_HasSpellEffect);

	lua_register(state, "SetSuccessTimer", EQ2Emu_lua_SetSuccessTimer);
	lua_register(state, "SetFailureTimer", EQ2Emu_lua_SetFailureTimer);
	lua_register(state, "IsGroundSpawn", EQ2Emu_lua_IsGroundSpawn);
	lua_register(state, "CanHarvest", EQ2Emu_lua_CanHarvest);
	lua_register(state, "SummonDumbFirePet", EQ2Emu_lua_SummonDumbFirePet);

	lua_register(state, "GetSkillValue", EQ2Emu_lua_GetSkillValue);
	lua_register(state, "GetSkillMaxValue", EQ2Emu_lua_GetSkillMaxValue);
	lua_register(state, "GetSkillName", EQ2Emu_lua_GetSkillName);
	lua_register(state, "SetSkillMaxValue", EQ2Emu_lua_SetSkillMaxValue);
	lua_register(state, "SetSkillValue", EQ2Emu_lua_SetSkillValue);
	lua_register(state, "GetSkill", EQ2Emu_lua_GetSkill);
	lua_register(state, "GetSkillIDByName", EQ2Emu_lua_GetSkillIDByName);
	lua_register(state, "AddProc", EQ2Emu_lua_AddProc);
	lua_register(state, "RemoveProc", EQ2Emu_lua_RemoveProc);
	lua_register(state, "Knockback", EQ2Emu_lua_Knockback);

	lua_register(state, "IsEpic", EQ2Emu_lua_IsEpic);
	lua_register(state, "IsHeroic", EQ2Emu_lua_IsHeroic);
	lua_register(state, "ProcDamage", EQ2Emu_lua_ProcDamage);
	lua_register(state, "ProcHeal", EQ2Emu_lua_ProcHeal);
	lua_register(state, "LastSpellAttackHit", EQ2Emu_lua_LastSpellAttackHit);
	lua_register(state, "IsBehind", EQ2Emu_lua_IsBehind);
	lua_register(state, "IsFlanking", EQ2Emu_lua_IsFlanking);
	lua_register(state, "AddSpellTimer", EQ2Emu_lua_AddSpellTimer);
	lua_register(state, "GetItemCount", EQ2Emu_lua_GetItemCount);
	lua_register(state, "SetItemCount", EQ2Emu_lua_SetItemCount);
	lua_register(state, "Resurrect", EQ2Emu_lua_Resurrect);
	lua_register(state, "BreatheUnderwater", EQ2Emu_lua_BreatheUnderwater);
	lua_register(state, "BlurVision", EQ2Emu_lua_BlurVision);
	lua_register(state, "SetVision", EQ2Emu_lua_SetVision);
	lua_register(state, "GetItemSkillReq", EQ2Emu_lua_GetItemSkillReq);
	lua_register(state, "SetSpeedMultiplier", EQ2Emu_lua_SetSpeeedMultiplier);
	lua_register(state, "SetIllusion", EQ2Emu_lua_SetIllusion);
	lua_register(state, "ResetIllusion", EQ2Emu_lua_ResetIllusion);
	lua_register(state, "AddThreatTransfer", EQ2Emu_lua_AddThreatTransfer);
	lua_register(state, "RemoveThreatTransfer", EQ2Emu_lua_RemoveThreatTransfer);
	lua_register(state, "CureByType", EQ2Emu_lua_CureByType);
	lua_register(state, "CureByControlEffect", EQ2Emu_lua_CureByControlEffect);
	lua_register(state, "AddSpawnSpellBonus", EQ2Emu_lua_AddSpawnSpellBonus);
	lua_register(state, "CancelSpell", EQ2Emu_lua_CancelSpell);
	lua_register(state, "RemoveStealth", EQ2Emu_lua_RemoveStealth);
	lua_register(state, "RemoveInvis", EQ2Emu_lua_RemoveInvis);
	lua_register(state, "StartHeroicOpportunity", EQ2Emu_lua_StartHeroicOpportunity);
	lua_register(state, "CopySpawnAppearance", EQ2Emu_lua_CopySpawnAppearance);
	lua_register(state, "SetSpellTriggerCount", EQ2Emu_lua_SetSpellTriggerCount);
	lua_register(state, "GetSpellTriggerCount", EQ2Emu_lua_GetSpellTriggerCount);
	lua_register(state, "RemoveTriggerFromSpell", EQ2Emu_lua_RemoveTriggerFromSpell);
	lua_register(state, "AddImmunitySpell", EQ2Emu_lua_AddImmunitySpell);
	lua_register(state, "RemoveImmunitySpell", EQ2Emu_lua_RemoveImmunitySpell);
	lua_register(state, "SetSpellSnareValue", EQ2Emu_lua_SetSpellSnareValue);
	lua_register(state, "CheckRaceType", EQ2Emu_lua_CheckRaceType);
	lua_register(state, "GetRaceType", EQ2Emu_lua_GetRaceType);
	lua_register(state, "GetRaceBaseType", EQ2Emu_lua_GetRaceBaseType);
	lua_register(state, "GetQuestFlags", EQ2Emu_lua_GetQuestFlags);
	lua_register(state, "SetQuestFlags", EQ2Emu_lua_SetQuestFlags);
	lua_register(state, "SetQuestTimer", EQ2Emu_lua_SetQuestTimer);
	lua_register(state, "RemoveQuestStep", EQ2Emu_lua_RemoveQuestStep);
	lua_register(state, "ResetQuestStep", EQ2Emu_lua_ResetQuestStep);
	lua_register(state, "SetQuestTimerComplete", EQ2Emu_lua_SetQuestTimerComplete);
	lua_register(state, "AddQuestStepFailureAction", EQ2Emu_lua_AddQuestStepFailureAction);
	lua_register(state, "SetStepFailed", EQ2Emu_lua_SetStepFailed);
	lua_register(state, "GetQuestCompleteCount", EQ2Emu_lua_GetQuestCompleteCount);
	lua_register(state, "SetServerVariable", EQ2Emu_lua_SetServerVariable);
	lua_register(state, "GetServerVariable", EQ2Emu_lua_GetServerVariable);
	lua_register(state, "HasLanguage", EQ2Emu_lua_HasLanguage);
	lua_register(state, "AddLanguage", EQ2Emu_lua_AddLanguage);
	lua_register(state, "IsNight", EQ2Emu_lua_IsNight);
	lua_register(state, "AddMultiFloorLift", EQ2Emu_lua_AddMultiFloorLift);
	lua_register(state, "StartAutoMount", EQ2Emu_lua_StartAutoMount);
	lua_register(state, "EndAutoMount", EQ2Emu_lua_EndAutoMount);
	lua_register(state, "IsOnAutoMount", EQ2Emu_lua_IsOnAutoMount);
	lua_register(state, "SetPlayerHistory", EQ2Emu_lua_SetPlayerHistory);
	lua_register(state, "GetPlayerHistory", EQ2Emu_lua_GetPlayerHistory);
	lua_register(state, "SetGridID", EQ2Emu_lua_SetGridID);
	lua_register(state, "GetQuestStepProgress", EQ2Emu_lua_GetQuestStepProgress);
	lua_register(state, "SetPlayerLevel", EQ2Emu_lua_SetPlayerLevel);
	lua_register(state, "AddCoin", EQ2Emu_lua_AddCoin);
	lua_register(state, "RemoveCoin", EQ2Emu_lua_RemoveCoin);
	lua_register(state, "GetPlayersInZone", EQ2Emu_lua_GetPlayersInZone);
	lua_register(state, "SpawnGroupByID", EQ2Emu_lua_SpawnGroupByID);
	lua_register(state, "GetWeaponDamageType", EQ2Emu_lua_GetWeaponDamageType);
	lua_register(state, "PauseMovement", EQ2Emu_lua_PauseMovement);
	lua_register(state, "ResumeMovement", EQ2Emu_lua_ResumeMovement);
	lua_register(state, "GetProcPercentageForWeapon", EQ2Emu_lua_GetProcPercentageForWeapon);
	lua_register(state, "RemoveSpell", EQ2Emu_lua_RemoveSpell);
}

void LuaInterface::LogError(const char* error, ...)  {
	va_list argptr;
	char buffer[4096];

	va_start(argptr, error);
	vsnprintf(buffer, sizeof(buffer), error, argptr);
	va_end(argptr);
	SimpleLogError(buffer);
}

void LuaInterface::SimpleLogError(const char* error) {
	ProcessErrorMessage(error);
	LogWrite(LUA__ERROR, 0, "LUA", "%s", error);
}

void LuaInterface::ResetFunctionStack(lua_State* state) {
	lua_settop(state, 0);
}

void LuaInterface::AddUserDataPtr(LUAUserData* data) {
	MLUAUserData.lock();
	user_data[data] = Timer::GetCurrentTime2() + 300000; //allow a function to use this pointer for 5 minutes
	MLUAUserData.unlock();
}

void LuaInterface::DeletePendingSpells(bool all) {
	MSpellDelete.lock();
	if(spells_pending_delete.size() > 0){
		int32 time = Timer::GetCurrentTime2();
		map<LuaSpell*, int32>::iterator itr;
		vector<LuaSpell*> tmp_deletes;
		vector<LuaSpell*>::iterator del_itr;
		for(itr = spells_pending_delete.begin(); itr != spells_pending_delete.end(); itr++){
			if(all || time >= itr->second)
				tmp_deletes.push_back(itr->first);
		}
		LuaSpell* spell = 0;
		for(del_itr = tmp_deletes.begin(); del_itr != tmp_deletes.end(); del_itr++){
			spell = *del_itr;
			spells_pending_delete.erase(spell);
			safe_delete(spell);
		}
	}
	MSpellDelete.unlock();
}

void LuaInterface::DeleteUserDataPtrs(bool all) {
	MLUAUserData.lock();
	if(user_data.size() > 0){
		map<LUAUserData*, int32>::iterator itr;
		int32 time = Timer::GetCurrentTime2();
		vector<LUAUserData*> tmp_deletes;
		vector<LUAUserData*>::iterator del_itr;
		for(itr = user_data.begin(); itr != user_data.end(); itr++){
			if(all || time >= itr->second)
				tmp_deletes.push_back(itr->first);
		}
		LUAUserData* data = 0;
		for(del_itr = tmp_deletes.begin(); del_itr != tmp_deletes.end(); del_itr++){
			data = *del_itr;
			user_data.erase(data);
			safe_delete(data);
		}
	}
	MLUAUserData.unlock();
}

Spawn* LuaInterface::GetSpawn(lua_State* state, int8 arg_num) {
	Spawn* ret = 0;
	if (lua_islightuserdata(state, arg_num)){
		LUAUserData* data = (LUAUserData*)lua_touserdata(state, arg_num);
		if(!data || !data->IsCorrectlyInitialized()){
			LogError("GetSpawn error while processing %s", lua_tostring(state, -1));
		}
		else if(!data->IsSpawn()){
			lua_Debug ar;
			lua_getstack (state, 1, &ar);
			lua_getinfo(state, "Sln", &ar);
			LogError("Invalid data type used for GetSpawn in %s (line %d)", ar.source, ar.currentline);
		}
		else
			ret = data->spawn;
	}
	return ret;
}

vector<ConversationOption>*	LuaInterface::GetConversation(lua_State* state, int8 arg_num) {
	vector<ConversationOption>* ret = 0;
	if(lua_islightuserdata(state, arg_num)){
		LUAUserData* data = (LUAUserData*)lua_touserdata(state, arg_num);
		if(!data || !data->IsCorrectlyInitialized()){
			LogError("GetConversation error while processing %s", lua_tostring(state, -1));
		}
		else if(!data->IsConversationOption()){
			lua_Debug ar;
			lua_getstack (state, 1, &ar);
			lua_getinfo(state, "Sln", &ar);
			LogError("Invalid data type used for GetConversation in %s (line %d)", ar.source, ar.currentline);
		}
		else
			ret = data->conversation_options;
	}
	return ret;
}

vector<OptionWindowOption>*	LuaInterface::GetOptionWindow(lua_State* state, int8 arg_num) {
	vector<OptionWindowOption>* ret = 0;
	if(lua_islightuserdata(state, arg_num)){
		LUAUserData* data = (LUAUserData*)lua_touserdata(state, arg_num);
		if(!data || !data->IsCorrectlyInitialized()){
			LogError("GetOptionWindow error while processing %s", lua_tostring(state, -1));
		}
		else if(!data->IsOptionWindow()){
			lua_Debug ar;
			lua_getstack (state, 1, &ar);
			lua_getinfo(state, "Sln", &ar);
			LogError("Invalid data type used for GetOptionWindow in %s (line %d)", ar.source, ar.currentline);
		}
		else
			ret = data->option_window_option;
	}
	return ret;
}

Quest* LuaInterface::GetQuest(lua_State* state, int8 arg_num) {
	Quest* ret = 0;
	if(lua_islightuserdata(state, arg_num)){
		LUAUserData* data = (LUAUserData*)lua_touserdata(state, arg_num);
		if(!data || !data->IsCorrectlyInitialized()){
			LogError("GetQuest error while processing %s", lua_tostring(state, 0));
		}
		else if(!data->IsQuest()){
			lua_Debug ar;
 			lua_getstack (state, 1, &ar);
			lua_getinfo(state, "Sln", &ar);
			LogError("Invalid data type used for GetQuest in %s (line %d)", ar.source, ar.currentline);
		}
		else
			ret = data->quest;
	}
	return ret;
}

Item* LuaInterface::GetItem(lua_State* state, int8 arg_num) {
	Item* ret = 0;
	if(lua_islightuserdata(state, arg_num)){
		LUAUserData* data = (LUAUserData*)lua_touserdata(state, arg_num);
		if(!data || !data->IsCorrectlyInitialized()){
			LogError("GetItem error while processing %s", lua_tostring(state, 0));
		}
		else if(!data->IsItem()){
			lua_Debug ar;
 			lua_getstack (state, 1, &ar);
			lua_getinfo(state, "Sln", &ar);
			LogError("Invalid data type used for GetItem in %s (line %d)", ar.source, ar.currentline);
		}
		else
			ret = data->item;
	}
	return ret;
}

Skill* LuaInterface::GetSkill(lua_State* state, int8 arg_num) {
	Skill* ret = 0;
	if (lua_islightuserdata(state, arg_num)) {
		LUAUserData* data = (LUAUserData*)lua_touserdata(state, arg_num);
		if(!data || !data->IsCorrectlyInitialized()){
			LogError("GetSkill error while processing %s", lua_tostring(state, 0));
		}
		else if(!data->IsSkill()){
			lua_Debug ar;
 			lua_getstack (state, 1, &ar);
			lua_getinfo(state, "Sln", &ar);
			LogError("Invalid data type used for GetSkill in %s (line %d)", ar.source, ar.currentline);
		}
		else
			ret = data->skill;
	}
	return ret;
}

ZoneServer* LuaInterface::GetZone(lua_State* state, int8 arg_num) {
	ZoneServer* ret = 0;
	if(lua_islightuserdata(state, arg_num)){
		LUAUserData* data = (LUAUserData*)lua_touserdata(state, arg_num);
		if(!data || !data->IsCorrectlyInitialized()){
			LogError("GetZone error while processing %s", lua_tostring(state, -1));
		}
		else if(!data->IsZone()){
			lua_Debug ar;
			lua_getstack (state, 1, &ar);
			lua_getinfo(state, "Sln", &ar);
			LogError("Invalid data type used for GetZone in %s (line %d)", ar.source, ar.currentline);
		}
		else
			ret = data->zone;
	}
	return ret;
}

sint32 LuaInterface::GetSInt32Value(lua_State* state, int8 arg_num) {
	sint32 val = 0;
	if(lua_isnumber(state, arg_num)){
		val = lua_tonumber(state, arg_num);
	}
	return val;
}

int32 LuaInterface::GetInt32Value(lua_State* state, int8 arg_num) {
	int32 val = 0;
	if(lua_isnumber(state, arg_num)){
		val = (int32)lua_tonumber(state, arg_num);
	}
	return val;
}

int16 LuaInterface::GetInt16Value(lua_State* state, int8 arg_num) {
	int16 val = 0;
	if(lua_isnumber(state, arg_num)){
		val = lua_tointeger(state, arg_num);
	}
	return val;
}

int8 LuaInterface::GetInt8Value(lua_State* state, int8 arg_num) {
	int8 val = 0;
	if(lua_isnumber(state, arg_num)){
		val = lua_tointeger(state, arg_num);
	}
	return val;
}

float LuaInterface::GetFloatValue(lua_State* state, int8 arg_num) {
	float val = 0;
	if(lua_isnumber(state, arg_num))
		val = (float)lua_tonumber(state, arg_num);
	return val;
}

string LuaInterface::GetStringValue(lua_State* state, int8 arg_num) {
	string val;
	if(lua_isstring(state, arg_num)){
		size_t size = 0;
		const char* str = lua_tolstring(state, arg_num, &size);
		if(str)
			val = string(str);
	}
	return val;
}

bool LuaInterface::GetBooleanValue(lua_State* state, int8 arg_num) {
	return lua_toboolean(state, arg_num) == 1;
}

void LuaInterface::SetStringValue(lua_State* state, const char* value) {
	lua_pushstring(state, value);
}

void LuaInterface::SetBooleanValue(lua_State* state, bool value) {
	lua_pushboolean(state, value);
}

void LuaInterface::SetFloatValue(lua_State* state, float value) {
	lua_pushnumber(state, value);
}

void LuaInterface::SetInt32Value(lua_State* state, int32 value) {
	lua_pushinteger(state, value);
}

void LuaInterface::SetSInt32Value(lua_State* state, sint32 value) {
	lua_pushinteger(state, value);
}

void LuaInterface::SetSpawnValue(lua_State* state, Spawn* spawn) {
	LUASpawnWrapper* spawn_wrapper = new LUASpawnWrapper();
	spawn_wrapper->spawn = spawn;
	AddUserDataPtr(spawn_wrapper);
	lua_pushlightuserdata(state, spawn_wrapper);
}

void LuaInterface::SetConversationValue(lua_State* state, vector<ConversationOption>* conversation) {
	LUAConversationOptionWrapper* conv_wrapper = new LUAConversationOptionWrapper();
	conv_wrapper->conversation_options = conversation;
	AddUserDataPtr(conv_wrapper);
	lua_pushlightuserdata(state, conv_wrapper);
}

void LuaInterface::SetOptionWindowValue(lua_State* state, vector<OptionWindowOption>* optionWindow) {
	LUAOptionWindowWrapper* option_wrapper = new LUAOptionWindowWrapper();
	option_wrapper->option_window_option = optionWindow;
	AddUserDataPtr(option_wrapper);
	lua_pushlightuserdata(state, option_wrapper);
}

void LuaInterface::SetItemValue(lua_State* state, Item* item) {
	LUAItemWrapper* item_wrapper = new LUAItemWrapper();
	item_wrapper->item = item;
	AddUserDataPtr(item_wrapper);
	lua_pushlightuserdata(state, item_wrapper);
}

void LuaInterface::SetSkillValue(lua_State* state, Skill* skill) {
	LUASkillWrapper* skill_wrapper = new LUASkillWrapper();
	skill_wrapper->skill = skill;
	AddUserDataPtr(skill_wrapper);
	lua_pushlightuserdata(state, skill_wrapper);
}

void LuaInterface::SetQuestValue(lua_State* state, Quest* quest) {
	LUAQuestWrapper* quest_wrapper = new LUAQuestWrapper();
	quest_wrapper->quest = quest;
	AddUserDataPtr(quest_wrapper);
	lua_pushlightuserdata(state, quest_wrapper);
}

void LuaInterface::SetZoneValue(lua_State* state, ZoneServer* zone) {
	LUAZoneWrapper* zone_wrapper = new LUAZoneWrapper();
	zone_wrapper->zone = zone;
	AddUserDataPtr(zone_wrapper);
	lua_pushlightuserdata(state, zone_wrapper);
}

LuaSpell* LuaInterface::GetSpell(const char* name)  {
	string lua_script = string(name);
	if (lua_script.find(".lua") == string::npos)
		lua_script.append(".lua");
	if(spells.count(lua_script) > 0)
	{
		LogWrite(LUA__DEBUG, 0, "LUA", "Found LUA Spell Script: '%s'", lua_script.c_str());
		LuaSpell* spell = spells[lua_script];
		LuaSpell* new_spell = new LuaSpell;
		new_spell->state = spell->state;
		new_spell->file_name = spell->file_name;
		new_spell->timer = spell->timer;
		new_spell->timer.Disable();
		new_spell->resisted = false;
		new_spell->interrupted = false;
		new_spell->crit = false;
		new_spell->last_spellattack_hit = false;
		new_spell->MSpellTargets.SetName("LuaSpell.MSpellTargets");
		new_spell->cancel_after_all_triggers = false;
		new_spell->num_triggers = 0;
		new_spell->had_triggers = false;
		new_spell->had_dmg_remaining = false;
		new_spell->slot_pos = 0;
		new_spell->damage_remaining = 0;
		new_spell->effect_bitmask = 0;
		return new_spell;
	}
	else{
		LogWrite(LUA__ERROR, 0, "LUA", "Error LUA Spell Script: '%s'", name);
		return 0;
	}
}

Mutex* LuaInterface::GetItemScriptMutex(const char* name) {
	Mutex* mutex = 0;
	if(item_scripts_mutex.count(name) > 0)
		mutex = item_scripts_mutex[name];
	if(!mutex){
		mutex = new Mutex();
		item_scripts_mutex[name] = mutex;
	}
	return mutex;
}

Mutex* LuaInterface::GetSpawnScriptMutex(const char* name) {
	Mutex* mutex = 0;
	if(spawn_scripts_mutex.count(string(name)) > 0)
		mutex = spawn_scripts_mutex[name];
	if(!mutex){
		mutex = new Mutex();
		spawn_scripts_mutex[name] = mutex;
	}
	return mutex;
}

Mutex* LuaInterface::GetZoneScriptMutex(const char* name) {
	Mutex* mutex = 0;
	if(zone_scripts_mutex.count(name) > 0)
		mutex = zone_scripts_mutex[name];
	if(!mutex){
		mutex = new Mutex();
		zone_scripts_mutex[name] = mutex;
	}
	return mutex;
}

void LuaInterface::UseItemScript(const char* name, lua_State* state, bool val) {
	item_scripts[name][state] = val;
}

void LuaInterface::UseSpawnScript(const char* name, lua_State* state, bool val) {
	spawn_scripts[name][state] = val;
}

void LuaInterface::UseZoneScript(const char* name, lua_State* state, bool val) {
	zone_scripts[name][state] = val;
}

lua_State* LuaInterface::GetItemScript(const char* name, bool create_new, bool use) {
	map<string, map<lua_State*, bool> >::iterator itr;
	map<lua_State*, bool>::iterator item_script_itr;
	lua_State* ret = 0;
	Mutex* mutex = 0;
	if(item_scripts.count(name) > 0){
		mutex = GetItemScriptMutex(name);
		mutex->readlock(__FUNCTION__, __LINE__);
		for(item_script_itr = item_scripts[name].begin(); item_script_itr != item_scripts[name].end(); item_script_itr++){
			if(!item_script_itr->second){ //not in use
				if(use)
					item_script_itr->second = true;
				ret = item_script_itr->first;
			}
		}
		mutex->releasereadlock(__FUNCTION__, __LINE__);
	}
	if(!ret && create_new){
		if(name && LoadItemScript(name))
			ret = GetItemScript(name, create_new, use);
		else{
			LogError("Error LUA Item Script '%s'", name);
			return 0;
		}
	}
	return ret;
}

lua_State* LuaInterface::GetSpawnScript(const char* name, bool create_new, bool use) {
	if (spawn_scripts_reloading)
		return 0;
	map<string, map<lua_State*, bool> >::iterator itr;
	map<lua_State*, bool>::iterator spawn_script_itr;
	lua_State* ret = 0;
	Mutex* mutex = 0;
	if(spawn_scripts.count(string(name)) > 0){
		mutex = GetSpawnScriptMutex(name);
		mutex->readlock(__FUNCTION__, __LINE__);
		for(spawn_script_itr = spawn_scripts[name].begin(); spawn_script_itr != spawn_scripts[name].end(); spawn_script_itr++){
			if(!spawn_script_itr->second){ //not in use
				if(use)
					spawn_script_itr->second = true;
				ret = spawn_script_itr->first;
			}
		}
		mutex->releasereadlock(__FUNCTION__, __LINE__);
	}
	if(!ret && create_new){
		if(name && LoadSpawnScript(name))
			ret = GetSpawnScript(name, create_new, use);
		else{
			LogError("Error LUA Spawn Script '%s'", name);
			return 0;
		}
	}
	return ret;
}

lua_State* LuaInterface::GetZoneScript(const char* name, bool create_new, bool use)  {
	map<string, map<lua_State*, bool> >::iterator itr;
	map<lua_State*, bool>::iterator zone_script_itr;
	lua_State* ret = 0;
	Mutex* mutex = 0;
	if(zone_scripts.count(name) > 0){
		mutex = GetZoneScriptMutex(name);
		mutex->readlock(__FUNCTION__, __LINE__);
		for(zone_script_itr = zone_scripts[name].begin(); zone_script_itr != zone_scripts[name].end(); zone_script_itr++){
			if(!zone_script_itr->second){ //not in use
				if(use)
					zone_script_itr->second = true;
				ret = zone_script_itr->first;
			}
		}
		mutex->releasereadlock(__FUNCTION__, __LINE__);
	}
	if(!ret && create_new){
		if(name && LoadZoneScript(name))
			ret = GetZoneScript(name);
		else{
			LogError("Error LUA Zone Script '%s'", name);
			return 0;
		}
	}
	return ret;
}

bool LuaInterface::RunItemScript(string script_name, const char* function_name, Item* item, Spawn* spawn) {
	if(!item)
		return false;
	lua_State* state = GetItemScript(script_name.c_str(), true, true);
	if(state){
		Mutex* mutex = GetItemScriptMutex(script_name.c_str());
		if(mutex)
			mutex->readlock(__FUNCTION__, __LINE__);
		else{
			LogError("Error getting lock for '%s'", script_name.c_str());
			UseItemScript(script_name.c_str(), state, false);
			return false;
		}
		lua_getglobal(state, function_name);
		lua_getglobal(state, function_name);
		if (!lua_isfunction(state, lua_gettop(state))){
			lua_pop(state, 1);
			mutex->releasereadlock(__FUNCTION__);
			return false;
		}
		SetItemValue(state, item);
		int8 num_parms = 1;
		if(spawn){
			SetSpawnValue(state, spawn);
			num_parms++;
		}
		if(!CallItemScript(state, num_parms)){
			if(mutex)
				mutex->releasereadlock(__FUNCTION__, __LINE__);
			UseItemScript(script_name.c_str(), state, false);
			return false;
		}
		if(mutex)
			mutex->releasereadlock(__FUNCTION__, __LINE__);
		UseItemScript(script_name.c_str(), state, false);
		return true;
	}
	else
		return false;
}


bool LuaInterface::RunSpawnScript(string script_name, const char* function_name, Spawn* npc, Spawn* spawn, const char* message) {
	if(!npc || spawn_scripts_reloading)
		return false;
	lua_State* state = GetSpawnScript(script_name.c_str(), true, true);
	if(state){
		Mutex* mutex = GetSpawnScriptMutex(script_name.c_str());
		if(mutex)
			mutex->readlock(__FUNCTION__, __LINE__);
		else{
			LogError("Error getting lock for '%s'", script_name.c_str());
			UseSpawnScript(script_name.c_str(), state, false);
			return false;
		}
		lua_getglobal(state, function_name);
		if (!lua_isfunction(state, lua_gettop(state))){
			lua_pop(state, 1);
			mutex->releasereadlock(__FUNCTION__);
			return false;
		}
		SetSpawnValue(state, npc);
		int8 num_parms = 1;
		if(spawn){
			SetSpawnValue(state, spawn);
			num_parms++;
		}
		if(message){
			SetStringValue(state, message);
			num_parms++;
		}
		if(!CallSpawnScript(state, num_parms)){
			if(mutex)
				mutex->releasereadlock(__FUNCTION__, __LINE__);
			UseSpawnScript(script_name.c_str(), state, false);
			return false;
		}
		if(mutex)
			mutex->releasereadlock(__FUNCTION__, __LINE__);
		UseSpawnScript(script_name.c_str(), state, false);
		return true;
	}
	else
		return false;
}

bool LuaInterface::RunZoneScript(string script_name, const char* function_name, ZoneServer* zone, Spawn* spawn, int32 grid_id)  {
	if (!zone)
		return false;
	lua_State* state = GetZoneScript(script_name.c_str(), true, true);
	if (state) {
		Mutex* mutex = GetZoneScriptMutex(script_name.c_str());
		if (mutex)
			mutex->readlock(__FUNCTION__, __LINE__);
		else {
			LogError("Error getting lock for '%s'", script_name.c_str());
			UseZoneScript(script_name.c_str(), state, false);
			return false;
		}
		lua_getglobal(state, function_name);
		if (!lua_isfunction(state, lua_gettop(state))){
			lua_pop(state, 1);
			mutex->releasereadlock(__FUNCTION__);
			return false;
		}
		SetZoneValue(state, zone);
		int8 num_params = 1;
		if (spawn) {
			SetSpawnValue(state, spawn);
			num_params++;
		}
		if (grid_id > 0) {
			SetInt32Value(state, grid_id);
			num_params++;
		}
		if (!CallZoneScript(state, num_params)) {
			if (mutex)
				mutex->releasereadlock(__FUNCTION__, __LINE__);
			UseZoneScript(script_name.c_str(), state, false);
			return false;
		}
		if (mutex)
			mutex->releasereadlock(__FUNCTION__, __LINE__);
		UseZoneScript(script_name.c_str(), state, false);
		return true;
	}
	else
		return false;
}

void LuaInterface::AddPendingSpellDelete(LuaSpell* spell) {
	MSpellDelete.lock();
	spells_pending_delete[spell] = Timer::GetCurrentTime2() + 10000;
	MSpellDelete.unlock();
}

LUAUserData::LUAUserData(){
	correctly_initialized = false;
	quest = 0;
	conversation_options = 0;
	spawn = 0;
	zone = 0;
	skill = 0;
	option_window_option = 0;
	item = 0;
}

bool LUAUserData::IsCorrectlyInitialized(){
	return correctly_initialized;
}

bool LUAUserData::IsConversationOption(){
	return false;
}

bool LUAUserData::IsOptionWindow() {
	return false;
}

bool LUAUserData::IsSpawn(){
	return false;
}

bool LUAUserData::IsQuest(){
	return false;
}

bool LUAUserData::IsZone(){
	return false;
}

bool LUAUserData::IsItem(){
	return false;
}

bool LUAUserData::IsSkill() {
	return false;
}

LUAConversationOptionWrapper::LUAConversationOptionWrapper(){
	correctly_initialized = true;
}

bool LUAConversationOptionWrapper::IsConversationOption(){
	return true;
}

LUAOptionWindowWrapper::LUAOptionWindowWrapper() {
	correctly_initialized = true;
}

bool LUAOptionWindowWrapper::IsOptionWindow() {
	return true;
}

LUASpawnWrapper::LUASpawnWrapper(){
	correctly_initialized = true;
}

bool LUASpawnWrapper::IsSpawn(){
	return true;
}

LUAZoneWrapper::LUAZoneWrapper(){
	correctly_initialized = true;
}

bool LUAZoneWrapper::IsZone(){
	return true;
}

LUAQuestWrapper::LUAQuestWrapper(){
	correctly_initialized = true;
}

bool LUAQuestWrapper::IsQuest(){
	return true;
}

LUAItemWrapper::LUAItemWrapper(){
	correctly_initialized = true;
}

bool LUAItemWrapper::IsItem(){
	return true;
}

LUASkillWrapper::LUASkillWrapper() {
	correctly_initialized = true;
}

bool LUASkillWrapper::IsSkill() {
	return true;
}