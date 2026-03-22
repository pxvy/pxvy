#pragma once
#ifndef RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_CONTEXT_MENU_H
#define RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_CONTEXT_MENU_H

#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include "custom_msgbox.h"
#include "font_picker.h"
#include "about.h"
#include "database.h"
// ──────────────────── 메뉴 커맨드 ID ────────────────────
#define ID_EXIT                900
#define ID_OPEN                901
#define ID_PLAYLIST            902
#define ID_RECENT_VIDEOS       907
#define ID_THEME               903
#define ID_SCREENSHOT               904
#define ID_SETTINGS            905
#define ID_ABOUT               906
#define ID_RECENT_CLEAR  980
// Tools 서브메뉴
#define ID_SCREENSHOT_CAPTURE    910
#define ID_SCREENSHOT_FORMAT     911
#define ID_SCREENSHOT_JPG      950
#define ID_SCREENSHOT_PNG      951

#define ID_RECENT_VIDEO_BASE  970   // 970 ~ 979

#define ID_THEME_RED           1000
#define ID_THEME_ORANGE        1001
#define ID_THEME_YELLOW        1002
#define ID_THEME_GREEN         1003
#define ID_THEME_MINT          1004
#define ID_THEME_TEAL          1005
#define ID_THEME_CYAN          1006
#define ID_THEME_BLUE          1007
#define ID_THEME_INDIGO        1008
#define ID_THEME_PURPLE        1009
#define ID_THEME_BROWN         1011

// ──────────────────── 레이아웃 상수 ────────────────────
#define MENU_ITEM_HEIGHT  28
#define MENU_SEP_HEIGHT   9
#define MENU_PADDING      8
#define MENU_ROUND        10
#define MENU_WIDTH        220

// ──────────────────── 헬퍼 매크로 ────────────────────//

#define SUBMENU(arr)  (arr), (int)(_countof(arr))
static HWND g_root_menu_hwnd = NULL;
static UINT g_selected_theme = ID_THEME_BLUE;
static UINT g_selected_screenshot_format = ID_SCREENSHOT_JPG;

// ──────────────────── 테마 색상 테이블 ────────────────────
typedef struct {
    UINT id;
    BYTE r, g, b;
    const char *name;
} ThemeColor;

static const ThemeColor g_theme_colors[] = {
    {ID_THEME_RED, 255, 56, 60, "Red"},
    {ID_THEME_ORANGE, 255, 141, 40, "Orange"},
    {ID_THEME_YELLOW, 255, 204, 0, "Yellow"},
    {ID_THEME_GREEN, 52, 199, 89, "Green"},
    {ID_THEME_MINT, 0, 200, 179, "Mint"},
    {ID_THEME_TEAL, 0, 195, 208, "Teal"},
    {ID_THEME_CYAN, 0, 192, 232, "Cyan"},
    {ID_THEME_BLUE, 0, 136, 255, "Blue"},
    {ID_THEME_INDIGO, 97, 85, 245, "Indigo"},
    {ID_THEME_PURPLE, 203, 48, 224, "Purple"},
    {ID_THEME_BROWN, 172, 127, 94, "Brown"},
};
static const int g_theme_color_count = (int) _countof(g_theme_colors);

static void apply_theme(HWND hWnd, UINT id) {
    for (int i = 0; i < g_theme_color_count; i++) {
        if (g_theme_colors[i].id != id) continue;
        g_selected_theme = id;
        // 전역 primary color 업데이트
        g_primary_color.r = g_theme_colors[i].r;
        g_primary_color.g = g_theme_colors[i].g;
        g_primary_color.b = g_theme_colors[i].b;

        pxvy_set_color(g_primary_color.r, g_primary_color.g, g_primary_color.b);
        // DWM 보더 색 갱신
        COLORREF border = RGB(g_primary_color.r, g_primary_color.g, g_primary_color.b);
        DwmSetWindowAttribute(hWnd, 34, &border, sizeof(border));

        if (g_root_menu_hwnd != NULL) {
            DwmSetWindowAttribute(g_root_menu_hwnd, 34, &border, sizeof(border));
            InvalidateRect(g_root_menu_hwnd, NULL, FALSE);
            UpdateWindow(g_root_menu_hwnd);
        }

        // About 창 테마 갱신
        if (g_about_hwnd && IsWindow(g_about_hwnd)) {
            ab_draw_layered();
        }
        if (g_fp_hwnd != NULL) {
            DwmSetWindowAttribute(g_fp_hwnd, 34, &border, sizeof(border));
            InvalidateRect(g_fp_hwnd, NULL, FALSE);
            UpdateWindow(g_fp_hwnd);
        }

        InvalidateRect(hWnd, NULL, FALSE);
        break;
    }
}

// ──────────────────── 데이터 구조 ────────────────────

typedef struct MenuItemData MenuItemData;

struct MenuItemData {
    const wchar_t *text;
    UINT id;
    BOOL separator;
    MenuItemData *submenu;
    int submenu_count;
    int submenu_width; // ← 추가: 0이면 기본 MENU_WIDTH 사용
};

typedef struct {
    MenuItemData *items;
    int count;
    HWND owner; // 최상위 오너 윈도우 (WM_COMMAND 수신)
    HWND parent_menu; // 루트이면 NULLS
    int hover; // 현재 hover 인덱스 (-1 = 없음)
    HWND submenu_hwnd; // 현재 열린 자식 서브메뉴 윈도우
} MenuCtx;

// ──────────────────── 메뉴 항목 정의 ────────────────────
// * 자기 참조가 있으므로 반드시 아래(자식)에서 위(부모) 순서로 선언한다.
//
//  g_jpg_quality_items  ← 3단계
//       g_screenshot_items   ← 2단계
//           g_tools_submenu      ← 1단계
//              g_menu_items         ← 루트
//


// ── 3단계: JPG 품질 ──
static MenuItemData g_screenshot_items[] = {
    {L"JPG", ID_SCREENSHOT_JPG, FALSE, NULL, 0},
    {L"PNG", ID_SCREENSHOT_PNG, FALSE, NULL, 0},
};

// ── 2단계: Screenshot 포맷 ──
static MenuItemData g_capture_submenu[] = {
    {L"Capture Now\tCtrl+S", ID_SCREENSHOT_CAPTURE, FALSE, NULL, 0},
    {NULL, 0, TRUE, NULL, 0},
    {L"Capture Format", ID_SCREENSHOT_FORMAT, FALSE, SUBMENU(g_screenshot_items)},
};

static MenuItemData g_theme_submenu[] = {
    {L"Red", ID_THEME_RED, FALSE, NULL, 0},
    {L"Orange", ID_THEME_ORANGE, FALSE, NULL, 0},
    {L"Yellow", ID_THEME_YELLOW, FALSE, NULL, 0},
    {L"Green", ID_THEME_GREEN, FALSE, NULL, 0},
    {L"Mint", ID_THEME_MINT, FALSE, NULL, 0},
    {L"Teal", ID_THEME_TEAL, FALSE, NULL, 0},
    {L"Cyan", ID_THEME_CYAN, FALSE, NULL, 0},
    {L"Blue", ID_THEME_BLUE, FALSE, NULL, 0},
    {L"Indigo", ID_THEME_INDIGO, FALSE, NULL, 0},
    {L"Brown", ID_THEME_BROWN, FALSE, NULL, 0},
};


// ── 루트 메뉴 ──
static MenuItemData g_menu_items[] = {
    {L"Open File\tCtrl+O", ID_OPEN, FALSE, NULL, 0},
    {L"Recent Medias", ID_RECENT_VIDEOS, FALSE, NULL, 0},
    {NULL, 0, TRUE, NULL, 0},
    {L"Theme", ID_THEME, FALSE, SUBMENU(g_theme_submenu)},
    //{L"Playlist", ID_PLAYLIST, FALSE, NULL, 0},
    {L"Capture", ID_SCREENSHOT, FALSE, SUBMENU(g_capture_submenu)},
    {L"Settings", ID_SETTINGS, FALSE, NULL, 0},
    {L"About", ID_ABOUT, FALSE, NULL, 0},
    {NULL, 0, TRUE, NULL, 0},
    {L"Exit", ID_EXIT, FALSE, NULL, 0},
};
static int g_menu_count = (int) _countof(g_menu_items);

// 동적 최근 파일 서브메뉴용 스토리지
static wchar_t g_recent_submenu_text[12][MAX_PATH + 3];
static MenuItemData g_recent_submenu_items[12];
static int g_recent_submenu_count = 0;

static void build_recent_submenu(void) {
    pxvy_get_recent_video();

    if (g_recent_count == 0) {
        g_recent_submenu_items[0] = (MenuItemData){L"(Empty)", 0, FALSE, NULL, 0, 0};
        g_recent_submenu_count = 1;
    } else {
        for (int i = 0; i < g_recent_count; i++) {
            const char *p = g_recent_video[i];
            const char *name = strrchr(p, '\\');
            if (!name) name = strrchr(p, '/');
            name = name ? name + 1 : p;

            MultiByteToWideChar(CP_UTF8, 0, name, -1,
                                g_recent_submenu_text[i], MAX_PATH + 2);

            g_recent_submenu_items[i] = (MenuItemData){
                g_recent_submenu_text[i],
                (UINT) (ID_RECENT_VIDEO_BASE + i),
                FALSE, NULL, 0, 0
            };
        }
        // separator
        g_recent_submenu_items[g_recent_count] =
                (MenuItemData){NULL, 0, TRUE, NULL, 0, 0};
        // (Clear) 버튼
        g_recent_submenu_items[g_recent_count + 1] =
                (MenuItemData){L"(Clear)", ID_RECENT_CLEAR, FALSE, NULL, 0, 0};

        g_recent_submenu_count = g_recent_count + 2;
    }

    // ── 가장 긴 파일명 기준으로 너비 측정 ──
    int dynamic_width = MENU_WIDTH; // 최솟값
    HDC hdc = GetDC(NULL);
    if (hdc) {
        HFONT hFont = CreateFontA(
            20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, FONT_DEFAULT_FAMILY);
        HFONT old = (HFONT) SelectObject(hdc, hFont);

        // ── 가장 긴 파일명 기준으로 너비 측정 ──
        for (int i = 0; i < g_recent_submenu_count; i++) {
            if (!g_recent_submenu_items[i].text) continue; // ← NULL 체크 추가
            if (g_recent_submenu_items[i].separator) continue; // ← separator 스킵
            SIZE sz;
            GetTextExtentPoint32W(hdc, g_recent_submenu_items[i].text,
                                  (int) wcslen(g_recent_submenu_items[i].text), &sz);
            int needed = sz.cx + 18 + 24;
            if (needed > dynamic_width) dynamic_width = needed;
        }

        SelectObject(hdc, old);
        DeleteObject(hFont);
        ReleaseDC(NULL, hdc);
    }

    // g_menu_items의 Recent Medias 항목에 연결
    for (int i = 0; i < g_menu_count; i++) {
        if (g_menu_items[i].id == ID_RECENT_VIDEOS) {
            g_menu_items[i].submenu = g_recent_submenu_items;
            g_menu_items[i].submenu_count = g_recent_submenu_count;
            g_menu_items[i].submenu_width = dynamic_width; // ← 너비 저장
            break;
        }
    }
}

// ──────────────────── 내부 유틸리티 ────────────────────

static MenuCtx *get_ctx(HWND hWnd) {
    return (MenuCtx *) GetWindowLongPtr(hWnd, 0);
}

static int menu_total_height_for(MenuItemData *items, int count) {
    int h = MENU_PADDING * 2;
    for (int i = 0; i < count; i++)
        h += items[i].separator ? MENU_SEP_HEIGHT : MENU_ITEM_HEIGHT;
    return h;
}

static int menu_hit_test_ctx(const MenuCtx *ctx, int y) {
    int cy = MENU_PADDING;
    for (int i = 0; i < ctx->count; i++) {
        int ih = ctx->items[i].separator ? MENU_SEP_HEIGHT : MENU_ITEM_HEIGHT;
        if (y >= cy && y < cy + ih)
            return ctx->items[i].separator ? -1 : i;
        cy += ih;
    }
    return -1;
}

static int menu_item_top(const MenuCtx *ctx, int idx) {
    int cy = MENU_PADDING;
    for (int i = 0; i < idx; i++)
        cy += ctx->items[i].separator ? MENU_SEP_HEIGHT : MENU_ITEM_HEIGHT;
    return cy;
}

// ──────────────────── 메뉴 닫기 ────────────────────

static void close_submenu_tree(HWND hWnd) {
    MenuCtx *ctx = get_ctx(hWnd);
    if (!ctx || !ctx->submenu_hwnd) return;
    close_submenu_tree(ctx->submenu_hwnd);
    DestroyWindow(ctx->submenu_hwnd);
    ctx->submenu_hwnd = NULL;
}

static void close_all_menus(void) {
    if (!g_root_menu_hwnd) return;
    HWND root = g_root_menu_hwnd;
    g_root_menu_hwnd = NULL; // WM_DESTROY 재진입 방지
    close_submenu_tree(root);
    DestroyWindow(root);
}

//
// hWnd가 현재 열린 메뉴 체인(루트 → … → 말단) 안에 있는지 검사한다.
// WM_ACTIVATE에서 "같은 체인 내부의 포커스 이동인가?"를 판별하는 데 사용한다.
//이 방식 덕분에 3단계 이상에서도 중간 창이 잘못 닫히지 않는다.
//
static BOOL is_in_menu_chain(HWND hWnd) {
    HWND cur = g_root_menu_hwnd;
    while (cur) {
        if (cur == hWnd) return TRUE;
        MenuCtx *ctx = get_ctx(cur);
        if (!ctx) break;
        cur = ctx->submenu_hwnd;
    }
    return FALSE;
}

// ──────────────────── 레이어드 윈도우 렌더링 ────────────────────

static void menu_draw_layered(HWND hWnd) {
    MenuCtx *ctx = get_ctx(hWnd);
    if (!ctx) return;

    RECT rc;
    GetClientRect(hWnd, &rc);
    int w = rc.right, h = rc.bottom;
    if (w <= 0 || h <= 0) return;

    HDC screen_dc = GetDC(NULL);
    HDC mem_dc = CreateCompatibleDC(screen_dc);

    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    DWORD *bits = NULL;
    HBITMAP dib = CreateDIBSection(mem_dc, &bi, DIB_RGB_COLORS,
                                   (void **) &bits, NULL, 0);
    HBITMAP old_bmp = (HBITMAP) SelectObject(mem_dc, dib);

    // ── 1. 배경 ──
    HBRUSH bgBrush = CreateSolidBrush(
        RGB(os_theme.background.r, os_theme.background.g, os_theme.background.b));
    FillRect(mem_dc, &rc, bgBrush);
    DeleteObject(bgBrush);

    // ── 2. 항목 렌더링 ──
    SetBkMode(mem_dc, TRANSPARENT);
    HFONT hFont = CreateFontA(
        20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, FONT_DEFAULT_FAMILY);
    HFONT old_font = (HFONT) SelectObject(mem_dc, hFont);

    int item_y = MENU_PADDING;
    for (int i = 0; i < ctx->count; i++) {
        MenuItemData *item = &ctx->items[i];

        if (item->separator) {
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(70, 70, 70));
            HPEN oldp = (HPEN) SelectObject(mem_dc, hPen);
            int mid = item_y + MENU_SEP_HEIGHT / 2;
            MoveToEx(mem_dc, MENU_PADDING + 4, mid, NULL);
            LineTo(mem_dc, w - MENU_PADDING - 4, mid);
            SelectObject(mem_dc, oldp);
            DeleteObject(hPen);
            item_y += MENU_SEP_HEIGHT;
        } else {
            if (i == ctx->hover) {
                HBRUSH hv = CreateSolidBrush(RGB(55, 55, 55));
                RECT hvRc = {4, item_y + 1, w - 4, item_y + MENU_ITEM_HEIGHT - 1};
                HRGN hRgn = CreateRoundRectRgn(
                    hvRc.left, hvRc.top, hvRc.right, hvRc.bottom, 6, 6);
                FillRgn(mem_dc, hRgn, hv);
                DeleteObject(hRgn);
                DeleteObject(hv);
            }

            // 텍스트: 오른쪽 22px는 화살표 영역
            SetTextColor(mem_dc, RGB(220, 220, 220));
            RECT textRc = {18, item_y, w - 26, item_y + MENU_ITEM_HEIGHT};

            wchar_t buf[256];
            wcsncpy(buf, item->text, 255);
            buf[255] = L'\0';

            wchar_t *tab = wcschr(buf, L'\t');
            if (tab) {
                *tab = L'\0';
                DrawTextW(mem_dc, buf, -1, &textRc,
                          DT_SINGLELINE | DT_VCENTER | DT_LEFT);
                RECT sr = {2, item_y, w - 16, item_y + MENU_ITEM_HEIGHT};
                SetTextColor(mem_dc, RGB(140, 140, 140));
                DrawTextW(mem_dc, tab + 1, -1, &sr,
                          DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
            } else {
                DrawTextW(mem_dc, buf, -1, &textRc,
                          DT_SINGLELINE | DT_VCENTER | DT_LEFT);
            }
            // ── 체크마크 ✔ ───────────────────────────────────
            BOOL is_checked =
                    (item->id != 0) &&
                    (item->id == g_selected_theme ||
                     item->id == g_selected_screenshot_format);

            if (is_checked) {
                SetTextColor(mem_dc,
                             RGB(g_primary_color.r, g_primary_color.g, g_primary_color.b));
                RECT ckRc = {w - 22, item_y, w - 4, item_y + MENU_ITEM_HEIGHT};
                DrawTextW(mem_dc, L"\u2714", -1, &ckRc,
                          DT_SINGLELINE | DT_VCENTER | DT_CENTER);
            }
            // 서브메뉴 화살표 ▶
            if (item->submenu && item->submenu_count > 0) {
                SetTextColor(mem_dc, RGB(160, 160, 160));
                RECT arRc = {w - 22, item_y, w - 4, item_y + MENU_ITEM_HEIGHT};
                DrawTextW(mem_dc, L"\u25B6", -1, &arRc,
                          DT_SINGLELINE | DT_VCENTER | DT_CENTER);
            }

            item_y += MENU_ITEM_HEIGHT;
        }
    }

    SelectObject(mem_dc, old_font);
    DeleteObject(hFont);

    // 테두리
    HPEN borderPen = CreatePen(PS_SOLID, 1,
                               RGB(g_primary_color.r, g_primary_color.g, g_primary_color.b));
    HBRUSH nullBrush = (HBRUSH) GetStockObject(NULL_BRUSH);
    SelectObject(mem_dc, borderPen);
    SelectObject(mem_dc, nullBrush);
    RoundRect(mem_dc, 0, 0, w, h, MENU_ROUND * 2, MENU_ROUND * 2);
    DeleteObject(borderPen);

    // ── 3. Premultiplied alpha 적용 ──
    const BYTE MENU_A = 235;
    for (int i = 0; i < w * h; i++) {
        DWORD c = bits[i];
        BYTE r = (c >> 16) & 0xFF;
        BYTE g = (c >> 8) & 0xFF;
        BYTE b = c & 0xFF;
        bits[i] = ((DWORD) MENU_A << 24)
                  | ((DWORD) (r * MENU_A / 255) << 16)
                  | ((DWORD) (g * MENU_A / 255) << 8)
                  | (DWORD) (b * MENU_A / 255);
    }

    // ── 4. 모서리 AA ──
    float fr = (float) MENU_ROUND;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float fx = x + 0.5f, fy = y + 0.5f;
            float ccx = 0, ccy = 0;
            BOOL in_corner = FALSE;

            if (fx < fr && fy < fr) {
                ccx = fr;
                ccy = fr;
                in_corner = TRUE;
            } else if (fx > w - fr && fy < fr) {
                ccx = w - fr;
                ccy = fr;
                in_corner = TRUE;
            } else if (fx < fr && fy > h - fr) {
                ccx = fr;
                ccy = h - fr;
                in_corner = TRUE;
            } else if (fx > w - fr && fy > h - fr) {
                ccx = w - fr;
                ccy = h - fr;
                in_corner = TRUE;
            }

            if (!in_corner) continue;

            float dx = fx - ccx, dy = fy - ccy;
            float edge = sqrtf(dx * dx + dy * dy) - fr;

            if (edge >= 0.5f) {
                bits[y * w + x] = 0;
            } else if (edge > -0.5f) {
                float t = 0.5f - edge;
                DWORD c = bits[y * w + x];
                bits[y * w + x] =
                        ((DWORD) ((BYTE) (((c >> 24) & 0xFF) * t + 0.5f)) << 24) |
                        ((DWORD) ((BYTE) (((c >> 16) & 0xFF) * t + 0.5f)) << 16) |
                        ((DWORD) ((BYTE) (((c >> 8) & 0xFF) * t + 0.5f)) << 8) |
                        (DWORD) ((BYTE) ((c & 0xFF) * t + 0.5f));
            }
        }
    }

    // ── 5. UpdateLayeredWindow ──
    POINT ptSrc = {0, 0};
    SIZE sz = {w, h};
    RECT wr;
    GetWindowRect(hWnd, &wr);
    POINT ptDst = {wr.left, wr.top};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(hWnd, screen_dc, &ptDst, &sz,
                        mem_dc, &ptSrc, 0, &bf, ULW_ALPHA);

    SelectObject(mem_dc, old_bmp);
    DeleteObject(dib);
    DeleteDC(mem_dc);
    ReleaseDC(NULL, screen_dc);
}

// ──────────────────── 서브메뉴 열기 ────────────────────

static void register_menu_class(HINSTANCE hInst); // 전방 선언

static void open_submenu(HWND parent_hwnd, int item_idx) {
    MenuCtx *ctx = get_ctx(parent_hwnd);
    MenuItemData *item = &ctx->items[item_idx];
    if (!item->submenu || item->submenu_count <= 0) return;

    // 이미 같은 서브메뉴 열려 있으면 skip
    if (ctx->submenu_hwnd) {
        MenuCtx *existing = get_ctx(ctx->submenu_hwnd);
        if (existing && existing->items == item->submenu) return;
        // 다른 서브메뉴 → 닫기
        close_submenu_tree(ctx->submenu_hwnd);
        DestroyWindow(ctx->submenu_hwnd);
        ctx->submenu_hwnd = NULL;
    }

    register_menu_class(GetModuleHandle(NULL));

    // 위치: 부모 오른쪽, 해당 항목 y 정렬
    RECT wr;
    GetWindowRect(parent_hwnd, &wr);
    int item_y = menu_item_top(ctx, item_idx);
    int sub_w = (item->submenu_width > 0) ? item->submenu_width : MENU_WIDTH;
    int sub_h = menu_total_height_for(item->submenu, item->submenu_count);
    int sub_x = wr.right - 2;
    int sub_y = wr.top + item_y;

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    if (sub_x + sub_w > screen_w) sub_x = wr.left - sub_w + 2; // 왼쪽 반전
    if (sub_y + sub_h > screen_h) sub_y = screen_h - sub_h;
    if (sub_y < 0) sub_y = 0;

    // NOLINT(clang-analyzer-unix.calloc)
    MenuCtx *sub_ctx = (MenuCtx *) calloc(sizeof(MenuCtx), 1);

    sub_ctx->items = item->submenu;
    sub_ctx->count = item->submenu_count;
    sub_ctx->owner = ctx->owner;
    sub_ctx->parent_menu = parent_hwnd;
    sub_ctx->hover = -1;
    sub_ctx->submenu_hwnd = NULL;

    ctx->submenu_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"PXVYMenu", NULL, WS_POPUP,
        sub_x, sub_y, sub_w, sub_h,
        NULL, NULL, GetModuleHandle(NULL), (LPVOID) sub_ctx);

    ShowWindow(ctx->submenu_hwnd, SW_SHOWNA);
    menu_draw_layered(ctx->submenu_hwnd);
    SetForegroundWindow(ctx->submenu_hwnd);
}

// ──────────────────── WndProc ────────────────────

static LRESULT CALLBACK MenuWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW *cs = (CREATESTRUCTW *) lParam;
            SetWindowLongPtr(hWnd, 0, (LONG_PTR) cs->lpCreateParams);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            MenuCtx *ctx = get_ctx(hWnd);
            int new_hov = menu_hit_test_ctx(ctx, (int) (short) HIWORD(lParam));

            if (new_hov != ctx->hover) {
                ctx->hover = new_hov;
                menu_draw_layered(hWnd);

                if (new_hov >= 0) {
                    MenuItemData *item = &ctx->items[new_hov];
                    if (item->submenu && item->submenu_count > 0) {
                        open_submenu(hWnd, new_hov);
                    } else {
                        if (ctx->submenu_hwnd) {
                            close_submenu_tree(ctx->submenu_hwnd);
                            DestroyWindow(ctx->submenu_hwnd);
                            ctx->submenu_hwnd = NULL;
                        }
                    }
                }
            }

            TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hWnd, 0};
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE: {
            MenuCtx *ctx = get_ctx(hWnd);
            if (!ctx) return 0;
            // 마우스가 자식 서브메뉴 위로 이동 → hover 유지
            if (ctx->submenu_hwnd) {
                POINT pt;
                GetCursorPos(&pt);
                RECT sr;
                GetWindowRect(ctx->submenu_hwnd, &sr);
                if (PtInRect(&sr, pt)) return 0;
            }
            ctx->hover = -1;
            menu_draw_layered(hWnd);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            MenuCtx *ctx = get_ctx(hWnd);
            int idx = menu_hit_test_ctx(ctx, (int) (short) HIWORD(lParam));
            if (idx >= 0) {
                MenuItemData *item = &ctx->items[idx];
                if (item->submenu && item->submenu_count > 0) {
                    open_submenu(hWnd, idx);
                } else {
                    HWND owner = ctx->owner;
                    UINT id = item->id;

                    // ── 선택 상태 갱신 ──────────────────────────────
                    if (id == ID_SCREENSHOT_JPG || id == ID_SCREENSHOT_PNG) {
                        g_selected_screenshot_format = id;
                        if (id == ID_SCREENSHOT_JPG) {
                            pxvy_db_set_capture_type("JPG");
                        } else if (id == ID_SCREENSHOT_PNG) {
                            pxvy_db_set_capture_type("PNG");
                        }
                    }

                    // ────────────────────────────────────────────────

                    close_all_menus();
                    if (id == ID_ABOUT) {
                        show_about_window(owner);
                    } else {
                        PostMessage(owner, WM_COMMAND, id, 0);
                    }
                }
            }
            return 0;
        }

        case WM_ACTIVATE: {
            if (LOWORD(wParam) == WA_INACTIVE) {
                HWND new_active = (HWND) lParam;
                // 새로 활성화된 창이 메뉴 체인 안에 있으면 무시한다.
                // is_in_menu_chain()이 전체 체인을 순회하므로
                // 3단계 이상의 깊이에서도 중간 창이 잘못 닫히지 않는다.
                if (is_in_menu_chain(new_active)) return 0;
                close_all_menus();
            }
            return 0;
        }

        case WM_DESTROY: {
            MenuCtx *ctx = get_ctx(hWnd);
            if (ctx) {
                if (ctx->parent_menu) {
                    MenuCtx *par = get_ctx(ctx->parent_menu);
                    if (par && par->submenu_hwnd == hWnd)
                        par->submenu_hwnd = NULL;
                }
                free(ctx);
                SetWindowLongPtr(hWnd, 0, 0);
            }
            if (hWnd == g_root_menu_hwnd)
                g_root_menu_hwnd = NULL;
            return 0;
        }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ──────────────────── 윈도우 클래스 등록 ────────────────────

static void register_menu_class(HINSTANCE hInst) {
    static BOOL registered = FALSE;
    if (registered) return;

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = MenuWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"PXVYMenu";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.cbWndExtra = sizeof(LONG_PTR);
    RegisterClassW(&wc);
    registered = TRUE;
}

// ──────────────────── 공개 진입점 ────────────────────

static void show_context_menu(HWND hWnd, int x, int y) {
    close_all_menus();
    build_recent_submenu();
    register_menu_class(GetModuleHandle(NULL));

    MenuCtx *ctx = (MenuCtx *) calloc(sizeof(MenuCtx), 1);
    ctx->items = g_menu_items;
    ctx->count = g_menu_count;
    ctx->owner = hWnd;
    ctx->parent_menu = NULL;
    ctx->hover = -1;
    ctx->submenu_hwnd = NULL;

    const int w = MENU_WIDTH;
    const int h = menu_total_height_for(g_menu_items, g_menu_count);

    const int screen_w = GetSystemMetrics(SM_CXSCREEN);
    const int screen_h = GetSystemMetrics(SM_CYSCREEN);
    if (x + w > screen_w) x = screen_w - w;
    if (y + h > screen_h) y = screen_h - h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    g_root_menu_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"PXVYMenu", NULL, WS_POPUP,
        x, y, w, h,
        hWnd, NULL, GetModuleHandle(NULL), (LPVOID) ctx);

    ShowWindow(g_root_menu_hwnd, SW_SHOWNA);
    menu_draw_layered(g_root_menu_hwnd);
    SetForegroundWindow(g_root_menu_hwnd);
}

#endif //RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_CONTEXT_MENU_H
