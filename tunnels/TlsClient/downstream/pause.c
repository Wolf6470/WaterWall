#include "structure.h"

#include "loggers/network_logger.h"

void tlsclientTunnelDownStreamPause(tunnel_t *t, line_t *l)
{
    tunnelPrevDownStreamPause(t, l);
}
