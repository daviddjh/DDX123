#pragma once

#ifdef  _DEBUG
#define _CRTDBG_MAP_ALLOC //to get more details
#endif

#include "d_include.h"
#include <chrono>

// TODO: Re-Evaluate
#include <string>
#include <vector>
#include <map>
#include <stdio.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include "windowsx.h"
#include <wrl/client.h>

#include <d3d12.h>
#include "dxgi1_6.h"
#include <DirectXMath.h>
#include <d3dcompiler.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#ifdef _DEBUG
#define DEBUG_LOG(x) os_debug_print(__FILE__ ":" stringerize(__LINE__) " - Log: " x "\n")
#define DEBUG_ERROR(x) os_debug_print(__FILE__ ":" stringerize(__LINE__) " - ERROR: " x "\n"); DEBUG_BREAK
#else 
#define DEBUG_LOG(x)
#define DEBUG_ERROR(x)
#endif

#ifndef DEBUG_BREAK
#define DEBUG_BREAK
#endif

#ifdef _DEBUG
inline void ThrowIfFailed(HRESULT result){
	if(FAILED(result)){
		DEBUG_BREAK;
		throw std::exception();
	}
}
#else
inline void ThrowIfFailed(HRESULT result){}
#endif

