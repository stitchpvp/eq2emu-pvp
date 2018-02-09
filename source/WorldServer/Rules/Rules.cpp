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

#include <assert.h>
#include "../../common/debug.h"
#include "../../common/Log.h"
#include "../../common/database.h"
#include "Rules.h"

extern RuleManager rule_manager;

Rule::Rule() {
	category = 0;
	type = 0;
	strncpy(value, "", sizeof(value));
	strncpy(combined, "NONE", sizeof(combined));
}

Rule::Rule(int32 category, int32 type, const char *value, const char *combined) {
	this->category = category;
	this->type = type;
	strncpy(this->value, value, sizeof(this->value));
	strncpy(this->combined, combined, sizeof(this->combined));
}

Rule::Rule (Rule *rule_in) {
	category = rule_in->GetCategory();
	type = rule_in->GetType();
	strncpy(value, rule_in->GetValue(), sizeof(value));
	strncpy(combined, rule_in->GetCombined(), sizeof(combined));
}

Rule::~Rule() {
}

RuleSet::RuleSet() {
	id = 0;
	memset(name, 0, sizeof(name));
	m_rules.SetName("RuleSet::rules");
}

RuleSet::RuleSet(RuleSet *in_rule_set) {
	assert(in_rule_set);

	map<int32, map<int32, Rule *> > * in_rules = in_rule_set->GetRules();
	map<int32, map<int32, Rule *> >::iterator itr;
	map<int32, Rule *>::iterator itr2;
	Rule * rule;
		
	m_rules.SetName("RuleSet::rules");
	id = in_rule_set->GetID();
	strncpy(name, in_rule_set->GetName(), sizeof(name));
	for (itr = in_rules->begin(); itr != in_rules->end(); itr++) {
		for (itr2 = itr->second.begin(); itr2 != itr->second.end(); itr2++) {
			rule = itr2->second;
			rules[rule->GetCategory()][rule->GetType()] = new Rule(rule);
		}
	}
}

RuleSet::~RuleSet() {
	ClearRules();
}

void RuleSet::CopyRulesInto(RuleSet *in_rule_set) {
	assert(in_rule_set);

	map<int32, map<int32, Rule *> > * in_rules = in_rule_set->GetRules();
	map<int32, map<int32, Rule *> >::iterator itr;
	map<int32, Rule *>::iterator itr2;
	Rule * rule;
	
	ClearRules();
	m_rules.writelock(__FUNCTION__, __LINE__);
	for (itr = in_rules->begin(); itr != in_rules->end(); itr++) {
		for (itr2 = itr->second.begin(); itr2 != itr->second.end(); itr2++) {
			rule = itr2->second;
			rules[rule->GetCategory()][rule->GetType()] = new Rule(rule);
		}
	}
	m_rules.releasewritelock(__FUNCTION__, __LINE__);
}

void RuleSet::AddRule(Rule *rule) {
	int32 category, type;

	assert(rule);

	category = rule->GetCategory();
	type = rule->GetType();
	m_rules.writelock(__FUNCTION__, __LINE__);
	if (rules[category].count(type) == 0)
		rules[category][type] = rule;
	else
		rules[category][type]->SetValue(rule->GetValue());
	m_rules.releasewritelock(__FUNCTION__, __LINE__);
}

Rule * RuleSet::GetRule(int32 category, int32 type) {
	Rule *ret = 0;

	m_rules.readlock(__FUNCTION__, __LINE__);
	if (rules[category].count(type) > 0)
		ret = rules[category][type];
	m_rules.releasereadlock(__FUNCTION__, __LINE__);

	if (!ret)
		ret = rule_manager.GetBlankRule();

	LogWrite(RULESYS__DEBUG, 5, "Rules", "Rule: %s, Value: %s", ret->GetCombined(), ret->GetValue());
	return ret;
}

Rule * RuleSet::GetRule(const char *category, const char *type) {
	map<int32, map<int32, Rule *> >::iterator itr;
	map<int32, Rule *>::iterator itr2;
	char combined[256];
	Rule *ret = 0;

	snprintf(combined, sizeof(combined), "%s:%s", category, type);
	// Zero terminate ([max - 1] = 0) to prevent a warning/error
	combined[255] = 0;

	m_rules.readlock(__FUNCTION__, __LINE__);
	for (itr = rules.begin(); itr != rules.end(); itr++) {
		for (itr2 = itr->second.begin(); itr2 != itr->second.end(); itr2++) {
			if (!strcmp(itr2->second->GetCombined(), combined)) {
				ret = itr2->second;
				break;
			}
		}
	}
	m_rules.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}

void RuleSet::ClearRules() {
	map<int32, map<int32, Rule *> >::iterator itr;
	map<int32, Rule *>::iterator itr2;

	m_rules.writelock(__FUNCTION__, __LINE__);
	for (itr = rules.begin(); itr != rules.end(); itr++) {
		for (itr2 = itr->second.begin(); itr2 != itr->second.end(); itr2++)
			safe_delete(itr2->second);
	}
	rules.clear();
	m_rules.releasewritelock(__FUNCTION__, __LINE__);
}

RuleManager::RuleManager() {
	m_rule_sets.SetName("RuleManager::rule_sets");
	m_global_rule_set.SetName("RuleManager::global_rule_set");
	m_zone_rule_sets.SetName("RuleManager::zone_rule_sets");

	#define RULE_INIT(category, type, value) rules[category][type] = new Rule(category, type, value, #category ":" #type)

	/* CLIENT */
	RULE_INIT(R_Client, ShowWelcomeScreen, "0");
	RULE_INIT(R_Client, GroupSpellsTimer, "1000");

	/* FACTION */
	RULE_INIT(R_Faction, AllowFactionBasedCombat, "1");

	/* GUILD */
	RULE_INIT(R_Guild, MaxLevel, "50");
	RULE_INIT(R_Guild, MaxPlayers, "-1");

	/* PLAYER */
	RULE_INIT(R_Player, MaxLevel, "50");
	RULE_INIT(R_Player, MaxLevelOverrideStatus, "100");
	RULE_INIT(R_Player, VitalityAmount, ".5");
	RULE_INIT(R_Player, VitalityFrequency, "3600");
	RULE_INIT(R_Player, XPMultiplier, "1.0");
	RULE_INIT(R_Player, TSXPMultiplier, "1.0");
	RULE_INIT(R_Player, MaxAA, "320");
	RULE_INIT(R_Player, MaxClassAA, "100");
	RULE_INIT(R_Player, MaxSubclassAA, "100");
	RULE_INIT(R_Player, MaxShadowsAA, "70");
	RULE_INIT(R_Player, MaxHeroicAA, "50");
	RULE_INIT(R_Player, MaxTradeskillAA, "40");
	RULE_INIT(R_Player, MaxPrestigeAA, "25");
	RULE_INIT(R_Player, MaxTradeskillPrestigeAA, "25");
	RULE_INIT(R_Player, MinLastNameLevel, "20");
	RULE_INIT(R_Player, MaxLastNameLength, "20");
	RULE_INIT(R_Player, MinLastNameLength, "4");

	/* PVP */
	RULE_INIT(R_PVP, AllowPVP, "0");

	/* SPAWN */
	RULE_INIT(R_Spawn, SpeedMultiplier, "300"); // note: this value was 1280 until 6/1/2009, then was 600 til Sep 2009, when it became 300...?
	RULE_INIT(R_Spawn, SpeedRatio, "0");		// was 1280/7.5 and 600/7.5 until it became 300.

	/* TIMER */
	
	/* UI */
	RULE_INIT(R_UI, MaxWhoResults, "20");
	RULE_INIT(R_UI, MaxWhoOverrideStatus, "200");

	/* WORLD */
	RULE_INIT(R_World, DefaultStartingZoneID, "1");
	RULE_INIT(R_World, EnablePOIDiscovery, "0");
	RULE_INIT(R_World, GamblingTokenItemID, "2");
	RULE_INIT(R_World, GuildAutoJoin, "0");
	RULE_INIT(R_World, GuildAutoJoinID, "1");
	RULE_INIT(R_World, GuildAutoJoinDefaultRankID, "7");
	RULE_INIT(R_World, MaxPlayers, "-1");
	RULE_INIT(R_World, MaxPlayersOverrideStatus, "100");
	RULE_INIT(R_World, ServerLocked, "0");
	RULE_INIT(R_World, ServerLockedOverrideStatus, "10");
	RULE_INIT(R_World, SyncZonesWithLogin, "1");
	RULE_INIT(R_World, SyncEquipWithLogin, "1");
	RULE_INIT(R_World, UseBannedIPsTable, "0");
	RULE_INIT(R_World, LinkDeadTimer, "120000");					// default: 2 minutes
	RULE_INIT(R_World, RemoveDisconnectedClientsTimer, "30000");	// default: 30 seconds
	RULE_INIT(R_World, PlayerCampTimer, "20");						// default: 20 seconds
	RULE_INIT(R_World, GMCampTimer, "1");							// default: 1 second
	RULE_INIT(R_World, AutoAdminPlayers, "0");						// default: No
	RULE_INIT(R_World, AutoAdminGMs, "0");							// default: No
	RULE_INIT(R_World, AutoAdminStatusValue, "10");					// default: 10 (CSR)
	RULE_INIT(R_World, DuskTime, "20:00");							// default: 8pm
	RULE_INIT(R_World, DawnTime, "8:00");							// default: 8am
	RULE_INIT(R_World, ThreadedLoad, "0");							// default: no threaded loading
	RULE_INIT(R_World, TradeskillSuccessChance, "87.0");			// default: 87% chance of success while crafting
	RULE_INIT(R_World, TradeskillCritSuccessChance, "2.0");			// default: 2% chance of critical success while crafting
	RULE_INIT(R_World, TradeskillFailChance, "10.0");				// default: 10% chance of failure while crafting
	RULE_INIT(R_World, TradeskillCritFailChance, "1.0");			// default: 1% chance of critical failure while crafting
	RULE_INIT(R_World, TradeskillEventChance, "15.0");				// default: 15% chance of a tradeskill event while crafting
	RULE_INIT(R_World, EditorURL, "www.eq2emulator.net");			// default: www.eq2emulator.net
	RULE_INIT(R_World, EditorIncludeID, "0");						// default: 0 (0 = disabled, 1 = enabled)
	RULE_INIT(R_World, EditorOfficialServer, "0");					// default: 0 (0 = disabled, 1 = enabled)
	RULE_INIT(R_World, IRCEnabled, "0");							// default: 0 (0 = disabled, 1 = enabled)
	RULE_INIT(R_World, IRCGlobalEnabled, "0");						// default: 0 (0 = disabled, 1 = enabled)
	RULE_INIT(R_World, IRCAddress, "irc.eq2emulator.net");			// default: irc.eq2emulator.net
	RULE_INIT(R_World, IRCPort, "6667");							// default: 6667
	RULE_INIT(R_World, IRCChan, "#EQ2Emu");							// default: #EQ2Emu
	RULE_INIT(R_World, SavePaperdollImage, "1");					// default: true
	RULE_INIT(R_World, SaveHeadshotImage, "1");						// default: true
	RULE_INIT(R_World, SendPaperdollImagesToLogin, "1");			// default: true
	RULE_INIT(R_World, TreasureChestDisabled, "0");					// default: false
	//INSERT INTO `ruleset_details`(`id`, `ruleset_id`, `rule_category`, `rule_type`, `rule_value`, `description`) VALUES (NULL, '1', 'R_World', '', '', '')
	
	/* ZONE */
	RULE_INIT(R_Zone, MaxPlayers, "100");
	RULE_INIT(R_Zone, MinZoneLevelOverrideStatus, "1");
	RULE_INIT(R_Zone, MinZoneAccessOverrideStatus, "100");
	RULE_INIT(R_Zone, WeatherEnabled, "1");							// default: 1 (0 = disabled, 1 = enabled)
	RULE_INIT(R_Zone, WeatherType, "0");							// default: 1 (0 = normal, 1 = dynamic, 2 = random, 3 = chaotic)
	RULE_INIT(R_Zone, MinWeatherSeverity, "0.0");					// default: 0.0 or no weather
	RULE_INIT(R_Zone, MaxWeatherSeverity, "1.0");					// default: 1.0 or hard rain (range 0.0 - 1.0, rain starts at 0.75)
	RULE_INIT(R_Zone, WeatherChangeFrequency, "300");				// default: 5 minutes
	RULE_INIT(R_Zone, WeatherChangePerInterval, "0.02");			// default: 0.02 (slight changes)
	RULE_INIT(R_Zone, WeatherChangeChance, "20");					// default: 20% (in whole percents)
	RULE_INIT(R_Zone, WeatherDynamicMaxOffset, "0.08");				// default: 0.08 - dynamic weather changes can only change this max amount
	RULE_INIT(R_Zone, SpawnUpdateTimer, "50");						// default: 50ms - how often to check for spawn update sends
	RULE_INIT(R_Zone, CheckAttackNPC, "2000");                      // default: 2 seconds, how often to for NPCs to attack eachother
	RULE_INIT(R_Zone, CheckAttackPlayer, "2000");                   // default: 2 seconds, how often to check for NPCs to attack players
	RULE_INIT(R_Zone, HOTime, "10.0");								// default: 10 seconds, time to complete the HO wheel before it expires

	/* ZONE TIMERS */
	RULE_INIT(R_Zone, RegenTimer, "6000");
	RULE_INIT(R_Zone, ClientSaveTimer, "60000");
	RULE_INIT(R_Zone, DefaultZoneShutdownTimer, "300000");
	RULE_INIT(R_Zone, WeatherTimer, "60000");						// default: 1 minute
	RULE_INIT(R_Zone, SpawnDeleteTimer, "30000");					// default: 30 seconds, how long a spawn pointer is held onto after being removed from the world before deleting it
	#undef RULE_INIT
}

RuleManager::~RuleManager() {
	map<int32, map<int32, Rule *> >::iterator itr;
	map<int32, Rule *>::iterator itr2;

	for (itr = rules.begin(); itr != rules.end(); itr++) {
		for (itr2 = itr->second.begin(); itr2 != itr->second.end(); itr2++)
			safe_delete(itr2->second);
	}

	ClearRuleSets();
	ClearZoneRuleSets();
}

void RuleManager::LoadCodedDefaultsIntoRuleSet(RuleSet *rule_set) {
	map<int32, map<int32, Rule *> >::iterator itr;
	map<int32, Rule *>::iterator itr2;

	assert(rule_set);

	for (itr = rules.begin(); itr != rules.end(); itr++) {
		for (itr2 = itr->second.begin(); itr2 != itr->second.end(); itr2++)
			rule_set->AddRule(new Rule(itr2->second));
	}
}

bool RuleManager::AddRuleSet(RuleSet *rule_set) {
	bool ret = false;
	int32 id;

	assert(rule_set);

	id = rule_set->GetID();
	m_rule_sets.writelock(__FUNCTION__, __LINE__);
	if (rule_sets.count(id) == 0) {
		rule_sets[id] = rule_set;
		ret = true;
	}
	m_rule_sets.releasewritelock(__FUNCTION__, __LINE__);

	return ret;
}

int32 RuleManager::GetNumRuleSets() {
	int32 ret;

	m_rule_sets.readlock(__FUNCTION__, __LINE__);
	ret = rule_sets.size();
	m_rule_sets.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}

void RuleManager::ClearRuleSets() {
	map<int32, RuleSet *>::iterator itr;

	m_rule_sets.writelock(__FUNCTION__, __LINE__);
	for (itr = rule_sets.begin(); itr != rule_sets.end(); itr++)
		safe_delete(itr->second);
	rule_sets.clear();
	m_rule_sets.releasewritelock(__FUNCTION__, __LINE__);
}

bool RuleManager::SetGlobalRuleSet(int32 rule_set_id) {
	if (rule_sets.count(rule_set_id) == 0)
		return false;

	global_rule_set.CopyRulesInto(rule_sets[rule_set_id]);
	return true;
}

Rule * RuleManager::GetGlobalRule(int32 category, int32 type) {
	return global_rule_set.GetRule(category, type);
}

bool RuleManager::SetZoneRuleSet(int32 zone_id, int32 rule_set_id) {
	bool ret = true;
	RuleSet *rule_set;

	m_rule_sets.readlock(__FUNCTION__, __LINE__);
	if (rule_sets.count(rule_set_id) == 0)
		ret = false;

	rule_set = rule_sets[rule_set_id];
	if (ret) {
		m_zone_rule_sets.writelock(__FUNCTION__, __LINE__);
		zone_rule_sets[zone_id] = rule_set;
		m_zone_rule_sets.releasewritelock(__FUNCTION__, __LINE__);
	}
	m_rule_sets.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}

Rule * RuleManager::GetZoneRule(int32 zone_id, int32 category, int32 type) {
	Rule *ret = 0;

	/* we never want to return null so MAKE SURE the rule exists. if this assertion fails then the server admin must fix the problem */
	assert(rules.count(category) > 0);
	assert(rules[category].count(type) > 0);

	/* first try to get the zone rule */
	m_zone_rule_sets.readlock(__FUNCTION__, __LINE__);
	if (zone_rule_sets.count(zone_id) > 0)
		ret = zone_rule_sets[zone_id]->GetRule(category, type);
	m_zone_rule_sets.releasereadlock(__FUNCTION__, __LINE__);

	return ret ? ret : rules[category][type];
}

void RuleManager::ClearZoneRuleSets() {
	m_zone_rule_sets.writelock(__FUNCTION__, __LINE__);
	zone_rule_sets.clear();
	m_zone_rule_sets.releasewritelock(__FUNCTION__, __LINE__);
}

bool RuleManager::HasZoneRuleSet(int32 zone_id)
{
	m_zone_rule_sets.readlock(__FUNCTION__, __LINE__);
	if (zone_rule_sets.count(zone_id)) {
		m_zone_rule_sets.releasereadlock(__FUNCTION__, __LINE__);
		return true;
	} else {
		m_zone_rule_sets.releasereadlock(__FUNCTION__, __LINE__);
		return false;
	}}
