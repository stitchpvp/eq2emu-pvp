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
#include "Recipe.h"

extern MasterRecipeList master_recipe_list;
extern MasterRecipeBookList master_recipebook_list;

void WorldDatabase::LoadRecipes() {
	Recipe *recipe;
	Query query;
	MYSQL_ROW row;
	MYSQL_RES *res;

	res = query.RunQuery2(Q_SELECT,	"SELECT `recipe_id`,`tier`,`level`,`icon`,`skill_level`,`technique`,`knowledge`,`name`,`book`,`device`,`product_classes`,`unknown2`,`unknown3`,`unknown4`, "
									"`product_item_id`, `product_name`, `product_qty`, `primary_comp_title`, `build_comp_title`, `build2_comp_title`, `build3_comp_title`, `build4_comp_title`, "
									"`build_comp_qty`, `build2_comp_qty`, `build3_comp_qty`, `build4_comp_qty`, `fuel_comp_title`, `fuel_comp_qty`\n"
									"FROM `recipes`");
	if (res) {
		while ((row = mysql_fetch_row(res))) {
			recipe = new Recipe();
			recipe->SetID(atoul(row[0]));
			recipe->SetTier(atoi(row[1]));
			recipe->SetLevel(atoi(row[2]));
			recipe->SetIcon(atoi(row[3]));
			recipe->SetSkill(atoul(row[4]));
			recipe->SetTechnique(atoul(row[5]));
			recipe->SetKnowledge(atoul(row[6]));
			recipe->SetName(row[7]);
			recipe->SetBook(row[8]);
			recipe->SetDevice(row[9]);
			recipe->SetClasses(atoul(row[10]));
			recipe->SetUnknown2(atoul(row[11]));
			recipe->SetUnknown3(atoul(row[12]));
			recipe->SetUnknown4(atoul(row[13]));

			LogWrite(TRADESKILL__DEBUG, 5, "Recipes", "Loading recipe: %s (%u)", recipe->GetName(), recipe->GetID());

			recipe->SetProductID(atoul(row[14]));
			recipe->SetProductName(row[15]);
			recipe->SetProductQuantity(atoi(row[16]));
			recipe->SetPrimaryComponentTitle(row[17]);
			recipe->SetBuildComponentTitle(row[18]);
			if (row[19] != NULL)
				recipe->SetBuild2ComponentTitle(row[19]);
			if (row[20] != NULL)
				recipe->SetBuild3ComponentTitle(row[20]);
			if (row[21] != NULL)
				recipe->SetBuild4ComponentTitle(row[21]);
			recipe->SetBuild1ComponentQuantity(atoi(row[22]));
			if (row[23] != NULL)
				recipe->SetBuild2ComponentQuantity(atoi(row[23]));
			if (row[24] != NULL)
				recipe->SetBuild3ComponentQuantity(atoi(row[24]));
			if (row[25] != NULL)
				recipe->SetBuild4ComponentQuantity(atoi(row[25]));
			recipe->SetFuelComponentTitle(row[26]);
			recipe->SetFuelComponentQuantity(atoi(row[27]));

			LogWrite(TRADESKILL__DEBUG, 7, "Recipes", "Loading recipe: %s (%u)", recipe->GetName(), recipe->GetID());

			if (!master_recipe_list.AddRecipe(recipe)) {
				LogWrite(TRADESKILL__ERROR, 0, "Recipes", "Error adding recipe '%s' - duplicate ID: %u", recipe->GetName(), recipe->GetID()); 
				safe_delete(recipe);
				continue;
			}
		}
	}
	LoadRecipeComponents();
	LoadRecipeProducts();

	LogWrite(TRADESKILL__DEBUG, 0, "Recipes", "\tLoaded %u recipes", master_recipe_list.Size());
}

void WorldDatabase::LoadRecipeBooks(){
	Recipe *recipe;
	Query query;
	MYSQL_ROW row;
	MYSQL_RES *res;

	res = query.RunQuery2(Q_SELECT, "SELECT id, name FROM items WHERE item_type='Recipe'");
	if (res){
		while ((row = mysql_fetch_row(res))){
			recipe = new Recipe();
			recipe->SetBookID(atoul(row[0]));
			recipe->SetBookName(row[1]);

			LogWrite(TRADESKILL__DEBUG, 5, "Recipes", "Loading Recipe Books: %s (%u)", recipe->GetBookName(), recipe->GetBookID());

			if (!master_recipebook_list.AddRecipeBook(recipe)){
				LogWrite(TRADESKILL__ERROR, 0, "Recipes", "Error adding Recipe Book '%s' - duplicate ID: %u", recipe->GetBookName(), recipe->GetBookID());
				safe_delete(recipe);
				continue;
			}
		}
	}
	LogWrite(TRADESKILL__DEBUG, 0, "Recipes", "\tLoaded %u Recipe Books ", master_recipebook_list.Size());
}

void WorldDatabase::LoadPlayerRecipes(Player *player){
	Recipe *recipe;
	Query query;
	MYSQL_ROW row;
	MYSQL_RES *res;
	int16 total = 0;

	assert(player);

	res = query.RunQuery2(Q_SELECT, "SELECT recipe_id, highest_stage FROM character_recipes WHERE char_id = %u", player->GetCharacterID());
	if (res) {
		while ((row = mysql_fetch_row(res))){
			recipe = new Recipe(master_recipe_list.GetRecipe(atoul(row[0])));
			recipe->SetHighestStage(atoi(row[1]));

			LogWrite(TRADESKILL__DEBUG, 5, "Recipes", "Loading recipe: %s (%u) for player: %s (%u)", recipe->GetName(), recipe->GetID(), player->GetName(), player->GetCharacterID());

			if (!player->GetRecipeList()->AddRecipe(recipe)){
				LogWrite(TRADESKILL__ERROR, 0, "Recipes", "Error adding recipe %u to player '%s' - duplicate ID", recipe->GetID(), player->GetName());
				safe_delete(recipe);
				continue;
			}
			total++;
		}
		LogWrite(TRADESKILL__DEBUG, 0, "Recipes", "Loaded %u recipes for player: %s (%u)", total, player->GetName(), player->GetCharacterID());
	}
}

int32 WorldDatabase::LoadPlayerRecipeBooks(int32 char_id, Player *player) {
	assert(player);

	LogWrite(TRADESKILL__DEBUG, 0, "Recipes", "Loading Character Recipe Books for player '%s' ...", player->GetName());
	Query query;
	MYSQL_ROW row;
	MYSQL_RES *res;
	int32 count = 0;
	int32 old_id = 0;
	int32 new_id = 0;
	Recipe* recipe;

	res = query.RunQuery2(Q_SELECT, "SELECT recipebook_id FROM character_recipe_books WHERE char_id = %u", char_id);
	if (res && mysql_num_rows(res) > 0) {
		while (res && (row = mysql_fetch_row(res))){
			count++;
			new_id = atoul(row[0]);
			if(new_id == old_id)
				continue;

			Item* item = master_item_list.GetItem(new_id);
			if (!item)
				continue;

			recipe = new Recipe();
			recipe->SetBookID(new_id);
			recipe->SetBookName(item->name.c_str());

			LogWrite(TRADESKILL__DEBUG, 5, "Recipes", "Loading Recipe Books: %s (%u)", recipe->GetBookName(), recipe->GetBookID());

			if (!player->GetRecipeBookList()->AddRecipeBook(recipe)) {
				LogWrite(TRADESKILL__ERROR, 0, "Recipes", "Error adding player Recipe Book '%s' - duplicate ID: %u", recipe->GetBookName(), recipe->GetBookID());
				safe_delete(recipe);
				continue;
			}
			old_id = new_id;
		}
	}
	return count;
}

void WorldDatabase::SavePlayerRecipeBook(Player* player, int32 recipebook_id){
	Query query;
	query.RunQuery2(Q_INSERT, "INSERT INTO character_recipe_books (char_id, recipebook_id) VALUES(%u, %u)", player->GetCharacterID(), recipebook_id);
	if(query.GetErrorNumber() && query.GetError() && query.GetErrorNumber() < 0xFFFFFFFF)
		LogWrite(TRADESKILL__ERROR, 0, "Recipes", "Error in SavePlayerRecipeBook query '%s' : %s", query.GetQuery(), query.GetError());
}

void WorldDatabase::SavePlayerRecipe(Player* player, int32 recipe_id) {
	Query query;
	query.RunQuery2(Q_INSERT, "INSERT INTO character_recipes (char_id, recipe_id) VALUES (%u, %u)", player->GetCharacterID(), recipe_id);
	if(query.GetErrorNumber() && query.GetError() && query.GetErrorNumber() < 0xFFFFFFFF)
		LogWrite(TRADESKILL__ERROR, 0, "Recipes", "Error in SavePlayerRecipeBook query '%s' : %s", query.GetQuery(), query.GetError());
}

void WorldDatabase::LoadRecipeComponents() {
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT `recipe_id`, `item_id`, `slot_id` FROM `recipe_components` ORDER BY `recipe_id` asc");
	int32 id = 0;
	Recipe* recipe = 0;
	if (result && mysql_num_rows(result) > 0) {
		while (result && (row = mysql_fetch_row(result))) {
			if (id != strtoul(row[0], NULL, 0)) {
				id = strtoul(row[0], NULL, 0);
				recipe = master_recipe_list.GetRecipe(id);
			}
			if (recipe) {
				recipe->AddBuildComp(atoi(row[1]), atoi(row[2]));
			}
			else
				LogWrite(TRADESKILL__ERROR, 0, "Recipes", "Error loading `recipe_build_comps`, Recipe ID: %u", id);
		}
	}
}

void WorldDatabase::LoadRecipeProducts() {
	Query query;
	MYSQL_ROW row;
	MYSQL_RES* result = query.RunQuery2(Q_SELECT, "SELECT `recipe_id`, `stage`, `product_id`, `byproduct_id`, `product_qty`, `byproduct_qty` FROM `recipe_products` ORDER BY `recipe_id` ASC");
	int32 id = 0;
	Recipe* recipe = 0;
	if (result && mysql_num_rows(result) > 0) {
		while (result && (row = mysql_fetch_row(result))) {
			if (id != strtoul(row[0], NULL, 0)) {
				id = strtoul(row[0], NULL, 0);
				recipe = master_recipe_list.GetRecipe(id);
			}
			if (recipe) {
				int8 stage = atoi(row[1]);
				RecipeProducts* rp = new RecipeProducts;
				rp->product_id = atoul(row[2]);
				rp->byproduct_id = atoul(row[3]);
				rp->product_qty = atoi(row[4]);
				rp->byproduct_qty = atoi(row[5]);
				recipe->products[stage] = rp;
			}
			else
				LogWrite(TRADESKILL__ERROR, 0, "Recipes", "Error loading `recipe_products`, Recipe ID: %u", id);
		}
	}
}

void WorldDatabase::UpdatePlayerRecipe(Player* player, int32 recipe_id, int8 highest_stage) {
	Query query;
	query.RunQuery2(Q_UPDATE, "UPDATE `character_recipes` SET `highest_stage` = %i WHERE `char_id` = %u AND `recipe_id` = %u", highest_stage, player->GetCharacterID(), recipe_id);
}

/*
ALTER TABLE `character_recipes`
	ADD COLUMN `highest_stage` TINYINT(3) UNSIGNED NOT NULL DEFAULT '0' AFTER `recipe_id`;

*/