#pragma once

#include <list>

// --------------------------------------------------------------------------
// Helper utilities

namespace FPVR
{
	extern void DebugLog(const char* str, ...);
	extern void DebugLogS(const char* str);
	extern void DebugLogV(const char* str, va_list args);

	// Enumeration of supported texture formats
	typedef enum
	{
		TEXFMT_UNKNOWN = -1,
		TEXFMT_RGB24 = 0,
		TEXFMT_RGBA32 = 1,
		TEXFMT_ARGB32 = 2,
		TEXFMT_RGB565 = 3,
	} eTexFmt;

	// Returns internal texture format enum equivalent to Unity TextureFormat
	extern eTexFmt GetTexFmtFromUnity(int format);

	// Returns the bits per pixel of texture format
	extern int GetTexFmtBPP(eTexFmt texFmt);

	// Returns the FourCC code equivalent to texture format
	extern const char* GetTexFmtFourCC(eTexFmt texFmt);

	// Returns the Unity TextureFormat enum equivalent to texture format
	extern void GetTexFmtUnity(eTexFmt texFmt, int& format);

#if SUPPORT_D3D9
	// Returns the DX9 D3DFORMAT Unity enum equivalent to texture format
	extern void GetTexFmtD3D9(eTexFmt texFmt, D3DFORMAT& format);
#endif

#if SUPPORT_D3D11
	// Returns the DX11 DXGI_FORMAT_ enum equivalent to texture format
	extern void GetTexFmtD3D11(eTexFmt texFmt, DXGI_FORMAT& format);
#endif

#if SUPPORT_OPENGL_UNIFIED
	// Returns the GL_ data and format equivalent to texture format
	extern void GetTexFmtGL(eTexFmt texFmt, int& format, int& type);
#endif

	extern void DumpTextureDesc(void* texture, int unityFmt = 0);
	extern void GetTextureDesc(void* texture, int& width, int& height, int& format);
}

