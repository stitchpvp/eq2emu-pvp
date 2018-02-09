#include "stdio.h"
#include "string.h"
#if defined(_WIN32)
#else
# include <errno.h>
#endif
#include "tcp.h"

#include "../../common/MiscFunctions.h"

#if defined(_WIN32)
static int initialized = 0;
#endif

bool TCP::Start() {
#if defined(_WIN32)
    WSAData wsa;

    //each program only needs to do this once. so if mulitple TCP servers or
    //clients are being used, we only need to initialize WinSock once
    if (initialized == 0) {
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
            return false;
    }

    ++initialized;
#endif

    return true;
}

void TCP::Stop() {
#if defined(_WIN32)
    if (initialized > 0) {
        if (--initialized == 0)
            WSACleanup();
    }
#endif
}

void TCP::StoreError(char *dst, unsigned int size) {
#if defined(_WIN32)
    char *ptr;

    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&ptr, 0, NULL);
    strlcpy(dst, ptr, size);
    LocalFree(ptr);

    ptr = strchr(dst, '\n');
    if (ptr != NULL)
        *ptr = '\0';
#else
    strlcpy(dst, strerror(errno), size);
#endif
}

bool TCP::GetIPAddressString(struct sockaddr_storage *addr, char *host, unsigned int host_size, char *port, unsigned int port_size) {
    switch (addr->ss_family) {
        case AF_INET:
            inet_ntop(AF_INET, &(((struct sockaddr_in *)addr)->sin_addr), host, host_size);
            if (port != NULL)
                snprintf(port, port_size, "%u", ((struct sockaddr_in *)addr)->sin_port);
            break;
        case AF_INET6:
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)addr)->sin6_addr), host, host_size);
            if (port != NULL)
                snprintf(port, port_size, "%u", ((struct sockaddr_in6 *)addr)->sin6_port);
            break;
        default:
            host[0] = '\0';
            if (port != NULL)
                port[0] = '\0';
            break;
    }

    return host[0] != '\0' && (port == NULL || port[0] != '\0');
}
