#include "structure.h"

#include "loggers/network_logger.h"

void tcpconnectorTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    tcpconnector_lstate_t *lstate = lineGetState(l, t);

    if (lstate->write_paused)
    {
        tunnelPrevDownStreamPause(t, l);
        bufferqueuePush(&lstate->pause_queue, buf);
    }
    else
    {
        int bytes  = (int) sbufGetLength(buf);
        int nwrite = wioWrite(lstate->io, buf);

        wioSetReadTimeout(lstate->io, kReadWriteTimeoutMs); // resets the read timeout
        if (nwrite >= 0 && nwrite < bytes)
        {
            lstate->write_paused = true;
            wioSetCallBackWrite(lstate->io, tcpconnectorOnWriteComplete);
            tunnelPrevDownStreamPause(t, l);
        }
    }
}
