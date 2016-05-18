#pragma once

#include <mutex>
#include <queue>

#include <vlc/vlc.h>

#include "Unity/IUnityGraphics.h"
#include "PluginUtils.h"
#include "VideoFrameManager.h"
#include "VLCMediaPlayer.h"

namespace FPVR
{
	class VLCMediaPlayer;

	extern VLCMediaPlayer* gVLCMediaPlayer;

	// Event structure, provides an event and associated context
	typedef enum
	{
		NoEvent = 0,
		OnError = 1,					// An was encountered with an asynchronous operation (param = int error code)
		OnPrepared = 2,					// Media has been successfully parsed and is ready to play
		OnVideoRenderingStart = 3,		// First frame of video is ready to display
		OnPaused = 4,					// Media playback has been paused
		OnPlaying = 5,					// Media playback has started
		OnSeekComplete = 6,				// Seek to new position has completed
		OnReachedEnd = 7,				// Media playback has reached end of media
		OnBufferingStart = 8,			// Buffering has started
		OnBufferingProgress = 9,		// New percentage complete for buffering (param = float percentage(
		OnBufferingEnd = 10,			// Buffering has ended
		OnPositionChanged = 11			// Regular update on current position when playing
	} eMPEvent;

	typedef enum
	{
		NoError = 0,					// No error has occurred
		IncompatibleState = 1,			// Call made when player in state where call not allowed
		BadArgument = 2,				// Call made to function with error in argument(s)
		InternalError = 3,				// Unexpected error occurred internally
		MediaError = 4					// Problem occurred reading / processing media
	} eMPError;

	typedef struct
	{
		eMPEvent mMPEvent;
		int64_t mParam;
	} MPEvent;

	class VLCMediaPlayer
	{
	public:
		static const int MaxAudioChannels = 8;

		// ---------------------------------------------------------------------------------------------
		// Lifecycle management

		// Create an instance of a media player. Some resources will be allocated and player will be
		// in 'idle' state. Returns nullptr on failure.
		static VLCMediaPlayer* Create();

		// Release this media player (deletes media player)
		void Release();

		// Return player to idle state (clears all state except any listeners).
		void Reset();

		// ---------------------------------------------------------------------------------------------
		// Setup functions, these must be called prior to calling prepare

		// Set the path to the media to be played
		bool SetDataSource(const char* path);

		// Set the surface the media is to be played back to
		bool SetTexture(void* texture, int width, int height, eTexFmt format);

		// ---------------------------------------------------------------------------------------------
		// State and Information functions (only useful after Prepare is complete - ie input media is parsed)

		// Returns true if the media is thought to be seekable, false if it is known not to be
		bool IsSeekable() { return mMediaIsSeekable; }

		// Returns true if the media is thought to be pausable, false if it is known not to be
		bool IsPausable() { return mMediaIsPausable; }

		// Return width and height of source video (as opposed to playback resolution)
		int GetVideoWidth() { return mVideoWidth; }
		int GetVideoHeight() { return mVideoHeight; }

		// Returns duration of video if known (otherwise -1)
		int64_t GetDuration() { return mVideoDuration; }
		
		// Returns number of audio channels in current media
		int GetNumAudioChannels() { return mNumAudioChannels; }

		// Returns whether a specific channel is stereo (or not)
		bool IsAudioChannelStereo(int channel) { return mChannelStereo[channel]; }

		// Returns channel frequency in Hz
		int ChannelFrequency(int channel) { return mChannelFrequency[channel]; }

		// If returns true then retrieves next event, otherwise returns false and mpEvent unchanged
		bool GetMediaEvent(eMPEvent* mpEvent, int64_t* param);

		// Fills the specified buffer the available audio data for the channel up to maximum buffer length
		// Return is number of floats available after number returned. floatsCopied indicates how many were returned
		// if floatsCopied < maxLength then return value should always be zero
		// TODO: Change API to optimise, handle synchronisation and flush/drain etc
		int RetrieveAudioData(int channel, float* buffer, int maxLength, int* floatsCopied);

		// ---------------------------------------------------------------------------------------------
		// Playback control

		// Having specified data source and surface, this function gets ready to play. Once
		// play starts we should have valid info about the video (readable/playable, width, height
		// and possibly duration).
		bool PrepareAsync();

		// Call once per frame from the thread to complete updates.
		void Update();

		// Call once per frame from the render thread to update target texture
		void Render();

		// Start playing from current position (if immediately after Prepare or after ReachedEnd then from beginning)
		void Play();

		// Pause video at current position (only meaningful if playing, otherwise ignored or invalid state)
		void Pause();

		// Retrieve current playback position
		int64_t GetCurrentPosition();

		// Seek to specified position (if seekable) - can be playing or paused
		void SeekTo(int64_t pos);

	protected:
		// LibVLC objects
		libvlc_instance_t* mVLCInstance;			// Instance of VLC library
		libvlc_media_t* mVLCMedia;					// Media instance object for media we want to play
		libvlc_media_player_t* mVLCMediaPlayer;		// Media player object created to play media

		// Internal state
		bool mPrepared;								// True once media has been parsed
		bool mReachedEnd;							// True if we have reached the end (cleared once we sort out player)
		bool mHadVideoRenderingStart;				// True if we have already sent the OnVideoRenderingStart event

		// Management objects
		VideoFrameManager* mFrameManager;			// Video frame manager

		std::mutex mEventQueueMutex;				// Mutex to make event queue thread safe
		std::queue<MPEvent> mEventQueue;			// Queue of video events

		// General media information
		bool mMediaIsSeekable;						// True if media is thought to be seekable, false if known not to be
		bool mMediaIsPausable;						// True if media is thought to be pausable, false otherwise

		// Video information
		int mVideoWidth;							// Current known video width (-1 if unknown, 0 if no playable video)
		int mVideoHeight;							// Current known video height (-1 if unknown, 0 if no playable video)
		int64_t mVideoDuration;						// Current known video duration (-1 if unknown)

		// Audio information
		int mNumAudioChannels;						// Number of audio channels available
		bool mChannelStereo[MaxAudioChannels];		// For each channel true if stereo, otherwise mono
		int mChannelFrequency[MaxAudioChannels];	// For each channel sample frequency in Hz

		// User supplied state
		char* mVideoPath;							// Path for video we're to play
		bool mVideoPathIsURL;						// True if path is a URL (ie contains a recognised scheme:)

		// Add a media player event to the queue
		void AddMediaEvent(eMPEvent newEvent, int64_t param);

		void AddMediaEvent(eMPEvent newEvent) { AddMediaEvent(newEvent, (int64_t)0); }
		void AddMediaEvent(eMPEvent newEvent, int param) { AddMediaEvent(newEvent, (int64_t)param); }
		void AddMediaEvent(eMPEvent newEvent, float param) { AddMediaEvent(newEvent, (int64_t)(*((int*)&param))); }
		void AddMediaEvent(eMPEvent newEvent, int param1, int param2) { AddMediaEvent(newEvent, ((int64_t)param1) | (((int64_t)param2) << 32)); }

		// Clear the media event queue
		void ClearMediaEvents();

		void AttachMediaEvents();
		void AttachMediaPlayerEvents();

		static void OnVLCEvent(const libvlc_event_t* ev, void* vlcmp);

		// Call backs from VLC video playback to write to texture
		static void* VLCLockCB(void* opaque, void** planes);
		static void VLCUnlockCB(void* opaque, void* picture, void*const* planes);
		static void VLCDisplayCB(void* opaque, void* picture);

		// Callbacks from VLC audio playback to write to audio buffers
		static void VLCPlayCB(void* data, const void* samples,	unsigned count, int64_t pts);
		static void VLCPauseCB(void* data, int64_t pts);
		static void VLCResumeCB(void* data, int64_t pts);
		static void VLCFlushCB(void* data, int64_t pts);
		static void VLCDrainCB(void* data);

		// Debug logging functions
		static void LibVLCLogCB(void *data, int level, const libvlc_log_t *ctx, const char *fmt, va_list args);

		// Allocate any resources which don't change from video to video
		bool Initialize();

		// Release any resources allocated during Initialize
		void Shutdown();

		VLCMediaPlayer();
		~VLCMediaPlayer();
	};
}