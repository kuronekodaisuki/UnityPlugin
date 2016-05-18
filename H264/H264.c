// H264.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

/*
 * Copyright (c) 2001 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * libavcodec API use example.
 *
 * @example decoding_encoding.c
 * Note that libavcodec only handles codecs (mpeg, mpeg4, etc...),
 * not file formats (avi, vob, mp4, mov, mkv, mxf, flv, mpegts, mpegps, etc...). See library 'libavformat' for the
 * format handling
 */
#include <stdlib.h>
#include <windows.h>
#include <opencv2/core/core_c.h>
#include <opencv2/imgproc/types_c.h>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

#include <math.h>

#pragma warning(disable: 4996 4005)

#define inline

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libavformat/avformat.h>

#pragma comment(lib, "avcodec-55.lib")
#pragma comment(lib, "avutil-52.lib")
#pragma comment(lib, "avformat-55.lib")
//#pragma comment(lib, "avdevice-55.lib")


#ifdef _DEBUG
#pragma comment(lib, "opencv_core2410d.lib")
#pragma comment(lib, "opencv_highgui2410d.lib")
#pragma comment(lib, "opencv_imgproc2410d.lib")
#else
#pragma comment(lib, "opencv_core2410.lib")
#pragma comment(lib, "opencv_highgui2410.lib")
#pragma comment(lib, "opencv_imgproc2410.lib")
#endif

//#define AV_FORMAT
#define WIDTH	640
#define HEIGHT	480
#define BITRATE	400000
#define GOP 30

// read image and prepare to write(resize and convert to YUV420)
int ReadImage(AVCodecContext *context, AVFrame *frame, const char *filename)	
{
	int x, y;
	IplImage *image;
	IplImage *scaled;
	IplImage *yuv;
	IplImage *img_planes[3] = { NULL, NULL, NULL, };
	
	image = cvLoadImage(filename, 1);
	
	// resize
	scaled = cvCreateImage(cvSize(WIDTH, HEIGHT), image->depth, image->nChannels);
	cvResize(image, scaled, CV_INTER_LINEAR);

	// change color space
	yuv = cvCreateImage(cvSize(context->width, context->height), image->depth, image->nChannels);
	cvCvtColor(scaled, yuv, CV_BGR2YCrCb);

	// separate channel
	img_planes[0] = cvCreateImage(cvSize(context->width, context->height), image->depth, 1);
	img_planes[1] = cvCreateImage(cvSize(context->width, context->height), image->depth, 1);
	img_planes[2] = cvCreateImage(cvSize(context->width, context->height), image->depth, 1);
	cvSplit(yuv, img_planes[0], img_planes[1], img_planes[2], NULL);
		
	// copy data
    for (y = 0; y < context->height; y++) {
		// Y
		for (x = 0; x < context->width; x++) {
			frame->data[0][y * frame->linesize[0] + x] = img_planes[0]->imageData[y * img_planes[0]->widthStep + x];
		}
    }
    for (y = 0; y < context->height / 2; y++) {
		// Cb
		// Cr
		for (x = 0; x < context->width / 2; x++) {
			frame->data[1][(y) * frame->linesize[1] + (x)] = img_planes[2]->imageData[(y * 2) * img_planes[2]->widthStep + (x * 2)];
			frame->data[2][(y) * frame->linesize[2] + (x)] = img_planes[1]->imageData[(y * 2) * img_planes[1]->widthStep + (x * 2)];
		}
	}

	// release images
	cvReleaseImage(&image);
	cvReleaseImage(&scaled);
	cvReleaseImage(&yuv);
	cvReleaseImage(&img_planes[0]);
	cvReleaseImage(&img_planes[1]);
	cvReleaseImage(&img_planes[2]);
	return 0;
}

/*
 * Video encode
 */
static void video_encode(const char *filename, enum AVCodecID codec_id, const char *input_file)
{
    AVCodec *codec;
    AVCodecContext *context= NULL;
    int pts, ret, got_output;
    AVFrame *frame;
    AVPacket packet;
	AVFormatContext *format;
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };
	AVRational timebase;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA ffd;
	AVOutputFormat *oformat;
#ifndef AV_FORMAT
	FILE *file;
#else
	AVStream *stream;
#endif
	char input_path[MAX_PATH] = "..\\Archive\\";
	WCHAR buffer[MAX_PATH] = L"..\\Archive\\*.png";
	size_t wlen;

	av_register_all();
	avcodec_register_all();

	oformat = av_guess_format(NULL, filename, NULL);
	if (oformat == NULL)
	{
		oformat = av_guess_format("mp4", NULL, NULL);
	}
	
    printf("Encode video file %s\n", filename);

    /* find the mpeg1 video encoder */
    codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }

    context = avcodec_alloc_context3(codec);
    if (!context) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }

    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
	timebase.den = GOP;
	timebase.num = 15;

    context->time_base = timebase;    /* frames per second */
	context->codec = codec;
    context->gop_size = GOP;
    context->max_b_frames = 1;
    context->pix_fmt = AV_PIX_FMT_YUV420P;
	context->bit_rate = BITRATE;	/* put sample parameters */
    context->width = WIDTH;	/* resolution must be a multiple of two */
    context->height = HEIGHT;
	//context->flags |= CODEC_FLAG_GLOBAL_HEADER;

	format = avformat_alloc_context();
	format->oformat = oformat;
	format->video_codec = codec;
	format->video_codec_id = codec_id;

    if (codec_id == AV_CODEC_ID_H264)
        av_opt_set(context->priv_data, "preset", "slow", 0);

    /* open it */
    if (avcodec_open2(context, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

#ifndef AV_FORMAT
    file = fopen(filename, "ab+");
    if (!file) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }
#else
	stream = avformat_new_stream(format, codec);
	stream->codec->pix_fmt = AV_PIX_FMT_YUV420P;
	stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	stream->codec->width = WIDTH;
	stream->codec->height = HEIGHT;
	stream->codec->gop_size = GOP;
	stream->codec->bit_rate = BITRATE;
	stream->codec->time_base = timebase;	// set timebase here
	avio_open(&format->pb, filename, AVIO_FLAG_WRITE);
	avformat_write_header(format, NULL);
#endif

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
    frame->format = context->pix_fmt;
    frame->width  = context->width;
    frame->height = context->height;
	frame->pts = 0;
	//frame->pict_type = AV_PICTURE_TYPE_I;	// intra frame

    /* the image can be allocated by any means and av_image_alloc() is
     * just the most convenient way if av_malloc() is to be used */
    ret = av_image_alloc(frame->data, frame->linesize, context->width, context->height,
                         context->pix_fmt, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate raw picture buffer\n");
        exit(1);
    }

	// insert PNG images
	if (input_file != NULL)
	{
		char drive[MAX_PATH], dir[MAX_PATH];
		mbstowcs_s(&wlen, buffer, MAX_PATH, input_file, MAX_PATH);
		_splitpath(input_file, drive, dir, NULL, NULL);	
		if (0 < strlen(drive))
		{
			sprintf(input_path, "%s:%s", drive, dir);
		}
		else
		{
			sprintf(input_path, "%s", dir);
		}
	}
	hFind = FindFirstFile(buffer, &ffd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		pts = 0;
		// TODO sort by filename ascend
		do {
			av_init_packet(&packet);
			packet.data = NULL;    // packet data will be allocated by the encoder
			packet.size = 0;
#ifndef AV_FORMAT
			fflush(file);
#endif
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
			}
			else 
			{
				char name[80], buffer[100];
				size_t len;

				wcstombs_s(&len, name, 80, ffd.cFileName, 80);
				sprintf(buffer, "%s%s", input_path, name);
				ReadImage(context, frame, buffer);
				frame->pts = pts;
				frame->key_frame = 1;
				/*
				if (pts % 10 == 5)
				{
					frame->pict_type = AV_PICTURE_TYPE_P;	// prediction frame
				}
				else 
				{
					frame->pict_type = AV_PICTURE_TYPE_I;	// intra frame
				}
				*/
				/* encode the image */
				ret = avcodec_encode_video2(context, &packet, frame, &got_output);
				if (ret < 0) {
					fprintf(stderr, "Error encoding frame\n");
					exit(1);
				}

				// write frame
				if (got_output) {
					//int numBytes = avpicture_get_size(context->pix_fmt, context->width, context->height);

					context->coded_frame->pts = pts;
					if (context->coded_frame->pts != (0x8000000000000000LL))
					{
					//	pts = av_rescale_q(context->coded_frame->pts, context->time_base, format->streams[0]->time_base);
					}
					packet.pts = pts;

					if (context->coded_frame->key_frame)
					{
						packet.flags |= AV_PKT_FLAG_KEY;
					}
					printf("Write frame %3d (size=%5d)\n", pts, packet.size);
#ifndef AV_FORMAT
					fwrite(packet.data, 1, packet.size, file);
#else
					av_interleaved_write_frame(format, &packet);
#endif
					av_free_packet(&packet);
				}
				pts++;
			}
		} while (FindNextFile(hFind, &ffd) != 0);
	}

    /* get the delayed frames */
    for (got_output = 1; got_output; pts++) {
		//av_init_packet(&packet);
#ifndef AV_FORMAT
		fflush(file);
#endif
        ret = avcodec_encode_video2(context, &packet, NULL, &got_output);
        if (ret < 0) {
            fprintf(stderr, "Error encoding frame\n");
            exit(1);
        }

        if (got_output) {
            printf("Write frame %3d (size=%5d)\n", pts, packet.size);
#ifndef AV_FORMAT
            fwrite(packet.data, 1, packet.size, file);
#else
			av_interleaved_write_frame(format, &packet);
#endif
            av_free_packet(&packet);
        }
    }

    /* add sequence end code to have a real mpeg file */
#ifndef AV_FORMAT
	fwrite(endcode, 1, sizeof(endcode), file);
    fclose(file);
#else
	av_write_trailer(format);
	avio_close(format->pb);
	avformat_free_context(format);
#endif

    avcodec_close(context);
    av_free(context);
    av_freep(&frame->data[0]);
    av_frame_free(&frame);
    printf("\n");
}

// get duration of movie file
int64_t getDuration(const char *filename)
{
	int64_t duration = -1;
	AVFormatContext* context = avformat_alloc_context();
	if (context != NULL)
	{
		avformat_open_input(&context, filename, NULL, NULL);
		if (context != NULL)
		{
			duration = context->duration;
			printf("%d, %d\n", duration, context->start_time);
		// etc
			avformat_close_input(&context);
			avformat_free_context(context);
		}
	}
	return duration;
}

/*
 reference of avformat functions to make movie file
 http://www.ffmpeg.org/doxygen/trunk/muxing_8c-example.html
*/
int main(int argc, char **argv)
{
    const char *output_file;
	int64_t duration;

    /* register all the codecs */
    avcodec_register_all();
	 /* Initialize libavcodec, and register all codecs and formats. */
    av_register_all();

    if (argc < 2) {
        printf("usage: %s output_type\n"
               "API example program to decode/encode a media stream with libavcodec.\n"
               "This program generates a synthetic stream and encodes it to a file\n"
               "named test.h264, test.mp2 or test.mpg depending on output_type.\n"
               "The encoded stream is then decoded and written to a raw data output.\n"
               "output_type must be chosen between 'h264', 'mp2', 'mpg'.\n",
               argv[0]);
        return 1;
    }
    output_file = argv[1];

	duration = getDuration(output_file);

    switch (argc)
	{case 3:
        video_encode(output_file, AV_CODEC_ID_H264, argv[2]);
		break;

	case 2:
		video_encode(output_file, AV_CODEC_ID_H264, "..\\Archive\\*.png");
		break;
    }

    return 0;
}

/*
 append image reference
 http://forum.doom9.org/showthread.php?t=170404

 I have tried many different ways to try to create a 10 second video file out of an image file 
 and have used all the same switches and codecs as I used to encode my video file. However, 
 when I concat the two using anything but complex_filter (which forces the video through another round of transcoding),
 the resulting video file is corrupt. 
 I believe this is due to the inherent differences of the 10 second clip that ffmpeg created from the image,
 but there must be some way to get it to encode the exact same way as my video file.

 Here is the command I am using to turn the image into a 10s video clip 
 (I added a silent mp3 because I thought that an audio stream starting partway through the video was messing things up):

Code:
 ffmpeg -loop 1 -i splash.jpg -i silence.mp3 -c:v libx264 -preset slow -g 60 -r 29.97 -crf 16 -c:a libfdk_aac -b:a 256k -cutoff 18000 -t 5 tmpoutput1.mp4

 Here is the command I am using to encode my video:

Code:
 ffmpeg -i input.f4v -c:v libx264 -preset slow -g 60 -r 29.97 -crf 16 -c:a libfdk_aac -b:a 256k -cutoff 18000 tmpoutput2.mp4

 Here is the command I use to convert both of them to .ts to get ready for concat:

Code:
 ffmpeg -i tmpoutput1.mp4 -c copy -bsf:v h264_mp4toannexb -f mpegts tmpoutput1.ts

 And finally the concat (which is where I get crazy video corruption, everything along the way looks fine):

Code:
 ffmpeg -i "concat:tmpoutput1.ts|tmpoutput2.ts" -c copy output.mp4

 Again, the issue is that I'm already transcoding everything once 
 and I should be able to get it to transcode in a similar enough structure so that it can be concatenated 
 without another transcode tacked onto the end.

 Has anyone successfully added a full-frame splash graphic to the front of a video with ffmpeg before? 
 I am using a brand new cross-compile of ffmpeg as I thought that might be the issue, but alas,
 the issue persists after the update.

Thanks!

someone answered:

 2) When using x264, encoding with --stitchable may improve your chances of successfully appending segments. 
    In ffmpeg libx264 it would be -x264-params stitchable


reply:
 Thanks so much for all of your helpful feedback! The answer was actually a very simple one. 
 I did not explicitly set the profile (main), level (3.1), and pix_fmt (yuv420p) in my command line. 
 This has never been a problem for me until trying to work with image files turned to video. 
 For these image videos, ffmpeg autodetects the parameters as a 4:4:4 Predictive L3.1 instead of 4:2:0 Main L3.1.
 I only found out when I tried to upload my merged videos to be streamed and it only played audio, 
 but trusty ol' VLC played it no problem while I was testing.

 After adding those 3 parameters to the command to generate the image video file,
 I was able to merge the pieces using the basic concat method and did not have to transcode to get everything put together.
*/