/*
*   Provides context macros to identify what compiler / os is being used
*/

/////////////////
// COMPILER
/////////////////

#ifdef _MSC_VER
#define COMPILER_MSVC 1
#endif

#ifdef __clang__
#define COMPILER_CLANG 1
#endif

#ifdef __GNUC__
#define COMPILER_GCC 1
#endif

/////////////////
// OS
/////////////////

#ifdef _WIN32
#define OS_WINDOWS 1
#endif

#ifdef __linux__
#define OS_LINUX 1
#endif

#ifdef __APPLE__
#define OS_APPLE 1
#endif

#ifdef __ANDROID__
#define OS_ANDROID 1
#endif

/////////////////
// ARCH
/////////////////

#if defined __i386__ || defined _M_IX86
#define ARCH_x86 1
#endif

#if defined __amd64__ || defined _M_AMD64
#define ARCH_x64 1
#endif

#if defined __arm__ || defined _M_ARM
#define ARCH_ARM 1
#endif

#ifdef __aarch64__
#define ARCH_ARM64 1
#endif

/*
*   Defines a debug break
*/

#ifndef DEBUG_BREAK

#if 0
#ifdef OS_WINDOWS
#define DEBUG_BREAK if (IsDebuggerPresent() __debugbreak())
#endif

#ifdef OS_LINUX
#define DEBUG_BREAK if (IsDebuggerPresent() __raise(SIGTRAP))
#endif

#ifdef OS_APPLE
#define DEBUG_BREAK if (IsDebuggerPresent() asm {int 3} )
#endif
#endif

#define DEBUG_BREAK asm {int 3}

#endif // DEBUG_BREAK

#if USING_ASSERT
#define ASSERT(s) do(if(!s){DEBUG_BREAK})while(0)
#else
#define ASSERT(s) s
#endif // USING_ASSERT

/*
*   Helper Macros
*/

// ...

/*
*   Types
*/

typedef           __int8  s8;
typedef  unsigned __int8  u8;
typedef           __int16 s16;
typedef  unsigned __int16 u16;
typedef           __int32 s32;
typedef  unsigned __int32 u32;
typedef           __int64 s64;
typedef  unsigned __int64 u64;

typedef float  f32;
typedef double f64;

#define _1KB   1 * 1024
#define _2KB   2 * 1024
#define _4KB   4 * 1024
#define _8KB   8 * 1024
#define _16KB 16 * 1024
#define _32KB 32 * 1024
#define _64KB 64 * 1024

#define _1MB   1 * 1024 * 1024
#define _2MB   2 * 1024 * 1024
#define _4MB   4 * 1024 * 1024
#define _8MB   8 * 1024 * 1024
#define _16MB 16 * 1024 * 1024
#define _32MB 32 * 1024 * 1024
#define _64MB 64 * 1024 * 1024

