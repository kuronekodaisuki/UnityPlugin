#pragma once

#include <queue>

#include "VLCMediaPlayer.h"

namespace FPVR
{
	class MediaEvents
	{
		void AddEvent(eMPEvents event);
		eMPEvent GetEvent()
	};
}
