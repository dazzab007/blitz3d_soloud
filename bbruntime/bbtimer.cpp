#include "std.h"
#include "bbsys.h"
#include "../gxruntime/gxtimer.h"
#include "bb_handle.h"
#include "bb_handle_types.h"

BB_HANDLE(gxTimer) bbCreateTimer(int hertz)
{
    gxTimer* t = gx_runtime->createTimer(hertz);
    return BB_HANDLE(gxTimer)(t);
}

int bbWaitTimer(BB_HANDLE(gxTimer) t_h)
{
    gxTimer *t = t_h;
    int n = t->wait();
    if (!gx_runtime->idle())
        RTEX(0);
    return n;
}

void bbFreeTimer(BB_HANDLE(gxTimer) t_h)
{
    gxTimer *t = t_h;
#ifndef _WIN32
    g_handle_table.forget(t);
#endif
    gx_runtime->freeTimer(t);
}
