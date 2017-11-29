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
#ifdef WIN32
	#include <WinSock2.h>
	#include <windows.h>
#endif
#include <mysql.h>
#include <assert.h>
#include "../../common/Log.h"
#include "../WorldDatabase.h"
#include "Items_DoV.h"
#include "../World.h"

extern World world;

void WorldDatabase::LoadDataFromRow(MYSQL_ROW row, Item* item)
{
	LogWrite(ITEM__DEBUG, 5, "Items", "\tSetting details for item ID: %u", strtoul(row[0], NULL, 0));
	item->details.item_id = strtoul(row[0], NULL, 0);
	int8 size = strlen(row[1]);

	if (size > 63)
		size = 63;

	item->name = string(row[1]);
	item->lowername = ToLower(item->name);
	item->details.icon = atoi(row[2]);
	item->details.count = atoi(row[3]);
	item->details.tier = atoi(row[4]);
	item->generic_info.weight = atoi(row[5]);

	if (row[6] && strlen(row[6]))
		item->description = string(row[6]);

	item->generic_info.show_name = atoi(row[7]);

	if (atoi(row[8]))
		item->generic_info.item_flags += ATTUNEABLE;

	if (atoi(row[9]))
		item->generic_info.item_flags += ARTIFACT;

	if (atoi(row[10]))
		item->generic_info.item_flags += LORE;

	if (atoi(row[11]))
		item->generic_info.item_flags += TEMPORARY;

	if (atoi(row[12]))
		item->generic_info.item_flags += NO_TRADE;

	if (atoi(row[13]))
		item->generic_info.item_flags += NO_VALUE;

	if (atoi(row[14]))
		item->generic_info.item_flags += NO_ZONE;

	if (atoi(row[15]))
		item->generic_info.item_flags += NO_DESTROY;

	if (atoi(row[16]))
		item->generic_info.item_flags += CRAFTED;

	if (atoi(row[17]))
		item->generic_info.item_flags += GOOD_ONLY;

	if (atoi(row[18]))
		item->generic_info.item_flags += EVIL_ONLY;

	if (atoul(row[19]) == 0)
		item->generic_info.skill_req1 = 0xFFFFFFFF;
	else
		item->generic_info.skill_req1 = atoul(row[19]);

	if (atoul(row[20]) == 0)
		item->generic_info.skill_req2 = 0xFFFFFFFF;
	else
		item->generic_info.skill_req2 = atoul(row[20]);

	item->generic_info.skill_min = atoi(row[21]);

	if (atoul(row[23])>0)
		item->SetSlots(atoul(row[23]));

	item->sell_price = atoul(row[24]);
	item->stack_count = atoi(row[25]);
	item->generic_info.collectable = atoi(row[26]);
	item->generic_info.offers_quest_id = atoul(row[27]);
	item->generic_info.part_of_quest_id = atoul(row[28]);
	item->details.recommended_level = atoi(row[29]);
	item->generic_info.adventure_default_level = atoi(row[30]);
	item->generic_info.max_charges = atoi(row[31]);
	item->generic_info.display_charges = atoi(row[32]);
	item->generic_info.tradeskill_default_level = atoi(row[33]);

#ifdef WIN32
	item->generic_info.adventure_classes = _strtoui64(row[34], NULL, 10);
	item->generic_info.tradeskill_classes = _strtoui64(row[35], NULL, 10);
#else
	item->generic_info.adventure_classes = strtoull(row[34], 0, 10);
	item->generic_info.tradeskill_classes = strtoull(row[35], 0, 10);
#endif

	if (row[36] && strlen(row[36])) {
		item->SetItemScript(row[36]);
		LogWrite(ITEM__DEBUG, 5, "LUA", "\tLoading LUA Item Script: '%s'", item->item_script.c_str());
	}

	if (item->generic_info.max_charges > 0)
		item->details.count = item->generic_info.max_charges;

	if (item->details.count == 0)
		item->details.count = 1;

	item->generic_info.usable = atoi(row[37]);
	item->details.soe_id = atoi(row[39]);
}

// handle new database class til all functions are converted
void WorldDatabase::LoadDataFromRow(DatabaseResult* result, Item* item) 
{
	LogWrite(ITEM__DEBUG, 5, "Items", "\tSetting details for item ID: %i", result->GetInt32Str("id"));

	item->details.item_id						= result->GetInt32Str("id");
	int8 size									= strlen(result->GetStringStr("name"));

	if(size > 63)
		size = 63;

	item->name									= string(result->GetStringStr("name"));
	item->lowername								= ToLower(item->name);
	item->details.icon							= result->GetInt16Str("icon");
	item->details.count							= result->GetInt16Str("count");
	item->details.tier							= result->GetInt8Str("tier");
	item->generic_info.weight					= result->GetInt32Str("weight");

	if( strlen(result->GetStringStr("description")) > 0 )
		item->description						= string(result->GetStringStr("description"));

	item->generic_info.show_name				= result->GetInt8Str("show_name");

	if( result->GetInt8Str("attuneable") == 1 )
		item->generic_info.item_flags += ATTUNEABLE;

	if( result->GetInt8Str("artifact") == 1 )
		item->generic_info.item_flags += ARTIFACT;

	if( result->GetInt8Str("lore") == 1 )
		item->generic_info.item_flags += LORE;

	if( result->GetInt8Str("temporary") == 1 )
		item->generic_info.item_flags += TEMPORARY;

	if( result->GetInt8Str("notrade") == 1 )
		item->generic_info.item_flags += NO_TRADE;

	if( result->GetInt8Str("novalue") == 1 )
		item->generic_info.item_flags += NO_VALUE;

	if( result->GetInt8Str("nozone") == 1 )
		item->generic_info.item_flags += NO_ZONE;

	if( result->GetInt8Str("nodestroy") == 1 )
		item->generic_info.item_flags += NO_DESTROY;

	if( result->GetInt8Str("crafted") == 1 )
		item->generic_info.item_flags += CRAFTED;

	if( result->GetInt8Str("good_only") == 1 )
		item->generic_info.item_flags += GOOD_ONLY;

	if( result->GetInt8Str("evil_only") == 1 )
		item->generic_info.item_flags += EVIL_ONLY;

	if (result->GetInt8Str("stacklore") == 1)
		item->generic_info.item_flags += STACK_LORE;//add this line

	if (result->GetInt8Str("lore_equip") == 1)
		item->generic_info.item_flags += LORE_EQUIP;//add this line

	if (result->GetInt8Str("flags_16384") == 1)
		item->generic_info.item_flags += FLAGS_16384;//add this line

	if (result->GetInt8Str("flags_32768") == 1)
		item->generic_info.item_flags += FLAGS_32768;//add this line

	if (result->GetInt8Str("ornate") == 1)
		item->generic_info.item_flags2 += ORNATE;

	if (result->GetInt8Str("heirloom") == 1)
		item->generic_info.item_flags2 += HEIRLOOM;

	if (result->GetInt8Str("appearance_only") == 1)
		item->generic_info.item_flags2 += APPEARANCE_ONLY;

	if (result->GetInt8Str("unlocked") == 1)
		item->generic_info.item_flags2 += UNLOCKED;

	if (result->GetInt8Str("reforged") == 1)
		item->generic_info.item_flags2 += REFORGED;

	if (result->GetInt8Str("norepair") == 1)
		item->generic_info.item_flags2 += NO_REPAIR;

	if (result->GetInt8Str("etheral") == 1)
		item->generic_info.item_flags2 += ETHERAL;

	if (result->GetInt8Str("refined") == 1)
		item->generic_info.item_flags2 += REFINED;

	if (result->GetInt8Str("flags2_256") == 1)
		item->generic_info.item_flags2 += FLAGS2_256;

	// add more Flags/Flags2 here

	if (result->GetInt8Str("lore_equip") == 1)
		item->generic_info.item_flags += LORE_EQUIP;

	if (result->GetInt8Str("flags_16384") == 1)
		item->generic_info.item_flags += FLAGS_16384;

	if (result->GetInt8Str("flags_32768") == 1)
		item->generic_info.item_flags += FLAGS_32768;

	if (result->GetInt8Str("ornate") == 1)
		item->generic_info.item_flags2 += ORNATE;

	if (result->GetInt8Str("heirloom") == 1)
		item->generic_info.item_flags2 += HEIRLOOM;

	if (result->GetInt8Str("appearance_only") == 1)
		item->generic_info.item_flags2 += APPEARANCE_ONLY;

	if (result->GetInt8Str("unlocked") == 1)
		item->generic_info.item_flags2 += UNLOCKED;

	if (result->GetInt8Str("reforged") == 1)
		item->generic_info.item_flags2 += REFORGED;

	if (result->GetInt8Str("norepair") == 1)
		item->generic_info.item_flags2 += NO_REPAIR;

	if (result->GetInt8Str("etheral") == 1)
		item->generic_info.item_flags2 += ETHERAL;

	if (result->GetInt8Str("refined") == 1)
		item->generic_info.item_flags2 += REFINED;

	if (result->GetInt8Str("flags2_256") == 1)
		item->generic_info.item_flags2 += FLAGS2_256;

	

	if( result->GetInt32Str("skill_id_req") == 0 )
		item->generic_info.skill_req1			= 0xFFFFFFFF;
	else
		item->generic_info.skill_req1			= result->GetInt32Str("skill_id_req");

	if( result->GetInt32Str("skill_id_req2") == 0 )
		item->generic_info.skill_req2			= 0xFFFFFFFF;
	else
		item->generic_info.skill_req2			= result->GetInt32Str("skill_id_req2");

	item->generic_info.skill_min				= result->GetInt16Str("skill_min");

	if( result->GetInt32Str("slots") > 0)
		item->SetSlots(result->GetInt32Str("slots"));

	item->sell_price							= result->GetInt32Str("sell_price");
	item->sell_status							= result->GetInt32Str("sell_status_amount");
	item->stack_count							= result->GetInt8Str("stack_count");
	item->generic_info.collectable				= result->GetInt8Str("collectable");
	item->generic_info.offers_quest_id			= result->GetInt32Str("offers_quest_id");
	item->generic_info.part_of_quest_id			= result->GetInt32Str("part_of_quest_id");
	item->details.recommended_level				= result->GetInt16Str("recommended_level");
	item->generic_info.adventure_default_level	= result->GetInt16Str("adventure_default_level");
	item->generic_info.max_charges				= result->GetInt16Str("max_charges");
	item->generic_info.display_charges			= result->GetInt8Str("display_charges");
	item->generic_info.tradeskill_default_level	= result->GetInt16Str("tradeskill_default_level");

#ifdef WIN32
	item->generic_info.adventure_classes		= result->GetInt64Str("adventure_classes");
	item->generic_info.tradeskill_classes		= result->GetInt64Str("tradeskill_classes");
#else
	item->generic_info.adventure_classes		= result->GetInt64Str("adventure_classes");
	item->generic_info.tradeskill_classes		= result->GetInt64Str("tradeskill_classes");
#endif

	if( !result->IsNullStr("lua_script") && strlen(result->GetStringStr("lua_script")) > 0 )
	{
		item->SetItemScript(string(result->GetStringStr("lua_script")));
		LogWrite(ITEM__DEBUG, 5, "LUA", "--Loading LUA Item Script: '%s'", item->item_script.c_str());
	}

	if(item->generic_info.max_charges > 0)
		item->details.count						= item->generic_info.max_charges;

	if(item->details.count == 0)
		item->details.count						= 1;

	item->generic_info.usable					= result->GetInt8Str("usable");
	item->details.soe_id						= result->GetSInt32Str("soe_item_id");

	item->generic_info.harvest					= result->GetInt8Str("harvest");
}

int32 WorldDatabase::LoadSkillItems()
{
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT item_id, spell_id, spell_tier FROM item_details_skill");
	int32 total = 0;
	int32 id = 0;

	if(result)
	{
		while(result && (row = mysql_fetch_row(result)))
		{
			id = atoul(row[0]);
			Item* item = master_item_list.GetItem(id);

			if(item)
			{
				LogWrite(ITEM__DEBUG, 5, "Items", "\tLoading Skill for item_id %u", id);
				LogWrite(ITEM__DEBUG, 5, "Items", "\ttype: %i, spell: %i, tier: %i", ITEM_TYPE_SKILL, atoi(row[1]), atoi(row[2]));
				item->SetItemType(ITEM_TYPE_SKILL);
				item->skill_info->spell_id = atoul(row[1]);
				item->skill_info->spell_tier = atoi(row[2]);
				total++;
			}
			else
				LogWrite(ITEM__ERROR, 0, "Items", "Error loading `item_details_skill`, ID: %i", id);
		}
	}
	return total;
}

int32 WorldDatabase::LoadShields()
{
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT item_id, mitigation_low, mitigation_high FROM item_details_shield");
	int32 total = 0;
	int32 id = 0;

	if(result)
	{
		while(result && (row = mysql_fetch_row(result)))
		{
			id = strtoul(row[0], NULL, 0);
			Item* item = master_item_list.GetItem(id);

			if(item)
			{
				LogWrite(ITEM__DEBUG, 5, "Items", "\tItem Shield for item_id: %u", id);
				LogWrite(ITEM__DEBUG, 5, "Items", "\ttype: %i, mit_low: %i, mit_high: %i", ITEM_TYPE_SHIELD, atoi(row[1]), atoi(row[2]));
				item->SetItemType(ITEM_TYPE_SHIELD);
				item->armor_info->mitigation_low = atoi(row[1]);
				item->armor_info->mitigation_high = atoi(row[2]);
				total++;
			}
			else
				LogWrite(ITEM__ERROR, 0, "Items", "Error loading `item_details_shield`, ID: %i", id);
		}
	}
	return total;
}
int32 WorldDatabase::LoadAdornments()
{
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT item_id, duration, item_types,slot_type FROM item_details_adornments");
	int32 total = 0;
	int32 id = 0;

	if (result)
	{
		while (result && (row = mysql_fetch_row(result)))
		{
			id = strtoul(row[0], NULL, 0);
			Item* item = master_item_list.GetItem(id);

			if (item)
			{
				//LogWrite(ITEM__DEBUG, 0, "Items", "\tItem Adornment for item_id: %u", id);
				//LogWrite(ITEM__DEBUG, 0, "Items", "\ttype: %i, Duration: %i, item_types_: %i, slot_type: %i", ITEM_TYPE_ADORNMENT, atoi(row[1]), atoi(row[2]), atoi(row[3]));
				item->SetItemType(ITEM_TYPE_ADORNMENT);
				item->adornment_info->duration = atof(row[1]);
				item->adornment_info->item_types = atoi(row[2]);
				item->adornment_info->slot_type = atoi(row[3]);
				//LogWrite(ITEM__DEBUG, 0, "Items", "\ttype: %i, Duration: %i, item_types_: %i, slot_type: %i",item->generic_info.item_type, item->adornment_info->duration, item->adornment_info->item_types, item->adornment_info->slot_type);
				total++;
			}
			else
				LogWrite(ITEM__ERROR, 0, "Items", "Error loading `item_details_shield`, ID: %i", id);
		}
	}
	return total;
}
int32 WorldDatabase::LoadClassifications()
{
	int32 total = 0;
	int32 id = 0;
	return total;
}

int32 WorldDatabase::LoadBaubles()
{
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT item_id, cast, recovery, duration, recast, display_slot_optional, display_cast_time, display_bauble_type, effect_radius, max_aoe_targets, display_until_cancelled FROM item_details_bauble");
	int32 total = 0;
	int32 id = 0;

	if(result)
	{
		while(result && (row = mysql_fetch_row(result)))
		{
			id = strtoul(row[0], NULL, 0);
			Item* item = master_item_list.GetItem(id);

			if(item)
			{
				LogWrite(ITEM__DEBUG, 5, "Items", "\tItem Bauble for item_id %u", id);
				LogWrite(ITEM__DEBUG, 5, "Items", "\ttype: %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i", ITEM_TYPE_BAUBLE, atoi(row[1]), atoi(row[2]), atoi(row[3]), atoi(row[4]), atoi(row[5]), atoi(row[6]), atof(row[7]), atoi(row[8]), atoi(row[9]), atoi(row[10]));
				item->SetItemType(ITEM_TYPE_BAUBLE);
				item->bauble_info->cast = atoi(row[1]);
				item->bauble_info->recovery = atoi(row[2]);
				item->bauble_info->duration = atoi(row[3]);
				item->bauble_info->recast = atoi(row[4]);
				item->bauble_info->display_slot_optional = atoi(row[5]);
				item->bauble_info->display_cast_time = atoi(row[6]);
				item->bauble_info->display_bauble_type = atoi(row[7]);
				item->bauble_info->effect_radius = atof(row[8]);
				item->bauble_info->max_aoe_targets = atoi(row[9]);
				item->bauble_info->display_until_cancelled = atoi(row[10]);
				total++;
			}
			else
				LogWrite(ITEM__ERROR, 0, "Items", "Error loading `item_details_bauble`, ID: %i", id);
		}
	}
	return total;
}

int32 WorldDatabase::LoadBooks()
{
	DatabaseResult result;
	int32 total = 0;
	int32 id = 0;

	if( database_new.Select(&result, "SELECT item_id, language, author, title FROM item_details_book") )
	{
		while( result.Next() )
		{
			id = result.GetInt32Str("item_id");
			Item* item = master_item_list.GetItem(id);

			if(item)
			{
				LogWrite(ITEM__DEBUG, 5, "Items", "\tItem Book for item_id %u", id);
				LogWrite(ITEM__DEBUG, 5, "Items", "\ttype: %i, %i, %s, %s", 
					ITEM_TYPE_BOOK, 
					result.GetInt8Str("language"), 
					result.GetStringStr("author"), 
					result.GetStringStr("title"));

				item->SetItemType(ITEM_TYPE_BOOK);
				item->book_info->language = result.GetInt8Str("language");
				item->book_info->author.data = result.GetStringStr("author");
				item->book_info->author.size = item->book_info->author.data.length();
				item->book_info->title.data = result.GetStringStr("title");
				item->book_info->title.size = item->book_info->title.data.length();

				total++;
			}
			else
				LogWrite(ITEM__ERROR, 0, "Items", "Error loading `item_details_book`, ID: %i", id);
		}
	}

	return total;
}
int32 WorldDatabase::LoadItemsets()
{
	DatabaseResult result;
	int32 total = 0;
	int32 id = 0;

	if (database_new.Select(&result, "SELECT id, itemset_item_id, item_id, item_icon,item_stack_size,item_list_color,language_type FROM item_details_itemset"))
	{
		while (result.Next())
		{
			id = result.GetInt32Str("itemset_item_id");
			Item* item = master_item_list.GetItem(id);

			if (item)
			{
				item->SetItemType(ITEM_TYPE_ITEMCRATE);
				//int32 item_id = result.GetInt32Str("item_id");
				item->AddSet(result.GetInt32Str("item_id"),0, result.GetInt16Str("item_icon"), result.GetInt16Str("item_stack_size"), result.GetInt32Str("item_list_color"));
				

				total++;
			}
			else
				LogWrite(ITEM__ERROR, 0, "Item Set Crate Items", "Error loading `item_details_Items`, ID: %i", id);
		}
	}

	return total;
}
int32 WorldDatabase::LoadHouseItem()
{
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT item_id, rent_reduction, status_rent_reduction, coin_rent_reduction, house_only FROM item_details_house");
	int32 total = 0;
	int32 id = 0;

	if(result)
	{
		while(result && (row = mysql_fetch_row(result)))
		{
			id = strtoul(row[0], NULL, 0);
			Item* item = master_item_list.GetItem(id);

			if(item)
			{
				LogWrite(ITEM__DEBUG, 5, "Items", "\tItem HouseItem for item_id %u", id);
				LogWrite(ITEM__DEBUG, 5, "Items", "\ttype: %i, %i, %u, %.2f, %u", ITEM_TYPE_HOUSE, atoul(row[1]), atoi(row[2]), atof(row[3]), atoul(row[4]));
				item->SetItemType(ITEM_TYPE_HOUSE);
				item->houseitem_info->status_rent_reduction = atoi(row[2]);
				item->houseitem_info->coin_rent_reduction = atof(row[3]);
				item->houseitem_info->house_only = atoi(row[4]);
				total++;
			}
			else
				LogWrite(ITEM__ERROR, 0, "Items", "Error loading `item_details_house`, ID: %i", id);
		}
	}
	return total;
}

int32 WorldDatabase::LoadRecipeBookItems()
{
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT item_id, name FROM item_details_recipe_items");
	int32 total = 0;
	int32 id = 0;

	if (result)
	{
		while(result && (row = mysql_fetch_row(result)))
		{
			id = strtoul(row[0], NULL, 0);
			Item* item = master_item_list.GetItem(id);

			if(item)
			{
				LogWrite(ITEM__DEBUG, 5, "Items", "\tRecipe Book for item_id %u", id);
				LogWrite(ITEM__DEBUG, 5, "Items", "\tType: %i, '%s'", ITEM_TYPE_RECIPE, row[1]);
				item->SetItemType(ITEM_TYPE_RECIPE);
				item->recipebook_info->recipes.push_back(string(row[1]));
				total++;
			}
			else
				LogWrite(ITEM__ERROR, 0, "Items", "Error loading `item_details_recipe_items`, ID: %u", id);
		}
	}
	return total;
}

int32 WorldDatabase::LoadHouseContainers(){
	DatabaseResult result;
	int32 total = 0;
	int32 id = 0;

	if( database_new.Select(&result, "SELECT item_id, num_slots, allowed_types, broker_commission, fence_commission FROM item_details_house_container") )
	{
		while (result.Next() )
		{
			id = result.GetInt32Str("item_id");
			Item* item = master_item_list.GetItem(id);

			if (item)
			{
				LogWrite(ITEM__DEBUG, 5, "Items", "\tHouse Container for item_id %u", id);
				LogWrite(ITEM__DEBUG, 5, "Items", "\tType: %i, '%i', '%u', '%i', '%i'", ITEM_TYPE_RECIPE, result.GetInt8Str("num_slots"), result.GetInt64Str("allowed_types"), result.GetInt8Str("broker_commission"), result.GetInt8Str("fence_commission"));

				item->SetItemType(ITEM_TYPE_HOUSE_CONTAINER);
				item->housecontainer_info->num_slots = result.GetInt8Str("num_slots");
				item->housecontainer_info->allowed_types = result.GetInt64Str("allowed_types");
				item->housecontainer_info->broker_commission = result.GetInt8Str("broker_commission");
				item->housecontainer_info->fence_commission = result.GetInt8Str("fence_commission");

				total++;
			}
			else
				LogWrite(ITEM__ERROR, 0, "Items", "Error loading `item_details_house_container`, ID: %u", id);
		}
	}
	return total;
}

int32 WorldDatabase::LoadArmor()
{
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT item_id, mitigation_low, mitigation_high FROM item_details_armor");
	int32 total = 0;
	int32 id = 0;

	if(result)
	{
		while(result && (row = mysql_fetch_row(result)))
		{
			id = strtoul(row[0], NULL, 0);
			Item* item = master_item_list.GetItem(id);
			if(item)
			{
				LogWrite(ITEM__DEBUG, 5, "Items", "\tItem Armor for item_id %u", id);
				LogWrite(ITEM__DEBUG, 5, "Items", "\ttype: %i, mit_low: %i, mit_high: %i", ITEM_TYPE_ARMOR, atoi(row[1]), atoi(row[2]));
				item->SetItemType(ITEM_TYPE_ARMOR);
				item->armor_info->mitigation_low = atoi(row[1]);
				item->armor_info->mitigation_high = atoi(row[2]);
				total++;
			}
			else
				LogWrite(ITEM__ERROR, 0, "Items", "Error loading `item_details_armor`, ID: %i", id);
		}
	}
	return total;
}

int32 WorldDatabase::LoadBags()
{
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT item_id, num_slots, weight_reduction FROM item_details_bag");
	int32 total = 0;
	int32 id = 0;
	if(result)
	{
		while(result && (row = mysql_fetch_row(result)))
		{
			id = strtoul(row[0], NULL, 0);
			Item* item = master_item_list.GetItem(id);

			if(item)
			{
				LogWrite(ITEM__DEBUG, 5, "Items", "\tItem Bag for item_id %u", id);
				LogWrite(ITEM__DEBUG, 5, "Items", "\ttype: %i, slots: %i, wt_red: %i", id, ITEM_TYPE_BAG, atoi(row[1]), atoi(row[2]));
				item->SetItemType(ITEM_TYPE_BAG);
				item->details.num_slots = atoi(row[1]);
				item->details.num_free_slots = item->details.num_slots;
				item->bag_info->num_slots = item->details.num_slots;
				item->bag_info->weight_reduction = atoi(row[2]);
				total++;
			}
			else
				LogWrite(ITEM__ERROR, 0, "Items", "Error loading `item_details_bag`, ID: %i", id);
		}
	}
	return total;
}

int32 WorldDatabase::LoadFoods()
{
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT item_id, type, level, duration, satiation FROM item_details_food");
	int32 total = 0;
	int32 id = 0;

	if(result)
	{
		while(result && (row = mysql_fetch_row(result)))
		{
			id = strtoul(row[0], NULL, 0);
			Item* item = master_item_list.GetItem(id);

			if(item)
			{
				LogWrite(ITEM__DEBUG, 5, "Items", "\tItem Food for item_id %u", id);
				LogWrite(ITEM__DEBUG, 5, "Items", "\ttype: %i, lvl: %i, dur: %i, sat: %.2f, tier: %i", ITEM_TYPE_FOOD, atoi(row[1]), atoi(row[2]), atof(row[3]), atoi(row[4]));
				item->SetItemType(ITEM_TYPE_FOOD);
				item->food_info->type = atoi(row[1]);
				item->food_info->level = atoi(row[2]);
				item->food_info->duration = atof(row[3]);
				item->food_info->satiation = atoi(row[4]);
				item->details.tier = atoi(row[4]);
				total++;
			}
			else
				LogWrite(ITEM__ERROR, 0, "Items", "Error loading `item_details_food`, ID: %i", id);
		}
	}
	return total;
}

int32 WorldDatabase::LoadRangeWeapons()
{
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT item_id, damage_low1, damage_high1, damage_low2, damage_high2, damage_low3, damage_high3, delay, damage_rating, range_low, range_high, damage_type FROM item_details_range");
	int32 total = 0;
	int32 id = 0;

	if(result)
	{
		while(result && (row = mysql_fetch_row(result)))
		{
			id = strtoul(row[0], NULL, 0);
			Item* item = master_item_list.GetItem(id);

			if(item)
			{
				LogWrite(ITEM__DEBUG, 5, "Items", "\tItem Ranged for item_id %u", id);
				LogWrite(ITEM__DEBUG, 5, "Items", "\ttype: %i, %i, %i, %i, %i, %i, %i, %i, %.2f, %i, %i, %i", ITEM_TYPE_RANGED, atoi(row[1]), atoi(row[2]), atoi(row[3]), atoi(row[4]), atoi(row[5]), atoi(row[6]), atoi(row[7]), atof(row[8]), atoi(row[9]), atoi(row[10]), atoi(row[11]));
				item->SetItemType(ITEM_TYPE_RANGED);
				item->ranged_info->weapon_info.damage_low1 = atoi(row[1]);
				item->ranged_info->weapon_info.damage_high1 = atoi(row[2]);
				item->ranged_info->weapon_info.damage_low2 = atoi(row[3]);
				item->ranged_info->weapon_info.damage_high2 = atoi(row[4]);
				item->ranged_info->weapon_info.damage_low3 = atoi(row[5]);
				item->ranged_info->weapon_info.damage_high3 = atoi(row[6]);
				item->ranged_info->weapon_info.delay = atoi(row[7]);
				item->ranged_info->weapon_info.rating = atof(row[8]);
				item->ranged_info->range_low = atoi(row[9]);
				item->ranged_info->range_high = atoi(row[10]);
				item->SetWeaponType(atoi(row[11]));
				total++;
			}
			else
				LogWrite(ITEM__ERROR, 0, "Items", "Error loading `item_details_range`, ID: %i", id);
		}
	}
	return total;
}

int32 WorldDatabase::LoadThrownWeapons()
{
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT item_id, range_bonus, damage_bonus, hit_bonus, damage_type FROM item_details_thrown");
	int32 total = 0;
	int32 id = 0;

	if(result)
	{
		while(result && (row = mysql_fetch_row(result)))
		{
			id = strtoul(row[0], NULL, 0);
			Item* item = master_item_list.GetItem(id);

			if(item)
			{
				LogWrite(ITEM__DEBUG, 5, "Items", "\tItem Thrown for item_id %u", id);
				LogWrite(ITEM__DEBUG, 5, "Items", "\ttype: %i, %i, %u, %.2f, %u", ITEM_TYPE_THROWN, atoul(row[1]), atoi(row[2]), atof(row[3]), atoul(row[4]));
				item->SetItemType(ITEM_TYPE_THROWN);
				item->thrown_info->range = atoul(row[1]);
				item->thrown_info->damage_modifier = atoul(row[2]);
				item->thrown_info->hit_bonus = atof(row[3]);
				item->thrown_info->damage_type = atoul(row[4]);
				item->SetWeaponType(item->thrown_info->damage_type);
				total++;
			}
			else
				LogWrite(ITEM__ERROR, 0, "Items", "Error loading `item_details_thrown`, ID: %i", id);
		}
	}
	return total;
}

int32 WorldDatabase::LoadWeapons()
{
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT item_id, wield_style, damage_low1, damage_high1, damage_low2, damage_high2, damage_low3, damage_high3, delay, damage_rating, damage_type FROM  item_details_weapon");
	int32 total = 0;
	int32 id = 0;

	if(result)
	{
		while(result && (row = mysql_fetch_row(result)))
		{
			id = strtoul(row[0], NULL, 0);
			Item* item = master_item_list.GetItem(id);

			if(item)
			{
				LogWrite(ITEM__DEBUG, 5, "Items", "\tItem Weapon for item_id %u", id);
				LogWrite(ITEM__DEBUG, 5, "Items", "\ttype: %i, %i, %i, %i, %i, %i, %i, %i, %i, %.2f, %i", ITEM_TYPE_WEAPON, atoi(row[1]), atoi(row[2]), atoi(row[3]), atoi(row[4]), atoi(row[5]), atoi(row[6]), atoi(row[7]), atoi(row[8]), atof(row[9]), atoi(row[10]));
				item->SetItemType(ITEM_TYPE_WEAPON);
				item->weapon_info->wield_type = atoi(row[1]);
				item->weapon_info->damage_low1 = atoi(row[2]);
				item->weapon_info->damage_high1 = atoi(row[3]);
				item->weapon_info->damage_low2 = atoi(row[4]);
				item->weapon_info->damage_high2 = atoi(row[5]);
				item->weapon_info->damage_low3 = atoi(row[6]);
				item->weapon_info->damage_high3 = atoi(row[7]);
				item->weapon_info->delay = atoi(row[8]);
				item->weapon_info->rating = atof(row[9]);
				item->SetWeaponType(atoi(row[10]));
				total++;
			}
			else
				LogWrite(ITEM__ERROR, 0, "Items", "Error loading `item_weapons`, ID: %i", id);
		}
	}
	return total;
}

int32 WorldDatabase::LoadItemAppearances()
{
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT item_id, equip_type, red, green, blue, highlight_red, highlight_green, highlight_blue FROM item_appearances ORDER BY item_id asc");
	int32 id = 0;
	Item* item = 0;
	int32 total = 0;

	if(result && mysql_num_rows(result) >0)
	{
		while(result && (row = mysql_fetch_row(result)))
		{
			if(id != strtoul(row[0], NULL, 0))
			{
				id = strtoul(row[0], NULL, 0);
				item = master_item_list.GetItem(id);

				if(item)
				{
					LogWrite(ITEM__DEBUG, 5, "Items", "\tItem Appearance for item_id %u", id);
					LogWrite(ITEM__DEBUG, 5, "Items", "\tequip_type: %i, R: %i, G: %i, B: %i, HR: %i, HG: %i, HB: %i", atoi(row[1]), atoi(row[2]), atoi(row[3]), atoi(row[4]), atoi(row[5]), atoi(row[6]), atoi(row[7]));
					item->SetAppearance(atoi(row[1]), atoi(row[2]), atoi(row[3]), atoi(row[4]), atoi(row[5]), atoi(row[6]), atoi(row[7]));
					total++;
				}
				else
					LogWrite(ITEM__ERROR, 0, "Items", "Error Loading item_appearances, ID: %i", id);
			}
		}
	}
	return total;
}

int32 WorldDatabase::LoadItemEffects()
{
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT item_id, effect, percentage, bullet FROM item_effects ORDER BY item_id, id");
	int32 id = 0;
	Item* item = 0;
	int32 total = 0;

	if(result && mysql_num_rows(result) >0)
	{
		while(result && (row = mysql_fetch_row(result)))
		{
			if(id != atoul(row[0]))
			{
				id = atoul(row[0]);
				item = master_item_list.GetItem(id);
			}

			if(item && row[1])
			{
				LogWrite(ITEM__DEBUG, 5, "Items", "\tItem Effects for item_id %u", id);
				LogWrite(ITEM__DEBUG, 5, "Items", "\tEffect: '%s', Percent: %i, Sub: %i", row[1], atoi(row[2]), atoi(row[3]));
				item->AddEffect(row[1], atoi(row[2]), atoi(row[3]));
				total++;
			}
			else
				LogWrite(ITEM__ERROR, 0, "Items", "Error Loading item_effects, ID: %i", id);
		}
	}
	return total;
}

int32 WorldDatabase::LoadItemLevelOverride()
{
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT item_id, adventure_class_id, tradeskill_class_id, level FROM item_levels_override ORDER BY item_id asc");
	int32 id = 0;
	Item* item = 0;
	int32 total = 0;

	if(result && mysql_num_rows(result) >0)
	{
		while(result && (row = mysql_fetch_row(result)))
		{
			if(id != strtoul(row[0], NULL, 0))
			{
				id = strtoul(row[0], NULL, 0);
				item = master_item_list.GetItem(id);
			}

			if(item)
			{
				LogWrite(ITEM__DEBUG, 5, "Items", "\tLevel Override for item_id %u", id);
				LogWrite(ITEM__DEBUG, 5, "Items", "\tAdv: %i, TS: %i, Lvl: %i", atoi(row[1]), atoi(row[2]), atoi(row[3]));
				item->AddLevelOverride(atoi(row[1]), atoi(row[2]), atoi(row[3]));
				total++;
			}
			else
				LogWrite(ITEM__ERROR, 0, "Items", "Error loading `item_levels_override`, ID: %i", id);
		}
	}
	return total;
}

int32 WorldDatabase::LoadItemStats()
{
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT item_id, type, subtype, value, text FROM item_stats ORDER BY item_id asc");
	int32 id = 0;
	Item* item = 0;
	int32 total = 0;

	if(result && mysql_num_rows(result) >0)
	{
		while(result && (row = mysql_fetch_row(result)))
		{
			if(id != strtoul(row[0], NULL, 0))
			{
				id = strtoul(row[0], NULL, 0);
				item = master_item_list.GetItem(id);
			}

			if(item)
			{
				LogWrite(ITEM__DEBUG, 5, "Items", "\tItem Stats for item_id %u", id);
				LogWrite(ITEM__DEBUG, 5, "Items", "\ttype: %i, sub: %i, val: %.2f, name: %s", atoi(row[1]), atoi(row[2]), atof(row[3]), row[4]);
				item->AddStat(atoi(row[1]), atoi(row[2]), atof(row[3]), row[4]);
				total++;
			}
			else
				LogWrite(ITEM__ERROR, 0, "Items", "Error loading `item_stats`, ID: %i", id);
		}
	}
	return total;
}

void WorldDatabase::ReloadItemList() 
{
	LogWrite(ITEM__DEBUG, 0, "Items", "Unloading Item List...");
	master_item_list.RemoveAll();
	LoadItemList();
}

void WorldDatabase::LoadItemList()
{
	DatabaseResult result;

	int32 t_now = Timer::GetUnixTimeStamp();
	int32 total = 0;
	int32 normal_items = 0;
	string item_type;

	if( !database_new.Select(&result, "SELECT * FROM items") )
		LogWrite(ITEM__ERROR, 0, "Items", "Cannot load items in %s, line: %i", __FUNCTION__, __LINE__);
	else
	{
		while( result.Next() )
		{
			item_type = result.GetStringStr("item_type");
			LogWrite(ITEM__DEBUG, 5, "Items", "\tLoading: %s (ID: %i, Type: %s)...", result.GetStringStr("name"), result.GetInt32Str("id"), item_type.c_str());

			Item* item = new Item;
			LoadDataFromRow(&result, item);
			master_item_list.AddItem(item);

			if( strcmp(item_type.c_str(), "Normal") == 0 )
			{
				item->SetItemType(ITEM_TYPE_NORMAL);
				normal_items++;
			}
			total++;
		}
	}

	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u Normal Items", normal_items);
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u Baubles", LoadBaubles());
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u Bags", LoadBags());
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u Books", LoadBooks());
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u Item Sets", LoadItemsets());
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u House Items", LoadHouseItem());
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u Food Items", LoadFoods());
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u Weapons", LoadWeapons());
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u Ranged Weapons", LoadRangeWeapons());
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u Thrown Weapons", LoadThrownWeapons());
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u Armor Pieces", LoadArmor());
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u Shields", LoadShields());
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u Skill Items", LoadSkillItems());
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u Adornment Items", LoadAdornments());
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u Recipe Book Items", LoadRecipeBookItems());
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u House Containers", LoadHouseContainers());

	LogWrite(ITEM__DEBUG, 0, "Items", "Loading Item Appearances...");
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u Item Appearances", LoadItemAppearances());

	LogWrite(ITEM__DEBUG, 0, "Items", "Loading Item Stats...");
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u Item Stats", LoadItemStats());

	LogWrite(ITEM__DEBUG, 0, "Items", "Loading Item Effects...");
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u Item Effects", LoadItemEffects());

	LogWrite(ITEM__DEBUG, 0, "Items", "Loading Item Level Overrides...");
	LogWrite(ITEM__DEBUG, 0, "Items", "\tLoaded %u Item Level Overrides", LoadItemLevelOverride());

	LogWrite(ITEM__INFO, 0, "Items", "Loaded %u Total Item%s (took %u seconds)", total, ( total == 1 ) ? "" : "s", Timer::GetUnixTimeStamp() - t_now);
}

int32 WorldDatabase::LoadNextUniqueItemID()
{
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT max(id) FROM character_items");

	if(result && (row = mysql_fetch_row(result)))
	{
		if(row[0])
		{
			LogWrite(ITEM__DEBUG, 0, "Items", "%s: max(id): %u", __FUNCTION__, atoul(row[0]));
			return strtoul(row[0], NULL, 0);
		}
		else 
			return 0;
	}
	else if(!result)
		LogWrite(ITEM__ERROR, 0, "Items", "%s: Unable to load next unique item ID.", __FUNCTION__);

	return 0;
}

void WorldDatabase::SaveItems(Client* client) 
{
	LogWrite(ITEM__DEBUG, 3, "Items", "Save Items for Player %i", client->GetCharacterID());

	map<int32, Item*>* items = client->GetPlayer()->GetItemList();
	map<int32, Item*>::iterator item_iter;
	Item* item = 0;

	for(item_iter = items->begin(); item_iter != items->end(); item_iter++)
	{
		item = item_iter->second;

		if(item && item->save_needed)
		{
			LogWrite(ITEM__DEBUG, 5, "Items", "SaveItems: Acct: %u, Char: %u, Item: %u, NOT-EQUIPPED", client->GetAccountID(), client->GetCharacterID(), item);
			SaveItem(client->GetAccountID(), client->GetCharacterID(), item, "NOT-EQUIPPED");
			item->save_needed = false;
		}
	}
	safe_delete(items);

	vector<Item*>* equipped_list = client->GetPlayer()->GetEquippedItemList();

	for(int32 i=0;i<equipped_list->size();i++)
	{
		item = equipped_list->at(i);

		if(item && item->save_needed)
		{
			SaveItem(client->GetAccountID(), client->GetCharacterID(), item, "EQUIPPED");
			item->save_needed = false;
		}
	}
	safe_delete(equipped_list);

	vector<Item*>* overflow = client->GetPlayer()->item_list.GetOverflowItemList();
	for (int32 i = 0; i < overflow->size(); i++){
		item = overflow->at(i);
		if (item) {
			sint16 slot = item->details.slot_id;
			item->details.slot_id = i;
			SaveItem(client->GetAccountID(), client->GetCharacterID(), item, "NOT-EQUIPPED");
			item->details.slot_id = slot;
		}
	}
	safe_delete(overflow);
}

void WorldDatabase::SaveItem(int32 account_id, int32 char_id, Item* item, const char* type) 
{
	LogWrite(ITEM__DEBUG, 1, "Items", "Saving ItemID: %u (Type: %s) for account: %u, player: %u", item->details.item_id, type, account_id, char_id);

	Query query;
	string update_item = string("REPLACE INTO character_items (id, type, char_id, slot, item_id, creator,adorn0,adorn1,adorn2, condition_, attuned, bag_id, count, max_sell_value, account_id, login_checksum) VALUES (%u, '%s', %u, %i, %u, '%s', %i, %i, %i, %i, %i, %i, %i, %u, %u, 0)");
	query.RunQuery2(Q_REPLACE, update_item.c_str(), item->details.unique_id, type, char_id, item->details.slot_id, item->details.item_id, 
		getSafeEscapeString(item->creator.c_str()).c_str(),item->adorn0,item->adorn1,item->adorn2, item->generic_info.condition, item->CheckFlag(ATTUNED) ? 1 : 0, item->details.inv_slot_id, item->details.count, item->GetMaxSellValue(), account_id);
}

void WorldDatabase::DeleteItem(int32 char_id, Item* item, const char* type) 
{
	Query query;
	string delete_item;

	if(type)
	{
		LogWrite(ITEM__DEBUG, 1, "Items", "Deleting item_id %u (Type: %s) for player %u", item->details.item_id, type, char_id);

		delete_item = string("DELETE FROM character_items WHERE char_id = %u AND (id = %u OR bag_id = %u) AND type='%s'");
		query.RunQuery2(Q_DELETE, delete_item.c_str(), char_id, item->details.unique_id, item->details.unique_id, type);
	}
	else
	{
		LogWrite(ITEM__DEBUG, 0, "Items", "Deleting item_id %u for player %u", item->details.item_id, char_id);

		delete_item = string("DELETE FROM character_items WHERE char_id = %u AND (id = %u OR bag_id = %u)");
		query.RunQuery2(Q_DELETE, delete_item.c_str(), char_id, item->details.unique_id, item->details.unique_id);
	}
}

void WorldDatabase::LoadCharacterItemList(int32 account_id, int32 char_id, Player* player) 
{
	LogWrite(ITEM__DEBUG, 0, "Items", "Loading items for character '%s' (%u)", player->GetName(), char_id);

	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT type, id, slot, item_id, creator,adorn0,adorn1,adorn2, condition_, attuned, bag_id, count, max_sell_value FROM character_items where char_id = %u or (bag_id = -4 and account_id = %u) ORDER BY slot asc", char_id, account_id);

	if(result)
	{
		bool ret = true;

		while(result && (row = mysql_fetch_row(result)))
		{
			LogWrite(ITEM__DEBUG, 5, "Items", "\tLoading character item: %u, slot: %i", strtoul(row[1], NULL, 0), atoi(row[2]));
			Item* master_item = master_item_list.GetItem(strtoul(row[3], NULL, 0));

			if(master_item)
			{
				Item* item = new Item(master_item);
				item->details.unique_id = strtoul(row[1], NULL, 0);
				item->details.slot_id = atoi(row[2]);

				if(item->details.num_slots > 0)
					item->details.bag_id = item->details.unique_id;

				item->save_needed = false;

				if(row[4])
					item->creator = string(row[4]);//creator
				item->adorn0 = atoi(row[5]); //adorn0
				item->adorn1 = atoi(row[6]); //adorn1
				item->adorn2 = atoi(row[7]); //adorn2
				item->generic_info.condition = atoi(row[8]); //condition

				if(row[9] && atoi(row[9])>0) //attuned
				{
					if(item->CheckFlag(ATTUNEABLE))
						item->generic_info.item_flags -= ATTUNEABLE;

					if(!item->CheckFlag(NO_TRADE))
						item->generic_info.item_flags += NO_TRADE;

					item->generic_info.item_flags += ATTUNED;
				}

				item->details.inv_slot_id = atol(row[10]); //bag_id
				item->details.count = atoi(row[11]); //count
				item->SetMaxSellValue(atoul(row[12])); //max sell value

				if(strncasecmp(row[0], "EQUIPPED", 8)==0)
					ret = player->GetEquipmentList()->AddItem(item->details.slot_id, item);
				else if (strncasecmp(row[0], "APPEARANCE", 10) == 2)
					ret = player->GetEquipmentList()->AddItem(item->details.slot_id, item);
				else {
					if (item->details.inv_slot_id == -2)
						player->item_list.AddOverflowItem(item);
					else
						player->item_list.AddItem(item);
				}
			}
			else
				ret = false;
		}

		if(!ret)
			LogWrite(ITEM__ERROR, 0, "Items", "%s: Error Loading item(s) for Char ID: %u (%s)", __FUNCTION__, char_id, player->GetName());
	}
}

