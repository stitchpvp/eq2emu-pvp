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

#pragma once

#include <vector>
#include <map>
#include "../../common/types.h"

class Spawn;

#define CELLSIZEDEFAULT 150
#define FACECELLSIZEDEFAULT 15

struct Face {
	float Vertex1[3];
	float Vertex2[3];
	float Vertex3[3];
};

/*struct GridBounds {
	int32 Grid;
	float MinBounds[3];
	float MaxBounds[3];
};*/

struct Cell {
	std::vector<Spawn*> SpawnList;
	//std::map<int32, vector<Face*> > FaceList;
	//std::map<int32, GridBounds*> GridBounds;
};

struct FaceCell {
	std::map<int32, vector<Face*> > FaceList;
};

class SPGrid {
public:
	SPGrid(string file, int32 cellSize);
	~SPGrid();

	// Sets up the spacial partioning grid
	bool Init();

	// Get cell based on cell coordinates
	Cell* GetCell(int32 x, int32 z);

	// Get cell based on world coordinates
	Cell* GetCell(float x, float z);

	// Adds a face to the cells it needs to be in and sets its GridID
	void AddFace(Face* face, int32 grid);

	// Checks the faces below the given spawn to determine the GridID
	int32 GetGridID(Spawn* spawn);

	// Calculate a cell from spawn's position and add the spawn to it
	void AddSpawn(Spawn* spawn);

	// Adds the spawn to a specific spell
	void AddSpawn(Spawn* spawn, Cell* cell);

	// Removes the spawn from the cell it is currently in
	void RemoveSpawnFromCell(Spawn* spawn);

	// Get cell based on cell coordinates
	FaceCell* GetFaceCell(int32 x, int32 z);

	// Get cell based on world coordinates
	FaceCell* GetFaceCell(float x, float z);
private:
	// 1-D array for cells as it is better performance wise then 2-D
	std::vector<Cell> m_Cells;

	std::vector<FaceCell> m_FaceCells;

	string m_ZoneFile;
	int32 m_CellSize;

	float m_MinX;
	float m_MinZ;
	float m_MaxX;
	float m_MaxZ;

	int32 m_NumCellsX;
	int32 m_NumCellsZ;

	int32 m_NumFaceCellsX;
	int32 m_NumFaceCellsZ;
};