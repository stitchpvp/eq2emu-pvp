#include "MasterServer.h"
#include "net.h"
#include "WorldDatabase.h"
#include "World.h"

extern WorldDatabase database;
extern ZoneAuth zone_auth;
extern ZoneList zone_list;

MasterServer::MasterServer() {
  tcpc = new TCPConnection(false);
}

MasterServer::~MasterServer() {
  safe_delete(tcpc);
}

bool MasterServer::Connect() {
  char errbuf[TCPConnection_ErrorBufferSize];
  memset(errbuf, 0, TCPConnection_ErrorBufferSize);

  if (tcpc->Connect(getenv("MASTER_SERVER_HOST"), 9050, errbuf)) {
    return true;
  } else {
    return false;
  }
}

bool MasterServer::Process() {
  ServerPacket* pack = nullptr;

  while ((pack = tcpc->PopPacket())) {
    switch (pack->opcode) {
    case ServerOP_MSAuthPlayer: {
      auto utwr = reinterpret_cast<ServerMSAuthPlayer_Struct*>(pack->pBuffer);
      char* character_name = database.GetCharacterName(utwr->char_id);

      if (character_name) {
        auto zar = new ZoneAuthRequest(utwr->account_id, character_name, 1337);
        zar->SetTimeStamp(Timer::GetUnixTimeStamp() + 10000000);
        zar->setFirstLogin(true);
        zone_auth.AddAuth(zar);
        safe_delete_array(character_name);
      }
      break;
    }

    case ServerOP_MSKickPlayer: {
      auto data = reinterpret_cast<ServerMSKickPlayer_Struct*>(pack->pBuffer);

      shared_ptr<Client> client = zone_list.GetClientByCharID(data->char_id);

      if (client && client->getConnection()) {
        client->getConnection()->SendDisconnect();
      }

      break;
    }

    default: {
      printf("wat");
    }
    }
  }

  return true;
}

void MasterServer::SayHello(int32 zone_id) {
  auto pack = new ServerPacket(ServerOP_MSHello, sizeof(ServerMSInfo_Struct));
  auto msi = (ServerMSInfo_Struct*)pack->pBuffer;
  msi->zone_id = zone_id;
  strncpy(msi->uuid, getenv("ZONE_UUID"), 36);
  tcpc->SendPacket(pack);
  safe_delete(pack);
}

void MasterServer::PlayerOnline(int32 character_id) {
  auto pack = new ServerPacket(ServerOP_MSPlayerOnline, sizeof(ServerMSPlayer_Struct));
  auto msp = (ServerMSPlayer_Struct*)pack->pBuffer;
  msp->character_id = character_id;
  tcpc->SendPacket(pack);
  safe_delete(pack);
}

void MasterServer::PlayerOffline(int32 character_id) {
  auto pack = new ServerPacket(ServerOP_MSPlayerOffline, sizeof(ServerMSPlayer_Struct));
  auto msp = (ServerMSPlayer_Struct*)pack->pBuffer;
  msp->character_id = character_id;
  tcpc->SendPacket(pack);
  safe_delete(pack);
}
