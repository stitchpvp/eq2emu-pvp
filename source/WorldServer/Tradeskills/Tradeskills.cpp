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
#include "Tradeskills.h"
#include "../client.h"
#include "../../common/ConfigReader.h"
#include "../classes.h"
//#include "../../common/debug.h"
#include "../../common/Log.h"
//#include "../zoneserver.h"
//#include "../Skills.h"
//#include "../classes.h"
#include "../World.h"
//#include "../LuaInterface.h"
#include "../ClientPacketFunctions.h"
#include "../WorldDatabase.h"
#include "../Rules/Rules.h"

extern Classes classes;
extern ConfigReader configReader;
extern MasterSkillList master_skill_list;
extern MasterRecipeList master_recipe_list;
extern MasterTradeskillEventsList master_tradeskillevent_list;
extern WorldDatabase database;
extern RuleManager rule_manager;

TradeskillMgr::TradeskillMgr() {
	m_tradeskills.SetName("TradeskillMgr::tradeskillsList");
	// % chance for each was made up by me (Jabantiz) and may need some tweaking
	// 2% for crit fail
	m_success		= rule_manager.GetGlobalRule(R_World, TradeskillSuccessChance)->GetFloat();
	m_critSuccess	= rule_manager.GetGlobalRule(R_World, TradeskillCritSuccessChance)->GetFloat();
	m_fail			= rule_manager.GetGlobalRule(R_World, TradeskillFailChance)->GetFloat();
	m_critFail		= rule_manager.GetGlobalRule(R_World, TradeskillCritFailChance)->GetFloat();
	m_eventChance	= rule_manager.GetGlobalRule(R_World, TradeskillEventChance)->GetFloat();

	if ((m_success + m_critSuccess + m_fail + m_critFail) != 100.0f) {
		LogWrite(TRADESKILL__ERROR, 0, "Tradeskills", "Success, crit success, fail, and crit fail MUST add up to 100, reverting to defaults...");
		m_success = 87.0f;
		m_critSuccess = 2.0f;
		m_fail = 10.0f;
		m_critFail = 1.0f;
	}
}

TradeskillMgr::~TradeskillMgr() {
	m_tradeskills.writelock(__FUNCTION__, __LINE__);

	map<Client*, Tradeskill*>::iterator itr;
	for (itr = tradeskillList.begin(); itr != tradeskillList.end(); itr++)
		safe_delete(itr->second);

	tradeskillList.clear();

	m_tradeskills.releasewritelock(__FUNCTION__, __LINE__);
}

void TradeskillMgr::Process() {
	m_tradeskills.writelock(__FUNCTION__, __LINE__);
	map<Client*, Tradeskill*>::iterator itr = tradeskillList.begin();
	while (itr != tradeskillList.end()) {
		Tradeskill* tradeskill = 0;
		tradeskill = itr->second;
		if (!tradeskill)
			continue;
		if (Timer::GetCurrentTime2() >= tradeskill->nextUpdateTime) {
			Client* client = itr->first;
			sint32 progress = 0;
			sint32 durability = 0;
			/*
			Following was grabbed from
			http://eq2.stratics.com/content/guides/padasher_crafting_2.php
			old but the base fail/succes should still be the same

			-100 Durability / -50 Progress (Critical Failure) 
			-50 Durability / 0 Progress (Failure) 
			-10 Durability / +50 Progress (Standard tick) 
			+10 Durability / + 100 Progress (Critical Success) 
			*/
			float roll = MakeRandomFloat(0, 100);
			int8 effect = 0; //1 is critical success, 2 is success, 3 is failure, and 4 is critical failure.

			float success = m_success;
			float crit_success = m_critSuccess;
			float fail = m_fail;
			float crit_fail = m_critFail;

			// Modify the % chance for success based off of stats
			fail -= client->GetPlayer()->stats[ITEM_STAT_SUCCESS_MOD];
			success += client->GetPlayer()->stats[ITEM_STAT_SUCCESS_MOD];

			// add values together for the if
			crit_success += crit_fail;
			fail += crit_success;
			success += fail;

			// Crit fail
			if (roll <= crit_fail) {
				progress = -50;
				durability = -100;
				effect = 4;
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Critical failure!");
			}
			// Crit success
			else if (roll > crit_fail && roll <= crit_success) {
				progress = 100;
				durability = 10;
				effect = 1;
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Critical success!");
			}
			// Fail
			else if (roll > crit_success && roll <= fail) {
				progress = 0;
				durability = -50;
				effect = 3;
			}
			// Success
			else if (roll > fail && roll <= success) {
				progress = 50;
				durability = -10;
				effect = 2;
			}
			else {
				// Just a debug, should never end up in this, if we do write out a log but treat as a success for the player
				LogWrite(TRADESKILL__ERROR, 0, "Tradeskills", "Process roll was not within valid range. roll = %f, crit fail = %f, crit success = %f, fail = %f, success = %f", roll, crit_fail, crit_success, fail, success);
				progress = 50;
				durability = -10;
				effect = 2;
			}

			// Check to see if there was an event, if there was give out the rewards/penalties for it
			if (tradeskill->CurrentEvent) {
				if (tradeskill->eventCountered) {
					progress += tradeskill->CurrentEvent->SuccessProgress;
					durability += tradeskill->CurrentEvent->SuccessDurability;
				}
				else {
					progress += tradeskill->CurrentEvent->FailProgress;
					durability += tradeskill->CurrentEvent->FailDurability;
				}
			}

			// Modify the progress/durability by the players stats
			progress += client->GetPlayer()->stats[ITEM_STAT_PROGRESS_ADD];
			durability += client->GetPlayer()->stats[ITEM_STAT_DURABILITY_ADD];

			tradeskill->currentDurability += durability;
			tradeskill->currentProgress += progress;

			PacketStruct* packet = configReader.getStruct("WS_UpdateCreateItem", client->GetVersion());
			if (packet) {
				packet->setDataByName("spawn_id", client->GetPlayer()->player_spawn_reverse_id_map[tradeskill->table]);
				packet->setDataByName("effect", effect);
				packet->setDataByName("total_durability", tradeskill->currentDurability);
				packet->setDataByName("total_progress", tradeskill->currentProgress);
				packet->setDataByName("durability_change", durability);
				packet->setDataByName("progress_change", progress);

				if (tradeskill->currentProgress >= 1000)
					packet->setDataByName("progress_level", 4);
				else if (tradeskill->currentProgress >= 800)
					packet->setDataByName("progress_level", 3);
				else if (tradeskill->currentProgress >= 600)
					packet->setDataByName("progress_level", 2);
				else if (tradeskill->currentProgress >= 400)
					packet->setDataByName("progress_level", 1);
				else
					packet->setDataByName("progress_level", 0);

				// Reset the tradeskill event
				tradeskill->CurrentEvent = 0;
				tradeskill->eventChecked = false;
				tradeskill->eventCountered = false;

				// 15% chance for an event (change this to a rule probably)

				int eventRoll = MakeRandomFloat(0, 100);
				if (eventRoll <= m_eventChance) {
					// Get a vector of all possible events for this crafting technique
					vector<TradeskillEvent*>* events = master_tradeskillevent_list.GetEventByTechnique(tradeskill->recipe->GetTechnique());
					if (events) {
						// Get the size of the vector
						int size = events->size();
						// Get a random number from 0 to size - 1 to use as an index
						int index = MakeRandomInt(0, size - 1);
						// use the index to get an event
						TradeskillEvent* TSEvent = events->at(index);
						if (TSEvent) {
							// Now that we got a random event set it in the packet
							packet->setDataByName("reaction_icon", TSEvent->Icon);
							packet->setDataByName("reaction_name", TSEvent->Name);

							// Set the current tradeskill event
							tradeskill->CurrentEvent = TSEvent;
						}
					}
				}

				client->QueuePacket(packet->serialize());
				safe_delete(packet);
			}

			if (tradeskill->currentProgress >= 1000) {
				itr++;
				StopCrafting(client, false);
				continue;
			}
			else
				tradeskill->nextUpdateTime = Timer::GetCurrentTime2() + 4000;
		}
		itr++;
	}
	m_tradeskills.releasewritelock(__FUNCTION__, __LINE__);
}

void TradeskillMgr::BeginCrafting(Client* client, vector<int32> components) {
	Recipe* recipe = master_recipe_list.GetRecipe(client->GetPlayer()->GetCurrentRecipe());

	if (!recipe) {
		LogWrite(TRADESKILL__ERROR, 0, "Recipe", "Recipe (%u) not found in TradeskillMgr::BeginCrafting()", client->GetPlayer()->GetCurrentRecipe());
		ClientPacketFunctions::StopCrafting(client);
		return;
	}

	ClientPacketFunctions::SendItemCreationUI(client, recipe);
	Tradeskill* tradeskill = new Tradeskill;
	tradeskill->player = client->GetPlayer();
	tradeskill->table = client->GetPlayer()->GetTarget();
	tradeskill->recipe = recipe;
	tradeskill->currentDurability = 1000;
	tradeskill->currentProgress = 0;
	tradeskill->nextUpdateTime = Timer::GetCurrentTime2() + 500;
	tradeskill->usedComponents = components;
	tradeskill->CurrentEvent = 0;
	tradeskill->eventChecked = false;
	tradeskill->eventCountered = false;
	m_tradeskills.writelock(__FUNCTION__, __LINE__);
	tradeskillList.insert(make_pair(client, tradeskill));
	m_tradeskills.releasewritelock(__FUNCTION__, __LINE__);

	// Unlock TS Spells and lock all others
	client->GetPlayer()->UnlockTSSpells();

	// TODO: use the vecotr to lock inventory slots
	/*vector<Item*>::iterator itr;
	for (itr = components.begin(); itr != components.end(); itr++) {
		Item* item = *itr;
		//client->GetPlayer()->SendInventoryUpdate
		item->details.inv_slot_id;
	}*/
}

void TradeskillMgr::StopCrafting(Client* client, bool lock) {
	
	if (lock)
		m_tradeskills.writelock(__FUNCTION__, __LINE__);

	if (tradeskillList.count(client) == 0) {
		if (lock)
			m_tradeskills.releasewritelock(__FUNCTION__, __LINE__);
		return;
	}

	Tradeskill* tradeskill = 0;
	tradeskill = tradeskillList[client];

	//TODO: unlock inventory slots, give the product to the player, give tradeskill xp
	ClientPacketFunctions::StopCrafting(client);

	int32 dur = tradeskill->currentDurability;
	int32 progress = tradeskill->currentProgress;
	Recipe* recipe = tradeskill->recipe;
	vector<int32>::iterator itr;
	Item* item = 0;
	int32 item_id = 0;
	int8 i = 0;
	int8 qty = 0;

	// cycle through the list of used items and remove them
	for (itr = tradeskill->usedComponents.begin(); itr != tradeskill->usedComponents.end(); itr++, i++) {
		// get the quantity to remove, first item in the vectore is always the primary, last is always the fuel
		if (i == 0)
			qty = 1;
		else if (i == 1 && i != tradeskill->usedComponents.size() - 1)
			qty = recipe->GetBuild1ComponentQuantity();
		else if (i == 2 && i != tradeskill->usedComponents.size() - 1)
			qty = recipe->GetBuild2ComponentQuantity();
		else if (i == 3 && i != tradeskill->usedComponents.size() - 1)
			qty = recipe->GetBuild3ComponentQuantity();
		else if (i == 4 && i != tradeskill->usedComponents.size() - 1)
			qty = recipe->GetBuild4ComponentQuantity();
		else if (i == 5 || i == tradeskill->usedComponents.size() - 1)
			qty = recipe->GetFuelComponentQuantity();

		// Get the item in the players inventory and remove or reduce the quantity
		item = client->GetPlayer()->item_list.GetItemFromID(*itr);
		if (item->details.count <= qty)
			client->GetPlayer()->item_list.RemoveItem(item);
		else {
			item->details.count -= qty;
			item->save_needed = true;
		}
	}

	item = 0;
	qty = recipe->GetFuelComponentQuantity();	
	item_id = recipe->components[5][0];
	if (progress >= 400 && progress < 600) {
		if (client->GetPlayer()->GetRecipeList()->GetRecipe(recipe->GetID())->GetHighestStage() < 1) {
			client->GetPlayer()->GetRecipeList()->GetRecipe(recipe->GetID())->SetHighestStage(1);
			database.UpdatePlayerRecipe(client->GetPlayer(), recipe->GetID(), 1);
		}
		if (recipe->products.count(1) > 0) {
			item_id = recipe->products[1]->product_id;
			qty = recipe->products[1]->product_qty;
		}
	}
	else if ((dur < 200 && progress >= 600) || (dur >= 200 && progress >= 600 && progress < 800)) {
		if (client->GetPlayer()->GetRecipeList()->GetRecipe(recipe->GetID())->GetHighestStage() < 2) {
			client->GetPlayer()->GetRecipeList()->GetRecipe(recipe->GetID())->SetHighestStage(2);
			database.UpdatePlayerRecipe(client->GetPlayer(), recipe->GetID(), 2);
		}
		if (recipe->products.count(2) > 0) {
			item_id = recipe->products[2]->product_id;
			qty = recipe->products[2]->product_qty;
		}
	}
	else if ((dur >= 200 && dur < 800 && progress >= 800) || (dur >= 800 && progress >= 800 && progress < 1000)) {
		if (client->GetPlayer()->GetRecipeList()->GetRecipe(recipe->GetID())->GetHighestStage() < 3) {
			client->GetPlayer()->GetRecipeList()->GetRecipe(recipe->GetID())->SetHighestStage(3);
			database.UpdatePlayerRecipe(client->GetPlayer(), recipe->GetID(), 3);
		}
		if (recipe->products.count(3) > 0) {
			item_id = recipe->products[3]->product_id;
			qty = recipe->products[3]->product_qty;
		}
	}
	else if (dur >= 800 && progress >= 1000) {
		if (client->GetPlayer()->GetRecipeList()->GetRecipe(recipe->GetID())->GetHighestStage() < 4) {
			client->GetPlayer()->GetRecipeList()->GetRecipe(recipe->GetID())->SetHighestStage(4);
			database.UpdatePlayerRecipe(client->GetPlayer(), recipe->GetID(), 4);
		}
		if (recipe->products.count(4) > 0) {
			item_id = recipe->products[4]->product_id;
			qty = recipe->products[4]->product_qty;
		}
	}
	item = new Item(master_item_list.GetItem(item_id));
	if (!item) {
		LogWrite(TRADESKILL__ERROR, 0, "Tradeskills", "Item (%u) not found.", item_id);
	}
	else {
		item->details.count = qty;
		// use CHANNEL_COLOR_CHAT_RELATIONSHIP as that is the same value (4) as it is in a log for this message
		client->Message(CHANNEL_COLOR_CHAT_RELATIONSHIP, "You created \\aITEM %u 0:%s\\/a.", item->details.item_id, item->name.c_str());
		client->AddItem(item);
		//Check for crafting quest updates
		int8 update_amt = 0;
		if(item->stack_count > 1)
			update_amt = 1;
		else
			update_amt = qty;
		client->GetPlayer()->CheckQuestsCraftUpdate(item, update_amt);
	}

	float xp = client->GetPlayer()->CalculateTSXP(recipe->GetLevel());
	if (xp > 0) {
		int16 level = client->GetPlayer()->GetTSLevel();
		if (client->GetPlayer()->AddTSXP((int32)xp)) {
			client->Message(CHANNEL_COLOR_EXP, "You gain %u Tradeskill XP!", (int32)xp);
			LogWrite(PLAYER__DEBUG, 0, "Player", "Player: %s earned %u tradeskill experience.", client->GetPlayer()->GetName(), (int32)xp);
			if(client->GetPlayer()->GetTSLevel() != level)
				client->ChangeTSLevel(level, client->GetPlayer()->GetTSLevel());
			client->GetPlayer()->SetCharSheetChanged(true);
		}
	}

	tradeskillList.erase(client);
	safe_delete(tradeskill);

	if (lock)
		m_tradeskills.releasewritelock(__FUNCTION__, __LINE__);

	// Lock TS spells and unlock all others
	client->GetPlayer()->LockTSSpells();
}

bool TradeskillMgr::IsClientCrafting(Client* client) {
	bool ret = false;

	m_tradeskills.readlock(__FUNCTION__, __LINE__);
	ret = tradeskillList.count(client) > 0;
	m_tradeskills.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}

void TradeskillMgr::CheckTradeskillEvent(Client* client, int16 icon) {
	// Check to see if the given client is crafting
	if (!IsClientCrafting(client))
		return;

	m_tradeskills.writelock(__FUNCTION__, __LINE__);
	// check to see if the client currently has an event and if it does if we had already tried to counter it this round
	if (tradeskillList[client]->CurrentEvent == 0 || tradeskillList[client]->eventChecked) {
		// No current event, or we already tried to counter it, return out
		m_tradeskills.releasewritelock(__FUNCTION__, __LINE__);
		return;
	}

	// set the eventChecked flag so we don't try to counter it again
	tradeskillList[client]->eventChecked = true;
	// compare the event icon with the given spell icon to see if we countered it and store the result for the update
	bool countered = (icon == tradeskillList[client]->CurrentEvent->Icon);
	tradeskillList[client]->eventCountered = countered;

	// send the success or fail message to the client
	client->Message(CHANNEL_COLOR_WHITE, "You %s %s.", countered ? "successfully countered" : "failed to counter", tradeskillList[client]->CurrentEvent->Name);
	
	// unlock the list and send the result packet
	m_tradeskills.releasewritelock(__FUNCTION__, __LINE__);
	ClientPacketFunctions::CounterReaction(client, countered);
}

Tradeskill* TradeskillMgr::GetTradeskill(Client* client) {
	if (tradeskillList.count(client) == 0)
		return 0;

	return tradeskillList[client];
}

MasterTradeskillEventsList::MasterTradeskillEventsList() {
	m_eventList.SetName("MasterTradeskillEventsList::eventList");
}

MasterTradeskillEventsList::~MasterTradeskillEventsList() {
	m_eventList.writelock(__FUNCTION__, __LINE__);
	map<int32, vector<TradeskillEvent*> >::iterator itr;
	vector<TradeskillEvent*>::iterator ts_itr;
	for (itr = eventList.begin(); itr != eventList.end(); itr++){
		for (ts_itr = itr->second.begin(); ts_itr != itr->second.end(); ts_itr++){
			safe_delete(*ts_itr);
		}
	}
	eventList.clear();
	m_eventList.releasewritelock(__FUNCTION__, __LINE__);
}

void MasterTradeskillEventsList::AddEvent(TradeskillEvent* tradeskillEvent) {
	m_eventList.writelock(__FUNCTION__, __LINE__);
	eventList[tradeskillEvent->Technique].push_back(tradeskillEvent);
	m_eventList.releasewritelock(__FUNCTION__, __LINE__);
}

vector<TradeskillEvent*>* MasterTradeskillEventsList::GetEventByTechnique(int32 technique) {
	if (eventList.count(technique) == 0)
		return 0;

	return &eventList[technique];
}

int32 MasterTradeskillEventsList::Size() {
	int32 count = 0;
	m_eventList.readlock(__FUNCTION__, __LINE__);
	map<int32, vector<TradeskillEvent*> >::iterator itr;
	for (itr = eventList.begin(); itr != eventList.end(); itr++)
		count += itr->second.size();
	m_eventList.releasereadlock(__FUNCTION__, __LINE__);
	return count;
}