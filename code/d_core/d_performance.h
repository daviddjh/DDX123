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

#ifndef PROFILED_SCOPE(ID)

#define PROFILED_SCOPE(...)
#define PROFILED_FUNCTION()

#endif // ifndef PROFILED_SCOPE

#endif // ifndef _D_PERF