#include "structure.h"

#include "loggers/network_logger.h"

void rawsocketDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
   rawsocketWriteStreamPayload(t, l, buf);
}
