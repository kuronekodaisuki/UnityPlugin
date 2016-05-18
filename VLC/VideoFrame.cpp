#include <cassert>

#include "UnityPlugin.h"
#include "PluginUtils.h"
#include "VideoFrame.h"

// ---------------------------------------------------------------------------
// Video Frame Class

namespace FPVR
{
	// Clear video frame to 0 (only functional when frame is locked)
	void VideoFrame::Clear(int val)
	{
		if (IsLocked())
		{
			memset(mData, val, mRowPitch * mHeight);
		}
		//DebugLog("VideoFrame::Clear()");
	}

	// Maps memory for writing to the texture.
	void VideoFrame::Lock()
	{
		if (!IsLocked())
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
			UnityPlugin::D3D11Context()->Map((ID3D11Texture2D*)mTexture, 0, D3D11_MAP_WRITE, 0, &mappedResource);
			mData = mappedResource.pData;
			mRowPitch = mappedResource.RowPitch;
		}
		//DebugLog("VideoFrame::Lock(data=%08x, pitch=%d", data, pitch);
	}

	// Maps memory for writing to the texture.
	bool VideoFrame::TryLock()
	{
		if (!IsLocked())
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
			HRESULT hr = UnityPlugin::D3D11Context()->Map((ID3D11Texture2D*)mTexture, 0, D3D11_MAP_WRITE, D3D11_MAP_FLAG_DO_NOT_WAIT, &mappedResource);
			if (hr == S_OK)
			{
				mData = mappedResource.pData;
				mRowPitch = mappedResource.RowPitch;
			}
		}
		//DebugLog("VideoFrame::TryLock(data=%08x, pitch=%d", mData, mRowPitch);
		return IsLocked();
	}

	// Releases the memory
	void VideoFrame::Unlock()
	{
		if (IsLocked())
		{
			UnityPlugin::D3D11Context()->Unmap((ID3D11Texture2D*)mTexture, 0);
			mData = nullptr;
			mRowPitch = 0;
		}
		//DebugLog("VideoFrame::Unlock()");
	}

	// Copy this frame to specified target texture
	void VideoFrame::CopyTo(void* dstTex)
	{
		UnityPlugin::D3D11Context()->CopyResource((ID3D11Resource*)dstTex, (ID3D11Texture2D*)mTexture);
		//DebugLog("VideoFrame::CopyTo(dstTex=%08x)", dstTex);
	}

	// Create a video frame object with specified config
	VideoFrame* VideoFrame::Create(int width, int height, eTexFmt format)
	{
		VideoFrame* vf = new VideoFrame();
		if (vf->Initialize(width, height, format))
		{
			return vf;
		}
		else
		{
			DebugLog("VideoFrame::Create(width=%d, height=%d, format=%d) returns nullptr", width, height, format);
			delete vf;
			return nullptr;
		}
	}

	// Initialise frame for given config
	bool VideoFrame::Initialize(int width, int height, eTexFmt format)
	{
		assert(width != 0);

		D3D11_TEXTURE2D_DESC desc;
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		GetTexFmtD3D11(format, desc.Format);
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;

		HRESULT hr = UnityPlugin::D3D11Device()->CreateTexture2D(&desc, nullptr, (ID3D11Texture2D**)&mTexture);
		if (hr == S_OK)
		{
			mWidth = width;
			mHeight = height;
			mFormat = format;
		}
		DebugLog("VideoFrame::Initialize(width=%d, height=%d, format=%d) returns %s (hr=%08x)", width, height, format, (width != 0 ? "true" : "false"), hr);
		return (mWidth != 0);
	}

	void VideoFrame::Release()
	{
		if (mTexture != nullptr)
		{
			if (IsLocked())
			{
				Unlock();
			}
			((ID3D11Texture2D*)mTexture)->Release();
			mTexture = nullptr;
		}

		mWidth = 0;
		mHeight = 0;
		mFormat = 0;

		DebugLog("VideoFrame::Release()");

		delete this;
	}

	// Constructor set all members to initial state
	VideoFrame::VideoFrame()
	{
		mWidth = 0;
		mHeight = 0;
		mFormat = 0;
		mTexture = nullptr;
		mData = nullptr;
		mRowPitch = 0;

		DebugLog("VideoFrame::VideoFrame()");
	}

	// Destructor just makes sure we've been shutdown
	VideoFrame::~VideoFrame()
	{
		assert(mTexture == nullptr);
		assert(!IsLocked());

		DebugLog("VideoFrame::~VideoFrame()");
	}
}
