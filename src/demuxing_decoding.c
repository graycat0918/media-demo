/**
 * @file demuxing_decoding.c
 * show how to use the libavformat and libavcodec API to demux and
 * decode audio and video data
 *
 * @author  duruyao
 * @version 1.0  19-12-20
 * @update  [id] [yy-mm-dd] [author] [description] 
 */

#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>

#include <libavformat/avformat.h>

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext  *video_dec_ctx = NULL;
static AVCodecContext  *audio_dec_ctx = NULL;

static const char *src_filename = NULL;
static const char *video_dst_filename = NULL;
static const char *audio_dst_filename = NULL;
static FILE *video_dst_file = NULL;
static FILE *audio_dst_file = NULL;

static int  width, height;
static int  video_stream_idx = -1;
static int  audio_stream_idx = -1;
static enum AVPixelFormat pix_fmt;
static AVStream *video_stream = NULL;
static AVStream *audio_stream = NULL;

static uint8_t *video_dst_data[4] = {NULL};
static int      video_dst_linesize[4];
static int      video_dst_bufsize;

static AVPacket pkt; /* sizeof(AVPacket) is public ABI */
static AVFrame *frame = NULL;
/* Enable or disable frame reference counting. You are not supposed to 
 * support both paths in your application but pick the one most appropriate
 * to your needs. Look for the use of refcount in this example to see what
 * are the differences of API usage between them. */
static int refcount = 0;
static int video_frame_count = 0;
static int audio_frame_count = 0;

static int decode_packet(int);
static int open_codec_context(int *, AVCodecContext **,
                              AVFormatContext *, enum AVMediaType);
static int get_format_from_sample_fmt(const char **, enum AVSampleFormat);

int main(int argc, char **argv) {
    int ret = 0;

    if (argc != 4 && argc != 5) {
        fprintf(stderr, 
                "Usage:\n"
                "%s [-refcount] <infile> <video outfile> <audio outfile>\n\n"
                "API example program to show how to read frames from an \n"
                "input file.\n\n"
                "This program reads frames from a file, decodes them, and \n"
                "writes decoded video frames to a raw video file named \n" 
                "'video outfile', and decoded audio frames to a raw audio \n"
                "file named 'audio outfile'.\n\n"
                "If the -refcount option is specified, the program use the \n"
                "reference counting frame system which allows keeping a \n"
                "copy of the data for longer than one decode call.\n",
                argv[0]);
        ret = 1;
        goto end;
    }
    if (argc == 5 && !strcmp(argv[1], "-refcount")) {
        refcount = 1;
        argv++;
    }
    src_filename       = argv[1];
    video_dst_filename = argv[2];
    audio_dst_filename = argv[3];

/*

demuxing & decoding
 ____________________________________________________________
| avcodec_register_all();                                    |
|                                                            |
| - Register all the codecs, parsers and bitstream filters.  |
|____________________________________________________________|
| avformat_open_input(&fmt_ctx, src_filename, NULL, NULL);   | 
|                                                            |
| - Allocate AVFormatContext.                                |
| - Open an input stream.                                    |
|____________________________________________________________|
| avformat_find_stream_info(fmt_ctx, NULL);                  |
|                                                            |
| - Get stream information.                                  |
|____________________________________________________________|
                              |
                              |
 _____________________________v______________________________
| open_codec_context(&video_stream_idx, &video_dec_ctx,      |
|                    fmt_ctx, AVMEDIA_TYPE_VIDEO);           |
|                                                            |
| open_codec_context(&audio_stream_idx, &audio_dec_ctx,      |
|                    fmt_ctx, AVMEDIA_TYPE_AUDIO);           |
|    ________________________________________________________|___
|   | av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);       |
|   |                                                            |
|   | - Get AVPacket::stream_index.                              |
|   | - Get AVStream.                                            |
|   |____________________________________________________________|
|   | avcodec_find_decoder(st->codecpar->codec_id);              |
|   |                                                            |
|   | - Find AVCodec.                                            |
|   |____________________________________________________________|
|   | avcodec_alloc_context3(dec);                               |
|   |                                                            |
|   | - Allocate AVCodecContext.                                 |
|   |____________________________________________________________|
|   | avcodec_parameters_to_context(*dec_ctx, st->codecpar);     |
|   |                                                            |
|   | - Fill AVCodecContext using parameters of AVCodec.         |
|   |____________________________________________________________|
|   | avcodec_open2(*dec_ctx, dec, &opts);                       |
|   |                                                            |
|   | - Open AVCodec with AVDictionary.                          |
|   |____________________________________________________________|
|                                                            |
| - Get AVPacket::width, AVPacket::height.                   |
| - Get AVPixelFormat.                                       |
|____________________________________________________________|
                              |
                              |
 _____________________________v______________________________
| fopen(video_dst_filename, "wb");                           |
|                                                            |
| fopen(audio_dst_filename, "wb");                           |
|                                                            |
| - Open video / audio output stream.                        |
|____________________________________________________________|
| av_image_alloc(video_dst_data, video_dst_linesize,         |
|                width, height, pix_fmt, 1);                 |
|                                                            |
| - Allocate image buffer.                                   |
|     - Allocate AVFrame::data.                              |
|     - Allocate AVFrame::linesize.                          |
|____________________________________________________________|
| av_frame_alloc();                                          |
|                                                            |
| - Allocate AVFrame.                                        |
|____________________________________________________________|
| av_init_packet(&pkt);                                      |
|                                                            |
| - Initialize some fields of AVPacket with default values.  |
|____________________________________________________________|
                              |
                              |
 _____________________________v______________________________
| av_read_frame(fmt_ctx, &pkt);                              |
|                                                            |
| - Get next encoded AVPacket from AVFormatContext::Stream.  |
|____________________________________________________________|
| decode_packet(0);                                          |
|    ________________________________________________________|___
|   | avcodec_send_packet(video_dec_ctx, &pkt);                  |
|   |                                                            | 
|   | avcodec_send_packet(audio_dec_ctx, &pkt);                  |
|   |                                                            |
|   | - Send encoded AVPacket to AVCodec.                        | 
|   |____________________________________________________________|
|   | avcodec_receive_frame(video_dec_ctx, frame);               |
|   |                                                            | 
|   | avcodec_receive_frame(video_dec_ctx, frame);               |
|   |                                                            |
|   | - Receive decoded AVFrame from AVCodec.                    | 
|   |____________________________________________________________|
|   | av_image_copy(video_dst_data, video_dst_linesize,          | 
|   |               (const uint8_t **)(frame->data),             |
|   |               frame->linesize, pix_fmt, width, height);    | 
|   |                                                            |
|   | - Copy AVFrame to image buffer.                            |
|   |____________________________________________________________|
|   | fwrite(video_dst_data[0], 1,                               |
|   |        video_dst_bufsize, video_dst_file);                 |
|   |                                                            |
|   | fwrite(frame->extended_data[0], 1,                         |
|   |        unpadded_linesize, audio_dst_file);                 |
|   |                                                            |
|   | - Write decoded AVFrame to video/audio output stream.      |
|   |____________________________________________________________|
|                                                            | 
|____________________________________________________________|
                              |
                              |
                              v

*/
    
    avcodec_register_all();

    /* open input file, and allocate format context (fmt_ctx point to NULL,
     * in which case an AVFormatContext is allocated by this function) */
    if ((ret = avformat_open_input(&fmt_ctx,
                                   src_filename, NULL, NULL)) < 0) {
        fprintf(stderr,
                "Could not open source file '%s' (%s)\n",
                src_filename, av_err2str(ret));
        goto end;
    }

    /* retrieve stream information */
    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        fprintf(stderr,
                "Could not find stream information (%s)\n",
                av_err2str(ret));
        goto end;
    }

    if (open_codec_context(&video_stream_idx,
                           &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
        video_stream = fmt_ctx->streams[video_stream_idx];
        video_dst_file = fopen(video_dst_filename, "wb");
        if (!video_dst_file) {
            fprintf(stderr, 
                    "Could not open destination file '%s'\n",
                    video_dst_filename);
            ret = 1;
            goto end;
        }

        /* allocate image where the decoded image will be put */
        width = video_dec_ctx->width;
        height = video_dec_ctx->height;
        pix_fmt = video_dec_ctx->pix_fmt;
        /* the allocated image buffer has to be freed by using
         * av_freep(&video_dst_data[0]) */
        ret = av_image_alloc(video_dst_data, video_dst_linesize,
                             width, height, pix_fmt, 1);
        if (ret < 0) {
            fprintf(stderr,
                    "Could not allocate raw video buffer (%s)\n",
                    av_err2str(ret));
            goto end;
        }
        video_dst_bufsize = ret;
    }

    if (open_codec_context(&audio_stream_idx,
                           &audio_dec_ctx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
        audio_stream = fmt_ctx->streams[audio_stream_idx];
        audio_dst_file = fopen(audio_dst_filename, "wb");
        if (!audio_dst_file) {
            fprintf(stderr, 
                    "Could not open destination file '%s'\n",
                    audio_dst_filename);
            ret = 1;
            goto end;
        }
    }

    /* dump input information to stderr */
    av_dump_format(fmt_ctx, 0, src_filename, 0);

    if (!audio_stream && !video_stream) {
        fprintf(stderr, 
                "Could not find audio or video "
                "stream in the input, aborting\n");
        ret = 1;
        goto end;
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate frame\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* initialize packet, set data to NULL, let the demuxer fill it */
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    if (video_stream)
        fprintf(stdout, 
                "Demuxing video from file '%s' into '%s'\n", 
                src_filename, video_dst_filename);
    if (audio_stream)
        fprintf(stdout, 
                "Demuxing audio from file '%s' into '%s'\n", 
                src_filename, audio_dst_filename);

    /* read frames (encoded packets) from the file */
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        AVPacket orig_pkt = pkt; /* WHY do it ?? SHOW reference counting ?? */
        do {
            ret = decode_packet(0);
            if (ret < 0)
                break;
            pkt.data += ret;
            pkt.size -= ret;
        } while (pkt.size > 0);
        /* unreference the buffer referenced by the packet and reset the
         * remaining packet fields to their default values */
        if (refcount)
            av_packet_unref(&orig_pkt);
    }

    /* flush cached frames */
    pkt.data = NULL;
    pkt.size = 0;
    ret = decode_packet(1);
    if (ret < 0) {
        fprintf(stderr, "Error sending the first flush packet\n");
        goto end;
    }

    fprintf(stdout, "Demuxing succeeded\n");

    if (video_stream) {
        fprintf(stdout,
                "Play the output video file with the command:\n"
                "ffplay -f rawvideo -pix_fmt %s -video_size %dx%d %s\n",
                av_get_pix_fmt_name(pix_fmt), width, height,
                video_dst_filename);
    }

    if (audio_stream) {
        enum AVSampleFormat sfmt = audio_dec_ctx->sample_fmt;
        int n_channels = audio_dec_ctx->channels;
        const char *fmt;

        if (av_sample_fmt_is_planar(sfmt)) {
            const char *packed = av_get_sample_fmt_name(sfmt);
            fprintf(stdout, 
                    "Warning: the sample format the decoder produced "
                    "is planar (%s).\n"
                    "This example will output the first channel only.\n",
                    packed ? packed : "?");
            sfmt = av_get_packed_sample_fmt(sfmt);
            n_channels = 1;
        }

        if ((ret = get_format_from_sample_fmt(&fmt, sfmt)) < 0)
            goto end;

        fprintf(stdout, 
                "Play the output audio file with the command:\n"
                "ffplay -f %s -ac %d -ar %d %s\n",
                fmt, n_channels, audio_dec_ctx->sample_rate,
                audio_dst_filename);
    }

end:
    avcodec_free_context(&video_dec_ctx);
    avcodec_free_context(&audio_dec_ctx);
    avformat_close_input(&fmt_ctx);
    if (video_dst_file) fclose(video_dst_file);
    if (audio_dst_file) fclose(audio_dst_file);
    av_frame_free(&frame);
    av_free(video_dst_data[0]);

    return (ret != 0);
}

static int decode_packet(int cached) {
    int ret = 0;
    // int got_frame;
    int decoded = pkt.size;

    if (pkt.stream_index == video_stream_idx) {

        /* WARNING: 'avcodec_decode_video2()' is deprecated, if enable the
         * reference counting, the caller must release the frame using
         * av_frame_unref() */

        // ret = avcodec_decode_video2(video_dec_ctx,
        //                             frame, &got_frame, &pkt);
        
        ret = avcodec_send_packet(video_dec_ctx, &pkt);
        if (ret < 0) {
            fprintf(stderr, 
                    "Error sending a video packet for decoding (%s)\n",
                    av_err2str(ret));
            return ret;
        } 
        
        while (ret >= 0) {
            ret = avcodec_receive_frame(video_dec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                return 0;
            } else if (ret < 0) {
                fprintf(stderr,
                        "Error during decoding video frames (%s)\n",
                        av_err2str(ret));
                return ret;
            }

            if (frame->width  != width  ||
                frame->height != height || frame->format != pix_fmt) {
                /* To handle this change, one could call av_image_alloc
                 * again and decode the following frames into another
                 * rawvideo file. */
                fprintf(stderr, 
                        "Error: width, height and pixel format have to be "
                        "constant in a rawvideo file, but the width, height "
                        "or pixel format of the input video changed:\n"
                        "old: width = %d, height = %d, format = %s\n"
                        "new: width = %d, height = %d, format = %s\n",
                        width, height, av_get_pix_fmt_name(pix_fmt),
                        frame->width, frame->height,
                        av_get_pix_fmt_name(frame->format));
                return -1;
            }

            fprintf(stdout, 
                    "video_frame%s n:%d coded_n:%d\n",
                    cached ? "(cached)" : "", video_frame_count++, 
                    frame->coded_picture_number);

            /* copy decoded frame to destination buffer:
             * this is required since rawvideo expects non aligned data */
            
            /* (Does the FUNC convert PACKED FMT to PLANAR FMT ???) */
            av_image_copy(video_dst_data, video_dst_linesize,
                          (const uint8_t **)(frame->data),
                          frame->linesize, pix_fmt, width, height);

            /* write to rawvideo file */
            fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
        }
    } else if (pkt.stream_index == audio_stream_idx) {

        /* WARNING: 'avcodec_decode_audio4()' is deprecated, if enable the
         * reference counting, the caller must release the frame using
         * av_frame_unref() */

        // ret = avcodec_decode_audio4(audio_dec_ctx,
        //                             frame, &got_frame, &pkt); */

        ret = avcodec_send_packet(audio_dec_ctx, &pkt);
        if (ret < 0) {
            fprintf(stderr, 
                    "Error sending a audio packet for decoding (%s)\n",
                    av_err2str(ret));
            return ret;
        }

        /* Some audio decoders decode only part of the packet, and have to be
         * called again with the remainder of the packet data.
         * Sample: fate-suite/lossless-audio/luckynight-partial.shn
         * Also, some decoders might over-read the packet. */
        decoded = FFMIN(ret, pkt.size);

        while (ret >= 0) {
            ret = avcodec_receive_frame(audio_dec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                return 0;
            } else if (ret < 0) {
                fprintf(stderr,
                        "Error during decoding audio frames (%s)\n",
                        av_err2str(ret));
                return ret;
            }

            size_t unpadded_linesize = av_get_bytes_per_sample(frame->format) \
                                       * frame->nb_samples; 
            fprintf(stdout, 
                    "audio_frame%s n:%d nb_samples:%d pts:%s\n",
                    cached ? "(cached)" : "", 
                    audio_frame_count++, frame->nb_samples, 
                    av_ts2timestr(frame->pts, &audio_dec_ctx->time_base));

            /* Write the raw audio data samples of the first plane.
             * This works fine for packed formats (e.g. AV_SAMPLE_FMT_S16).
             * However, most audio decoders output planar audio, which uses
             * a separate plane of audio samples for each channel (e.g.
             * AV_SAMPLE_FMT_S16P).
             * In other words, this code will write only the first audio channel
             * in these cases.
             * You should use libswresample or libavfilter to convert the frame
             * to packed data. */

            /* (How to convert PLANAR FMT data to PACKED FMT data ???) */
            fwrite(frame->extended_data[0],
                   1, unpadded_linesize, audio_dst_file);
        }
    }

    /* If we use frame reference counting, we own the data and need to 
     * de-reference it when we don't use it anymore.
     * The function av_frame_unref() unreference all the buffers referenced
     * by frame and reset the frame fields */

    // if (refcount)
    //     av_frame_unref(frame);

    return decoded;
}

static int open_codec_context(int *stream_idx,
                              AVCodecContext **dec_ctx, 
                              AVFormatContext *fmt_ctx,
                              enum AVMediaType type) {
    int ret;
    int stream_index;
    AVStream *st;
    AVCodec  *dec = NULL;
    AVDictionary *opts = NULL;

    if ((ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0)) < 0) {
        fprintf(stderr, 
                "Could not find %s stream in input file '%s' (%s)\n",
                av_get_media_type_string(type), src_filename, av_err2str(ret));
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];

        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, 
                    "Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }

        /* allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            fprintf(stderr, 
                    "Failed to allocate the %s codec context\n",
                    av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }

        /* copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*dec_ctx,
                                                 st->codecpar)) < 0) {
            fprintf(stderr, 
                    "Failed to copy %s codec parameters to decoder context\n",
                    av_get_media_type_string(type));
            return ret;
        }

        /* init the decoders, with or without reference counting */
        av_dict_set(&opts, "refcounted_frames", refcount ? "1" : "0", 0);
        if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0) {
            fprintf(stderr, 
                    "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }

    return 0;
}

static int get_format_from_sample_fmt(const char **fmt,
                                      enum AVSampleFormat sample_fmt) {
    int i;
    struct sample_fmt_entry {
        enum AVSampleFormat sample_fmt;
        const char *fmt_be, *fmt_le;
    } sample_fmt_entries[] = {
        { AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
        { AV_SAMPLE_FMT_S16, "s16be", "s16le" },
        { AV_SAMPLE_FMT_S32, "s32be", "s32le" },
        { AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
        { AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
    };
    *fmt = NULL;

    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entry *entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt) {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }

    fprintf(stderr,
            "sample format %s is not supported as output format\n",
            av_get_sample_fmt_name(sample_fmt));
    return -1;
}

