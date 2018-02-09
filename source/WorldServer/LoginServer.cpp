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
#include "../common/Log.h"
#include <iostream>
using namespace std;
#include <string.h>
#include <stdio.h>
#include <iomanip>
using namespace std;
#include <stdlib.h>
#include "../common/md5.h"
#include "../common/version.h"
#include "../common/GlobalHeaders.h"

#ifdef WIN32
#include <process.h>
#include <WinSock2.h>
#include <windows.h>

#define snprintf	_snprintf
#define vsnprintf	_vsnprintf
#define strncasecmp	_strnicmp
#define strcasecmp	_stricmp
#else // Pyro: fix for linux
#include <sys/socket.h>
#ifdef FREEBSD //Timothy Whitman - January 7, 2003
#include <sys/types.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#include "../common/unix.h"

#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
extern int errno;
#endif

#include "../common/servertalk.h"
#include "LoginServer.h"
#include "../common/packet_dump.h"
#include "net.h"
#include "zoneserver.h"
#include "WorldDatabase.h"
#include "Variables.h"
#include "World.h"
#include "../common/ConfigReader.h"
#include "Rules/Rules.h"

extern sint32			numzones;
extern sint32			numclients;
extern NetConnection net;
extern LoginServer loginserver;
extern WorldDatabase	database;
extern ZoneAuth zone_auth;
extern Variables variables;
extern ZoneList zone_list;
extern ClientList client_list;
extern volatile bool	RunLoops;
volatile bool LoginLoopRunning = false;
extern ConfigReader configReader;
extern RuleManager rule_manager;

bool AttemptingConnect = false;

LoginServer::LoginServer(const char* iAddress, int16 iPort) {
	LoginServerIP = ResolveIP(iAddress);
	LoginServerPort = iPort;
	statusupdate_timer = new Timer(LoginServer_StatusUpdateInterval);
	tcpc = new TCPConnection(false);
	pTryReconnect = true;
	try_auto_update = true;
	updates_always = false;
	updates_verbose = false;
	updates_auto_data = false;
	updates_ask = false;
	minLockedStatus = 100;
	maxPlayers = -1;
	minGameFullStatus = 100;
	update_server_verified = false;
	last_checked_time = 0;
	UpdateServerPort = 0;
	UpdateServerIP = 0;
	update_server_completed = false;
	data_waiting = 0;
	zone_updates = 0;
	loginEquip_updates = 0;
}

LoginServer::~LoginServer() {
	delete statusupdate_timer;
	delete tcpc;
}

void LoginServer::SendImmediateEquipmentUpdatesForChar(int32 char_id) {
	LogWrite(WORLD__DEBUG, 5, "World", "Sending login equipment updates for char_id: %u", char_id);

	int16 count = 0;
	if(!loginEquip_updates)
		loginEquip_updates = database.GetEquipmentUpdates(char_id);
	if(loginEquip_updates && loginEquip_updates->size() > 0)
	{
		map<int32, LoginEquipmentUpdate> send_map;
		int32 size = 0;
		MutexMap<int32, LoginEquipmentUpdate>::iterator itr = loginEquip_updates->begin();
		while(itr.Next())
		{
			send_map[itr->first] = itr->second;
			size += sizeof(EquipmentUpdate_Struct);
			loginEquip_updates->erase(itr->first);
			count++;
		}
		ServerPacket* outpack = new ServerPacket(ServerOP_LoginEquipment, size + sizeof(EquipmentUpdateList_Struct)+5);
		EquipmentUpdateList_Struct* updates = (EquipmentUpdateList_Struct*)outpack->pBuffer;
		updates->total_updates = count;
		int32 pos = sizeof(EquipmentUpdateList_Struct);
		map<int32, LoginEquipmentUpdate>::iterator send_itr;
		for(send_itr = send_map.begin(); send_itr != send_map.end(); send_itr++)
		{
			EquipmentUpdate_Struct* update = (EquipmentUpdate_Struct*)(outpack->pBuffer + pos);
			update->id				= send_itr->first;
			update->world_char_id	= send_itr->second.world_char_id;
			update->equip_type		= send_itr->second.equip_type;
			update->red				= send_itr->second.red;
			update->green			= send_itr->second.green;
			update->blue			= send_itr->second.blue;
			update->highlight_red	= send_itr->second.red;
			update->highlight_green	= send_itr->second.green;
			update->highlight_blue	= send_itr->second.blue;
			update->slot			= send_itr->second.slot;
			pos += sizeof(EquipmentUpdate_Struct);
		}
		SendPacket(outpack);
		outpack->Deflate();
		safe_delete(outpack);
	}
	if(loginEquip_updates && count)
		loginEquip_updates->clear();
	if(loginEquip_updates && loginEquip_updates->size() == 0)
	{
		database.UpdateLoginEquipment();
		safe_delete(loginEquip_updates);
	}

}

bool LoginServer::Process() {
	if(last_checked_time > Timer::GetCurrentTime2())
		return true;
	last_checked_time = Timer::GetCurrentTime2() + 50;
	bool ret = true;
	if (statusupdate_timer->Check()) {
		this->SendStatus();
	}

	/************ Get all packets from packet manager out queue and process them ************/
	ServerPacket *pack = 0;
	while((pack = tcpc->PopPacket()))
	{
		switch(pack->opcode)
		{
		case ServerOP_LSFatalError:
			{
				LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): ServerOP_LSFatalError", pack->opcode, pack->opcode);

				LogWrite(WORLD__ERROR, 0, "World", "Login Server returned a fatal error: %s\nDisabling reconnect.", pack->pBuffer);
				pTryReconnect = false;
				ret = false;
				break;
			}
		case ServerOP_CharTimeStamp: 
			{
				LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): ServerOP_CharTimeStamp", pack->opcode, pack->opcode);

				if(pack->size != sizeof(CharacterTimeStamp_Struct))
					break;

				CharacterTimeStamp_Struct* cts = (CharacterTimeStamp_Struct*) pack->pBuffer;

				// determine if the character exists and retrieve its latest timestamp from the world server
				bool char_exist = false;
				//int32 character_timestamp = database.GetCharacterTimeStamp(cts->char_id,cts->account_id,&char_exist);

				if(!char_exist)
				{
					//Character doesn't exist, get rid of it
					SendDeleteCharacter ( cts );
					break;
				}

				break;
			}
		case ServerOP_GetTableData:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): ServerOP_GetTableData", pack->opcode, pack->opcode);

			uchar* tmp = new uchar[pack->size];
			memcpy(tmp, pack->pBuffer, pack->size);
			data_updates_waiting.push_back(tmp);
			break;
		}
		case ServerOP_GetTableQuery:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): ServerOP_GetTableQuery", pack->opcode, pack->opcode);

			uchar* tmp = new uchar[pack->size];
			memcpy(tmp, pack->pBuffer, pack->size);
			table_updates_waiting.push_back(tmp);
			break;
		}
		case ServerOP_GetLatestTables:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): ServerOP_GetLatestTables", pack->opcode, pack->opcode);

			data_waiting = new uchar[pack->size];
			memcpy(data_waiting, pack->pBuffer, pack->size);
			break;
		}

		// Push Character Select "item appearances" to login_equipment table
		case ServerOP_LoginEquipment:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): ServerOP_LoginEquipment", pack->opcode, pack->opcode);

			LogWrite(MISC__TODO, 0, "TODO", "Implement map<character id <map<slot id, updatestruct> > method to update Login.\n%s, %s, %i", __FILE__, __FUNCTION__, __LINE__);

			if( pack->size == sizeof(EquipmentUpdateRequest_Struct) )
			{
				int16 max = ((EquipmentUpdateRequest_Struct*)pack->pBuffer)->max_per_batch;
				int16 count = 0;
				if(!loginEquip_updates)
					loginEquip_updates = database.GetEquipmentUpdates();
				if(loginEquip_updates && loginEquip_updates->size() > 0)
				{
					map<int32, LoginEquipmentUpdate> send_map;
					int32 size = 0;
					MutexMap<int32, LoginEquipmentUpdate>::iterator itr = loginEquip_updates->begin();
					while(itr.Next() && count < max)
					{
						send_map[itr->first] = itr->second;
						size += sizeof(EquipmentUpdate_Struct);
						loginEquip_updates->erase(itr->first);
						count++;
					}
					ServerPacket* outpack = new ServerPacket(ServerOP_LoginEquipment, size + sizeof(EquipmentUpdateList_Struct)+5);
					EquipmentUpdateList_Struct* updates = (EquipmentUpdateList_Struct*)outpack->pBuffer;
					updates->total_updates = count;
					int32 pos = sizeof(EquipmentUpdateList_Struct);
					map<int32, LoginEquipmentUpdate>::iterator send_itr;
					for(send_itr = send_map.begin(); send_itr != send_map.end(); send_itr++)
					{
						EquipmentUpdate_Struct* update = (EquipmentUpdate_Struct*)(outpack->pBuffer + pos);
						update->id				= send_itr->first;
						update->world_char_id	= send_itr->second.world_char_id;
						update->equip_type		= send_itr->second.equip_type;
						update->red				= send_itr->second.red;
						update->green			= send_itr->second.green;
						update->blue			= send_itr->second.blue;
						update->highlight_red	= send_itr->second.red;
						update->highlight_green	= send_itr->second.green;
						update->highlight_blue	= send_itr->second.blue;
						update->slot			= send_itr->second.slot;
						pos += sizeof(EquipmentUpdate_Struct);
					}
					SendPacket(outpack);
					outpack->Deflate();
					safe_delete(outpack);
				}
				if(loginEquip_updates && count < max)
					loginEquip_updates->clear();
				if(loginEquip_updates && loginEquip_updates->size() == 0)
				{
					database.UpdateLoginEquipment();
					safe_delete(loginEquip_updates);
				}
			}
			break;
		}

		case ServerOP_ZoneUpdates:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): ServerOP_ZoneUpdates", pack->opcode, pack->opcode);

			if(pack->size == sizeof(ZoneUpdateRequest_Struct)){
				int16 max = ((ZoneUpdateRequest_Struct*)pack->pBuffer)->max_per_batch;
				int16 count = 0;
				if(!zone_updates)
					zone_updates = database.GetZoneUpdates();
				if(zone_updates && zone_updates->size() > 0){
					map<int32, LoginZoneUpdate> send_map;
					int32 size = 0;
					MutexMap<int32, LoginZoneUpdate>::iterator itr = zone_updates->begin();
					while(itr.Next() && count < max){
						send_map[itr->first] = itr->second;
						size += sizeof(ZoneUpdate_Struct) + itr->second.name.length() + itr->second.description.length();
						zone_updates->erase(itr->first);
						count++;
					}
					ServerPacket* outpack = new ServerPacket(ServerOP_ZoneUpdates, size + sizeof(ZoneUpdateList_Struct)+5);
					ZoneUpdateList_Struct* updates = (ZoneUpdateList_Struct*)outpack->pBuffer;
					updates->total_updates = count;
					int32 pos = sizeof(ZoneUpdateList_Struct);
					map<int32, LoginZoneUpdate>::iterator send_itr;
					for(send_itr = send_map.begin(); send_itr != send_map.end(); send_itr++){
						ZoneUpdate_Struct* update = (ZoneUpdate_Struct*)(outpack->pBuffer + pos);
						update->zone_id = send_itr->first;
						update->zone_name_length = send_itr->second.name.length();
						update->zone_desc_length = send_itr->second.description.length();
						strcpy(update->data, send_itr->second.name.c_str());
						strcpy(update->data + send_itr->second.name.length(), send_itr->second.description.c_str());
						pos += sizeof(ZoneUpdate_Struct) + send_itr->second.name.length() + send_itr->second.description.length();
					}
					SendPacket(outpack);
					outpack->Deflate();
					safe_delete(outpack);
				}
				if(zone_updates && count < max)
					zone_updates->clear();
				if(zone_updates && zone_updates->size() == 0){
					database.UpdateLoginZones();
					safe_delete(zone_updates);
				}
			}
			break;
		}
		case ServerOP_CharacterCreate:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): ServerOP_CharacterCreate", pack->opcode, pack->opcode);

			int16 version = 1;
			if(pack->pBuffer[0] > 0)
				memcpy(&version, pack->pBuffer, sizeof(int16));
			//DumpPacket(pack->pBuffer,pack->size);
			PacketStruct* packet = configReader.getStruct("CreateCharacter", version);
			int8 resp = 0;
			int32 acct_id = 0;
			int32 char_id = 0;
			if(packet && packet->LoadPacketData(pack->pBuffer+sizeof(int16),pack->size - sizeof(int16))){
				EQ2_16BitString name = packet->getType_EQ2_16BitString_ByName("name");
				resp = database.CheckNameFilter(name.data.c_str());
				acct_id = packet->getType_int32_ByName("account_id");
				LogWrite(WORLD__DEBUG, 0, "World", "Response: %i", (int)resp);
				
				sint16 lowestStatus = database.GetLowestCharacterAdminStatus(acct_id);
				if(lowestStatus == -2)
					resp = UNKNOWNERROR_REPLY2;
				else if(resp == CREATESUCCESS_REPLY)
					char_id = database.SaveCharacter(packet, acct_id);
			}
			else{
				LogWrite(WORLD__ERROR, 0, "World", "Invalid creation request!");
				resp = UNKNOWNERROR_REPLY;
			}
			// send name filter response data back to the login server
			SendFilterNameResponse ( resp , acct_id , char_id );
			safe_delete(packet);
			break;
			}
		case ServerOP_BasicCharUpdate: {
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): ServerOP_BasicCharUpdate", pack->opcode, pack->opcode);

			if(pack->size != sizeof(CharDataUpdate_Struct))
				break;

			CharDataUpdate_Struct* cdu = (CharDataUpdate_Struct*) pack->pBuffer;

			switch(cdu->update_field)
			{
			case DELETE_UPDATE_FLAG:
				{
					LogWrite(WORLD__DEBUG, 0, "World", "Delete character request: %i %i",cdu->account_id,cdu->char_id );
					database.DeleteCharacter(cdu->account_id,cdu->char_id);
					break;			
				}
			}
			break;
									   }
		case ServerOP_UsertoWorldReq:{
			LogWrite(OPCODE__DEBUG, 0, "Opcode", "Opcode 0x%X (%i): ServerOP_UsertoWorldReq", pack->opcode, pack->opcode);

			UsertoWorldRequest_Struct* utwr = (UsertoWorldRequest_Struct*) pack->pBuffer;	
			/*int32 id = database.GetAccountIDFromLSID(utwr->lsaccountid);
			sint16 status = database.CheckStatus(id);
			*/

			int32 access_key = 0;

			// if it is a accepted login, we add the zone auth request
			access_key = DetermineCharacterLoginRequest ( utwr );

			if (  access_key != 0 )
			{
				zone_auth.PurgeInactiveAuth();
				char* characterName = database.GetCharacterName( utwr->char_id );
				if(characterName != 0){
					ZoneAuthRequest* zar = new ZoneAuthRequest(utwr->lsaccountid,characterName,access_key);
					zar->setFirstLogin ( true );
					zone_auth.AddAuth(zar);
					safe_delete_array(characterName);
				}
			}
			break;
			}
		case ServerOP_ResetDatabase:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): ServerOP_ResetDatabase", pack->opcode, pack->opcode);

			database.ResetDatabase();
			break;
		}
		case UpdateServerOP_Verified:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): UpdateServerOP_Verified", pack->opcode, pack->opcode);

			update_server_verified = true;
			break;
		}
		case UpdateServerOP_DisplayMsg:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): UpdateServerOP_DisplayMsg", pack->opcode, pack->opcode);

			LogWrite(WORLD__WARNING, 0, "World", "\t%s", pack->pBuffer);
			break;
		}
		case UpdateServerOP_Completed:{
			LogWrite(OPCODE__DEBUG, 1, "Opcode", "Opcode 0x%X (%i): UpdateServerOP_Completed", pack->opcode, pack->opcode);

			update_server_completed = true;
			break;
		}

		default:
			{
				LogWrite(WORLD__ERROR, 0, "World", "Unhandled opcode: %i", pack->opcode);
				DumpPacket(pack);
			}
		}
		safe_delete(pack);
	}
	return ret;
}

// this should always be called in a new thread
#ifdef WIN32
void AutoInitLoginServer(void *tmp) {
#else
void *AutoInitLoginServer(void *tmp) {
#endif
	if (loginserver.GetState() == TCPS_Ready) {
		InitLoginServer();
	}
#ifndef WIN32
	return 0;
#endif
}

bool InitLoginServer() {
	if (loginserver.GetState() != TCPS_Ready) {
		LogWrite(WORLD__ERROR, 0, "World", "InitLoginServer() while already attempting connect.");
		return false;
	}
	if (!net.LoginServerInfo) {
		LogWrite(WORLD__ERROR, 0, "World", "Login server info not loaded.");
		return false;
	}

	AttemptingConnect = true;
	int16 port;
	char* address = net.GetLoginInfo(&port);
	loginserver.Connect(address, port);
	return true;
}

void LoginServer::InitLoginServerVariables() 
{
	minLockedStatus		= rule_manager.GetGlobalRule(R_World, ServerLockedOverrideStatus)->GetSInt16();
	maxPlayers			= rule_manager.GetGlobalRule(R_World, MaxPlayers)->GetSInt16();
	minGameFullStatus	= rule_manager.GetGlobalRule(R_World, MaxPlayersOverrideStatus)->GetSInt16();
}

void LoginServer::ProcessDataUpdate(uchar* data){
	TableDataQuery data_query;
	TableQuery table_queries;
	data_query.DeSerialize(data);
	string query;

	for(int32 i=0;i<data_query.num_queries;i++)
	{
		query = string("replace into ").append(data_query.tablename).append(" ");
		query.append(data_query.columns).append(" values(").append(data_query.queries[i]->query).append(")\0");
		char* new_query = new char[query.length() + 1];
		memset(new_query, 0, query.length() + 1);
		memcpy(new_query, query.c_str(), query.length());
		table_queries.AddQuery(new_query);
	}

	strcpy(table_queries.tablename, data_query.tablename);
	table_queries.latest_version = data_query.version;
	string queries = table_queries.GetQueriesString();
	char answer = 0;

	if(UpdatesAsk() || UpdatesVerbose())
		LogWrite(INIT__PATCHER_WARNING, 0, "Patcher", "Pending Queries:\n%s", queries.c_str());

	if(!UpdatesAuto() && UpdatesAsk())
	{
		LogWrite(INIT__PATCHER_WARNING, 0, "Patcher", "Update to the latest data version? ([Y]es, [N]o, [A]ll)");
		cin >> answer;
		if(toupper(answer) == 'A')
			UpdatesAuto(true);
	}
	if(UpdatesAuto() || (answer && toupper(answer) == 'Y'))
	{
		bool success = database.RunDatabaseQueries(&table_queries, false, true);
		string table = string(data_query.tablename);
		if(table != last_data_update_table)
		{
			if(success)
				LogWrite(INIT__PATCHER_DEBUG, 0, "Patcher", "\tData Updated: '%s' = SUCCESS!", data_query.tablename);
			else
				LogWrite(INIT__PATCHER_ERROR, 0, "Patcher", "Data Updated: '%s' = FAILED!", data_query.tablename);
				
			last_data_update_table = table;
		}
	}
}

void LoginServer::ProcessTableUpdate(uchar* data) {
	TableQuery table_query;
	table_query.DeSerialize(data);
	string queries = table_query.GetQueriesString();
	char answer = 0;
	if(UpdatesAsk() || UpdatesVerbose())
	{
		LogWrite(INIT__PATCHER_DEBUG, 0, "Patcher", "\tUpdate Found for '%s'!", table_query.tablename);
		LogWrite(INIT__PATCHER_DEBUG, 0, "Patcher", "\tNew Version: %i", table_query.latest_version);
		LogWrite(INIT__PATCHER_DEBUG, 0, "Patcher", "\tYour Version: %i", table_query.your_version);
		LogWrite(INIT__PATCHER_WARNING, 0, "Patcher", "Pending Query:\n%s", queries.c_str());
	}
	if(!UpdatesAuto() && UpdatesAsk())
	{
		LogWrite(INIT__PATCHER_WARNING, 0, "Patcher", "Update to the latest table version? ([Y]es, [N]o, [A]ll)");
		cin >> answer;
		if(toupper(answer) == 'A')
			UpdatesAuto(true);
	}
	if(UpdatesAuto() || (answer && toupper(answer) == 'Y'))
	{
		bool success = database.RunDatabaseQueries(&table_query, false);
		if(success)
			LogWrite(INIT__PATCHER_DEBUG, 0, "Patcher", "\tUpdate Table: '%s' = SUCCESS!", table_query.tablename);
		else
			LogWrite(INIT__PATCHER_ERROR, 0, "Patcher", "Update Table: '%s' = FAILED!", table_query.tablename);
	}
}

int32 LoginServer::ProcessTableUpdates(uchar* data) {
	int32 ret = 0;
	/*LatestTableVersions table_versions;
	table_versions.DeSerialize(data);
	int32 total_tables = table_versions.GetTotalTables();
	
	int32 max_version = CURRENT_DATABASE_MAJORVERSION*100 + CURRENT_DATABASE_MINORVERSION;
	LogWrite(INIT__PATCHER_DEBUG, 0, "Patcher", "--Max Version: %i", max_version);

	vector<ServerPacket*> packet_requests;
	ServerPacket* outpack = 0;
	for(int32 i=0;i<total_tables;i++){
		float version = database.CheckTableVersions(table_versions.GetTable(i).name);
		if( (version < table_versions.GetTable(i).version) )
		{
			outpack = new ServerPacket(ServerOP_GetTableQuery, sizeof(TableVersion));
			TableVersion* out_version = (TableVersion*)outpack->pBuffer;
			strcpy(out_version->name, table_versions.GetTable(i).name);
			out_version->max_table_version = max_version;
			out_version->version = (int32)version;
			out_version->last = 0;
			packet_requests.push_back(outpack);
			ret++;
		}
	}
	if( ret )
		LogWrite(INIT__PATCHER_DEBUG, 0, "Patcher", "--Found Table Updates!");
	else
		LogWrite(INIT__PATCHER_DEBUG, 0, "Patcher", "--No Table Updates, continuing...");

	int32 total_packets = packet_requests.size();
	for(int32 i=0;i<total_packets;i++)
	{
		outpack = packet_requests[i];
		if((i+1) == total_packets)
		{
			TableVersion* out_version = (TableVersion*)outpack->pBuffer;
			out_version->last = 1;
		}
		SendPacket(outpack);
		safe_delete(outpack);
	}*/
	return ret;
}

int32 LoginServer::ProcessDataUpdates(uchar* data) {
	int32 ret = 0;
	/*LatestTableVersions table_versions;
	table_versions.DeSerialize(data);
	int32 total_tables = table_versions.GetTotalTables();
	vector<ServerPacket*> packet_requests;
	ServerPacket* outpack = 0;
	for(int32 i=0;i<total_tables;i++){
		if(UpdatesAutoData()){
			float version = database.GetLatestDataTableVersion(table_versions.GetTable(i).name);
			if(version >= 0 && version < table_versions.GetTable(i).data_version )
			{
				ServerPacket* outpack = new ServerPacket(ServerOP_GetTableData, sizeof(TableVersion));
				TableVersion* out_version = (TableVersion*)outpack->pBuffer;
				strcpy(out_version->name, table_versions.GetTable(i).name);
				out_version->max_data_version = CURRENT_DATABASE_MAJORVERSION*100 + CURRENT_DATABASE_MINORVERSION;
				out_version->data_version = (sint32)version;
				out_version->last = 0;
				memset(out_version->column_names, 0, 1000);
				string columns = database.GetColumnNames(out_version->name);
				if(columns.length() > 1)
					strcpy(out_version->column_names, columns.c_str());
				packet_requests.push_back(outpack);
				ret++;
			}
		}
	}
	if( ret )
		LogWrite(INIT__PATCHER_DEBUG, 0, "Patcher", "--Found Data Updates!");

	int32 total_packets = packet_requests.size();
	for(int32 i=0;i<total_packets;i++)
	{
		outpack = packet_requests[i];
		if((i+1) == total_packets)
		{
			TableVersion* out_version = (TableVersion*)outpack->pBuffer;
			out_version->last = 1;
		}
		SendPacket(outpack);
		safe_delete(outpack);
	}*/
	return ret;
}

bool LoginServer::CheckAndWait(Timer* timer){
	Timer::SetCurrentTime();
	if(timer->Check(false)){
		if(timer->GetDuration() == 5000){
			LogWrite(INIT__PATCHER_WARNING, 0, "Patcher", "Please wait...");
			timer->SetTimer(25000);
			timer->Start();
		}
		else
			return false;
	}
	Sleep(10);
	if(!loginserver.Process())
		tcpc->Disconnect();
	return Connected();
}

bool LoginServer::ConnectToUpdateServer(const char* iAddress, int16 iPort)
{
	if(!try_auto_update)
		return true;

	char errbuf[TCPConnection_ErrorBufferSize];
	memset(errbuf, 0, TCPConnection_ErrorBufferSize);

	if (iAddress == 0) 
	{
		LogWrite(INIT__PATCHER_ERROR, 0, "Patcher", "PatchServer::ConnectToUpdateServer: address == 0");
		return false;
	}
	else 
	{
		if ((UpdateServerIP = ResolveIP(iAddress, errbuf)) == 0) 
		{
			LogWrite(INIT__PATCHER_ERROR, 0, "Patcher", "LoginServer::ConnectToUpdateServer: Resolving IP address: '%s'", errbuf[0] == '\0' ? errbuf : " ");
			return false;
		}
	}

	if (iPort != 0)
		UpdateServerPort = iPort;

	if (UpdateServerIP == 0 || UpdateServerPort == 0) 
	{
		LogWrite(INIT__PATCHER_ERROR, 0, "Patcher", "PatchServer::ConnectToUpdateServer: Connect info incomplete, cannot connect.");
		return false;
	}

	if (tcpc->Connect(UpdateServerIP, UpdateServerPort, errbuf)) 
	{
		LogWrite(INIT__PATCHER_DEBUG, 0, "Patcher", "-Connected to Update Server: %s:%i", iAddress, UpdateServerPort);
		SendInfo();

		//wait for a max of 30 seconds to be verified by patcher
		//display a message at 5 seconds
		Timer waitTimer(5000);
		waitTimer.Start();

		LogWrite(INIT__PATCHER_DEBUG, 0, "Patcher", "-Checking for updates.");
		GetLatestTables();

		while(CheckAndWait(&waitTimer) && !update_server_verified);
		database.CheckVersionTable();

		if(!update_server_verified)
			LogWrite(INIT__PATCHER_ERROR, 0, "Patcher", "Error while getting updates!");

		if(update_server_verified)
		{
			update_server_completed = false;
			waitTimer.SetTimer(5000);
			waitTimer.Start();

			while(CheckAndWait(&waitTimer) && !data_waiting);

			if(data_waiting)
			{
				int32 requests = ProcessTableUpdates(data_waiting);

				if(requests > 0)
					LogWrite(INIT__PATCHER_DEBUG, 0, "Patcher", "--Found Table Updates.");

				uchar* data = 0;
				waitTimer.SetTimer(30000);
				waitTimer.Start();

				if(requests > 0)
				{
					LogWrite(INIT__PATCHER_DEBUG, 0, "Patcher", "--Processing Table Updates.");
					database.DisableConstraints();
					deque<uchar*> alter_table_updates_waiting;

					while((CheckAndWait(&waitTimer) && !update_server_completed) || table_updates_waiting.size() > 0)
					{
						if(table_updates_waiting.size() > 0)
						{
							data = table_updates_waiting.front();
							table_updates_waiting.pop_front();
							ProcessTableUpdate(data);
							safe_delete(data);
							waitTimer.SetTimer(30000);
							waitTimer.Start();
						}
					}

					if(!CheckAndWait(&waitTimer))
						LogWrite(INIT__PATCHER_ERROR, 0, "Patcher", "Timeout waiting for updates to complete! Try again later.");
					else
						LogWrite(INIT__PATCHER_DEBUG, 0, "Patcher", "--Table Update complete.");

					database.EnableConstraints();
				}
				else
					LogWrite(INIT__PATCHER_DEBUG, 0, "Patcher", "-No changes to DB Schema...");

				requests = ProcessDataUpdates(data_waiting);

				if(requests > 0)
				{
					LogWrite(INIT__PATCHER_DEBUG, 0, "Patcher", "--Processing Data Updates.");
					LogWrite(INIT__PATCHER_WARNING, 0, "Patcher", "** Data Updates are SLOW! Do not abort! **");

					database.DisableConstraints();

					deque<uchar*> alter_table_updates_waiting;
					int32 count = 0;

					while((CheckAndWait(&waitTimer) && count < requests) || data_updates_waiting.size() > 0)
					{
						if(data_updates_waiting.size() > 0)
						{
							data = data_updates_waiting.front();
							data_updates_waiting.pop_front();
							ProcessDataUpdate(data);
							count++;
							safe_delete_array(data);
							waitTimer.SetTimer(30000);
							waitTimer.Start();
						}
					}

					if(!CheckAndWait(&waitTimer))
						LogWrite(INIT__PATCHER_ERROR, 0, "Patcher", "Timeout waiting for updates to complete!  Try again later.");
					else
						LogWrite(INIT__PATCHER_DEBUG, 0, "Patcher", "--Data Updates complete.");

					database.EnableConstraints();
				}
				else
					LogWrite(INIT__PATCHER_DEBUG, 0, "Patcher", "-No changes to DB Data...");

				safe_delete_array(data_waiting);
			}	
			try_auto_update = false;
		}

		tcpc->Disconnect();
		return update_server_verified;
	}
	else 
	{
		LogWrite(INIT__PATCHER_ERROR, 0, "Patcher", "PatchServer::ConnectToUpdateServer: '%s'", errbuf[0] == '\0' ? errbuf : " ");
		return false;
	}
}

bool LoginServer::Connect(const char* iAddress, int16 iPort) {
	if(!pTryReconnect)
		return false;

	char errbuf[TCPConnection_ErrorBufferSize];
	memset(errbuf, 0, TCPConnection_ErrorBufferSize);
	if (iAddress == 0) {
		LogWrite(WORLD__ERROR, 0, "World", "LoginServer::Connect: address == 0");
		return false;
	}
	else {
		if ((LoginServerIP = ResolveIP(iAddress, errbuf)) == 0) {
			LogWrite(WORLD__ERROR, 0, "World", "LoginServer::Connect: Resolving IP address: '%s'", errbuf);
			return false;
		}
	}
	if (iPort != 0)
		LoginServerPort = iPort;

	if (LoginServerIP == 0 || LoginServerPort == 0) {
		LogWrite(WORLD__ERROR, 0, "World", "LoginServer::Connect: Connect info incomplete, cannot connect");
		return false;
	}

	if (tcpc->Connect(LoginServerIP, LoginServerPort, errbuf)) {
		LogWrite(WORLD__INFO, 0, "World", "Connected to LoginServer: %s: %i", iAddress, LoginServerPort);
		SendInfo();
		SendStatus();
		return true;
	}
	else {
		LogWrite(WORLD__ERROR, 0, "World", "LoginServer::Connect: '%s'", errbuf);
		return false;
	}
}
void LoginServer::GetLatestTables(){
	ServerPacket* pack = new ServerPacket(ServerOP_GetLatestTables, sizeof(GetLatestTables_Struct));
	GetLatestTables_Struct* data = (GetLatestTables_Struct*)pack->pBuffer;
	data->table_version = CURRENT_DATABASE_MAJORVERSION*100 + CURRENT_DATABASE_MINORVERSION;
	data->data_version = CURRENT_DATABASE_MAJORVERSION*100 + CURRENT_DATABASE_MINORVERSION;
	SendPacket(pack);
	delete pack;
}
void LoginServer::SendInfo() {
	ServerPacket* pack = new ServerPacket;
	pack->opcode = ServerOP_LSInfo;
	pack->size = sizeof(ServerLSInfo_Struct);
	pack->pBuffer = new uchar[pack->size];
	memset(pack->pBuffer, 0, pack->size);
	ServerLSInfo_Struct* lsi = (ServerLSInfo_Struct*) pack->pBuffer;
	strcpy(lsi->protocolversion, EQEMU_PROTOCOL_VERSION);
	strcpy(lsi->serverversion, CURRENT_VERSION);
	strcpy(lsi->name, net.GetWorldName());
	strcpy(lsi->account, net.GetWorldAccount());
	lsi->dbversion = CURRENT_DATABASE_MAJORVERSION*100 + CURRENT_DATABASE_MINORVERSION;
#ifdef _DEBUG
	lsi->servertype = 4;
#endif
	int8 tmppass[16];
	MD5::Generate((uchar*)net.GetWorldPassword(), strlen(net.GetWorldPassword()), tmppass);
	char* ptr = lsi->password;
	for(int i=0;i<15;i++){
		sprintf(ptr, "%02x", tmppass[i]);
		ptr += 2;
	}
	//memcpy(lsi->password, tmppass, 16);
	strcpy(lsi->address, net.GetWorldAddress());
	SendPacket(pack);
	delete pack;
}

void LoginServer::SendStatus() {
	statusupdate_timer->Start();
	ServerPacket* pack = new ServerPacket;
	pack->opcode = ServerOP_LSStatus;
	pack->size = sizeof(ServerLSStatus_Struct);
	pack->pBuffer = new uchar[pack->size];
	memset(pack->pBuffer, 0, pack->size);
	ServerLSStatus_Struct* lss = (ServerLSStatus_Struct*) pack->pBuffer;

	if (net.world_locked)
		lss->status = -2;
	else if(loginserver.maxPlayers > -1 && numclients >= loginserver.maxPlayers)
		lss->status = -3;
	else
		lss->status = 1;

	lss->num_zones = numzones;
	lss->num_players = numclients;
	lss->world_max_level = rule_manager.GetGlobalRule(R_Player, MaxLevel)->GetInt8();
	SendPacket(pack);
	delete pack;
}

void LoginServer::SendDeleteCharacter ( CharacterTimeStamp_Struct* cts ) {
	ServerPacket* outpack = new ServerPacket(ServerOP_BasicCharUpdate, sizeof(CharDataUpdate_Struct));
	CharDataUpdate_Struct* cdu = (CharDataUpdate_Struct*)outpack->pBuffer;
	cdu->account_id = cts->account_id;
	cdu->char_id = cts->char_id;
	cdu->update_field = DELETE_UPDATE_FLAG;
	cdu->update_data = 1;
	SendPacket(outpack);
	safe_delete(outpack);
}

void LoginServer::SendFilterNameResponse ( int8 resp, int32 acct_id , int32 char_id ) {
	ServerPacket* outpack = new ServerPacket(ServerOP_CharacterCreate, sizeof(WorldCharNameFilterResponse_Struct));
	WorldCharNameFilterResponse_Struct* wcfr = (WorldCharNameFilterResponse_Struct*)outpack->pBuffer;
	wcfr->response = resp;
	wcfr->account_id = acct_id;
	wcfr->char_id = char_id;
	SendPacket(outpack);
	safe_delete(outpack);
}

int32 LoginServer::DetermineCharacterLoginRequest ( UsertoWorldRequest_Struct* utwr ) {
	LogWrite(LOGIN__TRACE, 9, "Login", "Enter: %s", __FUNCTION__);
	ServerPacket* outpack = new ServerPacket;
	outpack->opcode = ServerOP_UsertoWorldResp;
	outpack->size = sizeof(UsertoWorldResponse_Struct);
	outpack->pBuffer = new uchar[outpack->size];
	memset(outpack->pBuffer, 0, outpack->size);
	UsertoWorldResponse_Struct* utwrs = (UsertoWorldResponse_Struct*) outpack->pBuffer;
	utwrs->lsaccountid = utwr->lsaccountid;
	utwrs->char_id = utwr->char_id;
	utwrs->ToID = utwr->FromID;
	int32 timestamp = Timer::GetUnixTimeStamp();
	utwrs->access_key = timestamp;

	// set default response to 0
	utwrs->response = 0;

	sint16 lowestStatus = database.GetLowestCharacterAdminStatus( utwr->lsaccountid );

	sint16 status = 0;
	
	if(lowestStatus == -2)
		status = -1;
	else
		status = database.GetCharacterAdminStatus ( utwr->lsaccountid , utwr->char_id );

	if(status < 100 && zone_list.ClientConnected(utwr->lsaccountid))
		status = -9;
	if(status < 0){
		switch(status){
			case -10:
				utwrs->response = PLAY_ERROR_CHAR_NOT_LOADED;
				break;
			case -9:
				utwrs->response = 0;//PLAY_ERROR_ACCOUNT_IN_USE;
				break;
			case -8:
				utwrs->response = PLAY_ERROR_LOADING_ERROR;
				break;
			case -1:
				utwrs->response = PLAY_ERROR_ACCOUNT_BANNED;
				break;
			default:
				utwrs->response = PLAY_ERROR_PROBLEM;
		}
	}
	else if(net.world_locked == true){
		LogWrite(WORLD__INFO, 0, "World", "Login Lock Check (MinStatus: %i):, UserStatus: %i, CharID: %i",loginserver.minLockedStatus,status,utwr->char_id );

		// has high enough status, allow it
		if(status >= loginserver.minLockedStatus)
			utwrs->response = 1;
	}
	else if(loginserver.maxPlayers > -1 && ((sint16)client_list.Count()) >= loginserver.maxPlayers)
	{
		LogWrite(WORLD__INFO, 0, "World", "Login GameFull Check (MinStatus: %i):, UserStatus: %i, CharID: %i",loginserver.minGameFullStatus,status,utwr->char_id );

		// has high enough status, allow it
		if(status >= loginserver.minGameFullStatus)
		{
			utwrs->response = 1;
		}
		else
			utwrs->response = -3; // server full response is -3
	}
	else
		utwrs->response = 1;

	/*sint32 x = database.CommandRequirement("$MAXCLIENTS");
	if( (sint32)numplayers >= x && x != -1 && x != 255 && status < 80)
	utwrs->response = -3;

	if(status == -1)
	utwrs->response = -1;
	if(status == -2)
	utwrs->response = -2;
	*/
	//printf("Response is %i for %i\n",utwrs->response,id);
	if((strcmp(net.GetWorldAddress(), utwr->ip_address)==0) && (strlen(net.GetInternalWorldAddress())>0))
		strcpy(utwrs->ip_address, net.GetInternalWorldAddress());
	else
		strcpy(utwrs->ip_address, net.GetWorldAddress());
	utwrs->port = net.GetWorldPort();
	utwrs->worldid = utwr->worldid;
	SendPacket(outpack);
	delete outpack;

	LogWrite(LOGIN__TRACE, 9, "Login", "Exit: %s with timestamp=%u", __FUNCTION__, timestamp);
	// depending on the response determined above, this could return 0 (for failure)
	return timestamp;
}

