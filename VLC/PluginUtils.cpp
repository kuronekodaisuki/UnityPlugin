#include "UnityPlugin.h"
#include "PluginUtils.h"
#include "VLCMediaPlayer.h"	// TODO: Move the debug log stuff to the plugin utilities and make it global

#include <cstdio>

#include <stdarg.h>

namespace FPVR
{
	// Callback function
	typedef void(*DebugCallback)(const char* debugStr);

	// Pointer to user function to handle debug strings
	static DebugCallback gDebugCallback = nullptr;

	// Simple/single string debug log
	void DebugLogS(const char* str)
	{
		FILE* fp;
		if(fopen_s(&fp, "LibVLCWrapper.log.txt", "a+") == 0)
		{
			fprintf(fp, "%s\n", str);
			fclose(fp);
		}
/*		if (gDebugCallback != nullptr)
		{
			gDebugCallback(str);
		}
*/
	}

	// Printf style debug log
	void DebugLog(const char* str, ...)
	{
		va_list args;
		va_start(args, str);
		DebugLogV(str, args);
		va_end(args);
	}

	// VAArg debug log function
	void DebugLogV(const char* str, va_list args)
	{
		int len = _vscprintf(str, args);
		char* buf = (char*)alloca(len + 1);
		vsprintf_s(buf, len + 1, str, args);
		DebugLogS(buf);
	}

	// Set debug callback
	extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API FPVR_SetDebugCallback(DebugCallback cb)
	{
		gDebugCallback = cb;
	}

#if SUPPORT_D3D9
#define TF9(f)	,(f)
#else
#define TF9(f)
#endif
#if SUPPORT_D3D11
#define TF11(f)	,(f)
#else
#define TF_D3D11(f)
#endif
#if SUPPORT_OPENGL_UNIFIED
#define TFGL(f,t)	,(f),(t)
#else
#define TFGL(f)
#endif

	// --------------------------------------------------------------------------------------------
	// Texture Format Conversion / Information
	//
	// Helper functions to convert between different graphics API formats and
	// to get info about a particular texture format.
	typedef struct
	{
		int			mUnityTexFmt;	// Unity TextureFormat enum
		int			mBPP;			// Bits per pixel (>> 3 to get bytes per pixel)
		const char*	mFourCC;		// LibVLC FourCC code
#if SUPPORT_D3D9
		D3DFORMAT	mD3D9Format;	// D3D 9 format
#endif
#if SUPPORT_D3D11
		DXGI_FORMAT	mD3D11Format;	// D3D 11 format
#endif
#if SUPPORT_OPENGL_UNIFIED
		int			mGLFormat;		// OpenGL pixel format (GL_RGB or GL_RGBA)
		int			mGLType;		// OpenGL data type (GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT_5_6_5, GL_UNSIGNED_SHORT_4_4_4_4, and GL_UNSIGNED_SHORT_5_5_5_1)
#endif
	} sTexFmtInfo;

	// TODO: Fix this, it aint going to work (RV32, RGBA, ARGB, RV16)
	static const sTexFmtInfo gTexFmtInfo[] =
	{
		{ 2, 32, "RGBA" TF9(D3DFORMAT::D3DFMT_X8R8G8B8) TF11(DXGI_FORMAT_R8G8B8A8_UNORM) TFGL(GL_RGB,  GL_UNSIGNED_BYTE) },			// UnityEngine::TextureFormat.RGB24 = 2
		{ 3, 32, "RGBA" TF9(D3DFORMAT::D3DFMT_X8R8G8B8) TF11(DXGI_FORMAT_R8G8B8A8_UNORM) TFGL(GL_RGB,  GL_UNSIGNED_BYTE) },			// UnityEngine::TextureFormat.RGB24 = 3
		{ 4, 32, "RGBA" TF9(D3DFORMAT::D3DFMT_UNKNOWN)  TF11(DXGI_FORMAT_R8G8B8A8_UNORM) TFGL(GL_RGBA, GL_UNSIGNED_BYTE) },			// UnityEngine::TextureFormat.RGBA32 = 4
		{ 5, 32, "RGBA" TF9(D3DFORMAT::D3DFMT_A8R8G8B8) TF11(DXGI_FORMAT_R8G8B8A8_UNORM) TFGL(GL_RGBA, GL_UNSIGNED_BYTE) },			// UnityEngine::TextureFormat.ARGB32 = 5
		{ 7, 16, "RGBA" TF9(D3DFORMAT::D3DFMT_R5G6B5)   TF11(DXGI_FORMAT_R8G8B8A8_UNORM) TFGL(GL_RGB,  GL_UNSIGNED_SHORT_5_6_5) },	// UnityEngine::TextureFormat.RGB565 = 7
		{13, 16, "RGBA" TF9(D3DFORMAT::D3DFMT_R5G6B5)   TF11(DXGI_FORMAT_R8G8B8A8_UNORM) TFGL(GL_RGB,  GL_UNSIGNED_SHORT_5_6_5) },	// UnityEngine::TextureFormat.RGB565 = 13
		{14, 16, "BGRA" TF9(D3DFORMAT::D3DFMT_R5G6B5)   TF11(DXGI_FORMAT_B8G8R8A8_UNORM) TFGL(GL_RGB,  GL_UNSIGNED_SHORT_5_6_5) },	// UnityEngine::TextureFormat.RGB565 = 14
	};
	static const int kTexFmtInfoSize = (sizeof(gTexFmtInfo) / sizeof(sTexFmtInfo));

	// Returns internal texture format enum equivalent to Unity TextureFormat
	eTexFmt GetTexFmtFromUnity(int format)
	{
		for (int i = 0; i < kTexFmtInfoSize; i++)
		{
			if (gTexFmtInfo[i].mUnityTexFmt == format)
			{
				DebugLog("GetTexFmtFromUnity(unityTexFormat=%d) returns %d", format, i);
				return (eTexFmt)i;
			}
		}
		DebugLog("GetTexFmtFromUnity(unityTexFormat=%d) returns %d", format, TEXFMT_UNKNOWN);
		return TEXFMT_UNKNOWN;
	}

	// Returns the bits per pixel of texture format
	int GetTexFmtBPP(eTexFmt texFmt)
	{
		return gTexFmtInfo[(int)texFmt].mBPP;
	}

	// Returns the FourCC code equivalent to texture format
	const char* GetTexFmtFourCC(eTexFmt texFmt)
	{
		return gTexFmtInfo[texFmt].mFourCC;
	}

	// Returns the Unity TextureFormat enum equivalent to texture format
	void GetTexFmtUnity(eTexFmt texFmt, int& format)
	{
		format = gTexFmtInfo[texFmt].mUnityTexFmt;
	}

#if SUPPORT_D3D9
	// Returns the DX9 D3DFORMAT Unity enum equivalent to texture format
	void GetTexFmtD3D9(eTexFmt texFmt, D3DFORMAT& format)
	{
		format = gTexFmtInfo[texFmt].mD3D9Format;
	}
#endif

#if SUPPORT_D3D11
	// Returns the DX11 DXGI_FORMAT_ enum equivalent to texture format
	void GetTexFmtD3D11(eTexFmt texFmt, DXGI_FORMAT& format)
	{
		format = gTexFmtInfo[texFmt].mD3D11Format;
	}
#endif

#if SUPPORT_OPENGL_UNIFIED
	// Returns the GL_ data and format equivalent to texture format
	void GetTexFmtGL(eTexFmt texFmt, int& format, int& type)
	{
		format = gTexFmtInfo[texFmt].mGLFormat;
		type = gTexFmtInfo[texFmt].mGLType;
	}
#endif

	void DumpTextureDesc(void* texture, int unityFmt)
	{
		int width, height, format;
		GetTextureDesc(texture, width, height, format);
		DebugLog("Texture: %08x Width=%d Height=%d Format=%d UnityFmt:%d", texture, width, height, format, unityFmt);
	}

	void GetTextureDesc(void* texture, int& width, int& height, int& format)
	{
/*		D3DSURFACE_DESC desc;
		((IDirect3DTexture9*)texture)->GetLevelDesc(0, &desc);
		width = desc.Width;
		height = desc.Height;
		format = (int)desc.Format;
*/
		D3D11_TEXTURE2D_DESC desc;
		((ID3D11Texture2D*)texture)->GetDesc(&desc);
		width = desc.Width;
		height = desc.Height;
		format = (int)desc.Format;
/*
		GLuint gltex = (GLuint)(size_t)(texture);
		glBindTexture(GL_TEXTURE_2D, gltex);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
*/
	}
}