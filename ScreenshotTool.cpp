// ScreenshotTool.cpp
// x86_64-w64-mingw32-windres resource.rc -O coff -o resource.o
// x86_64-w64-mingw32-g++ -O2 -mwindows ScreenshotTool.cpp resource.o -lgdi32 -luser32 -lshell32 -static -o ScreenshotTool.exe
// x86_64-w64-mingw32-strip --strip-unneeded ScreenshotTool.exe

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <algorithm>

#define WM_TRAYICON   (WM_APP + 1)
#define TRAY_ICON_ID  1001

static const char OVERLAY_CLASS[] = "SC_Overlay";
static const char MAIN_CLASS[]    = "SC_Main";

static HWND  g_hwndOverlay = NULL;
static RECT  g_rectSelect  = {0};
static BOOL  g_bSelecting  = FALSE;
static POINT g_ptStart     = {0};

static BOOL CaptureRectToClipboard(const RECT& rect) {
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    if (w <= 0 || h <= 0) return FALSE;

    HDC hScreen = GetDC(NULL);
    if (!hScreen) return FALSE;
    HDC hMem = CreateCompatibleDC(hScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hScreen, w, h);
    HBITMAP hOld = (HBITMAP)SelectObject(hMem, hBmp);
    BOOL ok = BitBlt(hMem, 0, 0, w, h, hScreen, rect.left, rect.top, SRCCOPY);
    SelectObject(hMem, hOld);
    DeleteDC(hMem);
    ReleaseDC(NULL, hScreen);

    if (!ok) { DeleteObject(hBmp); return FALSE; }
    if (!OpenClipboard(NULL)) { DeleteObject(hBmp); return FALSE; }
    EmptyClipboard();
    HANDLE hData = SetClipboardData(CF_BITMAP, hBmp);
    CloseClipboard();
    if (!hData) { DeleteObject(hBmp); return FALSE; }
    return TRUE;
}

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            SetCursor(LoadCursor(NULL, IDC_CROSS));
            ZeroMemory(&g_rectSelect, sizeof(RECT));
            g_bSelecting = FALSE;
            g_hwndOverlay = hwnd;
            return 0;

        case WM_SETCURSOR:
            SetCursor(LoadCursor(NULL, IDC_CROSS));
            return TRUE;

        case WM_LBUTTONDOWN:
            g_bSelecting = TRUE;
            g_ptStart = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            g_rectSelect = { g_ptStart.x, g_ptStart.y, g_ptStart.x, g_ptStart.y };
            SetCapture(hwnd);
            return 0;

        case WM_MOUSEMOVE:
            if (g_bSelecting) {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                RECT oldRect = g_rectSelect;
                g_rectSelect = {
                    std::min(g_ptStart.x, pt.x), std::min(g_ptStart.y, pt.y),
                    std::max(g_ptStart.x, pt.x), std::max(g_ptStart.y, pt.y)
                };
                RECT r1 = oldRect; InflateRect(&r1, 2, 2);
                RECT r2 = g_rectSelect; InflateRect(&r2, 2, 2);
                InvalidateRect(hwnd, &r1, TRUE);
                InvalidateRect(hwnd, &r2, TRUE);
                UpdateWindow(hwnd);
            }
            return 0;

        case WM_LBUTTONUP:
            if (g_bSelecting) {
                ReleaseCapture();
                g_bSelecting = FALSE;
                ShowWindow(hwnd, SW_HIDE);
                GdiFlush();
                DestroyWindow(hwnd);
            }
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                g_bSelecting = FALSE;
                ZeroMemory(&g_rectSelect, sizeof(RECT));
                DestroyWindow(hwnd);
            }
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            HBRUSH hBg = CreateSolidBrush(RGB(80, 80, 80));
            FillRect(hdc, &ps.rcPaint, hBg);
            DeleteObject(hBg);
            if (g_rectSelect.right > g_rectSelect.left &&
                g_rectSelect.bottom > g_rectSelect.top) {
                HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 50, 50));
                HPEN hOld = (HPEN)SelectObject(hdc, hPen);
                SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, g_rectSelect.left, g_rectSelect.top,
                               g_rectSelect.right, g_rectSelect.bottom);
                SelectObject(hdc, hOld);
                DeleteObject(hPen);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            if (g_rectSelect.right > g_rectSelect.left &&
                g_rectSelect.bottom > g_rectSelect.top) {
                CaptureRectToClipboard(g_rectSelect);
            }
            g_hwndOverlay = NULL;
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void StartScreenshot() {
    if (g_hwndOverlay != NULL) return;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    HWND hwndOv = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST,
        OVERLAY_CLASS, "",
        WS_POPUP,
        0, 0, sw, sh,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );
    if (hwndOv) {
        SetLayeredWindowAttributes(hwndOv, 0, 120, LWA_ALPHA);
        ShowWindow(hwndOv, SW_SHOW);
        SetForegroundWindow(hwndOv);
        SetFocus(hwndOv);
    }
}

static void AddTrayIcon(HWND hwnd) {
    NOTIFYICONDATAA nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = TRAY_ICON_ID;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(1));
    if (!nid.hIcon) {
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    strncpy(nid.szTip, "Screenshot (Alt+A)", sizeof(nid.szTip) - 1);
    Shell_NotifyIconA(NIM_ADD, &nid);
}

static void RemoveTrayIcon(HWND hwnd) {
    NOTIFYICONDATAA nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = TRAY_ICON_ID;
    Shell_NotifyIconA(NIM_DELETE, &nid);
}

static void ShowTrayMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuA(hMenu, MF_STRING, 1, "Screenshot\tAlt+A");
    AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hMenu, MF_STRING, 2, "Exit");
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            if (!RegisterHotKey(hwnd, 1, MOD_ALT, 'A'))
                MessageBoxA(NULL, "Alt+A hotkey registration failed", "Warning", MB_OK);
            AddTrayIcon(hwnd);
            return 0;

        case WM_HOTKEY:
            if (wParam == 1) StartScreenshot();
            return 0;

        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) ShowTrayMenu(hwnd);
            else if (lParam == WM_LBUTTONUP) StartScreenshot();
            return 0;

        case WM_COMMAND:
            if (LOWORD(wParam) == 1) StartScreenshot();
            else if (LOWORD(wParam) == 2) DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            RemoveTrayIcon(hwnd);
            UnregisterHotKey(hwnd, 1);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    WNDCLASSEXA wcOv = {0};
    wcOv.cbSize        = sizeof(wcOv);
    wcOv.lpfnWndProc   = OverlayWndProc;
    wcOv.hInstance     = hInst;
    wcOv.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);
    wcOv.lpszClassName = OVERLAY_CLASS;
    if (!RegisterClassExA(&wcOv)) {
        MessageBoxA(NULL, "RegisterClass overlay failed", "Error", MB_OK);
        return 1;
    }

    WNDCLASSEXA wcMain = {0};
    wcMain.cbSize        = sizeof(wcMain);
    wcMain.lpfnWndProc   = MainWndProc;
    wcMain.hInstance     = hInst;
    wcMain.lpszClassName = MAIN_CLASS;
    if (!RegisterClassExA(&wcMain)) {
        MessageBoxA(NULL, "RegisterClass main failed", "Error", MB_OK);
        return 1;
    }

    HWND hwndMain = CreateWindowExA(0, MAIN_CLASS, "", WS_POPUP,
                                    0, 0, 0, 0, NULL, NULL, hInst, NULL);
    if (!hwndMain) {
        MessageBoxA(NULL, "CreateWindow main failed", "Error", MB_OK);
        return 1;
    }
    ShowWindow(hwndMain, SW_HIDE);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}
