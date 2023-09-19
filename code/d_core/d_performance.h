#ifndef _D_PERF
#define _D_PERF

#include "d_context.h"

#ifdef OS_WINDOWS

#define PERFORMANCEAPI_ENABLED 1
#include "../../third_party/Superluminal/include/Superluminal/PerformanceAPI.h"

#define PROFILED_SCOPE(ID) PERFORMANCEAPI_INSTRUMENT(ID)
#define PROFILED_FUNCTION() PERFORMANCEAPI_INSTRUMENT(__FUNCTION__)

#else // ifdef OS_WINDOWS

#define PROFILED_SCOPE(...)
#define PROFILED_FUNCTION()

#endif

#endif // ifndef _D_PERF