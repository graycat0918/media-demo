/**
 * @file avio_reading.c
 * make libavformat demuxer access media content through a custom
 * AVIOContext read callback
 *
 * @author  duruyao
 * @version 1.0  19-09-11
 * @update  [id] [yy-mm-dd] [author] [description] 
 */

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>

struct buffer_data {
    uint8_t *ptr;
    size_t size; /* size left in the buffer */
};

static int read_packet(void *, uint8_t *, int);

int main(int argc, char **argv) {
    int ret = 0;
    char *infilename = NULL;
    uint8_t *buffer          = NULL;
    uint8_t *avio_ctx_buffer = NULL;
    AVFormatContext *fmt_ctx  = NULL;
    AVIOContext     *avio_ctx = NULL;
    size_t buffer_size          = 0;
    size_t avio_ctx_buffer_size = 4096;
    struct buffer_data bd = {0};
    
    av_register_all();

    if (argc != 2) {
        fprintf(stderr,
                "Usage: %s <input file>\n"
                "API example program to show how to read from a custom "
                "buffer accessed through AVIOContext.\n", argv[0]);
        return 1;
    }
    infilename = argv[1];

    /* slurp file content into buffer and the buffer must be released with
     * av_file_unmap() */
    ret = av_file_map(infilename, &buffer, &buffer_size, 0, NULL);
    if (ret < 0)
        goto end;

    /* fill opaque structure used by the AVIOContext read callback */
    bd.ptr  = buffer;
    bd.size = buffer_size;
    
    /* allocate an AVFormatContext, avformat_free_context() can be used to
     * release the context and everything allocated by the framework */
    if (!(fmt_ctx = avformat_alloc_context())) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    
    /* allocate a memory block with alignment suitable for all memory
     * accesses */
    avio_ctx_buffer = av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* allocate and initialize an AVIOContext for buffered I/O, it must be
     * later freed with avio_context_free() */
    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
                                  0, &bd, &read_packet, NULL, NULL);
    if (!avio_ctx) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /**
     * AVFormatContext::pb
     *
     * muxing: set by the user before avformat_write_header(), the
     * caller must take care of closing / freeing the IO context
     *
     * demuxing: either set by the user before avformat_open_input()
     * (then the user must close it manually) or set by avformat_open_input()
     */
    fmt_ctx->pb = avio_ctx;

    ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open input\n");
        goto end;
    }

    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not find stream information\n");
        goto end;
    }

    av_dump_format(fmt_ctx, 0, infilename, 0);

end:
    avformat_close_input(&fmt_ctx);

    /* note: the internal buffer could have changed,
     * and be != avio_ctx_buffer */
    if (avio_ctx)
        av_freep(&avio_ctx->buffer);
    avio_context_free(&avio_ctx);

    av_file_unmap(buffer, buffer_size);

    if (ret < 0) {
        fprintf(stderr, "Error occurred (%s)\n", av_err2str(ret));
        return 1;
    }

    return 0;
}

static int read_packet(void *opaque, uint8_t *buf, int buf_size) {
    /* opaque data type is a type whose concrete data structure is not
     * defined in an interface, such as FILE */
    struct buffer_data *bd = (struct buffer_data *)opaque;
    buf_size = FFMIN(buf_size, bd->size);

    if (!buf_size)
        return AVERROR_EOF;
    printf("ptr:%p size:%zu\n", bd->ptr, bd->size);

    /* copy internal buffer data to buf */
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr  += buf_size;
    bd->size -= buf_size;

    return buf_size;
}

