#include "wsocket.h"
#include "wdef.h"

#ifdef OS_WIN
#include "watomic.h"
static atomic_flag s_wsa_initialized = ATOMIC_FLAG_INIT;
void WSAInit(void) {
    if (!atomicFlagTestAndSet(&s_wsa_initialized)) {
        WSADATA wsadata;
        WSAStartup(MAKEWORD(2, 2), &wsadata);
    }
}

void WSADeinit(void) {
    if (atomicFlagTestAndSet(&s_wsa_initialized)) {
        atomicFlagClear(&s_wsa_initialized);
        WSACleanup();
    }
}
#endif

static inline int socketErrnoNegative(int sockfd) {
    int err = socketERRNO();
    if (sockfd >= 0) closesocket(sockfd);
    return err > 0 ? -err : -1;
}

const char* socketStrError(int err) {
#ifdef OS_WIN
    static char buffer[128];

    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, 0, ABS(err), 0, buffer, sizeof(buffer), NULL);

    return buffer;
#else
    return strerror(ABS(err));
#endif
}

bool isIPVer4(const char* host) {
    struct sockaddr_in sin;
    return inet_pton(AF_INET, host, &sin) == 1;
}

bool isIPVer6(const char* host) {
    struct sockaddr_in6 sin6;
    return inet_pton(AF_INET6, host, &sin6) == 1;
}

int resolveAddr(const char* host, sockaddr_u* addr) {
#ifdef OS_WIN
    WSAInit();
#endif
    if (inet_pton(AF_INET, host, &addr->sin.sin_addr) == 1) {
        addr->sa.sa_family = AF_INET; // host is ipv4, so easy ;)
        return 0;
    }

    if (inet_pton(AF_INET6, host, &addr->sin6.sin6_addr) == 1) {
        addr->sa.sa_family = AF_INET6; // host is ipv6
    }

    struct addrinfo* ais = NULL;
    int ret = getaddrinfo(host, NULL, NULL, &ais);
    if (ret != 0 || ais == NULL || ais->ai_addr == NULL || ais->ai_addrlen == 0) {
        printd("unknown host: %s err:%d:%s\n", host, ret, gai_strerror(ret));
        return ret;
    }
    struct addrinfo* pai = ais;
    while (pai != NULL) {
        if (pai->ai_family == AF_INET) break;
        pai = pai->ai_next;
    }
    if (pai == NULL) pai = ais;
    memcpy(addr, pai->ai_addr, pai->ai_addrlen);
    freeaddrinfo(ais);
    return 0;
}

const char* sockaddrIp(sockaddr_u* addr, char* ip, int len) {
    if (addr->sa.sa_family == AF_INET) {
        return inet_ntop(AF_INET, &addr->sin.sin_addr, ip, len);
    }
    else if (addr->sa.sa_family == AF_INET6) {
        return inet_ntop(AF_INET6, &addr->sin6.sin6_addr, ip, len);
    }
    return ip;
}

uint16_t sockaddrPort(sockaddr_u* addr) {
    uint16_t port = 0;
    if (addr->sa.sa_family == AF_INET) {
        port = ntohs(addr->sin.sin_port);
    }
    else if (addr->sa.sa_family == AF_INET6) {
        port = ntohs(addr->sin6.sin6_port);
    }
    return port;
}

int sockaddrSetIp(sockaddr_u* addr, const char* host) {
    if (!host || *host == '\0') {
        addr->sin.sin_family = AF_INET;
        addr->sin.sin_addr.s_addr = htonl(INADDR_ANY);
        return 0;
    }
    return resolveAddr(host, addr);
}

void sockaddrSetPort(sockaddr_u* addr, int port) {
    if (addr->sa.sa_family == AF_INET) {
        addr->sin.sin_port = htons(port);
    }
    else if (addr->sa.sa_family == AF_INET6) {
        addr->sin6.sin6_port = htons(port);
    }
}

int sockaddrSetIpPort(sockaddr_u* addr, const char* host, int port) {
#ifdef ENABLE_UDS
    if (port < 0) {
        sockaddr_set_path(addr, host);
        return 0;
    }
#endif
    int ret = sockaddrSetIp(addr, host);
    if (ret != 0) return ret;
    sockaddrSetPort(addr, port);
    // SOCKADDR_PRINT(addr);
    return 0;
}

socklen_t sockaddrLen(sockaddr_u* addr) {
    if (addr->sa.sa_family == AF_INET) {
        return sizeof(struct sockaddr_in);
    }
    else if (addr->sa.sa_family == AF_INET6) {
        return sizeof(struct sockaddr_in6);
    }
#ifdef ENABLE_UDS
    else if (addr->sa.sa_family == AF_UNIX) {
        return sizeof(struct sockaddr_un);
    }
#endif
    return sizeof(sockaddr_u);
}

const char* sockaddrStr(sockaddr_u* addr, char* buf, int len) {
    char ip[SOCKADDR_STRLEN] = {0};
    uint16_t port = 0;
    if (addr->sa.sa_family == AF_INET) {
        inet_ntop(AF_INET, &addr->sin.sin_addr, ip, len);
        port = ntohs(addr->sin.sin_port);
        snprintf(buf, len, "%s:%d", ip, port);
    }
    else if (addr->sa.sa_family == AF_INET6) {
        inet_ntop(AF_INET6, &addr->sin6.sin6_addr, ip, len);
        port = ntohs(addr->sin6.sin6_port);
        snprintf(buf, len, "[%s]:%d", ip, port);
    }
#ifdef ENABLE_UDS
    else if (addr->sa.sa_family == AF_UNIX) {
        snprintf(buf, len, "%s", addr->sun.sun_path);
    }
#endif
    return buf;
}

static int sockaddrBind(sockaddr_u* localaddr, int type) {
    // socket -> setsockopt -> bind
#ifdef SOCK_CLOEXEC
    type |= SOCK_CLOEXEC;
#endif
    int sockfd = socket(localaddr->sa.sa_family, type, 0);
    if (sockfd < 0) {
        printError("socket");
        goto error;
    }

#ifdef OS_UNIX
    so_reuseaddr(sockfd, 1);
    // so_reuseport(sockfd, 1);
#endif

    if (localaddr->sa.sa_family == AF_INET6) {
        ipV6Only(sockfd, 0);
    }

    if (bind(sockfd, &localaddr->sa, sockaddrLen(localaddr)) < 0) {
        printError("bind");
        goto error;
    }

    return sockfd;
error:
    return socketErrnoNegative(sockfd);
}

static int sockaddrConnect(sockaddr_u* peeraddr, int nonblock) {
    // socket -> nonblocking -> connect
    int ret = 0;
    int connfd = socket(peeraddr->sa.sa_family, SOCK_STREAM, 0);
    if (connfd < 0) {
        printError("socket");
        goto error;
    }

    if (nonblock) {
        nonBlocking(connfd);
    }

    ret = connect(connfd, &peeraddr->sa, sockaddrLen(peeraddr));
#ifdef OS_WIN
    if (ret < 0 && socketERRNO() != WSAEWOULDBLOCK) {
#else
    if (ret < 0 && socket_errno() != EINPROGRESS) {
#endif
        // printError("connect");
        goto error;
    }

    return connfd;
error:
    return socketErrnoNegative(connfd);
}

static int ListenFD(int sockfd) {
    if (sockfd < 0) return sockfd;
    if (listen(sockfd, SOMAXCONN) < 0) {
        printError("listen");
        return socketErrnoNegative(sockfd);
    }
    return sockfd;
}

static int ConnectFDTimeout(int connfd, int ms) {
    int err = 0;
    socklen_t optlen = sizeof(err);
    struct timeval tv = {ms / 1000, (ms % 1000) * 1000};
    fd_set writefds;
    FD_ZERO(&writefds);
#if defined(OS_UNIX)
    FD_SET(connfd, &writefds);
#else
    FD_SET((unsigned long long)connfd, &writefds);
#endif
    int ret = select(connfd + 1, 0, &writefds, 0, &tv);
    if (ret < 0) {
        printError("select");
        goto error;
    }
    if (ret == 0) {
        errno = ETIMEDOUT;
        goto error;
    }
    if (getsockopt(connfd, SOL_SOCKET, SO_ERROR, (char*)&err, &optlen) < 0 || err != 0) {
        if (err != 0) errno = err;
        goto error;
    }
    blocking(connfd);
    return connfd;
error:
    return socketErrnoNegative(connfd);
}

int Bind(int port, const char* host, int type) {
#ifdef OS_WIN
    WSAInit();
#endif
    sockaddr_u localaddr;
    memorySet(&localaddr, 0, sizeof(localaddr));
    int ret = sockaddrSetIpPort(&localaddr, host, port);
    if (ret != 0) {
        return NABS(ret);
    }
    return sockaddrBind(&localaddr, type);
}

int wwListen(int port, const char* host) {
    int sockfd = Bind(port, host, SOCK_STREAM);
    if (sockfd < 0) return sockfd;
    return ListenFD(sockfd);
}

int Connect(const char* host, int port, int nonblock) {
#ifdef OS_WIN
    WSAInit();
#endif
    sockaddr_u peeraddr;
    memorySet(&peeraddr, 0, sizeof(peeraddr));
    int ret = sockaddrSetIpPort(&peeraddr, host, port);
    if (ret != 0) {
        return NABS(ret);
    }
    return sockaddrConnect(&peeraddr, nonblock);
}

int ConnectNonblock(const char* host, int port) {
    return Connect(host, port, 1);
}

int ConnectTimeout(const char* host, int port, int ms) {
    int connfd = Connect(host, port, 1);
    if (connfd < 0) return connfd;
    return ConnectFDTimeout(connfd, ms);
}

#ifdef ENABLE_UDS
int BindUnix(const char* path, int type) {
    sockaddr_u localaddr;
    memorySet(&localaddr, 0, sizeof(localaddr));
    sockaddr_set_path(&localaddr, path);
    return sockaddrBind(&localaddr, type);
}

int wwListenUnix(const char* path) {
    int sockfd = BindUnix(path, SOCK_STREAM);
    if (sockfd < 0) return sockfd;
    return ListenFD(sockfd);
}

int ConnectUnix(const char* path, int nonblock) {
    sockaddr_u peeraddr;
    memorySet(&peeraddr, 0, sizeof(peeraddr));
    sockaddr_set_path(&peeraddr, path);
    return sockaddrConnect(&peeraddr, nonblock);
}

int ConnectUnixNonblock(const char* path) {
    return ConnectUnix(path, 1);
}

int ConnectUnixTimeout(const char* path, int ms) {
    int connfd = ConnectUnix(path, 1);
    if (connfd < 0) return connfd;
    return ConnectFDTimeout(connfd, ms);
}
#endif

int createSocketPair(int family, int type, int protocol, int sv[2]) {
#if defined(OS_UNIX) && HAVE_SOCKETPAIR
    return socketpair(AF_LOCAL, type, protocol, sv);
#endif
    if (family != AF_INET || type != SOCK_STREAM) {
        return -1;
    }
#ifdef OS_WIN
    (void)protocol;
    WSAInit();
#endif
    int listenfd, connfd, acceptfd;
    listenfd = connfd = acceptfd = -1;
    struct sockaddr_in localaddr;
    socklen_t addrlen = sizeof(localaddr);
    memorySet(&localaddr, 0, addrlen);
    localaddr.sin_family = AF_INET;
    localaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    localaddr.sin_port = 0;
    // listener
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        printError("socket");
        goto error;
    }
    if (bind(listenfd, (struct sockaddr*)&localaddr, addrlen) < 0) {
        printError("bind");
        goto error;
    }
    if (listen(listenfd, 1) < 0) {
        printError("listen");
        goto error;
    }
    if (getsockname(listenfd, (struct sockaddr*)&localaddr, &addrlen) < 0) {
        printError("getsockname");
        goto error;
    }
    // connector
    connfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connfd < 0) {
        printError("socket");
        goto error;
    }
    if (connect(connfd, (struct sockaddr*)&localaddr, addrlen) < 0) {
        printError("connect");
        goto error;
    }
    // acceptor
    acceptfd = accept(listenfd, (struct sockaddr*)&localaddr, &addrlen);
    if (acceptfd < 0) {
        printError("accept");
        goto error;
    }

    closesocket(listenfd);
    sv[0] = connfd;
    sv[1] = acceptfd;
    return 0;
error:
    if (listenfd != -1) {
        closesocket(listenfd);
    }
    if (connfd != -1) {
        closesocket(connfd);
    }
    if (acceptfd != -1) {
        closesocket(acceptfd);
    }
    return -1;
}
