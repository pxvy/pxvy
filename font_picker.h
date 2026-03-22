// font_picker.h  –  PXVY 폰트 선택 다이얼로그 (v4)
//
// 변경점 (v4):
//   - 한글 폰트명 렌더: wgl outline → GDI 텍스처 (한글 완전 지원)
//   - 선택 확정 시 pxvy_set_subtitle_font() 자동 호출 (DB 저장)
//   - 스크롤바 드래그 유지
//
// 한글 렌더 원리:
//   TextOutA(FONT_SUBTITLE_FAMILY) → 32bpp DIB → R채널 = 알파
//   → GL 텍스처 업로드 → glColor 로 색상 변조 (GL_MODULATE)
//   → 지연 생성 (visible 항목만, 첫 렌더 시)
//
// 의존: main.c (os_theme, g_primary_color, glColor4f_255,
//               OS_THEME_BK, OS_THEME_FG, OS_THEME_PC,
//               FONT_SUBTITLE_FAMILY)
//       database.h (pxvy_set_subtitle_font)
//       stb_image.h, stb_image_resize2.h
#pragma once
#ifndef PXVY_FONT_PICKER_H
#define PXVY_FONT_PICKER_H

#define _USE_MATH_DEFINES
#include <math.h>
#include <shlwapi.h>
#include <windows.h>
#include <GL/gl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "database.h"

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif

// ─── 레이아웃 ────────────────────────────────────────────────────
#define FP_W            360
#define FP_H            440
#define FP_CAP_H         32
#define FP_BTN_W         46
#define FP_SYM_SZ        20
#define FP_ROUND         10
#define FP_FONT_H        20    // UI 폰트 (캡션·버튼·KR 뱃지)
#define FP_LIST_FONT_H   16    // 리스트 폰트 (FONT_SUBTITLE_FAMILY)
#define FP_ITEM_H        34
#define FP_LIST_X        16
#define FP_LIST_W       (FP_W - FP_LIST_X * 2)
#define FP_SCROLL_W      8
#define FP_ALPHA        230
#define FP_TITLE        "FONT"

#define FP_LIST_TOP_Y   (FP_CAP_H + 8)
#define FP_FOOTER_H      48

// ─── 폰트 항목 ───────────────────────────────────────────────────
typedef struct {
    char family[LF_FACESIZE];
    BYTE charset;
    BYTE pitch_and_family;
} FpFontItem;

// ─── GDI 텍스처 캐시 항목 ────────────────────────────────────────
typedef struct {
    GLuint tex;
    int    w, h;
} FpNameTex;

// ─── 콜백 ────────────────────────────────────────────────────────
typedef void (*FpCallback)(const char *family, void *userdata);

// ─── 컨텍스트 ────────────────────────────────────────────────────
typedef struct {
    HDC    gl_dc;
    HGLRC  gl_rc;
    GLuint sym_tex;

    FpFontItem *all_items;
    int         all_count;
    int        *filtered;
    int         filtered_count;

    // 한글 이름 텍스처 캐시 [all_count], 지연 생성
    FpNameTex  *name_textures;

    int   selected;
    int   hovered;

    float scroll_y;
    float scroll_max;

    // 스크롤바 드래그
    BOOL  sb_dragging;
    float sb_drag_offset;

    BOOL  close_hover;
    BOOL  ok_hover;

    FpCallback callback;
    void      *userdata;
} FpCtx;

// ─── 전역 핸들 ───────────────────────────────────────────────────
static HWND g_fp_hwnd    = NULL;
static HWND g_fp_gl_hwnd = NULL;

// ─── GL 폰트 (UI 전용: 캡션·버튼·KR 뱃지) ───────────────────────
static GLuint            g_fp_font  = 0;
static GLYPHMETRICSFLOAT g_fp_glyph[128];

// ═══════════════════════════════════════════════════════════════════
// 시스템 폰트 열거
// ═══════════════════════════════════════════════════════════════════

typedef struct { FpFontItem *buf; int count, capacity; } FpEnumState;

static BOOL fp_already_added(FpEnumState *st, const char *name) {
    for (int i = 0; i < st->count; i++)
        if (_stricmp(st->buf[i].family, name) == 0) return TRUE;
    return FALSE;
}

static int CALLBACK fp_enum_proc(const LOGFONTW *lf,
                                  const TEXTMETRICW *tm,
                                  DWORD type, LPARAM lp) {
    FpEnumState *st = (FpEnumState *)lp; (void)tm;
    if (lf->lfFaceName[0] == L'@') return 1;
    if ((type & TRUETYPE_FONTTYPE) == 0 &&
        (type & DEVICE_FONTTYPE)   == 0) return 1;

    // Wide → UTF-8 변환
    char utf8[LF_FACESIZE * 3] = {0};
    WideCharToMultiByte(CP_UTF8, 0, lf->lfFaceName, -1,
                        utf8, sizeof(utf8), NULL, NULL);

    if (fp_already_added(st, utf8)) return 1;

    if (st->count >= st->capacity) {
        int nc = st->capacity ? st->capacity * 2 : 256;
        FpFontItem *nb = (FpFontItem *)realloc(st->buf, sizeof(FpFontItem)*nc);
        if (!nb) return 0;
        st->buf = nb; st->capacity = nc;
    }
    strncpy_s(st->buf[st->count].family, LF_FACESIZE, utf8, _TRUNCATE);
    st->buf[st->count].charset          = lf->lfCharSet;
    st->buf[st->count].pitch_and_family = lf->lfPitchAndFamily;
    st->count++;
    return 1;
}
static int fp_cmp_font(const void *a, const void *b) {
    return _stricmp(((const FpFontItem *)a)->family,
                    ((const FpFontItem *)b)->family);
}

static void fp_enum_system_fonts(FpCtx *ctx) {
    FpEnumState st = {0};
    HDC hdc = GetDC(NULL);
    LOGFONTW lf = {0}; lf.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)fp_enum_proc,
                        (LPARAM)&st, 0);
    ReleaseDC(NULL, hdc);
    qsort(st.buf, st.count, sizeof(FpFontItem), fp_cmp_font);
    ctx->all_items = st.buf;
    ctx->all_count = st.count;
}


static void fp_build_full_list(FpCtx *ctx) {
    free(ctx->filtered);
    ctx->filtered = (int *)malloc(sizeof(int) * (ctx->all_count + 1));
    ctx->filtered_count = 0;
    for (int i = 0; i < ctx->all_count; i++)
        ctx->filtered[ctx->filtered_count++] = i;
    ctx->scroll_y = 0.0f; ctx->scroll_max = 0.0f;
    ctx->selected = (ctx->filtered_count > 0) ? 0 : -1;
}

static const FpFontItem *fp_selected_item(FpCtx *ctx) {
    if (ctx->selected < 0 || ctx->selected >= ctx->filtered_count) return NULL;
    return &ctx->all_items[ctx->filtered[ctx->selected]];
}

// ═══════════════════════════════════════════════════════════════════
// GDI 텍스처: 한글 포함 폰트명 렌더
// ═══════════════════════════════════════════════════════════════════

// 검은 배경 + 흰 텍스트 → R채널 = 알파 → GL_MODULATE 로 glColor 로 착색
static GLuint fp_make_name_tex(const char *text, int *out_w, int *out_h) {
    // UTF-8 → Wide 변환
    WCHAR wtext[LF_FACESIZE * 2] = {0};
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, sizeof(wtext)/sizeof(WCHAR));

    HDC src_dc = GetDC(NULL);
    HDC mem_dc = CreateCompatibleDC(src_dc);

    // HFONT hf = CreateFontW(-FP_LIST_FONT_H, 0, 0, 0, FW_NORMAL, 0, 0, 0,
    //     DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
    //     ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS,
    //     L"" FONT_SUBTITLE_FAMILY);      // Wide 리터럴


    WCHAR wfamily[LF_FACESIZE] = {0};
    MultiByteToWideChar(CP_UTF8, 0, FONT_SUBTITLE_FAMILY, -1,
                        wfamily, LF_FACESIZE);
    HFONT hf = CreateFontW(-FP_LIST_FONT_H, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, wfamily);

    HFONT old_f = (HFONT)SelectObject(mem_dc, hf);

    SIZE sz = {0};
    GetTextExtentPoint32W(mem_dc, wtext, (int)wcslen(wtext), &sz);
    int w = sz.cx + 4, h = sz.cy + 4;
    if (w < 2) w = 2; if (h < 2) h = 2;

    // 32bpp DIB 생성
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;   // top-down
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    DWORD *bits = NULL;
    HBITMAP dib   = CreateDIBSection(mem_dc, &bi, DIB_RGB_COLORS,
                                     (void **)&bits, NULL, 0);
    HBITMAP oldbm = (HBITMAP)SelectObject(mem_dc, dib);

    // 검은 배경 + 흰 텍스트
    memset(bits, 0, (size_t)w * h * 4);
    SetBkMode(mem_dc, OPAQUE);
    SetBkColor(mem_dc, RGB(0, 0, 0));
    SetTextColor(mem_dc, RGB(255, 255, 255));
    TextOutW(mem_dc, 2, 2, wtext, (int)wcslen(wtext));  // ← W 버전
    GdiFlush();

    // R 채널을 알파로 재구성: pixel = (R=255, G=255, B=255, A=R_원본)
    // → GL_MODULATE: fragment = glColor * (1,1,1,a) → glColor 색상 그대로
    for (int i = 0; i < w * h; i++) {
        BYTE r = (bits[i] >> 16) & 0xFF;
        bits[i] = ((DWORD)r << 24) | 0x00FFFFFF;
    }

    // GL 텍스처 업로드 (GL 컨텍스트가 현재 활성화 상태여야 함)
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_BGRA_EXT, GL_UNSIGNED_BYTE, bits);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    SelectObject(mem_dc, oldbm); SelectObject(mem_dc, old_f);
    DeleteObject(dib); DeleteObject(hf);
    DeleteDC(mem_dc); ReleaseDC(NULL, src_dc);

    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    return tex;
}

// glColor 로 색상이 결정되는 텍스처 쿼드 렌더
static void fp_draw_name_tex(GLuint tex, float x, float y, float w, float h) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBegin(GL_QUADS);
    glTexCoord2f(0,0); glVertex2f(x,     y);
    glTexCoord2f(1,0); glVertex2f(x + w, y);
    glTexCoord2f(1,1); glVertex2f(x + w, y + h);
    glTexCoord2f(0,1); glVertex2f(x,     y + h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

// ═══════════════════════════════════════════════════════════════════
// GL 유틸리티 (UI용: Segoe UI)
// ═══════════════════════════════════════════════════════════════════

static void fp_ortho(int w, int h) {
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);  glLoadIdentity();
}

static void fp_init_font(HDC hdc) {
    if (g_fp_font) return;
    g_fp_font = glGenLists(128);
    HFONT hf = CreateFontA(-FP_FONT_H, 0, 0, 0, FW_BOLD, 0, 0, 0,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT old = (HFONT)SelectObject(hdc, hf);
    wglUseFontOutlinesA(hdc, 0, 128, g_fp_font,
                        0.0f, 0.0f, WGL_FONT_POLYGONS, g_fp_glyph);
    SelectObject(hdc, old); DeleteObject(hf);
}

static float fp_str_w(const char *s, float scale) {
    float sz = FP_FONT_H * scale, w = 0.0f;
    for (const char *p = s; *p; p++) {
        int i = (unsigned char)*p;
        if (i < 128) w += g_fp_glyph[i].gmfCellIncX * sz;
    }
    return w;
}

static void fp_str(float x, float y, const char *s, float scale) {
    float sz = FP_FONT_H * scale;
    glPushMatrix(); glTranslatef(x, y, 0.0f); glScalef(sz, -sz, 1.0f);
    glPushAttrib(GL_LIST_BIT); glListBase(g_fp_font);
    glCallLists((GLsizei)strlen(s), GL_UNSIGNED_BYTE, s);
    glPopAttrib(); glPopMatrix();
}

static void fp_rect(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
    glVertex2f(x,y); glVertex2f(x+w,y); glVertex2f(x+w,y+h); glVertex2f(x,y+h);
    glEnd();
}

static void fp_quad_tex(GLuint tex, float x, float y, float w, float h) {
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, tex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4f(1,1,1,1);
    glBegin(GL_QUADS);
    glTexCoord2f(0,0); glVertex2f(x,   y);
    glTexCoord2f(1,0); glVertex2f(x+w, y);
    glTexCoord2f(1,1); glVertex2f(x+w, y+h);
    glTexCoord2f(0,1); glVertex2f(x,   y+h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

static GLuint fp_load_sym_tex(void) {
    char exe[MAX_PATH], sym[MAX_PATH];
    GetModuleFileNameA(NULL, exe, MAX_PATH);
    char *sl = strrchr(exe, '\\'); if (sl) *(sl+1) = '\0';
    snprintf(sym, MAX_PATH, "%ssymbol.png", exe);
    int sw, sh, ch;
    unsigned char *src = stbi_load(sym, &sw, &sh, &ch, 4);
    if (!src) return 0;
    unsigned char *dst = (unsigned char *)malloc((size_t)FP_SYM_SZ*FP_SYM_SZ*4);
    if (!dst) { stbi_image_free(src); return 0; }
    stbir_resize_uint8_linear(src, sw, sh, 0, dst, FP_SYM_SZ, FP_SYM_SZ, 0, STBIR_RGBA);
    stbi_image_free(src);
    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, FP_SYM_SZ, FP_SYM_SZ, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, dst);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0); free(dst);
    return t;
}

// ═══════════════════════════════════════════════════════════════════
// 스크롤바 thumb 계산
// ═══════════════════════════════════════════════════════════════════

static void fp_sb_thumb(FpCtx *ctx, float list_top, float list_h,
                         float *out_y, float *out_h) {
    float total  = ctx->filtered_count * (float)FP_ITEM_H;
    float ratio  = (total > 0.0f) ? list_h / total : 1.0f;
    float th     = fmaxf(20.0f, list_h * ratio);
    float ty     = (ctx->scroll_max > 0.0f)
                   ? list_top + (ctx->scroll_y / ctx->scroll_max) * (list_h - th)
                   : list_top;
    if (out_y) *out_y = ty;
    if (out_h) *out_h = th;
}

// ═══════════════════════════════════════════════════════════════════
// GL 렌더
// ═══════════════════════════════════════════════════════════════════

static void fp_render_gl(FpCtx *ctx) {
    const float W = FP_W, H = FP_H, capH = (float)FP_CAP_H;

    glViewport(0, 0, FP_W, FP_H);
    glClearColor(os_theme.background.r/255.0f,
                 os_theme.background.g/255.0f,
                 os_theme.background.b/255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    fp_ortho(FP_W, FP_H);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ── 1. 전체 배경 ──
    glColor4f_255(OS_THEME_BK, 1.0f);
    fp_rect(0, 0, W, H);

    // ── 2. 캡션 바 ──  OS_THEME_PC → OS_THEME_BK
    glColor4f_255(OS_THEME_BK, 1.0f);   // ← 변경
    fp_rect(0, 0, W, capH);

    // ── 3. 캡션 아이콘 ── ly 계산식을 about.h 와 동일하게
    if (ctx->sym_tex) {
        float lw = FP_SYM_SZ, lh = FP_SYM_SZ;
        float lx = 15.0f;
        float ly = (capH - lh) * 0.5f + 1.0f;   // ← about.h 완전 동일
        fp_quad_tex(ctx->sym_tex, lx, ly, lw, lh);
    }

    // ── 4. 캡션 텍스트 ── ty = capH * 0.70f (about.h 동일)
    {
        float tx = 15.0f + (float)FP_SYM_SZ + 6.0f;
        float ty = capH * 0.70f;                 // ← 0.72f → 0.70f
        glEnable(GL_POLYGON_SMOOTH);
        glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
        glColor4f(0.0f, 0.0f, 0.0f, 0.4f);      // ← 그림자 alpha 0.35 → 0.4
        fp_str(tx + 1.0f, ty + 1.0f, FP_TITLE, 1.0f);
        glColor4f(1.0f, 1.0f, 1.0f, 0.95f);
        fp_str(tx, ty, FP_TITLE, 1.0f);
        glDisable(GL_POLYGON_SMOOTH);
    }

    // ── 5. 닫기 버튼 ── hover 색상을 g_primary_color 로 (about.h 동일)
    {
        float bW = (float)FP_BTN_W, bH = capH, cL = W - bW;
        float cx = cL + bW * 0.5f, cy = bH * 0.5f, s = 5.0f;
        if (ctx->close_hover) {
            glColor4f_255(g_primary_color.r,          // ← white overlay → primary
                          g_primary_color.g,
                          g_primary_color.b, 0.9f);
            fp_rect(cL, 0, bW, bH);
        }
        glColor4f(1.0f, 1.0f, 1.0f, 0.90f);
        glLineWidth(1.5f);
        glBegin(GL_LINES);
        glVertex2f(cx - s, cy - s); glVertex2f(cx + s, cy + s);
        glVertex2f(cx + s, cy - s); glVertex2f(cx - s, cy + s);
        glEnd();
        glLineWidth(1.0f);
    }

    // ── 6. 리스트 ──
    const float LIST_TOP = (float)FP_LIST_TOP_Y;
    const float LIST_BOT = H - (float)FP_FOOTER_H;
    const float LIST_H   = LIST_BOT - LIST_TOP;
    const float ITEM_H   = (float)FP_ITEM_H;

    ctx->scroll_max = fmaxf(0.0f, ctx->filtered_count * ITEM_H - LIST_H);

    glEnable(GL_SCISSOR_TEST);
    glScissor((GLint)FP_LIST_X, (GLint)(H - LIST_BOT),
              (GLint)FP_LIST_W, (GLint)LIST_H);

    for (int fi = 0; fi < ctx->filtered_count; fi++) {
        float iy = LIST_TOP + fi * ITEM_H - ctx->scroll_y;
        if (iy + ITEM_H < LIST_TOP) continue;
        if (iy > LIST_BOT) break;

        int  ri     = ctx->filtered[fi];
        BOOL sel    = (fi == ctx->selected);
        BOOL hov    = (fi == ctx->hovered);
        BOOL hangul = (ctx->all_items[ri].charset == HANGUL_CHARSET);

        // 항목 배경
        if (sel) {
            glColor4f_255(OS_THEME_PC, 0.15f);
            fp_rect((float)FP_LIST_X, iy,
                    (float)(FP_LIST_W - FP_SCROLL_W - 4), ITEM_H - 1.0f);
            glColor4f_255(OS_THEME_PC, 1.0f);
            fp_rect((float)FP_LIST_X, iy + 4.0f, 3.0f, ITEM_H - 9.0f);
        } else if (hov) {
            glColor4f_255(OS_THEME_FG, 0.06f);
            fp_rect((float)FP_LIST_X, iy,
                    (float)(FP_LIST_W - FP_SCROLL_W - 4), ITEM_H - 1.0f);
        }

        // ── 폰트 이름: GDI 텍스처 (지연 생성) ──
        FpNameTex *nt = &ctx->name_textures[ri];
        if (!nt->tex)
            nt->tex = fp_make_name_tex(ctx->all_items[ri].family, &nt->w, &nt->h);

        if (nt->tex) {
            float tx  = (float)FP_LIST_X + 12.0f;
            float ty  = iy + (ITEM_H - (float)nt->h) * 0.5f;
            // 최대 너비 클리핑은 Scissor 가 담당
            if (sel) glColor4f_255(OS_THEME_PC, 1.0f);
            else     glColor4f_255(OS_THEME_FG, 0.88f);
            fp_draw_name_tex(nt->tex, tx, ty, (float)nt->w, (float)nt->h);
        }

        // KR 뱃지 (ASCII "KR" → g_fp_font 사용 가능)
        if (hangul) {
            glEnable(GL_POLYGON_SMOOTH);
            float bw = fp_str_w("KR", 0.52f) + 7.0f;
            float bx = (float)(FP_W - FP_LIST_X - FP_SCROLL_W - 8) - bw;
            float by = iy + (ITEM_H - 13.0f) * 0.5f;
            glColor4f_255(OS_THEME_PC, sel ? 0.35f : 0.16f);
            fp_rect(bx, by, bw, 13.0f);
            glColor4f_255(OS_THEME_PC, sel ? 1.0f : 0.60f);
            fp_str(bx + 3.5f, by + 9.5f, "KR", 0.52f);
            glDisable(GL_POLYGON_SMOOTH);
        }

        // 구분선
        glColor4f_255(OS_THEME_FG, 0.06f);
        glBegin(GL_LINES);
        glVertex2f((float)FP_LIST_X + 8.0f,
                   iy + ITEM_H - 1.0f);
        glVertex2f((float)(FP_W - FP_LIST_X - FP_SCROLL_W - 4),
                   iy + ITEM_H - 1.0f);
        glEnd();
    }
    glDisable(GL_SCISSOR_TEST);

    // ── 7. 스크롤바 ──
    {
        float sb_x = (float)(FP_W - FP_LIST_X - FP_SCROLL_W);
        glColor4f_255(OS_THEME_FG, 0.06f);
        fp_rect(sb_x, LIST_TOP, (float)FP_SCROLL_W, LIST_H);
        if (ctx->scroll_max > 0.0f) {
            float th_y, th_h;
            fp_sb_thumb(ctx, LIST_TOP, LIST_H, &th_y, &th_h);
            glColor4f_255(OS_THEME_PC, ctx->sb_dragging ? 0.80f : 0.50f);
            fp_rect(sb_x + 1.0f, th_y, (float)(FP_SCROLL_W - 2), th_h);
        }
    }

    // ── 8. 하단 구분선 ──
    glColor4f_255(OS_THEME_PC, 0.30f);
    glBegin(GL_LINES);
    glVertex2f(16.0f, LIST_BOT+2.0f); glVertex2f(W-16.0f, LIST_BOT+2.0f);
    glEnd();

    // ── 9. 선택된 폰트명 (하단 왼쪽) ──
    {
        const FpFontItem *si = fp_selected_item(ctx);
        if (si) {
            int ri = ctx->filtered[ctx->selected];
            FpNameTex *nt = &ctx->name_textures[ri];
            if (!nt->tex)
                nt->tex = fp_make_name_tex(si->family, &nt->w, &nt->h);
            if (nt->tex) {
                float ty2 = LIST_BOT + ((float)FP_FOOTER_H - (float)nt->h) * 0.5f;
                glColor4f_255(OS_THEME_FG, 0.42f);
                fp_draw_name_tex(nt->tex, (float)FP_LIST_X, ty2,
                                 (float)nt->w * 0.85f, (float)nt->h * 0.85f);
            }
        }
    }

    // ── 10. OK 버튼 ──
    {
        const float BW=72.0f, BH=26.0f;
        float bx=W-(float)FP_LIST_X-BW;
        float by=LIST_BOT+((float)FP_FOOTER_H-BH)*0.5f;
        glColor4f_255(OS_THEME_PC, ctx->ok_hover ? 1.0f : 0.80f);
        fp_rect(bx, by, BW, BH);
        glEnable(GL_POLYGON_SMOOTH);
        float tw=fp_str_w("OK",0.82f);
        glColor4f(1,1,1,0.95f);
        fp_str(bx+(BW-tw)*0.5f, by+BH*0.70f, "OK", 0.82f);
        glDisable(GL_POLYGON_SMOOTH);
    }

    glFlush();
}

// ═══════════════════════════════════════════════════════════════════
// UpdateLayeredWindow 파이프라인 (about.h 완전 동일)
// ═══════════════════════════════════════════════════════════════════

static void fp_draw_layered(void) {
    if (!g_fp_hwnd || !g_fp_gl_hwnd) return;
    FpCtx *ctx = (FpCtx *)GetWindowLongPtr(g_fp_hwnd, 0);
    if (!ctx || !ctx->gl_rc) return;

    const int W = FP_W, H = FP_H;
    wglMakeCurrent(ctx->gl_dc, ctx->gl_rc);
    fp_render_gl(ctx);

    DWORD *gl_pix = (DWORD *)malloc((size_t)W*H*4);
    if (!gl_pix) { wglMakeCurrent(NULL,NULL); return; }
    glReadPixels(0, 0, W, H, GL_BGRA_EXT, GL_UNSIGNED_BYTE, gl_pix);
    wglMakeCurrent(NULL, NULL);

    HDC scr_dc=GetDC(NULL), mem_dc=CreateCompatibleDC(scr_dc);
    BITMAPINFO bi={0};
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth=W; bi.bmiHeader.biHeight=-H;
    bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32;
    bi.bmiHeader.biCompression=BI_RGB;
    DWORD *bits=NULL;
    HBITMAP dib=CreateDIBSection(mem_dc,&bi,DIB_RGB_COLORS,(void**)&bits,NULL,0);
    HBITMAP oldbm=(HBITMAP)SelectObject(mem_dc,dib);

    for (int y=0;y<H;y++)
        memcpy_s(bits+y*W,(size_t)W*4,gl_pix+(H-1-y)*W,(size_t)W*4);
    free(gl_pix);

    {
        COLORREF bc=RGB(g_primary_color.r,g_primary_color.g,g_primary_color.b);
        HPEN pen=CreatePen(PS_SOLID,1,bc);
        HBRUSH nullb=(HBRUSH)GetStockObject(NULL_BRUSH);
        HPEN oldp=(HPEN)SelectObject(mem_dc,pen);
        HBRUSH oldb=(HBRUSH)SelectObject(mem_dc,nullb);
        RoundRect(mem_dc,0,0,W,H,FP_ROUND*2,FP_ROUND*2);
        SelectObject(mem_dc,oldp); SelectObject(mem_dc,oldb);
        DeleteObject(pen);
    }

    const BYTE A=FP_ALPHA;
    for (int i=0;i<W*H;i++){
        DWORD c=bits[i];
        BYTE r=(c>>16)&0xFF, g=(c>>8)&0xFF, b=c&0xFF;
        bits[i]=((DWORD)A<<24)|((DWORD)(r*A/255)<<16)
               |((DWORD)(g*A/255)<<8)|((DWORD)(b*A/255));
    }

    float fr=(float)FP_ROUND;
    for (int y=0;y<H;y++) for (int x=0;x<W;x++){
        float fx=x+0.5f,fy=y+0.5f,ccx=fr,ccy=fr; BOOL in=FALSE;
        if      (fx<fr    &&fy<fr   ){ccx=fr;   ccy=fr;   in=TRUE;}
        else if (fx>W-fr  &&fy<fr   ){ccx=W-fr; ccy=fr;   in=TRUE;}
        else if (fx<fr    &&fy>H-fr ){ccx=fr;   ccy=H-fr; in=TRUE;}
        else if (fx>W-fr  &&fy>H-fr ){ccx=W-fr; ccy=H-fr; in=TRUE;}
        if (!in) continue;
        float dx=fx-ccx,dy=fy-ccy,edge=sqrtf(dx*dx+dy*dy)-fr;
        if (edge>=0.5f){ bits[y*W+x]=0; }
        else if (edge>-0.5f){
            float t=(0.5f-edge)*(FP_ALPHA/255.0f); DWORD c=bits[y*W+x];
            bits[y*W+x]=
                ((DWORD)((BYTE)(((c>>24)&0xFF)*t+0.5f))<<24)|
                ((DWORD)((BYTE)(((c>>16)&0xFF)*t+0.5f))<<16)|
                ((DWORD)((BYTE)(((c>> 8)&0xFF)*t+0.5f))<< 8)|
                ((DWORD)((BYTE)(( c     &0xFF)*t+0.5f)));
        }
    }

    POINT ptSrc={0,0}; SIZE sz={W,H};
    RECT wr; GetWindowRect(g_fp_hwnd,&wr);
    POINT ptDst={wr.left,wr.top};
    BLENDFUNCTION bf={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
    UpdateLayeredWindow(g_fp_hwnd,scr_dc,&ptDst,&sz,mem_dc,&ptSrc,0,&bf,ULW_ALPHA);
    SelectObject(mem_dc,oldbm); DeleteObject(dib);
    DeleteDC(mem_dc); ReleaseDC(NULL,scr_dc);
}

// ═══════════════════════════════════════════════════════════════════
// 히트 테스트
// ═══════════════════════════════════════════════════════════════════

static int fp_hit_item(FpCtx *ctx, int cx, int cy) {
    const float LIST_TOP=(float)FP_LIST_TOP_Y, LIST_BOT=(float)(FP_H-FP_FOOTER_H);
    const float ITEM_H=(float)FP_ITEM_H;
    if (cx<FP_LIST_X||cx>FP_W-FP_LIST_X) return -1;
    if (cy<(int)LIST_TOP||cy>(int)LIST_BOT) return -1;
    int idx=(int)(((float)cy-LIST_TOP+ctx->scroll_y)/ITEM_H);
    if (idx<0||idx>=ctx->filtered_count) return -1;
    return idx;
}

static BOOL fp_hit_ok(int cx, int cy) {
    const float LIST_BOT=(float)(FP_H-FP_FOOTER_H);
    const float BW=72.0f,BH=26.0f;
    float bx=(float)(FP_W-FP_LIST_X)-BW;
    float by=LIST_BOT+((float)FP_FOOTER_H-BH)*0.5f;
    return(cx>=(int)bx&&cx<=(int)(bx+BW)&&cy>=(int)by&&cy<=(int)(by+BH));
}

static BOOL fp_hit_scrollbar(int cx, int cy) {
    float sb_x=(float)(FP_W-FP_LIST_X-FP_SCROLL_W);
    return(cx>=(int)sb_x&&cx<=(int)(sb_x+FP_SCROLL_W)&&
           cy>=(int)FP_LIST_TOP_Y&&cy<=(int)(FP_H-FP_FOOTER_H));
}

static BOOL fp_hit_thumb(FpCtx *ctx, int cy, float *out_thumb_y) {
    const float LIST_TOP=(float)FP_LIST_TOP_Y;
    const float LIST_H  =(float)(FP_H-FP_FOOTER_H-FP_LIST_TOP_Y);
    float th_y, th_h;
    fp_sb_thumb(ctx,LIST_TOP,LIST_H,&th_y,&th_h);
    if (out_thumb_y) *out_thumb_y=th_y;
    return((float)cy>=th_y&&(float)cy<=th_y+th_h);
}

// ═══════════════════════════════════════════════════════════════════
// 선택 확정 헬퍼 – callback + DB 저장 + 창 닫기
// ═══════════════════════════════════════════════════════════════════

static void fp_confirm(HWND hWnd) {
    FpCtx *ctx = (FpCtx *)GetWindowLongPtr(hWnd, 0);
    if (!ctx) { DestroyWindow(hWnd); return; }
    const FpFontItem *item = fp_selected_item(ctx);
    if (item) {
        pxvy_set_subtitle_font(item->family);          // DB 저장
        if (ctx->callback)
            ctx->callback(item->family, ctx->userdata);
    }
    DestroyWindow(hWnd);
}

// ═══════════════════════════════════════════════════════════════════
// WndProc
// ═══════════════════════════════════════════════════════════════════

#define FP_IDT_MOUSE  901

static LRESULT CALLBACK FpWndProc(HWND hWnd, UINT msg,
                                   WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW *cs=(CREATESTRUCTW*)lParam;
        SetWindowLongPtr(hWnd,0,(LONG_PTR)cs->lpCreateParams);
        SetTimer(hWnd,FP_IDT_MOUSE,16,NULL);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps; BeginPaint(hWnd,&ps);
        fp_draw_layered(); EndPaint(hWnd,&ps); return 0;
    }
    case WM_ERASEBKGND: return 1;

    case WM_TIMER: {
        if (wParam!=FP_IDT_MOUSE) return 0;
        FpCtx *ctx=(FpCtx*)GetWindowLongPtr(hWnd,0); if (!ctx) return 0;
        POINT pt; GetCursorPos(&pt);
        RECT wr; GetWindowRect(hWnd,&wr);
        BOOL in_win=PtInRect(&wr,pt);
        int cx=pt.x-wr.left, cy=pt.y-wr.top;
        BOOL nc=FALSE,no=FALSE; int nh=-1;
        if (in_win) {
            nc=(cy>=0&&cy<FP_CAP_H&&cx>=FP_W-FP_BTN_W);
            no=fp_hit_ok(cx,cy);
            nh=fp_hit_item(ctx,cx,cy);
        }
        if (nc!=ctx->close_hover||no!=ctx->ok_hover||nh!=ctx->hovered) {
            ctx->close_hover=nc; ctx->ok_hover=no; ctx->hovered=nh;
            fp_draw_layered();
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        FpCtx *ctx=(FpCtx*)GetWindowLongPtr(hWnd,0); if (!ctx) return 0;
        ctx->scroll_y -= GET_WHEEL_DELTA_WPARAM(wParam) * 0.35f;
        ctx->scroll_y = fmaxf(0.0f, fminf(ctx->scroll_y, ctx->scroll_max));
        fp_draw_layered(); return 0;
    }

    case WM_LBUTTONDOWN: {
        FpCtx *ctx=(FpCtx*)GetWindowLongPtr(hWnd,0); if (!ctx) return 0;
        int cx=(int)(short)LOWORD(lParam), cy=(int)(short)HIWORD(lParam);
        SetFocus(hWnd);

        if (fp_hit_ok(cx,cy)) { fp_confirm(hWnd); return 0; }

        if (fp_hit_scrollbar(cx,cy)) {
            float th_y;
            if (fp_hit_thumb(ctx,cy,&th_y)) {
                ctx->sb_dragging    = TRUE;
                ctx->sb_drag_offset = (float)cy - th_y;
                SetCapture(hWnd);
            } else {
                // 트랙 클릭 → 해당 위치로 이동
                const float LIST_TOP=(float)FP_LIST_TOP_Y;
                const float LIST_H  =(float)(FP_H-FP_FOOTER_H-FP_LIST_TOP_Y);
                float th_h; fp_sb_thumb(ctx,LIST_TOP,LIST_H,NULL,&th_h);
                float center=(float)cy - LIST_TOP - th_h * 0.5f;
                ctx->scroll_y = center / (LIST_H - th_h) * ctx->scroll_max;
                ctx->scroll_y = fmaxf(0.0f, fminf(ctx->scroll_y, ctx->scroll_max));
            }
            fp_draw_layered(); return 0;
        }

        int idx=fp_hit_item(ctx,cx,cy);
        if (idx>=0&&idx!=ctx->selected) {
            ctx->selected=idx; fp_draw_layered();
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        FpCtx *ctx=(FpCtx*)GetWindowLongPtr(hWnd,0);
        if (!ctx||!ctx->sb_dragging) return 0;
        int cy=(int)(short)HIWORD(lParam);
        const float LIST_TOP=(float)FP_LIST_TOP_Y;
        const float LIST_H  =(float)(FP_H-FP_FOOTER_H-FP_LIST_TOP_Y);
        float th_h; fp_sb_thumb(ctx,LIST_TOP,LIST_H,NULL,&th_h);
        ctx->scroll_y = ((float)cy - ctx->sb_drag_offset - LIST_TOP)
                        / (LIST_H - th_h) * ctx->scroll_max;
        ctx->scroll_y = fmaxf(0.0f, fminf(ctx->scroll_y, ctx->scroll_max));
        fp_draw_layered(); return 0;
    }

    case WM_LBUTTONUP: {
        FpCtx *ctx=(FpCtx*)GetWindowLongPtr(hWnd,0);
        if (ctx&&ctx->sb_dragging) {
            ctx->sb_dragging=FALSE; ReleaseCapture(); fp_draw_layered();
        }
        return 0;
    }

    case WM_LBUTTONDBLCLK: {
        FpCtx *ctx=(FpCtx*)GetWindowLongPtr(hWnd,0); if (!ctx) return 0;
        int cx=(int)(short)LOWORD(lParam), cy=(int)(short)HIWORD(lParam);
        int idx=fp_hit_item(ctx,cx,cy);
        if (idx>=0) { ctx->selected=idx; fp_confirm(hWnd); }
        return 0;
    }

    case WM_KEYDOWN: {
        FpCtx *ctx=(FpCtx*)GetWindowLongPtr(hWnd,0);
        switch (wParam) {
        case VK_ESCAPE: DestroyWindow(hWnd); return 0;
        case VK_RETURN: fp_confirm(hWnd); return 0;
        case VK_UP: case VK_DOWN:
            if (ctx&&ctx->filtered_count>0) {
                int d=(wParam==VK_DOWN)?1:-1;
                ctx->selected=(ctx->selected+d+ctx->filtered_count)%ctx->filtered_count;
                const float LH=(float)(FP_H-FP_FOOTER_H-FP_LIST_TOP_Y);
                float iy=ctx->selected*(float)FP_ITEM_H;
                if (iy<ctx->scroll_y) ctx->scroll_y=iy;
                if (iy+FP_ITEM_H>ctx->scroll_y+LH) ctx->scroll_y=iy+FP_ITEM_H-LH;
                ctx->scroll_y=fmaxf(0.0f,fminf(ctx->scroll_y,ctx->scroll_max));
                fp_draw_layered();
            }
            return 0;
        }
        return 0;
    }

    case WM_NCHITTEST: {
        POINT pt={(int)(short)LOWORD(lParam),(int)(short)HIWORD(lParam)};
        RECT wr; GetWindowRect(hWnd,&wr);
        int cx=pt.x-wr.left, cy=pt.y-wr.top;
        if (cy>=0&&cy<FP_CAP_H) {
            if (cx>=FP_W-FP_BTN_W) return HTCLOSE;
            return HTCAPTION;
        }
        return HTCLIENT;
    }
    case WM_NCLBUTTONDOWN:
        if (wParam==HTCLOSE) { DestroyWindow(hWnd); return 0; }
        break;

    case WM_DESTROY: {
        KillTimer(hWnd, FP_IDT_MOUSE);
        FpCtx *ctx=(FpCtx*)GetWindowLongPtr(hWnd,0);
        if (ctx) {
            if (ctx->gl_rc) {
                wglMakeCurrent(ctx->gl_dc, ctx->gl_rc);
                if (ctx->sym_tex) glDeleteTextures(1, &ctx->sym_tex);
                // 이름 텍스처 전체 해제
                if (ctx->name_textures) {
                    for (int i=0; i<ctx->all_count; i++)
                        if (ctx->name_textures[i].tex)
                            glDeleteTextures(1, &ctx->name_textures[i].tex);
                    free(ctx->name_textures);
                }
                if (g_fp_font) { glDeleteLists(g_fp_font,128); g_fp_font=0; }
                wglMakeCurrent(NULL,NULL);
                wglDeleteContext(ctx->gl_rc);
            }
            if (ctx->gl_dc&&g_fp_gl_hwnd)
                ReleaseDC(g_fp_gl_hwnd, ctx->gl_dc);
            free(ctx->all_items); free(ctx->filtered);
            free(ctx); SetWindowLongPtr(hWnd,0,0);
        }
        if (g_fp_gl_hwnd) { DestroyWindow(g_fp_gl_hwnd); g_fp_gl_hwnd=NULL; }
        g_fp_hwnd=NULL; return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ═══════════════════════════════════════════════════════════════════
// 윈도우 클래스 등록
// ═══════════════════════════════════════════════════════════════════

static void fp_register(HINSTANCE hi) {
    static BOOL done=FALSE; if (done) return;
    WNDCLASSA a={0};
    a.lpfnWndProc=DefWindowProcA; a.hInstance=hi;
    a.lpszClassName="PXVYFontPickerGL"; RegisterClassA(&a);
    WNDCLASSW w={0};
    w.lpfnWndProc=FpWndProc; w.hInstance=hi;
    w.lpszClassName=L"PXVYFontPicker";
    w.hCursor=LoadCursor(NULL,IDC_ARROW);
    w.cbWndExtra=sizeof(LONG_PTR); w.style=CS_DBLCLKS;
    RegisterClassW(&w); done=TRUE;
}

// ═══════════════════════════════════════════════════════════════════
// 공개 API
// ═══════════════════════════════════════════════════════════════════
//
// current  : 현재 선택된 패밀리명 (NULL 가능)
// callback : 선택 확정 시 호출 (pxvy_set_subtitle_font 이후)
// userdata : callback 에 그대로 전달
//
// 사용 예:
//   static void on_font(const char *fam, void *ud) {
//       strncpy_s(g_subtitle_font_family, LF_FACESIZE, fam, _TRUNCATE);
//       mpv_set_option_string(mpv, "sub-font", fam);
//   }
//   show_font_picker(g_hwnd, g_subtitle_font_family, on_font, NULL);
//
static void show_font_picker(HWND owner, const char *current,
                              FpCallback callback, void *userdata) {
    if (g_fp_hwnd&&IsWindow(g_fp_hwnd)) { SetForegroundWindow(g_fp_hwnd); return; }

    HINSTANCE hi=GetModuleHandleW(NULL);
    fp_register(hi);

    FpCtx *ctx=(FpCtx*)calloc(1,sizeof(FpCtx));
    if (!ctx) return;
    ctx->callback=callback; ctx->userdata=userdata;
    ctx->hovered=-1; ctx->selected=0;

    fp_enum_system_fonts(ctx);
    fp_build_full_list(ctx);

    // 이름 텍스처 캐시 배열 할당 (0으로 초기화 = 지연 생성)
    ctx->name_textures = (FpNameTex*)calloc(ctx->all_count, sizeof(FpNameTex));

    // current 항목으로 초기 선택
    if (current) {
        for (int fi=0; fi<ctx->filtered_count; fi++) {
            if (_stricmp(ctx->all_items[ctx->filtered[fi]].family, current)==0) {
                ctx->selected=fi; break;
            }
        }
    }

    g_fp_gl_hwnd=CreateWindowA("PXVYFontPickerGL",NULL,WS_POPUP,
                                -FP_W*2,0,FP_W,FP_H,NULL,NULL,hi,NULL);
    if (!g_fp_gl_hwnd) {
        free(ctx->name_textures); free(ctx->all_items);
        free(ctx->filtered); free(ctx); return;
    }

    ctx->gl_dc=GetDC(g_fp_gl_hwnd);
    PIXELFORMATDESCRIPTOR pfd={0};
    pfd.nSize=sizeof(pfd); pfd.nVersion=1;
    pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    pfd.iPixelType=PFD_TYPE_RGBA; pfd.cColorBits=32;
    pfd.cAlphaBits=8; pfd.iLayerType=PFD_MAIN_PLANE;
    int pf=ChoosePixelFormat(ctx->gl_dc,&pfd);
    SetPixelFormat(ctx->gl_dc,pf,&pfd);
    ctx->gl_rc=wglCreateContext(ctx->gl_dc);

    wglMakeCurrent(ctx->gl_dc, ctx->gl_rc);
    fp_init_font(ctx->gl_dc);     // Segoe UI (UI 전용)
    ctx->sym_tex=fp_load_sym_tex();
    wglMakeCurrent(NULL, NULL);

    // 선택 항목 초기 스크롤
    {
        const float LH=(float)(FP_H-FP_FOOTER_H-FP_LIST_TOP_Y);
        float iy=ctx->selected*(float)FP_ITEM_H;
        if (iy+FP_ITEM_H>LH) ctx->scroll_y=iy+FP_ITEM_H-LH;
    }

    int sx=GetSystemMetrics(SM_CXSCREEN), sy=GetSystemMetrics(SM_CYSCREEN);
    g_fp_hwnd=CreateWindowExW(
        WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_LAYERED,
        L"PXVYFontPicker",NULL,
        WS_POPUP|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
        (sx-FP_W)/2,(sy-FP_H)/2,FP_W,FP_H,
        owner,NULL,hi,(LPVOID)ctx);

    if (!g_fp_hwnd) {
        wglMakeCurrent(ctx->gl_dc,ctx->gl_rc);
        if (ctx->sym_tex) glDeleteTextures(1,&ctx->sym_tex);
        wglMakeCurrent(NULL,NULL); wglDeleteContext(ctx->gl_rc);
        ReleaseDC(g_fp_gl_hwnd,ctx->gl_dc);
        DestroyWindow(g_fp_gl_hwnd); g_fp_gl_hwnd=NULL;
        free(ctx->name_textures); free(ctx->all_items);
        free(ctx->filtered); free(ctx); return;
    }

    ShowWindow(g_fp_hwnd, SW_SHOW);
    fp_draw_layered();
    SetForegroundWindow(g_fp_hwnd);
    SetFocus(g_fp_hwnd);
}

#endif // PXVY_FONT_PICKER_H