// ---------------------------------------------------------------------------
// Video Frame Manager Class
//
// Manages a pool of video frames and the next frame to display

#include <cassert>

#include "VideoFrameManager.h"
#include "VideoFrame.h"
#include "PluginUtils.h"
#include "UnityPlugin.h"

namespace FPVR
{
	void VideoFrameManager::SetTarget(void* texture, int width, int height, eTexFmt texFmt)
	{
		//DebugLog("VideoFrameManager::SetTarget(width=%d, height=%d, format=%d)", width, height, unityFormat);
	
		std::lock_guard<std::mutex> lock(mMutex);

		// If any of format, width or height have changed then clear free list
		if (texFmt != mTexFmt
			|| width != mWidth
			|| height != mHeight)
		{
			mWidth = width;
			mHeight = height;
			mTexFmt = texFmt;

			// Move all frames on the free and pending lists to the release list
			MoveListToRelease(mFreeFrames);
			MoveListToRelease(mPendingFrames);

			// If there's a display frame then release that too
			if (mDisplayFrame != nullptr)
			{
				mAllocatedFrames.remove(mDisplayFrame);
				mReleaseFrames.push_back(mDisplayFrame);
				mDisplayFrame = nullptr;
			}

			// That may leave us with one or more frames on the allocated frame list
			// which might be being written to by some other thread, theoretically those
			// frames should be displayed and will get put on the release list at that point
		}
		mTexture = texture;
	}

	//  Walks a frame list and moves them to release list
	void VideoFrameManager::MoveListToRelease(std::list<VideoFrame*>& frameList)
	{
		std::list<VideoFrame*>::iterator it;
		for (it = frameList.begin(); it != frameList.end(); it++)
		{
			mReleaseFrames.push_back(*it);
		}
		frameList.clear();
	}

	//  Walks a frame list and releases all the frames
	void VideoFrameManager::ClearFrameList(std::list<VideoFrame*>& frameList)
	{
		if (!frameList.empty())
		{
			std::list<VideoFrame*>::iterator it;
			for (it = frameList.begin(); it != frameList.end(); it++)
			{
				VideoFrame *vf = *it;
				vf->Release();
				mNumBuffers--;
			}
			frameList.clear();
		}
	}

	// Returns true if the specified frame is in the supplied list
	bool VideoFrameManager::ListContains(std::list<VideoFrame*>& frameList, VideoFrame* frame)
	{
		std::list<VideoFrame*>::iterator it;
		for (it = frameList.begin(); it != frameList.end(); it++)
		{
			if (*it == frame)
			{
				return true;
			}
		}
		return false;
	}

	// Create a new frame and lock it for writing 
	VideoFrame* VideoFrameManager::NewFrame()
	{
		assert(mTexture != nullptr);
		VideoFrame* vf = VideoFrame::Create(mWidth, mHeight, mTexFmt);
		if (vf != nullptr)
		{
			vf->Lock();
		}
		return vf;
	}

	// Attempt to get a locked frame from the free frames list
	VideoFrame *VideoFrameManager::TryGetFrame()
	{
		//DebugLog("VideoFrameManager::TryGetFrame() %s", (mFreeFrames.empty() ? "allocate" : "freelist"));

		assert(mTexture != nullptr);

		VideoFrame* vf = nullptr;
		std::lock_guard<std::mutex> lock(mMutex);

		// If list isn't empty then grab first frame on list
		if (!mFreeFrames.empty())
		{
			vf = mFreeFrames.front();
			mFreeFrames.pop_front();
			mAllocatedFrames.push_back(vf);
		}

		return vf;
	}

	// Allocate a frame from free list if available, if not wait for one to appear
	// Only call this function on background thread if you can guarantee that another
	// thread will be calling update frames
	VideoFrame *VideoFrameManager::GetFrame()
	{
		VideoFrame* tb = nullptr;

		for(;;)
		{
			tb = TryGetFrame();
			if (tb != nullptr)
			{
				return tb;
			}
			Sleep(2);
		}
	}

	// Set specified frame as next frame to display
	void VideoFrameManager::DisplayFrame(VideoFrame* videoFrame)
	{
		//DebugLog("VideoFrameManager::DisplayFrame(%08x) DP:%s", videoFrame, (mDisplayFrame != nullptr ? "free" : "null"));
		std::lock_guard<std::mutex> lock(mMutex);
		assert(ListContains(mAllocatedFrames, videoFrame));

		// If the format doesn't match then shove it on the pending list to sort out later. We can't release
		// it here as we could be being called from a background thread and we want to do all graphics stuff on
		// the render thread
		if (videoFrame->Width() != mWidth || videoFrame->Height() != mHeight && videoFrame->Format() != mTexFmt)
		{
			mAllocatedFrames.remove(videoFrame);
			mPendingFrames.push_back(videoFrame);
		}
		else
		{
			// If there was already a display frame then move it to the free frame list as it was never unlocked / rendered
			if (mDisplayFrame != nullptr)
			{
				mAllocatedFrames.remove(mDisplayFrame);
				mFreeFrames.push_back(mDisplayFrame);
			}

			// Got new display frame
			mDisplayFrame = videoFrame;
		}
	}

	// Retrieves current display frame
	VideoFrame* VideoFrameManager::GrabDisplayFrame()
	{
		//DebugLog("VideoFrameManager::GrabDisplayFrame() %08x", mDisplayFrame);
		std::lock_guard<std::mutex> lock(mMutex);
		VideoFrame* frame = mDisplayFrame;
		mDisplayFrame = nullptr;
		return frame;
	}

	// Move video frame from allocated to pending list
	void VideoFrameManager::MoveAllocatedToPending(VideoFrame* videoFrame)
	{
		//DebugLog("VideoFrameManager::MoveToPending(%08x)", videoFrame);
		std::lock_guard<std::mutex> lock(mMutex);
		assert(ListContains(mAllocatedFrames, videoFrame));
		mAllocatedFrames.remove(videoFrame);
		mPendingFrames.push_back(videoFrame);
	}

	// Release previous allocated video frame
	void VideoFrameManager::MovePendingToFree()
	{
		std::lock_guard<std::mutex> lock(mMutex);

		while(!mPendingFrames.empty() && mPendingFrames.front()->TryLock())
		{
			VideoFrame* front = mPendingFrames.front();
			mPendingFrames.pop_front();
			mFreeFrames.push_back(front);
		}
	}

	// Add a new frame to the free list
	void VideoFrameManager::AddFrameToFree(VideoFrame* videoFrame)
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mFreeFrames.push_back(videoFrame);
	}

	static int frameCount = 0;
	static void FillTextureFromCode(int width, int height, int stride, unsigned char* dst)
	{
		frameCount++;

		const float t = (float)frameCount * 0.2f;

		for (int y = 0; y < height; ++y)
		{
			unsigned char* ptr = dst;
			for (int x = 0; x < width; ++x)
			{
				// Simple oldskool "plasma effect", a bunch of combined sine waves
				int vv = int(
					(127.0f + (127.0f * sinf(x/7.0f+t))) +
					(127.0f + (127.0f * sinf(y/5.0f-t))) +
					(127.0f + (127.0f * sinf((x+y)/6.0f-t))) +
					(127.0f + (127.0f * sinf(sqrtf(float(x*x + y*y))/4.0f-t)))
					) / 4;

				// Write the texture pixel
				ptr[0] = vv;
				ptr[1] = vv;
				ptr[2] = vv;
				ptr[3] = vv;

				// To next pixel (our pixels are 4 bpp)
				ptr += 4;
			}

			// To next image row
			dst += stride;
		}
	}

	// Moves pending frames to free list once they can be locked. Ensures we have
	// at least one frame on the free list and then renders current display frame
	// (if there is one) and transfers it to pending list.
	void VideoFrameManager::Render()
	{
		if (mTexture != nullptr)
		{
			// Try to lock pending textures and move them to free list
			MovePendingToFree();

			// We need at least two frames and at least one of them free, add frames until we have what we need
			while (mNumBuffers < 2 || mFreeFrames.empty())
			{
				VideoFrame* vf = NewFrame();
				if (vf != nullptr)
				{
					AddFrameToFree(vf);
					mNumBuffers++;
				}
				else
				{
					break;
				}
			}

			VideoFrame* frame = GrabDisplayFrame();
			if (frame != nullptr)
			{
				FillTextureFromCode(frame->Width() / 4, frame->Height() / 4, frame->RowPitch(), (unsigned char*)frame->Pixels());
				frame->Unlock();
				frame->CopyTo(mTexture);
				MoveAllocatedToPending(frame);
			}
		}

		std::lock_guard<std::mutex> lock(mMutex);
		ClearFrameList(mReleaseFrames);
	}

	// Create an instance of the VideoFrameManager class
	VideoFrameManager* VideoFrameManager::Create(int poolSize)
	{
		return new VideoFrameManager(poolSize);
	}

	// Release this instance of the VideoFrameManagerClass
	void VideoFrameManager::Release()
	{
		delete this;
	}

	// Constructor: Initialise all member variables to a known state
	VideoFrameManager::VideoFrameManager(int poolSize)
	{
		mTexture = nullptr;

		mWidth = 0;
		mHeight = 0;
		mTexFmt = TEXFMT_UNKNOWN;

		mPoolSize = poolSize;
		mNumBuffers = 0;
		mDisplayFrame = nullptr;
	}

	// Destructor: Release all resources allocated
	// Note: Destructor is not thread safe, do not call when there
	// is a possibility of another thread still using frames
	VideoFrameManager::~VideoFrameManager()
	{
		VideoFrame* frame = GrabDisplayFrame();

		ClearFrameList(mFreeFrames);
		ClearFrameList(mAllocatedFrames);
		ClearFrameList(mPendingFrames);
		ClearFrameList(mReleaseFrames);
	}

}
