#pragma once

// ------------------------------------------------------------------------------------------------
// UnityPlugin.h
//
// Code for handling basics of a Unity plugin.
//
// Platforms:
//	UNITY_WIN
//	UNITY_METRO
//	UNITY_ANDROID
//	UNITY_IPHONE
//	UNITY_OSX
//	UNITY_LINUX
//
// Graphics APIs:
//	SUPPORT_D3D9
//	SUPPORT_D3D11
//	SUPPORT_OPENGL_UNIFIED
//	SUPPORT_OPENGL_ES
//	SUPPORT_OPENGL_CORE
//
// Exported DLL functions:
//	UnityPluginLoad
//	UnityPluginUnload
//	UnityGraphicsDeviceEvent

// Which platform we are on?
#if _MSC_VER
#define UNITY_WIN 1
#elif defined(__APPLE__)
#if defined(__arm__)
#define UNITY_IPHONE 1
#else
#define UNITY_OSX 1
#endif
#elif defined(UNITY_METRO) || defined(UNITY_ANDROID) || defined(UNITY_LINUX)
// these are defined externally
#else
#error "Unknown platform!"
#endif

// Which graphics device APIs we possibly support?
#if UNITY_WIN
#define SUPPORT_D3D9			1
#define SUPPORT_D3D11			1
#define SUPPORT_OPENGL_UNIFIED	1
#define SUPPORT_OPENGL_CORE		1
#elif UNITY_IPHONE || UNITY_ANDROID
#define SUPPORT_OPENGL_UNIFIED	1
#define SUPPORT_OPENGL_ES		1
#elif UNITY_OSX || UNITY_LINUX
#define SUPPORT_OPENGL_UNIFIED	1
#define SUPPORT_OPENGL_CORE		1
#endif

// Include Unity interfaces and graphics interfaces for supported APIs
#include "Unity/IUnityInterface.h"
#include "Unity/IUnityGraphics.h"
#include "GraphicsDefs.h"

namespace FPVR
{
	// Singleton Unity plugin-class
	class UnityPlugin
	{
	protected:
		static IUnityInterfaces* mUnityInterfaces;		// Unity interfaces passed to load plugin
		static IUnityGraphics* mUnityGraphics;			// Graphics interfaces
		static UnityGfxRenderer mUnityDeviceType;		// Current Unity device type

#if SUPPORT_D3D9
		static IDirect3DDevice9* mD3D9Device;
#endif
#if SUPPORT_D3D11
		static ID3D11Device* mD3D11Device;
		static ID3D11DeviceContext* mD3D11Context;
#endif

	public:
		static IUnityInterfaces*  UnityInterfaces() { return mUnityInterfaces; }
		static UnityGfxRenderer UnityDeviceType() { return mUnityDeviceType; }
		static IUnityGraphics* UnityGraphics() { return mUnityGraphics; }

#if SUPPORT_D3D9
		static IDirect3DDevice9* D3D9Device() { return mD3D9Device; }
#endif
#if SUPPORT_D3D11
		static ID3D11Device* D3D11Device() { return mD3D11Device; }
		static ID3D11DeviceContext* D3D11Context() { return mD3D11Context; }
#endif

		static void Load(IUnityInterfaces* unityInterfaces);
		static void Unload();
		static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);
	};
}

// End of UnityPlugin.h
