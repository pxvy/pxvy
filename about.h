// about.h  –  PXVY About 다이얼로그 (OpenGL 렌더 + GDI 테두리 + UpdateLayeredWindow)
//
// 파이프라인:
//   [PXVYAboutGL]  숨겨진 창  →  GL 렌더 (백버퍼)
//   glReadPixels   bottom-up BGRA
//   Y-flip         → top-down DIB
//   premult alpha  } context_menu.h 완전 동일
//   AA corner      }
//   GDI RoundRect  → primary color 테두리 (끊김 없음)
//   UpdateLayeredWindow  → [PXVYAbout] WS_EX_LAYERED 창에 합성
//
// 의존: main.c (os_theme, g_primary_color, glColor4f_255,
//               OS_THEME_BK, OS_THEME_FG, OS_THEME_PC)
//       stb_image.h, stb_image_resize2.h  (main.c 에서 이미 구현 포함)
#pragma once
#ifndef RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_ABOUT_H
#define RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_ABOUT_H

#define _USE_MATH_DEFINES
#include <math.h>
#ifndef M_PI
#define M_PI acos(-1)
#endif

#include <windows.h>
#include <GL/gl.h>
#include <dwmapi.h>
#include <stdlib.h>
#include <string.h>
#include "version.h"
#include "compiler.h"
#include "build_time.h"

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif

// ─── 레이아웃 상수 ──────────────────────────────────────────────── 
#define ABOUT_W        460
#define ABOUT_H        300
#define ABOUT_CAP_H     32      // = CAPTION_HEIGHT                 
#define ABOUT_BTN_W     46      // = CAPTION_BTN_W                  
#define ABOUT_SYM_SZ    20      // symbol.png 렌더/리사이즈 크기    
#define ABOUT_LOGO_SZ   72      // logo.png 최대 크기               
#define ABOUT_ROUND     10      // = MENU_ROUND (context_menu.h)    
#define ABOUT_FONT_H    20      // = g_font_height (main.c)
#define ABOUT_ALPHA    210      // 창 전체 불투명도 (0=완전투명 ~ 255=불투명)
#define ABOUT_TITLE "ABOUT"

#define APP_NAME_MARGIN 20.f
#define APP_INFO_MARGIN 20.f

// ─── 전역 핸들 ───────────────────────────────────────────────────
static HWND g_about_hwnd = NULL; // 표시창 (WS_EX_LAYERED)
static HWND g_about_gl_hwnd = NULL; // 숨겨진 GL 렌더 전용 창

// ─── About 전용 폰트 (독립, wglShareLists 불필요) ────────────────
static GLuint g_ab_font = 0;
static GLYPHMETRICSFLOAT g_ab_glyph[128];

// ─── 컨텍스트 ────────────────────────────────────────────────────
typedef struct {
    HDC gl_dc;
    HGLRC gl_rc;
    GLuint sym_tex; // symbol.png 20×20
    GLuint logo_tex; // logo.png, 최대 ABOUT_LOGO_SZ
    int logo_w, logo_h;
    BOOL close_hover;
} AboutCtx;

// 내부 헬퍼
static void ab_ortho(int w, int h) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// 폰트 초기화 (main.c init_font 동일 파라미터)
static void ab_init_font(HDC hdc) {
    if (g_ab_font) return;
    g_ab_font = glGenLists(128);
    HFONT hf = CreateFontA(
        -ABOUT_FONT_H, 0, 0, 0, FW_BOLD, 0, 0, 0,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, FONT_DEFAULT_FAMILY);
    HFONT old = (HFONT) SelectObject(hdc, hf);
    wglUseFontOutlinesA(hdc, 0, 128, g_ab_font,
                        0.0f, 0.0f, WGL_FONT_POLYGONS, g_ab_glyph);
    SelectObject(hdc, old);
    DeleteObject(hf);
}

// scale 배 폰트로 문자열 렌더 (main.c gl_draw_string 동일)
static void ab_str(float x, float y, const char *s, float scale) {
    float sz = ABOUT_FONT_H * scale;
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glScalef(sz, -sz, 1.0f);
    glPushAttrib(GL_LIST_BIT);
    glListBase(g_ab_font);
    glCallLists((GLsizei) strlen(s), GL_UNSIGNED_BYTE, s);
    glPopAttrib();
    glPopMatrix();
}

// 문자열 너비 (main.c gl_measure_string 동일)
static float ab_w(const char *s, float scale) {
    float sz = ABOUT_FONT_H * scale, w = 0.0f;
    for (const char *p = s; *p; p++) {
        int i = (unsigned char) *p;
        if (i < 128) w += g_ab_glyph[i].gmfCellIncX * sz;
    }
    return w;
}

// stbi + stbir 로 GL 텍스처 생성, 지정 크기로 리사이즈
// target_w/h = -1 이면 원본 크기 사용
static GLuint ab_load_tex(const char *path,
                          int target_w, int target_h,
                          int *out_w, int *out_h) {
    int sw, sh, ch;
    unsigned char *src = stbi_load(path, &sw, &sh, &ch, 4);
    if (!src) {
        if (out_w) *out_w = 0;
        if (out_h) *out_h = 0;
        return 0;
    }

    int dw = (target_w > 0) ? target_w : sw;
    int dh = (target_h > 0) ? target_h : sh;

    unsigned char *dst;
    if (dw == sw && dh == sh) {
        dst = src;
    } else {
        dst = (unsigned char *) malloc((size_t) dw * dh * 4);
        if (!dst) {
            stbi_image_free(src);
            return 0;
        }
        stbir_resize_uint8_linear(src, sw, sh, 0,
                                  dst, dw, dh, 0, STBIR_RGBA);
        stbi_image_free(src);
    }

    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dw, dh, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, dst);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    free(dst);

    if (out_w) *out_w = dw;
    if (out_h) *out_h = dh;
    return t;
}

// 텍스처 쿼드 렌더
static void ab_quad_tex(GLuint tex, float x, float y, float w, float h) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(x, y);
    glTexCoord2f(1, 0);
    glVertex2f(x + w, y);
    glTexCoord2f(1, 1);
    glVertex2f(x + w, y + h);
    glTexCoord2f(0, 1);
    glVertex2f(x, y + h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

//  GL 렌더 (숨겨진 백버퍼에 그림, SwapBuffers 없음)
static void ab_render_gl(AboutCtx *ctx) {
    const float W = ABOUT_W, H = ABOUT_H;
    const float capH = (float) ABOUT_CAP_H;
    float lx = 15.0f;
    glViewport(0, 0, ABOUT_W, ABOUT_H);

    // clear – 배경색
    glClearColor(os_theme.background.r / 255.0f,
                 os_theme.background.g / 255.0f,
                 os_theme.background.b / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ab_ortho(ABOUT_W, ABOUT_H);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ── 1. 전체 배경 ──
    glColor4f_255(OS_THEME_BK, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(W, 0);
    glVertex2f(W, H);
    glVertex2f(0, H);
    glEnd();

    // ── 2. 캡션 바 ──
    glColor4f_255(OS_THEME_BK, 1.0f); // OS_THEME_PC → OS_THEME_BK
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(W, 0);
    glVertex2f(W, capH);
    glVertex2f(0, capH);
    glEnd();

    // ── 3. symbol.png (캡션 좌측, 20×20 고정) ──
    //  main.c: lx=15, ly=(CAPTION_HEIGHT-lh)*0.5+1
    if (ctx->sym_tex) {
        float lw = ABOUT_SYM_SZ, lh = ABOUT_SYM_SZ;
        float lx = 15.0f, ly = (capH - lh) * 0.5f + 1.0f;
        ab_quad_tex(ctx->sym_tex, lx, ly, lw, lh);
    }

    // ── 4. 캡션 "About" ──
    // 캡션이 primary color이므로 텍스트는 흰색 고정
    // main.c 스타일: 그림자(어두운색) + 본문(흰색)
    {
        float tx = 15.0f + (float) ABOUT_SYM_SZ + 6.0f;
        float ty = capH * 0.70f;

        glEnable(GL_POLYGON_SMOOTH);
        glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
        // 그림자 (반투명 검정)
        glColor4f(0.0f, 0.0f, 0.0f, 0.4f);
        ab_str(tx + 1.0f, ty + 1.0f, ABOUT_TITLE, 1.0f);
        // 본문 (흰색)
        glColor4f(1.0f, 1.0f, 1.0f, 0.95f);
        ab_str(tx, ty, ABOUT_TITLE, 1.0f);
        glDisable(GL_POLYGON_SMOOTH);
    }

    // ── 5. 닫기 버튼 (main.c gl_draw_caption 완전 동일) ──
    //  hover → primary color 배경, X → OS_THEME_FG, lineWidth 1.5f
    {
        float bW = (float) ABOUT_BTN_W, bH = capH;
        float re = W, cL = re - bW;
        float cx = cL + bW * 0.5f, cy = bH * 0.5f, s = 5.0f;

        if (ctx->close_hover) {
            glColor4f_255(g_primary_color.r,
                          g_primary_color.g,
                          g_primary_color.b, 0.9f);
            glBegin(GL_QUADS);
            glVertex2f(cL, 0);
            glVertex2f(re, 0);
            glVertex2f(re, bH);
            glVertex2f(cL, bH);
            glEnd();
        }
        // X – 흰색 (캡션이 primary color 이므로)
        glColor4f(1.0f, 1.0f, 1.0f, 0.9f);
        glLineWidth(1.5f);
        glBegin(GL_LINES);
        glVertex2f(cx - s, cy - s);
        glVertex2f(cx + s, cy + s);
        glVertex2f(cx + s, cy - s);
        glVertex2f(cx - s, cy + s);
        glEnd();
        glLineWidth(1.0f);
    }

    // ── 6. logo.png (중앙, 비율 유지 최대 ABOUT_LOGO_SZ) ──
    float logo_y = capH + 16.0f;
    float logo_dw = (float) (ctx->logo_w > 0 ? ctx->logo_w : ABOUT_LOGO_SZ);
    float logo_dh = (float) (ctx->logo_h > 0 ? ctx->logo_h : ABOUT_LOGO_SZ);
    if (ctx->logo_tex) {
        float lx = (W - logo_dw) * 0.5f;
        ab_quad_tex(ctx->logo_tex, lx, logo_y, logo_dw, logo_dh);
    }

    // ── 7. "PXVY" 제목 (scale=1.6, 중앙, 그림자+본문) ──
    const float TS = 1.6f;
    float title_y = logo_y + logo_dh + 8.0f + APP_NAME_MARGIN;
    {
        float tw = ab_w("PXVY", TS);
        float tx = (W - tw) * 0.5f;
        glEnable(GL_POLYGON_SMOOTH);
        glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
        // 그림자
        glColor4f_255(OS_THEME_BK, 0.5f);
        ab_str(tx + 1.0f, title_y + 1.0f, "PXVY", TS);
        // 본문
        glColor4f_255(OS_THEME_FG, 0.9f);
        ab_str(tx, title_y, "PXVY", TS);
        glDisable(GL_POLYGON_SMOOTH);
    }

    // ── 8. 구분선 (primary color) ──
    float sep_y = title_y + (float) ABOUT_FONT_H * TS + 10.0f;
    glColor4f_255(OS_THEME_PC, 0.8f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    glVertex2f(20.0f, sep_y);
    glVertex2f(W - 20.0f, sep_y);
    glEnd();

    // ── 9. 정보 행 (Scissor 없이 단순 렌더) ──
    //  라벨: primary color / 값: foreground
    // Compiler 같이 긴 값은 창 끝에서 자연스럽게 잘림
    {
        const float S = 0.75f;
        const float RH = (float) ABOUT_FONT_H * S + 8.0f;
        const float LX = 20.0f; // 라벨 x
        const float VX = 120.0f; // 값    x

        const char *lbl[] = {"Version", "Compiler", "Build Time"};
        const char *val[] = {VERSION, BUILD_ENV_STRING, BUILD_TIME_KST};

        float ry = sep_y + 14.0f + APP_INFO_MARGIN;

        glEnable(GL_POLYGON_SMOOTH);
        glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

        for (int i = 0; i < 3; i++) {
            // 라벨
            glColor4f_255(OS_THEME_PC, 1.0f);
            ab_str(LX, ry, lbl[i], S);
            // 값 – 우측 클리핑 (Y-down ortho → scissor Y 변환)
            float text_top = ry - (float) ABOUT_FONT_H * S;
            float text_bottom = ry + 4.0f;
            float sci_y = (float) ABOUT_H - text_bottom; // GL bottom-up
            float sci_h = text_bottom - text_top + 2.0f;
            glEnable(GL_SCISSOR_TEST);
            glScissor((GLint) VX, (GLint) sci_y,
                      (GLint) (W - VX - 18.0f), (GLint) sci_h);
            glColor4f_255(OS_THEME_FG, 0.9f);
            ab_str(VX, ry, val[i], S);
            glDisable(GL_SCISSOR_TEST);

            ry += RH;
        }

        glDisable(GL_POLYGON_SMOOTH);
    }

    glFlush();
}

// UpdateLayeredWindow 파이프라인 (context_menu.h 완전 동일)
// + GDI RoundRect 테두리 (끊김 없음)
static void ab_draw_layered(void) {
    if (!g_about_hwnd || !g_about_gl_hwnd) return;
    AboutCtx *ctx = (AboutCtx *) GetWindowLongPtr(g_about_hwnd, 0);
    if (!ctx || !ctx->gl_rc) return;

    const int W = ABOUT_W, H = ABOUT_H;

    // ── 1. GL 렌더 ──
    wglMakeCurrent(ctx->gl_dc, ctx->gl_rc);
    ab_render_gl(ctx);

    // ── 2. glReadPixels (bottom-up BGRA) ──
    DWORD *gl_pix = (DWORD *) malloc((size_t) W * H * 4);
    if (!gl_pix) {
        wglMakeCurrent(NULL, NULL);
        return;
    }
    glReadPixels(0, 0, W, H, GL_BGRA_EXT, GL_UNSIGNED_BYTE, gl_pix);
    wglMakeCurrent(NULL, NULL);

    // ── 3. DIB 생성 (top-down) ──
    HDC scr_dc = GetDC(NULL);
    HDC mem_dc = CreateCompatibleDC(scr_dc);
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    DWORD *bits = NULL;
    HBITMAP dib = CreateDIBSection(mem_dc, &bi, DIB_RGB_COLORS,
                                   (void **) &bits, NULL, 0);
    HBITMAP oldbm = (HBITMAP) SelectObject(mem_dc, dib);

    // ── 4. Y-flip ──
    for (int y = 0; y < H; y++)
        memcpy_s(bits + y * W,
                 (size_t) W * 4,
                 gl_pix + (H - 1 - y) * W,
                 (size_t) W * 4);
    free(gl_pix);

    // ── 5. GDI RoundRect 테두리 (premult 전에 먼저 그림) ──
    {
        COLORREF bc = RGB(g_primary_color.r, g_primary_color.g, g_primary_color.b);
        HPEN pen = CreatePen(PS_SOLID, 1, bc);
        HBRUSH nullb = (HBRUSH) GetStockObject(NULL_BRUSH);
        HPEN oldp = (HPEN) SelectObject(mem_dc, pen);
        HBRUSH oldb = (HBRUSH) SelectObject(mem_dc, nullb);
        RoundRect(mem_dc, 0, 0, W, H, ABOUT_ROUND * 2, ABOUT_ROUND * 2);
        SelectObject(mem_dc, oldp);
        SelectObject(mem_dc, oldb);
        DeleteObject(pen);
    }

    // ── 6. Premultiplied alpha ──
    const BYTE A = ABOUT_ALPHA;
    for (int i = 0; i < W * H; i++) {
        DWORD c = bits[i];
        BYTE r = (c >> 16) & 0xFF;
        BYTE g = (c >> 8) & 0xFF;
        BYTE b = c & 0xFF;
        bits[i] = ((DWORD) A << 24)
                  | ((DWORD) (r * A / 255) << 16)
                  | ((DWORD) (g * A / 255) << 8)
                  | ((DWORD) (b * A / 255));
    }

    // ── 7. AA 모서리 ──
    float fr = (float) ABOUT_ROUND;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float fx = x + 0.5f, fy = y + 0.5f;
            float ccx = fr, ccy = fr;
            BOOL in = FALSE;
            if (fx < fr && fy < fr) {
                ccx = fr;
                ccy = fr;
                in = TRUE;
            } else if (fx > W - fr && fy < fr) {
                ccx = W - fr;
                ccy = fr;
                in = TRUE;
            } else if (fx < fr && fy > H - fr) {
                ccx = fr;
                ccy = H - fr;
                in = TRUE;
            } else if (fx > W - fr && fy > H - fr) {
                ccx = W - fr;
                ccy = H - fr;
                in = TRUE;
            }
            if (!in) continue;
            float dx = fx - ccx, dy = fy - ccy;
            float edge = sqrtf(dx * dx + dy * dy) - fr;
            if (edge >= 0.5f) {
                bits[y * W + x] = 0;
            } else if (edge > -0.5f) {
                float t = (0.5f - edge) * (ABOUT_ALPHA / 255.0f);
                DWORD c = bits[y * W + x];
                bits[y * W + x] =
                        ((DWORD) ((BYTE) (((c >> 24) & 0xFF) * t + 0.5f)) << 24) |
                        ((DWORD) ((BYTE) (((c >> 16) & 0xFF) * t + 0.5f)) << 16) |
                        ((DWORD) ((BYTE) (((c >> 8) & 0xFF) * t + 0.5f)) << 8) |
                        ((DWORD) ((BYTE) ((c & 0xFF) * t + 0.5f)));
            }
        }
    }

    // ── 8. UpdateLayeredWindow ──
    POINT ptSrc = {0, 0};
    SIZE sz = {W, H};
    RECT wr;
    GetWindowRect(g_about_hwnd, &wr);
    POINT ptDst = {wr.left, wr.top};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(g_about_hwnd, scr_dc,
                        &ptDst, &sz, mem_dc, &ptSrc,
                        0, &bf, ULW_ALPHA);

    SelectObject(mem_dc, oldbm);
    DeleteObject(dib);
    DeleteDC(mem_dc);
    ReleaseDC(NULL, scr_dc);
}

// WndProc – 표시 창 (WS_EX_LAYERED)

#define ABOUT_IDT_MOUSE 801

static LRESULT CALLBACK AboutWndProc(HWND hWnd, UINT msg,
                                     WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW *cs = (CREATESTRUCTW *) lParam;
            SetWindowLongPtr(hWnd, 0, (LONG_PTR) cs->lpCreateParams);
            SetTimer(hWnd, ABOUT_IDT_MOUSE, 100, NULL);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            ab_draw_layered();
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND: return 1;

        case WM_TIMER: {
            if (wParam == ABOUT_IDT_MOUSE) {
                AboutCtx *ctx = (AboutCtx *) GetWindowLongPtr(hWnd, 0);
                if (!ctx) return 0;

                POINT pt;
                GetCursorPos(&pt);
                RECT wr;
                GetWindowRect(hWnd, &wr);

                BOOL in_window = PtInRect(&wr, pt);
                BOOL hover = FALSE;

                if (in_window) {
                    int cx = pt.x - wr.left;
                    int cy = pt.y - wr.top;
                    hover = (cy >= 0 && cy < ABOUT_CAP_H &&
                             cx >= ABOUT_W - ABOUT_BTN_W);
                }

                if (hover != ctx->close_hover) {
                    ctx->close_hover = hover;
                    ab_draw_layered();
                }
            }
            return 0;
        }

        case WM_NCHITTEST: {
            POINT pt = {
                (int) (short) LOWORD(lParam),
                (int) (short) HIWORD(lParam)
            };
            RECT wr;
            GetWindowRect(hWnd, &wr);
            int cx = pt.x - wr.left, cy = pt.y - wr.top;
            if (cy >= 0 && cy < ABOUT_CAP_H) {
                if (cx >= ABOUT_W - ABOUT_BTN_W) return HTCLOSE;
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_NCMOUSEMOVE: {
            AboutCtx *ctx = (AboutCtx *) GetWindowLongPtr(hWnd, 0);
            if (!ctx) return 0;
            BOOL was = ctx->close_hover;
            ctx->close_hover = (wParam == HTCLOSE);
            if (ctx->close_hover != was) ab_draw_layered();
            return 0;
        }

        case WM_NCLBUTTONDOWN:
            if (wParam == HTCLOSE) {
                DestroyWindow(hWnd);
                return 0;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) DestroyWindow(hWnd);
            return 0;

        case WM_DESTROY: {
            KillTimer(hWnd, ABOUT_IDT_MOUSE);
            AboutCtx *ctx = (AboutCtx *) GetWindowLongPtr(hWnd, 0);
            if (ctx) {
                if (ctx->gl_rc) {
                    wglMakeCurrent(ctx->gl_dc, ctx->gl_rc);
                    if (ctx->sym_tex) glDeleteTextures(1, &ctx->sym_tex);
                    if (ctx->logo_tex) glDeleteTextures(1, &ctx->logo_tex);
                    if (g_ab_font) {
                        glDeleteLists(g_ab_font, 128);
                        g_ab_font = 0;
                    }
                    wglMakeCurrent(NULL, NULL);
                    wglDeleteContext(ctx->gl_rc);
                }
                if (ctx->gl_dc && g_about_gl_hwnd)
                    ReleaseDC(g_about_gl_hwnd, ctx->gl_dc);
                free(ctx);
                SetWindowLongPtr(hWnd, 0, 0);
            }
            if (g_about_gl_hwnd) {
                DestroyWindow(g_about_gl_hwnd);
                g_about_gl_hwnd = NULL;
            }
            g_about_hwnd = NULL;
            return 0;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

//윈도우 클래스 등록
static void ab_register(HINSTANCE hi) {
    static BOOL done = FALSE;
    if (done) return;
    // GL 전용 (숨김)
    WNDCLASSA a = {0};
    a.lpfnWndProc = DefWindowProcA;
    a.hInstance = hi;
    a.lpszClassName = "PXVYAboutGL";
    RegisterClassA(&a);
    // 표시 (Layered)
    WNDCLASSW w = {0};
    w.lpfnWndProc = AboutWndProc;
    w.hInstance = hi;
    w.lpszClassName = L"PXVYAbout";
    w.hCursor = LoadCursor(NULL, IDC_ARROW);
    w.cbWndExtra = sizeof(LONG_PTR);
    RegisterClassW(&w);
    done = TRUE;
}

// 공개 API
static void show_about_window(HWND owner) {
    if (g_about_hwnd && IsWindow(g_about_hwnd)) {
        SetForegroundWindow(g_about_hwnd);
        return;
    }
    HINSTANCE hi = GetModuleHandleW(NULL);
    ab_register(hi);

    AboutCtx *ctx = (AboutCtx *) calloc(1, sizeof(AboutCtx));
    if (!ctx) return;

    // ── 숨겨진 GL 창 ──
    g_about_gl_hwnd = CreateWindowA("PXVYAboutGL", NULL, WS_POPUP,
                                    -ABOUT_W*2, 0, ABOUT_W, ABOUT_H,
                                    NULL, NULL, hi, NULL);
    if (!g_about_gl_hwnd) {
        free(ctx);
        return;
    }

    ctx->gl_dc = GetDC(g_about_gl_hwnd);
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(ctx->gl_dc, &pfd);
    SetPixelFormat(ctx->gl_dc, pf, &pfd);
    ctx->gl_rc = wglCreateContext(ctx->gl_dc);

    // ── 폰트 + 텍스처 (GL 컨텍스트 내에서) ──
    wglMakeCurrent(ctx->gl_dc, ctx->gl_rc);
    ab_init_font(ctx->gl_dc);

    char exe[MAX_PATH], sym[MAX_PATH], logo[MAX_PATH];
    GetModuleFileNameA(NULL, exe, MAX_PATH);
    char *sl = strrchr(exe, '\\');
    if (sl) *(sl + 1) = '\0';
    snprintf(sym, MAX_PATH, "%ssymbol.png", exe);
    snprintf(logo, MAX_PATH, "%slogo.png", exe);

    // symbol: main.c 와 동일하게 20×20 으로 리사이즈
    ctx->sym_tex = ab_load_tex(sym, ABOUT_SYM_SZ, ABOUT_SYM_SZ, NULL, NULL);

    // logo: 비율 유지 최대 ABOUT_LOGO_SZ
    {
        int ow, oh;
        ab_load_tex(logo, -1, -1, &ow, &oh); // 원본 크기 측정
        if (ow > 0 && oh > 0) {
            float sc = (float) ABOUT_LOGO_SZ /
                       (float) (ow > oh ? ow : oh);
            int tw = (int) (ow * sc), th = (int) (oh * sc);
            ctx->logo_tex = ab_load_tex(logo, tw, th,
                                        &ctx->logo_w, &ctx->logo_h);
        }
    }

    wglMakeCurrent(NULL, NULL);

    // ── WS_EX_LAYERED 표시 창 ──
    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    g_about_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"PXVYAbout", NULL,
        WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        (sx - ABOUT_W) / 2, (sy - ABOUT_H) / 2, ABOUT_W, ABOUT_H,
        owner, NULL, hi, (LPVOID) ctx);

    if (!g_about_hwnd) {
        wglMakeCurrent(ctx->gl_dc, ctx->gl_rc);
        if (ctx->sym_tex) glDeleteTextures(1, &ctx->sym_tex);
        if (ctx->logo_tex) glDeleteTextures(1, &ctx->logo_tex);
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(ctx->gl_rc);
        ReleaseDC(g_about_gl_hwnd, ctx->gl_dc);
        DestroyWindow(g_about_gl_hwnd);
        g_about_gl_hwnd = NULL;
        free(ctx);
        return;
    }

    ShowWindow(g_about_hwnd, SW_SHOW);
    ab_draw_layered();
    SetForegroundWindow(g_about_hwnd);
}

#endif // RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_ABOUT_H
