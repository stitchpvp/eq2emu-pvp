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

#include "PlayerGroups.h"
#include "../common/Log.h"
#include "World.h"
#include "Spells.h"
#include "LuaInterface.h"
#include "Bots/Bot.h"

extern ZoneList	zone_list;

/******************************************************** PlayerGroup ********************************************************/

PlayerGroup::PlayerGroup(int32 id) {
	m_id = id;
}

PlayerGroup::~PlayerGroup() {
	Disband();
}

bool PlayerGroup::AddMember(Entity* member) {
	// Check to make sure the entity we are adding is valid
	if (!member) {
		LogWrite(GROUP__ERROR, 0, "Group", "New member is null");
		return false;
	}

	// Make sure entity we are adding isn't already in a group by checking if it has a GroupMemberInfo pointer
	if (member->GetGroupMemberInfo()) {
		LogWrite(GROUP__ERROR, 0, "Group", "New member (%s) already has a group", member->GetName());
		return false;
	}

	// Create a new GroupMemberInfo and assign it to the new member
	GroupMemberInfo* gmi = new GroupMemberInfo;
	gmi->group_id = m_id;
	gmi->member = member;
	gmi->leader = false;
	if (member->IsPlayer()) {
		gmi->client = member->GetZone()->GetClientBySpawn(member);
	} else {
		gmi->client = 0;
	}

	member->SetGroupMemberInfo(gmi);
	member->UpdateGroupMemberInfo();
	member->AddSpawnUpdate(true, false, false);
	m_members.push_back(gmi);

	SendGroupUpdate();
	return true;
}

bool PlayerGroup::RemoveMember(Entity* member) {
	GroupMemberInfo* gmi = member->GetGroupMemberInfo();
	if (!gmi) {
		return false;
	}

	bool ret = false;

	member->SetGroupMemberInfo(0);
	deque<GroupMemberInfo*>::iterator erase_itr = m_members.end();
	deque<GroupMemberInfo*>::iterator itr;
	for (itr = m_members.begin(); itr != m_members.end(); itr++) {
		if (gmi == *itr)
			erase_itr = itr;
		if ((*itr)->client) {
			(*itr)->client->GetPlayer()->SetCharSheetChanged(true);
			(*itr)->member->AddSpawnUpdate(true, false, false);
		}
	}
	if (erase_itr != m_members.end()) {
		ret = true;
		m_members.erase(erase_itr);
	}

	safe_delete(gmi);
	if (member->IsBot())
		((Bot*)member)->Camp();

	return ret;
}

void PlayerGroup::Disband() {
	deque<GroupMemberInfo*>::iterator itr;
	for (itr = m_members.begin(); itr != m_members.end(); itr++) {
		if ((*itr)->member) {
			(*itr)->member->SetGroupMemberInfo(0);
			if ((*itr)->member->IsBot())
				((Bot*)(*itr)->member)->Camp();
		}
		if ((*itr)->client)
			(*itr)->client->GetPlayer()->SetCharSheetChanged(true);

		safe_delete(*itr);
	}

	m_members.clear();
}

void PlayerGroup::SendGroupUpdate(shared_ptr<Client> exclude) {
	deque<GroupMemberInfo*>::iterator itr;
	for (itr = m_members.begin(); itr != m_members.end(); itr++) {
		GroupMemberInfo* gmi = *itr;
		if (gmi->client && gmi->client != exclude && !gmi->client->IsZoning())
			gmi->client->GetPlayer()->SetCharSheetChanged(true);
	}
}

void PlayerGroup::SimpleGroupMessage(const char* message) {
	deque<GroupMemberInfo*>::iterator itr;
	for(itr = m_members.begin(); itr != m_members.end(); itr++) {
		GroupMemberInfo* info = *itr;
		if(info->client)
			info->client->SimpleMessage(CHANNEL_GROUP, message);
	}
}

void PlayerGroup::GroupChatMessage(Spawn* from, const char* message) {
	deque<GroupMemberInfo*>::iterator itr;
	for(itr = m_members.begin(); itr != m_members.end(); itr++) {
		GroupMemberInfo* info = *itr;
		if(info->client && info->client->GetCurrentZone())
			info->client->GetCurrentZone()->HandleChatMessage(info->client, from, 0, CHANNEL_GROUP_SAY, message, 0);
	}
}

void PlayerGroup::MakeLeader(Entity* new_leader) {
	deque<GroupMemberInfo*>::iterator itr;
	for (itr = m_members.begin(); itr != m_members.end(); itr++) {
		GroupMemberInfo* info = *itr;
		if (info->leader) {
			info->leader = false;
			break;
		}
	}

	new_leader->GetGroupMemberInfo()->leader = true;
	SendGroupUpdate();
}



/******************************************************** PlayerGroupManager ********************************************************/

PlayerGroupManager::PlayerGroupManager() {
	m_nextGroupID = 1;

	MGroups.SetName("PlayerGroupManager::m_groups");
	MPendingInvites.SetName("PlayerGroupManager::m_pendingInvites");
}

PlayerGroupManager::~PlayerGroupManager() {
	MPendingInvites.writelock(__FUNCTION__, __LINE__);
	m_pendingInvites.clear();
	MPendingInvites.releasewritelock(__FUNCTION__, __LINE__);

	MGroups.writelock(__FUNCTION__, __LINE__);
	map<int32, PlayerGroup*>::iterator itr;
	for (itr = m_groups.begin(); itr != m_groups.end(); itr++)
		safe_delete(itr->second);

	m_groups.clear();
	MGroups.releasewritelock(__FUNCTION__, __LINE__);
}

bool PlayerGroupManager::AddGroupMember(int32 group_id, Entity* member) {
	bool ret = false;
	MGroups.writelock(__FUNCTION__, __LINE__);

	if (m_groups.count(group_id) > 0) {
		PlayerGroup* group = m_groups[group_id];
		ret = group->AddMember(member);
	}

	MGroups.releasewritelock(__FUNCTION__, __LINE__);

	return ret;
}

bool PlayerGroupManager::RemoveGroupMember(int32 group_id, Entity* member) {
	bool ret = false;
	bool remove = false;
	shared_ptr<Client> client = 0;
	MGroups.writelock(__FUNCTION__, __LINE__);

	if (m_groups.count(group_id) > 0) {
		PlayerGroup* group = m_groups[group_id];

		LogWrite(MISC__TODO, 0, "TODO", "Update PlayerGroupManager::RemoveGroupMember() to support removing buffs from NPC group members");
		if (member->IsPlayer())
			client = member->GetGroupMemberInfo()->client;

		ret = group->RemoveMember(member);

		// If only 1 person left in the group set a flag to remove the group
		if (group->Size() == 1)
			remove = true;
	}

	MGroups.releasewritelock(__FUNCTION__, __LINE__);

	if (client)
		RemoveGroupBuffs(group_id, client);

	// Call RemoveGroup outside the locks as it uses the same locks
	if (remove)
		RemoveGroup(group_id);

	return ret;
}

void PlayerGroupManager::NewGroup(Entity* leader) {
	MGroups.writelock(__FUNCTION__, __LINE__);

	// Highly doubt this will ever be needed but putting it in any way, basically bump the id and ensure
	// no active group is currently using this id, if we hit the max for an int32 then reset the id to 1
	while (m_groups.count(m_nextGroupID) > 0) {
		// If m_nextGroupID is at its max then reset it to 1, else increment it
		if (m_nextGroupID == 4294967295)
			m_nextGroupID = 1;
		else
			m_nextGroupID++;
	}

	// Create a new group with the valid ID we got from above
	PlayerGroup* new_group = new PlayerGroup(m_nextGroupID);
	// Add the leader to the group
	new_group->AddMember(leader);

	leader->GetGroupMemberInfo()->leader = true;

	// Add the new group to the list
	m_groups[m_nextGroupID] = new_group;

	MGroups.releasewritelock(__FUNCTION__, __LINE__);
}

void PlayerGroupManager::RemoveGroup(int32 group_id) {
	MGroups.writelock(__FUNCTION__, __LINE__);

	// Check to see if the id is in the list
	if (m_groups.count(group_id) > 0) {
		// Get a pointer to the group
		PlayerGroup* group = m_groups[group_id];
		// Erase the group from the list
		m_groups.erase(group_id);
		// Delete the group
		safe_delete(group);
	}

	MGroups.releasewritelock(__FUNCTION__, __LINE__);
}

int8 PlayerGroupManager::Invite(Player* leader, Entity* member) {
	int8 ret = 255; // Should be changed, if it is not then we have an unknown error

	// Lock the pending invite list so we can work with it
	MPendingInvites.writelock(__FUNCTION__, __LINE__);

	// Disable npc's in group until we are ready for mercs
	if (!member || (member->IsNPC() && !member->IsBot()) || member->IsHostile(leader))
		ret = 6; // failure, not a valid target
	else if (member->IsNPC() && (!member->IsBot() /*|| !member->IsMec()*/))
		ret = 6;
	else if (leader == member)
		ret = 5; // failure, can't invite yourself
	else if (member->GetGroupMemberInfo())
		ret = 1; // failure, member already in a group
	// Check to see if the target of the invite already has a pending invite
	else if (m_pendingInvites.count(member->GetName()) > 0)
		ret = 2; // Target already has an invite
	// Check to see if the player that invited is already in a group
	else if (leader->GetGroupMemberInfo()) {
		// Read lock the group list so we can get the size of the inviters group
		MGroups.readlock(__FUNCTION__, __LINE__);
		int32 group_size = m_groups[leader->GetGroupMemberInfo()->group_id]->Size();
		MGroups.releasereadlock(__FUNCTION__, __LINE__);

		// Check to see if the group is full
		if (m_groups[leader->GetGroupMemberInfo()->group_id]->Size() >= 6)
			ret = 3; // Group full
		// Group isn't full so add the member to the pending invite list
		else {
			m_pendingInvites[member->GetName()] = leader->GetName();
			ret = 0; // Success
		}
	}
	// Inviter is not in a group
	else { 
		// Check to see if the inviter has a pending invite himself
		if (m_pendingInvites.count(leader->GetName()) > 0)
			ret = 4;	// inviter already has a pending group invite
		// No pending invites for the inviter add both the inviter and the target of the invite to the list
		else {
			m_pendingInvites[leader->GetName()] = leader->GetName();
			m_pendingInvites[member->GetName()] = leader->GetName();
			ret = 0; // success
		}
	}
	// Release the lock on pending invites
	MPendingInvites.releasewritelock(__FUNCTION__, __LINE__);

	/* testing purposes only */
	if (ret == 0 && member->IsNPC())
		AcceptInvite(member);

	return ret; 
}

int8 PlayerGroupManager::AcceptInvite(Entity* member) {
	int8 ret = 3; // default to unknown error

	MPendingInvites.writelock(__FUNCTION__, __LINE__);

	if (m_pendingInvites.count(member->GetName()) > 0) {
		string leader = m_pendingInvites[member->GetName()];
		shared_ptr<Client> client_leader = zone_list.GetClientByCharName(leader);

		if (client_leader) {
			if (m_pendingInvites.count(leader) > 0) {
				NewGroup(client_leader->GetPlayer());
				m_pendingInvites.erase(leader);
			}

			// Remove from invite list and add to the group
			m_pendingInvites.erase(member->GetName());
			AddGroupMember(client_leader->GetPlayer()->GetGroupMemberInfo()->group_id, member);
			ret = 0; // success
		}
		else {
			// Was unable to find the leader, remove from the invite list
			m_pendingInvites.erase(member->GetName());
			ret = 2; // failure, can't find leader
		}
	}
	else
		ret = 1; // failure, no pending invite

	MPendingInvites.releasewritelock(__FUNCTION__, __LINE__);

	return ret;
}

void PlayerGroupManager::DeclineInvite(Entity* member) {
	MPendingInvites.writelock(__FUNCTION__, __LINE__);

	if (m_pendingInvites.count(member->GetName()) > 0) {
		string leader = m_pendingInvites[member->GetName()];
		m_pendingInvites.erase(member->GetName());
		if (m_pendingInvites.count(leader) > 0)
			m_pendingInvites.erase(leader);
	}

	MPendingInvites.releasewritelock(__FUNCTION__, __LINE__);
}

bool PlayerGroupManager::IsGroupIDValid(int32 group_id) {
	bool ret = false;
	MGroups.readlock(__FUNCTION__, __LINE__);
	ret = m_groups.count(group_id) > 0;
	MGroups.releasereadlock(__FUNCTION__, __LINE__);
	return ret;
}

void PlayerGroupManager::SendGroupUpdate(int32 group_id, shared_ptr<Client> exclude) {
	MGroups.writelock(__FUNCTION__, __LINE__);

	if (m_groups.count(group_id) > 0) {
		m_groups[group_id]->SendGroupUpdate(exclude);
	}

	MGroups.releasewritelock(__FUNCTION__, __LINE__);
}

deque<GroupMemberInfo*>* PlayerGroupManager::GetGroupMembers(int32 group_id) {
	if (m_groups.count(group_id) > 0)
		return m_groups[group_id]->GetMembers();

	return 0;
}

void PlayerGroupManager::ClearPendingInvite(Entity* member) {
	MPendingInvites.writelock(__FUNCTION__, __LINE__);

	if (m_pendingInvites.count(member->GetName()) > 0)
		m_pendingInvites.erase(member->GetName());

	MPendingInvites.releasewritelock(__FUNCTION__, __LINE__);
}

void PlayerGroupManager::RemoveGroupBuffs(int32 group_id, shared_ptr<Client> client) {
	PlayerGroup* group = nullptr;
	map<Player*, vector<shared_ptr<LuaSpell>>> to_remove;

	MGroups.readlock(__FUNCTION__, __LINE__);
	if (m_groups.count(group_id) > 0)
		group = m_groups[group_id];

	if (group && client) {
		Player* player = client->GetPlayer();
		SpellEffects *se = player->GetSpellEffects();

		for (int i = 0; i < NUM_SPELL_EFFECTS; i++) {
			if (se[i].spell_id != 0xFFFFFFFF) {
				shared_ptr<LuaSpell> luaspell = se[i].spell;
				Spell* spell = luaspell->spell;

				if (spell && spell->GetSpellData()->group_spell && spell->GetSpellData()->friendly_spell) {
					if (luaspell->caster == player) {
						deque<GroupMemberInfo*>* members = group->GetMembers();
						deque<GroupMemberInfo*>::iterator itr;
						for (itr = members->begin(); itr != members->end(); itr++) {
							GroupMemberInfo* info = *itr;

							if (info->client == client || info->client->GetCurrentZone() != client->GetCurrentZone())
								continue;

							to_remove[info->client->GetPlayer()].push_back(luaspell);
						}
					} else {
						to_remove[player].push_back(luaspell);
					}
				}
			}
		}

		for (const auto& remove : to_remove) {
			Player* target = remove.first;
			vector<shared_ptr<LuaSpell>> effects = remove.second;

			for (const auto effect : effects) {
				target->RemoveSpellEffect(effect);
				target->RemoveSpellBonus(effect);
				target->RemoveSkillBonus(effect->spell->GetSpellID());

				Entity* pet = nullptr;
				Entity* charmed_pet = nullptr;

				if (target->HasPet()) {
					pet = target->GetPet();
					charmed_pet = target->GetCharmedPet();
				}

				if (pet) {
					pet->RemoveSpellEffect(effect);
					pet->RemoveSpellBonus(effect);
				}

				if (charmed_pet) {
					charmed_pet->RemoveSpellEffect(effect);
					charmed_pet->RemoveSpellBonus(effect);
				}
			}

			shared_ptr<Client> target_client = target->GetZone()->GetClientBySpawn(target);

			if (target_client) {
				EQ2Packet* packet = target->GetSkills()->GetSkillPacket(target_client->GetVersion());

				if (packet) {
					target_client->QueuePacket(packet);
				}
			}
		}
	}
	MGroups.releasereadlock(__FUNCTION__, __LINE__);
}

int32 PlayerGroupManager::GetGroupSize(int32 group_id) {
	int32 ret = 0;
	MGroups.readlock(__FUNCTION__, __LINE__);

	if (m_groups.count(group_id) > 0)
		ret = m_groups[group_id]->Size();

	MGroups.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}

void PlayerGroupManager::SendGroupQuests(int32 group_id, shared_ptr<Client> client) {
	GroupMemberInfo* info = 0;
	MGroups.readlock(__FUNCTION__, __LINE__);
	if (m_groups.count(group_id) > 0) {
		deque<GroupMemberInfo*>* members = m_groups[group_id]->GetMembers();
		deque<GroupMemberInfo*>::iterator itr;
		for (itr = members->begin(); itr != members->end(); itr++) {
			info = *itr;
			if (info->client) {
				LogWrite(PLAYER__DEBUG, 0, "Player", "Send Quest Journal...");
				info->client->SendQuestJournal(false, client);
				client->SendQuestJournal(false, info->client);
			}
		}
	}
	MGroups.releasereadlock(__FUNCTION__, __LINE__);
}

void PlayerGroupManager::SimpleGroupMessage(int32 group_id, const char* message) {
	MGroups.readlock(__FUNCTION__, __LINE__);

	if (m_groups.count(group_id) > 0)
		m_groups[group_id]->SimpleGroupMessage(message);

	MGroups.releasereadlock(__FUNCTION__, __LINE__);
}

void PlayerGroupManager::GroupMessage(int32 group_id, const char* message, ...) {
	va_list argptr;
	char buffer[4096];
	buffer[0] = 0;
	va_start(argptr, message);
	vsnprintf(buffer, sizeof(buffer), message, argptr);
	va_end(argptr);
	SimpleGroupMessage(group_id, buffer);
}

void PlayerGroupManager::GroupChatMessage(int32 group_id, Spawn* from, const char* message) {
	MGroups.readlock(__FUNCTION__, __LINE__);

	if (m_groups.count(group_id) > 0)
		m_groups[group_id]->GroupChatMessage(from, message);

	MGroups.releasereadlock(__FUNCTION__, __LINE__);
}

void PlayerGroupManager::MakeLeader(int32 group_id, Entity* new_leader) {
	MGroups.writelock(__FUNCTION__, __LINE__);

	if (m_groups.count(group_id) > 0)
		m_groups[group_id]->MakeLeader(new_leader);

	MGroups.releasewritelock(__FUNCTION__, __LINE__);
}

void PlayerGroupManager::UpdateGroupBuffs() {
	map<int32, PlayerGroup*>::iterator itr;
	deque<GroupMemberInfo*>::iterator member_itr;
	deque<GroupMemberInfo*>::iterator target_itr;
	map<int32, SkillBonusValue*>::iterator itr_skills;
	MaintainedEffects* me = 0;
	Spell* spell = 0;
	Entity* group_member = 0;
	SkillBonus* sb;
	EQ2Packet* packet;
	int32 i = 0;
	PlayerGroup* group = 0;
	Player* caster = 0;
	vector<int32> new_target_list;
	shared_ptr<Client> client = 0;
	bool has_effect = false;
	vector<BonusValues*>* sb_list = 0;
	BonusValues* bv = 0;
	Entity* pet = 0;
	Entity* charmed_pet = 0;

	for (itr = m_groups.begin(); itr != m_groups.end(); itr++) {
		group = itr->second;

		/* loop through the group members and see if any of them have any maintained spells that are group buffs and friendly.
		if so, update the list of targets and apply/remove effects as needed */

		for (member_itr = group->GetMembers()->begin(); member_itr != group->GetMembers()->end(); member_itr++) {
			if ((*member_itr)->client)
				caster = (*member_itr)->client->GetPlayer();
			else caster = 0;

			if (!caster || (*member_itr)->client->IsZoning())
				continue;

			if (!caster->GetMaintainedSpellBySlot(0))
				continue;

			// go through the player's maintained spells
			me = caster->GetMaintainedSpells();
			caster->GetMaintainedMutex()->readlock(__FUNCTION__, __LINE__);
			for (i = 0; i < NUM_MAINTAINED_EFFECTS; i++){
				if (me[i].spell_id == 0xFFFFFFFF)
					continue;

				shared_ptr<LuaSpell> luaspell = me[i].spell;

				if (!luaspell)
					continue;

				spell = luaspell->spell;

				if (spell && spell->GetSpellData()->group_spell && spell->GetSpellData()->friendly_spell &&
					(spell->GetSpellData()->target_type == SPELL_TARGET_GROUP_AE || spell->GetSpellData()->target_type == SPELL_TARGET_RAID_AE)) {

					luaspell->MSpellTargets.writelock(__FUNCTION__, __LINE__);

					for (target_itr = group->GetMembers()->begin(); target_itr != group->GetMembers()->end(); target_itr++) {
						group_member = (*target_itr)->member;

						if (!group_member)
							continue;

						if (group_member == caster)
							continue;

						if (!group_member->Alive())
							continue;

						client = (*target_itr)->client;

						has_effect = false;

						if (group_member->GetSpellEffect(spell->GetSpellID(), caster))
							has_effect = true;

						pet = 0;
						charmed_pet = 0;

						if (group_member->HasPet()) {
							pet = group_member->GetPet();
							charmed_pet = group_member->GetCharmedPet();
						}

						if (has_effect) {
							if (group_member->GetZone() != caster->GetZone() || caster->GetDistance(group_member) > spell->GetSpellData()->radius) {
								group_member->RemoveSpellEffect(luaspell);
								group_member->RemoveSpellBonus(luaspell);
								group_member->RemoveSkillBonus(spell->GetSpellID());
								if (client) {
									packet = ((Player*)group_member)->GetSkills()->GetSkillPacket(client->GetVersion());
								if (packet)
									client->QueuePacket(packet);
								}
								//Also remove group buffs from pet
								if (group_member->HasPet()) {
									pet = group_member->GetPet();
									charmed_pet = group_member->GetCharmedPet();

									if (pet) {
										pet->RemoveSpellEffect(luaspell);
										pet->RemoveSpellBonus(luaspell);
									}

									if (charmed_pet) {
										charmed_pet->RemoveSpellEffect(luaspell);
										charmed_pet->RemoveSpellBonus(luaspell);
									}
								}
							} else {
								new_target_list.push_back(group_member->GetID());

								if (pet)
									new_target_list.push_back(pet->GetID());

								if (charmed_pet)
									new_target_list.push_back(charmed_pet->GetID());
							}
						} else if (group_member->GetZone() == caster->GetZone() && caster->GetDistance(group_member) <= spell->GetSpellData()->radius) {
							group_member->AddSpellEffect(luaspell);
							new_target_list.push_back(group_member->GetID());

							if (pet) {
								pet->AddSpellEffect(luaspell);
								new_target_list.push_back(pet->GetID());
							}

							if (charmed_pet) {
								new_target_list.push_back(charmed_pet->GetID());
								charmed_pet->AddSpellEffect(luaspell);
							}

							// look for a spell bonus on caster's spell
							sb_list = caster->GetAllSpellBonuses(luaspell);
							for (int32 x = 0; x < sb_list->size(); x++) {
								bv = sb_list->at(x);
								group_member->AddSpellBonus(luaspell, bv->type, bv->value, bv->class_req);

								if (pet)
									pet->AddSpellBonus(luaspell, bv->type, bv->value, bv->class_req);

								if (charmed_pet)
									charmed_pet->AddSpellBonus(luaspell, bv->type, bv->value, bv->class_req);
							}

							sb_list->clear();
							safe_delete(sb_list);

							// look for a skill bonus on the caster's spell
							sb = caster->GetSkillBonus(me[i].spell_id);
							if (sb) {
								for (itr_skills = sb->skills.begin(); itr_skills != sb->skills.end(); itr_skills++)
									group_member->AddSkillBonus(sb->spell_id, (*itr_skills).second->skill_id, (*itr_skills).second->value);
							}
						}

						if (client) {
							packet = ((Player*)group_member)->GetSkills()->GetSkillPacket(client->GetVersion());
							if (packet)
								client->QueuePacket(packet);
						}
					}

					new_target_list.push_back(caster->GetID());
					luaspell->targets.swap(new_target_list);
					luaspell->MSpellTargets.releasewritelock(__FUNCTION__, __LINE__);
					new_target_list.clear();
				}
			}
			caster->GetMaintainedMutex()->releasereadlock(__FUNCTION__, __LINE__);
		}
	}
}

bool PlayerGroupManager::IsInGroup(int32 group_id, Entity* member) {
	bool ret = false;

	MGroups.readlock(__FUNCTION__, __LINE__);

	if (m_groups.count(group_id) > 0) {
		deque<GroupMemberInfo*>* members = m_groups[group_id]->GetMembers();
		for (int8 i = 0; i < members->size(); i++) {
			if (member == members->at(i)->member) {
				ret = true;
				break;
			}
		}
	}

	MGroups.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}
