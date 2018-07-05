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
#include "SpellProcess.h"
#include "../common/Log.h"
#include "Tradeskills/Tradeskills.h"
#include "ClientPacketFunctions.h"
#include "PVP.h"

extern MasterSpellList master_spell_list;
extern MasterSkillList master_skill_list;
extern ConfigReader configReader;
extern LuaInterface* lua_interface;
extern Commands commands;
extern World world;

SpellProcess::SpellProcess(){
	last_checked_time = 0;
	MSpellProcess.SetName("SpellProcess::MSpellProcess");
	MRemoveTargetList.SetName("SpellProcess::MRemoveTargetList");
	MSoloHO.SetName("SpellProcess::m_soloHO");
	MGroupHO.SetName("SpellProcess:m_groupHO");
	MSpellCancelList.SetName("SpellProcess::SpellCancelList");
}

SpellProcess::~SpellProcess(){
	RemoveAllSpells();
}

void SpellProcess::RemoveAllSpells(){
	MSpellProcess.lock();	
	ClearSpellScriptTimerList();

	{
		lock_guard<mutex> guard(active_spells_mutex);

		for (const auto lua_spell : active_spells) {
			DeleteCasterSpell(lua_spell);
		}

		active_spells.clear();
	}

	{
		lock_guard<mutex> guard(interrupt_list_mutex);

		for (auto interrupt : interrupt_list) {
			CheckInterrupt(interrupt);
			safe_delete(interrupt);
		}

		interrupt_list.clear();
	}

	{
		lock_guard<mutex> guard(cast_timers_mutex);

		for (auto cast_timer : cast_timers) {
			safe_delete(cast_timer->timer);
			safe_delete(cast_timer);
		}

		cast_timers.clear();
	}

	MRecastTimers.writelock(__FUNCTION__, __LINE__);
	for (auto recast_timer : recast_timers) {
		safe_delete(recast_timer->timer);
		safe_delete(recast_timer);
	}
	recast_timers.clear();
	MRecastTimers.releasewritelock(__FUNCTION__, __LINE__);

	MRemoveTargetList.writelock(__FUNCTION__, __LINE__);
	remove_target_list.clear();
	MRemoveTargetList.releasewritelock(__FUNCTION__, __LINE__);

	map<shared_ptr<Client>, HeroicOP*>::iterator solo_ho_itr;
	MSoloHO.writelock(__FUNCTION__, __LINE__);
	for (solo_ho_itr = m_soloHO.begin(); solo_ho_itr != m_soloHO.end(); solo_ho_itr++)
		safe_delete(solo_ho_itr->second);
	m_soloHO.clear();
	MSoloHO.releasewritelock(__FUNCTION__, __LINE__);

	map<int32, HeroicOP*>::iterator group_ho_itr;
	MGroupHO.writelock(__FUNCTION__, __LINE__);
	for (group_ho_itr = m_groupHO.begin(); group_ho_itr != m_groupHO.end(); group_ho_itr++)
		safe_delete(group_ho_itr->second);
	m_groupHO.clear();
	MGroupHO.releasewritelock(__FUNCTION__, __LINE__);

	{
		lock_guard<mutex> guard(spell_queue_mutex);
		spell_queue.clear();
	}
	
	MSpellCancelList.writelock(__FUNCTION__, __LINE__);
	SpellCancelList.clear();
	MSpellCancelList.releasewritelock(__FUNCTION__, __LINE__);

	MSpellProcess.unlock();
}

void SpellProcess::Process(){
	if(last_checked_time > Timer::GetCurrentTime2())
		return;

	last_checked_time = Timer::GetCurrentTime2() + 50;

	MSpellProcess.lock();
	CheckSpellScriptTimers();

	{
		unique_lock<mutex> guard(active_spells_mutex);

		if (active_spells.size() > 0) {	
			auto spell_itr = begin(active_spells);

			while (spell_itr != end(active_spells)) {
				auto spell = (*spell_itr);

				if (spell->timer.Check()) {
					spell->num_calls++;

					// ProcessSpell(spell, flase) will atempt to call the tick() function in the lua script
					// if there is no tick function it will return false, this will cause the server to crash in the event
					// of a spell that has a duration but is not a "until canceled" spell or a spell with a tick (tradeskill buffs)
					// to counter this check to see if the spell has a call_frequency > 0 before we call ProcessSpell()
					if (spell->spell->GetSpellData()->call_frequency > 0) {
						spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
						if (spell->targets.size() > 0) {
							ZoneServer* zone = spell->caster->GetZone();

							for (int32 i = 0; i < spell->targets.size(); i++) {
								Spawn* target = zone->GetSpawnByID(spell->targets[i]);

								guard.unlock();
								if (!ProcessSpell(spell, target, false)) {
									guard.lock();
									break;
								}
								guard.lock();
							}
						}
						spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);
					}

					if (!spell->spell->GetSpellData()->duration_until_cancel && (spell->timer.GetDuration() * spell->num_calls) > spell->spell->GetSpellData()->duration1 * 100) {
						DeleteCasterSpell(spell);
					}
				}

				spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
				if (spell->targets.size() == 0) {
					spell_itr = active_spells.erase(spell_itr);

					if (spell->caster) {
						spell->caster->RemoveMaintainedSpell(spell);

						if (spell->spell->GetSpellData()->cast_type == SPELL_CAST_TYPE_TOGGLE) {
							float recast = spell->spell->GetModifiedRecast(spell->caster);

							UnlockAllSpells(spell->caster->GetZone()->GetClientBySpawn(spell->caster));

							if (recast > 0) {
								CheckRecast(spell->spell, spell->caster, recast);

								if (spell->caster && spell->caster->IsPlayer()) {
									SendSpellBookUpdate(spell->caster->GetZone()->GetClientBySpawn(spell->caster));
								}
							} else {
								UnlockSpell(spell->caster->GetZone()->GetClientBySpawn(spell->caster), spell->spell);
							}
						}
					}
				} else {
					++spell_itr;
				}
				spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);
			}
		}
	}

	CheckRemoveTargetFromSpell();

	MSpellCancelList.writelock(__FUNCTION__, __LINE__);
	if (SpellCancelList.size() > 0){
		for (auto spell : SpellCancelList) {
			DeleteCasterSpell(spell);
		}
		SpellCancelList.clear();
	}
	MSpellCancelList.releasewritelock(__FUNCTION__, __LINE__);

	{
		lock_guard<mutex> guard(interrupt_list_mutex);

		if (interrupt_list.size() > 0) {
			for (auto interrupt : interrupt_list) {
				CheckInterrupt(interrupt);
				safe_delete(interrupt);
			}

			interrupt_list.clear();
		}
	}

	vector<CastTimer*> finished_casts;

	{
		lock_guard<mutex> guard(cast_timers_mutex);

		if (cast_timers.size() > 0) {
			for (const auto cast_timer : cast_timers) {
				if (cast_timer) {
					if (cast_timer->timer->Check(false) || cast_timer->delete_timer) {
						finished_casts.push_back(cast_timer);
					}
				}
			}
		}
	}

	for (auto cast_timer : finished_casts) {
		if (!cast_timer->delete_timer) {
			if (cast_timer->spell) {
				shared_ptr<Client> client = cast_timer->zone->GetClientBySpawn(cast_timer->spell->caster);

				if (client) {
					PacketStruct* packet = configReader.getStruct("WS_FinishCastSpell", client->GetVersion());

					if (packet) {
						packet->setMediumStringByName("spell_name", cast_timer->spell->spell->GetSpellData()->name.data.c_str());
						client->QueuePacket(packet->serialize());
						safe_delete(packet);
					}
				}

				if (cast_timer->spell && cast_timer->spell->caster) {
					cast_timer->spell->caster->IsCasting(false);
				}

				CastProcessedSpell(cast_timer->spell);
			} else if (cast_timer->entity_command && !cast_timer->delete_timer) {
				CastProcessedEntityCommand(cast_timer->entity_command, cast_timer->caster);
			}
		}

		cast_timers.erase(remove(cast_timers.begin(), cast_timers.end(), cast_timer), cast_timers.end());

		safe_delete(cast_timer->timer);
		safe_delete(cast_timer);
	}

	MRecastTimers.writelock(__FUNCTION__, __LINE__);
	if (recast_timers.size() > 0){
		auto itr = recast_timers.begin();

		while (itr != recast_timers.end()) {
			RecastTimer* recast_timer = (*itr);

			if (recast_timer->timer->Check(false)) {
				UnlockSpell(recast_timer->client, recast_timer->spell);

				itr = recast_timers.erase(itr);

				safe_delete(recast_timer->timer);
				safe_delete(recast_timer);
			} else {
				++itr;
			}
		}
	}
	MRecastTimers.releasewritelock(__FUNCTION__, __LINE__);

	map<Entity*, Spell*> to_cast;

	{
		lock_guard<mutex> guard(spell_queue_mutex);

		if (spell_queue.size() > 0) {
			for (const auto& queued_spell : spell_queue) {
				Entity* caster = queued_spell.first;
				Spell* spell = queued_spell.second;

				if (!caster->IsCasting() && IsReady(spell, caster)) {
					to_cast.insert(pair<Entity*, Spell*>(caster, spell));
				}
			}
		}
	}

	for (const auto& cast_spell : to_cast) {
		Entity* caster = cast_spell.first;
		Spell* spell = cast_spell.second;

		RemoveSpellFromQueue(spell, caster);
		ProcessSpell(caster->GetZone(), spell, caster, caster->GetTarget());
	}

	// Check solo HO timers
	MSoloHO.writelock(__FUNCTION__, __LINE__);
	if (m_soloHO.size() > 0) {
		auto itr = m_soloHO.begin();

		while (itr != m_soloHO.end()) {
			if (itr->second->GetWheel() && Timer::GetCurrentTime2() >= (itr->second->GetStartTime() + (itr->second->GetTotalTime() * 1000))) {
				itr->second->SetComplete(1);
				ClientPacketFunctions::SendHeroicOPUpdate(itr->first, itr->second);
				safe_delete(itr->second);
				itr = m_soloHO.erase(itr);
			} else {
				itr++;
			}
		}
	}
	MSoloHO.releasewritelock(__FUNCTION__, __LINE__);

	// Check group HO timers
	MGroupHO.writelock(__FUNCTION__, __LINE__);
	if (m_groupHO.size() > 0) {
		auto itr = m_groupHO.begin();

		while (itr != m_groupHO.end()) {
			if (itr->second->GetWheel() && Timer::GetCurrentTime2() >= (itr->second->GetStartTime() + (itr->second->GetTotalTime() * 1000))) {
				itr->second->SetComplete(1);

				world.GetGroupManager()->GroupLock(__FUNCTION__, __LINE__);
				deque<GroupMemberInfo*>* members = world.GetGroupManager()->GetGroupMembers(itr->first);
				for (auto group_member : *members) {
					if (group_member->client) {
						ClientPacketFunctions::SendHeroicOPUpdate(group_member->client, itr->second);
					}
				}
				world.GetGroupManager()->ReleaseGroupLock(__FUNCTION__, __LINE__);

				safe_delete(itr->second);
				itr = m_groupHO.erase(itr);
			} else {
				itr++;
			}
		}
	}
	MGroupHO.releasewritelock(__FUNCTION__, __LINE__);

	MSpellProcess.unlock();
}

bool SpellProcess::IsReady(Spell* spell, Entity* caster) {
	if (caster->IsCasting())
		return false;

	if (spell->GetSpellData()->cast_type == SPELL_CAST_TYPE_TOGGLE && caster->IsPlayer() && static_cast<Player*>(caster)->HasLinkedSpellEffect(spell))
		return false;

	if (OnCooldown(spell, caster))
		return false;

	return true;
}

bool SpellProcess::OnCooldown(Spell* spell, Entity* caster) {
	bool ret = false;

	MRecastTimers.readlock(__FUNCTION__, __LINE__);
	for (const auto recast_timer : recast_timers) {
		if (recast_timer->spell == spell && recast_timer->caster == caster) {
			ret = true;
			break;
		}
	}
	MRecastTimers.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}

void SpellProcess::CheckRecast(Spell* spell, Entity* caster, float timer_override, bool check_linked_timers) {
	if (spell && caster) {
		if (timer_override > 0) {
			auto timer = new RecastTimer;
			timer->caster = caster;
			timer->spell = spell;
			timer->timer = new Timer((int32)(timer_override * 1000));

			if (caster->IsPlayer()) {
				timer->client = caster->GetZone()->GetClientBySpawn(caster);
			} else {
				timer->client = nullptr;
			}

			MRecastTimers.writelock(__FUNCTION__, __LINE__);
			recast_timers.push_back(timer);
			MRecastTimers.releasewritelock(__FUNCTION__, __LINE__);
		}

		if (caster->IsPlayer()) {
			((Player*)caster)->LockSpell(spell, timer_override * 10);

			if (check_linked_timers && spell->GetSpellData()->linked_timer > 0) {
				vector<Spell*> linkedSpells = static_cast<Player*>(caster)->GetSpellBookSpellsByTimer(spell->GetSpellData()->linked_timer);

				for (auto spell2 : linkedSpells) {
					if (spell2 && spell2 != spell) {
						CheckRecast(spell2, caster, timer_override, false);
					}
				}
			}
		}
	}
}
void SpellProcess::CheckInterrupt(InterruptStruct* interrupt){
	if (!interrupt || !interrupt->interrupted || !interrupt->interrupted->IsEntity())
		return;

	auto entity = static_cast<Entity*>(interrupt->interrupted);
	shared_ptr<Client> client = entity->GetZone()->GetClientBySpawn(entity);

	if (client) {
		auto lua_spell = GetLuaSpell(entity);
		if (lua_spell) {
			SendFinishedCast(lua_spell.get(), client);
		}
	}

	RemoveSpellTimersFromSpawn(entity, false);
	entity->IsCasting(false);
	entity->GetZone()->SendInterruptPacket(entity, interrupt->spell.get());

	if (interrupt->error_code > 0) {
		entity->GetZone()->SendSpellFailedPacket(client, interrupt->error_code);
	}
}

bool SpellProcess::DeleteCasterSpell(Spawn* caster, Spell* spell){

	auto ret = false;

	{
		lock_guard<mutex> guard(active_spells_mutex);

		if (caster && spell && active_spells.size() > 0) {
			for (auto luaspell : active_spells) {
				if (luaspell->spell == spell && luaspell->caster == caster) {
					ret = DeleteCasterSpell(luaspell);
					break;
				}
			}
		}
	}

	return ret;
}

bool SpellProcess::DeleteCasterSpell(shared_ptr<LuaSpell> spell, bool call_remove_function) {
	auto ret = false;

	if (spell) {
		auto zone = spell->caster->GetZone();
		
		spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
		for (const auto& target_id : spell->targets) {
			auto target = zone->GetSpawnByID(target_id);

			if (target) {
				RemoveTargetFromSpell(spell, target);

				ret = true;
			}
		}
		spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);
	}

	return ret;
}

bool SpellProcess::ProcessSpell(shared_ptr<LuaSpell> spell, Spawn* target, bool first_cast, const char* function, SpellScriptTimer* timer) {
	bool ret = false;

	if (lua_interface && !spell->interrupted) {
		lua_interface->AddSpawnPointers(spell.get(), first_cast, false, function, timer);
		
		if (target)
			lua_interface->SetSpawnValue(spell->state, target);

		vector<LUAData> data = spell->spell->GetScaledLUAData(spell->caster->GetLevel());
		for (const auto& datum : data) {
			switch (datum.type) {
				case 0: {
							lua_interface->SetSInt32Value(spell->state, datum.int_value);
							break;
						}

				case 1: {
							lua_interface->SetFloatValue(spell->state, datum.float_value);
							break;
						}

				case 2: {
							lua_interface->SetBooleanValue(spell->state, datum.bool_value);
							break;
						}

				case 3: {
							lua_interface->SetStringValue(spell->state, datum.string_value.c_str());
							break;
						}

				default: {
							 LogWrite(SPELL__ERROR, 0, "Spell", "Error: Unknown LUA Type '%i' in Entity::CastProc for Spell '%s'", (int)datum.type, spell->spell->GetName());
							 return false;
						 }
			}
		}
		ret = lua_interface->CallSpellProcess(spell, 2 + data.size());
	}

	return ret;
}

bool SpellProcess::CastPassives(Spell* spell, Entity* caster, bool remove) {
	shared_ptr<LuaSpell> lua_spell = nullptr;

	if(lua_interface)
		lua_spell = lua_interface->GetSpell(spell->GetSpellData()->lua_script.c_str());

	if (!lua_spell) {
		string lua_script = spell->GetSpellData()->lua_script;
		lua_script.append(".lua");
		lua_spell = nullptr;

		if (lua_interface)
			lua_spell = lua_interface->GetSpell(lua_script.c_str());

		if (!lua_spell) {
			LogWrite(SPELL__ERROR, 0, "Spell", "Failed to get a LuaSpell for %s (%u)", spell->GetName(), spell->GetSpellID());
			return false;
		} else {
			spell->GetSpellData()->lua_script = lua_script;
		}
	}

	lua_spell->caster = caster;
	lua_spell->spell = spell;
	lua_spell->initial_target = caster->GetID();

	GetSpellTargets(lua_spell.get());

	if (!remove)
		return CastProcessedSpell(lua_spell, true);

	return true;
}

void SpellProcess::SendStartCast(LuaSpell* spell, const shared_ptr<Client>& client){
	if(client) {
		PacketStruct* packet = configReader.getStruct("WS_StartCastSpell", client->GetVersion());

		if(packet) {
			packet->setDataByName("cast_time", spell->spell->GetModifiedCastTime(client->GetPlayer()) * .01);
			packet->setMediumStringByName("spell_name", spell->spell->GetSpellData()->name.data.c_str());
			EQ2Packet* outapp = packet->serialize();
			client->QueuePacket(outapp);
			safe_delete(packet);	
		}
	}
}

void SpellProcess::SendFinishedCast(LuaSpell* spell, const shared_ptr<Client>& client){
	if (spell && spell->spell) {
		float recast = spell->spell->GetModifiedRecast(spell->caster);

		if (spell->resisted && recast > 0) {
			CheckRecast(spell->spell, spell->caster, 0.5); // half sec recast on resisted spells
		} else if (!spell->interrupted) {
			CheckRecast(spell->spell, spell->caster, recast);

			TakePower(spell);
			TakeHP(spell);
			TakeSavagery(spell);
			AddDissonance(spell);
			AddConcentration(spell);
		}
		
		if (client) {
			PacketStruct* packet = configReader.getStruct("WS_FinishCastSpell", client->GetVersion());
			if (packet) {
				packet->setMediumStringByName("spell_name", spell->spell->GetSpellData()->name.data.c_str());
				client->QueuePacket(packet->serialize());
				safe_delete(packet);
			}

			UnlockAllSpells(client);
			SendSpellBookUpdate(client);
			client->CheckPlayerQuestsSpellUpdate(spell->spell);
		}
 	}
}

void SpellProcess::LockAllSpells(const shared_ptr<Client>& client){
	if(client){
		client->GetPlayer()->LockAllSpells();
		SendSpellBookUpdate(client);
	}
}

void SpellProcess::UnlockAllSpells(const shared_ptr<Client>& client){
	if(client)
		client->GetPlayer()->UnlockAllSpells();
}

void SpellProcess::UnlockSpell(const shared_ptr<Client>& client, Spell* spell){
	if(client && client->GetPlayer() && spell) {
		if (!client->GetPlayer()->IsCasting()) {
			client->GetPlayer()->UnlockSpell(spell);
			SendSpellBookUpdate(client);
		}
	}
}

bool SpellProcess::CheckPower(LuaSpell* spell){
	int16 req = 0;
	if(spell->caster){
		req = spell->spell->GetPowerRequired(spell->caster);
		if(spell->caster->GetPower() >= req)
			return true;
	}
	return false;
}

bool SpellProcess::TakePower(LuaSpell* spell){
	int16 req = 0;
	if(spell->caster){
		req = spell->spell->GetPowerRequired(spell->caster);
		if(spell->caster->GetPower() >= req){
			spell->caster->SetPower(spell->caster->GetPower() - req);
			if(spell->caster->IsPlayer())
				spell->caster->GetZone()->TriggerCharSheetTimer();
			return true;
		}
	}
	return false;
}

bool SpellProcess::CheckHP(LuaSpell* spell) { 
   int16 req = 0; 
   if(spell->caster){  
     req = spell->spell->GetHPRequired(spell->caster); 
     if(spell->caster->GetHP() >= req)
        return true; 
   } 
   return false; 
}

bool SpellProcess::TakeHP(LuaSpell* spell) { 
   int16 req = 0; 
   if(spell->caster && spell->caster->IsPlayer()){ 
     req = spell->spell->GetHPRequired(spell->caster); 
     if(spell->caster->GetHP() >= req){ 
        spell->caster->SetHP(spell->caster->GetHP() - req);
		if(spell->caster->IsPlayer())
			spell->caster->GetZone()->TriggerCharSheetTimer(); 
        return true; 
     } 
   } 
   return false; 
}

bool SpellProcess::CheckConcentration(LuaSpell* spell) {
	if (spell && spell->caster) {
		int8 req = spell->spell->GetSpellData()->req_concentration;
		int8 current_avail = 5 - spell->caster->GetConcentrationCurrent();
		if (current_avail >= req)
			return true;
	}
	return false;
}

bool SpellProcess::AddConcentration(LuaSpell* spell) {
	if (spell && spell->caster) {
		int8 req = spell->spell->GetSpellData()->req_concentration;
		int8 current_avail = 5 - spell->caster->GetConcentrationCurrent();
		if (current_avail >= req) {
			spell->caster->GetInfoStruct()->cur_concentration += req;
			if (spell->caster->IsPlayer())
				spell->caster->GetZone()->TriggerCharSheetTimer();
			LogWrite(SPELL__DEBUG, 0, "Spell", "Concentration is now %u on %s", spell->caster->GetInfoStruct()->cur_concentration, spell->caster->GetName());
			return true;
		}
	}
	return false;
}

bool SpellProcess::CheckSavagery(LuaSpell* spell) {
	if (spell && spell->caster) {
		int16 req = spell->spell->GetSavageryRequired(spell->caster); 
		if(spell->caster->GetSavagery() >= req)
			return true; 
	}
	return false;
}

bool SpellProcess::TakeSavagery(LuaSpell* spell) { 
   int16 req = 0; 
   if(spell && spell->caster && spell->caster->IsPlayer()){ 
     req = spell->spell->GetSavageryRequired(spell->caster); 
     if(spell->caster->GetSavagery() >= req){ 
        spell->caster->SetSavagery(spell->caster->GetSavagery() - req);
		if(spell->caster->IsPlayer())
			spell->caster->GetZone()->TriggerCharSheetTimer(); 
        return true; 
     } 
   } 
   return false; 
}

bool SpellProcess::CheckDissonance(LuaSpell* spell) {
	if (spell && spell->caster) {
		int16 req = spell->spell->GetDissonanceRequired(spell->caster); 
		if(spell->caster->GetDissonance() <= req)
			return true; 
	}
	return false;
}

bool SpellProcess::AddDissonance(LuaSpell* spell) { 
   int16 req = 0; 
   if(spell && spell->caster && spell->caster->IsPlayer()){ 
     req = spell->spell->GetDissonanceRequired(spell->caster); 
     if(spell->caster->GetDissonance() >= req){ 
        spell->caster->SetDissonance(spell->caster->GetDissonance() - req);
		if(spell->caster->IsPlayer())
			spell->caster->GetZone()->TriggerCharSheetTimer(); 
        return true; 
     } 
   } 
   return false; 
}

void SpellProcess::AddSpellToQueue(Spell* spell, Entity* caster){
	if (caster && caster->IsPlayer() && spell) {
		{
			lock_guard<mutex> guard(spell_queue_mutex);
			spell_queue.insert(pair<Entity*, Spell*>(caster, spell));
		}

		static_cast<Player*>(caster)->QueueSpell(spell);

		shared_ptr<Client> client = caster->GetZone()->GetClientBySpawn(caster);
		if (client) {
			SendSpellBookUpdate(client);
		}
	}
}

void SpellProcess::RemoveSpellFromQueue(Spell* spell, Entity* caster){
	if (caster && caster->IsPlayer() && spell) {
		{
			lock_guard<mutex> guard(spell_queue_mutex);
			spell_queue.erase(caster);
		}

		static_cast<Player*>(caster)->UnQueueSpell(spell);

		shared_ptr<Client> client = caster->GetZone()->GetClientBySpawn(caster);
		if (client) {
			SendSpellBookUpdate(client);
		}
	}
}

void SpellProcess::RemoveSpellFromQueue(Entity* caster, bool hostile_only) {
	lock_guard<mutex> guard(spell_queue_mutex);

	if (caster && spell_queue.count(caster) > 0) {
		Spell* spell = spell_queue[caster];

		if (spell) {
			bool remove = true;

			if (hostile_only && (spell->GetSpellData()->target_type != SPELL_TARGET_OTHER  || spell->GetSpellData()->friendly_spell)) {
				remove = false;
			}

			if (remove) {
				spell_queue.erase(caster);

				static_cast<Player*>(caster)->UnQueueSpell(spell);

				shared_ptr<Client> client = caster->GetZone()->GetClientBySpawn(caster);
				if (client) {
					SendSpellBookUpdate(client);
				}
			}
		}
	}
}

void SpellProcess::CheckSpellQueue(Spell* spell, Entity* caster) {
	if (!caster->IsPlayer())
		return;

	if (GetSpell(caster) == spell)
		return;

	Spell* existing_spell = nullptr;

	unique_lock<mutex> guard(spell_queue_mutex);

	if (spell_queue.count(caster) > 0) {
		existing_spell = spell_queue[caster];

		guard.unlock();
		RemoveSpellFromQueue(existing_spell, caster);
		guard.lock();
	}

	if (!existing_spell || existing_spell != spell) {
		guard.unlock();
		AddSpellToQueue(spell, caster);
		guard.lock();
	}
}

void SpellProcess::SendSpellBookUpdate(const shared_ptr<Client>& client){
	if(client){
		EQ2Packet* app = client->GetPlayer()->GetSpellBookUpdatePacket(client->GetVersion());
		if(app)
			client->QueuePacket(app);
	}
}

shared_ptr<LuaSpell> SpellProcess::GetLuaSpell(Entity* caster) {
	lock_guard<mutex> guard(cast_timers_mutex);

	if (caster && cast_timers.size() > 0) {		
		for (const auto cast_timer : cast_timers) {
			 if (cast_timer && cast_timer->spell && cast_timer->spell->caster == caster) {
				return cast_timer->spell;
			 }
		}
	}

	return nullptr;
}

Spell* SpellProcess::GetSpell(Entity* caster) {
	lock_guard<mutex> guard(cast_timers_mutex);

	if (cast_timers.size() > 0){		
		for (const auto cast_timer : cast_timers) {
			if (cast_timer && cast_timer->spell && cast_timer->spell->caster == caster) {
				return cast_timer->spell->spell;
			}
		}
	}

	return nullptr;
}

Spawn* SpellProcess::GetSpellTarget(Entity* caster) {
	lock_guard<mutex> guard(cast_timers_mutex);

	if (cast_timers.size() > 0) {
		for (const auto cast_timer : cast_timers) {
			 if (cast_timer && cast_timer->spell->caster == caster) {
				return caster->GetZone()->GetSpawnByID(cast_timer->spell->initial_target);
			 }
		}
	}

	return nullptr;
}

void SpellProcess::SetInitialTarget(LuaSpell* lua_spell, Spawn* target) {
	if (!lua_spell || !lua_spell->spell || !lua_spell->caster || !target) {
		return;
	}

	lua_spell->initial_target = target->GetID();

	int8 target_type = lua_spell->spell->GetSpellData()->target_type;

	if (lua_spell->caster->IsPlayer() && (target_type == SPELL_TARGET_OTHER || target_type == SPELL_TARGET_OTHER_CORPSE || target_type == SPELL_TARGET_GROUP_CORPSE || target_type == SPELL_TARGET_OTHER_GROUP_AE)) {
		if (lua_spell->spell->GetSpellData()->friendly_spell && lua_spell->caster->IsHostile(target)) {
			lua_spell->initial_target = lua_spell->caster->GetID();

			if (target->HasTarget()) {
				Spawn* secondary_target = target->GetTarget();

				if (!lua_spell->caster->IsHostile(secondary_target)) {
					lua_spell->initial_target = secondary_target->GetID();
				}
			}
		} else if (!lua_spell->spell->GetSpellData()->friendly_spell && !lua_spell->caster->IsHostile(target)) {
			if (target->HasTarget()) {
				Spawn* secondary_target = target->GetTarget();

				if (lua_spell->caster->IsHostile(secondary_target)) {
					lua_spell->initial_target = secondary_target->GetID();
				}
			}
		}
	} else if (target_type == SPELL_TARGET_GROUP_AE || target_type == SPELL_TARGET_RAID_AE) {
		lua_spell->initial_target = lua_spell->caster->GetID();
	} else if (target_type == SPELL_TARGET_SELF) {
		lua_spell->initial_target = lua_spell->caster->GetID();
	}
}

bool SpellProcess::CanCast(shared_ptr<LuaSpell> lua_spell, bool harvest_spell = false) {
	Entity* caster = lua_spell->caster;
	Spell* spell = lua_spell->spell;

	if (caster && spell) {
		shared_ptr<Client> client = nullptr;
		ZoneServer* zone = lua_spell->caster->GetZone();

		if (zone) {
			if (caster->IsPlayer()) {
				client = caster->GetZone()->GetClientBySpawn(caster);
			}

			if (spell->GetSpellData()->cast_type == SPELL_CAST_TYPE_TOGGLE) {
				bool ret_val = DeleteCasterSpell(caster, spell);

				if (ret_val) {
					int8 actual_concentration = spell->GetSpellData()->req_concentration;

					if (actual_concentration > 0) {
						caster->GetInfoStruct()->cur_concentration -= actual_concentration;
						if (caster->IsPlayer())
							caster->GetZone()->TriggerCharSheetTimer();
					}

					return false;
				}
			}

			if (client && spell->GetSpellData()->spell_book_type == SPELL_BOOK_TYPE_TRADESKILL) {
				if (client->IsCrafting()) {
					Tradeskill* tradeskill = nullptr;

					zone->GetTradeskillMgr()->ReadLock(__FUNCTION__, __LINE__);

					tradeskill = zone->GetTradeskillMgr()->GetTradeskill(client);

					if (spell->GetSpellData()->mastery_skill != tradeskill->recipe->GetTechnique()) {
						zone->GetTradeskillMgr()->ReleaseReadLock(__FUNCTION__, __LINE__);

						// send a message to the client, used chat_relationship to match other tradeskill messages, not sure if it is accurate though
						client->Message(CHANNEL_COLOR_CHAT_RELATIONSHIP, "You are not using %s on this recipe.", master_skill_list.GetSkill(spell->GetSpellData()->mastery_skill)->name.data.c_str());
						return false;
					}

					zone->GetTradeskillMgr()->ReleaseReadLock(__FUNCTION__, __LINE__);
				} else {
					zone->SendSpellFailedPacket(client, SPELL_ERROR_ONLY_WHEN_CRAFTING);
					return false;
				}
			}

			if (!IsReady(spell, caster)) {
				CheckSpellQueue(spell, caster);
				return false;
			}

			if ((caster->IsMezzed() && !spell->CastWhileMezzed()) || (caster->IsStunned() && !spell->CastWhileStunned())) {
				zone->SendSpellFailedPacket(client, SPELL_ERROR_CANNOT_CAST_STUNNED);
				return false;
			}

			if (caster->IsStifled() && !spell->CastWhileStifled()) {
				zone->SendSpellFailedPacket(client, SPELL_ERROR_CANNOT_CAST_STIFFLED);
				return false;
			}

			if (caster->IsFeared() && !spell->CastWhileFeared()) {
				zone->SendSpellFailedPacket(client, SPELL_ERROR_CANNOT_CAST_FEARED);
				return false;
			}

			if (!CheckPower(lua_spell.get())) {
				zone->SendSpellFailedPacket(client, SPELL_ERROR_NOT_ENOUGH_POWER);
				return false;
			}

			if (!CheckHP(lua_spell.get())) {
				zone->SendSpellFailedPacket(client, SPELL_ERROR_NOT_ENOUGH_HEALTH);
				return false;
			}

			if (!CheckSavagery(lua_spell.get())) {
				zone->SendSpellFailedPacket(client, SPELL_ERROR_NOT_ENOUGH_SAVAGERY);
				return false;
			}

			if (!CheckDissonance(lua_spell.get())) {
				zone->SendSpellFailedPacket(client, SPELL_ERROR_NOT_ENOUGH_DISSONANCE);
				return false;
			}

			if (!CheckConcentration(lua_spell.get())) {
				zone->SendSpellFailedPacket(client, SPELL_ERROR_NOT_ENOUGH_CONC);
				return false;
			}

			if (spell->MustBeStealthed() && !caster->IsStealthed()) {
				if (client) {
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You must be sneaking to use this ability.");
				}

				return false;
			}

			if (spell->GetSpellData()->max_aoe_targets == 0) {
				Spawn* target = caster->GetZone()->GetSpawnByID(lua_spell->initial_target);
				int8 target_type = spell->GetSpellData()->target_type;

				if (target_type != SPELL_TARGET_SELF && target_type != SPELL_TARGET_GROUP_AE && target_type != SPELL_TARGET_NONE) {
					if (!target) {
						zone->SendSpellFailedPacket(client, SPELL_ERROR_NO_ELIGIBLE_TARGET);
						return false;
					}

					if (caster->GetDistance(target) > spell->GetSpellData()->range) {
						zone->SendSpellFailedPacket(client, SPELL_ERROR_TOO_FAR_AWAY);
						return false;
					}

					if (caster->GetDistance(target) < spell->GetSpellData()->min_range) {
						zone->SendSpellFailedPacket(client, SPELL_ERROR_TOO_CLOSE);
						return false;
					}

					if (target_type == SPELL_TARGET_OTHER) {
						if (!target->IsEntity()) {
							zone->SendSpellFailedPacket(client, SPELL_ERROR_NO_ELIGIBLE_TARGET);
							return false;
						}

						if (spell->GetSpellData()->friendly_spell) {
							if (caster->IsHostile(target)) {
								zone->SendSpellFailedPacket(client, SPELL_ERROR_NOT_A_FRIEND);
								return false;
							}
						} else {
							if (!caster->IsHostile(target)) {
								zone->SendSpellFailedPacket(client, SPELL_ERROR_NOT_AN_ENEMY);
								return false;
							}

							if (!target->Alive()) {
								zone->SendSpellFailedPacket(client, SPELL_ERROR_NOT_ALIVE);
								return false;
							}

							if (target->GetInvulnerable()) {
								zone->SendSpellFailedPacket(client, SPELL_ERROR_TARGET_INVULNERABLE);
								return false;
							}

							if (target->IsPlayer() && caster->IsPlayer() && !static_cast<Player*>(caster)->CanAttackTarget(static_cast<Player*>(target))) {
								zone->SendSpellFailedPacket(client, SPELL_ERROR_NOT_AN_ENEMY);
								return false;
							}

							if (target->IsPet() && ((NPC*)target)->GetOwner() && ((NPC*)target)->GetOwner() == caster) {
								zone->SendSpellFailedPacket(client, SPELL_ERROR_NOT_AN_ENEMY);
								return false;
							}

							if (spell->MustBeFlanking() || spell->MustBeBehind()) {
								bool is_flanking = caster->FlankingTarget(target);
								bool is_behind = caster->BehindTarget(target);

								if (spell->MustBeFlanking() && spell->MustBeBehind()) {
									if (!is_flanking && !is_behind) {
										if (client) {
											client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You must be behind or flanking your target.");
										}

										return false;
									}
								} else if (spell->MustBeFlanking() && spell->MustBeInFrontOf()) {
									if (!is_flanking && is_behind) {
										if (client) {
											client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You must be in front of or flanking your target.");
										}
										return false;
									}
								} else if (spell->MustBeInFrontOf() && (is_flanking || is_behind)) {
									if (client) {
										client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You must be in front of your target.");
									}

									return false;
								} else if (spell->MustBeFlanking() && !is_flanking) {
									if (client) {
										client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You must be flanking your target.");
									}

									return false;
								} else if (spell->MustBeBehind() && !is_behind) {
									if (client) {
										client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You must be behind your target.");
									}

									return false;
								}
							}
						}
					} else if (target_type == SPELL_TARGET_OTHER_CORPSE || target_type == SPELL_TARGET_GROUP_CORPSE) {
						if (target->Alive()) {
							zone->SendSpellFailedPacket(client, SPELL_ERROR_NOT_DEAD);
							return false;
						}
						if (target->IsPlayer() && zone->GetClientBySpawn(target)->GetCurrentRez()->active) {
							zone->SendSpellFailedPacket(client, SPELL_ERROR_ALREADY_CAST);
							return false;
						}
					}
				} else if (target_type == SPELL_TARGET_SELF && harvest_spell && (!target || !target->IsGroundSpawn())) {
					zone->SendSpellFailedPacket(client, SPELL_ERROR_NO_ELIGIBLE_TARGET);
					return false;
				}
			}
		}
	}

	return true;
}

void SpellProcess::ProcessSpell(ZoneServer* zone, Spell* spell, Entity* caster, Spawn* target, bool harvest_spell, bool force_cast) {
	if (spell && caster) {
		shared_ptr<Client> client = nullptr;
		shared_ptr<LuaSpell> lua_spell = nullptr;

		if (lua_interface)
			lua_spell = lua_interface->GetSpell(spell->GetSpellData()->lua_script.c_str());

		if (!lua_spell)
			return;

		if (!target)
			target = caster;

		lua_spell->caster = caster;
		lua_spell->spell = spell;

		SetInitialTarget(lua_spell.get(), target);

		if ((force_cast && !OnCooldown(spell, caster)) || CanCast(lua_spell, harvest_spell)) {
			if (!harvest_spell) {
				GetSpellTargets(lua_spell.get());
			} else {
				lua_spell->MSpellTargets.writelock(__FUNCTION__, __LINE__);
				lua_spell->targets.push_back(lua_spell->initial_target);
				lua_spell->MSpellTargets.releasewritelock(__FUNCTION__, __LINE__);
			}

			if (!lua_spell->targets.size() && !spell->GetSpellData()->max_aoe_targets) {
				return;
			}

			if (caster->IsPlayer())
				client = caster->GetZone()->GetClientBySpawn(caster);

			if (lua_interface) {
				bool result = false;

				lua_interface->AddSpawnPointers(lua_spell.get(), false, true);

				if (lua_pcall(lua_spell->state, 2, 2, 0) == 0) {
					result = lua_interface->GetBooleanValue(lua_spell->state, 1);
					int8 error = lua_interface->GetInt8Value(lua_spell->state, 2) == 0 ? SPELL_ERROR_CANNOT_PREPARE : lua_interface->GetInt8Value(lua_spell->state, 2);
					lua_interface->ResetFunctionStack(lua_spell->state);

					if (!result) {
						zone->SendSpellFailedPacket(client, error);
						return;
					}
				} else {
					lua_interface->ResetFunctionStack(lua_spell->state);
				}
			}

			LockAllSpells(client);
			SendStartCast(lua_spell.get(), client);

			if (spell->GetSpellData()->type == SPELL_TYPE_SPELL && spell->ShouldCancelStealth() && (caster->IsInvis() || caster->IsStealthed()))
				caster->CancelAllStealth();

			int16 cast_time = spell->GetModifiedCastTime(caster);

			if (cast_time > 0) {
				auto cast_timer = new CastTimer;
				cast_timer->entity_command = nullptr;
				cast_timer->spell = lua_spell;
				cast_timer->spell->caster = caster;
				cast_timer->delete_timer = false;
				cast_timer->timer = new Timer(cast_time * 10);
				cast_timer->zone = zone;

				{
					lock_guard<mutex> guard(cast_timers_mutex);
					cast_timers.push_back(cast_timer);
				}

				if (caster) {
					caster->IsCasting(true);
				}
			} else {
				if (!CastProcessedSpell(lua_spell)) {
					return;
				}
			}

			if (caster) {
				caster->GetZone()->SendCastSpellPacket(lua_spell.get(), caster, cast_time);
			}
		} else {
			return;
		}
	}
}

void SpellProcess::ProcessEntityCommand(ZoneServer* zone, EntityCommand* entity_command, Entity* caster, Spawn* target, bool lock) {
	if (zone && entity_command && caster && target && !target->IsPlayer()) {
		shared_ptr<Client> client = zone->GetClientBySpawn(caster);
		if (caster->GetDistance(target) > entity_command->distance) {
			zone->SendSpellFailedPacket(client, SPELL_ERROR_TOO_FAR_AWAY);
			return;
		}
		if (entity_command->cast_time > 0) {
			PacketStruct* packet = configReader.getStruct("WS_StartCastSpell", client->GetVersion());
			if (packet) {
				LockAllSpells(client);
				packet->setDataByName("cast_time", entity_command->cast_time * 0.01);
				packet->setMediumStringByName("spell_name", entity_command->name.c_str());
				EQ2Packet* outapp = packet->serialize();
				client->QueuePacket(outapp);
				safe_delete(packet);

				auto cast_timer = new CastTimer;
				cast_timer->caster = client;
				cast_timer->entity_command = entity_command;
				cast_timer->spell = nullptr;
				cast_timer->delete_timer = false;
				cast_timer->timer = new Timer(entity_command->cast_time * 10);
				cast_timer->zone = zone;

				{
					lock_guard<mutex> guard(cast_timers_mutex);
					cast_timers.push_back(cast_timer);
				}

				caster->IsCasting(true);
			}
		}
		else if (!CastProcessedEntityCommand(entity_command, client))
			return;
		if (entity_command && entity_command->spell_visual > 0)
			caster->GetZone()->SendCastEntityCommandPacket(entity_command, caster->GetID(), target->GetID());
	}
}

bool SpellProcess::CastProcessedSpell(shared_ptr<LuaSpell> spell, bool passive) {
	if (!spell || !spell->caster || !spell->caster->Alive() || !spell->spell || spell->interrupted) {
		return false;
	}

	shared_ptr<Client> client = nullptr;
	bool hit_target = false;
	bool living_target = false;

	if (spell->caster && spell->caster->IsPlayer())
		client = spell->caster->GetZone()->GetClientBySpawn(spell->caster);

	if (spell->spell->GetSpellData()->max_aoe_targets > 0 && spell->targets.size() == 0) {
		GetSpellTargetsTrueAOE(spell.get());

		if (spell->targets.size() == 0) {
			spell->caster->GetZone()->SendSpellFailedPacket(client, SPELL_ERROR_NO_TARGETS_IN_RANGE);
			UnlockAllSpells(client);
			return false;
		}
	}

	if (spell->targets.size() > 0) {
		ZoneServer* zone = spell->caster->GetZone();

		spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
		for (int32 i = 0; i < spell->targets.size(); i++) {
			bool processedSpell = false;
			int8 damage_type = 0;
			int8 hit_result = 0;

			Spawn* target = zone->GetSpawnByID(spell->targets[i]);

			if (!target) {
				continue;
			}

			if (!target->Alive() && (spell->spell->GetSpellData()->target_type != SPELL_TARGET_OTHER_CORPSE && spell->spell->GetSpellData()->target_type != SPELL_TARGET_GROUP_CORPSE)) {
				continue;
			}

			if (!spell->spell->GetSpellData()->friendly_spell) {
				if (spell->spell->GetSpellData()->type == SPELL_BOOK_TYPE_COMBAT_ART) {
					damage_type = DAMAGE_PACKET_DAMAGE_TYPE_SLASH;
					hit_result = spell->caster->DetermineHit(target, damage_type, spell->spell->GetSpellData()->hit_bonus, false);
				} else {
					damage_type = DAMAGE_PACKET_DAMAGE_TYPE_MAGIC;
					hit_result = spell->caster->DetermineHit(target, damage_type, spell->spell->GetSpellData()->hit_bonus, true);
				}

				if (target->IsNPC())
					((NPC*)target)->AddHate(spell->caster, 1);

			} else if (spell->spell->GetSpellData()->group_spell) {
				GroupMemberInfo* gmi = spell->caster->GetGroupMemberInfo();
				bool in_group = false;

				if (gmi) {
					world.GetGroupManager()->GroupLock(__FUNCTION__, __LINE__);

					deque<GroupMemberInfo*>::iterator itr;
					deque<GroupMemberInfo*>* members = world.GetGroupManager()->GetGroupMembers(gmi->group_id);
					for (itr = members->begin(); itr != members->end(); itr++) {
						if ((*itr)->member == target) {
							in_group = true;
							break;
						}
					}

					world.GetGroupManager()->ReleaseGroupLock(__FUNCTION__, __LINE__);
				}

				if (!in_group && spell->caster != target)
					continue;
			}

			if (spell->spell->GetSpellData()->friendly_spell || hit_result == DAMAGE_PACKET_RESULT_SUCCESSFUL) {
				processedSpell = ProcessSpell(spell, target);

				if (processedSpell) {
					hit_target = true;

					if (!target)
						continue;

					if (client && client->IsZoning())
						continue;

					if (!living_target && target->Alive())
						living_target = true;

					if (spell->caster->IsPlayer() && target->IsPlayer() && living_target)
						PVP::HandlePlayerEncounter(static_cast<Player*>(spell->caster), static_cast<Player*>(target), !spell->spell->GetSpellData()->friendly_spell);
						
					if (spell->spell->GetSpellData()->success_message.length() > 0) {
						if (client) {
							string success_message = spell->spell->GetSpellData()->success_message;
							if (success_message.find("%t") != string::npos)
								success_message.replace(success_message.find("%t"), 2, spell->caster->GetName());
							client->Message(CHANNEL_COLOR_SPELL, success_message.c_str());
						}
					}

					if (spell->spell->GetSpellData()->effect_message.length() > 0) {
						string effect_message = spell->spell->GetSpellData()->effect_message;
						if (effect_message.find("%t") < 0xFFFFFFFF)
							effect_message.replace(effect_message.find("%t"), 2, target->GetName());
						spell->caster->GetZone()->SimpleMessage(CHANNEL_COLOR_SPELL_EFFECT, effect_message.c_str(), target, 50);
					}

					if (spell->spell->GetSpellDuration() > 0 || spell->spell->GetSpellData()->duration_until_cancel) {
						SpellEffects* effect = ((Entity*)target)->GetSpellEffect(spell->spell->GetSpellID());
						if (effect && effect->tier > spell->spell->GetSpellTier()) {
							if (client) {
								spell->caster->GetZone()->SendSpellFailedPacket(client, SPELL_ERROR_TAKE_EFFECT_MOREPOWERFUL);
								client->SimpleMessage(CHANNEL_COLOR_YELLOW, "The spell did not take hold as the target already has a better spell of this type.");
							}
						} else {
							((Entity*)target)->AddSpellEffect(spell);
							if (spell->spell->GetSpellData()->det_type > 0)
								((Entity*)target)->AddDetrimentalSpell(spell);
						}
					}

					spell->num_calls = 1;

					if (!spell->resisted && spell->spell->GetSpellData()->duration1 > 0) {
						spell->timer.Start();

						if (spell->spell->GetSpellData()->call_frequency > 0) {
							spell->timer.SetTimer(spell->spell->GetSpellData()->call_frequency * 100);
						} else {
							spell->timer.SetTimer(spell->spell->GetSpellData()->duration1 * 100);
						}

						if (spell->effect_bitmask & EFFECT_FLAG_SPELLBONUS && target->IsEntity()) {
							static_cast<Entity*>(target)->CalculateBonuses();
						}
					}

					target->GetZone()->CallSpawnScript(target, SPAWN_SCRIPT_CASTED_ON, spell->caster, spell->spell->GetName());
				}
			} else {
				spell->targets.erase(spell->targets.begin() + i);
				spell->last_spellattack_hit = false;

				zone->SendDamagePacket(spell->caster, target, DAMAGE_PACKET_TYPE_SPELL_DAMAGE, hit_result, damage_type, 0, spell->spell->GetName());
			}
		}
		spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);
	}

	if (client && client->IsZoning())
		return true;

	if (spell->spell->GetSpellData()->type == SPELL_BOOK_TYPE_SPELL)
		spell->resisted = !hit_target;

	if (hit_target && living_target) {
		if (spell->spell->GetSpellDuration() > 0 || spell->spell->GetSpellData()->duration_until_cancel) {
			if (!spell->spell->GetSpellData()->not_maintained)
				spell->caster->AddMaintainedSpell(spell);
			
			active_spells_mutex.lock();
			active_spells.push_back(spell);
			active_spells_mutex.unlock();
		}

		if (spell->num_triggers > 0)
			ClientPacketFunctions::SendMaintainedExamineUpdate(client, spell->slot_pos, spell->num_triggers, 0);

		if (client && client->GetPlayer()->GetWard(spell) && client->GetPlayer()->GetWard(spell)->DamageLeft > 0)
			ClientPacketFunctions::SendMaintainedExamineUpdate(client, spell->slot_pos, client->GetPlayer()->GetWard(spell)->DamageLeft, 1);

		spell->caster->GetZone()->TriggerCharSheetTimer();
	}

	if (spell->spell->ShouldCancelStealth() && (spell->caster->IsInvis() || spell->caster->IsStealthed()))
		spell->caster->CancelAllStealth(spell);

	if (!spell->spell->GetSpellData()->friendly_spell) {
		if (spell->caster->IsPlayer()) {
			static_cast<Player*>(spell->caster)->InCombat(true);

			if (static_cast<Player*>(spell->caster)->GetTarget()) {
				if ((spell->spell->GetSpellData()->casting_flags & CASTING_FLAG_ENABLE_MELEE_AUTO) == CASTING_FLAG_ENABLE_MELEE_AUTO) {
					static_cast<Player*>(spell->caster)->SetMeleeAttack(true);
					static_cast<Player*>(spell->caster)->SetRangeAttack(false);
				} else if ((spell->spell->GetSpellData()->casting_flags & CASTING_FLAG_ENABLE_RANGED_AUTO) == CASTING_FLAG_ENABLE_RANGED_AUTO) {
					static_cast<Player*>(spell->caster)->SetRangeAttack(true);
					static_cast<Player*>(spell->caster)->SetMeleeAttack(false);
				}
			}
		} else {
			spell->caster->InCombat(true);
		}
	}

	if (!passive)
		SendFinishedCast(spell.get(), client);

	return true;
}

bool SpellProcess::CastProcessedEntityCommand(EntityCommand* entity_command, const shared_ptr<Client>& client) {
	bool ret = false;
	if (entity_command && client) {
		UnlockAllSpells(client);
		client->GetPlayer()->IsCasting(false);
		if (entity_command->cast_time == 0) {
			client->GetPlayer()->GetZone()->CallSpawnScript(client->GetPlayer()->GetTarget(), SPAWN_SCRIPT_CASTED_ON, client->GetPlayer(), entity_command->command.c_str());
			ret = true;;
		}
		if (!ret) {
			PacketStruct* packet = configReader.getStruct("WS_FinishCastSpell", client->GetVersion());
			if (packet) {
				packet->setMediumStringByName("spell_name", entity_command->name.c_str());		
				client->QueuePacket(packet->serialize());
				safe_delete(packet);
				SendSpellBookUpdate(client);
				client->GetPlayer()->GetZone()->CallSpawnScript(client->GetPlayer()->GetTarget(), SPAWN_SCRIPT_CASTED_ON, client->GetPlayer(), entity_command->command.c_str());
				ret = true;
			}
		}
		if (ret) {
			EQ2_16BitString command;
			command.data = entity_command->command;
			command.size = entity_command->command.length();
			int32 handler = commands.GetCommandHandler(command.data.c_str());
			if (handler != 0xFFFFFFFF && handler < 999)
				commands.Process(handler, &command, client);
		}
	}
	return ret;
}

void SpellProcess::Interrupted(Entity* caster, Spawn* interruptor, int16 error_code, bool cancel, bool from_movement)
{
	if(caster)
	{
		LogWrite(SPELL__DEBUG, 0, "Spell", "'%s' is Interrupting spell of '%s'...", interruptor ? interruptor->GetName() : "unknown", caster->GetName());
		shared_ptr<LuaSpell> spell = GetLuaSpell(caster);

		if (spell && ((from_movement && !spell->spell->GetSpellData()->cast_while_moving) || (!from_movement && spell->spell->GetSpellData()->interruptable) ||
			cancel)) 
		{
			Spawn* target = GetSpellTarget(caster);
			auto interrupt = new InterruptStruct;
			interrupt->interrupted = caster;
			interrupt->spell = spell;
			interrupt->target = target;
			interrupt->error_code = error_code;
			spell->interrupted = true;

			{
				lock_guard<mutex> guard(interrupt_list_mutex);
				interrupt_list.push_back(interrupt);
			}

			if (interruptor && interruptor->IsPlayer()) {
				shared_ptr<Client> client = interruptor->GetZone()->GetClientBySpawn(interruptor);

				if (client) {
					client->Message(CHANNEL_COLOR_SPELL_INTERRUPT, "You interrupt %s's ability to cast!", interruptor->GetName());
				}
			}
			
		}
	}
}

void SpellProcess::RemoveSpellTimersFromSpawn(Spawn* spawn, bool remove_all, bool delete_recast){
	int32 i = 0;

	{
		lock_guard<mutex> guard(cast_timers_mutex);

		if (cast_timers.size() > 0) {
			for (auto cast_timer : cast_timers) {
				if (cast_timer && cast_timer->spell && cast_timer->spell->caster == spawn) {
					cast_timer->delete_timer = true;
				}
			}
		}
	}

	if (remove_all) {
		{
			lock_guard<mutex> guard(active_spells_mutex);

			for (auto spell : active_spells) {
				auto client = spell->caster->GetZone()->GetClientBySpawn(spell->caster);

				/*if (spell->spell->GetSpellData()->persist_though_death) {
					if (client && client->IsConnected()) {
						continue;
					}
				}*/

				if (spell->caster == spawn) {
					/*if (spell->caster != spell->caster->GetZone()->GetSpawnByID(spell->initial_target) && spell->spell->GetSpellData()->friendly_spell && spell->spell->GetSpellData()->target_type == SPELL_TARGET_OTHER && !spell->spell->GetSpellData()->group_spell) {
						if (!client || client->IsZoning()) {
							spell->caster = spell->caster->GetZone()->unknown_spawn;
							continue;
						}
					}*/

					DeleteCasterSpell(spell);
					continue;
				}

				spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
				for (const auto& target_id : spell->targets) {
					if (spawn->GetID() == target_id){
						RemoveTargetFromSpell(spell, spawn);
						break;
					}
				}
				spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);
			}
		}

		MRecastTimers.writelock(__FUNCTION__, __LINE__);
		if (recast_timers.size() > 0 && delete_recast) {
			auto itr = recast_timers.begin();

			while (itr != recast_timers.end()) {
				RecastTimer* recast_timer = (*itr);

				if (recast_timer && recast_timer->caster == spawn) {
					safe_delete(recast_timer->timer);
					safe_delete(recast_timer);
					itr = recast_timers.erase(itr);
				} else {
					itr++;
				}
			}
		}
		MRecastTimers.releasewritelock(__FUNCTION__, __LINE__);

		{
			lock_guard<mutex> guard(spell_queue_mutex);

			if (spell_queue.size() > 0 && spawn->IsEntity()) {
				spell_queue.erase(static_cast<Entity*>(spawn));
			}
		}

		{
			lock_guard<mutex> guard(interrupt_list_mutex);

			if (interrupt_list.size() > 0) {
				interrupt_list.erase(
					remove_if(
						interrupt_list.begin(),
						interrupt_list.end(),
						[&](InterruptStruct* interrupt) { return interrupt->interrupted == spawn; }
					),
					interrupt_list.end()
				);
			}
		}
	}
}

void SpellProcess::GetSpellTargets(LuaSpell* luaspell) 
{
	if (luaspell && luaspell->spell && luaspell->caster && luaspell->spell->GetSpellData()->max_aoe_targets == 0) 
	{
		int8 target_type = luaspell->spell->GetSpellData()->target_type;
		Spawn* caster = luaspell->caster;
		Spawn* target = caster->GetZone()->GetSpawnByID(luaspell->initial_target);
		SpellData* data = luaspell->spell->GetSpellData();

		luaspell->MSpellTargets.writelock(__FUNCTION__, __LINE__);
		// Group AE type                            NOTE: Add support for RAID AE to affect raid members once raids have been completed
		if (target_type == SPELL_TARGET_GROUP_AE || target_type == SPELL_TARGET_RAID_AE) 
		{
			// is friendly
			if (data->friendly_spell) 
			{
				// caster is an Entity
				if (luaspell->caster->IsEntity()) 
				{
					// entity caster is in a player group
					if (((Entity*)caster)->GetGroupMemberInfo()) 
					{
						world.GetGroupManager()->GroupLock(__FUNCTION__, __LINE__);

						// get group members
						deque<GroupMemberInfo*>* members = world.GetGroupManager()->GetGroupMembers(((Entity*)caster)->GetGroupMemberInfo()->group_id);
						deque<GroupMemberInfo*>::iterator itr;

						// iterate through list of group members
						for (itr = members->begin(); itr != members->end(); itr++) 
						{
							// get group member player info
							Entity* group_member = (*itr)->member;

							// if the group member is in the casters zone, and is alive
							if (group_member->GetZone() == luaspell->caster->GetZone() && group_member->Alive() && caster->GetDistance(group_member) <= luaspell->spell->GetSpellData()->radius) {
								shared_ptr<Client> client = caster->GetZone()->GetClientBySpawn(group_member);

								if (((Player*)group_member)->IsResurrecting() || !client || client->IsZoning())
									continue;

								luaspell->targets.push_back(group_member->GetID());

								if (group_member->HasPet()){
									Entity* pet = group_member->GetPet();
									if (!pet)
										pet = group_member->GetCharmedPet();
									if (pet)
										luaspell->targets.push_back(pet->GetID());
								}
							}
						}

						world.GetGroupManager()->ReleaseGroupLock(__FUNCTION__, __LINE__);
					}
					else
						luaspell->targets.push_back(caster->GetID()); // else caster is not in a group, thus alone
				}
				else if (caster->IsNPC()) // caster is NOT a player
				{
					// caster is NPC and in a spawn group with other NPCs
					vector<Spawn*>* group = ((NPC*)caster)->GetSpawnGroup();
					if (group) 
					{
						vector<Spawn*>::iterator itr;

						for (itr = group->begin(); itr != group->end(); itr++) 
						{
							Spawn* group_member = *itr;

							if (group_member->IsNPC() && group_member->Alive()){
								luaspell->targets.push_back(group_member->GetID());
								if (((Entity*)group_member)->HasPet()){
									Entity* pet = ((Entity*)group_member)->GetPet();
									if (pet)
									    luaspell->targets.push_back(pet->GetID());
									pet = ((Entity*)group_member)->GetCharmedPet();
									if (pet)
										luaspell->targets.push_back(pet->GetID());
								}
							}
						}
					}
					else
						luaspell->targets.push_back(caster->GetID());

					safe_delete(group);
				} // end is player
			} // end is friendly
		} // end is Group AE

		else if (target_type == SPELL_TARGET_SELF && caster)
			luaspell->targets.push_back(caster->GetID()); // if spell is SELF, return caster

		else if (target_type == SPELL_TARGET_CASTER_PET && caster && caster->IsEntity() && ((Entity*)caster)->HasPet()) {
			if (((Entity*)caster)->GetPet())
				luaspell->targets.push_back(((Entity*)caster)->GetPet()->GetID());
			if (((Entity*)caster)->GetCharmedPet())
				luaspell->targets.push_back(((Entity*)caster)->GetCharmedPet()->GetID());
		}

		else if (target_type == SPELL_TARGET_OTHER && target && target->Alive()) // if target is enemy, and is alive
		{
			// if friendly spell
			if (data->friendly_spell > 0) 
			{
				// if caster is a player
				if (caster->IsPlayer()) 
				{
					// if spell can affect raid, only group members or is a group spell
					if (data->can_effect_raid > 0 || data->affect_only_group_members > 0 || data->group_spell > 0) 
					{
						// if caster is in a group, and target is a player and targeted player is a group member
						if (((Player*)caster)->GetGroupMemberInfo() && (target->IsPlayer() || target->IsBot()) && ((Player*)caster)->IsGroupMember((Entity*)target))
							luaspell->targets.push_back(target->GetID()); // return the target
						else
							luaspell->targets.push_back(caster->GetID()); // else return the caster
					} else if (target->IsPlayer() || target->IsBot() || target->IsPet()) {
						luaspell->targets.push_back(target->GetID()); // return target for single spell
					} else {
						luaspell->targets.push_back(caster->GetID()); // and if no target, cast on self
					}
				}
				else if (caster->IsNPC()) // caster is an NPC
				{
					// if NPC's spell can affect raid, only group members or is a group spell
					if (data->can_effect_raid > 0 || data->affect_only_group_members > 0 || data->group_spell > 0) 
					{
						if (caster->IsBot() && (target->IsBot() || target->IsPlayer())) {
							GroupMemberInfo* gmi = ((Entity*)caster)->GetGroupMemberInfo();
							if (gmi && target->IsEntity() && world.GetGroupManager()->IsInGroup(gmi->group_id, (Entity*)target)) {
								luaspell->targets.push_back(target->GetID()); // return the target
							}
							else
								luaspell->targets.push_back(caster->GetID()); // else return the caster
						}
						// if NPC caster is in a group, and target is a player and targeted player is a group member
						else if (((NPC*)caster)->HasSpawnGroup() && target->IsNPC() && ((NPC*)caster)->IsInSpawnGroup((NPC*)target))
							luaspell->targets.push_back(target->GetID()); // return the target
						else
							luaspell->targets.push_back(caster->GetID()); // else return the caster
					}
					else {
						if (caster->IsBot() && (target->IsBot() || target->IsPlayer()))
							luaspell->targets.push_back(target->GetID());
					else
						luaspell->targets.push_back(target->GetID());
					}
				} // end is player
			} // end is friendly

			else if (data->group_spell > 0) // is not friendly, but is a group spell
			{
				// target is non-player
				if (target->IsNPC())
				{
					// Check to see if the npc is a spawn group by getting the group and checikng if valid
					vector<Spawn*>* group = ((NPC*)target)->GetSpawnGroup();
					if (group) 
					{
						vector<Spawn*>::iterator itr;

						// iterate through spawn group members
						for (itr = group->begin(); itr != group->end(); itr++) 
						{
							Spawn* group_member = *itr;

							// if NPC group member is (still) an NPC (wtf?) and is alive, send the NPC group member back as a successful target of non-friendly spell
							if (group_member->IsNPC() && group_member->Alive() && !((Entity*)group_member)->IsAOEImmune())
								luaspell->targets.push_back(group_member->GetID());

							// note: this should generate some hate towards the caster
						}
					} // end is spawngroup
					else
						luaspell->targets.push_back(target->GetID()); // return single target NPC for non-friendly spell

					safe_delete(group);
				} // end is NPC

				else if (target->IsPlayer())
				{
					// player is in a group
					if (((Player*)target)->GetGroupMemberInfo()) 
					{
						world.GetGroupManager()->GroupLock(__FUNCTION__, __LINE__);

						deque<GroupMemberInfo*>::iterator itr;
						deque<GroupMemberInfo*>* members = world.GetGroupManager()->GetGroupMembers(((Player*)target)->GetGroupMemberInfo()->group_id);

						// iterate through players group members
						for (itr = members->begin(); itr != members->end(); itr++) 
						{
							Entity* group_member = (*itr)->member;

							// if the group member is in the same zone as caster, and group member is alive, and group member is within distance
							if (group_member->GetZone() == caster->GetZone() && group_member->Alive() && caster->GetDistance(group_member) <= data->range
								&& (group_member == target || !group_member->IsAOEImmune()))
								luaspell->targets.push_back(group_member->GetID()); // add as target
						}

						world.GetGroupManager()->ReleaseGroupLock(__FUNCTION__, __LINE__);
					}
					else
						luaspell->targets.push_back(target->GetID()); // player not in group
				} // end is caster player or npc
			}
			else
				luaspell->targets.push_back(target->GetID()); // is not friendly nor a group spell
		}
		//Rez spells
		else if(target_type == SPELL_TARGET_OTHER_CORPSE){
			//is friendly
			if(data->friendly_spell){
				//target is player
				if(target->IsPlayer()){
					luaspell->targets.push_back(target->GetID());
				}
			}
		}
		else if(target_type == SPELL_TARGET_GROUP_CORPSE){
			//is friendly
			if(data->friendly_spell){
				//target is player
				if(target && target->IsPlayer()){
					//if target has group
					if(((Player*)target)->GetGroupMemberInfo()) {
						world.GetGroupManager()->GroupLock(__FUNCTION__, __LINE__);

						deque<GroupMemberInfo*>::iterator itr;
						deque<GroupMemberInfo*>* members = world.GetGroupManager()->GetGroupMembers(((Player*)target)->GetGroupMemberInfo()->group_id);
						for(itr = members->begin(); itr != members->end(); itr++) {
							Entity* group_member = (*itr)->member;
							//Check if group member is in the same zone in range of the spell and dead
							if(group_member->GetZone() == target->GetZone() && !group_member->Alive() && target->GetDistance(group_member) <= data->radius){
								luaspell->targets.push_back(group_member->GetID());
							}
						}

						world.GetGroupManager()->ReleaseGroupLock(__FUNCTION__, __LINE__);
					}
					else
						luaspell->targets.push_back(target->GetID());
				}
			}
		}
		luaspell->MSpellTargets.releasewritelock(__FUNCTION__, __LINE__);
	}

	if (luaspell && luaspell->targets.size() > 20)
		LogWrite(SPELL__WARNING, 0, "Spell", "Warning in %s: Size of targets array is %u", __FUNCTION__, luaspell->targets.size());
}

void SpellProcess::GetSpellTargetsTrueAOE(LuaSpell* luaspell) {
	if (luaspell && luaspell->caster && luaspell->spell && luaspell->spell->GetSpellData()->max_aoe_targets > 0) {
		if (luaspell->caster->HasTarget() && luaspell->caster->GetTarget() != luaspell->caster){
			//Check if the caster has an implied target
			if (luaspell->caster->GetDistance(luaspell->caster->GetTarget()) <= luaspell->spell->GetSpellData()->radius) {
				luaspell->initial_target = luaspell->caster->GetTarget()->GetID();
			}
		}

		int32 ignore_target = 0;
		vector<Spawn*> spawns = luaspell->caster->GetZone()->GetAttackableSpawnsByDistance(luaspell->caster, luaspell->spell->GetSpellData()->radius);

		luaspell->MSpellTargets.writelock(__FUNCTION__, __LINE__);
		if (luaspell->initial_target && luaspell->caster->GetID() != luaspell->initial_target) {
			//this is the "Direct" target and aoe can't be avoided
			luaspell->targets.push_back(luaspell->initial_target);
			ignore_target = luaspell->initial_target;
		}

		for (const auto spawn : spawns) {
			if (luaspell->targets.size() >= luaspell->spell->GetSpellData()->max_aoe_targets) {
				break;
			}

			if (spawn && spawn->GetID() != ignore_target){
				if (static_cast<Entity*>(spawn)->IsAOEImmune() || (luaspell->caster->IsNPC() && spawn->IsNPC())) {
					continue;
				}

				luaspell->targets.push_back(spawn->GetID());
			}
		}
		luaspell->MSpellTargets.releasewritelock(__FUNCTION__, __LINE__);
	}

	if (luaspell && luaspell->targets.size() > 20) {
		LogWrite(SPELL__DEBUG, 0, "Spell", "Warning in  SpellProcess::GetSpellTargetsTrueAOE Size of targets array is %u", luaspell->targets.size());
	}
}

void SpellProcess::AddSpellScriptTimer(SpellScriptTimer* timer) {
	MSpellScriptTimers.writelock(__FUNCTION__, __LINE__);
	m_spellScriptList.push_back(timer);
	MSpellScriptTimers.releasewritelock(__FUNCTION__, __LINE__);
}

void SpellProcess::RemoveSpellScriptTimer(SpellScriptTimer* timer) {
	vector<SpellScriptTimer*>::iterator itr;
	MSpellScriptTimers.writelock(__FUNCTION__, __LINE__);
	for (itr = m_spellScriptList.begin(); itr != m_spellScriptList.end(); itr++) {
		if ((*itr) == timer) {
			m_spellScriptList.erase(itr);
			break;
		}
	}
	MSpellScriptTimers.releasewritelock(__FUNCTION__, __LINE__);
}

void SpellProcess::CheckSpellScriptTimers() {
	vector<SpellScriptTimer*>::iterator itr;
	vector<SpellScriptTimer*> temp_list;
	
	MSpellScriptTimers.readlock(__FUNCTION__, __LINE__);
	for (itr = m_spellScriptList.begin(); itr != m_spellScriptList.end(); itr++) {
		if (Timer::GetCurrentTime2() >= (*itr)->time) {
			temp_list.push_back((*itr));
		}
	}
	MSpellScriptTimers.releasereadlock(__FUNCTION__, __LINE__);

	for (itr = temp_list.begin(); itr != temp_list.end(); itr++) {
		RemoveSpellScriptTimer(*itr);
		safe_delete(*itr);
	}
}

void SpellProcess::ClearSpellScriptTimerList() {
	vector<SpellScriptTimer*>::iterator itr;
	MSpellScriptTimers.writelock(__FUNCTION__, __LINE__);

	for(itr = m_spellScriptList.begin(); itr != m_spellScriptList.end(); itr++) {
		safe_delete((*itr));
	}

	m_spellScriptList.clear();
	MSpellScriptTimers.releasewritelock(__FUNCTION__, __LINE__);
}

void SpellProcess::RemoveTargetFromSpell(shared_ptr<LuaSpell> spell, Spawn* target){
	if (!spell || !target)
		return;

	MRemoveTargetList.writelock(__FUNCTION__, __LINE__);
	if (!remove_target_list[spell])
		remove_target_list[spell] = new vector<Spawn*>;
	remove_target_list[spell]->push_back(target);
	MRemoveTargetList.releasewritelock(__FUNCTION__, __LINE__);
}

void SpellProcess::CheckRemoveTargetFromSpell() {
	map<shared_ptr<LuaSpell>, vector<Spawn*>> to_remove;

	MRemoveTargetList.writelock(__FUNCTION__, __LINE__);
	if (remove_target_list.size() > 0) {
		for (auto& kv : remove_target_list) {
			auto spell = kv.first;
			auto targets = kv.second;
			
			if (to_remove.count(spell) == 0) {
				to_remove[spell];
			}

			for (const auto& target : *targets) {
				if (target) {
					spell->MSpellTargets.writelock(__FUNCTION__, __LINE__);

					for (auto spell_itr = spell->targets.begin(); spell_itr != spell->targets.end(); ++spell_itr) {
						auto spell_target = (*spell_itr);

						if (target->GetID() == spell_target) {
							spell->targets.erase(spell_itr);
							to_remove[spell].push_back(target);

							break;
						}
					}

					spell->MSpellTargets.releasewritelock(__FUNCTION__, __LINE__);
				}
			}

			if (targets)
				targets->clear();

			safe_delete(targets);
		}

		remove_target_list.clear();
	}
	MRemoveTargetList.releasewritelock(__FUNCTION__, __LINE__);

	for (const auto& kv : to_remove) {
		auto spell = kv.first;
		auto targets = kv.second;

		for (auto target : targets) {
			lua_interface->RemoveSpell(spell, target, true);

			if (target->IsEntity()) {
				static_cast<Entity*>(target)->RemoveSpellEffect(spell);
				static_cast<Entity*>(target)->RemoveEffectsFromLuaSpell(spell);

				if (spell->spell->GetSpellData()->det_type > 0 && (spell->spell->GetSpellDuration() > 0 || spell->spell->GetSpellData()->duration_until_cancel)) {
					static_cast<Entity*>(target)->RemoveDetrimentalSpell(spell);
				}

				if (target->IsPlayer() && spell->spell->GetSpellData()->fade_message.length() > 0) {
					auto client = target->GetZone()->GetClientBySpawn(target);

					if (client) {
						auto fade_message = spell->spell->GetSpellData()->fade_message;

						if (fade_message.find("%t") != string::npos)
							fade_message.replace(fade_message.find("%t"), 2, target->GetName());

						client->Message(CHANNEL_COLOR_SPELL_FADE, fade_message.c_str());
					}
				}


				if (spell->effect_bitmask & EFFECT_FLAG_SPELLBONUS) {
					static_cast<Entity*>(target)->CalculateBonuses();
				}
			}
		}
	}

}

bool SpellProcess::AddHO(shared_ptr<Client> client, HeroicOP* ho) {
	bool ret = true;

	if (client && ho) {
		MSoloHO.writelock(__FUNCTION__, __LINE__);
		if (m_soloHO.count(client) > 0) {
			if (m_soloHO[client]->GetWheel()) {
				ret = false;
			}
			else {
				safe_delete(m_soloHO[client]);
				m_soloHO[client] = ho;
			}
		}
		else
			m_soloHO[client] = ho;
		MSoloHO.releasewritelock(__FUNCTION__, __LINE__);
	}

	return ret;
}

bool SpellProcess::AddHO(int32 group_id, HeroicOP* ho) {
	bool ret = true;

	if (group_id > 0 && ho) {
		MGroupHO.writelock(__FUNCTION__, __LINE__);
		if (m_groupHO.count(group_id) > 0) {
			if (m_groupHO[group_id]->GetWheel()) {
				ret = false;
			}
			else {
				safe_delete(m_groupHO[group_id]);
				m_groupHO[group_id] = ho;
			}
		}
		else
			m_groupHO[group_id] = ho;
		MGroupHO.releasewritelock(__FUNCTION__, __LINE__);
	}

	return ret;
}

void SpellProcess::KillHOBySpawnID(int32 spawn_id) {
	// Check solo HO's
	MSoloHO.writelock(__FUNCTION__, __LINE__);
	auto itr = m_soloHO.begin();
	while (itr != m_soloHO.end()) {
		if (itr->second->GetTarget() == spawn_id) {
			itr->second->SetComplete(1);
			ClientPacketFunctions::SendHeroicOPUpdate(itr->first, itr->second);
			safe_delete(itr->second);
			itr = m_soloHO.erase(itr);
		} else {
			itr++;
		}
	}
	MSoloHO.releasewritelock(__FUNCTION__, __LINE__);

	// Check Group HO's
	MGroupHO.writelock(__FUNCTION__, __LINE__);
	auto itr2 = m_groupHO.begin();
	while (itr2 != m_groupHO.end()) {
		if (itr2->second->GetTarget() == spawn_id) {
			itr2->second->SetComplete(1);

			world.GetGroupManager()->GroupLock(__FUNCTION__ , __LINE__);
			deque<GroupMemberInfo*>::iterator itr3;
			deque<GroupMemberInfo*>* members = world.GetGroupManager()->GetGroupMembers(itr2->first);
			for (auto member : *members) {
				if (member->client) {
					ClientPacketFunctions::SendHeroicOPUpdate(member->client, itr2->second);
				}
			}
			world.GetGroupManager()->ReleaseGroupLock(__FUNCTION__, __LINE__);

			safe_delete(itr2->second);
			itr2 = m_groupHO.erase(itr2);
		} else {
			itr2++;
		}
	}
	MGroupHO.releasewritelock(__FUNCTION__, __LINE__);
}

void SpellProcess::AddSpellCancel(shared_ptr<LuaSpell> spell){
	MSpellCancelList.writelock(__FUNCTION__, __LINE__);
	SpellCancelList.push_back(spell);
	MSpellCancelList.releasewritelock(__FUNCTION__, __LINE__);
}

void SpellProcess::CastSpell(int32 spell_id, int8 tier, Entity* caster, int32 initial_target, int32 duration) {
	Spell* spell = master_spell_list.GetSpell(spell_id, tier);

	if (spell) {
		if (duration > 0) {
			spell->GetSpellData()->duration1 = duration;
			spell->GetSpellData()->duration2 = duration;
		}

		shared_ptr<LuaSpell> lua_spell = nullptr;

		if (lua_interface) {
			lua_spell = lua_interface->GetSpell(spell->GetSpellData()->lua_script.c_str());
		}

		if (lua_spell) {
			lua_spell->caster = caster;
			lua_spell->initial_target = initial_target;
			lua_spell->spell = spell;

			GetSpellTargets(lua_spell.get());
			CastProcessedSpell(lua_spell, true);
		}
	}
}

bool SpellProcess::HasActiveSpell(shared_ptr<LuaSpell> spell, bool lock_required) {
	mutex dummy_mutex;
	lock_guard<mutex> guard(lock_required ? active_spells_mutex : dummy_mutex);

	for (const auto active_spell : active_spells) {
		if (spell == active_spell) {
			return true;
		}
	}

	return false;
}
