#ifndef _D_PERF
#define _D_PERF

#include "d_context.h"

#ifdef PCOUNTER
#ifdef OS_WINDOWS

#define PERFORMANCEAPI_ENABLED 1
#include "../../third_party/Superluminal/include/Superluminal/PerformanceAPI.h"

#define PROFILED_SCOPE(ID) PERFORMANCEAPI_INSTRUMENT(ID)
#define PROFILED_FUNCTION() PERFORMANCEAPI_INSTRUMENT(__FUNCTION__)

#endif // ifdef OS_WINDOWS
#endif // ifdef PCOUNTER

#ifndef PROFILED_SCOPE
    #define PROFILED_SCOPE(...)
#endif // ifndef PROFILED_SCOPE

#ifndef PROFILED_FUNCTION
    #define PROFILED_FUNCTION()
#endif // ifndef PROFILED_FUNCTION

#endif // ifndef _D_PERF