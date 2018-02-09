#include "MasterServer.h"
#include "net.h"
#include "WorldDatabase.h"

extern WorldDatabase database;
extern ZoneAuth zone_auth;

MasterServer::MasterServer() {
	tcpc = new TCPConnection(false);
}

MasterServer::~MasterServer() {
	safe_delete(tcpc);
}

bool MasterServer::Connect() {
	char errbuf[TCPConnection_ErrorBufferSize];
	memset(errbuf, 0, TCPConnection_ErrorBufferSize);

	if (tcpc->Connect("165.227.50.17", 9000, errbuf)) {
		return true;
	} else {
		return false;
	}
}

bool MasterServer::Process() {
	/************ Get all packets from packet manager out queue and process them ************/
	ServerPacket *pack = 0;
	while ((pack = tcpc->PopPacket())) {
		switch (pack->opcode) {
			case ServerOP_MSAuthPlayer:
			{
				ServerMSAuthPlayer_Struct* utwr = (ServerMSAuthPlayer_Struct*)pack->pBuffer;
				int32 access_key = 1337;
				char* character_name = database.GetCharacterName(utwr->char_id);
				if (character_name != 0) {
					ZoneAuthRequest* zar = new ZoneAuthRequest(utwr->account_id, character_name, access_key);
					zar->SetTimeStamp(Timer::GetUnixTimeStamp() + 10000000);
					zar->setFirstLogin(true);
					zone_auth.AddAuth(zar);
					safe_delete_array(character_name);
				}
				break;
			}

			default:
			{
				printf("wat");
			}
		}
	}

	return true;
}

void MasterServer::SayHello() {
	ServerPacket* pack = new ServerPacket(ServerOP_MSHello, sizeof(ServerMSInfo_Struct));
	ServerMSInfo_Struct* msi = (ServerMSInfo_Struct*)pack->pBuffer;
	strcpy(msi->hello, "hell");
	tcpc->SendPacket(pack);
	safe_delete(pack);
}