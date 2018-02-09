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
#include <sys/types.h>
#include "Commands.h"
#include "../ClientPacketFunctions.h"
#include "../../common/version.h"
#include "../../common/seperator.h"
#include "../../common/servertalk.h"
#include "../WorldDatabase.h"
#include "../World.h"
#include "../../common/ConfigReader.h"
#include "../VisualStates.h"
#include "../../common/debug.h"
#include "../LuaInterface.h"
#include "../Quests.h"
#include "../client.h"
#include "../NPC.h"
#include "../Guilds/Guild.h"
#include "../SpellProcess.h"
#include "../Tradeskills/Tradeskills.h"
#include "../../common/Log.h"
#include "../../common/MiscFunctions.h"
#include "../Languages.h"
#include "../IRC/IRC.h"
#include "../Traits/Traits.h"
#include "../Chat/Chat.h"
#include "../Rules/Rules.h"
#include "../AltAdvancement/AltAdvancement.h"
#include "../RaceTypes/RaceTypes.h"
#include "../PVP.h"

extern WorldDatabase database;
extern MasterSpellList master_spell_list;
extern MasterTraitList master_trait_list;
extern MasterRecipeList master_recipe_list;
extern MasterRecipeBookList master_recipebook_list;
extern World world;
extern ClientList client_list;
extern ConfigReader configReader;
extern VisualStates visual_states;
extern ZoneList	zone_list;
extern LuaInterface* lua_interface;
extern MasterQuestList master_quest_list;
extern MasterCollectionList master_collection_list;
extern MasterSkillList master_skill_list;
extern MasterFactionList master_faction_list;
extern GuildList guild_list;
extern MasterLanguagesList master_languages_list;
extern IRC irc;
extern Chat chat;
extern RuleManager rule_manager;
extern MasterAAList master_aa_list;
extern MasterRaceTypeList race_types_list;

EQ2Packet* RemoteCommands::serialize(){
	buffer.clear();
	vector<EQ2_RemoteCommandString>::iterator command_list;
	buffer.append((char*)&num_commands, sizeof(int16));
	for( command_list = commands.begin(); command_list != commands.end(); command_list++ ) {
		AddDataCommand(&(*command_list));
	}
	EQ2Packet* app = new EQ2Packet(OP_SetRemoteCmdsMsg, (uchar*)buffer.c_str(), buffer.length() + 1);
	return app;
}

Commands::Commands(){ 
	remote_commands = new RemoteCommands(); 
	spawn_set_values["list"] = SPAWN_SET_VALUE_LIST;
	spawn_set_values["name"] = SPAWN_SET_VALUE_NAME;
	spawn_set_values["level"] = SPAWN_SET_VALUE_LEVEL;	// TODO: Fix this, min_level, max_level
	spawn_set_values["difficulty"] = SPAWN_SET_VALUE_DIFFICULTY;
	spawn_set_values["model_type"] = SPAWN_SET_VALUE_MODEL_TYPE;
	spawn_set_values["class"] = SPAWN_SET_VALUE_CLASS;
	spawn_set_values["gender"] = SPAWN_SET_VALUE_GENDER;
	spawn_set_values["show_name"] = SPAWN_SET_VALUE_SHOW_NAME;
	spawn_set_values["attackable"] = SPAWN_SET_VALUE_ATTACKABLE;
	spawn_set_values["show_level"] = SPAWN_SET_VALUE_SHOW_LEVEL;
	spawn_set_values["targetable"] = SPAWN_SET_VALUE_TARGETABLE;
	spawn_set_values["show_command_icon"] = SPAWN_SET_VALUE_SHOW_COMMAND_ICON;
	spawn_set_values["display_hand_icon"] = SPAWN_SET_VALUE_HAND_ICON;
	spawn_set_values["hair_type"] = SPAWN_SET_VALUE_HAIR_TYPE;
	spawn_set_values["facial_hair_type"] = SPAWN_SET_VALUE_FACIAL_HAIR_TYPE;
	spawn_set_values["wing_type"] = SPAWN_SET_VALUE_WING_TYPE;
	spawn_set_values["chest_type"] = SPAWN_SET_VALUE_CHEST_TYPE;
	spawn_set_values["legs_type"] = SPAWN_SET_VALUE_LEGS_TYPE;
	spawn_set_values["soga_hair_type"] = SPAWN_SET_VALUE_SOGA_HAIR_TYPE;
	spawn_set_values["soga_facial_hair_type"] = SPAWN_SET_VALUE_SOGA_FACIAL_HAIR_TYPE;
	spawn_set_values["soga_model_type"] = SPAWN_SET_VALUE_SOGA_MODEL_TYPE;
	spawn_set_values["size"] = SPAWN_SET_VALUE_SIZE;
	spawn_set_values["hp"] = SPAWN_SET_VALUE_HP;
	spawn_set_values["power"] = SPAWN_SET_VALUE_POWER;
	spawn_set_values["heroic"] = SPAWN_SET_VALUE_HEROIC;
	spawn_set_values["respawn"] = SPAWN_SET_VALUE_RESPAWN;
	spawn_set_values["x"] = SPAWN_SET_VALUE_X;
	spawn_set_values["y"] = SPAWN_SET_VALUE_Y;
	spawn_set_values["z"] = SPAWN_SET_VALUE_Z;
	spawn_set_values["heading"] = SPAWN_SET_VALUE_HEADING;
	spawn_set_values["location"] = SPAWN_SET_VALUE_LOCATION;
	spawn_set_values["command_primary"] = SPAWN_SET_VALUE_COMMAND_PRIMARY;
	spawn_set_values["command_secondary"] = SPAWN_SET_VALUE_COMMAND_SECONDARY;
	spawn_set_values["visual_state"] = SPAWN_SET_VALUE_VISUAL_STATE;
	spawn_set_values["action_state"] = SPAWN_SET_VALUE_ACTION_STATE;
	spawn_set_values["mood_state"] = SPAWN_SET_VALUE_MOOD_STATE;
	spawn_set_values["initial_state"] = SPAWN_SET_VALUE_INITIAL_STATE;
	spawn_set_values["activity_state"] = SPAWN_SET_VALUE_ACTIVITY_STATE;
	spawn_set_values["collision_radius"] = SPAWN_SET_VALUE_COLLISION_RADIUS;
	spawn_set_values["faction"] = SPAWN_SET_VALUE_FACTION;
	spawn_set_values["spawn_script"] = SPAWN_SET_VALUE_SPAWN_SCRIPT;
	spawn_set_values["spawnentry_script"] = SPAWN_SET_VALUE_SPAWNENTRY_SCRIPT;
	spawn_set_values["spawnlocation_script"] = SPAWN_SET_VALUE_SPAWNLOCATION_SCRIPT;
	spawn_set_values["sub_title"] = SPAWN_SET_VALUE_SUB_TITLE;
	spawn_set_values["expire"] = SPAWN_SET_VALUE_EXPIRE;
	spawn_set_values["expire_offset"] = SPAWN_SET_VALUE_EXPIRE_OFFSET;
	spawn_set_values["x_offset"] = SPAWN_SET_VALUE_X_OFFSET;
	spawn_set_values["y_offset"] = SPAWN_SET_VALUE_Y_OFFSET;
	spawn_set_values["z_offset"] = SPAWN_SET_VALUE_Z_OFFSET;
	spawn_set_values["device_id"] = SPAWN_SET_VALUE_DEVICE_ID;
	spawn_set_values["pitch"] = SPAWN_SET_VALUE_PITCH;
	spawn_set_values["roll"] = SPAWN_SET_VALUE_ROLL;
	spawn_set_values["hide_hood"] = SPAWN_SET_VALUE_HIDE_HOOD;
	spawn_set_values["emote_state"] = SPAWN_SET_VALUE_EMOTE_STATE;
	spawn_set_values["icon"] = SPAWN_SET_VALUE_ICON;
	spawn_set_values["prefix"] = SPAWN_SET_VALUE_PREFIX;
	spawn_set_values["suffix"] = SPAWN_SET_VALUE_SUFFIX;
	spawn_set_values["lastname"] = SPAWN_SET_VALUE_LASTNAME;

	zone_set_values["expansion_id"] = ZONE_SET_VALUE_EXPANSION_ID;
	zone_set_values["name"] = ZONE_SET_VALUE_NAME;
	zone_set_values["file"] = ZONE_SET_VALUE_FILE	;
	zone_set_values["description"] = ZONE_SET_VALUE_DESCRIPTION;
	zone_set_values["safe_x"] = ZONE_SET_VALUE_SAFE_X;
	zone_set_values["safe_y"] = ZONE_SET_VALUE_SAFE_Y;
	zone_set_values["safe_z"] = ZONE_SET_VALUE_SAFE_Z;
	zone_set_values["underworld"] = ZONE_SET_VALUE_UNDERWORLD;
	zone_set_values["min_recommended"] = ZONE_SET_VALUE_MIN_RECOMMENDED;
	zone_set_values["max_recommended"] = ZONE_SET_VALUE_MAX_RECOMMENDED;
	zone_set_values["zone_type"] = ZONE_SET_VALUE_ZONE_TYPE;
	zone_set_values["always_loaded"] = ZONE_SET_VALUE_ALWAYS_LOADED;
	zone_set_values["city_zone"] = ZONE_SET_VALUE_CITY_ZONE;
	zone_set_values["weather_allowed"] = ZONE_SET_VALUE_WEATHER_ALLOWED;
	zone_set_values["min_status"] = ZONE_SET_VALUE_MIN_STATUS;
	zone_set_values["min_level"] = ZONE_SET_VALUE_MIN_LEVEL;
	zone_set_values["start_zone"] = ZONE_SET_VALUE_START_ZONE;
	zone_set_values["instance_type"] = ZONE_SET_VALUE_INSTANCE_TYPE;
	zone_set_values["default_reenter_time"] = ZONE_SET_VALUE_DEFAULT_REENTER_TIME;
	zone_set_values["default_reset_time"] = ZONE_SET_VALUE_DEFAULT_RESET_TIME;
	zone_set_values["default_lockout_time"] = ZONE_SET_VALUE_DEFAULT_LOCKOUT_TIME;
	zone_set_values["force_group_to_zone"] = ZONE_SET_VALUE_FORCE_GROUP_TO_ZONE;
	zone_set_values["lua_script"] = ZONE_SET_VALUE_LUA_SCRIPT;
	zone_set_values["shutdown_timer"] = ZONE_SET_VALUE_SHUTDOWN_TIMER;
	zone_set_values["zone_motd"] = ZONE_SET_VALUE_ZONE_MOTD;
}

Commands::~Commands() { 
	safe_delete(remote_commands); 
}

int32 Commands::GetSpawnSetType(string val){
	if(spawn_set_values.count(val) > 0)
		return spawn_set_values[val];
	return 0xFFFFFFFF;
}

bool Commands::SetSpawnCommand(Client* client, Spawn* target, int8 type, const char* value, bool send_update, bool temporary, string* temp_value){
	if(!target)
		return false;
	int32 val = 0;
	try{
		if(type != SPAWN_SET_VALUE_NAME && !(type >= SPAWN_SET_VALUE_SPAWN_SCRIPT && type <= SPAWN_SET_VALUE_SUB_TITLE) && !(type >= SPAWN_SET_VALUE_PREFIX && type <= SPAWN_SET_VALUE_LASTNAME))
			val = atoul(value);
	}
	catch(...){
		if(client)
			client->Message(CHANNEL_COLOR_RED, "Invalid numeric spawn value: %s", value);
		return false;
	}
	if(temporary && temp_value){
		char tmp[128] = {0};
		switch(type){
			case SPAWN_SET_VALUE_NAME:{
				sprintf(tmp, "%s", target->GetName());
				target->SetName(value);
				client->GetCurrentZone()->SendUpdateTitles(target);
				break;
									  }
			case SPAWN_SET_VALUE_X_OFFSET: 
				{
				sprintf(tmp, "%f", target->GetXOffset());
				target->SetXOffset(float(val));
				break;
				}
			case SPAWN_SET_VALUE_Y_OFFSET: 
				{
				sprintf(tmp, "%f", target->GetYOffset());
				target->SetYOffset(float(val));
				break;
				}
			case SPAWN_SET_VALUE_Z_OFFSET: 
				{
				sprintf(tmp, "%f", target->GetZOffset());
				target->SetZOffset(float(val));
				break;
				}
			case SPAWN_SET_VALUE_EXPIRE: {
				sprintf(tmp, "%u", target->GetExpireTime());
				target->SetExpireTime(val);
				break;
											}
			case SPAWN_SET_VALUE_EXPIRE_OFFSET: {
				sprintf(tmp, "%u", target->GetExpireOffsetTime());
				target->SetExpireOffsetTime(val);
				break;
											}
			case SPAWN_SET_VALUE_SUB_TITLE: {
				sprintf(tmp, "%s", target->GetSubTitle());
				target->SetSubTitle(value);
				client->GetCurrentZone()->SendUpdateTitles(target);
				break;
											}
			case SPAWN_SET_VALUE_LEVEL:{
				sprintf(tmp, "%i", target->GetLevel());
				target->SetLevel(val, send_update);
				break;
									   }
			case SPAWN_SET_VALUE_DIFFICULTY:{
				sprintf(tmp, "%i", target->GetEncounterLevel());
				target->SetEncounterLevel(val, send_update);
				break;
											}
			case SPAWN_SET_VALUE_MODEL_TYPE:{
				sprintf(tmp, "%i", target->GetModelType());
				target->SetModelType(val, send_update);
				break;
											}
			case SPAWN_SET_VALUE_CLASS:{
				sprintf(tmp, "%i", target->GetAdventureClass());
				target->SetAdventureClass(val, send_update);
				break;
									   }
			case SPAWN_SET_VALUE_GENDER:{
				sprintf(tmp, "%i", target->GetGender());
				target->SetGender(val, send_update);
				break;
										}
			case SPAWN_SET_VALUE_SHOW_NAME:{
				sprintf(tmp, "%i", target->GetShowName());
				target->SetShowName(val, send_update);
				break;
										   }
			case SPAWN_SET_VALUE_ATTACKABLE:{
				sprintf(tmp, "%i", target->GetAttackable());
				target->SetAttackable(val, send_update);
				break;
											}
			case SPAWN_SET_VALUE_SHOW_LEVEL:{
				sprintf(tmp, "%i", target->GetShowLevel());
				target->SetShowLevel(val, send_update);
				break;
											}
			case SPAWN_SET_VALUE_TARGETABLE:{
				sprintf(tmp, "%i", target->GetTargetable());
				target->SetTargetable(val, send_update);
				break;
											}
			case SPAWN_SET_VALUE_SHOW_COMMAND_ICON:{
				sprintf(tmp, "%i", target->GetShowCommandIcon());
				target->SetShowCommandIcon(val, send_update);
				break;
												   }
			case SPAWN_SET_VALUE_HAND_ICON:{
				sprintf(tmp, "%i", target->GetShowHandIcon());
				target->SetShowHandIcon(val, send_update);
				break;
										   }
			case SPAWN_SET_VALUE_HAIR_TYPE:{
				if(target->IsEntity()){
					sprintf(tmp, "%i", ((Entity*)target)->GetHairType());
					((Entity*)target)->SetHairType(val, send_update);
				}
				break;
										   }
			case SPAWN_SET_VALUE_FACIAL_HAIR_TYPE:{
				if(target->IsEntity()){
					sprintf(tmp, "%i", ((Entity*)target)->GetHairType());
					((Entity*)target)->SetHairType(val, send_update);
				}
				break;
												  }
			case SPAWN_SET_VALUE_WING_TYPE:{
				if(target->IsEntity()){
					sprintf(tmp, "%i", ((Entity*)target)->GetWingType());
					((Entity*)target)->SetWingType(val, send_update);
				}
				break;
										   }
			case SPAWN_SET_VALUE_CHEST_TYPE:{
				if(target->IsEntity()){
					sprintf(tmp, "%i", ((Entity*)target)->GetChestType());
					((Entity*)target)->SetChestType(val, send_update);
				}
				break;
											}
			case SPAWN_SET_VALUE_LEGS_TYPE:{
				if(target->IsEntity()){
					sprintf(tmp, "%i", ((Entity*)target)->GetLegsType());
					((Entity*)target)->SetLegsType(val, send_update);
				}
				break;
										   }
			case SPAWN_SET_VALUE_SOGA_HAIR_TYPE:{
				if(target->IsEntity()){
					sprintf(tmp, "%i", ((Entity*)target)->GetSogaHairType());
					((Entity*)target)->SetSogaHairType(val, send_update);
				}
				break;
												}
			case SPAWN_SET_VALUE_SOGA_FACIAL_HAIR_TYPE:{
				if(target->IsEntity()){
					sprintf(tmp, "%i", ((Entity*)target)->GetSogaFacialHairType());
					((Entity*)target)->SetSogaFacialHairType(val, send_update);
				}
				break;
													   }
			case SPAWN_SET_VALUE_SOGA_MODEL_TYPE:{
				sprintf(tmp, "%i", target->GetSogaModelType());
				target->SetSogaModelType(val, send_update);
				break;
												 }
			case SPAWN_SET_VALUE_SIZE:{
				sprintf(tmp, "%i", target->GetSize());
				target->SetSize(val, send_update);
				break;
									  }
			case SPAWN_SET_VALUE_HP:{
				sprintf(tmp, "%i", target->GetHP());
				target->SetTotalHP(val);
				break;
									}
			case SPAWN_SET_VALUE_POWER:{
				sprintf(tmp, "%i", target->GetPower());
				target->SetTotalPower(val);
				break;
									   }
			case SPAWN_SET_VALUE_HEROIC:{
				sprintf(tmp, "%i", target->GetHeroic());
				target->SetHeroic(val, send_update);
				break;
										}
			case SPAWN_SET_VALUE_RESPAWN:{
				sprintf(tmp, "%u", target->GetRespawnTime());
				target->SetRespawnTime(val);
				break;
										 }
			case SPAWN_SET_VALUE_X:{
				sprintf(tmp, "%f", target->GetX());
				target->SetX(atof(value), send_update);
				break;
								   }
			case SPAWN_SET_VALUE_Y:{
				sprintf(tmp, "%f", target->GetY());
				target->SetY(atof(value), send_update);
				break;
								   }
			case SPAWN_SET_VALUE_Z:{
				sprintf(tmp, "%f", target->GetZ());
				target->SetZ(atof(value), send_update);
				break;
								   }
			case SPAWN_SET_VALUE_HEADING:{
				sprintf(tmp, "%f", target->GetHeading());
				target->SetHeading(atof(value) + 360, send_update);
				break;
										 }
			case SPAWN_SET_VALUE_VISUAL_STATE:{
				sprintf(tmp, "%i", target->GetVisualState());
				target->SetVisualState(val, send_update);
				break;
											  }
			case SPAWN_SET_VALUE_ACTION_STATE:{
				sprintf(tmp, "%i", target->GetActionState());
				target->SetActionState(val, send_update);
				break;
											  }
			case SPAWN_SET_VALUE_MOOD_STATE:{
				sprintf(tmp, "%i", target->GetMoodState());
				target->SetMoodState(val, send_update);
				break;
											}
			case SPAWN_SET_VALUE_INITIAL_STATE:{
				sprintf(tmp, "%i", target->GetInitialState());
				target->SetInitialState(val, send_update);
				break;
											   }
			case SPAWN_SET_VALUE_ACTIVITY_STATE:{
				sprintf(tmp, "%i", target->GetActivityStatus());
				target->SetActivityStatus(val, send_update);
				break;
												}
			case SPAWN_SET_VALUE_COLLISION_RADIUS:{
				sprintf(tmp, "%i", target->GetCollisionRadius());
				target->SetCollisionRadius(val, send_update);
				break;
												  }
			case SPAWN_SET_VALUE_DEVICE_ID: {
				if (target->IsObject()) {
					sprintf(tmp, "%i", ((Object*)target)->GetDeviceID());
					((Object*)target)->SetDeviceID(val);
				}
				break;
											}
			case SPAWN_SET_VALUE_PITCH: {
				sprintf(tmp, "%f", target->GetPitch());
				target->SetPitch(atof(value), send_update);
				break;
										}
			case SPAWN_SET_VALUE_ROLL: {
				sprintf(tmp, "%f", target->GetRoll());
				target->SetRoll(atof(value), send_update);
				break;
									   }
			case SPAWN_SET_VALUE_HIDE_HOOD: {
				sprintf(tmp, "%i", target->appearance.hide_hood);
				target->SetHideHood(val);
				break;
											}
			case SPAWN_SET_VALUE_EMOTE_STATE: {
				sprintf(tmp, "%i", target->appearance.emote_state);
				target->SetEmoteState(val);
				break;
											  }
			case SPAWN_SET_VALUE_ICON: {
			    sprintf(tmp, "%i", target->GetIconValue());
			    target->SetIcon(val);
			    break;
			}

			case SPAWN_SET_VALUE_PREFIX: {
				sprintf(tmp, "%s", target->GetPrefixTitle());
				target->SetPrefixTitle(value);
				client->GetCurrentZone()->SendUpdateTitles(target);
				break;
			}

			case SPAWN_SET_VALUE_SUFFIX: {
				sprintf(tmp, "%s", target->GetSuffixTitle());
				target->SetSuffixTitle(value);
				client->GetCurrentZone()->SendUpdateTitles(target);
				break;
			}

			case SPAWN_SET_VALUE_LASTNAME: {
				 sprintf(tmp, "%s", target->GetLastName());
				 target->SetLastName(value);
				 client->GetCurrentZone()->SendUpdateTitles(target);
				 break;
			}

			*temp_value = string(tmp);
		}
	}
	else{
		switch(type){
			case SPAWN_SET_VALUE_NAME:{
				target->SetName(value);
				client->GetCurrentZone()->SendUpdateTitles(target);
				break;
									  }
			case SPAWN_SET_VALUE_SUB_TITLE: {
				target->SetSubTitle(value);
				client->GetCurrentZone()->SendUpdateTitles(target);
				break;
											}
			case SPAWN_SET_VALUE_X_OFFSET: 
				{
				target->SetXOffset(atof(value));
				break;
				}
			case SPAWN_SET_VALUE_Y_OFFSET: 
				{
				target->SetYOffset(atof(value));
				break;
				}
			case SPAWN_SET_VALUE_Z_OFFSET: 
				{
				target->SetZOffset(atof(value));
				break;
				}
			case SPAWN_SET_VALUE_EXPIRE: {
				target->SetExpireTime(val);
				break;
											}
			case SPAWN_SET_VALUE_EXPIRE_OFFSET: {
				target->SetExpireOffsetTime(val);
				break;
											}
			case SPAWN_SET_VALUE_LEVEL:{
				target->SetLevel(val, send_update);
				break;
									   }
			case SPAWN_SET_VALUE_DIFFICULTY:{
				target->SetEncounterLevel(val, send_update);
				break;
											}
			case SPAWN_SET_VALUE_MODEL_TYPE:{
				target->SetModelType(val, send_update);
				break;
											}
			case SPAWN_SET_VALUE_CLASS:{
				target->SetAdventureClass(val, send_update);
				break;
									   }
			case SPAWN_SET_VALUE_GENDER:{
				target->SetGender(val, send_update);
				break;
										}
			case SPAWN_SET_VALUE_SHOW_NAME:{
				target->SetShowName(val, send_update);
				break;
										   }
			case SPAWN_SET_VALUE_ATTACKABLE:{
				target->SetAttackable(val, send_update);
				break;
											}
			case SPAWN_SET_VALUE_SHOW_LEVEL:{
				target->SetShowLevel(val, send_update);
				break;
											}
			case SPAWN_SET_VALUE_TARGETABLE:{
				target->SetTargetable(val, send_update);
				break;
											}
			case SPAWN_SET_VALUE_SHOW_COMMAND_ICON:{
				target->SetShowCommandIcon(val, send_update);
				break;
												   }
			case SPAWN_SET_VALUE_HAND_ICON:{
				target->SetShowHandIcon(val, send_update);
				break;
										   }
			case SPAWN_SET_VALUE_HAIR_TYPE:{
				if(target->IsEntity())
					((Entity*)target)->SetHairType(val, send_update);
				break;
										   }
			case SPAWN_SET_VALUE_FACIAL_HAIR_TYPE:{
				if(target->IsEntity())
					((Entity*)target)->SetHairType(val, send_update);
				break;
												  }
			case SPAWN_SET_VALUE_WING_TYPE:{
				if(target->IsEntity())
					((Entity*)target)->SetWingType(val, send_update);
				break;
										   }
			case SPAWN_SET_VALUE_CHEST_TYPE:{
				if(target->IsEntity())
					((Entity*)target)->SetChestType(val, send_update);
				break;
											}
			case SPAWN_SET_VALUE_LEGS_TYPE:{
				if(target->IsEntity())
					((Entity*)target)->SetLegsType(val, send_update);
				break;
										   }
			case SPAWN_SET_VALUE_SOGA_HAIR_TYPE:{
				if(target->IsEntity())
					((Entity*)target)->SetSogaHairType(val, send_update);
				break;
												}
			case SPAWN_SET_VALUE_SOGA_FACIAL_HAIR_TYPE:{
				if(target->IsEntity())
					((Entity*)target)->SetSogaFacialHairType(val, send_update);
				break;
													   }
			case SPAWN_SET_VALUE_SOGA_MODEL_TYPE:{
				target->SetSogaModelType(val, send_update);
				break;
												 }
			case SPAWN_SET_VALUE_SIZE:{
				target->SetSize(val, send_update);
				break;
									  }
			case SPAWN_SET_VALUE_HP:{
				target->SetTotalHP(val);
				break;
									}
			case SPAWN_SET_VALUE_POWER:{
				target->SetTotalPower(val);
				break;
									   }
			case SPAWN_SET_VALUE_HEROIC:{
				target->SetHeroic(val, send_update);
				break;
										}
			case SPAWN_SET_VALUE_RESPAWN:{
				target->SetRespawnTime(val);
				break;
										 }
			case SPAWN_SET_VALUE_X:{
				target->SetX(atof(value), send_update);
				target->SetSpawnOrigX(target->GetX());
				break;
								   }
			case SPAWN_SET_VALUE_Y:{
				target->SetY(atof(value), send_update);
				target->SetSpawnOrigY(target->GetY());
				break;
								   }
			case SPAWN_SET_VALUE_Z:{
				target->SetZ(atof(value), send_update);
				target->SetSpawnOrigZ(target->GetZ());
				break;
								   }
			case SPAWN_SET_VALUE_HEADING:{
				target->SetHeading(atof(value), send_update);
				target->SetSpawnOrigHeading(target->GetHeading());
				break;
										 }
			case SPAWN_SET_VALUE_COMMAND_PRIMARY:{
				ZoneServer* zone = target->GetZone();
				if (!zone)
					zone = client->GetCurrentZone();

				if(zone->GetEntityCommandList(val))
					target->SetPrimaryCommands(zone->GetEntityCommandList(val));
				target->primary_command_list_id = val;
				break;
												 }
			case SPAWN_SET_VALUE_COMMAND_SECONDARY:{
				ZoneServer* zone = target->GetZone();
				if (!zone)
					zone = client->GetCurrentZone();

				if (zone->GetEntityCommandList(val))
					target->SetSecondaryCommands(zone->GetEntityCommandList(val));
				target->secondary_command_list_id = val;
				break;
												   }
			case SPAWN_SET_VALUE_VISUAL_STATE:{
				target->SetVisualState(val, send_update);
				break;
											  }
			case SPAWN_SET_VALUE_ACTION_STATE:{
				target->SetActionState(val, send_update);
				break;
											  }
			case SPAWN_SET_VALUE_MOOD_STATE:{
				target->SetMoodState(val, send_update);
				break;
											}
			case SPAWN_SET_VALUE_INITIAL_STATE:{
				target->SetInitialState(val, send_update);
				break;
											   }
			case SPAWN_SET_VALUE_ACTIVITY_STATE:{
				target->SetActivityStatus(val, send_update);
				break;
												}
			case SPAWN_SET_VALUE_COLLISION_RADIUS:{
				target->SetCollisionRadius(val, send_update);
				break;
												  }
			case SPAWN_SET_VALUE_FACTION:{
				ZoneServer* zone = target->GetZone();
				if (!zone)
					zone = client->GetCurrentZone();

				target->faction_id = val;
				zone->RemoveDeadEnemyList(target);
				if(target->IsNPC())
					zone->AddEnemyList((NPC*)target);
				break;
										 }
			case SPAWN_SET_VALUE_DEVICE_ID:{
				if (target->IsObject())
					((Object*)target)->SetDeviceID(val);
				break;
										   }
			case SPAWN_SET_VALUE_PITCH: {
				target->SetPitch(atof(value), send_update);
				target->SetSpawnOrigPitch(atof(value));
				break;
										}
			case SPAWN_SET_VALUE_ROLL: {
				target->SetRoll(atof(value), send_update);
				target->SetSpawnOrigRoll(atof(value));
				break;
									   }
			case SPAWN_SET_VALUE_HIDE_HOOD: {
				target->SetHideHood(val);
				break;
											}
			case SPAWN_SET_VALUE_EMOTE_STATE: {
				target->SetEmoteState(val);
				break;
											  }
			case SPAWN_SET_VALUE_ICON: {
				target->SetIcon(val);
				break;
									   }
			case SPAWN_SET_VALUE_PREFIX: {
				target->SetPrefixTitle(value);
				client->GetCurrentZone()->SendUpdateTitles(target);
				break;
										 }
			case SPAWN_SET_VALUE_SUFFIX: {
				target->SetSuffixTitle(value);
				client->GetCurrentZone()->SendUpdateTitles(target);
				break;
			}
			case SPAWN_SET_VALUE_LASTNAME: {
				target->SetLastName(value);
				client->GetCurrentZone()->SendUpdateTitles(target);
			    break;
			}
			case SPAWN_SET_VALUE_SPAWN_SCRIPT:{
				if(lua_interface && lua_interface->GetSpawnScript(value) == 0){
					if(client){
						client->Message(CHANNEL_COLOR_RED, "Invalid Spawn Script file.  Be sure you give the absolute path.");
						client->Message(CHANNEL_COLOR_RED, "Example: /spawn set spawn_script 'SpawnScripts/example.lua'");
					}
					return false;
				}
				else if(!database.UpdateSpawnScriptData(target->GetDatabaseID(), 0, 0, value))
					return false;
				else{
					if(!world.GetSpawnLocationScript(target->GetSpawnLocationID()))
						target->SetSpawnScript(value);
				}
				break;
											  }
			case SPAWN_SET_VALUE_SPAWNLOCATION_SCRIPT:{
				if(lua_interface && lua_interface->GetSpawnScript(value) == 0){
					if(client){
						client->Message(CHANNEL_COLOR_RED, "Invalid Spawn Script file.  Be sure you give the absolute path.");
						client->Message(CHANNEL_COLOR_RED, "Example: /spawn set spawnlocation_script 'SpawnScripts/example.lua'");
					}
					return false;
				}
				else if(!database.UpdateSpawnScriptData(0, target->GetSpawnLocationID(), 0, value))
					return false;
				else{
					if(!world.GetSpawnEntryScript(target->GetSpawnEntryID()))
						target->SetSpawnScript(value);
				}
				break;
												   }
			case SPAWN_SET_VALUE_SPAWNENTRY_SCRIPT:{
				if(lua_interface && lua_interface->GetSpawnScript(value) == 0){
					if(client){
						client->Message(CHANNEL_COLOR_RED, "Invalid Spawn Script file.  Be sure you give the absolute path.");
						client->Message(CHANNEL_COLOR_RED, "Example: /spawn set spawnentry_script 'SpawnScripts/example.lua'");
					}
					return false;
				}
				else if(!database.UpdateSpawnScriptData(0, 0, target->GetSpawnEntryID(), value))
					return false;
				else
					target->SetSpawnScript(value);
				break;
												   }
		}
	}
	return true;
}

/* The zone object will be NULL if the zone is not currently running.  We pass both of these in so we can update 
   the database fields always and also update the zone in memory if it's running. */
bool Commands::SetZoneCommand(Client* client, int32 zone_id, ZoneServer* zone, int8 type, const char* value) {
	if (client && zone_id > 0 && type > 0 && value) {
		sint32 int_value = 0;
		float float_value = 0;
		if (type == ZONE_SET_VALUE_SAFE_X || type == ZONE_SET_VALUE_SAFE_Y || type == ZONE_SET_VALUE_SAFE_Z || type == ZONE_SET_VALUE_UNDERWORLD) {
			try {
				float_value = atof(value);
			}
			catch (...) {
				client->Message(CHANNEL_COLOR_RED, "Error converting '%s' to a float value", value);
				return false;
			}
		}
		else if (type != ZONE_SET_VALUE_NAME && type != ZONE_SET_VALUE_FILE && type != ZONE_SET_VALUE_DESCRIPTION && type != ZONE_SET_VALUE_ZONE_TYPE && type != ZONE_SET_VALUE_LUA_SCRIPT && type != ZONE_SET_VALUE_ZONE_MOTD) {
			try {
				int_value = atoi(value);
			}
			catch (...) {
				client->Message(CHANNEL_COLOR_RED, "Error converting '%s' to an integer value", value);
				return false;
			}
		}
		switch (type) {
			case ZONE_SET_VALUE_EXPANSION_ID: {
				break;
			}
			case ZONE_SET_VALUE_NAME: {
				if (zone)
					zone->SetZoneName(const_cast<char*>(value));
				database.SaveZoneInfo(zone_id, "name", value);
				break;
			}
			case ZONE_SET_VALUE_FILE: {
				if (zone)
					zone->SetZoneFile(const_cast<char*>(value));
				database.SaveZoneInfo(zone_id, "file", value);
				break;
			}
			case ZONE_SET_VALUE_DESCRIPTION: {
				if (zone)
					zone->SetZoneDescription(const_cast<char*>(value));
				database.SaveZoneInfo(zone_id, "description", value);
				break;
			}
			case ZONE_SET_VALUE_SAFE_X: {
				if (zone)
					zone->SetSafeX(float_value);
				database.SaveZoneInfo(zone_id, "safe_x", float_value);
				break;
			}
			case ZONE_SET_VALUE_SAFE_Y: {
				if (zone)
					zone->SetSafeY(float_value);
				database.SaveZoneInfo(zone_id, "safe_y", float_value);
				break;
			}
			case ZONE_SET_VALUE_SAFE_Z: {
				if (zone)
					zone->SetSafeZ(float_value);
				database.SaveZoneInfo(zone_id, "safe_z", float_value);
				break;
			}
			case ZONE_SET_VALUE_UNDERWORLD: {
				if (zone)
					zone->SetUnderWorld(float_value);
				database.SaveZoneInfo(zone_id, "underworld", float_value);
				break;
			}
			case ZONE_SET_VALUE_MIN_RECOMMENDED: {
				break;
			}
			case ZONE_SET_VALUE_MAX_RECOMMENDED: {
				break;
			}
			case ZONE_SET_VALUE_ZONE_TYPE: {
				break;
			}
			case ZONE_SET_VALUE_ALWAYS_LOADED: {
				if (zone)
					zone->SetAlwaysLoaded(int_value == 1);
				database.SaveZoneInfo(zone_id, "always_loaded", int_value);
				break;
			}
			case ZONE_SET_VALUE_CITY_ZONE: {
				if (zone)
					zone->SetCityZone(int_value == 1);
				database.SaveZoneInfo(zone_id, "city_zone", int_value);
				break;
			}
			case ZONE_SET_VALUE_WEATHER_ALLOWED: {
				if (zone)
					zone->SetWeatherAllowed(int_value == 1);
				database.SaveZoneInfo(zone_id, "weather_allowed", int_value);
				break;
			}
			case ZONE_SET_VALUE_MIN_STATUS: {
				if (zone)
					zone->SetMinimumStatus(int_value);
				database.SaveZoneInfo(zone_id, "min_status", int_value);
				break;
			}
			case ZONE_SET_VALUE_MIN_LEVEL: {
				if (zone)
					zone->SetMinimumLevel(int_value);
				database.SaveZoneInfo(zone_id, "min_level", int_value);
				break;
			}
			case ZONE_SET_VALUE_MAX_LEVEL: {
				if (zone)
					zone->SetMaximumLevel(int_value);
				database.SaveZoneInfo(zone_id, "max_level", int_value);
				break;
			}
			case ZONE_SET_VALUE_START_ZONE: {
				database.SaveZoneInfo(zone_id, "start_zone", int_value);
				break;
			}
			case ZONE_SET_VALUE_INSTANCE_TYPE: {
				if (zone)
					zone->SetInstanceType(int_value);
				database.SaveZoneInfo(zone_id, "instance_type", int_value);
				break;
			}
			case ZONE_SET_VALUE_DEFAULT_REENTER_TIME: {
				if (zone)
					zone->SetDefaultReenterTime(int_value);
				database.SaveZoneInfo(zone_id, "default_reenter_time", int_value);
				break;
			}
			case ZONE_SET_VALUE_DEFAULT_RESET_TIME: {
				if (zone)
					zone->SetDefaultResetTime(int_value);
				database.SaveZoneInfo(zone_id, "default_reset_time", int_value);
				break;
			}
			case ZONE_SET_VALUE_DEFAULT_LOCKOUT_TIME: {
				if (zone)
					zone->SetDefaultLockoutTime(int_value);
				database.SaveZoneInfo(zone_id, "default_lockout_time", int_value);
				break;
			}
			case ZONE_SET_VALUE_FORCE_GROUP_TO_ZONE: {
				if (zone)
					zone->SetForceGroupZoneOption(int_value);
				database.SaveZoneInfo(zone_id, "force_group_to_zone", int_value);
				break;
			}
			case ZONE_SET_VALUE_LUA_SCRIPT: {
				if (lua_interface && lua_interface->GetZoneScript(value) == 0) {
					client->Message(CHANNEL_COLOR_RED, "Invalid Zone Script file.  Be sure you give the absolute path.");
					client->Message(CHANNEL_COLOR_RED, "Example: /zone set lua_script 'ZoneScripts/QueensColony.lua'");
					return false;
				}
				else {
					world.AddZoneScript(zone_id, const_cast<char*>(value));
					database.SaveZoneInfo(zone_id, "lua_script", value);
				}
				break;
			}
			case ZONE_SET_VALUE_SHUTDOWN_TIMER: {
				if (zone)
					zone->SetShutdownTimer(int_value);
				database.SaveZoneInfo(zone_id, "shutdown_timer", int_value);
				break;
			}
			case ZONE_SET_VALUE_ZONE_MOTD: {
				if (zone)
					zone->SetZoneMOTD(string(value));
				database.SaveZoneInfo(zone_id, "zone_motd", value);
				break;
			}
			default: {
				client->Message(CHANNEL_COLOR_RED, "Invalid zone attribute %i", type);
				return false;
			}
		}
	}
	else {
		if (client)
			client->SimpleMessage(CHANNEL_COLOR_RED, "An error occured saving new zone data.");
		return false;
	}
	return true;
}

void Commands::Process(int32 index, EQ2_16BitString* command_parms, Client* client){
	if(index>=remote_commands->commands.size()){
		LogWrite(COMMAND__ERROR, 0, "Command", "Error, command handler of %u was requested, but max handler is %u", index, remote_commands->commands.size());
		return;
	}
	EQ2_RemoteCommandString* parent_command = 0;
	EQ2_RemoteCommandString* command = &remote_commands->commands[index];
	Seperator* sep = 0;
	if(command_parms->size > 0){
		sep = new Seperator(command_parms->data.c_str(), ' ', 10, 500, true);
		if(sep && sep->arg[0] && remote_commands->validSubCommand(command->command.data, string(sep->arg[0]))){
			parent_command = command;
			command = &(remote_commands->subcommands[command->command.data][string(sep->arg[0])]);
			safe_delete(sep);
			if(command_parms->data.length() > (command->command.data.length() + 1))
				sep = new Seperator(command_parms->data.c_str() + (command->command.data.length() + 1), ' ', 10, 500, true);
			LogWrite(COMMAND__DEBUG, 1, "Command", "Handler: %u, COMMAND: '%s', SUBCOMMAND: '%s'", index, parent_command->command.data.c_str(), command->command.data.c_str());
		}
		else
			LogWrite(COMMAND__DEBUG, 1, "Command", "Handler: %u, COMMAND: '%s'", index, command->command.data.c_str());
	}

	int ndx = 0;
	if(command->required_status > client->GetAdminStatus())
	{
		LogWrite(COMMAND__ERROR, 0, "Command", "Player '%s' (%u) needs status %i to use command: %s", client->GetPlayer()->GetName(), client->GetAccountID(), command->required_status, command->command.data.c_str());
		safe_delete(sep);
		client->SimpleMessage(3,"Error: Your status is insufficient for this command.");
		return;
	}

	Player* player = client->GetPlayer();
	LogWrite(COMMAND__DEBUG, 0, "Command", "Player '%s' (%u), Command: %s", player->GetName(), client->GetAccountID(), command->command.data.c_str());
	
	switch(command->handler){
		case COMMAND_RELOAD:{
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Reload commands:");
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"/reload structs");
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"/reload items");
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"/reload luasystem");
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"/reload spawnscripts");
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"/reload spells");
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"/reload quests");
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"/reload spawns");
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"/reload groundspawns");
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"/reload zonescripts");
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"/reload entity_commands");
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"/reload factions");
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"/reload mail");
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"/reload guilds");
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"/reload locations");
			break;
							}
		case COMMAND_RELOADSTRUCTS:{
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Reloading Structs...");
			configReader.ReloadStructs();
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Done!");
			break;
								   }
		case COMMAND_RELOAD_QUESTS:{
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Reloading Quests...");
			master_quest_list.Reload();
			client_list.ReloadQuests();
			zone_list.ReloadClientQuests();
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Done!");
			break;
		}
		case COMMAND_RELOAD_SPAWNS:{
			client->GetCurrentZone()->ReloadSpawns();
			break;
								   }
		case COMMAND_RELOAD_SPELLS:{
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Reloading Spells...");
			zone_list.DeleteSpellProcess();
			master_spell_list.Reload();
			if(lua_interface)
				lua_interface->ReloadSpells();
			zone_list.LoadSpellProcess();
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Done!");
			break;
								   }
		case COMMAND_RELOAD_GROUNDSPAWNS:{
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Reloading Groundspawn Entries...");
			client->GetCurrentZone()->DeleteGroundSpawnItems();
			client->GetCurrentZone()->LoadGroundSpawnEntries();
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Done!");
			break;
											 }

		case COMMAND_RELOAD_ZONESCRIPTS:{
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Reloading Zone Scripts...");
			world.ResetZoneScripts();
			database.LoadZoneScriptData();
			if(lua_interface)
				lua_interface->DestroyZoneScripts();
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Done!");
			break;
										}
		case COMMAND_RELOAD_ENTITYCOMMANDS: {
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Reloading Entity Commands...");
			client->GetCurrentZone()->ClearEntityCommands();
			database.LoadEntityCommands(client->GetCurrentZone());
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Done!");
			break;
										   }
		case COMMAND_RELOAD_FACTIONS: {
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Reloading Factions...");
			master_faction_list.Clear();
			database.LoadFactionList();
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Done!");
			break;
									  }
		case COMMAND_RELOAD_MAIL: {
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Reloading Mail...");
			zone_list.ReloadMail();
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Done!");
			break;
								  }
		case COMMAND_RELOAD_GUILDS: {
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Reloading Guilds...");
			world.ReloadGuilds();
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Done!");
			break;
					   }
		case COMMAND_RELOAD_LOCATIONS: {
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Reloading Locations...");
			client->GetPlayer()->GetZone()->RemoveLocationGrids();
			database.LoadLocationGrids(client->GetPlayer()->GetZone());
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Done!");
			break;
									   }
		case COMMAND_USEABILITY:{
			if(sep && sep->arg[0][0] && sep->IsNumber(0)){
				if(client->GetPlayer()->GetHP() == 0){
					client->SimpleMessage(CHANNEL_COLOR_RED,"You cannot do that right now.");
				}
				else{
					int32 spell_id = atoul(sep->arg[0]);
					int8 spell_tier = 0;
					if(sep->arg[1][0] && sep->IsNumber(1))
						spell_tier = atoi(sep->arg[1]);
					else
						spell_tier = client->GetPlayer()->GetSpellTier(spell_id);
					if (!spell_tier) 
						spell_tier = 1;
					Spell* spell = master_spell_list.GetSpell(spell_id, spell_tier);
					if (spell) {
						if (strncmp(spell->GetName(), "Gathering", 9) == 0 || strncmp(spell->GetName(), "Mining", 6) == 0 || strncmp(spell->GetName(), "Trapping", 8) == 0 || strncmp(spell->GetName(), "Foresting", 9) == 0 || strncmp(spell->GetName(), "Fishing", 7) == 0 || strncmp(spell->GetName(), "Collecting", 10) == 0)
							client->GetCurrentZone()->ProcessSpell(spell, client->GetPlayer(), client->GetPlayer()->GetTarget(), true, true);
						else
						{
							if (client->GetPlayer()->HasTarget())
								client->GetCurrentZone()->ProcessSpell(spell, client->GetPlayer(), client->GetPlayer()->GetTarget());
							else
								client->GetCurrentZone()->ProcessSpell(spell, client->GetPlayer(), client->GetPlayer());
						}
					}
				}
			}
			else{
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage:  /useability {spell_id} [spell_tier]");
			}
			break;
								}
		case COMMAND_INFO:{
			if(sep && sep->arg[1][0] && sep->IsNumber(1)){
				if(strcmp(sep->arg[0], "inventory") == 0){
					int32 item_index = atol(sep->arg[1]);
					Item* item = client->GetPlayer()->item_list.GetItemFromIndex(item_index);
					if(item){
						if (item->IsCollectable() && client->SendCollectionsForItem(item))
							break;

						EQ2Packet* app = item->serialize(client->GetVersion(), (!item->GetItemScript() || !lua_interface), client->GetPlayer());
						//DumpPacket(app);
						client->QueuePacket(app);
						if(item->GetItemScript() && lua_interface)
							lua_interface->RunItemScript(item->GetItemScript(), "examined", item, client->GetPlayer());
						else if(item->generic_info.offers_quest_id > 0){ //leave the current functionality in place if it doesnt have an item script
							Quest* quest = master_quest_list.GetQuest(item->generic_info.offers_quest_id);
							if(quest && client->GetPlayer()->GetCompletedQuest(item->generic_info.offers_quest_id) == 0 && client->GetPlayer()->GetQuest(item->generic_info.offers_quest_id) == 0)
								client->AddPendingQuest(quest);
						}
					}
					else
						LogWrite(COMMAND__ERROR, 0, "Command", "Unknown Index: %u", item_index);
				}
				else if(strcmp(sep->arg[0], "equipment") == 0){
					int32 item_index = atol(sep->arg[1]);
					Item* item = client->GetPlayer()->GetEquipmentList()->GetItem(item_index);
					if(item){
						EQ2Packet* app = item->serialize(client->GetVersion(), true, client->GetPlayer());
						client->QueuePacket(app);
						if(item->GetItemScript() && lua_interface)
							lua_interface->RunItemScript(item->GetItemScript(), "examined", item, client->GetPlayer());
					}
					else
						LogWrite(COMMAND__ERROR, 0, "Command", "Unknown Index: %u", item_index);
				}
				else if(strcmp(sep->arg[0], "item") == 0 || strcmp(sep->arg[0], "merchant") == 0 || strcmp(sep->arg[0], "store") == 0 || strcmp(sep->arg[0], "buyback") == 0 || strcmp(sep->arg[0], "consignment") == 0){
					int32 item_id = atoul(sep->arg[1]);
					Item* item = master_item_list.GetItem(item_id);
					if(item){
						EQ2Packet* app = item->serialize(client->GetVersion(), true, client->GetPlayer());
						client->QueuePacket(app);
					}
					else
						LogWrite(COMMAND__ERROR, 0, "Command", "Unknown Item ID: %u", item_id);
				}
				else if (strcmp(sep->arg[0], "spell") == 0) {
					sint32 spell_id = atol(sep->arg[1]);
					int8 tier = atoi(sep->arg[3]);
					EQ2Packet* outapp = master_spell_list.GetSpellPacket(spell_id, tier, client, true, 0x2A);
					if (outapp)
						client->QueuePacket(outapp);
					else
						LogWrite(COMMAND__ERROR, 0, "Command", "Unknown Spell ID and/or Tier, ID: %u, Tier: %i", spell_id, tier);
				}
				else if (strcmp(sep->arg[0], "spellbook") == 0) {
					sint32 spell_id = atol(sep->arg[1]);
					int8 tier = atoi(sep->arg[2]);
					EQ2Packet* outapp = master_spell_list.GetSpellPacket(spell_id, tier, client, true, 0x2A);
					if (outapp)
						client->QueuePacket(outapp);
					else
						LogWrite(COMMAND__ERROR, 0, "Command", "Unknown Spell ID and/or Tier, ID: %u, Tier: %i", spell_id, tier);
				}
				else if (strcmp(sep->arg[0], "recipe") == 0) {
					sint32 recipe_id = atol(sep->arg[1]);
					EQ2Packet* outapp = master_recipe_list.GetRecipePacket(recipe_id, client, true, 0x2C);
					if(outapp)
						client->QueuePacket(outapp);
					else
						LogWrite(COMMAND__ERROR, 0, "Command", "Unknown Recipe ID: %u", recipe_id);
				}
				else if (strcmp(sep->arg[0], "maintained") ==0) {
					int32 slot = atol(sep->arg[1]);
					int32 spell_id = atol(sep->arg[2]);
					LogWrite(COMMAND__DEBUG, 5, "Command", "Unknown Spell ID - Slot: %u unknown: %u", slot, spell_id);
					//int8 tier = client->GetPlayer()->GetSpellTier(spell_id);
					MaintainedEffects* info = client->GetPlayer()->GetMaintainedSpellBySlot(slot);
					EQ2Packet* outapp = master_spell_list.GetSpellPacket(info->spell_id, info->tier, client, true, 0x00);
					if(outapp)
						client->QueuePacket(outapp);
					else
						LogWrite(COMMAND__ERROR, 0, "Command", "Unknown Spell ID: %u", spell_id);
				}
				else if (strcmp(sep->arg[0], "effect") == 0) {
					Entity* target = nullptr;
					int32 spell_id = atol(sep->arg[1]);
					int8 tier = 0;
					
					SpellEffects* spell_effect = client->GetPlayer()->GetSpellEffect(spell_id);
					if (spell_effect)
						tier = spell_effect->tier;

					if (!tier && client->GetPlayer()->GetTarget() && client->GetPlayer()->GetTarget()->IsEntity()) {
						target = static_cast<Entity*>(client->GetPlayer()->GetTarget());
						SpellEffects* spell_effect = target->GetSpellEffect(spell_id);

						if (spell_effect)
							tier = spell_effect->tier;
					}

					if (!tier && target && target->GetTarget() && target->GetTarget()->IsEntity()) {
						Entity* assist = static_cast<Entity*>(target->GetTarget());
						SpellEffects* spell_effect = assist->GetSpellEffect(spell_id);

						if (spell_effect)
							tier = spell_effect->tier;
					}

					EQ2Packet* outapp = master_spell_list.GetSpecialSpellPacket(spell_id, tier, client, true, 0x00);
					if (outapp) {
						client->QueuePacket(outapp);
					} else {
						LogWrite(COMMAND__ERROR, 0, "Command", "Unknown Spell ID: %u", spell_id);
					}
				}
			}
			else if (sep && strcmp(sep->arg[0], "overflow") == 0) {
				Item* item = player->item_list.GetOverflowItem();
				if(item) {
					EQ2Packet* app = item->serialize(client->GetVersion(), true, client->GetPlayer());
					client->QueuePacket(app);
				}
				else
					LogWrite(COMMAND__ERROR, 0,"Command", "Unable to retrieve an overflow item.");
			}
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage:  /info {inventory|equipment|spell} {id}");
			break;
						  }
		case COMMAND_USE_EQUIPPED_ITEM:{
			if (sep && sep->arg[0] && sep->IsNumber(0)){
				int32 slot_id = atoul(sep->arg[0]);
				Item* item = player->GetEquipmentList()->GetItem(slot_id);
				if (item && item->generic_info.usable && item->GetItemScript())
					lua_interface->RunItemScript(item->GetItemScript(), "used", item, player);
			}
			break;
		}
	    case COMMAND_USE_ITEM: {
			if (sep && sep->arg[0] && sep->IsNumber(0)) {
				int32 item_index = atoul(sep->arg[0]);
				Item* item = player->item_list.GetItemFromIndex(item_index);
				if (item && item->GetItemScript())
					lua_interface->RunItemScript(item->GetItemScript(), "used", item, player);
			}
			break;
							   }
		case COMMAND_SCRIBE_SCROLL_ITEM: {
			if (sep && sep->arg[0] && sep->IsNumber(0)) {
				Item* item = player->item_list.GetItemFromUniqueID(atoul(sep->arg[0]));
				if (item) {
					LogWrite(ITEM__DEBUG, 0, "Items", "ITEM ID: %u", item->details.item_id);

					if(item->generic_info.item_type == 6) {
						Spell* spell = master_spell_list.GetSpell(item->skill_info->spell_id, item->skill_info->spell_tier);
						int8 old_slot = 0;
						if (spell) {
							if (!player->HasSpell(spell->GetSpellID(), spell->GetSpellTier(), true)) {
								old_slot = player->GetSpellSlot(spell->GetSpellID());
								player->RemoveSpellBookEntry(spell->GetSpellID());
								player->AddSpellBookEntry(spell->GetSpellID(), spell->GetSpellTier(), old_slot, spell->GetSpellData()->spell_book_type, spell->GetSpellData()->linked_timer, true);
								player->UnlockSpell(spell);
								client->SendSpellUpdate(spell);
								database.DeleteItem(client->GetCharacterID(), item, 0);
								player->item_list.RemoveItem(item, true);
								client->QueuePacket(player->GetSpellBookUpdatePacket(client->GetVersion()));
								client->QueuePacket(player->SendInventoryUpdate(client->GetVersion()));
							}
						}
						else
							LogWrite(COMMAND__ERROR, 0, "Command", "Unknown spell ID: %u and tier: %u", item->skill_info->spell_id, item->skill_info->spell_tier);
					}
					else if(item->generic_info.item_type == 7){
						LogWrite(TRADESKILL__DEBUG, 0, "Recipe", "Scribing recipe book %s (%u) for player %s.", item->name.c_str(), item->details.item_id, player->GetName());
						Recipe* recipe_book = new Recipe(master_recipebook_list.GetRecipeBooks(item->details.item_id));
						// if valid recipe book and the player doesn't have it
						if (recipe_book && recipe_book->GetLevel() > client->GetPlayer()->GetTSLevel()) {
							client->Message(CHANNEL_COLOR_WHITE, "Your tradeskill level is not high enough to scribe this book.");
							safe_delete(recipe_book);
						}
						else if (recipe_book && !(client->GetPlayer()->GetRecipeBookList()->HasRecipeBook(item->details.item_id))) {
							LogWrite(TRADESKILL__DEBUG, 0, "Recipe", "Valid recipe book that the player doesn't have");
							// Add recipe book to the players list
							client->GetPlayer()->GetRecipeBookList()->AddRecipeBook(recipe_book);

							// Get a list of all recipes this book contains
							vector<Recipe*>* book_recipes = master_recipe_list.GetRecipes(recipe_book->GetBookName());
							LogWrite(TRADESKILL__DEBUG, 0, "Recipe", "%i recipes found for %s book", book_recipes->size(), recipe_book->GetBookName());

							// Create the packet to send to update the players recipe list
							PacketStruct* packet = 0;
							if (client->GetRecipeListSent()) {
								packet = configReader.getStruct("WS_RecipeList", client->GetVersion());

								if (packet)
									packet->setArrayLengthByName("num_recipes", book_recipes->size());
							}

							// loop through the list
							vector<Recipe*>::iterator itr;
							int16 i = 0;
							for (itr = book_recipes->begin(); itr != book_recipes->end(); itr++) {
								// check to see if the player already has this recipe some how
								if (!client->GetPlayer()->GetRecipeList()->GetRecipe((*itr)->GetID())) {
									// Player doesn't already have this recipe so lets add it
									Recipe* recipe = new Recipe(master_recipe_list.GetRecipe((*itr)->GetID()));
									client->GetPlayer()->GetRecipeList()->AddRecipe(recipe);
									database.SavePlayerRecipe(client->GetPlayer(), recipe->GetID());
									client->Message(CHANNEL_COLOR_WHITE, "Recipe: \"%s\" put in recipe book.", recipe->GetName());

									if (packet && client->GetRecipeListSent()) {
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
								}
							}
							LogWrite(TRADESKILL__DEBUG, 0, "Recipe", "Done adding recipes");
							database.SavePlayerRecipeBook(client->GetPlayer(), recipe_book->GetBookID());
							database.DeleteItem(client->GetCharacterID(), item, 0);
							client->GetPlayer()->item_list.RemoveItem(item, true);
							client->QueuePacket(client->GetPlayer()->SendInventoryUpdate(client->GetVersion()));
							if (packet && client->GetRecipeListSent())
								client->QueuePacket(packet->serialize());

							safe_delete(packet);
						}
						else {
							if (recipe_book)
								client->Message(CHANNEL_COLOR_WHITE, "You have already learned all you can from this item.");
							safe_delete(recipe_book);
						}
						
					}
				}
				else
					LogWrite(COMMAND__ERROR, 0, "Command", "Unknown unique item ID: %s", sep->arg[0]);
			}
			break;
										 }
		case COMMAND_SUMMONITEM:{
			if(sep && sep->arg[0][0] && sep->IsNumber(0)){
				int32 item_id = atol(sep->arg[0]);
				int32 quantity = 1;

				if (sep->arg[1] && sep->IsNumber(1))
					quantity = atol(sep->arg[1]);

				client->AddItem(item_id, quantity);
			}
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage: /summonitem {item_id} [quantity]");
			break;
		}
		case COMMAND_COLLECTION_ADDITEM: {
			// takes 2 params: collection_id, slot
			if (sep && sep->arg[0] && sep->arg[1] && sep->IsNumber(0) && sep->IsNumber(1)) {
				Item *item = client->GetPlayer()->GetPlayerItemList()->GetItemFromIndex(atoul(sep->arg[1]));
				if (item)
					client->HandleCollectionAddItem(atoul(sep->arg[0]), item);
				else
					LogWrite(COLLECTION__ERROR, 0, "Collect", "Error in 'COMMAND_COLLECTION_ADDITEM'. Unable to get item from player '%s' at index %u", client->GetPlayer()->GetName(), atoul(sep->arg[0]));
			}
			else
				LogWrite(COLLECTION__ERROR, 0, "Collect", "Received command 'COMMAND_COLLECTION_ADDITEM' with unhandeled argument types");
			break;
		}
		case COMMAND_COLLECTION_FILTER_MATCHITEM: {
			// takes 1 param: slot
			printf("COMMAND_COLLECTION_FILTER_MATCHITEM:\n");
			int i = 0;
			while (sep->arg[i] && strlen(sep->arg[i]) > 0) {
				printf("\t%u: %s\n", i, sep->arg[i]);
				i++;
			}
			break;
		}
		case COMMAND_WHO:{
			const char* who = 0;
			if(sep && sep->arg[0]){
				//cout << "Who query: \n";
				who = sep->argplus[0];
				//cout << who << endl;
			}
			zone_list.ProcessWhoQuery(who, client);
			break;
						 }
		case COMMAND_SELECT_JUNCTION:{
			LogWrite(MISC__TODO, 1, "TODO", "For access/travel selections (i.e. Mariner Bells, Player Death Window, etc.)\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
			int32 choice = 0;
			if(sep && sep->arg[0] && sep->IsNumber(0))
				choice = atoul(sep->arg[0]);
			if(client->GetPlayer()->GetHP() == 0){ //dead and this is a revive request
				client->HandlePlayerRevive(choice);
			}
			break;
									 }
		case COMMAND_SPAWN_MOVE:{
			Spawn* target = client->GetPlayer()->GetTarget();
			if(target && target->IsPlayer() == false){
				PacketStruct* packet = configReader.getStruct("WS_MoveObjectMode", client->GetVersion());
				if(packet){
					float unknown2_3 = 0;
					int8 placement_mode = 0;
					if(sep && sep->arg[0][0]){
						if(strcmp(sep->arg[0], "wall") == 0){
							placement_mode = 2;
							unknown2_3 = 150;
						}
						else if(strcmp(sep->arg[0], "ceiling") == 0)
							placement_mode = 1;
					}
					packet->setDataByName("placement_mode", placement_mode);
					packet->setDataByName("spawn_id", client->GetPlayer()->GetIDWithPlayerSpawn(target));
					packet->setDataByName("model_type", target->GetModelType());
					packet->setDataByName("unknown", 1); //size
					packet->setDataByName("unknown2", 1); //size 2
					packet->setDataByName("unknown2", .5, 1); //size 3
					packet->setDataByName("unknown2", 3, 2);
					packet->setDataByName("unknown2", unknown2_3, 3);
					packet->setDataByName("max_distance", 500);
					packet->setDataByName("CoEunknown", 0xFFFFFFFF);
					client->QueuePacket(packet->serialize());
					safe_delete(packet);
				}
			}
			else{
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /spawn move (wall OR ceiling)");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Moves a spawn anywhere you like.  Optionally wall/ceiling can be provided to place wall/ceiling items.");
			}
			break;
								}
		case COMMAND_HAIL:{
			Spawn* spawn = client->GetPlayer()->GetTarget();
			if(spawn && spawn->GetTargetable()){
				char tmp[75] = {0};
				sprintf(tmp, "Hail, %s", spawn->GetName());
				bool show_bubble = true;
				if (spawn->IsNPC())
					show_bubble = false;
				client->GetCurrentZone()->HandleChatMessage(client->GetPlayer(), 0, CHANNEL_SAY, tmp, HEAR_SPAWN_DISTANCE, 0, show_bubble);
				if(spawn->IsPlayer() == false && spawn->GetDistance(client->GetPlayer()) < 30){
					if (spawn->IsNPC() && ((NPC*)spawn)->EngagedInCombat()) {
						spawn->GetZone()->CallSpawnScript(spawn, SPAWN_SCRIPT_HAILED_BUSY, client->GetPlayer());
					} else {
						if (spawn->IsEntity())
							static_cast<Entity*>(spawn)->FaceTarget(client->GetPlayer());

						spawn->GetZone()->CallSpawnScript(spawn, SPAWN_SCRIPT_HAILED, client->GetPlayer());
					}
				}
			}
			else {
				string tmp = "Hail";
				client->GetCurrentZone()->HandleChatMessage(client->GetPlayer(), 0, CHANNEL_SAY, tmp.c_str(), HEAR_SPAWN_DISTANCE, 0, true);
			}
			break;
						  }
		case COMMAND_SAY:{
			if (sep && sep->arg[0][0]) {
				client->GetCurrentZone()->HandleChatMessage(client->GetPlayer(), 0, CHANNEL_SAY, sep->argplus[0], HEAR_SPAWN_DISTANCE);
				if (client->GetPlayer()->GetTarget() && !(client->GetPlayer()->GetTarget()->IsPlayer()))
					client->GetCurrentZone()->CallSpawnScript(client->GetPlayer()->GetTarget(), SPAWN_SCRIPT_HEAR_SAY, client->GetPlayer(), sep->argplus[0]);
			}
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage:  /say {message}");
			break;
						 }
		case COMMAND_TELL:{
			if(sep && sep->arg[0] && sep->argplus[1]){
				if(!zone_list.HandleGlobalChatMessage(client, sep->arg[0], CHANNEL_TELL, sep->argplus[1]))
					client->Message(CHANNEL_COLOR_RED,"Unable to find client %s",sep->arg[0]);
			}else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage:  /tell {character_name} {message}");
			break;
						  }
		case COMMAND_SHOUT:{
			if(sep && sep->arg[0][0])
				client->GetCurrentZone()->HandleChatMessage(client->GetPlayer(), 0, CHANNEL_SHOUT, sep->argplus[0]);
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage:  /shout {message}");
			break;
						   }
		case COMMAND_AUCTION:{
			if(sep && sep->arg[0][0])
				client->GetCurrentZone()->HandleChatMessage(client->GetPlayer(), 0, CHANNEL_AUCTION, sep->argplus[0]);
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage:  /auction {message}");
			break;
							 }
		case COMMAND_OOC:{
			//For now ooc will be the global chat channel, eventually when we create more channels we will create a global chat channel
			if(sep && sep->arg[0][0])
				zone_list.HandleGlobalChatMessage(client, 0, CHANNEL_OOC, sep->argplus[0]);
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage:  /ooc {message}");
			break;
						 }
		case COMMAND_EMOTE:{
			if(sep && sep->arg[0][0])
				client->GetCurrentZone()->HandleChatMessage(client->GetPlayer(), 0, CHANNEL_EMOTE, sep->argplus[0]);
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage:  /emote {action}");
			break;
						   }
		case COMMAND_RACE:{
			if(sep && sep->arg[0][0] && sep->IsNumber(0)){
				if(sep->arg[1][0] && sep->IsNumber(1)){
					client->GetPlayer()->GetInfoStruct()->race = atoi(sep->arg[1]);
					client->GetPlayer()->SetRace(atoi(sep->arg[1]));
					client->UpdateTimeStampFlag ( RACE_UPDATE_FLAG );
					client->GetPlayer()->SetCharSheetChanged(true);
				}
				client->GetPlayer()->SetModelType(atoi(sep->arg[0]));
				//EQ2Packet* outapp = client->GetPlayer()->spawn_update_packet(client->GetPlayer(), client->GetVersion(), client->GetPlayer()->GetFeatures());
				//client->QueuePacket(outapp);
			}
			else{
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /race {race type id} {race id}");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "{race id} is optional");
			}
			break;
						  }
		case COMMAND_SIZE: {
			if (sep && sep->arg[0] && sep->IsNumber(0)) {
				client->GetPlayer()->SetSize(atoi(sep->arg[0]));
				client->GetCurrentZone()->SendPlayerPositionChanges(client->GetPlayer());

				EQ2Packet* outapp = client->GetPlayer()->player_position_update_packet(client->GetPlayer(), client->GetVersion());
				if (outapp)
					client->QueuePacket(outapp);
			} else {
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /size {size}");
			}
			break;
		}
		case COMMAND_VISUAL: {
			if (sep && sep->arg[0] && sep->IsNumber(0)) {
				int32 visual_id = atoi(sep->arg[0]);
				Spawn* target = client->GetPlayer();
				
				if (client->GetPlayer()->HasTarget()) {
					target = client->GetPlayer()->GetTarget();
				}

				client->GetCurrentZone()->SendCastSpellPacket(visual_id, target, client->GetPlayer());
			} else {
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /visual {spell_visual}");
			}
			break;
		}
		case COMMAND_BANK_DEPOSIT:{
			if(client->GetBanker() && sep && sep->arg[0]){
				int64 amount = 0;
				string deposit = string(sep->arg[0]);
				amount = atoi64(deposit.c_str());
				client->BankDeposit(amount);
			}
			break;
								  }
		case COMMAND_BANK_WITHDRAWAL:{
			if(client->GetBanker() && sep && sep->arg[0] && sep->IsNumber(0)){
				int64 amount = 0;
				string deposit = string(sep->arg[0]);
				amount = atoi64(deposit.c_str());
				client->BankWithdrawal(amount);
			}
			break;
									 }
		case COMMAND_BANK_CANCEL:{
			Spawn* banker = client->GetPlayer()->GetTarget();
			client->Bank(banker, true);
			break;
								 }
		case COMMAND_BANK:{
			LogWrite(PLAYER__DEBUG, 0, "Players", "Open Player Personal Bank...");
			Spawn* banker = client->GetPlayer()->GetTarget();
			client->Bank(banker);
			break;
						  }
		case COMMAND_GUILD_BANK:{
			LogWrite(GUILD__DEBUG, 0, "Guilds", "Open Guild Bank...");
			//Spawn* banker = client->GetPlayer()->GetTarget();
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "This will eventually open the guild bank!");
			break;
						  }
		case COMMAND_START_MAIL: {
			client->SetMailTransaction(client->GetPlayer()->GetTarget());
			client->SendMailList();
			break;
								}
		case COMMAND_CANCEL_MAIL: {
			client->SetMailTransaction(0);
			break;
								  }
		case COMMAND_GET_MAIL_MESSAGE: {
			if (sep && sep->arg[0] && sep->IsNumber(0))
				client->DisplayMailMessage(atoul(sep->arg[0]));
			break;
									   }
		case COMMAND_ADD_MAIL_PLAT: {
			if (sep && sep->arg[0] && sep->IsNumber(0))
				client->AddMailCoin(0, 0, 0, atoul(sep->arg[0]));
			break;
									}
		case COMMAND_ADD_MAIL_GOLD: {
			if (sep && sep->arg[0] && sep->IsNumber(0))
				client->AddMailCoin(0, 0, atoul(sep->arg[0]));
			break;
									}
		case COMMAND_ADD_MAIL_SILVER: {
			if (sep && sep->arg[0] && sep->IsNumber(0))
				client->AddMailCoin(0, atoul(sep->arg[0]));
			break;
									  }
		case COMMAND_ADD_MAIL_COPPER: {
			if (sep && sep->arg[0] && sep->IsNumber(0))
				client->AddMailCoin(atoul(sep->arg[0]));
			break;
									  }
		case COMMAND_SET_MAIL_ITEM: {
			LogWrite(MISC__TODO, 1, "TODO", " received set_mail_item\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
			if (sep && sep->arg[0]) LogWrite(COMMAND__DEBUG, 0, "Command", "%s\n", sep->argplus[0]);
			break;
							   }
		case COMMAND_REMOVE_MAIL_PLAT: {
			if (sep && sep->arg[0] && sep->IsNumber(0))
				client->RemoveMailCoin(0, 0, 0, atoul(sep->arg[0]));
			break;
									   }
		case COMMAND_REMOVE_MAIL_GOLD: {
			if (sep && sep->arg[0] && sep->IsNumber(0))
				client->RemoveMailCoin(0, 0, atoul(sep->arg[0]));
			break;
									   }
		case COMMAND_REMOVE_MAIL_SILVER: {
			if (sep && sep->arg[0] && sep->IsNumber(0))
				client->RemoveMailCoin(0, atoul(sep->arg[0]));
			break;
										 }
		case COMMAND_REMOVE_MAIL_COPPER: {
			if (sep && sep->arg[0] && sep->IsNumber(0))
				client->RemoveMailCoin(atoul(sep->arg[0]));
			break;
										 }
		case COMMAND_TAKE_MAIL_ATTACHMENTS: {
			if (sep && sep->arg[0] && sep->IsNumber(0))
				client->TakeMailAttachments(atoul(sep->arg[0]));
			break;
											}
		case COMMAND_CANCEL_SEND_MAIL: {
			client->CancelSendMail();
			break;
									   }
		case COMMAND_DELETE_MAIL_MESSAGE: {
			if (sep && sep->arg[0] && sep->IsNumber(0))
				client->DeleteMail(atoul(sep->arg[0]), true);
			break;
										  }
		case COMMAND_REPORT_SPAM: {
			LogWrite(MISC__TODO, 1, "TODO", " received reportspam\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
			if (sep && sep->arg[0]) LogWrite(COMMAND__DEBUG, 0, "Command", "%s\n", sep->argplus[0]);
			break;
								  }
		case COMMAND_KILL:{
			Spawn* dead = 0;
			if (sep && sep->arg[0] && strncasecmp(sep->arg[0],"self",4)==0){
				dead=client->GetPlayer();
				client->GetPlayer()->SetHP(0);
				client->GetPlayer()->KillSpawn(dead);
			}else{
				dead=client->GetPlayer()->GetTarget();
				if(dead && dead->IsPlayer() == false){
					dead->SetHP(0);
					if(sep && sep->arg[0] && sep->IsNumber(0) && atoi(sep->arg[0]) == 1)
						client->GetCurrentZone()->RemoveSpawn(dead, true);
					else
						client->GetPlayer()->KillSpawn(dead);
				}else{
					client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage:  /kill (self)");
					client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Kills currently selected non-player target.");
					client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Optionally, self may be used to kill yourself");
				}
			}
			break;
						  }
		case COMMAND_LEVEL:{
			if(sep && sep->arg[ndx][0] && sep->IsNumber(0)){
				int16 new_level = atoi(sep->arg[ndx]);
				if (!client->GetPlayer()->CheckLevelStatus(new_level))
					client->SimpleMessage(CHANNEL_COLOR_RED, "You do not have the required status to level up anymore!");
				else {
					client->ChangeLevel(client->GetPlayer()->GetLevel(), new_level);
					client->GetPlayer()->SetXP(1);
					client->GetPlayer()->SetNeededXP();
				}
			}else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage: /level {new_level}");
			break;
						   }
		case COMMAND_SIT: {
			LogWrite(MISC__TODO, 1, "TODO", " Send update packet to all clients\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
			if(client->GetPlayer()->GetHP() > 0){
				client->QueuePacket(new EQ2Packet(OP_SitMsg, 0, 0));
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"You sit down.");
				client->GetPlayer()->set_character_flag(CF_IS_SITTING);
			}
			break;
						  }
		case COMMAND_STAND: {
			LogWrite(MISC__TODO, 1, "TODO", " Send update packet to all clients\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
			if(client->GetPlayer()->GetHP() > 0){
				client->QueuePacket(new EQ2Packet(OP_StandMsg, 0, 0));
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"You stand up.");
				client->GetPlayer()->reset_character_flag(CF_IS_SITTING);
			}
			break;
							}
		case COMMAND_CLASS:{
			if(sep && sep->arg[ndx][0]){
				client->GetPlayer()->GetInfoStruct()->class1 = atoi(sep->arg[ndx]);
				client->GetPlayer()->GetInfoStruct()->class2 = atoi(sep->arg[ndx]);
				client->GetPlayer()->GetInfoStruct()->class3 = atoi(sep->arg[ndx]);
				client->GetPlayer()->SetCharSheetChanged(true);
				client->UpdateTimeStampFlag ( CLASS_UPDATE_FLAG );
			}else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage:  /class {class_id}");
			break;
						   }
		case COMMAND_GENDER:{
			if(sep && sep->arg[ndx][0]){
				client->GetPlayer()->GetInfoStruct()->gender = atoi(sep->arg[ndx]);
				client->GetPlayer()->SetCharSheetChanged(true);
				client->UpdateTimeStampFlag ( GENDER_UPDATE_FLAG );
			}else{
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage:  /gender {new_gender_id}");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"  Male: 1");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"  Female: 2");
			}
			break;
							}
		case COMMAND_FLYMODE:{
			PacketStruct* packet = configReader.getStruct("WS_ServerControlFlags", client->GetVersion());
			if(packet && sep && sep->arg[0] && sep->IsNumber(0)){
				PrintSep(sep, "COMMAND_FLYMODE");
				int8 val = atoi(sep->arg[0]);
				packet->setDataByName("parameter5", 32);
				packet->setDataByName("value", val);
				client->QueuePacket(packet->serialize());

				client->Message(CHANNEL_STATUS, "Flymode %s", val == 1 ? "on" : "off");
				/*
				Some other values for this packet
				first param:
				01 flymode
				02 collisons off
				04 unknown
				08 forward movement
				16 heading movement
				32 low gravity
				64 sit

				second
				2 crouch


				third:
				04 float when trying to jump, no movement
				08 jump high, no movement

				fourth:
				04 autorun (fear?)
				16 moon jumps
				32 safe fall (float to ground)
				64 cant move

				fifth:
				01 die
				08 hover (fae)
				32 flymode2?

				*/
				safe_delete(packet);
			}
			else{
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage ON: /flymode 1");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage OFF: /flymode 0");
			}
			break;
							 }
		case COMMAND_LOOT_LIST:{
			Spawn* spawn = client->GetPlayer()->GetTarget();
			if(spawn && spawn->IsEntity()){
				vector<int32> loot_list = client->GetCurrentZone()->GetSpawnLootList(spawn->GetDatabaseID(), spawn->GetZone()->GetZoneID(), spawn->GetLevel(), race_types_list.GetRaceType(spawn->GetModelType()));
				if(loot_list.size() > 0){
					client->Message(CHANNEL_COLOR_YELLOW, "%s belongs to the following loot lists: ", spawn->GetName());
					vector<int32>::iterator list_itr;
					LootTable* table = 0;
					for(list_itr = loot_list.begin(); list_itr != loot_list.end(); list_itr++){
						table = client->GetCurrentZone()->GetLootTable(*list_itr);
						if(table)
							client->Message(CHANNEL_COLOR_YELLOW, "%u - %s", *list_itr, table->name.c_str());
					}
				}
				Entity* target = (Entity*)spawn;
				client->Message(CHANNEL_COLOR_YELLOW, "Coins being carried: %u", target->GetLootCoins());
				vector<Item*>* items = target->GetLootItems();
				if(items){
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Spawn is carrying the following items: ");
					vector<Item*>::iterator itr;
					Item* item = 0;
					for(itr = items->begin(); itr != items->end(); itr++){
						item = *itr;
						client->Message(CHANNEL_COLOR_YELLOW, "%u - %s", item->details.item_id, item->name.c_str());
					}
				}
				else
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Spawn is not carrying any items.");
			}
			break;
							   }
		case COMMAND_LOOT_SETCOIN:{
			Spawn* spawn = client->GetPlayer()->GetTarget();
			if(spawn && spawn->IsEntity() && sep && sep->arg[0] && sep->IsNumber(0)){
				Entity* target = (Entity*)spawn;
				target->SetLootCoins(atoul(sep->arg[0]));
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Successfully set coins.");
			}
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Syntax: /loot setcoin {amount}");
			break;
								  }
		case COMMAND_LOOT_ADDITEM:{
			Spawn* spawn = client->GetPlayer()->GetTarget();
			if(spawn && spawn->IsEntity() && sep && sep->arg[0] && sep->IsNumber(0)){
				Entity* target = (Entity*)spawn;
				int16 charges = 1;
				if(sep->arg[1] && sep->IsNumber(1))
					charges = atoi(sep->arg[1]);
				target->AddLootItem(atoul(sep->arg[0]), charges);
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Successfully added item.");
			}
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Syntax: /loot additem {item_id}");
			break;
								  }
		case COMMAND_LOOT_REMOVEITEM:{
			Spawn* spawn = client->GetPlayer()->GetTarget();
			if(spawn && spawn->IsEntity() && sep && sep->arg[0] && sep->IsNumber(0)){
				Entity* target = (Entity*)spawn;
				target->LootItem(atoul(sep->arg[0]));
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Successfully removed item.");
			}
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Syntax: /loot removeitem {item_id}");
			break;
									 }
		case COMMAND_LOOT_CORPSE:
		case COMMAND_LOOT:{
			Spawn* target = client->GetPlayer()->GetTarget();
			if(target && target->IsEntity() && target->GetHP() == 0){
				if (target->GetDistance(client->GetPlayer()) <= 10){
					client->Loot((Entity*)target);
					if (!((Entity*)target)->HasLoot()){
						if (((Entity*)target)->IsNPC())
							client->GetCurrentZone()->RemoveDeadSpawn(target);
					}
				}
				else
					client->Message(CHANNEL_COLOR_YELLOW, "You are too far away to interact with that");
			}
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Invalid target.");
			break;
						  }
		case COMMAND_RELOADSPAWNSCRIPTS:{
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Reloading Spawn Scripts....");
			if (lua_interface)
				lua_interface->SetSpawnScriptsReloading(true);
			world.ResetSpawnScripts();
			database.LoadSpawnScriptData();
			if(lua_interface) {
				lua_interface->DestroySpawnScripts();
				lua_interface->SetSpawnScriptsReloading(false);
			}
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Success!");
			break;
										}
		case COMMAND_RELOADLUASYSTEM:{
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Attempting to reload entire LUA system....");
			map<Client*, int32> debug_clients = lua_interface->GetDebugClients();
			safe_delete(lua_interface);
			lua_interface = new LuaInterface();
			if(lua_interface)
				lua_interface->SetSpawnScriptsReloading(true);

			if(lua_interface && debug_clients.size() > 0){
				map<Client*, int32>::iterator itr;
				for(itr = debug_clients.begin(); itr != debug_clients		.end(); itr++){
					if(lua_interface)
						lua_interface->UpdateDebugClients(itr->first);
				}
			}

			world.ResetSpawnScripts();
			database.LoadSpawnScriptData();

			world.ResetZoneScripts();
			database.LoadZoneScriptData();

			int32 quest_count = database.LoadQuests();

			zone_list.DeleteSpellProcess();
			master_spell_list.Reload();
			int32 spell_count = 0;

			if(lua_interface) {
				lua_interface->DestroySpawnScripts();
				lua_interface->DestroySpells();
				spell_count = database.LoadSpellScriptData();
				lua_interface->SetSpawnScriptsReloading(false);
			}
			zone_list.LoadSpellProcess();

			client->Message(CHANNEL_COLOR_YELLOW, "Successfully loaded %u spell(s), %u quest(s).", spell_count, quest_count);
			break;
									 }
		case COMMAND_DECLINE_QUEST:{
			int32 quest_id = 0;
			if(sep && sep->arg[0] && sep->IsNumber(0))
				quest_id = atoul(sep->arg[0]);
			if(quest_id > 0){
				Quest* quest = client->GetPendingQuest(quest_id);
				if(quest){
					client->RemovePendingQuest(quest);
					if(lua_interface)
						lua_interface->CallQuestFunction(quest, "Declined", client->GetPlayer());
					safe_delete(quest);
					client->GetCurrentZone()->SendQuestUpdates(client);
				}
			}
			break;
								   }
		case COMMAND_DELETE_QUEST:{
			int32 quest_id = 0;
			if(sep && sep->arg[0] && sep->IsNumber(0))
				quest_id = atoul(sep->arg[0]);
			if(quest_id > 0){
				if(lua_interface && client->GetPlayer()->player_quests.count(quest_id) > 0) {
					Quest* quest = client->GetPlayer()->player_quests[quest_id];
					if (quest)
						lua_interface->CallQuestFunction(quest, "Deleted", client->GetPlayer());
				}
				client->RemovePlayerQuest(quest_id);
				client->GetCurrentZone()->SendQuestUpdates(client);
			}
			break;
								  }
		case COMMAND_ACCEPT_QUEST:{
			int32 quest_id = 0;
			if(sep && sep->arg[0] && sep->IsNumber(0))
				quest_id = atoul(sep->arg[0]);
			if(quest_id > 0){
				Quest* quest = client->GetPendingQuest(quest_id);
				if(quest){
					client->RemovePendingQuest(quest);
					client->AddPlayerQuest(quest);
					client->GetCurrentZone()->SendQuestUpdates(client);

					// If character has already completed this quest once update the given date in the database
					if (client->GetPlayer()->GetCompletedPlayerQuests()->count(quest_id) > 0) {
						Quest* quest2 = client->GetPlayer()->GetCompletedQuest(quest_id);
						if (quest2)
							quest->SetCompleteCount(quest2->GetCompleteCount());

						database.SaveCharRepeatableQuest(client, quest_id, quest->GetCompleteCount());
						
					}
				}
			}
			break;
								  }
		case COMMAND_NAME:{
			if(sep && sep->arg[ndx][0]){
				memset(client->GetPlayer()->GetInfoStruct()->name, 0, 40);
				strncpy(client->GetPlayer()->GetInfoStruct()->name, sep->arg[ndx], 39);
				client->GetPlayer()->SetCharSheetChanged(true);
			}else
				client->Message(CHANNEL_COLOR_YELLOW,"Usage: /name {new_name}");

			break;
		}
		case COMMAND_GROUPSAY:{
			GroupMemberInfo* gmi = client->GetPlayer()->GetGroupMemberInfo();
			if(sep && sep->arg[0] && gmi)
				world.GetGroupManager()->GroupChatMessage(gmi->group_id, client->GetPlayer(), sep->argplus[0]);
			break;
		}
		case COMMAND_GROUPINVITE: {
			Entity* target = 0;
			Client* target_client = 0;

			if (client->GetPlayer()->GetGroupMemberInfo() && !client->GetPlayer()->GetGroupMemberInfo()->leader) {
				client->SimpleMessage(CHANNEL_COMMANDS, "You must be the group leader to invite.");
				return;
			}

			// name provided with command so it has to be a player, npc not supported here
			if (sep && sep->arg[0] && strlen(sep->arg[0]) > 0){
				target_client = zone_list.GetClientByCharName(sep->arg[0]);
				if (target_client) {
					if (!target_client->IsConnected() || !target_client->IsReadyForSpawns())
						target = 0;
					else
						target = target_client->GetPlayer();
				}
			}

			// if name was not provided use the players target, npc or player supported here
			if (!target && client->GetPlayer()->GetTarget() && client->GetPlayer()->GetTarget()->IsEntity()) {
				target = (Entity*)client->GetPlayer()->GetTarget();
				if (target->IsPlayer()) {
					target_client = client->GetCurrentZone()->GetClientBySpawn(target);
				}
			}

			int8 result = world.GetGroupManager()->Invite(client->GetPlayer(), target);

			if (result == 0) {
				client->Message(CHANNEL_COMMANDS, "You invite %s to group with you.", target->GetName());
				if (target_client) {
					PacketStruct* packet = configReader.getStruct("WS_ReceiveOffer", target_client->GetVersion());
					if (packet) {
						packet->setDataByName("type", 1);
						packet->setDataByName("name", client->GetPlayer()->GetName());
						packet->setDataByName("unknown2", 1);
						target_client->QueuePacket(packet->serialize());
						safe_delete(packet);
					}
				}
			}
			else if (result == 1)
				client->SimpleMessage(CHANNEL_COMMANDS, "That player is already in a group.");
			else if (result == 2)
				client->SimpleMessage(CHANNEL_COMMANDS, "That player has been invited to another group.");
			else if (result == 3)
				client->SimpleMessage(CHANNEL_COMMANDS, "Your group is already full.");
			else if (result == 4)
				client->SimpleMessage(CHANNEL_COMMANDS, "You have a pending invitation, cancel it first.");
			else if (result == 5)
				client->SimpleMessage(CHANNEL_COMMANDS, "You cannot invite yourself!");
			else if (result == 6)
				client->SimpleMessage(CHANNEL_COMMANDS, "Could not locate the player.");
			else
				client->SimpleMessage(CHANNEL_COMMANDS, "Group invite failed, unknown error!");

			break;
		}
		case COMMAND_GROUPDISBAND:
		case COMMAND_GROUP_LEAVE: {
			GroupMemberInfo* gmi = client->GetPlayer()->GetGroupMemberInfo();

			if (gmi) {
				int32 group_id = gmi->group_id;
				world.GetGroupManager()->RemoveGroupMember(group_id, client->GetPlayer());

				if (world.GetGroupManager()->IsGroupIDValid(group_id)) {
					world.GetGroupManager()->GroupMessage(group_id, "%s has left the group.", client->GetPlayer()->GetName());
				}

				client->SimpleMessage(CHANNEL_COLOR_GROUP, "You have left the group");
			}

			break;
		}
		case COMMAND_FRIEND_ADD:{
			string name = "";
			if(sep && sep->arg[0] && strlen(sep->arg[0]) > 1)
				name = database.GetPlayerName(sep->arg[0]);
			else if(client->GetPlayer()->GetTarget() && client->GetPlayer()->GetTarget()->IsPlayer())
				name = string(client->GetPlayer()->GetTarget()->GetName());
			if(name.length() > 0){
				if(strcmp(name.c_str(), client->GetPlayer()->GetName()) != 0){
					if(client->GetPlayer()->IsIgnored(name.c_str())){
						client->GetPlayer()->RemoveIgnore(name.c_str());
						client->Message(CHANNEL_COLOR_CHAT_RELATIONSHIP, "Removed ignore: %s", name.c_str());
						client->SendChatRelationship(3, name.c_str());
					}
					client->GetPlayer()->AddFriend(name.c_str(), true);
					client->SendChatRelationship(0, name.c_str());
					client->Message(CHANNEL_COLOR_CHAT_RELATIONSHIP, "Added friend: %s", name.c_str());
					if(zone_list.GetClientByCharName(name))
						client->Message(CHANNEL_COLOR_CHAT_RELATIONSHIP, "Friend: %s has logged in.", name.c_str());
				}
				else
					client->SimpleMessage(CHANNEL_COLOR_RED, "You cannot be more friendly towards yourself!");
			}
			else
				client->SimpleMessage(CHANNEL_COLOR_CHAT_RELATIONSHIP, "That character does not exist.");
			break;
		}
		case COMMAND_FRIEND_REMOVE:{
			string name = "";
			if(sep && sep->arg[0] && strlen(sep->arg[0]) > 1)
				name = database.GetPlayerName(sep->arg[0]);
			else if(client->GetPlayer()->GetTarget() && client->GetPlayer()->GetTarget()->IsPlayer())
				name = string(client->GetPlayer()->GetTarget()->GetName());
			if(name.length() > 0){
				client->GetPlayer()->RemoveFriend(name.c_str());
				client->SendChatRelationship(1, name.c_str());
				client->Message(CHANNEL_COLOR_CHAT_RELATIONSHIP, "Removed friend: %s", name.c_str());
			}
			else
				client->SimpleMessage(CHANNEL_COLOR_CHAT_RELATIONSHIP, "That character does not exist.");
			break;
		}
		case COMMAND_FRIENDS:{

			break;
		}
	    case COMMAND_IGNORE_ADD:{
			string name = "";
			if(sep && sep->arg[0] && strlen(sep->arg[0]) > 1)
				name = database.GetPlayerName(sep->arg[0]);
			else if(client->GetPlayer()->GetTarget() && client->GetPlayer()->GetTarget()->IsPlayer())
				name = string(client->GetPlayer()->GetTarget()->GetName());
			if(name.length() > 0){
				if(strcmp(name.c_str(), client->GetPlayer()->GetName()) != 0){
					if(client->GetPlayer()->IsFriend(name.c_str())){
						client->GetPlayer()->RemoveFriend(name.c_str());
						client->Message(CHANNEL_COLOR_CHAT_RELATIONSHIP, "Removed friend: %s", name.c_str());
						client->SendChatRelationship(1, name.c_str());
					}
					client->GetPlayer()->AddIgnore(name.c_str(), true);
					client->SendChatRelationship(2, name.c_str());
					client->Message(CHANNEL_COLOR_CHAT_RELATIONSHIP, "Added ignore: %s", name.c_str());
				}
				else
					client->SimpleMessage(CHANNEL_COLOR_RED, "You cannot ignore yourself!");
			}
			else
				client->SimpleMessage(CHANNEL_COLOR_CHAT_RELATIONSHIP, "That character does not exist.");
			break;
		}
		case COMMAND_IGNORE_REMOVE:{
			string name = "";
			if(sep && sep->arg[0] && strlen(sep->arg[0]) > 1)
				name = database.GetPlayerName(sep->arg[0]);
			else if(client->GetPlayer()->GetTarget() && client->GetPlayer()->GetTarget()->IsPlayer())
				name = string(client->GetPlayer()->GetTarget()->GetName());
			if(name.length() > 0){
				client->GetPlayer()->RemoveIgnore(name.c_str());
				client->SendChatRelationship(3, name.c_str());
				client->Message(CHANNEL_COLOR_CHAT_RELATIONSHIP, "Removed ignore: %s", name.c_str());
			}
			break;
		}
		case COMMAND_IGNORES:{

			break;
		}
		case COMMAND_GROUP_KICK:{
			Entity* kicked = 0;
			Client* kicked_client = 0;
			int32 group_id = 0;

			if (!client->GetPlayer()->GetGroupMemberInfo() || !client->GetPlayer()->GetGroupMemberInfo()->leader)
				return;

			LogWrite(MISC__TODO, 0, "TODO", "Add a check to see if the client using the command (COMMAND_GROUP_KICK) is the group leader");

			if (sep && sep->arg[0]) {
				kicked_client = zone_list.GetClientByCharName(string(sep->arg[0]));
				if (kicked_client)
					kicked = kicked_client->GetPlayer();
			}
			else if (client->GetPlayer()->GetTarget() && client->GetPlayer()->GetTarget()->IsEntity()) {
				kicked = (Entity*)client->GetPlayer()->GetTarget();
			}

			group_id = client->GetPlayer()->GetGroupMemberInfo()->group_id;

			bool send_kicked_message = world.GetGroupManager()->GetGroupSize(group_id) > 2;
			if (kicked)
				world.GetGroupManager()->RemoveGroupMember(group_id, kicked);

			if(send_kicked_message)
				world.GetGroupManager()->GroupMessage(group_id, "%s was kicked from the group.", kicked->GetName());
			else
				client->Message(CHANNEL_COLOR_GROUP, "You kicked %s from the group", kicked->GetName());

			if (kicked_client)
				kicked_client->SimpleMessage(CHANNEL_COLOR_GROUP, "You were kicked from the group");
				
			break;
		}
	    case COMMAND_GROUP_MAKE_LEADER:{
			
			if (!client->GetPlayer()->GetGroupMemberInfo() || !client->GetPlayer()->GetGroupMemberInfo()->leader) {
				client->SimpleMessage(CHANNEL_COMMANDS, "You are not a group leader.");
				return;
			}

			if (sep && sep->arg[0] && strlen(sep->arg[0]) > 1) {
				Client* new_leader = zone_list.GetClientByCharName(sep->arg[0]);
				if (new_leader) {
					if (client->GetPlayer()->IsGroupMember(new_leader->GetPlayer())) {
						world.GetGroupManager()->MakeLeader(client->GetPlayer()->GetGroupMemberInfo()->group_id, new_leader->GetPlayer());
					}
				}
				else
					client->SimpleMessage(CHANNEL_COMMANDS, "Unable to find the given player.");
			}
			else if (client->GetPlayer()->GetTarget() && client->GetPlayer()->GetTarget()->IsPlayer())
				world.GetGroupManager()->MakeLeader(client->GetPlayer()->GetGroupMemberInfo()->group_id, (Entity*)client->GetPlayer()->GetTarget());
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Unable to change group leader.");

			break;
		}
		case COMMAND_GROUP_ACCEPT_INVITE: {
			if(sep && sep->arg[0] && strcmp(sep->arg[0], "group") == 0) {
				int8 result = world.GetGroupManager()->AcceptInvite(client->GetPlayer());

				if (result == 0)
					client->SimpleMessage(CHANNEL_GROUP, "You have joined the group.");
				else if (result == 1)
					client->SimpleMessage(CHANNEL_GROUP, "You do not have a pending invite.");
				else if (result == 2)
					client->SimpleMessage(CHANNEL_GROUP, "Unable to join group - could not find leader.");
				else
					client->SimpleMessage(CHANNEL_GROUP, "Unable to join group - unknown error.");
			}
			break;
		}
		case COMMAND_GROUP_DECLINE_INVITE: {
			if(sep && sep->arg[0] && strcmp(sep->arg[0], "group") == 0) {
				world.GetGroupManager()->DeclineInvite(client->GetPlayer()); // TODO: Add message to leader
			}
			break;
		}
		case COMMAND_SUMMON:{
			char* search_name = 0;
			if(sep && sep->arg[0][0])
				search_name = sep->arg[0];

			if(!search_name && !client->GetPlayer()->GetTarget()){
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage: /summon [name]");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Summons a targeted spawn or a spawn by name to you.  If more than one spawn matches name, it will summon closest.");
			}
			else{ 
				if(!client->Summon(search_name))
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "No matches found.");
			}
			break;
							}
		case COMMAND_GOTO:{
			const char* search_name = 0;
			if(sep && sep->arg[0][0])
				search_name = sep->argplus[0];
			if(!search_name && !client->GetPlayer()->GetTarget()){
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage: /goto [name]");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Moves to targeted spawn or to a spawn by name.  If more than one spawn matches name, you will move to closest.");
			}
			else{
				if(!client->GotoSpawn(search_name))
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "No matches found.");
			}
			break;
						  }
		case COMMAND_MOVE:{
			bool success = false;
			try{
				if(sep && sep->arg[2][0] && sep->IsNumber(0) && sep->IsNumber(1) && sep->IsNumber(2)){
					EQ2Packet* app = client->GetPlayer()->Move(atof(sep->arg[0]), atof(sep->arg[1]), atof(sep->arg[2]), client->GetVersion());
					if(app){
						client->QueuePacket(app);
						success = true;
					}
				}
			}
			catch(...){}
			if(!success){
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage: /move x y z");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Moves the client to the given coordinates.");
			}
			break;
						  }
		case COMMAND_SETTIME:{
			if(sep && sep->arg[0]){
				sscanf (sep->arg[0], "%d:%d", &world.GetWorldTimeStruct()->hour, &world.GetWorldTimeStruct()->minute);
				if(sep->arg[1] && sep->IsNumber(1))
					world.GetWorldTimeStruct()->month = atoi(sep->arg[1]) - 1; //zero based indexes
				if(sep->arg[2] && sep->IsNumber(2))
					world.GetWorldTimeStruct()->day = atoi(sep->arg[2]) - 1; //zero based indexes
				if(sep->arg[3] && sep->IsNumber(3))
					world.GetWorldTimeStruct()->year = atoi(sep->arg[3]);
				client->GetCurrentZone()->SendTimeUpdateToAllClients();
			}
			else{
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage: /settime [hour:minute] (month) (day) (year)");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Example: /settime 8:30");
			}
			break;
							 }
		case COMMAND_VERSION:{
			client->Message(CHANNEL_COLOR_YELLOW,"%s %s", EQ2EMU_MODULE, CURRENT_VERSION);
			client->Message(CHANNEL_COLOR_YELLOW,"Last Compiled on %s %s", COMPILE_DATE, COMPILE_TIME);
			break;
							 }
		case COMMAND_ZONE:{
			string* zonename = 0;
			int32 instanceID = 0;
			const char* zone = 0;
			int32 zone_id = 0;
			bool listSearch = false;
			bool isInstance = false;
			ZoneServer* zsZone = 0;
			
			if(sep && sep->arg[0][0])
			{
				if(strncasecmp(sep->arg[0], "list", 4) == 0)
					listSearch = true;

				else if(strncasecmp(sep->arg[0], "active", 6) == 0)
				{
					zone_list.SendZoneList(client);
					break;
				}

				else if(strncasecmp(sep->arg[0], "instance", 6) == 0)
				{
					if(sep->IsNumber(1))
						instanceID = atol(sep->arg[1]);
				}

				else if(strncasecmp(sep->arg[0], "lock", 4) == 0)
				{
					PrintSep(sep, "ZONE LOCK");

					if(sep->IsNumber(1))
						zsZone = zone_list.Get(atoul(sep->arg[1]), false);
					else
						zsZone = zone_list.Get(sep->arg[1], false);

					if( zsZone )
					{
						zsZone->SetZoneLockState(true);
						client->Message(CHANNEL_COLOR_YELLOW, "Zone %s (%u) is now locked.", zsZone->GetZoneName(), zsZone->GetZoneID());
					}
					else
						client->Message(CHANNEL_COLOR_RED, "Zone %s is not running and cannot be locked.", sep->arg[1]);

					break;
				}
				else if(strncasecmp(sep->arg[0], "unlock", 6) == 0)
				{
					PrintSep(sep, "ZONE UNLOCK");

					if(sep->IsNumber(1))
						zsZone = zone_list.Get(atoul(sep->arg[1]), false);
					else
						zsZone = zone_list.Get(sep->arg[1], false);

					if( zsZone )
					{
						zsZone->SetZoneLockState(false);
						client->Message(CHANNEL_COLOR_YELLOW, "Zone %s (%u) is now unlocked.", zsZone->GetZoneName(), zsZone->GetZoneID());
					}
					else
						client->Message(CHANNEL_COLOR_RED, "Zone %s is not running and cannot be unlocked.", sep->arg[1]);
					break;
				}
				else
				{
					if(sep->IsNumber(0))
					{
						zone_id = atoul(sep->arg[0]);
						zonename = database.GetZoneName(zone_id);

						if(zonename)
							zone = zonename->c_str();
					}
					else
						zone = sep->arg[0];
				}
				if(instanceID > 0)
				{
					ZoneServer* zsInstance = zone_list.GetByInstanceID(instanceID);

					if(zsInstance != NULL)
					{
						instanceID = zsInstance->GetInstanceID();
						zone = zsInstance->GetZoneName();
						zone_id = zsInstance->GetZoneID();
						isInstance = true;
					}
				}
			}

			if(!listSearch)
			{
				if(!zone)
					client->Message(CHANNEL_COLOR_RED, "Error: Invalid Zone");
				else if(instanceID != client->GetCurrentZone()->GetInstanceID() || 
					zone_id != client->GetCurrentZone()->GetZoneID())
				{
					if(database.VerifyZone(zone))
					{
						if(client->CheckZoneAccess(zone))
						{
							client->Message(CHANNEL_COLOR_YELLOW,"Zoning to %s...", zone);
							if(isInstance)
								client->Zone(instanceID,true,true);
							else
								client->Zone(zone);
						}
					}
					else
						client->Message(CHANNEL_COLOR_RED, "Error: Zone '%s' not found.  To get a list type /zone list", zone);
				}
				else
					client->Message(CHANNEL_COLOR_RED, "Error: You are already in that zone!");
			}
			else
			{
				const char* name = 0;
				if(sep && sep->arg[1][0])
				{
					map<int32, string>* zone_names;
					name = sep->argplus[1];
					sint16 status = client->GetAdminStatus();
					if( status > 0 )
					{
						client->SimpleMessage(CHANNEL_COLOR_RED, "ADMIN MODE");
						zone_names = database.GetZoneList(name, true);
					}
					else
						zone_names = database.GetZoneList(name);

					if(!zone_names)
						client->SimpleMessage(CHANNEL_COLOR_YELLOW, "No zones found.");
					else
					{
						map<int32, string>::iterator itr;
						client->SimpleMessage(CHANNEL_COLOR_YELLOW," ID   Name: ");
						for(itr = zone_names->begin(); itr != zone_names->end(); itr++)
							client->Message(CHANNEL_COLOR_YELLOW,"%03lu %s", itr->first, itr->second.c_str());
						safe_delete(zone_names);
					}
				}
				else
				{
					client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Syntax: /zone [zone name]");
					client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Syntax: /zone list [partial zone name]");
				}
			}
			safe_delete(zonename);
			break;
		}
		case COMMAND_USE:{
			Spawn* target = client->GetPlayer()->GetTarget();
			if(target->IsWidget())
				((Widget*)target)->HandleUse(client, "use");
			break;
						 }
		case COMMAND_ATTACK:
		case COMMAND_AUTO_ATTACK:{
			int8 type = 1;
			Player* player = client->GetPlayer();

			if(!player)
				break;

			if (client->GetPlayer()->GetHP() == 0) {
				client->SimpleMessage(CHANNEL_COLOR_RED, "You cannot do that right now.");
				break;
			}

			bool in_combat = player->EngagedInCombat();
			if(sep && sep->arg[0] && sep->IsNumber(0))
				type = atoi(sep->arg[0]);

			switch (type) {
			case 0:
				if (in_combat)
					client->SimpleMessage(CHANNEL_COLOR_COMBAT, "You stop fighting.");

				player->InCombat(false);
				player->SetRangeAttack(false);
				player->SetMeleeAttack(false);

				break;

			case 1:
				if (!in_combat)
					client->SimpleMessage(CHANNEL_COLOR_COMBAT, "You start fighting.");
				player->InCombat(true);
				player->SetMeleeAttack(true);
				player->SetRangeAttack(false);

				break;

			case 2:
				player->InCombat(true);
				player->SetMeleeAttack(false);
				player->SetRangeAttack(true);

				break;
			}

			break;
		}
		case COMMAND_DEPOP:{
			bool allow_respawns = false;
			if(sep && sep->arg[0] && sep->IsNumber(0)){
				if(atoi(sep->arg[0]) == 1)
					allow_respawns = true;
			}
			client->GetCurrentZone()->Depop(allow_respawns);
			break;
						   }
		case COMMAND_REPOP:{
			client->GetCurrentZone()->Depop(false, true);
			break;
						   }
		case COMMAND_BUYBACK:{
			if(sep && sep->arg[1][0] && sep->IsNumber(0) && sep->IsNumber(1)){
				// Item ID and Unique ID get combined in this command so we need to get the number as an int64 and break it into 2 int32's
				int64 id = atoi64(sep->arg[0]);
				// get the first int32, the item id
				//int32 item_id = (int32)id;
				// get the second int32, the unique id
				int32 unique_id = (int32)(id>>32);
				int8 quantity = atoi(sep->arg[1]);
				if(quantity == 255)
					quantity = 1;
				client->BuyBack(unique_id, quantity);
			}
			break;
							 }
		case COMMAND_MERCHANT_BUY:{
			if(sep && sep->arg[1][0] && sep->IsNumber(0) && sep->IsNumber(1)){
				int32 item_id = atoul(sep->arg[0]);
				int8 quantity = atoi(sep->arg[1]);
				client->BuyItem(item_id, quantity);
			}
			break;
								  }
		case COMMAND_MERCHANT_SELL:{
			if(sep && sep->arg[1][0] && sep->IsNumber(0) && sep->IsNumber(1)){
				// Item ID and Unique ID get combined in this command so we need to get the number as an int64 and break it into 2 int32's
				int64 id = atoi64(sep->arg[0]);
				// get the first int32, the item id
				int32 item_id = (int32)id;
				// get the second int32, the unique id
				int32 unique_id = (int32)(id>>32);

				int8 quantity = atoi(sep->arg[1]);
				LogWrite(MERCHANT__DEBUG, 0, "Merchant", "Selling Item: Item Id = %ul, unique id = %ul, Quantity = %i", item_id, unique_id, quantity);
				client->SellItem(item_id, quantity, unique_id);
			}
			break;
								   }
		case COMMAND_MENDER_REPAIR: {
			if (sep && sep->arg[0] && sep->IsNumber(0))
				client->RepairItem(atoul(sep->arg[0]));
			break;
									}
		case COMMAND_MENDER_REPAIR_ALL: {
			client->RepairAllItems();
			break;
										}
		case COMMAND_CANCEL_MERCHANT:{
			client->SetMerchantTransaction(0);
			break;
									 }
		case COMMAND_START_MERCHANT:{
			break;
									}
		case COMMAND_INVULNERABLE:{
			sint8 val = -1;
			if(sep && sep->arg[0] && sep->IsNumber(0)){
				val = atoi(sep->arg[0]);
				client->GetPlayer()->SetInvulnerable(val==1);
				if(client->GetPlayer()->GetInvulnerable())
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are now invulnerable!");
				else
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are no longer invulnerable!");
			}
			if(val == -1)
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /invulnerable [0/1]");
			break;
		}
		case COMMAND_REPAIR: {
			Spawn* spawn = client->GetPlayer()->GetTarget();
			if (spawn && spawn->GetMerchantType() & MERCHANT_TYPE_REPAIR) {
				client->SetMerchantTransaction(spawn);
				client->SendRepairList();
			}
			break;
							 }
		case COMMAND_LOTTO: {
			Spawn* spawn = client->GetPlayer()->GetTarget();
			if (spawn && spawn->GetMerchantType() & MERCHANT_TYPE_LOTTO) {
				client->SetMerchantTransaction(spawn);
				client->ShowLottoWindow();
			}
			break;
							}
		case COMMAND_ACCEPT_REWARD:{
			int32 unknown = 0;
			int32 selectable_item_id = 0;
			//Quest *quest = 0;
			Collection *collection = 0;
			
			/* no idea what the first argument is for (faction maybe?)
			   if the reward has a selectable item reward, it's sent as the second argument
			   if neither of these are included in the reward, there is no sep
			   since the regular item rewards are manditary to receive, we don't have to know what they are until we accept the collection or quest
			   only 1 quest or collection reward may be displayed at a time */
			if (sep && sep->arg[0] && sep->arg[1] && sep->IsNumber(0) && sep->IsNumber(1)) {
				unknown = atoul(sep->arg[0]);
				selectable_item_id = atoul(sep->arg[1]);
			}

			//if ((quest = player->GetPendingQuestReward()))
			//	client->AcceptQuestRewards(quest, selectable_item_id);

			/* the below needs to go away eventually and be redone */
			int32 item_id = 0;
			if(sep && sep->arg[0][0] && sep->IsNumber(0))
				item_id = atoul(sep->arg[0]);
			Quest* quest = client->GetPendingQuestAcceptance(item_id);
			if(quest){
				client->AcceptQuestReward(quest, item_id);
				break;
			}

			if (collection = player->GetPendingCollectionReward())
				client->AcceptCollectionRewards(collection, selectable_item_id);
			else
				LogWrite(COMMAND__ERROR, 0, "Command", "Error in COMMAND_ACCEPT_REWARD. No pending quest or collection reward was found (unknown=%u).", unknown);
			break;
		}
		case COMMAND_BUY_FROM_BROKER:{
			if(sep && sep->arg[1][0] && sep->IsNumber(0) && sep->IsNumber(1)){
				int32 item_id = atoul(sep->arg[0]);
				int8 quantity = atoul(sep->arg[1]);
				client->AddItem(item_id, quantity);
			}
			break;
		}
		case COMMAND_SEARCH_STORES_PAGE:{
			if(sep && sep->arg[0][0] && sep->IsNumber(0)){
				int32 page = atoul(sep->arg[0]);
				client->SearchStore(page);
			}
			break;
		}
		case COMMAND_SEARCH_STORES:{
			if(sep && sep->arg[0][0]){
				const char* values = sep->argplus[0];
				if(values){
					map<string, string> str_values = TranslateBrokerRequest(values);
					vector<Item*>* items = master_item_list.GetItems(str_values);
					if(items){
						client->SetItemSearch(items);
						client->SearchStore(0);
					}
					}
				}
			break;
		}
		
		case COMMAND_LUADEBUG:{
			if(sep && sep->arg[0][0] && strcmp(sep->arg[0], "start") == 0){
				client->SetLuaDebugClient(true);
				if(lua_interface)
					lua_interface->UpdateDebugClients(client);
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You will now receive LUA error messages.");
			}
			else if(sep && sep->arg[0][0] && strcmp(sep->arg[0], "stop") == 0){
				client->SetLuaDebugClient(false);
				if(lua_interface)
					lua_interface->RemoveDebugClients(client);
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You will no longer receive LUA error messages.");
			}
			else{
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Syntax: /luadebug {start | stop}");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "This will allow you to receive lua debug messages normally seen only in the console.");
			}
			break;
		}
		case COMMAND_SPAWN_GROUP:
		{
			Spawn* target = client->GetPlayer()->GetTarget();

			if(target && target->IsPlayer() == false)
			{
				if(sep && sep->arg[0][0] && strcmp(sep->arg[0], "create") == 0)
				{
					if(target->GetSpawnLocationPlacementID() > 0)
					{
						int32 id = database.CreateSpawnGroup(target, "");

						if(id > 0)
							client->Message(CHANNEL_COLOR_YELLOW, "Successfully created new spawn group with id: %u", id);
						else
							client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Error creating group, check console for details.");

						target->SetSpawnGroupID(id);
						target->AddSpawnToGroup(target);
					}
					else
						client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Could not use spawn to create a new group.  Likely cause would be a newly spawned spawn that did not exist when /reload spawns was last used.");
				}
				else if(sep && sep->arg[0][0] && strcmp(sep->arg[0], "add") == 0)
				{
					if(sep->arg[1] && sep->IsNumber(1))
					{
						int32 group_id = atoul(sep->arg[1]);
						Spawn* leader = client->GetCurrentZone()->GetSpawnGroup(group_id);

						if(leader)
						{
							leader->AddSpawnToGroup(target);
							target->SetSpawnGroupID(group_id);

							if(database.SpawnGroupAddSpawn(target, group_id))
								client->Message(CHANNEL_COLOR_YELLOW, "Successfully added '%s' to group id: %u", target->GetName(), group_id);
							else
								client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Error adding spawn to group, check console for details.");
						}
						else
							client->SimpleMessage(CHANNEL_COLOR_YELLOW, "No spawns found in the current zone for that spawn group, be sure to use '/spawn group create' first!");
					}
					else
						client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Syntax: /spawn group add [group id]");
				}
				else if(target->GetSpawnGroupID() == 0)
				{
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "That spawn is not in a group!");
				}
				else if(sep && sep->arg[0][0] && strcmp(sep->arg[0], "remove") == 0)
				{
					int32 group_id = target->GetSpawnGroupID();
					target->RemoveSpawnFromGroup();

					if(database.SpawnGroupRemoveSpawn(target, group_id))
						client->Message(CHANNEL_COLOR_YELLOW, "Successfully removed '%s' from group id: %u", target->GetName(), group_id);
					else
						client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Error removing spawn from group, check console for details.");
				}
				else if(sep && sep->arg[0][0] && strcmp(sep->arg[0], "associate") == 0)
				{
					int32 group_id = target->GetSpawnGroupID();

					if(sep->arg[1] && sep->IsNumber(1))
					{
						if(database.SpawnGroupAddAssociation(group_id, atoul(sep->arg[1])))
							client->Message(CHANNEL_COLOR_YELLOW, "Successfully associated group id %u with group id %u", group_id, atoul(sep->arg[1]));
						else
							client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Error associating groups, check console for details.");
					}
					else
						client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Allows you to spawn only one associated group at once.  Syntax: /spawn group associate [other group id]");
				}
				else if(sep && sep->arg[0][0] && strcmp(sep->arg[0], "deassociate") == 0)
				{
					int32 group_id = target->GetSpawnGroupID();

					if(sep->arg[1] && sep->IsNumber(1))
					{
						if(database.SpawnGroupRemoveAssociation(group_id, atoul(sep->arg[1])))
							client->Message(CHANNEL_COLOR_YELLOW, "Successfully removed association between group id %u and group id %u", group_id, atoul(sep->arg[1]));
						else
							client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Error removing group associations, check console for details.");
					}
					else
						client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Removes previous group associations.  Syntax: /spawn group deassociate [other group id]");
				}
				else if(sep && sep->arg[0][0] && strcmp(sep->arg[0], "chance") == 0)
				{
					if(sep->arg[1] && sep->IsNumber(1))
					{
						if(database.SetGroupSpawnChance(target->GetSpawnGroupID(), atof(sep->arg[1])))
							client->Message(CHANNEL_COLOR_YELLOW, "Successfully set group spawn chance to %f for group id: %u", atof(sep->arg[1]), target->GetSpawnGroupID());
						else
							client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Error setting group spawn chance, check console for details.");
					}
					else
						client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Syntax: /spawn group chance [group's spawn chance percentage]");
				}
				else
				{
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "This command allows you to modify spawn groups.");
					client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Syntax: /spawn group [create/add/remove/chance/associate/deassociate]");
				}
			}
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You must select a valid spawn to use this command.");
			break;
		}
		case COMMAND_SPAWN_COMBINE:{
			Spawn* spawn = client->GetPlayer()->GetTarget();
			float radius = 0;
			bool failed = true;
			if(spawn && !spawn->IsPlayer() && sep && sep->arg[0] && sep->arg[0][0]){
				failed = false;
				if(spawn->GetSpawnGroupID() > 0){
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Can not combine this spawn as it is in a spawn group.  Remove it from the group and try again.");
				}
				else if(sep->IsNumber(0)){
					radius = atof(sep->arg[0]);
					client->CombineSpawns(radius, spawn);
				}
				else{
					if(strncasecmp(sep->arg[0], "add", 3) == 0){
						client->AddCombineSpawn(spawn);
					}
					else if(strncasecmp(sep->arg[0], "remove", 6) == 0){
						client->RemoveCombineSpawn(spawn);
					}
					else if(strncasecmp(sep->arg[0], "save", 4) == 0){
						const char* name = 0;
						if(sep->arg[1] && sep->arg[1][0])
							name = sep->argplus[1];
						client->SaveCombineSpawns(name);
					}
					else
						failed = true;
				}
			}
			if(failed){
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "This command combines several spawns into a single spawn group.");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Syntax: /spawn combine [radius/add/remove/save]");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Ex: /spawn combine 1, /spawn combine save (spawn group name)");
			}
			break;
								   }
		case COMMAND_SPAWN_CREATE:{
			Spawn* spawn = 0;
			if(sep && sep->arg[4][0] && strncasecmp(sep->arg[0], "object", 6) == 0 && sep->IsNumber(1) && sep->IsNumber(2) && sep->IsNumber(3)){
				spawn = new Object();
				memset(&spawn->appearance, 0, sizeof(spawn->appearance));
			}
			else if (sep && sep->arg[4][0] && strncasecmp(sep->arg[0], "groundspawn", 11) == 0 && sep->IsNumber(1) && sep->IsNumber(2) && sep->IsNumber(3)) {
				spawn = new GroundSpawn();
				memset(&spawn->appearance, 0, sizeof(spawn->appearance));
			}
			else if (sep && sep->arg[4][0] && strncasecmp(sep->arg[0], "sign", 4) == 0 && sep->IsNumber(1) && sep->IsNumber(2) && sep->IsNumber(3)) {
				spawn = new Sign();
				memset(&spawn->appearance, 0, sizeof(spawn->appearance));
			}
			else if(sep && sep->arg[4][0] && strncasecmp(sep->arg[0], "npc", 3) == 0 && sep->IsNumber(1) && sep->IsNumber(2) && sep->IsNumber(3)){
				spawn = new NPC();
				memset(&spawn->appearance, 0, sizeof(spawn->appearance));
				spawn->appearance.pos.collision_radius = 32;
				spawn->secondary_command_list_id = 0;
				spawn->primary_command_list_id = 0;
				spawn->appearance.display_name = 1;
				spawn->appearance.show_level = 1;
				spawn->appearance.attackable = 1;
			}
			else{
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Syntax: /spawn create [spawn type] [race type] [class type] [level] [name] (difficulty) (size)");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "All parameters are required except difficulty and size.  Valid types are Object, NPC, Sign, or GroundSpawn");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Ex: /spawn create npc 203 1 50 'Lady Vox' 1 32");
				break;
			}
			spawn->SetID(Spawn::NextID());
			spawn->SetX(client->GetPlayer()->GetX());
			spawn->SetY(client->GetPlayer()->GetY());
			spawn->SetZ(client->GetPlayer()->GetZ());
			spawn->SetHeading(client->GetPlayer()->GetHeading());
			spawn->SetSpawnOrigX(spawn->GetX());
			spawn->SetSpawnOrigY(spawn->GetY());
			spawn->SetSpawnOrigZ(spawn->GetZ());
			spawn->SetSpawnOrigHeading(spawn->GetHeading());
			spawn->appearance.targetable = 1;
			spawn->appearance.race = 255;
			spawn->appearance.pos.grid_id = client->GetPlayer()->appearance.pos.grid_id;
			spawn->SetModelType(atoi(sep->arg[1]));
			spawn->SetAdventureClass(atoi(sep->arg[2]));
			spawn->SetLevel(atoi(sep->arg[3]));
			spawn->SetName(sep->arg[4]);
			if(sep->arg[5][0] && sep->IsNumber(5))
				spawn->SetEncounterLevel(atoi(sep->arg[5]));
			if(sep->arg[6][0] && sep->IsNumber(6))
				spawn->size = atoi(sep->arg[6]);
			if(spawn->GetTotalHP() == 0){
				spawn->SetTotalHP(25*spawn->GetLevel() + 1);
				spawn->SetHP(25*spawn->GetLevel() + 1);
			}
			if(spawn->GetTotalPower() == 0){
				spawn->SetTotalPower(25*spawn->GetLevel() + 1);
				spawn->SetPower(25*spawn->GetLevel() + 1);
			}
			client->GetCurrentZone()->AddSpawn(spawn);
			break;
								  }
		case COMMAND_SPAWN_EQUIPMENT:{
			Spawn* spawn = client->GetPlayer()->GetTarget();
			if(spawn && sep && sep->arg[1][0] && sep->IsNumber(0) && sep->IsNumber(1) && spawn->IsEntity()){
				int8 slot = atoi(sep->arg[0]);
				int16 type = atoi(sep->arg[1]);
				int8 red = 0;
				int8 green = 0;
				int8 blue = 0;
				int8 h_red = 0;
				int8 h_green = 0;
				int8 h_blue = 0;
				if(sep->arg[2])
					red = atoi(sep->arg[2]);
				if(sep->arg[3])
					green = atoi(sep->arg[3]);
				if(sep->arg[4])
					blue = atoi(sep->arg[4]);
				if(sep->arg[5])
					h_red = atoi(sep->arg[5]);
				if(sep->arg[6])
					h_green = atoi(sep->arg[6]);
				if(sep->arg[7])
					h_blue = atoi(sep->arg[7]);
				((Entity*)spawn)->SetEquipment(slot, type, red, green, blue, h_red, h_green, h_blue);
				database.SaveNPCAppearanceEquipment(spawn->GetDatabaseID() , slot, type, red, green, blue, h_red, h_green, h_blue);
			}
			else{
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Syntax: /spawn equipment [slot] [appearance id] (red) (green) (blue) (highlight red) (hgreen) (hblue)");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "This will set the given spawn's equipment. ");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Slot is 0-24, Appearance ID from appearances table, Colors are 0-255");
			}
			break;
									 }
		case COMMAND_SPAWN_DETAILS:{
			Spawn* spawn = client->GetPlayer()->GetTarget();
			if(spawn){
				const char* type = "NPC";
				if(spawn->IsObject())
					type = "Object";
				else if(spawn->IsSign())
					type = "Sign";
				else if(spawn->IsWidget())
					type = "Widget";
				else if(spawn->IsGroundSpawn())
					type = "GroundSpawn";
				client->Message(CHANNEL_COLOR_YELLOW, "Name: %s, %s ID: %u", spawn->GetName(), type, spawn->GetDatabaseID());
				client->Message(CHANNEL_COLOR_YELLOW, "Last Name: %s, Sub-Title: %s, Prefix: %s, Suffix: %s", spawn->GetLastName(), spawn->GetSubTitle(), spawn->GetPrefixTitle(), spawn->GetSuffixTitle());
				client->Message(CHANNEL_COLOR_YELLOW, "Spawn Location ID: %u, Spawn Group ID: %u", spawn->GetSpawnLocationID(), spawn->GetSpawnGroupID());
				client->Message(CHANNEL_COLOR_YELLOW, "Faction ID: %u, Merchant ID: %u, Transporter ID: %u", spawn->GetFactionID(), spawn->GetMerchantID(), spawn->GetTransporterID());
				client->Message(CHANNEL_COLOR_YELLOW, "Grid ID: %u", spawn->GetLocation());
				client->Message(CHANNEL_COLOR_YELLOW, "Race: %i, Class: %i, Gender: %i", spawn->GetRace(), spawn->GetAdventureClass(), spawn->GetGender());
				client->Message(CHANNEL_COLOR_YELLOW, "Level: %i, HP: %u, Power: %u", spawn->GetLevel(), spawn->GetHP(), spawn->GetPower());
				client->Message(CHANNEL_COLOR_YELLOW, "Respawn Time: %u (sec), X: %f, Y: %f, Z: %f Heading: %f", spawn->GetRespawnTime(), spawn->GetX(), spawn->GetY(), spawn->GetZ(), spawn->GetHeading());
				client->Message(CHANNEL_COLOR_YELLOW, "Collision Radius: %i, Size: %i, Difficulty: %i, Heroic: %i", spawn->GetCollisionRadius(), spawn->GetSize(), spawn->GetEncounterLevel(), spawn->GetHeroic());
				client->Message(CHANNEL_COLOR_YELLOW, "Targetable: %i, Show Name: %i, Attackable: %i, Show Level: %i", spawn->GetTargetable(), spawn->GetShowName(), spawn->GetAttackable(), spawn->GetShowLevel());
				client->Message(CHANNEL_COLOR_YELLOW, "Show Command Icon: %i, Display Hand Icon: %i", spawn->GetShowCommandIcon(), spawn->GetShowHandIcon());
				if(spawn->IsEntity()){
					client->Message(CHANNEL_COLOR_YELLOW, "Facial Hair Type: %i, Hair Type: %i, Chest Type: %i, Legs Type: %i", ((Entity*)spawn)->GetFacialHairType(), ((Entity*)spawn)->GetHairType(), ((Entity*)spawn)->GetChestType(), ((Entity*)spawn)->GetLegsType());
					client->Message(CHANNEL_COLOR_YELLOW, "Soga Facial Hair Type: %i, Soga Hair Type: %i, Wing Type: %i", ((Entity*)spawn)->GetSogaFacialHairType(), ((Entity*)spawn)->GetSogaHairType(), ((Entity*)spawn)->GetWingType());
				}
				client->Message(CHANNEL_COLOR_YELLOW, "Model Type: %i, Soga Race Type: %i", spawn->GetModelType(), spawn->GetSogaModelType());
				client->Message(CHANNEL_COLOR_YELLOW, "Primary Command Type: %u, Secondary Command Type: %u", spawn->GetPrimaryCommandListID(), spawn->GetSecondaryCommandListID());
				client->Message(CHANNEL_COLOR_YELLOW, "Visual State: %i, Action State: %i, Mood State: %i, Initial State: %i, Activity Status: %i", spawn->GetVisualState(), spawn->GetActionState(), spawn->GetMoodState(), spawn->GetInitialState(), spawn->GetActivityStatus());
				client->Message(CHANNEL_COLOR_YELLOW, "Emote State: %i, Pitch: %f, Roll: %f, Hide Hood: %i", spawn->GetEmoteState(), spawn->GetPitch(), spawn->GetRoll(), spawn->appearance.hide_hood);
				if (spawn->IsNPC())
					client->Message(CHANNEL_COLOR_YELLOW, "Randomize: %u", ((NPC*)spawn)->GetRandomize());
				if (spawn->IsPlayer())
					client->Message(CHANNEL_COLOR_YELLOW, "Alignment: %i", ((Player*)spawn)->GetAlignment());
			}
			else{
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Syntax: /spawn details ");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "This will display information about the currently selected spawn");
			}
			break;
								   }
		case COMMAND_SPAWN_TARGET:{
			if(sep && sep->arg[0][0] && sep->IsNumber(0)){
				int32 spawn_id = atoul(sep->arg[0]);
				int16 response = client->GetCurrentZone()->SetSpawnTargetable(spawn_id);
				client->Message(CHANNEL_COLOR_YELLOW, "%i spawn(s) in the current zone were reset to targetable.", response);
			}
			else if(sep && sep->arg[0][0] && sep->arg[1][0] && sep->IsNumber(1) && ToLower(string(sep->arg[0])) == "radius"){
				float distance = atof(sep->arg[1]);
				int16 response = client->GetCurrentZone()->SetSpawnTargetable(client->GetPlayer(), distance);
				client->Message(CHANNEL_COLOR_YELLOW, "%i spawn(s) in the current zone were reset to targetable.", response);
			}
			else{
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Syntax: /spawn target [spawn id]");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "This will set the given spawn as targetable.  Used to change a spawn if it was set to untargetable.");
			}
			break;
								  }
		case COMMAND_SPAWN_SET:
			{
			Spawn* spawn = client->GetPlayer()->GetTarget();
			sint16 set_type = -1; 
			string type_str;

			if (spawn)
			{
				// check if parameters are (location or list or not player and not a 2nd param), and that there is at least 1 value
				if(sep && ((sep->arg[0][0] && ToLower(string(sep->arg[0])) == "location") || (sep->arg[0][0] && ToLower(string(sep->arg[0])) == "list") || (spawn && spawn->IsPlayer() == false && sep->arg[1][0])) && spawn_set_values.count(ToLower(string(sep->arg[0]))) == 1)
				{
					// set the type, which will be 0 if location or list or invalid
					set_type = spawn_set_values[ToLower(string(sep->arg[0]))];
				}

				if(set_type > 0)
				{
					// check if spawn set is NOT a char update, or not location, or isn't a number
					if(!(set_type >= SPAWN_SET_VALUE_PREFIX) && !(set_type <= SPAWN_SET_VALUE_LASTNAME) && set_type != SPAWN_SET_VALUE_NAME && ((set_type < SPAWN_SET_VALUE_SPAWN_SCRIPT) || (set_type > SPAWN_SET_VALUE_SUB_TITLE)) && set_type != SPAWN_SET_VALUE_LOCATION && sep->IsNumber(1) == false)
					{
						client->SimpleMessage(CHANNEL_COLOR_RED, "Invalid value for set command.");
					}
					else
					{
						string name = string(spawn->GetName());
						if(SetSpawnCommand(client, spawn, set_type, sep->arg[1]))
						{
							if(set_type == SPAWN_SET_VALUE_NAME)
							{
								client->SimpleMessage(CHANNEL_COLOR_YELLOW, "New name will not be effective until zone reload.");
							}
							else if(set_type == SPAWN_SET_VALUE_LOCATION)
							{
								spawn->SetLocation(client->GetPlayer()->GetLocation());
								client->Message(CHANNEL_COLOR_YELLOW, "Successfully set '%s' to '%u' for spawn '%s'", sep->arg[0], client->GetPlayer()->GetLocation(), name.c_str());
							}
							else
							{
								client->Message(CHANNEL_COLOR_YELLOW, "Successfully set '%s' to '%s' for spawn '%s'", sep->arg[0], sep->arg[1], name.c_str());
							}

							if(set_type != SPAWN_SET_VALUE_FACTION)
							{
								client->GetCurrentZone()->ApplySetSpawnCommand(client, spawn, set_type, sep->arg[1]);
							}
						
							if((set_type >= SPAWN_SET_VALUE_RESPAWN && set_type <=SPAWN_SET_VALUE_LOCATION) || (set_type >= SPAWN_SET_VALUE_EXPIRE && set_type <=SPAWN_SET_VALUE_Z_OFFSET) || (set_type == SPAWN_SET_VALUE_PITCH || set_type == SPAWN_SET_VALUE_ROLL))
							{
								if(spawn->GetSpawnLocationID() > 0 && database.UpdateSpawnLocationSpawns(spawn))
								{
									client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Successfully saved spawn information.");
								}
								else if(spawn->GetSpawnLocationID() > 0)
								{
									client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Error saving spawn information, see console window for details.");				
								}
							}
							else
							{
								if(spawn->GetDatabaseID() > 0 && database.SaveSpawnInfo(spawn))
								{
									client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Successfully saved spawn.");
								}
								else if(spawn->GetDatabaseID() > 0)
								{
									client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Error saving spawn, see console window for details.");
								}
							}
						}
					}
				}
				else if(set_type == 0)
				{
					// /spawn set list - lists all possible attributes that can be changed with this command, 10 per line.
					map<string, int8>::iterator itr;
					int i=0;
					string list;
					for(itr = spawn_set_values.begin(); itr != spawn_set_values.end(); itr++, i++)
					{
						if(i==10)
						{
							client->SimpleMessage(CHANNEL_COLOR_YELLOW, list.c_str());
							i = 0;
						}

						if(i>0)
						{
							list.append(", ").append(itr->first);
						}
						else
						{
							list = itr->first;
						}
					}

					if(list.length() > 0)
					{
						// if 1 or more, display in client.
						client->SimpleMessage(CHANNEL_COLOR_YELLOW, list.c_str());
					}
				}
				else
				{
					// syntax fail
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Syntax: /spawn set [type] [value]");
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "This command is used to change various settings for the targeted NPC or Object.");
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "For a list of changeable settings use /spawn set list");
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Note: /spawn set location does not require a value. The client's current location is used.");
				}
			}
			break;
			}
		case COMMAND_SPAWN_REMOVE:{
			Spawn* spawn = client->GetPlayer()->GetTarget();
			if(spawn && !spawn->IsPlayer()){
				if(spawn->GetSpawnLocationID() > 0){
					if(database.RemoveSpawnFromSpawnLocation(spawn)){
						bool delete_spawn = false;
						if(sep && sep->arg[0][0] && sep->IsNumber(0)){
							if(atoi(sep->arg[0]) == 1)
								delete_spawn = true;
						}
						if(delete_spawn)
							client->GetCurrentZone()->RemoveSpawn(spawn, true, false);
						else
							spawn->SetSpawnLocationID(0);
						client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Successfully removed spawn from zone");
					}
					else
						client->SimpleMessage(CHANNEL_COLOR_RED, "Error removing spawn, see console window for details.");
				}
				else
					client->SimpleMessage(CHANNEL_COLOR_RED, "This spawn does not have a spawn group associated with it");
			}
			else{
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Syntax: /spawn remove");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "This command is used for removing the targeted NPC or Object from the zone.");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Optionally, you can supply a 1 after the command to despawn it as well.  IE /spawn remove 1");
			}
			break;
								  }
		case COMMAND_SPAWN_LIST:{
			if(sep && sep->arg[0][0]){
				vector<string>* results = database.GetSpawnNameList(sep->argplus[0]);
				vector<string>::iterator itr;
				if(results){
					for(itr=results->begin();itr!=results->end();itr++){
						client->SimpleMessage(CHANNEL_COLOR_YELLOW, (*itr).c_str());
					}
					safe_delete(results);
				}
				else
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "No matches found. ");
			}
			else{
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Syntax: /spawn list [name]");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Name can be a partial match.");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Ex: /spawn list Qeynos Guard");
			}
			break;
								}
		case COMMAND_SPAWN_ADD:{
			Spawn* spawn = client->GetPlayer()->GetTarget();
			if(spawn && sep && (sep->IsNumber(0) || strncasecmp(sep->arg[0], "new", 3) == 0)){
				if(spawn->GetSpawnLocationID() > 0){
					client->Message(CHANNEL_COLOR_RED, "This spawn already has a spawn group id of %u, use /spawn remove to reassign it", spawn->GetSpawnLocationID());
					break;
				}
				if(spawn->GetDatabaseID() == 0){
					if(database.SaveSpawnInfo(spawn)) {
						char spawn_type[32];
						memset(spawn_type, 0, sizeof(spawn_type));
						if (spawn->IsNPC())
							strncpy(spawn_type, "NPC", sizeof(spawn_type) - 1);
						else if (spawn->IsObject())
							strncpy(spawn_type, "Object", sizeof(spawn_type) - 1);
						else if (spawn->IsSign())
							strncpy(spawn_type, "Sign", sizeof(spawn_type) - 1);
						else if (spawn->IsGroundSpawn())
							strncpy(spawn_type, "GroundSpawn", sizeof(spawn_type) - 1);
						else
							strncpy(spawn_type, "Unknown", sizeof(spawn_type) - 1);
						client->Message(CHANNEL_COLOR_YELLOW, "Successfully saved spawn information with a %s id of %u", spawn_type, spawn->GetDatabaseID());
					}
					else
						client->SimpleMessage(CHANNEL_COLOR_RED, "Error saving spawn information, see console window for details.");
				}
				int32 spawn_group_id = 0;
				if(strncasecmp(sep->arg[0], "new", 3) == 0)
					spawn_group_id = database.GetNextSpawnLocation();
				else
					spawn_group_id = atol(sep->arg[0]);
				int8 percent = 100;
				if(sep->arg[1] && sep->IsNumber(1))
					percent = atoi(sep->arg[1]);
				spawn->SetSpawnLocationID(spawn_group_id);
				float x_offset = database.GetSpawnLocationPlacementOffsetX(spawn->GetSpawnLocationID());
				float y_offset = database.GetSpawnLocationPlacementOffsetY(spawn->GetSpawnLocationID());
				float z_offset = database.GetSpawnLocationPlacementOffsetZ(spawn->GetSpawnLocationID());
				if(database.SaveSpawnEntry(spawn, "", percent, x_offset, y_offset, z_offset))
					client->Message(CHANNEL_COLOR_YELLOW, "Successfully saved spawn location with a spawn group of %u", spawn->GetSpawnLocationID());
				else
					client->SimpleMessage(CHANNEL_COLOR_RED, "Error saving spawn location, see console window for details.");	
			}
			else{
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Syntax: /spawn add [spawn group id] (percentage)");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "This command is used for adding the targeted NPC or Object to the database.");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You can substitute new for [spawn group id] to create a new one.");
			}
			break;
							   }
		case COMMAND_SPAWN:{
			int32 id = 0;
			Spawn* spawn = 0;
			if(sep && sep->arg[0] && sep->IsNumber(0)){
				id = atol(sep->arg[0]);
				spawn = client->GetCurrentZone()->GetSpawn(id);
			}
			if(id > 0 && spawn && spawn->appearance.name[0] == 0)
				id = 0;
			if(!id || !spawn){
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Syntax: /spawn [spawn id] (x) (y) (z) (heading) (location)");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "All parameters are optional except the id.  The spawn id must be in the database.");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Ex: /spawn 1 0 0 0 0");
				safe_delete(sep);
				return;
			}
			if(sep && sep->arg[1][0]){
				float x = atof(sep->arg[1]);
				spawn->appearance.pos.X = x;
			}
			else
				spawn->SetX(client->GetPlayer()->GetX(), false);
			if(sep && sep->arg[2][0]){
				float y = atof(sep->arg[2]);
				spawn->appearance.pos.Y = y;
			}
			else
				spawn->SetY(client->GetPlayer()->GetY(), false);
			if(sep && sep->arg[3][0]){
				float z = atof(sep->arg[3]);
				spawn->appearance.pos.Z = z;
			}
			else
				spawn->SetZ(client->GetPlayer()->GetZ(), false);
			if(sep && sep->arg[4][0]){
				float heading = atof(sep->arg[4]);
				spawn->SetHeading(heading);
			}
			else
				spawn->SetHeading(client->GetPlayer()->GetHeading(), false);
			spawn->SetSpawnOrigX(spawn->GetX());
			spawn->SetSpawnOrigY(spawn->GetY());
			spawn->SetSpawnOrigZ(spawn->GetZ());
			spawn->SetSpawnOrigHeading(spawn->GetHeading());
			if(sep && sep->arg[5][0])
				spawn->appearance.pos.grid_id = atol(sep->arg[5]);
			else
				spawn->appearance.pos.grid_id = client->GetPlayer()->appearance.pos.grid_id;

			if(spawn->IsNPC() && spawn->GetTotalHP() == 0){
				spawn->SetTotalHP(spawn->GetLevel() * 15);
				spawn->SetHP(spawn->GetTotalHP());
			}
			if(spawn->GetTotalPower() == 0){
				spawn->SetTotalPower(spawn->GetLevel() * 15);
				spawn->SetPower(spawn->GetTotalPower());
			}
			const char* script = world.GetSpawnScript(id);
			if(script && lua_interface && lua_interface->GetSpawnScript(script) != 0)
				spawn->SetSpawnScript(string(script));
			client->GetCurrentZone()->AddSpawn(spawn);
			if(spawn->IsNPC())
				spawn->GetZone()->AddLoot((NPC*)spawn);
			spawn->GetZone()->CallSpawnScript(spawn, SPAWN_SCRIPT_SPAWN);
			LogWrite(COMMAND__INFO, 0, "Command", "Received spawn command - Parms: %s", command_parms->data.c_str());
			break;
						   }
		case COMMAND_ADMINFLAG:
			{
				if(sep && sep->arg[0]){
					sint16 tmp_status = database.GetCharacterAdminStatus(sep->arg[0]);
					sint16 new_status = atoi(sep->arg[1]);
					if(tmp_status == -10)
						client->SimpleMessage(CHANNEL_ERROR,"Unable to flag character.  Reason: Character does not exist.");
					else if(tmp_status >= client->GetAdminStatus())
						client->SimpleMessage(CHANNEL_ERROR,"Unable to flag character.  Reason: Character has same or higher level status.");
					else if (new_status > client->GetAdminStatus())
						client->SimpleMessage(CHANNEL_ERROR, "Unable to flag character.  Reason: New status is higher then your status.");
					else{
						Client* client2 = client->GetCurrentZone()->GetClientByName(sep->arg[0]);
						if (!client2)
							client2 = zone_list.GetClientByCharName(sep->arg[0]);
						
						if(database.UpdateAdminStatus(sep->arg[0],new_status)) {
							client->Message(CHANNEL_COLOR_YELLOW,"Character status updated to %i for %s.",new_status,sep->arg[0]);
							if (client2) {
								client2->SetAdminStatus(new_status);
								client2->Message(CHANNEL_COLOR_YELLOW, "%s has set your admin status to %i.", client->GetPlayer()->GetName(), new_status);
							}
						}
						else
							client->SimpleMessage(CHANNEL_ERROR,"Unable to flag character.  Unknown reason.");
					}
				}else{
					client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage: /flag {name} {new_status}");
					client->SimpleMessage(CHANNEL_COLOR_YELLOW," Standard User: 0");
					client->SimpleMessage(CHANNEL_COLOR_YELLOW," Admin User: 100");
				}
				break;
			}
		case COMMAND_CANNEDEMOTE:{
				client->GetCurrentZone()->HandleEmote(client, command_parms->data);
				break;
		}
		case COMMAND_BROADCAST: {
			if (sep && sep->arg[0])
				zone_list.HandleGlobalBroadcast(sep->argplus[0]);
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /broadcast {message}");
			break;
								}
		case COMMAND_ANNOUNCE: {
			if (sep && sep->arg[0])
				zone_list.HandleGlobalAnnouncement(sep->argplus[0]);
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /announce {message}");
			break;
							   }
		case COMMAND_RELOAD_ITEMS:{
			LogWrite(COMMAND__INFO, 0, "Command", "Reloading items..");
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Started Reloading items (this might take a few minutes...)");
			database.ReloadItemList();
			database.LoadMerchantInformation();
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Finished Reloading items.");
			break;
								  }
		case COMMAND_ENABLE_ABILITY_QUE:{
			EQ2Packet* app = client->GetPlayer()->GetSpellBookUpdatePacket(client->GetVersion());
			if(app)
				client->QueuePacket(app);
			break;
										}
		case COMMAND_ITEMSEARCH:{
			PacketStruct* packet = configReader.getStruct("WS_StartBroker", client->GetVersion());
			if(packet){
				packet->setDataByName("spawn_id", client->GetPlayer()->GetIDWithPlayerSpawn(client->GetPlayer()));
				//packet->setDataByName("unknown", 1);
				packet->setDataByName("unknown2", 5, 0); 
				packet->setDataByName("unknown2", 20, 1);
				packet->setDataByName("unknown2", 58, 3);
				packet->setDataByName("unknown2", 40, 4);
				client->QueuePacket(packet->serialize());
				PacketStruct* packet2 = configReader.getStruct("WS_BrokerBags", client->GetVersion());
				if(packet2){
					packet2->setDataByName("char_id", client->GetCharacterID());
					client->QueuePacket(packet2->serialize()); //send this for now, needed to properly clear data
					safe_delete(packet2);
				}
				safe_delete(packet);
			}
			break;
		}
		case COMMAND_ANIMTEST:{
			PacketStruct* command_packet = configReader.getStruct("WS_CannedEmote", client->GetVersion());
			if(command_packet){
				command_packet->setDataByName ( "spawn_id" , client->GetPlayer()->GetIDWithPlayerSpawn(client->GetPlayer()));

				int animID = 1;

				if(sep && sep->arg[0] && sep->IsNumber(0))
					animID = atoi(sep->arg[0]);

				VisualState* vs = NULL;
				if(animID == 0)
				{
					vs = visual_states.FindVisualState(sep->arg[0]);
				}


				char msg[128];
				sprintf(msg,"Animation Test ID: %i",animID);
				command_packet->setMediumStringByName ( "emote_msg" , msg );

				if(vs != NULL)
					command_packet->setDataByName ( "anim_type", vs->GetID ( ) );
				else
					command_packet->setDataByName ( "anim_type", animID );

				command_packet->setDataByName ( "unknown0", 0 );
				EQ2Packet* outapp = command_packet->serialize();
				client->QueuePacket(outapp);
				safe_delete(command_packet);
			}
			break;
		}
		case COMMAND_KICK:
			{
				if( sep == 0 || sep->arg[0] == 0)
				{
					client->SimpleMessage(CHANNEL_COLOR_RED, "/kick [name]");
				}
				else
				{
					Client* kickClient = zone_list.GetClientByCharName(string(sep->arg[0]));

					if ( kickClient == client )
					{
						client->Message(CHANNEL_COLOR_RED, "You can't kick yourself!");
						break;
					}
					else if(kickClient != NULL)
					{
						sint16 maxStatus = database.GetHighestCharacterAdminStatus(kickClient->GetAccountID());

						if ( maxStatus >= client->GetAdminStatus( ) || kickClient->GetAdminStatus() >= client->GetAdminStatus() )
						{
							client->Message(CHANNEL_COLOR_RED,"Don't even think about it...");
							break;
						}

						client->Message(CHANNEL_COLOR_RED, "Kicking %s...",sep->arg[0]);

						kickClient->Disconnect();
					}
					else
					{
						client->Message(CHANNEL_COLOR_RED, "Could not find %s.",sep->arg[0]);
					}
				}

				break;
			}
		case COMMAND_LOCK:
			{
				if( sep != NULL && sep->arg[0] != NULL && sep->IsNumber(0)){
					int worldLocked = atoi(sep->arg[0]);
					net.world_locked = worldLocked;
					if ( worldLocked )
						client->Message(CHANNEL_COLOR_YELLOW,"World server has been locked.");
					else
						client->Message(CHANNEL_COLOR_YELLOW,"World server has been unlocked.");
				}
				else
					client->SimpleMessage(CHANNEL_COLOR_RED, "/lock [0/1]");

				break;
		}
		case COMMAND_BAN:{
				if( sep == 0 || sep->arg[0] == 0 || (sep->arg[1][0] != 0 && !sep->IsNumber(1) ) )
				{
					client->SimpleMessage(CHANNEL_COLOR_RED, "/ban [name] [permanent:0/1]");
				}
				else
				{
					Client* kickClient = zone_list.GetClientByCharName(sep->arg[0]);

					if ( kickClient == client )
					{
						client->Message(CHANNEL_COLOR_RED, "You can't ban yourself!");
						break;
					}
					else if(kickClient != NULL)
					{
						sint16 maxStatus = database.GetHighestCharacterAdminStatus(kickClient->GetAccountID());

						if ( maxStatus > client->GetAdminStatus( ) || 
							client->GetAdminStatus ( ) > kickClient->GetAdminStatus ( ) )
						{
							client->Message(CHANNEL_COLOR_RED,"Don't even think about it...");
							break;
						}

						client->Message(CHANNEL_COLOR_RED, "Kicking & Banning %s...",sep->arg[0]);

						int perm = atol(sep->arg[1]);
						if ( perm == 1 )
							database.UpdateAdminStatus(sep->arg[0],-2);
						else
							database.UpdateAdminStatus(sep->arg[0],-1);
						kickClient->Disconnect();
					}
					else
					{
						client->Message(CHANNEL_COLOR_RED, "Could not find %s.",sep->arg[0]);
					}
				}
				break;
		}
		case COMMAND_SET_COMBAT_VOICE:{
			int32 value = 0;
			if(sep && sep->arg[0] && sep->IsNumber(0))
				value = atoi(sep->arg[0]);
			client->GetPlayer()->SetCombatVoice(value);
			break;
		}
		case COMMAND_SET_EMOTE_VOICE:{
			int32 value = 0;
			if(sep && sep->arg[0] && sep->IsNumber(0))
				value = atoi(sep->arg[0]);
			client->GetPlayer()->SetEmoteVoice(value);
			break;
		}
		case COMMAND_GIVEITEM:{

			if(sep && sep->arg[0][0] && sep->arg[0][1] && sep->IsNumber(1)){
				Client* itemToClient = zone_list.GetClientByCharName(sep->arg[0]);

				if ( itemToClient == NULL )
					client->Message(CHANNEL_COLOR_YELLOW,"Could not find %s.",sep->arg[0]);
				else
				{
					int32 item_id = atol(sep->arg[1]);
					client->Message(CHANNEL_COLOR_YELLOW,"Gave %s item id %i.",sep->arg[0],item_id);
					itemToClient->AddItem(item_id);
				}
			}
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage: /giveitem [name] [item_id]");

			break;
							  }

		case COMMAND_REPORT_BUG			: { Command_ReportBug(client, sep); break; }
		case COMMAND_INVENTORY			: { Command_Inventory(client, sep, command); break; }
		case COMMAND_WEAPONSTATS		: { Command_WeaponStats(client); break; }
		case COMMAND_SKILL				: 
		case COMMAND_SKILL_ADD			:
		case COMMAND_SKILL_REMOVE		:
		case COMMAND_SKILL_LIST			: { Command_Skills(client, sep, command->handler); break; }
		case COMMAND_ZONE_SET			: { Command_ZoneSet(client, sep); break; }
		case COMMAND_ZONE_DETAILS		: { Command_ZoneDetails(client, sep); break; }
		case COMMAND_ENTITYCOMMAND		: 
		case COMMAND_ENTITYCOMMAND_LIST	: { Command_EntityCommand(client, sep, command->handler); break; }
		case COMMAND_MERCHANT			: 
		case COMMAND_MERCHANT_LIST		: { Command_Merchant(client, sep, command->handler); break; }
		case COMMAND_APPEARANCE			: 
		case COMMAND_APPEARANCE_LIST	: { Command_Appearance(client, sep, command->handler); break; }
		case COMMAND_TRACK				: { Command_Track(client); break; }
		case COMMAND_DISTANCE			: { Command_Distance(client); break; }
		case COMMAND_INSPECT_PLAYER		: { Command_InspectPlayer(client, sep); break; }
		case COMMAND_ZONE_SAFE			: { Command_ZoneSafeCoords(client, sep); break; }
		case COMMAND_GUILDSAY			: { Command_GuildSay(client, sep); break; }
		case COMMAND_OFFICERSAY			: { Command_OfficerSay(client, sep); break; }
		case COMMAND_SET_GUILD_MEMBER_NOTE	: { Command_SetGuildMemberNote(client, sep); break; }
		case COMMAND_SET_GUILD_OFFICER_NOTE	: { Command_SetGuildOfficerNote(client, sep); break; }
		case COMMAND_GUILD				: { Command_Guild(client, sep); break; }
		case COMMAND_CREATE_GUILD		: { Command_CreateGuild(client); break; }
		case COMMAND_GUILDS				: { Command_Guilds(client); break; }
		case COMMAND_GUILDS_ADD			: { Command_GuildsAdd(client, sep); break; }
		case COMMAND_GUILDS_CREATE		: { Command_GuildsCreate(client, sep); break; }
		case COMMAND_GUILDS_DELETE		: { Command_GuildsDelete(client, sep); break; }
		case COMMAND_GUILDS_LIST		: { Command_GuildsList(client); break; }
		case COMMAND_GUILDS_REMOVE		: { Command_GuildsRemove(client, sep); break; }
		case COMMAND_CLAIM				: { Command_Claim(client, sep); break; }
		case COMMAND_CLEAR_ALL_QUEUED	: { Command_ClearAllQueued(client); break; }
		case COMMAND_LOCATION			: { Command_Location(client); break; }
		case COMMAND_LOCATION_ADD		: { Command_LocationAdd(client, sep); break; }
		case COMMAND_LOCATION_CREATE	: { Command_LocationCreate(client, sep); break; }
		case COMMAND_LOCATION_DELETE	: { Command_LocationDelete(client, sep); break; }
		case COMMAND_LOCATION_LIST		: { Command_LocationList(client, sep); break; }
		case COMMAND_LOCATION_REMOVE	: { Command_LocationRemove(client, sep); break; }
		case COMMAND_GRID				: { Command_Grid(client); break; }
		case COMMAND_TRY_ON				: { Command_TryOn(client, sep); break; }
		case COMMAND_IRC				: { Command_IRC(client, sep); break; }
		case COMMAND_RANDOMIZE			: { Command_Randomize(client, sep); break; }
		case COMMAND_AFK				: { Command_AFK(client, sep); break; }
		case COMMAND_SHOW_CLOAK			: { Command_ShowCloak(client, sep); break; }
		case COMMAND_SHOW_HELM			: { Command_ShowHelm(client, sep); break; }
		case COMMAND_SHOW_HOOD			: { Command_ShowHood(client, sep); break; }
		case COMMAND_SHOW_HOOD_OR_HELM	: { Command_ShowHoodHelm(client, sep); break; }
		case COMMAND_SHOW_RANGED		: { Command_ShowRanged(client, sep); break; }
		case COMMAND_STOP_DRINKING		: { Command_StopDrinking(client); break; }
		case COMMAND_STOP_EATING		: { Command_StopEating(client); break; }
		case COMMAND_TOGGLE_ANONYMOUS	: { Command_Toggle_Anonymous(client); break; }
		case COMMAND_TOGGLE_AUTOCONSUME	: { Command_Toggle_AutoConsume(client, sep); break; }
		case COMMAND_TOGGLE_BONUS_EXP	: { Command_Toggle_BonusXP(client); break; }
		case COMMAND_TOGGLE_COMBAT_EXP	: { Command_Toggle_CombatXP(client); break; }
		case COMMAND_TOGGLE_GM_HIDE		: { Command_Toggle_GMHide(client); break; }
		case COMMAND_TOGGLE_GM_VANISH	: { Command_Toggle_GMVanish(client); break; }
		case COMMAND_TOGGLE_ILLUSIONS	: { Command_Toggle_Illusions(client, sep); break; }
		case COMMAND_TOGGLE_LFG			: { Command_Toggle_LFG(client); break; }
		case COMMAND_TOGGLE_LFW			: { Command_Toggle_LFW(client); break; }
		case COMMAND_TOGGLE_QUEST_EXP	: { Command_Toggle_QuestXP(client); break; }
		case COMMAND_TOGGLE_ROLEPLAYING	: { Command_Toggle_Roleplaying(client); break; }
		case COMMAND_TOGGLE_DUELS		: { Command_Toggle_Duels(client); break; }
		case COMMAND_TOGGLE_TRADES		: { Command_Toggle_Trades(client); break; }
		case COMMAND_TOGGLE_GUILDS		: { Command_Toggle_Guilds(client); break; }
		case COMMAND_TOGGLE_GROUPS		: { Command_Toggle_Groups(client); break; }
		case COMMAND_TOGGLE_RAIDS		: { Command_Toggle_Raids(client); break; }
		case COMMAND_TOGGLE_LON			: { Command_Toggle_LON(client); break; }
		case COMMAND_TOGGLE_VCINVITE	: { Command_Toggle_VoiceChat(client); break; }
		case COMMAND_CANCEL_MAINTAINED	: { Command_CancelMaintained(client, sep); break; }
		case COMMAND_MOTD				: { Command_MOTD(client); break; }
		case COMMAND_RANDOM				: { Command_Random(client, sep); break; }
		case COMMAND_CREATE				: { Command_Create(client, sep); break; }
		case COMMAND_CREATEFROMRECIPE	: { Command_CreateFromRecipe(client, sep); break; }
		case COMMAND_TITLE				: { Command_Title(client); break; }
		case COMMAND_TITLE_LIST			: { Command_TitleList(client); break; }
		case COMMAND_TITLE_SETPREFIX	: { Command_TitleSetPrefix(client, sep); break; }
		case COMMAND_TITLE_SETSUFFIX	: { Command_TitleSetSuffix(client, sep); break; }
		case COMMAND_TITLE_FIX			: { Command_TitleFix(client, sep); break; }
		case COMMAND_LANGUAGES			: { Command_Languages(client, sep); break; }
		case COMMAND_SET_LANGUAGE		: { Command_SetLanguage(client, sep); break; }
		case COMMAND_FOLLOW				: { Command_Follow(client, sep); break; }
		case COMMAND_STOP_FOLLOW		: { Command_StopFollow(client, sep); break; }
		case COMMAND_LASTNAME			: { Command_LastName(client, sep); break; }
		case COMMAND_CONFIRMLASTNAME	: { Command_ConfirmLastName(client, sep); break; }
		case COMMAND_PET				: { Command_Pet(client, sep); break; }
		case COMMAND_PETNAME			: { Command_PetName(client, sep); break; }
		case COMMAND_NAME_PET			: { Command_NamePet(client, sep); break; }
		case COMMAND_RENAME				: { Command_Rename(client, sep); break; }
		case COMMAND_CONFIRMRENAME		: { Command_ConfirmRename(client, sep); break; }
		case COMMAND_PETOPTIONS			: { Command_PetOptions(client, sep); break; }
		case COMMAND_START_TRADE		: { Command_TradeStart(client, sep); break; }
		case COMMAND_ACCEPT_TRADE		: { Command_TradeAccept(client, sep); break; }
		case COMMAND_REJECT_TRADE		: { Command_TradeReject(client, sep); break; }
		case COMMAND_CANCEL_TRADE		: { Command_TradeCancel(client, sep); break; }
		case COMMAND_SET_TRADE_COIN		: { Command_TradeSetCoin(client, sep); break; }
		case COMMAND_ADD_TRADE_COPPER	: { Command_TradeAddCoin(client, sep, COMMAND_ADD_TRADE_COPPER); break; }
		case COMMAND_ADD_TRADE_SILVER	: { Command_TradeAddCoin(client, sep, COMMAND_ADD_TRADE_SILVER); break; }
		case COMMAND_ADD_TRADE_GOLD		: { Command_TradeAddCoin(client, sep, COMMAND_ADD_TRADE_GOLD); break; }
		case COMMAND_ADD_TRADE_PLAT		: { Command_TradeAddCoin(client, sep, COMMAND_ADD_TRADE_PLAT); break; }
		case COMMAND_REMOVE_TRADE_COPPER: { Command_TradeRemoveCoin(client, sep, COMMAND_REMOVE_TRADE_COPPER); break; }
		case COMMAND_REMOVE_TRADE_SILVER: { Command_TradeRemoveCoin(client, sep, COMMAND_REMOVE_TRADE_SILVER); break; }
		case COMMAND_REMOVE_TRADE_GOLD	: { Command_TradeRemoveCoin(client, sep, COMMAND_REMOVE_TRADE_GOLD); break; }
		case COMMAND_REMOVE_TRADE_PLAT	: { Command_TradeRemoveCoin(client, sep, COMMAND_REMOVE_TRADE_PLAT); break; }
		case COMMAND_ADD_TRADE_ITEM		: { Command_TradeAddItem(client, sep); break; }
		case COMMAND_REMOVE_TRADE_ITEM	: { Command_TradeRemoveItem(client, sep); break; }
		case COMMAND_ACCEPT_ADVANCEMENT	: { Command_AcceptAdvancement(client, sep); break; }
		case COMMAND_DUEL				: { Command_Duel(client, sep); break; }
		case COMMAND_DUELBET			: { Command_DuelBet(client, sep); break; }
		case COMMAND_DUEL_ACCEPT		: { Command_DuelAccept(client, sep); break; }
		case COMMAND_DUEL_DECLINE		: { Command_DuelDecline(client, sep); break; }
		case COMMAND_DUEL_SURRENDER		: { Command_DuelSurrender(client, sep); break; }
		case COMMAND_DUEL_TOGGLE		: { Command_DuelToggle(client, sep); break; }
		case COMMAND_SPAWN_TEMPLATE		: { Command_SpawnTemplate(client, sep); break; }

		case COMMAND_MODIFY				: { Command_Modify(client); break; }
		case COMMAND_MODIFY_CHARACTER	: { Command_ModifyCharacter(client, sep); break; }
		case COMMAND_MODIFY_QUEST		: { Command_ModifyQuest(client, sep); break; }
		case COMMAND_MODIFY_FACTION		: { Command_ModifyFaction(client, sep); break; }
		case COMMAND_MODIFY_GUILD		: { Command_ModifyGuild(client, sep); break; }
		case COMMAND_MODIFY_ITEM		: { Command_ModifyItem(client, sep); break; }
		case COMMAND_MODIFY_SKILL		: { Command_ModifySkill(client, sep); break; }
		case COMMAND_MODIFY_SPAWN		: { Command_ModifySpawn(client, sep); break; }
	    case COMMAND_MODIFY_SPELL		: { Command_ModifySpell(client, sep); break; }
		case COMMAND_MODIFY_ZONE		: { Command_ModifyZone(client, sep); break; }

		case COMMAND_JOIN_CHANNEL		: { Command_JoinChannel(client, sep); break;}
		case COMMAND_JOIN_CHANNEL_FROM_LOAD: { Command_JoinChannelFromLoad(client, sep); break;}
		case COMMAND_TELL_CHANNEL		: { Command_TellChannel(client, sep); break;}
		case COMMAND_LEAVE_CHANNEL		: { Command_LeaveChannel(client, sep); break;}
		case COMMAND_WHO_CHANNEL		: { Command_WhoChannel(client, sep); break;}
		case COMMAND_RAIN				: { Command_Rain(client, sep); break; }
		case COMMAND_WIND				: { Command_Wind(client, sep); break; }
		case COMMAND_WEATHER			: { Command_Weather(client, sep); break; }
		case COMMAND_FROM_MERCHANT		: { Command_SendMerchantWindow(client, sep); break; }
		case COMMAND_TO_MERCHANT		: { Command_SendMerchantWindow(client, sep, true); break; }
		case COMMAND_SELECT				: { Command_Select(client, sep); break; }
		case COMMAND_SMP				: { Command_StationMarketPlace(client, sep); break; }
		case COMMAND_CONSUME_FOOD		: { Command_ConsumeFood(client, sep); break; }
		case COMMAND_AQUAMAN			: { Command_Aquaman(client, sep); break; }
		case COMMAND_ATTUNE_INV			: { Command_Attune_Inv(client, sep); break; }
		case COMMAND_PLAYER				: { Command_Player(client, sep); break; }
		case COMMAND_PLAYER_COINS		: { Command_Player_Coins(client, sep); break; }
		case COMMAND_PLAYER_SET			: { Command_Player_Set(client, sep); break; }
		case COMMAND_RESET_ZONE_TIMER	: { Command_Reset_Zone_Timer(client, sep); break; }
		case COMMAND_ACHIEVEMENT_ADD	: { Command_AchievementAdd(client, sep); break; }
		case COMMAND_EDITOR				: { Command_Editor(client, sep); break; }
		case COMMAND_ACCEPT_RESURRECTION: { Command_AcceptResurrection(client, sep); break; }
		case COMMAND_DECLINE_RESURRECTION:{ Command_DeclineResurrection(client, sep); break; }
		case COMMAND_TEST				: { Command_Test(client, sep); break; }
		case COMMAND_MOUNT				: { Command_Mount(client, sep); break; }
		case COMMAND_SPEED				: { Command_Speed(client, sep); break; }
		case COMMAND_SERVER_FLAG        : { Command_ServerFlag(client, sep); break; }
		case COMMAND_PVP_RANGE			: { Command_PVPRange(client); break; }
		case COMMAND_PVP				: { Command_PVP(client); break; }
		case COMMAND_KNOWLEDGEWINDOW_SORT:{ Command_KnowledgeWindow_Sort(client, sep); break; }
		case COMMAND_RESET_ENCOUNTER    : { Command_ResetEncounter(client); break; }
		case COMMAND_KNOCKBACK			: { Command_Knockback(client, sep);  break; }
		case COMMAND_HEAL				: { Command_Heal(client);  break; }

		case COMMAND_BOT				: { Command_Bot(client, sep); break; }
		case COMMAND_BOT_CREATE			: { Command_Bot_Create(client, sep); break; }
		case COMMAND_BOT_CUSTOMIZE		: { Command_Bot_Customize(client, sep); break; }
		case COMMAND_BOT_SPAWN			: { Command_Bot_Spawn(client, sep); break; }
		case COMMAND_BOT_LIST			: { Command_Bot_List(client, sep); break; }
		case COMMAND_BOT_INV			: { Command_Bot_Inv(client, sep); break; }
		case COMMAND_BOT_SETTINGS		: { Command_Bot_Settings(client, sep); break; }
		case COMMAND_BOT_HELP			: { Command_Bot_Help(client, sep); break; }

		default: 
		{
			LogWrite(COMMAND__WARNING, 1, "Command", "Unhandled command: %s", command->command.data.c_str());
			break;
		}

	}
	safe_delete(sep);
}


/******************** New COMMAND Handler Functions ********************/
/*
	Started breaking apart the huge switch() for commands into sepErate 
	functions so it is easier to locate the blocks of code by command
	-- JA 2012.03.03
*/

// sample function header
/* 
	Function: 
	Purpose	: 
	Params	: 
	Dev		: 
	Example	: 
*/ 
//void Commands::Command()
//{
//}


/* 
	Function: Command_AcceptAdvancement()
	Purpose	: Player accepts a new advancement option
	Params	: Spell ID
	Dev		: Jabantiz
*/ 
void Commands::Command_AcceptAdvancement(Client* client, Seperator* sep)
{
	 Player *player = client->GetPlayer();
	TraitData* trait = master_trait_list.GetTrait(atoul(sep->arg[0]));

	// Check to see if this is a trait or grandmaster training (traits are always new spells, training is always upgrades)
	if (!player->HasSpell(trait->spellID, 0, true)) 
	{
		Spell* spell = master_spell_list.GetSpell(trait->spellID, trait->tier);
		player->AddSpellBookEntry(trait->spellID, trait->tier, player->GetFreeSpellBookSlot(spell->GetSpellData()->spell_book_type), spell->GetSpellData()->spell_book_type, spell->GetSpellData()->linked_timer, true);
	}
	else 
	{
		Spell* spell = master_spell_list.GetSpell(trait->spellID, trait->tier);
		int8 old_slot = player->GetSpellSlot(spell->GetSpellID());
		player->RemoveSpellBookEntry(spell->GetSpellID());
		player->AddSpellBookEntry(spell->GetSpellID(), spell->GetSpellTier(), old_slot, spell->GetSpellData()->spell_book_type, spell->GetSpellData()->linked_timer, true);
		player->UnlockSpell(spell);
		client->SendSpellUpdate(spell);
	}

	// Spell book update
	client->QueuePacket(player->GetSpellBookUpdatePacket(client->GetVersion()));
	client->QueuePacket(master_trait_list.GetTraitListPacket(client));
}

/* 
	Function: 
	Purpose	: 
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_AFK(Client* client, Seperator* sep)
{
	Player* player = client->GetPlayer();

	player->toggle_character_flag(CF_AFK);
	client->Message(CHANNEL_COLOR_YELLOW,"You are %s afk.", client->GetPlayer()->get_character_flag(CF_AFK)?"now":"no longer");

	if (player->get_character_flag(CF_AFK))
	{
		LogWrite(MISC__TODO, 1, "TODO", "player->SetActivityStatus(player->GetActivityStatus() + ACTIVITY_STATUS_AFK); <-- need to find this");
		/*player->SetActivityStatus(player->GetActivityStatus() + ACTIVITY_STATUS_AFK); <-- need to find this */

		if (sep && sep->argplus[0])
			player->SetAwayMessage("I am away from the keyboard, " + string(sep->argplus[0]));
		else
			player->SetAwayMessage("Sorry, I am A.F.K. (Away From Keyboard)");

		string message = string(player->GetName()) + " is going afk.";
		Spawn* target = player->GetTarget();

		if (target && target != player)
		{
			message = string(player->GetName()) + " tells " + string(target->GetName()) + " that ";
			player->GetGender() == 1 ? message += "he" : message += "she";
			message += " is going afk.";
		}

		player->GetZone()->SimpleMessage(CHANNEL_COLOR_YELLOW, message.c_str(), player, 30);
	}
	/*else
		player->SetActivityStatus(player->GetActivityStatus() - ACTIVITY_STATUS_AFK); <-- need to find this */
}

/* 
	Function: Command_Appearance()
	Purpose	: Handles /appearance commands
	Params	: list
	Dev		: Scatman
	Example	: /appearance list
*/ 
void Commands::Command_Appearance(Client* client, Seperator* sep, int handler)
{
	if( handler == COMMAND_APPEARANCE )
	{
		// /appearance command by itself shows help (to be extended)
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /appearance list [appearance name]");
		return;
	}
	else if( handler == COMMAND_APPEARANCE_LIST )
	{
		// /appearance list command expects "name" param
		if (sep && sep->arg[0]) 
		{
			const char* appearance_name = sep->argplus[0];
			client->Message(CHANNEL_COLOR_YELLOW, "Listing appearances like '%s':", appearance_name);
			vector<int16>* appearances = database.GetAppearanceIDsLikeName(string(appearance_name));

			if (appearances) 
			{
				vector<int16>::iterator itr;
				for (itr = appearances->begin(); itr != appearances->end(); itr++) 
				{
					int16 id = *itr;
					string name = database.GetAppearanceName(id);

					if (ToLower(name).find(ToLower(string(appearance_name))) < 0xFFFFFFFF)
						client->Message(CHANNEL_COLOR_YELLOW, "%s (%u)", name.c_str(), id);
				}
				safe_delete(appearances);
			}
		}
		else // no param
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /appearance list [appearance name]");
	}

}

/* 
	Function: Command_Claim()
	Purpose	: 
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_Claim(Client* client, Seperator* sep)
{
	if (sep && sep->arg[0] && sep->IsNumber(0)) 
	{
		LogWrite(MISC__TODO, 1, "TODO", "On Scatman's TODO list!\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "On Scatman's TODO :)");
	}
	else
		client->ShowClaimWindow();
}

/* 
	Function: Command_ClearAllQueued()
	Purpose	: 
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_ClearAllQueued(Client* client)
{
	ZoneServer* zone = client->GetPlayer()->GetZone();
	if (zone && zone->GetSpellProcess())
		zone->GetSpellProcess()->RemoveSpellFromQueue(client->GetPlayer());
}

/* 
	Function: Command_CancelMaintained()
	Purpose	: Cancels maintained spells
	Params	: Maintained Spell Index
	Dev		: Zcoretri
	Example	: /cancel_maintained 1 - would cancel the spell in slot 1 of Maintained Spells list
*/ 
void Commands::Command_CancelMaintained(Client* client, Seperator* sep)
{
	if (sep && sep->arg[0] && sep->IsNumber(0)) 
	{
		int32 spell_index = atoul(sep->arg[0]);
		MaintainedEffects mEffects = client->GetPlayer()->GetInfoStruct()->maintained_effects[spell_index];
		//Spell* spell = master_spell_list.GetSpell(mEffects.spell_id, mEffects.tier);

	//	if (spell && spell->GetSpellData()->friendly_spell)  -- NOTE::You can cancel hostile maintained spells, 
		                                                     // just not spelleffects/dets - Foof
		//{
			if (!client->GetPlayer()->GetZone()->GetSpellProcess()->DeleteCasterSpell(mEffects.spell))
				client->Message(CHANNEL_COLOR_RED, "The maintained spell could not be cancelled.");
	//	}
		//else
			//client->Message(CHANNEL_COLOR_RED, "You can only cancel friendly spells!");
	}
}

/* 
	Function: Command_Create()
	Purpose	: Handler for starting Tradeskilling table
	Params	: 
	Dev		: Zcoretri
	Example	: 
*/ 
void Commands::Command_Create(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_CREATE");
	client->SendRecipeList();
	client->ShowRecipeBook();
}

void Commands::Command_CreateFromRecipe(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_CREATEFROMRECIPE");
	if (sep && sep->arg[0] && sep->IsNumber(0))
		ClientPacketFunctions::SendCreateFromRecipe(client, atoul(sep->arg[0]));
}

/* 
	Function: Command_Distance()
	Purpose	: Displays distance from targeted spawn
	Params	: 
	Dev		: Scatman
	Example	: /distance
*/ 
void Commands::Command_Distance(Client* client)
{
	Spawn* target = client->GetPlayer()->GetTarget();

	if (target)
		client->Message(CHANNEL_COLOR_YELLOW, "Your distance from %s is %f", target->GetName(), client->GetPlayer()->GetDistance(target));
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You must have a spawn targeted to use /distance");
}

/* 
	Function: Command_Duel()
	Purpose	: Handle the /duel commands - not yet implemented
	Params	: unknown
	Dev		: 
	Example	: 
*/ 
void Commands::Command_Duel(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_DUEL");
	LogWrite(MISC__TODO, 1, "Command", "TODO-Command: Duel Command");
	client->Message(CHANNEL_COLOR_YELLOW, "You cannot duel other players (Not Implemented)");
}

/* 
	Function: Command_DuelBet()
	Purpose	: Handle the /duel commands - not yet implemented
	Params	: unknown
	Dev		: 
	Example	: 
*/ 
void Commands::Command_DuelBet(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_DUELBET");
	LogWrite(MISC__TODO, 1, "Command", "TODO-Command: Duel Bet Command");
	client->Message(CHANNEL_COLOR_YELLOW, "You cannot duel other players (Not Implemented)");
}

/* 
	Function: Command_DuelAccept()
	Purpose	: Handle the /duel commands - not yet implemented
	Params	: unknown
	Dev		: 
	Example	: 
*/ 
void Commands::Command_DuelAccept(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_DUEL_ACCEPT");
	LogWrite(MISC__TODO, 1, "Command", "TODO-Command: Accept Duel Command");
	client->Message(CHANNEL_COLOR_YELLOW, "You cannot duel other players (Not Implemented)");
}

/* 
	Function: Command_DuelDecline()
	Purpose	: Handle the /duel commands - not yet implemented
	Params	: unknown
	Dev		: 
	Example	: 
*/ 
void Commands::Command_DuelDecline(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_DUEL_DECLINE");
	LogWrite(MISC__TODO, 1, "Command", "TODO-Command: Decline Duel Request Command");
	client->Message(CHANNEL_COLOR_YELLOW, "You cannot duel other players (Not Implemented)");
}

/* 
	Function: Command_DuelSurrender()
	Purpose	: Handle the /duel commands - not yet implemented
	Params	: unknown
	Dev		: 
	Example	: 
*/ 
void Commands::Command_DuelSurrender(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_DUEL_SURRENDER");
	LogWrite(MISC__TODO, 1, "Command", "TODO-Command: Surrender Duel Command");
	client->Message(CHANNEL_COLOR_YELLOW, "You cannot duel other players (Not Implemented)");

	// JA, just messin around ;)
	char surrender[64];
	sprintf(surrender, "%s surrendered like a cowardly dog!", client->GetPlayer()->GetName());
	zone_list.HandleGlobalAnnouncement(surrender);
}

/* 
	Function: Command_DuelToggle()
	Purpose	: Handle the /duel commands - not yet implemented
	Params	: unknown
	Dev		: 
	Example	: 
*/ 
void Commands::Command_DuelToggle(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_DUEL");
	LogWrite(MISC__TODO, 1, "Command", "TODO-Command: Duel Commands");
	client->Message(CHANNEL_COLOR_YELLOW, "You cannot duel other players (Not Implemented)");
}

/* 
	Function: 
	Purpose	: 
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_EntityCommand(Client* client, Seperator* sep, int handler)
{
	if( handler == COMMAND_ENTITYCOMMAND )
	{
		// /entitycommand command by itself shows help (to be extended)
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /entity_command list [entity command name]");
		return;
	}
	else if( handler == COMMAND_ENTITYCOMMAND_LIST )
	{
		// /entitycommand list command expects "name" param
		if (sep && sep->arg[0]) 
		{
			const char* entity_command_name = sep->argplus[0];
			client->Message(CHANNEL_COLOR_YELLOW, "Listing entity commands like '%s':", entity_command_name);
			map<int32, vector<EntityCommand*>*>* entity_command_list_all = client->GetCurrentZone()->GetEntityCommandListAll();
			map<int32, vector<EntityCommand*>*>::iterator itr;

			for (itr = entity_command_list_all->begin(); itr != entity_command_list_all->end(); itr++) 
			{
				vector<EntityCommand*>* entity_command_list = itr->second;
				vector<EntityCommand*>::iterator itr2;

				for (itr2 = entity_command_list->begin(); itr2 != entity_command_list->end(); itr2++) 
				{
					EntityCommand* entity_command = *itr2;

					if (ToLower(entity_command->name).find(ToLower(string(entity_command_name))) < 0xFFFFFFFF)
						client->Message(CHANNEL_COLOR_YELLOW, "Command Text: %s, Command List ID: %u, Distance: %f\n", entity_command->name.c_str(), itr->first, entity_command->distance);
				}
			}
		}
		else // no command name
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /entity_command list [entity command name]");
	}
}


/* 
	Function: Command_Follow()
	Purpose	: Handle the /follow command - not yet implemented
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_Follow(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_FOLLOW");
	// flag to toggle if the players target is in the players group
	bool targetInGroup = false;
	// get a pointer to the players group
	GroupMemberInfo* gmi = client->GetPlayer()->GetGroupMemberInfo();
	// If the player has a group and has a target
	if (gmi && client->GetPlayer()->GetTarget()) {
		deque<GroupMemberInfo*>::iterator itr;

		world.GetGroupManager()->GroupLock(__FUNCTION__, __LINE__);

		deque<GroupMemberInfo*>* members = world.GetGroupManager()->GetGroupMembers(gmi->group_id);
		// Loop through the group members
		for (itr = members->begin(); itr != members->end(); itr++) {
			// If a group member matches a target
			if ((*itr)->member == client->GetPlayer()->GetTarget()) {
				// toggle the flag and break the loop
				targetInGroup = true;
				break;
			}
		}

		world.GetGroupManager()->ReleaseGroupLock(__FUNCTION__, __LINE__);
	}
	if (targetInGroup) {
		// CHANNEL_COLOR_CHAT_RELATIONSHIP = 4, which matches the value in logs
		client->Message(CHANNEL_COLOR_CHAT_RELATIONSHIP, "You start to follow %s.", client->GetPlayer()->GetTarget()->GetName());
		client->GetPlayer()->SetFollowTarget(client->GetPlayer()->GetTarget());
		client->GetPlayer()->info_changed = true;
		client->GetPlayer()->changed = true;
	}
	else
		client->Message(CHANNEL_COLOR_WHITE, "You must first select a group member to follow.");
}

/* 
	Function: Command_StopFollow()
	Purpose	: Handle the /stop_follow command - not yet implemented
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_StopFollow(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_STOP_FOLLOW");
	if (client->GetPlayer()->GetFollowTarget()) {
		// CHANNEL_COLOR_CHAT_RELATIONSHIP = 4, which matches the value in logs
		client->Message(CHANNEL_COLOR_CHAT_RELATIONSHIP, "You are no longer following %s", client->GetPlayer()->GetFollowTarget()->GetName());
		client->GetPlayer()->SetFollowTarget(0);
		client->GetPlayer()->info_changed = true;
		client->GetPlayer()->changed = true;
	}
}

#include "../Zone/SPGrid.h"
/* 
	Function: Command_Grid()
	Purpose	: Show player's current Grid ID
	Params	: 
	Dev		: Scatman
	Example	: /grid
*/ 
void Commands::Command_Grid(Client* client)
{
	client->Message(CHANNEL_COLOR_YELLOW, "Your Grid ID is %u", client->GetPlayer()->appearance.pos.grid_id);

	if (client->GetCurrentZone()->Grid != nullptr) {
		int32 grid = client->GetCurrentZone()->Grid->GetGridID(client->GetPlayer());
		client->Message(CHANNEL_COLOR_YELLOW, "SPGrid result is %u", grid);
	}
}

/* 
	Function: Command_Guild()
	Purpose	: Handler for all UI-related guild commands
	Dev		: Scatman
*/ 
void Commands::Command_Guild(Client* client, Seperator* sep)
{
	Guild* guild = client->GetPlayer()->GetGuild();

	if (sep && sep->arg[0]) 
	{
		const char* command = sep->arg[0];
		int32 length = strlen(command);

		LogWrite(COMMAND__DEBUG, 0, "Command", "Guild Command: %s", command);

		if (strncmp(command, "rank_name", length) == 0 && sep->arg[1] && sep->IsNumber(1) && sep->arg[2] && guild)
			guild->SetRankName(atoi(sep->arg[1]), sep->argplus[2]);
		else if (strncmp(command, "rank_permission", length) == 0 && sep->arg[1] && sep->IsNumber(1) && sep->arg[2] && sep->IsNumber(2) && sep->arg[3] && guild)
			guild->SetPermission(atoi(sep->arg[1]), atoi(sep->arg[2]), strncmp(sep->arg[3], "true", 4) == 0 ? 1 : 0);
		else if (strncmp(command, "filter_event", length) == 0 && sep->arg[1] && sep->IsNumber(1) && sep->arg[2] && sep->IsNumber(2) && sep->arg[3] && guild)
			guild->SetEventFilter(atoi(sep->arg[1]), atoi(sep->arg[2]), strncmp(sep->arg[3], "true", 4) == 0 ? 1 : 0);
		else if (strncmp(command, "kick", length) == 0 && sep->arg[1] && guild)
			guild->KickGuildMember(client, sep->arg[1]);
		else if (strncmp(command, "demote", length) == 0 && sep->arg[1] && guild)
			guild->DemoteGuildMember(client, sep->arg[1]);
		else if (strncmp(command, "promote", length) == 0 && sep->arg[1] && guild)
			guild->PromoteGuildMember(client, sep->arg[1]);
		else if (strncmp(command, "points", length) == 0 && guild)
		{
			if (sep->arg[1] && strncmp(sep->arg[1], "add", length) == 0)
			{
				if (sep->arg[2] && sep->IsNumber(2) && sep->arg[3])
				{
					float points = atof(sep->arg[2]);
					const char* option = sep->arg[3];
					const char* comment = sep->argplus[4];

					if (strncmp(option, "all", strlen(option)) == 0)
						guild->AddPointsToAll(client, points, comment);
					else if (strncmp(option, "online", strlen(option)) == 0)
						guild->AddPointsToAllOnline(client, points, comment);
					else if (strncmp(option, "group", strlen(option)) == 0)
						guild->AddPointsToGroup(client, points, comment);
					else if (strncmp(option, "raid", strlen(option)) == 0)
						guild->AddPointsToRaid(client, points, comment);
					else
						guild->AddPointsToGuildMember(client, points, option, comment);
				}
			}
			else if (sep->arg[1] && strncmp(sep->arg[1], "view", strlen(sep->arg[1])) == 0 && sep->arg[2])
				guild->ViewGuildMemberPoints(client, sep->arg[2]);
			else
				LogWrite(COMMAND__ERROR, 0, "Command", "Unhandled guild points command: %s", sep->argplus[0]);
		}
		else if (strncmp(command, "motd", length) == 0 && sep->arg[1] && guild)
			guild->SetMOTD(sep->argplus[1]);
		else if (strncmp(command, "recruiting", length) == 0 && guild) 
		{
			if (sep->arg[1])
			{
				const char* option = sep->arg[1];

				if (strncmp(option, "short_text", strlen(option)) == 0 && sep->arg[2])
					guild->SetRecruitingShortDesc(sep->argplus[2]);
				else if (strncmp(option, "long_text", strlen(option)) == 0 && sep->arg[2])
					guild->SetRecruitingFullDesc(sep->argplus[2]);
				else if (strncmp(option, "flag", strlen(option)) == 0 && sep->arg[2] && sep->IsNumber(2) && sep->arg[3])
					guild->SetRecruitingFlag(atoi(sep->arg[2]), strncmp(sep->arg[3], "true", 4) == 0 ? 1 : 0);
				else if (strncmp(option, "min_level", strlen(option)) == 0 && sep->arg[2] && sep->IsNumber(2))
					guild->SetRecruitingMinLevel(atoi(sep->arg[2]));
				else if (strncmp(option, "playstyle", strlen(option)) == 0 && sep->arg[2] && sep->IsNumber(2))
					guild->SetRecruitingPlayStyle(atoi(sep->arg[2]));
				else if (strncmp(option, "tag", strlen(option)) == 0 && sep->arg[2] && sep->IsNumber(2) && sep->arg[3] && sep->IsNumber(3))
					guild->SetRecruitingDescTag(atoi(sep->arg[2]), atoi(sep->arg[3]));
				else if (strncmp(command, "recruiting", strlen(option)) == 0)
					guild->ChangeMemberFlag(client, GUILD_MEMBER_FLAGS_RECRUITING_FOR_GUILD, strncmp(sep->arg[1], "true", 4) == 0 ? 1 : 0);
				else
					LogWrite(COMMAND__ERROR, 0, "Command", "Unhandled guild recruiting command: %s", sep->argplus[0]);
			}
			else
				LogWrite(COMMAND__ERROR, 0, "Command", "Unhandled guild recruiting command: %s", sep->argplus[0]);
		}
		else if (strncmp(command, "notify_online", length) == 0 && sep->arg[1] && guild)
			guild->ChangeMemberFlag(client, GUILD_MEMBER_FLAGS_NOTIFY_LOGINS, strncmp(sep->arg[1], "true", 4) == 0 ? 1 : 0);
		else if (strncmp(command, "event_privacy", length) == 0 && sep->arg[1] && guild)
			guild->ChangeMemberFlag(client, GUILD_MEMBER_FLAGS_DONT_GENERATE_EVENTS, strncmp(sep->arg[1], "true", 4) == 0 ? 1 : 0);
		else if (strncmp(command, "recruiter", length) == 0 && sep->arg[1] && sep->arg[2] && guild)
			guild->SetGuildRecruiter(client, sep->arg[1], strncmp(sep->arg[2], "true", 4) == 0 ? true : false);
		else if (strncmp(command, "recruiter_description", length) == 0 && sep->arg[1] && guild)
			guild->SetGuildRecruiterDescription(client, sep->argplus[1]);
		else if (strncmp(command, "lock_event", length) == 0 && sep->arg[1] && sep->arg[2] && guild)
			guild->LockGuildEvent(atoul(sep->arg[1]), strncmp(sep->arg[2], "true", 4) == 0 ? true : false);
		else if (strncmp(command, "delete_event", length) == 0 && sep->arg[1] && guild)
			guild->DeleteGuildEvent(atoul(sep->arg[1]));
		else if (strncmp(command, "invite", length) == 0 && guild) 
		{
			if (sep->arg[1] && strlen(sep->arg[1]) > 0)
				guild->InvitePlayer(client, sep->arg[1]);
			else 
			{
				Spawn* target = client->GetPlayer()->GetTarget();
				if (target) 
				{
					if (target->IsPlayer())
						guild->InvitePlayer(client, target->GetName());
					else
						client->Message(CHANNEL_COLOR_WHITE, "%s is not a player.", target->GetName());
				}
			}
		}
		else if (strncmp(command, "accept", length) == 0) 
		{
			PendingGuildInvite* pgi = client->GetPendingGuildInvite();

			if (pgi && pgi->guild && pgi->invited_by)
				pgi->guild->AddNewGuildMember(client, pgi->invited_by->GetName());
			client->SetPendingGuildInvite(0);
		}
		else if (strncmp(command, "decline", length) == 0) 
		{
			PendingGuildInvite* pgi = client->GetPendingGuildInvite();

			if (pgi && pgi->guild && pgi->invited_by)
			{
				Client* client_inviter = pgi->invited_by->GetZone()->GetClientBySpawn(pgi->invited_by);

				if (client_inviter)
					client_inviter->Message(CHANNEL_COLOR_WHITE, "%s has declined your invitation to join %s.", client->GetPlayer()->GetName(), pgi->guild->GetName());
			}
			client->SetPendingGuildInvite(0);
		}
		else if (strncmp(command, "create", length) == 0 && sep->arg[1]) 
		{
			const char* guild_name = sep->argplus[0];

			if (!guild_list.GetGuild(guild_name))
				world.CreateGuild(guild_name, client, client->GetPlayer()->GetGroupMemberInfo()->group_id);
			else
				client->SimpleMessage(CHANNEL_COLOR_WHITE, "A guild with that name already exists.");
		}
		else if (strncmp(command, "search", length) == 0)
			client->ShowGuildSearchWindow();
		else if (strncmp(command, "recruiting_details", length) == 0 && sep->arg[1] && sep->IsNumber(1))
		{
			Guild* to_guild = guild_list.GetGuild(atoul(sep->arg[1]));

			if (to_guild)
				to_guild->SendGuildRecruitingDetails(client);
		}
		else if (strncmp(command, "recruiting_image", length) == 0 && sep->arg[1] && sep->IsNumber(1)) 
		{
			Guild* to_guild = guild_list.GetGuild(atoul(sep->arg[1]));

			if (to_guild)
				to_guild->SendGuildRecruitingImages(client);
		}
		else if (strncmp(command, "recruiter_adventure_class", length) == 0) 
		{
			if (sep->arg[1])
			{
				const char* option = sep->arg[1];

				if (strncmp(option, "toggle", strlen(option)) == 0)
					guild->ToggleGuildRecruiterAdventureClass(client);
			}
			else
				LogWrite(COMMAND__ERROR, 0, "Command", "Unhandled guild recruiter_adventure_class command: '%s'", sep->argplus[0]);
		}
		else if (strncmp(command, "display_heraldry", length) == 0)
		{
			// JA: not sure this is right...
			client->GetPlayer()->toggle_character_flag(CF_SHOW_GUILD_HERALDRY);
			client->GetPlayer()->SetCharSheetChanged(true);
		}
		else
			LogWrite(COMMAND__ERROR, 0, "Command", "Unhandled guild command: '%s'", sep->argplus[0]);
	}
}

/* 
	Function: Command_GuildCreate()
	Purpose	: Display's in-game Guild Creation window
	Dev		: Scatman
*/ 
void Commands::Command_CreateGuild(Client* client)
{
	client->SendGuildCreateWindow();
}

/* 
	Function: Command_SetGuildOfficerNote()
	Purpose	: 
	Dev		: Scatman
*/ 
void Commands::Command_SetGuildOfficerNote(Client* client, Seperator* sep)
{
	if (sep && sep->arg[0] && sep->arg[1]) 
	{
		Guild* guild = client->GetPlayer()->GetGuild();

		if (guild)
			guild->SetGuildOfficerNote(sep->arg[0], sep->argplus[1]);
	}
}

/* 
	Function: Command_SetGuildMemberNote()
	Purpose	: 
	Dev		: Scatman
*/ 
void Commands::Command_SetGuildMemberNote(Client* client, Seperator* sep)
{
	if (sep && sep->arg[0] && sep->arg[1]) 
	{
		Guild* guild = client->GetPlayer()->GetGuild();

		if (guild)
			guild->SetGuildMemberNote(sep->arg[0], sep->argplus[1]);
	}
}

/* 
	Function: Command_GuildSay()
	Purpose	: 
	Dev		: Scatman
*/ 
void Commands::Command_GuildSay(Client* client, Seperator* sep)
{
	Guild* guild = client->GetPlayer()->GetGuild();

	if (guild) 
	{
		if (sep && sep->arg[0])
			guild->HandleGuildSay(client, sep->argplus[0]);
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_WHITE, "You are not a member of a guild");
}

/* 
	Function: Command_OfficerSay()
	Purpose	: 
	Dev		: Scatman
*/ 
void Commands::Command_OfficerSay(Client* client, Seperator* sep)
{
	Guild* guild = client->GetPlayer()->GetGuild();

	if (guild) 
	{
		if (sep && sep->arg[0])
			guild->HandleOfficerSay(client, sep->argplus[0]);
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_WHITE, "You are not a member of a guild");
}

/* 
	Function: Command_Guilds()
	Purpose	: Shows help for /guild command
	Params	: 
	Dev		: Scatman
	Example	: 
*/ 
void Commands::Command_Guilds(Client* client)
{
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /guilds [create|delete|add|remove|list]");
}

/* 
	Function: Command_GuildsAdd()
	Purpose	: Add's players to a guild
	Params	: guild_name|guild_id, player name
	Dev		: Scatman
	Example	: /guilds add 1 Admin  = adds player Admin to guild_id 1
*/ 
void Commands::Command_GuildsAdd(Client* client, Seperator* sep)
{
	if (sep && sep->arg[0] && strlen(sep->arg[0]) > 0) 
	{
		Guild* guild = 0;
		bool found = true;

		if (sep->IsNumber(0)) 
		{
			guild = guild_list.GetGuild(atoul(sep->arg[0]));

			if (!guild) 
			{
				client->Message(CHANNEL_COLOR_YELLOW, "Guild with ID %u does not exist.", atoul(sep->arg[0]));
				found = false;
			}
		}
		else 
		{
			guild = guild_list.GetGuild(sep->arg[0]);

			if (!guild) 
			{
				client->Message(CHANNEL_COLOR_YELLOW, "Guild '%s' does not exist.", sep->arg[0]);
				found = false;
			}
		}
		if (found) 
		{
			Client* to_client = 0;

			if (sep->arg[1] && strlen(sep->arg[1]) > 0)
				to_client = zone_list.GetClientByCharName(string(sep->arg[1]));
			else if (client->GetPlayer()->GetTarget() && client->GetPlayer()->GetTarget()->IsPlayer())
				to_client = client->GetPlayer()->GetTarget()->GetZone()->GetClientBySpawn(client->GetPlayer()->GetTarget());

			if (to_client)
				guild->InvitePlayer(client, to_client->GetPlayer()->GetName());
			else
				client->Message(CHANNEL_COLOR_YELLOW, "Could not find player '%s' to invite to the guild.", sep->arg[1]);
		}
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /guilds add [guild name|guild id] (player name).");
}

/* 
	Function: Command_GuildsCreate()
	Purpose	: Creates a guild
	Params	: guild_name, player_name (optional)
	Dev		: Scatman
	Example	: /guilds create [guild name] (player name)
*/ 
void Commands::Command_GuildsCreate(Client* client, Seperator* sep)
{
	if (sep && sep->arg[0]) 
	{
		const char* guild_name = sep->arg[0];

		if (!guild_list.GetGuild(guild_name)) 
		{
			bool ret = false;

			if (sep->arg[1] && strlen(sep->arg[1]) > 0) 
			{
				Client* to_client = zone_list.GetClientByCharName(string(sep->arg[1]));

				if (to_client) 
				{
					world.CreateGuild(guild_name, to_client);
					ret = true;
				}
				else
					client->Message(CHANNEL_COLOR_YELLOW, "Could not find player with name '%s'", sep->arg[1]);
			}
			else if (client->GetPlayer()->GetTarget() && client->GetPlayer()->GetTarget()->IsPlayer()) 
			{
				Client* to_client = client->GetPlayer()->GetTarget()->GetZone()->GetClientBySpawn(client->GetPlayer()->GetTarget());

				if (to_client) 
				{
					world.CreateGuild(guild_name, to_client);
					ret = true;
				}
			}
			else 
			{
				world.CreateGuild(guild_name);
				ret = true;
			}

			if (ret)
				client->Message(CHANNEL_COLOR_YELLOW, "Guild '%s' was successfully created.", guild_name);
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "There was an error creating the guild.");
		}
		else
			client->Message(CHANNEL_COLOR_YELLOW, "Guild '%s' already exists.", guild_name);
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /guilds create [guild name] (player name).  If no player is specified, the player's target will be used.  If the player has no target, a guild with no members will be created.");
}

/* 
	Function: Command_GuildsDelete()
	Purpose	: Delete's a guild
	Params	: guild name
	Dev		: Scatman
	Example	: /guilds delete Test
*/ 
void Commands::Command_GuildsDelete(Client* client, Seperator* sep)
{
	if (sep && sep->arg[0]) 
	{
		Guild* guild = 0;
		bool found = true;

		if (sep->IsNumber(0)) 
		{
			guild = guild_list.GetGuild(atoul(sep->arg[0]));

			if (!guild) 
			{
				client->Message(CHANNEL_COLOR_YELLOW, "Guild with ID %u does not exist.", atoul(sep->arg[0]));
				found = false;
			}
		}
		else 
		{
			guild = guild_list.GetGuild(sep->arg[0]);

			if (!guild) 
			{
				client->Message(CHANNEL_COLOR_YELLOW, "Guild '%s' does not exist.", sep->arg[0]);
				found = false;
			}
		}

		if (found) 
		{
			guild->RemoveAllGuildMembers();
			database.DeleteGuild(guild);

			client->Message(CHANNEL_COLOR_YELLOW, "Guild '%s' was successfully deleted.", guild->GetName());
			guild_list.RemoveGuild(guild, true);
		}
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /guilds delete [guild name].");
}

/* 
	Function: Command_GuildsList()
	Purpose	: Lists current guilds on server
	Params	: 
	Dev		: Scatman
	Example	: /guilds list
*/ 
void Commands::Command_GuildsList(Client* client)
{
	MutexMap<int32, Guild*>* guilds = guild_list.GetGuilds();
	MutexMap<int32, Guild*>::iterator itr = guilds->begin();
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Guild List:");
	if (guilds->size() == 0)
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "None.");
	else {
		while (itr.Next()) {
			Guild* guild = itr.second;
			client->Message(CHANNEL_COLOR_YELLOW, "%u) %s", guild->GetID(), guild->GetName());
		}
	}
}

/* 
	Function: Command_GuildsRemove()
	Purpose	: Removes a player from a guild
	Params	: guild name|guild id, player name
	Dev		: Scatman
	Example	:  /guilds remove 1 Admin = removes Admin from guild 1
*/ 
void Commands::Command_GuildsRemove(Client* client, Seperator* sep)
{
	if (sep && sep->arg[0] && strlen(sep->arg[0]) > 0) 
	{
		Guild* guild = 0;
		bool found = true;

		if (sep->IsNumber(0)) 
		{
			guild = guild_list.GetGuild(atoul(sep->arg[0]));

			if (!guild) 
			{
				client->Message(CHANNEL_COLOR_YELLOW, "Guild with ID %u does not exist.", atoul(sep->arg[0]));
				found = false;
			}
		}
		else 
		{
			guild = guild_list.GetGuild(sep->arg[0]);

			if (!guild) 
			{
				client->Message(CHANNEL_COLOR_YELLOW, "Guild '%s' does not exist.", sep->arg[0]);
				found = false;
			}
		}

		if (found) 
		{
			Client* to_client = 0;

			if (sep->arg[1] && strlen(sep->arg[1]) > 0)
				to_client = zone_list.GetClientByCharName(string(sep->arg[1]));
			else if (client->GetPlayer()->GetTarget() && client->GetPlayer()->GetTarget()->IsPlayer())
				to_client = client->GetPlayer()->GetTarget()->GetZone()->GetClientBySpawn(client->GetPlayer()->GetTarget());

			if (to_client) 
			{
				Player* to_player = to_client->GetPlayer();
				if (to_player->GetGuild())
				{
					if (to_player->GetGuild() == guild) 
					{
						guild->KickGuildMember(client, to_player->GetName());
					}
					else
						client->Message(CHANNEL_COLOR_YELLOW, "%s is not in the guild '%s'.", to_player->GetName(), guild->GetName());
				}
				else
					client->Message(CHANNEL_COLOR_YELLOW, "%s is not in a guild.", to_player->GetName());
			}
			else
				client->Message(CHANNEL_COLOR_YELLOW, "Could not find player '%s' to invite to the guild.", sep->arg[1]);
		}
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /guilds remove [guild name|guild id] (player name).");
}

/* 
	Function: Command_InspectPlayer()
	Purpose	: Handle the Inspect functions
	Params	: Client to inspect
	Dev		: Scatman
	Example	: /inspect Scatman
*/ 
void Commands::Command_InspectPlayer(Client* client, Seperator* sep)
{
	if (sep && sep->arg[0]) 
	{
		Client* inspect_client = zone_list.GetClientByCharName(string(sep->arg[0]));

		if (inspect_client)
			client->InspectPlayer(inspect_client->GetPlayer());
	}
	else 
	{
		Spawn* target = client->GetPlayer()->GetTarget();

		if (target && target->IsPlayer())
			client->InspectPlayer((Player*)target);
	}
}

/* 
	Function: Command_Inventory()
	Purpose	: Handle changes in player inventory
	Params	: <TBD>
	Dev		: All
	Example	: /inventory delete item_id
*/ 
void Commands::Command_Inventory(Client* client, Seperator* sep, EQ2_RemoteCommandString* command)
{

	PrintSep(sep, "Command_Inventory"); // temp to figure out the params

	Player* player = client->GetPlayer();

	if(sep && sep->arg[0][0])
	{
		LogWrite(COMMAND__INFO, 0, "Command", "command: %s", command->command.data.c_str());

		if(client->GetPlayer()->GetHP() == 0)
			client->SimpleMessage(CHANNEL_COLOR_RED,"You cannot do that right now.");
		else if(sep->arg[1][0] && strncasecmp("destroy", sep->arg[0], 6) == 0 && sep->IsNumber(1))
		{
			int16 index = atoi(sep->arg[1]);
			Item* item = client->GetPlayer()->item_list.GetItemFromIndex(index);

			if(item)
			{
				if(item->GetItemScript() && lua_interface)
					lua_interface->RunItemScript(item->GetItemScript(), "destroyed", item, client->GetPlayer());

				database.DeleteItem(client->GetCharacterID(), item, 0);
				client->GetPlayer()->item_list.DestroyItem(index);
				EQ2Packet* outapp = client->GetPlayer()->SendInventoryUpdate(client->GetVersion());
				client->QueuePacket(outapp);
			}
		}
		else if(sep->arg[4][0] && strncasecmp("move", sep->arg[0], 4) == 0 && sep->IsNumber(1) && sep->IsNumber(2) && sep->IsNumber(3) && sep->IsNumber(4))
		{
			int16 from_index = atoi(sep->arg[1]);
			sint16 to_slot = atoi(sep->arg[2]);
			sint32 bag_id = atol(sep->arg[3]);
			int8 charges = atoi(sep->arg[4]);
			Item* item = client->GetPlayer()->item_list.GetItemFromIndex(from_index);

			if(bag_id == -4 && !client->GetPlayer()->item_list.SharedBankAddAllowed(item))
			{
				client->SimpleMessage(CHANNEL_COLOR_RED, "That item (or an item inside) cannot be shared.");
				return;
			}

			sint32 old_inventory_id = 0;

			if(item)
				old_inventory_id = item->details.inv_slot_id;

			//autobank
			if (bag_id == -3 && to_slot == -1) 
			{ 
				if (player->HasFreeBankSlot())
					to_slot = player->FindFreeBankSlot();
				else 
				{
					client->SimpleMessage(CHANNEL_COLOR_RED, "You do not have any free bank slots.");
					return;
				}
			}

			//auto inventory
			if (bag_id == 0 && to_slot == -1) 
			{
				if (!player->item_list.GetFirstFreeSlot(&bag_id, &to_slot)) 
				{
					client->SimpleMessage(CHANNEL_COLOR_RED, "You do not have any free slots.");
					return;
				}
			}

			EQ2Packet* outapp = client->GetPlayer()->MoveInventoryItem(bag_id, from_index, (int8)to_slot, charges, client->GetVersion());
			client->QueuePacket(outapp);

			//removed from bag send update
			if(old_inventory_id > 0 && item && item->details.inv_slot_id != old_inventory_id)
			{ 
				outapp = client->GetPlayer()->SendBagUpdate(old_inventory_id, client->GetVersion());
				if(outapp)
					client->QueuePacket(outapp);
			}

			if(item && item->details.inv_slot_id > 0 && item->details.inv_slot_id != old_inventory_id)
			{
				outapp = client->GetPlayer()->SendBagUpdate(item->details.inv_slot_id, client->GetVersion());

				if(outapp)
					client->QueuePacket(outapp);
			}
		}
		else if(sep->arg[1][0] && strncasecmp("equip", sep->arg[0], 5) == 0 && sep->IsNumber(1))
		{
			if(client->GetPlayer()->EngagedInCombat())
				client->SimpleMessage(CHANNEL_COLOR_RED, "You may not equip items while in combat.");
			else
			{
				int16 index = atoi(sep->arg[1]);
				int8 slot_id = 255;

				if(sep->arg[2][0] && sep->IsNumber(2))
					slot_id = atoi(sep->arg[2]);

				vector<EQ2Packet*> packets = client->GetPlayer()->EquipItem(index, client->GetVersion(), slot_id);
				EQ2Packet* outapp = 0;

				for(int32 i=0;i<packets.size();i++)
				{
					outapp = packets[i];
					if(outapp)
						client->QueuePacket(outapp);
				}

				client->GetPlayer()->ChangePrimaryWeapon();
				client->GetPlayer()->ChangeSecondaryWeapon();
				client->GetPlayer()->ChangeRangedWeapon();
				EQ2Packet* characterSheetPackets = client->GetPlayer()->GetPlayerInfo()->serialize(client->GetVersion());
				client->QueuePacket(characterSheetPackets);
			}
		}
		else if(sep->arg[1][0] && strncasecmp("unequip", sep->arg[0], 7) == 0 && sep->IsNumber(1))
		{
			if(client->GetPlayer()->EngagedInCombat())
				client->SimpleMessage(CHANNEL_COLOR_RED, "You may not unequip items while in combat.");
			else
			{
				int16 index = atoi(sep->arg[1]);
				sint32 bag_id = -999;
				int8 to_slot = 255;

				if(sep->arg[3][0])
				{
					if(sep->IsNumber(2))
						bag_id = atol(sep->arg[2]);

					if(sep->IsNumber(3))
						to_slot = atoi(sep->arg[3]);
				}

				vector<EQ2Packet*> packets = client->GetPlayer()->UnequipItem(index, bag_id, to_slot, client->GetVersion());
				EQ2Packet* outapp = 0;

				for(int32 i=0;i<packets.size();i++)
				{
					outapp = packets[i];

					if(outapp)
						client->QueuePacket(outapp);
				}

				client->GetPlayer()->ChangePrimaryWeapon();
				client->GetPlayer()->ChangeSecondaryWeapon();
				client->GetPlayer()->ChangeRangedWeapon();
				EQ2Packet* characterSheetPackets = client->GetPlayer()->GetPlayerInfo()->serialize(client->GetVersion());
				client->QueuePacket(characterSheetPackets);
			}
		}
		else if(sep->arg[2][0] && strncasecmp("swap_equip", sep->arg[0], 10) == 0 && sep->IsNumber(1) && sep->IsNumber(2))
		{
			LogWrite(MISC__TODO, 1, "TODO", " fix this, need to get how live does it\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
			int16 index1 = atoi(sep->arg[1]);
			int16 index2 = atoi(sep->arg[2]);
			EQ2Packet* outapp = client->GetPlayer()->SwapEquippedItems(index1, index2, client->GetVersion());

			if(outapp)
				client->QueuePacket(outapp);
			else
			{
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Unable to swap items");
				return;
			}
		}
		else if (sep->arg[2][0] && strncasecmp("pop", sep->arg[0], 3) == 0 && sep->IsNumber(1) && sep->IsNumber(2)) 
		{
			sint16 to_slot = atoi(sep->arg[1]);
			sint32 bag_id = atoi(sep->arg[2]);
			Item* item = client->GetPlayer()->item_list.GetOverflowItem();
			if (item) {
				//auto inventory
				if (bag_id == 0 && to_slot == -1) 
				{ 
					if (!player->item_list.GetFirstFreeSlot(&bag_id, &to_slot)) 
					{
						client->SimpleMessage(CHANNEL_ERROR, "You do not have any free slots.");
						return;
					}
					// Set the slot for the item
					item->details.inv_slot_id = bag_id;
					item->details.slot_id = to_slot;
					// Flag the item so it gets saved in its new location
					item->save_needed = true;
					// Add the item to its new location
					player->item_list.AddItem(item);
					// Remove the item from the overflow list
					player->item_list.RemoveOverflowItem(item);
					// Send the inventory update packet
					client->QueuePacket(player->item_list.serialize(player, client->GetVersion()));
					return;
				}
				else if (bag_id == -3 && to_slot == -1) {
					// Auto Bank
					if (!player->item_list.GetFirstFreeBankSlot(&bag_id, &to_slot)) {
						client->SimpleMessage(CHANNEL_STATUS, "You do not have any free bank slots.");
						return;
					}
					item->details.inv_slot_id = bag_id;
					item->details.slot_id = to_slot;
					item->save_needed = true;
					player->item_list.AddItem(item);
					player->item_list.RemoveOverflowItem(item);
					client->QueuePacket(player->item_list.serialize(player, client->GetVersion()));
				}
				else if (bag_id == -4) {
					// Shared Bank
					if (!player->item_list.SharedBankAddAllowed(item)) {
						client->SimpleMessage(CHANNEL_STATUS, "That item (or an item inside) cannot be shared.");
						return;
					}
					Item* tmp_item = player->item_list.GetItem(-4, to_slot);
					if (tmp_item) {
						client->SimpleMessage(CHANNEL_STATUS, "You can not place an overflow item into an occupied slot");
						return;
					}
					else {
						item->details.inv_slot_id = bag_id;
						item->details.slot_id = to_slot;
						item->save_needed = true;
						player->item_list.AddItem(item);
						player->item_list.RemoveOverflowItem(item);
						client->QueuePacket(player->item_list.serialize(player, client->GetVersion()));
						return;
					}
				}
				else {
					// Try to get an item from the given bag id and slot id
					Item* tmp_item = player->item_list.GetItem(bag_id, to_slot);
					// Check to see if we got an item, if we do send an error,
					// if we don't put the overflow item into this slot
					if (tmp_item) {
						client->SimpleMessage(CHANNEL_STATUS, "You can not place an overflow item into an occupied slot");
					}
					else {
						item->details.inv_slot_id = bag_id;
						item->details.slot_id = to_slot;
						item->save_needed = true;
						player->item_list.AddItem(item);
						player->item_list.RemoveOverflowItem(item);
						client->QueuePacket(player->item_list.serialize(player, client->GetVersion()));
						return;
					}
				}
			}
		}
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage: /inventory {destroy|move|equip|unequip|swap_equip|pop} {item_id} [to_slot] [bag_id]");

}

/* 
	Function: Command_IRC()
	Purpose	: Handle the IRC functions
	Params	: IRC Commands
	Dev		: Scatman
	Example	: /irc say #Channel Hello World!
*/ 
void Commands::Command_IRC(Client* client, Seperator* sep)
{
	if (!rule_manager.GetGlobalRule(R_World, IRCEnabled)->GetBool()) {
		client->SimpleMessage(CHANNEL_STATUS, "IRC is currently disabled on this server");
		return;
	}

	if (sep && sep->arg[0][0]) {
		if (strcasecmp(sep->arg[0], "say") == 0) {
			if (sep->arg[1][0] && sep->arg[2][0]) {
				if (sep->IsNumber(sep->arg[1]))
					irc.Say(client, atoul(sep->arg[1]), sep->argplus[2]);
				else
					irc.Say(client, sep->arg[1], sep->argplus[2]);
			}
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /irc say <channel name | channel index> <message>");
		}
		else if (strcasecmp(sep->arg[0], "join") == 0) {
			if (sep->arg[1][0])
				irc.JoinChannel(client, sep->arg[1]);
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /irc join <channel name>");
		}
		else if (strcasecmp(sep->arg[0], "leave") == 0) {
			if (sep->arg[1][0])
				irc.LeaveChannel(client, sep->arg[1]);
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /irc leave <channel name>");
		}
		else if (strcasecmp(sep->arg[0], "list") == 0)
			irc.ListChannels(client);
		else if (strcasecmp(sep->arg[0], "connect") == 0) {
			if (sep->arg[1][0] && sep->arg[2][0] && sep->IsNumber(2))
				irc.ConnectToServer(client, sep->arg[1], atoi(sep->arg[2]), sep->arg[3]);
			else
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /irc connect <host> <port> (nick)");
		}
		else if (strcasecmp(sep->arg[0], "disconnect") == 0)
			irc.DisconnectFromServer(client);
		else
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Unknown IRC command.");
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Unknown IRC command.");
}
/* 
	Function: Command_Languages()
	Purpose	: Show's languages the player knows
	Params	: 
	Dev		: Zcoretri
	Example	: 
*/ 
void Commands::Command_Languages(Client* client, Seperator* sep)
{
	list<Language*>* languages = client->GetPlayer()->GetPlayerLanguages()->GetAllLanguages();
	list<Language*>::iterator itr;
	Language* language;
	client->Message(CHANNEL_COLOR_WHITE, "You know the following languages:");

	for(itr = languages->begin(); itr != languages->end(); itr++)
	{
		language = *itr;
		client->Message(CHANNEL_COLOR_WHITE, "%s", language->GetName());
	}
}

/* 
	Function: Command_SetLanguage()
	Purpose	: Handles language commands
	Params	: Language ID
	Dev		: Zcoretri
	Example	: 
*/ 
void Commands::Command_SetLanguage(Client* client, Seperator* sep)
{
	Player* player = client->GetPlayer();

	if (sep && sep->arg[0])
	{
		if(!sep->IsNumber(0))
		{
			//String passed in
			const char* value = sep->arg[0];

			if(strncasecmp(value, "Common", strlen(value)) == 0)
			{
				database.SaveCharacterCurrentLang(0, client->GetCharacterID(), client);
				client->SendLanguagesUpdate(0);
				client->Message(CHANNEL_COLOR_WHITE, "You are now speaking %s", value);
			}
			else
			{
				if(player->HasLanguage(value))
				{
					Language* language = player->GetPlayerLanguages()->GetLanguageByName(value);
					database.SaveCharacterCurrentLang(language->GetID(), client->GetCharacterID(), client);
					client->SendLanguagesUpdate(language->GetID());
					client->Message(CHANNEL_COLOR_WHITE, "You are now speaking %s", language->GetName());
				}
				else
					client->Message(CHANNEL_COLOR_WHITE, "You do not know how to speak %s", value);
			}
		}
		else
		{
			//Number passed in
			int32 id = atoul(sep->arg[0]);

			if(player->HasLanguage(id))
			{
				Language* language = player->GetPlayerLanguages()->GetLanguage(id);
				database.SaveCharacterCurrentLang(id, client->GetCharacterID(), client);
				client->SendLanguagesUpdate(id);
				client->Message(CHANNEL_COLOR_WHITE, "You are now speaking %s", language->GetName());
			}
			else
			{
				Language* language = master_languages_list.GetLanguage(id);
				client->Message(CHANNEL_COLOR_WHITE, "You do not know how to speak %s", language->GetName());
			}
		}
	}
	else
	{
		//No value was passed in
		int32 id = database.GetCharacterCurrentLang(client->GetCharacterID(), player);

		if(id > 0)
		{
			Language* language = player->GetPlayerLanguages()->GetLanguage(id);
			client->Message(CHANNEL_COLOR_WHITE, "You are currently speaking %s ", language->GetName());
		}
		else
			client->Message(CHANNEL_COLOR_WHITE, "You are currently speaking Common");
	}
}

/* 
	Function: Command_LastName()
	Purpose	: Sets player surname
	Params	: Name text
	Dev		: theFoof
	Example	: 
*/ 
void Commands::Command_LastName(Client* client, Seperator* sep)
{
	if (!client)
		return;

	if (sep && sep->arg[0])
	{
		if (!client->GetPlayer()->get_character_flag(CF_ENABLE_CHANGE_LASTNAME)){
			client->Message(CHANNEL_COLOR_YELLOW, "You must be atleast level %i to change your last name.", rule_manager.GetGlobalRule(R_Player, MinLastNameLevel)->GetInt8());
			return;
		}
		client->RemovePendingLastName();
		string last_name = (string)sep->arg[0];
		int8 max_length = rule_manager.GetGlobalRule(R_Player, MaxLastNameLength)->GetInt8();
		int8 min_length = rule_manager.GetGlobalRule(R_Player, MinLastNameLength)->GetInt8();
		if (last_name.length() <= max_length && last_name.length() >= min_length){
			client->SetPendingLastName(last_name);
			client->SendLastNameConfirmation();
		}
		else
			client->Message(CHANNEL_COLOR_YELLOW, "Your last name must be between %i and %i characters long.", min_length, max_length);
	}
}

/* 
	Function: Command_ConfirmLastName()
	Purpose	: Confirms setting of player surname
	Params	: Name text
	Dev		: theFoof
	Example	: 
*/ 
void Commands::Command_ConfirmLastName(Client* client, Seperator* sep)
{
	if (!client)
		return;

	string* name = client->GetPendingLastName();
	if (name){
		Player* player = client->GetPlayer();
		player->SetLastName(name->c_str(), false);
		client->SendTitleUpdate();
		player->SetCharSheetChanged(true);
		client->RemovePendingLastName();
	}
}

/* 
	Function: Command_Location()
	Purpose	: Display's Help for /location commands
	Params	: 
	Dev		: Scatman
	Example	: /location = show's help for command
*/ 
void Commands::Command_Location(Client* client)
{
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Valid /location commands are:");
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "/location create [name] (include y).  Include y defaults to false");
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "/location add [location id]");
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "/location remove [location point id]");
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "/location delete [location id]");
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "/location list [locations|points] [location id if points used]");
}

/* 
	Function: Command_LocationAdd()
	Purpose	: Add's a location to an existing location config
	Params	: location_id
	Dev		: Scatman
	Example	: /location add {location_id}
*/ 
void Commands::Command_LocationAdd(Client* client, Seperator* sep)
{
	if (sep && sep->arg[0] && sep->IsNumber(0)) 
	{
		int32 location_id = atoul(sep->arg[0]);
		float x = client->GetPlayer()->GetX();
		float y = client->GetPlayer()->GetY();
		float z = client->GetPlayer()->GetZ();

		if (database.AddLocationPoint(location_id, x, y, z))
			client->Message(CHANNEL_COLOR_YELLOW, "Point (%f, %f, %f) was successfully added to location %u", x, y, z, location_id);
		else
			client->Message(CHANNEL_COLOR_YELLOW, "A location with ID %u does not exist.  Use /location create to create one", location_id);
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /location add [location id]");
}

/* 
	Function: Command_LocationCreate()
	Purpose	: Creates a new location config
	Params	: location name, 0/1 for include_y (optional)
	Dev		: Scatman 
	Example	: /location create Test 1 = creates a new location named Test with include_y True
*/ 
void Commands::Command_LocationCreate(Client* client, Seperator* sep)
{
	if (sep && sep->arg[0] && strlen(sep->arg[0]) > 0) 
	{
		const char* name = sep->arg[0];
		bool include_y = false;

		if (sep->arg[1] && sep->IsNumber(1) && atoi(sep->arg[1]) > 0)
			include_y = true;

		int32 location_id = database.CreateLocation(client->GetPlayer()->GetZone()->GetZoneID(), client->GetPlayer()->appearance.pos.grid_id, name, include_y);

		if (location_id > 0)
			client->Message(CHANNEL_COLOR_YELLOW, "Location '%s' was successfully created with location id %u", name, location_id);
		else
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "There was an error creating the requested location");
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /location create [name] (include_y).  Include y defaults to false");
}

/* 
	Function: Command_LocationDelete()
	Purpose	: Delete's a location config and all it's location points
	Params	: location_id
	Dev		: Scatman
	Example	: /location delete {location_id}
*/ 
void Commands::Command_LocationDelete(Client* client, Seperator* sep)
{
	if (sep && sep->arg[0] && sep->IsNumber(0))
	{
		int32 location_id = atoul(sep->arg[0]);

		if (database.DeleteLocation(location_id))
			client->Message(CHANNEL_COLOR_YELLOW, "Location id %u and all its points were successfully deleted", location_id);
		else
			client->Message(CHANNEL_COLOR_YELLOW, "A location with ID %u does not exist", location_id);
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /location delete [location id]");
}

/* 
	Function: Command_LocationList()
	Purpose	: Display's a list of location points
	Params	: location_id
	Dev		: Scatman
	Example	: /location list {location_id}
*/ 
void Commands::Command_LocationList(Client* client, Seperator* sep)
{
	if (sep && sep->arg[0]) 
	{
		const char* option = sep->arg[0];

		if (strncmp(option, "locations", strlen(option)) == 0)
			database.ListLocations(client);
		else if (strncmp(option, "points", strlen(option)) == 0 && sep->arg[1] && sep->IsNumber(1)) 
		{
			int32 location_id = atoul(sep->arg[1]);
			database.ListLocationPoints(client, location_id);
		}
		else
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Useage: /location list [locations|points] [location ID if points used]");
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Useage: /location list [locations|points] [location ID if points used]");
}

/* 
	Function: Command_LocationRemove()
	Purpose	: Removes a single location point from a location config
	Params	: location_point_id (gotten from /location list {id})
	Dev		: Scatman
	Example	: /location remove 1 = will remove location_point_id 1
*/ 
void Commands::Command_LocationRemove(Client* client, Seperator* sep)
{
	if (sep && sep->arg[0] && sep->IsNumber(0)) 
	{
		int32 location_point_id = atoul(sep->arg[0]);

		if (database.DeleteLocationPoint(location_point_id))
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Location point was successfully deleted");
		else
			client->Message(CHANNEL_COLOR_YELLOW, "Location point with ID %u does not exist", location_point_id);
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /location remove [location point id]");
}

/* 
	Function: Command_Merchant()
	Purpose	: Handles Merchant commands
	Params	: list
	Dev		: Scatman
	Example	: /merchant list
*/ 
void Commands::Command_Merchant(Client* client, Seperator* sep, int handler)
{
	if( handler == COMMAND_MERCHANT )
	{
		// /merchant command by itself shows help (to be extended)
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /merchant list [merchant description]");
		return;
	}
	else if( handler == COMMAND_MERCHANT_LIST )
	{
		// /merchant list command expects "description" param
		if (sep && sep->arg[0])
		{
			const char* merchant_description = sep->argplus[0];
			client->Message(CHANNEL_COLOR_YELLOW, "Listing merchants like '%s':", merchant_description);
			map<int32, MerchantInfo*>* merchant_info = world.GetMerchantInfo();
			map<int32, MerchantInfo*>::iterator itr;

			for (itr = merchant_info->begin(); itr != merchant_info->end(); itr++) 
			{
				string description = database.GetMerchantDescription(itr->first);

				if (ToLower(description).find(ToLower(string(merchant_description))) < 0xFFFFFFFF)
					client->Message(CHANNEL_COLOR_YELLOW, "Merchant ID: %u, Description: %s", itr->first, description.c_str());
			}
		}
		else // no description
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /merchant list [merchant description]");
	}
}

/* 
	Function: Command_Modify()
	Purpose	: to replace our other "set" commands
	Params	: System = the system to modify, Action = the action to perform, Target = what to change (target, or index)
	Dev		: John Adams
	Example	: /modify spell set name "Aegolism III"
*/ 
void Commands::Command_Modify(Client* client)
{
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /modify [system] [action] [field] [value] {target|id}");
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Systems: character, faction, guild, item, skill, spawn, spell, zone");
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Actions: set, create, delete, add, remove");
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Value  : field name in the table being modified");
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Target : modify current target, or specified {system} ID");
}


/* 
	Function: Command_ModifySpawn()
	Purpose	: replace "spawn set" commands
	Params	: Action	: the action to perform (or special handler)
			: Field		: the DB field to change
			: Value		: what to set the value to
			: Target	: what object to change (my target, or spawn_id)
	Dev		: John Adams
	Example	: /modify spawn set name "Lady Vox"
			:
	Note	: Special Handlers, like "zoneto" for Signs
			:	/modify spawn zoneto
			:	Will set a sign's zone x/y/z to my current coords
*/ 
void Commands::Command_ModifySpawn(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_MODIFY");

	// need at least 2 args for a valid command
	if( sep && sep->arg[1] )
	{
		// JA: just a quick implementation because I need it :)
		if (strcmp(sep->arg[0], "zoneto") == 0) 
		{
			if( sep->IsNumber(1) )
			{
				int32 spawn_id = atoul(sep->arg[1]);
				float x_coord = client->GetPlayer()->GetX();
				float y_coord = client->GetPlayer()->GetY();
				float z_coord = client->GetPlayer()->GetZ();
				float h_coord = client->GetPlayer()->GetHeading();

				database.SaveSignZoneToCoords(spawn_id, x_coord, y_coord, z_coord, h_coord);
			}
			else
				client->SimpleMessage(CHANNEL_COLOR_RED, "Usage: /modify spawn zoneto spawn_id - sets spawn_id to your coords");
		}
	}
	else
		Command_Modify(client);
}

/* 
	Function: Command_ModifyCharacter()
	Purpose	: to replace our other "set" commands
	Params	: Action	: Add, Remove
			: Field		: copper, silver, gold, plat
			: Value		: min 1, max unlimited
			: Target	: Self, Player
	Dev		: Cynnar
	Example	: /modify character add gold 50
			: /modify character remove silver 25
*/
void Commands::Command_ModifyCharacter(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_MODIFY");

	int64 value = 0;

	Player* player = client->GetPlayer();
	if (player->HasTarget() && player->GetTarget()->IsPlayer())
		player = (Player*)player->GetTarget();

	// need at least 2 args for a valid command
	if( sep && sep->arg[1] )

	{
		if (strcmp(sep->arg[0], "add") == 0)
			
		{
			if (strcmp(sep->arg[1], "copper") == 0)
			{
				value = atoi64(sep->arg[2]);
				player->AddCoins(value);
				if (client->GetPlayer() == player)
				{
					client->Message(CHANNEL_COLOR_YELLOW, "You give yourself %llu copper coin%s", value, (value > 1 ? "s" : ""));
				}
				else {
					client->Message(CHANNEL_COLOR_YELLOW, "You give %s %llu copper coin%s", player->GetName(), value, (value > 1 ? "s" : ""));
					client->GetCurrentZone()->GetClientBySpawn(player)->Message(CHANNEL_COLOR_YELLOW, "%s gave you %llu copper coin%s", client->GetPlayer()->GetName(), value, (value > 1 ? "s" : ""));
				}
			}

			else if (strcmp(sep->arg[1], "silver") == 0)
			{
				value = atoi64(sep->arg[2]) * 100;
				player->AddCoins(value);
				if (client->GetPlayer() == player)
				{
					client->Message(CHANNEL_COLOR_YELLOW, "You give yourself %llu silver coin%s", value / 100, (value > 1 ? "s" : ""));
				}
				else {
					client->Message(CHANNEL_COLOR_YELLOW, "You give %s %llu silver coin%s", player->GetName(), value / 100, (value > 1 ? "s" : ""));
					client->GetCurrentZone()->GetClientBySpawn(player)->Message(CHANNEL_COLOR_YELLOW, "%s gave you %llu silver coin%s", client->GetPlayer()->GetName(), value / 100, (value > 1 ? "s" : ""));
				}
			}

			else if (strcmp(sep->arg[1], "gold") == 0)
			{
				value = atoi64(sep->arg[2]) * 10000;
				player->AddCoins(value);
				if (client->GetPlayer() == player)
				{
					client->Message(CHANNEL_COLOR_YELLOW, "You give yourself %llu gold coin%s", value / 10000, (value > 1 ? "s" : ""));
				}
				else {
					client->Message(CHANNEL_COLOR_YELLOW, "You give %s %llu gold coin%s", player->GetName(), value / 10000, (value > 1 ? "s" : ""));
					client->GetCurrentZone()->GetClientBySpawn(player)->Message(CHANNEL_COLOR_YELLOW, "%s gave you %llu gold coin%s", client->GetPlayer()->GetName(), value / 10000, (value > 1 ? "s" : ""));
				}
			}

			else if (strcmp(sep->arg[1], "plat") == 0)
			{
				value = atoi64(sep->arg[2]) * 1000000;
				player->AddCoins(value);
				if (client->GetPlayer() == player)
				{
					client->Message(CHANNEL_COLOR_YELLOW, "You give yourself %llu platinum coin%s", value / 1000000, (value > 1 ? "s" : ""));
				}
				else {
					client->Message(CHANNEL_COLOR_YELLOW, "You give %s %llu platinum coin%s", player->GetName(), value / 1000000, (value > 1 ? "s" : ""));
					client->GetCurrentZone()->GetClientBySpawn(player)->Message(CHANNEL_COLOR_YELLOW, "%s gave you %llu platinum coin%s", client->GetPlayer()->GetName(), value / 1000000, (value > 1 ? "s" : ""));
				}
			}

			else
			{
				client->SimpleMessage(CHANNEL_COLOR_RED, "Usage: /modify character [action] [field] [value]");
				client->SimpleMessage(CHANNEL_COLOR_RED, "Actions: add, remove");
				client->SimpleMessage(CHANNEL_COLOR_RED, "Value  : copper, silver, gold, plat");
				client->SimpleMessage(CHANNEL_COLOR_RED, "Example: /modify character add copper 20");
			}
		}

		else if (strcmp(sep->arg[0], "remove") == 0)
		{
			if (strcmp(sep->arg[1], "copper") == 0)
			{
				value = atoi64(sep->arg[2]);
				player->RemoveCoins(value);
				if (client->GetPlayer() == player)
				{
					client->Message(CHANNEL_COLOR_YELLOW, "You take %llu copper coin%s from yourself", value, (value > 1 ? "s" : ""));
				}
				else {
					client->Message(CHANNEL_COLOR_YELLOW, "You take %llu copper coin%s from %s", value, (value > 1 ? "s" : ""), player->GetName());
					client->GetCurrentZone()->GetClientBySpawn(player)->Message(CHANNEL_COLOR_YELLOW, "%s takes %llu copper coin%s from you", client->GetPlayer()->GetName(), value, (value > 1 ? "s" : ""));
				}
			}

			else if (strcmp(sep->arg[1], "silver") == 0)
			{
				value = atoi64(sep->arg[2]) * 100;
				player->RemoveCoins(value);
				if (client->GetPlayer() == player)
				{
					client->Message(CHANNEL_COLOR_YELLOW, "You take %llu silver coin%s from yourself", value / 100, (value > 1 ? "s" : ""));
				}
				else {
					client->Message(CHANNEL_COLOR_YELLOW, "You take %llu silver coin%s from %s", value / 100, (value > 1 ? "s" : ""), player->GetName());
					client->GetCurrentZone()->GetClientBySpawn(player)->Message(CHANNEL_COLOR_YELLOW, "%s takes %llu silver coin%s from you", client->GetPlayer()->GetName(), value / 100, (value > 1 ? "s" : ""));
				}
			}

			else if (strcmp(sep->arg[1], "gold") == 0)
			{
				value = atoi64(sep->arg[2]) * 10000;
				player->RemoveCoins(value);
				if (client->GetPlayer() == player)
				{
					client->Message(CHANNEL_COLOR_YELLOW, "You take %llu gold coin%s from yourself", value / 10000, (value > 1 ? "s" : ""));
				}
				else {
					client->Message(CHANNEL_COLOR_YELLOW, "You take %llu gold coin%s from %s", value / 10000, (value > 1 ? "s" : ""), player->GetName());
					client->GetCurrentZone()->GetClientBySpawn(player)->Message(CHANNEL_COLOR_YELLOW, "%s takes %llu gold coin%s from you", client->GetPlayer()->GetName(), value / 10000, (value > 1 ? "s" : ""));
				}
			}

			else if (strcmp(sep->arg[1], "plat") == 0)
			{
				value = atoi64(sep->arg[2]) * 1000000;
				player->RemoveCoins(value);
				if (client->GetPlayer() == player)
				{
					client->Message(CHANNEL_COLOR_YELLOW, "You take %llu platinum coin%s from yourself", value / 1000000, (value > 1 ? "s" : ""));
				}
				else {
					client->Message(CHANNEL_COLOR_YELLOW, "You take %llu platinum coin%s from %s", value / 1000000, (value > 1 ? "s" : ""), player->GetName());
					client->GetCurrentZone()->GetClientBySpawn(player)->Message(CHANNEL_COLOR_YELLOW, "%s takes %llu platinum coin%s from you", client->GetPlayer()->GetName(), value / 1000000, (value > 1 ? "s" : ""));
				}
			}

			else
			{
				client->SimpleMessage(CHANNEL_COLOR_RED, "Usage: /modify character [action] [field] [value]");
				client->SimpleMessage(CHANNEL_COLOR_RED, "Actions: add, remove");
				client->SimpleMessage(CHANNEL_COLOR_RED, "Value  : copper, silver, gold, plat");
				client->SimpleMessage(CHANNEL_COLOR_RED, "Example: /modify character remove gold 15");
			}
		}
	}
	else
		Command_Modify(client);
}


void Commands::Command_ModifyFaction(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_MODIFY");

	// need at least 2 args for a valid command
	if( sep && sep->arg[1] )
	{
	}
	else
		Command_Modify(client);
}


void Commands::Command_ModifyGuild(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_MODIFY");

	// need at least 2 args for a valid command
	if( sep && sep->arg[1] )
	{
	}
	else
		Command_Modify(client);
}


void Commands::Command_ModifyItem(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_MODIFY");

	// need at least 2 args for a valid command
	if( sep && sep->arg[1] )
	{
	}
	else
		Command_Modify(client);
}

/* 
	Function: Command_ModifyQuest()
	Purpose	: to list players quest and completed quests
	Params	: Action	: list, completed
			: Target	: Self, Player
	Dev		: Cynnar
	Example	: /modify quest list
			: /modify quest completed
*/

void Commands::Command_ModifyQuest(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_MODIFY");

	int64 value = 0;

	Player* player = client->GetPlayer();
	if (player->HasTarget() && player->GetTarget()->IsPlayer())
		player = (Player*)player->GetTarget();

	// need at least 2 args for a valid command

	if( sep && sep->arg[1] )
	{
		
		if (strcmp(sep->arg[0], "list") == 0)
		{
			map<int32, Quest*>* quests = player->GetPlayerQuests();
			map<int32, Quest*>::iterator itr;
			client->Message(CHANNEL_COLOR_YELLOW, "%s's Quest List:.", client->GetPlayer()->GetName());
			if (quests->size() == 0)
				client->Message(CHANNEL_COLOR_YELLOW, "%s has no quests.", client->GetPlayer()->GetName());
			else
			{
				for( itr = quests->begin(); itr != quests->end(); itr++ )
				{
					Quest* quest = itr->second;
					client->Message(CHANNEL_COLOR_YELLOW, "%u) %s", itr->first, quest->GetName());
				}
			}
		
		}

		else if (strcmp(sep->arg[0], "completed") == 0)
		{

			map<int32, Quest*>* quests = player->GetCompletedPlayerQuests();
			map<int32, Quest*>::iterator itr;
			client->Message(CHANNEL_COLOR_YELLOW, "%s's Completed Quest List:.", client->GetPlayer()->GetName());
			if (quests->size() == 0)
				client->Message(CHANNEL_COLOR_YELLOW, "%s has no completed quests.", client->GetPlayer()->GetName());
			else
			{
				for( itr = quests->begin(); itr != quests->end(); itr++ )
				{
				Quest* quest = itr->second;
				client->Message(CHANNEL_COLOR_YELLOW, "%u) %s", itr->first, quest->GetName());
				}
			}
		
		}
		// Add in a progress step, and a LogWrite() for tracking GM Commands.
		// LogWrite(LUA__DEBUG, 0, "LUA", "Quest: %s, function: %s", quest->GetName(), function);
		else if (strcmp(sep->arg[0], "remove") == 0)
		{
			int32 quest_id = 0;

			if (sep && sep->arg[1] && sep->IsNumber(1))
				quest_id = atoul(sep->arg[1]);

			if (quest_id > 0)
			{
				if (lua_interface && client->GetPlayer()->player_quests.count(quest_id) > 0)
				{
					Quest* quest = client->GetPlayer()->player_quests[quest_id];
					if (quest)
						if (client->GetPlayer() == player)
						{
							client->Message(CHANNEL_COLOR_YELLOW, "The quest %s has been removed from your journal", quest->GetName());
						}
						else
						{
							client->Message(CHANNEL_COLOR_YELLOW, "You have removed the quest %s from %s's journal", quest->GetName(), player->GetName());
							client->GetCurrentZone()->GetClientBySpawn(player)->Message(CHANNEL_COLOR_YELLOW, "%s has removed the quest %s from your journal", client->GetPlayer()->GetName(), quest->GetName());
						}
						LogWrite(COMMAND__INFO, 0, "GM Command", "%s removed the quest %s from %s", client->GetPlayer()->GetName(), quest->GetName(), player->GetName());
						lua_interface->CallQuestFunction(quest, "Deleted", client->GetPlayer());
				}
				client->RemovePlayerQuest(quest_id);
				client->GetCurrentZone()->SendQuestUpdates(client);
			}	
		}

		else
		{
			client->SimpleMessage(CHANNEL_COLOR_RED, "Usage: /modify quest [action] [quest id]");
			client->SimpleMessage(CHANNEL_COLOR_RED, "Actions: list, completed, remove");
			client->SimpleMessage(CHANNEL_COLOR_RED, "Example: /modify quest list");
			client->SimpleMessage(CHANNEL_COLOR_RED, "Example: /modify quest remove 156");
		}
	}

	else
		Command_Modify(client);
}


void Commands::Command_ModifySkill(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_MODIFY");

	// need at least 2 args for a valid command
	if( sep && sep->arg[1] )
	{
		if (strcmp(sep->arg[0], "add") == 0) {
			const char* skill_name = sep->argplus[1];
			Skill* skill = master_skill_list.GetSkillByName(skill_name);

			if (skill) {
				Player* player = 0;
				Client* to_client = 0;

				if (client->GetPlayer()->GetTarget() && client->GetPlayer()->GetTarget()->IsPlayer()) {
					player = (Player*)client->GetPlayer()->GetTarget();
					to_client = client->GetCurrentZone()->GetClientBySpawn(player);
				}
				else {
					player = client->GetPlayer();
					to_client = client;
				}

				if (player)
				{
					if (!player->GetSkills()->HasSkill(skill->skill_id))
					{
						player->AddSkill(skill->skill_id, 1, player->GetLevel() * 5, true);
						if (to_client == client) {
							client->Message(CHANNEL_STATUS, "Added skill '%s'.", skill_name);
						}
						else {
							client->Message(CHANNEL_STATUS, "You gave skill '%s' to player '%s'.", skill_name, player->GetName());
							to_client->Message(CHANNEL_STATUS, "%s gave you skill '%s'.", client->GetPlayer()->GetName(), skill_name);
						}
					}
					else
						client->Message(CHANNEL_STATUS, "%s already has the skill '%s'.", to_client == client ? "You" : player->GetName(), skill_name);
				}
			}
			else
				client->Message(CHANNEL_STATUS, "The skill '%s' does not exist.", skill_name);
		}
		else if (strcmp(sep->arg[0], "remove") == 0) {
			const char* skill_name = sep->argplus[1];
			Skill* skill = master_skill_list.GetSkillByName(skill_name);

			if (skill) {
				Player* player = 0;
				Client* to_client = 0;

				if (client->GetPlayer()->GetTarget() && client->GetPlayer()->GetTarget()->IsPlayer()) {
					player = (Player*)client->GetPlayer()->GetTarget();
					to_client = client->GetCurrentZone()->GetClientBySpawn(player);
				}
				else {
					player = client->GetPlayer();
					to_client = client;
				}

				if (player)
				{
					if (player->GetSkills()->HasSkill(skill->skill_id))
					{
						player->RemoveSkillFromDB(skill, true);
						if (client == to_client) {
							client->Message(CHANNEL_STATUS, "Removed skill '%s'.", skill_name);
						}
						else {
							client->Message(CHANNEL_STATUS, "Removed skill '%s' from player %s.", skill_name, player->GetName());
							to_client->Message(CHANNEL_STATUS, "%s has removed skill '%s' from you.", client->GetPlayer()->GetName(), skill_name);
						}
					}
					else {
						if (client == to_client)
							client->Message(CHANNEL_STATUS, "You do not have the skill '%s'.", skill_name);
						else
							client->Message(CHANNEL_STATUS, "Player '%s' does not have the skill '%s'.", player->GetName(), skill_name);
					}
				}
			}
			else
				client->Message(CHANNEL_STATUS, "The skill '%s' does not exist.", skill_name);
		}
		else if (strcmp(sep->arg[0], "set") == 0) {
			if (!sep->IsNumber(2)) {
				client->Message(CHANNEL_STATUS, "The last parameter must be a number.");
				return;
			}

			const char* skill_name = sep->arg[1];
			Skill* skill = master_skill_list.GetSkillByName(skill_name);
			if (skill) {
				int16 val = atoi(sep->arg[2]);
				Player* player = 0;
				Client* to_client = 0;

				if (client->GetPlayer()->GetTarget() && client->GetPlayer()->GetTarget()->IsPlayer()) {
					player = (Player*)client->GetPlayer()->GetTarget();
					to_client = client->GetCurrentZone()->GetClientBySpawn(player);
				}
				else {
					player = client->GetPlayer();
					to_client = client;
				}

				if (player)
				{
					if (player->GetSkills()->HasSkill(skill->skill_id)) {
						player->GetSkills()->SetSkill(skill->skill_id, val);
						if (client != to_client)
							client->Message(CHANNEL_STATUS, "You set %s's '%s' skill to %i.", player->GetName(), skill_name, val);

						to_client->Message(CHANNEL_STATUS, "Your '%s' skill has been set to %i.", skill_name, val);
					}
					else {
						client->Message(CHANNEL_STATUS, "Target does not have the skill '%s'.", skill_name);
					}
				}
			}
			else {
				client->Message(CHANNEL_STATUS, "The skill '%s' does not exist.", skill_name);
			}
		}
		else {
			client->SimpleMessage(CHANNEL_STATUS, "Usage: /modify skill [action] [skill]");
			client->SimpleMessage(CHANNEL_STATUS, "Actions: add, remove, set");
			client->SimpleMessage(CHANNEL_STATUS, "Example: /modify skill add parry");
		}
	}
	else {
		client->SimpleMessage(CHANNEL_STATUS, "Usage: /modify skill [action] [skill]");
		client->SimpleMessage(CHANNEL_STATUS, "Actions: add, remove, set");
		client->SimpleMessage(CHANNEL_STATUS, "Example: /modify skill add parry");
	}
}


void Commands::Command_ModifySpell(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_MODIFY");

	// need at least 2 args for a valid command
	if( sep && sep->arg[1] )
	{
	}
	else
		Command_Modify(client);
}


void Commands::Command_ModifyZone(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_MODIFY");

	// need at least 2 args for a valid command
	if( sep && sep->arg[1] )
	{
	}
	else
		Command_Modify(client);
}


/* 
	Function: Command_MOTD()
	Purpose	: Displays server MOTD
	Params	: 
	Dev		: LethalEncounter
	Example	: /motd
*/ 
void Commands::Command_MOTD(Client* client)
{
	if (client)
		ClientPacketFunctions::SendMOTD(client);
}

/* 
	Function: Command_Pet()
	Purpose	: Handle Pet {Command} commands
	Params	: attack, backoff, preserve_master, preserve_self, follow, stay, getlost
	Dev		: 
	Example	: /pet preserve_master
*/ 
void Commands::Command_Pet(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_PET");
	//LogWrite(MISC__TODO, 1, "Command", "TODO-Command: Pet Commands");
	//client->Message(CHANNEL_COLOR_YELLOW, "Pets are not yet implemented.");

	if (strcmp(sep->arg[0], "hide") == 0) {
		// doing /pet hide will toggle the hide status on all the pets that can be hidden
		Entity* pet = client->GetPlayer()->GetDeityPet();
		if (pet) {
			if (pet->IsPrivateSpawn())
				client->GetPlayer()->HideDeityPet(false);
			else
				client->GetPlayer()->HideDeityPet(true);
		}

		pet = client->GetPlayer()->GetCosmeticPet();
		if (pet) {
			if (pet->IsPrivateSpawn())
				client->GetPlayer()->HideCosmeticPet(false);
			else
				client->GetPlayer()->HideCosmeticPet(true);
		}

		return;
	}

	// below is for all combat pets
	if (!client->GetPlayer()->HasPet()) {
		client->Message(CHANNEL_COLOR_YELLOW, "You do not have a pet.");
		return;
	}
	
	if (strcmp(sep->arg[0], "stay") == 0) {
		client->Message(CHANNEL_COLOR_YELLOW, "You command your pet to stay.");
		client->GetPlayer()->GetInfoStruct()->pet_movement = 1;
		client->GetPlayer()->SetCharSheetChanged(true);
		if (client->GetPlayer()->GetPet())
			client->GetPlayer()->GetPet()->following = false;
		if (client->GetPlayer()->GetCharmedPet())
			client->GetPlayer()->GetCharmedPet()->following = false;
	}
	else if (strcmp(sep->arg[0], "follow") == 0) {
		client->Message(CHANNEL_COLOR_YELLOW, "You command your pet to follow.");
		client->GetPlayer()->GetInfoStruct()->pet_movement = 2;
		client->GetPlayer()->SetCharSheetChanged(true);
	}
	else if (strcmp(sep->arg[0], "preserve_master") == 0) {
		if (client->GetPlayer()->GetInfoStruct()->pet_behavior & 1) {
			client->Message(CHANNEL_COLOR_YELLOW, "Your pet will no longer protect you.");
			client->GetPlayer()->GetInfoStruct()->pet_behavior -= 1;
		}
		else {
			client->Message(CHANNEL_COLOR_YELLOW, "You command your pet to protect you.");
			client->GetPlayer()->GetInfoStruct()->pet_behavior += 1;
		}
		client->GetPlayer()->SetCharSheetChanged(true);
	}
	else if (strcmp(sep->arg[0], "preserve_self") == 0) {
		if (client->GetPlayer()->GetInfoStruct()->pet_behavior & 2) {
			client->Message(CHANNEL_COLOR_YELLOW, "Your pet will no longer protect itself.");
			client->GetPlayer()->GetInfoStruct()->pet_behavior -= 2;
		}
		else {
			client->Message(CHANNEL_COLOR_YELLOW, "You command your pet to protect itself.");
			client->GetPlayer()->GetInfoStruct()->pet_behavior += 2;
		}
		client->GetPlayer()->SetCharSheetChanged(true);
	}
	else if (strcmp(sep->arg[0], "backoff") == 0) {
		client->Message(CHANNEL_COLOR_YELLOW, "You command your pet to back down.");
		if (client->GetPlayer()->GetPet())
			((NPC*)client->GetPlayer()->GetPet())->Brain()->ClearHate();
		if (client->GetPlayer()->GetCharmedPet())
			((NPC*)client->GetPlayer()->GetCharmedPet())->Brain()->ClearHate();
		client->GetPlayer()->GetInfoStruct()->pet_behavior = 0;
		client->GetPlayer()->SetCharSheetChanged(true);
	}
	else if (strcmp(sep->arg[0], "attack") == 0) {
		if (client->GetPlayer()->HasTarget() && client->GetPlayer()->GetTarget()->IsEntity()) {
			if (client->GetPlayer()->AttackAllowed((Entity*)client->GetPlayer()->GetTarget())){
				client->Message(CHANNEL_COLOR_YELLOW, "You command your pet to attack your target.");
				if (client->GetPlayer()->GetPet())
					client->GetPlayer()->GetPet()->AddHate((Entity*)client->GetPlayer()->GetTarget(), 1);
				if (client->GetPlayer()->GetCharmedPet())
					client->GetPlayer()->GetCharmedPet()->AddHate((Entity*)client->GetPlayer()->GetTarget(), 1);
			}
			else
				client->Message(CHANNEL_COLOR_YELLOW, "You can not attack that.");
		}
		else
			client->Message(CHANNEL_COLOR_YELLOW, "You do not have a target.");

	}
	else if (strcmp(sep->arg[0], "getlost") == 0) {
		client->GetPlayer()->DismissPet((NPC*)client->GetPlayer()->GetPet());
		client->GetPlayer()->DismissPet((NPC*)client->GetPlayer()->GetCharmedPet());

		client->Message(CHANNEL_COLOR_YELLOW, "You tell your pet to get lost.");
	}
	else
		client->Message(CHANNEL_COLOR_YELLOW, "Unknown pet command.");
	
}

/* 
	Function: Command_PetName()
	Purpose	: Pet your name (???)
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_PetName(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_PETNAME");
	LogWrite(MISC__TODO, 1, "Command", "TODO-Command: Pet Name Command");
	client->Message(CHANNEL_COLOR_YELLOW, "Pets are not yet implemented.");
}

/* 
	Function: Command_NamePet()
	Purpose	: Name your pet
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_NamePet(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_NAME_PET");
	LogWrite(MISC__TODO, 1, "Command", "TODO-Command: Name Pet Command");
	client->Message(CHANNEL_COLOR_YELLOW, "Pets are not yet implemented.");
}

/* 
	Function: Command_Rename()
	Purpose	: Renames existing pet
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_Rename(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_RENAME");
	LogWrite(MISC__TODO, 1, "Command", "TODO-Command: Rename Pet Command");
	client->Message(CHANNEL_COLOR_YELLOW, "Pets are not yet implemented.");
}

/* 
	Function: Command_ConfirmRename()
	Purpose	: Confirms renaming your pet
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_ConfirmRename(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_CONFIRMRENAME");
	LogWrite(MISC__TODO, 1, "Command", "TODO-Command: Confirm Rename Pet Command");
	client->Message(CHANNEL_COLOR_YELLOW, "Pets are not yet implemented.");
}

/* 
	Function: Command_PetOptions()
	Purpose	: Sets various pet options
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_PetOptions(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_PETOPTIONS");

	Player* player = client->GetPlayer();
	Spawn* target = player->GetTarget();
	if (!target)
		return;

	if (target == player->GetPet())
		client->SendPetOptionsWindow(player->GetPet()->GetName());
	else if (target == player->GetCharmedPet())
		client->SendPetOptionsWindow(player->GetCharmedPet()->GetName());
	else if (target == player->GetCosmeticPet())
		client->SendPetOptionsWindow(player->GetCosmeticPet()->GetName(), 0);
	else if (target == player->GetDeityPet())
		client->SendPetOptionsWindow(player->GetDeityPet()->GetName(), 0);
}

/* 
	Function: Command_Random()
	Purpose	: Handles /random dice roll in-game
	Params	: 1-100
	Dev		: Scatman
	Example	: /randon 1 100
*/ 
void Commands::Command_Random(Client* client, Seperator* sep)
{
	char message[256] = {0};

	if (sep)
	{
		if (sep->GetArgNumber() == 0 && sep->IsNumber(0))
			sprintf(message, "Random: %s rolls 1 to %i on the magic dice...and scores a %i!", client->GetPlayer()->GetName(), atoi(sep->arg[0]), MakeRandomInt(1, atoi(sep->arg[0])));
		else if (sep->GetArgNumber() > 0 && sep->IsNumber(0) && sep->IsNumber(1))
			sprintf(message, "Random: %s rolls from %i to %i on the magic dice...and scores a %i!", client->GetPlayer()->GetName(), atoi(sep->arg[0]), atoi(sep->arg[1]), MakeRandomInt(atoi(sep->arg[0]), atoi(sep->arg[1])));
		else
			sprintf(message, "Random: %s rolls from 1 to 100 on the magic dice...and scores a %i!", client->GetPlayer()->GetName(), MakeRandomInt(1, 100));
	}
	else
		sprintf(message, "Random: %s rolls from 1 to 100 on the magic dice...and scores a %i!", client->GetPlayer()->GetName(), MakeRandomInt(1, 100));
			
	client->GetPlayer()->GetZone()->HandleChatMessage(0, 0, CHANNEL_EMOTE, message);
}

/* 
	Function: Command_Randomize()
	Purpose	: Sets randomize (appearance) values for NPCs
	Params	: Attrib Name
	Dev		: Scatman
	Example	: /randomize gender 1  -- will randomize the NPCs gender (male/female)
*/ 
void Commands::Command_Randomize(Client* client, Seperator* sep)
{
	NPC* target = (NPC*)client->GetPlayer()->GetTarget();
	if (target) 
	{
		if (target->IsNPC()) 
		{
			if (sep && sep->arg[0] && !sep->IsNumber(0)) 
			{
				const char* value = sep->arg[0];
				if (strncasecmp(value, "show", strlen(value)) == 0) 
				{
					client->DisplayRandomizeFeatures(target->GetRandomize());
				}
				else
				{
					if (sep->arg[1] && sep->IsNumber(1)) 
					{
						int8 option = atoi(sep->arg[1]);
						if (option == 0 || option == 1) 
						{
							int32 feature = 0;
							char feature_text[32] = {0};
							if (strncasecmp(value, "gender", strlen(value)) == 0) {
								feature = RANDOMIZE_GENDER;
								strncpy(feature_text, "gender", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "race", strlen(value)) == 0) {
								feature = RANDOMIZE_RACE;
								strncpy(feature_text, "race", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "model", strlen(value)) == 0) {
								feature = RANDOMIZE_MODEL_TYPE;
								strncpy(feature_text, "model", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "facial_hair", strlen(value)) == 0) {
								feature = RANDOMIZE_FACIAL_HAIR_TYPE;
								strncpy(feature_text, "facial hair", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "hair", strlen(value)) == 0) {
								feature = RANDOMIZE_HAIR_TYPE;
								strncpy(feature_text, "hair", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "wing", strlen(value)) == 0) {
								feature = RANDOMIZE_WING_TYPE;
								strncpy(feature_text, "wings", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "cheek", strlen(value)) == 0) {
								feature = RANDOMIZE_CHEEK_TYPE;
								strncpy(feature_text, "cheeks", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "chin", strlen(value)) == 0) {
								feature = RANDOMIZE_CHIN_TYPE;
								strncpy(feature_text, "chin", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "ear", strlen(value)) == 0) {
								feature = RANDOMIZE_EAR_TYPE;
								strncpy(feature_text, "ears", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "eye_brow", strlen(value)) == 0) {
								feature = RANDOMIZE_EYE_BROW_TYPE;
								strncpy(feature_text, "eye brows", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "eye", strlen(value)) == 0) {
								feature = RANDOMIZE_EYE_TYPE;
								strncpy(feature_text, "eyes", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "lip", strlen(value)) == 0) {
								feature = RANDOMIZE_LIP_TYPE;
								strncpy(feature_text, "lips", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "nose", strlen(value)) == 0) {
								feature = RANDOMIZE_NOSE_TYPE;
								strncpy(feature_text, "nose", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "eye_color", strlen(value)) == 0) {
								feature = RANDOMIZE_EYE_COLOR;
								strncpy(feature_text, "eye color", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "hair_color", strlen(value)) == 0) {
								feature = RANDOMIZE_HAIR_TYPE_COLOR;
								strncpy(feature_text, "hair color", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "hair_color1", strlen(value)) == 0) {
								feature = RANDOMIZE_HAIR_COLOR1;
								strncpy(feature_text, "hair color1", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "hair_color2", strlen(value)) == 0) {
								feature = RANDOMIZE_HAIR_COLOR2;
								strncpy(feature_text, "hair color2", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "hair_highlight", strlen(value)) == 0) {
								feature = RANDOMIZE_HAIR_HIGHLIGHT;
								strncpy(feature_text, "hair highlights", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "facial_hair_color", strlen(value)) == 0) {
								feature = RANDOMIZE_HAIR_FACE_COLOR;
								strncpy(feature_text, "facial hair color", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "facial_hair_color_highlight", strlen(value)) == 0) {
								feature = RANDOMIZE_HAIR_FACE_HIGHLIGHT_COLOR;
								strncpy(feature_text, "facial hair color highlights", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "hair_color_highlight", strlen(value)) == 0) {
								feature = RANDOMIZE_HAIR_TYPE_HIGHLIGHT_COLOR;
								strncpy(feature_text, "hair color highlights", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "skin_color", strlen(value)) == 0) {
								feature = RANDOMIZE_SKIN_COLOR;
								strncpy(feature_text, "skin color", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "wing_color1", strlen(value)) == 0) {
								feature = RANDOMIZE_WING_COLOR1;
								strncpy(feature_text, "wing color1", sizeof(feature_text) - 1);
							}
							else if (strncasecmp(value, "wing_color2", strlen(value)) == 0) {
								feature = RANDOMIZE_WING_COLOR2;
								strncpy(feature_text, "wing color2", sizeof(feature_text) - 1);
							}
							else {
								client->Message(CHANNEL_COLOR_YELLOW, "'%s' is not a valid feature to randomize.", value);
								client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Valid features are: 'gender', 'race', 'model', 'hair', 'facial_hair', 'legs', 'wings', 'cheek', 'chin', 'ear', 'eye', 'eye_brow', 'lip', 'nose', 'eye_color', 'hair_color1', 'hair_color2', 'hair_highlight', 'facial_hair_color', 'facial_hair_color_highlight', 'hair_color', 'hair_color_highlight', 'skin_color', 'wing_color1', 'wing_color2'.");
							}

							if (feature > 0) {
								if (option == 1) {
									if (target->GetRandomize() & feature)
										client->Message(CHANNEL_COLOR_YELLOW, "'%s' is already set to randomly generate their %s.", target->GetName(), feature_text);
									else {
										target->AddRandomize(feature);
										((NPC*)client->GetCurrentZone()->GetSpawn(target->GetDatabaseID()))->AddRandomize(feature);
										database.UpdateRandomize(target->GetDatabaseID(), feature);
										client->Message(CHANNEL_COLOR_YELLOW, "'%s' will now generate their %s randomly.", target->GetName(), feature_text);
									}
								}
								else {
									if (target->GetRandomize() & feature) {
										target->AddRandomize(-feature);
										((NPC*)client->GetCurrentZone()->GetSpawn(target->GetDatabaseID()))->AddRandomize(-feature);
										database.UpdateRandomize(target->GetDatabaseID(), -feature);
										client->Message(CHANNEL_COLOR_YELLOW, "'%s' will no longer generate their %s randomly.", target->GetName(), feature_text);
									}
									else
										client->Message(CHANNEL_COLOR_YELLOW, "'%s' already does not randomly generate their %s.", target->GetName(), feature_text);
								}
							}
						}
						else
							client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You must specify either a 1(on) or 0(off) as the second parameter.");
					}
					else
						client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You must specify either a 1(on) or 0(off) as the second parameter.");
				}
			}
			else {
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /randomize [show | [feature [1|0]]");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "'show' will display current configured features");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Valid features are: 'gender', 'race', 'model', 'hair', 'facial_hair', 'legs', 'wings', 'cheek', 'chin', 'ear', 'eye', 'eye_brow', 'lip', 'nose', 'eye_color', 'hair_color1', 'hair_color2', 'hair_highlight', 'facial_hair_color', 'facial_hair_color_highlight', 'hair_color', 'hair_color_highlight', 'skin_color', 'wing_color1', 'wing_color2'.");
			}
		}
		else
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "The target you wish to randomize must be an NPC.");
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You must select the target you wish to randomize.");
}

/* 
	Function: Command_ShowCloak()
	Purpose	: Character Settings combo box for Show Cloak
	Params	: true/false
	Dev		: John Adams
	Example	: 
*/ 
void Commands::Command_ShowCloak(Client* client, Seperator* sep)
{
	Player* player = client->GetPlayer();

	if (sep && sep->arg[0])
	{
		const char* value = sep->arg[0];
		if (strncasecmp(value, "true", strlen(value)) == 0) 
		{
			player->set_character_flag(CF_SHOW_CLOAK);
			player->reset_character_flag(CF2_SHOW_RANGED);
		}
		else if (strncasecmp(value, "false", strlen(value)) == 0) 
			player->reset_character_flag(CF_SHOW_CLOAK);
		else
		{
			client->Message(CHANNEL_COLOR_YELLOW, "Not supposed to be here! Please /bug this: Error in %s (%u)", __FUNCTION__, __LINE__);
			LogWrite(COMMAND__WARNING, 0, "Command", "Not supposed to be here! Please /bug this: Error in %s (%u)", __FUNCTION__, __LINE__);
		}
	}
}

/* 
	Function: Command_ShowHelm()
	Purpose	: Character Settings combo box for Show Helm
	Params	: true/false
	Dev		: John Adams
	Example	: 
*/ 
void Commands::Command_ShowHelm(Client* client, Seperator* sep)
{
	Player* player = client->GetPlayer();

	if (sep && sep->arg[0])
	{
		const char* value = sep->arg[0];
		if (strncasecmp(value, "true", strlen(value)) == 0) 
		{
			player->reset_character_flag(CF_HIDE_HELM);
			player->set_character_flag(CF_HIDE_HOOD);
		}
		else if (strncasecmp(value, "false", strlen(value)) == 0) 
			player->set_character_flag(CF_HIDE_HELM);
		else
		{
			client->Message(CHANNEL_COLOR_YELLOW, "Not supposed to be here! Please /bug this: Error in %s (%u)", __FUNCTION__, __LINE__);
			LogWrite(COMMAND__WARNING, 0, "Command", "Not supposed to be here! Please /bug this: Error in %s (%u)", __FUNCTION__, __LINE__);
		}
	}
}

/* 
	Function: Command_ShowHood()
	Purpose	: Character Settings combo box for Show Hood
	Params	: true/false
	Dev		: John Adams
	Example	: 
*/ 
void Commands::Command_ShowHood(Client* client, Seperator* sep)
{
	Player* player = client->GetPlayer();

	if (sep && sep->arg[0])
	{
		const char* value = sep->arg[0];

		if (strncasecmp(value, "true", strlen(value)) == 0) 
		{
			player->reset_character_flag(CF_HIDE_HOOD);
			player->set_character_flag(CF_HIDE_HELM);
		}
		else if (strncasecmp(value, "false", strlen(value)) == 0) 
			player->set_character_flag(CF_HIDE_HOOD);
		else
		{
			client->Message(CHANNEL_COLOR_YELLOW, "Not supposed to be here! Please /bug this: Error in %s (%u)", __FUNCTION__, __LINE__);
			LogWrite(COMMAND__WARNING, 0, "Command", "Not supposed to be here! Please /bug this: Error in %s (%u)", __FUNCTION__, __LINE__);
		}
	}
}

/* 
	Function: Command_ShowHoodHelm()
	Purpose	: Character Settings combo box for Show Helm or Hood
	Params	: true/false
	Dev		: John Adams
	Example	: 
*/ 
void Commands::Command_ShowHoodHelm(Client* client, Seperator* sep)
{
	Player* player = client->GetPlayer();

	if (sep && sep->arg[0])
	{
		const char* value = sep->arg[0];
		if (strncasecmp(value, "true", strlen(value)) == 0) 
		{
			player->reset_character_flag(CF_HIDE_HOOD);
			player->reset_character_flag(CF_HIDE_HELM);
		}
		else if (strncasecmp(value, "false", strlen(value)) == 0) 
		{
			// don't think we ever wind up in here...
			player->set_character_flag(CF_HIDE_HOOD);
			player->set_character_flag(CF_HIDE_HELM);
		}
		else
		{
			client->Message(CHANNEL_COLOR_YELLOW, "Not supposed to be here! Please /bug this: Error in %s (%u)", __FUNCTION__, __LINE__);
			LogWrite(COMMAND__WARNING, 0, "Command", "Not supposed to be here! Please /bug this: Error in %s (%u)", __FUNCTION__, __LINE__);
		}
	}
}

/* 
	Function: Command_ShowRanged()
	Purpose	: Character Settings combo box for Show Ranged weapon
	Params	: true/false
	Dev		: John Adams
	Example	: 
*/ 
void Commands::Command_ShowRanged(Client* client, Seperator* sep)
{
	Player* player = client->GetPlayer();

	if (sep && sep->arg[0])
	{
		const char* value = sep->arg[0];
		if (strncasecmp(value, "true", strlen(value)) == 0) 
		{
			player->set_character_flag(CF2_SHOW_RANGED);
			player->reset_character_flag(CF_SHOW_CLOAK);
		}
		else if (strncasecmp(value, "false", strlen(value)) == 0) 
			player->reset_character_flag(CF2_SHOW_RANGED);
		else
		{
			client->Message(CHANNEL_COLOR_YELLOW, "Not supposed to be here! Please /bug this: Error in %s (%u)", __FUNCTION__, __LINE__);
			LogWrite(COMMAND__WARNING, 0, "Command", "Not supposed to be here! Please /bug this: Error in %s (%u)", __FUNCTION__, __LINE__);
		}
	}
}

/* 
	Function: 
	Purpose	: 
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_Skills(Client* client, Seperator* sep, int handler)
{
	Player* player = 0;
	Client* to_client = 0;

	if(client->GetPlayer()->GetTarget() && client->GetPlayer()->GetTarget()->IsPlayer())
		to_client = client->GetPlayer()->GetTarget()->GetZone()->GetClientBySpawn(client->GetPlayer()->GetTarget());

	switch(handler)
	{
		case COMMAND_SKILL:
			{
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /skill [add|remove|list] [skill name]");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Target player to add/remove skills for that player.");
				break;
			}
		case COMMAND_SKILL_ADD:
			{
				if (sep && sep->arg[0]) 
				{
					const char* skill_name = sep->argplus[0];
					Skill* skill = master_skill_list.GetSkillByName(skill_name);

					if (skill && to_client && !(client->GetPlayer() == to_client->GetPlayer())) // add skill to your target, if target is not you
					{
						player = to_client->GetPlayer();

						if (player) 
						{
							if (!player->GetSkills()->HasSkill(skill->skill_id)) 
							{
								player->AddSkill(skill->skill_id, 1, player->GetLevel() * 5, true);
								client->Message(CHANNEL_COLOR_YELLOW, "You gave skill '%s' to player '%s'.", skill_name, player->GetName());
								to_client->Message(CHANNEL_COLOR_YELLOW, "%s gave you skill '%s'.", client->GetPlayer()->GetName(), skill_name);
							}
							else
								client->Message(CHANNEL_COLOR_YELLOW, "%s already has the skill '%s'.", player->GetName(), skill_name);
						}
					}
					else if (skill) // add skill to yourself
					{
						player = client->GetPlayer();

						if (player) 
						{
							if (!player->GetSkills()->HasSkill(skill->skill_id)) 
							{
								player->AddSkill(skill->skill_id, 1, player->GetLevel() * 5, true);
								client->Message(CHANNEL_COLOR_YELLOW, "Added skill '%s'.", skill_name);
							}
							else
								client->Message(CHANNEL_COLOR_YELLOW, "You already have the skill '%s'.", skill_name);
						}
					}
					else
						client->Message(CHANNEL_COLOR_YELLOW, "The skill '%s' does not exist.", skill_name);
				}
				else
				{
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /skill add [skill name]");
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Target player to give skill to that player.");
				}
				break;
			}
		case COMMAND_SKILL_REMOVE:
			{
				if (sep && sep->arg[0]) 
				{
					const char* skill_name = sep->argplus[0];
					Skill* skill = master_skill_list.GetSkillByName(skill_name);

					if (skill && to_client && !(client->GetPlayer() == to_client->GetPlayer())) // remove skill from your target, if target is not you
					{
						player = to_client->GetPlayer();

						if (player)
						{
							if (player->GetSkills()->HasSkill(skill->skill_id)) 
							{
								player->RemoveSkillFromDB(skill, true);
								client->Message(CHANNEL_COLOR_YELLOW, "Removed skill '%s' from player %s.", skill_name, player->GetName());
								to_client->Message(CHANNEL_COLOR_YELLOW, "%s has removed skill '%s' from you.", client->GetPlayer()->GetName(), skill_name);
							}
							else
								client->Message(CHANNEL_COLOR_YELLOW, "Player '%s' does not have the skill '%s'.", player->GetName(), skill_name);
						}
					}
					else if(skill) // remove skill from yourself
					{
						Player* player = client->GetPlayer();

						if (player) 
						{
							if (player->GetSkills()->HasSkill(skill->skill_id)) 
							{
								player->RemoveSkillFromDB(skill, true);
								client->Message(CHANNEL_COLOR_YELLOW, "Removed skill '%s'.", skill_name);
							}
							else
								client->Message(CHANNEL_COLOR_YELLOW, "You do not have the skill '%s'.", skill_name);
						}
					}
					else
						client->Message(CHANNEL_COLOR_YELLOW, "The skill '%s' does not exist.", skill_name);
				}
				else
				{
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /skill remove [skill name]");
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Target player to give skill to that player.");
				}
				break;
			}
		case COMMAND_SKILL_LIST:
			{
				if (sep && sep->arg[0]) 
				{
					const char* skill_name = sep->argplus[0];
					client->Message(CHANNEL_COLOR_YELLOW, "Listing skills like '%s':", skill_name);
					map<int32, Skill*>* skills = master_skill_list.GetAllSkills();
					map<int32, Skill*>::iterator itr;

					if (skills && skills->size() > 0) 
					{
						for (itr = skills->begin(); itr != skills->end(); itr++) 
						{
							Skill* skill = itr->second;
							string current_skill_name = ::ToLower(string(skill->name.data.c_str()));

							if (current_skill_name.find(::ToLower(string(skill_name))) < 0xFFFFFFFF)
								client->Message(CHANNEL_COLOR_YELLOW, "%s (%u)", skill->name.data.c_str(), skill->skill_id);
						}
					}
				}
				else
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /skill list [skill name]");
				break;
			}
	}
}

/* 
	Function: Command_SpawnTemplate()
	Purpose	: Create or Use defined Spawn Templates
	Params	: create, save, rempve, list
	Dev		: John Adams
	Example	: /spawn template create "Test"
*/ 
void Commands::Command_SpawnTemplate(Client* client, Seperator* sep)
{
	if (sep == NULL || sep->arg[0] == NULL) 
	{
		client->Message(CHANNEL_COLOR_YELLOW, "Examples:\n/spawn template save Test - saves a template of the targetted spawn as the name Test\n/spawn template list - shows a list of current templates\n/spawn template create [id|name] creates a new spawn based on template [id|name] at your current location.");
		return;
	} 

	// got params, continue
	const char * template_cmd = sep->arg[0];

	if (strncasecmp(template_cmd, "list", strlen(template_cmd)) == 0) 
	{
		if (!sep->IsSet(1))
		{
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Usage: /spawn template list [name|location_id]");
			client->SimpleMessage(CHANNEL_COLOR_YELLOW,"Examples:\n/spawn template list [name] - will show all templates containing [name].\n/spawn template list [location_id] - will show all templates that [location_id] is assigned to.");
			return;
		}

		map<int32, string>* template_names = 0;
		if (sep->IsNumber(1))
		{
			int32 location_id = 0;
			location_id = atoi(sep->arg[1]);
			template_names = database.GetSpawnTemplateListByID(location_id);
		}
		else
		{
			const char* name = 0;
			name = sep->argplus[1];
			template_names = database.GetSpawnTemplateListByName(name);
		}

		if(!template_names)
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "No templates found.");
		else
		{
			map<int32, string>::iterator itr;
			client->SimpleMessage(CHANNEL_COLOR_YELLOW," ID   Name: ");
			for(itr = template_names->begin(); itr != template_names->end(); itr++)
				client->Message(CHANNEL_COLOR_YELLOW,"%03lu %s", itr->first, itr->second.c_str());
			safe_delete(template_names);
		}
	}

	else if (strncasecmp(template_cmd, "save", strlen(template_cmd)) == 0) 
	{
		NPC* target = (NPC*)client->GetPlayer()->GetTarget();
		if ( target && (target->IsNPC() || target->IsObject() || target->IsSign() || target->IsWidget() || target->IsGroundSpawn()) ) 
		{
			if (sep && sep->arg[1][0] && !sep->IsNumber(1))
			{
				// first, lookup to see if template name already exists
				const char* name = 0;
				name = sep->argplus[1];
				map<int32, string>* template_names = database.GetSpawnTemplateListByName(name);
				if(!template_names)
				{
					int32 new_template_id = database.SaveSpawnTemplate(target->GetSpawnLocationID(), name);
					if( new_template_id > 0 )
						client->Message(CHANNEL_COLOR_YELLOW, "Spawn template '%s' added for spawn '%s' (%u) as TemplateID: %u", name, target->GetName(), target->GetSpawnLocationID(), new_template_id);
					else
						client->Message(CHANNEL_COLOR_RED, "ERROR: Failed to add new template '%s' for spawn '%s' (%u).", name, target->GetName(), target->GetSpawnLocationID());
				}
				else
					client->Message(CHANNEL_COLOR_RED, "ERROR: Failed to add new template '%s' - Already exists!", name);
			}
			else
				client->Message(CHANNEL_COLOR_RED, "ERROR: Saving a new spawn template requires a valid template name!");
		}
		else
			client->Message(CHANNEL_COLOR_RED, "ERROR: You must target the spawn you wish to save as a template!");
	}

	else if (strncasecmp(template_cmd, "remove", strlen(template_cmd)) == 0) 
	{
		if (sep && sep->arg[1][0] && sep->IsNumber(1))
		{
			int32 template_id = 0;
			template_id = atoi(sep->arg[1]);
			if (database.RemoveSpawnTemplate(template_id))
				client->Message(CHANNEL_COLOR_YELLOW, "Spawn template ID: %u successfully removed.", template_id);
			else
				client->Message(CHANNEL_COLOR_RED, "ERROR: Failed to remove spawn template ID: %u", template_id);
		}
		else
			client->Message(CHANNEL_COLOR_RED, "ERROR: Removing a spawn template requires a valid template ID!");
	}

	else if (strncasecmp(template_cmd, "create", strlen(template_cmd)) == 0) 
	{
		if (sep && sep->arg[1][0]) 
		{
			int32 new_location = 0;

			if (sep->IsNumber(1))
			{
				int32 template_id = 0;
				template_id = atoi(sep->arg[1]);

				new_location = database.CreateSpawnFromTemplateByID(client, template_id);
				if( new_location > 0 )
					client->Message(CHANNEL_COLOR_YELLOW, "New spawn location %u created from template ID: %u", new_location, template_id);
				else
					client->Message(CHANNEL_COLOR_RED, "ERROR: Failed to create new spawn location from template ID: %u", template_id);
			}
			else
			{
				const char* name = 0;
				name = sep->argplus[1];

				new_location = database.CreateSpawnFromTemplateByName(client, name);
				if( new_location > 0 )
					client->Message(CHANNEL_COLOR_YELLOW, "New spawn location %u created from template: '%s'", new_location, name);
				else
					client->Message(CHANNEL_COLOR_RED, "ERROR: Failed to create new spawn location from template: '%s'", name);
			}
		}
		else
			client->Message(CHANNEL_COLOR_RED, "ERROR: Creating a new spawn location requires a valid template name or ID!");
	}

	else
		client->Message(CHANNEL_COLOR_RED, "ERROR: Unknown /spawn template command.");
}

void Commands::Command_Speed(Client* client, Seperator* sep) {
	if(sep && sep->arg[0][0] && sep->IsNumber(0)){
		float new_speed = atof(sep->arg[0]);
		client->GetPlayer()->SetSpeed(new_speed);
		client->GetPlayer()->SetCharSheetChanged(true);
		client->Message(CHANNEL_STATUS, "Setting speed to %.2f.", new_speed);
	}
	else{
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /speed {new speed value}");
	}

}

/* 
	Function: Command_StationMarketPlace()
	Purpose	: just trying to eat the console spam for now
	Params	: 
	Dev		: John
*/ 
void Commands::Command_StationMarketPlace(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_SMP");
	// This will reduce the spam from once every 10 sec to once every 30 sec,
	// can't seem to reduce it any more get rid of it completely...
	if (sep && strcmp(sep->arg[0], "gkw") == 0) {
		PacketStruct* packet = configReader.getStruct("WS_MarketFundsUpdate", client->GetVersion());
		if (packet) {
			packet->setDataByName("account_id", client->GetAccountID());
			packet->setDataByName("character_id", client->GetCharacterID());
			packet->setDataByName("unknown1", 1, 5);
			packet->setDataByName("unknown1", 26, 6);
			packet->setDataByName("unknown2", 0xFFFFFFFF);
			packet->setDataByName("unknown3", 248, 1);
			client->QueuePacket(packet->serialize());
		}
		safe_delete(packet);
	}
}

/* 
	Function: Command_StopDrinking()
	Purpose	: Stops player from auto-consuming drink
	Params	: 
	Dev		: Zcoretri
	Example	: /stopdrinking
*/ 
void Commands::Command_StopDrinking(Client* client)
{
	client->GetPlayer()->reset_character_flag(CF_DRINK_AUTO_CONSUME);
	client->Message(CHANNEL_COLOR_YELLOW,"You stop drinking your current drink.");
}

/* 
	Function: Command_StopEating()
	Purpose	: Stops player from auto-consuming food
	Params	: 
	Dev		: Zcoretri
	Example	: /stopeating
*/ 
void Commands::Command_StopEating(Client* client)
{
	client->GetPlayer()->reset_character_flag(CF_FOOD_AUTO_CONSUME);
	client->Message(CHANNEL_COLOR_YELLOW,"You stop eating your current food.");
}

/* 
	Function: Command_Title()
	Purpose	: Help for /title command
	Params	: n/a
	Dev		: Zcoretri
	Example	: /title
*/ 
void Commands::Command_Title(Client* client)
{
	client->Message(CHANNEL_COLOR_YELLOW, "Available subcommands: list, setprefix <index>, setsuffix <index>, fix");
}

/* 
	Function: Command_TitleList()
	Purpose	: List available titles for player
	Params	: n/a
	Dev		: Zcoretri
	Example	: /title list
*/ 
void Commands::Command_TitleList(Client* client)
{
	list<Title*>* titles = client->GetPlayer()->GetPlayerTitles()->GetAllTitles();
	list<Title*>::iterator itr;
	Title* title;
	int16 i = 0;

	client->Message(CHANNEL_COLOR_WHITE, "Listing available titles:");
	for(itr = titles->begin(); itr != titles->end(); itr++)
	{
		title = *itr;
		client->Message(CHANNEL_COLOR_WHITE, "%i: type=[%s] title=[%s]", i, title->GetPrefix() ? "Prefix":"Suffix", title->GetName());
		i++;
	}

}

/* 
	Function: Command_TitleSetPrefix()
	Purpose	: Set Prefix title for player
	Params	: Title ID
	Dev		: Zcoretri
	Example	: /title setprefix 1
*/ 
void Commands::Command_TitleSetPrefix(Client* client, Seperator* sep)
{
	if (sep && sep->arg[0] && sep->IsNumber(0))
	{
		int32 index = atoul(sep->arg[0]);

		database.SaveCharPrefixIndex(index, client->GetCharacterID(), client);
		client->SendTitleUpdate();
	}
}

/* 
	Function: Command_TitleSetSuffix()
	Purpose	: Set Suffix title for player
	Params	: Title ID
	Dev		: Zcoretri
	Example	: /title setsuffix 1
*/ 
void Commands::Command_TitleSetSuffix(Client* client, Seperator* sep)
{
	if (sep && sep->arg[0] && sep->IsNumber(0))
	{
		int32 index = atoul(sep->arg[0]);

		database.SaveCharSuffixIndex(index, client->GetCharacterID(), client);
		client->SendTitleUpdate();
	}
}

/* 
	Function: Command_TitleFix()
	Purpose	: Fix title for player (???)
	Params	: 
	Dev		: Zcoretri
	Example	: 
*/ 
void Commands::Command_TitleFix(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_TITLE_FIX");
	LogWrite(MISC__TODO, 1, "Titles", "TODO-Command: TITLE_FIX");
}


/* 
	Function: Command_Toggle_Anonymous()
	Purpose	: Toggles player Anonymous
	Params	: 
	Dev		: paulgh
	Example	: /anon
*/ 
void Commands::Command_Toggle_Anonymous(Client* client)
{
	Player* player = client->GetPlayer();

	player->toggle_character_flag(CF_ANONYMOUS);
	client->Message(CHANNEL_COLOR_YELLOW,"You are %s anonymous.", player->get_character_flag(CF_ANONYMOUS)?"now":"no longer");

	if (player->get_character_flag(CF_ANONYMOUS))
		player->SetActivityStatus(player->GetActivityStatus() + ACTIVITY_STATUS_ANONYMOUS);
	else
		player->SetActivityStatus(player->GetActivityStatus() - ACTIVITY_STATUS_ANONYMOUS);
}

/* 
	Function: Command_Toggle_AutoConsume()
	Purpose	: Toggles player food/drink auto consume
	Params	: unknown
	Dev		: paulgh
	Example	: /set_auto_consume
*/ 
void Commands::Command_Toggle_AutoConsume(Client* client, Seperator* sep)
{
	Player* player = client->GetPlayer();

	if (sep && sep->arg[0])
		PrintSep(sep, "COMMAND_SET_AUTO_CONSUME");
	if (sep && sep->arg[0] && sep->IsNumber(0))
	{
		int8 slot = atoi(sep->arg[0]);
		int8 flag = atoi(sep->arg[1]);
		if (slot == EQ2_FOOD_SLOT)
		{
			player->toggle_character_flag(CF_FOOD_AUTO_CONSUME);
			player->SetCharSheetChanged(true);
			client->QueuePacket(player->GetEquipmentList()->serialize(client->GetVersion(), player));
			if (flag == 1)
				client->Message(CHANNEL_COLOR_WHITE, "You decide to eat immediately whenever you become hungry.");
			else
				client->Message(CHANNEL_COLOR_WHITE, "You decide to ignore the hunger.");
		}
		else
		{
			player->toggle_character_flag(CF_DRINK_AUTO_CONSUME);
			player->SetCharSheetChanged(true);
			client->QueuePacket(player->GetEquipmentList()->serialize(client->GetVersion(), player));
			if (flag == 1)
				client->Message(CHANNEL_COLOR_WHITE, "You decide to drink immediately whenever you become thirsty.");
			else
				client->Message(CHANNEL_COLOR_WHITE, "You decide to ignore the thirst.");
		}
	}
}

/* 
	Function: Command_Toggle_BonusXP()
	Purpose	: Toggles player Bonus XP
	Params	: 
	Dev		: John Adams
	Example	: /disable_char_bonus_exp
*/ 
void Commands::Command_Toggle_BonusXP(Client* client)
{
	Player* player = client->GetPlayer();

	player->toggle_character_flag(CF2_CHARACTER_BONUS_EXPERIENCE_ENABLED);
	player->SetCharSheetChanged(true);
}

/* 
	Function: Command_Toggle_CombatXP()
	Purpose	: Toggles player Adventure XP
	Params	: 
	Dev		: John Adams
	Example	: /disable_combat_exp
*/ 
void Commands::Command_Toggle_CombatXP(Client* client)
{
	Player* player = client->GetPlayer();

	player->toggle_character_flag(CF_COMBAT_EXPERIENCE_ENABLED);
	player->SetCharSheetChanged(true);
}

/* 
	Function: Command_Toggle_GMHide()
	Purpose	: Toggles hiding player GM status
	Params	: 
	Dev		: Scatman
	Example	: /gm_hide
*/ 
void Commands::Command_Toggle_GMHide(Client* client)
{
	Player* player = client->GetPlayer();

	player->toggle_character_flag(CF_HIDE_STATUS);
	client->Message(CHANNEL_COLOR_YELLOW,"You are %s hiding your GM status.", player->get_character_flag(CF_HIDE_STATUS)?"now":"no longer");
}

/* 
	Function: Command_Toggle_GMVanish()
	Purpose	: Toggles hiding GM players from /who searches
	Params	: 
	Dev		: Scatman
	Example	: /gm_vanish
*/ 
void Commands::Command_Toggle_GMVanish(Client* client)
{
	Player* player = client->GetPlayer();

	player->toggle_character_flag(CF_GM_HIDDEN);
	client->Message(CHANNEL_COLOR_YELLOW,"You are %s invisible to who queries.", player->get_character_flag(CF_GM_HIDDEN)?"now":"no longer");
}

/* 
	Function: Command_Toggle_Illusions()
	Purpose	: Toggles player illusion form
	Params	: not sure sep is needed, testing
	Dev		: paulgh
	Example	: /hide_illusions
*/ 
void Commands::Command_Toggle_Illusions(Client* client, Seperator* sep)
{
	if (sep && sep->arg[0])
		PrintSep(sep, "COMMAND_TOGGLE_ILLUSIONS");
	client->GetPlayer()->toggle_character_flag(CF_SHOW_ILLUSION);
}

/* 
	Function: Command_Toggle_LFG()
	Purpose	: Toggles player LFG Flag
	Params	: 
	Dev		: paulgh
	Example	: /lfg
*/ 
void Commands::Command_Toggle_LFG(Client* client)
{
	Player* player = client->GetPlayer();

	player->toggle_character_flag(CF_LFG);
	client->Message(CHANNEL_COLOR_YELLOW,"You are %s LFG.", player->get_character_flag(CF_LFG)?"now":"no longer");

	if (player->get_character_flag(CF_LFG))
		player->SetActivityStatus(player->GetActivityStatus() + ACTIVITY_STATUS_LFG);
	else
		player->SetActivityStatus(player->GetActivityStatus() - ACTIVITY_STATUS_LFG);
}

/* 
	Function: Command_Toggle_LFW()
	Purpose	: Toggles player LFW Flag
	Params	: 
	Dev		: paulgh
	Example	: /lfw
*/ 
void Commands::Command_Toggle_LFW(Client* client)
{
	Player* player = client->GetPlayer();

	player->toggle_character_flag(CF_LFW);
	client->Message(CHANNEL_COLOR_YELLOW,"You %s looking for work.", player->get_character_flag(CF_LFW)?"let others know you are":"stop");

	if (player->get_character_flag(CF_LFW))
		player->SetActivityStatus(player->GetActivityStatus() + ACTIVITY_STATUS_LFW);
	else
		player->SetActivityStatus(player->GetActivityStatus() - ACTIVITY_STATUS_LFW);
}

/* 
	Function: Command_Toggle_QuestXP()
	Purpose	: Toggles player Quest XP
	Params	: 
	Dev		: John Adams
	Example	: /disable_quest_exp
*/ 
void Commands::Command_Toggle_QuestXP(Client* client)
{
	Player* player = client->GetPlayer();

	player->toggle_character_flag(CF_QUEST_EXPERIENCE_ENABLED);
	player->SetCharSheetChanged(true);
}

/* 
	Function: Command_Toggle_Roleplaying()
	Purpose	: Toggles player Roleplaying flag
	Params	: 
	Dev		: paulgh
	Example	: /role
*/ 
void Commands::Command_Toggle_Roleplaying(Client* client)
{
	Player* player = client->GetPlayer();

	player->toggle_character_flag(CF_ROLEPLAYING);
	client->Message(CHANNEL_COLOR_YELLOW,"You are %s roleplaying.", player->get_character_flag(CF_ROLEPLAYING)?"now":"no longer");

	if (player->get_character_flag(CF_ROLEPLAYING))
		player->SetActivityStatus(player->GetActivityStatus() + ACTIVITY_STATUS_ROLEPLAYING);
	else
		player->SetActivityStatus(player->GetActivityStatus() - ACTIVITY_STATUS_ROLEPLAYING);
}

void Commands::Command_Toggle_Duels(Client* client)
{
	Player* player = client->GetPlayer();

	player->toggle_character_flag(CF_ALLOW_DUEL_INVITES);
	client->Message(CHANNEL_COLOR_YELLOW,"You are %s accepting duel invites.", player->get_character_flag(CF_ALLOW_DUEL_INVITES)?"now":"no longer");
}

void Commands::Command_Toggle_Trades(Client* client)
{
	Player* player = client->GetPlayer();

	player->toggle_character_flag(CF_ALLOW_TRADE_INVITES);
	client->Message(CHANNEL_COLOR_YELLOW,"You are %s accepting trade invites.", player->get_character_flag(CF_ALLOW_TRADE_INVITES)?"now":"no longer");
}

void Commands::Command_Toggle_Guilds(Client* client)
{
	Player* player = client->GetPlayer();

	player->toggle_character_flag(CF_ALLOW_GUILD_INVITES);
	client->Message(CHANNEL_COLOR_YELLOW,"You are %s accepting guild invites.", player->get_character_flag(CF_ALLOW_GUILD_INVITES)?"now":"no longer");
}

void Commands::Command_Toggle_Groups(Client* client)
{
	Player* player = client->GetPlayer();

	player->toggle_character_flag(CF_ALLOW_GROUP_INVITES);
	client->Message(CHANNEL_COLOR_YELLOW,"You are %s accepting group invites.", player->get_character_flag(CF_ALLOW_GROUP_INVITES)?"now":"no longer");
}

void Commands::Command_Toggle_Raids(Client* client)
{
	Player* player = client->GetPlayer();

	player->toggle_character_flag(CF_ALLOW_RAID_INVITES);
	client->Message(CHANNEL_COLOR_YELLOW,"You are %s accepting raid invites.", player->get_character_flag(CF_ALLOW_RAID_INVITES)?"now":"no longer");
}

void Commands::Command_Toggle_LON(Client* client)
{
	/*Player* player = client->GetPlayer();

	player->toggle_character_flag(CF2_ALLOW_LON_INVITES);
	client->Message(CHANNEL_COLOR_YELLOW,"You are %s accepting LoN invites.", player->get_character_flag(CF2_ALLOW_LON_INVITES)?"now":"no longer");*/
}

void Commands::Command_Toggle_VoiceChat(Client* client)
{
	Player* player = client->GetPlayer();

	player->toggle_character_flag(CF2_ALLOW_VOICE_INVITES);
	client->Message(CHANNEL_COLOR_YELLOW,"You are %s accepting voice chat invites.", player->get_character_flag(CF2_ALLOW_VOICE_INVITES)?"now":"no longer");
}

/* 
	Function: Command_TradeStart()
	Purpose	: Starts item/coin trade between players
	Params	: 
	Dev		: 
	Example	: 
*/ 
#include "../Trade.h"
void Commands::Command_TradeStart(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_START_TRADE");

	Entity* trader = client->GetPlayer();
	Entity* trader2 = 0;
	if (sep && sep->IsSet(0) && sep->IsNumber(0)) {
		Spawn* spawn = client->GetPlayer()->GetSpawnWithPlayerID(atoi(sep->arg[0]));
		if (spawn) {
			if (spawn->IsEntity())
				trader2 = (Entity*)spawn;
		}
	}
	else if (client->GetPlayer()->GetTarget()) {
		if (client->GetPlayer()->GetTarget()->IsEntity())
			trader2 = (Entity*)client->GetPlayer()->GetTarget();
	}

	// can only trade with player or bots
	if (trader && trader2) {
		if (trader == trader2) {
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You can't trade with yourself.");
		}
		else if (trader2->IsPlayer() || trader2->IsBot()) {
			LogWrite(PLAYER__ERROR, 0, "Trade", "creating trade");
			if (trader->trade)
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are already trading.");
			else if (trader2->trade)
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Your target is already trading.");
			else {
				Trade* trade = new Trade(trader, trader2);
				trader->trade = trade;
				trader2->trade = trade;
			}
		}
		else {
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You can only trade with another player or a bot.");
		}
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Unable to find target");
}

/* 
	Function: Command_Track()
	Purpose	: Starts/Stops Tracking for a player
	Params	: 
	Dev		: Scatman
	Example	: /track
*/ 
void Commands::Command_Track(Client* client)
{
	if (!client->GetPlayer()->GetIsTracking())
		client->GetPlayer()->GetZone()->AddPlayerTracking(client->GetPlayer());
	else
		client->GetPlayer()->GetZone()->RemovePlayerTracking(client->GetPlayer(), TRACKING_STOP);
}

/* 
	Function: Command_TradeAccept()
	Purpose	: Accepts item/coin trade between players
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_TradeAccept(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_ACCEPT_TRADE");
	Trade* trade = client->GetPlayer()->trade;
	if (trade) {
		bool trade_complete = trade->SetTradeAccepted(client->GetPlayer());
		if (trade_complete)
			safe_delete(trade);
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are not currently trading.");
}

/* 
	Function: Command_TradeReject()
	Purpose	: Rejects item/coin trade between players
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_TradeReject(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_REJECT_TRADE");
	LogWrite(MISC__TODO, 1, "Command", "TODO-Command: Reject Player Trading");
	client->Message(CHANNEL_COLOR_YELLOW, "You cannot trade with other players (Not Implemented)");
}

/* 
	Function: Command_TradeCancel()
	Purpose	: Cancels item/coin trade between players
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_TradeCancel(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_CANCEL_TRADE");
	Trade* trade = client->GetPlayer()->trade;
	if (trade) {
		trade->CancelTrade(client->GetPlayer());
		safe_delete(trade);
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are not currently trading.");
}

/* 
	Function: Command_TradeSetCoin()
	Purpose	: Sets coin trade between players
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_TradeSetCoin(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_SET_TRADE_COIN");
	LogWrite(MISC__TODO, 1, "Command", "TODO-Command: Set Trade Coin");
	client->Message(CHANNEL_COLOR_YELLOW, "You cannot trade with other players (Not Implemented)");
}

/* 
	Function: Command_TradeAddCoin()
	Purpose	: Adds coin to trade between players
	Params	: Passes "handler" through so we can process copper, silver, gold and plat in the same function
	Dev		: 
	Example	: 
*/ 
void Commands::Command_TradeAddCoin(Client* client, Seperator* sep, int handler)
{
	PrintSep(sep, "COMMAND_ADD_TRADE_{coin type}");
	Trade* trade = client->GetPlayer()->trade;
	if (trade) {
		if (sep && sep->IsSet(0) && sep->IsNumber(0)) {
			int32 amount = atoi(sep->arg[0]);
			int64 val = 0;
			switch (handler) {
		case COMMAND_ADD_TRADE_COPPER:
			{
				val = amount;
				trade->AddCoinToTrade(client->GetPlayer(), val);
				break;
			}

		case COMMAND_ADD_TRADE_SILVER:
			{
				val = amount * 100;
				trade->AddCoinToTrade(client->GetPlayer(), val);
				break;
			}

		case COMMAND_ADD_TRADE_GOLD:
			{
				val = amount * 10000;
				trade->AddCoinToTrade(client->GetPlayer(), val);
				break;
			}

		case COMMAND_ADD_TRADE_PLAT:
			{
				val = amount * 1000000;
				trade->AddCoinToTrade(client->GetPlayer(), val);
				break;
			}

		default:
			{
				LogWrite(COMMAND__ERROR, 0, "Command", "No coin type specified in func: '%s', line %u", __FUNCTION__, __LINE__);
				break;
			}
	}
		}
		else
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Invalid coin amount.");
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are not currently trading.");
}

/* 
	Function: Command_TradeRemoveCoin()
	Purpose	: Removes coin from trade between players
	Params	: Passes "handler" through so we can process copper, silver, gold and plat in the same function
	Dev		: 
	Example	: 
*/ 
void Commands::Command_TradeRemoveCoin(Client* client, Seperator* sep, int handler)
{
	PrintSep(sep, "COMMAND_REMOVE_TRADE_{coin type}");

	Trade* trade = client->GetPlayer()->trade;
	if (trade) {
		if (sep && sep->IsSet(0) && sep->IsNumber(0)) {
			int32 amount = atoi(sep->arg[0]);
			int64 val = 0;
			switch (handler) {
		case COMMAND_REMOVE_TRADE_COPPER:
			{
				val = amount;
				trade->RemoveCoinFromTrade(client->GetPlayer(), val);
				break;
			}

		case COMMAND_REMOVE_TRADE_SILVER:
			{
				val = amount * 100;
				trade->RemoveCoinFromTrade(client->GetPlayer(), val);
				break;
			}

		case COMMAND_REMOVE_TRADE_GOLD:
			{
				val = amount * 10000;
				trade->RemoveCoinFromTrade(client->GetPlayer(), val);
				break;
			}

		case COMMAND_REMOVE_TRADE_PLAT:
			{
				val = amount * 1000000;
				trade->RemoveCoinFromTrade(client->GetPlayer(), val);
				break;
			}

		default:
			{
				LogWrite(COMMAND__ERROR, 0, "Command", "No coin type specified in func: '%s', line %u", __FUNCTION__, __LINE__);
				break;
			}
	}
		}
		else
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Invalid coin amount");
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are not currently trading.");
}

/* 
	Function: Command_TradeAddItem()
	Purpose	: Adds item to trade between players
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_TradeAddItem(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_ADD_TRADE_ITEM");
	/*
	arg[0] = item index
	arg[1] = slot
	arg[2] = quantity
	*/
	if (!client->GetPlayer()->trade) {
		LogWrite(PLAYER__ERROR, 0, "Trade", "Player is not currently trading.");
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are not currently trading.");
		return;
	}

	if (sep && sep->IsSet(0) && sep->IsNumber(0) && sep->IsSet(1) && sep->IsNumber(1) && sep->IsSet(2) && sep->IsNumber(2)) {
		Item* item = 0;
		int32 index = atoi(sep->arg[0]);
		item = client->GetPlayer()->GetPlayerItemList()->GetItemFromIndex(index);
		if (item) {
			int8 result = client->GetPlayer()->trade->AddItemToTrade(client->GetPlayer(), item, atoi(sep->arg[2]), atoi(sep->arg[1]));
			if (result == 1)
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Item is already being traded.");
			else if (result == 2)
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You can't trade NO-TRADE items.");
			else if (result == 3)
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You can't trade HEIRLOOM items.");
			else if (result == 255)
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Unknown error trying to add the item to the trade...");
		}
		else {
			LogWrite(PLAYER__ERROR, 0, "Trade", "Unable to get an item for the player (%s) from the index (%u)", client->GetPlayer()->GetName(), index);
			client->Message(CHANNEL_ERROR, "Unable to find item at index %u", index);
		}
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Invalid item.");
}

/* 
	Function: Command_TradeRemoveItem()
	Purpose	: Removes item from trade between players
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_TradeRemoveItem(Client* client, Seperator* sep)
{
	PrintSep(sep, "COMMAND_REMOVE_TRADE_ITEM");
	/*
	arg[0] = trade window slot
	*/

	Trade* trade = client->GetPlayer()->trade;
	if (trade) {
		if (sep && sep->IsSet(0) && sep->IsNumber(0)) {
			trade->RemoveItemFromTrade(client->GetPlayer(), atoi(sep->arg[0]));
		}
		else
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Invalid item.");
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are not currently trading.");
}

void Commands::Command_TryOn(Client* client, Seperator* sep)
{
	Item *item = 0;
	sint32 crc;

	if (sep) {
		// 1096+ sends more info so need to do a version check so older clients (SF) still work
		if (client->GetVersion() < 1096) {
			if (sep->arg[1] && sep->IsNumber(1) && sep->arg[2] && sep->IsNumber(2)) 
			{
				item = master_item_list.GetItem(atoul(sep->arg[1]));
				crc = atol(sep->arg[2]);
			}
		}
		else {
			// From the broker and links in chat
			if (strcmp(sep->arg[0], "crc") == 0) {
				if (sep->IsNumber(2) && sep->IsNumber(3)) {
					item = master_item_list.GetItem(atoul(sep->arg[2]));
					crc = atol(sep->arg[3]);
				}
			}
			// From inventory
			if (strcmp(sep->arg[0], "dbid") == 0) {
				if (sep->IsNumber(2) && sep->IsNumber(4)) {
					item = master_item_list.GetItem(atoul(sep->arg[2]));
					crc = atol(sep->arg[4]);
				}
			}
		}
		if (item)
			client->ShowDressingRoom(item, crc);
	}
}

void Commands::Command_JoinChannel(Client * client, Seperator *sep) {
	const char *channel_name, *password = NULL;

	if (sep == NULL || !sep->IsSet(0)) {
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /joinchannel <channel name> [password]");
		return;
	}

	channel_name = sep->arg[0];
	if (sep->IsSet(1))
		password = sep->arg[1];

	if (!chat.ChannelExists(channel_name) && !chat.CreateChannel(channel_name, password)) {
		client->Message(CHANNEL_COLOR_RED, "Unable to create channel '%s'.", channel_name);
		return;
	}

	if (chat.IsInChannel(client, channel_name)) {
		client->Message(CHANNEL_COLOR_WHITE, "You are already in '%s'.", channel_name);
		return;
	}

	if (chat.HasPassword(channel_name)) {
		if (password == NULL) {
			client->Message(CHANNEL_COLOR_WHITE, "Unable to join '%s': That channel is password protected.", channel_name);
			return;
		}
		if (!chat.PasswordMatches(channel_name, password)) {
			client->Message(CHANNEL_COLOR_WHITE, "Unable to join '%s': The password is not correc.t", channel_name);
			return;
		}
	}
	
	if (!chat.JoinChannel(client, channel_name))
		client->Message(CHANNEL_COLOR_RED, "There was an internal error preventing you from joining '%s'.", channel_name);
}

void Commands::Command_JoinChannelFromLoad(Client * client, Seperator *sep) {
	printf("ScatDebug: Received 'joinfromchannel', using the same function as 'joinchannel' (not sure what the difference is)\n");
	Command_JoinChannel(client, sep);
}

void Commands::Command_TellChannel(Client *client, Seperator *sep) {
	if (sep == NULL || !sep->IsSet(0) || !sep->IsSet(1)) {
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /tellchannel <channel name> <message>");
		PrintSep(sep, "tellchannel");
		return;
	}

	chat.TellChannel(client, sep->arg[0], sep->argplus[1]);
}

void Commands::Command_Mount(Client* client, Seperator* sep) {
	if (sep == nullptr) return;

	if (sep->IsSet(0) && sep->IsNumber(0)) {
		client->GetPlayer()->SetMount(atol(sep->arg[0]));
	}
}

void Commands::Command_Knockback(Client* client, Seperator* sep) {
	if (sep == nullptr) return;

	float vertical = 0.0;
	float horizontal = 0.0;
	bool use_heading = false;

	if (sep->IsSet(0) && sep->IsNumber(0))
		vertical = atol(sep->arg[0]);

	if (sep->IsSet(1) && sep->IsNumber(1))
		horizontal = atol(sep->arg[1]);

	if (sep->IsSet(2) && sep->IsNumber(2))
		use_heading = atol(sep->arg[2]);

	Client* knockback_client = client;
	if (client->GetPlayer()->GetTarget() && client->GetPlayer()->GetTarget()->IsPlayer())
		knockback_client = client->GetCurrentZone()->GetClientBySpawn(client->GetPlayer()->GetTarget());

	PacketStruct* packet = configReader.getStruct("WS_PlayerKnockback", knockback_client->GetVersion());
	if (packet) {
		packet->setDataByName("target_x", knockback_client->GetPlayer()->GetX());
		packet->setDataByName("target_y", knockback_client->GetPlayer()->GetY());
		packet->setDataByName("target_z", knockback_client->GetPlayer()->GetZ());
		packet->setDataByName("vertical_movement", vertical);
		packet->setDataByName("horizontal_movement", horizontal);
		packet->setDataByName("use_player_heading", use_heading);

		knockback_client->QueuePacket(packet->serialize());
	}
	safe_delete(packet);
}

void Commands::Command_ResetEncounter(Client* client) {
	Spawn* target = client->GetPlayer()->GetTarget();

	if (target && target->IsNPC()) {
		static_cast<NPC*>(target)->Brain()->ClearHate();
		static_cast<NPC*>(target)->Brain()->ClearEncounter();
		static_cast<NPC*>(target)->GetZone()->RemoveSpellTimersFromSpawn(target, true, true);
	}
}

void Commands::Command_Test(Client* client, Seperator* sep) {
	if (sep == nullptr) return;

	/*if (sep->IsSet(2) && sep->IsNumber(2)) {
		client->GetCurrentZone()->SendStateCommand(client->GetPlayer(), atol(sep->arg[2]));
	}

	if (sep->IsSet(0) && sep->IsNumber(0)) {
		client->GetPlayer()->SetActionState(atol(sep->arg[0]));
	}

	if (sep->IsSet(1) && sep->IsNumber(1)) {
		client->GetPlayer()->SetVisualState(atol(sep->arg[1]));
	}*/
	/*if (sep->IsSet(0) && sep->IsNumber(0)) {
		client->GetPlayer()->size_mod_a = atol(sep->arg[0]);
	}

	if (sep->IsSet(1) && sep->IsNumber(1)) {
		client->GetPlayer()->size_mod_b = atol(sep->arg[1]);
	}

	if (sep->IsSet(2) && sep->IsNumber(2)) {
		client->GetPlayer()->size_mod_c = atol(sep->arg[2]);
	}

	if (sep->IsSet(3) && sep->IsNumber(3)) {
		client->GetPlayer()->size_shrink_multiplier = atol(sep->arg[3]);
	}

	if (sep->IsSet(4) && sep->IsNumber(4)) {
		client->GetPlayer()->size_mod_unknown = atol(sep->arg[3]);
	}*/

	if (sep->IsSet(0) && sep->IsNumber(0))
		client->GetPlayer()->temp_status = atol(sep->arg[0]);

	client->QueuePacket(client->GetPlayer()->GetSpellBookUpdatePacket(client->GetVersion()));
}

void Commands::Command_LeaveChannel(Client *client, Seperator *sep) {
	const char *channel_name;

	if (sep == NULL || !sep->IsSet(0)) {
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /leavechat <channel name>");
		PrintSep(sep);
		return;
	}

	channel_name = sep->arg[0];

	if (!chat.IsInChannel(client, channel_name))
		client->Message(CHANNEL_COLOR_WHITE, "Unable to leave '%s': You are not in the channel.", channel_name);
	else if (!chat.LeaveChannel(client, channel_name))
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "There was an internal error preventing you from leaving that channel MUAHAHA");
}

/* 
	Function: 
	Purpose	: 
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_WeaponStats(Client* client)
{
	Player* player = client->GetPlayer();

	Item* primary = player->GetEquipmentList()->GetItem(EQ2_PRIMARY_SLOT);
	Item* secondary = player->GetEquipmentList()->GetItem(EQ2_SECONDARY_SLOT);
	Item* ranged = player->GetEquipmentList()->GetItem(EQ2_RANGE_SLOT);
	client->SimpleMessage(0, "Primary:");
	if (primary) {
		client->Message(0, "Name: %s", primary->name.c_str());
		client->Message(0, "Base Damage: %u - %u", primary->weapon_info->damage_low3, primary->weapon_info->damage_high3);
		client->Message(0, "Actual Damage: %u - %u", player->GetPrimaryWeaponMinDamage(), player->GetPrimaryWeaponMaxDamage());
		client->Message(0, "Actual Delay: %u", player->GetPrimaryWeaponDelay());
		client->Message(0, "Proc Percent: %d%%", 0);
		client->Message(0, "Procs Per Minute: %d", 0);
	}
	else {
		client->SimpleMessage(0, "Name: fist");
		client->Message(0, "Base Damage: %u - %u", player->GetPrimaryWeaponMinDamage(), player->GetPrimaryWeaponMaxDamage());
		client->Message(0, "Actual Damage: %u - %u", player->GetPrimaryWeaponMinDamage(), player->GetPrimaryWeaponMaxDamage());
		client->Message(0, "Actual Delay: %d", player->GetPrimaryWeaponDelay() * 0.1);
		client->Message(0, "Proc Percent: %d%%", 0);
		client->Message(0, "Procs Per Minute: %d", 0);
	}
	client->SimpleMessage(0, " ");
	client->SimpleMessage(0, " ");
	if (secondary) {
		client->SimpleMessage(0, "Secondary:");
		client->Message(0, "Name: %s", secondary->name.c_str());
		client->Message(0, "Base Damage: %u - %u", secondary->weapon_info->damage_low3, secondary->weapon_info->damage_high3);
		client->Message(0, "Actual Damage: %u - %u", player->GetSecondaryWeaponMinDamage(), player->GetSecondaryWeaponMaxDamage());
		client->Message(0, "Actual Delay: %d", player->GetSecondaryWeaponDelay() * 0.1);
		client->Message(0, "Proc Percent: %d%%", 0);
		client->Message(0, "Procs Per Minute: %d", 0);
		client->SimpleMessage(0, " ");
		client->SimpleMessage(0, " ");
	}
	client->SimpleMessage(0, "Ranged:");
	if (ranged) {
		client->Message(0, "Name: %s", ranged->name.c_str());
		client->Message(0, "Base Damage: %u - %u", ranged->ranged_info->weapon_info.damage_low3, ranged->ranged_info->weapon_info.damage_high3);
		client->Message(0, "Actual Damage: %u - %u", player->GetRangedWeaponMinDamage(), player->GetRangedWeaponMaxDamage());
		client->Message(0, "Actual Delay: %d", player->GetRangeWeaponDelay() * 0.1);
		client->Message(0, "Proc Percent: %d%%", 0);
		client->Message(0, "Procs Per Minute: %d", 0);
	}
	else
		client->SimpleMessage(0, "None");

}

void Commands::Command_WhoChannel(Client *client, Seperator *sep) {
	const char *channel_name;

	if (sep == NULL || !sep->IsSet(0)) {
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /chatwho <channel name>");
		return;
	}

	channel_name = sep->arg[0];

	if (!chat.ChannelExists(channel_name))
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "That channel does not exist!");
	else
		chat.SendChannelUserList(client, channel_name);
}

void Commands::Command_ZoneSafeCoords(Client *client, Seperator *sep) 
{
	ZoneServer* zone = 0;
	int32 zone_id = client->GetPlayer()->GetZone()->GetZoneID();

	if (zone_id > 0)
	{
		zone = zone_list.Get(zone_id, false);
		if (zone)
		{
			zone->SetSafeX(client->GetPlayer()->GetX());
			zone->SetSafeY(client->GetPlayer()->GetY());
			zone->SetSafeZ(client->GetPlayer()->GetZ());
			zone->SetSafeHeading(client->GetPlayer()->GetHeading());
			if( database.SaveZoneSafeCoords(zone_id, zone->GetSafeX(), zone->GetSafeY(), zone->GetSafeZ(), zone->GetSafeHeading()) )
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Zone safe coordinates updated!");
			else
				client->SimpleMessage(CHANNEL_COLOR_RED, "FAILED to update zone safe coordinates!");
		}
	}
}

/* 
	Function: 
	Purpose	: 
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_ZoneDetails(Client* client, Seperator* sep)
{
	ZoneInfo* zone_info = new ZoneInfo;

	if (sep && sep->arg[0]) 
	{
		if (sep->IsNumber(0))
			zone_info->id = atoi(sep->arg[0]);
		else
			zone_info->id = database.GetZoneID(sep->arg[0]);

		if (zone_info->id > 0) 
		{
			database.LoadZoneInfo(zone_info);
			client->Message(CHANNEL_COLOR_YELLOW, "id: %u, name: %s, file: %s, description: %s", zone_info->id, zone_info->name, zone_info->file, zone_info->description);
			client->Message(CHANNEL_COLOR_YELLOW, "safe_x: %f, safe_y: %f, safe_z: %f, underworld: %f", zone_info->safe_x, zone_info->safe_y, zone_info->safe_z, zone_info->underworld);
			client->Message(CHANNEL_COLOR_YELLOW, "min_status: %u, min_level: %u, max_level: %u, xp_modifier: %u", zone_info->min_status, zone_info->min_level, zone_info->max_level, zone_info->xp_modifier);
			client->Message(CHANNEL_COLOR_YELLOW, "instance_type: %u, shutdown_timer: %u, ruleset_id: %u", zone_info->instance_type, zone_info->shutdown_timer, zone_info->ruleset_id);
			client->Message(CHANNEL_COLOR_YELLOW, "default_reenter_time: %u, default_reset_time: %u, default_lockout_time: %u", zone_info->default_reenter_time, zone_info->default_reenter_time, zone_info->default_lockout_time);
			client->Message(CHANNEL_COLOR_YELLOW, "force_group_to_zone: %u, expansion_id: %u, min_version: %u", zone_info->force_group_to_zone, zone_info->expansion_id, zone_info->min_version);
			client->Message(CHANNEL_COLOR_YELLOW, "always_loaded: %u, city_zone: %u, start_zone: %u, weather_allowed: %u", zone_info->always_loaded, zone_info->city_zone, zone_info->start_zone, zone_info->weather_allowed);
			client->Message(CHANNEL_COLOR_YELLOW, "zone_type: %s", zone_info->zone_type);
			client->Message(CHANNEL_COLOR_YELLOW, "lua_script: %s", zone_info->lua_script);
			client->Message(CHANNEL_COLOR_YELLOW, "zone_motd: %s", zone_info->zone_motd);
		}
		else
			client->Message(CHANNEL_COLOR_YELLOW, "The zone name or ID '%s' does not exist.", sep->arg[0]);
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /zone details [zone id|zone name]");

	safe_delete(zone_info);
}

/* 
	Function: 
	Purpose	: 
	Params	: 
	Dev		: 
	Example	: 
*/ 
void Commands::Command_ZoneSet(Client* client, Seperator* sep)
{
	if (sep && sep->arg[0] && sep->arg[1] && sep->arg[2]) 
	{
		ZoneServer* zone = 0;
		int32 zone_id = 0;

		if (sep->IsNumber(0) && atoi(sep->arg[0]) > 0) 
		{
			zone_id = atoi(sep->arg[0]);
			zone = zone_list.Get(atoi(sep->arg[0]), false);
		}
		else 
		{
			zone_id = database.GetZoneID(sep->arg[0]);

			if (zone_id > 0)
				zone = zone_list.Get(sep->arg[0], false);
		}

		if (zone_id > 0) 
		{
			if (zone_set_values.count(string(sep->arg[1])) > 0)
				SetZoneCommand(client, zone_id, zone, zone_set_values[sep->arg[1]], sep->arg[2]);
			else
				client->Message(CHANNEL_COLOR_YELLOW, "The attribute '%s' is not valid.", sep->arg[1]);
		}
		else
			client->Message(CHANNEL_COLOR_YELLOW, "The zone name or ID '%s' does not exist.", sep->arg[0]);
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /zone set [zone id|zone name] [attribute] [value]");
}

void Commands::Command_Rain(Client* client, Seperator* sep) {
	if (sep == NULL || !sep->IsSet(0) || !sep->IsNumber(0)) {
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /rain <float>");
		return;
	}

	client->Message(CHANNEL_COLOR_YELLOW,"Setting rain to %.2f", atof(sep->arg[0]));
	client->GetCurrentZone()->SetRain(atof(sep->arg[0]));
	client->GetCurrentZone()->SetCurrentWeather(atof(sep->arg[0]));
}

void Commands::Command_Wind(Client* client, Seperator* sep) {
	if (sep == NULL || !sep->IsSet(0) || !sep->IsNumber(0)) {
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /wind <float>");
		return;
	}

	client->Message(CHANNEL_COLOR_YELLOW, "Setting wind to %.2f", atof(sep->arg[0]));
	client->GetCurrentZone()->SetWind(atof(sep->arg[0]));
}


void Commands::Command_SendMerchantWindow(Client* client, Seperator* sep, bool sell) {
	Spawn* spawn = client->GetPlayer()->GetTarget();
	if(spawn) {
		client->SetMerchantTransaction(spawn);
		if (spawn->GetMerchantID() > 0){
			//MerchantFactionMultiplier* multiplier = world.GetMerchantMultiplier(spawn->GetMerchantID());
			//if(!multiplier || (multiplier && client->GetPlayer()->GetFactions()->GetFactionValue(multiplier->faction_id) >= multiplier->faction_min)){
			client->SendBuyMerchantList(sell);
			if(!(spawn->GetMerchantType() & MERCHANT_TYPE_NO_BUY))
				client->SendSellMerchantList(sell);
			if(!(spawn->GetMerchantType() & MERCHANT_TYPE_NO_BUY_BACK))
				client->SendBuyBackList(sell);

			PacketStruct* packet = configReader.getStruct("WS_UpdateMerchant", client->GetVersion());
			if (packet) {
				packet->setDataByName("spawn_id", 0xFFFFFFFF);
				packet->setDataByName("type", 16);
				EQ2Packet* outapp = packet->serialize();
				if (outapp)
					client->QueuePacket(outapp);
				safe_delete(packet);
			}
		}
		if (spawn->GetMerchantType() & MERCHANT_TYPE_REPAIR)
			client->SendRepairList();
	}
	//	client->SimpleMessage(CHANNEL_COLOR_RED, "Your faction is too low to use this merchant.");
}


void Commands::Command_Weather(Client* client, Seperator* sep) 
{
	//PrintSep(sep, "Weather");

	if( sep && sep->arg[0] )
	{
		ZoneServer* zsZone = client->GetCurrentZone();
		const char* value = sep->arg[0];

		// process single-param commands first
		if( strncasecmp(value, "details", strlen(value)) == 0 ) 
		{
			client->Message(CHANNEL_COLOR_YELLOW, "Weather Details for zone %s: ", zsZone->GetZoneName());
			client->Message(CHANNEL_COLOR_YELLOW, "enabled: %i, allowed: %i, type: %i, frequency: %u", zsZone->isWeatherEnabled(), zsZone->isWeatherAllowed(), zsZone->GetWeatherType(), zsZone->GetWeatherFrequency());
			client->Message(CHANNEL_COLOR_YELLOW, "severity: %.2f = %.2f, current: %.2f", zsZone->GetWeatherMinSeverity(), zsZone->GetWeatherMaxSeverity(), zsZone->GetCurrentWeather());
			client->Message(CHANNEL_COLOR_YELLOW, "pattern: %i, chance: %i, amount: %.2f, offset: %.2f", zsZone->GetWeatherPattern(), zsZone->GetWeatherChance(), zsZone->GetWeatherChangeAmount(), zsZone->GetWeatherDynamicOffset());
		}
		else if( strncasecmp(value, "process", strlen(value)) == 0 ) 
		{
			zsZone->SetWeatherLastChangedTime(Timer::GetUnixTimeStamp() - zsZone->GetWeatherFrequency());
			zsZone->ProcessWeather();
		}
		else if( strncasecmp(value, "reset", strlen(value)) == 0 ) 
		{
			zsZone->SetWeatherType(rule_manager.GetGlobalRule(R_Zone, WeatherType)->GetInt8());
			zsZone->SetWeatherFrequency(rule_manager.GetGlobalRule(R_Zone, WeatherChangeFrequency)->GetInt32());
			zsZone->SetWeatherMinSeverity(rule_manager.GetGlobalRule(R_Zone, MinWeatherSeverity)->GetFloat());
			zsZone->SetWeatherMaxSeverity(rule_manager.GetGlobalRule(R_Zone, MaxWeatherSeverity)->GetFloat());
			zsZone->SetCurrentWeather(zsZone->GetWeatherMinSeverity());
			zsZone->SetWeatherPattern(1);
			zsZone->SetWeatherChance(rule_manager.GetGlobalRule(R_Zone, WeatherChangeChance)->GetInt8());
			zsZone->SetWeatherChangeAmount(rule_manager.GetGlobalRule(R_Zone, WeatherChangePerInterval)->GetFloat());
			zsZone->SetWeatherDynamicOffset(rule_manager.GetGlobalRule(R_Zone, WeatherDynamicMaxOffset)->GetFloat());
			zsZone->SetWeatherLastChangedTime(Timer::GetUnixTimeStamp() - zsZone->GetWeatherFrequency());
			zsZone->ProcessWeather();
		}

		// process commands with params
		if( sep->arg[1] )
		{
			if( strncasecmp(value, "enable", strlen(value)) == 0 && sep->IsNumber(1) ) 
				zsZone->SetWeatherEnabled( ( atoi(sep->arg[1]) == 1 ) ? true : false );
			else if( strncasecmp(value, "type", strlen(value)) == 0 && sep->IsNumber(1) && (atoi(sep->arg[1]) >= 0 && atoi(sep->arg[1]) <= 3) ) 
				zsZone->SetWeatherType(atoi(sep->arg[1]));
			else if( strncasecmp(value, "frequency", strlen(value)) == 0 && sep->IsNumber(1)) 
				zsZone->SetWeatherFrequency( atoul(sep->arg[1]) );
			else if( strncasecmp(value, "range", strlen(value)) == 0 && sep->IsNumber(1) && sep->IsNumber(2) ) {
				zsZone->SetWeatherMinSeverity(atof(sep->arg[1]));
				zsZone->SetWeatherMaxSeverity(atof(sep->arg[2]));
				zsZone->SetRain(zsZone->GetWeatherMinSeverity());
			}
			else if( strncasecmp(value, "current", strlen(value)) == 0 && sep->IsNumber(1) ) {
				zsZone->SetCurrentWeather(atof(sep->arg[1]));
				zsZone->SetRain(zsZone->GetCurrentWeather());
			}
			else if( strncasecmp(value, "pattern", strlen(value)) == 0 && sep->IsNumber(1) && (atoi(sep->arg[1]) >= 0 && atoi(sep->arg[1]) <= 2) )
				zsZone->SetWeatherPattern( atoi(sep->arg[1]) );
			else if( strncasecmp(value, "chance", strlen(value)) == 0 && sep->IsNumber(1) && (atoi(sep->arg[1]) >= 0 && atoi(sep->arg[1]) <= 100) )
				zsZone->SetWeatherChance( atoi(sep->arg[1]) );
			else if( strncasecmp(value, "amount", strlen(value)) == 0 && sep->IsNumber(1) && (atoi(sep->arg[1]) >= 0 && atoi(sep->arg[1]) <= 1) )
				zsZone->SetWeatherChangeAmount( atof(sep->arg[1]) );
			else if( strncasecmp(value, "offset", strlen(value)) == 0 && sep->IsNumber(1) && (atoi(sep->arg[1]) >= 0 && atoi(sep->arg[1]) <= 1) )
				zsZone->SetWeatherDynamicOffset( atof(sep->arg[1]) );
		}
	}
	else
	{
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /weather [command] [param]");
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Commands: enable (0|1), type (0-3), frequency (sec), range|current (0.0 - 1.0)");
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Commands: details, process, pattern (0|1), chance (%), amount (float), offset (float)");
		return;
	}

}

void Commands::Command_Select(Client* client, Seperator* sep) {
	if (sep && sep->arg[1]) {
		Spawn* spawn = client->GetPlayer()->GetTarget();
		if (spawn && strlen(sep->arg[1]) > 0)
			client->GetCurrentZone()->CallSpawnScript(spawn, SPAWN_SCRIPT_CUSTOM, client->GetPlayer(), sep->arg[1]);
	}
}

/* 
	Function: Command_ConsumeFood()
	Purpose	: Consume Food/Drink and apply mods
	Params	: Slot ID - EQ2_FOOD_SLOT 22, EQ2_DRINK_SLOT 23
	Dev		: Zcoretri
	Example	: /consume_food 22
*/ 
void Commands::Command_ConsumeFood(Client* client, Seperator* sep) {
	if (sep && sep->arg[0] && sep->IsNumber(0))
	{
		Player* player = client->GetPlayer();
		int8 slot = atoi(sep->arg[0]);
		Item* item = player->GetEquipmentList()->GetItem(slot);

		if(item) {
			LogWrite(MISC__INFO, 1, "Command", "ItemID: %u, ItemName: %s ItemCount: %i ", item->details.item_id, item->name.c_str(), item->details.count);
			if(item->GetItemScript() && lua_interface){
				lua_interface->RunItemScript(item->GetItemScript(), "cast", item, client->GetPlayer());
				if (slot == 22)
					client->Message(CHANNEL_COLOR_WHITE, "You eat a %s.", item->name.c_str());
				else
					client->Message(CHANNEL_COLOR_WHITE, "You drink a %s.", item->name.c_str());
			}
		}

		if (item->details.count > 1) {
			item->details.count -= 1;
			item->save_needed = true;
		}
		else {
			player->GetEquipmentList()->RemoveItem(slot, true);
			database.DeleteItem(player->GetCharacterID(), item, "EQUIPPED");
		}
		client->GetPlayer()->SetCharSheetChanged(true);
		client->QueuePacket(player->GetEquipmentList()->serialize(client->GetVersion(), player));
	}
}

void Commands::Command_Aquaman(Client* client, Seperator* sep) {
	if (sep && sep->arg[0] && sep->IsNumber(0)) {
		if (atoi(sep->arg[0]) == 1) {
			client->GetPlayer()->GetInfoStruct()->vision = 4;
			client->GetPlayer()->GetInfoStruct()->breathe_underwater = 1;
			client->GetPlayer()->SetCharSheetChanged(true);
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Maybe you ought to stick to the shallow end until you know how to swim.");
		}
		else {
			client->GetPlayer()->GetInfoStruct()->vision = 0;
			client->GetPlayer()->GetInfoStruct()->breathe_underwater = 0;
			client->GetPlayer()->SetCharSheetChanged(true);
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Aquaman mode turned off.");
		}
	}
	else
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /aquaman [0|1]");
}

void Commands::Command_ReportBug(Client* client, Seperator* sep) 
{
	if(sep)
	{
		string data;

		if(sep->arg[0]){
			data = string(sep->arg[0]);
		}

		for(int i=1;i<sep->GetMaxArgNum();i++){
			if(sep->arg[i])
				data.append(" ").append(sep->arg[i]);
		}

		const char* target_name = 0;
		int32 spawn_id = 0;

		if(client->GetPlayer()->GetTarget())
		{
			target_name = client->GetPlayer()->GetTarget()->GetName();
			spawn_id = client->GetPlayer()->GetTarget()->GetDatabaseID();
		}
		else
			target_name = "N/A";

		LogWrite(COMMAND__DEBUG, 1, "Command", "%s", data.c_str());

		if(world.ReportBug(data, client->GetPlayer()->GetName(), client->GetAccountID(), target_name, spawn_id, client->GetCurrentZone()->GetZoneID()))
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Successfully submitted bug.");
		else
			client->SimpleMessage(CHANNEL_COLOR_RED, "Error submitting bug.");
	}
}

void Commands::Command_Attune_Inv(Client* client, Seperator* sep) {
	PrintSep(sep, "Command_Attune_Inv");
	if (sep && sep->arg[0] && sep->IsNumber(0)) {
		// Get the item index from the first parameter
		int16 index = atoi(sep->arg[0]);
		// Check to see if this is a valid item index for the player, if not exit out of the function
		if (client->GetPlayer()->item_list.indexed_items.count(index) == 0) {
			LogWrite(ITEM__DEBUG, 0, "Items", "%s has no item with an index of %i", client->GetPlayer()->GetName(), index);
			return;
		}

		// Get the item
		Item* item = client->GetPlayer()->item_list.indexed_items[index];
		if (item) {
			// Valid item lets check to make sure this item is attunable, if not return out
			if (!item->CheckFlag(ATTUNEABLE)) {
				LogWrite(ITEM__DEBUG, 0, "Items", "attune_inv called for an item that is not attunable (%s)", item->name.c_str());
				return;
			}

			// Remove the attunable flag
			item->generic_info.item_flags -= ATTUNEABLE;
			// Set the attuned flag
			item->generic_info.item_flags += ATTUNED;
			// Flag this item for saving
			item->save_needed = true;

			client->QueuePacket(item->serialize(client->GetVersion(), false, client->GetPlayer()));

			vector<EQ2Packet*> packets = client->GetPlayer()->EquipItem(index, client->GetVersion(), -1);
			EQ2Packet* outapp = 0;

			for (int32 i = 0; i < packets.size(); i++) {
				outapp = packets[i];
				if (outapp)
					client->QueuePacket(outapp);
			}
		}

	}
}

void Commands::Command_Reset_Zone_Timer(Client* client, Seperator* sep) {
	PrintSep(sep, "Command_Reset_Zone_Timer");
	/*if (sep && sep->arg[0] && sep->IsNumber(0)) {
		int32 db_id = atoul(sep->arg[0]);
		InstanceData* data = client->GetPlayer()->GetCharacterInstances().FindInstanceByDBID(db_id);
		if (data) {
			// TODO: add a check to timers to ensure it can be reset

			// Delete the character from the instance
			database.DeleteCharacterFromInstance(client->GetPlayer()->GetCharacterID(), data->instance_id);
			data->instance_id = 0;

			// Update the success time and set to 0 so the player can enter it again
			database.UpdateCharacterInstance(client->GetPlayer()->GetCharacterID(), data->zone_name, 0, 1, 0);
			data->last_success_timestamp = 0;
		}
	}*/
}

void Commands::Command_Player(Client* client, Seperator* sep) {
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, " -- /player syntax --");
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "/player coins");
}

void Commands::Command_Player_Set(Client* client, Seperator* sep) {
	Player* player = client->GetPlayer();
	if (player->HasTarget() && player->GetTarget()->IsPlayer())
		player = static_cast<Player*>(player->GetTarget());

	if (sep && sep->arg[0] && sep->arg[1]) {
		const char* attribute = sep->arg[0];
		int value = 0;

		if (strncasecmp(attribute, "alignment", strlen(attribute)) == 0) {
			if (sep->IsNumber(1)) {
				value = atoi(sep->arg[1]);
				player->SetAlignment(value);

				vector<Entity*> players = player->GetZone()->GetPlayers();
				vector<Entity*>::iterator itr;
				for (itr = players.begin(); itr != players.end(); ++itr) {
					Player* update_player = static_cast<Player*>(*itr);

					if (update_player == player) continue;
					if (update_player->GetDistance(player) > SEND_SPAWN_DISTANCE) continue;

					update_player->GetZone()->RepopSpawns(update_player->GetZone()->GetClientBySpawn(update_player), player);
					player->GetZone()->RepopSpawns(player->GetZone()->GetClientBySpawn(player), update_player);
				}
				return;
			}
		} else if (strncasecmp(attribute, "fame", strlen(attribute)) == 0) {
			if (sep->IsNumber(1)) {
				value = atoi(sep->arg[1]);
				player->SetFame(value);
				player->GetZone()->GetClientBySpawn(player)->SendTitleUpdate();
				return;
			}
		} else if (strncasecmp(attribute, "str", strlen(attribute)) == 0) {
			if (sep->IsNumber(1)) {
				player->GetInfoStruct()->str_temp = atoi(sep->arg[1]);
				player->CalculateBonuses();
				return;
			}
		} else if (strncasecmp(attribute, "sta", strlen(attribute)) == 0) {
			if (sep->IsNumber(1)) {
				player->GetInfoStruct()->sta_temp = atoi(sep->arg[1]);
				player->CalculateBonuses();
				return;
			}
		} else if (strncasecmp(attribute, "agi", strlen(attribute)) == 0) {
			if (sep->IsNumber(1)) {
				player->GetInfoStruct()->agi_temp = atoi(sep->arg[1]);
				player->CalculateBonuses();
				return;
			}
		} else if (strncasecmp(attribute, "wis", strlen(attribute)) == 0) {
			if (sep->IsNumber(1)) {
				player->GetInfoStruct()->wis_temp = atoi(sep->arg[1]);
				player->CalculateBonuses();
				return;
			}
		} else if (strncasecmp(attribute, "int", strlen(attribute)) == 0) {
			if (sep->IsNumber(1)) {
				player->GetInfoStruct()->intel_temp = atoi(sep->arg[1]);
				player->CalculateBonuses();
				return;
			}
		} else if (strncasecmp(attribute, "crit", strlen(attribute)) == 0) {
			if (sep->IsNumber(1)) {
				player->GetInfoStruct()->crit_chance_temp = atof(sep->arg[1]);
				player->CalculateBonuses();
				return;
			}
		} else if (strncasecmp(attribute, "reuse", strlen(attribute)) == 0) {
			if (sep->IsNumber(1)) {
				player->GetInfoStruct()->reuse_speed_temp = atof(sep->arg[1]);
				player->CalculateBonuses();
				return;
			}
		} else if (strncasecmp(attribute, "castspeed", strlen(attribute)) == 0) {
			if (sep->IsNumber(1)) {
				player->GetInfoStruct()->casting_speed_temp = atof(sep->arg[1]);
				player->CalculateBonuses();
				return;
			}
		}
	}

	client->SimpleMessage(CHANNEL_COLOR_YELLOW, " -- /player set syntax --");
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "/player set [attribute] [value] - sets the given attribute to the provided value");
}

void Commands::Command_Player_Coins(Client* client, Seperator* sep) {
	// /player coins add 10
	// /player coins add plat 10
	Player* player = client->GetPlayer();
	if (player->HasTarget() && player->GetTarget()->IsPlayer())
		player = (Player*)player->GetTarget();


	if (sep && sep->arg[0] && sep->arg[1]) {
		const char* action = sep->arg[0];
		int64 value = 0;

		if (strncasecmp(action, "add", strlen(action)) == 0) {
			if (sep->IsNumber(1)) {
				value = atoi64(sep->arg[1]);
				player->AddCoins(value);
				
				if (client->GetPlayer() == player)
					client->Message(CHANNEL_COLOR_YELLOW, "You give yourself %llu coin%s", value, (value > 1 ? "s" : ""));
				else {
					client->Message(CHANNEL_COLOR_YELLOW, "You give %s %llu coin%s", player->GetName(), value, (value > 1 ? "s" : ""));
					client->GetCurrentZone()->GetClientBySpawn(player)->Message(CHANNEL_COLOR_YELLOW, "%s gave you %llu coin%s", client->GetPlayer()->GetName(), value, (value > 1 ? "s" : ""));
				}

				return;
			}
			else if (sep->arg[2] && sep->IsNumber(2)) {
				const char* type = sep->arg[1];
				if (strncasecmp(type, "copper", strlen(type)) == 0) {
					value = atoi64(sep->arg[2]);
				}
				else if (strncasecmp(type, "silver", strlen(type)) == 0) {
					value = atoi64(sep->arg[2]) * 100;
				}
				else if (strncasecmp(type, "gold", strlen(type)) == 0) {
					value = atoi64(sep->arg[2]) * 10000;
				}
				else if (strncasecmp(type, "plat", strlen(type)) == 0) {
					value = atoi64(sep->arg[2]) * 1000000;
				}
				player->AddCoins(value);
				
				if (client->GetPlayer() == player)
					client->Message(CHANNEL_COLOR_YELLOW, "You give yourself %llu coin%s", value, (value > 1 ? "s" : ""));
				else {
					client->Message(CHANNEL_COLOR_YELLOW, "You give %s %llu coin%s", player->GetName(), value, (value > 1 ? "s" : ""));
					client->GetCurrentZone()->GetClientBySpawn(player)->Message(CHANNEL_COLOR_YELLOW, "%s gave you %llu coin%s", client->GetPlayer()->GetName(), value, (value > 1 ? "s" : ""));
				}
				return;
			}
		}
	}

	client->SimpleMessage(CHANNEL_COLOR_YELLOW, " -- /player coins syntax --");
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "/player coins add [value] - adds the given number of coins to the player");
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "/player coins add copper [value] - adds the given amount of copper to the player");
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "/player coins add silver [value] - adds the given amount of silver to the player");
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "/player coins add gold [value] - adds the given amount of gold to the player");
	client->SimpleMessage(CHANNEL_COLOR_YELLOW, "/player coins add plat [value] - adds the given amount of platinum to the player");
}

void Commands::Command_AchievementAdd(Client* client, Seperator* sep) {
	PrintSep(sep, "ACHIEVEMENT_ADD");
}

void Commands::Command_Editor(Client* client, Seperator* sep) {
	PacketStruct* packet = configReader.getStruct("WS_ChoiceWindow", client->GetVersion());
	if (packet) {
		string url = string(rule_manager.GetGlobalRule(R_World, EditorURL)->GetString());

		if (rule_manager.GetGlobalRule(R_World, EditorOfficialServer)->GetBool()) {
			char command[255];
			url = "browser " + url;
			int32 spawn_id = 0;
			int32 zone_id = 0;
			string type;

			if (client->GetCurrentZone())
				zone_id = client->GetCurrentZone()->GetZoneID();

			if (client->GetPlayer()->GetTarget()) {
				Spawn* target = client->GetPlayer()->GetTarget();
				if (target->IsWidget())
					type = "widgets";
				else if (target->IsSign())
					type = "signs";
				else if (target->IsObject())
					type = "objects";
				else if (target->IsNPC())
					type = "npcs";
				else if (target->IsGroundSpawn())
					type = "ground";
				else
					type = "spawn";

				spawn_id = client->GetPlayer()->GetTarget()->GetDatabaseID();
			}

			sprintf(command, url.c_str(), zone_id, type.c_str(), spawn_id);

			packet->setDataByName("accept_command", command);
		}
		else if (rule_manager.GetGlobalRule(R_World, EditorIncludeID)->GetBool()) {
			char command[255];
			url = "browser " + url;
			if (client->GetPlayer()->GetTarget())
				sprintf(command, url.c_str(), client->GetPlayer()->GetTarget()->GetDatabaseID());

			packet->setDataByName("accept_command", command);
		}
		else {
			string command = "browser " + url;
			packet->setDataByName("accept_command", command.c_str());
		}

		packet->setDataByName("text", "Open the web editor?");
		packet->setDataByName("accept_text", "Open");
		
		packet->setDataByName("cancel_text", "Cancel");
		// No clue if we even need the following 2 unknowns, just added them so the packet matches what live sends
		packet->setDataByName("unknown2", 50);
		packet->setDataByName("unknown4", 1);

		client->QueuePacket(packet->serialize());
		safe_delete(packet);
	}
}

void Commands::Command_AcceptResurrection(Client* client, Seperator* sep) {
	if(!client || !sep || client->GetPlayer()->GetID() != atoul(sep->arg[0]))
		return;
	client->GetResurrectMutex()->writelock(__FUNCTION__, __LINE__);
	if(client->GetCurrentRez()->active)
		client->AcceptResurrection();
	client->GetResurrectMutex()->releasewritelock(__FUNCTION__, __LINE__);
}

void Commands::Command_DeclineResurrection(Client* client, Seperator* sep) {
	if(!client || !sep || client->GetPlayer()->GetID() != atoul(sep->arg[0]))
		return;
	client->GetResurrectMutex()->writelock(__FUNCTION__, __LINE__);
	if(client->GetCurrentRez()->active)
		client->GetCurrentRez()->should_delete = true;
	client->GetResurrectMutex()->releasewritelock(__FUNCTION__, __LINE__);
}

void Commands::Command_ServerFlag(Client* client, Seperator* sep) {
	if (sep && sep->arg[0] && sep->arg[1] && sep->arg[2]) {
		client->GetPlayer()->SetPlayerControlFlag(atoi(sep->arg[0]), atoi(sep->arg[1]), atoi(sep->arg[2]));
	}
	else {
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Usage: /server_flag param1 param2 [0/1]");
	}
}

void Commands::Command_PVPRange(Client* client) {
	if (PVP::IsEnabled()) {
		if (PVP::IsEnabled(client->GetCurrentZone())) {
			client->SimpleMessage(CHANNEL_COLOR_WHITE, "PVP is enabled in this zone.");
			client->SimpleMessage(CHANNEL_COLOR_WHITE, "Level Range: Unlimited");
		} else {
			client->SimpleMessage(CHANNEL_COLOR_WHITE, "PVP is disabled in this zone.");
		}
	}
}

void Commands::Command_PVP(Client* client) {
	if (!PVP::IsEnabled())
		return;

	client->SimpleMessage(CHANNEL_COLOR_WHITE, "PVP Statistics");
	client->Message(CHANNEL_COLOR_WHITE, "Kills: %i", client->GetPlayer()->GetPlayerStatisticValue(STAT_PLAYER_TOTAL_PVP_KILLS));
	client->Message(CHANNEL_COLOR_WHITE, "Deaths: %i", client->GetPlayer()->GetPlayerStatisticValue(STAT_PLAYER_TOTAL_PVP_DEATHS));
}

void Commands::Command_KnowledgeWindow_Sort(Client* client, Seperator* sep) {
	if (sep && sep->arg[0] && sep->arg[1] && sep->arg[2] && sep->arg[3] && sep->arg[4]) {
		int8 tab = atoi(sep->arg[0]);
		int8 sort_by = atoi(sep->arg[1]);
		bool reverse = atoi(sep->arg[2]);
		int8 fill_style = atoi(sep->arg[3]);
		bool max_level_only = atoi(sep->arg[4]);

		client->GetPlayer()->SortSpellBook();
		ClientPacketFunctions::SendSkillSlotMappings(client);
	}
}

void Commands::Command_Heal(Client* client) {
	Player* player = client->GetPlayer();

	if (client->GetPlayer()->GetTarget() && client->GetPlayer()->GetTarget()->IsPlayer())
		player = client->GetCurrentZone()->GetClientBySpawn(client->GetPlayer()->GetTarget())->GetPlayer();

	if (!player->Alive()) {
		Spell* spell = master_spell_list.GetSpell(1002182, 1);
		LuaSpell* lua_spell = nullptr;

		if (spell && lua_interface) {
			lua_spell = lua_interface->GetSpell(spell->GetSpellData()->lua_script.c_str());
		}

		if (lua_spell) {
			lua_spell->caster = client->GetPlayer();
			lua_spell->initial_target = player->GetID();
			lua_spell->spell = spell;

			client->GetCurrentZone()->GetSpellProcess()->GetSpellTargets(lua_spell);
			client->GetCurrentZone()->GetSpellProcess()->CastProcessedSpell(lua_spell, true);
		}
	} else {
		int32 heal_amount = player->GetTotalHP() - player->GetHP();

		if (heal_amount > 0)
			client->GetPlayer()->ProcHeal(player, "Heal", heal_amount, heal_amount, "Dev Heal");
	}
}