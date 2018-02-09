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
#ifndef __EQ2_SPELL_PROCESS__
#define __EQ2_SPELL_PROCESS__
#include "client.h"
#include "Spells.h"
#include "zoneserver.h"
#include "LuaInterface.h"
#include "MutexMap.h"
#include "MutexList.h"
#include "World.h"
#include "HeroicOp/HeroicOp.h"

#define MODIFY_HEALTH					1
#define MODIFY_FOCUS					2
#define MODIFY_DEFENSE					3
#define MODIFY_POWER					4
#define MODIFY_SPEED					5
#define MODIFY_INT						6
#define MODIFY_WIS						7
#define MODIFY_STR						8
#define MODIFY_AGI						9
#define MODIFY_STA						10
#define MODIFY_COLD_RESIST				11
#define MODIFY_HEAT_RESIST				12
#define MODIFY_DISEASE_RESIST			13
#define MODIFY_POISON_RESIST			14
#define MODIFY_MAGIC_RESIST				15
#define MODIFY_MENTAL_RESIST			16
#define MODIFY_DIVINE_RESIST			17
#define MODIFY_ATTACK					18
#define MODIFY_MITIGATION				19
#define MODIFY_AVOIDANCE				20
#define MODIFY_CONCENTRATION			21
#define MODIFY_EXP						22
#define MODIFY_FACTION					23
#define CHANGE_SIZE						24
#define CHANGE_RACE						25
#define CHANGE_LOCATION					26
#define CHANGE_ZONE						27
#define CHANGE_PREFIX_TITLE				28
#define CHANGE_DEITY					29
#define CHANGE_LAST_NAME				30
#define MODIFY_HASTE					31
#define MODIFY_SKILL					32
#define CHANGE_TARGET					33
#define CHANGE_LEVEL					34
#define MODIFY_SPELL_CAST_TIME			35
#define MODIFY_SPELL_POWER_REQ			36
#define MODIFY_SPELL_HEALTH_REQ			37
#define MODIFY_SPELL_RECOVERY			38
#define MODIFY_SPELL_RECAST_TIME		39
#define MODIFY_SPELL_RADIUS				40
#define MODIFY_SPELL_AOE_TARGETS		41
#define MODIFY_SPELL_RANGE				42
#define MODIFY_SPELL_DURATION			43
#define MODIFY_SPELL_RESISTIBILITY		44
#define MODIFY_DAMAGE					45
#define MODIFY_DELAY					46
#define MODIFY_TRADESKILL_EXP			47
#define ADD_MOUNT						48
#define REMOVE_MOUNT					49
#define MODIFY_SPELL_CRIT_CHANCE		50
#define MODIFY_CRIT_CHANCE				51
#define SUMMON_ITEM						52
#define MODIFY_JUMP						53
#define MODIFY_FALL_SPEED				54
#define INFLICT_DAMAGE					55
#define ADD_DOT							56
#define REMOVE_DOT						57
#define HEAL_TARGET						58
#define HEAL_AOE						59
#define INFLICT_AOE_DAMAGE				60
#define HEAL_GROUP_AOE					61
#define ADD_AOE_DOT						62
#define REMOVE_AOE_DOT					63
#define ADD_HOT							64
#define REMOVE_HOT						65
#define MODIFY_AGGRO_RANGE				66
#define BLIND_TARGET					67
#define UNBLIND_TARGET					68
#define KILL_TARGET						69
#define RESURRECT_TARGET				70
#define CHANGE_SUFFIX_TITLE				71
#define SUMMON_PET						72
#define MODIFY_HATE						73
#define ADD_REACTIVE_HEAL				74
#define MODIFY_POWER_REGEN				75
#define MODIFY_HP_REGEN					76
#define FEIGN_DEATH						77
#define MODIFY_VISION					78
#define INVISIBILITY					79
#define CHARM_TARGET					80
#define MODIFY_TRADESKILL_DURABILITY	81
#define MODIFY_TRADESKILL_PROGRESS		82


#define ACTIVE_SPELL_NORMAL				0
#define ACTIVE_SPELL_ADD				1
#define ACTIVE_SPELL_REMOVE				2

#define GET_VALUE_BAD_VALUE				0xFFFFFFFF
struct InterruptStruct{
	Spawn*			interrupted;
	Spawn*			target;
	LuaSpell*		spell;
	int16			error_code;
};
struct CastTimer{
	Client*			caster;
	EntityCommand*	entity_command;
	LuaSpell*		spell;
	Timer*			timer;
	ZoneServer*		zone;
	bool			delete_timer;
};
struct CastSpell{
	Entity*			caster;
	Spawn*			target;
	int32			spell_id;
	ZoneServer*		zone;
};
struct RecastTimer{
	Entity*			caster;
	Client*			client;
	Spell*			spell;
	Timer*			timer;
};

/// <summary> Handles all spell casts for a zone, only 1 SpellProcess per zone </summary>
class SpellProcess{
public:
	SpellProcess();
	~SpellProcess();

	/// <summary>Remove all spells from the SpellProcess </summary>
	void RemoveAllSpells();

	/// <summary>Main loop, handles everything (interupts, cast time, recast, ...) </summary>
	void Process();

	/// <summary>Interrupts the caster (creates the InterruptStruct and adds it to a list)</summary>
	/// <param name='caster'>Entity being interrupted</param>
	/// <param name='interruptor'>Spawn that interrupted the caster</param>
	/// <param name='error_code'>The error code</param>
	/// <param name='cancel'>Bool if the spell was cancelled not interrupted</param>
	void Interrupted(Entity* caster, Spawn* interruptor, int16 error_code, bool cancel = false, bool from_movement = false);

	/// <summary>Does all the checks and actually casts the spell</summary>
	/// <param name='zone'>The current ZoneServer</param>
	/// <param name='spell'>The Spell to cast</param>
	/// <param name='caster'>The Entity casting the spell</param>
	/// <param name='target'>The target(Spawn) of the spell</param>
	/// <param name='lock'>??? not currently used</param>
	/// <param name='harvest_spell'>Is this a harvest spell?</param>
	void ProcessSpell(ZoneServer* zone, Spell* spell, Entity* caster, Spawn* target = 0, bool lock = true, bool harvest_spell = false);

	/// <summary>Cast an EntityCommand (right click menu)</summary>
	/// <param name='zone'>The current ZoneServer</param>
	/// <param name='entity_command'>the EntityCommand to cast</param>
	/// <param name='caster'>The Entity casting the EntityCommand</param>
	/// <param name='target'>The target(Spawn*) of the EntityCommand</param>
	/// <param name='lock'>??? not currently used</param>
	void ProcessEntityCommand(ZoneServer* zone, EntityCommand* entity_command, Entity* caster, Spawn* target, bool lock = true);

	/// <summary>Checks to see if the caster has enough power and takes it</summary>
	/// <param name='spell'>LuaSpell to check and take power for (LuaSpell contains the caster)</param>
	/// <returns>True if caster had enough power</returns>
	bool TakePower(LuaSpell* spell);

	/// <summary>Check to see if the caster has enough power to cast the spell</summary>
	/// <param name='spell'>LuaSpell to check (LuaSpell contains the caster)</param>
	/// <returns>True if the caster has enough power</returns>
	bool CheckPower(LuaSpell* spell);

	/// <summary>Check to see if the caster has enough hp and take it</summary>
	/// <param name='spell'>LuaSpell to check and take hp for (LuaSpell contains the caster)</param>
	/// <returns>True if the caster had enough hp</returns>
	bool TakeHP(LuaSpell* spell); 

	/// <summary>Check to see if the caster has enough hp to cast the spell</summary>
	/// <param name='spell'>LuaSpell to check (LuaSpell contains the caster)</param>
	/// <returns>True if the caster had enough hp</returns>
	bool CheckHP(LuaSpell* spell);

	/// <summary>Check to see if the caster has enough concentration available to cast the spell</summary>
	/// <param name='spell'>LuaSpell to check (LuaSpell contains the caster)</param>
	/// <returns>True if the caster has enough concentration</returns>
	bool CheckConcentration(LuaSpell* spell);

	bool CheckSavagery(LuaSpell* spell);
	bool TakeSavagery(LuaSpell* spell);
	bool CheckDissonance(LuaSpell* spell);
	bool AddDissonance(LuaSpell* spell);

	/// <summary>Check to see if the caster has enough concentration available and add to the casters concentration</summary>
	/// <param name='spell'>LuaSpell to check (LuaSpell contains the caster)</param>
	/// <returns>True of the caster had enough concentration</returns>
	bool AddConcentration(LuaSpell* spell);

	/// <summary>Cast the spell, calls ProcessSpell for the given LuaSpell, as well as sends the messages for the spells and calls the casted on function in the targets spawn script</summary>
	/// <param name='spell'>LuaSpell to cast</param>
	/// <param name='passive'>Is this a passive spell being cast?</param>
	/// <returns>True if the spell was casted</returns>
	bool CastProcessedSpell(LuaSpell* spell, bool passive = false);

	/// <summary>Cast the EntityCommand, calls ProcessEntityCommand for the given EntityCommand, as well as sends the messages for the command and calls the casted on function in the targets spawn script</summary>
	/// <param name='entity_command'>EntityCommand to cast</param>
	/// <param name='client'>Client casting the entity command</param>
	/// <returns>True if the spell was casted</returns>
	bool CastProcessedEntityCommand(EntityCommand* entity_command, Client* client);

	/// <summary>Sends the start cast packet for the given client</summary>
	/// <param name='spell'>LuaSpell being cast</param>
	/// <param name='client'>The client casting the spell</param>
	void SendStartCast(LuaSpell* spell, Client* client);

	/// <summary>Send finish cast packet and take power/hp or add conc, also checks for quest updates</summary>
	/// <param name='spell'>LuaSpell that just finished casting</param>
	/// <param name='client'>Client that just finished casting, null if not a player</param>
	void SendFinishedCast(LuaSpell* spell, Client* client);

	/// <summary>Locks all the spells for the given client (shades them all gray)</summary>
	/// <param name='client'>Client to lock the spells for</param>
	void LockAllSpells(Client* client);

	/// <summary>Unlock all the spells for the given client</summary>
	/// <param name='client'>Client to unlock the spells for</param>
	void UnlockAllSpells(Client* client);

	/// <summary>Unlock a single spell for the given client</summary>
	/// <param name='client'>The client to unlock the spell for</param>
	/// <param name='spell'>The spell to unlock</param>
	void UnlockSpell(Client* client, Spell* spell);

	/// <summary>Remove the given spell for the given caster from the SpellProcess</summary>
	/// <param name='caster'>The spawn to remove the spell for</param>
	/// <param name='spell'>The spell to remove</param>
	bool DeleteCasterSpell(Spawn* caster, Spell* spell);

	/// <summary>Remove the given spell from the ZpellProcess</summary>
	/// <param name='spell'>LuaSpell to remove</param>
	bool DeleteCasterSpell(LuaSpell* spell, bool call_remove_function = true);

	/// <summary>Interrupt the spell</summary>
	/// <param name='interrupt'>InterruptStruct that contains all the info</param>
	void CheckInterrupt(InterruptStruct* interrupt);

	/// <summary>Removes the timers for the given spawn</summary>
	/// <param name='spawn'>Spawn to remove the timers for</param>
	/// <param name='remove_all'>Remove all timers (cast, recast, active, queue, interrupted)? If false only cast timers are removed</param>
	void RemoveSpellTimersFromSpawn(Spawn* spawn, bool remove_all = false, bool delete_recast = true);

	/// <summary>Sets the recast timer for the spell </summary>
	/// <param name='spell'>The spell to set the recast for</param>
	/// <param name='caster'>The entity to set the recast for</param>
	/// <param name='timer_override'>Change the recast timer of the spell</param>
	/// <param name='check_linked_timers'>Set the recast on all other spells the player has with the same timer</param>
	void CheckRecast(Spell* spell, Entity* caster, float timer_override = 0, bool check_linked_timers = true);

	/// <summary>Add a spell to the queue for the player</summary>
	/// <param name='spell'>Spell to add</param>
	/// <param name='caster'>Entity's queue to add the spell to, if not a player function does nothing</param>
	void AddSpellToQueue(Spell* spell, Entity* caster);

	/// <summary>Removes a spell from the queue for the player</summary>
	/// <param name='spell'>Spell to remove from the queue</param>
	/// <param name='caster'>Entity's queue to remove the spell from, if not a player function does nothing</param>
	void RemoveSpellFromQueue(Spell* spell, Entity* caster);

	/// <summary>Clear the queue, or clear only hostile spells from the queue</summary>
	/// <param name='caster'>Entity to clear the queue for, if not player function does nothing</param>
	/// <param name='hostile_only'>Set to true to only remove hostile spells, default is false</param>
	void RemoveSpellFromQueue(Entity* caster, bool hostile_only = false);

	/// <summary>Clear the queue for the given caster</summary>
	/// <param name='caster'>Entity to clear the queue for, is not player function does nothing</param>
	void CheckSpellQueue(Entity* caster);

	/// <summary>Check the given enities queue for the spell, if found remove, if not found add</summary>
	/// <param name='spell'>Spell to check for</param>
	/// <param name='caster'>Entity's queue to check, if not player function does nothing</param>
	void CheckSpellQueue(Spell* spell, Entity* caster);

	/// <summary>Checks to see if the entity can cast the spell </summary>
	/// <param name='spell'>The spell being cast</param>
	/// <param name='caster'>The entity casting the spell </param>
	bool IsReady(Spell* spell, Entity* caster);

	/// <summary>Send the spell book update packet to the given client</summary>
	/// <param name='client'>Client to send the packet to</param>
	void SendSpellBookUpdate(Client* client);

	/// <summary>Gets the target of the currently casting spell for the given entity</summary>
	/// <param name='caster'>Entity whos spell we are checking</param>
	/// <returns>Spawn* - the spells target</returns>
	Spawn* GetSpellTarget(Entity* caster);

	void SetInitialTarget(LuaSpell* lua_spell, Spawn* target);
	bool CanCast(LuaSpell* lua_spell, bool harvest_spell);

	/// <summary>Gets the currently casting spell for the given entity</summary>
	/// <param name='caster'>Entity to get the spell for</param>
	/// <returns>Spell* for the currently casting spell</returns>
	Spell* GetSpell(Entity* caster);

	/// <summary>Gets the currently casting LuaSpell for the given entity</summary>
	/// <param name='caster'>Entity to get the LuaSpell for</param>
	/// <returns>LuaSpell* for the currently casting spell</returns>
	LuaSpell* GetLuaSpell(Entity* caster);

	/// <summary>Gets the targets for the spell and adds them to the LuaSpell targets array</summary>
	/// <param name='luaspell'>LuaSpell to get the targets for</param>
	static void GetSpellTargets(LuaSpell* luaspell);

	/// <summary>Gets targets for a true aoe spell (not an encounter ae) and adds them to the LuaSpell targets array</summary>
	/// <param name='luaspell'>LuaSpell to get the targets for</param>
	static void GetSpellTargetsTrueAOE(LuaSpell* luaspell);

	/// <summary>Applies or removes passive spells, bypasses the spell queue and treats the spell as an insta cast spell</summary>
	/// <param name='spell'>The passive spell to apply or remove</param>
	/// <param name='caster'>The Entity to apply or remove the passive spell to</param>
	/// <param name='remove'>Tells the function to remove the spell effects of this passive, default is false</param>
	bool CastPassives(Spell* spell, Entity* caster, bool remove = false);

	/// <summary>Adds a spell script timer to the list</summary>
	/// <param name='timer'>Timer to add</param>
	void AddSpellScriptTimer(SpellScriptTimer* timer);

	/// <summary>Removes a spell script timer from the list</summary>
	/// <param name='timer'>Timer to remove</param>
	void RemoveSpellScriptTimer(SpellScriptTimer* timer);

	/// <summary>Checks the spell script timers</summary>
	void CheckSpellScriptTimers();

	/// <summary>Checks to see if the list has the spell</summary>
	bool SpellScriptTimersHasSpell(LuaSpell* spell);

	void ClearSpellScriptTimerList();

	MutexList<LuaSpell*>* GetActiveSpells() { return &active_spells; }

	void RemoveTargetFromSpell(LuaSpell* spell, Spawn* target);
	void CheckRemoveTargetFromSpell(LuaSpell* spell, bool allow_delete = true);

	/// <summary>Adds a solo HO to the SpellProcess</summary>
	/// <param name='client'>The client who is starting the HO</param>
	/// <param name='ho'>The HO that is being started</param>
	bool AddHO(Client* client, HeroicOP* ho);

	/// <summary>Adds a group HO to the SpellProcess</summary>
	/// <param name='group_id'>ID of the group that is starting the HO</param>
	/// <param name='ho'>The HO that is being started</param>
	bool AddHO(int32 group_id, HeroicOP* ho);

	/// <summary>Stops the HO that targets the given spawn</summary>
	/// <param name='spawn_id'>ID of the spawn targeted by the HO we want to stop</param>
	void KillHOBySpawnID(int32 spawn_id);

	void AddSpellCancel(LuaSpell* spell);

private:
	/// <summary>Sends the spell data to the lua script</summary>
	/// <param name='spell'>LuaSpell to call the lua script for</param>
	/// <param name='first_cast'>No clue, not currently used</param>
	/// <returns>True if the spell script was called successfully</returns>
	bool ProcessSpell(LuaSpell* spell, Spawn * target, bool first_cast = true, const char* function = 0, SpellScriptTimer* timer = 0);
	Mutex MSpellProcess;
	Mutex MRecastTimers;
	MutexMap<Entity*,Spell*> spell_que;
	MutexList<LuaSpell*> active_spells;
	MutexList<CastTimer*> cast_timers;
	MutexList<InterruptStruct*>interrupt_list;
	vector<RecastTimer*> recast_timers;
	int32 last_checked_time;
	vector<SpellScriptTimer*> m_spellScriptList;
	Mutex MSpellScriptTimers;
	map<LuaSpell*, vector<Spawn*>*> remove_target_list;
	Mutex MRemoveTargetList;
	vector<LuaSpell*> SpellCancelList;
	Mutex MSpellCancelList;

	Mutex MSoloHO;
	Mutex MGroupHO;
	map<Client*, HeroicOP*> m_soloHO;
	map<int32, HeroicOP*> m_groupHO;
};

#endif

