#include "structure.h"

#include "loggers/network_logger.h"

void bridgeTunnelUpStreamResume(tunnel_t *t, line_t *l)
{
    bridge_tstate_t *state = tunnelGetState(t);

    tunnelPrevDownStreamResume(state->pair_tun, l);
}
