#pragma once

#include <list>
#include <mutex>

#include "UnityPlugin.h"
#include "VideoFrame.h"

namespace FPVR
{
	// ------------------------------------------------------------------------------------------------
	// Video Frame Manager Class
	//
	// Manages a pool of buffers used to store video frames. Required to be thread safe.
	//
	// Life cycle:
	//		Player:
	//			Asks for new frame / locks memory
	//			fills buffer with video frame
	//			unlocks memory
	//			Says buffer ready to display
	//		Viewer:
	//			requests a frame to display
	//			Copies memory away
	//			Finishes copy and releases memory
	class VideoFrameManager
	{
	protected:
		void* mTexture;				// Texture to be updated

		int mWidth;					// Width of frame in pixels
		int mHeight;				// Height of frame in pixels
		eTexFmt mTexFmt;			// Format of a pixel

		int mPoolSize;				// Number of buffers manager aims to have in pool
		int mNumBuffers;			// Total number of buffers owned by manager

		std::mutex	mMutex;			// Mutex used to make frame list/reference access thread safe

		std::list<VideoFrame*>	mFreeFrames;		// List of free video frames (all locked)
		std::list<VideoFrame*>	mAllocatedFrames;	// List of allocated video frames (all locked)
		VideoFrame*				mDisplayFrame;		// Frame to display next (will be on mAllocatedFrames list, will be unlocked before rendered)
		std::list<VideoFrame*>	mPendingFrames;		// List of frames we want to lock before they go back on free list (all unlocked)
		std::list<VideoFrame*>	mReleaseFrames;		// List of frames we want to release 

		void MoveListToRelease(std::list<VideoFrame*>& frameList);
		void ClearFrameList(std::list<VideoFrame*>& frameList);
		static bool ListContains(std::list<VideoFrame*>& frameList, VideoFrame* frame);

		void AddFrameToFree(VideoFrame* videoFrame);
		void MovePendingToFree();
		void MoveAllocatedToPending(VideoFrame* videoFrame);

		VideoFrame* NewFrame();
		VideoFrame* GrabDisplayFrame();
		void AllocFrame();

		// Free the video frame

		VideoFrameManager(int poolSize);
		~VideoFrameManager();

	public:
		static VideoFrameManager* Create(int poolSize);
		void Release();

		void SetTarget(void* texture, int width, int height, eTexFmt texFmt);

		// Retrieve video frame manager configuration
		int Width() const { return mWidth; }
		int Height() const { return mHeight; }
		eTexFmt Format() const { return mTexFmt; }

		// Get texture format info
		const char* FourCC() const { return GetTexFmtFourCC(mTexFmt); }
		int BytesPerPixel()  const { return (GetTexFmtBPP(mTexFmt) >> 3); }
		int Stride()  const { return (GetTexFmtBPP(mTexFmt) >> 3) * mWidth; }

		// If there's a free frame on the list then grab it
		VideoFrame* TryGetFrame();

		// Get a free frame from the list (if none available stall until we get one)
		VideoFrame* GetFrame();

		// Set specified frame as next frame to display
		void DisplayFrame(VideoFrame* videoFrame);

		// Attempt to map pending frames and put them on the free list.
		void UpdateFrames();

		// If there is a current display frame then copy it to target and release
		// display frame.
		void Render();

	};
}