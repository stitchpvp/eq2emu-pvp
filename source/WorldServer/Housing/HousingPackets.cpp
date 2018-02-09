#include "../ClientPacketFunctions.h"
#include "../World.h"
#include "../client.h"

extern ConfigReader configReader;
extern World world;

void ClientPacketFunctions::SendHousePurchace(Client* client, HouseZone* hz, int32 spawnID) {
	PacketStruct* packet = configReader.getStruct("WS_PlayerHousePurchase", client->GetVersion());
	if (packet) {
		packet->setDataByName("house_name", hz->name.c_str());
		packet->setDataByName("house_id", hz->id);
		packet->setDataByName("spawn_id", spawnID);
		packet->setDataByName("purchase_coins", hz->cost_coin);
		packet->setDataByName("purchace_status", hz->cost_status);
		packet->setDataByName("upkeep_coins", hz->upkeep_coin);
		packet->setDataByName("upkeep_status", hz->upkeep_status);
		packet->setDataByName("vendor_vault_slots", hz->vault_slots);
		string req;
		if (hz->alignment > 0) {
			req = "You must be of ";
			if (hz->alignment == 1)
				req.append("Good");
			else
				req.append("Evil");
			req.append(" alignment");
		}
		if (hz->guild_level > 0) {
			if (req.length() > 0) {
				req.append(", and a guild level of ");
				char temp[5];
				sprintf(temp, "%i", hz->guild_level);
				req.append(temp);
				//req.append(std::to_string(static_cast<long long>(hz->guild_level)));
			}
			else {
				req.append("Requires a guild of level ");
				char temp[5];
				sprintf(temp, "%i", hz->guild_level);
				req.append(temp);
				//req.append(std::to_string(static_cast<long long>(hz->guild_level)))
				req.append(" or above");
			}
		}
		if (req.length() > 0) {
			req.append(" in order to purchase a home within the ");
			req.append(hz->name);
			req.append(".");
		}

		packet->setDataByName("additional_reqs", req.c_str());

		bool enable_buy = true;
		if (hz->alignment > 0 && client->GetPlayer()->GetAlignment() != hz->alignment)
			enable_buy = false;
		if (hz->guild_level > 0 && (!client->GetPlayer()->GetGuild() || (client->GetPlayer()->GetGuild() && client->GetPlayer()->GetGuild()->GetLevel() < hz->guild_level)))
			enable_buy = false;

		packet->setDataByName("enable_buy", enable_buy ? 1 : 0);

		client->QueuePacket(packet->serialize());
	}

	safe_delete(packet);
}

void ClientPacketFunctions::SendBaseHouseWindow(Client* client, HouseZone* hz, PlayerHouse* ph, int32 spawnID) {
	PacketStruct* packet = configReader.getStruct("WS_PlayerHouseBaseScreen", client->GetVersion());
	if (packet) {
		packet->setDataByName("house_id", ph->unique_id);
		packet->setDataByName("spawn_id", spawnID);
		string name;
		name = ph->player_name;
		name.append("'s ");
		name.append(hz->name);
		packet->setDataByName("house_name", name.c_str());
		packet->setDataByName("zone_name", hz->name.c_str());
		packet->setDataByName("upkeep_cost_coins", hz->upkeep_coin);
		packet->setDataByName("upkeep_cost_status", hz->upkeep_status);
		int32 upkeep_due = ph->upkeep_due - Timer::GetUnixTimeStamp();
		packet->setDataByName("upkeep_due", upkeep_due);

		packet->setDataByName("escrow_balance_coins", ph->escrow_coins);
		packet->setDataByName("escrow_balance_status", ph->escrow_status);

		// temp - set priv level to owner for now
		packet->setDataByName("privlage_level", 4);
		// temp - set house type to personal house for now
		packet->setDataByName("house_type", 0);

		if (client->GetCurrentZone()->GetInstanceType() == PERSONAL_HOUSE_INSTANCE
			|| client->GetCurrentZone()->GetInstanceType() == GUILD_HOUSE_INSTANCE) {
				// Inside a house need to set a flag and set the history for the tabs

			packet->setDataByName("inside_house", 1);
			packet->setDataByName("num_access", 0);
			packet->setDataByName("public_access_level", 1);
			packet->setDataByName("num_history", 0);
		}

		client->QueuePacket(packet->serialize());
	}

	safe_delete(packet);
}

void ClientPacketFunctions::SendHouseVisitWindow(Client* client, vector<PlayerHouse*> houses) {
	PacketStruct* packet = configReader.getStruct("WS_DisplayVisitScreen", client->GetVersion());
	if (packet) {
		vector<PlayerHouse*>::iterator itr;
		packet->setArrayLengthByName("num_houses", houses.size());
		int16 i = 0;
		for (itr = houses.begin(); itr != houses.end(); itr++) {
			PlayerHouse* ph = *itr;
			if (ph) {
				HouseZone* hz = world.GetHouseZone(ph->house_id);
				if (hz) {
					packet->setArrayDataByName("house_id", ph->unique_id, i);
					packet->setArrayDataByName("house_owner", ph->player_name.c_str(), i);
					packet->setArrayDataByName("house_location", hz->name.c_str(), i);
					packet->setArrayDataByName("house_zone", client->GetCurrentZone()->GetZoneName(), i);
					packet->setArrayDataByName("access_level", 1, i);
					packet->setArrayDataByName("visit_flag", 0, i); // 0 = allowed to visit, 1 = owner hasn't paid upkeep
					i++;
				}
			}
		}
		client->QueuePacket(packet->serialize());
	}
	safe_delete(packet);
}

/*
<Struct Name="WS_DisplayVisitScreen" ClientVersion="1193" OpcodeName="OP_DisplayInnVisitScreenMsg">
<Data ElementName="num_houses" Type="int16" Size="1" />
<Data ElementName="visithouse_array" Type="Array" ArraySizeVariable="num_houses">
  <Data ElementName="house_id" Type="int64" />
  <Data ElementName="house_owner" Type="EQ2_16Bit_String" />
  <Data ElementName="house_location" Type="EQ2_16Bit_string" />
  <Data ElementName="house_zone" Type="EQ2_16Bit_String" />
  <Data ElementName="access_level" Type="int8" Size="1" />
  <Data ElementName="unknown3" Type="int8" Size="3" />
  <Data ElementName="visit_flag" Type="int8" Size="1" />
</Data>
<Data ElementName="unknown4" Type="int32" Size="1" />
<Data ElementName="unknown5" Type="int8" Size="1" />
</Struct>
*/