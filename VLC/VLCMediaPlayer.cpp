#include <cassert>

#include <vlc/vlc.h>
#include "UnityPlugin.h"

#include "PluginUtils.h"
#include "VideoFrameManager.h"
#include "VLCMediaPlayer.h"

// --------------------------------------------------------------------------
// SetTimeFromUnity, an example function we export which is called by one of the scripts.

namespace FPVR
{
	VLCMediaPlayer* gVLCMediaPlayer = nullptr;

	// --------------------------------------------------------------------------
	// Helper utilities

	// ---------------------------------------------------------------------------------------------
	// Lifecycle management

	// Create an instance of a media player. Some resources will be allocated and player will be
	// in 'idle' state. Returns nullptr on failure.
	VLCMediaPlayer* VLCMediaPlayer::Create()
	{
		VLCMediaPlayer* mp = new VLCMediaPlayer();
		if (mp != nullptr  && mp->Initialize())
		{
			return mp;
		}
		return nullptr;
	}

	// Releases all resources and deletes this media player
	void VLCMediaPlayer::Release()
	{
		DebugLog("VLCMediaPlayer::Release()");
		Shutdown();
		delete this;
	}

	// Return player to idle state.
	void VLCMediaPlayer::Reset()
	{
		if (mVLCMediaPlayer != nullptr)
		{
			libvlc_media_player_stop(mVLCMediaPlayer);
			libvlc_media_player_release(mVLCMediaPlayer);
			Sleep(0);
			mVLCMediaPlayer = nullptr;
		}
		if (mVLCMedia != nullptr)
		{
			libvlc_media_release(mVLCMedia);
			mVLCMedia = nullptr;
		}

		mPrepared = false;
		mReachedEnd = false;
		mHadVideoRenderingStart = false;

		// All states unknown
		mVideoWidth = -1;
		mVideoHeight = -1;
		mVideoDuration = -1;
		mMediaIsSeekable = true;
		mMediaIsPausable = true;

		mNumAudioChannels = 0;
		for(int i = 0; i < MaxAudioChannels; i++)
		{
			mChannelStereo[i] = false;
			mChannelFrequency[i] = 0;
		}
		if (mVideoPath != nullptr)
		{
			free(mVideoPath);
			mVideoPath = nullptr;
		}
		mVideoPathIsURL = false;

		ClearMediaEvents();
	}

	// Returns true if path refers to a URL. Currently assumes local if not one of our recognised schemes
	static bool PathIsURL(const char* path)
	{
		return (_strnicmp(path, "http:", 5) == 0
			|| _strnicmp(path, "https:", 6) == 0
			|| _strnicmp(path, "rtsp:", 5) == 0
			|| _strnicmp(path, "rtmp:", 5) == 0
			|| _strnicmp(path, "file:", 5) == 0);
	}

	// ---------------------------------------------------------------------------------------------
	// Setup functions, these must be called prior to calling prepare

	// Set the path to the media to be played. Ignored if player is not in idle state
	bool VLCMediaPlayer::SetDataSource(const char* path)
	{
		if (mVLCMedia == nullptr)
		{
			if (mVideoPath != nullptr)
			{
				free(mVideoPath);
			}
			if (path != nullptr)
			{
				mVideoPath = _strdup(path);
			}
			else
			{
				mVideoPath = nullptr;
			}
			mVideoPathIsURL = (mVideoPath != nullptr && PathIsURL(mVideoPath));
			DebugLog("VLCMediaPlayer::SetDataSource(%s=%s)", (mVideoPathIsURL ? "url" : "path"), (mVideoPath == nullptr ? "null" : mVideoPath));
			return true;
		}
		else
		{
			AddMediaEvent(eMPEvent::OnError, eMPError::IncompatibleState);
			return false;
		}
	}

	// Set the texture the media is to be played back to
	bool VLCMediaPlayer::SetTexture(void* texture, int width, int height, eTexFmt texFmt)
	{
		DebugLogS("SetTexture 0004");
		if (mVLCMedia == nullptr)
		{
			mFrameManager->SetTarget(texture, width, height, texFmt);
			DumpTextureDesc(texture);
			DebugLog("VLCMediaPlayer::SetSurface(texture=%08x, width=%d, height=%d, format=%s)", texture, width, height, GetTexFmtFourCC(texFmt));
			return true;
		}
		else
		{
			AddMediaEvent(eMPEvent::OnError, eMPError::IncompatibleState);
			return false;
		}
	}

	// ---------------------------------------------------------------------------------------------
	// State and Information functions (must be called after prepare complete)

/*
	const char *GetVLCStateName(libvlc_state_t state)
	{
		switch (state)
		{
		case libvlc_NothingSpecial:
			return "NothingSpecial";
		case libvlc_Opening:
			return "Opening";
		case libvlc_Buffering:
			return "Buffering";
		case libvlc_Playing:
			return "Playing";
		case libvlc_Paused:
			return "Paused";
		case libvlc_Stopped:
			return "Stopped";
		case libvlc_Ended:
			return "Ended";
		case libvlc_Error:
			return "Error";
		default:
			return "Unknown";
		}
	}
*/

	// ---------------------------------------------------------------------------------------------
	// Playback control

	void VLCMediaPlayer::OnVLCEvent(const libvlc_event_t* ev, void* vsmp)
	{
		VLCMediaPlayer* mp = (VLCMediaPlayer*)vsmp;
		char extra[64];
		extra[0] = '\0';

		switch (ev->type)
		{
		case libvlc_MediaParsedChanged:
		{
			unsigned int w = 0, h = 0;
			if (ev->u.media_parsed_changed.new_status != 0)
			{
				libvlc_video_get_size(mp->mVLCMediaPlayer, 0, &w, &h);
				mp->mVideoWidth = w;
				mp->mVideoHeight = h;
				mp->AddMediaEvent(eMPEvent::OnPrepared);
				mp->mPrepared = true;
			}
			_snprintf_s(extra, sizeof(extra), _TRUNCATE, "parsed=%d, w=%d, h=%d", ev->u.media_parsed_changed.new_status, w, h);
			break;
		}
		case libvlc_MediaPlayerPlaying:
			mp->AddMediaEvent(eMPEvent::OnPlaying);
			break;
		case libvlc_MediaPlayerPaused:
			mp->AddMediaEvent(eMPEvent::OnPaused);
			break;
		case libvlc_MediaPlayerBuffering:
			if (ev->u.media_player_buffering.new_cache >= 100.0f)
			{
					mp->AddMediaEvent(eMPEvent::OnBufferingEnd);
			}
			else if (ev->u.media_player_buffering.new_cache == 0.0f)
			{
				mp->AddMediaEvent(eMPEvent::OnBufferingStart);
			}
			else
			{
				mp->AddMediaEvent(eMPEvent::OnBufferingProgress, ev->u.media_player_buffering.new_cache);
			}
			_snprintf_s(extra, sizeof(extra), _TRUNCATE, "cache=%f", ev->u.media_player_buffering.new_cache);
			break;
		case libvlc_MediaPlayerEndReached:
			mp->AddMediaEvent(eMPEvent::OnReachedEnd);
			mp->mReachedEnd = true;
			break;
		case libvlc_MediaPlayerTimeChanged:
			mp->AddMediaEvent(eMPEvent::OnPositionChanged, ev->u.media_player_time_changed.new_time);
			_snprintf_s(extra, sizeof(extra), _TRUNCATE, "new_time=%I64d", ev->u.media_player_time_changed.new_time);
			break;
		case libvlc_MediaPlayerEncounteredError:
			mp->AddMediaEvent(eMPEvent::OnError, eMPError::MediaError);
			break;
		case libvlc_MediaPlayerSeekableChanged:
			mp->mMediaIsSeekable = (ev->u.media_player_seekable_changed.new_seekable != 0);
			_snprintf_s(extra, sizeof(extra), _TRUNCATE, "seekable=%d", ev->u.media_player_seekable_changed.new_seekable);
			break;
		case libvlc_MediaPlayerPausableChanged:
			mp->mMediaIsPausable = (ev->u.media_player_pausable_changed.new_pausable != 0);
			_snprintf_s(extra, sizeof(extra), _TRUNCATE, "pausable=%d", ev->u.media_player_pausable_changed.new_pausable);
			break;
		case libvlc_MediaPlayerLengthChanged:
			mp->mVideoDuration = ev->u.media_player_length_changed.new_length;
			_snprintf_s(extra, sizeof(extra), _TRUNCATE, "length=%I64d", ev->u.media_player_length_changed.new_length);
			break;
		}
		// TODO: detect seek complete

		DebugLog("VLCMediaPlayer::OnMediaEvent(%s) - %s", libvlc_event_type_name(ev->type), extra);
	}

	// Add a media event and associated parameter to end of queue
	void VLCMediaPlayer::AddMediaEvent(eMPEvent newEvent, int64_t param)
	{
		std::lock_guard<std::mutex> lock(mEventQueueMutex);
		MPEvent mpEvent;
		mpEvent.mMPEvent = newEvent;
		mpEvent.mParam = param;
		mEventQueue.push(mpEvent);
	}


	bool VLCMediaPlayer::GetMediaEvent(eMPEvent* mpEvent, int64_t* param)
	{
		std::lock_guard<std::mutex> lock(mEventQueueMutex);

		if (!mEventQueue.empty())
		{
			MPEvent mpev = mEventQueue.front();
			*mpEvent = mpev.mMPEvent;
			*param = mpev.mParam;
			mEventQueue.pop();
			return true;
		}
		else
		{
			return false;
		}
	}

	void VLCMediaPlayer::ClearMediaEvents()
	{
		std::lock_guard<std::mutex> lock(mEventQueueMutex);
		while (!mEventQueue.empty())
		{
			mEventQueue.pop();
		}
	}

	void VLCMediaPlayer::AttachMediaEvents()
	{
		assert(mVLCMedia != nullptr);
		libvlc_event_manager_t* eventMgr = libvlc_media_event_manager(mVLCMedia);
		libvlc_event_attach(eventMgr, libvlc_MediaParsedChanged, OnVLCEvent, this);
	}

	void VLCMediaPlayer::AttachMediaPlayerEvents()
	{
		assert(mVLCMediaPlayer != nullptr);
		libvlc_event_manager_t* eventMgr = libvlc_media_player_event_manager(mVLCMediaPlayer);
		libvlc_event_attach(eventMgr, libvlc_MediaPlayerOpening, OnVLCEvent, this);
		libvlc_event_attach(eventMgr, libvlc_MediaPlayerBuffering, OnVLCEvent, this);
		libvlc_event_attach(eventMgr, libvlc_MediaPlayerPlaying, OnVLCEvent, this);
		libvlc_event_attach(eventMgr, libvlc_MediaPlayerPaused, OnVLCEvent, this);
		libvlc_event_attach(eventMgr, libvlc_MediaPlayerStopped, OnVLCEvent, this);
		libvlc_event_attach(eventMgr, libvlc_MediaPlayerEndReached, OnVLCEvent, this);
		libvlc_event_attach(eventMgr, libvlc_MediaPlayerTimeChanged, OnVLCEvent, this);
		libvlc_event_attach(eventMgr, libvlc_MediaPlayerEncounteredError, OnVLCEvent, this);
		libvlc_event_attach(eventMgr, libvlc_MediaPlayerSeekableChanged, OnVLCEvent, this);
		libvlc_event_attach(eventMgr, libvlc_MediaPlayerPausableChanged, OnVLCEvent, this);
		libvlc_event_attach(eventMgr, libvlc_MediaPlayerLengthChanged, OnVLCEvent, this);
	}
	
	// When the video frame needs to be shown, as determined by the media playback clock, the display callback is invoked.
	void* VLCMediaPlayer::VLCLockCB(
		void*	opaque,		// Pointer to this VLCMediaPlayer passed to libvlc_video_set_callbacks()
		void**	planes)		// start address of the pixel planes (LibVLC allocates the array of void pointers, this callback must initialize the array)
	{
		VLCMediaPlayer* mp = (VLCMediaPlayer*)opaque;
		VideoFrame* frame = mp->mFrameManager->GetFrame();
		assert(frame != nullptr);

		*planes = frame->Pixels();

		//DebugLog("VLCLockCB plane:%08x, frame:%08x", *planes, frame);
		return frame;
	}

	// When the video frame decoding is complete, the unlock callback is invoked.
	// This callback might not be needed at all.It is only an indication that the
	// application can now read the pixel values if it needs to.
	void VLCMediaPlayer::VLCUnlockCB(
		void*		opaque,		// Pointer to VideoFrameManager passed to libvlc_video_set_callbacks()
		void*		picture,	// Pointer to VideoFrame returned from the VLCLockCB callback()
		void*const*	planes)		// Pixel planes as defined by the libvlc_video_lock_cb callback (this parameter is only for convenience)
	{
		//VLCMediaPlayer* vmp = (VLCMediaPlayer*)opaque;
		//VideoFrame* frame = (VideoFrame*)picture;

		//DebugLog("VLCUnlockCB plane:%08x, frame:%08x", *planes, picture);
	}

	// When the video frame needs to be shown, as determined by the media playback
	// clock, the display callback is invoked
	void VLCMediaPlayer::VLCDisplayCB(
		void*	opaque,			// Pointer to VideoFrameManager passed to libvlc_video_set_callbacks()
		void*	picture)		// Pointer to VideoFrame returned from the VLCLockCB callback()
	{
		VLCMediaPlayer* mp = (VLCMediaPlayer*)opaque;
		VideoFrame* frame = (VideoFrame*)picture;
		mp->mFrameManager->DisplayFrame(frame);

		// TODO: Actually first frame has only been rendered when the first copy to in the
		// frame manager has completed. So this needs to move to the frame manager
		if (!mp->mHadVideoRenderingStart)
		{
			mp->mHadVideoRenderingStart = true;
			mp->AddMediaEvent(eMPEvent::OnVideoRenderingStart);
		}
		//DebugLog("VLCDisplayCB frame:%08x", frame);
	}

	// When new audio data is available LibVLC calls this function
	void VLCMediaPlayer::VLCPlayCB(
		void*		data,		//	data data pointer as passed to libvlc_audio_set_callbacks() [IN]
		const void*	samples,	//	samples pointer to the first audio sample to play back [IN]
		unsigned	count,		//	count number of audio samples to play back
		int64_t		pts)		//	expected play time stamp (see libvlc_delay())
	{
		VLCMediaPlayer* mp = (VLCMediaPlayer*)data;
	}

	// Audio playback should be paused
	void VLCMediaPlayer::VLCPauseCB(
		void*	data,	// data pointer as passed to libvlc_audio_set_callbacks()[IN]
		int64_t	pts)	// time stamp of the pause request(should be elapsed already)
	{
		VLCMediaPlayer* mp = (VLCMediaPlayer*)data;

	}

	// Audio playback should be resumed after pause
	void VLCMediaPlayer::VLCResumeCB(
		void*	data,	// pointer as passed to libvlc_audio_set_callbacks() [IN]
		int64_t	pts)	// time stamp of the resumption request (should be elapsed already)
	{
		VLCMediaPlayer* mp = (VLCMediaPlayer*)data;
	}

	// Callback prototype for audio buffer flush (i.e. discard all pending buffers and
	// stop playback as soon as possible).
	void VLCMediaPlayer::VLCFlushCB(
		void*	data,	// pointer as passed to libvlc_audio_set_callbacks() [IN]
		int64_t	pts)
	{
		VLCMediaPlayer* mp = (VLCMediaPlayer*)data;
	}

	// Callback for audio buffer drain (wait for pending buffers to be played)
	void VLCMediaPlayer::VLCDrainCB(
		void*	data)	// pointer as passed to libvlc_audio_set_callbacks() [IN]
	{
		VLCMediaPlayer* mp = (VLCMediaPlayer*)data;
//?		AddMediaEvent(eMPEvent::AudioDrain);
	}

	// Fills the specified buffer the available audio data for the channel up to maximum buffer length
	// Return is number of floats available after number returned. floatsCopied indicates how many were returned
	// if floatsCopied < maxLength then return value should always be zero
	int VLCMediaPlayer::RetrieveAudioData(int channel, float* buffer, int maxLength, int* floatsCopied)
	{
		*floatsCopied = 0;
		return 0;
	}

	// Having specified data source and surface, this function gets ready to play. Once
	// play starts we should have valid info about the video (readable/playable, width, height
	// and possibly duration).
	bool VLCMediaPlayer::PrepareAsync()
	{
		assert(mVLCInstance != nullptr && mVideoPath != nullptr);

		DebugLog("VLCMediaPlayer::PrepareAsync()");

		// Check objects in expected state
		if (mVLCInstance == nullptr
			|| mVLCMedia != nullptr
			|| mVLCMediaPlayer != nullptr)
		{
			AddMediaEvent(eMPEvent::OnError, eMPError::IncompatibleState);
			DebugLog("VLCMediaPlayer::PrepareAsync() IncompatibleState");
			return false;
		}

		// User must have supplied video path and a texture of known format
		if (mVideoPath == nullptr
			|| mFrameManager->Format() == eTexFmt::TEXFMT_UNKNOWN)
		{
			DebugLog("VLCMediaPlayer::PrepareAsync() BadArgument");
			AddMediaEvent(eMPEvent::OnError, eMPError::BadArgument);
			return false;
		}

		// Create media object
		if (mVideoPathIsURL)
		{
			mVLCMedia = libvlc_media_new_location(mVLCInstance, mVideoPath);
		}
		else
		{
			mVLCMedia = libvlc_media_new_path(mVLCInstance, mVideoPath);
		}
		if (mVLCMedia != nullptr)
		{
			AttachMediaEvents();

			// Create media player instance
			mVLCMediaPlayer = libvlc_media_player_new_from_media(mVLCMedia);
			if (mVLCMediaPlayer != nullptr)
			{
				AttachMediaPlayerEvents();
				libvlc_log_set(mVLCInstance, LibVLCLogCB, this);

				libvlc_video_set_callbacks(mVLCMediaPlayer, VLCLockCB, VLCUnlockCB, VLCDisplayCB, this);
				libvlc_video_set_format(mVLCMediaPlayer, mFrameManager->FourCC(), mFrameManager->Width(), mFrameManager->Height(), mFrameManager->Stride());

				libvlc_audio_set_callbacks(mVLCMediaPlayer, VLCPlayCB, VLCPauseCB, VLCResumeCB, VLCFlushCB, VLCDrainCB, this);
				libvlc_audio_set_format(mVLCMediaPlayer, "f32l", 48000, 1);

				// Start playing (this forces player to actually read media)
				libvlc_media_player_play(mVLCMediaPlayer);
			}
			else
			{
				libvlc_media_release(mVLCMedia);
				mVLCMedia = nullptr;
			}
		}
		if (mVLCMediaPlayer != nullptr)
		{
			return true;
		}
		else
		{
			AddMediaEvent(eMPEvent::OnError, eMPError::InternalError);
			return false;
		}
	}

	// Update target texture with latest frame (if changed)
	void VLCMediaPlayer::Render()
	{
		mFrameManager->Render();
	}

	// Call every frame to process video events
	void VLCMediaPlayer::Update()
	{
	}

	// Start playing from current position (if immediately after Prepare then from beginning)
	// If no video, then sets intent for when video is prepared
	void VLCMediaPlayer::Play()
	{
		if (mVLCMediaPlayer != nullptr && mPrepared)
		{
			if (mReachedEnd)
			{
				libvlc_media_player_set_media(mVLCMediaPlayer, mVLCMedia);
				mReachedEnd = true;
			}
			libvlc_media_player_play(mVLCMediaPlayer);
		}
		else
		{
			AddMediaEvent(eMPEvent::OnError, eMPError::IncompatibleState);
		}
	}

	// Pause video at current position
	void VLCMediaPlayer::Pause()
	{
		if (mVLCMediaPlayer != nullptr && mPrepared && !mReachedEnd)
		{
			libvlc_media_player_pause(mVLCMediaPlayer);
		}
		else
		{
			AddMediaEvent(eMPEvent::OnError, eMPError::IncompatibleState);
		}
	}

	// Retrieve current playback position
	int64_t VLCMediaPlayer::GetCurrentPosition()
	{
		int64_t pos = 0;
		if (mVLCMediaPlayer != nullptr && mPrepared)
		{
			if (mReachedEnd)
			{
				pos = mVideoDuration;
			}
			else
			{
				pos = libvlc_media_player_get_time(mVLCMediaPlayer);
			}
		}
		else
		{
			AddMediaEvent(eMPEvent::OnError, eMPError::IncompatibleState);
		}
		return pos;
	}

	// Seek to specified position (if seekable)
	void VLCMediaPlayer::SeekTo(int64_t pos)
	{
		if (mVLCMediaPlayer != nullptr && mPrepared)
		{
			if (mReachedEnd)
			{
				mReachedEnd = false;
				libvlc_media_player_set_media(mVLCMediaPlayer, mVLCMedia);
				libvlc_media_player_play(mVLCMediaPlayer);
			}
			libvlc_media_player_set_time(mVLCMediaPlayer, pos);
		}
		else
		{
			AddMediaEvent(eMPEvent::OnError, eMPError::IncompatibleState);
		}
	}

	// Debug callback from LibVLC
	void VLCMediaPlayer::LibVLCLogCB(void *data, int level, const libvlc_log_t *ctx, const char *fmt, va_list args)
	{
		static const int g_DebugLevelThreshold = LIBVLC_NOTICE;	// LIBVLC_DEBUG
		VLCMediaPlayer*mp = (VLCMediaPlayer*)data;

		if (level >= g_DebugLevelThreshold)
		{
			DebugLogV(fmt, args);
		}
	}

	// Allocate any resources we know we'll need
	bool VLCMediaPlayer::Initialize()
	{
		mVLCInstance = libvlc_new(0, NULL);
		if (mVLCInstance != nullptr)
		{
			mFrameManager = VideoFrameManager::Create(2);
			if (mFrameManager == nullptr)
			{
				libvlc_release(mVLCInstance);
				mVLCInstance = nullptr;
			}
		}
		DebugLog("VLCMediaPlayer::Initialize(): %s", (mVLCInstance != nullptr ? "succeeded" : "failed"));

		return (mVLCInstance != nullptr);
	}

	// Release all remaining resources (calls reset)
	void VLCMediaPlayer::Shutdown()
	{
		DebugLogS("VLCMediaPlayer::Shutdown()");

		Reset();

		if (mVideoPath != nullptr)
		{
			free(mVideoPath);
			mVideoPath = nullptr;
		}
		mVideoPathIsURL = false;

		if (mFrameManager != nullptr)
		{
			mFrameManager->Release();
			mFrameManager = nullptr;
		}

		if (mVLCInstance != nullptr)
		{
			libvlc_release(mVLCInstance);
			mVLCInstance = nullptr;
		}
	}

	// If any of the following function pointers are non-null the function will be
	// called when the corresponding event occurs.
	VLCMediaPlayer::VLCMediaPlayer()
	{
		mVLCInstance = nullptr;
		mVLCMedia = nullptr;
		mVLCMediaPlayer = nullptr;

		mPrepared = false;
		mReachedEnd = false;
		mHadVideoRenderingStart = false;

		mFrameManager = nullptr;

		mMediaIsSeekable = true;
		mMediaIsPausable = true;

		mVideoWidth = -1;
		mVideoHeight = -1;
		mVideoDuration = -1;

		mNumAudioChannels = 0;
		for (int i = 0; i < MaxAudioChannels; i++)
		{
			mChannelStereo[i] = false;
			mChannelFrequency[i] = 0;
		}
		mVideoPath = nullptr;
		mVideoPathIsURL = false;

		mVideoPath = nullptr;
		mVideoPathIsURL = false;

		DebugLogS("VLCMediaPlayer::VLCMediaPlayer()");
	}

	// Destructor
	VLCMediaPlayer::~VLCMediaPlayer()
	{
		DebugLogS("VLCMediaPlayer::~VLCMediaPlayer()");
		assert(mVLCInstance == nullptr);
		if (mVLCInstance != nullptr)
		{
			DebugLog("VLCMediaPlayer destructor called without previously being shutdown");
		}
	}

}