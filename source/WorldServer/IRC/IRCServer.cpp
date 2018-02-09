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
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <WinSock2.h>
#include <windows.h>
#else
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "../../common/unix.h"
#endif
#include "../World.h"
#include "IRCReplyCodes.h"
#include "IRCServer.h"
#include "../Chat/Chat.h"
#include "../../common/Log.h"

extern ZoneList zone_list;
extern Chat chat;

IRCServer::IRCServer() {
	character_id = 0;
	memset(host, 0, sizeof(host));
	port = 0;
	memset(nick, 0, sizeof(nick));
	sockfd = -1;
	connected = false;
	m_globalServer = false;
}

IRCServer::IRCServer(int32 character_id, const char *host, short port, const char *nick) {
	assert(host != NULL);
	assert(nick != NULL);

	this->character_id = character_id;
	strncpy(this->host, host, IRC_HOST_LEN_MAX);
	this->port = port;
	strncpy(this->nick, nick, IRC_NICK_LEN_MAX);
	sockfd = -1;
	connected = false;
	m_globalServer = false;
}

IRCServer::IRCServer(const char *host, short port, const char *nick) {
	assert(host != NULL);
	assert(nick != NULL);

	this->character_id = 0;
	strncpy(this->host, host, IRC_HOST_LEN_MAX);
	this->port = port;
	strncpy(this->nick, nick, IRC_NICK_LEN_MAX);
	sockfd = -1;
	connected = false;
	m_globalServer = true;
}

IRCServer::~IRCServer() {
	Disconnect();
}

IRCChannel * IRCServer::GetChannel(const char *channel_name) {
	vector<IRCChannel *>::iterator itr;
	IRCChannel *channel = NULL;

	assert(channel_name != NULL);

	for (itr = channels.begin(); itr != channels.end(); itr++) {
		if (strncmp((*itr)->GetName(), channel_name, IRC_CHANNEL_LEN_MAX) == 0) {
			channel = *itr;
			break;
		}
	}

	return channel;
}

IRCChannel * IRCServer::GetChannel(int32 channel_index) {
	//channel indexes start at 1 according to the client
	channel_index--;

	return channel_index < channels.size() ? channels[channel_index] : NULL;
}

int IRCServer::Connect() {
	struct sockaddr_in server_addr;
	struct hostent *server;
#ifdef _WIN32
	u_long nonblocking = 1;
	WSADATA wsa;
#else
	int nonblocking = 1;
#endif

	//you're already connected numbnuts!
	if (connected)
		return IRC_CONNECT_ALREADY_CONNECTED;

	//make sure a nick was given (should always have one)
	if (nick[0] == '\0')
		return IRC_CONNECT_NO_NICK;

#ifdef _WIN32
	//initialize winsock for version 2.2
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		Disconnect();
		return IRC_CONNECT_WINSOCK_INIT;
	}

	//make sure we support this version of winsock
	if (LOBYTE(wsa.wVersion) != 2 || HIBYTE(wsa.wHighVersion) != 2) {
		Disconnect();
		return IRC_CONNECT_WINSOCK_VERSION;
	}
#endif

	//attempt to create a tcp socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		Disconnect();
		return IRC_CONNECT_SOCKET;
	}

	//find the hostname
	if ((server = gethostbyname(host)) == NULL) {
		Disconnect();
		return IRC_CONNECT_NO_HOST;
	}

	//setup the server info
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
	server_addr.sin_port = htons(port);

	//connect to the irc server
	if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		Disconnect();
		return IRC_CONNECT_FAIL;
	}

	//we want a non blocking socket
#ifdef _WIN32
	if (ioctlsocket(sockfd, FIONBIO, &nonblocking) != 0) {
#else
	if (ioctl(sockfd, FIONBIO, &nonblocking) != 0) {
#endif
		Disconnect();
		return IRC_CONNECT_SOCKET_OPT;
	}

	//woohoo! connected - send the register messages
	Send(new IRCMessage("NICK %s", nick));
	Send(new IRCMessage("USER %s 0 * :scott", nick));

	connected = true;
	return IRC_CONNECT_SUCCESS;
}

void IRCServer::Disconnect() {
	connected = false;
#ifdef _WIN32
	if (sockfd > 0) {
		closesocket(sockfd);
		sockfd = -1;
	}
	WSACleanup();
#else
	if (sockfd > 0) {
		close(sockfd);
		sockfd = -1;
	}
#endif

	vector<IRCChannel*>::iterator itr;
	for (itr = channels.begin(); itr != channels.end(); itr++) {
		safe_delete(*itr);
	}
	channels.clear();
}

int IRCServer::JoinChannel(const char *channel_name) {
	vector<IRCChannel *>::iterator itr;
	IRCChannel *channel;

	assert(channel_name != NULL);

	//let's make sure they're already not in the channel
	for (itr = channels.begin(); itr != channels.end(); itr++) {
		if (strncmp((*itr)->GetName(), channel_name, IRC_CHANNEL_LEN_MAX) == 0)
			return IRC_JOIN_CHANNEL_ALREADY_IN;
	}

	//add the channel to the list of channels and mark it as joining. we'll unmark it once we get a response back from the server
	channel = new IRCChannel(channel_name);
	channel->SetJoining(true);
	channels.push_back(channel);

	//send the JOIN command to the IRC server
	Send(new IRCMessage("JOIN %s", channel_name));

	return IRC_JOIN_CHANNEL_SUCCESS;
}

int IRCServer::LeaveChannel(const char *channel_name) {
	vector<IRCChannel *>::iterator itr;
	IRCChannel *channel;

	assert(channel_name != NULL);

	for (itr = channels.begin(); itr != channels.end(); itr++) {
		channel = *itr;
		if (strncmp(channel->GetName(), channel_name, IRC_CHANNEL_LEN_MAX) == 0) {

			//send the leave message to the IRC server
			Send(new IRCMessage("PART %s", channel->GetName()));

			//free resourceses
			safe_delete(channel);
			channels.erase(itr);

			return IRC_LEAVE_CHANNEL_SUCCESS;
		}
	}

	return IRC_LEAVE_CHANNEL_NOT_IN;
}

int IRCServer::Say(const char *channel_name, const char *message) {
	IRCChannel *channel;

	assert(channel_name != NULL);
	assert(message != NULL);

	if ((channel = GetChannel(channel_name)) == NULL)
		return IRC_SAY_NOT_IN;

	Say(channel, message);
	return IRC_SAY_SUCCESS;
}

int IRCServer::Say(int32 channel_index, const char *message) {
	IRCChannel *channel;

	assert(message != NULL);

	if ((channel = GetChannel(channel_index)) == NULL)
		return IRC_SAY_NOT_IN;

	Say(channel, message);
	return IRC_SAY_SUCCESS;
}

void IRCServer::Say(IRCChannel *channel, const char *message) {
	assert(channel != NULL);
	assert(message != NULL);

	Send(new IRCMessage("PRIVMSG %s %s", channel->GetName(), message));
}

bool IRCServer::Process() {
	char *start, *end;
	char buf[8192];
	int i, count;
	Client *client = 0;

	if (!connected)
		return false;

	//make sure we can find a client associated with this character id
	if (!m_globalServer && (client = zone_list.GetClientByCharID(character_id)) == NULL) {
		LogWrite(CHAT__ERROR, 0, "IRC", "No client found with character ID %u", character_id);
		Disconnect();
		return false;
	}

	//read off the socket - remember this socket is non blocking
	memset(buf, 0, sizeof(buf));
	count = recv(sockfd, buf, sizeof(buf), 0);

	//did server shut us down gracefully?
	if (count == 0) {
		if (client)
			client->Message(CHANNEL_COLOR_YELLOW, "You have been disconnected from IRC server %s.", host);
		else
			LogWrite(CHAT__DEBUG, 0, "IRC", "You have been disconnected from global IRC server");
		Disconnect();
		return false;
	}

	//did we get an error?
	//if we get WSAWEOULDBLOCK or EAGAIN, then there was no data for us on the socket this time around, don't disconnect
#ifdef _WIN32
	if (count < 0) {
		if (WSAGetLastError() == WSAEWOULDBLOCK)
			return true;
#else
	if (count < 0) {
		if (errno == EAGAIN)
			return true;
#endif
		else {
			if (client)
				client->Message(CHANNEL_COLOR_YELLOW, "You have been disconnected from IRC server %s because of an error.", host);
			else
				LogWrite(CHAT__DEBUG, 0, "IRC", "You have been disconnected from global IRC server because of an error");
			Disconnect();
			return false;
		}
	}

	//if we made it here then we got data!
	//irc messages are separated by \r\n, so loop until we find a \n, replace it with a null to create a string then process it
	start = buf;
	end = buf;
	i = 0;

	while  (i < count) {
		if (*end == '\n') {
			*end = '\0';
			ProcessLine(client, start);

			//move to the next potential string
			start = end + 1;
		}

		i++;
		end++;
	}

	return true;
}

void IRCServer::ProcessLine(Client *client, const char *line) {
	char *ptr, *ptr2, *ptr3, *ptr4, *findme;
	size_t len;
	char *copy;

	//assert(client != NULL);
	assert(line != NULL);

	if ((len = strlen(line)) == 0) {
		LogWrite(CHAT__DEBUG, 0, "IRC", "Blank line recieved in IRCServer::ProcessLine");
		//fprintf(stderr, "Blank line received in IRCServer::ProcessLine\n");
		return;
	}

	LogWrite(CHAT__DEBUG, 7, "IRC", "%s", line);
	//printf("%s\n", line);

	//copy the line buffer so we dont ruin it with strtok
	copy = new char[len + 1];
	strncpy(copy, line, len);
	
	if (len >= 4 && strncmp(line, "PING", 4) == 0)
		HandlePing();
	else if (line[0] == ':') {
		ptr = strtok(copy, " :");

		if (ptr == NULL) {
			LogWrite(CHAT__DEBUG, 0, "IRC", "Unknown IRC line in IRCServer::ProcessLine: '%s'", line);
			//fprintf(stderr, "Unknown IRC line in IRCServer::ProcessLine: '%s'\n", line);
		}
		else {

			//:scatman!scatman@hidden-BC7E1801.hsd1.pa.comcast.net PRIVMSG #EQ2Dev :test	--from user, public channel msg
			//:scatman!scatman@hidden-BC7E1801.hsd1.pa.comcast.net PRIVMSG scatman :test	--from user, private message

			//is this message from the server or a user?
			if (strncmp(ptr, host, IRC_HOST_LEN_MAX) == 0) {
			}
			else {
				ptr2 = strtok(NULL, " :");
				if (ptr2 == NULL) {
					LogWrite(CHAT__DEBUG, 0, "IRC", "Unknown IRC line in IRCServer::ProcessLine: '%s'", line);
					//fprintf(stderr, "Unknown IRC line in IRCServer::ProcessLine: '%s'\n", line);
				}
				else {

					if (strncmp(ptr2, "PRIVMSG", 7) == 0) {
						ptr3 = strtok(NULL, " :");	//channel
						ptr4 = strtok(NULL, "\r");	//message

						//strip the user's address
						if ((findme = strstr(ptr, "!")) != NULL)
							*findme = '\0';

						//remove the leading semi-colon from the beginning of the message
						if (ptr4[0] == ':')
							ptr4++;

						if (ptr[0] == ':')
							ptr++;

						//client->Message(CHANNEL_COLOR_YELLOW, "%s says to %s: \"%s\"", ptr, ptr3, ptr4);
						if (ptr3[0] == '#')
							ptr3++;

						// Add "[IRC]" to the front of the name
						string name = string("[IRC]") + string(ptr);
						LogWrite(CHAT__DEBUG, 7, "IRC", "IRC sending chat to EQ2 channel (Channel Name: %s, Message: %s, Name: %s)", ptr3, ptr4, name.c_str());
						chat.TellChannel(client, ptr3, ptr4, name.c_str());
					}

				}
			}
		}
	}

	safe_delete_array(copy);
}

void IRCServer::Send(IRCMessage *message) {
	const char *msg;
	size_t len;
	int count;
	
	assert(message != NULL);

	msg = message->Serialize();
	len = message->GetLength();

	if (msg == NULL) {
		LogWrite(CHAT__ERROR, 0, "IRC", "Error sending IRC message to server %s. Message was null.", host);
		//fprintf(stderr, "Error sending IRC message to server %s. Message was null.", host);
	}
	else if (len == 0) {
		LogWrite(CHAT__ERROR, 0, "IRC", "Error sending IRC message server %s. Message length was 0.", host);
		//fprintf(stderr, "Error sending IRC message server %s. Message length was 0.", host);
	}
	else {
		LogWrite(CHAT__DEBUG, 7, "IRC", "IRC MSG: '%s'", msg);
		//printf("IRC MSG: '%s'", msg);
		if ((count = send(sockfd, msg, len, 0)) != (int)len) {
			LogWrite(CHAT__ERROR, 0, "IRC", "Error sending IRC message '%s' to server %s. Tried to write %u bytes but only wrote %i", msg, host, len, count);
			//fprintf(stderr, "Error sending IRC message '%s' to server %s. Tried to write %u bytes but only wrote %i", msg, host, len, count);
		}
	}

	safe_delete(message);
}

void IRCServer::HandlePing() {
	Send(new IRCMessage("PONG %s", host));
}
