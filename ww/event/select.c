#include "iowatcher.h"

#ifdef EVENT_SELECT
#include "wplatform.h"
#include "wdef.h"
#include "wevent.h"
#include "wsocket.h"

typedef struct select_ctx_s {
    int max_fd;
    fd_set readfds;
    fd_set writefds;
    int nread;
    int nwrite;
} select_ctx_t;

int iowatcherInit(wloop_t* loop) {
    if (loop->iowatcher) return 0;
    select_ctx_t* select_ctx;
    EVENTLOOP_ALLOC_SIZEOF(select_ctx);
    select_ctx->max_fd = -1;
    FD_ZERO(&select_ctx->readfds);
    FD_ZERO(&select_ctx->writefds);
    select_ctx->nread = 0;
    select_ctx->nwrite = 0;
    loop->iowatcher = select_ctx;
    return 0;
}

int iowatcherCleanUp(wloop_t* loop) {
    EVENTLOOP_FREE(loop->iowatcher);
    return 0;
}

int iowatcherAddEvent(wloop_t* loop, int fd, int events) {
    if (loop->iowatcher == NULL) {
        iowatcherInit(loop);
    }
    select_ctx_t* select_ctx = (select_ctx_t*)loop->iowatcher;
    if (fd > select_ctx->max_fd) {
        select_ctx->max_fd = fd;
    }
    if (events & WW_READ) {
        if (!FD_ISSET(fd, &select_ctx->readfds)) {
            FD_SET(fd, &select_ctx->readfds);
            select_ctx->nread++;
        }
    }
    if (events & WW_WRITE) {
        if (!FD_ISSET(fd, &select_ctx->writefds)) {
            FD_SET(fd, &select_ctx->writefds);
            select_ctx->nwrite++;
        }
    }
    return 0;
}

int iowatcherDelEvent(wloop_t* loop, int fd, int events) {
    select_ctx_t* select_ctx = (select_ctx_t*)loop->iowatcher;
    if (select_ctx == NULL)    return 0;
    if (fd == select_ctx->max_fd) {
        select_ctx->max_fd = -1;
    }
    if (events & WW_READ) {
        if (FD_ISSET(fd, &select_ctx->readfds)) {
            FD_CLR(fd, &select_ctx->readfds);
            select_ctx->nread--;
        }
    }
    if (events & WW_WRITE) {
        if (FD_ISSET(fd, &select_ctx->writefds)) {
            FD_CLR(fd, &select_ctx->writefds);
            select_ctx->nwrite--;
        }
    }
    return 0;
}

static int find_max_active_fd(wloop_t* loop) {
    wio_t* io = NULL;
    for (int i = loop->ios.maxsize-1; i >= 0; --i) {
        io = loop->ios.ptr[i];
        if (io && io->active && io->events) return i;
    }
    return -1;
}

static int remove_bad_fds(wloop_t* loop) {
    select_ctx_t* select_ctx = (select_ctx_t*)loop->iowatcher;
    if (select_ctx == NULL)    return 0;
    int badfds = 0;
    int error = 0;
    socklen_t optlen = sizeof(error);
    for (int fd = 0; fd <= select_ctx->max_fd; ++fd) {
        if (FD_ISSET(fd, &select_ctx->readfds) ||
            FD_ISSET(fd, &select_ctx->writefds)) {
            error = 0;
            optlen = sizeof(int);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&error, &optlen) < 0 || error != 0) {
                ++badfds;
                wio_t* io = loop->ios.ptr[fd];
                if (io) {
                    wioDel(io, WW_RDWR);
                }
            }
        }
    }
    return badfds;
}

int iowatcherPollEvents(wloop_t* loop, int timeout) {
    select_ctx_t* select_ctx = (select_ctx_t*)loop->iowatcher;
    if (select_ctx == NULL)    return 0;
    if (select_ctx->nread == 0 && select_ctx->nwrite == 0) {
        return 0;
    }
    int     max_fd = select_ctx->max_fd;
    fd_set  readfds = select_ctx->readfds;
    fd_set  writefds = select_ctx->writefds;
    if (max_fd == -1) {
        select_ctx->max_fd = max_fd = find_max_active_fd(loop);
    }
    struct timeval tv, *tp;
    if (timeout == (int)INFINITE) {
        tp = NULL;
    }
    else {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        tp = &tv;
    }
    int nselect = select(max_fd+1, &readfds, &writefds, NULL, tp);
    if (nselect < 0) {
#ifdef OS_WIN
        if (WSAGetLastError() == WSAENOTSOCK) {
#else
        if (errno == EBADF) {
            printError("select");
#endif
            remove_bad_fds(loop);
            return -EBADF;
        }
        return nselect;
    }
    if (nselect == 0)   return 0;
    int nevents = 0;
    int revents = 0;
    for (int fd = 0; fd <= max_fd; ++fd) {
        revents = 0;
        if (FD_ISSET(fd, &readfds)) {
            ++nevents;
            revents |= WW_READ;
        }
        if (FD_ISSET(fd, &writefds)) {
            ++nevents;
            revents |= WW_WRITE;
        }
        if (revents) {
            wio_t* io = loop->ios.ptr[fd];
            if (io) {
                io->revents = revents;
                EVENT_PENDING(io);
            }
        }
        if (nevents == nselect) break;
    }
    return nevents;
}
#endif
