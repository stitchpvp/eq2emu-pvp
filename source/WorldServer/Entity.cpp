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
#include "classes.h"
#include "ClientPacketFunctions.h"
#include "PVP.h"
#include "Spells.h"
#include "SpellProcess.h"
#include "World.h"

extern World world;
extern MasterItemList master_item_list;
extern MasterSpellList master_spell_list;
extern Classes classes;

Entity::Entity() {
  max_speed = 6.0;
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
  trade = 0;
  deity = 0;
  MProcList.SetName("Entity::m_procList");
  MDetriments.SetName("Entity::MDetriments");
  MMaintainedSpells.SetName("Entity::MMaintainedSpells");
  MSpellEffects.SetName("Entity::MSpellEffects");
  m_procList.clear();

  {
    lock_guard<mutex> guard(control_effects_mutex);
    control_effects.clear();
  }

  {
    lock_guard<mutex> guard(immunity_effects_mutex);
    immunity_effects.clear();
  }

  has_secondary_weapon = false;

  for (int i = 0; i < NUM_SPELL_EFFECTS; i++) {
    info_struct.spell_effects[i].spell_id = 0xFFFFFFFF;
    if (IsPlayer()) {
      info_struct.spell_effects[i].icon = 0xFFFF;
    }
  }

  for (int i = 0; i < NUM_MAINTAINED_EFFECTS; i++) {
    info_struct.maintained_effects[i].spell_id = 0xFFFFFFFF;
    if (IsPlayer()) {
      info_struct.maintained_effects[i].icon = 0xFFFF;
    }
  }
}

Entity::~Entity() {
  for (auto item : loot_items) {
    safe_delete(item);
  }
  loot_items.clear();

  MutexList<BonusValues*>::iterator itr2 = bonus_list.begin();
  while (itr2.Next()) {
    safe_delete(itr2.value);
  }

  ClearProcs();
  safe_delete(m_threatTransfer);

  {
    lock_guard<mutex> guard(control_effects_mutex);
    control_effects.clear();
  }

  {
    lock_guard<mutex> guard(immunity_effects_mutex);
    immunity_effects.clear();
  }
}

bool Entity::HasMoved(bool include_heading) {
  if (GetX() == last_x && GetY() == last_y && GetZ() == last_z && ((!include_heading) || (include_heading && GetHeading() == last_heading)))
    return false;
  bool ret_val = true;
  if (last_x == -1 && last_y == -1 && last_z == -1 && last_heading == -1) {
    ret_val = false;
  }
  last_x = GetX();
  last_y = GetY();
  last_z = GetZ();
  last_heading = GetHeading();
  return ret_val;
}

int16 Entity::GetStr() {
  return GetInfoStruct()->str;
}

int16 Entity::GetSta() {
  return GetInfoStruct()->sta;
}

int16 Entity::GetInt() {
  return GetInfoStruct()->intel;
}

int16 Entity::GetWis() {
  return GetInfoStruct()->wis;
}

int16 Entity::GetAgi() {
  return GetInfoStruct()->agi;
}

int16 Entity::GetPrimaryStat() {
  int8 base_class = classes.GetBaseClass(GetAdventureClass());

  if (base_class == FIGHTER) {
    return GetInfoStruct()->str;
  } else if (base_class == PRIEST) {
    return GetInfoStruct()->wis;
  } else if (base_class == MAGE) {
    return GetInfoStruct()->intel;
  } else {
    return GetInfoStruct()->agi;
  }
}

int16 Entity::GetHeatResistance() {
  return GetInfoStruct()->heat;
}

int16 Entity::GetColdResistance() {
  return GetInfoStruct()->cold;
}

int16 Entity::GetMagicResistance() {
  return GetInfoStruct()->magic;
}

int16 Entity::GetMentalResistance() {
  return GetInfoStruct()->mental;
}

int16 Entity::GetDivineResistance() {
  return GetInfoStruct()->divine;
}

int16 Entity::GetDiseaseResistance() {
  return GetInfoStruct()->disease;
}

int16 Entity::GetPoisonResistance() {
  return GetInfoStruct()->poison;
}

int8 Entity::GetConcentrationCurrent() {
  return GetInfoStruct()->cur_concentration;
}

int8 Entity::GetConcentrationMax() {
  return GetInfoStruct()->max_concentration;
}

int16 Entity::GetStrBase() {
  return GetInfoStruct()->str_base;
}

int16 Entity::GetStaBase() {
  return GetInfoStruct()->sta_base;
}

int16 Entity::GetIntBase() {
  return GetInfoStruct()->intel_base;
}

int16 Entity::GetWisBase() {
  return GetInfoStruct()->wis_base;
}

int16 Entity::GetAgiBase() {
  return GetInfoStruct()->agi_base;
}

int16 Entity::GetHeatResistanceBase() {
  return GetInfoStruct()->heat_base;
}

int16 Entity::GetColdResistanceBase() {
  return GetInfoStruct()->cold_base;
}

int16 Entity::GetMagicResistanceBase() {
  return GetInfoStruct()->magic_base;
}

int16 Entity::GetMentalResistanceBase() {
  return GetInfoStruct()->mental_base;
}

int16 Entity::GetDivineResistanceBase() {
  return GetInfoStruct()->divine_base;
}

int16 Entity::GetDiseaseResistanceBase() {
  return GetInfoStruct()->disease_base;
}

int16 Entity::GetPoisonResistanceBase() {
  return GetInfoStruct()->poison_base;
}

void Entity::SetAlignment(sint8 new_value) {
  GetInfoStruct()->alignment = new_value;
}

sint8 Entity::GetAlignment() {
  return GetInfoStruct()->alignment;
}

bool Entity::IsCasting() {
  return casting;
}

void Entity::IsCasting(bool val) {
  casting = val;
}

int32 Entity::GetRangeLastAttackTime() {
  return ranged_combat_data.range_last_attack_time;
}

void Entity::SetRangeLastAttackTime(int32 time) {
  ranged_combat_data.range_last_attack_time = time;
}

int16 Entity::GetRangeAttackDelay() {
  return ranged_combat_data.ranged_attack_delay;
  //	if(IsPlayer()){
  //		Item* item = ((Player*)this)->GetEquipmentList()->GetItem(EQ2_RANGE_SLOT);
  //		if(item && item->IsRanged())
  //			return item->ranged_info->weapon_info.delay*100;
  //	}
  //	return 3000;
}

int32 Entity::GetPrimaryLastAttackTime() {
  return melee_combat_data.primary_last_attack_time;
}

int16 Entity::GetPrimaryAttackDelay() {
  return melee_combat_data.primary_attack_delay;
}

void Entity::SetPrimaryAttackDelay(int16 new_delay) {
  melee_combat_data.primary_attack_delay = new_delay;
}

void Entity::SetPrimaryLastAttackTime(int32 new_time) {
  melee_combat_data.primary_last_attack_time = new_time;
}

int32 Entity::GetSecondaryLastAttackTime() {
  return melee_combat_data.secondary_last_attack_time;
}

int16 Entity::GetSecondaryAttackDelay() {
  return melee_combat_data.secondary_attack_delay;
}

void Entity::SetSecondaryAttackDelay(int16 new_delay) {
  melee_combat_data.secondary_attack_delay = new_delay;
}

void Entity::SetSecondaryLastAttackTime(int32 new_time) {
  melee_combat_data.secondary_last_attack_time = new_time;
}

void Entity::ChangePrimaryWeapon() {
  Item* item = equipment_list.GetItem(EQ2_PRIMARY_SLOT);
  if (item && item->details.item_id > 0 && item->IsWeapon()) {
    melee_combat_data.primary_weapon_delay = item->weapon_info->delay * 100;
    melee_combat_data.primary_weapon_damage_low = item->weapon_info->damage_low3;
    melee_combat_data.primary_weapon_damage_high = item->weapon_info->damage_high3;
    melee_combat_data.primary_weapon_type = item->GetWeaponType();
    melee_combat_data.wield_type = item->weapon_info->wield_type;
  } else {
    double reducer = 10.0;

    if (IsNPC()) {
      if (IsPet()) {
        reducer = 7.2;

        if (static_cast<NPC*>(this)->GetPetType() == PET_TYPE_DUMBFIRE) {
          reducer = 12.0;
        }
      } else {
        reducer = 2.5 - (appearance.heroic_flag * 0.25);
      }
    }

    melee_combat_data.primary_weapon_delay = 1500;
    melee_combat_data.primary_weapon_damage_high = (int32)(5 + GetLevel() * (GetLevel() / reducer));

    if (IsNPC()) {
      melee_combat_data.primary_weapon_type = ((NPC*)this)->GetAttackType();

      if (GetEncounterLevel() > 6) {
        melee_combat_data.primary_weapon_damage_high *= (GetEncounterLevel() - 4) / 1.8;
      } else if (GetEncounterLevel() <= 6) {
        melee_combat_data.primary_weapon_damage_high *= GetEncounterLevel() / 6.0;
      }
    } else {
      melee_combat_data.primary_weapon_type = 1;
    }

    melee_combat_data.primary_weapon_damage_low = (int32)(melee_combat_data.primary_weapon_damage_high * 0.65);
    melee_combat_data.wield_type = 2;
  }
  if (IsNPC())
    melee_combat_data.primary_weapon_damage_high += (int32)(GetInfoStruct()->str / 10);
  else
    melee_combat_data.primary_weapon_damage_high += (int32)(GetInfoStruct()->str / 25);
}

void Entity::ChangeSecondaryWeapon() {
  Item* item = equipment_list.GetItem(EQ2_SECONDARY_SLOT);
  if (item && item->details.item_id > 0 && item->IsWeapon()) {
    melee_combat_data.secondary_weapon_delay = item->weapon_info->delay * 100;
    melee_combat_data.secondary_weapon_damage_low = item->weapon_info->damage_low3;
    melee_combat_data.secondary_weapon_damage_high = item->weapon_info->damage_high3;
    melee_combat_data.secondary_weapon_type = item->GetWeaponType();
    has_secondary_weapon = true;
  } else {
    has_secondary_weapon = false;

    double reducer = 10.0;

    if (IsNPC()) {
      if (IsPet()) {
        reducer = 7.2;

        if (static_cast<NPC*>(this)->GetPetType() == PET_TYPE_DUMBFIRE) {
          reducer = 12.0;
        }
      } else {
        reducer = 2.5 - (appearance.heroic_flag * 0.25);
      }
    }

    melee_combat_data.secondary_weapon_delay = 1500;
    melee_combat_data.secondary_weapon_damage_high = (int32)(5 + GetLevel() * (GetLevel() / reducer));
    melee_combat_data.secondary_weapon_type = 1;

    if (IsNPC()) {
      if (GetEncounterLevel() > 6) {
        melee_combat_data.secondary_weapon_damage_high *= (GetEncounterLevel() - 4) / 2.3;
      } else if (GetEncounterLevel() <= 6) {
        melee_combat_data.secondary_weapon_damage_high *= GetEncounterLevel() / 6.0;
      }
    }

    melee_combat_data.secondary_weapon_damage_low = (int32)(melee_combat_data.secondary_weapon_damage_high * 0.65);
  }

  if (IsNPC())
    melee_combat_data.secondary_weapon_damage_high += (int32)(GetInfoStruct()->str / 10);
  else
    melee_combat_data.secondary_weapon_damage_high += (int32)(GetInfoStruct()->str / 25);
}

void Entity::ChangeRangedWeapon() {
  Item* item = equipment_list.GetItem(EQ2_RANGE_SLOT);
  if (item && item->details.item_id > 0 && item->IsRanged()) {
    ranged_combat_data.ranged_weapon_delay = item->ranged_info->weapon_info.delay * 100;
    ranged_combat_data.ranged_weapon_damage_low = item->ranged_info->weapon_info.damage_low3;
    ranged_combat_data.ranged_weapon_damage_high = item->ranged_info->weapon_info.damage_high3;
    ranged_combat_data.ranged_weapon_type = item->GetWeaponType();
  }
}

int32 Entity::GetPrimaryWeaponMinDamage() {
  return melee_combat_data.primary_weapon_damage_low;
}

int32 Entity::GetPrimaryWeaponMaxDamage() {
  return melee_combat_data.primary_weapon_damage_high;
}

int32 Entity::GetSecondaryWeaponMinDamage() {
  return melee_combat_data.secondary_weapon_damage_low;
}

int32 Entity::GetSecondaryWeaponMaxDamage() {
  return melee_combat_data.secondary_weapon_damage_high;
}

int8 Entity::GetPrimaryWeaponType() {
  return melee_combat_data.primary_weapon_type;
}

int8 Entity::GetSecondaryWeaponType() {
  return melee_combat_data.secondary_weapon_type;
}

int32 Entity::GetRangedWeaponMinDamage() {
  return ranged_combat_data.ranged_weapon_damage_low;
}

int32 Entity::GetRangedWeaponMaxDamage() {
  return ranged_combat_data.ranged_weapon_damage_high;
}

int8 Entity::GetRangedWeaponType() {
  return ranged_combat_data.ranged_weapon_type;
}

bool Entity::IsDualWield() {
  if (has_secondary_weapon && (melee_combat_data.wield_type == 1 || melee_combat_data.wield_type == 2))
    return true;
  return false;
}

int8 Entity::GetWieldType() {
  return melee_combat_data.wield_type;
}

double Entity::SpawnAngle(Spawn* target) {
  double diff_x = -(GetX()) - -(target->GetX());
  double diff_z = GetZ() - target->GetZ();
  double diff_length = GetVectorLength(diff_x, diff_z);
  diff_x /= diff_length;
  diff_z /= diff_length;

  float heading = target->GetHeading();
  if (heading < 270) {
    heading += 90;
  } else {
    heading -= 270;
  }

  double dir_x = cos(heading * (3.14159 / 180.0));
  double dir_z = sin(heading * (3.14159 / 180.0));

  return GetDotProduct(dir_x, dir_z, diff_x, diff_z) * -1;
}

bool Entity::FacingTarget(Spawn* target) {
  if (!target->IsEntity()) {
    return true;
  }

  return (static_cast<Entity*>(target)->SpawnAngle(this) >= 0.9);
}

bool Entity::BehindTarget(Spawn* target) {
  double product = SpawnAngle(target);

  return (product < -0.75);
}

bool Entity::FlankingTarget(Spawn* target) {
  double product = SpawnAngle(target);

  return (product <= 0.45 && product > -0.75);
}

float Entity::GetShieldBlockChance() {
  float ret = 0;
  Item* item = equipment_list.GetItem(1);
  if (item && item->details.item_id > 0 && item->IsShield()) {
  }
  return ret;
}

float Entity::GetDodgeChance() {
  float ret = 0;

  return ret;
}

bool Entity::EngagedInCombat() {
  return in_combat;
}

void Entity::InCombat(bool val) {
  in_combat = val;
}

void Entity::SetHPRegen(int16 new_val) {
  regen_hp_rate = new_val;
}

void Entity::SetPowerRegen(int16 new_val) {
  regen_power_rate = new_val;
}

int16 Entity::GetHPRegen() {
  return regen_hp_rate;
}

int16 Entity::GetPowerRegen() {
  return regen_power_rate;
}

int16 Entity::GetTotalHPRegen() {
  int16 ret = regen_hp_rate;

  if (EngagedInCombat()) {
    ret *= 0.5;
    ret += stats[ITEM_STAT_COMBATHPREGEN];
  } else {
    ret += stats[ITEM_STAT_HPREGEN];
  }

  return ret;
}

int16 Entity::GetTotalPowerRegen() {
  int16 ret = regen_power_rate;

  if (EngagedInCombat()) {
    ret *= 0.5;
    ret += stats[ITEM_STAT_COMBATMANAREGEN];
  } else {
    ret += stats[ITEM_STAT_MANAREGEN];
  }

  return ret;
}

void Entity::DoRegenUpdate() {
  if (!Alive()) {
    return;
  }

  if (IsNPC() && EngagedInCombat()) {
    return;
  }

  if (GetHP() < GetTotalHP()) {
    int16 amount = GetTotalHPRegen();

    if ((GetHP() + amount) > GetTotalHP()) {
      SetHP(GetTotalHP());
    } else {
      SetHP(GetHP() + amount);
    }
  }

  if (GetPower() < GetTotalPower()) {
    int16 amount = GetTotalPowerRegen();

    if ((GetPower() + amount) > GetTotalPower()) {
      SetPower(GetTotalPower());
    } else {
      SetPower(GetPower() + amount);
    }
  }
}

void Entity::AddMaintainedSpell(shared_ptr<LuaSpell> luaspell) {
  if (!luaspell)
    return;

  Spell* spell = luaspell->spell;
  MaintainedEffects* effect = GetFreeMaintainedSpellSlot();

  if (effect) {
    MMaintainedSpells.writelock(__FUNCTION__, __LINE__);
    effect->spell = luaspell;
    effect->spell_id = spell->GetSpellData()->id;
    LogWrite(NPC__SPELLS, 5, "NPC", "AddMaintainedSpell Spell ID: %u", spell->GetSpellData()->id);
    effect->conc_used = spell->GetSpellData()->req_concentration;
    effect->total_time = spell->GetSpellDuration() / 10;
    effect->tier = spell->GetSpellData()->tier;
    if (spell->GetSpellData()->duration_until_cancel)
      effect->expire_timestamp = 0xFFFFFFFF;
    else
      effect->expire_timestamp = Timer::GetCurrentTime2() + (spell->GetSpellDuration() * 100);
    MMaintainedSpells.releasewritelock(__FUNCTION__, __LINE__);
  }
}

void Entity::AddSpellEffect(shared_ptr<LuaSpell> luaspell) {
  if (!luaspell || !luaspell->caster)
    return;

  Spell* spell = luaspell->spell;
  SpellEffects* old_effect = GetSpellEffect(spell->GetSpellID(), luaspell->caster);
  SpellEffects* effect = 0;
  if (old_effect) {
    GetZone()->RemoveTargetFromSpell(old_effect->spell, this);
    RemoveSpellEffect(old_effect->spell);
  }
  effect = GetFreeSpellEffectSlot();

  if (effect) {
    MSpellEffects.writelock(__FUNCTION__, __LINE__);
    effect->spell = luaspell;
    effect->spell_id = spell->GetSpellData()->id;
    effect->caster = luaspell->caster;
    effect->total_time = spell->GetSpellDuration() / 10;
    if (spell->GetSpellData()->duration_until_cancel)
      effect->expire_timestamp = 0xFFFFFFFF;
    else
      effect->expire_timestamp = Timer::GetCurrentTime2() + (spell->GetSpellDuration() * 100);
    effect->icon = spell->GetSpellData()->icon;
    effect->icon_backdrop = spell->GetSpellData()->icon_backdrop;
    effect->tier = spell->GetSpellTier();
    MSpellEffects.releasewritelock(__FUNCTION__, __LINE__);

    AddSpawnUpdate(true, false, false);
  }
}

void Entity::RemoveMaintainedSpell(shared_ptr<LuaSpell> luaspell) {
  if (!luaspell)
    return;

  bool found = false;
  MMaintainedSpells.writelock(__FUNCTION__, __LINE__);
  for (int i = 0; i < 30; i++) {
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

void Entity::RemoveSpellEffect(shared_ptr<LuaSpell> spell) {
  bool found = false;

  MSpellEffects.writelock(__FUNCTION__, __LINE__);

  for (int i = 0; i < NUM_SPELL_EFFECTS; i++) {
    if (found) {
      GetInfoStruct()->spell_effects[i - 1] = GetInfoStruct()->spell_effects[i];
    }

    if (GetInfoStruct()->spell_effects[i].spell == spell) {
      found = true;
    }
  }

  if (found) {
    memset(&GetInfoStruct()->spell_effects[NUM_SPELL_EFFECTS - 1], 0, sizeof(SpellEffects));
    GetInfoStruct()->spell_effects[NUM_SPELL_EFFECTS - 1].spell_id = 0xFFFFFFFF;
  }

  MSpellEffects.releasewritelock(__FUNCTION__, __LINE__);

  if (found) {
    AddSpawnUpdate(true, false, false);
  }
}

bool Entity::HasActiveMaintainedSpell(Spell* spell, Spawn* target) {
  LogWrite(MISC__TODO, 1, "TODO", "This does nothing... yet...\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
  return false;
}

bool Entity::HasActiveSpellEffect(Spell* spell, Spawn* target) {
  LogWrite(MISC__TODO, 1, "TODO", "This does nothing... yet...\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
  return false;
}

MaintainedEffects* Entity::GetFreeMaintainedSpellSlot() {
  MaintainedEffects* ret = 0;
  InfoStruct* info = GetInfoStruct();
  MMaintainedSpells.readlock(__FUNCTION__, __LINE__);
  for (int i = 0; i < NUM_MAINTAINED_EFFECTS; i++) {
    if (info->maintained_effects[i].spell_id == 0xFFFFFFFF) {
      ret = &info->maintained_effects[i];
      ret->spell_id = 0;
      ret->slot_pos = i;
      break;
    }
  }
  MMaintainedSpells.releasereadlock(__FUNCTION__, __LINE__);
  return ret;
}

MaintainedEffects* Entity::GetMaintainedSpell(int32 spell_id) {
  MaintainedEffects* ret = 0;
  InfoStruct* info = GetInfoStruct();
  MMaintainedSpells.readlock(__FUNCTION__, __LINE__);
  for (int i = 0; i < NUM_MAINTAINED_EFFECTS; i++) {
    if (info->maintained_effects[i].spell_id == spell_id) {
      ret = &info->maintained_effects[i];
      break;
    }
  }
  MMaintainedSpells.releasereadlock(__FUNCTION__, __LINE__);
  return ret;
}

SpellEffects* Entity::GetFreeSpellEffectSlot() {
  SpellEffects* ret = 0;
  InfoStruct* info = GetInfoStruct();
  MSpellEffects.readlock(__FUNCTION__, __LINE__);
  for (int i = 0; i < NUM_SPELL_EFFECTS; i++) {
    if (info->spell_effects[i].spell_id == 0xFFFFFFFF) {
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
  for (int i = 0; i < NUM_SPELL_EFFECTS; i++) {
    if (info->spell_effects[i].spell_id == id) {
      if (!caster || info->spell_effects[i].caster == caster) {
        ret = &info->spell_effects[i];
        break;
      }
    }
  }
  MSpellEffects.releasereadlock(__FUNCTION__, __LINE__);
  return ret;
}

InfoStruct* Entity::GetInfoStruct() {
  return &info_struct;
}

Item* Entity::LootItem(int32 id) {
  Item* ret = 0;
  vector<Item*>::iterator itr;
  MLootItems.lock();
  for (itr = loot_items.begin(); itr != loot_items.end(); itr++) {
    if ((*itr)->details.item_id == id) {
      ret = *itr;
      loot_items.erase(itr);
      break;
    }
  }
  MLootItems.unlock();
  return ret;
}

int32 Entity::GetLootItemID() {
  int32 ret = 0;
  vector<Item*>::iterator itr;
  MLootItems.lock();
  for (itr = loot_items.begin(); itr != loot_items.end(); itr++) {
    ret = (*itr)->details.item_id;
    break;
  }
  MLootItems.unlock();
  return ret;
}

Skill* Entity::GetSkillByName(const char* name, bool check_update) {
  LogWrite(MISC__TODO, 1, "TODO", "This does nothing... yet...\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
  return 0;
}

float Entity::GetMaxSpeed() {
  return max_speed;
}

void Entity::SetMaxSpeed(float val) {
  max_speed = val;
}

void Entity::CalculateBonuses() {
  InfoStruct* info = &info_struct;

  info->block = info->block_base;
  info->cur_attack = info->attack_base;
  info->cur_mitigation = info->mitigation_base;
  info->base_avoidance_pct = info->avoidance_base;
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

  info->sta = info->sta_base + info->sta_temp;
  info->agi = info->agi_base + info->agi_temp;
  info->str = info->str_base + info->str_temp;
  info->wis = info->wis_base + info->wis_temp;
  info->intel = info->intel_base + info->intel_temp;

  info->ability_modifier = 0;
  info->critical_mitigation = 0;
  info->block_chance = 0;
  info->crit_chance = info->crit_chance_temp;
  info->crit_bonus = 0;
  info->potency = 0;
  info->hate_mod = 0;
  info->reuse_speed = info->reuse_speed_temp;
  info->casting_speed = info->casting_speed_temp;
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
  info->offensive_speed = 0;
  info->mount_speed = 0;
  info->base_avoidance_bonus = 0;
  info->minimum_deflection_chance = 0;
  info->riposte_chance = 0;
  info->physical_damage_reduction = 0;
  info->ability_cost_multiplier = 1.0;

  stats.clear();
  ItemStatsValues* values = equipment_list.CalculateEquipmentBonuses(this);
  CalculateSpellBonuses(values);

  info->sta += values->sta;
  if (info->sta < 0) {
    info->sta = 0;
  }

  info->str += values->str;
  if (info->str < 0) {
    info->str = 0;
  }

  info->agi += values->agi;
  if (info->agi < 0) {
    info->agi = 0;
  }

  info->wis += values->wis;
  if (info->wis < 0) {
    info->wis = 0;
  }

  info->intel += values->int_;
  if (info->intel < 0)
    info->intel = 0;

  info->disease += values->vs_disease;
  if (info->disease < 0) {
    info->disease = 0;
  }

  info->divine += values->vs_divine;
  if (info->divine < 0) {
    info->divine = 0;
  }

  info->heat += values->vs_heat;
  if (info->heat < 0) {
    info->heat = 0;
  }

  info->magic += values->vs_magic;
  if (info->magic < 0) {
    info->magic = 0;
  }

  info->mental += values->vs_mental;
  if (info->mental < 0) {
    info->mental = 0;
  }

  info->poison += values->vs_poison;
  if (info->poison < 0) {
    info->poison = 0;
  }

  info->cold += values->vs_cold;
  if (info->cold < 0) {
    info->cold = 0;
  }

  info->cur_mitigation += values->vs_slash;
  info->cur_mitigation += values->vs_pierce;
  info->cur_mitigation += values->vs_crush;
  info->cur_mitigation += values->vs_physical;
  info->cur_mitigation += info->cur_mitigation * (values->mitigation_increase / 100.0);

  if (info->cur_mitigation < 0) {
    info->cur_mitigation = 0;
  }

  info->speed += values->speed;
  info->offensive_speed += values->offensive_speed;
  info->mount_speed += values->mount_speed;

  int32 sta_hp_bonus = 0.0;
  int32 prim_power_bonus = 0.0;

  if (IsPlayer()) {
    float bonus_mod = CalculateBonusMod();
    info->base_ability_modifier = CalculateBaseSpellIncrease();

    sta_hp_bonus = info->sta * bonus_mod;
    prim_power_bonus = GetPrimaryStat() * bonus_mod;
  }

  int16 base_regen = static_cast<int16>(GetLevel() * 1.6);
  SetHPRegen(base_regen);
  SetPowerRegen(base_regen);

  prim_power_bonus = floor(float(prim_power_bonus));
  sta_hp_bonus = floor(float(sta_hp_bonus));

  sint32 total_hp = GetTotalHP();
  sint32 total_power = GetTotalPower();

  SetTotalHP(GetTotalHPBase() + values->health + sta_hp_bonus);
  SetTotalPower(GetTotalPowerBase() + values->power + prim_power_bonus);

  sint32 hp_difference = GetTotalHP() - GetTotalHPBase();
  sint32 power_difference = GetTotalPower() - GetTotalPowerBase();

  if (hp_difference > 0) {
    SetHP(GetHP() + hp_difference);
  }

  if (power_difference > 0) {
    SetPower(GetPower() + power_difference);
  }

  if (GetHP() > GetTotalHP()) {
    SetHP(GetTotalHP());
  }

  if (GetPower() > GetTotalPower()) {
    SetPower(GetTotalPower());
  }

  info->max_concentration += values->concentration;
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
  info->attackspeed += values->attackspeed;
  info->multi_attack += values->multiattackchance;
  info->flurry += values->flurry;
  info->melee_ae += values->aeautoattackchance;
  info->strikethrough += values->strikethrough;
  info->accuracy += values->accuracy;
  info->base_avoidance_bonus += values->base_avoidance_bonus;
  info->minimum_deflection_chance += values->minimum_deflection_chance;
  info->riposte_chance += values->riposte_chance;
  info->physical_damage_reduction += values->physical_damage_reduction;
  info->ability_cost_multiplier += (values->ability_cost_modifier / 100.0);
  safe_delete(values);
}

EquipmentItemList* Entity::GetEquipmentList() {
  return &equipment_list;
}

void Entity::SetEquipment(Item* item, int8 slot) {
  if (!item && slot < NUM_SLOTS) {
    SetInfo(&equipment.equip_id[slot], 0);
    SetInfo(&equipment.color[slot].red, 0);
    SetInfo(&equipment.color[slot].green, 0);
    SetInfo(&equipment.color[slot].blue, 0);
    SetInfo(&equipment.highlight[slot].red, 0);
    SetInfo(&equipment.highlight[slot].green, 0);
    SetInfo(&equipment.highlight[slot].blue, 0);
  } else {
    SetInfo(&equipment.equip_id[item->details.slot_id], item->generic_info.appearance_id);
    SetInfo(&equipment.color[item->details.slot_id].red, item->generic_info.appearance_red);
    SetInfo(&equipment.color[item->details.slot_id].green, item->generic_info.appearance_green);
    SetInfo(&equipment.color[item->details.slot_id].blue, item->generic_info.appearance_blue);
    SetInfo(&equipment.highlight[item->details.slot_id].red, item->generic_info.appearance_highlight_red);
    SetInfo(&equipment.highlight[item->details.slot_id].green, item->generic_info.appearance_highlight_green);
    SetInfo(&equipment.highlight[item->details.slot_id].blue, item->generic_info.appearance_highlight_blue);
  }
}

bool Entity::CheckSpellBonusRemoval(shared_ptr<LuaSpell> spell, int16 type) {
  MutexList<BonusValues*>::iterator itr = bonus_list.begin();
  while (itr.Next()) {
    if (itr.value->luaspell == spell && itr.value->type == type) {
      bonus_list.Remove(itr.value, true);
      return true;
    }
  }
  return false;
}

void Entity::AddSpellBonus(shared_ptr<LuaSpell> spell, int16 type, sint32 value, int64 class_req) {
  CheckSpellBonusRemoval(spell, type);

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
  BonusValues* ret = 0;
  MutexList<BonusValues*>::iterator itr = bonus_list.begin();
  while (itr.Next()) {
    if (itr.value->spell_id == spell_id) {
      ret = itr.value;
      break;
    }
  }

  return ret;
}

vector<BonusValues*>* Entity::GetAllSpellBonuses(shared_ptr<LuaSpell> spell) {
  vector<BonusValues*>* list = new vector<BonusValues*>;
  MutexList<BonusValues*>::iterator itr = bonus_list.begin();
  while (itr.Next()) {
    if (itr.value->luaspell == spell)
      list->push_back(itr.value);
  }
  return list;
}

void Entity::RemoveSpellBonus(shared_ptr<LuaSpell> spell) {
  MutexList<BonusValues*>::iterator itr = bonus_list.begin();
  while (itr.Next()) {
    if (itr.value->luaspell == spell) {
      bonus_list.Remove(itr.value, true);
    }
  }
}

void Entity::CalculateSpellBonuses(ItemStatsValues* stats) {
  if (stats) {
    MutexList<BonusValues*>::iterator itr = bonus_list.begin();
    vector<BonusValues*> bv;
    //First check if we meet the requirement for each bonus
    while (itr.Next()) {
      int64 class1 = pow(2.0, (GetAdventureClass() - 1));
      int64 class2 = pow(2.0, (classes.GetSecondaryBaseClass(GetAdventureClass()) - 1));
      int64 class3 = pow(2.0, (classes.GetBaseClass(GetAdventureClass()) - 1));
      if (itr.value->class_req == 0 || (itr.value->class_req & class1) == class1 || (itr.value->class_req & class2) == class2 || (itr.value->class_req & class3) == class3)
        bv.push_back(itr.value);
    }
    //Sort the bonuses by spell id and luaspell
    BonusValues* bonus;
    map<int32, map<shared_ptr<LuaSpell>, vector<BonusValues*>>> sort;
    for (int8 i = 0; i < bv.size(); i++) {
      bonus = bv.at(i);
      sort[bonus->spell_id][bonus->luaspell].push_back(bonus);
    }
    //Now check for the highest tier of each spell id and apply those bonuses
    map<shared_ptr<LuaSpell>, vector<BonusValues*>>::iterator tier_itr;
    map<int32, map<shared_ptr<LuaSpell>, vector<BonusValues*>>>::iterator sort_itr;
    for (sort_itr = sort.begin(); sort_itr != sort.end(); sort_itr++) {
      shared_ptr<LuaSpell> key;
      sint8 highest_tier = -1;
      //Find the highest tier for this spell id
      for (tier_itr = sort_itr->second.begin(); tier_itr != sort_itr->second.end(); tier_itr++) {
        shared_ptr<LuaSpell> current_spell = tier_itr->first;
        sint8 current_tier;
        if (current_spell && current_spell->spell && ((current_tier = current_spell->spell->GetSpellTier()) > highest_tier)) {
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

void Entity::AddControlEffect(shared_ptr<LuaSpell> luaspell, int8 type) {
  if (!luaspell) {
    return;
  }

  if (IsImmuneToControlEffect(type)) {
    GetZone()->SendDamagePacket(luaspell->caster, this, DAMAGE_PACKET_TYPE_SIMPLE_DAMAGE, DAMAGE_PACKET_RESULT_IMMUNE, 0, 0, 0);
    return;
  }

  auto control_effect = make_unique<ControlEffect>();
  control_effect->luaspell = luaspell;
  control_effect->type = type;

  {
    lock_guard<mutex> guard(control_effects_mutex);
    control_effects[type].push_back(move(control_effect));
  }

  auto effect_flag = control_effect_flags.find(type);

  if (effect_flag != control_effect_flags.end()) {
    if (!(luaspell->effect_bitmask & effect_flag->second)) {
      luaspell->effect_bitmask += effect_flag->second;
    }
  }

  if (IsCasting()) {
    Spell* spell = GetZone()->GetSpellProcess()->GetSpell(this);

    if (spell) {
      if ((IsStunned() && !IsStunImmune() && !spell->CastWhileStunned()) ||
          (IsStifled() && !IsStifleImmune() && !spell->CastWhileStifled()) ||
          (IsMezzed() && !IsMezImmune() && !spell->CastWhileMezzed()) ||
          (IsFeared() && !IsFearImmune() && !spell->CastWhileFeared())) {
        GetZone()->GetSpellProcess()->Interrupted(this, luaspell->caster, SPELL_ERROR_INTERRUPTED);
      }
    }
  }

  if (luaspell->caster != this && luaspell->caster->IsPlayer() && IsPlayer()) {
    auto spell_id = control_effect_immunity_spells.find(type);

    if (spell_id != control_effect_immunity_spells.end()) {
      GetZone()->GetSpellProcess()->CastSpell(spell_id->second, 1, this, this->GetID(), luaspell->spell->GetSpellData()->duration1 * 2.5);
    }
  }
}

void Entity::RemoveControlEffect(shared_ptr<LuaSpell> luaspell, int8 type) {
  lock_guard<mutex> guard(control_effects_mutex);

  for (auto itr = control_effects[type].begin(); itr != control_effects[type].end();) {
    if (!luaspell || (*itr)->luaspell == luaspell) {
      itr = control_effects[type].erase(itr);

      if (luaspell) {
        break;
      }
    } else {
      ++itr;
    }
  }
}

bool Entity::HasControlEffect(int8 type) {
  lock_guard<mutex> guard(control_effects_mutex);

  auto effects = control_effects.find(type);

  if (effects != control_effects.end()) {
    return !effects->second.empty();
  } else {
    return false;
  }
}

bool Entity::IsImmuneToControlEffect(int8 type, bool active) {
  auto immunity_type = control_effect_immunities.find(type);

  if (immunity_type != control_effect_immunities.end()) {
    return HasImmunityEffect(immunity_type->second, active);
  } else {
    return false;
  }
}

void Entity::ApplyControlEffects() {
  if (IsPlayer()) {
    Player* player = static_cast<Player*>(this);
    bool is_stunned = IsStunned() && !IsStunImmune(true);
    bool is_stifled = IsStifled() && !IsStifleImmune(true);
    bool is_mezzed = IsMezzed() && !IsMezImmune(true);
    bool is_feared = IsFeared() && !IsFearImmune(true);
    bool is_rooted = IsRooted() && !IsRootImmune(true);
    bool is_force_faced = IsForceFaced();
    bool is_feigned = IsFeigned();

    if (is_stunned || is_stifled || is_mezzed || is_feared || is_feigned) {
      GetZone()->LockAllSpells(player);
    } else {
      GetZone()->UnlockAllSpells(player);
    }

    if (is_stunned || is_rooted || is_mezzed) {
      player->SetPlayerControlFlag(1, 8, true);
    } else {
      player->SetPlayerControlFlag(1, 8, false);
    }

    if (is_stunned || is_mezzed || is_force_faced) {
      player->SetPlayerControlFlag(1, 16, true);
    } else {
      player->SetPlayerControlFlag(1, 16, false);
    }

    if (HasControlEffect(CONTROL_EFFECT_TYPE_WALKUNDERWATER)) {
      player->SetPlayerControlFlag(3, 128, true);
    } else {
      player->SetPlayerControlFlag(3, 128, false);
    }

    if (HasControlEffect(CONTROL_EFFECT_TYPE_JUMPUNDERWATER)) {
      player->SetPlayerControlFlag(4, 1, true);
    } else {
      player->SetPlayerControlFlag(4, 1, false);
    }

    if (is_feared) {
      player->SetPlayerControlFlag(4, 4, true);
    } else {
      player->SetPlayerControlFlag(4, 4, false);
    }

    if (HasControlEffect(CONTROL_EFFECT_TYPE_GLIDE)) {
      player->SetPlayerControlFlag(4, 16, true);
    } else {
      player->SetPlayerControlFlag(4, 16, false);
    }

    if (HasControlEffect(CONTROL_EFFECT_TYPE_SAFEFALL)) {
      player->SetPlayerControlFlag(4, 32, true);
    } else {
      player->SetPlayerControlFlag(4, 32, false);
    }

    if (is_feigned) {
      player->SetPlayerControlFlag(5, 1, true);
      SetTempActionState(228);
    } else {
      player->SetPlayerControlFlag(5, 1, false);
      SetTempActionState(-1);
    }

    if (HasControlEffect(CONTROL_EFFECT_TYPE_FLIGHT)) {
      player->SetPlayerControlFlag(5, 32, true);
    } else {
      player->SetPlayerControlFlag(5, 32, false);
    }
  } else {
    if (IsRooted()) {
      SetSpeedMultiplier(0.0f);
    } else {
      SetSpeedMultiplier(1.0f);
    }
  }
}

void Entity::HideDeityPet(bool val) {
  if (!deityPet)
    return;

  if (val) {
    deityPet->AddAllowAccessSpawn(deityPet);
    GetZone()->HidePrivateSpawn(deityPet);
  } else
    deityPet->MakeSpawnPublic();
}

void Entity::HideCosmeticPet(bool val) {
  if (!cosmeticPet)
    return;

  if (val) {
    cosmeticPet->AddAllowAccessSpawn(cosmeticPet);
    GetZone()->HidePrivateSpawn(cosmeticPet);
  } else
    cosmeticPet->MakeSpawnPublic();
}

void Entity::RemoveDumbfirePet(Entity* pet) {
  dumbfire_pets.erase(remove(dumbfire_pets.begin(), dumbfire_pets.end(), pet), dumbfire_pets.end());
}

void Entity::DismissDumbfirePets() {
  for (auto dumbfire : dumbfire_pets) {
    DismissPet(static_cast<NPC*>(dumbfire));
  }

  dumbfire_pets.clear();
}

void Entity::DismissPet(NPC* pet, bool from_death) {
  if (!pet) {
    return;
  }

  Entity* owner = pet->GetOwner();

  pet->SetDismissing(true);

  if (pet->GetPetType() != PET_TYPE_DUMBFIRE) {
    Spell* spell = master_spell_list.GetSpell(pet->GetPetSpellID(), pet->GetPetSpellTier());

    if (spell) {
      GetZone()->GetSpellProcess()->DeleteCasterSpell(this, spell);
    }
  }

  if (pet->GetPetType() == PET_TYPE_CHARMED) {
    owner->SetCharmedPet(nullptr);

    if (!from_death) {
      // set the pet flag to false, owner to 0, and give the mob its old brain back
      pet->SetPet(false);
      pet->SetBrain(new Brain(pet));
      pet->SetDismissing(false);
    }
  } else if (pet->GetPetType() == PET_TYPE_COMBAT) {
    owner->SetCombatPet(nullptr);
  } else if (pet->GetPetType() == PET_TYPE_DEITY) {
    owner->SetDeityPet(nullptr);
  } else if (pet->GetPetType() == PET_TYPE_COSMETIC) {
    owner->SetCosmeticPet(nullptr);
  } else if (pet->GetPetType() == PET_TYPE_DUMBFIRE && from_death) {
    owner->RemoveDumbfirePet(pet);
  }

  // if owner is player and no combat pets left reset the pet info
  if (owner->IsPlayer()) {
    if (!owner->GetPet() && !owner->GetCharmedPet()) {
      static_cast<Player*>(owner)->ResetPetInfo();
    }
  }

  pet->SetOwner(nullptr);

  // remove the spawn from the world
  if (!from_death && pet->GetPetType() != PET_TYPE_CHARMED) {
    GetZone()->RemoveSpawn(pet);
  }
}

float Entity::CalculateBaseSpellIncrease() {
  int16 soft_cap = GetLevel() * 14;
  int16 hard_cap = GetLevel() * 16;
  int16 stat = GetPrimaryStat();

  float soft_bonus = min(stat, soft_cap) / GetLevel();
  float hard_bonus = min(stat, hard_cap) / GetLevel();

  float bonus = 0.0;

  for (int i = 0; i < 2; ++i) {
    float temp_bonus = 0;
    float temp_amount = 0;

    if (i == 0) {
      temp_bonus = soft_bonus;
      temp_amount = soft_bonus;
    } else {
      if (hard_bonus == soft_bonus) {
        break;
      }

      temp_bonus = hard_bonus;
      temp_amount = hard_bonus - soft_bonus;
    }

    if (temp_bonus <= 14) {
      bonus += temp_amount * 0.35;
    } else if (temp_bonus > 14 && temp_bonus <= 16) {
      bonus += temp_amount * 0.15;
    } else if (temp_bonus > 16) {
      bonus += temp_amount * 0.05;
    }
  }

  return bonus;
}

float Entity::CalculateBonusMod() {
  int8 level = GetLevel();

  if (level <= 20) {
    return 3.0;
  } else if (level >= 90) {
    return 10.0;
  } else {
    return (level - 20) * .1 + 3.0;
  }
}

float Entity::CalculateDPSMultiplier() {
  float dps = GetInfoStruct()->dps;

  if (dps > 0) {
    if (dps <= 100) {
      return (dps / 100 + 1);
    } else if (dps <= 200) {
      return (((dps - 100) * .25 + 100) / 100 + 1);
    } else if (dps <= 300) {
      return (((dps - 200) * .1 + 125) / 100 + 1);
    } else if (dps <= 900) {
      return (((dps - 300) * .05 + 135) / 100 + 1);
    } else {
      return (((dps - 900) * .01 + 165) / 100 + 1);
    }
  }

  return 1;
}

void Entity::AddWard(shared_ptr<LuaSpell> luaspell, WardInfo* ward) {
  if (m_wardList.count(luaspell) == 0) {
    m_wardList[luaspell] = ward;
  }
}

WardInfo* Entity::GetWard(shared_ptr<LuaSpell> luaspell) {
  WardInfo* ret = 0;

  if (m_wardList.count(luaspell) > 0)
    ret = m_wardList[luaspell];

  return ret;
}

void Entity::RemoveWard(shared_ptr<LuaSpell> luaspell) {
  if (m_wardList.count(luaspell) > 0) {
    // Delete the ward info
    safe_delete(m_wardList[luaspell]);
    // Remove from the ward list
    m_wardList.erase(luaspell);
  }
}

int32 Entity::CheckWards(int32 damage, int8 damage_type) {
  map<shared_ptr<LuaSpell>, WardInfo*>::iterator itr;
  WardInfo* ward = 0;
  shared_ptr<LuaSpell> spell = 0;

  int amount_warded = 0;

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
      amount_warded += ward->DamageLeft;
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
      amount_warded += damage;
      damage = 0;
    }

    if (spell->caster->IsPlayer())
      ClientPacketFunctions::SendMaintainedExamineUpdate(GetZone()->GetClientBySpawn(spell->caster), spell->slot_pos, ward->DamageLeft, 1);

    // Reset ward pointer
    ward = 0;
  }

  SetLastDamageWarded(amount_warded);

  return damage;
}

void Entity::AddStoneskin(shared_ptr<LuaSpell> luaspell, StoneskinInfo* stoneskin) {
  if (m_stoneskinList.count(luaspell) == 0) {
    m_stoneskinList[luaspell] = stoneskin;
  }
}

void Entity::RemoveStoneskin(shared_ptr<LuaSpell> luaspell) {
  if (m_stoneskinList.count(luaspell) > 0) {
    safe_delete(m_stoneskinList[luaspell]);
    m_stoneskinList.erase(luaspell);
  }
}

int32 Entity::CheckStoneskins(int32 damage, Entity* attacker) {
  vector<shared_ptr<LuaSpell>> to_delete;
  int total_absorbed = 0;

  for (const auto& kv : m_stoneskinList) {
    shared_ptr<LuaSpell> spell = kv.first;
    StoneskinInfo* stoneskin = kv.second;

    if (!stoneskin->infinite && damage >= stoneskin->DamageLeft) {
      damage -= stoneskin->DamageLeft;
      total_absorbed += stoneskin->DamageLeft;
      stoneskin->DamageLeft = 0;

      if (!stoneskin->keepStoneskin) {
        to_delete.push_back(spell);
      }
    } else {
      if (!stoneskin->infinite) {
        stoneskin->DamageLeft -= damage;
      }

      total_absorbed = damage;
      damage = 0;
      break;
    }
  }

  for (const auto& spell : to_delete) {
    RemoveStoneskin(spell);
    GetZone()->GetSpellProcess()->DeleteCasterSpell(spell);
  }

  if (total_absorbed > 0) {
    string absorbed_str = FormatWithCommas(total_absorbed);

    if (IsPlayer()) {
      string message = "Your stoneskin absorbed " + absorbed_str + " points of damage!";
      GetZone()->GetClientBySpawn(this)->SimpleMessage(109, message.c_str());
    }

    if (attacker->IsPlayer()) {
      string message = "Your target's stoneskin absorbed " + absorbed_str + " points of damage!";
      GetZone()->GetClientBySpawn(attacker)->SimpleMessage(109, message.c_str());
    }
  }

  return damage;
}

void Entity::SetTriggerCount(shared_ptr<LuaSpell> luaspell, int16 count) {
  m_triggerCounts[luaspell] = count;
}

int16 Entity::GetTriggerCount(shared_ptr<LuaSpell> luaspell) {
  if (m_triggerCounts.count(luaspell) > 0) {
    return m_triggerCounts[luaspell];
  }

  return 0;
}

float Entity::CalculateCastingSpeedMod() {
  float cast_speed = info_struct.casting_speed;

  if (cast_speed > 0)
    return 100 * max((float)0.5, (float)(1 + (1 - (1 / (1 + (cast_speed * .01))))));
  else if (cast_speed < 0)
    return 100 * min((float)1.5, (float)(1 + (1 - (1 / (1 + (cast_speed * -.01))))));
  return 0;
}

float Entity::GetSpeed() {
  float ret = speed;

  if (IsStealthed() || IsInvis()) {
    ret += stats[ITEM_STAT_STEALTHINVISSPEEDMOD];
  } else if (EngagedInCombat()) {
    ret += GetInfoStruct()->offensive_speed;
  } else {
    ret += max(GetInfoStruct()->offensive_speed, max(GetInfoStruct()->speed, GetInfoStruct()->mount_speed));
  }

  ret *= speed_multiplier;

  if (IsPlayer()) {
    ret -= GetHighestSnare();
  }

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
  for (int32 i = 0; i < detrimental_spell_effects.size(); i++) {
    det = &detrimental_spell_effects.at(i);
    if (det && det->det_type == det_type && !det->incurable) {
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

void Entity::CureDetrimentByType(int8 cure_level, int8 det_type, string cure_name, Entity* caster) {
  if (cure_level <= 0 || (GetDetTypeCount(det_type) <= 0 && (det_type == DET_TYPE_ALL && GetDetCount() <= 0)))
    return;

  vector<DetrimentalEffects>* det_list = &detrimental_spell_effects;
  map<int8, vector<shared_ptr<LuaSpell>>> remove_list;
  int8 total_cure_level = 0;

  MDetriments.readlock(__FUNCTION__, __LINE__);
  for (const auto& det : *det_list) {
    if ((det.det_type == det_type || (det_type == DET_TYPE_ALL && det.det_type != DET_TYPE_CURSE)) && !det.incurable) {
      vector<LevelArray*>* levels = det.spell->spell->GetSpellLevels();
      InfoStruct* info_struct = det.caster->GetInfoStruct();

      if (levels->size() > 0) {
        for (const auto x : *levels) {
          int8 level = x->spell_level / 10;
          int8 det_class = x->adventure_class;

          if ((info_struct->class1 == det_class || info_struct->class2 == det_class || info_struct->class3 == det_class || det.caster->GetAdventureClass() == det_class) && cure_level >= level) {
            det.spell->was_cured = true;
            remove_list[level].push_back(det.spell);
            break;
          }
        }
      } else if (cure_level >= det.caster->GetLevel()) {
        det.spell->was_cured = true;
        remove_list[det.caster->GetLevel()].push_back(det.spell);
        break;
      }
    }
  }
  MDetriments.releasereadlock(__FUNCTION__, __LINE__);

  for (auto it = remove_list.rbegin(); it != remove_list.rend(); ++it) {
    if (total_cure_level + it->first > cure_level)
      break;

    for (const auto spell : it->second) {
      if (total_cure_level + it->first > cure_level)
        break;

      GetZone()->SendDispellPacket(caster, this, cure_name, (string)spell->spell->GetName(), DISPELL_TYPE_CURE);
      GetZone()->RemoveTargetFromSpell(spell, this);

      total_cure_level += it->first;
    }
  }
}

void Entity::CureDetrimentByControlEffect(int8 cure_level, int8 cc_type, string cure_name, Entity* caster) {
  if (cure_level <= 0 || GetDetCount() <= 0) {
    return;
  }

  map<int8, vector<shared_ptr<LuaSpell>>> remove_list;
  int8 total_cure_level = 0;

  for (const auto& effect : control_effects[cc_type]) {
    vector<LevelArray*>* levels = effect->luaspell->spell->GetSpellLevels();

    if (levels->size() > 0) {
      for (const auto x : *levels) {
        int8 level = x->spell_level / 10;
        effect->luaspell->was_cured = true;
        remove_list[level].push_back(effect->luaspell);
        break;
      }
    } else if (cure_level >= effect->luaspell->caster->GetLevel()) {
      effect->luaspell->was_cured = true;
      remove_list[effect->luaspell->caster->GetLevel()].push_back(effect->luaspell);
    }
  }

  for (auto it = remove_list.rbegin(); it != remove_list.rend(); ++it) {
    if (total_cure_level + it->first > cure_level)
      break;

    for (const auto spell : it->second) {
      if (total_cure_level + it->first > cure_level)
        break;

      GetZone()->SendDispellPacket(caster, this, cure_name, (string)spell->spell->GetName(), DISPELL_TYPE_CURE);
      GetZone()->RemoveTargetFromSpell(spell, this);

      total_cure_level += it->first;
    }
  }
}

void Entity::RemoveDetrimentalSpell(shared_ptr<LuaSpell> spell) {
  if (!spell || spell->spell->GetSpellData()->det_type == 0)
    return;
  MDetriments.writelock(__FUNCTION__, __LINE__);
  vector<DetrimentalEffects>* det_list = &detrimental_spell_effects;
  vector<DetrimentalEffects>::iterator itr;
  for (itr = det_list->begin(); itr != det_list->end(); itr++) {
    if ((*itr).spell == spell) {
      det_count_list[(*itr).det_type]--;
      det_list->erase(itr);
      if (IsPlayer())
        ((Player*)this)->SetCharSheetChanged(true);
      break;
    }
  }
  MDetriments.releasewritelock(__FUNCTION__, __LINE__);
}

int8 Entity::GetDetTypeCount(int8 det_type) {
  return det_count_list[det_type];
}

int8 Entity::GetDetCount() {
  int8 det_count = 0;
  map<int8, int8>::iterator itr;

  for (itr = det_count_list.begin(); itr != det_count_list.end(); itr++)
    det_count += (*itr).second;

  return det_count;
}

vector<DetrimentalEffects>* Entity::GetDetrimentalSpellEffects() {
  return &detrimental_spell_effects;
}

void Entity::AddDetrimentalSpell(shared_ptr<LuaSpell> luaspell) {
  if (!luaspell || !luaspell->caster)
    return;

  Spell* spell = luaspell->spell;
  DetrimentalEffects* det = GetDetrimentalEffect(spell->GetSpellID(), luaspell->caster);
  DetrimentalEffects new_det;
  if (det)
    RemoveDetrimentalSpell(det->spell);

  SpellData* data = spell->GetSpellData();
  if (!data)
    return;

  new_det.caster = luaspell->caster;
  new_det.spell = luaspell;
  if (spell->GetSpellData()->duration_until_cancel)
    new_det.expire_timestamp = 0xFFFFFFFF;
  else
    new_det.expire_timestamp = Timer::GetCurrentTime2() + (spell->GetSpellDuration() * 100);
  new_det.icon = data->icon;
  new_det.icon_backdrop = data->icon_backdrop;
  new_det.tier = data->tier;
  new_det.det_type = data->det_type;
  new_det.incurable = data->incurable;
  new_det.spell_id = spell->GetSpellID();
  new_det.control_effect = data->control_effect_type;
  new_det.total_time = spell->GetSpellDuration() / 10;

  MDetriments.writelock(__FUNCTION__, __LINE__);
  detrimental_spell_effects.push_back(new_det);
  det_count_list[new_det.det_type]++;
  MDetriments.releasewritelock(__FUNCTION__, __LINE__);
}

DetrimentalEffects* Entity::GetDetrimentalEffect(int32 spell_id, Entity* caster) {
  vector<DetrimentalEffects>* det_list = &detrimental_spell_effects;
  DetrimentalEffects* ret = 0;
  MDetriments.readlock(__FUNCTION__, __LINE__);
  for (int32 i = 0; i < det_list->size(); i++) {
    if (det_list->at(i).spell_id == spell_id && det_list->at(i).caster == caster)
      ret = &det_list->at(i);
  }
  MDetriments.releasereadlock(__FUNCTION__, __LINE__);

  return ret;
}

void Entity::CancelAllStealth(shared_ptr<LuaSpell> exclude_spell) {
  lock_guard<mutex> guard(control_effects_mutex);

  auto& stealth_effects = control_effects[CONTROL_EFFECT_TYPE_STEALTH];
  auto& invis_effects = control_effects[CONTROL_EFFECT_TYPE_INVIS];

  for (auto itr = stealth_effects.begin(); itr != stealth_effects.end(); ++itr) {
    if (exclude_spell != (*itr)->luaspell) {
      if ((*itr)->luaspell->caster == this) {
        GetZone()->GetSpellProcess()->AddSpellCancel((*itr)->luaspell);
      } else {
        GetZone()->RemoveTargetFromSpell((*itr)->luaspell, this);
      }
    }
  }

  for (auto itr = invis_effects.begin(); itr != invis_effects.end(); ++itr) {
    if (exclude_spell != (*itr)->luaspell) {
      if ((*itr)->luaspell->caster == this) {
        GetZone()->GetSpellProcess()->AddSpellCancel((*itr)->luaspell);
      } else {
        GetZone()->RemoveTargetFromSpell((*itr)->luaspell, this);
      }
    }
  }
}

void Entity::RemoveAllFeignEffects() {
  lock_guard<mutex> guard(control_effects_mutex);

  auto& feign_effects = control_effects[CONTROL_EFFECT_TYPE_FEIGNED];

  for (auto itr = feign_effects.begin(); itr != feign_effects.end(); ++itr) {
    if ((*itr)->luaspell->caster == this) {
      GetZone()->GetSpellProcess()->AddSpellCancel((*itr)->luaspell);
    } else {
      GetZone()->RemoveTargetFromSpell((*itr)->luaspell, this);
    }
  }
}

void Entity::RemoveAllMezSpells() {
  lock_guard<mutex> guard(control_effects_mutex);

  auto& mez_effects = control_effects[CONTROL_EFFECT_TYPE_MEZ];

  for (auto itr = mez_effects.begin(); itr != mez_effects.end(); ++itr) {
    if ((*itr)->luaspell->caster == this) {
      GetZone()->GetSpellProcess()->AddSpellCancel((*itr)->luaspell);
    } else {
      GetZone()->RemoveTargetFromSpell((*itr)->luaspell, this);
    }
  }
}

bool Entity::CanAttackTarget(Spawn* target) {
  if (target == this)
    return false;

  if (IsPlayer() && (target->IsPlayer() || (target->IsPet() && static_cast<NPC*>(target)->GetOwner() && static_cast<NPC*>(target)->GetOwner()->IsPlayer()))) {
    return PVP::CanAttack(static_cast<Player*>(this), target);
  } else {
    if (target->IsPlayer()) {
      return true;
    } else if (target->IsNPC()) {
      return !static_cast<NPC*>(target)->m_runningBack && target->GetAttackable();
    } else {
      return target->GetAttackable();
    }
  }
}

bool Entity::IsHostile(Spawn* target) {
  if (!target || target == this)
    return false;

  if (IsPlayer() && (target->IsPlayer() || (target->IsPet() && static_cast<NPC*>(target)->GetOwner() && static_cast<NPC*>(target)->GetOwner()->IsPlayer()))) {
    return PVP::IsHostile(static_cast<Player*>(this), target);
  } else if (target->IsPlayer()) {
    return true;
  } else {
    return target->GetAttackable();
  }
}

bool Entity::IsStealthed() {
  return HasControlEffect(CONTROL_EFFECT_TYPE_STEALTH);
}

bool Entity::IsInvis() {
  return HasControlEffect(CONTROL_EFFECT_TYPE_INVIS);
}

void Entity::RemoveInvisSpell(shared_ptr<LuaSpell> spell) {
  RemoveControlEffect(spell, CONTROL_EFFECT_TYPE_INVIS);

  AddSpawnUpdate(true, false, false);

  if (IsPlayer()) {
    static_cast<Player*>(this)->SetCharSheetChanged(true);

    if (!EngagedInCombat()) {
      static_cast<Player*>(this)->SetResendSpawns(RESEND_AGGRO);
    }
  }
}

void Entity::RemoveStealthSpell(shared_ptr<LuaSpell> spell) {
  RemoveControlEffect(spell, CONTROL_EFFECT_TYPE_STEALTH);

  AddSpawnUpdate(true, false, false);

  if (IsPlayer()) {
    static_cast<Player*>(this)->SetCharSheetChanged(true);

    if (!EngagedInCombat()) {
      static_cast<Player*>(this)->SetResendSpawns(RESEND_AGGRO);
    }
  }
}

void Entity::SetSnareValue(shared_ptr<LuaSpell> spell, float snare_val) {
  if (!spell) {
    return;
  }

  snare_values[spell] = snare_val;
}

float Entity::GetHighestSnare() {
  // For simplicity this will return the highest snare value, which is actually the lowest value
  float ret = 0.0f;

  if (snare_values.size() == 0)
    return ret;

  map<shared_ptr<LuaSpell>, float>::iterator itr;
  for (itr = snare_values.begin(); itr != snare_values.end(); itr++) {
    if (itr->second > ret)
      ret = itr->second;
  }

  return ret;
}

bool Entity::IsSnared() {
  return HasControlEffect(CONTROL_EFFECT_TYPE_SNARE);
}

bool Entity::IsMezzed() {
  return HasControlEffect(CONTROL_EFFECT_TYPE_MEZ);
}

bool Entity::IsStifled() {
  return HasControlEffect(CONTROL_EFFECT_TYPE_STIFLE);
}

bool Entity::IsDazed() {
  return HasControlEffect(CONTROL_EFFECT_TYPE_DAZE);
}

bool Entity::IsStunned() {
  return HasControlEffect(CONTROL_EFFECT_TYPE_STUN);
}

bool Entity::IsForceFaced() {
  return HasControlEffect(CONTROL_EFFECT_TYPE_FORCE_FACE);
}

bool Entity::IsTaunted() {
  return HasControlEffect(CONTROL_EFFECT_TYPE_TAUNT);
}

bool Entity::IsRooted() {
  return HasControlEffect(CONTROL_EFFECT_TYPE_ROOT);
}

bool Entity::IsFeared() {
  return HasControlEffect(CONTROL_EFFECT_TYPE_FEAR);
}

bool Entity::IsFeigned() {
  return HasControlEffect(CONTROL_EFFECT_TYPE_FEIGNED);
}

bool Entity::IsWarded() {
  map<shared_ptr<LuaSpell>, WardInfo*>::iterator itr;

  for (itr = m_wardList.begin(); itr != m_wardList.end(); itr++) {
    if (itr->second->DamageLeft > 0)
      return true;
  }

  return false;
}

void Entity::AddImmunityEffect(shared_ptr<LuaSpell> luaspell, int8 type, bool active) {
  if (!luaspell) {
    return;
  }

  auto immunity_effect = make_unique<ImmunityEffect>();
  immunity_effect->active = active;
  immunity_effect->luaspell = luaspell;
  immunity_effect->type = type;

  {
    lock_guard<mutex> guard(immunity_effects_mutex);
    immunity_effects[type].push_back(move(immunity_effect));
  }

  auto effect_flag = immunity_effect_flags.find(type);

  if (effect_flag != immunity_effect_flags.end()) {
    if (!(luaspell->effect_bitmask & effect_flag->second)) {
      luaspell->effect_bitmask += effect_flag->second;
    }
  }
}

bool Entity::HasImmunityEffect(int8 type, bool active) {
  lock_guard<mutex> guard(immunity_effects_mutex);

  if (active) {
    for (const auto& immunity : immunity_effects[type]) {
      if (immunity->active) {
        return true;
      }
    }
  } else {
    return !immunity_effects[type].empty();
  }
}

void Entity::RemoveImmunityEffect(shared_ptr<LuaSpell> luaspell, int8 type) {
  lock_guard<mutex> guard(immunity_effects_mutex);

  for (auto itr = immunity_effects[type].begin(); itr != immunity_effects[type].end();) {
    if (!luaspell || (*itr)->luaspell == luaspell) {
      itr = immunity_effects[type].erase(itr);

      if (luaspell) {
        break;
      }
    } else {
      ++itr;
    }
  }
}

bool Entity::IsAOEImmune(bool active) {
  return HasImmunityEffect(IMMUNITY_TYPE_AOE, active);
}

bool Entity::IsStunImmune(bool active) {
  return HasImmunityEffect(IMMUNITY_TYPE_STUN, active);
}

bool Entity::IsStifleImmune(bool active) {
  return HasImmunityEffect(IMMUNITY_TYPE_STIFLE, active);
}

bool Entity::IsMezImmune(bool active) {
  return HasImmunityEffect(IMMUNITY_TYPE_MEZ, active);
}

bool Entity::IsRootImmune(bool active) {
  return HasImmunityEffect(IMMUNITY_TYPE_ROOT, active);
}

bool Entity::IsFearImmune(bool active) {
  return HasImmunityEffect(IMMUNITY_TYPE_FEAR, active);
}

bool Entity::IsDazeImmune(bool active) {
  return HasImmunityEffect(IMMUNITY_TYPE_DAZE, active);
}

void Entity::RemoveEffectsFromLuaSpell(shared_ptr<LuaSpell> spell) {
  if (!spell) {
    return;
  }

  int32 effect_bitmask = spell->effect_bitmask;

  if (effect_bitmask == 0) {
    return;
  }

  bool has_cc = false;
  for (const auto control_effect_flag : control_effect_flags) {
    if (effect_bitmask & control_effect_flag.second) {
      RemoveControlEffect(spell, control_effect_flag.first);
      has_cc = true;

      if (control_effect_flag.first == CONTROL_EFFECT_TYPE_SNARE) {
        snare_values.erase(spell);
      }
    }
  }

  for (const auto immunity_effect_flag : immunity_effect_flags) {
    if (effect_bitmask & immunity_effect_flag.second) {
      RemoveImmunityEffect(spell, immunity_effect_flag.first);
      has_cc = true;
    }
  }

  if (has_cc) {
    ApplyControlEffects();
  }

  if (effect_bitmask & EFFECT_FLAG_SKILLBONUS) {
    RemoveSkillBonus(spell->spell->GetSpellID());

    if (IsPlayer()) {
      shared_ptr<Client> client = GetZone()->GetClientBySpawn(this);

      if (client) {
        EQ2Packet* packet = static_cast<Player*>(this)->GetSkills()->GetSkillPacket(client->GetVersion());

        if (packet) {
          client->QueuePacket(packet);
        }
      }
    }
  }

  if (effect_bitmask & EFFECT_FLAG_SPELLBONUS) {
    RemoveSpellBonus(spell);
  }

  if (effect_bitmask & EFFECT_FLAG_STEALTH) {
    RemoveStealthSpell(spell);
  }

  if (effect_bitmask & EFFECT_FLAG_INVIS) {
    RemoveInvisSpell(spell);
  }
}

void Entity::RemoveSkillBonus(int32 spell_id) {
  //This is a virtual, just making it so we don't have to do extra checks for player/npcs
  return;
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

#include "WorldDatabase.h"
extern WorldDatabase database;
void Entity::CustomizeAppearance(PacketStruct* packet) {

  bool is_soga = packet->getType_int8_ByName("is_soga") == 1 ? true : false;
  int16 model_id = database.GetAppearanceID(packet->getType_EQ2_16BitString_ByName("race_file").data);
  EQ2_Color skin_color = packet->getType_EQ2_Color_ByName("skin_color");
  EQ2_Color skin_color2 = packet->getType_EQ2_Color_ByName("skin_color2");
  EQ2_Color eye_color = packet->getType_EQ2_Color_ByName("eye_color");
  EQ2_Color hair_color1 = packet->getType_EQ2_Color_ByName("hair_color1");
  EQ2_Color hair_color2 = packet->getType_EQ2_Color_ByName("hair_color2");
  EQ2_Color hair_highlight = packet->getType_EQ2_Color_ByName("hair_highlight");
  int16 hair_id = database.GetAppearanceID(packet->getType_EQ2_16BitString_ByName("hair_file").data);
  EQ2_Color hair_type_color = packet->getType_EQ2_Color_ByName("hair_type_color");
  EQ2_Color hair_type_highlight_color = packet->getType_EQ2_Color_ByName("hair_type_highlight_color");
  int16 face_id = database.GetAppearanceID(packet->getType_EQ2_16BitString_ByName("face_file").data);
  EQ2_Color hair_face_color = packet->getType_EQ2_Color_ByName("hair_face_color");
  EQ2_Color hair_face_highlight_color = packet->getType_EQ2_Color_ByName("hair_face_highlight_color");
  int16 wing_id = database.GetAppearanceID(packet->getType_EQ2_16BitString_ByName("wing_file").data);
  EQ2_Color wing_color1 = packet->getType_EQ2_Color_ByName("wing_color1");
  EQ2_Color wing_color2 = packet->getType_EQ2_Color_ByName("wing_color2");
  int16 chest_id = database.GetAppearanceID(packet->getType_EQ2_16BitString_ByName("chest_file").data);
  EQ2_Color shirt_color = packet->getType_EQ2_Color_ByName("shirt_color");
  EQ2_Color unknown_chest_color = packet->getType_EQ2_Color_ByName("unknown_chest_color");
  int16 legs_id = database.GetAppearanceID(packet->getType_EQ2_16BitString_ByName("legs_file").data);
  EQ2_Color pants_color = packet->getType_EQ2_Color_ByName("pants_color");
  EQ2_Color unknown_legs_color = packet->getType_EQ2_Color_ByName("unknown_legs_color");
  EQ2_Color unknown2 = packet->getType_EQ2_Color_ByName("unknown2");

  float eyes2[3];
  eyes2[0] = packet->getType_float_ByName("eyes2", 0) * 100;
  eyes2[1] = packet->getType_float_ByName("eyes2", 1) * 100;
  eyes2[2] = packet->getType_float_ByName("eyes2", 2) * 100;

  float ears[3];
  ears[0] = packet->getType_float_ByName("ears", 0) * 100;
  ears[1] = packet->getType_float_ByName("ears", 1) * 100;
  ears[2] = packet->getType_float_ByName("ears", 2) * 100;

  float eye_brows[3];
  eye_brows[0] = packet->getType_float_ByName("eye_brows", 0) * 100;
  eye_brows[1] = packet->getType_float_ByName("eye_brows", 1) * 100;
  eye_brows[2] = packet->getType_float_ByName("eye_brows", 2) * 100;

  float cheeks[3];
  cheeks[0] = packet->getType_float_ByName("cheeks", 0) * 100;
  cheeks[1] = packet->getType_float_ByName("cheeks", 1) * 100;
  cheeks[2] = packet->getType_float_ByName("cheeks", 2) * 100;

  float lips[3];
  lips[0] = packet->getType_float_ByName("lips", 0) * 100;
  lips[1] = packet->getType_float_ByName("lips", 1) * 100;
  lips[2] = packet->getType_float_ByName("lips", 2) * 100;

  float chin[3];
  chin[0] = packet->getType_float_ByName("chin", 0) * 100;
  chin[1] = packet->getType_float_ByName("chin", 1) * 100;
  chin[2] = packet->getType_float_ByName("chin", 2) * 100;

  float nose[3];
  nose[0] = packet->getType_float_ByName("nose", 0) * 100;
  nose[1] = packet->getType_float_ByName("nose", 1) * 100;
  nose[2] = packet->getType_float_ByName("nose", 2) * 100;

  sint8 body_size = (sint8)(packet->getType_float_ByName("body_size") * 100);
  sint8 body_age = (sint8)(packet->getType_float_ByName("body_age") * 100);

  if (is_soga) {
    appearance.soga_model_type = model_id;
    features.soga_skin_color = skin_color;
    features.soga_eye_color = eye_color;
    features.soga_hair_color1 = hair_color1;
    features.soga_hair_color2 = hair_color2;
    features.soga_hair_highlight_color = hair_highlight;
    features.soga_hair_type = hair_id;
    features.soga_hair_type_color = hair_type_color;
    features.soga_hair_type_highlight_color = hair_type_highlight_color;
    features.soga_hair_face_type = face_id;
    features.soga_hair_face_color = hair_face_color;
    features.soga_hair_face_highlight_color = hair_face_highlight_color;
    features.wing_type = wing_id;
    features.wing_color1 = wing_color1;
    features.wing_color2 = wing_color2;
    features.soga_chest_type = chest_id;
    features.shirt_color = shirt_color;
    features.soga_legs_type = legs_id;
    features.pants_color = pants_color;
    features.soga_eye_type[0] = eyes2[0];
    features.soga_eye_type[1] = eyes2[1];
    features.soga_eye_type[2] = eyes2[2];
    features.soga_ear_type[0] = ears[0];
    features.soga_ear_type[0] = ears[1];
    features.soga_ear_type[0] = ears[2];
    features.soga_eye_brow_type[0] = eye_brows[0];
    features.soga_eye_brow_type[1] = eye_brows[1];
    features.soga_eye_brow_type[2] = eye_brows[2];
    features.soga_cheek_type[0] = cheeks[0];
    features.soga_cheek_type[1] = cheeks[1];
    features.soga_cheek_type[2] = cheeks[2];
    features.soga_lip_type[0] = lips[0];
    features.soga_lip_type[1] = lips[1];
    features.soga_lip_type[2] = lips[2];
    features.soga_chin_type[0] = chin[0];
    features.soga_chin_type[1] = chin[1];
    features.soga_chin_type[2] = chin[2];
    features.soga_nose_type[0] = nose[0];
    features.soga_nose_type[1] = nose[1];
    features.soga_nose_type[2] = nose[2];
  } else {
    appearance.model_type = model_id;
    features.skin_color = skin_color;
    features.eye_color = eye_color;
    features.hair_color1 = hair_color1;
    features.hair_color2 = hair_color2;
    features.hair_highlight_color = hair_highlight;
    features.hair_type = hair_id;
    features.hair_type_color = hair_type_color;
    features.hair_type_highlight_color = hair_type_highlight_color;
    features.hair_face_type = face_id;
    features.hair_face_color = hair_face_color;
    features.hair_face_highlight_color = hair_face_highlight_color;
    features.wing_type = wing_id;
    features.wing_color1 = wing_color1;
    features.wing_color2 = wing_color2;
    features.chest_type = chest_id;
    features.shirt_color = shirt_color;
    features.legs_type = legs_id;
    features.pants_color = pants_color;
    features.eye_type[0] = eyes2[0];
    features.eye_type[1] = eyes2[1];
    features.eye_type[2] = eyes2[2];
    features.ear_type[0] = ears[0];
    features.ear_type[0] = ears[1];
    features.ear_type[0] = ears[2];
    features.eye_brow_type[0] = eye_brows[0];
    features.eye_brow_type[1] = eye_brows[1];
    features.eye_brow_type[2] = eye_brows[2];
    features.cheek_type[0] = cheeks[0];
    features.cheek_type[1] = cheeks[1];
    features.cheek_type[2] = cheeks[2];
    features.lip_type[0] = lips[0];
    features.lip_type[1] = lips[1];
    features.lip_type[2] = lips[2];
    features.chin_type[0] = chin[0];
    features.chin_type[1] = chin[1];
    features.chin_type[2] = chin[2];
    features.nose_type[0] = nose[0];
    features.nose_type[1] = nose[1];
    features.nose_type[2] = nose[2];
  }

  features.body_size = body_size;
  features.body_age = body_age;

  AddSpawnUpdate(true, false, false);
}

void Entity::AddSkillBonus(int32 spell_id, int32 skill_id, float value) {
  // handled in npc or player
  return;
}

float Entity::GetSpellMitigationPercentage(int enemy_level, int8 damage_type) {
  int resist_value = 0;
  switch (damage_type) {
  case DAMAGE_PACKET_DAMAGE_TYPE_HEAT:
    resist_value = GetInfoStruct()->heat;
    break;

  case DAMAGE_PACKET_DAMAGE_TYPE_COLD:
    resist_value = GetInfoStruct()->cold;
    break;

  case DAMAGE_PACKET_DAMAGE_TYPE_MAGIC:
    resist_value = GetInfoStruct()->magic;
    break;

  case DAMAGE_PACKET_DAMAGE_TYPE_MENTAL:
    resist_value = GetInfoStruct()->mental;
    break;

  case DAMAGE_PACKET_DAMAGE_TYPE_DIVINE:
    resist_value = GetInfoStruct()->divine;
    break;

  case DAMAGE_PACKET_DAMAGE_TYPE_DISEASE:
    resist_value = GetInfoStruct()->disease;
    break;

  case DAMAGE_PACKET_DAMAGE_TYPE_POISON:
    resist_value = GetInfoStruct()->poison;
    break;
  }

  return 0.6 * resist_value / static_cast<float>((enemy_level - GetLevel()) * 25 + resist_value + 400);
}
