#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <vector>
#include "../../common/MiscFunctions.h"
#include "../../common/Log.h"
#include "buffer.h"
#include "../WorldDatabase.h"
#include "../../common/version.h"
#include "patch.h"

using namespace std;

extern WorldDatabase database;
extern PatchServer patch;

static void TCPClientDisconnect(TCPClient *tcp) {
}

static void TCPClientData(TCPClient *tcp, const char *data, unsigned int len) {
    static FILE *f = NULL;

    if (f == NULL) {
        if (len == 1) {
			LogWrite(PATCHER__INFO, 0, "Patcher", "Server is up to date");
            patch.SetReading(false);
            return;
        }

        if (strncmp(data, "ERROR", 5) == 0) {
			LogWrite(PATCHER__ERROR, 0, "Patcher", "There was an error downloading updates");
            tcp->Disconnect();
            patch.SetReading(false);
            patch.SetQuitAfter(true);
            return;
        }

        if (strncmp(data, "SPEED", 5) == 0) {
			LogWrite(PATCHER__ERROR, 0, "Patcher", "You have tried to download updates too quickly and are now locked out, try again later");
            tcp->Disconnect();
            patch.SetReading(false);
            patch.SetQuitAfter(true);
            return;
        }

        if (strncmp(data, "LOCKOUT", 7) == 0) {
			LogWrite(PATCHER__ERROR, 0, "Patcher", "You are locked out, try again later");
            tcp->Disconnect();
            patch.SetReading(false);
            patch.SetQuitAfter(true);
            return;
        }

        if (strncmp(data, "UNKNOWN", 7) == 0) {
			LogWrite(PATCHER__ERROR, 0, "Patcher", "An unknown error occured");
            tcp->Disconnect();
            patch.SetReading(false);
            patch.SetQuitAfter(true);
            return;
        }

        f = fopen("eq2-updates.sql", "wb");
        if (f == NULL) {
			LogWrite(PATCHER__ERROR, 0, "Patcher", "Failed to open 'eve-updates.sql' for writing: %s", strerror(errno));
            tcp->Disconnect();
            patch.SetReading(false);
            patch.SetQuitAfter(true);
            return;
        }

        fprintf(f, "SET unique_checks=0;\n");
        fprintf(f, "SET foreign_key_checks=0;\n");
		LogWrite(PATCHER__INFO, 0, "Patcher", "Downloading updates");
    }

    if (memchr(data, '\0', len) == NULL)
        fwrite(data, sizeof(*data), len, f);
    else {
        fwrite(data, sizeof(*data), len - 1, f);
        fprintf(f, "SET foreign_key_checks=1;\n");
        fprintf(f, "SET unique_checks=1;\n");
        fclose(f);
        f = NULL;
		LogWrite(PATCHER__INFO, 0, "Patcher", "Updates finished downloading");

		LogWrite(PATCHER__INFO, 0, "Patcher", "Executing updates");
        //database.BeginTransaction();
        if (!database.QueriesFromFile("eq2-updates.sql")) {
            //database.RollbackTransaction();
			LogWrite(PATCHER__INFO, 0, "Patcher", "Failed to update database");
        }
        else {
            //database.CommitTransaction();
			LogWrite(PATCHER__INFO, 0, "Patcher", "Database updated");
        }
        patch.SetReading(false);
    }
}

PatchServer::PatchServer() {
    enabled = true;
    reading = false;
    quit_after = false;
}

PatchServer::~PatchServer() {
    Stop();
}

void PatchServer::SetHost(const char *host) {
    tcp.SetHost(host);
}

void PatchServer::SetPort(const char *port) {
    tcp.SetPort(port);
}

void PatchServer::SetEnabled(bool enabled) {
    this->enabled = enabled;
}

void PatchServer::SetReading(bool reading) {
    this->reading = reading;
}

void PatchServer::SetQuitAfter(bool quit_after) {
    this->quit_after = quit_after;
}

bool PatchServer::IsEnabled() {
    return enabled;
}

bool PatchServer::QuitAfter() {
    return quit_after;
}

bool PatchServer::Start() {
    TCPClientCallbacks callbacks;
    vector<TableVersion *> table_versions;
    vector<TableVersion *>::iterator itr;
    Buffer buffer;

    TCP::Start();

    if (!enabled) {
		LogWrite(PATCHER__INFO, 0, "Patcher", "Skipping check for database updates from the patch server");
        return true;
    }
    
    callbacks.data = TCPClientData;
    callbacks.disconnect = TCPClientDisconnect;
    tcp.SetCallbacks(&callbacks);

    if (!tcp.Start()) {
		LogWrite(PATCHER__ERROR, 0, "Patcher", "Failed to connect to patch server at %s:%s: %s", tcp.GetHost()[0] == '\0' ? "localhost" : tcp.GetHost(), tcp.GetPort(), tcp.GetError());
        return false;
    }

	LogWrite(PATCHER__INFO, 0, "Patcher", "Connected to patch server at %s:%s", tcp.GetHost()[0] == '\0' ? "localhost" : tcp.GetHost(), tcp.GetPort());

    if (!database.GetTableVersions(&table_versions))
        return false;
	
	int32 version = (CURRENT_DATABASE_MAJORVERSION * 100) + CURRENT_DATABASE_MINORVERSION;
    buffer.WriteUInt32(version);
    for (itr = table_versions.begin(); itr != table_versions.end(); itr++) {
        buffer.WriteUInt8((*itr)->name_len);
        buffer.Write((*itr)->name, (*itr)->name_len);
        buffer.WriteUInt32((*itr)->version);
        buffer.WriteUInt32((*itr)->data_version);

        free(*itr);
    }
    buffer.WriteAtUInt32(0, buffer.GetLength());

    reading = true;

    return tcp.Queue(buffer.Peek(), buffer.GetLength());
}

void PatchServer::Stop() {
    tcp.Stop();
    TCP::Stop();
}

bool PatchServer::Process() {
    bool success = true;

    if (!enabled)
        return true;

    while (success && reading) {
        success = tcp.Process(0);
        SleepMS(10);
    }

    return success;
}
