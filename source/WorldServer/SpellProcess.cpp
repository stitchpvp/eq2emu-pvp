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

	MutexList<LuaSpell*>::iterator active_spells_itr = active_spells.begin();
	while(active_spells_itr.Next()){
		DeleteCasterSpell(active_spells_itr->value);
	}

	active_spells_itr = active_spells.begin();
	while(active_spells_itr.Next()){
		active_spells.Remove(active_spells_itr->value, true);
	}
	active_spells.clear();	

	InterruptStruct* interrupt = 0;
	MutexList<InterruptStruct*>::iterator interrupt_list_itr = interrupt_list.begin();
	while(interrupt_list_itr.Next()){
		interrupt = interrupt_list_itr->value;
		CheckInterrupt(interrupt);
		interrupt_list.Remove(interrupt_list_itr->value, true);		
	}
	interrupt_list.clear();

	MutexList<CastTimer*>::iterator cast_timers_itr = cast_timers.begin();	
	while(cast_timers_itr.Next()){
		safe_delete(cast_timers_itr->value->timer);
		safe_delete(cast_timers_itr->value->spell);
		cast_timers.Remove(cast_timers_itr->value, true);
	}
	cast_timers.clear();

	MRecastTimers.writelock(__FUNCTION__, __LINE__);
	recast_timers.clear();
	MRecastTimers.releasewritelock(__FUNCTION__, __LINE__);

	MRemoveTargetList.writelock(__FUNCTION__, __LINE__);
	remove_target_list.clear();
	MRemoveTargetList.releasewritelock(__FUNCTION__, __LINE__);

	map<Client*, HeroicOP*>::iterator solo_ho_itr;
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

	spell_que.clear();
	
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

	if(active_spells.size(true) > 0){		
		LuaSpell* spell = 0;
		MutexList<LuaSpell*>::iterator itr = active_spells.begin();
		map<LuaSpell*, vector<Spawn*> >::iterator remove_itr;
		vector<Spawn*>::iterator target_itr;
		vector<Spawn*>::iterator remove_target_itr;

		while(itr.Next()){
			spell = itr->value;

			if (!spell->spell->GetSpellData()->duration_until_cancel && spell->timer.Check()){
				spell->num_calls++;

				// ProcessSpell(spell, flase) will atempt to call the tick() function in the lua script
				// if there is no tick function it will return false, this will cause the server to crash in the event
				// of a spell that has a duration but is not a "until canceled" spell or a spell with a tick (tradeskill buffs)
				// to counter this check to see if the spell has a call_frequency > 0 before we call ProcessSpell()
				if (spell->spell->GetSpellData()->call_frequency > 0) {
					if (spell->targets.size() > 0) {
						ZoneServer* zone = spell->caster->GetZone();
						Spawn* target = 0;

						spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
						for (int32 i = 0; i < spell->targets.size(); i++) {
							target = zone->GetSpawnByID(spell->targets[i]);

							if (!ProcessSpell(spell, target, false)) {
								active_spells.Remove(spell, true, 2000);
								break;
							}
						}
						spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);
					}
				}
				
				if ((spell->timer.GetDuration() * spell->num_calls) > spell->spell->GetSpellData()->duration1 * 100)
					DeleteCasterSpell(spell);
			}
			else
				CheckRemoveTargetFromSpell(spell);
		}
	}
	if (SpellCancelList.size() > 0){
		MSpellCancelList.writelock(__FUNCTION__, __LINE__);
		vector<LuaSpell*>::iterator itr = SpellCancelList.begin();
		while (itr != SpellCancelList.end()){
			DeleteCasterSpell(*itr);
			itr++;
		}
		SpellCancelList.clear();
		MSpellCancelList.releasewritelock(__FUNCTION__, __LINE__);
	}
	if(interrupt_list.size(true) > 0){		
		InterruptStruct* interrupt = 0;
		MutexList<InterruptStruct*>::iterator itr = interrupt_list.begin();
		while(itr.Next()){
			interrupt = itr->value;
			CheckInterrupt(interrupt);
			safe_delete(interrupt);
		}
		interrupt_list.clear();
	}
	if(cast_timers.size(true) > 0){
		CastTimer* cast_timer = 0;
		MutexList<CastTimer*>::iterator itr = cast_timers.begin();
		while (itr.Next()) {
			cast_timer = itr->value;
			if (cast_timer) {
				if (cast_timer->timer->Check(false)) {
					if (cast_timer->spell) {
						Client* client = cast_timer->zone->GetClientBySpawn(cast_timer->spell->caster);
						if (client) {
							PacketStruct* packet = configReader.getStruct("WS_FinishCastSpell", client->GetVersion());
								if (packet) {
									packet->setMediumStringByName("spell_name", cast_timer->spell->spell->GetSpellData()->name.data.c_str());
									client->QueuePacket(packet->serialize());
									safe_delete(packet);
								}
						}
						if (cast_timer->spell && cast_timer->spell->caster)
							cast_timer->spell->caster->IsCasting(false);
						cast_timer->delete_timer = true;
						CastProcessedSpell(cast_timer->spell);
					} else if (cast_timer->entity_command && cast_timer->timer->Check(false)) {
						cast_timer->delete_timer = true;
						CastProcessedEntityCommand(cast_timer->entity_command, cast_timer->caster);
					}
				}
			}
			if (cast_timer->delete_timer) {
				safe_delete(cast_timer->timer);
				cast_timers.Remove(cast_timer, true);
			}
		}
	}
	MRecastTimers.writelock(__FUNCTION__, __LINE__);
	if (recast_timers.size() > 0){
		vector<RecastTimer*>::iterator itr = recast_timers.begin();
		while (itr != recast_timers.end()) {
			RecastTimer* recast_timer = *itr;
			if (recast_timer->timer->Check(false)) {
				UnlockSpell(recast_timer->client, recast_timer->spell);
				safe_delete(recast_timer->timer);
				itr = recast_timers.erase(itr);
			} else {
				itr++;
			}
		}
	}
	MRecastTimers.releasewritelock(__FUNCTION__, __LINE__);
	if(spell_que.size(true) > 0){
		MutexMap<Entity*, Spell*>::iterator itr = spell_que.begin();
		while(itr.Next()){
			if(itr->first->IsCasting() == false && IsReady(itr->second, itr->first)){
				RemoveSpellFromQueue(itr->second, itr->first);
				ProcessSpell(itr->first->GetZone(), itr->second, itr->first, itr->first->GetTarget());
			}
		}
	}

	// Check solo HO timers
	MSoloHO.writelock(__FUNCTION__, __LINE__);
	if (m_soloHO.size() > 0) {
		map<Client*, HeroicOP*>::iterator itr = m_soloHO.begin();
		map<Client*, HeroicOP*>::iterator delete_itr;
		while (itr != m_soloHO.end()) {
			if (itr->second->GetWheel() && Timer::GetCurrentTime2() >= (itr->second->GetStartTime() + (itr->second->GetTotalTime() * 1000))) {
				itr->second->SetComplete(1);
				ClientPacketFunctions::SendHeroicOPUpdate(itr->first, itr->second);
				safe_delete(itr->second);
				delete_itr = itr;
				itr++;
				m_soloHO.erase(delete_itr);
			}
			else
				itr++;
		}
	}
	MSoloHO.releasewritelock(__FUNCTION__, __LINE__);

	// Check group HO timers
	MGroupHO.writelock(__FUNCTION__, __LINE__);
	if (m_groupHO.size() > 0) {
		map<int32, HeroicOP*>::iterator itr = m_groupHO.begin();
		map<int32, HeroicOP*>::iterator delete_itr;
		while (itr != m_groupHO.end()) {
			if (itr->second->GetWheel() && Timer::GetCurrentTime2() >= (itr->second->GetStartTime() + (itr->second->GetTotalTime() * 1000))) {
				itr->second->SetComplete(1);

				world.GetGroupManager()->GroupLock(__FUNCTION__, __LINE__);
				deque<GroupMemberInfo*>::iterator itr2;
				deque<GroupMemberInfo*>* members = world.GetGroupManager()->GetGroupMembers(itr->first);
				for (itr2 = members->begin(); itr2 != members->end(); itr2++) {
					if ((*itr2)->client)
						ClientPacketFunctions::SendHeroicOPUpdate((*itr2)->client, itr->second);
				}
				world.GetGroupManager()->ReleaseGroupLock(__FUNCTION__, __LINE__);

				safe_delete(itr->second);
				delete_itr = itr;
				itr++;
				m_groupHO.erase(delete_itr);
			}
			else
				itr++;
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

	bool ret = true;	
	MRecastTimers.readlock(__FUNCTION__, __LINE__);
	for (const auto recast_timer : recast_timers) {
		if (recast_timer->spell == spell && recast_timer->caster == caster) {
			ret = false;
			break;
		}
	}
	MRecastTimers.releasereadlock(__FUNCTION__, __LINE__);
	return ret;
}

void SpellProcess::CheckRecast(Spell* spell, Entity* caster, float timer_override, bool check_linked_timers) {
	if (spell && caster) {
		if (timer_override > 0) {
			RecastTimer* timer = new RecastTimer;
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
				vector<Spell*> linkedSpells = ((Player*)caster)->GetSpellBookSpellsByTimer(spell->GetSpellData()->linked_timer);
				for (int8 i = 0; i < linkedSpells.size(); i++) {
					Spell* spell2 = linkedSpells.at(i);
					if (spell2 && spell2 != spell)
						CheckRecast(spell2, caster, timer_override, false);
				}
			}
		}
	}
}
void SpellProcess::CheckInterrupt(InterruptStruct* interrupt){
	if(!interrupt || !interrupt->interrupted || !interrupt->interrupted->IsEntity())
		return;
	Entity* entity = (Entity*)interrupt->interrupted;
	Client* client = entity->GetZone()->GetClientBySpawn(entity);
	if(client)
		SendFinishedCast(GetLuaSpell(entity), client);
	RemoveSpellTimersFromSpawn(entity, false);
	entity->IsCasting(false);
	entity->GetZone()->SendInterruptPacket(entity, interrupt->spell);
	if(interrupt->error_code > 0)
		entity->GetZone()->SendSpellFailedPacket(client, interrupt->error_code);
}

bool SpellProcess::DeleteCasterSpell(Spawn* caster, Spell* spell){

	bool ret = false;
	// need to use size(true) to get pending updates to the list as well
	if (caster && spell && active_spells.size() > 0) {
		LuaSpell* lua_spell = 0;
		MutexList<LuaSpell*>::iterator itr = active_spells.begin();
		while (itr.Next()){
			lua_spell = itr->value;
			if (lua_spell->spell == spell && lua_spell->caster == caster) {
				ret = DeleteCasterSpell(lua_spell);
				break;
			}
		}
	}
	return ret;
}

bool SpellProcess::DeleteCasterSpell(LuaSpell* spell, bool call_remove_function){
	bool ret = false;

	if (spell) {
		ZoneServer* zone = spell->caster->GetZone();

		if (active_spells.count(spell) > 0)
			active_spells.Remove(spell);

		if (spell->caster) {
			spell->caster->RemoveMaintainedSpell(spell);

			spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
			for (int32 i = 0; i < spell->targets.size(); i++) {
				Spawn* target = zone->GetSpawnByID(spell->targets.at(i));

				if (target && lua_interface) {
					static_cast<Entity*>(target)->RemoveEffectsFromLuaSpell(spell);
					lua_interface->RemoveSpell(spell, target, call_remove_function, SpellScriptTimersHasSpell(spell));
				}

				if (target && target->IsEntity()) {
					((Entity*)target)->RemoveSpellEffect(spell);
					if (spell->spell->GetSpellData()->det_type > 0 && (spell->spell->GetSpellDuration() > 0 || spell->spell->GetSpellData()->duration_until_cancel))
						((Entity*)target)->RemoveDetrimentalSpell(spell);
				} else {
					spell->caster->RemoveSpellEffect(spell);
					if (spell->spell->GetSpellData()->det_type > 0 && (spell->spell->GetSpellDuration() > 0 || spell->spell->GetSpellData()->duration_until_cancel))
						spell->caster->RemoveDetrimentalSpell(spell);
				}

				if (target && target->IsPlayer() && spell->spell->GetSpellData()->fade_message.length() > 0) {
					Client* client = target->GetZone()->GetClientBySpawn(target);

					if (client) {
						string fade_message = spell->spell->GetSpellData()->fade_message;

						if (fade_message.find("%t") != string::npos)
							fade_message.replace(fade_message.find("%t"), 2, target->GetName());

						client->Message(CHANNEL_COLOR_SPELL_FADE, fade_message.c_str());
					}
				}
			}
			spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);

			if (spell->spell->GetSpellData()->cast_type == SPELL_CAST_TYPE_TOGGLE) {
				float recast = spell->spell->GetModifiedRecast(spell->caster);

				if (recast > 0) {
					CheckRecast(spell->spell, spell->caster, recast);
					if (spell->caster && spell->caster->IsPlayer())
						SendSpellBookUpdate(spell->caster->GetZone()->GetClientBySpawn(spell->caster));
				} else {
					UnlockSpell(spell->caster->GetZone()->GetClientBySpawn(spell->caster), spell->spell);
				}
			}

			ret = true;
		}	
	}
	return ret;
}

bool SpellProcess::ProcessSpell(LuaSpell* spell, Spawn* target, bool first_cast, const char* function, SpellScriptTimer* timer) {
	bool ret = false;
	if(lua_interface && !spell->interrupted){
		lua_interface->AddSpawnPointers(spell, first_cast, false, function, timer);
		
		if (target)
			lua_interface->SetSpawnValue(spell->state, target);

		vector<LUAData*>* data = spell->spell->GetLUAData();
		for(int32 i=0;i<data->size();i++){
			switch(data->at(i)->type){
				case 0:{
					lua_interface->SetSInt32Value(spell->state, data->at(i)->int_value);
					break;
				}
				case 1:{
					lua_interface->SetFloatValue(spell->state, data->at(i)->float_value);
					break;
				}
				case 2:{
					lua_interface->SetBooleanValue(spell->state, data->at(i)->bool_value);
					break;
				}
				case 3:{
					lua_interface->SetStringValue(spell->state, data->at(i)->string_value.c_str());
					break;
				}
				default:{
					LogWrite(SPELL__ERROR, 0, "Spell", "Error: Unknown LUA Type '%i' in SpellProcess::ProcessSpell for Spell '%s'", (int)data->at(i)->type, spell->spell->GetName());
					return false;
				}
			}
		}
		ret = lua_interface->CallSpellProcess(spell, 2 + data->size());
	}
	return ret;
}

bool SpellProcess::CastPassives(Spell* spell, Entity* caster, bool remove) {

	LuaSpell* lua_spell = 0;

	if(lua_interface)
		lua_spell = lua_interface->GetSpell(spell->GetSpellData()->lua_script.c_str());

	if(!lua_spell)
	{
		string lua_script = spell->GetSpellData()->lua_script;
		lua_script.append(".lua");
		lua_spell = 0;

		if(lua_interface)
			lua_spell = lua_interface->GetSpell(lua_script.c_str());

		if(!lua_spell) {
			LogWrite(SPELL__ERROR, 0, "Spell", "Failed to get a LuaSpell for %s (%u)", spell->GetName(), spell->GetSpellID());
			return false;
		}
		else
			spell->GetSpellData()->lua_script = lua_script;
	}

	lua_spell->caster = caster;
	lua_spell->spell = spell;
	lua_spell->initial_target = caster->GetID();
	GetSpellTargets(lua_spell);

	if (!remove)
		return CastProcessedSpell(lua_spell, true);

	//lua_interface->RemoveSpell(lua_spell, true, SpellScriptTimersHasSpell(lua_spell));
	return true;
}

void SpellProcess::SendStartCast(LuaSpell* spell, Client* client){
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

void SpellProcess::SendFinishedCast(LuaSpell* spell, Client* client){
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

void SpellProcess::LockAllSpells(Client* client){
	if(client){
		client->GetPlayer()->LockAllSpells();
		SendSpellBookUpdate(client);
	}
}

void SpellProcess::UnlockAllSpells(Client* client){
	if(client)
		client->GetPlayer()->UnlockAllSpells();
}

void SpellProcess::UnlockSpell(Client* client, Spell* spell){
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
	if(caster && caster->IsPlayer() && spell){
		spell_que.Put(caster, spell);
		((Player*)caster)->QueueSpell(spell);
		Client* client = caster->GetZone()->GetClientBySpawn(caster);
		if(client)
			SendSpellBookUpdate(client);
	}
}

void SpellProcess::RemoveSpellFromQueue(Spell* spell, Entity* caster){
	if(caster && caster->IsPlayer() && spell){
		spell_que.erase(caster);
		((Player*)caster)->UnQueueSpell(spell);
		Client* client = caster->GetZone()->GetClientBySpawn(caster);
		if(client)
			SendSpellBookUpdate(client);
	}
}

void SpellProcess::RemoveSpellFromQueue(Entity* caster, bool hostile_only) {
	if (caster && spell_que.count(caster) > 0) {
		Spell* spell = spell_que.Get(caster);
		if (spell) {
			bool remove = true;
			if (hostile_only && spell->GetSpellData()->target_type != SPELL_TARGET_OTHER)
				remove = false;
			if (remove) {
				spell_que.erase(caster);
				((Player*)caster)->UnQueueSpell(spell);
				Client* client = caster->GetZone()->GetClientBySpawn(caster);
				if (client)
					SendSpellBookUpdate(client);
			}
		}
	}
}

void SpellProcess::CheckSpellQueue(Entity* caster){
	if(caster && caster->IsPlayer()){
		if(spell_que.count(caster) > 0)
			RemoveSpellFromQueue(spell_que.Get(caster), caster);	
	}
}

void SpellProcess::CheckSpellQueue(Spell* spell, Entity* caster) {
	if (!caster->IsPlayer())
		return;

	Spell* existing_spell = nullptr;

	if (spell_que.count(caster) > 0) {
		existing_spell = spell_que.Get(caster);
		RemoveSpellFromQueue(existing_spell, caster);
	}

	if (!existing_spell || existing_spell != spell) {
		AddSpellToQueue(spell, caster);
	}
}

void SpellProcess::SendSpellBookUpdate(Client* client){
	if(client){
		EQ2Packet* app = client->GetPlayer()->GetSpellBookUpdatePacket(client->GetVersion());
		if(app)
			client->QueuePacket(app);
	}
}

LuaSpell* SpellProcess::GetLuaSpell(Entity* caster){
	LuaSpell* spell = 0;
	if(caster && cast_timers.size() > 0){		
		CastTimer* cast_timer = 0;
		MutexList<CastTimer*>::iterator itr = cast_timers.begin();
		while(itr.Next()){
			 cast_timer = itr->value;
			 if(cast_timer && cast_timer->spell && cast_timer->spell->caster == caster){
				spell = cast_timer->spell;
				break;
			 }
		}
	}
	return spell;
}

Spell* SpellProcess::GetSpell(Entity* caster){
	Spell* spell = 0;
	if(cast_timers.size() > 0){		
		CastTimer* cast_timer = 0;
		MutexList<CastTimer*>::iterator itr = cast_timers.begin();
		while(itr.Next()){
			 cast_timer = itr->value;
			 if(cast_timer && cast_timer->spell && cast_timer->spell->caster == caster){
				spell = cast_timer->spell->spell;
				break;
			 }
		}
	}
	return spell;
}

Spawn* SpellProcess::GetSpellTarget(Entity* caster){
	Spawn* target = 0;
	if(cast_timers.size() > 0){		
		CastTimer* cast_timer = 0;
		MutexList<CastTimer*>::iterator itr = cast_timers.begin();
		while(itr.Next()){
			 cast_timer = itr->value;
			 if(cast_timer && cast_timer->spell->caster == caster){
				target = caster->GetZone()->GetSpawnByID(cast_timer->spell->initial_target);
				break;
			 }
		}
	}
	return target;
}

void SpellProcess::SetInitialTarget(LuaSpell* lua_spell, Spawn* target) {
	if (!lua_spell || !lua_spell->spell || !lua_spell->caster || !target)
		return;

	lua_spell->initial_target = target->GetID();

	int8 target_type = lua_spell->spell->GetSpellData()->target_type;

	if (lua_spell->caster->IsPlayer() && (target_type == SPELL_TARGET_OTHER || target_type == SPELL_TARGET_OTHER_CORPSE || target_type == SPELL_TARGET_GROUP_CORPSE || target_type == SPELL_TARGET_OTHER_GROUP_AE)) {
		if (target->HasTarget()) {
			Spawn* secondary_target = target->GetTarget();

			if ((lua_spell->spell->GetSpellData()->friendly_spell && lua_spell->caster->IsHostile(target) && !lua_spell->caster->IsHostile(secondary_target)) ||
				(!lua_spell->spell->GetSpellData()->friendly_spell && !lua_spell->caster->IsHostile(target) && lua_spell->caster->IsHostile(secondary_target))) {
				lua_spell->initial_target = secondary_target->GetID();
			} else {
				lua_spell->initial_target = target->GetID();
			}
		} else if (lua_spell->spell->GetSpellData()->friendly_spell && lua_spell->caster->IsHostile(target)) {
			lua_spell->initial_target = lua_spell->caster->GetID();
		}
	} else if (target_type == SPELL_TARGET_GROUP_AE || target_type == SPELL_TARGET_RAID_AE) {
		lua_spell->initial_target = lua_spell->caster->GetID();
	} else if (target_type == SPELL_TARGET_SELF) {
		lua_spell->initial_target = lua_spell->caster->GetID();
	}
}

bool SpellProcess::CanCast(LuaSpell* lua_spell, bool harvest_spell = false) {
	Entity* caster = lua_spell->caster;
	Client* client = nullptr;
	Spell* spell = lua_spell->spell;
	ZoneServer* zone = lua_spell->caster->GetZone();

	int8 target_type = spell->GetSpellData()->target_type;

	if (caster->IsPlayer() && zone)
		client = caster->GetZone()->GetClientBySpawn(caster);

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

	if (!CheckPower(lua_spell)) {
		zone->SendSpellFailedPacket(client, SPELL_ERROR_NOT_ENOUGH_POWER);
		return false;
	}

	if (!CheckHP(lua_spell)) {
		zone->SendSpellFailedPacket(client, SPELL_ERROR_NOT_ENOUGH_HEALTH);
		return false;
	}

	if (!CheckSavagery(lua_spell)) {
		zone->SendSpellFailedPacket(client, SPELL_ERROR_NOT_ENOUGH_SAVAGERY);
		return false;
	}

	if (!CheckDissonance(lua_spell)) {
		zone->SendSpellFailedPacket(client, SPELL_ERROR_NOT_ENOUGH_DISSONANCE);
		return false;
	}

	if (!CheckConcentration(lua_spell)) {
		zone->SendSpellFailedPacket(client, SPELL_ERROR_NOT_ENOUGH_CONC);
		return false;
	}

	if (spell->GetSpellData()->max_aoe_targets == 0) {
		Spawn* target = caster->GetZone()->GetSpawnByID(lua_spell->initial_target);

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

	return true;
}

void SpellProcess::ProcessSpell(ZoneServer* zone, Spell* spell, Entity* caster, Spawn* target, bool lock, bool harvest_spell) {
	if (spell && caster) {
		Client* client = nullptr;
		LuaSpell* lua_spell = nullptr;

		int8 target_type = spell->GetSpellData()->target_type;

		if (lua_interface)
			lua_spell = lua_interface->GetSpell(spell->GetSpellData()->lua_script.c_str());

		if (!lua_spell)
			return;

		if (!target)
			target = caster;

		lua_spell->caster = caster;
		lua_spell->spell = spell;

		SetInitialTarget(lua_spell, target);

		if (CanCast(lua_spell, harvest_spell)) {
			if (!harvest_spell) {
				GetSpellTargets(lua_spell);
			} else {
				lua_spell->MSpellTargets.writelock(__FUNCTION__, __LINE__);
				lua_spell->targets.push_back(lua_spell->initial_target);
				lua_spell->MSpellTargets.releasewritelock(__FUNCTION__, __LINE__);
			}

			if (lua_spell->targets.size() == 0 && spell->GetSpellData()->max_aoe_targets == 0) {
				safe_delete(lua_spell);
				return;
			}

			if (caster->IsPlayer() && zone)
				client = caster->GetZone()->GetClientBySpawn(caster);

			if (lua_interface) {
				bool result = false;

				lua_interface->AddSpawnPointers(lua_spell, false, true);

				if (lua_pcall(lua_spell->state, 2, 2, 0) == 0) {
					result = lua_interface->GetBooleanValue(lua_spell->state, 1);
					int8 error = lua_interface->GetInt8Value(lua_spell->state, 2) == 0 ? SPELL_ERROR_CANNOT_PREPARE : lua_interface->GetInt8Value(lua_spell->state, 2);
					lua_interface->ResetFunctionStack(lua_spell->state);

					if (!result) {
						zone->SendSpellFailedPacket(client, error);
						safe_delete(lua_spell);
						return;
					}
				} else {
					lua_interface->ResetFunctionStack(lua_spell->state);
				}
			}

			LockAllSpells(client);
			SendStartCast(lua_spell, client);
				
			if (caster->IsInvis())
				caster->CancelAllStealth();

			int16 cast_time = spell->GetModifiedCastTime(caster);

			if (cast_time > 0) {
				CastTimer* cast_timer = new CastTimer;
				cast_timer->entity_command = 0;
				cast_timer->spell = lua_spell;
				cast_timer->spell->caster = caster;
				cast_timer->delete_timer = false;
				cast_timer->timer = new Timer(cast_time * 10);
				cast_timer->zone = zone;
				cast_timers.Add(cast_timer);

				if (caster) {
					caster->IsCasting(true);
				}
			} else {
				if (!CastProcessedSpell(lua_spell)) {
					safe_delete(lua_spell);
					return;
				}
			}

			if (caster) {
				caster->GetZone()->SendCastSpellPacket(lua_spell, caster, cast_time);
			}
		} else {
			safe_delete(lua_spell);
			return;
		}
	}
}

void SpellProcess::ProcessEntityCommand(ZoneServer* zone, EntityCommand* entity_command, Entity* caster, Spawn* target, bool lock) {
	if (zone && entity_command && caster && target && !target->IsPlayer()) {
		Client* client = zone->GetClientBySpawn(caster);
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

				CastTimer* cast_timer = new CastTimer;
				cast_timer->caster = client;
				cast_timer->entity_command = entity_command;
				cast_timer->spell = 0;
				cast_timer->delete_timer = false;
				cast_timer->timer = new Timer(entity_command->cast_time * 10);
				cast_timer->zone = zone;
				cast_timers.Add(cast_timer);
				caster->IsCasting(true);
			}
		}
		else if (!CastProcessedEntityCommand(entity_command, client))
			return;
		if (entity_command && entity_command->spell_visual > 0)
			caster->GetZone()->SendCastEntityCommandPacket(entity_command, caster->GetID(), target->GetID());
	}
}

bool SpellProcess::CastProcessedSpell(LuaSpell* spell, bool passive) {
	if (!spell || !spell->caster || !spell->spell || spell->interrupted)
		return false;

	Client* client = 0;
	bool hit_target = false;
	bool living_target = false;

	if (spell->caster && spell->caster->IsPlayer())
		client = spell->caster->GetZone()->GetClientBySpawn(spell->caster);

	if (spell->spell->GetSpellData()->max_aoe_targets > 0 && spell->targets.size() == 0) {
		GetSpellTargetsTrueAOE(spell);

		if (spell->targets.size() == 0) {
			spell->caster->GetZone()->SendSpellFailedPacket(client, SPELL_ERROR_NO_TARGETS_IN_RANGE);
			UnlockAllSpells(client);
			return false;
		}
	}

	if (spell->targets.size() > 0) {
		ZoneServer* zone = spell->caster->GetZone();
		Spawn* target = 0;

		spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
		for (int32 i = 0; i < spell->targets.size(); i++) {
			bool processedSpell = false;
			int8 damage_type = 0;
			int8 hit_result = 0;

			target = zone->GetSpawnByID(spell->targets[i]);

			if (!target)
				continue;

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

						if (spell->spell->GetSpellData()->call_frequency > 0)
							spell->timer.SetTimer(spell->spell->GetSpellData()->call_frequency * 100);
						else
							spell->timer.SetTimer(spell->spell->GetSpellData()->duration1 * 100);
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
			
			active_spells.Add(spell);
		}

		if (spell->num_triggers > 0)
			ClientPacketFunctions::SendMaintainedExamineUpdate(client, spell->slot_pos, spell->num_triggers, 0);

		if (client && client->GetPlayer()->GetWard(spell) && client->GetPlayer()->GetWard(spell)->DamageLeft > 0)
			ClientPacketFunctions::SendMaintainedExamineUpdate(client, spell->slot_pos, client->GetPlayer()->GetWard(spell)->DamageLeft, 1);

		spell->caster->GetZone()->TriggerCharSheetTimer();
	}

	if (!passive)
		SendFinishedCast(spell, client);

	if (!spell->spell->GetSpellData()->friendly_spell) {
		if (spell->caster->IsPlayer()) {
			static_cast<Player*>(spell->caster)->InCombat(true);
			if (!static_cast<Player*>(spell->caster)->GetRangeAttack()) {
				static_cast<Player*>(spell->caster)->SetMeleeAttack(true);
			}
		} else {
			spell->caster->InCombat(true);
		}
	}


	/*MutexList<LuaSpell*>::iterator itr = active_spells.begin();
	bool processedSpell = false;
	LuaSpell* replace_spell = 0;
	// Check to see if we already casted this spell and it is in the active_spells list
	/*while (itr.Next()) {
		LuaSpell* luaspell = itr.value;
		// Check to see if this is the same spell by comparing caster, spell id, and spell tier
		if (luaspell->caster == spell->caster && luaspell->spell->GetSpellID() == spell->spell->GetSpellID() && luaspell->spell->GetSpellTier() == spell->spell->GetSpellTier() && luaspell->targets.size() == spell->targets.size()) {
			vector<Spawn*>::iterator itr;
			vector<Spawn*>::iterator itr2;
			bool all_match = true;
			// compare the target vector to make sure this is the same spell
			for (itr = luaspell->targets.begin(); itr != luaspell->targets.end(); itr++) {
				bool match = false;
				for (itr2 = spell->targets.begin(); itr2 != spell->targets.end(); itr2++) {
					if ((*itr) == (*itr2)) {
						match = true;
						break;
					}
				}
				if (!match) {
					all_match = false;
					break;
				}
			}
			if (all_match) {
				// set a pointer to replace the spell in the active spell list so we don't have to run this loop again
				replace_spell = luaspell;
				// if friendly skip the calling of the lua function as they have already been called for the previous version of this spell
				if (spell->spell->GetSpellData()->friendly_spell == 1)
					processedSpell = true;

				break;
			}
		}
	}


	if(processedSpell){
		for (int32 i = 0; i < spell->targets.size(); i++) {
			target = zone->GetSpawnByID(spell->targets[i]);
			if (!target)
				continue;
			if(spell->spell->GetSpellData()->success_message.length() > 0){
				if(client){
					string success_message = spell->spell->GetSpellData()->success_message;
					if(success_message.find("%t") != string::npos)
						success_message.replace(success_message.find("%t"), 2, target->GetName());
					client->Message(CHANNEL_COLOR_SPELL, success_message.c_str());
				}
			}
			if(spell->spell->GetSpellData()->effect_message.length() > 0){
				string effect_message = spell->spell->GetSpellData()->effect_message;
				if(effect_message.find("%t") < 0xFFFFFFFF)
					effect_message.replace(effect_message.find("%t"), 2, target->GetName());
				if (effect_message.find("%c") != string::npos)
					effect_message.replace(effect_message.find("%c"), 2, spell->caster->GetName());
				spell->caster->GetZone()->SimpleMessage(CHANNEL_COLOR_SPELL_EFFECT, effect_message.c_str(), target, 50);
			}
			target->GetZone()->CallSpawnScript(target, SPAWN_SCRIPT_CASTED_ON, spell->caster, spell->spell->GetName());
		}
	}
	else{
		if (!passive)
			SendFinishedCast(spell, client);
		return false;
	}
	if(!spell->resisted && (spell->spell->GetSpellDuration() > 0 || spell->spell->GetSpellData()->duration_until_cancel)) {
		for (int32 i = 0; i < spell->targets.size(); i++) {
			target = zone->GetSpawnByID(spell->targets.at(i));
			if (i == 0 && !spell->spell->GetSpellData()->not_maintained)
				spell->caster->AddMaintainedSpell(spell);
			SpellEffects* effect = ((Entity*)target)->GetSpellEffect(spell->spell->GetSpellID());
			if (effect && effect->tier > spell->spell->GetSpellTier()) {
				if(client) {
					spell->caster->GetZone()->SendSpellFailedPacket(client, SPELL_ERROR_TAKE_EFFECT_MOREPOWERFUL);
					client->SimpleMessage(CHANNEL_COLOR_YELLOW, "The spell did not take hold as the target already has a better spell of this type.");
				}
			}
			else{
				((Entity*)target)->AddSpellEffect(spell);
				if(spell->spell->GetSpellData()->det_type > 0)
					((Entity*)target)->AddDetrimentalSpell(spell);
			}
		}

		if (replace_spell) {
			active_spells.Remove(replace_spell);
			active_spells.Add(spell);
		}
		else {
			active_spells.Add(spell);
		}

		if (spell->num_triggers > 0)
			ClientPacketFunctions::SendMaintainedExamineUpdate(client, spell->slot_pos, spell->num_triggers, 0);
		if (spell->damage_remaining > 0)
			ClientPacketFunctions::SendMaintainedExamineUpdate(client, spell->slot_pos, spell->damage_remaining, 1);

		spell->caster->GetZone()->TriggerCharSheetTimer();
	}
	spell->num_calls = 1;
	if(!spell->resisted && spell->spell->GetSpellData()->duration1 > 0){
		spell->timer.Start();
		if(spell->spell->GetSpellData()->call_frequency > 0)
			spell->timer.SetTimer(spell->spell->GetSpellData()->call_frequency*100);
		else
			spell->timer.SetTimer(spell->spell->GetSpellData()->duration1*100);
		if (active_spells.count(spell) > 0) {
			active_spells.Add(spell);
		}
	}

	// if the caster is a player and the spell is a tradeskill spell check for a tradeskill event
	if (client && spell->spell->GetSpellData()->spell_book_type == SPELL_BOOK_TYPE_TRADESKILL)
		client->GetCurrentZone()->GetTradeskillMgr()->CheckTradeskillEvent(client, spell->spell->GetSpellData()->icon);

	if (spell->spell->GetSpellData()->friendly_spell && zone->GetSpawnByID(spell->initial_target))
		spell->caster->CheckProcs(PROC_TYPE_BENEFICIAL, zone->GetSpawnByID(spell->initial_target));

	/*
	// Check HO's
	if (client) {
		MSoloHO.writelock(__FUNCTION__, __LINE__);
		MGroupHO.writelock(__FUNCTION__, __LINE__);
		if (m_soloHO.count(client) > 0) {
			bool match = false;
			LogWrite(SPELL__ERROR, 0, "HO", "target = %u", m_soloHO[client]->GetTarget());
			spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
			for (int8 i = 0; i < spell->targets.size(); i++) {
				LogWrite(SPELL__ERROR, 0, "HO", "%u", spell->targets.at(i));
				if (spell->targets.at(i) == m_soloHO[client]->GetTarget()) {
					match = true;
					LogWrite(SPELL__ERROR, 0, "HO", "match found");
					break;
				}
			}
			spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);
			if (match && m_soloHO[client]->UpdateHeroicOP(spell->spell->GetSpellIconHeroicOp())) {
				ClientPacketFunctions::SendHeroicOPUpdate(client, m_soloHO[client]);
				if (m_soloHO[client]->GetComplete() > 0) {
					Spell* ho_spell = master_spell_list.GetSpell(m_soloHO[client]->GetWheel()->spell_id, 1);
					if (ho_spell)
						client->GetCurrentZone()->ProcessSpell(ho_spell, client->GetPlayer(), spell->caster->GetZone()->GetSpawnByID(m_soloHO[client]->GetTarget()));
					else
						LogWrite(SPELL__ERROR, 0, "HO", "Invalid spell for a HO, spell id = %u", m_soloHO[client]->GetWheel()->spell_id);

					safe_delete(m_soloHO[client]);
					m_soloHO.erase(client);
				}
			}
		}
		else if (client->GetPlayer()->GetGroupMemberInfo() && m_groupHO.count(client->GetPlayer()->GetGroupMemberInfo()->group_id)) {
			int32 group_id = client->GetPlayer()->GetGroupMemberInfo()->group_id;
			spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
			if (spell->targets.at(0) == m_groupHO[group_id]->GetTarget() && m_groupHO[group_id]->UpdateHeroicOP(spell->spell->GetSpellIconHeroicOp())) {
				spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);

				world.GetGroupManager()->GroupLock(__FUNCTION__, __LINE__);
				deque<GroupMemberInfo*>::iterator itr;
				deque<GroupMemberInfo*>* members = world.GetGroupManager()->GetGroupMembers(group_id);
				for (itr = members->begin(); itr != members->end(); itr++) {
					if ((*itr)->client)
						ClientPacketFunctions::SendHeroicOPUpdate((*itr)->client, m_groupHO[group_id]);
				}
				world.GetGroupManager()->ReleaseGroupLock(__FUNCTION__, __LINE__);

				if (m_groupHO[group_id]->GetComplete() > 0) {
					Spell* ho_spell = master_spell_list.GetSpell(m_groupHO[group_id]->GetWheel()->spell_id, 1);
					if (ho_spell)
						client->GetCurrentZone()->ProcessSpell(ho_spell, client->GetPlayer(), spell->caster->GetZone()->GetSpawnByID(spell->targets.at(0)));
					else
						LogWrite(SPELL__ERROR, 0, "HO", "Invalid spell for a HO, spell id = %u", m_groupHO[group_id]->GetWheel()->spell_id);

					safe_delete(m_groupHO[group_id]);
					m_groupHO.erase(group_id);
				}
			}
			else
				spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);
		}
		MGroupHO.releasewritelock(__FUNCTION__, __LINE__);
		MSoloHO.releasewritelock(__FUNCTION__, __LINE__);
	}
	

	if (spell->spell->GetSpellData()->friendly_spell == 1 && spell->initial_target)
		spell->caster->CheckProcs(PROC_TYPE_BENEFICIAL, spell->caster->GetZone()->GetSpawnByID(spell->initial_target));

	if (!passive)
		SendFinishedCast(spell, client);*/

	return true;
}

bool SpellProcess::CastProcessedEntityCommand(EntityCommand* entity_command, Client* client) {
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
		LuaSpell* spell = GetLuaSpell(caster);

		if (spell && ((from_movement && !spell->spell->GetSpellData()->cast_while_moving) || (!from_movement && spell->spell->GetSpellData()->interruptable) ||
			cancel)) 
		{
			Spawn* target = GetSpellTarget(caster);
			InterruptStruct* interrupt = new InterruptStruct;
			interrupt->interrupted = caster;
			interrupt->spell = spell;
			interrupt->target = target;
			interrupt->error_code = error_code;
			spell->interrupted = true;
			interrupt_list.Add(interrupt);

			Client* client = 0;
			if(interruptor && interruptor->IsPlayer())
			{
				client = interruptor->GetZone()->GetClientBySpawn(interruptor);
				client->Message(CHANNEL_COLOR_SPELL_INTERRUPT, "You interrupt %s's ability to cast!", interruptor->GetName());
			}
			
		}
	}
}

void SpellProcess::RemoveSpellTimersFromSpawn(Spawn* spawn, bool remove_all, bool delete_recast){
	int32 i = 0;
	if(cast_timers.size() > 0){		
		CastTimer* cast_timer = 0;
		MutexList<CastTimer*>::iterator itr = cast_timers.begin();
		while(itr.Next()){
			 cast_timer = itr->value;
			 if(cast_timer && cast_timer->spell && cast_timer->spell->caster == spawn){
				cast_timer->spell->caster = 0;
				cast_timer->delete_timer = true;
			 }
		}
	}
	if(remove_all){				
		LuaSpell* spell = 0;
		MutexList<LuaSpell*>::iterator itr = active_spells.begin();
		while(itr.Next()){
			spell = itr->value;
			if (!spell)
				continue;
			if (spell->spell->GetSpellData()->persist_though_death && spell->caster->GetZone()->GetClientBySpawn(spell->caster)->IsConnected())
				continue;
			if (spell->caster == spawn && spell->caster != spell->caster->GetZone()->GetSpawnByID(spell->initial_target) && spell->spell->GetSpellData()->friendly_spell && spell->spell->GetSpellData()->target_type == SPELL_TARGET_OTHER && !spell->spell->GetSpellData()->group_spell) {
				spell->caster = spell->caster->GetZone()->unknown_spawn;
				continue;
			}
			if(spell->caster == spawn){
				DeleteCasterSpell(spell);
				continue;
			}
			spell->MSpellTargets.readlock(__FUNCTION__, __LINE__);
			for (i = 0; i < spell->targets.size(); i++){
				if (spawn->GetID() == spell->targets.at(i)){
					if (spawn->IsEntity()) {
						static_cast<Entity*>(spawn)->RemoveSpellEffect(spell);
						static_cast<Entity*>(spawn)->RemoveDetrimentalSpell(spell);
					}
					RemoveTargetFromSpell(spell, spawn);
					break;
				}
			}
			spell->MSpellTargets.releasereadlock(__FUNCTION__, __LINE__);
		}
		MRecastTimers.writelock(__FUNCTION__, __LINE__);
		if(recast_timers.size() > 0 && delete_recast){			
			vector<RecastTimer*>::iterator itr = recast_timers.begin();
			while (itr != recast_timers.end()) {
				RecastTimer* recast_timer = *itr;
				if (recast_timer && recast_timer->caster == spawn) {
					safe_delete(recast_timer->timer);
					itr = recast_timers.erase(itr);
				} else {
					itr++;
				}
			}
		}
		MRecastTimers.releasewritelock(__FUNCTION__, __LINE__);
		if(spell_que.size() > 0 && spawn->IsEntity()){
			spell_que.erase((Entity*)spawn);
		}
		if(interrupt_list.size() > 0){			
			InterruptStruct* interrupt = 0;
			MutexList<InterruptStruct*>::iterator itr = interrupt_list.begin();
			while(itr.Next()){
				interrupt = itr->value;
				if(interrupt && interrupt->interrupted == spawn){
					interrupt_list.Remove(interrupt);
				}
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
								Client* client = caster->GetZone()->GetClientBySpawn(group_member);

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
						Entity* group_member = 0;
						for(itr = members->begin(); itr != members->end(); itr++) {
							group_member = (*itr)->member;
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
			if (luaspell->caster->GetDistance(luaspell->caster->GetTarget()) <= luaspell->spell->GetSpellData()->radius)
				luaspell->initial_target = luaspell->caster->GetTarget()->GetID();
		}
		int32 ignore_target = 0;
		vector<Spawn*> spawns = luaspell->caster->GetZone()->GetAttackableSpawnsByDistance(luaspell->caster, luaspell->spell->GetSpellData()->radius);
		luaspell->MSpellTargets.writelock(__FUNCTION__, __LINE__);
		for (int8 i = 0; i < spawns.size(); i++) {
			Spawn* spawn = spawns.at(i);
			if (i == 0){
				if (luaspell->initial_target && luaspell->caster->GetID() != luaspell->initial_target){
					//this is the "Direct" target and aoe can't be avoided
					luaspell->targets.push_back(luaspell->initial_target);
					ignore_target = luaspell->initial_target;
				}
				if (luaspell->targets.size() >= luaspell->spell->GetSpellData()->max_aoe_targets)
					break;
			}
			//If we have already added this spawn, check the next spawn in the list
			if (spawn && spawn->GetID() == ignore_target){
				i++;
				if (i < spawns.size())
					spawn = spawns.at(i);
				else
					break;
			}

			if (spawn) {
				if (static_cast<Entity*>(spawn)->IsAOEImmune() || (luaspell->caster->IsNPC() && spawn->IsNPC()))
					continue;

				luaspell->targets.push_back(spawn->GetID());
			}

			if (luaspell->targets.size() >= luaspell->spell->GetSpellData()->max_aoe_targets)
				break;
		}
		luaspell->MSpellTargets.releasewritelock(__FUNCTION__, __LINE__);
	}
	if (luaspell->targets.size() > 20)
		LogWrite(SPELL__DEBUG, 0, "Spell", "Warning in  SpellProcess::GetSpellTargetsTrueAOE Size of targets array is %u", luaspell->targets.size());
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
			if ((*itr) && (*itr)->deleteWhenDone && lua_interface) {
				lua_interface->AddPendingSpellDelete(timer->spell);
			}
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
			//ProcessSpell((*itr)->spell, , false, (*itr)->customFunction.c_str(), (*itr));
		}
	}
	MSpellScriptTimers.releasereadlock(__FUNCTION__, __LINE__);

	for (itr = temp_list.begin(); itr != temp_list.end(); itr++) {
		RemoveSpellScriptTimer(*itr);
		safe_delete(*itr);
	}
}

bool SpellProcess::SpellScriptTimersHasSpell(LuaSpell* spell) {
	bool ret = false;
	vector<SpellScriptTimer*>::iterator itr;

	MSpellScriptTimers.readlock(__FUNCTION__, __LINE__);
	for (itr = m_spellScriptList.begin(); itr != m_spellScriptList.end(); itr++) {
		SpellScriptTimer* timer = *itr;
		if (timer && timer->spell == spell) {
			ret = true;
			break;
		}
	}
	MSpellScriptTimers.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}

void SpellProcess::ClearSpellScriptTimerList() {
	vector<SpellScriptTimer*>::iterator itr;
	MSpellScriptTimers.writelock(__FUNCTION__, __LINE__);

	for(itr = m_spellScriptList.begin(); itr != m_spellScriptList.end(); itr++) {
		if ((*itr) && (*itr)->deleteWhenDone && lua_interface)
			lua_interface->AddPendingSpellDelete((*itr)->spell);

		safe_delete((*itr));
	}

	m_spellScriptList.clear();
	MSpellScriptTimers.releasewritelock(__FUNCTION__, __LINE__);
}

void SpellProcess::RemoveTargetFromSpell(LuaSpell* spell, Spawn* target){
	if (!spell || !target)
		return;

	MRemoveTargetList.writelock(__FUNCTION__, __LINE__);
	if (!remove_target_list[spell])
		remove_target_list[spell] = new vector<Spawn*>;
	remove_target_list[spell]->push_back(target);
	MRemoveTargetList.releasewritelock(__FUNCTION__, __LINE__);
}

void SpellProcess::CheckRemoveTargetFromSpell(LuaSpell* spell, bool allow_delete){
	if (!spell)
		return;

	if (remove_target_list.size() > 0){
		map<LuaSpell*, vector<Spawn*>*>::iterator remove_itr;
		vector<Spawn*>::iterator remove_target_itr;
		vector<int32>::iterator target_itr;
		vector<int32>* targets;
		vector<Spawn*>* remove_targets = 0;
		Spawn* remove_spawn = 0;
		bool should_delete = false;

		MRemoveTargetList.writelock(__FUNCTION__, __LINE__);
		for (remove_itr = remove_target_list.begin(); remove_itr != remove_target_list.end(); remove_itr++){
			if (remove_itr->first == spell){
				targets = &spell->targets;
				remove_targets = remove_itr->second;
				if (remove_targets && targets){
					for (remove_target_itr = remove_targets->begin(); remove_target_itr != remove_targets->end(); remove_target_itr++){
						remove_spawn = (*remove_target_itr);
						if (remove_spawn){
							spell->MSpellTargets.writelock(__FUNCTION__, __LINE__);
							for (target_itr = targets->begin(); target_itr != targets->end(); target_itr++){
								if (remove_spawn->GetID() == (*target_itr)){
									targets->erase(target_itr);
									if (remove_spawn->IsEntity()) {
										static_cast<Entity*>(remove_spawn)->RemoveEffectsFromLuaSpell(spell);
										lua_interface->RemoveSpell(spell, remove_spawn, true, false);
									}
									break;
								}
							}
							spell->MSpellTargets.releasewritelock(__FUNCTION__, __LINE__);
							if (targets->size() == 0 && allow_delete){
								should_delete = true;
								break;
							}
						}
					}
				}
				break;
			}
		}
		remove_target_list.erase(spell);
		if (remove_targets)
			remove_targets->clear();
		safe_delete(remove_targets);
		MRemoveTargetList.releasewritelock(__FUNCTION__, __LINE__);
		if (should_delete)
			DeleteCasterSpell(spell);
	}
}

bool SpellProcess::AddHO(Client* client, HeroicOP* ho) {
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
	map<Client*, HeroicOP*>::iterator itr = m_soloHO.begin();
	map<Client*, HeroicOP*>::iterator delete_itr;
	while (itr != m_soloHO.end()) {
		if (itr->second->GetTarget() == spawn_id) {
			itr->second->SetComplete(1);
			ClientPacketFunctions::SendHeroicOPUpdate(itr->first, itr->second);
			delete_itr = itr;
			safe_delete(itr->second);
			itr++;
			m_soloHO.erase(delete_itr);
		}
		else
			itr++;
	}
	MSoloHO.releasewritelock(__FUNCTION__, __LINE__);

	// Check Group HO's
	MGroupHO.writelock(__FUNCTION__, __LINE__);
	map<int32, HeroicOP*>::iterator itr2 = m_groupHO.begin();
	map<int32, HeroicOP*>::iterator delete_itr2;
	while (itr2 != m_groupHO.end()) {
		if (itr2->second->GetTarget() == spawn_id) {
			itr2->second->SetComplete(1);

			world.GetGroupManager()->GroupLock(__FUNCTION__ , __LINE__);
			deque<GroupMemberInfo*>::iterator itr3;
			deque<GroupMemberInfo*>* members = world.GetGroupManager()->GetGroupMembers(itr2->first);
			for (itr3 = members->begin(); itr3 != members->end(); itr3++) {
				if ((*itr3)->client)
					ClientPacketFunctions::SendHeroicOPUpdate((*itr3)->client, itr2->second);
			}
			world.GetGroupManager()->ReleaseGroupLock(__FUNCTION__, __LINE__);

			delete_itr2 = itr2;
			safe_delete(itr2->second);
			itr2++;
			m_groupHO.erase(delete_itr2);
		}
		else
			itr2++;
	}
	MGroupHO.releasewritelock(__FUNCTION__, __LINE__);
}

void SpellProcess::AddSpellCancel(LuaSpell* spell){
	MSpellCancelList.writelock(__FUNCTION__, __LINE__);
	SpellCancelList.push_back(spell);
	MSpellCancelList.releasewritelock(__FUNCTION__, __LINE__);
}