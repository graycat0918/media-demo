/**
 * @file avio_dir_cmd.c 
 * using API of ffmpeg to manage media file
 *
 * @author  duruyao
 * @version 1.0  19-08-03
 * @update  [id] [yy-mm-dd] [author] [description] 
 */

#include <libavutil/log.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

static void  usage(const char *);

static int   del_op(const char *);

static int   list_op(const char *);

static int   move_op(const char *, const char *);

static const char *type_string(int);

int main(int argc, char **argv) {
    int ret = 0;
    const char *op = NULL;

    av_log_set_level(AV_LOG_DEBUG);

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    /* do global initialization of network libraries (this functions only
     * exists to work around thread-safety issues with older GnuTLS or
     * OpenSSL libraries) */
    avformat_network_init();

    op = argv[1];
    if (strcmp(op, "list") == 0) {
        if (argc < 3) {
            av_log(NULL, AV_LOG_INFO,
                   "Missing argument for list operation.\n");
            ret = AVERROR(EINVAL);
        } else {
            ret = list_op(argv[2]);
        }
    } else if (strcmp(op, "del") == 0) {
        if (argc < 3) {
            av_log(NULL, AV_LOG_INFO,
                   "Missing argument for del operation.\n");
            ret = AVERROR(EINVAL);
        } else {
            ret = del_op(argv[2]);
        }
    } else if (strcmp(op, "move") == 0) {
        if (argc < 4) {
            av_log(NULL, AV_LOG_INFO,
                   "Missing argument for move operation.\n");
            ret = AVERROR(EINVAL);
        } else {
            ret = move_op(argv[2], argv[3]);
        }
    } else {
        av_log(NULL, AV_LOG_INFO, "Invalid operation %s\n", op);
        ret = AVERROR(EINVAL);
    }

    avformat_network_deinit();

    return ret < 0 ? 1 : 0;
}

static void usage(const char *program_name) {
    fprintf(stderr,
            "usage: %s OPERATION entry1 [entry2]\n"
            "API example program to show how to manipulate resources "
            "accessed through AVIOContext.\n"
            "OPERATIONS:\n"
            "list      list content of the directory\n"
            "move      rename content in directory\n"
            "del       delete content in directory\n",
            program_name);
}

static int del_op(const char *url) {
    int ret = avpriv_io_delete(url);
    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR,
               "Cannot delete '%s' (%s)\n", url, av_err2str(ret));
    return ret;
}

static int list_op(const char *input_dir) {
    AVIODirEntry   *entry = NULL;
    AVIODirContext *ctx   = NULL;
    int cnt, ret;
    char filemode[4], uid_and_gid[20];

    if ((ret = avio_open_dir(&ctx, input_dir, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR,
               "Cannot open directory (%s)\n", av_err2str(ret));
        goto fail;
    }

    cnt = 0;
    for (;;) {
        /* reading end of entry is not a error */
        if ((ret = avio_read_dir(ctx, &entry)) < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Cannot list directory: %s.\n", av_err2str(ret));
            goto fail;
        }
        
        /* end of entry */
        if (!entry)
            break;
        
        /* Unix file mode, -1 if unknown */
        if (entry->filemode == -1) {
            /* return length of the pre-written string, the function is
             * safer than sprintf() */
            snprintf(filemode, 4, "???"); 
        } else { 
            /* PRIo64 is "llo" (64 wordsize), an unsigned 64 bits octal
             * integer value */
            snprintf(filemode, 4, "%3"PRIo64, entry->filemode);
        }
        
        /* PRI64 is "lld" (64 wordsize), a signed 64 bits decimal
         * integer value */
        snprintf(uid_and_gid, 20,
                 "%"PRId64"(%"PRId64")", entry->user_id, entry->group_id);
        if (cnt == 0)
            av_log(NULL, AV_LOG_INFO,
                   "%-8s %12s %30s %10s %s %16s %16s %16s\n",
                   "TYPE", "SIZE", "NAME", "UID(GID)", "UGO",
                   "MODIFIED", "ACCESSED", "STATUS_CHANGED");
        av_log(NULL, AV_LOG_INFO,
               "%-8s %12" PRId64 " %30s %10s %s %16" PRId64 " "
               "%16" PRId64 " %16" PRId64"\n",
               type_string(entry->type),
               entry->size,
               entry->name,
               uid_and_gid,
               filemode,
               entry->modification_timestamp,
               entry->access_timestamp,
               entry->status_change_timestamp);
        avio_free_directory_entry(&entry);
        cnt++;
    };

fail:
    avio_close_dir(&ctx);
    return ret;
}

static int move_op(const char *src, const char *dst) {
    int ret = avpriv_io_move(src, dst);
    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR,
               "Cannot move '%s' into '%s' (%s)\n",
               src, dst, av_err2str(ret));
    return ret;
}

static const char *type_string(int type) {
    switch (type) {
        case AVIO_ENTRY_DIRECTORY:
            return "<DIR>";
        case AVIO_ENTRY_FILE:
            return "<FILE>";
        case AVIO_ENTRY_BLOCK_DEVICE:
            return "<BLOCK DEVICE>";
        case AVIO_ENTRY_CHARACTER_DEVICE:
            return "<CHARACTER DEVICE>";
        case AVIO_ENTRY_NAMED_PIPE:
            return "<PIPE>";
        case AVIO_ENTRY_SYMBOLIC_LINK:
            return "<LINK>";
        case AVIO_ENTRY_SOCKET:
            return "<SOCKET>";
        case AVIO_ENTRY_SERVER:
            return "<SERVER>";
        case AVIO_ENTRY_SHARE:
            return "<SHARE>";
        case AVIO_ENTRY_WORKGROUP:
            return "<WORKGROUP>";
        case AVIO_ENTRY_UNKNOWN:
        default:
            break;
    }
    return "<UNKNOWN>";
}

