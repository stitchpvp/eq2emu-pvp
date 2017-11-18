#include "MasterServer.h"

MasterServer::MasterServer() {
	tcpc = new TCPConnection(false);
}

MasterServer::~MasterServer() {
	safe_delete(tcpc);
}

bool MasterServer::Connect() {
	char errbuf[TCPConnection_ErrorBufferSize];
	memset(errbuf, 0, TCPConnection_ErrorBufferSize);

	if (tcpc->Connect("192.168.0.3", 9000, errbuf)) {
		return true;
	} else {
		return false;
	}
}

void MasterServer::SayHello() {
	ServerPacket* pack = new ServerPacket(ServerOP_MSHello, sizeof(ServerMSInfo_Struct));
	ServerMSInfo_Struct* msi = (ServerMSInfo_Struct*)pack->pBuffer;
	strcpy(msi->hello, "hell");
	tcpc->SendPacket(pack);
	safe_delete(pack);
}