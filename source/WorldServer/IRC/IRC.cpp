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
#ifdef _WIN32
#include <WinSock2.h>
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <errno.h>
#include "../../common/unix.h"
#endif
#include "IRCReplyCodes.h"
#include "IRC.h"
#include "../Chat/Chat.h"
#include "../../common/Log.h"
#include "../Rules/Rules.h"

extern Chat chat;
extern RuleManager rule_manager;

IRC::IRC() {
	running = false;
	m_globalServer = 0;
	m_servers.SetName("IRC::servers");
}

IRC::~IRC() {
	map<int32, IRCServer *>::iterator itr;

	m_servers.writelock(__FUNCTION__, __LINE__);
	for (itr = servers.begin(); itr != servers.end(); itr++)
		safe_delete(itr->second);
	m_servers.releasewritelock(__FUNCTION__, __LINE__);

	safe_delete(m_globalServer);
}

ThreadReturnType ServerLoop(void *arg);

void IRC::Start() {
	if (running)
		return;

	running = true;
#ifdef _WIN32
	_beginthread(ServerLoop, 0, this);
#else
	pthread_t thread;
	pthread_create(&thread, NULL, ServerLoop, this);
	pthread_detach(thread);
#endif
}

int32 IRC::GetNumServers() {
	int32 count;

	m_servers.readlock(__FUNCTION__, __LINE__);
	count = servers.size();
	m_servers.releasereadlock(__FUNCTION__, __LINE__);

	return count;
}

const char * IRC::GetSafeChannelName(const char *channel_name) {
	char *safe_channel_name;
	size_t len;

	assert(channel_name != NULL);

	len = strlen(channel_name) + 2;
	safe_channel_name = new char[len];
	if (channel_name[0] != '#')
		snprintf(safe_channel_name, len, "#%s", channel_name);
	else
		strncpy(safe_channel_name, channel_name, len);

	return safe_channel_name;
}

void IRC::ConnectToServer(Client *client, const char *host, short port, const char *nick) {
	int32 character_id;
	IRCServer *server;
	int ret;

	assert(client != NULL);
	assert(host != NULL);

	character_id = client->GetCharacterID();

	m_servers.writelock(__FUNCTION__, __LINE__);
	if (servers.count(character_id) > 0)
		client->Message(CHANNEL_COLOR_YELLOW, "You are already connected to IRC server %s.", servers[character_id]->GetHost());
	else {
		client->Message(CHANNEL_COLOR_YELLOW, "Connecting to IRC server %s:%i.", host, port);
		server = new IRCServer(character_id, host, port, nick == NULL || strlen(nick) == 0 ? client->GetPlayer()->GetName() : nick);
		ret = server->Connect();

		switch (ret) {
			case IRC_CONNECT_SUCCESS:
				servers[character_id] = server;
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Successfully connected to IRC server!");
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You can now join channels on this server using /irc join <channel name>.");
				break;
			case IRC_CONNECT_ALREADY_CONNECTED:
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are already connected to this IRC server!");
				break;
			case IRC_CONNECT_NO_NICK:
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Could not connect to IRC server. A nick was never given.");
				break;
			case IRC_CONNECT_WINSOCK_INIT:
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Could not connect to IRC server. Failed to initailize Winsock.");
				break;
			case IRC_CONNECT_WINSOCK_VERSION:
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Could not connect to IRC server. Winsock version 2.2 was not found. Contact your EQ2Emu server administrator.");
				break;
			case IRC_CONNECT_SOCKET:
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Could not connect to IRC server. Failed to create a socket.");
				break;
			case IRC_CONNECT_NO_HOST:
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Could not connect to IRC server. That host does not exist.");
				break;
			case IRC_CONNECT_FAIL:
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Could not connect to IRC server. Failed to connect.");
				break;
			case IRC_CONNECT_SOCKET_OPT:
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "Could not connect to IRC server. Could not set socket options.");
				break;
			default:
				client->Message(CHANNEL_COLOR_YELLOW, "Could not connect to IRC server. Unknown error (%i).", ret);
				break;
		}
	}
	m_servers.releasewritelock(__FUNCTION__, __LINE__);
}

void IRC::ConnectToGlobalServer(const char *host, short port, const char *nick) {
	IRCServer* server = 0;
	ChatChannel* channel = 0;
	const char* channel_name = 0;
	int ret;

	assert(host != NULL);
	assert(nick != NULL);

	if (m_globalServer)
		LogWrite(CHAT__ERROR, 0, "IRC", "You are already connected to the global IRC server %s.", m_globalServer->GetHost());
	else {
		LogWrite(CHAT__DEBUG, 0, "IRC", "Connecting to IRC server %s:%i.", host, port);
		server = new IRCServer(host, port, nick);
		ret = server->Connect();

		switch (ret) {
			case IRC_CONNECT_SUCCESS:
				m_globalServer = server;
				LogWrite(CHAT__DEBUG, 0, "IRC", "Successfully connected to the global IRC server!");

				// Get the global irc channel
				channel_name = rule_manager.GetGlobalRule(R_World, IRCChan)->GetString();
				// Join the channel
				m_globalServer->JoinChannel(channel_name);

				// Remove the leading # if there was one
				if (channel_name[0] == '#')
					channel_name++;

				// Check to see if a EQ2 chat channel exists, if not create it
				if (!chat.ChannelExists(channel_name)) {
					//chat.CreateChannel(channel_name);
					ChatChannel* new_channel = new ChatChannel();
					new_channel->SetName(channel_name);
					
					new_channel->SetLevelRestriction(0);
					new_channel->SetClassesAllowed(0);
					new_channel->SetRacesAllowed(0);
					new_channel->SetType(CHAT_CHANNEL_TYPE_WORLD);

					chat.AddChannel(new_channel);
				}

				// Get the EQ2 Channel
				channel = chat.GetChannel(channel_name);
				// Make sure we got the channel
				if (channel) {
					// Set the channel as the global IRC channel
					channel->SetGlobalIRCChannel(true);
				}
				else {
					// "Should" never end up here as we make sure the channel exists before we get it, send an error if we do end up in here some how
					LogWrite(CHAT__ERROR, 0, "IRC", "Unable to set the global IRC channel");
				}

				break;
			case IRC_CONNECT_ALREADY_CONNECTED:
				LogWrite(CHAT__ERROR, 0, "IRC", "You are already connected to the global IRC server!");
				safe_delete(server);
				break;
			case IRC_CONNECT_NO_NICK:
				LogWrite(CHAT__ERROR, 0, "IRC", "Could not connect to global IRC server. A nick was never given.");
				safe_delete(server);
				break;
			case IRC_CONNECT_WINSOCK_INIT:
				LogWrite(CHAT__ERROR, 0, "IRC", "Could not connect to global IRC server. Failed to initailize Winsock.");
				safe_delete(server);
				break;
			case IRC_CONNECT_WINSOCK_VERSION:
				LogWrite(CHAT__ERROR, 0, "IRC", "Could not connect to global IRC server. Winsock version 2.2 was not found. Contact your EQ2Emu server administrator.");
				safe_delete(server);
				break;
			case IRC_CONNECT_SOCKET:
				LogWrite(CHAT__ERROR, 0, "IRC", "Could not connect to global IRC server. Failed to create a socket.");
				safe_delete(server);
				break;
			case IRC_CONNECT_NO_HOST:
				LogWrite(CHAT__ERROR, 0, "IRC", "Could not connect to global IRC server. That host does not exist.");
				safe_delete(server);
				break;
			case IRC_CONNECT_FAIL:
				LogWrite(CHAT__ERROR, 0, "IRC", "Could not connect to global IRC server. Failed to connect.");
				safe_delete(server);
				break;
			case IRC_CONNECT_SOCKET_OPT:
				LogWrite(CHAT__ERROR, 0, "IRC", "Could not connect to global IRC server. Could not set socket options.");
				safe_delete(server);
				break;
			default:
				LogWrite(CHAT__ERROR, 0, "IRC", "Could not connect to global IRC server. Unknown error (%i).", ret);
				safe_delete(server);
				break;
		}
	}
}

void IRC::DisconnectFromServer(Client *client) {
	IRCServer *server;
	int32 character_id;

	assert(client != NULL);

	character_id = client->GetCharacterID();

	m_servers.writelock(__FUNCTION__, __LINE__);
	if (servers.count(character_id) == 0)
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are not connected to an IRC server!");
	else {
		server = servers[character_id];
		client->Message(CHANNEL_COLOR_YELLOW, "You have been disconnected from IRC server %s.", server->GetHost());
		server->Disconnect();
		safe_delete(server);
		servers.erase(character_id);
	}
	m_servers.releasewritelock(__FUNCTION__, __LINE__);
}

void IRC::DisconnectFromGlobalServer() {
	if (!m_globalServer)
		LogWrite(CHAT__ERROR, 0, "IRC", "You are not connected to a global IRC server!");
	else {
		LogWrite(CHAT__DEBUG, 0, "IRC", "You have been disconnected from the global IRC server (%s).", m_globalServer->GetHost());		
		m_globalServer->Disconnect();
		safe_delete(m_globalServer);
	}
}

void IRC::JoinChannel(Client *client, const char *channel_name) {
	const char *safe_channel_name;
	int32 character_id;
	int ret;

	assert(client != NULL);
	assert(channel_name != NULL);

	character_id = client->GetCharacterID();

	//if the user didn't include a hash in the channel name, add it
	safe_channel_name = GetSafeChannelName(channel_name);

	m_servers.readlock(__FUNCTION__, __LINE__);
	if (servers.count(character_id) == 0)
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are not connected to an IRC server!");
	else {
		ret = servers[character_id]->JoinChannel(safe_channel_name);

		switch (ret) {
			case IRC_JOIN_CHANNEL_SUCCESS:
				client->Message(CHANNEL_COLOR_YELLOW, "Joining IRC channel %s.", safe_channel_name);
				if (channel_name[0] == '#')
					channel_name++;

				if (!chat.ChannelExists(channel_name))
					chat.CreateChannel(channel_name);
				if (!chat.IsInChannel(client, channel_name))
					chat.JoinChannel(client, channel_name);

				break;
			case IRC_JOIN_CHANNEL_ALREADY_IN:
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are already in that IRC channel!");
				break;
			default:
				client->Message(CHANNEL_COLOR_YELLOW, "Error joining channel. Unknown error (%i).", ret);
				break;
		}
	}
	m_servers.releasereadlock(__FUNCTION__, __LINE__);

	safe_delete_array(safe_channel_name);
}

void IRC::LeaveChannel(Client *client, const char *channel_name) {
	const char *safe_channel_name;
	int32 character_id;
	int ret;

	assert(client != NULL);
	assert(channel_name != NULL);

	character_id = client->GetCharacterID();

	//if the user didn't include a hash in the channel name, add it
	safe_channel_name = GetSafeChannelName(channel_name);

	m_servers.readlock(__FUNCTION__, __LINE__);
	if (servers.count(character_id) == 0)
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are not connected to an IRC server!");
	else {
		ret = servers[character_id]->LeaveChannel(safe_channel_name);

		switch (ret) {
			case IRC_LEAVE_CHANNEL_SUCCESS:
				client->Message(CHANNEL_COLOR_YELLOW, "You have left IRC channel %s.", safe_channel_name);
				break;
			case IRC_LEAVE_CHANNEL_NOT_IN:
				client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are not in that IRC channel.");
				break;
			default:
				client->Message(CHANNEL_COLOR_YELLOW, "Error leaving channel. Unknown error (%i).", ret);
				break;
		}
	}
	m_servers.releasereadlock(__FUNCTION__, __LINE__);

	safe_delete_array(safe_channel_name);
}

void IRC::ListChannels(Client *client) {
	vector<IRCChannel *> *channels;
	vector<IRCChannel *>::iterator itr;
	IRCServer *server;
	int32 character_id, i;

	assert(client != NULL);

	character_id = client->GetCharacterID();

	m_servers.readlock(__FUNCTION__, __LINE__);
	if (servers.count(character_id) == 0)
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are not connected to an IRC server!");
	else {
		server = servers[character_id];
		channels = server->GetChannels();

		client->Message(CHANNEL_COLOR_YELLOW, "IRC Channels you are logged into on %s:", server->GetHost());
		if (channels->size() == 0)
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "   None");
		else {
			i = 1;
			for (itr = channels->begin(); itr != channels->end(); itr++)
				client->Message(CHANNEL_COLOR_YELLOW, "   %i) %s", i++, ((*itr)->GetName()));
		}
	}
	m_servers.releasereadlock(__FUNCTION__, __LINE__);
}

void IRC::Say(Client *client, const char *channel_name, const char *message) {
	const char *safe_channel_name;
	int32 character_id;
	int ret;
	IRCServer* server = 0;

	assert(channel_name != NULL);
	assert(message != NULL);

	if (client)
		character_id = client->GetCharacterID();

	//if the user didn't include a hash in the channel name, add it
	safe_channel_name = GetSafeChannelName(channel_name);

	m_servers.readlock(__FUNCTION__, __LINE__);
	if (!m_globalServer && servers.count(character_id) == 0) {
		if (client)
			client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are not connected to an IRC server!");
		else
			LogWrite(CHAT__DEBUG, 0, "IRC", "Not connected to a global IRC server");
	}
	else {
		if (client)
			server = servers[character_id];
		else
			server = m_globalServer;

		ret = server->Say(safe_channel_name, message);

		switch (ret) {
			case IRC_SAY_SUCCESS:
				//client->Message(CHANNEL_COLOR_YELLOW, "You say to %s, \"%s\"", safe_channel_name, message);
				break;
			case IRC_SAY_NOT_IN:
				if (client)
					client->Message(CHANNEL_COLOR_YELLOW, "You are not in channel %s. Use /irc list to see what channels you are in.", safe_channel_name);
				else
					LogWrite(CHAT__DEBUG, 0, "IRC", "Global IRC does not contain the channel %s.", safe_channel_name);
				break;
			default:
				if (client)
					client->Message(CHANNEL_COLOR_YELLOW, "Error sending message to %s. Unknown error (%i).", safe_channel_name, ret);
				else
					LogWrite(CHAT__DEBUG, 0, "IRC", "Error sending message to %s. Unknown error (%i).", safe_channel_name, ret);

				break;
		}
	}
	m_servers.releasereadlock(__FUNCTION__, __LINE__);

	safe_delete_array(safe_channel_name);
}

void IRC::Say(Client *client, int32 channel_index, const char *message) {
	IRCChannel *channel;
	int32 character_id;
	int ret;

	assert(client != NULL);
	assert(message != NULL);

	character_id = client->GetCharacterID();

	m_servers.readlock(__FUNCTION__, __LINE__);
	if (servers.count(character_id) == 0)
		client->SimpleMessage(CHANNEL_COLOR_YELLOW, "You are not connected to an IRC server!");
	else {
		ret = servers[character_id]->Say(channel_index, message);
		channel = servers[character_id]->GetChannel(channel_index);

		switch (ret) {
			case IRC_SAY_SUCCESS:
				client->Message(CHANNEL_COLOR_YELLOW, "You say to %s, \"%s\"", channel->GetName(), message);
				break;
			case IRC_SAY_NOT_IN:
				client->Message(CHANNEL_COLOR_YELLOW, "You are not in a channel at index %u. Use /irc list to see what channels you are in.", channel_index);
				break;
			default:
				client->Message(CHANNEL_COLOR_YELLOW, "Error sending message to channel at index %i. Unknown error (%i).", channel_index, ret);
				break;
		}
	}
	m_servers.releasereadlock(__FUNCTION__, __LINE__);
}

void IRC::Process() {
	map<int32, IRCServer *>::iterator itr;
	vector<int32> removes;
	vector<int32>::iterator itr_removes;
	int32 character_id;
	IRCServer *server;

	m_servers.readlock(__FUNCTION__, __LINE__);
	for (itr = servers.begin(); itr != servers.end(); itr++) {
		server = itr->second;
		if (server->IsConnected()) {

			//if this connection fails to process, disconnect it
			if (!server->Process())
				removes.push_back(itr->first);
		}
	}
	m_servers.releasereadlock(__FUNCTION__, __LINE__);

	// Process the global irc server if there is one
	if (m_globalServer && m_globalServer->IsConnected())
		m_globalServer->Process();

	//process any bad connections
	if (removes.size() > 0) {
		m_servers.writelock(__FUNCTION__, __LINE__);
		for (itr_removes = removes.begin(); itr_removes != removes.end(); itr_removes++) {
			character_id = *itr_removes;

			if (servers.count(character_id) > 0) {
				safe_delete(servers[character_id]);
				servers.erase(character_id);
			}
		}
		m_servers.releasewritelock(__FUNCTION__, __LINE__);
	}
}

ThreadReturnType ServerLoop(void *arg) {
	IRC *irc = (IRC *)arg;

	while (irc->IsRunning()) {
		irc->Process();
		
		if (irc->GetNumServers() == 0)
			Sleep(1000);
		else
			Sleep(100);
	}

	THREAD_RETURN(NULL);
}

IRCServer* IRC::GetServer(Client* client) {
	IRCServer* ret = 0;

	m_servers.readlock(__FUNCTION__, __LINE__);
	if (servers.count(client->GetCharacterID()) != 0)
		ret = servers[client->GetCharacterID()];
	m_servers.releasereadlock(__FUNCTION__, __LINE__);

	return ret;
}