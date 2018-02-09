#pragma once

#include "tcp-client.h"

class PatchServer {
public:
    PatchServer();
    ~PatchServer();

    void SetHost(const char *host);
    void SetPort(const char *port);
    void SetEnabled(bool enabled);
    void SetReading(bool reading);
    void SetQuitAfter(bool quit_after);

    bool IsEnabled();
    bool QuitAfter();

    bool Start();
    void Stop();
    bool Process();

private:
    TCPClient tcp;
    bool enabled;
    bool reading;
    bool quit_after;
};
