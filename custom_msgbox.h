// custom_msgbox.h  –  PXVY 커스텀 메시지 박스
//
// 파이프라인: about.h / font_picker.h 와 완전 동일
//   [PXVYMsgBoxGL]  숨겨진 GL 렌더 창
//   UpdateLayeredWindow → [PXVYMsgBox] WS_EX_LAYERED 창
//
// 사용법:
//   // 기본 (확인 버튼만)
//   show_msgbox(hWnd, "제목", "내용 메시지", MB_OK, NULL, NULL);
//
//   // 예/아니오 + 콜백
//   static void on_result(int btn, void *ud) {
//       if (btn == IDYES) { ... }
//   }
//   show_msgbox(hWnd, "확인", "정말 삭제하시겠습니까?",
//               MB_YESNO, on_result, NULL);
//
//   지원 타입: MB_OK / MB_OKCANCEL / MB_YESNO / MB_YESNOCANCEL
//
// 의존: main.c (os_theme, g_primary_color, glColor4f_255,
//               OS_THEME_BK, OS_THEME_FG, OS_THEME_PC,
//               FONT_DEFAULT_FAMILY)
//       stb_image.h, stb_image_resize2.h
#pragma once
#ifndef RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_CUSTOM_MSGBOX_H
#define RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_CUSTOM_MSGBOX_H

#define _USE_MATH_DEFINES
#include <math.h>
#include <windows.h>
#include <GL/gl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif

// ─── 레이아웃 ─────────────────────────────────────────────────────
#define MB_W_MIN        320   // 최소 너비 (텍스트 길이에 따라 자동 확장)
#define MB_H            180
#define MB_CAP_H         32
#define MB_BTN_W         46   // 닫기(×) 버튼
#define MB_SYM_SZ        20   // symbol.png
#define MB_ROUND         10
#define MB_FONT_H        20   // = ABOUT_FONT_H
#define MB_ALPHA        240
#define MB_BTN_HEIGHT    28   // 하단 액션 버튼 높이
#define MB_BTN_MIN_W     72   // 최소 버튼 너비
#define MB_FOOTER_H      48   // 하단 버튼 영역
char MB_TITLE[1024] = {0};

// ─── 버튼 ID (WinAPI 호환) ────────────────────────────────────────
#ifndef IDOK
#define IDOK      1
#define IDCANCEL  2
#define IDYES     6
#define IDNO      7
#endif

// ─── 콜백 ─────────────────────────────────────────────────────────
typedef void (*MbCallback)(int button_id, void *userdata);

// ─── 버튼 정의 ────────────────────────────────────────────────────
typedef struct {
    int   id;
    char  label[32];
    float x, w;          // 렌더 시 계산
    BOOL  hover;
    BOOL  is_primary;    // TRUE = primary color, FALSE = secondary
} MbButton;

// ─── 컨텍스트 ─────────────────────────────────────────────────────
typedef struct {
    HDC    gl_dc;
    HGLRC  gl_rc;
    GLuint sym_tex;

    // 텍스처 캐시 (title, message – GDI 렌더)
    GLuint title_tex;   int title_w,   title_h;
    GLuint msg_tex;     int msg_w,     msg_h;

    char   title[256];
    char   message[512];

    MbButton btns[3];
    int      btn_count;
    int      dyn_w;            // 동적으로 계산된 창 너비

    BOOL   close_hover;

    HWND   owner;          // 모달: 비활성화할 부모 창

    MbCallback callback;
    void      *userdata;
} MbCtx;

// ─── 전역 핸들 ────────────────────────────────────────────────────
static HWND g_mb_hwnd    = NULL;
static HWND g_mb_gl_hwnd = NULL;

// ─── GL 폰트 (UI/버튼 라벨: FONT_DEFAULT_FAMILY) ─────────────────
static GLuint            g_mb_font  = 0;
static GLYPHMETRICSFLOAT g_mb_glyph[128];

// ═══════════════════════════════════════════════════════════════════
// GDI 텍스처 (한글/유니코드 지원)
// ═══════════════════════════════════════════════════════════════════

static GLuint mb_make_tex(const char *text, const char *font_name,
                           int font_h, int bold,
                           int *out_w, int *out_h) {
    WCHAR wtext[512] = {0}, wfont[LF_FACESIZE] = {0};
    MultiByteToWideChar(CP_UTF8, 0, text,      -1, wtext,  512);
    MultiByteToWideChar(CP_UTF8, 0, font_name, -1, wfont,  LF_FACESIZE);

    HDC src_dc = GetDC(NULL);
    HDC mem_dc = CreateCompatibleDC(src_dc);

    HFONT hf = CreateFontW(-font_h, 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, wfont);
    HFONT old_f = (HFONT)SelectObject(mem_dc, hf);

    SIZE sz = {0};
    GetTextExtentPoint32W(mem_dc, wtext, (int)wcslen(wtext), &sz);
    int w = sz.cx + 4, h = sz.cy + 4;
    if (w < 2) w = 2;
    if (h < 2) h = 2;

    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    DWORD *bits = NULL;
    HBITMAP dib   = CreateDIBSection(mem_dc, &bi, DIB_RGB_COLORS,
                                     (void **)&bits, NULL, 0);
    HBITMAP oldbm = (HBITMAP)SelectObject(mem_dc, dib);

    memset(bits, 0, (size_t)w * h * 4);
    SetBkMode(mem_dc, OPAQUE);
    SetBkColor(mem_dc, RGB(0, 0, 0));
    SetTextColor(mem_dc, RGB(255, 255, 255));
    TextOutW(mem_dc, 2, 2, wtext, (int)wcslen(wtext));
    GdiFlush();

    // R → alpha, RGB = 0xFFFFFF
    for (int i = 0; i < w * h; i++) {
        BYTE r = (bits[i] >> 16) & 0xFF;
        bits[i] = ((DWORD)r << 24) | 0x00FFFFFF;
    }

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

// glColor 로 색상 변조하는 텍스처 쿼드
static void mb_draw_tex(GLuint tex, float x, float y, float w, float h) {
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
// GL 유틸리티
// ═══════════════════════════════════════════════════════════════════

static void mb_ortho(int w, int h) {
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);  glLoadIdentity();
}

static void mb_init_font(HDC hdc) {
    if (g_mb_font) return;
    g_mb_font = glGenLists(128);
    HFONT hf = CreateFontA(-MB_FONT_H, 0, 0, 0, FW_BOLD, 0, 0, 0,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, FONT_DEFAULT_FAMILY);
    HFONT old = (HFONT)SelectObject(hdc, hf);
    wglUseFontOutlinesA(hdc, 0, 128, g_mb_font,
                        0.0f, 0.0f, WGL_FONT_POLYGONS, g_mb_glyph);
    SelectObject(hdc, old); DeleteObject(hf);
}

static float mb_str_w(const char *s, float scale) {
    float sz = MB_FONT_H * scale, w = 0.0f;
    for (const char *p = s; *p; p++) {
        int i = (unsigned char)*p;
        if (i < 128) w += g_mb_glyph[i].gmfCellIncX * sz;
    }
    return w;
}

static void mb_str(float x, float y, const char *s, float scale) {
    float sz = MB_FONT_H * scale;
    glPushMatrix(); glTranslatef(x, y, 0.0f); glScalef(sz, -sz, 1.0f);
    glPushAttrib(GL_LIST_BIT); glListBase(g_mb_font);
    glCallLists((GLsizei)strlen(s), GL_UNSIGNED_BYTE, s);
    glPopAttrib(); glPopMatrix();
}

static void mb_rect(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
    glVertex2f(x,y); glVertex2f(x+w,y); glVertex2f(x+w,y+h); glVertex2f(x,y+h);
    glEnd();
}

static void mb_quad_tex(GLuint tex, float x, float y, float w, float h) {
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

// ═══════════════════════════════════════════════════════════════════
// GDI 텍스트 너비 측정 (GL 컨텍스트 불필요)
// ═══════════════════════════════════════════════════════════════════

static int mb_measure_text_w(const char *text, const char *font_name, int font_h) {
    WCHAR wtext[512] = {0}, wfont[LF_FACESIZE] = {0};
    MultiByteToWideChar(CP_UTF8, 0, text,      -1, wtext, 512);
    MultiByteToWideChar(CP_UTF8, 0, font_name, -1, wfont, LF_FACESIZE);
    HDC hdc = GetDC(NULL);
    HFONT hf = CreateFontW(-font_h, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, wfont);
    HFONT old = (HFONT)SelectObject(hdc, hf);
    SIZE sz = {0};
    GetTextExtentPoint32W(hdc, wtext, (int)wcslen(wtext), &sz);
    SelectObject(hdc, old);
    DeleteObject(hf);
    ReleaseDC(NULL, hdc);
    return sz.cx;
}

static GLuint mb_load_sym_tex(void) {
    char exe[MAX_PATH], sym[MAX_PATH];
    GetModuleFileNameA(NULL, exe, MAX_PATH);
    char *sl = strrchr(exe, '\\'); if (sl) *(sl+1) = '\0';
    snprintf(sym, MAX_PATH, "%ssymbol.png", exe);
    int sw, sh, ch;
    unsigned char *src = stbi_load(sym, &sw, &sh, &ch, 4);
    if (!src) return 0;
    unsigned char *dst = (unsigned char *)malloc((size_t)MB_SYM_SZ*MB_SYM_SZ*4);
    if (!dst) { stbi_image_free(src); return 0; }
    stbir_resize_uint8_linear(src, sw, sh, 0, dst, MB_SYM_SZ, MB_SYM_SZ, 0, STBIR_RGBA);
    stbi_image_free(src);
    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, MB_SYM_SZ, MB_SYM_SZ, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, dst);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0); free(dst);
    return t;
}

// ═══════════════════════════════════════════════════════════════════
// 버튼 레이아웃 계산
// ═══════════════════════════════════════════════════════════════════

static void mb_layout_buttons(MbCtx *ctx) {
    const float W       = (float)ctx->dyn_w;
    const float MARGIN  = 12.0f;
    const float GAP     = 8.0f;
    const float BH      = MB_BTN_HEIGHT;
    const float BY      = MB_H - MB_FOOTER_H + (MB_FOOTER_H - BH) * 0.5f;

    int n = ctx->btn_count;
    // 버튼 너비: 라벨 기준으로 최소 MB_BTN_MIN_W
    float total_w = 0.0f;
    for (int i = 0; i < n; i++) {
        float tw = mb_str_w(ctx->btns[i].label, 0.75f) + 24.0f;
        ctx->btns[i].w = (tw < MB_BTN_MIN_W) ? MB_BTN_MIN_W : tw;
        total_w += ctx->btns[i].w;
    }
    total_w += GAP * (n - 1);

    // 오른쪽 정렬
    float start_x = W - MARGIN - total_w;
    for (int i = 0; i < n; i++) {
        ctx->btns[i].x = start_x;
        start_x += ctx->btns[i].w + GAP;
    }
    (void)BY;
}

// ═══════════════════════════════════════════════════════════════════
// GL 렌더
// ═══════════════════════════════════════════════════════════════════

static void mb_render_gl(MbCtx *ctx) {
    const float W    = (float)ctx->dyn_w, H = MB_H;
    const float capH = (float)MB_CAP_H;

    glViewport(0, 0, ctx->dyn_w, MB_H);
    glClearColor(os_theme.background.r/255.0f,
                 os_theme.background.g/255.0f,
                 os_theme.background.b/255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    mb_ortho(ctx->dyn_w, MB_H);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ── 1. 전체 배경 ──
    glColor4f_255(OS_THEME_BK, 1.0f);
    mb_rect(0, 0, W, H);

    // ── 2. 캡션 바 (primary color) ── about.h 동일
    glColor4f_255(OS_THEME_PC, 1.0f);
    mb_rect(0, 0, W, capH);

    // ── 3. 캡션 아이콘 ── about.h: lx=15, ly=(capH-lh)*0.5+1
    if (ctx->sym_tex) {
        float lw = MB_SYM_SZ, lh = MB_SYM_SZ;
        float lx = 15.0f, ly = (capH - lh) * 0.5f + 1.0f;
        mb_quad_tex(ctx->sym_tex, lx, ly, lw, lh);
    }

    // ── 4. 캡션 "ERROR" ── about.h 완전 동일 (wgl outline, ty=capH*0.70f)
    {
        float tx = 15.0f + (float)MB_SYM_SZ + 6.0f;
        float ty = capH * 0.70f;
        glEnable(GL_POLYGON_SMOOTH);
        glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
        // 그림자
        glColor4f(0.0f, 0.0f, 0.0f, 0.4f);
        mb_str(tx + 1.0f, ty + 1.0f, MB_TITLE, 1.0f);
        // 본문 (흰색)
        glColor4f(1.0f, 1.0f, 1.0f, 0.95f);
        mb_str(tx, ty, MB_TITLE, 1.0f);
        glDisable(GL_POLYGON_SMOOTH);
    }

    // ── 5. 닫기 버튼 (×) ── hover 시 빨간색
    {
        float bW = (float)MB_BTN_W, bH = capH, cL = W - bW;
        float cx = cL + bW * 0.5f, cy = bH * 0.5f, s = 5.0f;
        if (ctx->close_hover) {
            glColor4f(0.90f, 0.15f, 0.15f, 1.0f);   // 빨간색
            mb_rect(cL, 0, bW, bH);
        }
        glColor4f(1.0f, 1.0f, 1.0f, 0.9f);
        glLineWidth(1.5f);
        glBegin(GL_LINES);
        glVertex2f(cx-s, cy-s); glVertex2f(cx+s, cy+s);
        glVertex2f(cx+s, cy-s); glVertex2f(cx-s, cy+s);
        glEnd();
        glLineWidth(1.0f);
    }

    // ── 6. 메시지 본문 ──
    if (ctx->msg_tex) {
        float ty = capH + (H - MB_FOOTER_H - capH - (float)ctx->msg_h) * 0.5f
                   + capH;
        // 실제로는 본문 영역 중앙
        float content_top    = capH + 8.0f;
        float content_bottom = H - (float)MB_FOOTER_H;
        float content_h      = content_bottom - content_top;
        float msg_y = content_top + (content_h - (float)ctx->msg_h) * 0.5f;
        float msg_x = 20.0f;

        glEnable(GL_POLYGON_SMOOTH);
        glColor4f_255(OS_THEME_FG, 0.90f);
        mb_draw_tex(ctx->msg_tex, msg_x, msg_y,
                    (float)ctx->msg_w, (float)ctx->msg_h);
        glDisable(GL_POLYGON_SMOOTH);
    }

    // ── 7. 하단 구분선 ──
    const float footer_top = H - (float)MB_FOOTER_H;
    glColor4f_255(OS_THEME_PC, 0.30f);
    glBegin(GL_LINES);
    glVertex2f(16.0f,     footer_top + 2.0f);
    glVertex2f(W - 16.0f, footer_top + 2.0f);
    glEnd();

    // ── 8. 하단 액션 버튼 ──
    const float BH  = MB_BTN_HEIGHT;
    const float BY  = footer_top + ((float)MB_FOOTER_H - BH) * 0.5f;

    glEnable(GL_POLYGON_SMOOTH);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

    for (int i = 0; i < ctx->btn_count; i++) {
        MbButton *b = &ctx->btns[i];

        // 버튼 배경
        if (b->is_primary)
            glColor4f_255(OS_THEME_PC, b->hover ? 1.0f : 0.85f);
        else
            glColor4f_255(OS_THEME_FG, b->hover ? 0.16f : 0.09f);
        mb_rect(b->x, BY, b->w, BH);

        // 버튼 라벨 (ASCII → wgl outline)
        float tw  = mb_str_w(b->label, 0.75f);
        float tx2 = b->x + (b->w - tw) * 0.5f;
        float ty2 = BY + BH * 0.70f;
        if (b->is_primary)
            glColor4f(1.0f, 1.0f, 1.0f, 0.95f);
        else
            glColor4f_255(OS_THEME_FG, 0.85f);
        mb_str(tx2, ty2, b->label, 0.75f);
    }

    glDisable(GL_POLYGON_SMOOTH);
    glFlush();
}

// ═══════════════════════════════════════════════════════════════════
// UpdateLayeredWindow 파이프라인 (about.h 완전 동일)
// ═══════════════════════════════════════════════════════════════════

static void mb_draw_layered(void) {
    if (!g_mb_hwnd || !g_mb_gl_hwnd) return;
    MbCtx *ctx = (MbCtx *)GetWindowLongPtr(g_mb_hwnd, 0);
    if (!ctx || !ctx->gl_rc) return;

    const int W = ctx->dyn_w, H = MB_H;
    wglMakeCurrent(ctx->gl_dc, ctx->gl_rc);
    mb_render_gl(ctx);

    DWORD *gl_pix = (DWORD *)malloc((size_t)W*H*4);
    if (!gl_pix) { wglMakeCurrent(NULL,NULL); return; }
    glReadPixels(0,0,W,H,GL_BGRA_EXT,GL_UNSIGNED_BYTE,gl_pix);
    wglMakeCurrent(NULL,NULL);

    HDC scr_dc=GetDC(NULL), mem_dc=CreateCompatibleDC(scr_dc);
    BITMAPINFO bi={0};
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth=W; bi.bmiHeader.biHeight=-H;
    bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32;
    bi.bmiHeader.biCompression=BI_RGB;
    DWORD *bits=NULL;
    HBITMAP dib=CreateDIBSection(mem_dc,&bi,DIB_RGB_COLORS,(void**)&bits,NULL,0);
    HBITMAP oldbm=(HBITMAP)SelectObject(mem_dc,dib);

    for(int y=0;y<H;y++)
        memcpy_s(bits+y*W,(size_t)W*4,gl_pix+(H-1-y)*W,(size_t)W*4);
    free(gl_pix);

    // GDI RoundRect 테두리
    {
        COLORREF bc=RGB(g_primary_color.r,g_primary_color.g,g_primary_color.b);
        HPEN pen=CreatePen(PS_SOLID,1,bc);
        HBRUSH nullb=(HBRUSH)GetStockObject(NULL_BRUSH);
        HPEN oldp=(HPEN)SelectObject(mem_dc,pen);
        HBRUSH oldb=(HBRUSH)SelectObject(mem_dc,nullb);
        RoundRect(mem_dc,0,0,W,H,MB_ROUND*2,MB_ROUND*2);
        SelectObject(mem_dc,oldp); SelectObject(mem_dc,oldb);
        DeleteObject(pen);
    }

    // Premultiplied alpha
    const BYTE A=MB_ALPHA;
    for(int i=0;i<W*H;i++){
        DWORD c=bits[i];
        BYTE r=(c>>16)&0xFF, g=(c>>8)&0xFF, b=c&0xFF;
        bits[i]=((DWORD)A<<24)|((DWORD)(r*A/255)<<16)
               |((DWORD)(g*A/255)<<8)|((DWORD)(b*A/255));
    }

    // AA 모서리
    float fr=(float)MB_ROUND;
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        float fx=x+0.5f,fy=y+0.5f,ccx=fr,ccy=fr; BOOL in=FALSE;
        if     (fx<fr    &&fy<fr   ){ccx=fr;   ccy=fr;   in=TRUE;}
        else if(fx>W-fr  &&fy<fr   ){ccx=W-fr; ccy=fr;   in=TRUE;}
        else if(fx<fr    &&fy>H-fr ){ccx=fr;   ccy=H-fr; in=TRUE;}
        else if(fx>W-fr  &&fy>H-fr ){ccx=W-fr; ccy=H-fr; in=TRUE;}
        if(!in) continue;
        float dx=fx-ccx,dy=fy-ccy,edge=sqrtf(dx*dx+dy*dy)-fr;
        if(edge>=0.5f){ bits[y*W+x]=0; }
        else if(edge>-0.5f){
            float t=(0.5f-edge)*(MB_ALPHA/255.0f); DWORD c=bits[y*W+x];
            bits[y*W+x]=
                ((DWORD)((BYTE)(((c>>24)&0xFF)*t+0.5f))<<24)|
                ((DWORD)((BYTE)(((c>>16)&0xFF)*t+0.5f))<<16)|
                ((DWORD)((BYTE)(((c>> 8)&0xFF)*t+0.5f))<< 8)|
                ((DWORD)((BYTE)(( c     &0xFF)*t+0.5f)));
        }
    }

    POINT ptSrc={0,0}; SIZE sz={W,H};
    RECT wr; GetWindowRect(g_mb_hwnd,&wr);
    POINT ptDst={wr.left,wr.top};
    BLENDFUNCTION bf={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
    UpdateLayeredWindow(g_mb_hwnd,scr_dc,&ptDst,&sz,mem_dc,&ptSrc,0,&bf,ULW_ALPHA);
    SelectObject(mem_dc,oldbm); DeleteObject(dib);
    DeleteDC(mem_dc); ReleaseDC(NULL,scr_dc);
}

// ═══════════════════════════════════════════════════════════════════
// 히트 테스트
// ═══════════════════════════════════════════════════════════════════

static int mb_hit_btn(MbCtx *ctx, int cx, int cy) {
    const float BH = MB_BTN_HEIGHT;
    const float BY = (float)(MB_H - MB_FOOTER_H)
                   + ((float)MB_FOOTER_H - BH) * 0.5f;
    if ((float)cy < BY || (float)cy > BY + BH) return -1;
    for (int i = 0; i < ctx->btn_count; i++) {
        MbButton *b = &ctx->btns[i];
        if ((float)cx >= b->x && (float)cx <= b->x + b->w)
            return i;
    }
    return -1;
}

// ═══════════════════════════════════════════════════════════════════
// WndProc
// ═══════════════════════════════════════════════════════════════════

#define MB_IDT_MOUSE  951

static LRESULT CALLBACK MbWndProc(HWND hWnd, UINT msg,
                                   WPARAM wParam, LPARAM lParam) {
    switch(msg){
    case WM_CREATE:{
        CREATESTRUCTW *cs=(CREATESTRUCTW*)lParam;
        SetWindowLongPtr(hWnd,0,(LONG_PTR)cs->lpCreateParams);
        SetTimer(hWnd,MB_IDT_MOUSE,16,NULL);
        return 0;
    }
    case WM_PAINT:{
        PAINTSTRUCT ps; BeginPaint(hWnd,&ps);
        mb_draw_layered(); EndPaint(hWnd,&ps); return 0;
    }
    case WM_ERASEBKGND: return 1;

    case WM_TIMER:{
        if(wParam!=MB_IDT_MOUSE) return 0;
        MbCtx *ctx=(MbCtx*)GetWindowLongPtr(hWnd,0); if(!ctx) return 0;
        POINT pt; GetCursorPos(&pt);
        RECT wr; GetWindowRect(hWnd,&wr);
        BOOL in_win=PtInRect(&wr,pt);
        int cx=pt.x-wr.left, cy=pt.y-wr.top;

        BOOL nc=FALSE; BOOL dirty=FALSE;
        if(in_win) nc=(cy>=0&&cy<MB_CAP_H&&cx>=ctx->dyn_w-MB_BTN_W);
        if(nc!=ctx->close_hover){ ctx->close_hover=nc; dirty=TRUE; }

        int hi=in_win ? mb_hit_btn(ctx,cx,cy) : -1;
        for(int i=0;i<ctx->btn_count;i++){
            BOOL hv=(i==hi);
            if(hv!=ctx->btns[i].hover){ ctx->btns[i].hover=hv; dirty=TRUE; }
        }
        if(dirty) mb_draw_layered();
        return 0;
    }

    case WM_LBUTTONDOWN:{
        MbCtx *ctx=(MbCtx*)GetWindowLongPtr(hWnd,0); if(!ctx) return 0;
        int cx=(int)(short)LOWORD(lParam), cy=(int)(short)HIWORD(lParam);
        int bi=mb_hit_btn(ctx,cx,cy);
        if(bi>=0){
            int id=ctx->btns[bi].id;
            MbCallback cb=ctx->callback; void *ud=ctx->userdata;
            DestroyWindow(hWnd);
            if(cb) cb(id,ud);
            return 0;
        }
        return 0;
    }

    case WM_KEYDOWN:
        switch(wParam){
        case VK_ESCAPE:{
            MbCtx *ctx=(MbCtx*)GetWindowLongPtr(hWnd,0);
            MbCallback cb=ctx?ctx->callback:NULL; void *ud=ctx?ctx->userdata:NULL;
            DestroyWindow(hWnd);
            if(cb) cb(IDCANCEL,ud);
            return 0;
        }
        case VK_SPACE:
        case VK_RETURN:{
            MbCtx *ctx=(MbCtx*)GetWindowLongPtr(hWnd,0);
            if(ctx){
                // primary 버튼 우선
                for(int i=0;i<ctx->btn_count;i++){
                    if(ctx->btns[i].is_primary){
                        int id=ctx->btns[i].id;
                        MbCallback cb=ctx->callback; void *ud=ctx->userdata;
                        DestroyWindow(hWnd);
                        if(cb) cb(id,ud);
                        return 0;
                    }
                }
                // primary 없으면 마지막 버튼 (MB_OK 등)
                if(ctx->btn_count>0){
                    int id=ctx->btns[ctx->btn_count-1].id;
                    MbCallback cb=ctx->callback; void *ud=ctx->userdata;
                    DestroyWindow(hWnd);
                    if(cb) cb(id,ud);
                }
            }
            return 0;
        }
        }
        return 0;

    case WM_NCHITTEST:{
        POINT pt={(int)(short)LOWORD(lParam),(int)(short)HIWORD(lParam)};
        RECT wr; GetWindowRect(hWnd,&wr);
        int cx=pt.x-wr.left, cy=pt.y-wr.top;
        MbCtx *ctx=(MbCtx*)GetWindowLongPtr(hWnd,0);
        int dw = ctx ? ctx->dyn_w : MB_W_MIN;
        if(cy>=0&&cy<MB_CAP_H){
            if(cx>=dw-MB_BTN_W) return HTCLOSE;
            return HTCAPTION;
        }
        return HTCLIENT;
    }
    case WM_NCLBUTTONDOWN:
        if(wParam==HTCLOSE){
            MbCtx *ctx=(MbCtx*)GetWindowLongPtr(hWnd,0);
            MbCallback cb=ctx?ctx->callback:NULL; void *ud=ctx?ctx->userdata:NULL;
            DestroyWindow(hWnd);
            if(cb) cb(IDCANCEL,ud);
            return 0;
        }
        break;

    case WM_DESTROY:{
        KillTimer(hWnd,MB_IDT_MOUSE);
        MbCtx *ctx=(MbCtx*)GetWindowLongPtr(hWnd,0);
        if(ctx){
            // 모달 해제: owner 재활성화 후 포커스 복원
            if(ctx->owner && IsWindow(ctx->owner)){
                EnableWindow(ctx->owner, TRUE);
                SetForegroundWindow(ctx->owner);
            }
            if(ctx->gl_rc){
                wglMakeCurrent(ctx->gl_dc,ctx->gl_rc);
                if(ctx->sym_tex)   glDeleteTextures(1,&ctx->sym_tex);
                if(ctx->title_tex) glDeleteTextures(1,&ctx->title_tex);
                if(ctx->msg_tex)   glDeleteTextures(1,&ctx->msg_tex);
                if(g_mb_font){ glDeleteLists(g_mb_font,128); g_mb_font=0; }
                wglMakeCurrent(NULL,NULL);
                wglDeleteContext(ctx->gl_rc);
            }
            if(ctx->gl_dc&&g_mb_gl_hwnd)
                ReleaseDC(g_mb_gl_hwnd,ctx->gl_dc);
            free(ctx); SetWindowLongPtr(hWnd,0,0);
        }
        if(g_mb_gl_hwnd){ DestroyWindow(g_mb_gl_hwnd); g_mb_gl_hwnd=NULL; }
        g_mb_hwnd=NULL; return 0;
    }
    }
    return DefWindowProcW(hWnd,msg,wParam,lParam);
}

// ═══════════════════════════════════════════════════════════════════
// 윈도우 클래스 등록
// ═══════════════════════════════════════════════════════════════════

static void mb_register(HINSTANCE hi){
    static BOOL done=FALSE; if(done) return;
    WNDCLASSA a={0};
    a.lpfnWndProc=DefWindowProcA; a.hInstance=hi;
    a.lpszClassName="PXVYMsgBoxGL"; RegisterClassA(&a);
    WNDCLASSW w={0};
    w.lpfnWndProc=MbWndProc; w.hInstance=hi;
    w.lpszClassName=L"PXVYMsgBox";
    w.hCursor=LoadCursor(NULL,IDC_ARROW);
    w.cbWndExtra=sizeof(LONG_PTR);
    RegisterClassW(&w); done=TRUE;
}

// ═══════════════════════════════════════════════════════════════════
// 공개 API
// ═══════════════════════════════════════════════════════════════════
//
// type     : MB_OK / MB_OKCANCEL / MB_YESNO / MB_YESNOCANCEL
// callback : NULL 가능 (결과 무시)
//
// 사용 예:
//   show_msgbox(hWnd, "삭제", "파일을 삭제하시겠습니까?",
//               MB_YESNO, on_result, NULL);
//
static void show_msgbox(HWND owner,
                         const char *title,
                         const char *message,
                         UINT type,
                         MbCallback callback,
                         void *userdata) {
    if(g_mb_hwnd&&IsWindow(g_mb_hwnd)){SetForegroundWindow(g_mb_hwnd);return;}

    HINSTANCE hi=GetModuleHandleW(NULL);
    mb_register(hi);

    MbCtx *ctx=(MbCtx*)calloc(1,sizeof(MbCtx));
    if(!ctx) return;
    ctx->callback=callback; ctx->userdata=userdata;
    ctx->owner=owner;       // 모달용 부모 창 저장
    strncpy_s(ctx->title,   sizeof(ctx->title),   title   ? title   : "", _TRUNCATE);
    strncpy_s(ctx->message, sizeof(ctx->message), message ? message : "", _TRUNCATE);

    strncpy(MB_TITLE,title,strlen(title));

    // ── 동적 너비 계산 ──
    // 메시지 텍스트 너비 측정 (GDI, GL 컨텍스트 불필요)
    int msg_px  = mb_measure_text_w(ctx->message, FONT_DEFAULT_FAMILY,
                                     MB_FONT_H - 4);
    // 버튼 영역 너비 추정: 버튼 수 × 최소폭 + 여백
    UINT t_type = type & 0x0F;
    int  nb     = (t_type == MB_OK) ? 1
                : (t_type == MB_YESNOCANCEL) ? 3 : 2;
    int  btn_px = nb * MB_BTN_MIN_W + (nb - 1) * 8 + 24;  // GAP=8, MARGIN=12×2
    // 텍스트 좌우 패딩 40px (msg_x=20 × 2)
    int  need_w = msg_px + 40;
    if (btn_px > need_w) need_w = btn_px;
    // 최솟값 MB_W_MIN, 최댓값 화면 80%
    int  scr_w  = GetSystemMetrics(SM_CXSCREEN);
    if (need_w < MB_W_MIN) need_w = MB_W_MIN;
    if (need_w > scr_w * 4 / 5) need_w = scr_w * 4 / 5;
    ctx->dyn_w = need_w;

    // 버튼 구성
    UINT t = type & 0x0F;
    if(t==MB_OK){
        ctx->btn_count=1;
        ctx->btns[0]=(MbButton){IDOK,     "OK",     0,0,0,FALSE};
    } else if(t==MB_OKCANCEL){
        ctx->btn_count=2;
        ctx->btns[0]=(MbButton){IDCANCEL, "Cancel", 0,0,0,FALSE};
        ctx->btns[1]=(MbButton){IDOK,     "OK",     0,0,0,FALSE};
    } else if(t==MB_YESNO){
        ctx->btn_count=2;
        ctx->btns[0]=(MbButton){IDNO,     "No",     0,0,0,FALSE};
        ctx->btns[1]=(MbButton){IDYES,    "Yes",    0,0,0,FALSE};
    } else { // MB_YESNOCANCEL
        ctx->btn_count=3;
        ctx->btns[0]=(MbButton){IDCANCEL, "Cancel", 0,0,0,FALSE};
        ctx->btns[1]=(MbButton){IDNO,     "No",     0,0,0,FALSE};
        ctx->btns[2]=(MbButton){IDYES,    "Yes",    0,0,0,FALSE};
    }

    // 숨겨진 GL 창
    g_mb_gl_hwnd=CreateWindowA("PXVYMsgBoxGL",NULL,WS_POPUP,
                                -ctx->dyn_w*2,0,ctx->dyn_w,MB_H,NULL,NULL,hi,NULL);
    if(!g_mb_gl_hwnd){ free(ctx); return; }

    ctx->gl_dc=GetDC(g_mb_gl_hwnd);
    PIXELFORMATDESCRIPTOR pfd={0};
    pfd.nSize=sizeof(pfd); pfd.nVersion=1;
    pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    pfd.iPixelType=PFD_TYPE_RGBA; pfd.cColorBits=32;
    pfd.cAlphaBits=8; pfd.iLayerType=PFD_MAIN_PLANE;
    int pf=ChoosePixelFormat(ctx->gl_dc,&pfd);
    SetPixelFormat(ctx->gl_dc,pf,&pfd);
    ctx->gl_rc=wglCreateContext(ctx->gl_dc);

    wglMakeCurrent(ctx->gl_dc,ctx->gl_rc);
    mb_init_font(ctx->gl_dc);
    ctx->sym_tex=mb_load_sym_tex();

    // 버튼 레이아웃 (폰트 glyph 필요하므로 GL 컨텍스트 안에서)
    mb_layout_buttons(ctx);

    // 메시지 GDI 텍스처 생성 (title은 wgl outline으로 렌더하므로 불필요)
    ctx->msg_tex   = mb_make_tex(ctx->message, FONT_DEFAULT_FAMILY,
                                  MB_FONT_H - 4, 0,
                                  &ctx->msg_w, &ctx->msg_h);
    wglMakeCurrent(NULL,NULL);

    int sx=GetSystemMetrics(SM_CXSCREEN), sy=GetSystemMetrics(SM_CYSCREEN);
    g_mb_hwnd=CreateWindowExW(
        WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_LAYERED,
        L"PXVYMsgBox",NULL,
        WS_POPUP|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
        (sx-ctx->dyn_w)/2,(sy-MB_H)/2,ctx->dyn_w,MB_H,
        owner,NULL,hi,(LPVOID)ctx);

    if(!g_mb_hwnd){
        wglMakeCurrent(ctx->gl_dc,ctx->gl_rc);
        if(ctx->sym_tex)   glDeleteTextures(1,&ctx->sym_tex);
        if(ctx->title_tex) glDeleteTextures(1,&ctx->title_tex);
        if(ctx->msg_tex)   glDeleteTextures(1,&ctx->msg_tex);
        wglMakeCurrent(NULL,NULL); wglDeleteContext(ctx->gl_rc);
        ReleaseDC(g_mb_gl_hwnd,ctx->gl_dc);
        DestroyWindow(g_mb_gl_hwnd); g_mb_gl_hwnd=NULL;
        free(ctx); return;
    }

    // 모달: owner 비활성화 → msgbox 닫힐 때 WM_DESTROY 에서 복원
    if(owner && IsWindow(owner))
        EnableWindow(owner, FALSE);

    ShowWindow(g_mb_hwnd,SW_SHOW);
    mb_draw_layered();
    SetForegroundWindow(g_mb_hwnd);
    SetFocus(g_mb_hwnd);
}

#endif // RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_CUSTOM_MSGBOX_H