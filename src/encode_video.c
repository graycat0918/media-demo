/**
 * @file [filename] 
 * video encoding with libavcodec API example
 *
 * @author  duruyao
 * @version 1.0  20-02-10
 * @update  [id] [yy-mm-dd] [author] [description] 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include <libavutil/opt.h>
#include <libavutil/imgutils.h>

#include <libavcodec/avcodec.h>

static int encode(AVCodecContext *, AVFrame *, AVPacket *, FILE *);

int main(int argc, char **argv) {
    const char *filename   = NULL;
    const char *codec_name = NULL;
    const AVCodec  *codec     = NULL;
    AVCodecContext *codec_ctx = NULL;
    int  ret = 0;
    FILE *fd = NULL;
    AVPacket *pkt      = NULL;
    AVFrame  *frame    = NULL;
    uint8_t  endcode[] = {0, 0, 1, 0xb7};

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <output file> <codec name>\n", argv[0]);
        exit(0);
    }
    filename   = argv[1];
    codec_name = argv[2];

    avcodec_register_all();

    /* find the mpeg1video encoder */
    codec = avcodec_find_encoder_by_name(codec_name);
    if (!codec) {
        fprintf(stderr, "Codec '%s' not found\n", codec_name);
        ret = 1;
        goto end;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        ret = 1;
        goto end;
    }

    codec_ctx->width     = 352;
    codec_ctx->height    = 288;
    codec_ctx->bit_rate  = 400000;
    codec_ctx->time_base = (AVRational){1, 25};
    codec_ctx->framerate = (AVRational){25, 1};

    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size */
    codec_ctx->gop_size     = 10;
    codec_ctx->max_b_frames = 1;
    codec_ctx->pix_fmt      = AV_PIX_FMT_YUV420P;

    if (codec->id == AV_CODEC_ID_H264)
        av_opt_set(codec_ctx->priv_data, "preset", "slow", 0);

    /* open it */
    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open codec %s\n", codec->name);
        goto end;
    }

    fd = fopen(filename, "wb");
    if (!fd) {
        fprintf(stderr, "Could not open '%s'\n", filename);
        ret = 1;
        goto end;
    }

    pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "Could not allocate packet\n");
        ret = 1;
        goto end;
    }

    /* set sample parameters, resolution must be a multiple of two */
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        ret = 1;
        goto end;
    }
    frame->format = codec_ctx->pix_fmt;
    frame->width  = codec_ctx->width;
    frame->height = codec_ctx->height;

    ret = av_frame_get_buffer(frame, 32);
    if (ret < 0) {
        fprintf(stderr,
                "Could not allocate video frame buffer(s) (%s)\n",
                av_err2str(ret));
        goto end;
    }

    /* encode 1 second of video */
    for (int i = 0; i < 25; i++) {
        fflush(stdout);

        /* make sure the frame data is writable */
        ret = av_frame_make_writable(frame);
        if (ret < 0) {
            fprintf(stderr,
                    "Error checking frame is writable (%s)\n",
                    av_err2str(ret));
        }

        /* prepare a dummy image */
        /* Y */
        for (int y = 0; y < codec_ctx->height; y++) {
            for (int x = 0; x < codec_ctx->width; x++) {
                frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
            }
        }

        /* Cb and Cr */
        for (int y = 0; y < codec_ctx->height / 2; y++) {
            for (int x = 0; x < codec_ctx->width / 2; x++) {
                frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
            }
        }

        frame->pts = i;

        /* encode the image */
        if ((ret = encode(codec_ctx, frame, pkt, fd)) < 0)
            goto end;
    }

    /* flush the encoder */
    if ((ret = encode(codec_ctx, NULL, pkt, fd)) < 0) 
        goto end;

    /* add sequence end code to have a real MPEG file */
    if (codec->id == AV_CODEC_ID_MPEG1VIDEO ||
        codec->id == AV_CODEC_ID_MPEG2VIDEO)
        fwrite(endcode, 1, sizeof(endcode), fd);
end: 
    if (fd) fclose(fd);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);

    return (ret != 0);
}

static int encode(AVCodecContext *enc_ctx,
                  AVFrame *frame, AVPacket *pkt, FILE *outfile) {
    int ret = 0;

    /* send the frame to the encoder */
    if (frame)
        fprintf(stdout, "Send frame %3" PRId64 "\n", frame->pts);

    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        fprintf(stderr,
                "Error sending a frame for encoding (%s)\n",
                av_err2str(ret));
        return ret;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        else if (ret < 0) {
            fprintf(stderr,
                    "Error during encoding (%s)\n",
                    av_err2str(ret));
            return ret;
        }

        fprintf(stdout, 
                "Write packet %3" PRId64 " (size = %5d)\n",
                pkt->pts, pkt->size);
        fwrite(pkt->data, 1, pkt->size, outfile);

        /* WHY unrference counting ?? 
         * Just to wipe the packet (reset the remaining packet
         * fields to their default values) ?? */
        av_packet_unref(pkt);
    }

    return 0;
}

