#pragma once

#include "../common/TCPConnection.h"

class MasterServer {
public:
	MasterServer();
	~MasterServer();

	bool Connect();
	bool Connected() { return tcpc->Connected(); }
	bool Process();
	void SayHello();

private:
	TCPConnection* tcpc;
};