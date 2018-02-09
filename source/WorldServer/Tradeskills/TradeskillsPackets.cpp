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

#include <algorithm>
#include "../ClientPacketFunctions.h"
#include "../client.h"
#include "../../common/ConfigReader.h"
#include "../../common/PacketStruct.h"
#include "../Recipes/Recipe.h"
#include "../../common/Log.h"
#include "../Spells.h"
#include "../../common/MiscFunctions.h"

extern ConfigReader configReader;
extern MasterRecipeList master_recipe_list;
extern MasterSpellList master_spell_list;

void ClientPacketFunctions::SendCreateFromRecipe(Client* client, int32 recipeID) {

	// if recipeID is 0 we are repeating the last recipe, if not set the players current recipe to the new one
	if (recipeID == 0)
		recipeID = client->GetPlayer()->GetCurrentRecipe();
	else
		client->GetPlayer()->SetCurrentRecipe(recipeID);

	// Get the recipe
	Recipe* recipe = master_recipe_list.GetRecipe(recipeID);
	if (!recipe) {
		LogWrite(TRADESKILL__ERROR, 0, "Recipes", "Error loading recipe (%u) in ClientPacketFunctions::SendCreateFromRecipe()", recipeID);
		return;
	}

	// Create the packet
	PacketStruct* packet = configReader.getStruct("WS_CreateFromRecipe", client->GetVersion());
	if (!packet) {
		LogWrite(TRADESKILL__ERROR, 0, "Recipes", "Error loading WS_CreateFromRecipe in ClientPacketFunctions::SendCreateFromRecipe()");
		return;
	}
	vector<int32>::iterator itr;
	int8 i = 0;
	int32 firstID = 0;
	int32 primary_comp_id = 0;
	Item* item = 0;

	// Recipe and crafting table info
	packet->setDataByName("crafting_station", recipe->GetDevice());
	packet->setDataByName("recipe_name", recipe->GetName());
	packet->setDataByName("tier", recipe->GetTier());

	// Product info
	item = master_item_list.GetItem(recipe->GetProductID());
	packet->setDataByName("product_name", item->name.c_str());
	packet->setDataByName("icon", item->details.icon);
	packet->setDataByName("product_qty", recipe->GetProductQuantity());
	packet->setDataByName("primary_title", recipe->GetPrimaryBuildComponentTitle());
	packet->setDataByName("primary_qty", 1);

	// Reset item to 0
	item = 0;

	// Check to see if we have a primary component (slot = 0)
	if (recipe->components.count(0) > 0) {
		vector<int32> rc = recipe->components[0];
		packet->setArrayLengthByName("num_primary_choices", rc.size());
		for (itr = rc.begin(); itr != rc.end(); itr++, i++) {
			if (firstID == 0)
				firstID = *itr;

			item = master_item_list.GetItem(*itr);

			packet->setArrayDataByName("primary_component", item->name.c_str(), i);
			packet->setArrayDataByName("primary_item_id", (*itr), i);
			packet->setArrayDataByName("primary_icon", item->details.icon, i);
			item = 0;
			item = client->GetPlayer()->item_list.GetItemFromID((*itr));
			if (item)
				packet->setArrayDataByName("primary_total_quantity", item->details.count, i);
		}
		// store the id of the primary comp
		primary_comp_id = firstID;
		// Set the default item id to the first component id
		packet->setDataByName("primary_item_selected", 1);
		packet->setDataByName("primary_default_selected_id", firstID);
		item = 0;
		item = client->GetPlayer()->item_list.GetItemFromID(firstID);
		if (item)
			packet->setDataByName("primary_selected_item_qty", min((int8)1, (int8)item->details.count));

		// Reset the variables we will reuse
		i = 0;
		firstID = 0;
		item = 0;
	}
	else {
		LogWrite(TRADESKILL__ERROR, 0, "Recipes", "Recipe has no primary component");
		return;
	}

	// Check to see if we have build components (slot = 1 - 4)
	int8 total_build_components = 0;
	if (recipe->components.count(1) > 0)
		total_build_components++;
	if (recipe->components.count(2))
		total_build_components++;
	if (recipe->components.count(3))
		total_build_components++;
	if (recipe->components.count(4))
		total_build_components++;

	if (total_build_components > 0) {
		packet->setArrayLengthByName("num_build_components", total_build_components);
		for (int8 index = 0; index < 4; index++) {

			if (recipe->components.count(index + 1) == 0)
				continue;

			packet->setArrayDataByName("build_slot", index, index);

			vector<int32> rc = recipe->components[index + 1];

			packet->setSubArrayLengthByName("num_build_choices", rc.size(), index);
			for (itr = rc.begin(); itr != rc.end(); itr++, i++) {
				if (firstID == 0)
					firstID = *itr;

				item = master_item_list.GetItem(*itr);
			
				packet->setSubArrayDataByName("build_component", item->name.c_str(), index, i);
				packet->setSubArrayDataByName("build_item_id", (*itr), index, i);
				packet->setSubArrayDataByName("build_icon", item->details.icon, index, i);

				item = 0;
				item = client->GetPlayer()->item_list.GetItemFromID((*itr));
				if (item)
					packet->setSubArrayDataByName("build_total_quantity", item->details.count, index, i);
			}

			// Set the default item id to the first component id
			packet->setArrayDataByName("build_item_selected", 1, index);
			packet->setArrayDataByName("build_selected_item_id", firstID, index);
			item = 0;
			item = client->GetPlayer()->item_list.GetItemFromID(firstID);
			int8 qty = 0;
			if (item) {
				qty = (int8)item->details.count;
				if (qty > 0 && firstID == primary_comp_id)
					qty -= 1;
			}

			if (index == 0) {
				packet->setArrayDataByName("build_title", recipe->GetBuild1ComponentTitle(), index);
				packet->setArrayDataByName("build_qty", recipe->GetBuild1ComponentQuantity(), index);
				if (item)
					packet->setArrayDataByName("build_selected_item_qty", min(qty, recipe->GetBuild1ComponentQuantity()), index);
			}
			else if (index == 1) {
				packet->setArrayDataByName("build_title", recipe->GetBuild2ComponentTitle(), index);
				packet->setArrayDataByName("build_qty", recipe->GetBuild2ComponentQuantity(), index);
				if (item)
					packet->setArrayDataByName("build_selected_item_qty", min(qty, recipe->GetBuild2ComponentQuantity()), index);
			}
			else if (index == 2) {
				packet->setArrayDataByName("build_title", recipe->GetBuild3ComponentTitle(), index);
				packet->setArrayDataByName("build_qty", recipe->GetBuild3ComponentQuantity(), index);
				if (item)
					packet->setArrayDataByName("build_selected_item_qty", min(qty, recipe->GetBuild3ComponentQuantity()), index);
			}
			else {
				packet->setArrayDataByName("build_title", recipe->GetBuild4ComponentTitle(), index);
				packet->setArrayDataByName("build_qty", recipe->GetBuild4ComponentQuantity(), index);
				if (item)
					packet->setArrayDataByName("build_selected_item_qty", min(qty, recipe->GetBuild4ComponentQuantity()), index);
			}

			// Reset the variables we will reuse
			i = 0;
			firstID = 0;
			item = 0;
		}
	}

	// Check to see if we have a fuel component (slot = 5)
	if (recipe->components.count(5) > 0) {
		vector<int32> rc = recipe->components[5];
		packet->setArrayLengthByName("num_fuel_choices", rc.size());
		for (itr = rc.begin(); itr != rc.end(); itr++, i++) {
			if (firstID == 0)
				firstID = *itr;

			item = master_item_list.GetItem(*itr);

			packet->setArrayDataByName("fuel_component", item->name.c_str(), i);
			packet->setArrayDataByName("fuel_item_id", (*itr), i);
			packet->setArrayDataByName("fuel_icon", item->details.icon, i);
			item = 0;
			item = client->GetPlayer()->item_list.GetItemFromID((*itr));
			if (item)
				packet->setArrayDataByName("fuel_total_quantity", item->details.count, i);
		}

		// Set the default item id to the first component id
		packet->setDataByName("fuel_selected_item_id", firstID);
		packet->setDataByName("fuel_item_selected", 1);
		item = 0;
		item = client->GetPlayer()->item_list.GetItemFromID(firstID);
		if (item)
			packet->setDataByName("fuel_selected_item_qty", min(recipe->GetFuelComponentQuantity(), (int8)item->details.count));
		packet->setDataByName("fuel_title",recipe->GetFuelComponentTitle());
		packet->setDataByName("fuel_qty",recipe->GetFuelComponentQuantity());

		// Reset the variables we will reuse
		i = 0;
		firstID = 0;
		item = 0;
	}
	else {
		LogWrite(TRADESKILL__ERROR, 0, "Recipes", "Recipe has no fuel component");
		return;
	}

	packet->setDataByName("unknown1", recipeID);
	packet->setDataByName("unknown8", 1); // Possible array for amount you can craft
	packet->setDataByName("unknown8", 1, 1); // amounts you can craft
	//packet->PrintPacket();
	// Send the packet
	client->QueuePacket(packet->serialize());
	safe_delete(packet);
}

void ClientPacketFunctions::SendItemCreationUI(Client* client, Recipe* recipe) {
	// Check for valid recipe
	if (!recipe) {
		LogWrite(TRADESKILL__ERROR, 0, "Recipes", "Recipe = null in ClientPacketFunctions::SendItemCreationUI()");
		return;
	}

	// Load the packet
	PacketStruct* packet = configReader.getStruct("WS_ShowItemCreation", client->GetVersion());
	if (!packet) {
		LogWrite(TRADESKILL__ERROR, 0, "Recipes", "Error loading WS_ShowItemCreation in ClientPacketFunctions::SendItemCreationUI()");
		return;
	}

	int16 item_version = GetItemPacketType(packet->GetVersion());

	Item* item = 0;
	RecipeProducts* rp = 0;

	packet->setDataByName("unknown1", 1000);
	packet->setDataByName("max_possible_progress", 1000);

	// All the packets I have looked at these unknowns are always the same
	// so hardcoding them until they are identified.
	packet->setDataByName("unknown2", 1045220557, 0);
	packet->setDataByName("unknown2", 1061997773, 1);

	// Highest stage the player has been able to complete
	// TODO: store this for the player, for now use 0 (none known)
	packet->setDataByName("progress_levels_known", client->GetPlayer()->GetRecipeList()->GetRecipe(recipe->GetID())->GetHighestStage());

	packet->setArrayLengthByName("num_process", 4);
	for (int8 i = 0; i < 4; i++) {

		// Don't like this code but need to change the IfVariableNotSet value on unknown3 element
		// to point to the currect progress_needed
		vector<DataStruct*> dataStructs = packet->GetDataStructs();
		vector<DataStruct*>::iterator itr;
		for (itr = dataStructs.begin(); itr != dataStructs.end(); itr++) {
			DataStruct* data = *itr;
			char tmp[20] = {0};
			sprintf(tmp,"_%i",i);
			string name = "unknown3";
			name.append(tmp);
			if (strcmp(data->GetName(), name.c_str()) == 0) {
				name = "progress_needed";
				name.append(tmp);
				data->SetIfNotSetVariable(name.c_str());
			}
		}
		if (i == 1)
			packet->setArrayDataByName("progress_needed", 400, i);
		else if (i == 2)
			packet->setArrayDataByName("progress_needed", 600, i);
		else if (i == 3)
			packet->setArrayDataByName("progress_needed", 800, i);

		// get the product for this stage, if none found default to fuel
		if (recipe->products.count(i) > 0)
			rp = recipe->products[i];
		if (!rp) {
			rp = new RecipeProducts;
			rp->product_id = recipe->components[5].front();
			rp->product_qty = recipe->GetFuelComponentQuantity();
			rp->byproduct_id = 0;
			rp->byproduct_qty = 0;
			recipe->products[i] = rp;
		}
		item = master_item_list.GetItem(rp->product_id);
		if (!item) {
			LogWrite(TRADESKILL__ERROR, 0, "Recipe", "Error loading item (%u) in ClientPacketFunctions::SendItemCreationUI()", rp->product_id);
			return;
		}

		packet->setArrayDataByName("item_name", item->name.c_str(), i);
		packet->setArrayDataByName("item_icon", item->details.icon, i);
		if(client->GetVersion() < 860)
			packet->setItemArrayDataByName("item", item, client->GetPlayer(), i, 0, -1);
		else if (client->GetVersion() < 1193)
			packet->setItemArrayDataByName("item", item, client->GetPlayer(), i);
		else
			packet->setItemArrayDataByName("item", item, client->GetPlayer(), i, 0, 2);

		if (rp->byproduct_id > 0) {
			item = 0;
			item = master_item_list.GetItem(rp->byproduct_id);
			if (item) {
				packet->setArrayDataByName("item_byproduct_name", item->name.c_str(), i);
				packet->setArrayDataByName("item_byproduct_icon", item->details.icon, i);
			}
		}
		
		packet->setArrayDataByName("packettype", item_version, i);
		packet->setArrayDataByName("packetsubtype", 0xFF, i);

		item = 0;
		rp = 0;
	}
	packet->setDataByName("product_progress_needed", 1000);

	rp = recipe->products[4];
	item = master_item_list.GetItem(rp->product_id);

	packet->setDataByName("product_item_name", item->name.c_str());
	packet->setDataByName("product_item_icon", item->details.icon);

	if(client->GetVersion() < 860)
		packet->setItemByName("product_item", item, client->GetPlayer(), 0, -1);
	else if (client->GetVersion() < 1193)
		packet->setItemByName("product_item", item, client->GetPlayer());
	else
		packet->setItemByName("product_item", item, client->GetPlayer(), 0, 2);

	packet->setItemByName("product_item", item, client->GetPlayer());

	if (rp->byproduct_id > 0) {
		item = 0;
		item = master_item_list.GetItem(rp->byproduct_id);
		if (item) {
			packet->setDataByName("product_byproduct_name", item->name.c_str());
			packet->setDataByName("product_byproduct_icon", item->details.icon);
		}
	}

	packet->setDataByName("packettype", item_version);
	packet->setDataByName("packetsubtype", 0xFF);

	// Start of basic work to get the skills to show on the tradeskill window
	// not even close to accurate but skills do get put on the ui
	
	int8 size = 0;
	vector<int32>::iterator itr;
	vector<int32> spells = client->GetPlayer()->GetSpellBookSpellIDBySkill(recipe->GetTechnique());
	for (itr = spells.begin(); itr != spells.end(); itr++) {
			size++;

			if (size > 6) {
				// only 6 slots for skills on the ui
				break;
			}
			char str[20];
			char temp[20];
			strcpy(str, "skill");
			itoa(size, temp, 10);
			strcat(str, temp);
			strcat(str, "_id");
			packet->setDataByName(str, *itr);
	}


	EQ2Packet* outapp = packet->serialize();
	//DumpPacket(outapp);
	client->QueuePacket(outapp);
	safe_delete(packet);
}

void ClientPacketFunctions::StopCrafting(Client* client) {
	client->QueuePacket(new EQ2Packet(OP_StopItemCreationMsg, 0, 0));
}

void ClientPacketFunctions::CounterReaction(Client* client, bool countered) {
	PacketStruct* packet = configReader.getStruct("WS_TSEventReaction", client->GetVersion());
	if (packet) {
		packet->setDataByName("counter_reaction", countered ? 1 : 0);
		client->QueuePacket(packet->serialize());
	}
	safe_delete(packet);
}
