// --------------------------------------------------------------------------
// UnityPlugin

#include "UnityPlugin.h"
#include "PluginUtils.h"
#include "LibVLCWrapper.h"

// --------------------------------------------------------------------------
// UnitySetInterfaces

// Unity plugin load
extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	FPVR::UnityPlugin::Load(unityInterfaces);
}

// Unity plugin unload
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	FPVR::UnityPlugin::Unload();
}

namespace FPVR
{
	// ------------------------------------------------------------------------------------------------
	// UnityPlugin: Static member variables

	IUnityInterfaces* UnityPlugin::mUnityInterfaces = nullptr;				// Unity interfaces passed to load plugin
	UnityGfxRenderer UnityPlugin::mUnityDeviceType = kUnityGfxRendererNull;	// Current Unity device type
	IUnityGraphics* UnityPlugin::mUnityGraphics = nullptr;					// Graphics interfaces

#if SUPPORT_D3D9
	IDirect3DDevice9* UnityPlugin::mD3D9Device = nullptr;
#endif
#if SUPPORT_D3D11
	ID3D11Device* UnityPlugin::mD3D11Device = nullptr;
	ID3D11DeviceContext* UnityPlugin::mD3D11Context = nullptr;
#endif
#if SUPPORT_D3D12
#endif

	// ------------------------------------------------------------------------------------------------
	// UnityPlugin: Static members

	// Called on load of the plugin
	void UnityPlugin::Load(IUnityInterfaces* unityInterfaces)
	{
		DebugLog("UnityPlugin::Load(unityInterfaces=%08x)", unityInterfaces);

		mUnityInterfaces = unityInterfaces;
		mUnityGraphics = mUnityInterfaces->Get<IUnityGraphics>();
		mUnityGraphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

		// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
		OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
	}

	// Called when the plugin is unloaded
	void UnityPlugin::Unload()
	{
		DebugLog("UnityPlugin::Unload");
		mUnityGraphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
	}

	// Called when a graphics event is called
	void UNITY_INTERFACE_API UnityPlugin::OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
	{
		UnityGfxRenderer currentDeviceType = mUnityDeviceType;

		switch (eventType)
		{
		case kUnityGfxDeviceEventInitialize:	// Plugin should initialise allocate any static resources
			DebugLog("OnGraphicsDeviceEvent(Initialize)");
			mUnityDeviceType = mUnityGraphics->GetRenderer();
			currentDeviceType = mUnityDeviceType;

#if SUPPORT_D3D9
			if(currentDeviceType == kUnityGfxRendererD3D9)
				mD3D9Device = UnityPlugin::UnityInterfaces()->Get<IUnityGraphicsD3D9>()->GetDevice();
#endif
#if SUPPORT_D3D11
			if (currentDeviceType == kUnityGfxRendererD3D11)
			{
				mD3D11Device = UnityPlugin::UnityInterfaces()->Get<IUnityGraphicsD3D11>()->GetDevice();
				mD3D11Device->GetImmediateContext(&mD3D11Context);
			}
#endif
			break;

		case kUnityGfxDeviceEventBeforeReset:	// Plugin should free direct X resources ready for a reset (D3D9)
			DebugLog("OnGraphicsDeviceEvent(BeforeReset)");
			break;

		case kUnityGfxDeviceEventAfterReset:	// Plugin should recreate any resources following a reset (D3D9)
			DebugLog("OnGraphicsDeviceEvent(AfterReset)");
			break;

		case kUnityGfxDeviceEventShutdown:		// Plugin should shutdown, freeing all resources
			DebugLog("OnGraphicsDeviceEvent(Shutdown)");
			mUnityDeviceType = kUnityGfxRendererNull;
#if SUPPORT_D3D9
			mD3D9Device = nullptr;
#endif
#if SUPPORT_D3D11
			SAFE_RELEASE(mD3D11Context);
			mD3D11Device = nullptr;
#endif
			break;

		default:
			DebugLog("OnGraphicsDeviceEvent(Unknown)");
			break;
		};

		switch (currentDeviceType)
		{
		case kUnityGfxRendererD3D9:
#if !SUPPORT_D3D9
			DebugLog("UnityPlugin::OnGraphicsDeviceEvent(D3D9): Unsupported graphics renderer");
#endif
			break;
		case kUnityGfxRendererD3D11:
#if !SUPPORT_D3D11
			DebugLog("UnityPlugin::OnGraphicsDeviceEvent(D3D11): Unsupported graphics renderer");
#endif
			break;
		case kUnityGfxRendererD3D12:
#if !SUPPORT_D3D12
			DebugLog("UnityPlugin::OnGraphicsDeviceEvent(D3D12): Unsupported graphics renderer");
#endif
			break;
		case kUnityGfxRendererOpenGLES20:
		case kUnityGfxRendererOpenGLES30:
		case kUnityGfxRendererOpenGLCore:
#if !SUPPORT_OPENGL_UNIFIED
			DebugLog("UnityPlugin::OnGraphicsDeviceEvent(GL: %d): Unsupported graphics renderer", currentDeviceType);
#endif
			break;
		case kUnityGfxRendererMetal:
		case kUnityGfxRendererXenon:
		case kUnityGfxRendererGCM:
		case kUnityGfxRendererGXM:
		case kUnityGfxRendererXboxOne:
		case kUnityGfxRendererPS4:
		case kUnityGfxRendererOpenGL:		// Legacy - probably never support
		default:
			DebugLog("UnityPlugin::OnGraphicsDeviceEvent(%d): Unsupported graphics renderer", currentDeviceType);
			break;
		case kUnityGfxRendererNull:			// For some reason Shutdown gets called twice
			break;
		}

	}
}