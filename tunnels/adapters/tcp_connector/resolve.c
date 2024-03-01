#include "shared.h"
#include "loggers/dns_logger.h"

bool tcpConnectorResolvedomain(socket_context_t *dest)
{
    uint16_t old_port = sockaddr_port(&(dest->addr));
    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);
    /* resolve domain */
    {
        if (sockaddr_set_ipport(&(dest->addr), dest->domain, old_port) != 0)
        {
            LOGE("Connector: resolve failed  %s", dest->domain);
            return false;
        }
        else
        {
            char ip[60];
            sockaddr_str(&(dest->addr), ip, 60);
            LOGI("Connector: %s resolved to %s", dest->domain, ip);
        }
    }
    gettimeofday(&tv2, NULL);

    double time_spent = (double)(tv2.tv_usec - tv1.tv_usec) / 1000000 + (double)(tv2.tv_sec - tv1.tv_sec);
    LOGD("Connector: dns resolve took %lf sec", time_spent);
    dest->resolved = true;

    return true;
}
