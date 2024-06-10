#ifndef _D_CONTEXT
#define _D_CONTEXT

/*
*   Provides context macros to identify what compiler / os is being used
*/

// Define debug... seems like its always one or the other
#ifdef _DEBUG
#ifndef DEBUG
#define DEBUG
#endif
#endif

#ifdef DEBUG
#ifndef _DEBUG
#define _DEBUG
#endif
#endif

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
#ifdef _WIN64
#define OS_WINDOWS_64 1
#endif
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

#endif // _D_CONTEXT