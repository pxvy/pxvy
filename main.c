#define _USE_MATH_DEFINES
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define EPSILON 0.00001f
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#include <Windows.h>
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

#include <ShlObj.h>
#include <dwmapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <GL/gl.h>
#include <mpv/client.h>
#include <mpv/render_gl.h>
#include "resource.h"
#include "database.h"
#include "colors.h"
#include "compiler.h"
#include "build_time.h"
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include "wmmap.h"

// GUID 직접 정의
static const CLSID MY_CLSID_MMDeviceEnumerator = {
    0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}
};
static const IID MY_IID_IMMDeviceEnumerator = {
    0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}
};
static const IID MY_IID_IAudioEndpointVolume = {
    0x5CDF2C82, 0x841E, 0x4546, {0x97, 0x22, 0x0C, 0xF7, 0x40, 0x78, 0x22, 0x9A}
};

// 링크: -lole32 -loleaut32
// 또는 #pragma comment(lib, "ole32.lib")

// 현재 시스템 볼륨 가져오기 (0.0 ~ 1.0), 실패 시 -1.f 반환
static float GetSystemVolume(void) {
    float vol = -1.f;
    IMMDeviceEnumerator *pEnum = NULL;
    IMMDevice *pDev = NULL;
    IAudioEndpointVolume *pEPV = NULL;

    if (FAILED(CoCreateInstance(&MY_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
        &MY_IID_IMMDeviceEnumerator, (void **)&pEnum)))
        return vol;
    if (FAILED(pEnum->lpVtbl->GetDefaultAudioEndpoint(pEnum, eRender, eConsole, &pDev)))
        goto cleanup;
    if (FAILED(pDev->lpVtbl->Activate(pDev, &MY_IID_IAudioEndpointVolume,
        CLSCTX_ALL, NULL, (void **)&pEPV)))
        goto cleanup;
    pEPV->lpVtbl->GetMasterVolumeLevelScalar(pEPV, &vol);
cleanup:
    if (pEPV) pEPV->lpVtbl->Release(pEPV);
    if (pDev) pDev->lpVtbl->Release(pDev);
    if (pEnum) pEnum->lpVtbl->Release(pEnum);
    return vol;
}

static void SetSystemVolume(float vol) {
    if (vol < 0.f) vol = 0.f;
    if (vol > 1.f) vol = 1.f;
    IMMDeviceEnumerator *pEnum = NULL;
    IMMDevice *pDev = NULL;
    IAudioEndpointVolume *pEPV = NULL;

    if (FAILED(CoCreateInstance(&MY_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
        &MY_IID_IMMDeviceEnumerator, (void **)&pEnum)))
        return;
    if (FAILED(pEnum->lpVtbl->GetDefaultAudioEndpoint(pEnum, eRender, eConsole, &pDev)))
        goto cleanup;
    if (FAILED(pDev->lpVtbl->Activate(pDev, &MY_IID_IAudioEndpointVolume,
        CLSCTX_ALL, NULL, (void **)&pEPV)))
        goto cleanup;
    pEPV->lpVtbl->SetMasterVolumeLevelScalar(pEPV, vol, NULL);
cleanup:
    if (pEPV) pEPV->lpVtbl->Release(pEPV);
    if (pDev) pDev->lpVtbl->Release(pDev);
    if (pEnum) pEnum->lpVtbl->Release(pEnum);
}


#define HT_AOT_BUTTON  101

#define ATIME_MOUSECHECK    100
#define ASIZE_MOUSEIDLE 30

// @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif

#define PXVY_CMD_OPEN  1UL
// ──────────────────── 상수 ────────────────────

#define CAPTION_HEIGHT   32
#define CAPTION_BTN_W    46
#define RESIZE_BORDER     6

#define CTRL_PANEL_H     90
#define CTRL_PANEL_MARGIN 25
#define CTRL_CORNER_R    12.0f

#define CTRL_NONE         0
#define CTRL_PLAY         1
#define CTRL_REW          2
#define CTRL_FWD          3
#define CTRL_SEEK         4
#define CTRL_VOL_SLIDER   5
#define CTRL_VOL_ICON     6
#define CTRL_REPEAT       7
#define CTRL_SETTINGS     8
#define CTRL_SUBTITLE     9
#define CTRL_SPEED        10

#define WM_MPV_EVENT        (WM_USER + 1)
#define WM_VIDEO_RESIZE     (WM_USER + 2)
#define WM_FIT_RATIO        (WM_USER + 3)


mpv_render_param MPV_RENDER_PARAM_END() {
    mpv_render_param param = {MPV_RENDER_PARAM_INVALID, 0};
    return param;
}

typedef BOOL (APIENTRY *PFNWGLSWAPINTERVALEXTPROC)(int);

static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT_fn = NULL;

// ──────────────────── 전역 상태 ────────────────────
static BOOL g_fullscreen = FALSE;
static BOOL g_enter_fullscreen = FALSE;
static RECT g_windowed_rect;
static LONG g_windowed_style;

static mpv_handle *mpv = NULL;
static mpv_render_context *mpv_gl = NULL;

static HWND g_hWnd = NULL;
static HDC g_hDC = NULL;
static HGLRC g_hRC = NULL;

static BOOL g_is_playing = FALSE;

static HANDLE g_render_thread = NULL;
static HANDLE g_render_event = NULL;
static volatile BOOL g_running = TRUE;
static volatile BOOL g_in_sizemove = FALSE;
static volatile BOOL g_is_maximized = FALSE;
static volatile BOOL g_always_on_top = FALSE;
#define CAPTION_HOVER_CLOSE       4
#define CAPTION_HOVER_MAXIMIZE    3
#define CAPTION_HOVER_MINIMIZE    2
#define CAPTION_HOVER_ALWAYSONTOP 1
#define CAPTION_HOVER_NONE        0
static volatile int g_hover_btn = CAPTION_HOVER_NONE;

static volatile int g_video_dw = 0;
static volatile int g_video_dh = 0;
static volatile double g_aspect_ratio = 0.0;

static volatile int g_paused = 1;
static volatile double g_duration = 0.0;
static volatile int g_eof_reached = 0;

// ── 오버레이 페이드 ──
static volatile BOOL g_mouse_in_window = FALSE;
static float g_overlay_alpha = 0.0f;
static BOOL g_hide_element = FALSE;

// ── 컨트롤 바 ──
static volatile int g_ctrl_hover = CTRL_NONE;
static volatile BOOL g_seeking = FALSE;
static volatile BOOL g_vol_dragging = FALSE;
static volatile double g_seek_frac = 0.0;
static volatile double g_vol_frac = 1.0;
static volatile BOOL g_ctrl_down = FALSE;
static volatile BOOL g_is_muted = FALSE;

// GDI 프레임 캡처
static CRITICAL_SECTION g_frame_cs;
static unsigned char *g_frame_buf = NULL;
static int g_frame_w = 0;
static int g_frame_h = 0;
static int g_frame_buf_size = 0;
static BOOL g_frame_valid = FALSE;
static RECT g_restore_rect = {0};
static BOOL g_fitted = FALSE;

static POINT g_point_history[ASIZE_MOUSEIDLE] = {0};
static int g_point_history_index = 0;
static POINT g_point_last = {0};

#define IDT_MOUSE_CHECK 702
#define IDT_OPEN_FILE   703
#define IDT_DEBUG_INFO  777

static volatile BOOL g_mouse_idle = FALSE;

static float g_play_speed = 1.0f;
static volatile int g_repeat_mode = 0;
static volatile BOOL g_sub_enabled = TRUE;

static UINT WM_PXVY_COMMAND = 0;

// ++++++++++++++++++++++++ THEME +++++++++++++++++++

typedef struct OSTheme {
    Color3 background;
    Color3 foreground;
} OSTheme;

static volatile OSTheme os_theme;
static Color3 g_primary_color;
static HBRUSH g_bg_brush = NULL;
#define OS_THEME_BK os_theme.background.r,os_theme.background.g,os_theme.background.b
#define OS_THEME_FG os_theme.foreground.r,os_theme.foreground.g,os_theme.foreground.b
#define OS_THEME_PC g_primary_color.r,g_primary_color.g,g_primary_color.b

#define RGB_THEME_BK    RGB(os_theme.background.r,os_theme.background.g,os_theme.background.b)
#define RGB_THEME_FG    RGB(os_theme.foreground.r,os_theme.foreground.g,os_theme.foreground.b)
#define RGB_THEME_PRIMRY RGB(g_primary_color.r, g_primary_color.g, g_primary_color.b);

static GLuint g_logo_tex = 0;
static int g_logo_w = 0;
static int g_logo_h = 0;
static GLuint g_splash_tex = 0;
static int g_splash_w = 0;
static int g_splash_h = 0;

// ──────────────────── 폰트 리소스 핸들 ────────────────────
// [변경] RC에서 로드한 폰트 핸들 전역 보관
static HANDLE g_font_handle_notosans = NULL;
static HANDLE g_font_handle_poppins = NULL;
static HANDLE g_font_handle_roboto = NULL;
static HANDLE g_font_handle_d2coding = NULL;
// 단일 폰트 리소스 로드 헬퍼
static HANDLE load_font_resource(HINSTANCE hInst, int res_id) {
    HRSRC hRes = FindResourceA(hInst, MAKEINTRESOURCE(res_id), RT_FONT);
    if (!hRes) return NULL;
    HGLOBAL hGlob = LoadResource(hInst, hRes);
    if (!hGlob) return NULL;
    void *pData = LockResource(hGlob);
    DWORD size = SizeofResource(hInst, hRes);
    if (!pData || size == 0) return NULL;
    DWORD nFonts = 0;
    return AddFontMemResourceEx(pData, size, NULL, &nFonts);
}

static void load_fonts_from_resource(HINSTANCE hInst) {
    g_font_handle_notosans = load_font_resource(hInst, IDR_FONT_NOTOSANS);
    g_font_handle_poppins = load_font_resource(hInst, IDR_FONT_POPPINS);
    g_font_handle_roboto = load_font_resource(hInst, IDR_FONT_ROBOTO);
    g_font_handle_d2coding = load_font_resource(hInst, IDR_FONT_D2CODING);
}

#define UNLOAD_FONT(VAL) if(VAL){RemoveFontMemResourceEx(VAL);VAL=NULL;}

static void unload_fonts_from_resource(void) {
    UNLOAD_FONT(g_font_handle_notosans);
    UNLOAD_FONT(g_font_handle_poppins);
    UNLOAD_FONT(g_font_handle_roboto);
    UNLOAD_FONT(g_font_handle_d2coding);
}

// ███████╗░█████╗░███╗░░██╗████████╗░░░░░░██████╗░██╗░█████╗░██╗░░██╗███████╗██████╗░
// ██╔════╝██╔══██╗████╗░██║╚══██╔══╝░░░░░░██╔══██╗██║██╔══██╗██║░██╔╝██╔════╝██╔══██╗
// █████╗░░██║░░██║██╔██╗██║░░░██║░░░░░░░░░██████╔╝██║██║░░╚═╝█████═╝░█████╗░░██████╔╝
// ██╔══╝░░██║░░██║██║╚████║░░░██║░░░░░░░░░██╔═══╝░██║██║░░██╗██╔═██╗░██╔══╝░░██╔══██╗
// ██║░░░░░╚█████╔╝██║░╚███║░░░██║░░░█████╗██║░░░░░██║╚█████╔╝██║░╚██╗███████╗██║░░██║
// ╚═╝░░░░░░╚════╝░╚═╝░░╚══╝░░░╚═╝░░░╚════╝╚═╝░░░░░╚═╝░╚════╝░╚═╝░░╚═╝╚══════╝╚═╝░░╚═╝
static char g_subtitle_font_family[LF_FACESIZE] = FONT_SUBTITLE_FAMILY;

static void on_font_pick(const char *family, void *ud) {
    (void) ud;
    strncpy_s(g_subtitle_font_family, LF_FACESIZE, family, _TRUNCATE);
    mpv_set_option_string(mpv, "sub-font", g_subtitle_font_family);
    pxvy_set_subtitle_font(g_subtitle_font_family); // DB 저장
}

// ─────────────────────────────────────────────────────────

void SetPrimaryColor(void) {
    g_primary_color = pxvy_get_color();
}

void CheckOSTheme(void) {
    BOOL is_dark_mode = TRUE;

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD value = 0;
        DWORD size = sizeof(value);
        if (RegQueryValueExW(hKey, L"AppsUseLightTheme",
                             NULL, NULL, (LPBYTE) &value, &size) == ERROR_SUCCESS) {
            is_dark_mode = (value == 0);
        }
        RegCloseKey(hKey);
    }

    if (is_dark_mode) {
        os_theme.background.r = 34;
        os_theme.background.g = 37;
        os_theme.background.b = 41;
        os_theme.foreground.r = 255;
        os_theme.foreground.g = 255;
        os_theme.foreground.b = 255;
    } else {
        os_theme.background.r = 255;
        os_theme.background.g = 255;
        os_theme.background.b = 255;
        os_theme.foreground.r = 51;
        os_theme.foreground.g = 51;
        os_theme.foreground.b = 51;
    }
}

static volatile BOOL g_pending_show = FALSE;
static int g_pending_show_cmd = SW_SHOWNORMAL;

// $$$$$$$$$$$$$$$$$$$$$ Mediainfo $$$$$$$$$$$$$$$$$$$$$$$$$$
#include "mediainfo_c.h"
char g_file_info[4096] = {0};
static volatile BOOL g_show_info = FALSE;

static GLuint g_info_tex = 0;
static int g_info_tex_w = 0;
static int g_info_tex_h = 0;
static int g_info_tex_vh = 0;
static char g_info_tex_key[4096] = {0};

// ──────────────────── 유틸리티 ────────────────────
BOOL is_maximized_window(HWND hWnd, RECT rect) {
    HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfo(hMon, &mi);
    int workW = mi.rcWork.right - mi.rcWork.left;
    int workH = mi.rcWork.bottom - mi.rcWork.top;

    int wW = workW;
    int wH = (int) (wW / g_aspect_ratio + 0.5);
    if (wH > workH) {
        wH = workH;
        wW = (int) (wH * g_aspect_ratio + 0.5);
    }

    int x = mi.rcWork.left + (workW - wW) / 2;
    int y = mi.rcWork.top + (workH - wH) / 2;
    if (x == rect.left && y == rect.top && x + wW == rect.right && y + wH == rect.bottom)
        return TRUE;
    return FALSE;
}

static void toggle_fullscreen(HWND hWnd) {
    if (!g_fullscreen) {
        g_windowed_style = GetWindowLong(hWnd, GWL_STYLE);
        GetWindowRect(hWnd, &g_windowed_rect);

        HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = {sizeof(mi)};
        GetMonitorInfo(hMon, &mi);

        SetWindowLong(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hWnd, HWND_TOP,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_NOACTIVATE);

        COLORREF no_border = 0xFFFFFFFE;
        DwmSetWindowAttribute(hWnd, 34, &no_border, sizeof(no_border));
        g_mouse_idle = TRUE;

        g_hide_element = TRUE;
        if (mpv) mpv_set_option_string(mpv, "background-color", "#000000");
        g_enter_fullscreen = TRUE;
    } else {
        SetWindowLong(hWnd, GWL_STYLE, g_windowed_style);
        SetWindowPos(hWnd, NULL,
                     g_windowed_rect.left, g_windowed_rect.top,
                     g_windowed_rect.right - g_windowed_rect.left,
                     g_windowed_rect.bottom - g_windowed_rect.top,
                     SWP_NOZORDER | SWP_FRAMECHANGED);

        COLORREF border = RGB(g_primary_color.r, g_primary_color.g, g_primary_color.b);
        DwmSetWindowAttribute(hWnd, 34, &border, sizeof(border));

        if (mpv) mpv_set_option_string(mpv, "background-color", "#222529");

        RECT rect;
        GetWindowRect(hWnd, &rect);
        g_is_maximized = is_maximized_window(hWnd, rect);
    }
    g_fullscreen = !g_fullscreen;
}

// static int open_file_dialog(HWND hWnd, char *utf8path, int maxlen) {
//     wchar_t wpath[MAX_PATH] = {0};
//     OPENFILENAMEW ofn = {0};
//     ofn.lStructSize = sizeof(ofn);
//     ofn.hwndOwner = hWnd;
//     ofn.lpstrFilter =
//             L"Media Files\0"
//             L"*.mp4;*.mkv;*.avi;*.mov;*.webm;*.flv;*.wmv;*.ts;*.m2ts;*.mts;*.vob;*.rm;*.rmvb;*.asf;*.divx;*.xvid;*.3gp;*.3g2;*.f4v;*.ogv;*.m4v;*.mpg;*.mpeg;*.mp2;*.mpe;*.mpv;*.m2v;*.mxf;*.dv;*.wtv;*.dvr-ms;"
//             L"*.mp3;*.aac;*.flac;*.wav;*.ogg;*.opus;*.m4a;*.wma;*.ape;*.alac;*.aiff;*.aif;*.mka;*.ac3;*.dts;*.amr;*.mid;*.midi;*.ra;*.tta;*.wv\0";
//     ofn.lpstrFile = wpath;
//     ofn.nMaxFile = MAX_PATH;
//     ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
//     if (!GetOpenFileNameW(&ofn)) return 0;
//
//     char dbg[MAX_PATH * 3];
//     wide_to_utf8(wpath, dbg, sizeof(dbg));
//     printf("path: %s\n", dbg);
//
//     return wide_to_utf8(wpath, utf8path, maxlen);
// }

#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
static HHOOK g_file_dlg_hook = NULL;
static COLORREF g_file_dlg_color = 0;

static LRESULT CALLBACK file_dlg_cbt_proc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HCBT_ACTIVATE) {
        HWND hwnd = (HWND) wParam;
        wchar_t cls[64] = {0};
        GetClassNameW(hwnd, cls, 64);
        // 파일 다이얼로그 최상위 창은 항상 "#32770"
        if (wcscmp(cls, L"#32770") == 0) {
            COLORREF color = g_file_dlg_color;
            DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &color, sizeof(color));
            DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &color, sizeof(color));
        }
    }
    return CallNextHookEx(g_file_dlg_hook, code, wParam, lParam);
}

static int open_file_dialog(HWND hWnd, char *utf8path, int maxlen) {
    wchar_t wpath[MAX_PATH] = {0};
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter =
            L"Media Files\0"
            L"*.mp4;*.mkv;*.avi;*.mov;*.webm;*.flv;*.wmv;*.ts;*.m2ts;*.mts;*.vob;*.rm;*.rmvb;*.asf;*.divx;*.xvid;*.3gp;*.3g2;*.f4v;*.ogv;*.m4v;*.mpg;*.mpeg;*.mp2;*.mpe;*.mpv;*.m2v;*.mxf;*.dv;*.wtv;*.dvr-ms;"
            L"*.mp3;*.aac;*.flac;*.wav;*.ogg;*.opus;*.m4a;*.wma;*.ape;*.alac;*.aiff;*.aif;*.mka;*.ac3;*.dts;*.amr;*.mid;*.midi;*.ra;*.tta;*.wv\0";
    ofn.lpstrFile = wpath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST; // ← 훅 플래그 없음

    // GetOpenFileNameW 호출 전에 같은 스레드에 CBT 훅 설치
    g_file_dlg_color = RGB(g_primary_color.r, g_primary_color.g, g_primary_color.b);
    g_file_dlg_hook = SetWindowsHookExW(WH_CBT, file_dlg_cbt_proc,
                                        NULL, GetCurrentThreadId());

    BOOL ok = GetOpenFileNameW(&ofn);

    if (g_file_dlg_hook) {
        UnhookWindowsHookEx(g_file_dlg_hook);
        g_file_dlg_hook = NULL;
    }

    if (!ok) return 0;

    char dbg[MAX_PATH * 3];
    wide_to_utf8(wpath, dbg, sizeof(dbg));
    printf("path: %s\n", dbg);

    return wide_to_utf8(wpath, utf8path, maxlen);
}

static char g_pending_sub[MAX_PATH * 3] = {0};

static void load_file(const char *path) {
    if (!mpv) return;



    // ── smi_to_srt.exe -s "절대경로" 실행 후 stdout에서 자막 경로 수신 ──
    char sub_path[MAX_PATH * 3] = {0};
    {
        // ① exe 디렉터리 (wide)
        wchar_t exe_dir_w[MAX_PATH];
        GetModuleFileNameW(NULL, exe_dir_w, MAX_PATH);
        wchar_t *slash_w = wcsrchr(exe_dir_w, L'\\');
        if (slash_w) *(slash_w + 1) = L'\0';

        // ② path(UTF-8) → wide
        wchar_t path_w[MAX_PATH * 3];
        MultiByteToWideChar(CP_UTF8, 0, path, -1, path_w, MAX_PATH * 3);

        // ② -1 같은 이름의 .smi 파일 존재 여부 확인
        wchar_t smi_path_w[MAX_PATH * 3];
        wcscpy(smi_path_w, path_w);
        wchar_t *dot_w = wcsrchr(smi_path_w, L'.');
        if (!dot_w) goto run_mpv;
        wcscpy(dot_w, L".smi");
        if (GetFileAttributesW(smi_path_w) == INVALID_FILE_ATTRIBUTES)
            goto run_mpv;

        // ③ 커맨드라인 조합 (wide)
        wchar_t cmd_line_w[MAX_PATH * 6];
        _snwprintf(cmd_line_w, MAX_PATH * 6,
                   L"\"%ssmi_to_srt.exe\" -s \"%s\"", exe_dir_w, path_w);

        // ④ stdout 파이프
        HANDLE hRead = NULL, hWrite = NULL;
        SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
        if (!CreatePipe(&hRead, &hWrite, &sa, 0)) goto run_mpv;
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si = {0};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdOutput = hWrite;
        si.hStdError = NULL;
        si.hStdInput = NULL;

        PROCESS_INFORMATION pi = {0};
        if (CreateProcessW(NULL, cmd_line_w, NULL, NULL,
                           TRUE, CREATE_NO_WINDOW,
                           NULL, NULL, &si, &pi)) {
            CloseHandle(hWrite);
            hWrite = NULL;

            WaitForSingleObject(pi.hProcess, 5000);

            // ⑤ stdout 읽기
            char buf[MAX_PATH * 3] = {0};
            DWORD bytesRead = 0;
            ReadFile(hRead, buf, sizeof(buf) - 1, &bytesRead, NULL);

            if (bytesRead > 0) {
                buf[bytesRead] = '\0';

                // 줄바꿈/공백 제거
                char *end = buf + strlen(buf) - 1;
                while (end >= buf &&
                       (*end == '\r' || *end == '\n' || *end == ' '))
                    *end-- = '\0';

                // ⑥ UTF-8 출력 그대로 사용
                if (strlen(buf) > 0)
                    strncpy(sub_path, buf, sizeof(sub_path) - 1);
            }

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        } else {
            if (hWrite) CloseHandle(hWrite);
        }
        CloseHandle(hRead);
    }

run_mpv:
    g_pending_sub[0] = '\0';
    if (sub_path[0] != '\0') {
        strncpy(g_pending_sub, sub_path, sizeof(g_pending_sub) - 1);
        say("subtitle queued: %s", sub_path);
    }

    mpv_set_option_string(mpv, "sub-auto", "fuzzy");
    mpv_set_option_string(mpv, "sub-codepage", "enca:ko:utf-8");
    const char *load_cmd[] = {"loadfile", path, NULL};
    mpv_command(mpv, load_cmd);
    mpv_set_property_string(mpv, "pause", "no");

    StrBuf sb;
    mediainfo_func(path, &sb);
    say(sb.data);
    memset(g_file_info, 0, sizeof(g_file_info));
    strncpy(g_file_info, sb.data, strlen(sb.data));
    sb_free(&sb);
    g_is_playing = TRUE;
}


static void play_or_restart(void) {
    if (!mpv) return;
    if (g_eof_reached) {
        const char *seek_cmd[] = {"seek", "0", "absolute", NULL};
        mpv_command_async(mpv, 0, seek_cmd);
        mpv_set_property_string(mpv, "pause", "no");
    } else {
        const char *cmd[] = {"cycle", "pause", NULL};
        mpv_command_async(mpv, 0, cmd);
    }
}

static void handle_mpv_events(void) {
    if (!mpv) return;
    while (1) {
        mpv_event *event = mpv_wait_event(mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) break;
        // ── 파일 로드 완료 시 자막 추가 ──
        if (event->event_id == MPV_EVENT_START_FILE) {
            if (g_pending_sub[0] != '\0') {
                const char *sub_cmd[] = {"sub-add", g_pending_sub, "select", NULL};
                mpv_command(mpv, sub_cmd);
                g_pending_sub[0] = '\0';
            }
        }
        if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            mpv_event_property *prop = (mpv_event_property *) event->data;
            if (strcmp(prop->name, "video-params/dw") == 0 && prop->format == MPV_FORMAT_INT64) {
                int newDW = (int) (*(int64_t *) prop->data);
                if (newDW != g_video_dw) {
                    g_video_dw = newDW;
                    if (g_video_dw > 0 && g_video_dh > 0) {
                        g_aspect_ratio = (double) g_video_dw / (double) g_video_dh;
                        PostMessageA(g_hWnd, WM_VIDEO_RESIZE, 0, 0);
                    }
                }
            } else if (strcmp(prop->name, "video-params/dh") == 0 && prop->format == MPV_FORMAT_INT64) {
                int newDH = (int) (*(int64_t *) prop->data);
                if (newDH != g_video_dh) {
                    g_video_dh = newDH;
                    if (g_video_dw > 0 && g_video_dh > 0) {
                        g_aspect_ratio = (double) g_video_dw / (double) g_video_dh;
                        PostMessageA(g_hWnd, WM_VIDEO_RESIZE, 0, 0);
                    }
                }
            } else if (strcmp(prop->name, "pause") == 0 && prop->format == MPV_FORMAT_FLAG) {
                g_paused = *(int *) prop->data;
            } else if (strcmp(prop->name, "duration") == 0 && prop->format == MPV_FORMAT_DOUBLE) {
                g_duration = *(double *) prop->data;
            } else if (strcmp(prop->name, "eof-reached") == 0 && prop->format == MPV_FORMAT_FLAG) {
                g_eof_reached = *(int *) prop->data;
            }
        }
    }
}

static void glColor4f_255(int _r, int _g, int _b, float _a) {
    glColor4f(_r / 255.0f, _g / 255.0f, _b / 255.0f, _a);
}

// ──────────────────── 프레임 캡처 ────────────────────

static void capture_frame_to_dib(int w, int h) {
    int needed = w * h * 4;
    EnterCriticalSection(&g_frame_cs);
    if (needed > g_frame_buf_size) {
        free(g_frame_buf);
        g_frame_buf = (unsigned char *) malloc(needed);
        g_frame_buf_size = g_frame_buf ? needed : 0;
    }
    if (g_frame_buf) {
        glReadPixels(0, 0, w, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, g_frame_buf);
        g_frame_w = w;
        g_frame_h = h;
        g_frame_valid = TRUE;
    }
    LeaveCriticalSection(&g_frame_cs);
}

// ──────────────────── mpv 콜백 ────────────────────

static void mpv_wakeup_cb(void *ctx) {
    PostMessageA((HWND) ctx, WM_MPV_EVENT, 0, 0);
}

static void on_mpv_render_update(void *ctx) {
    (void) ctx;
}

static void *get_proc_address(void *ctx, const char *name) {
    void *p = (void *) wglGetProcAddress(name);
    if (!p || p == (void *) 0x1 || p == (void *) 0x2 || p == (void *) 0x3 || p == (void *) (intptr_t) -1)
        p = (void *) GetProcAddress(GetModuleHandleA("opengl32.dll"), name);
    return p;
}

// ──────────────────── OpenGL 초기화 ────────────────────

static BOOL init_opengl(HWND hWnd) {
    g_hDC = GetDC(hWnd);
    if (!g_hDC) return FALSE;
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(g_hDC, &pfd);
    if (!pf) return FALSE;
    SetPixelFormat(g_hDC, pf, &pfd);
    g_hRC = wglCreateContext(g_hDC);
    return g_hRC != NULL;
}

static void cleanup_opengl(void) {
    if (g_hRC) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(g_hRC);
        g_hRC = NULL;
    }
    if (g_hDC && g_hWnd) {
        ReleaseDC(g_hWnd, g_hDC);
        g_hDC = NULL;
    }
}

// ──────────────────── GL 도형 헬퍼 ────────────────────

static void gl_fill_arc(float cx, float cy, float r, float a0, float a1, int n) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= n; i++) {
        float a = a0 + (a1 - a0) * (float) i / (float) n;
        glVertex2f(cx + r * cosf(a), cy - r * sinf(a));
    }
    glEnd();
}

static void gl_fill_rounded_rect(float x, float y, float w, float h, float r) {
    float pi = (float) M_PI;
    float pi2 = pi / 2.0f;
    glBegin(GL_QUADS);
    glVertex2f(x + r, y);
    glVertex2f(x + w - r, y);
    glVertex2f(x + w - r, y + h);
    glVertex2f(x + r, y + h);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(x, y + r);
    glVertex2f(x + r, y + r);
    glVertex2f(x + r, y + h - r);
    glVertex2f(x, y + h - r);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(x + w - r, y + r);
    glVertex2f(x + w, y + r);
    glVertex2f(x + w, y + h - r);
    glVertex2f(x + w - r, y + h - r);
    glEnd();
    gl_fill_arc(x + r, y + r, r, pi2, pi, 8);
    gl_fill_arc(x + w - r, y + r, r, 0, pi2, 8);
    gl_fill_arc(x + w - r, y + h - r, r, pi + pi2, 2 * pi, 8);
    gl_fill_arc(x + r, y + h - r, r, pi, pi + pi2, 8);
}

static void gl_fill_circle(float cx, float cy, float r, int n) {
    const float AA_WIDTH = 1.5f;
    const float inner_r = r - AA_WIDTH;

    float col[4];
    glGetFloatv(GL_CURRENT_COLOR, col);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(col[0], col[1], col[2], col[3]);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= n; i++) {
        float a = 2.0f * (float) M_PI * (float) i / (float) n;
        glVertex2f(cx + inner_r * cosf(a), cy + inner_r * sinf(a));
    }
    glEnd();

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= n; i++) {
        float a = 2.0f * (float) M_PI * (float) i / (float) n;
        float ca = cosf(a), sa = sinf(a);
        glColor4f(col[0], col[1], col[2], 0.0f);
        glVertex2f(cx + r * ca, cy + r * sa);
        glColor4f(col[0], col[1], col[2], col[3]);
        glVertex2f(cx + inner_r * ca, cy + inner_r * sa);
    }
    glEnd();

    glColor4fv(col);
}

// ──────────────────── 비트맵 폰트 ────────────────────

// ── 기본 폰트: [변경] Segoe UI → Poppins ──
static GLuint g_font_base;
static GLYPHMETRICSFLOAT g_glyph_metrics[128];
static int g_font_height = 20;

static void init_font(HDC hdc) {
    g_font_base = glGenLists(128);
    HFONT hFont = CreateFontA(
        -20, 0, 0, 0,
        FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_SWISS,
        FONT_DEFAULT_FAMILY); // [변경] "Segoe UI" → "Poppins"
    HFONT old = (HFONT) SelectObject(hdc, hFont);
    wglUseFontOutlinesA(hdc, 0, 128, g_font_base, 0.0f, 0.0f,
                        WGL_FONT_POLYGONS, g_glyph_metrics);
    SelectObject(hdc, old);
    DeleteObject(hFont);
}

// ── 고정폭 폰트 (Consolas) ──
static GLuint g_mono_font_base;
static GLYPHMETRICSFLOAT g_mono_glyph_metrics[128];
static int g_mono_font_height = 32;

static void init_mono_font(HDC hdc) {
    g_mono_font_base = glGenLists(128);
    HFONT hFont = CreateFontA(
        -16, 0, 0, 0,
        FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,
        FIXED_PITCH | FF_MODERN,
        FONT_MONOSPACE_FAMILY);
    HFONT old = (HFONT) SelectObject(hdc, hFont);
    wglUseFontOutlinesA(hdc, 0, 128, g_mono_font_base, 0.0f, 0.0f,
                        WGL_FONT_POLYGONS, g_mono_glyph_metrics);
    SelectObject(hdc, old);
    DeleteObject(hFont);
}

static void gl_draw_string_mono(float x, float y, const char *str) {
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glScalef((float) g_mono_font_height, -(float) g_mono_font_height, 1.0f);
    glPushAttrib(GL_LIST_BIT);
    glListBase(g_mono_font_base);
    glCallLists((GLsizei) strlen(str), GL_UNSIGNED_BYTE, str);
    glPopAttrib();
    glPopMatrix();
}

static void gl_info_rebuild_cache(
    const char *const *lines, int line_count,
    float font_size, float line_gap, int vw, int vh) {
    if (line_count <= 0) return;

    int fh = (int) (font_size + 0.5f);
    if (fh < 4) fh = 4;

    int img_w = vw < 4096 ? vw : 4096;
    int img_h = (int) (line_count * line_gap + fh + 8);
    if (img_w < 1 || img_h < 1) return;

    HFONT hFont = CreateFontA(
        -fh, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, // D2Coding은 고정폭이므로 pitch 플래그도 맞춰주는 게 좋음
        FONT_MONOSPACE_FAMILY);

    HDC hdc = CreateCompatibleDC(NULL);
    HFONT oldF = (HFONT) SelectObject(hdc, hFont);

    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = img_w;
    bi.bmiHeader.biHeight = -img_h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    DWORD *pix = NULL;
    HBITMAP hbm = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, (void **) &pix, NULL, 0);
    HBITMAP oldB = (HBITMAP) SelectObject(hdc, hbm);

    memset(pix, 0, img_w * img_h * sizeof(DWORD));
    SetBkMode(hdc, TRANSPARENT);

    float cy = 0.0f;
    for (int li = 0; li < line_count; li++) {
        wchar_t wbuf[2048];
        MultiByteToWideChar(CP_UTF8, 0, lines[li], -1, wbuf, 2048);
        int iy = (int) cy;
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT r = {0, iy, img_w, img_h};
        DrawTextW(hdc, wbuf, -1, &r, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP);
        cy += line_gap;
    }
    GdiFlush();

    for (int i = 0; i < img_w * img_h; i++) {
        DWORD c = pix[i];
        BYTE b = (c) & 0xFF;
        BYTE g = (c >> 8) & 0xFF;
        BYTE r = (c >> 16) & 0xFF;
        BYTE a = r > g ? (r > b ? r : b) : (g > b ? g : b);
        pix[i] = ((DWORD) a << 24) | ((DWORD) r << 16) | ((DWORD) g << 8) | b;
    }

    if (g_info_tex) glDeleteTextures(1, &g_info_tex);
    glGenTextures(1, &g_info_tex);
    glBindTexture(GL_TEXTURE_2D, g_info_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_w, img_h, 0,
                 GL_BGRA_EXT, GL_UNSIGNED_BYTE, pix);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    g_info_tex_w = img_w;
    g_info_tex_h = img_h;

    SelectObject(hdc, oldB);
    SelectObject(hdc, oldF);
    DeleteObject(hbm);
    DeleteDC(hdc);
    DeleteObject(hFont);
}

static void load_logo_texture(const char *path, int target_w, int target_h) {
    int src_w, src_h, ch;
    unsigned char *src = stbi_load(path, &src_w, &src_h, &ch, 4);
    if (!src) {
        MessageBoxA(NULL, "Error loading logo texture", "Error", MB_OK);
        exit(EXIT_FAILURE);
    }

    unsigned char *dst = (unsigned char *) malloc(target_w * target_h * 4);
    if (!dst) {
        stbi_image_free(src);
        return;
    }

    stbir_resize_uint8_linear(src, src_w, src_h, 0, dst, target_w, target_h, 0, STBIR_RGBA);
    stbi_image_free(src);

    if (g_logo_tex) glDeleteTextures(1, &g_logo_tex);
    glGenTextures(1, &g_logo_tex);
    glBindTexture(GL_TEXTURE_2D, g_logo_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, target_w, target_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, dst);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    free(dst);

    g_logo_w = target_w;
    g_logo_h = target_h;
}

static float gl_measure_string(const char *str) {
    float w = 0.0f;
    for (const char *p = str; *p; p++) {
        int idx = (unsigned char) *p;
        if (idx >= 0 && idx < 128)
            w += g_glyph_metrics[idx].gmfCellIncX * (float) g_font_height;
    }
    return w;
}

static void gl_draw_string(float x, float y, const char *str) {
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glScalef((float) g_font_height, -(float) g_font_height, 1.0f);
    glPushAttrib(GL_LIST_BIT);
    glListBase(g_font_base);
    glCallLists((GLsizei) strlen(str), GL_UNSIGNED_BYTE, str);
    glPopAttrib();
    glPopMatrix();
}

static float gl_draw_time(float x, float y, double seconds, float oa) {
    int total = (int) seconds;
    if (total < 0) total = 0;

    int hh = total / 3600;
    int mm = (total % 3600) / 60;
    int ss = total % 60;

    char buf[16];
    if (hh > 0) snprintf(buf, sizeof(buf), "%d:%02d:%02d", hh, mm, ss);
    else snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);

    glEnable(GL_POLYGON_SMOOTH);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f_int(os_theme.background.r, os_theme.background.g, os_theme.background.b, 0.55f * oa);
    gl_draw_string(x + 1.0f, y + 13.0f, buf);
    glColor4f_int(os_theme.foreground.r, os_theme.foreground.g, os_theme.foreground.b, 0.9f * oa);
    gl_draw_string(x, y + 12.0f, buf);
    glDisable(GL_POLYGON_SMOOTH);

    return x + gl_measure_string(buf);
}

// ──────────────────── GL 캡션바 ────────────────────

static void gl_draw_caption(int w, int h, float oa) {
    if (oa < 0.01f) return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int hover = g_hover_btn;
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    float alpha = 0.82f * oa;

    glColor4f_255(OS_THEME_BK, alpha);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f((float) w, 0);
    glVertex2f((float) w, (float) CAPTION_HEIGHT);
    glVertex2f(0, (float) CAPTION_HEIGHT);
    glEnd();

    if (g_logo_tex && g_logo_w > 0) {
        float lw = (float) g_logo_w, lh = (float) g_logo_h;
        float lx = 15.0f;
        float ly = ((float) CAPTION_HEIGHT - lh) * 0.5f + 1;
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, g_logo_tex);
        glColor4f(1.0f, 1.0f, 1.0f, oa);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0);
        glVertex2f(lx, ly);
        glTexCoord2f(1, 0);
        glVertex2f(lx + lw, ly);
        glTexCoord2f(1, 1);
        glVertex2f(lx + lw, ly + lh);
        glTexCoord2f(0, 1);
        glVertex2f(lx, ly + lh);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    }

    {
        float tx = 16.0f + (g_logo_tex && g_logo_w > 0 ? (float) g_logo_w + 6.0f : 0.0f);
        glEnable(GL_POLYGON_SMOOTH);
        glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f_255(OS_THEME_BK, 0.5f * oa);
        gl_draw_string(tx + 1.0f, (float) CAPTION_HEIGHT * 0.70f + 1.0f, "PXVY");
        glColor4f_255(OS_THEME_FG, 0.9f * oa);
        gl_draw_string(tx, (float) CAPTION_HEIGHT * 0.70f, "PXVY");
        glDisable(GL_POLYGON_SMOOTH);
    }

    float bH = (float) CAPTION_HEIGHT, bW = (float) CAPTION_BTN_W;
    float re = (float) w, cy = bH / 2, s = 5, cx;

    float cL = re - bW;
    {
        if (hover == CAPTION_HOVER_CLOSE)
            glColor4f_255(g_primary_color.r, g_primary_color.g, g_primary_color.b, .9f * oa);
        else
            glColor4f(0, 0, 0, 0);
        glBegin(GL_QUADS);
        glVertex2f(cL, 0);
        glVertex2f(re, 0);
        glVertex2f(re, bH);
        glVertex2f(cL, bH);
        glEnd();
        cx = cL + bW / 2;
        glColor4f_255(OS_THEME_FG, .9f * oa);
        glLineWidth(1.5f);
        glBegin(GL_LINES);
        glVertex2f(cx - s, cy - s);
        glVertex2f(cx + s, cy + s);
        glVertex2f(cx + s, cy - s);
        glVertex2f(cx - s, cy + s);
        glEnd();
    }

    float mxL = cL - bW;
    {
        if (hover == CAPTION_HOVER_MAXIMIZE)
            glColor4f_255(OS_THEME_FG, .15f * oa);
        else
            glColor4f(0, 0, 0, 0);

        if (g_is_maximized) {
            glBegin(GL_QUADS);
            glVertex2f(mxL, 0);
            glVertex2f(cL, 0);
            glVertex2f(cL, bH);
            glVertex2f(mxL, bH);
            glEnd();
            cx = mxL + bW / 2;
            glColor4f_255(OS_THEME_FG, .9f * oa);
            glLineWidth(1.2f);
            float o = s * 0.4f;
            glBegin(GL_LINE_LOOP);
            glVertex2f(cx - s, cy - s);
            glVertex2f(cx + s - o, cy - s);
            glVertex2f(cx + s - o, cy + s - o);
            glVertex2f(cx - s, cy + s - o);
            glEnd();
            glBegin(GL_LINE_STRIP);
            glVertex2f(cx - s + o, cy + s - o);
            glVertex2f(cx - s + o, cy + s);
            glVertex2f(cx + s, cy + s);
            glVertex2f(cx + s, cy - s + o);
            glVertex2f(cx + s - o, cy - s + o);
            glEnd();
        } else {
            glBegin(GL_QUADS);
            glVertex2f(mxL, 0);
            glVertex2f(cL, 0);
            glVertex2f(cL, bH);
            glVertex2f(mxL, bH);
            glEnd();
            cx = mxL + bW / 2;
            glColor4f_255(OS_THEME_FG, .9f * oa);
            glLineWidth(1.2f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(cx - s, cy - s);
            glVertex2f(cx + s, cy - s);
            glVertex2f(cx + s, cy + s);
            glVertex2f(cx - s, cy + s);
            glEnd();
        }
    }

    float mnL = mxL - bW;
    {
        if (hover == CAPTION_HOVER_MINIMIZE)
            glColor4f_255(OS_THEME_FG, .15f * oa);
        else
            glColor4f(0, 0, 0, 0);
        glBegin(GL_QUADS);
        glVertex2f(mnL, 0);
        glVertex2f(mxL, 0);
        glVertex2f(mxL, bH);
        glVertex2f(mnL, bH);
        glEnd();
        cx = mnL + bW / 2;
        glColor4f_255(OS_THEME_FG, .9f * oa);
        glLineWidth(1.2f);
        glBegin(GL_LINES);
        glVertex2f(cx - s, cy);
        glVertex2f(cx + s, cy);
        glEnd();
    }

    {
        float aoL = mnL - bW;
        if (hover == CAPTION_HOVER_ALWAYSONTOP)
            glColor4f_255(OS_THEME_FG, .15f * oa);
        else
            glColor4f(0, 0, 0, 0);
        glBegin(GL_QUADS);
        glVertex2f(aoL, 0);
        glVertex2f(mnL, 0);
        glVertex2f(mnL, bH);
        glVertex2f(aoL, bH);
        glEnd();

        cx = aoL + bW / 2;
        float ps = s * 0.88f;
        float gw = ps * 0.38f;
        float bw = ps * 0.92f;
        float ptop = cy - ps * 1.10f;
        float g_bot = ptop + ps * 0.33f;
        float b_mid = g_bot + ps * 0.52f;
        float b_bot = b_mid + ps * 0.40f;
        float s_bot = b_bot + ps * 0.95f;
        const int arc_n = 14;

        glColor4f_255(OS_THEME_FG, .9f * oa);
        glLineWidth(1.3f);

        if (g_always_on_top) {
            glBegin(GL_QUADS);
            glVertex2f(cx - gw, ptop);
            glVertex2f(cx + gw, ptop);
            glVertex2f(cx + gw, g_bot);
            glVertex2f(cx - gw, g_bot);
            glEnd();
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx, g_bot);
            glVertex2f(cx - gw, g_bot);
            glVertex2f(cx - bw, b_mid);
            for (int i = 0; i <= arc_n; i++) {
                float a = (float) M_PI * (1.0f - (float) i / (float) arc_n);
                glVertex2f(cx + bw * cosf(a), b_mid + (b_bot - b_mid) * sinf(a));
            }
            glVertex2f(cx + gw, g_bot);
            glEnd();
            glBegin(GL_LINES);
            glVertex2f(cx, b_bot);
            glVertex2f(cx, s_bot);
            glEnd();
        } else {
            glBegin(GL_LINE_LOOP);
            glVertex2f(cx - gw, ptop);
            glVertex2f(cx + gw, ptop);
            glVertex2f(cx + gw, g_bot);
            glVertex2f(cx - gw, g_bot);
            glEnd();
            glBegin(GL_LINE_STRIP);
            glVertex2f(cx - gw, g_bot);
            glVertex2f(cx - bw, b_mid);
            for (int i = 0; i <= arc_n; i++) {
                float a = (float) M_PI * (1.0f - (float) i / (float) arc_n);
                glVertex2f(cx + bw * cosf(a), b_mid + (b_bot - b_mid) * sinf(a));
            }
            glVertex2f(cx + gw, g_bot);
            glEnd();
            glBegin(GL_LINES);
            glVertex2f(cx, b_bot);
            glVertex2f(cx, s_bot);
            glEnd();
        }
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glDisable(GL_BLEND);
}

// ──────────────────── 컨트롤 레이아웃 + 히트테스트 ────────────────────

typedef struct {
    float px, py, pw, ph;
    float seekX, seekY, seekW;
    float volX, volY, volW;
    float playX, playY;
    float rewX, fwdX, btnCY;
    float timeY, timeLeftX, timeRightX;
    float volIconX, volIconY;
    float repeatX, repeatY;
    float settingsX, settingsY;
    float subtitleX, subtitleY;
    float speedX, speedY;
} CtrlLayout;

static CtrlLayout ctrl_layout(int w, int h) {
    CtrlLayout L = {0};
    L.pw = (float) w * 0.58f;
    if (L.pw < 460) L.pw = 460;
    if (L.pw > (float) w - 40) L.pw = (float) w - 40;
    L.ph = (float) CTRL_PANEL_H;
    L.px = ((float) w - L.pw) / 2.0f;
    L.py = (float) h - L.ph - (float) CTRL_PANEL_MARGIN;

    float centerX = L.px + L.pw / 2.0f;
    L.btnCY = L.py + 28.0f;
    L.playX = centerX;
    L.playY = L.btnCY;
    L.rewX = centerX - 60.0f;
    L.fwdX = centerX + 60.0f;

    L.volIconX = L.px + 18.0f;
    L.volIconY = L.btnCY;
    L.volX = L.px + 52.0f;
    L.volY = L.btnCY;
    L.volW = 80.0f;

    L.settingsX = L.px + L.pw - 18.0f;
    L.settingsY = L.btnCY;
    L.repeatX = L.px + L.pw - 54.0f;
    L.repeatY = L.btnCY;
    L.subtitleX = L.px + L.pw - 90.0f;
    L.subtitleY = L.btnCY;
    L.speedX = L.px + L.pw - 132.0f;
    L.speedY = L.btnCY;

    L.timeY = L.py + 62.0f;
    L.timeLeftX = L.px + 15.0f;
    L.timeRightX = L.px + L.pw - 60.0f;
    L.seekX = L.px + 68.0f + 10;
    L.seekW = L.pw - 136.0f - 10;
    L.seekY = L.timeY + 6.0f;

    return L;
}

static int ctrl_hittest(int w, int h, int mx, int my) {
    CtrlLayout L = ctrl_layout(w, h);
    if ((float) mx < L.px || (float) mx > L.px + L.pw ||
        (float) my < L.py || (float) my > L.py + L.ph)
        return CTRL_NONE;

    float fmx = (float) mx, fmy = (float) my;

    if (fmx > L.playX - 18 && fmx < L.playX + 18 && fmy > L.playY - 16 && fmy < L.playY + 16) return CTRL_PLAY;
    if (fmx > L.rewX - 18 && fmx < L.rewX + 18 && fmy > L.btnCY - 16 && fmy < L.btnCY + 16) return CTRL_REW;
    if (fmx > L.fwdX - 18 && fmx < L.fwdX + 18 && fmy > L.btnCY - 16 && fmy < L.btnCY + 16) return CTRL_FWD;
    if (fmx >= L.seekX - 6 && fmx <= L.seekX + L.seekW + 6 && fmy >= L.seekY - 10 && fmy <= L.seekY + 10)
        return
                CTRL_SEEK;
    if (fmx >= L.volX - 6 && fmx <= L.volX + L.volW + 6 && fmy >= L.volY - 10 && fmy <= L.volY + 10)
        return
                CTRL_VOL_SLIDER;
    if (fmx >= L.volIconX - 8 && fmx <= L.volIconX + 16 && fmy >= L.volIconY - 12 && fmy <= L.volIconY + 12)
        return
                CTRL_VOL_ICON;
    if (fmx >= L.settingsX - 13 && fmx <= L.settingsX + 13 && fmy >= L.settingsY - 13 && fmy <= L.settingsY + 13)
        return
                CTRL_SETTINGS;
    if (fmx >= L.repeatX - 13 && fmx <= L.repeatX + 13 && fmy >= L.repeatY - 13 && fmy <= L.repeatY + 13)
        return
                CTRL_REPEAT;
    if (fmx >= L.subtitleX - 13 && fmx <= L.subtitleX + 13 && fmy >= L.subtitleY - 13 && fmy <= L.subtitleY + 13)
        return
                CTRL_SUBTITLE;
    if (fmx >= L.speedX - 20 && fmx <= L.speedX + 20 && fmy >= L.speedY - 13 && fmy <= L.speedY + 13) return CTRL_SPEED;

    return CTRL_NONE;
}

// ──────────────────── 아이콘: 기어(설정) ────────────────────

static void gl_draw_gear_icon(float cx, float cy, float r, float oa, BOOL hover) {
    float alpha = (hover ? 1.0f : 0.72f) * oa;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int teeth = 6;
    float ro = r;
    float rb = r * 0.70f;
    float ri = r * 0.40f;
    float tw = (float) M_PI / (float) teeth * 0.52f;
    float fringe = 1.4f;

    glColor4f_255(OS_THEME_FG, alpha);
    int segs_ring = 48;
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segs_ring * teeth; i++) {
        float base_a = 2.0f * (float) M_PI * i / (segs_ring * teeth);
        float tooth_step = 2.0f * (float) M_PI / (float) teeth;
        float within = fmodf(base_a + 1000.0f * tooth_step, tooth_step);
        float cur_r = (within < tw || within > tooth_step - tw) ? ro : rb;
        glVertex2f(cx + cur_r * cosf(base_a), cy + cur_r * sinf(base_a));
    }
    glEnd();

    glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f_255(OS_THEME_BK, alpha);
    int hole_segs = 32;
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= hole_segs; i++) {
        float a = 2.0f * (float) M_PI * i / hole_segs;
        glVertex2f(cx + ri * cosf(a), cy + ri * sinf(a));
    }
    glEnd();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBegin(GL_TRIANGLE_STRIP);
    int aa_segs = segs_ring * teeth;
    for (int i = 0; i <= aa_segs; i++) {
        float base_a = 2.0f * (float) M_PI * i / aa_segs;
        float tooth_step = 2.0f * (float) M_PI / (float) teeth;
        float within = fmodf(base_a + 1000.0f * tooth_step, tooth_step);
        float cur_r = (within < tw || within > tooth_step - tw) ? ro : rb;
        float ca = cosf(base_a), sa = sinf(base_a);
        glColor4f_255(OS_THEME_FG, alpha);
        glVertex2f(cx + cur_r * ca, cy + cur_r * sa);
        glColor4f(0.9f, 0.9f, 0.9f, 0.0f);
        glVertex2f(cx + (cur_r + fringe) * ca, cy + (cur_r + fringe) * sa);
    }
    glEnd();

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= hole_segs; i++) {
        float a = 2.0f * (float) M_PI * i / hole_segs;
        float ca = cosf(a), sa = sinf(a);
        glColor4f(0.9f, 0.9f, 0.9f, 0.0f);
        glVertex2f(cx + (ri - fringe) * ca, cy + (ri - fringe) * sa);
        glColor4f_255(OS_THEME_FG, alpha);
        glVertex2f(cx + ri * ca, cy + ri * sa);
    }
    glEnd();
}

// ──────────────────── 아이콘: 반복 ────────────────────

static void gl_draw_repeat_icon(float cx, float cy, float r, int mode, float oa, BOOL hover) {
    float base_alpha = (mode > 0) ? 1.0f : 0.42f;
    float alpha = base_alpha * (hover ? 1.0f : 0.82f) * oa;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    if (mode > 0)
        glColor4f_255(g_primary_color.r, g_primary_color.g, g_primary_color.b, alpha);
    else
        glColor4f_255(OS_THEME_FG, alpha);

    glLineWidth(1.5f);

    float w = r * 0.82f;
    float h = r * 0.48f;
    float cr = r * 0.26f;
    float aw = r * 0.28f;
    int cn = 5;

    glBegin(GL_LINE_STRIP);
    glVertex2f(cx - w + cr, cy - h);
    glVertex2f(cx + w - cr, cy - h);
    for (int i = 0; i <= cn; i++) {
        float a = -(float) M_PI * 0.5f + (float) M_PI * 0.5f * i / cn;
        glVertex2f(cx + w - cr + cr * cosf(a), cy - h + cr + cr * sinf(a));
    }
    glVertex2f(cx + w, cy + h - cr - aw * 0.6f);
    glEnd();

    glBegin(GL_TRIANGLES);
    glVertex2f(cx + w - aw * 0.65f, cy + h - cr - aw * 0.55f);
    glVertex2f(cx + w + aw * 0.65f, cy + h - cr - aw * 0.55f);
    glVertex2f(cx + w, cy + h - cr + aw * 0.45f);
    glEnd();

    glBegin(GL_LINE_STRIP);
    glVertex2f(cx + w - cr, cy + h);
    glVertex2f(cx - w + cr, cy + h);
    for (int i = 0; i <= cn; i++) {
        float a = (float) M_PI * 0.5f + (float) M_PI * 0.5f * i / cn;
        glVertex2f(cx - w + cr + cr * cosf(a), cy + h - cr + cr * sinf(a));
    }
    glVertex2f(cx - w, cy - h + cr + aw * 0.6f);
    glEnd();

    glBegin(GL_TRIANGLES);
    glVertex2f(cx - w - aw * 0.65f, cy - h + cr + aw * 0.55f);
    glVertex2f(cx - w + aw * 0.65f, cy - h + cr + aw * 0.55f);
    glVertex2f(cx - w, cy - h + cr - aw * 0.45f);
    glEnd();

    glDisable(GL_LINE_SMOOTH);

    if (mode == 1) {
        glLineWidth(1.8f);
        float lh = r * 0.28f;
        float lx = cx + 0.5f;
        glBegin(GL_LINES);
        glVertex2f(lx, cy - lh);
        glVertex2f(lx, cy + lh);
        glEnd();
        glBegin(GL_LINES);
        glVertex2f(lx - r * 0.14f, cy - lh + r * 0.14f);
        glVertex2f(lx, cy - lh);
        glEnd();
    }
}

static void apply_repeat_mode(void) {
    if (!mpv) return;
    switch (g_repeat_mode) {
        case 1:
            mpv_set_property_string(mpv, "loop-file", "inf");
            mpv_set_property_string(mpv, "loop-playlist", "no");
            break;
        case 2:
            mpv_set_property_string(mpv, "loop-file", "no");
            mpv_set_property_string(mpv, "loop-playlist", "inf");
            break;
        default:
            mpv_set_property_string(mpv, "loop-file", "no");
            mpv_set_property_string(mpv, "loop-playlist", "no");
            break;
    }
}

// ──────────────────── GL 컨트롤 바 ────────────────────

static void gl_draw_controls(int w, int h, double time_pos, double duration, double volume, int paused, float oa) {
    if (duration <= 0.01 || oa < 0.01f) return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    CtrlLayout L = ctrl_layout(w, h);
    int hover = g_ctrl_hover;
    float alpha = 0.82f * oa;

    glColor4f_255(OS_THEME_BK, alpha);
    gl_fill_rounded_rect(L.px, L.py, L.pw, L.ph, CTRL_CORNER_R);

    float cy = L.btnCY;

    // 볼륨 아이콘
    {
        float ix = L.volIconX, iy = cy;
        float a = ((hover == CTRL_VOL_ICON) ? 1.0f : 0.8f) * oa;
        if (g_ctrl_down) {
            glColor4f_int(OS_THEME_PC, a);
            volume = GetSystemVolume() * 100.0f;
        } else { glColor4f_int(OS_THEME_FG, a); }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_POLYGON_SMOOTH);
        glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

        glBegin(GL_QUADS);
        glVertex2f(ix, iy - 3);
        glVertex2f(ix + 4, iy - 3);
        glVertex2f(ix + 4, iy + 3);
        glVertex2f(ix, iy + 3);
        glEnd();
        glBegin(GL_TRIANGLES);
        glVertex2f(ix + 4, iy - 3);
        glVertex2f(ix + 10, iy - 7);
        glVertex2f(ix + 10, iy + 7);
        glEnd();
        glBegin(GL_TRIANGLES);
        glVertex2f(ix + 4, iy - 3);
        glVertex2f(ix + 10, iy + 7);
        glVertex2f(ix + 4, iy + 3);
        glEnd();

        if (volume < 1.0 || g_is_muted) {
            glColor4f_255(g_primary_color.r, g_primary_color.g, g_primary_color.b, a);
            glLineWidth(2.0f);
            glBegin(GL_LINES);
            glVertex2f(ix - 1, iy + 9);
            glVertex2f(ix + 13, iy - 9);
            glEnd();
        } else {
            glLineWidth(1.5f);
            glBegin(GL_LINE_STRIP);
            for (int i = -4; i <= 4; i++) {
                float t = (float) i / 4.0f;
                glVertex2f(ix + 13 + 5.0f * cosf(t * 0.8f) - 5.0f, iy + t * 5.0f);
            }
            glEnd();
            if (volume > 40.0) {
                glBegin(GL_LINE_STRIP);
                for (int i = -4; i <= 4; i++) {
                    float t = (float) i / 4.0f;
                    glVertex2f(ix + 17 + 8.0f * cosf(t * 0.7f) - 8.0f, iy + t * 7.0f);
                }
                glEnd();
            }
        }
        glDisable(GL_POLYGON_SMOOTH);
        glDisable(GL_LINE_SMOOTH);
    }

    // 볼륨 슬라이더
    {
        float vx = L.volX, vy = L.volY, vw = L.volW;
        double vf = g_vol_dragging ? g_vol_frac : (volume / 100.0);
        if (g_ctrl_down) vf = GetSystemVolume();
        if (vf <= 0) vf = 0;
        if (vf >= 1) vf = 1.0 - EPSILON;

        glColor4f(0.4f, 0.4f, 0.45f, 0.6f * oa);
        glBegin(GL_QUADS);
        glVertex2f(vx, vy - 2);
        glVertex2f(vx + vw, vy - 2);
        glVertex2f(vx + vw, vy + 2);
        glVertex2f(vx, vy + 2);
        glEnd();
        float filledW = (float) (vf * vw);
        if (g_ctrl_down) glColor4f_int(OS_THEME_PC, 0.9f * oa);
        else glColor4f_int(OS_THEME_FG, 0.9f * oa);
        glBegin(GL_QUADS);
        glVertex2f(vx, vy - 2);
        glVertex2f(vx + filledW, vy - 2);
        glVertex2f(vx + filledW, vy + 2);
        glVertex2f(vx, vy + 2);
        glEnd();
        glColor4f_255(OS_THEME_FG, 0.95f * oa);
        gl_fill_circle(vx + filledW, vy, 5.0f, 12);
    }

    // 되감기
    {
        float bx = L.rewX, by = cy, s = 8.0f;
        glColor4f_255(OS_THEME_FG, ((hover == CTRL_REW) ? 1.0f : 0.8f) * oa);
        glBegin(GL_TRIANGLES);
        glVertex2f(bx - s - 2, by);
        glVertex2f(bx - 2, by - s);
        glVertex2f(bx - 2, by + s);
        glEnd();
        glBegin(GL_TRIANGLES);
        glVertex2f(bx + 2, by);
        glVertex2f(bx + s + 2, by - s);
        glVertex2f(bx + s + 2, by + s);
        glEnd();
    }

    // 재생/일시정지
    {
        float bx = L.playX, by = L.playY;
        glColor4f_255(OS_THEME_FG, ((hover == CTRL_PLAY) ? 1.0f : 0.85f) * oa);
        if (paused) {
            float s = 12.0f;
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_POLYGON_SMOOTH);
            glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
            glBegin(GL_TRIANGLES);
            glVertex2f(bx - s * 0.7f, by - s);
            glVertex2f(bx - s * 0.7f, by + s);
            glVertex2f(bx + s * 0.9f, by);
            glEnd();
            glDisable(GL_POLYGON_SMOOTH);
        } else {
            float bw = 4.0f, bh = 11.0f, gap = 3.0f;
            glBegin(GL_QUADS);
            glVertex2f(bx - gap - bw, by - bh);
            glVertex2f(bx - gap, by - bh);
            glVertex2f(bx - gap, by + bh);
            glVertex2f(bx - gap - bw, by + bh);
            glEnd();
            glBegin(GL_QUADS);
            glVertex2f(bx + gap, by - bh);
            glVertex2f(bx + gap + bw, by - bh);
            glVertex2f(bx + gap + bw, by + bh);
            glVertex2f(bx + gap, by + bh);
            glEnd();
        }
    }

    // 빨리감기
    {
        float bx = L.fwdX, by = cy, s = 8.0f;
        glColor4f_255(OS_THEME_FG, ((hover == CTRL_FWD) ? 1.0f : 0.8f) * oa);
        glBegin(GL_TRIANGLES);
        glVertex2f(bx - s - 2, by - s);
        glVertex2f(bx - s - 2, by + s);
        glVertex2f(bx - 2, by);
        glEnd();
        glBegin(GL_TRIANGLES);
        glVertex2f(bx + 2, by - s);
        glVertex2f(bx + 2, by + s);
        glVertex2f(bx + s + 2, by);
        glEnd();
    }

    gl_draw_time(L.timeLeftX, L.timeY, time_pos, oa);
    gl_draw_time(L.timeRightX, L.timeY, duration, oa);

    // 시크바
    {
        float sx = L.seekX, sy = L.seekY, sw = L.seekW;
        double sf = g_seeking ? g_seek_frac : ((duration > 0) ? time_pos / duration : 0);
        if (sf < 0) sf = 0;
        if (sf > 1) sf = 1;
        glColor4f(0.4f, 0.4f, 0.45f, 0.6f * oa);
        glBegin(GL_QUADS);
        glVertex2f(sx, sy - 2);
        glVertex2f(sx + sw, sy - 2);
        glVertex2f(sx + sw, sy + 2);
        glVertex2f(sx, sy + 2);
        glEnd();
        float filledW = (float) (sf * sw);
        glColor4f(0.75f, 0.78f, 0.85f, 0.9f * oa);
        glBegin(GL_QUADS);
        glVertex2f(sx, sy - 2);
        glVertex2f(sx + filledW, sy - 2);
        glVertex2f(sx + filledW, sy + 2);
        glVertex2f(sx, sy + 2);
        glEnd();
        glColor4f_255(OS_THEME_FG, ((hover == CTRL_SEEK || g_seeking) ? 1.0f : 0.7f) * oa);
        gl_fill_circle(sx + filledW, sy, 5.0f, 12);
    }

    gl_draw_gear_icon(L.settingsX, L.settingsY, 9.5f, oa, hover == CTRL_SETTINGS);
    gl_draw_repeat_icon(L.repeatX, L.repeatY, 10.0f, g_repeat_mode, oa, hover == CTRL_REPEAT);

    // 자막 버튼
    {
        float cx2 = L.subtitleX, cy2 = L.subtitleY;
        BOOL sub_on = g_sub_enabled;
        BOOL hov = (hover == CTRL_SUBTITLE);
        float a = (hov ? 1.0f : 0.72f) * oa;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

        glColor4f_int(OS_THEME_FG, sub_on ? a : a * 0.45f);

        float rw = 18.0f, rh = 12.0f;
        float rx = cx2 - rw * 0.5f, ry = cy2 - rh * 0.5f;
        glLineWidth(1.5f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(rx + 2, ry);
        glVertex2f(rx + rw - 2, ry);
        glVertex2f(rx + rw, ry + 2);
        glVertex2f(rx + rw, ry + rh - 2);
        glVertex2f(rx + rw - 2, ry + rh);
        glVertex2f(rx + 2, ry + rh);
        glVertex2f(rx, ry + rh - 2);
        glVertex2f(rx, ry + 2);
        glEnd();

        float lw = rw * 0.55f;
        float ly1 = cy2 - 2.0f, ly2 = cy2 + 2.5f;
        glBegin(GL_LINES);
        glVertex2f(cx2 - lw * 0.5f, ly1);
        glVertex2f(cx2 + lw * 0.5f, ly1);
        glVertex2f(cx2 - lw * 0.5f, ly2);
        glVertex2f(cx2 + lw * 0.35f, ly2);
        glEnd();

        if (!sub_on) {
            glColor4f_int(OS_THEME_PC, a * 0.8f);
            glLineWidth(1.5f);
            glBegin(GL_LINES);
            glVertex2f(rx - 1, ry + rh + 2);
            glVertex2f(rx + rw + 1, ry - 2);
            glEnd();
        }
        glDisable(GL_LINE_SMOOTH);
    }

    // 재생 속도
    {
        float cx2 = L.speedX, cy2 = L.speedY;
        BOOL hov = (hover == CTRL_SPEED);
        float a = (hov ? 1.0f : 0.72f) * oa;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_POLYGON_SMOOTH);
        glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

        char spd_buf[8];
        snprintf(spd_buf, sizeof(spd_buf), "%.1fx", g_play_speed);
        float tw2 = gl_measure_string(spd_buf);
        float tx = cx2 - tw2 * 0.5f;
        float ty = cy2 + 7.0f;

        if (fabsf(g_play_speed - 1.0f) < 0.1f - EPSILON)
            glColor4f_int(OS_THEME_FG, a * 0.72f);
        else
            glColor4f_int(OS_THEME_PC, a);
        gl_draw_string(tx, ty, spd_buf);
        glDisable(GL_POLYGON_SMOOTH);
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glDisable(GL_BLEND);
}

// ──────────────────── 렌더 스레드 ────────────────────
static volatile BOOL g_show_fps = FALSE;

static DWORD WINAPI render_thread_func(LPVOID param) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    wglMakeCurrent(g_hDC, g_hRC);

    wglSwapIntervalEXT_fn = (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");
    if (wglSwapIntervalEXT_fn) wglSwapIntervalEXT_fn(1);

    LARGE_INTEGER freq, prev_time, cur_time;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev_time);
    float fps_display = 0.0f;
    int fps_frame_count = 0;
    float fps_accum = 0.0f;

    ULARGE_INTEGER s_cpu_last_time = {0};
    ULARGE_INTEGER s_cpu_last_kern = {0};
    ULARGE_INTEGER s_cpu_last_user = {0};
    float s_cpu_usage = 0.0f;
    int s_cpu_nproc = 0;
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        s_cpu_nproc = (int) si.dwNumberOfProcessors;
        if (s_cpu_nproc < 1) s_cpu_nproc = 1;
    }

    while (g_running) {
        mpv_render_context_update(mpv_gl);

        RECT rc;
        GetClientRect(g_hWnd, &rc);
        int w = rc.right > 0 ? rc.right : 1;
        int h = rc.bottom > 0 ? rc.bottom : 1;

        // mpv 렌더링
        {
            glViewport(0, 0, w, h);
            if (g_fullscreen) {
                glDisable(GL_SCISSOR_TEST);
                glClearColor(0, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT);
            }

            mpv_opengl_fbo fbo = {0};
            fbo.fbo = 0;
            fbo.w = w;
            fbo.h = h;
            int flip_y = 1, block = 0;
            mpv_render_param params[] = {
                {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
                {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
                {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block},
                MPV_RENDER_PARAM_END()
            };
            mpv_render_context_render(mpv_gl, params);
        }

        // 스플래시
        if (g_aspect_ratio <= 0.0 && !g_in_sizemove) {
            // ← !g_in_sizemove 추가
            glViewport(0, 0, w, h);
            glDisable(GL_SCISSOR_TEST);
            if (g_fullscreen) glClearColor(0, 0, 0, 1);
            else glClearColor(34 / 255.0f, 37 / 255.0f, 41 / 255.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            if (g_splash_tex) {
                glMatrixMode(GL_PROJECTION);
                glPushMatrix();
                glLoadIdentity();
                glOrtho(0, w, h, 0, -1, 1);
                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();
                glLoadIdentity();

                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, g_splash_tex);
                glColor4f(1, 1, 1, 1);

                float iw = (float) g_splash_w, ih = (float) g_splash_h;
                float ix = (w - iw) * 0.5f, iy = (h - ih) * 0.5f;
                glBegin(GL_QUADS);
                glTexCoord2f(0, 0);
                glVertex2f(ix, iy);
                glTexCoord2f(1, 0);
                glVertex2f(ix + iw, iy);
                glTexCoord2f(1, 1);
                glVertex2f(ix + iw, iy + ih);
                glTexCoord2f(0, 1);
                glVertex2f(ix, iy + ih);
                glEnd();

                glDisable(GL_TEXTURE_2D);
                glDisable(GL_BLEND);
                glMatrixMode(GL_MODELVIEW);
                glPopMatrix();
                glMatrixMode(GL_PROJECTION);
                glPopMatrix();
            }
        }

        // 오버레이 페이드
        {
            float target = (g_mouse_in_window && !g_mouse_idle && !g_in_sizemove) ? 1.0f : 0.0f;
            float speed = 0.08f;
            if (g_overlay_alpha < target) {
                g_overlay_alpha += speed;
                if (g_overlay_alpha > target) g_overlay_alpha = target;
            } else if (g_overlay_alpha > target) {
                g_overlay_alpha -= speed;
                if (g_overlay_alpha < target) g_overlay_alpha = target;
            }
        }

        float oa = g_overlay_alpha;
        if (!g_in_sizemove && !g_hide_element) {
            glViewport(0, 0, w, h);
            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);

            if (!g_fullscreen) gl_draw_caption(w, h, oa);

            {
                double time_pos = 0.0, dur = g_duration, vol = 100.0;
                int is_paused = g_paused;
                mpv_get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &time_pos);
                mpv_get_property(mpv, "volume", MPV_FORMAT_DOUBLE, &vol);
                gl_draw_controls(w, h, time_pos, dur, vol, is_paused, oa);
            }
        } else if (!g_is_playing) {
            gl_draw_caption(w, h, oa);
        }

        // 파일 정보 오버레이
        if (g_show_info && g_file_info[0] != '\0') {
            glViewport(0, 0, w, h);
            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glOrtho(0, w, h, 0, -1, 1);
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            char info_copy[4096];
            strncpy(info_copy, g_file_info, sizeof(info_copy) - 1);
            info_copy[sizeof(info_copy) - 1] = '\0';

            const char *lines[128];
            int line_count = 0;
            char *tok = info_copy;
            while (*tok && line_count < 128) {
                lines[line_count++] = tok;
                char *nl = strchr(tok, '\n');
                if (!nl) break;
                *nl = '\0';
                tok = nl + 1;
            }

            CtrlLayout cl = ctrl_layout(w, h);
            float max_bot = cl.py - 4.0f;
            float top_y = g_fullscreen ? 8.0f : (float) (CAPTION_HEIGHT + 8);
            float avail_h = max_bot - top_y;
            if (avail_h < 1.0f) avail_h = 1.0f;

            float font_size = 16.0f;
            if (line_count > 0) {
                float ideal = avail_h / (float) line_count / 1.6f;
                if (ideal < font_size) font_size = ideal;
                if (font_size < 8.0f) font_size = 8.0f;
            }
            float line_gap = font_size * 1.6f;

            if ((strcmp(g_info_tex_key, g_file_info) != 0 || g_info_tex_vh != h)
                && !g_in_sizemove) {
                // ← && !g_in_sizemove 추가
                gl_info_rebuild_cache(lines, line_count, font_size, line_gap, w, h);
                strncpy(g_info_tex_key, g_file_info, sizeof(g_info_tex_key) - 1);
                g_info_tex_vh = h;
            }

            if (g_info_tex && g_info_tex_w > 0 && g_info_tex_h > 0) {
                float x0 = 12.0f, y0 = top_y;
                float tw = (float) g_info_tex_w, th = (float) g_info_tex_h, so = 2.0f;

                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, g_info_tex);
                glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

                glColor4f(0, 0, 0, 0.85f);
                glBegin(GL_QUADS);
                glTexCoord2f(0, 0);
                glVertex2f(x0 + so, y0 + so);
                glTexCoord2f(1, 0);
                glVertex2f(x0 + so + tw, y0 + so);
                glTexCoord2f(1, 1);
                glVertex2f(x0 + so + tw, y0 + so + th);
                glTexCoord2f(0, 1);
                glVertex2f(x0 + so, y0 + so + th);
                glEnd();

                glColor4f(1, 1, 1, 1);
                glBegin(GL_QUADS);
                glTexCoord2f(0, 0);
                glVertex2f(x0, y0);
                glTexCoord2f(1, 0);
                glVertex2f(x0 + tw, y0);
                glTexCoord2f(1, 1);
                glVertex2f(x0 + tw, y0 + th);
                glTexCoord2f(0, 1);
                glVertex2f(x0, y0 + th);
                glEnd();

                glDisable(GL_TEXTURE_2D);
            }

            glDisable(GL_BLEND);
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
        }

        // FPS / CPU 표시
        {
            if (g_show_fps) {
                QueryPerformanceCounter(&cur_time);
                float delta = (float) (cur_time.QuadPart - prev_time.QuadPart) / (float) freq.QuadPart;
                prev_time = cur_time;
                fps_accum += delta;
                fps_frame_count++;
                if (fps_accum >= 0.5f) {
                    fps_display = fps_frame_count / fps_accum;
                    fps_frame_count = 0;
                    fps_accum = 0.0f;

                    FILETIME ft_now, ft_kern, ft_user, ft_dummy;
                    GetSystemTimeAsFileTime(&ft_now);
                    GetProcessTimes(GetCurrentProcess(), &ft_dummy, &ft_dummy, &ft_kern, &ft_user);
                    ULARGE_INTEGER now, kern, user;
                    now.LowPart = ft_now.dwLowDateTime;
                    now.HighPart = ft_now.dwHighDateTime;
                    kern.LowPart = ft_kern.dwLowDateTime;
                    kern.HighPart = ft_kern.dwHighDateTime;
                    user.LowPart = ft_user.dwLowDateTime;
                    user.HighPart = ft_user.dwHighDateTime;
                    if (s_cpu_last_time.QuadPart != 0) {
                        ULONGLONG dt = now.QuadPart - s_cpu_last_time.QuadPart;
                        ULONGLONG used = (kern.QuadPart - s_cpu_last_kern.QuadPart)
                                         + (user.QuadPart - s_cpu_last_user.QuadPart);
                        if (dt > 0)
                            s_cpu_usage = (float) (used * 100.0 / ((double) dt * s_cpu_nproc));
                        if (s_cpu_usage < 0.0f) s_cpu_usage = 0.0f;
                        if (s_cpu_usage > 100.0f) s_cpu_usage = 100.0f;
                    }
                    s_cpu_last_time = now;
                    s_cpu_last_kern = kern;
                    s_cpu_last_user = user;
                }

                glViewport(0, 0, w, h);
                glMatrixMode(GL_PROJECTION);
                glPushMatrix();
                glLoadIdentity();
                glOrtho(0, w, h, 0, -1, 1);
                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();
                glLoadIdentity();
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glEnable(GL_POLYGON_SMOOTH);
                glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

                char fps_buf[32], cpu_buf[32];
                snprintf(fps_buf, sizeof(fps_buf), "%.1f fps", fps_display);
                snprintf(cpu_buf, sizeof(cpu_buf), "CPU %.1f%%", s_cpu_usage);

                float margin = 12.0f;
                float line_h = (float) g_font_height + 6.0f;
                float fy = (float) (g_fullscreen ? 20.0f : CAPTION_HEIGHT + 20.0f);
                float fx = (float) w - margin - gl_measure_string(fps_buf);

                glColor4f(0, 0, 0, 0.7f);
                gl_draw_string(fx + 1.0f, fy + 1.0f, fps_buf);
                glColor4f_255(OS_THEME_FG, 0.95f);
                gl_draw_string(fx, fy, fps_buf);

                float fy_cpu = fy + line_h;
                float fx_cpu = (float) w - margin - gl_measure_string(cpu_buf);
                glColor4f(0, 0, 0, 0.7f);
                gl_draw_string(fx_cpu + 1.0f, fy_cpu + 1.0f, cpu_buf);
                glColor4f_255(OS_THEME_FG, 0.95f);
                gl_draw_string(fx_cpu, fy_cpu, cpu_buf);

                glDisable(GL_POLYGON_SMOOTH);
                glDisable(GL_BLEND);
                glMatrixMode(GL_MODELVIEW);
                glPopMatrix();
                glMatrixMode(GL_PROJECTION);
                glPopMatrix();
            } else {
                QueryPerformanceCounter(&cur_time);
                float delta = (float) (cur_time.QuadPart - prev_time.QuadPart) / (float) freq.QuadPart;
                prev_time = cur_time;
                fps_accum += delta;
                fps_frame_count++;
                if (fps_accum >= 0.5f) {
                    fps_display = fps_frame_count / fps_accum;
                    fps_frame_count = 0;
                    fps_accum = 0.0f;
                }
            }
        }
        if (g_aspect_ratio <= 0.0)
            Sleep(16);
        SwapBuffers(g_hDC);
        DwmFlush();
        mpv_render_context_report_swap(mpv_gl);
    }

    wglMakeCurrent(NULL, NULL);
    return 0;
}

#include "context_menu.h"

// ──────────────────── WndProc ────────────────────
void MaximizeWindow(HWND hWnd) {
    say("Call MaximizedWindow");
    if (!g_is_playing) {
        if (g_is_maximized) {
            g_is_maximized = FALSE;
            SetWindowPos(hWnd, NULL,
                         g_restore_rect.left, g_restore_rect.top,
                         g_restore_rect.right - g_restore_rect.left,
                         g_restore_rect.bottom - g_restore_rect.top,
                         SWP_NOZORDER);
        } else {
            GetWindowRect(hWnd, &g_restore_rect);
            HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = {sizeof(mi)};
            GetMonitorInfo(hMon, &mi);
            SetWindowPos(hWnd, NULL, mi.rcWork.left, mi.rcWork.top,
                         mi.rcWork.right, mi.rcWork.bottom, SWP_NOZORDER);
            g_is_maximized = TRUE;
        }
    } else if (g_fitted) {
        SetWindowPos(hWnd, NULL,
                     g_restore_rect.left, g_restore_rect.top,
                     g_restore_rect.right - g_restore_rect.left,
                     g_restore_rect.bottom - g_restore_rect.top,
                     SWP_NOZORDER);
        g_fitted = FALSE;
        g_is_maximized = FALSE;
    } else {
        if (g_aspect_ratio <= 0.0) return;
        GetWindowRect(hWnd, &g_restore_rect);

        HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = {sizeof(mi)};
        GetMonitorInfo(hMon, &mi);
        int workW = mi.rcWork.right - mi.rcWork.left;
        int workH = mi.rcWork.bottom - mi.rcWork.top;

        int wW = workW;
        int wH = (int) (wW / g_aspect_ratio + 0.5);
        if (wH > workH) {
            wH = workH;
            wW = (int) (wH * g_aspect_ratio + 0.5);
        }

        int x = mi.rcWork.left + (workW - wW) / 2;
        int y = mi.rcWork.top + (workH - wH) / 2;
        SetWindowPos(hWnd, NULL, x, y, wW, wH, SWP_NOZORDER);
        g_fitted = TRUE;
        g_is_maximized = TRUE;
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PXVY_COMMAND) {
#define WM_PXVY_THEME 1
#define WM_PXVY_TEST  2
        switch (wParam) {
            case WM_PXVY_THEME: {
                CheckOSTheme();
                g_primary_color = pxvy_get_color();
                COLORREF border = RGB_THEME_PRIMRY;
                DwmSetWindowAttribute(hWnd, 34, &border, sizeof(border));
                break;
            }
            case WM_PXVY_TEST:
                MessageBoxA(hWnd, "Lelo", "Check", 0);
                break;
        }
        return 0;
    }

    switch (msg) {
        case WM_SETTINGCHANGE: {
            if (lParam && lstrcmpiW((LPCWSTR) lParam, L"ImmersiveColorSet") == 0) {
                CheckOSTheme();
                g_primary_color = pxvy_get_color();
                COLORREF border = RGB(g_primary_color.r, g_primary_color.g, g_primary_color.b);
                DwmSetWindowAttribute(hWnd, 34, &border, sizeof(border));
            }
            break;
        }
        case WM_DWMCOLORIZATIONCOLORCHANGED: {
            CheckOSTheme();
            g_primary_color = pxvy_get_color();
        }
        break;
        case WM_SYSCOMMAND: {
            if ((wParam & 0xFFF0) == SC_MAXIMIZE) return 0;
            break;
        }
        break;
        case WM_MOVING: {
            if (!g_is_maximized) break;
            RECT *r = (RECT *) lParam;
            int restoreW = g_restore_rect.right - g_restore_rect.left;
            int restoreH = g_restore_rect.bottom - g_restore_rect.top;
            POINT pt;
            GetCursorPos(&pt);
            int newX = pt.x - restoreW / 2;
            int newY = pt.y - CAPTION_HEIGHT / 2;
            r->left = newX;
            r->top = newY;
            r->right = newX + restoreW;
            r->bottom = newY + restoreH;
            SetWindowPos(hWnd, NULL, newX, newY, restoreW, restoreH,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            g_fitted = FALSE;
            g_is_maximized = FALSE;
            return TRUE;
        }
        break;
        case WM_SETCURSOR: {
            if (g_hide_element) return 0;
        }
        break;
        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = (MINMAXINFO *) lParam;
            mmi->ptMinTrackSize.x = 480;
            mmi->ptMinTrackSize.y = (g_video_dw > 0)
                                        ? (int) (480.0 * g_video_dh / g_video_dw + 0.5)
                                        : 480;
            return 0;
        }
        break;
        case WM_MPV_EVENT: {
            handle_mpv_events();
            return 0;
        }
        break;
        case WM_SIZE: {
            if (g_fullscreen) break;
            g_is_maximized = FALSE;
            if (g_aspect_ratio <= 0.0) break;
            int cw = LOWORD(lParam), ch = HIWORD(lParam);
            if (cw < 1 || ch < 1) break;
            PostMessage(hWnd, WM_FIT_RATIO, cw, ch);
            break;
        }
        break;
        case WM_FIT_RATIO: {
            if (g_fullscreen) break;
            if (g_aspect_ratio <= 0.0) break;
            int cw = (int) wParam, ch = (int) lParam;
            if (cw < 1 || ch < 1) break;

            int newCH = (int) (cw / g_aspect_ratio + 0.5);
            if (newCH < CAPTION_HEIGHT + 100) {
                newCH = CAPTION_HEIGHT + 100;
                cw = (int) (newCH * g_aspect_ratio + 0.5);
            }
            if (abs(newCH - ch) <= 2) break;

            RECT wr, cr;
            GetWindowRect(hWnd, &wr);
            GetClientRect(hWnd, &cr);
            int frameW = (wr.right - wr.left) - cr.right;
            int frameH = (wr.bottom - wr.top) - cr.bottom;
            int wW = cw + frameW, wH = newCH + frameH;

            HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = {sizeof(mi)};
            GetMonitorInfo(hMon, &mi);
            int workW = mi.rcWork.right - mi.rcWork.left;
            int workH = mi.rcWork.bottom - mi.rcWork.top;
            if (wW > workW || wH > workH) {
                double scale = fmin((double) workW / wW, (double) workH / wH);
                wW = (int) (wW * scale + 0.5);
                wH = (int) (wH * scale + 0.5);
            }

            int x = wr.left + ((wr.right - wr.left) - wW) / 2;
            int y = wr.top + ((wr.bottom - wr.top) - wH) / 2;
            if (x + wW > mi.rcWork.right) x = mi.rcWork.right - wW;
            if (x < mi.rcWork.left) x = mi.rcWork.left;
            if (y + wH > mi.rcWork.bottom) y = mi.rcWork.bottom - wH;
            if (y < mi.rcWork.top) y = mi.rcWork.top;

            SetWindowPos(hWnd, NULL, x, y, wW, wH, SWP_NOZORDER);
            int pref = 2;
            DwmSetWindowAttribute(hWnd, 33, &pref, sizeof(pref));
            return 0;
        }
        break;
        case WM_VIDEO_RESIZE: {
            if (g_fullscreen) break;
            if (g_aspect_ratio <= 0.0 || IsZoomed(hWnd)) break;
            g_hide_element = FALSE;

            HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = {sizeof(mi)};
            GetMonitorInfo(hMon, &mi);
            int workW = mi.rcWork.right - mi.rcWork.left;
            int workH = mi.rcWork.bottom - mi.rcWork.top;
            int wW = g_video_dw, wH = g_video_dh;

            if (wW > workW || wH > workH) {
                g_in_sizemove = TRUE;
                SetTimer(hWnd, IDT_OPEN_FILE, 1000, NULL);
                GetWindowRect(hWnd, &g_restore_rect);
                double scale = fmin((double) workW / wW, (double) workH / wH);
                wW = (int) (wW * scale + 0.5);
                wH = (int) (wH * scale + 0.5);
                g_is_maximized = TRUE;
                g_fitted = TRUE;
            } else {
                g_in_sizemove = FALSE;
                g_is_maximized = FALSE;
            }
            if (wH < CAPTION_HEIGHT + 100) {
                g_in_sizemove = TRUE;
                wH = CAPTION_HEIGHT + 100;
                wW = (int) (wH * g_aspect_ratio + 0.5);
            }

            int cx = mi.rcWork.left + (workW - wW) / 2;
            int cy = mi.rcWork.top + (workH - wH) / 2;
            SetWindowPos(hWnd, NULL, cx, cy, wW, wH, SWP_NOZORDER);

            if (g_pending_show) {
                g_pending_show = FALSE;
                ShowWindow(hWnd, g_pending_show_cmd);
                UpdateWindow(hWnd);
                COLORREF border = RGB(g_primary_color.r, g_primary_color.g, g_primary_color.b);
                DwmSetWindowAttribute(hWnd, 34, &border, sizeof(border));
            }
            GetWindowRect(hWnd, &g_restore_rect);
            return 0;
        }
        break;
        case WM_SIZING: {
            if (g_fullscreen) break;
            if (g_aspect_ratio <= 0.0) break;
            RECT *r = (RECT *) lParam;
            int w = r->right - r->left, h = r->bottom - r->top;
            switch (wParam) {
                case WMSZ_LEFT:
                case WMSZ_RIGHT: {
                    int cx = (r->left + r->right) / 2;
                    int new_h = (int) (w / g_aspect_ratio + 0.5);
                    int cy2 = (r->top + r->bottom) / 2;
                    r->left = cx - w / 2;
                    r->right = r->left + w;
                    r->top = cy2 - new_h / 2;
                    r->bottom = r->top + new_h;
                    break;
                }
                case WMSZ_TOP:
                case WMSZ_BOTTOM: {
                    int cy2 = (r->top + r->bottom) / 2;
                    int new_w = (int) (h * g_aspect_ratio + 0.5);
                    int cx = (r->left + r->right) / 2;
                    r->top = cy2 - h / 2;
                    r->bottom = r->top + h;
                    r->left = cx - new_w / 2;
                    r->right = r->left + new_w;
                    break;
                }
                case WMSZ_TOPLEFT:
                case WMSZ_TOPRIGHT:
                    r->top = r->bottom - (int) (w / g_aspect_ratio + 0.5);
                    break;
                case WMSZ_BOTTOMLEFT:
                case WMSZ_BOTTOMRIGHT:
                    r->bottom = r->top + (int) (w / g_aspect_ratio + 0.5);
                    break;
            }
            GetWindowRect(hWnd, &g_restore_rect);
            g_is_maximized = FALSE;
            return TRUE;
        }
        break;
        case WM_ENTERSIZEMOVE: {
            g_in_sizemove = TRUE;
            if (mpv) mpv_set_property_string(mpv, "video-sync", "audio");
            return 0;
        }
        break;
        case WM_EXITSIZEMOVE: {
            g_in_sizemove = FALSE;
            g_info_tex_vh = 0; // ← 추가: 캐시 무효화
            if (mpv) mpv_set_property_string(mpv, "video-sync", "display-resample");
            return 0;
        }
        break;
            break;
        case WM_NCCALCSIZE: {
            if (wParam == TRUE) {
                NCCALCSIZE_PARAMS *p = (NCCALCSIZE_PARAMS *) lParam;
                if (IsZoomed(hWnd)) {
                    int f = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                    p->rgrc[0].left += f;
                    p->rgrc[0].top += f;
                    p->rgrc[0].right -= f;
                    p->rgrc[0].bottom -= f;
                }
                return 0;
            }
        }
        break;
        case WM_NCHITTEST: {
            if (g_fullscreen) return HTCLIENT;
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hWnd, &pt);
            RECT rc;
            GetClientRect(hWnd, &rc);
            if (!IsZoomed(hWnd)) {
                if (pt.y < RESIZE_BORDER) {
                    if (pt.x < RESIZE_BORDER) return HTTOPLEFT;
                    if (pt.x > rc.right - RESIZE_BORDER) return HTTOPRIGHT;
                    return HTTOP;
                }
                if (pt.y > rc.bottom - RESIZE_BORDER) {
                    if (pt.x < RESIZE_BORDER) return HTBOTTOMLEFT;
                    if (pt.x > rc.right - RESIZE_BORDER) return HTBOTTOMRIGHT;
                    return HTBOTTOM;
                }
                if (pt.x < RESIZE_BORDER) return HTLEFT;
                if (pt.x > rc.right - RESIZE_BORDER) return HTRIGHT;
            }
            if (pt.y < CAPTION_HEIGHT && g_overlay_alpha > 0.3f) {
                int br = rc.right;
                if (pt.x > br - CAPTION_BTN_W) return HTCLOSE;
                if (pt.x > br - CAPTION_BTN_W * 2) return HTMAXBUTTON;
                if (pt.x > br - CAPTION_BTN_W * 3) return HTMINBUTTON;
                if (pt.x > br - CAPTION_BTN_W * 4) return HT_AOT_BUTTON;
                return HTCAPTION;
            }
            return HTCLIENT;
        }
        break;
        case WM_NCLBUTTONDBLCLK: {
            if (wParam == HTCAPTION) {
                MaximizeWindow(hWnd);
                return 0;
            }
        }
        break;
        case WM_NCLBUTTONDOWN: {
            switch (wParam) {
                case HTMINBUTTON: ShowWindow(hWnd, SW_MINIMIZE);
                    return 0;
                case HTMAXBUTTON: MaximizeWindow(hWnd);
                    return 0;
                case HTCLOSE: PostMessageA(hWnd, WM_CLOSE, 0, 0);
                    return 0;
                case HT_AOT_BUTTON:
                    g_always_on_top = !g_always_on_top;
                    SetWindowPos(hWnd, g_always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST,
                                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                    return 0;
            }
        }
        break;
        case WM_NCMOUSEMOVE: {
            g_mouse_in_window = TRUE;
            switch (wParam) {
                case HTCLOSE: g_hover_btn = CAPTION_HOVER_CLOSE;
                    break;
                case HTMAXBUTTON: g_hover_btn = CAPTION_HOVER_MAXIMIZE;
                    break;
                case HTMINBUTTON: g_hover_btn = CAPTION_HOVER_MINIMIZE;
                    break;
                case HT_AOT_BUTTON: g_hover_btn = CAPTION_HOVER_ALWAYSONTOP;
                    break;
                default: g_hover_btn = CAPTION_HOVER_NONE;
                    break;
            }
            if (g_mouse_idle) {
                g_mouse_idle = FALSE;
                g_hide_element = FALSE;
                SetCursor(LoadCursor(NULL, IDC_ARROW));
            }
            return 0;
        }
        break;
        case WM_NCMOUSELEAVE: {
            g_hover_btn = 0;
            return 0;
        }
        break;
        case WM_MOUSELEAVE: { return 0; }
        break;
        case WM_MOUSEMOVE: {
            g_hover_btn = 0;
            g_mouse_in_window = TRUE;
            g_hide_element = FALSE;
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            int mx = GET_X_LPARAM(lParam), my = GET_Y_LPARAM(lParam);
            RECT rc;
            GetClientRect(hWnd, &rc);

            if (g_seeking) {
                CtrlLayout L = ctrl_layout(rc.right, rc.bottom);
                double f = ((float) mx - L.seekX) / L.seekW;
                if (f < 0) f = 0;
                if (f > 1) f = 1;
                g_seek_frac = f;
                if (mpv && g_duration > 0) {
                    static double s_last_seek_frac = -1.0;
                    if (fabs(f - s_last_seek_frac) >= 0.005 || f <= 0.0 || f >= 1.0) {
                        s_last_seek_frac = f;
                        char buf[64];
                        if (f >= 1.0) {
                            snprintf(buf, sizeof(buf), "%.3f", g_duration);
                            const char *cmd[] = {"seek", buf, "absolute", NULL};
                            mpv_command_async(mpv, 0, cmd);
                            mpv_set_property_string(mpv, "pause", "yes");
                            g_eof_reached = 1;
                        } else {
                            snprintf(buf, sizeof(buf), "%.3f", f * g_duration);
                            const char *cmd[] = {"seek", buf, "absolute", NULL};
                            mpv_command_async(mpv, 0, cmd);
                            g_eof_reached = 0;
                        }
                    }
                }
                return 0;
            }
            if (g_vol_dragging) {
                CtrlLayout L = ctrl_layout(rc.right, rc.bottom);
                double f = ((float) mx - L.volX) / L.volW;
                if (f < 0) f = 0;
                if (f > 1) f = 1;
                g_vol_frac = f;
                if (mpv) {
                    double vol = f * 100.0;
                    mpv_set_property(mpv, "volume", MPV_FORMAT_DOUBLE, &vol);
                }
                return 0;
            }

            g_ctrl_hover = ctrl_hittest(rc.right, rc.bottom, mx, my);
            POINT pt;
            GetCursorPos(&pt);
            if (g_mouse_idle && (g_point_last.x != pt.x || g_point_last.y != pt.y)) {
                g_mouse_idle = FALSE;
                g_hide_element = FALSE;
                SetCursor(LoadCursor(NULL, IDC_ARROW));
            }
            g_point_last = pt;
            break;
        }
        break;
        case WM_TIMER: {
            if (wParam == IDT_MOUSE_CHECK) {
                RECT rect;
                POINT pt;
                GetWindowRect(hWnd, &rect);
                GetCursorPos(&pt);
                if (g_enter_fullscreen) {
                    g_mouse_idle = TRUE;
                    SetCursor(NULL);
                    g_hide_element = TRUE;
                    g_enter_fullscreen = FALSE;
                } else if (!PtInRect(&rect, pt)) {
                    g_mouse_in_window = FALSE;
                } else {
                    g_point_history[g_point_history_index] = pt;
                    g_point_history_index = (g_point_history_index + 1) % ASIZE_MOUSEIDLE;
                    BOOL is_idle = TRUE;
                    for (int i = 0; i < ASIZE_MOUSEIDLE - 1; i++) {
                        if (g_point_history[i].x == 0 || g_point_history[i].y == 0 ||
                            g_point_history[i].x != g_point_history[i + 1].x ||
                            g_point_history[i].y != g_point_history[i + 1].y) {
                            is_idle = FALSE;
                            break;
                        }
                    }
                    if (is_idle && !g_mouse_idle && !g_hide_element) {
                        g_mouse_idle = TRUE;
                        SetCursor(NULL);
                        g_hide_element = TRUE;
                    } else if (g_point_last.x != pt.x || g_point_last.y != pt.y) {
                        g_hide_element = FALSE;
                    }
                }
            }
            if (wParam == IDT_OPEN_FILE) {
                g_in_sizemove = FALSE;
                KillTimer(hWnd, IDT_OPEN_FILE);
            }
        }
        break;
        case WM_LBUTTONDOWN: {
            if (!mpv) break;
            int mx = GET_X_LPARAM(lParam), my = GET_Y_LPARAM(lParam);
            RECT rc;
            GetClientRect(hWnd, &rc);
            int hit = ctrl_hittest(rc.right, rc.bottom, mx, my);
            switch (hit) {
                case CTRL_PLAY: play_or_restart();
                    return 0;
                case CTRL_REW: {
                    const char *c[] = {"seek", "-10", NULL};
                    mpv_command_async(mpv, 0, c);
                    return 0;
                }
                case CTRL_FWD: {
                    const char *c[] = {"seek", "10", NULL};
                    mpv_command_async(mpv, 0, c);
                    return 0;
                }
                case CTRL_SEEK: {
                    CtrlLayout L = ctrl_layout(rc.right, rc.bottom);
                    double f = ((float) mx - L.seekX) / L.seekW;
                    if (f < 0) f = 0;
                    if (f > 1) f = 1;
                    g_seek_frac = f;
                    g_seeking = TRUE;
                    SetCapture(hWnd);
                    if (mpv && g_duration > 0) {
                        char buf[64];
                        if (f >= 1.0) {
                            snprintf(buf, sizeof(buf), "%.3f", g_duration);
                            const char *cmd[] = {"seek", buf, "absolute", NULL};
                            mpv_command_async(mpv, 0, cmd);
                            mpv_set_property_string(mpv, "pause", "yes");
                            g_eof_reached = 1;
                        } else {
                            snprintf(buf, sizeof(buf), "%.3f", f * g_duration);
                            const char *cmd[] = {"seek", buf, "absolute", NULL};
                            mpv_command_async(mpv, 0, cmd);
                            g_eof_reached = 0;
                        }
                    }
                    return 0;
                }
                case CTRL_VOL_SLIDER: {
                    CtrlLayout L = ctrl_layout(rc.right, rc.bottom);
                    double f = ((float) mx - L.volX) / L.volW;
                    if (f < 0) f = 0;
                    if (f > 1) f = 1;
                    g_vol_frac = f;
                    g_vol_dragging = TRUE;
                    SetCapture(hWnd);
                    double vol = f * 100.0;
                    mpv_set_property(mpv, "volume", MPV_FORMAT_DOUBLE, &vol);
                    return 0;
                }
                case CTRL_VOL_ICON: {
                    const char *cmd[] = {"cycle", "mute", NULL};
                    g_is_muted = !g_is_muted;
                    mpv_command_async(mpv, 0, cmd);
                    return 0;
                }
                case CTRL_SETTINGS:
                    MessageBoxA(hWnd,
                                "PXVY Settings\n\n"
                                "Keyboard Shortcuts:\n"
                                "  Space       Play / Pause\n"
                                "  Left/Right  Seek -5 / +5 sec\n"
                                "  Up/Down     Volume +5 / -5\n"
                                "  C           Speed +0.1x\n"
                                "  X           Speed -0.1x\n"
                                "  Enter       Toggle Fullscreen\n\n"
                                "Mouse:\n"
                                "  Scroll      Volume\n"
                                "  Right-click Context menu",
                                "Settings", MB_OK | MB_ICONINFORMATION);
                    return 0;
                case CTRL_REPEAT:
                    g_repeat_mode = (g_repeat_mode + 1) % 3;
                    apply_repeat_mode();
                    return 0;
                case CTRL_SUBTITLE:
                    g_sub_enabled = !g_sub_enabled;
                    mpv_set_property_string(mpv, "sub-visibility", g_sub_enabled ? "yes" : "no");
                    return 0;
                case CTRL_SPEED:
                    g_play_speed = 1.0f;
                    mpv_set_property_string(mpv, "speed", "1.0");
                    return 0;
                default: break;
            }
            break;
        }
        break;
        case WM_LBUTTONUP: {
            if (g_seeking) {
                g_seeking = FALSE;
                ReleaseCapture();
                return 0;
            }
            if (g_vol_dragging) {
                g_vol_dragging = FALSE;
                ReleaseCapture();
                return 0;
            }
        }
        break;
        case WM_ERASEBKGND: return TRUE;
        case WM_KEYDOWN: {
            if (!mpv) break;
            switch (wParam) {
                case VK_SPACE: play_or_restart();
                    break;
                case VK_RIGHT: {
                    const char *c[] = {"seek", "5", NULL};
                    mpv_command_async(mpv, 0, c);
                }
                break;
                case VK_LEFT: {
                    const char *c[] = {"seek", "-5", NULL};
                    mpv_command_async(mpv, 0, c);
                }
                break;
                case VK_UP: {
                    const char *c[] = {"add", "volume", "5", NULL};
                    mpv_command_async(mpv, 0, c);
                }
                break;
                case VK_DOWN: {
                    const char *c[] = {"add", "volume", "-5", NULL};
                    mpv_command_async(mpv, 0, c);
                }
                break;
                case VK_RETURN: toggle_fullscreen(hWnd);
                    break;
                case VK_ESCAPE:
                    if (g_fullscreen) toggle_fullscreen(hWnd);
                    else ShowWindow(hWnd, SW_MINIMIZE);
                    break;
                case 'C':
                    if (g_play_speed <= 5.0f - EPSILON) {
                        const char *cmd[] = {"add", "speed", "0.1", NULL};
                        mpv_command_async(mpv, 0, cmd);
                        g_play_speed += 0.1f;
                    }
                    break;
                case 'X':
                    if (g_play_speed >= 0.1f + EPSILON) {
                        const char *cmd[] = {"add", "speed", "-0.1", NULL};
                        mpv_command_async(mpv, 0, cmd);
                        g_play_speed -= 0.1f;
                    }
                    break;
                case 'F': {
                    if (g_ctrl_down) {
                        show_font_picker(hWnd,
                                         g_subtitle_font_family,
                                         on_font_pick, NULL);
                    } else {
                        g_show_fps = !g_show_fps;
                    }
                }
                break;
                case VK_CONTROL: g_ctrl_down = TRUE;
                    break;
                case VK_TAB: g_show_info = !g_show_info;
                    break;
                case 'H': g_hide_element = TRUE;
                    SetCursor(NULL);
                    break;
                case 'O':
                    if (g_ctrl_down) {
                        char p[MAX_PATH * 3];
                        if (open_file_dialog(hWnd, p, sizeof(p))) {
                            g_hide_element = TRUE;
                            load_file(p);
                            SetForegroundWindow(hWnd);
                        }
                    }
                    break;
                case 'Q': {
                    if (g_ctrl_down) PostMessageA(hWnd, WM_CLOSE, 0, 0);
                }break;
#ifdef _DEBUG
                case 'M': {
                    show_msgbox(hWnd, "알림", "저장되었습니다.", MB_OK, NULL, NULL);
                }break;
#endif

                default: break;
            }
            return 0;
        }
        break;
        case WM_KEYUP: {
            if (wParam == VK_CONTROL) {
                g_ctrl_down = FALSE;
            }
        }
        break;
        case WM_MOUSEWHEEL: {
            if (!mpv) break;
            g_mouse_idle = FALSE;
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (g_ctrl_down) {
                float vol = GetSystemVolume();
                if (vol >= 0.f) SetSystemVolume(vol + (delta > 0 ? 0.05f : -0.05f));
            } else {
                memset(g_point_history, 0, sizeof(g_point_history));
                if (delta > 0) {
                    const char *c[] = {"add", "volume", "5", NULL};
                    mpv_command_async(mpv, 0, c);
                } else {
                    const char *c[] = {"add", "volume", "-5", NULL};
                    mpv_command_async(mpv, 0, c);
                }
            }
            return 0;
        }
        break;
        case WM_RBUTTONUP: {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ClientToScreen(hWnd, &pt);
            show_context_menu(hWnd, pt.x, pt.y);
            return 0;
        }
        break;
        case WM_DROPFILES: {
            HDROP hd = (HDROP) wParam;
            wchar_t wp[MAX_PATH];
            char u8[MAX_PATH * 3];
            DragQueryFileW(hd, 0, wp, MAX_PATH);
            DragFinish(hd);
            wide_to_utf8(wp, u8, sizeof(u8));
            load_file(u8);
            pxvy_add_recent_video(u8);
            SetForegroundWindow(hWnd);
            return 0;
        }
        break;
        case WM_CREATE: {
            SetTimer(hWnd, IDT_MOUSE_CHECK, ATIME_MOUSECHECK, NULL);
#ifdef _DEBUG
            SetTimer(hWnd, IDT_DEBUG_INFO, 1000, NULL);
            say(BUILD_ENV_STRING);
            say(BUILD_TIME_KST);
#endif
            DragAcceptFiles(hWnd, TRUE);
            return 0;
        }
        break;
        case WM_SHOWWINDOW: {
            if (!g_pending_show) {
                COLORREF border = RGB(g_primary_color.r, g_primary_color.g, g_primary_color.b);
                DwmSetWindowAttribute(hWnd, 34, &border, sizeof(border));
            }
        }
        break;
        case WM_COPYDATA: {
            COPYDATASTRUCT *cds = (COPYDATASTRUCT *) lParam;
            if (cds && cds->dwData == PXVY_CMD_OPEN && cds->lpData) {
                load_file((const char *) cds->lpData);
                if (IsIconic(hWnd)) ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
            }
            return TRUE;
        }
        break;
        case WM_COMMAND: {
            UINT cmd = LOWORD(wParam);
            switch (cmd) {
                case ID_OPEN: {
                    char p[MAX_PATH * 3];
                    if (open_file_dialog(hWnd, p, sizeof(p))) {
                        g_hide_element = TRUE;
                        load_file(p);
                        pxvy_add_recent_video(p);
                        SetForegroundWindow(hWnd);
                    }
                    break;
                }
                case ID_PLAYLIST: break;
                case ID_TOOLS: break;
                case ID_SETTINGS: {
                    show_font_picker(hWnd,
                                     g_subtitle_font_family,
                                     on_font_pick, NULL);
                }
                break;
                case ID_EXIT: {
                    PostQuitMessage(0);
                }
                break;
                case ID_THEME_RED:
                case ID_THEME_ORANGE:
                case ID_THEME_YELLOW:
                case ID_THEME_GREEN:
                case ID_THEME_MINT:
                case ID_THEME_TEAL:
                case ID_THEME_CYAN:
                case ID_THEME_BLUE:
                case ID_THEME_INDIGO:
                case ID_THEME_PURPLE:
                case ID_THEME_BROWN:
                    apply_theme(hWnd, cmd);
                    break;
                case ID_RECENT_CLEAR:
                    pxvy_clear_recent_video();
                    break;
            }

            if (cmd >= ID_RECENT_VIDEO_BASE && cmd < ID_RECENT_VIDEO_BASE + 10) {
                int idx = cmd - ID_RECENT_VIDEO_BASE;
                if (idx < g_recent_count) {

                    // UTF-8 → Wide 변환 후 파일 존재 여부 확인
                    WCHAR wpath[MAX_PATH] = {0};
                    MultiByteToWideChar(CP_UTF8, 0,
                                        g_recent_video[idx], -1,
                                        wpath, MAX_PATH);

                    if (GetFileAttributesW(wpath) == INVALID_FILE_ATTRIBUTES) {
                        char msg[MAX_PATH + 64];
                        snprintf(msg, sizeof(msg),
                                 "File not found:\n%s", g_recent_video[idx]);
                        show_msgbox(hWnd, "Error", msg, MB_OK, NULL, NULL);

                        pxvy_remove_recent_video(g_recent_video[idx]);

                        // g_recent_video 메모리에서 제거 후 shift
                        for (int i = idx; i < g_recent_count - 1; i++)
                            memcpy(g_recent_video[i], g_recent_video[i + 1], MAX_PATH + 3);
                        memset(g_recent_video[--g_recent_count], 0, MAX_PATH + 3);
                        break;
                    }

                    load_file(g_recent_video[idx]);
                    pxvy_add_recent_video(g_recent_video[idx]);
                }
                break;
            }
            return 0;
        }
        break;
        case WM_DESTROY: {
            KillTimer(hWnd, IDT_MOUSE_CHECK);
#ifdef _DEBUG
            FreeConsole();
#endif
            if (g_bg_brush) {
                DeleteObject(g_bg_brush);
                g_bg_brush = NULL;
            }
            if (g_logo_tex) {
                glDeleteTextures(1, &g_logo_tex);
                g_logo_tex = 0;
            }
            if (g_splash_tex) {
                glDeleteTextures(1, &g_splash_tex);
                g_splash_tex = 0;
            }
            if (g_info_tex) {
                glDeleteTextures(1, &g_info_tex);
                g_info_tex = 0;
            }
            PostQuitMessage(0);
            return 0;
        }
        break;
        default: break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ──────────────────── WinMain ────────────────────

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
#ifdef _DEBUG
    if (AllocConsole() == TRUE) {
        SetConsoleOutputCP(CP_UTF8);
        FILE *nfp[3];
        freopen_s(nfp + 0, "CONOUT$", "rb", stdin);
        freopen_s(nfp + 1, "CONOUT$", "wb", stdout);
        freopen_s(nfp + 2, "CONOUT$", "wb", stderr);
    }
#endif
    HANDLE g_mutex = CreateMutexA(NULL, TRUE, "PXVY_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExist = FindWindowA("PXVY", NULL);
        if (hExist) {
            int argc;
            LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
            if (argv && argc >= 2) {
                char path_utf8[MAX_PATH * 3] = {0};
                wide_to_utf8(argv[1], path_utf8, sizeof(path_utf8));
                COPYDATASTRUCT cds = {0};
                cds.dwData = PXVY_CMD_OPEN;
                cds.cbData = (DWORD) (strlen(path_utf8) + 1);
                cds.lpData = path_utf8;
                SendMessageA(hExist, WM_COPYDATA, (WPARAM) NULL, (LPARAM) &cds);
            }
            if (argv) LocalFree(argv);
            if (IsIconic(hExist)) ShowWindow(hExist, SW_RESTORE);
            SetForegroundWindow(hExist);
        }
        CloseHandle(g_mutex);
        return 0;
    }

    static const wchar_t *title = L"PXVY";
    MSG msg;

    InitializeCriticalSection(&g_frame_cs);
    build_db_path();
    pxvy_db_init();
    CheckOSTheme();
    SetPrimaryColor();
    say("WINMAIN: %d, %d, %d", g_primary_color.r, g_primary_color.g, g_primary_color.b);

    // [변경] ① 폰트 리소스 로드 (init_font 전에 반드시 호출)
    load_fonts_from_resource(inst);

    WNDCLASSW wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hIcon = LoadIcon(inst, MAKEINTRESOURCE(IDI_MAIN_ICON));
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR) IDC_ARROW);
    g_bg_brush = CreateSolidBrush(RGB_THEME_BK);
    wc.hbrBackground = g_bg_brush;
    wc.lpszClassName = title;
    RegisterClassW(&wc);
    WM_PXVY_COMMAND = RegisterWindowMessageW(L"WM_PXVY_COMMAND");

    g_hWnd = CreateWindowW(title, title,
                           WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT, 960, 600,
                           NULL, NULL, inst, NULL);

    MARGINS margins = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(g_hWnd, &margins);

    COLORREF black = RGB(0, 0, 0);
    COLORREF border = RGB(g_primary_color.r, g_primary_color.g, g_primary_color.b);
    DwmSetWindowAttribute(g_hWnd, 34, &border, sizeof(border));
    DwmSetWindowAttribute(g_hWnd, 35, &black, sizeof(black));
    SetWindowPos(g_hWnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    if (!init_opengl(g_hWnd)) {
        MessageBoxA(NULL, "OpenGL init failed", "Error", MB_OK);
        return EXIT_FAILURE;
    }
    wglMakeCurrent(g_hDC, g_hRC);

    // [변경] ② Poppins가 등록된 상태에서 폰트 초기화
    init_font(g_hDC);
    init_mono_font(g_hDC);

    char exe_dir[MAX_PATH];
    GetModuleFileNameA(NULL, exe_dir, MAX_PATH);
    char *last_slash = strrchr(exe_dir, '\\');
    if (last_slash) *(last_slash + 1) = '\0';

    char symbol_path[MAX_PATH];
    snprintf(symbol_path, sizeof(symbol_path), "%ssymbol.png", exe_dir);
    load_logo_texture(symbol_path, 20, 20);

    if (!g_splash_tex) {
        int sw, sh, ch;
        char logo_path[MAX_PATH];
        snprintf(logo_path, sizeof(symbol_path), "%slogo.png", exe_dir);
        unsigned char *src = stbi_load(logo_path, &sw, &sh, &ch, 4);
        if (src) {
            unsigned char *dst = (unsigned char *) malloc(360 * 360 * 4);
            if (dst) {
                stbir_resize_uint8_linear(src, sw, sh, 0, dst, 360, 360, 0, STBIR_RGBA);
                glGenTextures(1, &g_splash_tex);
                glBindTexture(GL_TEXTURE_2D, g_splash_tex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 360, 360, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, dst);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glBindTexture(GL_TEXTURE_2D, 0);
                g_splash_w = 360;
                g_splash_h = 360;
                free(dst);
            }
            stbi_image_free(src);
        }
    }

    mpv = mpv_create();
    if (!mpv) {
        MessageBoxA(NULL, "mpv_create() failed", "Error", MB_OK);
        return 1;
    }

    mpv_set_option_string(mpv, "vo", "libmpv");
    mpv_set_option_string(mpv, "hwdec", "auto-safe");
    mpv_set_option_string(mpv, "keep-open", "yes");
    mpv_set_option_string(mpv, "osd-level", "0");
    mpv_set_option_string(mpv, "video-sync", "display-resample");
    mpv_set_option_string(mpv, "interpolation", "yes");
    mpv_set_option_string(mpv, "tscale", "sphinx");
    mpv_set_option_string(mpv, "tscale-radius", "3.0");
    mpv_set_option_string(mpv, "tscale-antiring", "0.7");
    mpv_set_option_string(mpv, "panscan", "0.0");
    mpv_set_option_string(mpv, "background-color", "#222529");

    {
        if (pxvy_get_subtitle_font(g_subtitle_font_family, LF_FACESIZE) != 0)
            strncpy_s(g_subtitle_font_family, LF_FACESIZE,
                      FONT_SUBTITLE_FAMILY, _TRUNCATE); // 실패 시 기본값
        mpv_set_option_string(mpv, "sub-font", g_subtitle_font_family);

        //mpv_set_option_string(mpv, "sub-font", FONT_SUBTITLE_FAMILY);
        mpv_set_option_string(mpv, "sub-font-size", "48");
        mpv_set_option_string(mpv, "sub-bold", "yes");
        mpv_set_option_string(mpv, "sub-border-size", "2.5");
        mpv_set_option_string(mpv, "sub-border-color", "#222529");
        mpv_set_option_string(mpv, "sub-color", "#FFFFFF");
        mpv_set_option_string(mpv, "sub-shadow-offset", "2");
        mpv_set_option_string(mpv, "sub-shadow-color", "#80000000");
    }
    {
        mpv_set_option_string(mpv, "vd-lavc-threads", "0");
        mpv_set_option_string(mpv, "demuxer-thread", "yes");
    }
    {
        mpv_set_option_string(mpv, "demuxer-max-bytes", "150MiB");
        mpv_set_option_string(mpv, "demuxer-max-back-bytes", "75MiB");
        mpv_set_option_string(mpv, "cache", "yes");
    }

    if (mpv_initialize(mpv) < 0) {
        MessageBoxA(NULL, "mpv_initialize() failed", "Error", MB_OK);
        return 1;
    }

    mpv_observe_property(mpv, 1, "video-params/dw", MPV_FORMAT_INT64);
    mpv_observe_property(mpv, 2, "video-params/dh", MPV_FORMAT_INT64);
    mpv_observe_property(mpv, 3, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 4, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 5, "eof-reached", MPV_FORMAT_FLAG);

    mpv_opengl_init_params gl_init = {0};
    gl_init.get_proc_address = get_proc_address;
    mpv_render_param rp[] = {
        {MPV_RENDER_PARAM_API_TYPE, (void *) MPV_RENDER_API_TYPE_OPENGL},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init},
        MPV_RENDER_PARAM_END()
    };
    if (mpv_render_context_create(&mpv_gl, mpv, rp) < 0) {
        MessageBoxA(NULL, "mpv_render_context_create() failed", "Error", MB_OK);
        return 1;
    }

    mpv_render_context_set_update_callback(mpv_gl, on_mpv_render_update, NULL);
    mpv_set_wakeup_callback(mpv, mpv_wakeup_cb, (void *) g_hWnd);

    wglMakeCurrent(NULL, NULL);
    g_render_event = CreateEventA(NULL, FALSE, FALSE, NULL);
    g_render_thread = CreateThread(NULL, 0, render_thread_func, NULL, 0, NULL);

    {
        int argc;
        LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv && argc >= 2) {
            char u[MAX_PATH * 3];
            wide_to_utf8(argv[1], u, sizeof(u));
            load_file(u);
            g_pending_show = TRUE;
            g_pending_show_cmd = show;
            SetForegroundWindow(g_hWnd);
        }
        if (argv) LocalFree(argv);
    }

    if (!g_pending_show) {
        ShowWindow(g_hWnd, show);
        UpdateWindow(g_hWnd);
    }

    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    g_running = FALSE;
    if (g_render_event) SetEvent(g_render_event);
    WaitForSingleObject(g_render_thread, 5000);
    CloseHandle(g_render_thread);
    if (g_render_event) CloseHandle(g_render_event);

    mpv_render_context_free(mpv_gl);
    mpv_terminate_destroy(mpv);
    mpv = NULL;
    cleanup_opengl();

    EnterCriticalSection(&g_frame_cs);
    free(g_frame_buf);
    g_frame_buf = NULL;
    LeaveCriticalSection(&g_frame_cs);
    DeleteCriticalSection(&g_frame_cs);

    // [변경] ③ 종료 전 폰트 리소스 해제
    unload_fonts_from_resource();

    return (int) msg.wParam;
}
