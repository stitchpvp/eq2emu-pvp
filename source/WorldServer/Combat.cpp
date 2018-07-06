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
#include "Combat.h"
#include "client.h"
#include "../common/ConfigReader.h"
#include "classes.h"
#include "../common/debug.h"
#include "../common/Log.h"
#include "zoneserver.h"
#include "Skills.h"
#include "classes.h"
#include "World.h"
#include "LuaInterface.h"
#include "Rules/Rules.h"
#include "SpellProcess.h"
#include <math.h>
#include "PVP.h"

extern Classes classes;
extern ConfigReader configReader;
extern MasterSkillList master_skill_list;
extern RuleManager rule_manager;
extern LuaInterface* lua_interface;

/* ******************************************************************************

DamageSpawn() - Damage equation
MeleeAttack() - Melee auto attacks
RangeAttack() - Range auto attacks
DetermineHit() - ToHit chance as well as defender parry / dodge / block / riposte
CheckInterruptSpell() - Interrupt equations


No mitigation equations yet

****************************************************************************** */

/*			New Combat code				*/

bool Entity::PrimaryWeaponReady() {
	//Can only be ready if no ranged timer
	if(GetPrimaryLastAttackTime() == 0 || (Timer::GetCurrentTime2() >= (GetPrimaryLastAttackTime() + GetPrimaryAttackDelay())))
		if (GetRangeLastAttackTime() == 0 || Timer::GetCurrentTime2() >= GetRangeLastAttackTime() + GetRangeAttackDelay())
			return true;

	return false;
}

bool Entity::SecondaryWeaponReady() {
	//Can only be ready if no ranged timer
	if(IsDualWield() && (GetSecondaryLastAttackTime() == 0 || (Timer::GetCurrentTime2() >= (GetSecondaryLastAttackTime() + GetSecondaryAttackDelay())))) {
		if(GetRangeLastAttackTime() == 0 || Timer::GetCurrentTime2() >= GetRangeLastAttackTime() + GetRangeAttackDelay())
			return true;
	}

	return false;
}

bool Entity::RangeWeaponReady() {
	//Ranged can only be ready if no other attack timers are active
	if(GetRangeLastAttackTime() == 0 || (Timer::GetCurrentTime2() >= GetRangeLastAttackTime() + GetRangeAttackDelay())) {
		if((GetPrimaryLastAttackTime() == 0 || (Timer::GetCurrentTime2() >= GetPrimaryLastAttackTime() + GetPrimaryAttackDelay())) && (GetSecondaryLastAttackTime() == 0  || Timer::GetCurrentTime2() >= GetSecondaryLastAttackTime() + GetSecondaryAttackDelay())){
			if(!IsPlayer() || ((Player*)this)->GetRangeAttack()) {
				return true;
			}
		}
	}

	return false;
}

bool Entity::AttackAllowed(Entity* target, float distance, bool range_attack) {
	Entity* attacker = this;
	shared_ptr<Client> client = nullptr;

	if (!target || IsMezzedOrStunned() || IsDazed()) {
		LogWrite(COMBAT__DEBUG, 3, "AttackAllowed", "Failed to attack: no target, mezzed, stunned or dazed");
		return false;
	}

	if (IsPlayer()) {
		client = GetZone()->GetClientBySpawn(this);
	}

	if (IsPet()) {
		attacker = ((NPC*)this)->GetOwner();
	}

	if (target->IsNPC() && ((NPC*)target)->IsPet()) {
		if (((NPC*)target)->GetOwner()) {
			target = ((NPC*)target)->GetOwner();
		}
	}

	if (attacker == target) {
		LogWrite(COMBAT__DEBUG, 3, "AttackAllowed", "Failed to attack: attacker tried to attack himself or his pet.");
		return false;
	}

	if (!attacker->CanAttackTarget(target)) {
		LogWrite(COMBAT__DEBUG, 3, "AttackAllowed", "Failed to attack: target is not attackable");
		return false;
	}

	if (IsPlayer() && target->IsBot()) {
		LogWrite(COMBAT__DEBUG, 3, "AttackAllowed", "Failed to attack: players are not allowed to attack bots");
		return false;
	}

	if (!target->Alive()) {
		LogWrite(COMBAT__DEBUG, 3, "AttackAllowed", "Failed to attack: target is dead");
		return false;
	}

	if (!FacingTarget(target)) {
		LogWrite(COMBAT__DEBUG, 3, "AttackAllowed", "Failed to attack: not facing target");
		return false;
	}

	if (range_attack && distance != 0) {
		Item* weapon = 0;
		Item* ammo = 0;
		if(attacker->IsPlayer()) {
				weapon = ((Player*)attacker)->GetEquipmentList()->GetItem(EQ2_RANGE_SLOT);
				ammo = ((Player*)attacker)->GetEquipmentList()->GetItem(EQ2_AMMO_SLOT);
		}
		if(weapon && weapon->IsRanged() && ammo && ammo->IsAmmo() && ammo->IsThrown()) {
			// Distance is less then min weapon range
			if(distance < weapon->ranged_info->range_low) {
				if (client)
					client->SimpleMessage(CHANNEL_COLOR_COMBAT, "Your target is too close! Move back!");
				LogWrite(COMBAT__DEBUG, 3, "AttackAllowed", "Failed to attack: range attack, target to close");
				return false;
			}
			// Distance is greater then max weapon range
			if  (distance > (weapon->ranged_info->range_high + ammo->thrown_info->range)) {
				if (client)
					client->SimpleMessage(CHANNEL_COLOR_COMBAT, "Your target is too far away! Move closer!");
				LogWrite(COMBAT__DEBUG, 3, "AttackAllowed", "Failed to attack: range attack, target is to far");
				return false;
			}
		}
	}
	else if (distance != 0) {
		if(distance > MAX_COMBAT_RANGE) {
			LogWrite(COMBAT__DEBUG, 3, "AttackAllowed", "Failed to attack: distance is beyond melee range");
			return false;
		}
	}
	LogWrite(MISC__TODO, 3, "TODO", "Add more AttackAllowed calculations\n\t(%s, function: %s, line #: %i)", __FILE__, __FUNCTION__, __LINE__);
	return true;
}

void Entity::MeleeAttack(Spawn* victim, float distance, bool primary, bool multi_attack) {
	if (!victim)
		return;

	int8 damage_type = 0;
	int32 min_damage = 0;
	int32 max_damage = 0;

	if (primary) {
		damage_type = GetPrimaryWeaponType();
		min_damage = GetPrimaryWeaponMinDamage();
		max_damage = GetPrimaryWeaponMaxDamage();
	} else {
		damage_type = GetSecondaryWeaponType();
		min_damage = GetSecondaryWeaponMinDamage();
		max_damage = GetSecondaryWeaponMaxDamage();
	}

	if (IsStealthed() || IsInvis()) {
		CancelAllStealth();
	}
		
	int8 hit_result = DetermineHit(victim, damage_type, 0, false);

	if (hit_result == DAMAGE_PACKET_RESULT_SUCCESSFUL) {
		DamageSpawn((Entity*)victim, DAMAGE_PACKET_TYPE_SIMPLE_DAMAGE, damage_type, min_damage, max_damage, 0);

		if (!multi_attack) {
			if (victim->IsEntity() && victim->Alive()) {
				static_cast<Entity*>(victim)->CheckProcs(PROC_TYPE_PHYSICAL_DEFENSIVE, this);
			}

			CheckProcs(PROC_TYPE_OFFENSIVE, victim);
			CheckProcs(PROC_TYPE_PHYSICAL_OFFENSIVE, victim);
		}
	} else {
		GetZone()->SendDamagePacket(this, victim, DAMAGE_PACKET_TYPE_SIMPLE_DAMAGE, hit_result, damage_type, 0, 0);

		if (hit_result == DAMAGE_PACKET_RESULT_RIPOSTE && victim->IsEntity()) {
			((Entity*)victim)->MeleeAttack(this, distance, true);
		}
	}

	if (!multi_attack) {
		float multi_attack = info_struct.multi_attack;

		if (multi_attack > 0) {
			float chance = multi_attack;

			if (multi_attack > 100) {
				int8 automatic_multi = (int8)floor((float)(multi_attack / 100));
				chance = (multi_attack - (floor((float) ((multi_attack / 100) * 100))));

				while (automatic_multi > 0) {
					MeleeAttack(victim, 100, primary, true);
					automatic_multi--;
				}
			}

			if (MakeRandomFloat(0, 100) <= chance) {
				MeleeAttack(victim, 100, primary, true);
			}
		}
	}

	if (!multi_attack) {
		SetAttackDelay(primary);
	}

	if (victim->IsNPC() && !victim->EngagedInCombat()) {
		((NPC*)victim)->AddHate(this, 50);
	}

	if (IsPlayer() && victim->IsPlayer()) {
		PVP::HandlePlayerEncounter(static_cast<Player*>(this), static_cast<Player*>(victim), true);
	}

	if (victim->IsEntity() && victim->Alive() && ((Entity*)victim)->HasPet()) {
		Entity* pet = 0;
		bool AddHate = false;

		if (victim->IsPlayer()) {
			if (((Player*)victim)->GetInfoStruct()->pet_behavior & 1) {
				AddHate = true;
			}
		} else {
			AddHate = true;
		}
		
		if (AddHate) {
			pet = ((Entity*)victim)->GetPet();

			if (pet) {
				pet->AddHate(this, 1);
			}

			pet = ((Entity*)victim)->GetCharmedPet();

			if (pet) {
				pet->AddHate(this, 1);
			}
		}
	}
}

void Entity::RangeAttack(Spawn* victim, float distance, Item* weapon, Item* ammo, bool multi_attack) {
	if (!victim) {
		return;
	}

	if (IsStealthed() || IsInvis()) {
		CancelAllStealth();
	}

	if (weapon && weapon->IsRanged() && ammo && ammo->IsAmmo() && ammo->IsThrown()) {
		if (weapon->ranged_info->range_low <= distance && (weapon->ranged_info->range_high + ammo->thrown_info->range) >= distance) {
			int8 hit_result = DetermineHit(victim, ammo->thrown_info->damage_type, ammo->thrown_info->hit_bonus, false);

			if (hit_result == DAMAGE_PACKET_RESULT_SUCCESSFUL) {
				DamageSpawn((Entity*)victim, DAMAGE_PACKET_TYPE_RANGE_DAMAGE, ammo->thrown_info->damage_type, weapon->ranged_info->weapon_info.damage_low3, weapon->ranged_info->weapon_info.damage_high3+ammo->thrown_info->damage_modifier, 0);

				if (!multi_attack) {
					if (victim->IsEntity() && victim->Alive()) {
						static_cast<Entity*>(victim)->CheckProcs(PROC_TYPE_PHYSICAL_DEFENSIVE, this);
						static_cast<Entity*>(victim)->CheckProcs(PROC_TYPE_RANGED_DEFENSE, this);
					}

					CheckProcs(PROC_TYPE_OFFENSIVE, victim);
					CheckProcs(PROC_TYPE_PHYSICAL_OFFENSIVE, victim);
					CheckProcs(PROC_TYPE_RANGED_ATTACK, victim);
				}
			} else {
				GetZone()->SendDamagePacket(this, victim, DAMAGE_PACKET_TYPE_RANGE_DAMAGE, hit_result, ammo->thrown_info->damage_type, 0, 0);
			}

			if (IsPlayer()) {
				if (ammo->details.count > 1) {
					ammo->details.count -= 1;
					ammo->save_needed = true;
				} else {
					((Player*)this)->equipment_list.RemoveItem(ammo->details.slot_id, true);
				}

				shared_ptr<Client> client = GetZone()->GetClientBySpawn(this);
				EQ2Packet* outapp = ((Player*)this)->GetEquipmentList()->serialize(client->GetVersion());
				if (outapp) {
					client->QueuePacket(outapp);
				}
			}

			if (victim->IsNPC() && !victim->EngagedInCombat()) {
				((NPC*)victim)->AddHate(this, 50);
			}

			if (IsPlayer() && victim->IsPlayer()) {
				PVP::HandlePlayerEncounter(static_cast<Player*>(this), static_cast<Player*>(victim), true);
			}

			if (victim->IsEntity() && victim->Alive() && ((Entity*)victim)->HasPet()) {
				Entity* pet = 0;
				bool AddHate = false;

				if (victim->IsPlayer()) {
					if (((Player*)victim)->GetInfoStruct()->pet_behavior & 1) {
						AddHate = true;
					}
				} else {
					AddHate = true;
				}

				if (AddHate) {
					pet = ((Entity*)victim)->GetPet();

					if (pet) {
						pet->AddHate(this, 1);
					}

					pet = ((Entity*)victim)->GetCharmedPet();

					if (pet) {
						pet->AddHate(this, 1);
					}
				}
			}

			SetRangeLastAttackTime(Timer::GetCurrentTime2());
		}
	}

	if (!multi_attack) {
		float multi_attack = info_struct.multi_attack;

		if (multi_attack > 0) {
			float chance = multi_attack;

			if (multi_attack > 100) {
				int8 automatic_multi = (int8)floor((float)(multi_attack / 100));
				chance = (multi_attack - (floor((float)(multi_attack / 100) * 100)));

				while (automatic_multi > 0) {
					RangeAttack(victim, 100, weapon, ammo, true);
					automatic_multi--;
				}
			}

			if (MakeRandomFloat(0, 100) <= chance) {
				RangeAttack(victim, 100, weapon, ammo, true);
			}
		}
	}

	if (!multi_attack) {
		SetAttackDelay(false, true);
	}
}

bool Entity::SpellAttack(Spawn* victim, float distance, shared_ptr<LuaSpell> luaspell, int8 damage_type, int32 low_damage, int32 high_damage, int8 crit_mod, bool no_calcs){
	if (!victim || !luaspell || !luaspell->spell) {
		return false;
	}

	Spell* spell = luaspell->spell;
	Skill* skill = nullptr;
	float bonus = 0;
	int8 hit_result = 0;

	luaspell->last_spellattack_hit = true;

	bool is_tick = GetZone()->GetSpellProcess()->HasActiveSpell(luaspell, false);

	if (spell->GetSpellData()->resistibility > 0) {
		bonus -= (1 - spell->GetSpellData()->resistibility) * 100;
	}

	skill = master_skill_list.GetSkill(spell->GetSpellData()->mastery_skill);

	if (skill) {
		skill = GetSkillByName(skill->name.data.c_str(), true);

		if (skill) {
			bonus += skill->current_val / 25;
		}
	}

	if (is_tick) {
		if (luaspell->crit) {
			crit_mod = 1;
		} else {
			crit_mod = 2;
		}
	}

	luaspell->crit = DamageSpawn((Entity*)victim, DAMAGE_PACKET_TYPE_SPELL_DAMAGE, damage_type, low_damage, high_damage, spell->GetName(), crit_mod, is_tick, no_calcs);

	if (!is_tick) {
		CheckProcs(PROC_TYPE_OFFENSIVE, victim);

		if (spell->GetSpellData()->spell_book_type == SPELL_BOOK_TYPE_SPELL) {
			CheckProcs(PROC_TYPE_MAGICAL_OFFENSIVE, victim);

			if (victim->IsEntity() && victim->Alive()) {
				static_cast<Entity*>(victim)->CheckProcs(PROC_TYPE_MAGICAL_DEFENSIVE, this);
			}
		} else if (spell->GetSpellData()->spell_book_type == SPELL_BOOK_TYPE_COMBAT_ART) {
			CheckProcs(PROC_TYPE_PHYSICAL_OFFENSIVE, victim);

			if (victim->IsEntity() && victim->Alive()) {
				static_cast<Entity*>(victim)->CheckProcs(PROC_TYPE_PHYSICAL_DEFENSIVE, this);
			}
		}
	}

	if (victim->IsEntity() && victim->Alive() && ((Entity*)victim)->HasPet()) {
		Entity* pet = 0;
		bool AddHate = false;

		if (victim->IsPlayer()) {
			if (((Player*)victim)->GetInfoStruct()->pet_behavior & 1) {
				AddHate = true;
			}
		} else {
			AddHate = true;
		}

		if (AddHate) {
			pet = ((Entity*)victim)->GetPet();

			if (pet) {
				pet->AddHate(this, 1);
			}

			pet = ((Entity*)victim)->GetCharmedPet();

			if (pet) {
				pet->AddHate(this, 1);
			}
		}
	}

	return true;
}

bool Entity::ProcAttack(Spawn* victim, int8 damage_type, int32 low_damage, int32 high_damage, string name, string success_msg, string effect_msg, bool perform_calcs) {
	int8 hit_result = DetermineHit(victim, damage_type, 0, true);

	if (hit_result == DAMAGE_PACKET_RESULT_SUCCESSFUL) {
		DamageSpawn(static_cast<Entity*>(victim), DAMAGE_PACKET_TYPE_SPELL_DAMAGE, damage_type, low_damage, high_damage, name.c_str(), 0, !perform_calcs);

		last_proc_hit = true;

		if (IsPlayer() && success_msg.length()) {
			shared_ptr<Client> client = GetZone()->GetClientBySpawn(this);

			if (client) {
				if (success_msg.find("%t") < 0xFFFFFFFF) {
					success_msg.replace(success_msg.find("%t"), 2, victim->GetName());
				}

				client->Message(CHANNEL_COLOR_SPELL, success_msg.c_str());
			}
		}

		if (effect_msg.length()) {
			if (effect_msg.find("%t") < 0xFFFFFFFF) {
				effect_msg.replace(effect_msg.find("%t"), 2, victim->GetName());
			}

			GetZone()->SimpleMessage(CHANNEL_COLOR_SPELL_EFFECT, effect_msg.c_str(), victim, 50);
		}
	} else {
		if (victim->IsNPC()) {
			((NPC*)victim)->AddHate(this, 5);
		}

		GetZone()->SendDamagePacket(this, victim, DAMAGE_PACKET_TYPE_SPELL_DAMAGE, hit_result, damage_type, 0, name.c_str());

		last_proc_hit = false;
	}
	

	if (victim->IsEntity() && victim->Alive() && static_cast<Entity*>(victim)->HasPet()) {
		bool AddHate = false;

		if (victim->IsPlayer()) {
			if (static_cast<Player*>(victim)->GetInfoStruct()->pet_behavior & 1) {
				AddHate = true;
			}
		} else {
			AddHate = true;
		}

		if (AddHate) {
			Entity* pet = static_cast<Entity*>(victim)->GetPet();

			if (pet) {
				pet->AddHate(this, 1);
			}

			pet = static_cast<Entity*>(victim)->GetCharmedPet();

			if (pet) {
				pet->AddHate(this, 1);
			}
		}
	}

	return true;
}

bool Entity::SpellHeal(Spawn* target, float distance, shared_ptr<LuaSpell> luaspell, string heal_type, int32 low_heal, int32 high_heal, int8 crit_mod, bool no_calcs) {
	if (!target || !luaspell || !luaspell->spell) {
		return false;
	}

	bool is_tick = GetZone()->GetSpellProcess()->HasActiveSpell(luaspell, false);

	if (!is_tick) {
		if (luaspell->crit) {
			crit_mod = 1;
		} else {
			crit_mod = 2;
		}

		CheckProcs(PROC_TYPE_HEALING, target);
		CheckProcs(PROC_TYPE_BENEFICIAL, target);
	}

	luaspell->crit = HealSpawn(target, heal_type, low_heal, high_heal, luaspell->spell->GetName(), crit_mod, is_tick, !no_calcs);

	return true;
}

bool Entity::ProcHeal(Spawn* target, string heal_type, int32 low_heal, int32 high_heal, string name, bool perform_calcs) {
	HealSpawn(target, heal_type, low_heal, high_heal, name.c_str(), 2, false, perform_calcs);

	return true;
}

int8 Entity::DetermineHit(Spawn* victim, int8 damage_type, float ToHitBonus, bool spell){
	if(!victim) {
		return DAMAGE_PACKET_RESULT_MISS;
	}

	if(victim->GetInvulnerable()) {
		return DAMAGE_PACKET_RESULT_INVULNERABLE;
	}

	if(!victim->IsEntity() || (!spell && BehindTarget(victim))) {
		return DAMAGE_PACKET_RESULT_SUCCESSFUL;
	}

	float bonus = ToHitBonus;
	Skill* skill = GetSkillByWeaponType(damage_type, true);
	if (skill)
		bonus += skill->current_val / 25;
	if (victim->IsEntity())
		bonus -= ((Entity*)victim)->GetDamageTypeResistPercentage(damage_type);


	Entity* entity_victim = (Entity*)victim;
	float chance = 90 + bonus; // 90% base chance that the victim will get hit (plus bonus)
	sint16 roll_chance = 100;
	if(skill)
		roll_chance -= skill->current_val / 25;

	if(!spell){ // melee or range attack		
		skill = GetSkillByName("Offense", true); //add this skill for NPCs
		if(skill)
			roll_chance -= skill->current_val / 25;

		if(rand()%roll_chance >= (chance - entity_victim->GetInfoStruct()->base_avoidance_bonus - entity_victim->GetAgi() / 125)){
			entity_victim->CheckProcs(PROC_TYPE_EVADE, this);
			return DAMAGE_PACKET_RESULT_DODGE;//successfully dodged
		}
		if(rand() % roll_chance >= chance)
			return DAMAGE_PACKET_RESULT_MISS; //successfully avoided

		skill = entity_victim->GetSkillByName("Parry", true);
		if(skill){
			if(rand()%roll_chance >= (chance - 5 - skill->current_val/25)){ //successful parry
				if(rand()%100 <= 20) {
					entity_victim->CheckProcs(PROC_TYPE_RIPOSTE, this);
					return DAMAGE_PACKET_RESULT_RIPOSTE;
				}
				entity_victim->CheckProcs(PROC_TYPE_PARRY, this);
				return DAMAGE_PACKET_RESULT_PARRY;
			}
		}

		skill = entity_victim->GetSkillByName("Deflection", true);
		if(skill){
			if(rand()%100 >= (chance - entity_victim->GetInfoStruct()->minimum_deflection_chance - skill->current_val/25)) { //successfully deflected
				return DAMAGE_PACKET_RESULT_DEFLECT;
			}
		}
	}
	else{
		skill = entity_victim->GetSkillByName("Spell Avoidance", true);
		if(skill)
			chance -= skill->current_val / 25;
		if(rand()%roll_chance >= chance) {
			return DAMAGE_PACKET_RESULT_RESIST; //successfully resisted	
		}
	}

	return DAMAGE_PACKET_RESULT_SUCCESSFUL;
}

float Entity::GetDamageTypeResistPercentage(int8 damage_type) {
	float ret = 1;

	switch(damage_type) {
	case DAMAGE_PACKET_DAMAGE_TYPE_CRUSH:
	case DAMAGE_PACKET_DAMAGE_TYPE_PIERCE:
	case DAMAGE_PACKET_DAMAGE_TYPE_SLASH: {
		Skill* skill = GetSkillByName("Defense", true);
		if(skill)
			ret += skill->current_val / 25;
		if(IsNPC())
			LogWrite(COMBAT__DEBUG, 3, "Combat", "DamageType: Crush/Pierce/Slash (%i)", damage_type, ret);
		break;
										  }
	case DAMAGE_PACKET_DAMAGE_TYPE_HEAT: {
		ret += GetInfoStruct()->heat / 50;
		LogWrite(COMBAT__DEBUG, 3, "Combat", "DamageType: Heat (%i), Amt: %.2f", damage_type, ret);
		break;
										 }
	case DAMAGE_PACKET_DAMAGE_TYPE_COLD: {
		ret += GetInfoStruct()->cold / 50;
		LogWrite(COMBAT__DEBUG, 3, "Combat", "DamageType: Cold (%i), Amt: %.2f", damage_type, ret);
		break;
										 }
	case DAMAGE_PACKET_DAMAGE_TYPE_MAGIC: {
		ret += GetInfoStruct()->magic / 50;
		LogWrite(COMBAT__DEBUG, 3, "Combat", "DamageType: Magic (%i), Amt: %.2f", damage_type, ret);
		break;
										  }
	case DAMAGE_PACKET_DAMAGE_TYPE_MENTAL: {
		ret += GetInfoStruct()->mental / 50;
		LogWrite(COMBAT__DEBUG, 3, "Combat", "DamageType: Mental (%i), Amt: %.2f", damage_type, ret);
		break;
										   }
	case DAMAGE_PACKET_DAMAGE_TYPE_DIVINE: {
		ret += GetInfoStruct()->divine / 50;
		LogWrite(COMBAT__DEBUG, 3, "Combat", "DamageType: Divine (%i), Amt: %.2f", damage_type, ret);
		break;
										   }
	case DAMAGE_PACKET_DAMAGE_TYPE_DISEASE: {
		ret += GetInfoStruct()->disease / 50;
		LogWrite(COMBAT__DEBUG, 3, "Combat", "DamageType: Disease (%i), Amt: %.2f", damage_type, ret);
		break;
											}
	case DAMAGE_PACKET_DAMAGE_TYPE_POISON: {
		ret += GetInfoStruct()->poison / 50;
		LogWrite(COMBAT__DEBUG, 3, "Combat", "DamageType: Poison (%i), Amt: %.2f", damage_type, ret);
		break;
										   }
	}

	return ret;
}

Skill* Entity::GetSkillByWeaponType(int8 type, bool update) {
	switch(type) {
	case DAMAGE_PACKET_DAMAGE_TYPE_SLASH: // slash
		return GetSkillByName("Slashing", update);
	case DAMAGE_PACKET_DAMAGE_TYPE_CRUSH: // crush
		return GetSkillByName("Crushing", update);
	case DAMAGE_PACKET_DAMAGE_TYPE_PIERCE: // pierce
		return GetSkillByName("Piercing", update);
	case DAMAGE_PACKET_DAMAGE_TYPE_HEAT:
	case DAMAGE_PACKET_DAMAGE_TYPE_COLD:
	case DAMAGE_PACKET_DAMAGE_TYPE_MAGIC:
	case DAMAGE_PACKET_DAMAGE_TYPE_MENTAL:
	case DAMAGE_PACKET_DAMAGE_TYPE_DIVINE:
	case DAMAGE_PACKET_DAMAGE_TYPE_DISEASE:
	case DAMAGE_PACKET_DAMAGE_TYPE_POISON:
		return GetSkillByName("Disruption", update);
	}

	return 0;
}

bool Entity::DamageSpawn(Entity* victim, int8 type, int8 damage_type, int32 low_damage, int32 high_damage, const char* spell_name, int8 crit_mod, bool is_tick, bool no_calcs) {
	if (!victim || !victim->Alive()) {
		return false;
	}

	int8 hit_result = 0;
	int16 blow_type = 0;
	sint32 damage = 0;
	bool crit = false;

	if (low_damage > high_damage) {
		high_damage = low_damage;
	}

	if (low_damage == high_damage) {
		damage = low_damage;
	} else {
		damage = MakeRandomInt(low_damage, high_damage);
	}

	if (!no_calcs) {
		if (type == DAMAGE_PACKET_TYPE_SIMPLE_DAMAGE || type == DAMAGE_PACKET_TYPE_RANGE_DAMAGE ) {
			//DPS mod is only applied to auto attacks
			damage *= max(1.0f, info_struct.dps_multiplier);
		} else {
			damage *= 1 + (info_struct.base_ability_modifier / 100.0);
			damage = ApplyPotency(damage);
			damage = ApplyAbilityMod(damage);
		}


		if (crit_mod == 1) {
			crit = true;
		} else {
			float chance = max((float)0, (info_struct.crit_chance - victim->stats[ITEM_STAT_CRITAVOIDANCE]));

			if (MakeRandomFloat(0, 100) <= chance) {
				crit = true;
			}
		}

		if (crit) {
			if (info_struct.crit_bonus > 0) {
				damage *= (1.3 + (info_struct.crit_bonus / 100));
			} else {
				damage *= 1.3;
			}

			// Change packet type to crit
			if (type == DAMAGE_PACKET_TYPE_SIMPLE_DAMAGE) {
				type = DAMAGE_PACKET_TYPE_SIMPLE_CRIT_DMG;
			} else if (type == DAMAGE_PACKET_TYPE_RANGE_DAMAGE) {
				type = DAMAGE_PACKET_TYPE_RANGE_CRIT_DMG;
			} else if (type == DAMAGE_PACKET_TYPE_SPELL_DAMAGE) {
				type = DAMAGE_PACKET_TYPE_SPELL_CRIT_DMG;
			}
		}
	}

	damage = victim->CheckStoneskins(damage, this);

	// Rudimentary mitigation
	if (damage_type == DAMAGE_PACKET_DAMAGE_TYPE_SLASH || damage_type == DAMAGE_PACKET_DAMAGE_TYPE_CRUSH || damage_type == DAMAGE_PACKET_DAMAGE_TYPE_PIERCE) {
		damage *= 1 - victim->GetMitigationPercentage(GetLevel());
		damage *= 1 - victim->GetInfoStruct()->physical_damage_reduction / 100.0;
	} else if (damage_type == DAMAGE_PACKET_DAMAGE_TYPE_DISEASE || damage_type == DAMAGE_PACKET_DAMAGE_TYPE_POISON || damage_type == DAMAGE_PACKET_DAMAGE_TYPE_HEAT || damage_type == DAMAGE_PACKET_DAMAGE_TYPE_COLD || damage_type == DAMAGE_PACKET_DAMAGE_TYPE_MAGIC || damage_type == DAMAGE_PACKET_DAMAGE_TYPE_DIVINE) {
		damage *= 1 - victim->GetSpellMitigationPercentage(GetLevel(), damage_type);
	}

	if (damage <= 0) {
		hit_result = DAMAGE_PACKET_RESULT_NO_DAMAGE;
		damage = 0;
	} else {
		hit_result = DAMAGE_PACKET_RESULT_SUCCESSFUL;

		damage = victim->CheckWards(damage, damage_type);

		victim->TakeDamage(damage);

		GetZone()->SendDamagePacket(this, victim, type, hit_result, damage_type, damage, spell_name);	

		if (victim->Alive()) {
			victim->CheckProcs(PROC_TYPE_DAMAGED, this);
		}

		GetZone()->CallSpawnScript(victim, SPAWN_SCRIPT_HEALTHCHANGED, this);

		if (IsPlayer()) {
			switch (damage_type) {
			case DAMAGE_PACKET_DAMAGE_TYPE_SLASH:
			case DAMAGE_PACKET_DAMAGE_TYPE_CRUSH:
			case DAMAGE_PACKET_DAMAGE_TYPE_PIERCE:
				if (((Player*)this)->GetPlayerStatisticValue(STAT_PLAYER_HIGHEST_MELEE_HIT) < damage) {
					((Player*)this)->UpdatePlayerStatistic(STAT_PLAYER_HIGHEST_MELEE_HIT, damage, true);
				}

				if (victim->Alive()) {
					victim->CheckProcs(PROC_TYPE_DAMAGED_MELEE, this);
				}

				break;
			case DAMAGE_PACKET_DAMAGE_TYPE_HEAT:
			case DAMAGE_PACKET_DAMAGE_TYPE_COLD:
			case DAMAGE_PACKET_DAMAGE_TYPE_MAGIC:
			case DAMAGE_PACKET_DAMAGE_TYPE_MENTAL:
			case DAMAGE_PACKET_DAMAGE_TYPE_DIVINE:
			case DAMAGE_PACKET_DAMAGE_TYPE_DISEASE:
			case DAMAGE_PACKET_DAMAGE_TYPE_POISON:
				if (((Player*)this)->GetPlayerStatisticValue(STAT_PLAYER_HIGHEST_MAGIC_HIT) < damage) {
					((Player*)this)->UpdatePlayerStatistic(STAT_PLAYER_HIGHEST_MAGIC_HIT, damage, true);
				}

				if (victim->Alive()) {
					victim->CheckProcs(PROC_TYPE_DAMAGED_MAGIC, this);
				}

				break;
			}
		}
	}

	if (victim->IsNPC() && victim->Alive()) {
		((Entity*)victim)->AddHate(this, damage);
	}

	if (victim->IsEntity()) {
		((Entity*)victim)->CheckInterruptSpell(this);
	}

	if ((!is_tick && victim->IsStealthed()) || victim->IsInvis()) {
		victim->CancelAllStealth();
	}

	if (!victim->Alive()) {
		KillSpawn(victim, damage_type, blow_type);
	}

	if (!is_tick && victim->EngagedInCombat()) {
		victim->CheckProcs(PROC_TYPE_DEFENSIVE, this);
	}

	return crit;
}

bool Entity::HealSpawn(Spawn* target, string heal_type, int32 low_heal, int32 high_heal, const char* spell_name, int8 crit_mod, bool is_tick, bool perform_calcs) {
	if (!target) {
		return false;
	}

	int32 heal_amt = 0;
	bool crit = false;

	if (high_heal < low_heal) {
		high_heal = low_heal;
	}

	if (high_heal == low_heal) {
		heal_amt = high_heal;
	} else {
		heal_amt = MakeRandomInt(low_heal, high_heal);
	}

	if (perform_calcs) {
		if (heal_amt > 0) {
			heal_amt *= 1 + (info_struct.base_ability_modifier / 100.0);
			heal_amt = ApplyPotency(heal_amt);
			heal_amt = ApplyAbilityMod(heal_amt);
		}

		if (!crit_mod || crit_mod == 1) {
			if (crit_mod == 1) {
				crit = true;
			} else {
				float chance = max(0.0f, info_struct.crit_chance);
				crit = (MakeRandomFloat(0, 100) <= chance); 
			}

			if (crit) {
				heal_amt *= (info_struct.crit_bonus / 100) + 1.3;
			}
		}
	}

	int16 type = 0;

	if (heal_type == "Heal") {
		if (target->GetHP() == target->GetTotalHP()) {
			return crit;
		}

		if (crit) {
			type = HEAL_PACKET_TYPE_CRIT_HEAL;
		} else {
			type = HEAL_PACKET_TYPE_SIMPLE_HEAL;
		}

		if (target->GetHP() + heal_amt > target->GetTotalHP()) {
			heal_amt = target->GetTotalHP() - target->GetHP();
		}

		target->SetHP(target->GetHP() + heal_amt);
	} else if (heal_type == "Power") {
		if (target->GetPower() == target->GetTotalPower()) {
			return crit;
		}

		if (crit) {
			type = HEAL_PACKET_TYPE_CRIT_MANA;
		} else {
			type = HEAL_PACKET_TYPE_SIMPLE_MANA;
		}

		if (target->GetPower() + heal_amt > target->GetTotalPower()) {
			heal_amt = target->GetTotalPower() - target->GetPower();
		}

		target->SetPower(GetPower() + heal_amt);
	} else {
		return crit;
	}

	target->GetZone()->TriggerCharSheetTimer();

	if (heal_amt > 0) {
		GetZone()->SendHealPacket(this, target, type, heal_amt, spell_name);

		if (target->IsEntity()) {
			int32 hate_amt = heal_amt * 0.25;
			set<int32>::iterator itr;
			for (itr = ((Entity*)target)->HatedBy.begin(); itr != ((Entity*)target)->HatedBy.end(); itr++) {
				Spawn* spawn = GetZone()->GetSpawnByID(*itr);
				if (spawn && spawn->IsEntity()) {
					((Entity*)spawn)->AddHate(this, hate_amt);
				}
			}
		}
	}

	return crit;
}

void Entity::AddHate(Entity* attacker, sint32 hate, bool unprovoked) {
	if (!attacker || !Alive() || !attacker->Alive()) {
		return;
	}

	// If a players pet and protect self is off
	if (IsPet() && static_cast<NPC*>(this)->GetOwner() && static_cast<NPC*>(this)->GetOwner()->IsPlayer() && !(static_cast<Player*>(static_cast<NPC*>(this)->GetOwner())->GetInfoStruct()->pet_behavior & 2)) {
		return;
	}

	if (IsNPC()) {
		auto npc = static_cast<NPC*>(this);

		npc->Brain()->AddHate(attacker, hate, unprovoked);

		if (!unprovoked && npc->Brain()->GetEncounterSize() == 0) {
			npc->Brain()->AddToEncounter(attacker);
		}
	}

	if (attacker->GetThreatTransfer() && hate > 0) {
		Spawn* transfer_target = GetZone()->GetSpawnByID(attacker->GetThreatTransfer()->Target);

		if (transfer_target && transfer_target->IsEntity()) {
			sint32 transfered_hate = hate * (attacker->GetThreatTransfer()->Amount / 100);
			hate -= transfered_hate;

			this->AddHate(static_cast<Entity*>(transfer_target), transfered_hate);
		}
	}

	// If pet is adding hate add some to the pets owner as well
	if (attacker->IsNPC() && static_cast<NPC*>(attacker)->IsPet()) {
		AddHate(static_cast<NPC*>(attacker)->GetOwner(), hate * 0.1);
	}

	// If player and player has a pet and protect master is set add hate to the pet
	if (IsPlayer() && HasPet() && static_cast<Player*>(this)->GetInfoStruct()->pet_behavior & 1) {
		if (static_cast<Player*>(this)->GetPet()) {
			AddHate(static_cast<Player*>(this)->GetPet(), 1);
		}

		if (static_cast<Player*>(this)->GetCharmedPet()) {
			AddHate(static_cast<Player*>(this)->GetCharmedPet(), 1);
		}
	}

	// If this spawn has a spawn group then add the attacker to the hate list of the other
	// group members if not already in their list
	if (HasSpawnGroup()) {
		vector<Spawn*>* group = GetSpawnGroup();

		for (const auto& spawn : *group) {
			if (spawn->IsNPC()) {
				NPC* npc = static_cast<NPC*>(spawn);

				if (npc->Brain()->GetHate(attacker) == 0) {
					npc->Brain()->AddHate(attacker, 1, unprovoked);
				}
			}
		}

		safe_delete(group);
	}
}

bool Entity::CheckInterruptSpell(Entity* attacker) {
	if(!IsCasting())
		return false;

	Spell* spell = GetZone()->GetSpell(this);
	if(!spell || spell->GetSpellData()->interruptable == 0)
		return false;

	if (this->IsWarded())
		return false;

	int8 interrupt_chance = 10;

	Skill* skill = GetSkillByName("Focus", true);

	if (skill)
		interrupt_chance -= interrupt_chance * (max(skill->current_val, (int16)1) / (5 * GetLevel()));

	if (MakeRandomInt(1, 100) < interrupt_chance) {
		LogWrite(COMBAT__DEBUG, 0, "Combat", "'%s' interrupted spell for '%s': %i%%", attacker->GetName(), GetName(), interrupt_chance);
		GetZone()->Interrupted(this, attacker, SPELL_ERROR_INTERRUPTED);
		return true;
	}

	LogWrite(COMBAT__DEBUG, 0, "Combat", "'%s' failed to interrupt spell for '%s': %i%%", attacker->GetName(), GetName(), interrupt_chance);
	return false;
}

void Entity::KillSpawn(Spawn* dead, int8 damage_type, int16 kill_blow_type) {
	if(!dead)
		return;

	if (IsPlayer() && dead->IsEntity())
		GetZone()->GetSpellProcess()->KillHOBySpawnID(dead->GetID());

	// If not in combat and no one in the encounter list add this killer to the list
	if(dead->EngagedInCombat() == false && dead->IsNPC() && ((NPC*)dead)->Brain()->GetEncounterSize() == 0)
		((NPC*)dead)->Brain()->AddToEncounter(this);

	if (IsCasting() && GetTarget() && GetTarget() == dead)
		GetZone()->Interrupted(this, dead, SPELL_ERROR_NOT_ALIVE);

	LogWrite(COMBAT__DEBUG, 3, "Combat", "Killing '%s'", dead->GetName());

	// Kill movement for the dead npc so the corpse doesn't move
	dead->CalculateRunningLocation(true);

	GetZone()->KillSpawn(dead, this, true, damage_type, kill_blow_type);
}

void Entity::ProcessCombat() {
	// This is a virtual function so when a NPC calls this it will use the NPC::ProcessCombat() version
	// and a player will use the Player::ProcessCombat() version, leave this function blank.
}

void NPC::ProcessCombat() {
	MBrain.writelock(__FUNCTION__, __LINE__);
	// Check to see if it is time to call the AI again
	if (GetHP() > 0 && Timer::GetCurrentTime2() >= (m_brain->LastTick() + m_brain->Tick())) {
		// Probably never want to use the following log, will spam the console for every NPC in a zone 4 times a second
		//LogWrite(NPC_AI__DEBUG, 9, "NPC_AI", "%s is thinking...", GetName());
		m_brain->Think();
		// Set the time for when the brain was last called
		m_brain->SetLastTick(Timer::GetCurrentTime2());
	}
	MBrain.releasewritelock(__FUNCTION__, __LINE__);
}

void Player::ProcessCombat() {
	CheckEncounterList();

	if (!EngagedInCombat() || IsCasting() || IsDazed() || IsFeared())
		return;

	//If no target delete combat_target and return out
	Spawn* Target = GetZone()->GetSpawnByID(target);
	if (!Target) {
		combat_target = 0;
		if (target > 0) {
			SetTarget(0);
		}
		return;
	}
	// If is not an entity return out
	if (!Target->IsEntity())
		return;

	// Reset combat target
	combat_target = 0;

	if (Target->HasTarget() && !IsHostile(Target)) {
		Spawn* secondary_target = Target->GetTarget();

		if (IsHostile(secondary_target)) {
			combat_target = secondary_target;
		}
	}
	
	// If combat_target wasn't set in the above if set it to the original target
	if (!combat_target) {
		combat_target = Target;
	}
	
	float distance = GetDistance(combat_target);

	// Check to see if we are doing ranged auto attacks if not check to see if we are in melee range
	if (GetRangeAttack() && AttackAllowed((Entity*)combat_target, distance, true) && RangeWeaponReady()) {
		Item* weapon = 0;
		Item* ammo = 0;
		// Get the currently equiped weapon and ammo for the ranged attack
		weapon = GetEquipmentList()->GetItem(EQ2_RANGE_SLOT);
		ammo = GetEquipmentList()->GetItem(EQ2_AMMO_SLOT);
		LogWrite(COMBAT__DEBUG, 1, "Combat", "Weapon '%s', Ammo '%s'", ( weapon )? weapon->name.c_str() : "None", ( ammo ) ? ammo->name.c_str() : "None");

		// If weapon and ammo are both valid perform the ranged attack else send a message to the client
		if(weapon && ammo) {
			LogWrite(COMBAT__DEBUG, 1, "Combat", "Weapon: Primary, Fighter: '%s', Target: '%s', Distance: %.2f", GetName(), combat_target->GetName(), distance);
			RangeAttack(combat_target, distance, weapon, ammo);
		}
		else {
			shared_ptr<Client> client = GetZone()->GetClientBySpawn(this);
			if (client) {
				// Need to get messages from live, made these up so the player knows what is wrong in game if weapon or ammo are not valid
				if (!ammo)
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Out of ammo.");
				if (!weapon)
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "No ranged weapon found.");
					
			}
		}
	}
	else if (GetMeleeAttack() && AttackAllowed((Entity*)combat_target, distance)) {
		// Check to see if the primary melee weapon is ready
		if(PrimaryWeaponReady()) {
			// Set the time of the last melee attack with the primary weapon and perform the melee attack with primary weapon
			SetPrimaryLastAttackTime(Timer::GetCurrentTime2());
			MeleeAttack(combat_target, distance, true);
		}

		// Check to see if the secondary weapon is ready
		if(SecondaryWeaponReady()) {
			// set the time of the last melee attack with the secondary weapon and perform the melee attack with the secondary weapon
			SetSecondaryLastAttackTime(Timer::GetCurrentTime2());
			MeleeAttack(combat_target, distance, false);
		}
	}
}

void Entity::SetAttackDelay(bool primary, bool ranged) {
	float mod = CalculateAttackSpeedMod();
	bool dual_wield = IsDualWield();

	//Note: Capping all attack speed increases at 125% normal speed (from function CalculateAttackSpeedMod())
	//Add 33% longer delay if dual wielding
	if(dual_wield && ! ranged) {
		if(primary)
			SetPrimaryAttackDelay((GetPrimaryWeaponDelay() * 1.33) / mod);
		else
			SetSecondaryAttackDelay((GetSecondaryWeaponDelay() * 1.33) / mod);
	}
	else {
		if(primary)
			SetPrimaryAttackDelay(GetPrimaryWeaponDelay() / mod);
		else if(ranged)
			SetRangeAttackDelay(GetRangeWeaponDelay() / mod);
		else
			SetSecondaryAttackDelay(GetSecondaryWeaponDelay() / mod);
	}
}

float Entity::CalculateAttackSpeedMod(){
	float aspeed = info_struct.attackspeed;
	
	if(aspeed > 0) {
		if (aspeed <= 100)
			return (aspeed / 100 + 1);
		else if (aspeed <= 200)
			return 2.25;
	}
	return 1;
}

void Entity::AddProc(int8 type, float chance, Item* item, shared_ptr<LuaSpell> spell) {
	if (type == 0) {
		LogWrite(COMBAT__ERROR, 0, "Proc", "Entity::AddProc called with an invalid type.");
		return;
	}

	if (!item && !spell) {
		LogWrite(COMBAT__ERROR, 0, "Proc", "Entity::AddProc must have a valid item or spell.");
		return;
	}

	MProcList.writelock(__FUNCTION__, __LINE__);
	Proc* proc = new Proc();
	proc->chance = chance;
	proc->item = item;
	proc->spell = spell;
	m_procList[type].push_back(proc);
	MProcList.releasewritelock(__FUNCTION__, __LINE__);
}

void Entity::RemoveProc(Item* item, shared_ptr<LuaSpell> spell) {
	if (!item && !spell) {
		LogWrite(COMBAT__ERROR, 0, "Proc", "Entity::RemoveProc must have a valid item or spell.");
		return;
	}

	MProcList.writelock(__FUNCTION__, __LINE__);
	map<int8, vector<Proc*> >::iterator proc_itr;
	vector<Proc*>::iterator itr;
	for (proc_itr = m_procList.begin(); proc_itr != m_procList.end(); proc_itr++) {
		itr = proc_itr->second.begin();
		while (itr != proc_itr->second.end()) {
			Proc* proc = *itr;

			if ((item && proc->item == item) || (spell && proc->spell == spell)) {
				safe_delete(*itr);
				itr = proc_itr->second.erase(itr);
			}
			else
				itr++;
		}
	}
	MProcList.releasewritelock(__FUNCTION__, __LINE__);
}

bool Entity::CastProc(Proc* proc, int8 type, Spawn* target) {
	lua_State* state = 0;
	bool item_proc = false;
	int8 num_args = 3;

	if (proc->spell) {
		state = proc->spell->state;
		lua_interface->SetCurrentSpell(state, proc->spell);
	} else if (proc->item) {
		state = lua_interface->GetItemScript(proc->item->GetItemScript());
		item_proc = true;
	}
	
	if (!state) {
		LogWrite(COMBAT__ERROR, 0, "Proc", "No valid lua_State* found");
		return false;
	}

	lua_getglobal(state, "proc");

	if (item_proc) {
		num_args++;
		lua_interface->SetItemValue(state, proc->item);
	}

	lua_interface->SetSpawnValue(state, this);
	lua_interface->SetSpawnValue(state, target);
	lua_interface->SetInt32Value(state, type);

	/*
	Add spell data from db in case of a spell proc here...
	*/
	if (!item_proc) {
		// Append spell data to the param list
		vector<LUAData> data = proc->spell->spell->GetScaledLUAData(GetLevel());
		for (int32 i = 0; i < data.size(); i++) {
			switch (data.at(i).type) {
				case 0:
					lua_interface->SetSInt32Value(proc->spell->state, data.at(i).int_value);
					break;

				case 1:
					lua_interface->SetFloatValue(proc->spell->state, data.at(i).float_value);
					break;

				case 2:
					lua_interface->SetBooleanValue(proc->spell->state, data.at(i).bool_value);
					break;

				case 3:
					lua_interface->SetStringValue(proc->spell->state, data.at(i).string_value.c_str());
					break;

				default:
					LogWrite(SPELL__ERROR, 0, "Spell", "Error: Unknown LUA Type '%i' in Entity::CastProc for Spell '%s'", (int)data.at(i).type, proc->spell->spell->GetName());
					return false;
			}
			num_args++;
		}
	}

	if (lua_pcall(state, num_args, 0, 0) != 0) {
		LogWrite(COMBAT__ERROR, 0, "Proc", "Unable to call the proc function");
		lua_pop(state, 1);
		if (proc->spell) {
			lua_interface->SetCurrentSpell(proc->spell->state, nullptr);
		}
		return false;
	}

	if (proc->spell) {
		lua_interface->SetCurrentSpell(proc->spell->state, nullptr);
	}

	return true;
}

void Entity::CheckProcs(int8 type, Spawn* target) {
	if (type == 0) {
		LogWrite(COMBAT__ERROR, 0, "Proc", "Entity::CheckProcs called with an invalid type.");
		return;
	}

	vector<Proc*> procs;

	MProcList.readlock(__FUNCTION__, __LINE__);
	for (int8 i = 0; i < m_procList[type].size(); i++) {
		Proc* proc = m_procList[type].at(i);
		float roll = MakeRandomFloat(0, 100);

		if (roll <= proc->chance)
			procs.push_back(proc);
	}
	MProcList.releasereadlock(__FUNCTION__, __LINE__);

	for (const auto proc : procs)
		CastProc(proc, type, target);
}

void Entity::ClearProcs() {
	MProcList.writelock(__FUNCTION__, __LINE__);

	map<int8, vector<Proc*> >::iterator proc_itr;
	vector<Proc*>::iterator itr;
	for (proc_itr = m_procList.begin(); proc_itr != m_procList.end(); proc_itr++) {
		itr = proc_itr->second.begin();
		while (itr != proc_itr->second.end()) {
				safe_delete(*itr);
				itr = proc_itr->second.erase(itr);
		}
		proc_itr->second.clear();
	}
	m_procList.clear();

	MProcList.releasewritelock(__FUNCTION__, __LINE__);
}
