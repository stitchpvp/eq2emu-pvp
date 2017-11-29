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
#include "../common/debug.h"

#include <iostream>
using namespace std;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errmsg.h>
//#include <mysqld_error.h>
#include <limits.h>
#include <ctype.h>
#include <assert.h>
#include <map>

// Disgrace: for windows compile
#ifdef WIN32
#include <WinSock2.h>
#include <windows.h>
#define snprintf	_snprintf
#define strncasecmp	_strnicmp
#define strcasecmp	_stricmp
#else
#include "unix.h"
#include <netinet/in.h>
#endif

#include "database.h"
#include "EQStream.h"
#include "packet_functions.h"
#include "emu_opcodes.h"
#ifdef WORLD
	#include "../WorldServer/WorldDatabase.h"
	extern WorldDatabase database;
#endif
#ifdef LOGIN
	#include "../LoginServer/LoginDatabase.h"
	extern LoginDatabase database;
#endif
#ifdef PARSER
	#include "../PacketParser/ParserDatabase.h"
	extern ParserDatabase database;
#endif

#ifdef PATCHER
	#include "../PatchServer/PatcherDatabase.h"
	extern PatcherDatabase database;
#endif
#include "../common/EQEMuError.h"
#include "../common/packet_dump.h"
#include "../common/Log.h"

Database::Database()
{
	InitVars();
	
}

bool Database::Init() {
	char host[200], user[200], passwd[200], database[200];
	int32 port=0;
	bool compression = false;
	bool items[6] = {false, false, false, false, false, false};
	
	if(!ReadDBINI(host, user, passwd, database, port, compression, items)) {
		exit(1);
	}
	
	if (!items[0] || !items[1] || !items[2] || !items[3])
	{
		LogWrite(DATABASE__ERROR, 0, "DB", "Incomplete DB.INI file.");
		LogWrite(DATABASE__ERROR, 0, "DB", "Read README.TXT!");
		exit (1);
	}
	
	int32 errnum = 0;
	char errbuf[MYSQL_ERRMSG_SIZE];
	if (!Open(host, user, passwd, database,port, &errnum, errbuf)) 
	{
		LogWrite(DATABASE__ERROR, 0, "DB", "Failed to connect to database: Error: %s", errbuf);
		HandleMysqlError(errnum);
		exit(1);
	}
	else
	{
		LogWrite(DATABASE__INFO, 0, "DB", "Using database '%s' at %s", database, host);
	}

	return true;
}

map<int16, int16> Database::GetVersions(){
	map<int16, int16> opcodes;
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "select distinct version_range1, version_range2 from opcodes");
	while(result && (row = mysql_fetch_row(result))){
		if(row[0] && row[1])
			opcodes[atoi(row[0])] = atoi(row[1]);
	}
	return opcodes;
}

map<string, uint16> Database::GetOpcodes(int16 version){
	map<string, uint16> opcodes;
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "select name, opcode from opcodes where %i between version_range1 and version_range2 order by version_range1, id", version);
	while(result && (row = mysql_fetch_row(result))){
		opcodes[row[0]] = atoi(row[1]);
	}
	return opcodes;
}

void Database::HandleMysqlError(int32 errnum) {
	switch(errnum) {
		case 0:
			break;
		case 1045: // Access Denied
		case 2001: {
			AddEQEMuError(EQEMuError_Mysql_1405, true);
			break;
		}
		case 2003: { // Unable to connect
			AddEQEMuError(EQEMuError_Mysql_2003, true);
			break;
		}
		case 2005: { // Unable to connect
			AddEQEMuError(EQEMuError_Mysql_2005, true);
			break;
		}
		case 2007: { // Unable to connect
			AddEQEMuError(EQEMuError_Mysql_2007, true);
			break;
		}
	}
}

void Database::InitVars() {

}

Database::~Database()
{
}
MYSQL_RES* Query::RunQuery2(QUERY_TYPE type, const char* format, ...){
	va_list args;
	va_start( args, format );
	#ifdef WIN32
		char * buffer;
		int buf_len = _vscprintf( format, args ) + 1;
		buffer = new char[buf_len];
		vsprintf( buffer, format, args );
	#else
		char* buffer;
		int buf_len; 
		va_list argcopy;
		va_copy(argcopy, args);
		buf_len = vsnprintf(NULL, 0, format, argcopy) + 1;
		va_end(argcopy);
		
		buffer = new char[buf_len];
		vsnprintf(buffer, buf_len, format, args);
	#endif
	va_end(args);
	query = string(buffer);

	
	safe_delete_array( buffer );
	

	return RunQuery2(query.c_str(), type);
}
MYSQL_RES* Query::RunQuery2(string in_query, QUERY_TYPE type){
	switch(type){
		case Q_SELECT:
			break;
		case Q_DBMS:
		case Q_REPLACE:
		case Q_DELETE:
		case Q_UPDATE:
			safe_delete(affected_rows);
			affected_rows = new int32;
			break;
		case Q_INSERT:
			safe_delete(last_insert_id);
			last_insert_id = new int32;
	}
	if(result){
		if(!multiple_results)
			multiple_results = new vector<MYSQL_RES*>();
		multiple_results->push_back(result);
	}	
	query = in_query;
	database.RunQuery(query.c_str(), query.length(), errbuf, &result, affected_rows, last_insert_id, &errnum, retry); 
	return result;
}
