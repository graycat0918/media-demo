/**
 * @file decode_video.c
 * video decoding with libavcodec API example
 *
 * @author  duruyao
 * @version 1.0  19-10-03
 * @update  [id] [yy-mm-dd] [author] [description] 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>

#define INBUF_SIZE 4096

static void pgm_save(unsigned char *, int, int, int, char *);

static int  decode(AVCodecContext *, AVFrame *, AVPacket *, const char *);

int main(int argc, char **argv) {
    int ret;
    const char *infilename  = NULL;
    const char *outfilename = NULL;
    FILE *fd = NULL;
    const AVCodec  *codec     = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVCodecParserContext *parser_ctx = NULL;
    uint8_t   inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    uint8_t  *data;
    size_t    data_size;
    AVPacket *pkt = NULL;
    AVFrame  *decoded_frame = NULL;

/*

video decoding
 ____________           ___________          _________________  
|            |   read  |           | prase  |                 | decode
| input file | ------->| in buffer |------->| encoded packets |-------+
|____________|         |___________|        |_________________| 

     ________________          _____________
    |                | write  |             |
+-->| decoded frames |------->| output file |
    |________________|        |_____________|

*/

    if (argc != 3) {
        fprintf(stderr, 
                "Usage: %s <input file> <output file>\n"
                "And check your input file is encoded by MPEG-1 Video please.\n",
                argv[0]);
        exit(0);
    }
    infilename  = argv[1];
    outfilename = argv[2];

    avcodec_register_all();

    pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "Cannot allocate packet\n");   
        exit(1);
    }

    /* set end of buffer to 0 (this ensures that no overreading happens
     * for damaged MPEG streams) */
    memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

    /* find the MPEG-1 video decoder */
    codec = avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO);
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
        fprintf(stderr, "Cannot allocate video codec context\n");
        goto end;
    }

    /* For some codecs, such as msmpeg4 and mpeg4, width and height MUST be 
     * initialized there because this information is not available in the 
     * bitstream. */

    /* open it */
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Cannot open codec\n");
        goto end;
    }

    decoded_frame = av_frame_alloc();
    if (!decoded_frame) {
        fprintf(stderr, "Cannot allocate video frame\n");
        goto end;
    }

    fd = fopen(infilename, "rb");
    if (!fd) {
        fprintf(stderr, "Cannot open %s\n", infilename);
        goto end;
    }

    while (!feof(fd)) {
        /* read raw data from the input file */
        data_size = fread(inbuf, 1, INBUF_SIZE, fd);
        if (!data_size)
            break;

        /* use the parser to split the data into frames 
         * (IS 'frames' the same as ENCODED PACKETS?? YES) */
        data = inbuf;
        while (data_size > 0) {
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
                if (decode(codec_ctx, decoded_frame, pkt, outfilename) < 0)
                    goto end;
        }
    }

    /* flush the decoder */
    if (decode(codec_ctx, decoded_frame, NULL, outfilename) < 0)
        goto end;

end:
    if (fd) fclose(fd);
    av_frame_free(&decoded_frame);
    avcodec_free_context(&codec_ctx);
    av_parser_close(parser_ctx);
    av_packet_free(&pkt);

    return 0;
}

static void pgm_save(unsigned char *buf, int wrap,
                     int xsize, int ysize, char *filename) {
    int i;
    FILE *fd;

    fd = fopen(filename,"w");
    fprintf(fd, "P5\n%d %d\n%d\n", xsize, ysize, 255);
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, fd);
    fclose(fd);
}

static int decode(AVCodecContext *dec_ctx, AVFrame *frame,
                  AVPacket *pkt, const char *filename) {
    int ret;
    char buf[1024];

    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr,
                "Error sending a packet for decoding (%s)\n",
                av_err2str(ret));
        return -1;
    }

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

        fprintf(stdout, "saving frame %3d\n", dec_ctx->frame_number);
        fflush(stdout);

        /* The picture is allocated by the decoder, no need to free it */
        snprintf(buf, sizeof(buf), "%s-%d",
                 filename, dec_ctx->frame_number);
        pgm_save(frame->data[0], frame->linesize[0], 
                 frame->width, frame->height, buf);
    }
    return 0;
}

