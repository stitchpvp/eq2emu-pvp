#include "ClientPacketFunctions.h"
#include "MasterServer.h"
#include "net.h"
#include "WorldDatabase.h"
#include "World.h"

extern ConfigReader configReader;
extern WorldDatabase database;
extern ZoneAuth zone_auth;
extern ZoneList zone_list;
extern ZoneServer* zone;

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
    case ServerOP_AuthPlayer: {
      auto utwr = reinterpret_cast<ServerMSAuthPlayer_Struct*>(pack->pBuffer);
      char* character_name = database.GetCharacterName(utwr->char_id);

      if (character_name) {
        auto zar = new ZoneAuthRequest(utwr->account_id, character_name, utwr->access_key);
        LogWrite(WORLD__INFO, 0, "World", "AddAuth: %u Key: %u", zar->GetAccountID(), zar->GetAccessKey());
        zar->setFirstLogin(utwr->first_login);
        zone_auth.AddAuth(zar);
        safe_delete_array(character_name);
      }
      break;
    }

    case ServerOP_KickPlayer: {
      auto data = reinterpret_cast<ServerMSKickPlayer_Struct*>(pack->pBuffer);

      shared_ptr<Client> client = zone_list.GetClientByCharID(data->char_id);

      if (client && client->getConnection()) {
        client->getConnection()->SendDisconnect();
      }

      break;
    }

    case ServerOP_ZonePlayer: {
      auto data = reinterpret_cast<ServerMSZonePlayer_Struct*>(pack->pBuffer);

      shared_ptr<Client> client = zone_list.GetClientByCharID(data->character_id);

      if (client) {
        ClientPacketFunctions::SendZoneChange(client, data->addr, data->port, data->access_key);
      }

      break;
    }

    case ServerOP_Message: {
      auto data = reinterpret_cast<ServerMessage_Struct*>(pack->pBuffer);

      Spawn* spawn = nullptr;
      shared_ptr<Client> client = nullptr;
      char* message = new char[data->size];
      string msg_str(data->message);
      msg_str.resize(data->size);

      if (data->spawn_id > 0) {
        spawn = zone->GetSpawnByID(data->spawn_id);
      }
      
      if (data->character_id > 0) {
        client = zone_list.GetClientByCharID(data->character_id);
      }

      if (client) {
        client->SimpleMessage(data->channel, msg_str.c_str());
      } else {
        zone->SimpleMessage(data->channel, msg_str.c_str(), spawn, data->distance);
      }

      break;
    }

    case ServerOP_Tell: {
      auto data = reinterpret_cast<ServerTell_Struct*>(pack->pBuffer);

      shared_ptr<Client> client = zone_list.GetClientByCharID(data->character_id);

      if (client) {
        PacketStruct* packet = configReader.getStruct("WS_HearChat", client->GetVersion());

        if (packet) {
          string msg_str(data->message);
          msg_str.resize(data->size);

          packet->setMediumStringByName("from", data->from);
          packet->setMediumStringByName("to", data->to);
          packet->setDataByName("channel", CHANNEL_TELL);
          packet->setDataByName("from_spawn_id", 0xFFFFFFFF);
          packet->setDataByName("to_spawn_id", 0xFFFFFFFF);
          packet->setDataByName("understood", 1);
          packet->setMediumStringByName("message", msg_str.c_str());
          EQ2Packet* outpacket = packet->serialize();
          client->QueuePacket(outpacket);
          safe_delete(packet);
        }
      }

      break;
    }

    case ServerOP_WhoReply: {
      auto data = reinterpret_cast<ServerWhoReply_Struct*>(pack->pBuffer);

      shared_ptr<Client> client = zone_list.GetClientByCharID(data->character_id);

      if (client) {
        PacketStruct* packet = configReader.getStruct("WS_WhoQueryReply", client->GetVersion());

        if (packet) {
          packet->setDataByName("account_id", client->GetAccountID());
          packet->setDataByName("unknown", 0xFFFFFFFF);
          packet->setDataByName("response", data->response);
          packet->setArrayLengthByName("num_characters", data->num_characters);
          packet->setDataByName("unknown10", data->zone_only);

          for (int i = 0; i < data->num_characters; ++i) {
            auto character = reinterpret_cast<ServerWhoPlayer_Struct*>(data->characters + (i * sizeof(ServerWhoPlayer_Struct)));
            packet->setArrayDataByName("char_name", character->name, i);
            packet->setArrayDataByName("level", character->level, i);
            packet->setArrayDataByName("admin_level", character->admin_status, i);
            packet->setArrayDataByName("class", character->class_id, i);
            packet->setArrayDataByName("unknown4", 0xFF, i); //probably tradeskill class
            packet->setArrayDataByName("flags", 0, i); //probably tradeskill class
            packet->setArrayDataByName("race", character->race, i);
            packet->setArrayDataByName("zone", character->zone, i);
            packet->setArrayDataByName("guild", character->guild, i);
          }

          EQ2Packet* outpacket = packet->serialize();
          client->QueuePacket(outpacket);
          safe_delete(packet);
        }
      }

      break;
    }

    case ServerOP_GroupInvite: {
      auto data = reinterpret_cast<ServerGroupInviteRequest_Struct*>(pack->pBuffer);

      shared_ptr<Client> client = zone_list.GetClientByCharID(data->invitee_id);

      if (client) {
        PacketStruct* packet = configReader.getStruct("WS_ReceiveOffer", client->GetVersion());

        if (packet) {
          packet->setDataByName("type", 1);
          packet->setDataByName("name", data->inviter);
          packet->setDataByName("unknown2", 1);
          client->QueuePacket(packet->serialize());
          safe_delete(packet);
        }
      }

      break;
    }

    default: {
      printf("wat");
    }
    }

    safe_delete(pack);
  }

  return true;
}

void MasterServer::SayHello(int32 zone_id) {
  auto pack = new ServerPacket(ServerOP_HelloWorld, sizeof(ServerMSInfo_Struct));
  auto msi = (ServerMSInfo_Struct*)pack->pBuffer;
  msi->zone_id = zone_id;
  strncpy(msi->uuid, getenv("ZONE_UUID"), 36);
  tcpc->SendPacket(pack);
  safe_delete(pack);
}

void MasterServer::PlayerOnline(int32 character_id, int32 spawn_id) {
  auto pack = new ServerPacket(ServerOP_PlayerOnline, sizeof(ServerMSPlayer_Struct));
  auto msp = (ServerMSPlayer_Struct*)pack->pBuffer;
  msp->character_id = character_id;
  msp->spawn_id = spawn_id;
  tcpc->SendPacket(pack);
  safe_delete(pack);
}

void MasterServer::PlayerOffline(int32 character_id) {
  auto pack = new ServerPacket(ServerOP_PlayerOffline, sizeof(ServerMSPlayer_Struct));
  auto msp = (ServerMSPlayer_Struct*)pack->pBuffer;
  msp->character_id = character_id;
  tcpc->SendPacket(pack);
  safe_delete(pack);
}

void MasterServer::ZoneRequest(shared_ptr<Client> client, int32 zone_id) {
  auto pack = new ServerPacket(ServerOP_ZoneRequest, sizeof(ServerMSZoneRequest_Struct));
  auto data = (ServerMSZoneRequest_Struct*)pack->pBuffer;
  data->account_id = client->GetAccountID();
  data->character_id = client->GetCharacterID();
  data->zone_id = zone_id;
  tcpc->SendPacket(pack);
  safe_delete(pack);
}

void MasterServer::Tell(const char* from, const char* to, const char* message) {
  auto pack = new ServerPacket(ServerOP_Tell, sizeof(ServerTell_Struct));
  auto data = (ServerTell_Struct*)pack->pBuffer;
  strncpy(data->from, from, 32);
  strncpy(data->to, to, 32);
  data->size = strlen(message);
  strncpy(data->message, message, 256);
  tcpc->SendPacket(pack);
  safe_delete(pack);
}

void MasterServer::WhoQuery(int32 character_id, const char* query) {
  auto pack = new ServerPacket(ServerOP_WhoQuery, sizeof(ServerWhoQuery_Struct));
  auto data = (ServerWhoQuery_Struct*)pack->pBuffer;
  data->character_id = character_id;
  strncpy(data->query, query, 256);
  tcpc->SendPacket(pack);
  safe_delete(pack);
}

void MasterServer::InviteToGroup(int32 from_id, const char* to) {
  auto pack = new ServerPacket(ServerOP_GroupInvite, sizeof(ServerGroupInviteByName_Struct));
  auto data = (ServerGroupInviteByName_Struct*)pack->pBuffer;
  data->inviter_id = from_id;
  strncpy(data->name, to, 40);
  tcpc->SendPacket(pack);
  safe_delete(pack);
}

void MasterServer::JoinGroup(int32 character_id) {
  auto pack = new ServerPacket(ServerOP_GroupJoin, sizeof(ServerGroupJoin_Struct));
  auto data = (ServerGroupJoin_Struct*)pack->pBuffer;
  data->character_id = character_id;
  tcpc->SendPacket(pack);
  safe_delete(pack);
}