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
#ifndef LUA_INTERFACE_H
#define LUA_INTERFACE_H

#include "Spawn.h"
#include "Spells.h"
#include "../common/Mutex.h"
#include "Quests.h"
#include "zoneserver.h"
#include "client.h"

#include "../LUA/lua.hpp"

using namespace std;

struct ConversationOption{
	string option;
	string function;
};

struct OptionWindowOption {
	string	optionName;
	string	optionDescription;
	string	optionCommand;
	int32	optionIconSheet;
	int16	optionIconID;
	string	optionConfirmTitle;
};

//Bitmask Values
#define EFFECT_FLAG_STUN 1
#define EFFECT_FLAG_ROOT 2
#define EFFECT_FLAG_MEZ 4
#define EFFECT_FLAG_STIFLE 8
#define EFFECT_FLAG_DAZE 16
#define EFFECT_FLAG_FEAR 32
#define EFFECT_FLAG_SPELLBONUS 64
#define EFFECT_FLAG_SKILLBONUS 128
#define EFFECT_FLAG_STEALTH 256
#define EFFECT_FLAG_INVIS 512
#define EFFECT_FLAG_SNARE 1024
#define EFFECT_FLAG_WATERWALK 2048
#define EFFECT_FLAG_WATERJUMP 4096
#define EFFECT_FLAG_FLIGHT 8192
#define EFFECT_FLAG_GLIDE 16384
#define EFFECT_FLAG_AOE_IMMUNE 32768
#define EFFECT_FLAG_STUN_IMMUNE 65536
#define EFFECT_FLAG_MEZ_IMMUNE 131072
#define EFFECT_FLAG_DAZE_IMMUNE 262144
#define EFFECT_FLAG_ROOT_IMMUNE 524288
#define EFFECT_FLAG_STIFLE_IMMUNE 1048576
#define EFFECT_FLAG_FEAR_IMMUNE 2097152
#define EFFECT_FLAG_SAFEFALL 4194304

struct LuaSpell{
	Entity*			caster;
	int32			initial_target;
	vector<int32>	targets;
	Spell*			spell;
	lua_State*		state;
	string			file_name;
	Timer			timer;
	int16			num_calls;
	int16           num_triggers;
	int8            slot_pos;
	int32           damage_remaining;
	bool			resisted;
	bool			interrupted;
	bool            crit;
	bool            last_spellattack_hit;
	bool            cancel_after_all_triggers;
	bool            had_triggers;
	bool            had_dmg_remaining;
	bool			was_cured;
	Mutex           MSpellTargets;
	int32           effect_bitmask;

};

class LUAUserData{
public:
	LUAUserData();
	virtual ~LUAUserData(){};
	virtual bool IsCorrectlyInitialized();
	virtual bool IsConversationOption();
	virtual bool IsOptionWindow();
	virtual bool IsSpawn();
	virtual bool IsQuest();
	virtual bool IsZone();
	virtual bool IsItem();
	virtual bool IsSkill();
	bool correctly_initialized;
	Item* item;
	ZoneServer* zone;
	Spawn* spawn;
	vector<ConversationOption>* conversation_options;
	vector<OptionWindowOption>* option_window_option;
	Quest* quest;
	Skill* skill;
};

class LUAConversationOptionWrapper : public LUAUserData{
public:
	LUAConversationOptionWrapper();
	bool IsConversationOption();
};

class LUAOptionWindowWrapper : public LUAUserData {
public:
	LUAOptionWindowWrapper();
	bool IsOptionWindow();
};

class LUASpawnWrapper : public LUAUserData{
public:
	LUASpawnWrapper();
	bool IsSpawn();
};

class LUAZoneWrapper : public LUAUserData{
public:
	LUAZoneWrapper();
	bool IsZone();
};

class LUAQuestWrapper : public LUAUserData{
public:
	LUAQuestWrapper();
	bool IsQuest();
};

class LUAItemWrapper : public LUAUserData{
public:
	LUAItemWrapper();
	bool IsItem();
};

class LUASkillWrapper: public LUAUserData {
public:
	LUASkillWrapper();
	bool IsSkill();
};

class LuaInterface {
public:
	LuaInterface();
	~LuaInterface();
	bool			LoadLuaSpell(const char* name);
	bool			LoadLuaSpell(string name);
	bool			LoadItemScript(string name);
	bool			LoadItemScript(const char* name);
	bool			LoadSpawnScript(string name);
	bool			LoadSpawnScript(const char* name);
	bool			LoadZoneScript(string name);
	bool			LoadZoneScript(const char* name);
	void			RemoveSpell(LuaSpell* spell, Spawn* spawn, bool call_remove_function = true, bool can_delete = true);
	Spawn*			GetSpawn(lua_State* state, int8 arg_num = 1);
	Item*			GetItem(lua_State* state, int8 arg_num = 1);
	Quest*			GetQuest(lua_State* state, int8 arg_num = 1);
	ZoneServer*		GetZone(lua_State* state, int8 arg_num = 1);
	Skill*			GetSkill(lua_State* state, int8 arg_num = 1);
	vector<ConversationOption>*	GetConversation(lua_State* state, int8 arg_num = 1);
	vector<OptionWindowOption>* GetOptionWindow(lua_State* state, int8 arg_num = 1);
	int8			GetInt8Value(lua_State* state, int8 arg_num = 1);
	int16			GetInt16Value(lua_State* state, int8 arg_num = 1);
	int32			GetInt32Value(lua_State* state, int8 arg_num = 1);
	sint32			GetSInt32Value(lua_State* state, int8 arg_num = 1);
	float			GetFloatValue(lua_State* state, int8 arg_num = 1);
	string			GetStringValue(lua_State* state, int8 arg_num = 1);
	bool			GetBooleanValue(lua_State*state, int8 arg_num = 1);

	void			Process();

	void			SetInt32Value(lua_State* state, int32 value);
	void			SetSInt32Value(lua_State* state, sint32 value);
	void			SetFloatValue(lua_State* state, float value);
	void			SetBooleanValue(lua_State* state, bool value);
	void			SetStringValue(lua_State* state, const char* value);
	void			SetSpawnValue(lua_State* state, Spawn* spawn);
	void			SetSkillValue(lua_State* state, Skill* skill);
	void			SetItemValue(lua_State* state, Item* item);
	void			SetQuestValue(lua_State* state, Quest* quest);
	void			SetZoneValue(lua_State* state, ZoneServer* zone);
	void			SetConversationValue(lua_State* state, vector<ConversationOption>* conversation);
	void			SetOptionWindowValue(lua_State* state, vector<OptionWindowOption>* optionWindow);

	void			AddSpawnPointers(LuaSpell* spell, bool first_cast, bool precast = false, const char* function = 0, SpellScriptTimer* timer = 0);
	LuaSpell*		GetCurrentSpell(lua_State* state);
	void			SetCurrentSpell(lua_State* state, LuaSpell* spell);
	bool			CallSpellProcess(LuaSpell* spell, int8 num_parameters);
	LuaSpell*		GetSpell(const char* name);
	void			UseItemScript(const char* name, lua_State* state, bool val);
	void			UseSpawnScript(const char* name, lua_State* state, bool val);
	void			UseZoneScript(const char* name, lua_State* state, bool val);
	lua_State*		GetItemScript(const char* name, bool create_new = true, bool use = false);
	lua_State*		GetSpawnScript(const char* name, bool create_new = true, bool use = false);
	lua_State*		GetZoneScript(const char* name, bool create_new = true, bool use = false);
	Quest*			LoadQuest(int32 id, const char* name, const char* type, const char* zone, int8 level, const char* description, char* script_name);

	void			RemoveSpawnScript(const char* name);
	bool			RunItemScript(string script_name, const char* function_name, Item* item, Spawn* spawn = 0);
	bool			CallItemScript(lua_State* state, int8 num_parameters);
	bool			RunSpawnScript(string script_name, const char* function_name, Spawn* npc, Spawn* spawn = 0, const char* message = 0);
	bool			CallSpawnScript(lua_State* state, int8 num_parameters);
	bool			RunZoneScript(string script_name, const char* function_name, ZoneServer* zone, Spawn* spawn = 0, int32 grid_id = 0);
	bool			CallZoneScript(lua_State* state, int8 num_parameters);
	void			ResetFunctionStack(lua_State* state);
	void			DestroySpells();
	void			DestroySpawnScripts();
	void			DestroyItemScripts();
	void			ReloadSpells();
	void			DestroyQuests(bool reload = false);
	void			DestroyZoneScripts();
	void			SimpleLogError(const char* error);
	void			LogError(const char* error, ...);


	void			CallQuestFunction(Quest* quest, const char* function, Spawn* player, int32 step_id = 0xFFFFFFFF);
	void			RemoveDebugClients(Client* client);
	void			UpdateDebugClients(Client* client);
	void			ProcessErrorMessage(const char* message);
	map<Client*, int32> GetDebugClients(){ return debug_clients; }
	void			AddUserDataPtr(LUAUserData* data);
	void			DeleteUserDataPtrs(bool all);
	void			DeletePendingSpells(bool all);
	Mutex*			GetSpawnScriptMutex(const char* name);
	Mutex*			GetItemScriptMutex(const char* name);
	Mutex*			GetZoneScriptMutex(const char* name);
	Mutex*			GetQuestMutex(Quest* quest);

	void			SetSpawnScriptsReloading(bool val) { spawn_scripts_reloading = val; }

	void			AddPendingSpellDelete(LuaSpell* spell);
private:
	bool			shutting_down;
	bool			spawn_scripts_reloading;
	map<LuaSpell*, int32> spells_pending_delete;
	Timer*			user_data_timer;
	Timer*			spell_delete_timer;
	map<LUAUserData*, int32> user_data;
	map<Client*, int32>	debug_clients;
	map<lua_State*, LuaSpell*> current_spells;
	vector<string>*	GetDirectoryListing(const char* directory);
	lua_State*		LoadLuaFile(const char* name);
	void			RegisterFunctions(lua_State* state);
	map<string, LuaSpell*> spells;
	map<int32, Quest*>		quests;
	map<int32, lua_State*> quest_states;
	map<string, map<lua_State*, bool> > item_scripts;
	map<string, map<lua_State*, bool> > spawn_scripts;
	map<string, map<lua_State*, bool> > zone_scripts;
	map<string, Mutex*> item_scripts_mutex;
	map<string, Mutex*> spawn_scripts_mutex;
	map<string, Mutex*> zone_scripts_mutex;
	map<int32, Mutex*> quests_mutex;	
	Mutex			MDebugClients;
	Mutex			MSpells;
	Mutex			MSpawnScripts;
	Mutex			MItemScripts;
	Mutex			MZoneScripts;
	Mutex			MQuests;
	Mutex			MLUAUserData;
	Mutex			MLUAMain;
	Mutex			MSpellDelete;
};
#endif
