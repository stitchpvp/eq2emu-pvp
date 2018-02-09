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
#include "Spells.h"
#include "../common/ConfigReader.h"
#include "WorldDatabase.h"
#include "../common/Log.h"
#include "Traits/Traits.h"
#include "AltAdvancement/AltAdvancement.h"

extern ConfigReader configReader;
extern WorldDatabase database;
extern MasterTraitList master_trait_list;
extern MasterAAList master_aa_list;

Spell::Spell(){
	spell = new SpellData;
	heal_spell = false;
	buff_spell = false;
	damage_spell = false;
	control_spell = false;
	offense_spell = false;
	MSpellInfo.SetName("Spell::MSpellInfo");
}

Spell::Spell(SpellData* in_spell){
	spell = in_spell;
	heal_spell = false;
	buff_spell = false;
	damage_spell = false;
	control_spell = false;
	offense_spell = false;
	MSpellInfo.SetName("Spell::MSpellInfo");
}

Spell::~Spell(){
	vector<LevelArray*>::iterator itr1;
	for(itr1=levels.begin();itr1!=levels.end();itr1++) {
		safe_delete(*itr1);
	}
	vector<SpellDisplayEffect*>::iterator itr2;
	for(itr2=effects.begin();itr2!=effects.end();itr2++) {
		safe_delete(*itr2);
	}
	vector<LUAData*>::iterator itr3;
	for(itr3=lua_data.begin();itr3!=lua_data.end();itr3++) {
		safe_delete(*itr3);
	}
	safe_delete(spell);
}

void Spell::AddSpellLuaData(int8 type, int int_value, float float_value, bool bool_value, string string_value){
	LUAData* data = new LUAData;
	data->type = type;
	data->int_value = int_value;
	data->float_value = float_value;
	data->bool_value = bool_value;
	data->string_value = string_value;
	MSpellInfo.lock();
	lua_data.push_back(data);
	MSpellInfo.unlock();
}

void Spell::AddSpellLuaDataInt(int value) {
	LUAData *data = new LUAData;

	data->type = 0;
	data->int_value = value;
	data->float_value = 0;
	data->bool_value = false;

	MSpellInfo.lock();
	lua_data.push_back(data);
	MSpellInfo.unlock();
}

void Spell::AddSpellLuaDataFloat(float value) {
	LUAData *data = new LUAData;

	data->type = 1;
	data->int_value = 0;
	data->float_value = value;
	data->bool_value = false;

	MSpellInfo.lock();
	lua_data.push_back(data);
	MSpellInfo.unlock();
}

void Spell::AddSpellLuaDataBool(bool value) {
	LUAData *data = new LUAData;

	data->type = 2;
	data->int_value = 0;
	data->float_value = 0;
	data->bool_value = value;

	MSpellInfo.lock();
	lua_data.push_back(data);
	MSpellInfo.unlock();
}

void Spell::AddSpellLuaDataString(string value) {
	LUAData *data = new LUAData;

	data->type = 3;
	data->int_value = 0;
	data->float_value = 0;
	data->bool_value = false;
	data->string_value = value;

	MSpellInfo.lock();
	lua_data.push_back(data);
	MSpellInfo.unlock();
}

int16 Spell::GetLevelRequired(Client* client){
	int16 ret = 0xFFFF;
	if(!client)
		return ret;
	LevelArray* level = 0;
	vector<LevelArray*>::iterator itr;
	for(itr = levels.begin(); itr != levels.end(); itr++){
		level = *itr;
		if(level && level->adventure_class == client->GetPlayer()->GetAdventureClass()){
			ret = level->spell_level/10;
			break;
		}
	}
	return ret;
}

void Spell::SetPacketInformation(PacketStruct* packet, Client* client, bool display_tier) {
	SetSpellPacketInformation(packet, client, display_tier, false);
	SetSpellPacketInformation(packet, client, display_tier, true);
}

void Spell::SetSpellPacketInformation(PacketStruct* packet, Client* client, bool display_tier, bool pvp) {
	const char* name = "spell_info";
	if (pvp)
		name = "pvp_spell_info";

	packet->setSubstructDataByName(name, "id", spell->id);
	packet->setSubstructDataByName(name, "icon", spell->icon);
	packet->setSubstructDataByName(name, "icon2", spell->icon_heroic_op);	// fix struct element name eventually
	packet->setSubstructDataByName(name, "icontype", spell->icon_backdrop);	// fix struct element name eventually
	
	if (packet->GetVersion() >= 63119) {
		packet->setSubstructDataByName(name, "version", 0x04);
		packet->setSubstructDataByName(name, "sub_version", 0x24);
	} else {
		packet->setSubstructDataByName(name, "version", 0x11);
		packet->setSubstructDataByName(name, "sub_version", 0x14);
	}

	packet->setSubstructDataByName(name, "type", spell->type);
	packet->setSubstructDataByName(name, "unknown_MJ1d", 1); //63119 test
	packet->setSubstructDataByName(name, "class_skill", spell->class_skill);
	packet->setSubstructDataByName(name, "mastery_skill", spell->mastery_skill);
	packet->setSubstructDataByName(name, "duration_flag", spell->duration_until_cancel);

	if (client && spell->type != 2) {
		sint8 spell_text_color = client->GetPlayer()->GetArrowColor(GetLevelRequired(client));

		if(spell_text_color != ARROW_COLOR_WHITE && spell_text_color != ARROW_COLOR_RED && spell_text_color != ARROW_COLOR_GRAY)
			spell_text_color = ARROW_COLOR_WHITE;

		spell_text_color -= 6;

		if(spell_text_color < 0)
			spell_text_color *= -1;

		packet->setSubstructDataByName(name, "spell_text_color", spell_text_color);
	} else {
		packet->setSubstructDataByName(name, "spell_text_color", 3);
	}

	if (spell->type != 2) {
		packet->setSubstructArrayLengthByName(name, "num_levels", levels.size());

		for (int32 i=0;i<levels.size();i++) {
			packet->setArrayDataByName("adventure_class", levels[i]->adventure_class, i);
			packet->setArrayDataByName("tradeskill_class", levels[i]->tradeskill_class, i);
			packet->setArrayDataByName("spell_level", levels[i]->spell_level, i);
		}
	}

	packet->setSubstructDataByName(name, "unknown9", 20);

	int16 hp_req = 0;
	int16 power_req = 0;

	if (client) {
		hp_req = GetHPRequired(client->GetPlayer());
		power_req = GetPowerRequired(client->GetPlayer());

		// might need version checks around these?
		if( client->GetVersion() >= 1193 )
		{
			int16 savagery_req = GetSavageryRequired(client->GetPlayer()); // dunno why we need to do this
			packet->setSubstructDataByName(name, "savagery_req",savagery_req);
			packet->setSubstructDataByName(name, "savagery_upkeep",spell->savagery_upkeep);
		}
		if( client->GetVersion() >= 57048 )
		{
			int16 dissonance_req = GetDissonanceRequired(client->GetPlayer()); // dunno why we need to do this
			packet->setSubstructDataByName(name, "dissonance_req",dissonance_req);
			packet->setSubstructDataByName(name, "dissonance_upkeep",spell->dissonance_upkeep);
		}
	}

	packet->setSubstructDataByName(name, "target", spell->target_type);
	packet->setSubstructDataByName(name, "recovery", spell->recovery);
	packet->setSubstructDataByName(name, "health_upkeep", spell->hp_upkeep);
	packet->setSubstructDataByName(name, "health_req", hp_req);
	packet->setSubstructDataByName(name, "tier", spell->tier);
	packet->setSubstructDataByName(name, "power_req", power_req);
	packet->setSubstructDataByName(name, "power_upkeep", spell->power_upkeep);
	packet->setSubstructDataByName(name, "cast_time", GetModifiedCastTime(client->GetPlayer()));
	packet->setSubstructDataByName(name, "recast", GetModifiedRecast(client->GetPlayer()));
	packet->setSubstructDataByName(name, "radius", spell->radius);
	packet->setSubstructDataByName(name, "req_concentration", spell->req_concentration);
	packet->setSubstructDataByName(name, "max_aoe_targets", spell->max_aoe_targets);
	packet->setSubstructDataByName(name, "friendly_spell", spell->friendly_spell);

	packet->setSubstructArrayLengthByName(name, "num_effects", effects.size());
	for(int32 i=0;i<effects.size();i++){
		packet->setArrayDataByName("effect", effects[i]->description.c_str(), i);
		packet->setArrayDataByName("percentage", effects[i]->percentage, i);
		packet->setArrayDataByName("subbulletflag", effects[i]->subbullet, i);
	}

	if (display_tier == true) {
		packet->setSubstructDataByName(name, "display_spell_tier", spell->display_spell_tier);
	} else {
		packet->setSubstructDataByName(name, "display_spell_tier", 0);
	}

	packet->setSubstructDataByName(name, "range",spell->range);
	packet->setSubstructDataByName(name, "duration1",spell->duration1);
	packet->setSubstructDataByName(name, "duration2",spell->duration2);
	packet->setSubstructDataByName(name, "can_effect_raid",spell->can_effect_raid);
	packet->setSubstructDataByName(name, "affect_only_group_members",spell->affect_only_group_members);
	packet->setSubstructDataByName(name, "group_spell",spell->group_spell);
	packet->setSubstructDataByName(name, "resistibility",spell->resistibility);
	packet->setSubstructDataByName(name, "name",&(spell->name));
	packet->setSubstructDataByName(name, "description",&(spell->description));
}

EQ2Packet* Spell::SerializeSpecialSpell(Client* client, bool display, int8 packet_type, int8 sub_packet_type){
	return SerializeSpell(client, display, false, packet_type, sub_packet_type, "WS_ExamineSpecialSpellInfo");
}

EQ2Packet* Spell::SerializeAASpell(Client* client, AltAdvanceData* data, bool display, int16 packet_type, int8 sub_packet_type){
	if (!client)
		return 0;

	PacketStruct* packet = configReader.getStruct("WS_ExamineAASpellInfo", client->GetVersion());
	packet->setSubstructDataByName("info_header", "show_name", 0);
	packet->setSubstructDataByName("info_header", "show_popup", 0);
	packet->setSubstructDataByName("info_header", "packettype", packet_type);
	packet->setSubstructDataByName("info_header", "packetsubtype", sub_packet_type);
	packet->setSubstructDataByName("Spell_info", "aa_id", data->spellID);
	packet->setSubstructDataByName("Spell_info", "aa_tab_id", data->group);
	packet->setSubstructDataByName("Spell_info", "aa_icon", data->icon);
	packet->setSubstructDataByName("Spell_info", "aa_icon2", data->icon2);
	packet->setSubstructDataByName("Spell_info", "current_rank", 0); // how to get this info to here?
	packet->setSubstructDataByName("Spell_info", "max_rank", data->maxRank);
	packet->setSubstructDataByName("Spell_info", "rank_cost", data->rankCost);
	// Spell info
	packet->setSubstructDataByName("Spell_info", "id", spell->id);
	packet->setSubstructDataByName("spell_info", "icon", spell->icon);
	packet->setSubstructDataByName("spell_info", "icon2", spell->icon_heroic_op);
	packet->setSubstructDataByName("spell_info", "icontype", spell->icon_backdrop);
	packet->setSubstructDataByName("spell_info", "version",0x04);
	packet->setSubstructDataByName("spell_info", "sub_version",0x03EA);
	packet->setSubstructDataByName("spell_info", "type", spell->type);
	packet->setSubstructDataByName("spell_info", "class_skill", spell->class_skill);
	packet->setSubstructDataByName("spell_info", "mastery_skill", spell->mastery_skill);
	if(spell->type != 2){
		sint8 spell_text_color = client->GetPlayer()->GetArrowColor(GetLevelRequired(client));
		if(spell_text_color != ARROW_COLOR_WHITE && spell_text_color != ARROW_COLOR_RED && spell_text_color != ARROW_COLOR_GRAY)
			spell_text_color = ARROW_COLOR_WHITE;
		spell_text_color -= 6;
		if(spell_text_color < 0)
			spell_text_color*=-1;
		packet->setSubstructDataByName("spell_info", "spell_text_color", spell_text_color);
	}
	else
		packet->setSubstructDataByName("spell_info", "spell_text_color", 3);
	if(spell->type != 2){
		packet->setSubstructArrayLengthByName("spell_info", "num_levels", levels.size());
		for(int32 i=0;i<levels.size();i++){
			packet->setArrayDataByName("adventure_class", levels[i]->adventure_class, i);
			packet->setArrayDataByName("tradeskill_class", levels[i]->tradeskill_class, i);
			packet->setArrayDataByName("spell_level", levels[i]->spell_level, i);
		}
	}
	int16 hp_req = GetHPRequired(client->GetPlayer());
	int16 power_req = GetPowerRequired(client->GetPlayer());
	packet->setSubstructDataByName("spell_info", "tier",spell->tier);
	packet->setSubstructDataByName("spell_info", "health_req",hp_req);
	packet->setSubstructDataByName("spell_info", "health_upkeep",spell->hp_upkeep);
	packet->setSubstructDataByName("spell_info", "power_req",power_req);
	packet->setSubstructDataByName("spell_info", "power_upkeep",spell->power_upkeep);
	// might need version checks around these?
	if( client->GetVersion() >= 1193 )
	{
		int16 savagery_req = GetSavageryRequired(client->GetPlayer()); // dunno why we need to do this
		packet->setSubstructDataByName("spell_info", "savagery_req",savagery_req);
		packet->setSubstructDataByName("spell_info", "savagery_upkeep",spell->savagery_upkeep);
	}
	if( client->GetVersion() >= 57048 )
	{
		int16 dissonance_req = GetDissonanceRequired(client->GetPlayer()); // dunno why we need to do this
		packet->setSubstructDataByName("spell_info", "dissonance_req",dissonance_req);
		packet->setSubstructDataByName("spell_info", "dissonance_upkeep",spell->dissonance_upkeep);
	}
	packet->setSubstructDataByName("spell_info", "req_concentration",spell->req_concentration);
	packet->setSubstructDataByName("spell_info", "cast_time", GetModifiedCastTime(client->GetPlayer()));
	packet->setSubstructDataByName("Spell_info", "recovery", spell->recovery);
	packet->setSubstructDataByName("spell_info", "recast", GetModifiedRecast(client->GetPlayer()));
	packet->setSubstructDataByName("spell_info", "radius",spell->radius);
	packet->setSubstructDataByName("spell_info", "max_aoe_targets",spell->max_aoe_targets);
	packet->setSubstructDataByName("spell_info", "friendly_spell",spell->friendly_spell);
	packet->setSubstructArrayLengthByName("spell_info", "num_effects", effects.size());
	for(int32 i=0;i<effects.size();i++){
		packet->setArrayDataByName("effect", effects[i]->description.c_str(), i);
		packet->setArrayDataByName("percentage", effects[i]->percentage, i);
		packet->setArrayDataByName("subbulletflag", effects[i]->subbullet, i);
	}
	packet->setSubstructDataByName("spell_info", "display_spell_tier", spell->display_spell_tier);
	packet->setSubstructDataByName("spell_info", "minimum_range", spell->min_range);
	packet->setSubstructDataByName("spell_info", "range",spell->range);
	packet->setSubstructDataByName("spell_info", "duration1",spell->duration1);
	packet->setSubstructDataByName("spell_info", "duration2",spell->duration2);
	packet->setSubstructDataByName("spell_info", "duration_flag", spell->duration_until_cancel);
	packet->setSubstructDataByName("spell_info", "target", spell->target_type);
	packet->setSubstructDataByName("spell_info", "can_effect_raid",spell->can_effect_raid);
	packet->setSubstructDataByName("spell_info", "affect_only_group_members",spell->affect_only_group_members);
	packet->setSubstructDataByName("spell_info", "group_spell",spell->group_spell);
	packet->setSubstructDataByName("spell_info", "resistibility",spell->resistibility);
	packet->setSubstructDataByName("spell_info", "hit_bonus", spell->hit_bonus);
	packet->setSubstructDataByName("spell_info", "name", &(spell->name));
	packet->setSubstructDataByName("spell_info", "description", &(spell->description));

	EQ2Packet* packetdata = packet->serialize();
	EQ2Packet* app = new EQ2Packet(OP_AdventureList, packetdata->pBuffer, packetdata->size);
	//DumpPacket(app);
	safe_delete(packet);
	safe_delete(data);
	return app;
}

EQ2Packet* Spell::SerializeSpell(Client* client, bool display, bool trait_display, int8 packet_type, int8 sub_packet_type, const char* struct_name){
	int16 version = 1;
	if(client)
		version = client->GetVersion();
	if(!struct_name)
		struct_name = "WS_ExamineSpellInfo";
	PacketStruct* packet = configReader.getStruct(struct_name, version);
	if(display)
		packet->setSubstructDataByName("info_header", "show_name", 1);
	else
		if (!trait_display)
			packet->setSubstructDataByName("info_header", "show_popup", 1);
		else
			packet->setSubstructDataByName("info_header", "show_popup", 0);
	
	if(packet_type > 0)
		packet->setSubstructDataByName("info_header", "packettype", packet_type*256 + 0xFE);
	else
		packet->setSubstructDataByName("info_header", "packettype", GetItemPacketType(version));
	//packet->setDataByName("unknown2",5);
	//packet->setDataByName("unknown7", 1);
	//packet->setDataByName("unknown9", 20);
	//packet->setDataByName("unknown10", 1, 2);
	if(sub_packet_type == 0)
		sub_packet_type = 0x83;
	packet->setSubstructDataByName("info_header", "packetsubtype", sub_packet_type);
	//packet->setDataByName("unknown3",2);
	//packet->setDataByName("unknown7", 50);
	if(sub_packet_type == 0x81)
		SetPacketInformation(packet, client);
	else
		SetPacketInformation(packet, client, true);
	packet->setSubstructDataByName("spell_info", "uses_remaining", 0xFFFF);
	packet->setSubstructDataByName("spell_info", "damage_remaining", 0xFFFF);
	//packet->PrintPacket();
	// This adds the second portion to the spell packet. Could be used for bonuses etc.?
	string* data1 = packet->serializeString();
	uchar*  data2 = (uchar*)data1->c_str();
	uchar*  ptr2 = data2;
	int32 size = data1->length() * 2;
	uchar* data3 = new uchar[size];
	memcpy(data3, data2, data1->length());
	uchar* ptr = data3;
	size -=17;
	memcpy(ptr, &size, sizeof(int32)); 
	size +=3;
	ptr += data1->length();
	ptr2 += 14;
	memcpy(ptr, ptr2, data1->length() - 14);

	EQ2Packet* outapp = new EQ2Packet(OP_ClientCmdMsg, data3, size);
	//DumpPacket(outapp);
	safe_delete_array(data3);
	safe_delete(packet);
	return outapp;
}

void Spell::AddSpellEffect(int8 percentage, int8 subbullet, string description){
	SpellDisplayEffect* effect = new SpellDisplayEffect;
	effect->description = description;
	effect->subbullet = subbullet;
	effect->percentage = percentage;
	MSpellInfo.lock();
	effects.push_back(effect);
	MSpellInfo.unlock();
}

int16 Spell::GetHPRequired(Spawn* spawn){
	int16 hp_req = spell->hp_req;
	if(spawn && spell->hp_req_percent > 0){
		double result = ((double)spell->hp_req_percent/100)*spawn->GetTotalHP();
		if(result >= (((int16)result) + .5))
			result++;
		hp_req = (int16)result;
	}
	return hp_req;
}

int16 Spell::GetPowerRequired(Spawn* spawn){
	int16 power_req = spell->power_req;
	if(spawn && spell->power_req_percent > 0){
		double result = ((double)spell->power_req_percent/100)*spawn->GetTotalPower();
		if(result >= (((int16)result) + .5))
			result++;
		power_req = (int16)result;
	}
	return power_req;
}

int16 Spell::GetSavageryRequired(Spawn* spawn){
	int16 savagery_req = spell->savagery_req;
	if(spawn && spell->savagery_req_percent > 0){
		double result = ((double)spell->savagery_req_percent/100)*spawn->GetTotalSavagery();
		if(result >= (((int16)result) + .5))
			result++;
		savagery_req = (int16)result;
	}
	return savagery_req;
}

int16 Spell::GetDissonanceRequired(Spawn* spawn){
	int16 dissonance_req = spell->dissonance_req;
	if(spawn && spell->dissonance_req_percent > 0){
		double result = ((double)spell->dissonance_req_percent/100)*spawn->GetTotalDissonance();
		if(result >= (((int16)result) + .5))
			result++;
		dissonance_req = (int16)result;
	}
	return dissonance_req;
}

int32 Spell::GetSpellDuration(){
	if(spell->duration1 == spell->duration2)
		return spell->duration1;

	int32 difference = 0;
	int32 lower = 0;
	if(spell->duration2 > spell->duration1){
		difference = spell->duration2 - spell->duration1;
		lower = spell->duration1;
	}
	else{
		difference = spell->duration1 - spell->duration2;
		lower = spell->duration2;
	}
	int32 duration = (rand()%difference) + lower;
	return duration;
}

const char* Spell::GetName(){
	return spell->name.data.c_str();
}

const char* Spell::GetDescription(){
	return spell->description.data.c_str();
}

void Spell::AddSpellLevel(int8 adventure_class, int8 tradeskill_class, int16 level){
	LevelArray* lvl = new LevelArray;
	lvl->adventure_class = adventure_class;
	lvl->tradeskill_class = tradeskill_class;
	lvl->spell_level = level;
	MSpellInfo.lock();
	levels.push_back(lvl);
	MSpellInfo.unlock();
}

int32 Spell::GetSpellID(){
	if (spell)
		return spell->id;
	return 0;
}

int8 Spell::GetSpellTier(){
	if (spell)
		return spell->tier;
	return 0;
}

vector<LUAData*>* Spell::GetLUAData(){
	return &lua_data;
}
SpellData* Spell::GetSpellData(){
	return spell;
}

int16 Spell::GetSpellIcon(){
	if (spell)
		return spell->icon;
	return 0;
}

int16 Spell::GetSpellIconBackdrop(){
	if (spell)
		return spell->icon_backdrop;
	return 0;
}

int16 Spell::GetSpellIconHeroicOp(){
	if (spell)
		return spell->icon_heroic_op;
	return 0;
}

bool Spell::IsHealSpell(){
	return heal_spell;
}

bool Spell::IsBuffSpell(){
	return buff_spell;
}

bool Spell::IsDamageSpell(){
	return damage_spell;
}
bool Spell::IsControlSpell(){
	return control_spell;
}

bool Spell::IsOffenseSpell(){
	return offense_spell;
}

int16 Spell::GetModifiedCastTime(Entity* caster){
	int16 cast_time = spell->cast_time;
	float cast_speed = caster->GetInfoStruct()->casting_speed;

	if (cast_time > 0) {
		if (cast_speed > 0) { // casting speed can only reduce up to half a cast time
			return cast_time * max((float) 0.5, (float)(1 / (1 + (cast_speed * .01))));
		} else if (cast_speed < 0) { // not sure if casting speed debuff is capped on live or not, capping at 1.5 * the normal rate for now
			return cast_time * min((float) 1.5, (float)(1 + (1 - (1 / (1 + (cast_speed * -.01))))));
		}
	}

	return cast_time;
}

float Spell::GetModifiedRecast(Entity* caster) {
	float recast = spell->recast;
	float reuse_speed = caster->GetInfoStruct()->reuse_speed;

	if (recast > 0) {
		if (reuse_speed > 0) { // casting speed can only reduce up to half a cast time
			return recast * max((float) 0.5, (float)(1 / (1 + (reuse_speed * .01))));
		} else if (reuse_speed < 0) { // not sure if casting speed debuff is capped on live or not, capping at 1.5 * the normal rate for now
			return recast * min((float) 1.5, (float)(1 + (1 - (1 / (1 + (reuse_speed * -.01))))));
		}
	}

	return recast;
}

vector <SpellDisplayEffect*>* Spell::GetSpellEffects(){
	MSpellInfo.lock();
	vector <SpellDisplayEffect*>* ret = &effects;
	MSpellInfo.unlock();
	return ret;
}

vector <LevelArray*>* Spell::GetSpellLevels(){
	MSpellInfo.lock();
	vector <LevelArray*>* ret = &levels;
	MSpellInfo.unlock();
	return ret;
}

bool Spell::ScribeAllowed(Player* player){
	bool ret = false;
	if(player){
		MSpellInfo.lock();
		for(int32 i=0;!ret && i<levels.size();i++){
			if((player->GetAdventureClass() == levels[i]->adventure_class || player->GetTradeskillClass() == levels[i]->tradeskill_class) && player->GetLevel() >= levels[i]->spell_level)
				ret = true;
		}
		MSpellInfo.unlock();
	}
	return ret;
}

MasterSpellList::MasterSpellList(){
	MMasterSpellList.SetName("MasterSpellList::MMasterSpellList");
}

MasterSpellList::~MasterSpellList(){
	DestroySpells();
}
void MasterSpellList::DestroySpells(){

	spell_errors.clear();

	MMasterSpellList.lock();
	map<int32, map<int32, Spell*> >::iterator iter;
	map<int32, Spell*>::iterator iter2;
	for(iter = spell_list.begin();iter != spell_list.end(); iter++){
		for(iter2 = iter->second.begin();iter2 != iter->second.end(); iter2++){
			safe_delete(iter2->second);
		}
	}
	spell_list.clear();
	MMasterSpellList.unlock();
}
void MasterSpellList::AddSpell(int32 id, int8 tier, Spell* spell){
	MMasterSpellList.lock();
	spell_list[id][tier] = spell;
	spell_name_map[spell->GetName()] = spell;
	spell_soecrc_map[spell->GetSpellData()->soe_spell_crc] = spell;
	MMasterSpellList.unlock();
}

Spell* MasterSpellList::GetSpell(int32 id, int8 tier){
	if (spell_list.count(id) > 0 && spell_list[id].count(tier) > 0)
		return spell_list[id][tier];
	return 0;
}

Spell* MasterSpellList::GetSpellByName(const char* name){
	if(spell_name_map.count(name) > 0)
		return spell_name_map[name];
	return 0;
}

Spell* MasterSpellList::GetSpellByCRC(int32 spell_crc){
	if(spell_soecrc_map.count(spell_crc) > 0)
		return spell_soecrc_map[spell_crc];
	return 0;
}

EQ2Packet* MasterSpellList::GetSpellPacket(int32 id, int8 tier, Client* client, bool display, int8 packet_type){
	Spell* spell = GetSpell(id, tier);
	if(spell)
		return spell->SerializeSpell(client, display, packet_type);
	return 0;
}

EQ2Packet* MasterSpellList::GetSpecialSpellPacket(int32 id, int8 tier, Client* client, bool display, int8 packet_type){
	Spell* spell = GetSpell(id, tier);
	if(spell)
		return spell->SerializeSpecialSpell(client, display, packet_type, 0x81);
	return 0;
}

vector<Spell*>* MasterSpellList::GetSpellListByAdventureClass(int8 class_id, int16 max_level, int8 max_tier){
	vector<Spell*>* ret = new vector<Spell*>;
	Spell* spell = 0;
	vector<LevelArray*>* levels = 0;
	LevelArray* level = 0;
	vector<LevelArray*>::iterator level_itr;
	MMasterSpellList.lock();
	map<int32, map<int32, Spell*> >::iterator iter;
	map<int32, Spell*>::iterator iter2;
	max_level *= 10; //convert to client level format, which is 10 times higher
	for(iter = spell_list.begin();iter != spell_list.end(); iter++){
		for(iter2 = iter->second.begin();iter2 != iter->second.end(); iter2++){
			spell = iter2->second;
			if(iter2->first <= max_tier && spell){
				levels = spell->GetSpellLevels();
				for(level_itr = levels->begin(); level_itr != levels->end(); level_itr++){
					level = *level_itr;
					if(level->spell_level <= max_level && level->adventure_class == class_id){
						ret->push_back(spell);
						break;
					}
				}
			}
		}
	}
	MMasterSpellList.unlock();
	return ret;
}

vector<Spell*>* MasterSpellList::GetSpellListByTradeskillClass(int8 class_id, int16 max_level, int8 max_tier){
	vector<Spell*>* ret = new vector<Spell*>;
	Spell* spell = 0;
	vector<LevelArray*>* levels = 0;
	LevelArray* level = 0;
	vector<LevelArray*>::iterator level_itr;
	MMasterSpellList.lock();
	map<int32, map<int32, Spell*> >::iterator iter;
	map<int32, Spell*>::iterator iter2;
	for(iter = spell_list.begin();iter != spell_list.end(); iter++){
		for(iter2 = iter->second.begin();iter2 != iter->second.end(); iter2++){
			spell = iter2->second;
			if(iter2->first <= max_tier && spell){
				levels = spell->GetSpellLevels();
				for(level_itr = levels->begin(); level_itr != levels->end(); level_itr++){
					level = *level_itr;
					if(level->spell_level <= max_level && level->tradeskill_class == class_id){
						ret->push_back(spell);
						break;
					}
				}
			}
		}
	}
	MMasterSpellList.unlock();
	return ret;
}

void MasterSpellList::Reload(){
	master_trait_list.DestroyTraits();
	DestroySpells();
	database.LoadSpells();
	database.LoadSpellErrors();
	database.LoadTraits();
}

int16 MasterSpellList::GetSpellErrorValue(int16 version, int8 error_index) {
	version = GetClosestVersion(version);

	if (spell_errors[version].count(error_index) == 0) {
		LogWrite(SPELL__ERROR, 0, "Spells", "No spell error entry. (version = %i, error_index = %i)", version, error_index);
		// 1 will give the client a pop up message of "Cannot cast" and a chat message of "[BUG] Cannot cast. Unknown failure casting spell."
		return 1;
	}
	return spell_errors[version][error_index];
}

void MasterSpellList::AddSpellError(int16 version, int8 error_index, int16 error_value) {
	if (spell_errors[version].count(error_index) == 0)
		spell_errors[version][error_index] = error_value;
}

int16 MasterSpellList::GetClosestVersion(int16 version) {
	int16 ret = 0;
	map<int16, map<int8, int16> >::iterator itr;
	// Get the closest version in the list that is less then or equal to the given version
	for (itr = spell_errors.begin(); itr != spell_errors.end(); itr++) {
		if (itr->first <= version) {
			if (itr->first > ret)
				ret = itr->first;
		}
	}

	return ret;
}

bool Spell::CastWhileStunned(){
	return (spell->casting_flags & CASTING_FLAG_STUNNED) == CASTING_FLAG_STUNNED;
}

bool Spell::CastWhileMezzed(){
	return (spell->casting_flags & CASTING_FLAG_MEZZED) == CASTING_FLAG_MEZZED;
}

bool Spell::CastWhileStifled(){
	return (spell->casting_flags & CASTING_FLAG_STIFLED) == CASTING_FLAG_STIFLED;
}

bool Spell::CastWhileFeared(){
	return (spell->casting_flags & CASTING_FLAG_FEARED) == CASTING_FLAG_FEARED;
}
