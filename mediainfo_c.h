#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <windows.h>

#include <MediaInfoDLL/MediaInfoDLL_Static.h>

#define LABEL_WIDTH 30

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

static void sb_init(StrBuf *sb) {
    sb->cap = 1024;
    sb->len = 0;
    sb->data = (char *) calloc(sb->cap, 1);
    if (sb->data) {
        sb->data[0] = '\0';
    }
}

static void sb_free(StrBuf *sb) {
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static int sb_reserve(StrBuf *sb, size_t extra) {
    if (!sb->data) {
        return 0;
    }

    size_t needed = sb->len + extra + 1;
    if (needed <= sb->cap) {
        return 1;
    }

    size_t new_cap = sb->cap;
    while (new_cap < needed) {
        new_cap *= 2;
    }

    char *new_data = (char *) realloc(sb->data, new_cap);
    if (!new_data) {
        return 0;
    }

    sb->data = new_data;
    sb->cap = new_cap;
    return 1;
}

static int sb_append(StrBuf *sb, const char *fmt, ...) {
    if (!sb || !sb->data) {
        return 0;
    }

    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);

    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        va_end(args);
        return 0;
    }

    if (!sb_reserve(sb, (size_t) needed)) {
        va_end(args);
        return 0;
    }

    vsnprintf(sb->data + sb->len, sb->cap - sb->len, fmt, args);
    va_end(args);

    sb->len += (size_t) needed;
    return 1;
}

static int wide_to_utf8(const wchar_t *src, char *dst, int dstlen) {
    return WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, dstlen, NULL, NULL);
}

static int utf8_to_wide(const char *src, wchar_t *dst, int dstlen) {
    return MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, dstlen);
}

static int wide_to_ansi(const wchar_t *src, char *dst, int dstlen) {
    return WideCharToMultiByte(CP_ACP, 0, src, -1, dst, dstlen, NULL, NULL);
}

static char *ansi_to_utf8_alloc(const char *src) {
    if (!src) {
        return NULL;
    }

    int wlen = MultiByteToWideChar(CP_ACP, 0, src, -1, NULL, 0);
    if (wlen <= 0) {
        return NULL;
    }

    wchar_t *wbuf = (wchar_t *) calloc((size_t) wlen, sizeof(wchar_t));
    if (!wbuf) {
        return NULL;
    }

    if (MultiByteToWideChar(CP_ACP, 0, src, -1, wbuf, wlen) <= 0) {
        free(wbuf);
        return NULL;
    }

    int u8len = wide_to_utf8(wbuf, NULL, 0);
    if (u8len <= 0) {
        free(wbuf);
        return NULL;
    }

    char *utf8 = (char *) calloc((size_t) u8len, 1);
    if (!utf8) {
        free(wbuf);
        return NULL;
    }

    if (wide_to_utf8(wbuf, utf8, u8len) <= 0) {
        free(wbuf);
        free(utf8);
        return NULL;
    }

    free(wbuf);
    return utf8;
}

static char *utf8_to_ansi_alloc(const char *src) {
    if (!src) {
        return NULL;
    }

    int wlen = utf8_to_wide(src, NULL, 0);
    if (wlen <= 0) {
        return NULL;
    }

    wchar_t *wbuf = (wchar_t *) calloc((size_t) wlen, sizeof(wchar_t));
    if (!wbuf) {
        return NULL;
    }

    if (utf8_to_wide(src, wbuf, wlen) <= 0) {
        free(wbuf);
        return NULL;
    }

    int alen = wide_to_ansi(wbuf, NULL, 0);
    if (alen <= 0) {
        free(wbuf);
        return NULL;
    }

    char *ansi = (char *) calloc((size_t) alen, 1);
    if (!ansi) {
        free(wbuf);
        return NULL;
    }

    if (wide_to_ansi(wbuf, ansi, alen) <= 0) {
        free(wbuf);
        free(ansi);
        return NULL;
    }

    free(wbuf);
    return ansi;
}

static void append_field(StrBuf *sb,
                         void *mi,
                         MediaInfo_stream_C stream_kind,
                         size_t stream_number,
                         const char *label,
                         const char *param) {
    const char *value_ansi = MediaInfoA_Get(
        mi,
        stream_kind,
        stream_number,
        param,
        MediaInfo_Info_Text,
        MediaInfo_Info_Name
    );

    if (!value_ansi || value_ansi[0] == '\0') {
        return;
    }

    char *value_utf8 = ansi_to_utf8_alloc(value_ansi);
    if (!value_utf8) {
        return;
    }

    sb_append(sb, " - %-*s : %s\n", LABEL_WIDTH, label, value_utf8);
    free(value_utf8);
}

static void append_field_fallback(StrBuf *sb,
                                  void *mi,
                                  MediaInfo_stream_C stream_kind,
                                  size_t stream_number,
                                  const char *label,
                                  const char *param1,
                                  const char *param2) {
    const char *value_ansi = MediaInfoA_Get(
        mi,
        stream_kind,
        stream_number,
        param1,
        MediaInfo_Info_Text,
        MediaInfo_Info_Name
    );

    if (value_ansi && value_ansi[0] != '\0') {
        char *value_utf8 = ansi_to_utf8_alloc(value_ansi);
        if (value_utf8) {
            sb_append(sb, " - %-*s : %s\n", LABEL_WIDTH, label, value_utf8);
            free(value_utf8);
        }
        return;
    }

    append_field(sb, mi, stream_kind, stream_number, label, param2);
}

static void mediainfo_func(const char *video_path_utf8, StrBuf *sb) {
    char *video_path_ansi = utf8_to_ansi_alloc(video_path_utf8);

    if (!video_path_ansi) {
        MessageBoxA(NULL, "Failed to convert file path from UTF-8 to ANSI", "Error", MB_OK);
        exit(EXIT_FAILURE);
    }

    void *mi = MediaInfoA_New();
    if (!mi) {
        MessageBoxA(NULL, "Failed to create MediaInfo handle", "Error", MB_OK);
        free(video_path_ansi);
        exit(EXIT_FAILURE);
    }

    if (MediaInfoA_Open(mi, video_path_ansi) == 0) {
        MessageBoxA(NULL, "Failed to open file", "Error", MB_OK);
        MediaInfoA_Delete(mi);
        free(video_path_ansi);
        exit(EXIT_FAILURE);
    }

    free(video_path_ansi);

    sb_init(sb);
    if (!sb->data) {
        MessageBoxA(NULL, "Buffer initialization failed", "Error", MB_OK);
        MediaInfoA_Delete(mi);
        MediaInfoA_Close(mi);
        MediaInfoA_Delete(mi);
        exit(EXIT_FAILURE);
    }

    sb_append(sb, "General\n");
    append_field(sb, mi, MediaInfo_Stream_General, 0, "Complete name", "CompleteName");
    append_field(sb, mi, MediaInfo_Stream_General, 0, "Format", "Format");
    append_field(sb, mi, MediaInfo_Stream_General, 0, "Format version", "Format_Version");
    append_field(sb, mi, MediaInfo_Stream_General, 0, "File size", "FileSize/String");
    append_field(sb, mi, MediaInfo_Stream_General, 0, "Duration", "Duration/String3");
    append_field(sb, mi, MediaInfo_Stream_General, 0, "Overall bit rate", "OverallBitRate/String");
    sb_append(sb, "\n");

    if (MediaInfoA_Count_Get(mi, MediaInfo_Stream_Video, (size_t) -1) > 0) {
        sb_append(sb, "Video\n");
        append_field(sb, mi, MediaInfo_Stream_Video, 0, "Format", "Format");
        append_field(sb, mi, MediaInfo_Stream_Video, 0, "Width", "Width/String");
        append_field(sb, mi, MediaInfo_Stream_Video, 0, "Height", "Height/String");
        append_field(sb, mi, MediaInfo_Stream_Video, 0, "Display aspect ratio", "DisplayAspectRatio/String");
        append_field(sb, mi, MediaInfo_Stream_Video, 0, "Frame rate mode", "FrameRate_Mode");
        append_field(sb, mi, MediaInfo_Stream_Video, 0, "Frame rate", "FrameRate/String");
        append_field(sb, mi, MediaInfo_Stream_Video, 0, "Color space", "ColorSpace");
        append_field(sb, mi, MediaInfo_Stream_Video, 0, "Chroma subsampling", "ChromaSubsampling");
        append_field(sb, mi, MediaInfo_Stream_Video, 0, "Bit depth", "BitDepth/String");
        append_field_fallback(sb, mi, MediaInfo_Stream_Video, 0, "Color range", "colour_range", "ColorRange");
        append_field_fallback(sb, mi, MediaInfo_Stream_Video, 0, "Color primaries", "colour_primaries",
                              "ColorPrimaries");
        sb_append(sb, "\n");
    }

    if (MediaInfoA_Count_Get(mi, MediaInfo_Stream_Audio, (size_t) -1) > 0) {
        sb_append(sb, "Audio\n");
        append_field(sb, mi, MediaInfo_Stream_Audio, 0, "Format", "Format");
        append_field(sb, mi, MediaInfo_Stream_Audio, 0, "Channel(s)", "Channel(s)");
        append_field(sb, mi, MediaInfo_Stream_Audio, 0, "Channel layout", "ChannelLayout");
        append_field(sb, mi, MediaInfo_Stream_Audio, 0, "Sampling rate", "SamplingRate/String");
        append_field(sb, mi, MediaInfo_Stream_Audio, 0, "Bit depth", "BitDepth/String");
        append_field(sb, mi, MediaInfo_Stream_Audio, 0, "Compression mode", "Compression_Mode");
        append_field(sb, mi, MediaInfo_Stream_Audio, 0, "Delay relative to video", "Video_Delay/String3");
    }

    MediaInfoA_Close(mi);
    MediaInfoA_Delete(mi);
}
