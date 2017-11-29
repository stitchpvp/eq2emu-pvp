#pragma once

#include "tcp.h"

class TCPClient;

typedef void(*TCPClientDisconnectCallback)(TCPClient *);
typedef void(*TCPClientDataCallback)(TCPClient *, const char *, unsigned int);

typedef struct {
    TCPClientDisconnectCallback disconnect;
    TCPClientDataCallback data;
} TCPClientCallbacks;

class TCPClient {
public:
    TCPClient();
    ~TCPClient();

    void SetHost(const char *host);
    void SetPort(const char *port);
    void SetCallbacks(TCPClientCallbacks *callbacks);

    const char * GetHost();
    const char * GetPort();
    bool IsConnected();
    bool HasError();

    const char * GetError();
//    struct sockaddr_storage * GetAddress();

    bool Start();
    void Stop();
    bool Process(unsigned int timeout);

    void Disconnect();
    bool Queue(const char *buf, unsigned int len);

private:
    char host[64];
    char port[16];
    bool connected;
    TCPClientCallbacks callbacks;
    char error[256];
    struct pollfd fds;
//    struct sockaddr_storage addr;
//    socklen_t addr_len;
    char *outgoing;
    unsigned int outgoing_size;
    unsigned int outgoing_len;

    void DisconnectHelper(bool silent);
    bool Read();
    bool Write();
};
