//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>
#include <windowsx.h>

#include <cassert>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "../Common/MathTypes.h"
#include "d3dx12.h"   // Agility SDK helpers: ThirdParty/D3D12Agility/include/d3dx12

// WinPixEventRuntime is optional (its NuGet package under src/packages is not in the repo).
// Without it, PIX events compile to no-ops so a fresh clone still builds.
#if MYENGINE_WITH_WINPIX
#include <pix3.h>
#else
#define PIXBeginEvent(...) ((void)0)
#define PIXEndEvent(...) ((void)0)
#endif

#include <string>
#include <vector>
#include <wrl.h>
#include <shellapi.h>
#include <memory.h>

#define uint64 unsigned long long
