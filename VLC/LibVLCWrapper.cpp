// LibVLCWrapper.cpp
//
// DLL for using LibVLC video player library from Unity. This module declares exported
// functions and rendering event system.

#include <cassert>

#include "UnityPlugin.h"
#include "VLCMediaPlayer.h"
#include "LibVLCWrapper.h"

using namespace FPVR;


// ------------------------------------------------------------------------------------------------
// Initialise media player
extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_Initialise()
{
	if (gVLCMediaPlayer == nullptr)
	{
		gVLCMediaPlayer = VLCMediaPlayer::Create();
	}
	return (gVLCMediaPlayer != nullptr);
}

// ------------------------------------------------------------------------------------------------
// Shutdown media player
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_Release()
{
	if (gVLCMediaPlayer != nullptr)
	{
		gVLCMediaPlayer->Release();
		gVLCMediaPlayer = nullptr;
	}
}

// ------------------------------------------------------------------------------------------------

// Return player to idle state 
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_Reset()
{
	if (gVLCMediaPlayer != nullptr)
	{
		gVLCMediaPlayer->Reset();
	}
}

// ---------------------------------------------------------------------------------------------
// Setup functions, these must be called prior to calling prepare

// Set the path to the media to be played
extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_SetDataSource(const char* path)
{
	if (gVLCMediaPlayer != nullptr)
	{
		return gVLCMediaPlayer->SetDataSource(path);
	}
	else
	{
		return false;
	}
}

// Set the surface the media is to be played back to
extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_SetTexture(void* texturePtr, int width, int height, int format)
{
	if (gVLCMediaPlayer != nullptr)
	{
		return gVLCMediaPlayer->SetTexture(texturePtr, width, height, GetTexFmtFromUnity(format));
	}
	else
	{
		return false;
	}
}

// ---------------------------------------------------------------------------------------------
// State and Information functions (must be called after prepare complete)

// Returns true if the video is thought to be seekable, false if it is known not to be
extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_IsSeekable()
{
	if (gVLCMediaPlayer != nullptr)
	{
		return gVLCMediaPlayer->IsSeekable();
	}
	else
	{
		return true;
	}
}

// Returns true if the video is thought to be pausable, false if it is known not to be
extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_IsPausable()
{
	if (gVLCMediaPlayer != nullptr)
	{
		return gVLCMediaPlayer->IsPausable();
	}
	else
	{
		return true;
	}
}

// Return width and height of source video (as opposed to playback resolution)
extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_GetVideoWidth()
{
	if (gVLCMediaPlayer != nullptr)
	{
		return gVLCMediaPlayer->GetVideoWidth();
	}
	else
	{
		return 0;
	}
}

// Return height of source video (as opposed to playback resolution)
extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_GetVideoHeight()
{
	if (gVLCMediaPlayer != nullptr)
	{
		return gVLCMediaPlayer->GetVideoHeight();
	}
	else
	{
		return 0;
	}
}

// Returns duration of video if known (otherwise -1)
extern "C" int64_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_GetDuration()
{
	if (gVLCMediaPlayer != nullptr)
	{
		return gVLCMediaPlayer->GetDuration();
	}
	else
	{
		return 0l;
	}
}


// Returns number of audio channels in current media
extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_GetNumAudioChannels()
{
	if (gVLCMediaPlayer != nullptr)
	{
		return gVLCMediaPlayer->GetNumAudioChannels();
	}
	else
	{
		return 0;
	}
}

// Returns whether a specific channel is stereo (or not)
extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_IsAudioChannelStereo(int channel)
{
	if (gVLCMediaPlayer != nullptr)
	{
		return gVLCMediaPlayer->IsAudioChannelStereo(channel);
	}
	else
	{
		return false;
	}
}

// Returns channel frequency in Hz
extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_ChannelFrequency(int channel)
{
	if (gVLCMediaPlayer != nullptr)
	{
		return gVLCMediaPlayer->ChannelFrequency(channel);
	}
	else
	{
		return 0;
	}
}

// Fills the specified buffer the available audio data for the channel up to maximum buffer length
// Return is number of floats available after number returned. floatsCopied indicates how many were returned
// if floatsCopied < maxLength then return value should always be zero
extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_RetrievAudioData(int channel, float* buffer, int maxLength, int* floatsCopied)
{
	if (gVLCMediaPlayer != nullptr)
	{
		return gVLCMediaPlayer->RetrieveAudioData(channel, buffer, maxLength, floatsCopied);
	}
	else
	{
		return 0;
	}
}

// If returns true then retrieves next event, otherwise returns false and mpEvent unchanged
extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_GetMediaEvent(eMPEvent* mpEvent, int64_t* param)
{
	if (gVLCMediaPlayer != nullptr)
	{
		return gVLCMediaPlayer->GetMediaEvent(mpEvent, param);
	}
	else
	{
		return false;
	}
}

// Having specified data source and surface, this function gets ready to play. Once
// play starts we should have valid info about the video (readable/playable, width, height
// and possibly duration).
extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_PrepareAsync()
{
	if (gVLCMediaPlayer != nullptr)
	{
		return gVLCMediaPlayer->PrepareAsync();
	}
	else
	{
		return false;
	}
}

// Call once per frame from the thread to complete updates.
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_Update()
{
	if (gVLCMediaPlayer != nullptr)
	{
		gVLCMediaPlayer->Update();
	}
}

// Call render function to update texture with latest video frame
static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
	if (gVLCMediaPlayer != nullptr)
	{
		gVLCMediaPlayer->Render();
	}
}

// GetRenderEventFunc, an example function we export which is used to get a rendering event callback function.
extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_GetRenderEventFunc()
{
	return OnRenderEvent;
}

// Start playing from current position (if immediately after Prepare then from beginning)
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_Play()
{
	if (gVLCMediaPlayer != nullptr)
	{
		gVLCMediaPlayer->Play();
	}
}

// Pause video at current position
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_Pause()
{
	if (gVLCMediaPlayer != nullptr)
	{
		gVLCMediaPlayer->Pause();
	}
}

// Retrieve current playback position
extern "C" int64_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_GetCurrentPosition()
{
	if (gVLCMediaPlayer != nullptr)
	{
		return gVLCMediaPlayer->GetCurrentPosition();
	}
	else
	{
		return 0;
	}
}

// Seek to specified position (if seekable)
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API VLCMP_SeekTo(int64_t pos)
{
	if(gVLCMediaPlayer != nullptr)
	{
		gVLCMediaPlayer->SeekTo(pos);
	}
}

// End of LibCVLCWrapper.cpp