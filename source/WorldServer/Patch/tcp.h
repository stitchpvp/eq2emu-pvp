#pragma once

#if defined(_WIN32)
# include <WinSock2.h>
# include <WS2tcpip.h>
# define SOCKET_INVALID                     INVALID_SOCKET
# define SOCKET_CLOSE(s)                    do {closesocket(s); (s) = SOCKET_INVALID;} while(0)
# define SOCKET_SOCKOPT_REUSEADDR_CAST(opt) (char *)(&(opt))
# define poll                               WSAPoll
typedef SOCKET socket_t;
typedef BOOL sockopt_reuseaddr_t;
#else
# include <unistd.h>
# include <sys/socket.h>
# include <sys/poll.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# define SOCKET_INVALID                     (-1)
# define SOCKET_ERROR                       (-1)
# define SOCKET_CLOSE(s)                    do {close(s); (s) = SOCKET_INVALID;} while(0)
# define SOCKET_SOCKOPT_REUSEADDR_CAST(opt) (&(opt))
typedef int socket_t;
typedef int sockopt_reuseaddr_t;
#endif

class TCP {
public:
    static bool Start();
    static void Stop();

    static void StoreError(char *dst, unsigned int size);

    static bool GetIPAddressString(struct sockaddr_storage *addr, char *host, unsigned int host_size, char *port, unsigned int port_size);
};
