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
#include <map>

using namespace std;

// Visual States must use a hash table because of the large amount that exists and the large spacing
// between their ID's.  String and character arrays could not be used for the first iterator because
// it would require the same pointer to access it from the hash table, which is obviously not possible
// since the text is from the client.

// maximum amount of iterations it will attempt to find a entree
#define HASH_SEARCH_MAX 20

class VisualState
{
public:
	VisualState(int inID, char* inName){
		if(!inName)
			return;
		name = string(inName);
		id = inID;
	}

	int GetID() { return id; }
	const char* GetName() { return name.c_str(); }
	string GetNameString() { return name; }

private:
	int id;
	string name;
};

class Emote{
public:
	Emote(char* in_name, int in_visual_state, char* in_message, char* in_targeted_message){
		if(!in_name)
			return;
		name = string(in_name);
		visual_state = in_visual_state;
		if(in_message)
			message = string(in_message);
		if(in_targeted_message)
			targeted_message = string(in_targeted_message);
	}
	int GetVisualState() { return visual_state; }
	const char* GetName() { return name.c_str(); }
	const char* GetMessage() { return message.c_str(); }
	const char* GetTargetedMessage() { return targeted_message.c_str(); }

	string GetNameString() { return name; }
	string GetMessageString() { return message; }
	string GetTargetedMessageString() { return targeted_message; }
private:
	int visual_state;
	string name;
	string message;
	string targeted_message;
};

class VisualStates
{
public:
	~VisualStates(){
		Reset();
	}

	void Reset(){
		ClearVisualStates();
		ClearEmotes();
	}

	void ClearEmotes(){
		map<string, Emote*>::iterator map_list;
		for(map_list = emoteMap.begin(); map_list != emoteMap.end(); map_list++ )
			safe_delete(map_list->second);
		emoteMap.clear();
	}

	void ClearVisualStates(){
		map<string, VisualState*>::iterator map_list;
		for(map_list = visualStateMap.begin(); map_list != visualStateMap.end(); map_list++ )
			safe_delete(map_list->second);
		visualStateMap.clear();
	}

	void InsertVisualState(VisualState* vs){
		visualStateMap[vs->GetNameString()] = vs;
	}

	VisualState* FindVisualState(string var){
		if(visualStateMap.count(var) > 0)
			return visualStateMap[var];
		return 0;
	}

	void InsertEmote(Emote* emote){
		emoteMap[emote->GetNameString()] = emote;
	}

	Emote* FindEmote(string var){
		if(emoteMap.count(var) > 0)
			return emoteMap[var];
		return 0;
	}
private:
	map<string,VisualState*> visualStateMap;
	map<string,Emote*> emoteMap;
};

