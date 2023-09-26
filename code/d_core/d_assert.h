#ifndef _D_ASSERT
#define _D_ASSERT

/*
*   Defines a debug break
*/

#ifndef DEBUG_BREAK

#ifdef DEBUG
#ifdef OS_WINDOWS
#define DEBUG_BREAK if (IsDebuggerPresent()) __debugbreak()
#endif

#ifdef OS_LINUX
#define DEBUG_BREAK if (IsDebuggerPresent() __raise(SIGTRAP))
#endif

#ifdef OS_APPLE
#define DEBUG_BREAK if (IsDebuggerPresent() asm {int 3} )
#endif

#else

#define DEBUG_BREAK

#endif

#endif // DEBUG_BREAK

#if defined DEBUG
#define ASSERT(s) do(if(!(s)){DEBUG_BREAK})while(0)
#else
#define ASSERT(s)
#endif // USING_ASSERT

#endif // _D_ASSERT