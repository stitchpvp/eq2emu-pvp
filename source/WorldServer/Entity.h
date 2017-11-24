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
#ifndef __EQ2_ENTITY__
#define __EQ2_ENTITY__
#include "Spawn.h"
#include "../common/Mutex.h"
#include "Skills.h"
#include "MutexList.h"
#include "MutexVector.h"


using namespace std;

class Entity;
class NPC;
struct LuaSpell;
struct GroupMemberInfo;

struct BonusValues{
	int32	spell_id;
	int8    tier;
	int16	type;
	sint32	value;
	int64	class_req;
	LuaSpell* luaspell;
};

struct MaintainedEffects{
	char	name[60]; //name of the spell
	int32	target;
	int8	target_type;
	int32	spell_id; 
	int32	slot_pos; 
	int16	icon;
	int16	icon_backdrop;
	int8	conc_used;
	int8	tier;
	float	total_time;
	int32	expire_timestamp;
	LuaSpell* spell;
};

struct SpellEffects{
	int32	spell_id; 
	Entity*	caster;
	float	total_time;
	int32	expire_timestamp;
	int16	icon;
	int16	icon_backdrop;
	int8	tier;
	LuaSpell* spell;
};

struct DetrimentalEffects {
	int32	spell_id; 
	Entity*	caster;
	int32	expire_timestamp;
	int16	icon;
	int16	icon_backdrop;
	int8	tier;
	int8    det_type;
	bool    incurable;
	LuaSpell*  spell;
	int8    control_effect;
	float   total_time;
};

struct CombatData{
	int32			range_last_attack_time;
	int32			primary_last_attack_time;
	int32			secondary_last_attack_time;
	int16			primary_attack_delay;
	int16			secondary_attack_delay;
	int16			ranged_attack_delay;
	int8			primary_weapon_type;
	int8			secondary_weapon_type;
	int8			ranged_weapon_type;
	int32			primary_weapon_damage_low;
	int32			primary_weapon_damage_high;
	int32			secondary_weapon_damage_low;
	int32			secondary_weapon_damage_high;
	int32			ranged_weapon_damage_low;
	int32			ranged_weapon_damage_high;
	int8			wield_type;
	int16           primary_weapon_delay;
	int16           secondary_weapon_delay;
	int16           ranged_weapon_delay;
};

struct InfoStruct{
	char			name[40];
	int8			class1;
	int8			class2;
	int8			class3;
	int8			race;
	int8			gender;
	int16			level;
	int16			max_level;
	int16			tradeskill_level;
	int16			tradeskill_max_level;
	
	int8			cur_concentration;
	int8			max_concentration;
	int16			cur_attack;
	int16			attack_base;
	int16			cur_mitigation;
	int16			max_mitigation;
	int16			mitigation_base;
	int16			avoidance_display;
	int16			cur_avoidance;
	int16			base_avoidance_pct;
	int16			avoidance_base;
	int16			max_avoidance;
	int16			parry;
	int16			parry_base;
	int16			deflection;
	int16			deflection_base;
	int16			block;
	int16			block_base;
	int16			str;
	int16			sta;
	int16			agi;
	int16			wis;
	int16			intel;
	int16			str_base;
	int16			sta_base;
	int16			agi_base;
	int16			wis_base;
	int16			intel_base;
	int16			heat;
	int16			cold;
	int16			magic;
	int16			mental;
	int16			divine;
	int16			disease;
	int16			poison;
	int16			disease_base;
	int16			cold_base;
	int16			divine_base;
	int16			magic_base;
	int16			mental_base;
	int16			heat_base;
	int16			poison_base;
	int16			elemental_base;
	int16			noxious_base;
	int16			arcane_base;
	int32			coin_copper;
	int32			coin_silver;
	int32			coin_gold;
	int32			coin_plat;
	int32			bank_coin_copper;
	int32			bank_coin_silver;
	int32			bank_coin_gold;
	int32			bank_coin_plat;
	
	int32			status_points;
	char			deity[32];
	int32			weight;
	int32			max_weight;
	SpellEffects	spell_effects[30];
	MaintainedEffects maintained_effects[30];
	int8			tradeskill_class1;
	int8			tradeskill_class2;
	int8			tradeskill_class3;
	int8			account_age_base;
	int8			account_age_bonus[19];
	int16			absorb;
	int32			xp;
	int32			xp_needed;
	int32			xp_debt;
	int16			xp_yellow;
	int16			xp_yellow_vitality_bar;
	int16			xp_blue_vitality_bar;
	int16			xp_blue;
	int32			ts_xp;
	int32			ts_xp_needed;
	int16			tradeskill_exp_yellow;
	int16			tradeskill_exp_blue;
	int32			flags;
	int32			flags2;
	float			xp_vitality;
	float			tradeskill_xp_vitality;
	int16			mitigation_skill1;
	int16			mitigation_skill2;
	int16			mitigation_skill3;
	float			ability_modifier;
	float			critical_mitigation;
	float			block_chance;
	float			crit_chance;
	float			crit_bonus;
	float			potency;
	float			hate_mod;
	float			reuse_speed;
	float			casting_speed;
	float			recovery_speed;
	float			spell_reuse_speed;
	float			spell_multi_attack;
	float			dps;
	float           dps_multiplier;
	float			attackspeed;
	float			haste;
	float			multi_attack;
	float			flurry;
	float			melee_ae;
	float			strikethrough;
	float			accuracy;
	float			offensivespeed;
	float			base_avoidance_bonus;
	float			minimum_deflection_chance;
	float			rain;
	float			wind;
	sint8			physical_damage_reduction;
	sint8			alignment;
	int16			fame;

	int32			pet_id;
	char			pet_name[32];
	float			pet_health_pct;
	float			pet_power_pct;
	int8			pet_movement;
	int8			pet_behavior;

	int8          	vision;
	int8			breathe_underwater;
	char			biography[512];
	float			drunk;
};

struct WardInfo {
	LuaSpell*	Spell;
	int32		BaseDamage;
	int32		DamageLeft;
	int8		WardType;
	int8		DamageType;
	bool		keepWard;
};

#define WARD_TYPE_ALL 0
#define WARD_TYPE_PHYSICAL 1
#define WARD_TYPE_MAGICAL 2

struct Proc {
	LuaSpell*	spell;
	Item*		item;
	float		chance;
};

#define PROC_TYPE_OFFENSIVE				1
#define PROC_TYPE_DEFENSIVE				2
#define PROC_TYPE_PHYSICAL_OFFENSIVE	3
#define PROC_TYPE_PHYSICAL_DEFENSIVE	4
#define PROC_TYPE_MAGICAL_OFFENSIVE		5
#define PROC_TYPE_MAGICAL_DEFENSIVE		6
#define PROC_TYPE_BLOCK					7
#define PROC_TYPE_PARRY					8
#define PROC_TYPE_RIPOSTE				9
#define PROC_TYPE_EVADE					10
#define PROC_TYPE_HEALING				11
#define PROC_TYPE_BENEFICIAL			12
#define PROC_TYPE_DEATH                 13
#define PROC_TYPE_KILL                  14
#define PROC_TYPE_DAMAGED               15
#define PROC_TYPE_DAMAGED_MELEE         16
#define PROC_TYPE_DAMAGED_MAGIC         17
#define PROC_TYPE_RANGED_ATTACK			18
#define PROC_TYPE_RANGED_DEFENSE		19

struct ThreatTransfer {
	int32		Target;
	float		Amount;
	LuaSpell*	Spell;
};

#define DET_TYPE_TRAUMA      1
#define DET_TYPE_ARCANE      2
#define DET_TYPE_NOXIOUS     3
#define DET_TYPE_ELEMENTAL   4
#define DET_TYPE_CURSE       5

#define DISPELL_TYPE_CURE    0
#define DISPELL_TYPE_DISPELL 1

#define CONTROL_EFFECT_TYPE_MEZ 1
#define CONTROL_EFFECT_TYPE_STIFLE 2
#define CONTROL_EFFECT_TYPE_DAZE 3
#define CONTROL_EFFECT_TYPE_STUN 4
#define CONTROL_EFFECT_TYPE_ROOT 5
#define CONTROL_EFFECT_TYPE_FEAR 6
#define CONTROL_EFFECT_TYPE_WALKUNDERWATER 7
#define CONTROL_EFFECT_TYPE_JUMPUNDERWATER 8
#define CONTROL_EFFECT_TYPE_INVIS 9
#define CONTROL_EFFECT_TYPE_STEALTH 10
#define CONTROL_EFFECT_TYPE_SNARE 11
#define CONTROL_EFFECT_TYPE_FLIGHT 12
#define CONTROL_EFFECT_TYPE_GLIDE 13
#define CONTROL_EFFECT_TYPE_SAFEFALL 14

#define IMMUNITY_TYPE_MEZ 1
#define IMMUNITY_TYPE_STIFLE 2
#define IMMUNITY_TYPE_DAZE 3
#define IMMUNITY_TYPE_STUN 4
#define IMMUNITY_TYPE_ROOT 5
#define IMMUNITY_TYPE_FEAR 6
#define IMMUNITY_TYPE_AOE 7

//class Spell;
//class ZoneServer;

//The entity class is for NPCs and Players, spawns which are able to fight
class Entity : public Spawn{
public:
	Entity();
	virtual ~Entity();
	virtual float GetShieldBlockChance();
	virtual float GetDodgeChance();
	virtual void AddMaintainedSpell(LuaSpell* spell);
	virtual void AddSpellEffect(LuaSpell* spell);
	virtual void RemoveMaintainedSpell(LuaSpell* spell);
	virtual void RemoveSpellEffect(LuaSpell* spell);
	virtual bool HasActiveMaintainedSpell(Spell* spell, Spawn* target);
	virtual bool HasActiveSpellEffect(Spell* spell, Spawn* target);
	void AddDetrimentalSpell(LuaSpell* spell);
	DetrimentalEffects* GetDetrimentalEffect(int32 spell_id, Entity* caster);
	virtual MaintainedEffects* GetMaintainedSpell(int32 spell_id);
	void RemoveDetrimentalSpell(LuaSpell* spell);
	void	SetDeity(int8 new_deity){
			deity = new_deity;
	}
	int8	GetDeity(){ return deity; }
	EquipmentItemList* GetEquipmentList();

	bool IsEntity(){ return true; }
	void CalculateBonuses();
	float CalculateBonusMod();
	float CalculateDPSMultiplier();
	float CalculateCastingSpeedMod();

	InfoStruct* GetInfoStruct();

	int16	GetStr();
	int16	GetSta();
	int16	GetInt();
	int16	GetWis();
	int16	GetAgi();
	int16   GetPrimaryStat();

	int16	GetHeatResistance();
	int16	GetColdResistance();
	int16	GetMagicResistance();
	int16	GetMentalResistance();
	int16	GetDivineResistance();
	int16	GetDiseaseResistance();
	int16	GetPoisonResistance();

	int16	GetStrBase();
	int16	GetStaBase();
	int16	GetIntBase();
	int16	GetWisBase();
	int16	GetAgiBase();

	int16	GetHeatResistanceBase();
	int16	GetColdResistanceBase();
	int16	GetMagicResistanceBase();
	int16	GetMentalResistanceBase();
	int16	GetDivineResistanceBase();
	int16	GetDiseaseResistanceBase();
	int16	GetPoisonResistanceBase();

	int8	GetConcentrationCurrent();
	int8	GetConcentrationMax();

	sint8	GetAlignment();
	void	SetAlignment(sint8 new_value);

	bool	HasMoved(bool include_heading);
	void	SetHPRegen(int16 new_val);
	void	SetPowerRegen(int16 new_val);
	int16	GetHPRegen();
	int16	GetPowerRegen();
	void	DoRegenUpdate();
	MaintainedEffects* GetFreeMaintainedSpellSlot();
	SpellEffects* GetFreeSpellEffectSlot();
	SpellEffects* GetSpellEffect(int32 id, Entity* caster = 0);

	//flags
	int32 GetFlags() { return info_struct.flags; }
	int32	GetFlags2() { return info_struct.flags2; }
	bool query_flags(int flag) {
			if (flag > 63) return false;
			if (flag < 32) return ((info_struct.flags & (1 << flag))?true:false);
			return ((info_struct.flags2 & (1 << (flag - 32)))?true:false);
	}
	float	GetMaxSpeed();
	void	SetMaxSpeed(float val);
	//combat stuff:
	int32	GetRangeLastAttackTime();
	void	SetRangeLastAttackTime(int32 time);
	int16	GetRangeAttackDelay();
	int16   GetRangeWeaponDelay() {return ranged_combat_data.ranged_weapon_delay;}
	void    SetRangeWeaponDelay(int16 new_delay) {ranged_combat_data.ranged_weapon_delay = new_delay * 100;}
	void    SetRangeAttackDelay(int16 new_delay) {ranged_combat_data.ranged_attack_delay = new_delay;}
	int32	GetPrimaryLastAttackTime();
	int16	GetPrimaryAttackDelay();
	void	SetPrimaryAttackDelay(int16 new_delay);
	void	SetPrimaryLastAttackTime(int32 new_time);
	void    SetPrimaryWeaponDelay(int16 new_delay) {melee_combat_data.primary_weapon_delay = new_delay * 100;}
	int32	GetSecondaryLastAttackTime();
	int16	GetSecondaryAttackDelay();
	void	SetSecondaryAttackDelay(int16 new_delay);
	void	SetSecondaryLastAttackTime(int32 new_time);
	void    SetSecondaryWeaponDelay(int16 new_delay) {melee_combat_data.primary_weapon_delay = new_delay * 100;}
	int32	GetPrimaryWeaponMinDamage();
	int32	GetPrimaryWeaponMaxDamage();
	int32	GetSecondaryWeaponMinDamage();
	int32	GetSecondaryWeaponMaxDamage();
	int32	GetRangedWeaponMinDamage();
	int32	GetRangedWeaponMaxDamage();
	int8	GetPrimaryWeaponType();
	int8	GetSecondaryWeaponType();
	int8	GetRangedWeaponType();
	int8	GetWieldType();
	int16   GetPrimaryWeaponDelay() {return melee_combat_data.primary_weapon_delay;}
	int16   GetSecondaryWeaponDelay() {return melee_combat_data.secondary_weapon_delay;}
	bool	IsDualWield();
	bool	BehindTarget(Spawn* target);
	bool	FlankingTarget(Spawn* target);
	void	ChangePrimaryWeapon();
	void	ChangeSecondaryWeapon();
	void	ChangeRangedWeapon();
	virtual Skill*	GetSkillByName(const char* name, bool check_update = false);
	bool			AttackAllowed(Entity* target, float distance = 0, bool range_attack = false);
	bool			PrimaryWeaponReady();
	bool			SecondaryWeaponReady();
	bool			RangeWeaponReady();
	void			MeleeAttack(Spawn* victim, float distance, bool primary, bool multi_attack = false);
	void			RangeAttack(Spawn* victim, float distance, Item* weapon, Item* ammo, bool multi_attack = false);
	bool			SpellAttack(Spawn* victim, float distance, LuaSpell* luaspell, int8 damage_type, int32 low_damage, int32 high_damage, int8 crit_mod = 0, bool no_calcs = false);
	bool			ProcAttack(Spawn* victim, int8 damage_type, int32 low_damage, int32 high_damage, string name, string success_msg, string effect_msg);
	bool			ProcHeal(Spawn* victim, string heal_type, int32 low_heal, int32 high_heal, string name);
	bool            SpellHeal(Spawn* target, float distance, LuaSpell* luaspell, string heal_type, int32 low_heal, int32 high_heal, int8 crit_mod = 0, bool no_calcs = false);
	int8			DetermineHit(Spawn* victim, int8 damage_type, float ToHitBonus, bool spell);
	float			GetDamageTypeResistPercentage(int8 damage_type);
	Skill*			GetSkillByWeaponType(int8 type, bool update);
	bool			DamageSpawn(Entity* victim, int8 type, int8 damage_type, int32 low_damage, int32 high_damage, const char* spell_name, int8 crit_mod = 0, bool is_tick = false, bool no_damage_calcs = false);
	void			AddHate(Entity* attacker, sint32 hate);
	bool			CheckInterruptSpell(Entity* attacker);
	void			KillSpawn(Spawn* dead, int8 damage_type = 0, int16 kill_blow_type = 0);
	void            SetAttackDelay(bool primary = false, bool ranged = false);
	float           CalculateAttackSpeedMod();
	virtual void	ProcessCombat();

	bool	EngagedInCombat();
	virtual void	InCombat(bool val);

	bool	IsCasting();
	void	IsCasting(bool val);
	bool HasLoot(){
		if(loot_items.size() == 0 && loot_coins == 0)
			return false;
		return true;
	}
	int32 GetLootItemID();
	Item*	LootItem(int32 id);
	vector<Item*>* GetLootItems(){
		return &loot_items;
	}
	void LockLoot(){
		MLootItems.lock();
	}
	void UnlockLoot(){
		MLootItems.unlock();
	}
	int32 GetLootCoins(){
		return loot_coins;
	}
	void SetLootCoins(int32 val){
		loot_coins = val;
	}
	void AddLootCoins(int32 coins){
		loot_coins += coins;
	}
	void AddLootItem(int32 id, int16 charges = 1){
		Item* master_item = master_item_list.GetItem(id);
		if(master_item){
			Item* item = new Item(master_item);
			item->details.count = charges;
			loot_items.push_back(item);
		}
	}
	void SetMount(int16 val, bool setUpdateFlags = true) {
		if(val == 0){
			EQ2_Color color;
			color.red = 0;
			color.green = 0;
			color.blue = 0;
			SetMountColor(&color);
			SetMountSaddleColor(&color);
		}
		SetInfo(&features.mount_model_type, val, setUpdateFlags); 
	}
	void SetEquipment(Item* item, int8 slot = 255);
	void SetEquipment(int8 slot, int16 type, int8 red, int8 green, int8 blue, int8 h_r, int8 h_g, int8 h_b){
		SetInfo(&equipment.equip_id[slot], type);
		SetInfo(&equipment.color[slot].red, red);
		SetInfo(&equipment.color[slot].green, green);
		SetInfo(&equipment.color[slot].blue, blue);
		SetInfo(&equipment.highlight[slot].red, h_r);
		SetInfo(&equipment.highlight[slot].green, h_g);
		SetInfo(&equipment.highlight[slot].blue, h_b);
	}
	void SetHairType(int16 new_val, bool setUpdateFlags = true){
		SetInfo(&features.hair_type, new_val, setUpdateFlags);
	}
	void SetHairColor(EQ2_Color new_val, bool setUpdateFlags = true){
		SetInfo(&features.hair_type_color, new_val, setUpdateFlags);
	}
	void SetHairHighlightColor(EQ2_Color new_val, bool setUpdateFlags = true){
		SetInfo(&features.hair_type_highlight_color, new_val, setUpdateFlags);
	}
	void SetFacialHairType(int16 new_val, bool setUpdateFlags = true){
		SetInfo(&features.hair_face_type, new_val, setUpdateFlags);
	}
	void SetFacialHairColor(EQ2_Color new_val, bool setUpdateFlags = true){
		SetInfo(&features.hair_face_color, new_val, setUpdateFlags);
	}
	void SetFacialHairHighlightColor(EQ2_Color new_val, bool setUpdateFlags = true){
		SetInfo(&features.hair_face_highlight_color, new_val, setUpdateFlags);
	}
	void SetWingType(int16 new_val, bool setUpdateFlags = true){
		SetInfo(&features.wing_type, new_val, setUpdateFlags);
	}
	void SetWingColor1(EQ2_Color new_val, bool setUpdateFlags = true){
		SetInfo(&features.wing_color1, new_val, setUpdateFlags);
	}
	void SetWingColor2(EQ2_Color new_val, bool setUpdateFlags = true){
		SetInfo(&features.wing_color2, new_val, setUpdateFlags);
	}
	void SetChestType(int16 new_val, bool setUpdateFlags = true){
		SetInfo(&features.chest_type, new_val, setUpdateFlags);
	}
	void SetLegsType(int16 new_val, bool setUpdateFlags = true){
		SetInfo(&features.legs_type, new_val, setUpdateFlags);
	}
	void SetSogaHairType(int16 new_val, bool setUpdateFlags = true){
		SetInfo(&features.soga_hair_type, new_val, setUpdateFlags);
	}	
	void SetSogaFacialHairType(int16 new_val, bool setUpdateFlags = true){
		SetInfo(&features.soga_hair_face_type, new_val, setUpdateFlags);
	}	
	void SetSogaChestType(int16 new_val, bool setUpdateFlags = true){
		SetInfo(&features.soga_chest_type, new_val, setUpdateFlags);
	}	
	void SetSogaLegType(int16 new_val, bool setUpdateFlags = true){
		SetInfo(&features.soga_legs_type, new_val, setUpdateFlags);
	}	
	void SetSkinColor(EQ2_Color* color){
		SetInfo(&features.skin_color, *color);
	}
	void SetCombatVoice(int16 val, bool setUpdateFlags = true) { 
		SetInfo(&features.combat_voice, val, setUpdateFlags); 
	}
	void SetEmoteVoice(int16 val, bool setUpdateFlags = true) { 
		SetInfo(&features.emote_voice, val, setUpdateFlags); 
	}
	int16 GetCombatVoice(){ return features.combat_voice; }
	int16 GetEmoteVoice(){ return features.emote_voice; }
	int16 GetMount(){ return features.mount_model_type; }
	void SetMountSaddleColor(EQ2_Color* color){
		SetInfo(&features.mount_saddle_color, *color);
	}
	void SetMountColor(EQ2_Color* color){
		SetInfo(&features.mount_color, *color);
	}
	void SetEyeColor(EQ2_Color* eye_color){
		SetInfo(&features.eye_color, *eye_color);
	}
	int16 GetHairType(){
		return features.hair_type;
	}
	int16 GetFacialHairType(){
		return features.hair_face_type;
	}
	int16 GetWingType(){
		return features.wing_type;
	}
	int16 GetChestType(){
		return features.chest_type;
	}
	int16 GetLegsType(){
		return features.legs_type;
	}
	int16 GetSogaHairType(){
		return features.soga_hair_type;
	}
	int16 GetSogaFacialHairType(){
		return features.soga_hair_face_type;
	}
	int16 GetSogaChestType(){
		return features.soga_chest_type;
	}
	int16 GetSogaLegType(){
		return features.soga_legs_type;
	}
	EQ2_Color* GetSkinColor(){
		return &features.skin_color;
	}
	EQ2_Color* GetEyeColor(){
		return &features.eye_color;
	}
	EQ2_Color* GetMountSaddleColor(){
		return &features.mount_saddle_color;
	}
	EQ2_Color* GetMountColor(){
		return &features.mount_color;
	}	
	EQ2_Equipment	equipment;
	CharFeatures	features;	

	void AddSpellBonus(LuaSpell* spell, int16 type, sint32 value, int64 class_req = 0);
	BonusValues* GetSpellBonus(int32 spell_id);
	vector<BonusValues*>* GetAllSpellBonuses(LuaSpell* spell);
	bool CheckSpellBonusRemoval(LuaSpell* spell, int16 type);
	void RemoveSpellBonus(LuaSpell* spell);
	void CalculateSpellBonuses(ItemStatsValues* stats);
	void AddMezSpell(LuaSpell* spell);
	void RemoveMezSpell(LuaSpell* spell);
	void RemoveAllMezSpells();
	bool IsMezzed();
	void AddStifleSpell(LuaSpell* spell);
	void RemoveStifleSpell(LuaSpell* spell);
	bool IsStifled();
	void AddDazeSpell(LuaSpell* spell);
	void RemoveDazeSpell(LuaSpell* spell);
	bool IsDazed();
	void AddStunSpell(LuaSpell* spell);
	void RemoveStunSpell(LuaSpell* spell);
	bool IsStunned();
	bool IsMezzedOrStunned() {return IsMezzed() || IsStunned();}
	void AddRootSpell(LuaSpell* spell);
	void RemoveRootSpell(LuaSpell* spell);
	bool IsRooted();
	void AddFearSpell(LuaSpell* spell);
	void RemoveFearSpell(LuaSpell* spell);
	bool IsFeared();
	bool IsWarded();
	void AddSnareSpell(LuaSpell* spell);
	void RemoveSnareSpell(LuaSpell* spell);
	void SetSnareValue(LuaSpell* spell, float snare_val);
	bool IsSnared();
	float GetHighestSnare();


	void SetCombatPet(Entity* pet) { this->pet = pet; }
	void SetCharmedPet(Entity* pet) { charmedPet = pet; }
	void SetDeityPet(Entity* pet) { deityPet = pet; }
	void SetCosmeticPet(Entity* pet) { cosmeticPet = pet; }
	Entity* GetPet() { return pet; }
	Entity* GetCharmedPet() { return charmedPet; }
	Entity* GetDeityPet() { return deityPet; }
	Entity* GetCosmeticPet() { return cosmeticPet; }
	/// <summary>Check to see if the entity has a combat pet</summary>
	/// <returns>True if the entity has a combat pet</returns>
	bool HasPet() { return (pet || charmedPet) ? true : false; }

	void HideDeityPet(bool val);
	void HideCosmeticPet(bool val);
	void DismissPet(NPC* pet, bool from_death = false);

	/// <summary>Creates a loot chest to drop in the world</summary>
	/// <returns>Pointer to the chest</returns>
	NPC* DropChest();

	/// <summary>Add a ward to the entities ward list</summary>
	/// <param name='spellID'>Spell id of the ward to add</param>
	/// <param name='ward'>WardInfo* of the ward we are adding</parma>
	void AddWard(LuaSpell* luaspell, WardInfo* ward);

	/// <summary>Gets ward info for the given spell id</summary>
	/// <param name='spellID'>The spell id of the ward we want to get</param>
	/// <returns>WardInfo for the given spell id</returns>
	WardInfo* GetWard(LuaSpell * luaspell);

	/// <summary>Removes the ward with the given spell id</summary>
	/// <param name='spellID'>The spell id of the ward to remove</param>
	void RemoveWard(LuaSpell* luaspell);

	/// <summary>Subtracts the given damage from the wards</summary>
	/// <param name='damage'>The damage to subtract from the wards</param>
	/// <returns>The amount of damage left after wards</returns>
	int32 CheckWards(int32 damage, int8 damage_type);

	map<int16, float> stats;

	/// <summary>Adds a proc to the list of current procs</summary>
	/// <param name='type'>The type of proc to add</param>
	/// <param name='chance'>The percent chance the proc has to go off</param>
	/// <param name='item'>The item the proc is coming from if any</param>
	/// <param name='spell'>The spell the proc is coming from if any</param>
	void AddProc(int8 type, float chance, Item* item = 0, LuaSpell* spell = 0);

	/// <summary>Removes a proc from the list of current procs</summary>
	/// <param name='item'>Item the proc is from</param>
	/// <param name='spell'>Spell the proc is from</param>
	void RemoveProc(Item* item = 0, LuaSpell* spell = 0);

	/// <summary>Cycles through the proc list and executes them if they can go off</summary>
	/// <param name='type'>The proc type to check</param>
	/// <param name='target'>The target of the proc if it goes off</param>
	void CheckProcs(int8 type, Spawn* target);

	/// <summary>Clears the entire proc list</summary>
	void ClearProcs();

	float GetSpeed();
	float GetAirSpeed();
	float GetBaseSpeed() { return speed; }
	void SetSpeed(float val) { speed = val; }
	void SetSpeedMultiplier(float val) { speed_multiplier = val; }

	void SetThreatTransfer(ThreatTransfer* transfer) { m_threatTransfer = transfer; }
	ThreatTransfer* GetThreatTransfer() { return m_threatTransfer; }
	int8 GetTraumaCount();
	int8 GetArcaneCount();
	int8 GetNoxiousCount();
	int8 GetElementalCount();
	int8 GetCurseCount();
	int8 GetDetTypeCount(int8 det_type);
	int8 GetDetCount();
	bool HasCurableDetrimentType(int8 det_type);
	Mutex* GetDetrimentMutex();
	Mutex* GetMaintainedMutex();
	Mutex* GetSpellEffectMutex();
	void ClearAllDetriments();
	void CureDetrimentByType(int8 cure_count, int8 det_type, string cure_name, Entity* caster, int8 cure_level = 0);
	void CureDetrimentByControlEffect(int8 cure_count, int8 det_type, string cure_name, Entity* caster, int8 cure_level = 0);
	vector<DetrimentalEffects>* GetDetrimentalSpellEffects();
	void RemoveEffectsFromLuaSpell(LuaSpell* spell);
	virtual void RemoveSkillBonus(int32 spell_id);
	void CancelAllStealth();
	bool CanAttackTarget(Spawn* target);
	bool IsStealthed();
	bool IsInvis();
	void AddInvisSpell(LuaSpell* spell);
	void AddStealthSpell(LuaSpell* spell);
	void RemoveStealthSpell(LuaSpell* spell);
	void RemoveInvisSpell(LuaSpell* spell);
	void AddWaterwalkSpell(LuaSpell* spell);
	void AddWaterjumpSpell(LuaSpell* spell);
	void RemoveWaterwalkSpell(LuaSpell* spell);
	void RemoveWaterjumpSpell(LuaSpell* spell);
	void AddAOEImmunity(LuaSpell* spell);
	bool IsAOEImmune();
	void RemoveAOEImmunity(LuaSpell* spell);
	void AddStunImmunity(LuaSpell* spell);
	void RemoveStunImmunity(LuaSpell* spell);
	bool IsStunImmune();
	void AddStifleImmunity(LuaSpell* spell);
	void RemoveStifleImmunity(LuaSpell* spell);
	bool IsStifleImmune();
	void AddMezImmunity(LuaSpell* spell);
	void RemoveMezImmunity(LuaSpell* spell);
	bool IsMezImmune();
	void AddRootImmunity(LuaSpell* spell);
	void RemoveRootImmunity(LuaSpell* spell);
	bool IsRootImmune();
	void AddFearImmunity(LuaSpell* spell);
	void RemoveFearImmunity(LuaSpell* spell);
	bool IsFearImmune();
	void AddDazeImmunity(LuaSpell* spell);
	void RemoveDazeImmunity(LuaSpell* spell);
	bool IsDazeImmune();
	void AddFlightSpell(LuaSpell* spell);
	void RemoveFlightSpell(LuaSpell* spell);
	void AddSafefallSpell(LuaSpell* spell);
	void RemoveSafefallSpell(LuaSpell* spell);
	void AddGlideSpell(LuaSpell* spell);
	void RemoveGlideSpell(LuaSpell* spell);

	GroupMemberInfo* GetGroupMemberInfo() { return group_member_info; }
	void SetGroupMemberInfo(GroupMemberInfo* info) { group_member_info = info; }
	void UpdateGroupMemberInfo();

	float GetMitigationPercentage(int enemy_level) { return info_struct.cur_mitigation / ((40 * enemy_level) + info_struct.cur_mitigation + 200.0); }

protected:
	bool	in_combat;

private:
	MutexList<BonusValues*> bonus_list;
	map<int8, MutexList<LuaSpell*>*> control_effects;
	map<int8, MutexList<LuaSpell*>*> immunities;
	float	max_speed;
	vector<Item*>	loot_items;
	int32			loot_coins;
	int8	deity;
	sint16	regen_hp_rate;
	sint16	regen_power_rate;
	float	last_x;
	float	last_y;
	float	last_z;
	float	last_heading;
	bool	casting;
	bool has_secondary_weapon;
	InfoStruct		info_struct;
	CombatData melee_combat_data;
	CombatData ranged_combat_data;
	map<int8, int8> det_count_list;
	Mutex MDetriments;
	Mutex   MMaintainedSpells;
	Mutex   MSpellEffects;
	vector<DetrimentalEffects> detrimental_spell_effects;
	Mutex	MLootItems;
	// Pointers for the 4 types of pets (Summon, Charm, Deity, Cosmetic)
	Entity*	pet;
	Entity* charmedPet;
	Entity* deityPet;
	Entity* cosmeticPet;

	map<LuaSpell*, WardInfo*> m_wardList;

	// int8 = type, vector<Proc*> = list of pointers to proc info
	map <int8, vector<Proc*> > m_procList;
	Mutex MProcList;

	/// <summary>Actually calls the lua script to cast the proc</summary>
	/// <param name='proc'>Proc to be cast</param>
	/// <param name='type'>Type of proc going off</type>
	/// <param name='target'>Target of the proc</param>
	bool CastProc(Proc* proc, int8 type, Spawn* target);

	float speed;
	float speed_multiplier;

	map<LuaSpell*, float> snare_values;

	ThreatTransfer* m_threatTransfer;

	GroupMemberInfo* group_member_info;
};

#endif
