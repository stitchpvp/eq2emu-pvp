#pragma once

#include "../common/TCPConnection.h"

class MasterServer {
public:
	MasterServer();
	~MasterServer();

	bool Connect();
	void SayHello();

private:
	TCPConnection* tcpc;
};