#include <windows.h>
#include <dwmapi.h>
#include <d2d1.h>
#include <dwrite.h>
#include <string>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")

static ID2D1Factory* g_factory;
static ID2D1HwndRenderTarget* g_rt;
static IDWriteFactory* g_dwrite;
static IDWriteTextFormat* g_font;
static HWND g_hwnd;
static double g_time;

static void CreateDeviceResources(HWND hwnd)
{
    if (g_rt) return;
    RECT rc; GetClientRect(hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
    g_factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd, size),
        &g_rt);

static void Render()
{
    g_rt->BeginDraw();
    g_rt->Clear(D2D1::ColorF(0, 0, 0, 0));   // полностью прозрачный фон

    D2D1_SIZE_F sz = g_rt->GetSize();

    // Пример: мягкая пульсирующая виньетка по краям (чисто декоративная, ничего игровое не трогает)
    float pulse = 0.15f + 0.08f * (float)sin(g_time * 1.5);
    ID2D1RadialGradientStopCollection* stops = nullptr;
    D2D1_GRADIENT_STOP gs[2] = {
        { 0.0f, D2D1::ColorF(0, 0, 0, 0.0f) },
        { 1.0f, D2D1::ColorF(0.1f, 0.6f, 1.0f, pulse) }
    };
    g_rt->CreateGradientStopCollection(gs, 2, &stops);
    if (stops) {
        ID2D1RadialGradientBrush* brush = nullptr;
        D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES props = D2D1::RadialGradientBrushProperties(
            D2D1::Point2F(sz.width / 2, sz.height / 2), D2D1::Point2F(0, 0),
            sz.width * 0.75f, sz.height * 0.75f);
        g_rt->CreateRadialGradientBrush(props, stops, &brush);
        if (brush) {
            g_rt->FillRectangle(D2D1::RectF(0, 0, sz.width, sz.height), brush);
            brush->Release();
        }
        stops->Release();
    }

    // Пример: текст в углу (FPS-заглушка — просто показывает, что рендер живой)
    if (g_font) {
        ID2D1SolidColorBrush* textBrush = nullptr;
        g_rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 0.85f), &textBrush);
        wchar_t buf[64];
        swprintf_s(buf, L"Overlay active  t=%.1f", g_time);
        if (textBrush) {
            g_rt->DrawText(buf, (UINT32)wcslen(buf), g_font,
                D2D1::RectF(16, 16, sz.width - 16, 60), textBrush);
            textBrush->Release();
        }
    }

    g_rt->EndDraw();
}
// ---- конец блока с эффектами ----

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    switch (msg) {
    case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_SIZE:
        if (g_rt) g_rt->Resize(D2D1::SizeU(LOWORD(l), HIWORD(l)));
        return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

static void MakeClickThrough(HWND h)
{
    // Окно прозрачно для кликов: мышь и клавиатура проходят в игру под ним.
    SetWindowLongW(h, GWL_EXSTYLE,
        GetWindowLongW(h, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW);
    SetLayeredWindowAttributes(h, 0, 255, LWA_ALPHA);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int)
{
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"OverlayWndClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    g_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"OverlayWndClass", L"Overlay", WS_POPUP,
        0, 0, sw, sh, nullptr, nullptr, hInst, nullptr);
    MakeClickThrough(g_hwnd);

    // Включаем прозрачность через DWM (чтобы работало и в полноэкранном borderless-режиме)
    MARGINS m = { -1 };
    DwmExtendFrameIntoClientArea(g_hwnd, &m);

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_factory);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&g_dwrite);
    if (g_dwrite) g_dwrite->CreateTextFormat(L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        18.0f, L"", &g_font);
    CreateDeviceResources(g_hwnd);

    ShowWindow(g_hwnd, SW_SHOW);

    DWORD t0 = GetTickCount();
    MSG msg;
    bool running = true;
    while (running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        // Выход: Ctrl+Alt+F12
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_MENU) & 0x8000) && (GetAsyncKeyState(VK_F12) & 0x8000))
            running = false;

        g_time = (GetTickCount() - t0) / 1000.0;
        Render();
        Sleep(16);   // ~60 FPS
    }

    if (g_font) g_font->Release();
    if (g_dwrite) g_dwrite->Release();
    if (g_rt) g_rt->Release();
    if (g_factory) g_factory->Release();
    return 0;
}
