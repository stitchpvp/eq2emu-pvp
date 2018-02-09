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

#include "SPGrid.h"
#include "../../common/Log.h"

SPGrid::SPGrid(string file, int32 cellSize) {
	m_ZoneFile = file;
	m_CellSize = cellSize;
	m_MinX = 0;
	m_MinZ = 0;
	m_MaxX = 0;
	m_MaxZ = 0;
	m_NumCellsX = 0;
	m_NumCellsZ = 0;
	m_NumFaceCellsX = 0;
	m_NumFaceCellsZ = 0;
}

SPGrid::~SPGrid() {
	vector<Cell>::iterator CellItr;
	map<int32, vector<Face*> >::iterator MapItr;
	vector<Face*>::iterator FaceItr;

	// Loop through the vector of cells
	/*for (CellItr = m_Cells.begin(); CellItr != m_Cells.end(); CellItr++) {
		// Loop through the map of vertices on this cell
		for (MapItr = (*CellItr).FaceList.begin(); MapItr != (*CellItr).FaceList.end(); MapItr++) {
			// Loop through the vector of faces in the map and delete the pointers
			for (FaceItr = (*MapItr).second.begin(); FaceItr != (*MapItr).second.end(); FaceItr++) {
				safe_delete((*FaceItr));
			}
		}
	}*/
}

bool SPGrid::Init() {
	// Make sure we have a zone file
	if (m_ZoneFile.empty()) {
		LogWrite(ZONE__ERROR, 0, "SPGrid", "SPGrid::Init() m_ZoneFile is empty.");
		return false;
	}

	// Make sure we have a cell size
	if (m_CellSize == 0)
		m_CellSize = CELLSIZEDEFAULT;

	// Open the map file for this zone
	string filePath = "Maps/" + m_ZoneFile + ".EQ2Map";
	FILE* file = fopen(filePath.c_str(), "rb");
	if (file == nullptr) {
		LogWrite(ZONE__WARNING, 0, "SPGrid", "SPGrid::Init() unable to open the map file for %s. (zoneserver will continue to run fine without it)", m_ZoneFile.c_str());
		return false;
	}

	// Read the string for the zone file name this was created for
	int8 strSize;
	char name[256];
	fread(&strSize, sizeof(int8), 1, file);
	LogWrite(ZONE__DEBUG, 0, "SPGrid", "strSize = %u", strSize);

	size_t len = fread(&name, sizeof(char), strSize, file);
	name[len] = '\0';
	LogWrite(ZONE__DEBUG, 0, "SPGrid", "name = %s", name);

	// Make sure file contents are for the correct zone
	if (string(name) != m_ZoneFile) {
		fclose(file);
		LogWrite(ZONE__ERROR, 0, "SPGrid", "SPGrid::Init() map contents (%s) do not match its name (%s).", &name, m_ZoneFile.c_str());
		return false;
	}

	// Read the min bounds
	fread(&m_MinX, sizeof(float), 1, file);
	fread(&m_MinZ, sizeof(float), 1, file);
	LogWrite(ZONE__DEBUG, 0, "SPGrid", "minx = %f, minz = %f", m_MinX, m_MinZ);

	// Read the max bounds
	fread(&m_MaxX, sizeof(float), 1, file);
	fread(&m_MaxZ, sizeof(float), 1, file);
	LogWrite(ZONE__DEBUG, 0, "SPGrid", "maxx = %f, maxz = %f", m_MaxX, m_MaxZ);

	// Calculate how many cells we need
	// in both the X and Z direction
	float width = m_MaxX - m_MinX;
	float height = m_MaxZ - m_MinZ;
	m_NumCellsX = ceil(width / m_CellSize);
	m_NumCellsZ = ceil(height / m_CellSize);
	LogWrite(ZONE__DEBUG, 0, "SPGrid", "CellSize = %u, x cells = %u, z cells = %u", m_CellSize, m_NumCellsX, m_NumCellsZ);

	// Allocate all the cells
	m_Cells.resize(m_NumCellsZ * m_NumCellsX);

	m_NumFaceCellsX = ceil(width / FACECELLSIZEDEFAULT);
	m_NumFaceCellsZ = ceil(height / FACECELLSIZEDEFAULT);
	m_FaceCells.resize(m_NumFaceCellsX * m_NumFaceCellsZ);

	// Read the number of grids
	int32 NumGrids;
	fread(&NumGrids, sizeof(int32), 1, file);
	LogWrite(ZONE__DEBUG, 0, "SPGrid", "NumGrids = %u", NumGrids);

	// Loop through the grids loading the face list
	for (int32 i = 0; i < NumGrids; i++) {
		// Read the grid id
		int32 GridID;
		fread(&GridID, sizeof(int32), 1, file);
		LogWrite(ZONE__DEBUG, 0, "SPGrid", "GridID = %u", GridID);

		// Read the number of vertices
		int32 NumFaces;
		fread(&NumFaces, sizeof(int32), 1, file);
		LogWrite(ZONE__DEBUG, 0, "SPGrid", "NumFaces = %u", NumFaces);

		// Loop through the vertices list reading
		// 3 at a time to creat a triangle (face)
		for (int32 y = 0; y < NumFaces; ) {

			// Each vertex need an x,y,z coordinate and 
			// we will be reading 3 to create the face
			float x1, x2, x3;
			float y1, y2, y3;
			float z1, z2, z3;

			// Read the first vertex
			fread(&x1, sizeof(float), 1, file);
			fread(&y1, sizeof(float), 1, file);
			fread(&z1, sizeof(float), 1, file);
			y++;

			// Read the second vertex
			fread(&x2, sizeof(float), 1, file);
			fread(&y2, sizeof(float), 1, file);
			fread(&z2, sizeof(float), 1, file);
			y++;

			// Read the third (final) vertex
			fread(&x3, sizeof(float), 1, file);
			fread(&y3, sizeof(float), 1, file);
			fread(&z3, sizeof(float), 1, file);
			y++;
			
			// Create the face and add it to the grid
			Face* face = new Face;
			face->Vertex1[0] = x1;
			face->Vertex1[1] = y1;
			face->Vertex1[2] = z1;

			face->Vertex2[0] = x2;
			face->Vertex2[1] = y2;
			face->Vertex2[2] = z2;

			face->Vertex3[0] = x3;
			face->Vertex3[1] = y3;
			face->Vertex3[2] = z3;

			AddFace(face, GridID);
		}
	}

	fclose(file);

	/*map<int32, vector<Face*> >::iterator itr;
	vector<Face*>::iterator itr2;
	for (int32 i = 0; i < m_Cells.size(); i++) {
		Cell& cell = m_Cells[i];

		for (itr = cell.FaceList.begin(); itr != cell.FaceList.end(); itr++) {
			float min_x = 0.0f;
			float min_y = 0.0f;
			float min_z = 0.0f;
			float max_x = 0.0f;
			float max_y = 0.0f;
			float max_z = 0.0f;

			for (itr2 = (*itr).second.begin(); itr2 != (*itr).second.end(); itr2++) {
				Face* face = (*itr2);
				if (min_x == 0.0f || face->Vertex1[0] < min_x)
					min_x = face->Vertex1[0];
				if (face->Vertex2[0] < min_x)
					min_x = face->Vertex2[0];
				if (face->Vertex3[0] < min_x)
					min_x = face->Vertex3[0];

				if (min_y == 0.0f || face->Vertex1[1] < min_y)
					min_y = face->Vertex1[1];
				if (face->Vertex2[1] < min_y)
					min_y = face->Vertex2[1];
				if (face->Vertex3[1] < min_y)
					min_y = face->Vertex3[1];

				if (min_z == 0.0f || face->Vertex1[2] < min_z)
					min_z = face->Vertex1[2];
				if (face->Vertex2[2] < min_z)
					min_z = face->Vertex2[2];
				if (face->Vertex3[2] < min_z)
					min_z = face->Vertex3[2];

				// Max bounds
				if (max_x == 0.0f || face->Vertex1[0] > max_x)
					max_x = face->Vertex1[0];
				if (face->Vertex2[0] > max_x)
					max_x = face->Vertex2[0];
				if (face->Vertex3[0] > max_x)
					max_x = face->Vertex3[0];

				if (max_y == 0.0f || face->Vertex1[1] > max_y)
					max_y = face->Vertex1[1];
				if (face->Vertex2[1] > max_y)
					max_y = face->Vertex2[1];
				if (face->Vertex3[1] > max_y)
					max_y = face->Vertex3[1];

				if (max_z == 0.0f || face->Vertex1[2] > max_z)
					max_z = face->Vertex1[2];
				if (face->Vertex2[2] > max_z)
					max_z = face->Vertex2[2];
				if (face->Vertex3[2] > max_z)
					max_z = face->Vertex3[2];
			}

			GridBounds* bounds = new GridBounds;
			bounds->MinBounds[0] = min_x;
			bounds->MinBounds[1] = min_y;
			bounds->MinBounds[2] = min_z;

			bounds->MaxBounds[0] = max_x;
			bounds->MaxBounds[1] = max_y;
			bounds->MaxBounds[2] = max_z;

			cell.GridBounds[(*itr).first] = bounds;
		}
	}*/

	return true;
}

Cell* SPGrid::GetCell(int32 x, int32 z) {
	if (x >= m_NumCellsX)
		x = m_NumCellsX - 1;

	if (z >= m_NumCellsZ)
		z = m_NumCellsZ - 1;

	return &m_Cells[z * m_NumCellsX + x];
}

Cell* SPGrid::GetCell(float x, float z) {
	// As cell grid coordinates are all positive we need to
	// modify the coordinates by subtracting the min bounds
	float newX = x - m_MinX;
	float newZ = z - m_MinZ;

	// Get the cell coordinates by doing int division
	// with the modified coordinates and the cell size
	int32 CellX = (int32)(newX / m_CellSize);
	int32 CellZ = (int32)(newZ / m_CellSize);

	return GetCell(CellX, CellZ);
}

FaceCell* SPGrid::GetFaceCell(int32 x, int32 z) {
	if (x >= m_NumFaceCellsX)
		x = m_NumFaceCellsX - 1;

	if (z >= m_NumFaceCellsZ)
		z = m_NumFaceCellsZ - 1;

	return &m_FaceCells[z * m_NumFaceCellsX + x];
}

FaceCell* SPGrid::GetFaceCell(float x, float z) {
	// As cell grid coordinates are all positive we need to
	// modify the coordinates by subtracting the min bounds
	float newX = x - m_MinX;
	float newZ = z - m_MinZ;

	// Get the cell coordinates by doing int division
	// with the modified coordinates and the cell size
	int32 CellX = (int32)(newX / FACECELLSIZEDEFAULT);
	int32 CellZ = (int32)(newZ / FACECELLSIZEDEFAULT);

	return GetFaceCell(CellX, CellZ);
}

void SPGrid::AddFace(Face* face, int32 grid) {
	// As each face has three vertices we will need to check the cell
	// for all of them and add the face to each cell that it is within

	// Get the cell at the first vertex position (X and Z, Y is vertical in EQ2)
	// as this is the first check we will add it to this cell and compare it 
	// to the other two cells we get for the other two verticies
	FaceCell* cell = GetFaceCell(face->Vertex1[0], face->Vertex1[2]);
	cell->FaceList[grid].push_back(face);

	// Get the cells for the other two verticies and compare
	FaceCell* cell2 = GetFaceCell(face->Vertex2[0], face->Vertex2[2]);
	FaceCell* cell3 = GetFaceCell(face->Vertex3[0], face->Vertex3[2]);

	// If cell 2 is not the same cell as the original cell then add the face to cell2
	if (cell2 != cell)
		cell2->FaceList[grid].push_back(face);

	// If cell 3 is not the same as the original cell AND not the same as cell 2 then add the face to cell 3
	if (cell3 != cell && cell3 != cell2)
		cell3->FaceList[grid].push_back(face);
}

float rayIntersectsTriangle(float *p, float *d, float *v0, float *v1, float *v2);

int32 SPGrid::GetGridID(Spawn * spawn) {
	FaceCell* cell = GetFaceCell(spawn->GetX(), spawn->GetZ());
	

	/*if (cell->GridBounds.size() == 1)
		return cell->FaceList.begin()->first;*/

	// Create the starting point for the trace
	float point[3];
	point[0] = spawn->GetX();
	point[1] = spawn->GetY();
	point[2] = spawn->GetZ();

	// Create the direction for the trace, as we want what
	// is below it will just be -1 in the y direction
	float direction[3];
	direction[0] = 0.0f;
	direction[1] = -1.0f;
	direction[2] = 0.0f;

	float MinDistance = 0.0f;
	int32 Grid = 0;

	/*map<int32, GridBounds*>::iterator itr;
	for (itr = cell->GridBounds.begin(); itr != cell->GridBounds.end(); itr++) {
		GridBounds* bounds = (*itr).second;

		if (point[0] >= bounds->MinBounds[0] && point[1] >= bounds->MinBounds[1] && point[2] >= bounds->MinBounds[2]
			&& point[0] <= bounds->MaxBounds[0] && point[1] <= bounds->MaxBounds[1] && point[2] <= bounds->MaxBounds[2]) {

			vector<Face*>::iterator itr2;
			for (itr2 = cell->FaceList[(*itr).first].begin(); itr2 != cell->FaceList[(*itr).first].end(); itr2++) {
				Face* face = *itr2;
				float distance;
				if ((distance = rayIntersectsTriangle(point, direction, face->Vertex1, face->Vertex2, face->Vertex3)) != 0) {
					if (MinDistance == 0.0f || distance < MinDistance) {
						MinDistance = distance;
						Grid = (*itr).first;
					}
				}
			}
		}
	}*/
	map<int32, vector<Face*> >::iterator mapitr;
	for (mapitr = cell->FaceList.begin(); mapitr != cell->FaceList.end(); mapitr++) {
		vector<Face*>::iterator itr;
		for (itr = (*mapitr).second.begin(); itr != (*mapitr).second.end(); itr++) {
			Face* face = *itr;
			float distance;
			if ((distance = rayIntersectsTriangle(point, direction, face->Vertex1, face->Vertex2, face->Vertex3)) != 0) {
				if (MinDistance == 0.0f || distance < MinDistance) {
					MinDistance = distance;
					Grid = (*mapitr).first;
				}
			}
		}
	}

	return Grid;
}

void SPGrid::AddSpawn(Spawn * spawn) {
	Cell* cell = GetCell(spawn->GetX(), spawn->GetZ());
	AddSpawn(spawn, cell);
}

void SPGrid::AddSpawn(Spawn * spawn, Cell * cell) {
	cell->SpawnList.push_back(spawn);
	spawn->Cell_Info.CurrentCell = cell;
	spawn->Cell_Info.CellListIndex = cell->SpawnList.size() - 1;
}

void SPGrid::RemoveSpawnFromCell(Spawn * spawn) {
	vector<Spawn*>& spawns = spawn->Cell_Info.CurrentCell->SpawnList;

	// Only do the vector swap if the vector has more than 1 spawn in it
	if (spawns.size() > 1) {
		// Swap the last spawn in this list to our position and update its stored index to match its new index
		spawns[spawn->Cell_Info.CellListIndex] = spawns.back();
		spawns[spawn->Cell_Info.CellListIndex]->Cell_Info.CellListIndex = spawn->Cell_Info.CellListIndex;
	}

	// Remove the last spawn from the list which should now be the spawn passed as a parameter
	spawns.pop_back();

	// Reset the spawns CellInfo to default values now that it is no longer in a cell
	spawn->Cell_Info.CellListIndex = -1;
	spawn->Cell_Info.CurrentCell = nullptr;
}

/**********************************************************************
 
 Math functions/macros to test a ray intersection in 3D space

**********************************************************************/

/* a = b - c */
#define vector(a,b,c) \
	(a)[0] = (b)[0] - (c)[0];	\
	(a)[1] = (b)[1] - (c)[1];	\
	(a)[2] = (b)[2] - (c)[2];

#define crossProduct(a,b,c) \
	(a)[0] = (b)[1] * (c)[2] - (c)[1] * (b)[2]; \
	(a)[1] = (b)[2] * (c)[0] - (c)[2] * (b)[0]; \
	(a)[2] = (b)[0] * (c)[1] - (c)[0] * (b)[1];

#define innerProduct(v,q) \
	((v)[0] * (q)[0] + \
	(v)[1] * (q)[1] + \
	(v)[2] * (q)[2])

// all parameters should be vectors (float[3])
float rayIntersectsTriangle(float *p, float *d, float *v0, float *v1, float *v2) {

	float e1[3], e2[3], h[3], s[3], q[3];
	float a, f, u, v;
	vector(e1, v1, v0);
	vector(e2, v2, v0);

	crossProduct(h, d, e2);
	a = innerProduct(e1, h);

	if (a > -0.00001 && a < 0.00001)
		return 0;

	f = 1 / a;
	vector(s, p, v0);
	u = f * (innerProduct(s, h));

	if (u < 0.0 || u > 1.0)
		return 0;

	crossProduct(q, s, e1);
	v = f * innerProduct(d, q);

	if (v < 0.0 || u + v > 1.0)
		return 0;

	// at this stage we can compute t to find out where
	// the intersection point is on the line
	float t = f * innerProduct(e2, q);

	if (t > 0.00001) // ray intersection
		return t;

	else // this means that there is a line intersection
		 // but not a ray intersection
		return 0;
}