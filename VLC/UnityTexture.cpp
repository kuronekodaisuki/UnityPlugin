

namespace FPVR
{

	// TextureFormat.RGB24 = 3		"RV32"
	// TextureFormat.RGBA32 = 4		"RGBA"
	// TextureFormat.ARGB32 = 5		"ARGB"
	// TextureFormat.RGB565 = 7		"RV16"
	// TextureFormat.BGRA32 = 14	"BGRA"

	typedef struct _TextureFormat
	{
		int mUnityEnum;
		char*mFourCC;
	} sTextureFormat;

	sTextureFormat Formats[] = 
	{
		{3, "RV32", },
		{4, "RGBA", },
		{5, "RGBA", },
		{7, "RGBA", },
		{14, "RGBA", }
	};

}