#pragma once

#include <string>
#include "Player.h"

const int8 PVP_FAME_AMOUNT_PER_KILL = 30;
const int8 PVP_LOCK_DURATION = 30;

const map<sint32, string> PVP_TITLES = {
	{ 3000, string("Master") },
	{ 2400, string("General") },
	{ 1900, string("Dreadnaught") },
	{ 1400, string("Champion") },
	{ 1000, string("Destroyer") },
	{ 600, string("Slayer") },
	{ 300, string("Hunter") }
};

class PVP {
public:
	static void HandlePlayerEncounter(Player* attacker, Player* target, bool is_hostile);
	static bool CanAttack(Player*, Spawn*);
	static bool IsHostile(Player* attacker, Spawn* target);
	static string GetRank(Player*);
	static int GetRankIndex(Player* player);
	static void HandleFameChange(Player* attacker, Player* victim);
	static bool IsEnabled(ZoneServer* zone = 0);
};