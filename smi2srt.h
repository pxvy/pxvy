#pragma once
#ifndef RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_SMI2SRT_H
#define RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_SMI2SRT_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ---- UTF-8 invalid 바이트 제거 (errors='ignore') ----
static char* utf8_sanitize(const char* src, size_t len, size_t* out_len) {
    char* out = (char*)malloc(len + 1);
    size_t i = 0, j = 0;
    while (i < len) {
        unsigned char c = (unsigned char)src[i];
        int bytes;
        if (c < 0x80)            bytes = 1;
        else if ((c >> 5) == 0x6)     bytes = 2;
        else if ((c >> 4) == 0xE)     bytes = 3;
        else if ((c >> 3) == 0x1E)    bytes = 4;
        else { i++; continue; }

        int valid = 1;
        for (int k = 1; k < bytes; k++) {
            if (i + k >= len || ((unsigned char)src[i + k] & 0xC0) != 0x80) {
                valid = 0; break;
            }
        }
        if (valid) { memcpy(out + j, src + i, bytes); j += bytes; i += bytes; }
        else i++;
    }
    out[j] = '\0';
    *out_len = j;
    return out;
}

// ---- 파일 전체 읽기 ----
static char* read_file(const wchar_t* path, size_t* out_len) {
    FILE* f = _wfopen(path, L"rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    size_t sz = (size_t)ftell(f);
    rewind(f);
    char* buf = (char*)malloc(sz + 1);
    fread(buf, 1, sz, f);
    fclose(f);
    buf[sz] = '\0';
    *out_len = sz;

    if (sz >= 3 &&
        (unsigned char)buf[0] == 0xEF &&
        (unsigned char)buf[1] == 0xBB &&
        (unsigned char)buf[2] == 0xBF) {
        memmove(buf, buf + 3, sz - 2);
        *out_len = sz - 3;
    }
    return buf;
}

// ---- 대소문자 무시 strstr ----
static char* stristr(const char* hay, const char* needle) {
    size_t nlen = strlen(needle);
    for (; *hay; hay++) {
        if (_strnicmp(hay, needle, nlen) == 0)
            return (char*)hay;
    }
    return NULL;
}

// ---- HTML 태그 제거 + &nbsp; 치환 + 앞뒤 공백 제거 ----
static char* strip_html(const char* src) {
    size_t len = strlen(src);
    char* tmp = (char*)malloc(len + 1);
    size_t j = 0;
    int in_tag = 0;
    for (size_t i = 0; i < len; i++) {
        if (src[i] == '<') { in_tag = 1; continue; }
        if (src[i] == '>') { in_tag = 0; continue; }
        if (!in_tag) tmp[j++] = src[i];
    }
    tmp[j] = '\0';

    // &nbsp; -> 공백
    char* out = (char*)malloc(j + 1);
    size_t oi = 0;
    for (size_t i = 0; i < j; ) {
        if (strncmp(tmp + i, "&nbsp;", 6) == 0) {
            out[oi++] = ' ';
            i += 6;
        }
        else {
            out[oi++] = tmp[i++];
        }
    }
    out[oi] = '\0';
    free(tmp);

    // 앞뒤 공백/개행 trim
    char* start = out;
    while (*start && (unsigned char)*start <= ' ') start++;
    char* end = start + strlen(start);
    while (end > start && (unsigned char)*(end - 1) <= ' ') end--;
    *end = '\0';

    char* result = _strdup(start);
    free(out);
    return result;
}

// ---- ms -> SRT 타임코드 ----
static void ms_to_srt_time(long long ms, char* buf) {
    long long h = ms / 3600000; ms %= 3600000;
    long long m = ms / 60000;   ms %= 60000;
    long long s = ms / 1000;    ms %= 1000;
    sprintf(buf, "%02lld:%02lld:%02lld,%03lld", h, m, s, ms);
}

// ---- SYNC 파싱 ----
typedef struct {
    long long start;
    long long end;
    char* text;
} Entry;
static void smi_to_srt(const wchar_t* smi_path, const wchar_t* srt_path) {
    size_t raw_len;
    char* raw = read_file(smi_path, &raw_len);
    if (!raw) { fwprintf(stderr, L"Cannot open: %s\n", smi_path); return; }

    size_t clean_len;
    char* content = utf8_sanitize(raw, raw_len, &clean_len);
    free(raw);

    const char* SYNC_TAG = "<SYNC Start=";
    size_t cap = 256;
    size_t count = 0;
    char** positions = (char**)malloc(cap * sizeof(char*));

    char* p = content;
    while ((p = stristr(p, SYNC_TAG)) != NULL) {
        if (count >= cap) { cap *= 2; positions = (char**)realloc(positions, cap * sizeof(char*)); }
        positions[count++] = p;
        p++;
    }

    size_t entry_cap = 256;
    size_t entry_count = 0;
    Entry* entries = (Entry*)malloc(entry_cap * sizeof(Entry));

    for (size_t i = 0; i < count; i++) {
        char* tag_start = positions[i];
        char* num_start = tag_start + strlen(SYNC_TAG);
        long long start_time = strtoll(num_start, NULL, 10);

        char* text_start = strchr(tag_start, '>');
        if (!text_start) continue;
        text_start++;

        char* text_end = (i + 1 < count) ? positions[i + 1] : content + clean_len;
        size_t text_len = text_end - text_start;
        char* raw_text = (char*)malloc(text_len + 1);
        memcpy(raw_text, text_start, text_len);
        raw_text[text_len] = '\0';

        char* clean = strip_html(raw_text);
        free(raw_text);

        if (strlen(clean) == 0) { free(clean); continue; }

        long long end_time = (i + 1 < count)
            ? strtoll(positions[i + 1] + strlen(SYNC_TAG), NULL, 10)
            : start_time + 2000;

        if (entry_count >= entry_cap) {
            entry_cap *= 2;
            entries = (Entry*)realloc(entries, entry_cap * sizeof(Entry));
        }
        Entry e;
        e.start = start_time;
        e.end = end_time;
        e.text = clean;
        entries[entry_count++] = e;
    }

    free(positions);
    free(content);

    FILE* out = _wfopen(srt_path, L"wb");
    if (!out) { fwprintf(stderr, L"Cannot write: %s\n", srt_path); goto cleanup; }

    char t1[32], t2[32];
    for (size_t i = 0; i < entry_count; i++) {
        ms_to_srt_time(entries[i].start, t1);
        ms_to_srt_time(entries[i].end, t2);
        fprintf(out, "%zu\n%s --> %s\n%s\n\n", i + 1, t1, t2, entries[i].text);
    }
    fclose(out);

cleanup:
    for (size_t i = 0; i < entry_count; i++) free(entries[i].text);
    free(entries);
}
#endif //RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_SMI2SRT_H