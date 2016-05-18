#pragma once

#include "UnityPlugin.h"
#include "PluginUtils.h"

// ---------------------------------------------------------------------------
// Video Frame Class
//
// Wrapper for memory written to by video player.

namespace FPVR
{
	class VideoFrame
	{
	protected:
		int		mWidth;			// Width of frame in pixels
		int		mHeight;		// Height of frame in pixels
		int		mFormat;		// Native texture format

		void*	mTexture;		// Native texture pointer
		void*	mData;			// If mapped then pointer to memory
		int		mRowPitch;		// If mapped then pitch

		// Initialise underlying resources
		bool Initialize(int width, int height, eTexFmt format);

		// Constructor/destructor
		VideoFrame();
		~VideoFrame();

	public:
		// Width of video frame
		int Width() const { return mWidth; }

		// Height of video frame
		int Height() const { return mHeight; }

		// Format of video frame 
		int Format() const { return mFormat; }

		// True if locked
		bool IsLocked() const { return (mData != nullptr); }

		// Pointer to pixel data for writing
		void *Pixels() const { return mData; }

		// Pitch for frame row (valid when locked)
		int RowPitch() const { return mRowPitch; }

		// Create a video frame object with specified config
		static VideoFrame* Create(int width, int height, eTexFmt format);

		// Release resources and delete the video frame
		void Release();

		// Clear frame to 0
		void Clear(int val);

		// Get video frame memory for writing
		void Lock();

		// Try to lock the frame memory, return if not available
		bool TryLock();

		// Releases access to memory
		void Unlock();

		// Copy frame to specified native texture (assumed to be same format etc)
		void CopyTo(void* dstTex);
	};
}