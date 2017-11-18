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
#ifndef CLIENT_H
#define CLIENT_H

#include "../common/EQStream.h"
#include <list>
#include "../common/timer.h"
#include "zoneserver.h"
#include "Player.h"
#include "Quests.h"

using namespace std;
#define CLIENT_TIMEOUT 60000
struct TransportDestination;
struct ConversationOption;

#define MAIL_SEND_RESULT_SUCCESS				0
#define MAIL_SEND_RESULT_UNKNOWN_PLAYER			1
#define MAIL_SEND_RESULT_CANNOT_SEND_TO_PLAYER	2
#define MAIL_SEND_RESULT_GIFT_WRONG_SERVER		3	/* Cannot send gifts across worlds */
#define MAIL_SEND_RESULT_CANNOT_SEND_TO_SELF	4
#define MAIL_SEND_RESULT_MAILBOX_FULL			5
#define MAIL_SEND_RESULT_NOT_ENOUGH_COIN		6
#define MAIL_SEND_RESULT_ITEM_IN_BAG			7	/* Cannot send non-empty bags as gifts */
#define MAIL_SEND_RESULT_NOT_IN_GUILD			8
#define MAIL_SEND_RESULT_GUILD_ACCESS_DENIED	9
#define MAIL_SEND_RESULT_GIFTS_TO_GUILD			10	/* Cannot send gifts to entire guild */
#define MAIL_SEND_RESULT_EMPTY_TO_LIST			11	/* Empty recipient list */
#define MAIL_SEND_RESULT_TRIAL_PLAYERS			12	/* Cannot send mail to trial players */
#define MAIL_SEND_RESULT_MAIL_WRONG_SERVER		13	/* Cannot send mail across worlds */
#define MAIL_SEND_RESULT_UNKNOWN_ERROR			14

#define MAIL_TYPE_REGULAR	0
#define MAIL_TYPE_SPAM		1
#define MAIL_TYPE_GM		2

struct QueuedQuest{
	Quest* quest;
	int32 step;
	bool display_quest_helper;
};

struct BuyBackItem{
	int32	item_id;
	int32	unique_id;
	int8	quantity;
	int32	price;
	bool	save_needed;
};

struct MacroData{
	string	name;
	string	text;
	int16	icon;
};

struct Mail {
	int32	mail_id;
	int32	player_to_id;
	string	player_from;
	string	subject;
	string	mail_body;
	int8	already_read;
	int8	mail_type;
	int32	coin_copper;
	int32	coin_silver;
	int32	coin_gold;
	int32	coin_plat;
	int16	stack;
	int32	postage_cost;
	int32	attachment_cost;
	int32	char_item_id;
	int32	time_sent;
	int32	expire_time;
	int8	save_needed;
};

struct MailWindow {
	int32	coin_copper;
	int32	coin_silver;
	int32	coin_gold;
	int32	coin_plat;
	int32	char_item_id;
};

struct PendingGuildInvite {
	Guild* guild;
	Player* invited_by;
};

struct PendingResurrection {
	LuaSpell* spell;
	Spawn* caster;
	Timer* expire_timer;
	string spell_name;
	string heal_name;
	bool active;
	float hp_perc;
	float mp_perc;
	float range;
	int8 crit_mod;
	bool no_calcs;
	int32 subspell;
	bool crit;
	bool should_delete;
	int32 spell_visual;
};

#define PAPERDOLL_TYPE_FULL 0
#define PAPERDOLL_TYPE_HEAD 1

struct IncomingPaperdollImage {
	uchar* image_bytes;
	int32 current_size_bytes;
	int8 image_num_packets;
	int8 last_received_packet_index;
	int8 image_type;
};

class Client {
public:
	Client(EQStream* ieqs);
    ~Client();
	
	bool	Process(bool zone_process = false);
	void	Disconnect(bool send_disconnect = true);
	void	SetConnected(bool val){ connected = val; }
	bool	IsConnected(){ return connected; }
	bool	IsReadyForSpawns(){ return ready_for_spawns; }
	bool	IsZoning(){ return client_zoning; }
	void	SetReadyForSpawns(bool val);
	void	QueuePacket(EQ2Packet* app);
	void	SendLoginInfo();
	void	SimpleMessage(int8 color, const char* message);
	void	Message(int8 type, const char* message, ...);
	void	SendSpellUpdate(Spell* spell);
	void	Zone(ZoneServer* new_zone, bool set_coords = true);
	void	Zone(const char* new_zone, bool set_coords = true);
	void	Zone(int32 zoneid, bool set_coords = true);
	void	Zone(int32 instanceid, bool set_coords = true, bool byInstanceID=false);
	void	SendZoneInfo();
	void	SendZoneSpawns();
	void	HandleVerbRequest(EQApplicationPacket* app);
	void	SendCharInfo();
	void	SendLoginDeniedBadVersion();
	void	SendCharPOVGhost();
	void	SendPlayerDeathWindow();
	float	DistanceFrom(Client* client);
	void	SendDefaultGroupOptions();
	bool	HandleLootItem(Entity* entity, int32 item_id);
	bool	HandleLootItem(Entity* entity, Item* item);
	void	HandleLoot(EQApplicationPacket* app);
	void	HandleSkillInfoRequest(EQApplicationPacket* app);
	void	HandleExamineInfoRequest(EQApplicationPacket* app);
	void	HandleQuickbarUpdateRequest(EQApplicationPacket* app);
	void	SendPopupMessage(int8 unknown, const char* text, const char* type, float size, int8 red, int8 green, int8 blue);	
	void	PopulateSkillMap();
	void	ChangeLevel(int16 old_level, int16 new_level);
	void	ChangeTSLevel(int16 old_level, int16 new_level);
	bool	Summon(const char* search_name);
	bool	TryZoneInstance(int32 zoneID, bool zone_coords_valid=false);
	bool	GotoSpawn(const char* search_name);
	void	DisplayDeadWindow();
	void	HandlePlayerRevive(int32 point_id);
	void	Bank(Spawn* banker, bool cancel = false);
	void	BankWithdrawal(int64 amount);
	void	BankDeposit(int64 amount);
	Spawn*	GetBanker();
	void	SetBanker(Spawn* in_banker);
	bool	AddItem(int32 item_id, int8 quantity = 0);
	bool	AddItem(Item* item);
	bool	RemoveItem(Item *item, int8 quantity);
	void	ProcessTeleport(Spawn* spawn, vector<TransportDestination*>* destinations, int32 transport_id = 0);
	void	ProcessTeleportLocation(EQApplicationPacket* app); 

	void	UpdateCharacterInstances();
	void	SetLastSavedTimeStamp(int32 unixts) { last_saved_timestamp = unixts; }
	int32	GetLastSavedTimeStamp() { return last_saved_timestamp; }

	bool	CheckZoneAccess(const char* zoneName);
	
	ZoneServer* GetCurrentZone();
	void	SetCurrentZoneByInstanceID(int32 id, int32 zoneid);
	//void	SetCurrentZoneByInstanceID(instanceid, zoneid);
	void	SetCurrentZone(int32 id);
	void	SetCurrentZone(ZoneServer* zone) { 
		current_zone = zone;
		player->SetZone(zone);
	}
	Player*	GetPlayer(){ return player; }
	EQStream*	getConnection(){ return eqs; }

	inline int32		GetIP()				{ return ip; }
	inline int16		GetPort()			{ return port; }
	inline int32		WaitingForBootup()	{ return pwaitingforbootup; }
	inline int32		GetCharacterID()		{ return character_id; }
	inline int32		GetAccountID()			{ return account_id; }
	inline const char*	GetAccountName()		{ return account_name; }
	inline sint16		GetAdminStatus()		{ return admin_status; }
	inline int16		GetVersion()			{ return version; }
	void SetNameCRC(int32 val){ name_crc = val; }
	int32 GetNameCRC(){ return name_crc; }


	void				SetVersion(int16 new_version){ version = new_version; }
	void				SetAccountID(int32 in_accountid) { account_id = in_accountid; }
	void				SetCharacterID(int32 in_characterid) { character_id = in_characterid; }
	void				SetAdminStatus(sint16 in_status) { admin_status = in_status; }
	

	void	DetermineCharacterUpdates ( );

	void	UpdateTimeStampFlag ( int8 flagType )
	{
		if(! (timestamp_flag & flagType ) )
		timestamp_flag |= flagType;
	}

	int8	GetTimeStampFlag ( ) { return timestamp_flag; }
	bool	UpdateQuickbarNeeded();
	void	Save();
	bool	remove_from_list;
	void	CloseLoot();
	void	SendPendingLoot(int32 total_coins, Entity* entity);
	void	Loot(int32 total_coins, vector<Item*>* items, Entity* entity);
	void	Loot(Entity* entity);
	void	CheckPlayerQuestsKillUpdate(Spawn* spawn);
	void	CheckPlayerQuestsChatUpdate(Spawn* spawn);
	void	CheckPlayerQuestsItemUpdate(Item* item);
	void	CheckPlayerQuestsSpellUpdate(Spell* spell);
	void	CheckPlayerQuestsLocationUpdate();
	void	AddPendingQuest(Quest* quest);
	Quest*	GetPendingQuest(int32 id);
	void	RemovePendingQuest(Quest* quest);
	void	SetPlayerQuest(Quest* quest, map<int32, int32>* progress);
	void	AddPlayerQuest(Quest* quest, bool call_accepted = true, bool send_packets = true);
	void	RemovePlayerQuest(int32 id, bool send_update = true, bool delete_quest = true);
	void	SendQuestJournal(bool all_quests = false, Client* client = 0);
	void	SendQuestUpdate(Quest* quest);
	void	SendQuestFailure(Quest* quest);
	void	SendQuestUpdateStep(Quest* quest, int32 step, bool display_quest_helper = true);
	void	SendQuestUpdateStepImmediately(Quest* quest, int32 step, bool display_quest_helper = true);
	void	DisplayQuestComplete(Quest* quest);
	void	DisplayRandomizeFeatures(int32 features);
	void	AcceptQuestReward(Quest* quest, int32 item_id);
	Quest*	GetPendingQuestAcceptance(int32 item_id);
	Quest*	GetActiveQuest(int32 quest_id);
	void	DisplayConversation(int32 conversation_id, int32 spawn_id, vector<ConversationOption>* conversations, const char* text, const char* mp3, int32 key1, int32 key2);
	void	DisplayConversation(Item* item, vector<ConversationOption>* conversations, const char* text, int8 type, const char* mp3 = 0, int32 key1 = 0, int32 key2 = 0);
	void	DisplayConversation(Entity* npc, int8 type, vector<ConversationOption>* conversations, const char* text, const char* mp3 = 0, int32 key1 = 0, int32 key2 = 0);
	void	CloseDialog(int32 conversation_id);
	int32	GetConversationID(Spawn* spawn, Item* item);
	void	CombineSpawns(float radius, Spawn* spawn);
	void	AddCombineSpawn(Spawn* spawn);
	void	RemoveCombineSpawn(Spawn* spawn);
	void	SaveCombineSpawns(const char* name = 0);
	Spawn*	GetCombineSpawn();
	bool	ShouldTarget();
	void	TargetSpawn(Spawn* spawn);
	void	ReloadQuests();
	int32	GetCurrentQuestID(){ return current_quest_id; }
	void	SetLuaDebugClient(bool val);
	void	SetMerchantTransaction(Spawn* spawn);
	Spawn*	GetMerchantTransaction();
	void	SetMailTransaction(Spawn* spawn);
	Spawn*	GetMailTransaction();
	void	PlaySound(const char* name);
	void	SendBuyMerchantList(bool sell = false);
	void	SendSellMerchantList(bool sell = false);
	void	SendBuyBackList(bool sell = false);
	void	SendRepairList();
	void	ShowLottoWindow();
	void	PlayLotto(int32 price, int32 ticket_item_id);
	void	SendGuildCreateWindow();
	float	CalculateBuyMultiplier(int32 merchant_id);
	float	CalculateSellMultiplier(int32 merchant_id);
	void	BuyItem(int32 item_id, int8 quantity);
	void	SellItem(int32 item_id, int8 quantity, int32 unique_id = 0);
	void	BuyBack(int32 item_id, int8 quantity);
	void	RepairItem(int32 item_id);
	void	RepairAllItems();
	void	AddBuyBack(int32 unique_id, int32 item_id, int8 quantity, int32 price, bool save_needed = true);
	deque<BuyBackItem*>*	GetBuyBacks();
	vector<Item*>* GetRepairableItems();
	void	SendMailList();
	void	DisplayMailMessage(int32 mail_id);
	void	HandleSentMail(EQApplicationPacket* app);
	void	DeleteMail(int32 mail_id, bool from_database = false);
	bool	AddMailCoin(int32 copper, int32 silver = 0, int32 gold = 0, int32 plat = 0);
	bool	RemoveMailCoin(int32 copper, int32 silver = 0, int32 gold = 0, int32 plat = 0);
	void	TakeMailAttachments(int32 mail_id);
	void	CancelSendMail();
	bool	GateAllowed();
	bool	BindAllowed();
	bool	Bind();
	bool	Gate();
	void	SendChatRelationship(int8 type, const char* name);
	void	SendFriendList();
	void	SendIgnoreList();
	void	SendNewSpells(int8 class_id);
	string	GetCoinMessage(int32 total_coins);
	void	SetItemSearch(vector<Item*>* items);
	vector<Item*>* GetSearchItems();
	void	SearchStore(int32 page);
	void	SetPlayer(Player* new_player){
		player = new_player;
	}
	void	AddPendingQuestReward(Quest* quest);
	void	AddPendingQuestUpdate(int32 quest_id, int32 step_id, int32 progress = 0xFFFFFFFF);
	void	ProcessQuestUpdates();	
	void	AddWaypoint(const char* waypoint_name, int8 waypoint_category, int32 spawn_id);
	void	BeginWaypoint(const char* waypoint_name, float x, float y, float z);
	void	InspectPlayer(Player* player_to_inspect);
	void	SetPendingGuildInvite(Guild* guild, Player* invited_by = 0);
	PendingGuildInvite*	GetPendingGuildInvite() {return &pending_guild_invite;}
	void	ShowClaimWindow();
	void	ShowGuildSearchWindow();
	void	CheckQuestQueue();
	void	ShowDressingRoom(Item *item, sint32 crc);
	void	SendCollectionList();
	bool	SendCollectionsForItem(Item *item);
	void	HandleCollectionAddItem(int32 collection_id, Item *item);
	void	DisplayCollectionComplete(Collection *collection);
	void	HandInCollections();
	void	AcceptCollectionRewards(Collection *collection, int32 selectable_item_id = 0);
	void	SendRecipeList();
	void	SendTitleUpdate();
	void	SendUpdateTitles(sint16 prefix, sint16 suffix);
	void	SendLanguagesUpdate(int32 id);
	void	SendAchievementsList();
	void	SendAchievementUpdate(bool first_login = false);

	///<summary>Send the pet options window to the client</summary>
	///<param name="type">Type of pet, 1 = combat 0 = non combat</param>
	void	SendPetOptionsWindow(const char* pet_name, int8 type = 1);
	void	SendBiography();

	bool IsCrafting();

	void SetRecipeListSent(bool val) {m_recipeListSent = val; }
	bool GetRecipeListSent() { return m_recipeListSent; }
	void ShowRecipeBook();
	PendingResurrection* GetCurrentRez();
	void SendResurrectionWindow();
	void AcceptResurrection();
	Mutex m_resurrect;
	Mutex* GetResurrectMutex();
	void SetPendingLastName(string last_name);
	void RemovePendingLastName();
	string* GetPendingLastName();
	void SendLastNameConfirmation();

	void SetInitialSpawnsSent(bool val) { initial_spawns_sent = val; }

	bool GetInitialSpawnsSent() { return initial_spawns_sent; }

	void SendQuestJournalUpdate(Quest* quest);

	void AddQuestTimer(int32 quest_id);

	void RemoveQuestTimer(int32 quest_id);

	void SetPendingFlightPath(int32 val) { pending_flight_path = val; }
	int32 GetPendingFlightPath() { return pending_flight_path; }

	void EndAutoMount();
	bool GetOnAutoMount() { return on_auto_mount; }

private:
	void    SavePlayerImages();
	void	SkillChanged(Skill* skill, int16 previous_value, int16 new_value);
	void	GiveQuestReward(Quest* quest);
	void	SetStepComplete(int32 quest_id, int32 step);
	void	AddStepProgress(int32 quest_id, int32 step, int32 progress);
	map<int32, map<int32, int32> > quest_pending_updates;
	vector<QueuedQuest*> quest_queue;
	vector<Quest*> quest_pending_reward;
	volatile bool	quest_updates;
	Mutex	MQuestPendingUpdates;
	Mutex	MQuestQueue;
	Mutex	MDeletePlayer;
	vector<Item*>* search_items;
	Spawn*	transport_spawn;
	Mutex	MBuyBack;
	deque<BuyBackItem*> buy_back_items;
	Spawn*	merchant_transaction;
	Spawn*	mail_transaction;
	Mutex	MPendingQuestAccept;
	vector<Quest*> pending_quest_accept;	
	bool	lua_debug;
	bool	should_target;
	Spawn*	combine_spawn;
	int8	num_active_failures;
	int32	next_conversation_id;
	map<int32, Spawn*> conversation_spawns;
	map<int32, Item*> conversation_items;
	map<int32, map<int8, string> > conversation_map;
	int32	current_quest_id;
	Spawn*	banker;
	map<int32, bool> sent_spell_details;
	map<int32, bool> sent_item_details;
	Player*	player;
	int16	version;
	int8	timestamp_flag;
	int32	ip;
	int16	port;
	int32	account_id;
	int32	character_id;
	sint16	admin_status; // -2 Banned, -1 Suspended, 0 User, etc.
	char	account_name[64];
	char	zone_name[64];
	int32	zoneID;
	int32	instanceID;
	Timer*	autobootup_timeout;
	int32	pwaitingforbootup;
	int32	last_update_time;

	int32	last_saved_timestamp;

	Timer*	CLE_keepalive_timer;
	Timer*	connect;
	Timer*	camp_timer;
	Timer*	disconnect_timer;
	bool	connected;
	bool	ready_for_spawns;
	bool	seencharsel;
	bool	connected_to_zone;
	bool	client_zoning;
	bool	firstlogin;
	bool	new_client_login;
	Timer	pos_update;
	Timer   spawn_vis_update;
	Timer	quest_pos_timer;
	Timer	lua_debug_timer;
	bool	player_pos_changed;
	bool HandlePacket(EQApplicationPacket *app);
	EQStream* eqs;
	bool quickbar_changed;
	ZoneServer* current_zone;
	int32	name_crc;
	MailWindow	mail_window;
	PendingGuildInvite	pending_guild_invite;
	PendingResurrection current_rez;
	string* pending_last_name;
	IncomingPaperdollImage incoming_paperdoll;

	bool m_recipeListSent;
	bool initial_spawns_sent;
	bool should_load_spells;

	// int32 = quest id
	vector<int32> quest_timers;
	Mutex MQuestTimers;

	int32 pending_flight_path;

	bool on_auto_mount;
	bool EntityCommandPrecheck(Spawn* spawn, const char* command);
};

class ClientList {
public:
	ClientList();
	~ClientList();
	bool	ContainsStream(EQStream* eqs);
	void	Add(Client* client);
	Client*	Get(int32 ip, int16 port);
	Client* FindByAccountID(int32 account_id);
	Client* FindByName(char* charname);
	void	Remove(Client* client, bool delete_data = false);
	void	RemoveConnection(EQStream* eqs);
	void	Process();
	int32	Count();
	void	ReloadQuests();
private:
	Mutex	MClients;
	list<Client*> client_list;
};
#endif
