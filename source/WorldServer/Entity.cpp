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
#include "Entity.h"
#include <math.h>
#include "Items/Items.h"
#include "zoneserver.h"
#include "World.h"
#include "../common/Log.h"
#include "Rules/Rules.h"
#include "Spells.h"
#include "SpellProcess.h"
#include "classes.h"
#include "LuaInterface.h"
#include "ClientPacketFunctions.h"
#include "PVP.h"

extern World world;
extern MasterItemList master_item_list;
extern MasterSpellList master_spell_list;
extern Classes classes;

Entity::Entity(){
	max_speed = 6;
	last_x = -1;
	last_y = -1;
	last_z = -1;
	last_heading = -1;
	regen_hp_rate = 0;
	regen_power_rate = 0;
	in_combat = false;
	casting = false;
	memset(&melee_combat_data, 0, sizeof(CombatData));
	memset(&ranged_combat_data, 0, sizeof(CombatData));
	memset(&info_struct, 0, sizeof(InfoStruct));
	loot_coins = 0;
	memset(&features, 0, sizeof(CharFeatures));
	memset(&equipment, 0, sizeof(EQ2_Equipment));
	pet = 0;
	charmedPet = 0;
	deityPet = 0;
	cosmeticPet = 0;
	speed = 0;
	speed_multiplier = 1.0f;
	m_threatTransfer = 0;
	group_member_info = 0;
	MProcList.SetName("Entity::m_procList");
	MDetriments.SetName("Entity::MDetriments");
	MMaintainedSpells.SetName("Entity::MMaintainedSpells");
	MSpellEffects.SetName("Entity::MSpellEffects");
	m_procList.clear();
	control_effects.clear();
	immunities.clear();

	for(int i=0;i<NUM_SPELL_EFFECTS;i++){
		info_struct.spell_effects[i].spell_id = 0xFFFFFFFF;	
	}
	for(int i=0;i<NUM_MAINTAINED_EFFECTS;i++) {
		info_struct.maintained_effects[i].spell_id = 0xFFFFFFFF;
		if (IsPlayer())
			info_struct.maintained_effects[i].icon = 0xFFFF;
	}
}

Entity::~Entity(){
	vector<Item*>::iterator itr;
	for(itr = loot_items.begin(); itr != loot_items.end(); itr++)
		safe_delete(*itr);
	MutexList<BonusValues*>::iterator itr2 = bonus_list.begin();
	while(itr2.Next())
		safe_delete(itr2.value);
	ClearProcs();
	safe_delete(m_threatTransfer);
	map<int8, MutexList<LuaSpell*>*>::iterator itr3;
	for (itr3 = control_effects.begin(); itr3 != control_effects.end(); itr3++)
		safe_delete(itr3->second);
	control_effects.clear();
	map<int8, MutexList<LuaSpell*>*>::iterator itr4;
	for (itr4 = immunities.begin(); itr4 != immunities.end(); itr4++)
		safe_delete(itr4->second);
	immunities.clear();
}

bool Entity::HasMoved(bool include_heading){
	if(GetX() == last_x && GetY() == last_y && GetZ() == last_z && ((!include_heading) || (include_heading && GetHeading() == last_heading)))
		return false;
	bool ret_val = true;
	if(last_x == -1 && last_y == -1 && last_z == -1 && last_heading == -1){
		ret_val = false;
	}
	last_x = GetX();
	last_y = GetY();
	last_z = GetZ();
	last_heading = GetHeading();
	return ret_val;
}

int16 Entity::GetStr(){
	return GetInfoStruct()->str;
}

int16 Entity::GetSta(){
	return GetInfoStruct()->sta;
}

int16 Entity::GetInt(){
	return GetInfoStruct()->intel;
}

int16 Entity::GetWis(){
	return GetInfoStruct()->wis;
}

int16 Entity::GetAgi(){
	return GetInfoStruct()->agi;
}

int16 Entity::GetPrimaryStat(){
	int8 base_class = classes.GetBaseClass(GetAdventureClass());
	if (base_class == FIGHTER) 
		return GetInfoStruct()->str;	
	else if (base_class == PRIEST) 
		return GetInfoStruct()->wis;
	else if (base_class == MAGE) 
		return GetInfoStruct()->intel;
	else
		return GetInfoStruct()->agi;
}

int16 Entity::GetHeatResistance(){
	return GetInfoStruct()->heat;
}

int16 Entity::GetColdResistance(){
	return GetInfoStruct()->cold;
}

int16 Entity::GetMagicResistance(){
	return GetInfoStruct()->magic;
}

int16 Entity::GetMentalResistance(){
	return GetInfoStruct()->mental;
}

int16 Entity::GetDivineResistance(){
	return GetInfoStruct()->divine;
}

int16 Entity::GetDiseaseResistance(){
	return GetInfoStruct()->disease;
}

int16 Entity::GetPoisonResistance(){
	return GetInfoStruct()->poison;
}

int8 Entity::GetConcentrationCurrent() {
	return GetInfoStruct()->cur_concentration;
}

int8 Entity::GetConcentrationMax() {
	return GetInfoStruct()->max_concentration;
}

int16 Entity::GetStrBase(){
	return GetInfoStruct()->str_base;
}

int16 Entity::GetStaBase(){
	return GetInfoStruct()->sta_base;
}

int16 Entity::GetIntBase(){
	return GetInfoStruct()->intel_base;
}

int16 Entity::GetWisBase(){
	return GetInfoStruct()->wis_base;
}

int16 Entity::GetAgiBase(){
	return GetInfoStruct()->agi_base;
}

int16 Entity::GetHeatResistanceBase(){
	return GetInfoStruct()->heat_base;
}

int16 Entity::GetColdResistanceBase(){
	return GetInfoStruct()->cold_base;
}

int16 Entity::GetMagicResistanceBase(){
	return GetInfoStruct()->magic_base;
}

int16 Entity::GetMentalResistanceBase(){
	return GetInfoStruct()->mental_base;
}

int16 Entity::GetDivineResistanceBase(){
	return GetInfoStruct()->divine_base;
}

int16 Entity::GetDiseaseResistanceBase(){
	return GetInfoStruct()->disease_base;
}

int16 Entity::GetPoisonResistanceBase(){
	return GetInfoStruct()->poison_base;
}

void Entity::SetAlignment(sint8 new_value) {
	GetInfoStruct()->alignment = new_value;
}

sint8 Entity::GetAlignment(){
	return GetInfoStruct()->alignment;
}

bool Entity::IsCasting(){
	return casting;
}

void Entity::IsCasting(bool val){
	casting = val;
}

int32 Entity::GetRangeLastAttackTime(){
	return ranged_combat_data.range_last_attack_time;
}

void Entity::SetRangeLastAttackTime(int32 time){
	ranged_combat_data.range_last_attack_time = time;
}

int16 Entity::GetRangeAttackDelay(){
	return ranged_combat_data.ranged_attack_delay;
//	if(IsPlayer()){
//		Item* item = ((Player*)this)->GetEquipmentList()->GetItem(EQ2_RANGE_SLOT);
//		if(item && item->IsRanged())
//			return item->ranged_info->weapon_info.delay*100;
//	}
//	return 3000;
}

int32 Entity::GetPrimaryLastAttackTime(){
	return melee_combat_data.primary_last_attack_time;
}

int16 Entity::GetPrimaryAttackDelay(){
	return melee_combat_data.primary_attack_delay;
}

void Entity::SetPrimaryAttackDelay(int16 new_delay){
	melee_combat_data.primary_attack_delay = new_delay;
}

void Entity::SetPrimaryLastAttackTime(int32 new_time){
	melee_combat_data.primary_last_attack_time = new_time;
}

int32 Entity::GetSecondaryLastAttackTime(){
	return melee_combat_data.secondary_last_attack_time;
}

int16 Entity::GetSecondaryAttackDelay(){
	return melee_combat_data.secondary_attack_delay;
}

void Entity::SetSecondaryAttackDelay(int16 new_delay){
	melee_combat_data.secondary_attack_delay = new_delay;
}

void Entity::SetSecondaryLastAttackTime(int32 new_time){
	melee_combat_data.secondary_last_attack_time = new_time;
}

void Entity::ChangePrimaryWeapon(){
	Item* item = equipment_list.GetItem(EQ2_PRIMARY_SLOT);
	if(item && item->details.item_id > 0 && item->IsWeapon()){
		melee_combat_data.primary_weapon_delay = item->weapon_info->delay * 100;
		melee_combat_data.primary_weapon_damage_low = item->weapon_info->damage_low3;
		melee_combat_data.primary_weapon_damage_high = item->weapon_info->damage_high3;
		melee_combat_data.primary_weapon_type = item->GetWeaponType();
		melee_combat_data.wield_type = item->weapon_info->wield_type;
	}
	else{
		melee_combat_data.primary_weapon_delay = 1500;
		melee_combat_data.primary_weapon_damage_high = (int32)(5 + GetLevel() * (GetLevel()/2.5));
		melee_combat_data.primary_weapon_damage_low = (int32)(melee_combat_data.primary_weapon_damage_high * 0.5);

		if(IsNPC())
			melee_combat_data.primary_weapon_type = ((NPC*)this)->GetAttackType();
		else
			melee_combat_data.primary_weapon_type = 1;
		melee_combat_data.wield_type = 2;
	}
	if(IsNPC())
		melee_combat_data.primary_weapon_damage_high += (int32)(GetInfoStruct()->str / 10);
	else
		melee_combat_data.primary_weapon_damage_high += (int32)(GetInfoStruct()->str / 25);
}

void Entity::ChangeSecondaryWeapon(){
	Item* item = equipment_list.GetItem(EQ2_SECONDARY_SLOT);
	if(item && item->details.item_id > 0 && item->IsWeapon()){
		melee_combat_data.secondary_weapon_delay = item->weapon_info->delay * 100;
		melee_combat_data.secondary_weapon_damage_low = item->weapon_info->damage_low3;
		melee_combat_data.secondary_weapon_damage_high = item->weapon_info->damage_high3;
		melee_combat_data.secondary_weapon_type = item->GetWeaponType();
	}
	else{
		melee_combat_data.secondary_weapon_delay = 1500;
		melee_combat_data.secondary_weapon_damage_high = (int32)(5 + GetLevel() * (GetLevel()/3));
		melee_combat_data.secondary_weapon_damage_low = (int32)(melee_combat_data.secondary_weapon_damage_high * 0.5);
		melee_combat_data.secondary_weapon_type = 1;
	}
	if(IsNPC())
		melee_combat_data.secondary_weapon_damage_high += (int32)(GetInfoStruct()->str / 10);
	else
		melee_combat_data.secondary_weapon_damage_high += (int32)(GetInfoStruct()->str / 25);
}

void Entity::ChangeRangedWeapon(){
	Item* item = equipment_list.GetItem(EQ2_RANGE_SLOT);
	if(item && item->details.item_id > 0 && item->IsRanged()){
		ranged_combat_data.ranged_weapon_delay = item->ranged_info->weapon_info.delay*100;
		ranged_combat_data.ranged_weapon_damage_low = item->ranged_info->weapon_info.damage_low3;
		ranged_combat_data.ranged_weapon_damage_high = item->ranged_info->weapon_info.damage_high3;
		ranged_combat_data.ranged_weapon_type = item->GetWeaponType();
	}
}

int32 Entity::GetPrimaryWeaponMinDamage(){
	return melee_combat_data.primary_weapon_damage_low;
}

int32 Entity::GetPrimaryWeaponMaxDamage(){
	return melee_combat_data.primary_weapon_damage_high;
}

int32 Entity::GetSecondaryWeaponMinDamage(){
	return melee_combat_data.secondary_weapon_damage_low;
}

int32 Entity::GetSecondaryWeaponMaxDamage(){
	return melee_combat_data.secondary_weapon_damage_high;
}

int8 Entity::GetPrimaryWeaponType(){
	return melee_combat_data.primary_weapon_type;
}

int8 Entity::GetSecondaryWeaponType(){
	return melee_combat_data.secondary_weapon_type;
}

int32 Entity::GetRangedWeaponMinDamage(){
	return ranged_combat_data.ranged_weapon_damage_low;
}

int32 Entity::GetRangedWeaponMaxDamage(){
	return ranged_combat_data.ranged_weapon_damage_high;
}

int8 Entity::GetRangedWeaponType(){
	return ranged_combat_data.ranged_weapon_type;
}

bool Entity::IsDualWield() {
	if (melee_combat_data.wield_type == 1 || melee_combat_data.wield_type == 2)
		return true;
	return false;
}

int8 Entity::GetWieldType(){
	return melee_combat_data.wield_type;
}

bool Entity::BehindTarget(Spawn* target){
	float target_angle = 360 - target->GetHeading();	
	float angle = 360 - GetHeading();
	if(target_angle > angle)
		angle = target_angle - angle;
	else
		angle -= target_angle;
	return (angle < 90 || angle > 270);
}

bool Entity::FlankingTarget(Spawn* target) {
	float angle;
	double diff_x = target->GetX() - GetX();
	double diff_z = target->GetZ() - GetZ();
	if (diff_z == 0) {
	   if (diff_x > 0)
		   angle = 90;
	   else
		   angle = 270;
	}
	else
		angle = ((atan(diff_x / diff_z)) * 180) / 3.14159265358979323846;
	if (angle < 0)
		angle = angle + 360;
	else
		angle = angle + 180;
	if (diff_x < 0)
		angle = angle + 180;
	
	if (angle > GetHeading())
		angle = angle - GetHeading();
	else
		angle = GetHeading() - angle;

	if (angle > 360)
		angle -= 360;

	//LogWrite(SPAWN__ERROR, 0, "Angle", "spawn heading = %f", GetHeading());
	//LogWrite(SPAWN__ERROR, 0, "Angle", "angle = %f", angle);

	return (angle >= 45 && angle <= 315);
}

float Entity::GetShieldBlockChance(){
	float ret = 0;
	Item* item = equipment_list.GetItem(1);
	if(item && item->details.item_id > 0 && item->IsShield()){
	
	}
	return ret;
}

float Entity::GetDodgeChance(){
	float ret = 0;
	
	return ret;
}

bool Entity::EngagedInCombat(){
	return in_combat;
}

void Entity::InCombat(bool val){
	in_combat = val;
}

void Entity::SetHPRegen(int16 new_val){
	regen_hp_rate = new_val;
}
void Entity::SetPowerRegen(int16 new_val){
	regen_power_rate = new_val;
}
int16 Entity::GetHPRegen(){
	return regen_hp_rate;
}
int16 Entity::GetPowerRegen(){
	return regen_power_rate;
}

void Entity::DoRegenUpdate(){
	if(GetHP() == 0)//dead
		return;
	sint32 hp = GetHP();
	sint32 power = GetPower();
	int16 level = GetLevel();

	// No regen for NPC's while in combat
	// Temp solution for now
	if (IsNPC() && EngagedInCombat())
		return;

	if(hp < GetTotalHP()){
		if(regen_hp_rate == 0)
			regen_hp_rate = (int)(level*.75)+(int)(level/10) + 1;
		int16 temp = regen_hp_rate + stats[ITEM_STAT_HPREGEN];
		if((hp + temp) > GetTotalHP())
			SetHP(GetTotalHP());
		else
			SetHP(hp + temp);

		LogWrite(MISC__TODO, 1, "TODO", "Fix this later for mobs\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);

	}
	if(GetPower() < GetTotalPower()){
		if(regen_power_rate == 0)
			regen_power_rate = level + (int)(level/10) + 1;
		if((power + regen_power_rate) > GetTotalPower())
			SetPower(GetTotalPower());
		else
			SetPower(power + regen_power_rate);

		LogWrite(MISC__TODO, 1, "TODO", "Fix this later for mobs\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);

	}
}

void Entity::AddMaintainedSpell(LuaSpell* luaspell){
	if (!luaspell)
		return;

	Spell* spell = luaspell->spell;
	MaintainedEffects* effect = GetFreeMaintainedSpellSlot();

	if (effect){
		MMaintainedSpells.writelock(__FUNCTION__, __LINE__);
		effect->spell = luaspell;
		effect->spell_id = spell->GetSpellData()->id;
		LogWrite(NPC__SPELLS, 5, "NPC", "AddMaintainedSpell Spell ID: %u", spell->GetSpellData()->id);
		effect->conc_used = spell->GetSpellData()->req_concentration / 256;
		effect->total_time = spell->GetSpellDuration() / 10;
		effect->tier = spell->GetSpellData()->tier;
		if (spell->GetSpellData()->duration_until_cancel)
			effect->expire_timestamp = 0xFFFFFFFF;
		else
			effect->expire_timestamp = Timer::GetCurrentTime2() + (spell->GetSpellDuration() * 100);
		MMaintainedSpells.releasewritelock(__FUNCTION__, __LINE__);
	}
}

void Entity::AddSpellEffect(LuaSpell* luaspell){
	if (!luaspell || !luaspell->caster)
		return;

	Spell* spell = luaspell->spell;
	SpellEffects* old_effect = GetSpellEffect(spell->GetSpellID(), luaspell->caster);
	SpellEffects* effect = 0;
	if (old_effect){
		GetZone()->RemoveTargetFromSpell(old_effect->spell, this);
		RemoveSpellEffect(old_effect->spell);
	}
	effect = GetFreeSpellEffectSlot();

	if(effect){
		MSpellEffects.writelock(__FUNCTION__, __LINE__);
		effect->spell = luaspell;
		effect->spell_id = spell->GetSpellData()->id;
		effect->caster = luaspell->caster;
		effect->total_time = spell->GetSpellDuration()/10;
		if (spell->GetSpellData()->duration_until_cancel)
			effect->expire_timestamp = 0xFFFFFFFF;
		else
			effect->expire_timestamp = Timer::GetCurrentTime2() + (spell->GetSpellDuration()*100);
		effect->icon = spell->GetSpellData()->icon;
		effect->icon_backdrop = spell->GetSpellData()->icon_backdrop;
		effect->tier = spell->GetSpellTier();
		MSpellEffects.releasewritelock(__FUNCTION__, __LINE__);
		changed = true;
		info_changed = true;
		AddChangedZoneSpawn();
	}
}

void Entity::RemoveMaintainedSpell(LuaSpell* luaspell){
	if (!luaspell)
		return;

	bool found = false;
	MMaintainedSpells.writelock(__FUNCTION__, __LINE__);
	for (int i = 0; i<30; i++){
		// If we already found the spell then we are bumping all other up one so there are no gaps
		// This check needs to be first so found can never be true on the first iteration (i = 0)
		if (found) {
			GetInfoStruct()->maintained_effects[i].slot_pos = i - 1;
			GetInfoStruct()->maintained_effects[i - 1] = GetInfoStruct()->maintained_effects[i];

		}
		// Compare spells, if we found a match set the found flag
		if (GetInfoStruct()->maintained_effects[i].spell == luaspell)
			found = true;

	}
	// if we found the spell in the array then we need to set the last element to empty
	if (found) {
		memset(&GetInfoStruct()->maintained_effects[29], 0, sizeof(MaintainedEffects));
		GetInfoStruct()->maintained_effects[29].spell_id = 0xFFFFFFFF;
		GetInfoStruct()->maintained_effects[29].icon = 0xFFFF;
	}
	MMaintainedSpells.releasewritelock(__FUNCTION__, __LINE__);
}

void Entity::RemoveSpellEffect(LuaSpell* spell) {
	bool found = false;
	MSpellEffects.writelock(__FUNCTION__, __LINE__);
	for(int i=0;i<NUM_SPELL_EFFECTS;i++) {
		if (found) {
			GetInfoStruct()->spell_effects[i-1] = GetInfoStruct()->spell_effects[i];
		}
		if (GetInfoStruct()->spell_effects[i].spell == spell)
			found = true;
	}
	if (found) {
		memset(&GetInfoStruct()->spell_effects[NUM_SPELL_EFFECTS-1], 0, sizeof(SpellEffects));
		GetInfoStruct()->spell_effects[NUM_SPELL_EFFECTS-1].spell_id = 0xFFFFFFFF;
		changed = true;
		info_changed = true;
		AddChangedZoneSpawn();
	}
	MSpellEffects.releasewritelock(__FUNCTION__, __LINE__);
}

bool Entity::HasActiveMaintainedSpell(Spell* spell, Spawn* target){
	LogWrite(MISC__TODO, 1, "TODO", "This does nothing... yet...\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
	return false;
}

bool Entity::HasActiveSpellEffect(Spell* spell, Spawn* target){
	LogWrite(MISC__TODO, 1, "TODO", "This does nothing... yet...\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
	return false;
}

MaintainedEffects* Entity::GetFreeMaintainedSpellSlot(){
	MaintainedEffects* ret = 0;
	InfoStruct* info = GetInfoStruct();
	MMaintainedSpells.readlock(__FUNCTION__, __LINE__);
	for (int i = 0; i<NUM_MAINTAINED_EFFECTS; i++){
		if (info->maintained_effects[i].spell_id == 0xFFFFFFFF){
			ret = &info->maintained_effects[i];
			ret->spell_id = 0;
			ret->slot_pos = i;
			break;
		}
	}
	MMaintainedSpells.releasereadlock(__FUNCTION__, __LINE__);
	return ret;
}

MaintainedEffects* Entity::GetMaintainedSpell(int32 spell_id){
	MaintainedEffects* ret = 0;
	InfoStruct* info = GetInfoStruct();
	MMaintainedSpells.readlock(__FUNCTION__, __LINE__);
	for (int i = 0; i<NUM_MAINTAINED_EFFECTS; i++){
		if (info->maintained_effects[i].spell_id == spell_id){
			ret = &info->maintained_effects[i];
			break;
		}
	}
	MMaintainedSpells.releasereadlock(__FUNCTION__, __LINE__);
	return ret;
}

SpellEffects* Entity::GetFreeSpellEffectSlot(){
	SpellEffects* ret = 0;
	InfoStruct* info = GetInfoStruct();
	MSpellEffects.readlock(__FUNCTION__, __LINE__);
	for(int i=0;i<NUM_SPELL_EFFECTS;i++){
		if(info->spell_effects[i].spell_id == 0xFFFFFFFF){
			ret = &info->spell_effects[i];
			ret->spell_id = 0;
			break;
		}
	}
	MSpellEffects.releasereadlock(__FUNCTION__, __LINE__);
	return ret;
}

SpellEffects* Entity::GetSpellEffect(int32 id, Entity* caster) {
	SpellEffects* ret = 0;
	InfoStruct* info = GetInfoStruct();
	MSpellEffects.readlock(__FUNCTION__, __LINE__);
	for(int i = 0; i < NUM_SPELL_EFFECTS; i++) {
		if(info->spell_effects[i].spell_id == id) {
			if (!caster || info->spell_effects[i].caster == caster){
				ret = &info->spell_effects[i];
				break;
			}
		}
	}
	MSpellEffects.releasereadlock(__FUNCTION__, __LINE__);
	return ret;
}

InfoStruct* Entity::GetInfoStruct(){ 
	return &info_struct; 
}

Item* Entity::LootItem(int32 id){
	Item* ret = 0;
	vector<Item*>::iterator itr;
	MLootItems.lock();
	for(itr = loot_items.begin(); itr != loot_items.end(); itr++){
		if((*itr)->details.item_id == id){
			ret = *itr;
			loot_items.erase(itr);
			break;
		}
	}
	MLootItems.unlock();
	return ret;
}

int32 Entity::GetLootItemID(){
	int32 ret = 0;
	vector<Item*>::iterator itr;
	MLootItems.lock();
	for(itr = loot_items.begin(); itr != loot_items.end(); itr++){
		ret = (*itr)->details.item_id;
		break;
	}
	MLootItems.unlock();
	return ret;
}

Skill* Entity::GetSkillByName(const char* name, bool check_update){
	LogWrite(MISC__TODO, 1, "TODO", "This does nothing... yet...\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}

float Entity::GetMaxSpeed(){
	return max_speed;
}

void Entity::SetMaxSpeed(float val){
	max_speed = val;
}

void Entity::CalculateBonuses(){
	InfoStruct* info = &info_struct;
	info->block = info->block_base;
	
	info->cur_attack = info->attack_base;
	info->cur_mitigation = info->mitigation_base;
	info->base_avoidance_pct = info->avoidance_base;

	LogWrite(MISC__TODO, 1, "TODO", "Calculate via current spells\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);

	//info->cur_concentration = 0;
	info->parry = info->parry_base;
	info->deflection = info->deflection_base;

	info->disease = info->disease_base;
	info->divine = info->divine_base;
	info->heat = info->heat_base;
	info->magic = info->magic_base;
	info->mental = info->mental_base;
	info->cold = info->cold_base;
	info->poison = info->poison_base;
	info->elemental_base = info->heat;
	info->noxious_base = info->poison;
	info->arcane_base = info->magic;

	info->sta = info->sta_base;
	info->agi = info->agi_base;
	info->str = info->str_base;
	info->wis = info->wis_base;
	info->intel = info->intel_base;
	info->ability_modifier = 0;
	info->critical_mitigation = 0;
	info->block_chance = 0;
	info->crit_chance = 0;
	info->crit_bonus = 0;
	info->potency = 0;
	info->hate_mod = 0;
	info->reuse_speed = 0;
	info->casting_speed = 0;
	info->recovery_speed = 0;
	info->spell_reuse_speed = 0;
	info->spell_multi_attack = 0;
	info->dps = 0;
	info->dps_multiplier = 0;
	info->haste = 0;
	info->attackspeed = 0;
	info->multi_attack = 0;
	info->flurry = 0;
	info->melee_ae = 0;
	info->strikethrough = 0;
	info->accuracy = 0;
	info->offensivespeed = 0;

	stats.clear();
	ItemStatsValues* values = equipment_list.CalculateEquipmentBonuses(this);
	CalculateSpellBonuses(values);
	info->sta += values->sta;
	info->str += values->str;
	info->agi += values->agi;
	info->wis += values->wis;
	info->intel += values->int_;

	info->disease += values->vs_disease;
	info->divine += values->vs_divine;
	info->heat += values->vs_heat;
	info->magic += values->vs_magic;
	info->cur_mitigation += values->vs_slash;
	info->cur_mitigation += values->vs_pierce;
	info->cur_mitigation += values->vs_crush;
	int32 sta_hp_bonus = 0.0;
	int32 prim_power_bonus = 0.0;
	float bonus_mod = 0.0;
	if (IsPlayer()) {
		bonus_mod = CalculateBonusMod(); 
		sta_hp_bonus = info->sta * bonus_mod;
		prim_power_bonus = GetPrimaryStat() * bonus_mod;
	}
	prim_power_bonus = floor(float(prim_power_bonus));
	sta_hp_bonus = floor(float(sta_hp_bonus));
	SetTotalHP(GetTotalHPBase() + values->health + sta_hp_bonus);
	SetTotalPower(GetTotalPowerBase() + values->power + prim_power_bonus);
	if(GetHP() > GetTotalHP())
		SetHP(GetTotalHP());
	if(GetPower() > GetTotalPower())
		SetPower(GetTotalPower());
	info->mental += values->vs_mental;
	info->poison += values->vs_poison;
	info->max_concentration += values->concentration;
	info->cold += values->vs_cold;
	info->mitigation_skill1 += values->vs_slash;
	info->mitigation_skill2 += values->vs_pierce;
	info->mitigation_skill3 += values->vs_crush;
	info->ability_modifier += values->ability_modifier;
	info->critical_mitigation += values->criticalmitigation;
	info->block_chance += values->extrashieldblockchance;
	info->crit_chance += values->beneficialcritchance;
	info->crit_bonus += values->critbonus;
	info->potency += values->potency;
	info->hate_mod += values->hategainmod;
	info->reuse_speed += values->abilityreusespeed;
	info->casting_speed += values->abilitycastingspeed;
	info->recovery_speed += values->abilityrecoveryspeed;
	info->spell_reuse_speed += values->spellreusespeed;
	info->spell_multi_attack += values->spellmultiattackchance;
	info->dps += values->dps;
	info->dps_multiplier = CalculateDPSMultiplier();
	info->haste += values->attackspeed;
	info->multi_attack += values->multiattackchance;
	info->flurry += values->flurry;
	info->melee_ae += values->aeautoattackchance;
	info->strikethrough += values->strikethrough;
	info->accuracy += values->accuracy;
	info->offensivespeed += values->offensivespeed;
	safe_delete(values);
}

EquipmentItemList* Entity::GetEquipmentList(){
	return &equipment_list;
}

void Entity::SetEquipment(Item* item, int8 slot){
	if(!item && slot < NUM_SLOTS){
		SetInfo(&equipment.equip_id[slot], 0);
		SetInfo(&equipment.color[slot].red, 0);
		SetInfo(&equipment.color[slot].green, 0);
		SetInfo(&equipment.color[slot].blue, 0);
		SetInfo(&equipment.highlight[slot].red, 0);
		SetInfo(&equipment.highlight[slot].green, 0);
		SetInfo(&equipment.highlight[slot].blue, 0);
	}
	else{
		SetInfo(&equipment.equip_id[item->details.slot_id], item->generic_info.appearance_id);
		SetInfo(&equipment.color[item->details.slot_id].red, item->generic_info.appearance_red);
		SetInfo(&equipment.color[item->details.slot_id].green, item->generic_info.appearance_green);
		SetInfo(&equipment.color[item->details.slot_id].blue, item->generic_info.appearance_blue);
		SetInfo(&equipment.highlight[item->details.slot_id].red, item->generic_info.appearance_highlight_red);
		SetInfo(&equipment.highlight[item->details.slot_id].green, item->generic_info.appearance_highlight_green);
		SetInfo(&equipment.highlight[item->details.slot_id].blue, item->generic_info.appearance_highlight_blue);
	}
}

bool Entity::CheckSpellBonusRemoval(LuaSpell* spell, int16 type){
	MutexList<BonusValues*>::iterator itr = bonus_list.begin();
	while(itr.Next()){
		if(itr.value->luaspell == spell && itr.value->type == type){
			bonus_list.Remove(itr.value, true);
			return true;
		}
	}
	return false;
}

void Entity::AddSpellBonus(LuaSpell* spell, int16 type, sint32 value, int64 class_req){
	//CheckSpellBonusRemoval(spell, type);
	BonusValues* bonus = new BonusValues;
	bonus->luaspell = spell;
	bonus->spell_id = spell->spell->GetSpellID();
	bonus->type = type;
	bonus->value = value;
	bonus->class_req = class_req;
	bonus->tier = spell ? spell->spell->GetSpellTier() : 0;
	bonus_list.Add(bonus);
}

BonusValues* Entity::GetSpellBonus(int32 spell_id) {
	BonusValues *ret = 0;
	MutexList<BonusValues*>::iterator itr = bonus_list.begin();
	while (itr.Next()) {
		if (itr.value->spell_id == spell_id) {
			ret = itr.value;
			break;
		}
	}

	return ret;
}

vector<BonusValues*>* Entity::GetAllSpellBonuses(LuaSpell* spell) {
	vector<BonusValues*>* list = new vector<BonusValues*>;
	MutexList<BonusValues*>::iterator itr = bonus_list.begin();
	while (itr.Next()) {
		if (itr.value->luaspell == spell)
			list->push_back(itr.value);
	}
	return list;
}

void Entity::RemoveSpellBonus(LuaSpell* spell){
	MutexList<BonusValues*>::iterator itr = bonus_list.begin();
	while(itr.Next()){
		if(itr.value->luaspell == spell)
			bonus_list.Remove(itr.value, true);
	}
}

void Entity::CalculateSpellBonuses(ItemStatsValues* stats){
	if(stats){
		MutexList<BonusValues*>::iterator itr = bonus_list.begin();
		vector<BonusValues*> bv;
		//First check if we meet the requirement for each bonus
		while(itr.Next()) {
			int64 class1 = pow(2.0, (GetAdventureClass() - 1));
			int64 class2 = pow(2.0, (classes.GetSecondaryBaseClass(GetAdventureClass()) - 1));
			int64 class3 = pow(2.0, (classes.GetBaseClass(GetAdventureClass()) - 1));
			if (itr.value->class_req == 0 || (itr.value->class_req & class1) == class1 || (itr.value->class_req & class2) == class2 || (itr.value->class_req & class3) == class3)
				bv.push_back(itr.value);
		}
		//Sort the bonuses by spell id and luaspell
		BonusValues* bonus;
		map <int32, map<LuaSpell*, vector<BonusValues*> > > sort;
		for (int8 i = 0; i < bv.size(); i++){
			bonus = bv.at(i);
			sort[bonus->spell_id][bonus->luaspell].push_back(bonus);
		}
		//Now check for the highest tier of each spell id and apply those bonuses
		map<LuaSpell*, vector<BonusValues*> >::iterator tier_itr;
		map <int32, map<LuaSpell*, vector<BonusValues*> > >::iterator sort_itr;
		for (sort_itr = sort.begin(); sort_itr != sort.end(); sort_itr++){
			LuaSpell* key;
			sint8 highest_tier = -1;
			//Find the highest tier for this spell id
			for (tier_itr = sort_itr->second.begin(); tier_itr != sort_itr->second.end(); tier_itr++){
				LuaSpell* current_spell = tier_itr->first;
				sint8 current_tier;
				if (current_spell && ((current_tier = current_spell->spell->GetSpellTier()) > highest_tier)) {
					highest_tier = current_tier;
					key = current_spell;
				}
			}
			//We've found the highest tier for this spell id, so add the bonuses
			vector<BonusValues*>* final_bonuses = &sort_itr->second[key];
			for (int8 i = 0; i < final_bonuses->size(); i++) {
				world.AddBonuses(stats, final_bonuses->at(i)->type, final_bonuses->at(i)->value, this);
			}
		}
	}
}

void Entity::AddMezSpell(LuaSpell* spell) {
	if (!spell)
		return;

	if (!control_effects[CONTROL_EFFECT_TYPE_MEZ])
		control_effects[CONTROL_EFFECT_TYPE_MEZ] = new MutexList<LuaSpell*>;

	MutexList<LuaSpell*>* mez_spells = control_effects[CONTROL_EFFECT_TYPE_MEZ];

	if (IsPlayer() && !IsStunned() && !IsMezImmune() && mez_spells->size(true) == 0){
		((Player*)this)->SetPlayerControlFlag(1, 16, true);
		if (!IsRooted())
			((Player*)this)->SetPlayerControlFlag(1, 8, true);
		if (!IsStifled() && !IsFeared())
			GetZone()->LockAllSpells((Player*)this);
	}

	mez_spells->Add(spell);
}

void Entity::RemoveMezSpell(LuaSpell* spell) {
	MutexList<LuaSpell*>* mez_spells = control_effects[CONTROL_EFFECT_TYPE_MEZ];
	if (!mez_spells || mez_spells->size(true) == 0)
		return;

	mez_spells->Remove(spell);
	if (mez_spells->size(true) == 0){
		if (IsPlayer() && !IsMezImmune() && !IsStunned()){
			if (!IsStifled() && !IsFeared())
				GetZone()->UnlockAllSpells((Player*)this);
			((Player*)this)->SetPlayerControlFlag(1, 16, false);
			if (!IsRooted())
				((Player*)this)->SetPlayerControlFlag(1, 8, false);
		}
	}
}

void Entity::RemoveAllMezSpells() {
	MutexList<LuaSpell*>* mez_spells = control_effects[CONTROL_EFFECT_TYPE_MEZ];
	if (!mez_spells)
		return;

	MutexList<LuaSpell*>::iterator itr = mez_spells->begin();
	while (itr.Next()){
		LuaSpell* spell = itr.value;
		if (!spell)
			continue;
		GetZone()->RemoveTargetFromSpell(spell, this);
		RemoveDetrimentalSpell(spell);
		RemoveSpellEffect(spell);
		if (IsPlayer())
			((Player*)this)->RemoveSkillBonus(spell->spell->GetSpellID());
	}

	mez_spells->clear();
	if (IsPlayer() && !IsMezImmune() && !IsStunned()){
		if (!IsStifled() && !IsFeared())
			GetZone()->UnlockAllSpells((Player*)this);
		((Player*)this)->SetPlayerControlFlag(1, 16, false);
		if (!IsRooted())
			((Player*)this)->SetPlayerControlFlag(1, 8, false);
	}
}

void Entity::AddStifleSpell(LuaSpell* spell) {
	if (!spell)
		return;

	if (!control_effects[CONTROL_EFFECT_TYPE_STIFLE])
		control_effects[CONTROL_EFFECT_TYPE_STIFLE] = new MutexList<LuaSpell*>;

	if (IsPlayer() && control_effects[CONTROL_EFFECT_TYPE_STIFLE]->size(true) == 0 && !IsStifleImmune() && !IsMezzedOrStunned())
		GetZone()->LockAllSpells((Player*)this);

	control_effects[CONTROL_EFFECT_TYPE_STIFLE]->Add(spell);
}

void Entity::RemoveStifleSpell(LuaSpell* spell) {
	MutexList<LuaSpell*>* stifle_list = control_effects[CONTROL_EFFECT_TYPE_STIFLE];
	if (!stifle_list || stifle_list->size(true) == 0)
		return;

	stifle_list->Remove(spell);

	if (IsPlayer() && stifle_list->size(true) == 0 && !IsStifleImmune() && !IsMezzedOrStunned())
		GetZone()->UnlockAllSpells((Player*)this);
}

void Entity::AddDazeSpell(LuaSpell* spell) {
	if (!spell)
		return;

	if (!control_effects[CONTROL_EFFECT_TYPE_DAZE])
		control_effects[CONTROL_EFFECT_TYPE_DAZE] = new MutexList<LuaSpell*>;

	control_effects[CONTROL_EFFECT_TYPE_DAZE]->Add(spell);
}

void Entity::RemoveDazeSpell(LuaSpell* spell) {
	MutexList<LuaSpell*>* daze_list = control_effects[CONTROL_EFFECT_TYPE_DAZE];
	if (!daze_list || daze_list->size(true) == 0)
		return;

	daze_list->Remove(spell);
}

void Entity::AddStunSpell(LuaSpell* spell) {
	if (!spell)
		return;

	if (!control_effects[CONTROL_EFFECT_TYPE_STUN])
		control_effects[CONTROL_EFFECT_TYPE_STUN] = new MutexList<LuaSpell*>;

	if (IsPlayer() && control_effects[CONTROL_EFFECT_TYPE_STUN]->size(true) == 0 && !IsStunImmune()){
		if (!IsMezzed()){
			((Player*)this)->SetPlayerControlFlag(1, 16, true);
			if (!IsRooted())
				((Player*)this)->SetPlayerControlFlag(1, 8, true);
			if (!IsStifled() && !IsFeared())
				GetZone()->LockAllSpells((Player*)this);
		}
	}

	control_effects[CONTROL_EFFECT_TYPE_STUN]->Add(spell);
}

void Entity::RemoveStunSpell(LuaSpell* spell) {
	MutexList<LuaSpell*>* stun_list = control_effects[CONTROL_EFFECT_TYPE_STUN];
	if (!stun_list || stun_list->size(true) == 0)
		return;

	stun_list->Remove(spell);
	if (stun_list->size(true) == 0){
		if (IsPlayer() && !IsMezzed() && !IsStunImmune()){
			((Player*)this)->SetPlayerControlFlag(1, 16, false);
			if (!IsRooted())
				((Player*)this)->SetPlayerControlFlag(1, 8, false);
			if (!IsStifled() && !IsFeared())
				GetZone()->UnlockAllSpells((Player*)this);
		}
	}
}

void Entity::HideDeityPet(bool val) {
	if (!deityPet)
		return;

	if (val) {
		deityPet->AddAllowAccessSpawn(deityPet);
		GetZone()->HidePrivateSpawn(deityPet);
	}
	else
		deityPet->MakeSpawnPublic();
}

void Entity::HideCosmeticPet(bool val) {
	if (!cosmeticPet)
		return;

	if (val) {
		cosmeticPet->AddAllowAccessSpawn(cosmeticPet);
		GetZone()->HidePrivateSpawn(cosmeticPet);
	}
	else
		cosmeticPet->MakeSpawnPublic();
}

void Entity::DismissPet(NPC* pet, bool from_death) {
	if (!pet)
		return;

	Entity* PetOwner = pet->GetOwner();

	pet->SetDismissing(true);

	// Remove the spell maintained spell
	Spell* spell = master_spell_list.GetSpell(pet->GetPetSpellID(), pet->GetPetSpellTier());
	if (spell)
		GetZone()->GetSpellProcess()->DeleteCasterSpell(this, spell);

	if (pet->GetPetType() == PET_TYPE_CHARMED) {
		PetOwner->SetCharmedPet(0);

		if (!from_death) {
			// set the pet flag to false, owner to 0, and give the mob its old brain back
			pet->SetPet(false);
			pet->SetOwner(0);
			pet->SetBrain(new Brain(pet));
			pet->SetDismissing(false);
		}
	}
	else if (pet->GetPetType() == PET_TYPE_COMBAT)
		PetOwner->SetCombatPet(0);
	else if (pet->GetPetType() == PET_TYPE_DEITY)
		PetOwner->SetDeityPet(0);
	else if (pet->GetPetType() == PET_TYPE_COSMETIC)
		PetOwner->SetCosmeticPet(0);

	// if owner is player and no combat pets left reset the pet info
	if (PetOwner->IsPlayer()) {
		if (!PetOwner->GetPet() && !PetOwner->GetCharmedPet())
			((Player*)PetOwner)->ResetPetInfo();
	}

	// remove the spawn from the world
	if (!from_death && pet->GetPetType() != PET_TYPE_CHARMED)
		GetZone()->RemoveSpawn(pet);
}

float Entity::CalculateBonusMod() {
	int8 level = GetLevel();
	if (level <= 20)
		return 3.0;
	else if (level >= 90)
		return 10.0;
	else
		return (level - 20) * .1 + 3.0;
}

float Entity::CalculateDPSMultiplier(){
	float dps = GetInfoStruct()->dps;

	if (dps > 0){
		if (dps <= 100)
			return (dps / 100 + 1);
		else if (dps <= 200)
			return (((dps - 100) * .25 + 100) / 100 + 1);
		else if (dps <= 300)
			return (((dps - 200) * .1 + 125) / 100 + 1);
		else if (dps <= 900)
			return (((dps - 300) * .05 + 135) / 100 + 1);
		else
			return (((dps - 900) * .01 + 165) / 100 + 1);
	}
	return 1;
}	

void Entity::AddWard(LuaSpell* luaspell, WardInfo* ward) {
	if (m_wardList.count(luaspell) == 0) {
		m_wardList[luaspell] = ward;
	}
}

WardInfo* Entity::GetWard(LuaSpell* luaspell) {
	WardInfo* ret = 0;

	if (m_wardList.count(luaspell) > 0)
		ret = m_wardList[luaspell];

	return ret;
}

void Entity::RemoveWard(LuaSpell* luaspell) {
	if (m_wardList.count(luaspell) > 0) {
		// Delete the ward info
		safe_delete(m_wardList[luaspell]);
		// Remove from the ward list
		m_wardList.erase(luaspell);
	}
}

int32 Entity::CheckWards(int32 damage, int8 damage_type) {
	map<LuaSpell*, WardInfo*>::iterator itr;
	WardInfo* ward = 0;
	LuaSpell* spell = 0;

	while (m_wardList.size() > 0 && damage > 0) {
		// Get the ward with the lowest base damage
		for (itr = m_wardList.begin(); itr != m_wardList.end(); itr++) {
			if (!ward || itr->second->BaseDamage < ward->BaseDamage) {
				if (itr->second->DamageLeft > 0 &&
					(itr->second->WardType == WARD_TYPE_ALL ||
					(itr->second->WardType == WARD_TYPE_PHYSICAL && damage_type >= DAMAGE_PACKET_DAMAGE_TYPE_SLASH && damage_type <= DAMAGE_PACKET_DAMAGE_TYPE_PIERCE) ||
					(itr->second->WardType == WARD_TYPE_MAGICAL && ((itr->second->DamageType == 0 && damage_type >= DAMAGE_PACKET_DAMAGE_TYPE_PIERCE) || (damage_type >= DAMAGE_PACKET_DAMAGE_TYPE_PIERCE && itr->second->DamageType == damage_type)))))
					ward = itr->second;
			}
		}

		if (!ward)
			break;

		spell = ward->Spell;

		if (damage >= ward->DamageLeft) {
			// Damage is greater than or equal to the amount left on the ward
			damage -= ward->DamageLeft;

			GetZone()->SendHealPacket(spell->caster, this, HEAL_PACKET_TYPE_ABSORB, ward->DamageLeft, spell->spell->GetName());

			ward->DamageLeft = 0;
			spell->damage_remaining = 0;

			if (!ward->keepWard) {
				RemoveWard(spell);
				GetZone()->GetSpellProcess()->DeleteCasterSpell(spell);
			}
		} else {
			// Damage is less then the amount left on the ward
			ward->DamageLeft -= damage;
			spell->damage_remaining = ward->DamageLeft;
			GetZone()->SendHealPacket(ward->Spell->caster, this, HEAL_PACKET_TYPE_ABSORB, damage, spell->spell->GetName());
			damage = 0;
		}

		if (spell->caster->IsPlayer())
			ClientPacketFunctions::SendMaintainedExamineUpdate(GetZone()->GetClientBySpawn(spell->caster), spell->slot_pos, ward->DamageLeft, 1);

		// Reset ward pointer
		ward = 0;
	}

	return damage;
}

float Entity::CalculateCastingSpeedMod() {
	float cast_speed = info_struct.casting_speed;
	
	if(cast_speed > 0)
		return 100 * max((float) 0.5, (float) (1 + (1 - (1 / (1 + (cast_speed * .01))))));
	else if (cast_speed < 0)
		return 100 * min((float) 1.5, (float) (1 + (1 - (1 / (1 + (cast_speed * -.01))))));
	return 0;
}

float Entity::GetSpeed() {
	float ret = speed;

	if (IsStealthed() || IsInvis())
		ret += stats[ITEM_STAT_STEALTHINVISSPEEDMOD];
	else if (EngagedInCombat())
		ret += stats[ITEM_STAT_OFFENSIVESPEED];
	else
		ret += max(stats[ITEM_STAT_SPEED], stats[ITEM_STAT_MOUNTSPEED]);

	ret *= speed_multiplier;
	return ret;
}

float Entity::GetAirSpeed() {
	float ret = speed;

	if (!EngagedInCombat())
		ret += stats[ITEM_STAT_MOUNTAIRSPEED];

	ret *= speed_multiplier;
	return ret;
}

int8 Entity::GetTraumaCount() {
	return det_count_list[DET_TYPE_TRAUMA];
}

int8 Entity::GetArcaneCount() {
	return det_count_list[DET_TYPE_ARCANE];
}

int8 Entity::GetNoxiousCount() {
	return det_count_list[DET_TYPE_NOXIOUS];
}

int8 Entity::GetElementalCount() {
	return det_count_list[DET_TYPE_ELEMENTAL];
}

int8 Entity::GetCurseCount() {
	return det_count_list[DET_TYPE_CURSE];
}

Mutex* Entity::GetDetrimentMutex() {
	return &MDetriments;
}

Mutex* Entity::GetMaintainedMutex() {
	return &MMaintainedSpells;
}

Mutex* Entity::GetSpellEffectMutex() {
	return &MSpellEffects;
}

bool Entity::HasCurableDetrimentType(int8 det_type) {
	DetrimentalEffects* det;
	bool ret = false;
	MDetriments.readlock(__FUNCTION__, __LINE__);
	for (int32 i = 0; i < detrimental_spell_effects.size(); i++){
		det = &detrimental_spell_effects.at(i);
		if(det && det->det_type == det_type && !det->incurable){
			ret = true;
			break;
		}
	}
	MDetriments.releasereadlock(__FUNCTION__, __LINE__);
	return ret;
}

void Entity::ClearAllDetriments() {
	MDetriments.writelock(__FUNCTION__, __LINE__);
	detrimental_spell_effects.clear();
	det_count_list.clear();
	MDetriments.releasewritelock(__FUNCTION__, __LINE__);
}

void Entity::CureDetrimentByType(int8 cure_count, int8 det_type, string cure_name, Entity* caster, int8 cure_level) {
	if (cure_count <= 0 || GetDetTypeCount(det_type) <= 0)
		return;

	vector<DetrimentalEffects>* det_list = &detrimental_spell_effects;
	DetrimentalEffects* det;
	vector<LuaSpell*> remove_list;
	LuaSpell* spell = 0;
	vector<LevelArray*>* levels;
	int8 caster_class1 = 0;
	int8 caster_class2 = 0;
	int8 caster_class3 = 0;
	int8 level_class = 0;
	InfoStruct* info_struct = 0;
	bool pass_level_check = false;

	MDetriments.readlock(__FUNCTION__, __LINE__);
	for (int32 i = 0; i<det_list->size(); i++){
		det = &det_list->at(i);
		if (det && det->det_type == det_type && !det->incurable){
			levels = det->spell->spell->GetSpellLevels();
			info_struct = det->caster->GetInfoStruct();
			caster_class1 = info_struct->class1;
			caster_class2 = info_struct->class2;
			caster_class3 = info_struct->class3;
			pass_level_check = false;
			for (int32 x = 0; x < levels->size(); x++){
				level_class = levels->at(x)->adventure_class;
				if (!cure_level || ((caster_class1 == level_class || caster_class2 == level_class || caster_class3 == level_class)
					&& cure_level >= (levels->at(x)->spell_level / 10))){
					pass_level_check = true;
					break;
				}
			}
			if (pass_level_check){
				remove_list.push_back(det->spell);
				cure_count--;
				if (cure_count == 0)
					break;
			}
		}
	}
	MDetriments.releasereadlock(__FUNCTION__, __LINE__);

	for (int32 i = 0; i<remove_list.size(); i++){
		spell = remove_list.at(i);
		GetZone()->SendDispellPacket(caster, this, cure_name, (string)remove_list.at(i)->spell->GetName(), DISPELL_TYPE_CURE);
		if (GetZone())
			GetZone()->RemoveTargetFromSpell(spell, this);
		RemoveSpellEffect(spell);
		RemoveDetrimentalSpell(spell);
	}
	remove_list.clear();
}

void Entity::CureDetrimentByControlEffect(int8 cure_count, int8 control_type, string cure_name, Entity* caster, int8 cure_level) {
	if (cure_count <= 0 || GetDetCount() <= 0)
		return;

	vector<DetrimentalEffects>* det_list = &detrimental_spell_effects;
	DetrimentalEffects* det;
	vector<LuaSpell*> remove_list;
	LuaSpell* spell = 0;
	vector<LevelArray*>* levels;
	int8 caster_class1 = 0;
	int8 caster_class2 = 0;
	int8 caster_class3 = 0;
	int8 level_class = 0;
	InfoStruct* info_struct = 0;
	bool pass_level_check = false;

	MDetriments.readlock(__FUNCTION__, __LINE__);
	for (int32 i = 0; i<det_list->size(); i++){
		det = &det_list->at(i);
		if (det && det->control_effect == control_type && !det->incurable){
			levels = det->spell->spell->GetSpellLevels();
			info_struct = det->caster->GetInfoStruct();
			caster_class1 = info_struct->class1;
			caster_class2 = info_struct->class2;
			caster_class3 = info_struct->class3;
			pass_level_check = false;
			for (int32 x = 0; x < levels->size(); x++){
				level_class = levels->at(x)->adventure_class;
				if (!cure_level || ((caster_class1 == level_class || caster_class2 == level_class || caster_class3 == level_class)
					&& cure_level >= (levels->at(x)->spell_level / 10))){
					pass_level_check = true;
					break;
				}
			}
			if (pass_level_check){
				remove_list.push_back(det->spell);
				cure_count--;
				if (cure_count == 0)
					break;
			}
		}
	}
	MDetriments.releasereadlock(__FUNCTION__, __LINE__);

	for (int32 i = 0; i<remove_list.size(); i++){
		spell = remove_list.at(i);
		GetZone()->SendDispellPacket(caster, this, cure_name, (string)remove_list.at(i)->spell->GetName(), DISPELL_TYPE_CURE);
		if (GetZone())
			GetZone()->RemoveTargetFromSpell(spell, this);
		RemoveSpellEffect(spell);
		RemoveDetrimentalSpell(spell);
	}
	remove_list.clear();
}

void Entity::RemoveDetrimentalSpell(LuaSpell* spell) {
	if(!spell || spell->spell->GetSpellData()->det_type == 0)
		return;
	MDetriments.writelock(__FUNCTION__, __LINE__);
	vector<DetrimentalEffects>* det_list = &detrimental_spell_effects;
	vector<DetrimentalEffects>::iterator itr;
	for(itr = det_list->begin(); itr != det_list->end(); itr++){
		if((*itr).spell == spell){
			det_count_list[(*itr).det_type]--;
			det_list->erase(itr);
			if(IsPlayer())
				((Player*)this)->SetCharSheetChanged(true);
			break;
		}
	}
	MDetriments.releasewritelock(__FUNCTION__, __LINE__);
}

int8 Entity::GetDetTypeCount(int8 det_type){
	return det_count_list[det_type];
}

int8 Entity::GetDetCount() {
	int8 det_count = 0;
	map<int8, int8>::iterator itr;

	for(itr=det_count_list.begin(); itr != det_count_list.end(); itr++)
		det_count += (*itr).second;
	
	return det_count;
}

vector<DetrimentalEffects>* Entity::GetDetrimentalSpellEffects() {
	return &detrimental_spell_effects;
}

void Entity::AddDetrimentalSpell(LuaSpell* luaspell){
	if(!luaspell || !luaspell->caster)
		return;
	
	Spell* spell = luaspell->spell;
	DetrimentalEffects* det = GetDetrimentalEffect(spell->GetSpellID(), luaspell->caster);
	DetrimentalEffects new_det;
	if(det)
		RemoveDetrimentalSpell(det->spell);

	SpellData* data = spell->GetSpellData();
	if(!data)
		return;

	new_det.caster = luaspell->caster;
	new_det.spell = luaspell;
	if (spell->GetSpellData()->duration_until_cancel)
		new_det.expire_timestamp = 0xFFFFFFFF;
	else
		new_det.expire_timestamp = Timer::GetCurrentTime2() + (spell->GetSpellDuration()*100);
	new_det.icon = data->icon;
	new_det.icon_backdrop = data->icon_backdrop;
	new_det.tier = data->tier;
	new_det.det_type = data->det_type;
	new_det.incurable = data->incurable;
	new_det.spell_id = spell->GetSpellID();
	new_det.control_effect = data->control_effect_type;
	new_det.total_time = spell->GetSpellDuration()/10;

	MDetriments.writelock(__FUNCTION__, __LINE__);
	detrimental_spell_effects.push_back(new_det);
	det_count_list[new_det.det_type]++;
	MDetriments.releasewritelock(__FUNCTION__, __LINE__);
}

DetrimentalEffects* Entity::GetDetrimentalEffect(int32 spell_id, Entity* caster){
	vector<DetrimentalEffects>* det_list = &detrimental_spell_effects;
	DetrimentalEffects* ret = 0;
	MDetriments.readlock(__FUNCTION__, __LINE__);
	for(int32 i=0; i<det_list->size(); i++){
		if (det_list->at(i).spell_id == spell_id && det_list->at(i).caster == caster)
			ret = &det_list->at(i);
	}
	MDetriments.releasereadlock(__FUNCTION__, __LINE__);
	
	return ret;
}

void Entity::CancelAllStealth() {
	bool did_change = false;
	MutexList<LuaSpell*>* stealth_list = control_effects[CONTROL_EFFECT_TYPE_STEALTH];
	if (stealth_list){
		MutexList<LuaSpell*>::iterator itr = stealth_list->begin();
		while (itr.Next()){
			if (itr.value->caster == this)
				GetZone()->GetSpellProcess()->AddSpellCancel(itr.value);
			else{
				GetZone()->RemoveTargetFromSpell(itr.value, this);
				RemoveSpellEffect(itr.value);
			}
			did_change = true;
		}
	}
	MutexList<LuaSpell*>* invis_list = control_effects[CONTROL_EFFECT_TYPE_INVIS];
	if (invis_list){
		MutexList<LuaSpell*>::iterator invis_itr = invis_list->begin();
		while (invis_itr.Next()){
			if (invis_itr.value->caster == this)
				GetZone()->GetSpellProcess()->AddSpellCancel(invis_itr.value);
			else{
				GetZone()->RemoveTargetFromSpell(invis_itr.value, this);
				RemoveSpellEffect(invis_itr.value);
			}
			did_change = true;
		}
	}

	if (did_change){
		info_changed = true;
		changed = true;
		AddChangedZoneSpawn();
		if (IsPlayer())
			((Player*)this)->SetCharSheetChanged(true);
	}
}

bool Entity::CanAttackTarget(Spawn *target) {
	if (IsPlayer() && (target->IsPlayer() || target->IsPet() && ((NPC*)target)->GetOwner()->IsPlayer())) {
		return PVP::CanAttack((Player*)this, target);
	} else {
		if (target->IsPlayer())
			return true;
		return target->GetAttackable();
	}
}

bool Entity::IsStealthed(){
	MutexList<LuaSpell*>* stealth_list = control_effects[CONTROL_EFFECT_TYPE_STEALTH];
	return  (!stealth_list || stealth_list->size(true) == 0) == false;
}

bool Entity::IsInvis(){
	MutexList<LuaSpell*>* invis_list = control_effects[CONTROL_EFFECT_TYPE_INVIS];
	return  (!invis_list || invis_list->size(true) == 0) == false;
}

void Entity::AddStealthSpell(LuaSpell* spell) {
	if (!spell)
		return;

	if (!control_effects[CONTROL_EFFECT_TYPE_STEALTH])
		control_effects[CONTROL_EFFECT_TYPE_STEALTH] = new MutexList<LuaSpell*>;

	control_effects[CONTROL_EFFECT_TYPE_STEALTH]->Add(spell);
	if (control_effects[CONTROL_EFFECT_TYPE_STEALTH]->size(true) == 1){
		info_changed = true;
		changed = true;
		AddChangedZoneSpawn();
		if (IsPlayer()) {
			((Player*)this)->SetResendSpawns(true);
			((Player*)this)->SetCharSheetChanged(true);
		}
	}
}

void Entity::AddInvisSpell(LuaSpell* spell) {
	if (!spell)
		return;

	if (!control_effects[CONTROL_EFFECT_TYPE_INVIS])
		control_effects[CONTROL_EFFECT_TYPE_INVIS] = new MutexList<LuaSpell*>;

	control_effects[CONTROL_EFFECT_TYPE_INVIS]->Add(spell);
	if (control_effects[CONTROL_EFFECT_TYPE_INVIS]->size(true) == 1){
		info_changed = true;
		changed = true;
		AddChangedZoneSpawn();
		if (IsPlayer())
			((Player*)this)->SetResendSpawns(true);
			((Player*)this)->SetCharSheetChanged(true);
	}
}

void Entity::RemoveInvisSpell(LuaSpell* spell) {
	MutexList<LuaSpell*>* invis_list = control_effects[CONTROL_EFFECT_TYPE_INVIS];
	if (!invis_list || invis_list->size(true) == 0)
		return;

	invis_list->Remove(spell);
	RemoveSpellEffect(spell);
	if (invis_list->size(true) == 0){
		info_changed = true;
		changed = true;
		AddChangedZoneSpawn();
		if (IsPlayer())
			((Player*)this)->SetResendSpawns(true);
			((Player*)this)->SetCharSheetChanged(true);
	}
}

void Entity::RemoveStealthSpell(LuaSpell* spell) {
	MutexList<LuaSpell*>* stealth_list = control_effects[CONTROL_EFFECT_TYPE_STEALTH];
	if (!stealth_list || stealth_list->size(true) == 0)
		return;

	stealth_list->Remove(spell);
	RemoveSpellEffect(spell);
	if (stealth_list->size() == 0){
		info_changed = true;
		changed = true;
		AddChangedZoneSpawn();
		if (IsPlayer())
			((Player*)this)->SetResendSpawns(true);
			((Player*)this)->SetCharSheetChanged(true);
	}
}

void Entity::AddRootSpell(LuaSpell* spell) {
	if (!spell)
		return;

	if (!control_effects[CONTROL_EFFECT_TYPE_ROOT])
		control_effects[CONTROL_EFFECT_TYPE_ROOT] = new MutexList<LuaSpell*>;

	if (control_effects[CONTROL_EFFECT_TYPE_ROOT]->size(true) == 0 && !IsRootImmune()) {
		if (IsPlayer()){
			if (!IsMezzedOrStunned())
				((Player*)this)->SetPlayerControlFlag(1, 8, true); // heading movement only
		}
		else
			SetSpeedMultiplier(0.0f);
	}

	control_effects[CONTROL_EFFECT_TYPE_ROOT]->Add(spell);
}

void Entity::RemoveRootSpell(LuaSpell* spell) {
	MutexList<LuaSpell*>* root_list = control_effects[CONTROL_EFFECT_TYPE_ROOT];
	if (!root_list || root_list->size(true) == 0)
		return;

	root_list->Remove(spell);
	if (root_list->size(true) == 0 && !IsRootImmune()) {
		if (IsPlayer()){
			if (!IsMezzedOrStunned())
				((Player*)this)->SetPlayerControlFlag(1, 8, false); // heading movement only
		}
		else {
			// GetHighestSnare() will return 1.0f if no snares returning the spawn to full speed
			SetSpeedMultiplier(GetHighestSnare());
		}
	}
}

void Entity::AddFearSpell(LuaSpell* spell){
	if (!spell)
		return;

	if (!control_effects[CONTROL_EFFECT_TYPE_FEAR])
		control_effects[CONTROL_EFFECT_TYPE_FEAR] = new MutexList<LuaSpell*>;

	if (IsPlayer() && control_effects[CONTROL_EFFECT_TYPE_FEAR]->size(true) == 0 && !IsFearImmune()){
		((Player*)this)->SetPlayerControlFlag(4, 4, true); // feared
		if (!IsMezzedOrStunned() && !IsStifled())
			GetZone()->LockAllSpells((Player*)this);
	}

	control_effects[CONTROL_EFFECT_TYPE_FEAR]->Add(spell);
}

void Entity::RemoveFearSpell(LuaSpell* spell){
	MutexList<LuaSpell*>* fear_list = control_effects[CONTROL_EFFECT_TYPE_FEAR];
	if (!fear_list || fear_list->size(true) == 0)
		return;

	fear_list->Remove(spell);

	if (IsPlayer() && fear_list->size(true) == 0 && !IsFearImmune()){
		((Player*)this)->SetPlayerControlFlag(4, 4, false); // feared disabled
		if (!IsMezzedOrStunned() && !IsStifled())
			GetZone()->LockAllSpells((Player*)this);
	}
}

void Entity::AddSnareSpell(LuaSpell* spell) {
	if (!spell)
		return;

	if (!control_effects[CONTROL_EFFECT_TYPE_SNARE])
		control_effects[CONTROL_EFFECT_TYPE_SNARE] = new MutexList<LuaSpell*>;

	control_effects[CONTROL_EFFECT_TYPE_SNARE]->Add(spell);

	// Don't set speed multiplier if there is a root or no snare values
	MutexList<LuaSpell*>* roots = control_effects[CONTROL_EFFECT_TYPE_ROOT];
	if ((!roots || roots->size(true) == 0) && snare_values.size() > 0)
		SetSpeedMultiplier(GetHighestSnare());
}

void Entity::RemoveSnareSpell(LuaSpell* spell) {
	MutexList<LuaSpell*>* snare_list = control_effects[CONTROL_EFFECT_TYPE_SNARE];
	if (!snare_list || snare_list->size(true) == 0)
		return;

	snare_list->Remove(spell);
	snare_values.erase(spell);

	//LogWrite(PLAYER__ERROR, 0, "Debug", "snare_values.size() = %u", snare_values.size());

	// only change speeds if there are no roots
	MutexList<LuaSpell*>* roots = control_effects[CONTROL_EFFECT_TYPE_ROOT];
	if (!roots || roots->size(true) == 0) {
		float multiplier = GetHighestSnare();
		//LogWrite(PLAYER__ERROR, 0, "Debug", "GetHighestSnare() = %f", multiplier);
		SetSpeedMultiplier(multiplier);
	}
}

void Entity::SetSnareValue(LuaSpell* spell, float snare_val) {
	if (!spell)
		return;

	snare_values[spell] = snare_val;
}

float Entity::GetHighestSnare() {
	// For simplicity this will return the highest snare value, which is actually the lowest value
	float ret = 1.0f;

	if (snare_values.size() == 0)
		return ret;

	map<LuaSpell*, float>::iterator itr;
	for (itr = snare_values.begin(); itr != snare_values.end(); itr++) {
		if (itr->second < ret)
			ret = itr->second;
	}

	return ret;
}

bool Entity::IsSnared() {
	MutexList<LuaSpell*>* snare_list = control_effects[CONTROL_EFFECT_TYPE_SNARE];
	return (!snare_list || snare_list->size(true) == 0) == false;
}

bool Entity::IsMezzed(){
	MutexList<LuaSpell*>* mez_spells = control_effects[CONTROL_EFFECT_TYPE_MEZ];
	return  (!mez_spells || mez_spells->size(true) == 0 || IsMezImmune()) == false;
}

bool Entity::IsStifled(){
	MutexList<LuaSpell*>* stifle_list = control_effects[CONTROL_EFFECT_TYPE_STIFLE];
	return  (!stifle_list || stifle_list->size(true) == 0 || IsStifleImmune()) == false;
}

bool Entity::IsDazed(){
	MutexList<LuaSpell*>* daze_list = control_effects[CONTROL_EFFECT_TYPE_DAZE];
	return  (!daze_list || daze_list->size(true) == 0 || IsDazeImmune()) == false;
}

bool Entity::IsStunned(){
	MutexList<LuaSpell*>* stun_list = control_effects[CONTROL_EFFECT_TYPE_STUN];
	return (!stun_list || stun_list->size(true) == 0 || IsStunImmune()) == false;
}

bool Entity::IsRooted(){
	MutexList<LuaSpell*>* root_list = control_effects[CONTROL_EFFECT_TYPE_ROOT];
	return (!root_list || root_list->size(true) == 0 || IsRootImmune()) == false;
}

bool Entity::IsFeared(){
	MutexList<LuaSpell*>* fear_list = control_effects[CONTROL_EFFECT_TYPE_FEAR];
	return (!fear_list || fear_list->size(true) == 0 || IsFearImmune()) == false;
}

bool Entity::IsWarded() {
	map<LuaSpell*, WardInfo*>::iterator itr;

	for (itr = m_wardList.begin(); itr != m_wardList.end(); itr++) {
		if (itr->second->DamageLeft > 0)
			return true;
	}
	
	return false;
}

void Entity::AddWaterwalkSpell(LuaSpell* spell){
	if (!spell)
		return;

	if (!control_effects[CONTROL_EFFECT_TYPE_WALKUNDERWATER])
		control_effects[CONTROL_EFFECT_TYPE_WALKUNDERWATER] = new MutexList<LuaSpell*>;

	control_effects[CONTROL_EFFECT_TYPE_WALKUNDERWATER]->Add(spell);
	if (control_effects[CONTROL_EFFECT_TYPE_WALKUNDERWATER]->size(true) == 1 && IsPlayer())
		((Player*)this)->SetPlayerControlFlag(3, 128, true); // enable walking underwater
}

void Entity::RemoveWaterwalkSpell(LuaSpell* spell){
	MutexList<LuaSpell*>* waterwalk_list = control_effects[CONTROL_EFFECT_TYPE_WALKUNDERWATER];
	if (!waterwalk_list || waterwalk_list->size(true) == 0)
		return;

	waterwalk_list->Remove(spell);
	if (waterwalk_list->size(true) == 0 && IsPlayer())
		((Player*)this)->SetPlayerControlFlag(3, 128, false); // disable walking underwater
}

void Entity::AddWaterjumpSpell(LuaSpell* spell){
	if (!spell)
		return;

	if (!control_effects[CONTROL_EFFECT_TYPE_JUMPUNDERWATER])
		control_effects[CONTROL_EFFECT_TYPE_JUMPUNDERWATER] = new MutexList<LuaSpell*>;

	control_effects[CONTROL_EFFECT_TYPE_JUMPUNDERWATER]->Add(spell);
	if (control_effects[CONTROL_EFFECT_TYPE_JUMPUNDERWATER]->size(true) == 1 && IsPlayer())
		((Player*)this)->SetPlayerControlFlag(4, 1, true); // enable moonjumps underwater
}

void Entity::RemoveWaterjumpSpell(LuaSpell* spell){
	MutexList<LuaSpell*>* waterjump_list = control_effects[CONTROL_EFFECT_TYPE_JUMPUNDERWATER];
	if (!waterjump_list || waterjump_list->size(true) == 0)
		return;

	waterjump_list->Remove(spell);
	if (waterjump_list->size(true) == 0 && IsPlayer())
		((Player*)this)->SetPlayerControlFlag(4, 1, false); // disable moonjumps underwater
}

void Entity::AddAOEImmunity(LuaSpell* spell){
	if (!spell)
		return;

	if (!immunities[IMMUNITY_TYPE_AOE])
		immunities[IMMUNITY_TYPE_AOE] = new MutexList<LuaSpell*>;

	immunities[IMMUNITY_TYPE_AOE]->Add(spell);
}

void Entity::RemoveAOEImmunity(LuaSpell* spell){
	MutexList<LuaSpell*>* aoe_list = immunities[IMMUNITY_TYPE_AOE];
	if (!aoe_list || aoe_list->size(true) == 0)
		return;
	aoe_list->Remove(spell);
}

bool Entity::IsAOEImmune(){
	return (immunities[IMMUNITY_TYPE_AOE] && immunities[IMMUNITY_TYPE_AOE]->size(true));
}

void Entity::AddStunImmunity(LuaSpell* spell){
	if (!spell)
		return;

	if (!immunities[IMMUNITY_TYPE_STUN])
		immunities[IMMUNITY_TYPE_STUN] = new MutexList<LuaSpell*>;

	if (IsPlayer() && IsStunned() && !IsMezzed()){
		((Player*)this)->SetPlayerControlFlag(4, 64, false);
		if (!IsFeared() && !IsStifled())
			((Player*)this)->UnlockAllSpells();
	}

	immunities[IMMUNITY_TYPE_STUN]->Add(spell);
}

void Entity::RemoveStunImmunity(LuaSpell* spell){
	MutexList<LuaSpell*>* stun_list = immunities[IMMUNITY_TYPE_STUN];
	if (!stun_list || stun_list->size(true) == 0)
		return;

	stun_list->Remove(spell);

	if (IsPlayer() && IsStunned() && !IsMezzed()){
		((Player*)this)->SetPlayerControlFlag(4, 64, true);
		if (!IsFeared() && !IsStifled())
			((Player*)this)->UnlockAllSpells();
	}
}

bool Entity::IsStunImmune(){
	return (immunities[IMMUNITY_TYPE_STUN] && immunities[IMMUNITY_TYPE_STUN]->size(true) > 0);
}

void Entity::AddStifleImmunity(LuaSpell* spell){
	if (!spell)
		return;

	if (!immunities[IMMUNITY_TYPE_STIFLE])
		immunities[IMMUNITY_TYPE_STIFLE] = new MutexList<LuaSpell*>;

	if (IsPlayer() && immunities[IMMUNITY_TYPE_STIFLE]->size(true) == 0){
		if (IsStifled() && !IsMezzedOrStunned() && !IsFeared())
			((Player*)this)->UnlockAllSpells();
	}

	immunities[IMMUNITY_TYPE_STIFLE]->Add(spell);
}

void Entity::RemoveStifleImmunity(LuaSpell* spell){
	MutexList<LuaSpell*>* stifle_list = immunities[IMMUNITY_TYPE_STIFLE];
	if (!stifle_list || stifle_list->size(true) == 0)
		return;

	stifle_list->Remove(spell);

	if (IsPlayer() && IsStifled() && !IsMezzedOrStunned() && !IsFeared())
		((Player*)this)->UnlockAllSpells();
}

bool Entity::IsStifleImmune(){
	return (immunities[IMMUNITY_TYPE_STIFLE] && immunities[IMMUNITY_TYPE_STIFLE]->size(true) > 0);
}

void Entity::AddMezImmunity(LuaSpell* spell){
	if (!spell)
		return;

	if (!immunities[IMMUNITY_TYPE_MEZ])
		immunities[IMMUNITY_TYPE_MEZ] = new MutexList<LuaSpell*>;

	if (IsPlayer() && IsMezzed() && !IsStunned()){
		((Player*)this)->SetPlayerControlFlag(4, 64, false);
		if (!IsFeared() && !IsStifled())
			((Player*)this)->UnlockAllSpells();
	}

	immunities[IMMUNITY_TYPE_MEZ]->Add(spell);
}

void Entity::RemoveMezImmunity(LuaSpell* spell){
	MutexList<LuaSpell*>* mez_list = immunities[IMMUNITY_TYPE_MEZ];
	if (!mez_list || mez_list->size(true) == 0)
		return;

	mez_list->Remove(spell);

	if (IsPlayer() && IsMezzed() && !IsStunned()){
		((Player*)this)->SetPlayerControlFlag(4, 64, true);
		if (!IsFeared() && !IsStifled())
			((Player*)this)->LockAllSpells();
	}
}

bool Entity::IsMezImmune(){
	return (immunities[IMMUNITY_TYPE_MEZ] && immunities[IMMUNITY_TYPE_MEZ]->size(true) > 0);
}

void Entity::AddRootImmunity(LuaSpell* spell){
	if (!spell)
		return;

	if (!immunities[IMMUNITY_TYPE_ROOT])
		immunities[IMMUNITY_TYPE_ROOT] = new MutexList<LuaSpell*>;

	if (IsPlayer() && IsRooted())
		((Player*)this)->SetPlayerControlFlag(1, 8, false);

	immunities[IMMUNITY_TYPE_ROOT]->Add(spell);
}

void Entity::RemoveRootImmunity(LuaSpell* spell){
	MutexList<LuaSpell*>* root_list = immunities[IMMUNITY_TYPE_ROOT];
	if (!root_list || root_list->size(true) == 0)
		return;

	root_list->Remove(spell);

	if (IsPlayer() && IsRooted())
		((Player*)this)->SetPlayerControlFlag(1, 8, true);
}

bool Entity::IsRootImmune(){
	return (immunities[IMMUNITY_TYPE_ROOT] && immunities[IMMUNITY_TYPE_ROOT]->size(true) > 0);
}

void Entity::AddFearImmunity(LuaSpell* spell){
	if (!spell)
		return;

	if (!immunities[IMMUNITY_TYPE_FEAR])
		immunities[IMMUNITY_TYPE_FEAR] = new MutexList<LuaSpell*>;

	if (IsPlayer() && IsFeared()){
		if (!IsMezzedOrStunned() && !IsStifled())
			((Player*)this)->UnlockAllSpells();
		((Player*)this)->SetPlayerControlFlag(4, 4, false);
	}

	immunities[IMMUNITY_TYPE_FEAR]->Add(spell);
}

void Entity::RemoveFearImmunity(LuaSpell* spell){
	MutexList<LuaSpell*>* fear_list = immunities[IMMUNITY_TYPE_FEAR];
	if (!fear_list || fear_list->size(true) == 0)
		return;

	fear_list->Remove(spell);

	if (IsPlayer() && IsFeared()){
		if (!IsMezzedOrStunned() && !IsStifled())
			((Player*)this)->LockAllSpells();
		((Player*)this)->SetPlayerControlFlag(4, 4, true);
	}
}

bool Entity::IsFearImmune(){
	return (immunities[IMMUNITY_TYPE_FEAR] && immunities[IMMUNITY_TYPE_FEAR]->size(true) > 0);
}

void Entity::AddDazeImmunity(LuaSpell* spell){
	if (!spell)
		return;

	if (!immunities[IMMUNITY_TYPE_DAZE])
		immunities[IMMUNITY_TYPE_DAZE] = new MutexList<LuaSpell*>;

	immunities[IMMUNITY_TYPE_DAZE]->Add(spell);
}

void Entity::RemoveDazeImmunity(LuaSpell* spell){
	MutexList<LuaSpell*>* daze_list = immunities[IMMUNITY_TYPE_DAZE];
	if (!daze_list || daze_list->size(true) == 0)
		return;

	daze_list->Remove(spell);
}

bool Entity::IsDazeImmune(){
	return (immunities[IMMUNITY_TYPE_DAZE] && immunities[IMMUNITY_TYPE_DAZE]->size(true) > 0);
}

void Entity::RemoveEffectsFromLuaSpell(LuaSpell* spell){
	if (!spell)
		return;

	//Attempt to remove all applied effects from this spell when spell has been removed from just this target. Should improve performance/easier maitenance
	int32 effect_bitmask = spell->effect_bitmask;
	if (effect_bitmask == 0)
		return;

	if (effect_bitmask & EFFECT_FLAG_STUN)
		RemoveStunSpell(spell);
	if (effect_bitmask & EFFECT_FLAG_ROOT)
		RemoveRootSpell(spell);
	if (effect_bitmask & EFFECT_FLAG_MEZ)
		RemoveMezSpell(spell);
	if (effect_bitmask & EFFECT_FLAG_STIFLE)
		RemoveStifleSpell(spell);
	if (effect_bitmask & EFFECT_FLAG_DAZE)
		RemoveDazeSpell(spell);
	if (effect_bitmask & EFFECT_FLAG_FEAR)
		RemoveFearSpell(spell);
	if (effect_bitmask & EFFECT_FLAG_SPELLBONUS)
		RemoveSpellBonus(spell);
	if (effect_bitmask & EFFECT_FLAG_SKILLBONUS)
		RemoveSkillBonus(spell->spell->GetSpellID());
	if (effect_bitmask & EFFECT_FLAG_STEALTH)
		RemoveStealthSpell(spell);
	if (effect_bitmask & EFFECT_FLAG_INVIS)
		RemoveInvisSpell(spell);
	if (effect_bitmask & EFFECT_FLAG_SNARE)
		RemoveSnareSpell(spell);
	if (effect_bitmask & EFFECT_FLAG_WATERWALK)
		RemoveWaterwalkSpell(spell);
	if (effect_bitmask & EFFECT_FLAG_WATERJUMP)
		RemoveWaterjumpSpell(spell);
	if (effect_bitmask & EFFECT_FLAG_FLIGHT)
		RemoveFlightSpell(spell);
	if (effect_bitmask & EFFECT_FLAG_GLIDE)
		RemoveGlideSpell(spell);
	if (effect_bitmask & EFFECT_FLAG_AOE_IMMUNE)
		RemoveAOEImmunity(spell);
	if (effect_bitmask & EFFECT_FLAG_STUN_IMMUNE)
		RemoveStunImmunity(spell);
	if (effect_bitmask & EFFECT_FLAG_MEZ_IMMUNE)
		RemoveMezImmunity(spell);
	if (effect_bitmask & EFFECT_FLAG_DAZE_IMMUNE)
		RemoveDazeImmunity(spell);
	if (effect_bitmask & EFFECT_FLAG_ROOT_IMMUNE)
		RemoveRootImmunity(spell);
	if (effect_bitmask & EFFECT_FLAG_STIFLE_IMMUNE)
		RemoveStifleImmunity(spell);
	if (effect_bitmask & EFFECT_FLAG_FEAR_IMMUNE)
		RemoveFearImmunity(spell);
	if (effect_bitmask & EFFECT_FLAG_SAFEFALL)
		RemoveSafefallSpell(spell);
}

void Entity::RemoveSkillBonus(int32 spell_id){
	//This is a virtual, just making it so we don't have to do extra checks for player/npcs
	return;
}

void Entity::AddFlightSpell(LuaSpell* spell){
	if (!spell)
		return;

	if (!control_effects[CONTROL_EFFECT_TYPE_FLIGHT])
		control_effects[CONTROL_EFFECT_TYPE_FLIGHT] = new MutexList<LuaSpell*>;

	if (IsPlayer() && control_effects[CONTROL_EFFECT_TYPE_FLIGHT]->size(true) == 0)
		((Player*)this)->SetPlayerControlFlag(5, 32, true);

	control_effects[CONTROL_EFFECT_TYPE_FLIGHT]->Add(spell);
}

void Entity::RemoveFlightSpell(LuaSpell* spell){
	MutexList<LuaSpell*>* flight_list = control_effects[CONTROL_EFFECT_TYPE_FLIGHT];
	if (!flight_list || flight_list->size(true) == 0)
		return;

	flight_list->Remove(spell);
	if (IsPlayer() && flight_list->size(true) == 0)
		((Player*)this)->SetPlayerControlFlag(5, 32, false);
}

void Entity::AddGlideSpell(LuaSpell* spell){
	if (!spell)
		return;

	if (!control_effects[CONTROL_EFFECT_TYPE_GLIDE])
		control_effects[CONTROL_EFFECT_TYPE_GLIDE] = new MutexList<LuaSpell*>;

	if (IsPlayer() && control_effects[CONTROL_EFFECT_TYPE_GLIDE]->size(true) == 0)
		((Player*)this)->SetPlayerControlFlag(4, 16, true);

	control_effects[CONTROL_EFFECT_TYPE_GLIDE]->Add(spell);
}

void Entity::RemoveGlideSpell(LuaSpell* spell){
	MutexList<LuaSpell*>* glide_list = control_effects[CONTROL_EFFECT_TYPE_GLIDE];
	if (!glide_list || glide_list->size(true) == 0)
		return;

	glide_list->Remove(spell);
	if (IsPlayer() && glide_list->size(true) == 0)
		((Player*)this)->SetPlayerControlFlag(4, 16, false);
}

void Entity::AddSafefallSpell(LuaSpell* spell){
	if (!spell)
		return;

	if (!control_effects[CONTROL_EFFECT_TYPE_SAFEFALL])
		control_effects[CONTROL_EFFECT_TYPE_SAFEFALL] = new MutexList<LuaSpell*>;

	if (IsPlayer() && control_effects[CONTROL_EFFECT_TYPE_SAFEFALL]->size(true) == 0)
		((Player*)this)->SetPlayerControlFlag(4, 32, true);

	control_effects[CONTROL_EFFECT_TYPE_SAFEFALL]->Add(spell);
}

void Entity::RemoveSafefallSpell(LuaSpell* spell){
	MutexList<LuaSpell*>* safe_list = control_effects[CONTROL_EFFECT_TYPE_SAFEFALL];
	if (!safe_list || safe_list->size(true) == 0)
		return;

	safe_list->Remove(spell);
	if (IsPlayer() && safe_list->size(true) == 0)
		((Player*)this)->SetPlayerControlFlag(4, 32, false);
}

void Entity::UpdateGroupMemberInfo() {
	if (!group_member_info)
		return;

	group_member_info->class_id = GetAdventureClass();
	group_member_info->hp_max = GetTotalHP();
	group_member_info->hp_current = GetHP();
	group_member_info->level_max = GetLevel();
	group_member_info->level_current = GetLevel();
	group_member_info->name = string(GetName());
	group_member_info->power_current = GetPower();
	group_member_info->power_max = GetTotalPower();
	group_member_info->race_id = GetRace();
	if (GetZone())
		group_member_info->zone = GetZone()->GetZoneDescription();
	else
		group_member_info->zone = "Unknown";
}