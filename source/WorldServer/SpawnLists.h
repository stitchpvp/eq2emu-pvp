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
#ifndef EQ2_SPAWN_LISTS
#define EQ2_SPAWN_LISTS
#include "../common/types.h"
#include <vector>

#define SPAWN_ENTRY_TYPE_NPC		0
#define SPAWN_ENTRY_TYPE_OBJECT		1
#define SPAWN_ENTRY_TYPE_WIDGET		2
#define SPAWN_ENTRY_TYPE_SIGN		3
#define SPAWN_ENTRY_TYPE_GROUNDSPAWN 4

struct EntityCommand{
	string	name;
	float	distance;
	string	error_text;
	string	command;
	int16	cast_time;
	int32	spell_visual;
};
struct SpawnEntry{
	int32	spawn_entry_id;
	int32	spawn_location_id;
	int8	spawn_type;
	int32	spawn_id;
	float	spawn_percentage;
	int32	respawn;
	int32	expire_time;
	int32	expire_offset;
};
class SpawnLocation{
public:
	SpawnLocation(){
		x = 0; 
		y = 0;
		z = 0;
		heading = 0;
		total_percentage = 0;
		x_offset = 0;
		y_offset = 0;
		z_offset = 0;
		placement_id = 0;
		pitch = 0;
		roll = 0;
		grid_id = 0;
	}
	~SpawnLocation(){
		for(int32 i=0;i<entities.size();i++)
			safe_delete(entities[i]);
	}
	void AddSpawn(SpawnEntry* entity){ entities.push_back(entity); }
	vector<SpawnEntry*> entities;
	float	x;
	float	y;
	float	z;
	float	heading;
	float	x_offset;
	float	y_offset;
	float	z_offset;
	int32	placement_id;
	float   pitch;
	float   roll;
	float	total_percentage;
	int32	grid_id;
	string	script;
};
#endif

