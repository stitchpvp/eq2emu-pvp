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

#ifndef __PLAYERGROUPS_H__
#define __PLAYERGROUPS_H__

#include <deque>
#include <map>

#include "../common/types.h"
#include "Entity.h"

using namespace std;

/// <summary>All the generic info for the group window, plus a client pointer for players</summary>
struct GroupMemberInfo {
	int32	group_id;
	string	name;
	string	zone;
	sint32	hp_current;
	sint32	hp_max;
	sint32	power_current;
	sint32	power_max;
	int16	level_current;
	int16	level_max;
	int8	race_id;
	int8	class_id;
	bool	leader;
	Client*	client;
	Entity*	member;
};

/// <summary>Represents a players group in game</summary>
class PlayerGroup {
public:
	PlayerGroup(int32 id);
	~PlayerGroup();

	/// <summary>Adds a new member to the players group</summary>
	/// <param name='member'>Entity to add to the group, can be a Player or NPC</param>
	/// <returns>True if the member was added</returns>
	bool AddMember(Entity* member);

	/// <summary>Removes a member from the players group</summary>
	/// <param name='member'>Entity to remove from the player group</param>
	/// <returns>True if the member was removed</param>
	bool RemoveMember(Entity* member);

	/// <summary>Removes all members from the group and destroys the group</summary>
	void Disband();

	/// <summary>Sends updates to all the clients in the group</summary>
	/// <param name='exclude'>Client to exclude from the update</param>
	void SendGroupUpdate(Client* exclude = 0);

	/// <summary>Gets the total number of members in the group</summary>
	/// <returns>int32, number of members in the group</returns>
	int32 Size() { return m_members.size(); }

	/// <summary>Gets a pointer to the list of members</summary>
	/// <returns>deque pointer</returns>
	deque<GroupMemberInfo*>* GetMembers() { return &m_members; }


	void SimpleGroupMessage(const char* message);
	void GroupChatMessage(Spawn* from, const char* message);
	void MakeLeader(Entity* new_leader);

private:
	int32					m_id;		// ID of this group
	deque<GroupMemberInfo*>	m_members;	// List of members in this group
	
};

/// <summary>Responsible for managing all the player groups in the world</summary>
class PlayerGroupManager {
public:
	PlayerGroupManager();
	~PlayerGroupManager();

	/// <summary>Adds a member to a group</summary>
	/// <param name='group_id'>ID of the group to add a member to</param>
	/// <param name='member'>Entity* to add to the group</param>
	/// <returns>True if the member was added to the group</returns>
	bool AddGroupMember(int32 group_id, Entity* member);

	/// <summary>Removes a member from a group</summary>
	/// <param name='group_id'>ID of the group to remove a member from</param>
	/// <param name='member'>Entity* to remove from the group</param>
	/// <returns>True if the member was removed from the group</returns>
	bool RemoveGroupMember(int32 group_id, Entity* member);

	/// <summary>Creates a new group with the provided Entity* as the leader</summary>
	/// <param name='leader'>The Entity* that will be the leader of the group</param>
	void NewGroup(Entity* leader);

	/// <summary>Removes the group from the group manager</summary>
	/// <param name='group_id'>ID of the group to remove</param>
	void RemoveGroup(int32 group_id);

	/// <summary>Handles a player inviting another player or NPC to a group</summary>
	/// <param name='leader'>Player that sent the invite</param>
	/// <param name='member'>Player or NPC that is the target of the invite</param>
	/// <returns>Error code if invite was unsuccessful, 0 if successful</returns>
	int8 Invite(Player* leader, Entity* member);

	/// <summary>Handles accepting of a group invite</summary>
	/// <param name='member'>Entity* that is accepting the invite</param>
	/// <returns>Error code if accepting the invite failed, 0 if successful<returns>
	int8 AcceptInvite(Entity* member);

	/// <summary>Handles declining of a group invite</summary>
	/// <param name='member'>Entity* that is declining the invite</param>
	void DeclineInvite(Entity* member);

	/// <summary>Checks to see if there is a group with the given id in the group manager</summary>
	/// <param name='group_id'>ID to check for</param>
	/// <returns>True if a group with the given ID is found</returns>
	bool IsGroupIDValid(int32 group_id);

	/// <summary>Send updates to all the clients in the group</summary>
	/// <param name='group_id'>ID of the group to send updates to</param>
	/// <param name='exclude'>Client* to exclude from the update, usually the one that triggers the update</param>
	void SendGroupUpdate(int32 group_id, Client* exclude = 0);

	/// <summary>
	/// Gets the group members for the given group, be sure to call GroupLock() before calling this and ReleaseGroupLock() after you 
	/// are done with the list.  This is for reading purposes only, the list should never be altered using this function
	/// </summary>
	/// <param name='group_id'>ID of the group who's members we want</param>
	/// <returns>deque pointer of the members in the group</returns>
	deque<GroupMemberInfo*>* GetGroupMembers(int32 group_id);

	/// <summary>Read locks the group list, no changes to the list should be made when using this</summary>
	/// <param name='function'>Name of the function called from, used for better debugging in the event of a deadlock</param>
	/// <param name='line'>Line number that this was called from, used for better debugging in the event of a deadlock</param>
	void GroupLock(const char* function = 0, int32 line = 0U) { MGroups.readlock(function, line); }

	/// <summary>Releases the readlock acquired from GroupLock()</summary>
	/// <param name='function'>Name of the function called from, used for better debugging in the event of a deadlock</param>
	/// <param name='line'>Line number that this was called from, used for better debugging in the event of a deadlock</param>
	void ReleaseGroupLock(const char* function = 0, int32 line = 0U) { MGroups.releasereadlock(function, line); }

	void ClearPendingInvite(Entity* member);

	void RemoveGroupBuffs(int32 group_id, Client* client);

	int32 GetGroupSize(int32 group_id);

	void SendGroupQuests(int32 group_id, Client* client);

	void SimpleGroupMessage(int32 group_id, const char* message);
	void GroupMessage(int32 group_id, const char* message, ...);
	void GroupChatMessage(int32 group_id, Spawn* from, const char* message);
	void MakeLeader(int32 group_id, Entity* new_leader);
	void UpdateGroupBuffs();

	bool IsInGroup(int32 group_id, Entity* member);

	// TODO: Any function below this comment
	bool IsSpawnInGroup(int32 group_id, string name); // used in follow
	Player* GetGroupLeader(int32 group_id);

private:
	int32								m_nextGroupID;			// Used to generate a new unique id for new groups

	map<int32, PlayerGroup*>			m_groups;				// int32 is the group id, PlayerGroup* is a pointer to the actual group
	map<string, string>					m_pendingInvites;		// First string is the person invited to the group, second string is the leader of the group

	Mutex								MGroups;				// Mutex for the group map (m_groups)
	Mutex								MPendingInvites;		// Mutex for the pending invites map (m_pendingInvites)
};

#endif