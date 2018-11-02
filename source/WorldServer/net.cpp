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
#include "../common/debug.h"
#include "../common/Log.h"

#include <iostream>
#include <thread>
#include <memory>
using namespace std;
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <atomic>

#include <signal.h>
#include "../common/types.h"
#include "../common/queue.h"
#include "../common/timer.h"
#include "../common/EQStreamFactory.h"
#include "../common/EQStream.h"
#include "net.h"

#include "Variables.h"
#include "WorldDatabase.h"
#include "../common/seperator.h"
#include "../common/version.h"
#include "../common/EQEMuError.h"
#include "../common/opcodemgr.h"
#include "../common/Common_Defines.h"

#include "LoginServer.h"
#include "MasterServer.h"
#include "Commands/Commands.h"
#include "Factions.h"
#include "World.h"
#include "../common/ConfigReader.h"
#include "Skills.h"
#include "LuaInterface.h"
#include "Guilds/Guild.h"
#include "Commands/ConsoleCommands.h"
#include "Traits/Traits.h"
#include "IRC/IRC.h"

#ifdef WIN32
#include <process.h>
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#include <conio.h>
#else
#include <pthread.h>
#include "../common/unix.h"
#endif

#ifdef PROFILER
#define SHINY_PROFILER TRUE
#include "../Profiler/src/Shiny.h"
#endif

NetConnection net;
World world;
EQStreamFactory eqsf(LoginStream);
LoginServer loginserver;
MasterServer master_server;
LuaInterface* lua_interface = new LuaInterface();

#include "Rules/Rules.h"
#include "Titles.h"
#include "Languages.h"
#include "Achievements/Achievements.h"

#include "Patch/patch.h"

volatile bool RunLoops = true;
atomic<sint32> numclients(0);
atomic<sint32> numzones(0);
extern ClientList client_list;
extern ZoneList zone_list;
extern MasterFactionList master_faction_list;
extern WorldDatabase database;
extern MasterSpellList master_spell_list;
extern MasterTraitList master_trait_list;
extern MasterSkillList master_skill_list;
extern MasterItemList master_item_list;
extern GuildList guild_list;
extern Variables variables;
extern IRC irc;
ConfigReader configReader;
int32 MasterItemList::next_unique_id = 0;
int last_signal = 0;
RuleManager rule_manager;
MasterTitlesList master_titles_list;
MasterLanguagesList master_languages_list;
extern MasterAchievementList master_achievement_list;
extern map<int16, int16> EQOpcodeVersions;
PatchServer patch;

ThreadReturnType ItemLoad(void* tmp);
ThreadReturnType AchievmentLoad(void* tmp);
ThreadReturnType SpellLoad(void* tmp);
ThreadReturnType EQ2ConsoleListener(void* tmp);

int main(int argc, char** argv) {
  LogParseConfigs();
  WelcomeHeader();

  int32 t_total = Timer::GetUnixTimeStamp();

  LogStart();

  LogWrite(INIT__INFO, 0, "Init", "Starting EQ2Emulator WorldServer...");

  database.Init();

  if (!database.ConnectNewDatabase())
    return EXIT_FAILURE;

  srand(time(NULL));

  net.ReadLoginINI();

  // JA: Grouping all System (core) data loads together for timing purposes
  LogWrite(WORLD__INFO, 0, "World", "Loading System Data...");

  int32 t_now = Timer::GetUnixTimeStamp();

  EQOpcodeVersions = database.GetVersions();

  for (auto& version : EQOpcodeVersions) {
    int version1 = version.first;

    EQOpcodeManager[version1] = new RegularOpcodeManager();
    map<string, uint16> eq = database.GetOpcodes(version1);

    if (!EQOpcodeManager[version1]->LoadOpcodes(&eq)) {
      LogWrite(INIT__ERROR, 0, "Init", "Loading opcodes failed. Make sure you have sourced the opcodes.sql file!");
      return false;
    }
  }

  if (!configReader.LoadFile("CommonStructs.xml") || !configReader.LoadFile("WorldStructs.xml") || !configReader.LoadFile("SpawnStructs.xml") || !configReader.LoadFile("ItemStructs.xml")) {
    LogWrite(INIT__ERROR, 0, "Init", "Loading structs failed. Make sure you have CommonStructs.xml, WorldStructs.xml, SpawnStructs.xml, and ItemStructs.xml in the working directory!");
    return false;
  }

  world.init();

  bool threadedLoad = rule_manager.GetGlobalRule(R_World, ThreadedLoad)->GetBool();

  LogWrite(WORLD__DEBUG, 1, "World", "-Loading EQ2 time of day...");
  loginserver.InitLoginServerVariables();

  LogWrite(WORLD__INFO, 0, "World", "Loaded System Data (took %u seconds)", Timer::GetUnixTimeStamp() - t_now);

  if (threadedLoad) {
    LogWrite(WORLD__WARNING, 0, "Threaded", "Using Threaded loading of static data...");

    thread thr1(ItemLoad, &world);
    thr1.detach();
    thread thr2(SpellLoad, &world);
    thr2.detach();
  }

  if (!threadedLoad) {
    LogWrite(ITEM__INFO, 0, "Items", "Loading Items...");
    database.LoadItemList();
    MasterItemList::ResetUniqueID(database.LoadNextUniqueItemID());
  }

  if (!threadedLoad) {
    LogWrite(SPELL__INFO, 0, "Spells", "Loading Spells...");
    database.LoadSpells();

    LogWrite(SPELL__INFO, 0, "Spells", "Loading Spell Errors...");
    database.LoadSpellErrors();

    LogWrite(WORLD__INFO, 0, "Traits", "Loading Traits...");
    database.LoadTraits();
  }

  if (!threadedLoad) {
    LogWrite(QUEST__INFO, 0, "Quests", "Loading Quests...");
    database.LoadQuests();
  }

  if (!threadedLoad) {
    LogWrite(COLLECTION__INFO, 0, "Collect", "Loading Collections...");
    database.LoadCollections();
  }

  LogWrite(GUILD__INFO, 0, "Guilds", "Loading Guilds...");
  database.LoadGuilds();

  LogWrite(TRADESKILL__INFO, 0, "Recipes", "Loading Recipe Books...");
  database.LoadRecipeBooks();

  LogWrite(TRADESKILL__INFO, 0, "Recipes", "Loading Recipes...");
  database.LoadRecipes();

  LogWrite(TRADESKILL__INFO, 0, "Tradeskills", "Loading Tradeskill Events...");
  database.LoadTradeskillEvents();

  LogWrite(SPELL__INFO, 0, "AA", "Loading Alternate Advancements...");
  database.LoadAltAdvancements();

  LogWrite(SPELL__INFO, 0, "AA", "Loading AA Tree Nodes...");
  database.LoadTreeNodes();

  LogWrite(WORLD__INFO, 0, "Titles", "Loading Titles...");
  database.LoadTitles();

  LogWrite(WORLD__INFO, 0, "Languages", "Loading Languages...");
  database.LoadLanguages();

  LogWrite(CHAT__INFO, 0, "Chat", "Loading channels...");
  database.LoadChannels();

  if (!threadedLoad) {
    LogWrite(MERCHANT__INFO, 0, "Merchants", "Loading Merchants...");
    database.LoadMerchantInformation();
  }

  LogWrite(LUA__INFO, 0, "LUA", "Loading Spawn Scripts...");
  database.LoadSpawnScriptData();

  LogWrite(LUA__INFO, 0, "LUA", "Loading Zone Scripts...");
  database.LoadZoneScriptData();

  LogWrite(WORLD__INFO, 0, "World", "Loading House Zone Data...");
  database.LoadHouseZones();
  database.LoadPlayerHouses();

  LogWrite(WORLD__INFO, 0, "World", "Loading Heroic OP Data...");
  database.LoadHOStarters();
  database.LoadHOWheel();

  LogWrite(WORLD__INFO, 0, "World", "Loading Race Types Data...");
  database.LoadRaceTypes();

  if (threadedLoad) {
    LogWrite(WORLD__INFO, 0, "World", "Waiting for load threads to finish.");
    while (!world.items_loaded || !world.spells_loaded /*|| !world.achievments_loaded*/)
      Sleep(10);
    LogWrite(WORLD__INFO, 0, "World", "Load threads finished.");
  }

  LogWrite(WORLD__INFO, 0, "World", "Total World startup time: %u seconds.", Timer::GetUnixTimeStamp() - t_total);

  if (eqsf.Open(net.GetWorldPort())) {
    if (strlen(net.GetWorldAddress()) == 0) {
      LogWrite(NET__INFO, 0, "Net", "World server listening on port %i", net.GetWorldPort());
    } else {
      LogWrite(NET__INFO, 0, "Net", "World server listening on: %s:%i", net.GetWorldAddress(), net.GetWorldPort());
    }
  } else {
    LogWrite(NET__ERROR, 0, "Net", "Failed to open port %i.", net.GetWorldPort());
    return 1;
  }

  Timer InterserverTimer(INTERSERVER_TIMER); // does MySQL pings and auto-reconnect
  InterserverTimer.Trigger();

  Timer* TimeoutTimer = new Timer(5000);
  TimeoutTimer->Start();

  UpdateWindowTitle(0);

  //LogWrite(ZONE__INFO, 0, "Zone", "Starting static zones...");
  //database.LoadSpecialZones();

  map<EQStream*, int32> connecting_clients;

  //LogWrite(WORLD__DEBUG, 0, "Thread", "Starting console command thread...");
  //thread thr3(EQ2ConsoleListener, nullptr);
  //thr3.detach();

  //LogWrite(WORLD__INFO, 0, "Console", "Type 'help' or '?' and press enter for menu options.");

  ZoneServer* zs = new ZoneServer(argv[1]);
  database.LoadZoneInfo(zs);
  zs->Init();

  while (RunLoops) {
    Timer::SetCurrentTime();

    while (EQStream* eqs = eqsf.Pop()) {
      struct in_addr in;
      in.s_addr = eqs->GetRemoteIP();

      LogWrite(NET__DEBUG, 0, "Net", "New client from ip: %s port: %i", inet_ntoa(in), ntohs(eqs->GetRemotePort()));

      if (rule_manager.GetGlobalRule(R_World, UseBannedIPsTable)->GetInt8() == 1) {
        if (database.CheckBannedIPs(inet_ntoa(in))) {
          eqs->Close();
        }
      }

      if (eqs && eqs->CheckActive() && !client_list.ContainsStream(eqs)) {
        auto client = make_shared<Client>(eqs);
        client->SetCurrentZone(zs);
        //client_list.Add(client);
        zs->AddIncomingClient(client);
      } else if (eqs && !client_list.ContainsStream(eqs)) {
        connecting_clients[eqs] = Timer::GetCurrentTime2();
      }
    }

    if (connecting_clients.size() > 0) {
      for (auto cc_itr = connecting_clients.begin(); cc_itr != connecting_clients.end(); ++cc_itr) {
        if (cc_itr->first && cc_itr->first->CheckActive() && !client_list.ContainsStream(cc_itr->first)) {
          LogWrite(NET__DEBUG, 0, "Net", "Removing client from waiting list...");

          auto client = make_shared<Client>(cc_itr->first);
          //client_list.Add(client);
          zs->AddIncomingClient(client);

          connecting_clients.erase(cc_itr);
          break;
        } else if (Timer::GetCurrentTime2() >= (cc_itr->second + 10000)) {
          connecting_clients.erase(cc_itr);
          break;
        }
      }
    }

    world.Process();
    //client_list.Process();
    loginserver.Process();
    master_server.Process();

    if (TimeoutTimer->Check()) {
      eqsf.CheckTimeout();
    }

    if (InterserverTimer.Check()) {
      InterserverTimer.Start();
      database.ping();

      if (true /*getenv("MASTER_SERVER_ENABLED") == "true"*/ && !master_server.Connected() && master_server.Connect()) {
        LogWrite(WORLD__INFO, 0, "Master", "Connected to Master Server");
        master_server.SayHello(zs->GetZoneID());
      }

      /*if (net.LoginServerInfo && loginserver.Connected() == false && loginserver.CanReconnect()) {
				LogWrite(WORLD__DEBUG, 0, "Thread", "Starting autoinit loginserver thread...");

				thread thr4(AutoInitLoginServer, nullptr);
				thr4.detach();
			}*/
    }

    this_thread::yield();
  }

  LogWrite(WORLD__DEBUG, 0, "World", "The world is ending!");

  LogWrite(WORLD__DEBUG, 0, "IRC", "Shutting IRC down");
  irc.SetRunning(false);

  LogWrite(WORLD__DEBUG, 0, "World", "Shutting down zones...");
  zone_list.ShutDownZones();

  LogWrite(WORLD__DEBUG, 0, "World", "Shutting down LUA interface...");
  safe_delete(lua_interface);
  safe_delete(TimeoutTimer);
  eqsf.Close();

  for (auto opcode_itr = EQOpcodeManager.begin(); opcode_itr != EQOpcodeManager.end(); ++opcode_itr) {
    safe_delete(opcode_itr->second);
  }

  CheckEQEMuErrorAndPause();

#ifdef PROFILER
  PROFILER_UPDATE();
  PROFILER_OUTPUT();
#endif

  LogWrite(WORLD__INFO, 0, "World", "Exiting... we hope you enjoyed your flight.");
  LogStop();
  return 0;
}

ThreadReturnType ItemLoad(void* tmp) {
  LogWrite(WORLD__WARNING, 0, "Thread", "Item Loading Thread started.");
#ifdef WIN32
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
#endif
  if (tmp == 0) {
    ThrowError("ItemLoad(): tmp = 0!");
    THREAD_RETURN(NULL);
  }
  World* world = (World*)tmp;
  WorldDatabase db;
  db.Init();
  db.ConnectNewDatabase();

  LogWrite(ITEM__INFO, 0, "Items", "Loading Items...");
  db.LoadItemList();
  MasterItemList::ResetUniqueID(db.LoadNextUniqueItemID());

  // Relies on the item list so needs to be in the item thread
  LogWrite(COLLECTION__INFO, 0, "Collect", "Loading Collections...");
  db.LoadCollections();

  LogWrite(MERCHANT__INFO, 0, "Merchants", "Loading Merchants...");
  db.LoadMerchantInformation();

  LogWrite(QUEST__INFO, 0, "Quests", "Loading Quests...");
  db.LoadQuests();

  world->items_loaded = true;
  LogWrite(WORLD__WARNING, 0, "Thread", "Item Loading Thread completed.");

  mysql_thread_end();
  THREAD_RETURN(NULL);
}

ThreadReturnType SpellLoad(void* tmp) {
  LogWrite(WORLD__WARNING, 0, "Thread", "Spell Loading Thread started.");
#ifdef WIN32
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
#endif
  if (tmp == 0) {
    ThrowError("ItemLoad(): tmp = 0!");
    THREAD_RETURN(NULL);
  }
  World* world = (World*)tmp;
  WorldDatabase db;
  db.Init();
  db.ConnectNewDatabase();
  LogWrite(SPELL__INFO, 0, "Spells", "Loading Spells...");
  db.LoadSpells();

  LogWrite(SPELL__INFO, 0, "Spells", "Loading Spell Errors...");
  db.LoadSpellErrors();

  LogWrite(WORLD__INFO, 0, "Traits", "Loading Traits...");
  db.LoadTraits();

  world->spells_loaded = true;
  LogWrite(WORLD__WARNING, 0, "Thread", "Spell Loading Thread completed.");

  mysql_thread_end();
  THREAD_RETURN(NULL);
}

ThreadReturnType AchievmentLoad(void* tmp) {
  LogWrite(WORLD__WARNING, 0, "Thread", "Achievement Loading Thread started.");
#ifdef WIN32
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
#endif
  if (tmp == 0) {
    ThrowError("ItemLoad(): tmp = 0!");
    THREAD_RETURN(NULL);
  }
  World* world = (World*)tmp;
  WorldDatabase db;
  db.Init();
  db.ConnectNewDatabase();

  LogWrite(ACHIEVEMENT__INFO, 0, "Achievements", "Loading Achievements...");
  int32 t_now = Timer::GetUnixTimeStamp();
  db.LoadAchievements();
  master_achievement_list.CreateMasterAchievementListPacket();
  LogWrite(ACHIEVEMENT__INFO, 0, "Achievements", "Achievements loaded (took %u seconds)", Timer::GetUnixTimeStamp() - t_now);

  world->achievments_loaded = true;
  LogWrite(WORLD__WARNING, 0, "Thread", "Achievement Loading Thread completed.");

  mysql_thread_end();
  THREAD_RETURN(NULL);
}

ThreadReturnType EQ2ConsoleListener(void* tmp) {
  char cmd[300];
  size_t i = 0;
  size_t len;

  while (RunLoops) {
    // Read in single line from "stdin"
    memset(cmd, 0, sizeof(cmd));
    if (fgets(cmd, 300, stdin) == NULL)
      continue;

    if (!RunLoops)
      break;

    len = strlen(cmd);
    for (i = 0; i < len; ++i) {
      if (cmd[i] == '\n' || cmd[i] == '\r')
        cmd[i] = '\0';
    }

    ProcessConsoleInput(cmd);
  }
  THREAD_RETURN(NULL);
}

#include <fstream>
void CatchSignal(int sig_num) {
  // In rare cases this can be called after the log system is shut down causing a deadlock or crash
  // when the world shuts down, if this happens again comment out the LogWrite and uncomment the printf
  if (last_signal != sig_num) {
    static Mutex lock;
    static ofstream signal_out;

    lock.lock();
    if (!signal_out.is_open())
      signal_out.open("signal_catching.log", ios::trunc);
    if (signal_out) {
      signal_out << "Caught signal " << sig_num << "\n";
      signal_out.flush();
    }
    printf("Caught signal %i\n", sig_num);
    lock.unlock();

    last_signal = sig_num;
    RunLoops = false;
  }
}

bool NetConnection::ReadLoginINI() {
  strncpy(worldname, getenv("EQ2_WORLD_NAME"), 201);
  strncpy(worldaddress, getenv("EQ2_ADDRESS"), 250);
  worldport = atoi(getenv("EQ2_PORT"));
  publicport = atoi(getenv("PUBLIC_PORT"));

  strncpy(worldaccount, getenv("EQ2_LS_USERNAME"), 31);
  strncpy(worldpassword, getenv("EQ2_LS_PASSWORD"), 31);
  strncpy(loginaddress[0], getenv("EQ2_LS_ADDRESS"), 250);
  loginport[0] = atoi(getenv("EQ2_LS_PORT"));

  LoginServerInfo = 1;

  return true;
}

char* NetConnection::GetUpdateServerInfo(int16* oPort) {
  if (oPort == 0)
    return 0;
  if (updateaddress[0] == 0)
    return 0;

  *oPort = updateport;
  return updateaddress;
}

char* NetConnection::GetLoginInfo(int16* oPort) {
  if (oPort == 0)
    return 0;
  if (loginaddress[0][0] == 0)
    return 0;

  int8 tmp[3] = {0, 0, 0};
  int8 count = 0;

  for (int i = 0; i < 3; i++) {
    if (loginaddress[i][0])
      tmp[count++] = i;
  }

  int x = rand() % count;

  *oPort = loginport[tmp[x]];
  return loginaddress[tmp[x]];
}

void UpdateWindowTitle(char* iNewTitle) {

  char tmp[500];
  if (iNewTitle) {
    snprintf(tmp, sizeof(tmp), "World: %s", iNewTitle);
  } else {
    snprintf(tmp, sizeof(tmp), "%s, Version: %s: %i Clients(s) in %i Zones(s)", EQ2EMU_MODULE, CURRENT_VERSION, numclients.load(), numzones.load());
  }
  // Zero terminate ([max - 1] = 0) the string to prevent a warning
  tmp[499] = 0;
#ifdef WIN32
  SetConsoleTitle(tmp);
#else
  printf("%c]0;%s%c", '\033', tmp, '\007');
#endif
}

ZoneAuthRequest::ZoneAuthRequest(int32 account_id, char* name, int32 access_key) {
  accountid = account_id;
  character_name = string(name);
  accesskey = access_key;
  timestamp = Timer::GetUnixTimeStamp();
  firstlogin = false;
}

ZoneAuthRequest::~ZoneAuthRequest() {
}

void ZoneAuth::AddAuth(ZoneAuthRequest* zar) {
  LogWrite(NET__DEBUG, 0, "Net", "AddAuth: %u Key: %u", zar->GetAccountID(), zar->GetAccessKey());
  list.Insert(zar);
}

ZoneAuthRequest* ZoneAuth::GetAuth(int32 account_id, int32 access_key) {
  LinkedListIterator<ZoneAuthRequest*> iterator(list);

  iterator.Reset();
  while (iterator.MoreElements()) {
    if (iterator.GetData()->GetAccountID() == account_id && iterator.GetData()->GetAccessKey() == access_key) {
      ZoneAuthRequest* tmp = iterator.GetData();
      return tmp;
    }
    iterator.Advance();
  }
  return 0;
}

void ZoneAuth::PurgeInactiveAuth() {
  LinkedListIterator<ZoneAuthRequest*> iterator(list);

  iterator.Reset();
  int32 current_time = Timer::GetUnixTimeStamp();
  while (iterator.MoreElements()) {
    if ((iterator.GetData()->GetTimeStamp() + 60) < current_time) {
      iterator.RemoveCurrent();
    }
    iterator.Advance();
  }
}

void ZoneAuth::RemoveAuth(ZoneAuthRequest* zar) {
  LinkedListIterator<ZoneAuthRequest*> iterator(list);

  iterator.Reset();
  while (iterator.MoreElements()) {
    if (iterator.GetData() == zar) {
      iterator.RemoveCurrent();
      break;
    }
    iterator.Advance();
  }
}

void WelcomeHeader() {
#ifdef _WIN32
  HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute(console, FOREGROUND_WHITE_BOLD);
#endif
  printf("Module: %s, Version: %s", EQ2EMU_MODULE, CURRENT_VERSION);
#ifdef _WIN32
  SetConsoleTextAttribute(console, FOREGROUND_YELLOW_BOLD);
#endif
  printf("\n\nCopyright (C) 2007-2011 EQ2Emulator. http://eq2emulator.net/ \n\n");
  printf("EQ2Emulator is free software: you can redistribute it and/or modify\n");
  printf("it under the terms of the GNU General Public License as published by\n");
  printf("the Free Software Foundation, either version 3 of the License, or\n");
  printf("(at your option) any later version.\n\n");
  printf("EQ2Emulator is distributed in the hope that it will be useful,\n");
  printf("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
  printf("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
  printf("GNU General Public License for more details.\n\n");
#ifdef _WIN32
  SetConsoleTextAttribute(console, FOREGROUND_GREEN_BOLD);
#endif
  printf(" /$$$$$$$$  /$$$$$$   /$$$$$$  /$$$$$$$$                        \n");
  printf("| $$_____/ /$$__  $$ /$$__  $$| $$_____/                        \n");
  printf("| $$      | $$  \\ $$|__/  \\ $$| $$       /$$$$$$/$$$$  /$$   /$$\n");
  printf("| $$$$$   | $$  | $$  /$$$$$$/| $$$$$   | $$_  $$_  $$| $$  | $$\n");
  printf("| $$__/   | $$  | $$ /$$____/ | $$__/   | $$ \\ $$ \\ $$| $$  | $$\n");
  printf("| $$      | $$/$$ $$| $$      | $$      | $$ | $$ | $$| $$  | $$\n");
  printf("| $$$$$$$$|  $$$$$$/| $$$$$$$$| $$$$$$$$| $$ | $$ | $$|  $$$$$$/\n");
  printf("|________/ \\____ $$$|________/|________/|__/ |__/ |__/ \\______/ \n");
  printf("                \\__/                                            \n\n");
#ifdef _WIN32
  SetConsoleTextAttribute(console, FOREGROUND_MAGENTA_BOLD);
#endif
  printf(" Website     : http://eq2emulator.net \n");
  printf(" Forums      : http://eq2emulator.net/phpBB3/ \n");
  printf(" Contributors: http://eq2emulator.net/wiki/index.php/Developer:Contributors \n");
  printf(" SVN         : http://svn.eq2emulator.net/svn/eq2server \n\n");
#ifdef _WIN32
  SetConsoleTextAttribute(console, FOREGROUND_WHITE_BOLD);
#endif
  printf("For more detailed logging, modify 'Level' param the log_config.xml file.\n\n");
#ifdef _WIN32
  SetConsoleTextAttribute(console, FOREGROUND_WHITE);
#endif

  fflush(stdout);
}
