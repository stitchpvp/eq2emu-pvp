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

#include "NPC_AI.h"
#include "Combat.h"
#include "zoneserver.h"
#include "Spells.h"
#include "../common/Log.h"
#include "LuaInterface.h"
#include "World.h"

extern LuaInterface* lua_interface;
extern World world;

/*  The NEW AI code  */

Brain::Brain(NPC* npc) {
	// Set the npc this brain will controll
	m_body = npc;
	// Set the default time between calls to think to 250 miliseconds (1/4 a second)
	m_tick = 250;
	m_lastTick = Timer::GetCurrentTime2();
	m_spellRecovery = 0;
	m_playerInEncounter = false;
	// Set up the mutex for the hate list
	MHateList.SetName("Brain::m_hatelist");
	// Set up the mutex for the encounter list
	MEncounter.SetName("Brain::m_encounter");
}

Brain::~Brain() {

}

void Brain::Think() {

	// Get the entity this NPC hates the most,
	// GetMostHated() will handle dead spawns so no need to check the health in this function
	Entity* target = GetMostHated();

	// If mezzed, stunned or feared we can't do anything so skip
	if (!m_body->IsMezzedOrStunned() || !m_body->IsFeared()) {
		// Not mezzed or stunned

		// Get the distance to the runback location
		float run_back_distance = m_body->GetRunbackDistance();

		if (target) {
			LogWrite(NPC_AI__DEBUG, 7, "NPC_AI", "%s has %s targeted.", m_body->GetName(), target->GetName());
			// NPC has an entity that it hates

			// If the NPC is not in combat then put them in combat
			if (!m_body->EngagedInCombat()) {
				m_body->ClearRunningLocations();
				m_body->CalculateRunningLocation(true);
				m_body->InCombat(true);
			}

			// Set the NPC's target to the most hated entity if it is not already.
			if (m_body->GetTarget() != target) {
				m_body->SetTarget(target);
				m_body->FaceTarget(target);
			}

			// Check to see if the NPC has exceeded the max chase distance
			if (run_back_distance > MAX_CHASE_DISTANCE) {
				LogWrite(NPC_AI__DEBUG, 7, "NPC_AI", "Run back distance is greater then max chase distance, run_back_distance = %f", run_back_distance);
				// Over the max chase distance, Check to see if the target is is a client
				Client* client = target->GetZone()->GetClientBySpawn(target);
				if (client)
				{
					// Target is a client so send encounter break messages
					if (m_body->HasSpawnGroup())
						client->SimpleMessage(CHANNEL_COLOR_WHITE, "This encounter will no longer give encounter rewards.");
					else
						client->Message(CHANNEL_COLOR_WHITE, "%s is no longer worth any experience or treasure.", m_body->GetName());
				}
				// Clear the hate list for this NPC
				ClearHate();
				// Clear the encounter list
				ClearEncounter();
			}
			else {
				// Still within max chase distance lets to the combat stuff now
				float distance = m_body->GetDistance(target);

				if(!m_body->IsCasting() && (!HasRecovered() || !ProcessSpell(target, distance))) {
					LogWrite(NPC_AI__DEBUG, 7, "NPC_AI", "%s is attempting melee on %s.", m_body->GetName(), target->GetName());
					m_body->FaceTarget(target);
					ProcessMelee(target, distance);
				}
			}
		}
		else {
			// Nothing in the hate list

			// Check to see if the NPC is still flagged as in combat for some reason
			if (m_body->EngagedInCombat()) {
				// If it is set the combat flag to false
				m_body->InCombat(false);

				// Do not set a players pet to full health once they stop combat
				if (!m_body->IsPet() || (m_body->IsPet() && !m_body->GetOwner()->IsPlayer()))
					m_body->SetHP(m_body->GetTotalHP());
			}

			CheckBuffs();

			// If run back distance is greater then 0 then run back
			if (run_back_distance > 0) {
				m_body->Runback();
			}
			// If encounter size is greater then 0 then clear it
			if (GetEncounterSize() >= 0)
				ClearEncounter();
		}
	}
}

sint32 Brain::GetHate(Entity* entity) {
	// We will use this variable to return the value, default to 0
	sint32 ret = 0;

	// Lock the hate list, not altering it so do a read lock
	MHateList.readlock(__FUNCTION__, __LINE__);

	// First check to see if the given entity is even in the hate list
	if (m_hatelist.count(entity->GetID()) > 0)
		// Entity in the hate list so get the hate value for the entity
		ret = m_hatelist[entity->GetID()];

	// Unlock the hate list
	MHateList.releasereadlock(__FUNCTION__, __LINE__);

	// return the hate
	return ret;
}

void Brain::AddHate(Entity* entity, sint32 hate) {
	// Lock the hate list, we are altering the list so use write lock
	MHateList.writelock(__FUNCTION__, __LINE__);

	if (m_hatelist.count(entity->GetID()) > 0)
		m_hatelist[entity->GetID()] += hate;
	else
		m_hatelist.insert(std::pair<int32, sint32>(entity->GetID(), hate));

	// Unlock the list
	MHateList.releasewritelock(__FUNCTION__, __LINE__);
}

void Brain::ClearHate() {
	// Lock the hate list, we are altering the list so use a write lock
	MHateList.writelock(__FUNCTION__, __LINE__);
	// Clear the list
	m_hatelist.clear();
	// Unlock the hate list
	MHateList.releasewritelock(__FUNCTION__, __LINE__);
}

void Brain::ClearHate(Entity* entity) {
	// Lock the hate list, we could potentially modify the list so use write lock
	MHateList.writelock(__FUNCTION__, __LINE__);

	// Check to see if the given entity is in the hate list
	if (m_hatelist.count(entity->GetID()) > 0)
		// Erase the entity from the hate list
		m_hatelist.erase(entity->GetID());

	// Unlock the hate list
	MHateList.releasewritelock(__FUNCTION__, __LINE__);
}

Entity* Brain::GetMostHated() {
	map<int32, sint32>::iterator itr;
	int32 ret = 0;
	sint32 hate = 0;

	// Lock the hate list, not going to alter it so use a read lock
	MHateList.readlock(__FUNCTION__, __LINE__);

	if (m_hatelist.size() > 0) {
		// Loop through the list looking for the entity that this NPC hates the most
		for(itr = m_hatelist.begin(); itr != m_hatelist.end(); itr++) {
			// Compare the hate value for the current iteration to our stored highest value
			if(itr->second > hate) {
				// New high value store the entity
				ret = itr->first;
				// Store the value to compare with the rest of the entities
				hate = itr->second;
			}
		}
	}
	// Unlock the list
	MHateList.releasereadlock(__FUNCTION__, __LINE__);
	Entity* hated = (Entity*)GetBody()->GetZone()->GetSpawnByID(ret);
	// Check the reult to see if it is still alive
	if(hated && hated->GetHP() <= 0) {
		// Entity we got was dead so remove it from the list
		ClearHate(hated);
		// Call this function again now that we removed the dead entity
		hated = GetMostHated();
	}

	// Return our result
	return hated;
}

sint8 Brain::GetHatePercentage(Entity* entity) {
	float percentage = 0.0;
	MHateList.readlock(__FUNCTION__, __LINE__);
	if (entity && m_hatelist.count(entity->GetID()) > 0 && m_hatelist[entity->GetID()] > 0) {
		sint32 total_hate = 0;
		map<int32, sint32>::iterator itr;
		for (itr = m_hatelist.begin(); itr != m_hatelist.end(); itr++)
			total_hate += itr->second;
		percentage = m_hatelist[entity->GetID()] / total_hate;
	}
	MHateList.releasereadlock(__FUNCTION__, __LINE__);

	return (sint8)(percentage * 100);
}

vector<Entity*>* Brain::GetHateList() {
	vector<Entity*>* ret = new vector<Entity*>;
	map<int32, sint32>::iterator itr;

	// Lock the list
	MHateList.readlock(__FUNCTION__, __LINE__);
	// Loop over the list storing the values into the new list
	for (itr = m_hatelist.begin(); itr != m_hatelist.end(); itr++) {
		Entity* ent = (Entity*)GetBody()->GetZone()->GetSpawnByID(itr->first);
		if (ent)
			ret->push_back(ent);
	}
	// Unlock the list
	MHateList.releasereadlock(__FUNCTION__, __LINE__);

	// Return the copy of the list
	return ret;
}

void Brain::MoveCloser(Entity* target) {
	if (target && m_body->GetFollowTarget() != target)
		m_body->SetFollowTarget(target);

	if (m_body->GetFollowTarget() && !m_body->following) {
		m_body->CalculateRunningLocation(true);
		m_body->ClearRunningLocations();
		m_body->following = true;
	}
}

bool Brain::ProcessSpell(Entity* target, float distance) {
	if(rand()%100 > m_body->GetCastPercentage() || m_body->IsStifled() || m_body->IsFeared())
		return false;
	Spell* spell = m_body->GetNextSpell(distance);
	if(spell){
		Spawn* spell_target = 0;
		if(spell->GetSpellData()->friendly_spell == 1){
			vector<Spawn*>* group = m_body->GetSpawnGroup();
			if(group && group->size() > 0){
				vector<Spawn*>::iterator itr;
				for(itr = group->begin(); itr != group->end(); itr++){
					if((!spell_target && (*itr)->GetHP() > 0 && (*itr)->GetHP() < (*itr)->GetTotalHP()) || (spell_target && (*itr)->GetHP() > 0 && spell_target->GetHP() > (*itr)->GetHP()))
						spell_target = *itr;
				}
			}
			if(!spell_target)
				spell_target = m_body;

			safe_delete(group);
		}
		else
			spell_target = target;
		m_body->GetZone()->ProcessSpell(spell, m_body, spell_target);
		m_spellRecovery = (int32)(Timer::GetCurrentTime2() + (spell->GetSpellData()->cast_time * 10) + (spell->GetSpellData()->recovery * 10) + 2000);
		return true;
	}
	return false;
}

bool Brain::CheckBuffs() {
	if (!m_body->GetZone()->GetSpellProcess() || m_body->EngagedInCombat() || m_body->IsCasting() || m_body->IsMezzedOrStunned() || !m_body->Alive() || m_body->IsStifled() || !HasRecovered())
		return false;

	Spell* spell = m_body->GetNextBuffSpell();
	if (spell) {
		m_body->CalculateRunningLocation(true);
		m_body->GetZone()->ProcessSpell(spell, m_body, m_body);
		m_spellRecovery = (int32)(Timer::GetCurrentTime2() + (spell->GetSpellData()->cast_time * 10) + (spell->GetSpellData()->recovery * 10) + 2000);
		return true;
	}
	return false;
}

void Brain::ProcessMelee(Entity* target, float distance) {
	if(distance > MAX_COMBAT_RANGE)
		MoveCloser(target);
	else {
		LogWrite(NPC_AI__DEBUG, 7, "NPC_AI", "%s is within melee range of %s.", m_body->GetName(), target->GetName());
		if(target && m_body->AttackAllowed(target)) {
			LogWrite(NPC_AI__DEBUG, 7, "NPC_AI", "%s is allowed to attack %s.", m_body->GetName(), target->GetName());
			if(m_body->PrimaryWeaponReady() && !m_body->IsDazed() && !m_body->IsFeared()) {
				LogWrite(NPC_AI__DEBUG, 7, "NPC_AI", "%s swings its primary weapon at %s.", m_body->GetName(), target->GetName());
				m_body->SetPrimaryLastAttackTime(Timer::GetCurrentTime2());
				m_body->MeleeAttack(target, distance, true);
				m_body->GetZone()->CallSpawnScript(m_body, SPAWN_SCRIPT_AUTO_ATTACK_TICK, target);
			}
			if(m_body->SecondaryWeaponReady() && !m_body->IsDazed()) {
				m_body->SetSecondaryLastAttackTime(Timer::GetCurrentTime2());
				m_body->MeleeAttack(target, distance, false);
			}
		}
	}
}

bool Brain::HasRecovered() {
	if(m_spellRecovery > Timer::GetCurrentTime2())
		return false;

	m_spellRecovery = 0;
	return true;
}

void Brain::AddToEncounter(Entity* entity) {

	// If player pet then set the entity to the pets owner
	if (entity->IsPet() && ((NPC*)entity)->GetOwner()->IsPlayer())
		entity = ((NPC*)entity)->GetOwner();

	// If player then get the players group
	int32 group_id = 0;
	if (entity->IsPlayer()) {
		m_playerInEncounter = true;
		if (entity->GetGroupMemberInfo())
			group_id = entity->GetGroupMemberInfo()->group_id;
	}

	// Insert the entity into the encounter list, if there is a group add all group members as well
	// TODO: add raid members
	MEncounter.writelock(__FUNCTION__, __LINE__);
	if (group_id > 0) {
		world.GetGroupManager()->GroupLock(__FUNCTION__, __LINE__);

		deque<GroupMemberInfo*>::iterator itr;
		deque<GroupMemberInfo*>* members = world.GetGroupManager()->GetGroupMembers(group_id);
		for (itr = members->begin(); itr != members->end(); itr++)
			m_encounter.push_back((*itr)->client->GetPlayer()->GetID());

		world.GetGroupManager()->ReleaseGroupLock(__FUNCTION__, __LINE__);
	}
	else {
		m_encounter.push_back(entity->GetID());
	}
	MEncounter.releasewritelock(__FUNCTION__, __LINE__);
}

bool Brain::CheckLootAllowed(Entity* entity) {
	bool ret = false;
	vector<int32>::iterator itr;

	// Check the encounter list to see if the given entity is in it, if so return true.
	MEncounter.readlock(__FUNCTION__, __LINE__);
	for (itr = m_encounter.begin(); itr != m_encounter.end(); itr++) {
		if ((*itr) == entity->GetID()) {
			// found the entity in the encounter list, set return value to true and break the loop
			ret = true;
			break;
		}
	}
	MEncounter.releasereadlock(__FUNCTION__, __LINE__);	

	return ret;
}

int8 Brain::GetEncounterSize() {
	int8 ret = 0;

	MEncounter.readlock(__FUNCTION__, __LINE__);
	ret = (int8)m_encounter.size();
	MEncounter.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}

vector<int32>* Brain::GetEncounter() {
	vector<int32>* ret = new vector<int32>;
	vector<int32>::iterator itr;

	// Lock the list
	MEncounter.readlock(__FUNCTION__, __LINE__);
	// Loop over the list storing the values into the new list
	for (itr = m_encounter.begin(); itr != m_encounter.end(); itr++)
		ret->push_back(*itr);
	// Unlock the list
	MEncounter.releasereadlock(__FUNCTION__, __LINE__);

	// Return the copy of the list
	return ret;
}

void Brain::ClearEncounter() {
	MEncounter.writelock(__FUNCTION__, __LINE__);
	m_encounter.clear();
	m_playerInEncounter = false;
	MEncounter.releasewritelock(__FUNCTION__, __LINE__);
}


/* Example of how to extend the default AI */


CombatPetBrain::CombatPetBrain(NPC* body) : Brain(body) {
	// Make sure to have the " : Brain(body)" so it calls the parent class constructor
	// to set up the AI properly
}

CombatPetBrain::~CombatPetBrain() {

}

void CombatPetBrain::Think() {
	// We are extending the base brain so make sure to call the parent Think() function.
	// If we want to override then we could remove Brain::Think()
	Brain::Think();

	// All this Brain does is make the pet follow its owner, the combat comes from the default brain

	if (GetBody()->EngagedInCombat() || !GetBody()->IsPet() || GetBody()->IsMezzedOrStunned())
		return;
	
	LogWrite(NPC_AI__DEBUG, 7, "NPC_AI", "Pet AI code called for %s", GetBody()->GetName());

	// If owner is a player and player has stay set then return out
	if (GetBody()->GetOwner()->IsPlayer() && ((Player*)GetBody()->GetOwner())->GetInfoStruct()->pet_movement == 1)
		return;

	// Set target to owner
	Entity* target = GetBody()->GetOwner();
	GetBody()->SetTarget(target);

	// Get distance from the owner
	float distance = GetBody()->GetDistance(target);
	distance -= target->appearance.pos.collision_radius / 10;
	distance -= GetBody()->appearance.pos.collision_radius / 10;

	// If out of melee range then move closer
	if (distance > MAX_COMBAT_RANGE)
		MoveCloser(target);
}

/* Example of how to override the default AI */


NonCombatPetBrain::NonCombatPetBrain(NPC* body) : Brain(body) {
	// Make sure to have the " : Brain(body)" so it calls the parent class constructor
	// to set up the AI properly
}

NonCombatPetBrain::~NonCombatPetBrain() {

}

void NonCombatPetBrain::Think() {
	// All this Brain does is make the pet follow its owner

	if (!GetBody()->IsPet() || GetBody()->IsMezzedOrStunned())
		return;
	
	LogWrite(NPC_AI__DEBUG, 7, "NPC_AI", "Pet AI code called for %s", GetBody()->GetName());

	// Set target to owner
	Entity* target = GetBody()->GetOwner();
	GetBody()->SetTarget(target);

	// Get distance from the owner
	float distance = GetBody()->GetDistance(target);
	distance -= target->appearance.pos.collision_radius / 10;
	distance -= GetBody()->appearance.pos.collision_radius / 10;

	// If out of melee range then move closer
	if (distance > MAX_COMBAT_RANGE)
		MoveCloser(target);
}

BlankBrain::BlankBrain(NPC* body) : Brain(body) {
	// Make sure to have the " : Brain(body)" so it calls the parent class constructor
	// to set up the AI properly
	SetTick(50000);
}

BlankBrain::~BlankBrain() {

}

void BlankBrain::Think() {
	
}

LuaBrain::LuaBrain(NPC* body) : Brain(body) {

}

LuaBrain::~LuaBrain() {
}

void LuaBrain::Think() {
	if (!lua_interface)
		return;

	const char* script = GetBody()->GetSpawnScript();
	if(script) {
		if (!lua_interface->RunSpawnScript(script, "Think", GetBody(), GetBody()->GetTarget())) {
			lua_interface->LogError("LUA LuaBrain error: was unable to call the Think function in the spawn script (%s)", script);
		}
	}
	else {
		LogWrite(NPC_AI__ERROR, 0, "NPC_AI", "Lua brain set on a spawn that doesn't have a script...");
	}
}

DumbFirePetBrain::DumbFirePetBrain(NPC* body, Entity* target, int32 expire_time) : Brain(body) {
	m_expireTime = Timer::GetCurrentTime2() + expire_time;
	AddHate(target, INT_MAX);
}

DumbFirePetBrain::~DumbFirePetBrain() {

}

void DumbFirePetBrain::AddHate(Entity* entity, sint32 hate) {
	if (!GetMostHated())
		Brain::AddHate(entity, hate);
}

void DumbFirePetBrain::Think() {

	Entity* target = GetMostHated();

	if (target) {
		if (!GetBody()->IsMezzedOrStunned()) {
			// If the NPC is not in combat then put them in combat
			if (!GetBody()->EngagedInCombat()) {
				GetBody()->ClearRunningLocations();
				GetBody()->CalculateRunningLocation(true);
				GetBody()->InCombat(true);
			}

			// Set the NPC's target to the most hated entity if it is not already.
			if (GetBody()->GetTarget() != target) {
				GetBody()->SetTarget(target);
				GetBody()->FaceTarget(target);
			}

			float distance = GetBody()->GetDistance(target);
			distance -= target->appearance.pos.collision_radius / 10;
			distance -= GetBody()->appearance.pos.collision_radius / 10;

			if(!GetBody()->IsCasting() && (!HasRecovered() || !ProcessSpell(target, distance))) {
				LogWrite(NPC_AI__DEBUG, 7, "NPC_AI", "%s is attempting melee on %s.", GetBody()->GetName(), target->GetName());
				GetBody()->FaceTarget(target);
				ProcessMelee(target, distance);
			}
		}
	}
	else {
		// No hated target or time expired, kill this mob
		if (GetBody()->GetHP() > 0) {
			GetBody()->KillSpawn(GetBody());
			LogWrite(NPC_AI__DEBUG, 7, "NPC AI", "Dumbfire being killed because there is no target.");
		}
	}

	if (Timer::GetCurrentTime2() > m_expireTime) {
		if (GetBody()->GetHP() > 0) {
			GetBody()->KillSpawn(GetBody());
			LogWrite(NPC_AI__DEBUG, 7, "NPC AI", "Dumbfire being killed because timer expired.");
		}
	 }
}