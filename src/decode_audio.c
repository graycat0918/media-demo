/**
 * @file decode_audio.c 
 * audio decoding with libavcodec API example
 *
 * @author  duruyao
 * @version 1.0  19-09-19
 * @update  [id] [yy-mm-dd] [author] [description] 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/mem.h>
#include <libavutil/log.h>
#include <libavutil/frame.h>

#include <libavcodec/avcodec.h>

#define AUDIO_INBUF_SIZE    20480
#define AUDIO_REFILL_THRESH 4096

static int get_format_from_sample_fmt(const char **, enum AVSampleFormat);

static int decode(AVCodecContext *, AVPacket *, AVFrame *, FILE *);

int main(int argc, char **argv) {
    int len, ret;
    const char *infilename;
    const char *outfilename; 
    FILE *infile  = NULL;
    FILE *outfile = NULL;
    const AVCodec  *codec;
    AVCodecContext *codec_ctx = NULL;
    AVCodecParserContext *parser_ctx = NULL;
    uint8_t   inbuf[AUDIO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    uint8_t  *data;
    size_t    data_size;
    AVPacket *pkt;
    AVFrame  *decoded_frame = NULL;

/*

audio decoding
 ____________          ___________          _________________ 
|            |  read  |           | prase  |                 | decode  
| input file |------->| in buffer |------->| encoded packets |-------+
|____________|        |___________|        |_________________|    

     ________________          _____________ 
    |                | write  |             |
+-->| decoded frames |------->| output file |
    |________________|        |_____________|

*/

    if (argc <= 2) {
        fprintf(stderr, "Usage: %s <input file> <output file>\n"
                "And check your input file is encoded by AAC please.\n",
                argv[0]);
        exit(0);
    }
    infilename  = argv[1];
    outfilename = argv[2];

    avcodec_register_all();

    pkt = av_packet_alloc();
    if(!pkt) {
        fprintf(stderr, "Cannot allocate packet\n");
        exit(1);
    }

    /* find a registered decoder with a matching codec ID, such as the 
     * MPEG audio decoder */
    codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        goto end;
    }

    parser_ctx = av_parser_init(codec->id);
    if (!parser_ctx) {
        fprintf(stderr, "Parser not found\n");
        goto end;
    }

    codec_ctx = avcodec_alloc_context3(codec); 
    if (!codec_ctx) {
        fprintf(stderr, "Cannot allocate audio codec context\n");
        goto end;
    }

    /* open the audio decoder */
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Cannot open codec\n");
        goto end;
    }

    infile = fopen(infilename, "rb");
    if (!infile) {
        fprintf(stderr, "Cannot open %s\n", infilename);
        goto end;
    }
    outfile = fopen(outfilename, "wb");
    if (!outfile) {
        av_free(codec_ctx); /* av_freep() is more safer than av_free() */
        goto end;
    }

    /* read audio data */
    data      = inbuf;
    data_size = fread(inbuf, 1, AUDIO_INBUF_SIZE, infile);

    while (data_size > 0) {
        if (!decoded_frame) {
            if (!(decoded_frame = av_frame_alloc())) {
                fprintf(stderr, "Cannot allocate audio frame\n");
                exit(1);
            }
        }
       
        /* AV_NOPTS_VALUE, undefined timestamp value, usually reported by 
         * demuxer that work on containers that do not provide either pts
         * or dts */
        ret = av_parser_parse2(parser_ctx, codec_ctx, &pkt->data,
                               &pkt->size, data, data_size,
                               AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        if (ret < 0) {
            fprintf(stderr, "Error while parsing\n");
            exit(1);
        }
        data      += ret;
        data_size -= ret;

        if (pkt->size)
            if (decode(codec_ctx, pkt, decoded_frame, outfile) < 0)
                goto end;
        
        /* remaining undecoded size of data < 4096, refill from input to 
         * inbuf */
        if (data_size < AUDIO_REFILL_THRESH) {
            memmove(inbuf, data, data_size); /* is safer than memcpy() */
            data = inbuf;
            len = fread(data + data_size, 1,
                        AUDIO_INBUF_SIZE - data_size, infile);
            if (len > 0)
                data_size += len;
        }
    }

    /* flush the decoder */
    pkt->data = NULL;
    pkt->size = 0;
    if (decode(codec_ctx, pkt, decoded_frame, outfile) < 0)
        goto end;

    /* print output pcm info, because there have no metadata of pcm */
    enum AVSampleFormat sfmt = codec_ctx->sample_fmt;
    int n_channels = 0;
    const char *fmt;

    if (av_sample_fmt_is_planar(sfmt)) { /* packed data & planar data */
        const char *packed = av_get_sample_fmt_name(sfmt);
        fprintf(stdout, 
                "Warning: the sample format the decoder produced is planar "
                "(%s). This example will output the first channel only.\n",
                packed ? packed : "?");
        sfmt = av_get_packed_sample_fmt(sfmt);
    }

    n_channels = codec_ctx->channels;
    if ((ret = get_format_from_sample_fmt(&fmt, sfmt)) < 0)
        goto end;

    fprintf(stdout,
            "Play the output audio file with the command:\n"
            "ffplay -f %s -ac %d -ar %d %s\n",
            fmt, n_channels, codec_ctx->sample_rate, outfilename);
end:
    av_frame_free(&decoded_frame);
    if (outfile) fclose(outfile);                    
    if (infile) fclose(infile);
    avcodec_free_context(&codec_ctx);   
    av_parser_close(parser_ctx);
    av_packet_free(&pkt);         

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
            "Sample format %s is not supported as output format\n",
            av_get_sample_fmt_name(sample_fmt));
    return -1;
}

static int decode(AVCodecContext *dec_ctx,
                  AVPacket *pkt, AVFrame *frame, FILE *outfile) {
    int i, ch;
    int ret, data_size;

    /* send the packet with the compressed data to the decoder (an AVPacket 
     * with data set to NULL and size set to 0, it is considered a flush 
     * packet, which signals the end of the stream) */
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error submitting the packet to the decoder\n");
        fprintf(stderr, "%s\n", av_err2str(ret));
        return -1;
    }

    /* read all output frames (in general there may be any number of them) */
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        else if (ret < 0) {
            fprintf(stderr,
                    "Error during decoding (%s)\n",
                    av_err2str(ret));
            return -1;
        }
        data_size = av_get_bytes_per_sample(dec_ctx->sample_fmt);
        
        /* this should not occur, checking just for paranoia */
        if (data_size < 0) {    
            fprintf(stderr,
                    "Failed to calculate data size (%s)\n",
                    av_err2str(ret));
            return -1;
        }
        
        /* AVFrame::nb_samples is number of samples (frames), AVFrame of audio
         * may contain of multiple audio samples, one sample may contain of
         * multiple audio channels */

/*

uint8_t * AVFrame::data

(planar format)
         ______ ______ ______ __
data[0] |__C0__|__C0__|__C0__|__
         ______ ______ ______ __   
data[1] |__C1__|__C1__|__C1__|__
         ______ ______ ______ __  
data[2] |__C2__|__C2__|__C2__|__

(packed format)
         ______ ______ ______ ______ ______ ______ ______ ______ ______ __ 
data[0] |__C0__|__C1__|__C2__|__C0__|__C1__|__C2__|__C0__|__C1__|__C2__|__

*/

        for (i = 0; i < frame->nb_samples; i++) /* planar fmt to packed fmt */
            for (ch = 0; ch < dec_ctx->channels; ch++) 
                fwrite(frame->data[ch] + data_size * i, 1, data_size, outfile);
    }
    return 0;
}

