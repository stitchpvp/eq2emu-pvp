#include "NPC.h"
#include "Player.h"
#include "PVP.h"
#include "Rules/Rules.h"
#include "World.h"
#include "zoneserver.h"

extern RuleManager rule_manager;

int16 Player::GetFame() {
	return GetInfoStruct()->fame;
}

void Player::SetFame(sint16 value) {
	if (value < 0) {
		GetInfoStruct()->fame = 0;
	} else {
		GetInfoStruct()->fame = value;
	}
}

void PVP::HandlePlayerEncounter(Player* source, Player* target, bool is_hostile) {
	if (!PVP::IsEnabled(source->GetZone())) return;

	if (is_hostile) {
		source->AddToEncounterList(target->GetID(), Timer::GetCurrentTime2());
		target->AddToEncounterList(source->GetID(), Timer::GetCurrentTime2(), false);
	} else {
		vector<Spawn*> to_add;

		target->encounter_list_mutex.lock();
		for (const auto& kv : target->encounter_list) {
			Spawn* spawn = target->GetZone()->GetSpawnByID(kv.first);

			if (spawn && spawn->IsPlayer())
				to_add.push_back(spawn);
		}
		target->encounter_list_mutex.unlock();

		for (const auto spawn : to_add) {
			source->AddToEncounterList(spawn->GetID(), Timer::GetCurrentTime2(), true);
			static_cast<Player*>(spawn)->AddToEncounterList(source->GetID(), Timer::GetCurrentTime2(), false);
		}
	}
}

bool PVP::CanAttack(Player* attacker, Spawn* target)
{
	if (PVP::IsEnabled(attacker->GetZone())) {
		Entity* entity_target = static_cast<Entity*>(target);

		if (target->IsPet())
			entity_target =  static_cast<NPC*>(target)->GetOwner();

		if (attacker->GetPVPImmune() || (entity_target->IsPlayer() && static_cast<Player*>(entity_target)->GetPVPImmune()))
			return false;

		return PVP::IsHostile(attacker, entity_target);
	} else {
		return false;
	}
}

bool PVP::IsHostile(Player* attacker, Spawn* target) {
	if (PVP::IsEnabled(attacker->GetZone())) {
		Entity* entity_target = static_cast<Entity*>(target);

		if (target->IsPet())
			entity_target = static_cast<NPC*>(target)->GetOwner();

		// Alignment of 0 is currently "neutral"
		// Not attackable by either - only meant for GM, perhaps.
		if (attacker->GetAlignment() == 0 || entity_target->GetAlignment() == 0)
			return false;

		return (attacker->GetAlignment() != entity_target->GetAlignment());
	} else {
		return false;
	}
}

string PVP::GetRank(Player* player)
{
	if (!PVP::IsEnabled())
		return "";

	int16 fame = player->GetFame();

	for (auto kv = PVP_TITLES.rbegin(); kv != PVP_TITLES.rend(); ++kv)
		if (fame >= kv->first)
			return kv->second;

	return "";
}

int PVP::GetRankIndex(Player* player) {
	const char* rank = PVP::GetRank(player).c_str();

	if (strcmp(rank, "Dreadnaught") == 0) {
		return 5;
	} else if (strcmp(rank, "Champion") == 0) {
		return 4;
	} else if (strcmp(rank, "Destroyer") == 0) {
		return 3;
	} else if (strcmp(rank, "Slayer") == 0) {
		return 2;
	} else if (strcmp(rank, "Hunter") == 0) {
		return 1;
	} else {
		return 0;
	}
}

void PVP::HandleFameChange(Player* attacker, Player* victim) {
	if (attacker == victim) return;

	ZoneServer* zone = attacker->GetZone();
	int dead_rank = PVP::GetRankIndex(victim);
	int killer_rank = PVP::GetRankIndex(attacker);
	int ranking_difference = dead_rank - killer_rank;

	if (ranking_difference >= -2 && ranking_difference <= 2) {
		victim->SetFame(victim->GetFame() - PVP_FAME_AMOUNT_PER_KILL);
		attacker->SetFame(attacker->GetFame() + PVP_FAME_AMOUNT_PER_KILL);

		vector<Player*> fame_recipients;


		zone->GetClientBySpawn(victim)->SimpleMessage(CHANNEL_COLOR_YELLOW, "Your death has decreased your fame.");
		zone->GetClientBySpawn(attacker)->SimpleMessage(CHANNEL_COLOR_YELLOW, "Your victory has increased your fame.");
	}

	if (dead_rank != PVP::GetRankIndex(victim)) {
		int rank = PVP::GetRankIndex(victim);
		if (rank == 0) {
			zone->GetClientBySpawn(victim)->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are no longer ranked.");
			zone->GetClientBySpawn(victim)->SendPopupMessage(10, "Your are no longer ranked.", "", 2, 0xFF, 0xFF, 0xFF);
		} else if (rank < dead_rank) {
			char message[37];
			sprintf(message, "Your rank has dropped to %s.", PVP::GetRank(victim).c_str());
			zone->GetClientBySpawn(victim)->SimpleMessage(CHANNEL_COLOR_YELLOW, message);
			zone->GetClientBySpawn(victim)->SendPopupMessage(10, message, "", 2, 0xFF, 0xFF, 0xFF);
		}
		zone->GetClientBySpawn(victim)->SendTitleUpdate();
	}

	if (PVP::GetRankIndex(attacker) > killer_rank) {
		char message[42];
		sprintf(message, "You have obtained the rank of %s.", PVP::GetRank(attacker).c_str());
		zone->GetClientBySpawn(attacker)->SimpleMessage(CHANNEL_COLOR_YELLOW, message);
		zone->GetClientBySpawn(attacker)->SendPopupMessage(10, message, "", 2, 0xFF, 0xFF, 0xFF);
		zone->GetClientBySpawn(attacker)->SendTitleUpdate();
	}

	victim->UpdatePlayerStatistic(STAT_PLAYER_TOTAL_PVP_DEATHS, 1);
	attacker->UpdatePlayerStatistic(STAT_PLAYER_TOTAL_PVP_KILLS, 1);
}

bool PVP::IsEnabled(ZoneServer* zone)
{
	if (zone && rule_manager.HasZoneRuleSet(zone->GetZoneID())) {
		return PVP::IsEnabled() && rule_manager.GetZoneRule(zone->GetZoneID(), R_PVP, AllowPVP)->GetBool();
	} else {
		return rule_manager.GetGlobalRule(R_PVP, AllowPVP)->GetBool();
	}
}
