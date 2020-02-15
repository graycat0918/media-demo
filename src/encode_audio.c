/**
 * @file [filename] 
 * audio encoding with libavcodec API example
 *
 * @author  duruyao
 * @version 1.0  20-01-03
 * @update  [id] [yy-mm-dd] [author] [description] 
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <libavutil/frame.h>
#include <libavutil/common.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>

#include <libavcodec/avcodec.h>

static int  check_sample_fmt(const AVCodec *, enum AVSampleFormat);

static int  select_sample_rate(const AVCodec *);

static int  select_channel_layout(const AVCodec *);

static int  encode(AVCodecContext *, AVFrame *, AVPacket *, FILE *);

int main(int argc, char **argv) {
    const char *filename = NULL;
    FILE *fd = NULL;
    const AVCodec  *codec     = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVFrame  *frame = NULL;
    AVPacket *pkt   = NULL;
    int ret = 0;
    float tone;
    float tincr;
    uint16_t *samples;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <output file>\n", argv[0]);
        return 0;
    }
    filename = argv[1];

    avcodec_register_all();

    /* find the MP2 encoder */
    codec = avcodec_find_encoder(AV_CODEC_ID_MP2);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        ret = 1;
        goto end;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        ret = 1;
        goto end;
    }

    /* put sample parameters */
    codec_ctx->bit_rate = 64000;

    /* check that the encoder supports s16 pcm input */
    codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    if (!check_sample_fmt(codec, codec_ctx->sample_fmt)) {
        fprintf(stderr,
                "Encoder does not support sample format %s\n",
                av_get_sample_fmt_name(codec_ctx->sample_fmt));
        ret = 1;
        goto end;
    }

    /* select other audio parameters supported by the encoder */
    codec_ctx->sample_rate    = select_sample_rate(codec);
    codec_ctx->channel_layout = select_channel_layout(codec);
    codec_ctx->channels       = av_get_channel_layout_nb_channels(
                                codec_ctx->channel_layout);

    /* open it */
    if ((ret = avcodec_open2(codec_ctx, codec, NULL)) < 0) {
        fprintf(stderr, "Could not open codec %s\n", codec->name);
        goto end;
    }

    fd = fopen(filename, "wb");
    if (!fd) {
        fprintf(stderr, "Could not open '%s'\n", filename);
        ret = 1;
        goto end;
    }

    /* packet for holding encoded output */
    pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "Could not allocate packet\n");
        ret = 1;
        goto end;
    }

    /* frame containing input raw audio */
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate audio frame\n");
        ret = 1;
        goto end;
    }

    frame->nb_samples     = codec_ctx->frame_size;
    frame->format         = codec_ctx->sample_fmt;
    frame->channel_layout = codec_ctx->channel_layout;

    /* allocate the data buffer(s) */
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        fprintf(stderr,
                "Could not allocate audio data buffer(s) (%s)\n",
                av_err2str(ret));
        goto end;
    }

    /* encode a single tone sound */
    tone = 0;
    tincr = 2 * M_PI * 440.0 / codec_ctx->sample_rate;
    for (int i = 0; i < 200; i++) {
        /* make sure the frame is writable, allocate new buffers
         * and copy the data if it is not */
        ret = av_frame_make_writable(frame);
        if (ret < 0) {
            fprintf(stderr,
                    "Error checking frame is writable (%s)\n",
                    av_err2str(ret));
            goto end;
        }
        samples = (uint16_t*)frame->data[0];

        for (int j = 0; j < codec_ctx->frame_size; j++) {
            samples[2 * j] = (int)(sin(tone) * 10000);

            for (int k = 1; k < codec_ctx->channels; k++)
                samples[2 * j + k] = samples[2 * j];
            tone += tincr;
        }
        
        if ((ret = encode(codec_ctx, frame, pkt, fd)) < 0)
            goto end;
    }

    /* flush the encoder */
    if ((ret = encode(codec_ctx, NULL, pkt, fd)) < 0)
        goto end;

end:
    if (fd) fclose(fd);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);

    return (ret != 0);
}

/** 
 * check that a given sample format is supported by the encoder
 */

static int check_sample_fmt(const AVCodec *codec,
                            enum AVSampleFormat sample_fmt) {
    const enum AVSampleFormat *p = codec->sample_fmts;

    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sample_fmt)
            return 1;
        p++;
    }
    return 0;
}

/**
 * just pick the highest supported samplerate
 */

static int select_sample_rate(const AVCodec *codec) {
    const int *p;
    int best_samplerate = 0;

    if ((p = codec->supported_samplerates) == NULL)
        return 44100;

    while (*p) {
        /* WHY the best sample rate is closer to 44.1 KHz ?? */
        if (!best_samplerate ||
            abs(44100 - *p)  < abs(44100 - best_samplerate))
            best_samplerate = *p;
        p++;
    }
    return best_samplerate;
}

/**
 * select layout with the highest channel count
 */

static int select_channel_layout(const AVCodec *codec) {
    const uint64_t *p;
    int      nb_channels      = 0;
    int      best_nb_channels = 0;
    uint64_t best_ch_layout   = 0;

    if ((p = codec->channel_layouts) == NULL)
        return AV_CH_LAYOUT_STEREO;

    while (*p) {
        nb_channels = av_get_channel_layout_nb_channels(*p);

        if (nb_channels > best_nb_channels) {
            best_ch_layout   = *p;
            best_nb_channels =  nb_channels;
        }
        p++;
    }
    return best_ch_layout;
}

static int encode(AVCodecContext *ctx,
                  AVFrame *frame, AVPacket *pkt, FILE *output) {
    int ret;

    /* send the frame for encoding */
    ret = avcodec_send_frame(ctx, frame);
    if (ret < 0) {
        fprintf(stderr,
                "Error sending the frame to the encoder (%s)\n",
                av_err2str(ret));
        return ret;
    }

    /* read all the available output packets (in general there may be
     * any number of them) */
    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        else if (ret < 0) {
            fprintf(stderr,
                    "Error encoding audio frame (%s)\n",
                    av_err2str(ret));
            return ret;
        }

        fwrite(pkt->data, 1, pkt->size, output);

        /* WHY unrference counting ?? 
         * Just to wipe the packet (reset the remaining packet
         * fields to their default values) ?? */
        av_packet_unref(pkt);
    }

    return 0;
}

