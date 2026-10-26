#pragma once

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdlib.h>
#include <assert.h>
#include <chrono>
#include <memory>

#include <d3d11.h>
#include <dxgi.h>
#include <d3d11_4.h>
#include <dxgi1_4.h>

#include <CommonDefines.h>

#pragma comment( lib, "user32" )
#pragma comment( lib, "d3d11.lib" )
#pragma comment( lib, "dxgi.lib" )
#pragma comment( lib, "d3dcompiler.lib" )
