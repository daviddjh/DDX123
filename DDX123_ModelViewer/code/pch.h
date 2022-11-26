#ifdef  _DEBUG
#define _CRTDBG_MAP_ALLOC //to get more details
#endif

#include "d_include.h"
#include <chrono>

// TODO: Re-Evaluate
#include <string>
#include <vector>
#include <map>

#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include "windowsx.h"

#include <d3d12.h>
//#include "third_party/d3dx12.h"
#include "dxgi1_6.h"
#include <DirectXMath.h>
#include <d3dcompiler.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#if defined(min)
	#undef min
#endif

#if defined(max)
	#undef max
#endif

#if 0
#ifdef _DEBUG
inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        // Set a breakpoint on this line to catch DirectX API errors
        throw std::exception();
    }
}
#else 
inline void ThrowIfFailed(HRESULT hr)
{
}
#endif
#endif

// Debug Break
#ifdef _DEBUG

	#ifdef _MSC_VER
	#define DEBUG_BREAK __debugbreak()
	#endif

	#ifdef __INTEL_LLVM_COMPILER
	#define DEBUG_BREAK __debugbreak()
	#endif

#endif

#ifndef DEBUG_BREAK
#define DEBUG_BREAK
#endif

#ifdef _DEBUG
//#define ThrowIfFailed(result) if(FAILED(result)) { DEBUG_BREAK; throw std::exception(); }
inline void ThrowIfFailed(HRESULT result){
	if(FAILED(result)){
		DEBUG_BREAK;
		throw std::exception();
	}
}
#else
//#define ThrowIfFailed(hr) hr
inline void ThrowIfFailed(HRESULT result){}
#endif

