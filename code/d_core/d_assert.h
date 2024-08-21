#ifndef _D_ASSERT
#define _D_ASSERT
#include "d_os.h"

/*
*   Defines a debug break
*/

#ifndef DEBUG_BREAK

#ifdef DEBUG
#ifdef OS_WINDOWS
#define DEBUG_BREAK if (d_std::is_debugger_present()) __debugbreak()
#endif

#ifdef OS_LINUX
#define DEBUG_BREAK if (d_std::is_debugger_present()) __raise(SIGTRAP))
#endif

#ifdef OS_APPLE
#define DEBUG_BREAK if (d_std::is_debugger_present() asm {int 3} )
#endif

#else

#define DEBUG_BREAK

#endif

#endif // DEBUG_BREAK

#if defined DEBUG
#define ASSERT(s) do{if(!(s)){DEBUG_BREAK;}}while(0)
#define ASSERT_LOG(s, l) do{if(!(s)){os_debug_print(l); DEBUG_BREAK;}}while(0)
#else
#define ASSERT(s)
#define ASSERT_LOG(s,l)
#endif // USING_ASSERT

#endif // _D_ASSERT