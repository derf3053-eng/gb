// GoreBoxKeys.cpp — прозрачный оверлей поверх экрана в виде DLL (GoreBoxKeys.dll).
// НЕ инжектится в игру и не трогает её память: отдельное окно без рамки, всегда поверх,
// клики проходят сквозь него в игру.
// Рисует: пульсирующая рамка по краям экрана + прицел в центре + счётчик времени.
// Выход из оверлея: Ctrl+Alt+F12.
//
// ВАЖНО: workflow собирает ТОЛЬКО с -luser32, поэтому здесь используются
// исключительно функции user32/kernel32 (никаких GDI/D2D/DWrite/DwmApi).
//
// Сборка (как в GitHub Actions):
//   g++ -shared -O2 -static -o GoreBoxKeys.dll GoreBoxKeys.cpp -luser32

#include <windows.h>
#include <wchar.h>

static HWND g_hwnd;
static bool g_running;

// ---- Обработчик окна ----
static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    if (msg == WM_DESTROY) {
        g_running = false;
        return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

// ---- Поток оверлея ----
static DWORD WINAPI OverlayThread(LPVOID)
{
    HINSTANCE hInst = GetModuleHandleW(nullptr);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"GoreBoxOverlayWnd";
    // фон = системный цвет окна: он же становится цветовым ключом прозрачности
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    g_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"GoreBoxOverlayWnd", L"GoreBoxOverlay", WS_POPUP,
        0, 0, sw, sh, nullptr, nullptr, hInst, nullptr);
    if (!g_hwnd) return 0;

    // Делаем цвет окна (обычно белый) полностью прозрачным; всё остальное — видимо
    SetLayeredWindowAttributes(g_hwnd, GetSysColor(COLOR_WINDOW), 255, LWA_COLORKEY);
    ShowWindow(g_hwnd, SW_SHOW);

    const COLORREF keyColor = GetSysColor(COLOR_WINDOW);
    HBRUSH brushBg     = GetSysColorBrush(COLOR_WINDOW);      // фон (прозрачный)
    HBRUSH brushDark   = GetSysColorBrush(COLOR_WINDOWTEXT);    // чёрная рамка / прицел
    HBRUSH brushGray   = GetSysColorBrush(COLOR_GRAYTEXT);      // серая рамка (фаза пульса)

    DWORD t0 = GetTickCount();
    g_running = true;
    wchar_t buf[64];
    RECT rc;

    while (g_running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) g_running = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        // Выход: Ctrl+Alt+F12
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
            (GetAsyncKeyState(VK_MENU) & 0x8000) &&
            (GetAsyncKeyState(VK_F12) & 0x8000))
            g_running = false;

        double t = (GetTickCount() - t0) / 1000.0;
        // треугольная "пульсация" 0..1 без математической библиотеки
        double phase = t * 1.2;
        phase -= (int)phase;
        double pulse = phase < 0.5 ? phase * 2.0 : (1.0 - phase) * 2.0;

        HDC dc = GetDC(g_hwnd);
        if (dc) {
            GetClientRect(g_hwnd, &rc);

            // 1) заливаем фон ключевым цветом — он прозрачный
            FillRect(dc, &rc, brushBg);

            // 2) пульсирующая рамка по краям: чёрная <-> серая
            HBRUSH frameBrush = (pulse < 0.5) ? brushDark : brushGray;
            int th = 6 + (int)(pulse * 6.0);   // толщина тоже "дышит"
            RECT r;
            r.left = 0;        r.top = 0;           r.right = sw;       r.bottom = th;      FillRect(dc, &r, frameBrush);
            r.left = 0;        r.top = sh - th;    r.right = sw;       r.bottom = sh;      FillRect(dc, &r, frameBrush);
            r.left = 0;        r.top = 0;           r.right = th;       r.bottom = sh;      FillRect(dc, &r, frameBrush);
            r.left = sw - th;  r.top = 0;           r.right = sw;       r.bottom = sh;      FillRect(dc, &r, frameBrush);

            // 3) прицел в центре экрана (чёрный, видимый поверх игры)
            int cx = sw / 2, cy = sh / 2, arm = 18, gap = 5, w = 2;
            r.left = cx - arm; r.top = cy - w / 2; r.right = cx - gap;  r.bottom = cy + w / 2; FillRect(dc, &r, brushDark);
            r.left = cx + gap; r.top = cy - w / 2; r.right = cx + arm;  r.bottom = cy + w / 2; FillRect(dc, &r, brushDark);
            r.left = cx - w / 2; r.top = cy - arm; r.right = cx + w / 2; r.bottom = cy - gap; FillRect(dc, &r, brushDark);
            r.left = cx - w / 2; r.top = cy + gap; r.right = cx + w / 2; r.bottom = cy + arm; FillRect(dc, &r, brushDark);

            // 4) текст в углу (чёрный текст на белом фоне; белый = прозрачный ключ)
            swprintf(buf, 64, L"GoreBox overlay  t=%.1f", t);
            RECT tr = { 16, 16, sw - 16, 48 };
            DrawTextW(dc, buf, -1, &tr, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

            ReleaseDC(g_hwnd, dc);
        }
        Sleep(16);   // ~60 FPS
    }

    DestroyWindow(g_hwnd);
    return 0;
}

// ---- Точка входа DLL: при загрузке запускаем поток оверлея ----
BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        HANDLE th = CreateThread(nullptr, 0, OverlayThread, nullptr, 0, nullptr);
        if (th) CloseHandle(th);
    }
    return TRUE;
}
