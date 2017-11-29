#include <stdlib.h>
#include "string.h"
#include "tcp-client.h"
#include "../../common/MiscFunctions.h"

TCPClient::TCPClient() {
    host[0] = '\0';
    port[0] = '\0';
    connected = false;
    memset(&callbacks, 0, sizeof(callbacks));
    error[0] = '\0';
    fds.fd = SOCKET_INVALID;
//    memset(&addr, 0, sizeof(addr));
//    addr_len = 0;
    outgoing = NULL;
    outgoing_size = 0;
    outgoing_len = 0;
}

TCPClient::~TCPClient() {
    Stop();
}

void TCPClient::SetHost(const char *host) {
    if (host == NULL)
        this->host[0] = '\0';
    else
        strlcpy(this->host, host, sizeof(this->host));
}

void TCPClient::SetPort(const char *port) {
    strlcpy(this->port, port, sizeof(this->port));
}

void TCPClient::SetCallbacks(TCPClientCallbacks *callbacks) {
    memcpy(&this->callbacks, callbacks, sizeof(this->callbacks));
}

const char * TCPClient::GetHost() {
    return host;
}

const char * TCPClient::GetPort() {
    return port;
}

bool TCPClient::IsConnected() {
    return connected;
}

bool TCPClient::HasError() {
    return error[0] != '\0';
}

const char * TCPClient::GetError() {
    return error;
}

//struct sockaddr_storage * TCPClient::GetAddress() {
//    return &addr;
//}

bool TCPClient::Start() {
    struct addrinfo hints, *server, *p;
    socket_t fd;
    int ret;

    if (connected)
        return true;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    //get list of networks
    ret = getaddrinfo(host[0] == '\0' ? NULL : host, port, &hints, &server);
    if (ret != 0) {
        strlcpy(error, gai_strerror(ret), sizeof(error));
        return false;
    }

    //loop through the addresses and bind to the first one we can
    for (p = server; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd == SOCKET_INVALID) {
            TCP::StoreError(error, sizeof(error));
            continue;
        }

        //connect to the server
        if (connect(fd, p->ai_addr, (int)p->ai_addrlen) == SOCKET_ERROR) {
            TCP::StoreError(error, sizeof(error));
            SOCKET_CLOSE(fd);
            continue;
        }

        break;
    }

    connected = p != NULL;

    if (connected) {
        fds.fd = fd;
        fds.events = POLLIN | POLLOUT;
        fds.revents = 0;
//        memcpy(&addr, p->ai_addr, sizeof(addr));
//        addr_len = p->ai_addrlen;
        error[0] = '\0';
    }

    freeaddrinfo(server);

    return connected;
}

void TCPClient::Stop() {
    if (connected)
        DisconnectHelper(false);
}

void TCPClient::DisconnectHelper(bool silent) {
    if (!silent && callbacks.disconnect != NULL)
        callbacks.disconnect(this);

    connected = false;
    error[0] = '\0';

    if (fds.fd != SOCKET_INVALID)
        SOCKET_CLOSE(fds.fd);

//    memset(&addr, 0, sizeof(addr));
//    addr_len = 0;

    if (outgoing != NULL) {
        free(outgoing);
        outgoing = NULL;
    }
    outgoing_size = 0;
    outgoing_len = 0;
}

void TCPClient::Disconnect() {
    DisconnectHelper(true);
}

bool TCPClient::Read() {
    char buf[4096];
    int count;

    count = recv(fds.fd, buf, sizeof(buf), 0);

    if (count == SOCKET_ERROR) {
        TCP::StoreError(error, sizeof(error));
        return false;
    }

    //disconnected
    if (count == 0) {
        DisconnectHelper(false);
        return true;
    }

    callbacks.data(this, buf, count);

    return true;
}

bool TCPClient::Write() {
    int count;

    count = send(fds.fd, outgoing, outgoing_len, 0);
    if (count == SOCKET_ERROR) {
        TCP::StoreError(error, sizeof(error));
        return false;
    }

    outgoing_len -= count;

    return true;
}

bool TCPClient::Process(unsigned int timeout) {
    int count;

    if (!connected)
        return false;

    count = poll(&fds, 1, timeout);

    if (count == SOCKET_ERROR) {
        TCP::StoreError(error, sizeof(error));
        return false;
    }

    if (fds.revents == 0)
        return true;

    //socket is bad
    if (fds.revents & POLLERR) {
        DisconnectHelper(false);
        return false;
    }

    //hung up
    if (fds.revents & POLLHUP) {
        DisconnectHelper(false);
        return false;
    }

    //socket is ready for reading
    if (fds.revents & POLLIN) {
        if (!Read())
            return false;
    }

    //socket is ready for writing
    if (fds.revents & POLLOUT) {
        if (outgoing_len > 0) {
            if (!Write())
                return false;
        }
    }

    return true;
}

bool TCPClient::Queue(const char *buf, unsigned int len) {
    unsigned int new_size;
    char *new_outgoing;

    while (outgoing_len + len > outgoing_size) {
        new_size = outgoing_size == 0 ? 1024 : outgoing_size * 2;
        new_outgoing = (char *)realloc(outgoing, new_size);
        if (new_outgoing == NULL)
            return false;

        outgoing = new_outgoing;
        outgoing_size = new_size;
    }

    memcpy(outgoing + outgoing_len, buf, len);
    outgoing_len += len;

    return true;
}
