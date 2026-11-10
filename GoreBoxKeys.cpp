// GoreBoxKeys.cpp
// Отдельный прозрачный визуальный оверлей.
// Не инжектится в игру и не читает/изменяет её память.
//
// Клавиши:
// INSERT       — показать / скрыть оверлей
// ЛКМ          — RGB-кубики в месте нажатия мыши
// F8           — тестовый запуск RGB-кубиков в центре экрана
// F7           — включить / выключить постоянно парящие частицы
// CTRL+ALT+F12 — закрыть оверлей
//
// Сборка:
// g++ -shared -O2 -static -o GoreBoxKeys.dll GoreBoxKeys.cpp -lgdi32 -luser32

#include <windows.h>
#include <math.h>
#include <wchar.h>

static HWND g_hwnd = nullptr;
static volatile bool g_running = false;
static bool g_visible = false;

struct Particle {
    float x, y;
    float vx, vy;
    float life;
    float hue;
    int size;
};

static const int MAX_PARTICLES = 80;
static Particle particles[MAX_PARTICLES] = {};

// Небольшие декоративные частицы, постоянно парящие по экрану.
// Это 2D-эффект поверх игры, а не реальные объекты игрового мира.
struct AmbientParticle {
    float x, y;
    float vx, vy;
    float phase;
    float hue;
    float speed;
    int size;
};

static const int AMBIENT_COUNT = 58;
static AmbientParticle ambientParticles[AMBIENT_COUNT] = {};
static bool g_ambientEnabled = true;
static unsigned int g_randomSeed = 0xA341316Cu;

static unsigned int RandomNext()
{
    g_randomSeed = g_randomSeed * 1664525u + 1013904223u;
    return g_randomSeed;
}

static float RandomFloat(float minValue, float maxValue)
{
    const float unit = (RandomNext() & 0x00FFFFFFu) / 16777215.0f;
    return minValue + (maxValue - minValue) * unit;
}

static void InitAmbientParticles(int width, int height)
{
    for (int i = 0; i < AMBIENT_COUNT; ++i) {
        AmbientParticle& p = ambientParticles[i];
        p.x = RandomFloat(0.0f, (float)width);
        p.y = RandomFloat(0.0f, (float)height);
        p.vx = RandomFloat(-0.32f, 0.32f);
        p.vy = RandomFloat(-0.65f, -0.12f);
        p.phase = RandomFloat(0.0f, 6.28318f);
        p.hue = RandomFloat(0.48f, 0.82f);
        p.speed = RandomFloat(0.65f, 1.45f);
        p.size = (int)RandomFloat(2.0f, 5.0f);
    }
}

static COLORREF DimColor(COLORREF color, float amount)
{
    int r = (int)(GetRValue(color) * amount);
    int g = (int)(GetGValue(color) * amount);
    int b = (int)(GetBValue(color) * amount);
    return RGB(r, g, b);
}

static COLORREF HueColor(float h)
{
    h -= floorf(h);
    float r = 0.0f, g = 0.0f, b = 0.0f;
    int sector = (int)(h * 6.0f);
    float f = h * 6.0f - sector;
    float q = 1.0f - f;

    switch (sector % 6) {
    case 0: r = 1; g = f; b = 0; break;
    case 1: r = q; g = 1; b = 0; break;
    case 2: r = 0; g = 1; b = f; break;
    case 3: r = 0; g = q; b = 1; break;
    case 4: r = f; g = 0; b = 1; break;
    case 5: r = 1; g = 0; b = q; break;
    }

    return RGB((int)(r * 255), (int)(g * 255), (int)(b * 255));
}

static void DrawRect(HDC dc, HBRUSH brush,
                     int x, int y, int width, int height)
{
    RECT r = { x, y, x + width, y + height };
    FillRect(dc, &r, brush);
}

static void SpawnParticles(int cx, int cy)
{
    static unsigned int seed = 1234567u;

    for (int i = 0; i < 24; ++i) {
        int index = -1;

        for (int j = 0; j < MAX_PARTICLES; ++j) {
            if (particles[j].life <= 0.0f) {
                index = j;
                break;
            }
        }

        if (index < 0)
            break;

        seed = seed * 1664525u + 1013904223u;
        float angle = (seed % 6283u) / 1000.0f;

        seed = seed * 1664525u + 1013904223u;
        float speed = 2.0f + (seed % 400u) / 100.0f;

        Particle& p = particles[index];
        p.x = (float)cx;
        p.y = (float)cy;
        p.vx = cosf(angle) * speed;
        p.vy = sinf(angle) * speed;
        p.life = 1.0f;
        p.hue = (seed % 1000u) / 1000.0f;
        p.size = 4 + (int)(seed % 5u);
    }
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg,
                                WPARAM w, LPARAM l)
{
    if (msg == WM_DESTROY) {
        g_running = false;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(h, msg, w, l);
}

static DWORD WINAPI OverlayThread(LPVOID)
{
    HINSTANCE hInst = GetModuleHandleW(nullptr);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"GoreBoxOverlayWnd";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);
    InitAmbientParticles(screenW, screenH);

    g_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT |
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"GoreBoxOverlayWnd",
        L"GoreBox Visuals",
        WS_POPUP,
        0, 0, screenW, screenH,
        nullptr, nullptr, hInst, nullptr
    );

    if (!g_hwnd)
        return 0;

    // Цвет фона становится прозрачным.
    SetLayeredWindowAttributes(
        g_hwnd, GetSysColor(COLOR_WINDOW),
        255, LWA_COLORKEY
    );

    // По умолчанию оверлей скрыт до нажатия INSERT.
    ShowWindow(g_hwnd, SW_HIDE);

    HBRUSH backgroundBrush = GetSysColorBrush(COLOR_WINDOW);
    HBRUSH drawingBrush = (HBRUSH)GetStockObject(DC_BRUSH);

    const DWORD startTime = GetTickCount();
    DWORD previousTime = startTime;

    g_running = true;
    wchar_t textBuffer[96];

    while (g_running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (!g_running)
            break;

        // INSERT переключает видимость оверлея.
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            g_visible = !g_visible;
            ShowWindow(g_hwnd, g_visible ? SW_SHOW : SW_HIDE);
        }

        // Горячая клавиша полного выхода.
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
            (GetAsyncKeyState(VK_MENU) & 0x8000) &&
            (GetAsyncKeyState(VK_F12) & 0x8000)) {
            g_running = false;
            break;
        }

        // ЛКМ: частицы появляются в месте нажатия мыши.
        // Оверлей прозрачен для кликов, поэтому нажатие проходит в игру.
        if (g_visible && (GetAsyncKeyState(VK_LBUTTON) & 1)) {
            POINT cursorPos;
            if (GetCursorPos(&cursorPos)) {
                SpawnParticles(cursorPos.x, cursorPos.y);
            }
        }

        // F8 — тестовый запуск кубиков в центре экрана.
        if (g_visible && (GetAsyncKeyState(VK_F8) & 1)) {
            SpawnParticles(screenW / 2, screenH / 2);
        }

        // F7 — включить / выключить постоянные парящие частицы.
        if (g_visible && (GetAsyncKeyState(VK_F7) & 1)) {
            g_ambientEnabled = !g_ambientEnabled;
        }

        DWORD now = GetTickCount();
        float dt = (now - previousTime) / 1000.0f;
        previousTime = now;
        if (dt > 0.05f)
            dt = 0.05f;

        if (!g_visible) {
            Sleep(16);
            continue;
        }

        const float elapsed = (now - startTime) / 1000.0f;
        const float hue = fmodf(elapsed * 0.18f, 1.0f);

        HDC dc = GetDC(g_hwnd);
        if (dc) {
            RECT clientRect;
            GetClientRect(g_hwnd, &clientRect);
            FillRect(dc, &clientRect, backgroundBrush);

            // DC_BRUSH / DC_PEN позволяют менять цвета без создания
            // множества GDI-объектов каждый кадр.
            HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(DC_BRUSH));
            HGDIOBJ oldPen = SelectObject(dc, GetStockObject(DC_PEN));
            SetBkMode(dc, TRANSPARENT);

            // Постоянные парящие частицы — медленно дрейфуют по всему экрану.
            // Для глубины используются разные размеры и скорости.
            int ambientAlive = 0;
            if (g_ambientEnabled) {
                for (int i = 0; i < AMBIENT_COUNT; ++i) {
                    AmbientParticle& p = ambientParticles[i];

                    p.phase += dt * 1.15f;
                    p.x += (p.vx + sinf(p.phase) * 0.10f) *
                           p.speed * dt * 60.0f;
                    p.y += p.vy * p.speed * dt * 60.0f;

                    // Перезапуск частицы с противоположной стороны экрана.
                    if (p.y < -12.0f || p.x < -12.0f || p.x > screenW + 12.0f) {
                        p.x = RandomFloat(0.0f, (float)screenW);
                        p.y = (float)screenH + RandomFloat(2.0f, 80.0f);
                        p.vx = RandomFloat(-0.32f, 0.32f);
                        p.vy = RandomFloat(-0.65f, -0.12f);
                        p.hue = RandomFloat(0.48f, 0.82f);
                        p.speed = RandomFloat(0.65f, 1.45f);
                        p.size = (int)RandomFloat(2.0f, 5.0f);
                    }

                    ++ambientAlive;

                    const int x = (int)p.x;
                    const int y = (int)p.y;
                    const float twinkle = 0.68f + 0.32f * sinf(p.phase * 1.7f);
                    const COLORREF tint = HueColor(p.hue + elapsed * 0.025f);
                    const COLORREF glow = DimColor(tint, 0.34f + twinkle * 0.18f);
                    const COLORREF core = DimColor(tint, 0.72f + twinkle * 0.28f);
                    const int size = p.size;

                    // Три уровня свечения и яркая точка в центре.
                    SetDCBrushColor(dc, glow);
                    SetDCPenColor(dc, glow);
                    Ellipse(dc, x - size - 3, y - size - 3,
                            x + size + 3, y + size + 3);

                    SetDCBrushColor(dc, core);
                    SetDCPenColor(dc, core);
                    Ellipse(dc, x - size, y - size,
                            x + size, y + size);

                    SetDCBrushColor(dc, RGB(235, 249, 255));
                    SetDCPenColor(dc, RGB(235, 249, 255));
                    Ellipse(dc, x - 1, y - 1, x + 2, y + 2);

                    // Редкие маленькие крестики-искры.
                    if (i % 9 == 0 && twinkle > 0.88f) {
                        SetDCBrushColor(dc, RGB(225, 245, 255));
                        DrawRect(dc, drawingBrush, x - 4, y, 9, 1);
                        DrawRect(dc, drawingBrush, x, y - 4, 1, 9);
                    }
                }
            }

            // Компактный неоновый прицел: тонкий белый центр + RGB-кант.
            const int cx = screenW / 2;
            const int cy = screenH / 2;
            const int arm = 9;
            const int gap = 5;

            SetDCBrushColor(dc, HueColor(hue));
            SetDCPenColor(dc, HueColor(hue));
            DrawRect(dc, drawingBrush, cx - arm - 1, cy - 2,
                     arm - gap + 1, 4);
            DrawRect(dc, drawingBrush, cx + gap, cy - 2,
                     arm - gap + 1, 4);
            DrawRect(dc, drawingBrush, cx - 2, cy - arm - 1,
                     4, arm - gap + 1);
            DrawRect(dc, drawingBrush, cx - 2, cy + gap,
                     4, arm - gap + 1);

            SetDCBrushColor(dc, RGB(245, 250, 255));
            SetDCPenColor(dc, RGB(245, 250, 255));
            DrawRect(dc, drawingBrush, cx - arm, cy - 1,
                     arm - gap, 2);
            DrawRect(dc, drawingBrush, cx + gap, cy - 1,
                     arm - gap, 2);
            DrawRect(dc, drawingBrush, cx - 1, cy - arm,
                     2, arm - gap);
            DrawRect(dc, drawingBrush, cx - 1, cy + gap,
                     2, arm - gap);

            // RGB-кубики. Несколько слоёв создают более выразительный
            // неоновый вид без дополнительных графических библиотек.
            int aliveParticles = 0;
            for (int i = 0; i < MAX_PARTICLES; ++i) {
                Particle& p = particles[i];
                if (p.life <= 0.0f)
                    continue;

                p.x += p.vx * dt * 60.0f;
                p.y += p.vy * dt * 60.0f;
                p.vy += 0.10f * dt * 60.0f;
                p.life -= dt * 1.5f;

                if (p.life <= 0.0f)
                    continue;

                ++aliveParticles;
                const COLORREF cubeColor =
                    HueColor(p.hue + elapsed * 0.22f);
                const int x = (int)p.x;
                const int y = (int)p.y;
                const int s = p.size;

                SetDCBrushColor(dc, RGB(30, 35, 55));
                SetDCPenColor(dc, cubeColor);
                DrawRect(dc, drawingBrush, x - 2, y - 2, s + 4, s + 4);

                SetDCBrushColor(dc, cubeColor);
                SetDCPenColor(dc, cubeColor);
                DrawRect(dc, drawingBrush, x, y, s, s);

                SetDCBrushColor(dc, RGB(245, 250, 255));
                SetDCPenColor(dc, RGB(245, 250, 255));
                DrawRect(dc, drawingBrush, x + 1, y + 1,
                         (s > 3 ? 2 : 1), (s > 3 ? 2 : 1));
            }

            // Стильная нижняя HUD-плашка в духе современных visual-клиентов.
            const int panelX = 22;
            const int panelY = screenH - 91;
            const int panelW = 278;
            const int panelH = 66;

            // Тень и тёмная стеклянная карточка.
            SetDCBrushColor(dc, RGB(8, 10, 18));
            SetDCPenColor(dc, RGB(8, 10, 18));
            RoundRect(dc, panelX + 3, panelY + 4,
                      panelX + panelW + 3, panelY + panelH + 4, 14, 14);

            SetDCBrushColor(dc, RGB(17, 21, 34));
            SetDCPenColor(dc, RGB(48, 57, 78));
            RoundRect(dc, panelX, panelY,
                      panelX + panelW, panelY + panelH, 14, 14);

            // Неоновая акцентная полоса слева.
            SetDCBrushColor(dc, HueColor(hue));
            SetDCPenColor(dc, HueColor(hue));
            RoundRect(dc, panelX + 8, panelY + 10,
                      panelX + 12, panelY + panelH - 10, 4, 4);

            // Заголовок и состояние.
            SetTextColor(dc, RGB(245, 248, 255));
            RECT titleRect = {
                panelX + 23, panelY + 9,
                panelX + panelW - 12, panelY + 31
            };
            DrawTextW(dc, L"GOREBOX  /  VISUALS", -1, &titleRect,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            // Цветная точка состояния.
            SetDCBrushColor(dc, RGB(70, 235, 170));
            SetDCPenColor(dc, RGB(70, 235, 170));
            Ellipse(dc, panelX + 23, panelY + 41,
                    panelX + 31, panelY + 49);

            SetTextColor(dc, RGB(170, 185, 208));
            RECT infoRect = {
                panelX + 38, panelY + 36,
                panelX + panelW - 10, panelY + 55
            };
            swprintf(textBuffer, 96, L"FLOAT %d  |  CLICK %d  |  F7",
                     ambientAlive, aliveParticles);
            DrawTextW(dc, textBuffer, -1, &infoRect,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            // Маленькая цветная метка в правом верхнем углу карточки.
            SetTextColor(dc, HueColor(hue));
            RECT tagRect = {
                panelX + panelW - 68, panelY + 10,
                panelX + panelW - 12, panelY + 28
            };
            DrawTextW(dc, L"ACTIVE", -1, &tagRect,
                      DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            ReleaseDC(g_hwnd, dc);
        }
        Sleep(16);
    }

    if (g_hwnd && IsWindow(g_hwnd))
        DestroyWindow(g_hwnd);

    g_hwnd = nullptr;
    return 0;
}

// Примечание: запуск потока из DllMain оставлен для совместимости
// с исходным вариантом проекта.
BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(GetModuleHandleW(nullptr));

        HANDLE thread = CreateThread(
            nullptr, 0, OverlayThread, nullptr, 0, nullptr
        );

        if (thread)
            CloseHandle(thread);
    }

    return TRUE;
from pathlib import Path
import zipfile

folder = Path("/mnt/data/SpawnProbe")
folder.mkdir(parents=True, exist_ok=True)

hook_text = """// SpawnProbe — тест событий создания объектов для GoreBox 15.16.5.
// Проверяет, передаёт ли старая система модов имена новых объектов.
//
// ВАЖНО: это диагностический мод, не ESP.
// Он только выводит сведения о событии создания объекта в консоль.

ListenForInstantiation

OnInstantiate = function(args)
    print "=== NEW OBJECT CREATED ==="
    print "Spawner: " + args.player
    print "Object: " + args.name
end function

// Обязательный цикл обработки событий.
while true
    while _events.len > 0
        _nextEvent = _events.pull
        _nextEvent.invoke(_nextEvent.args)
    end while
    yield
end while
"""

info_text = """name:SpawnProbe
description:Logs object creation events for API testing.
version:1.0
active:true
"""

readme_text = """SpawnProbe — тестовый MiniScript-мод для GoreBox 15.x

Файлы:
- hook.txt — обработчик ListenForInstantiation
- info.txt — сведения о моде

Что делает:
При событии создания объекта выводит в консоль:
- args.player — имя создателя события
- args.name — имя созданного объекта

Это не ESP и не рисует контуры. Тест нужен, чтобы проверить,
какую информацию старая система модов передаёт при создании объектов.

Установка:
Скопируй папку SpawnProbe целиком в папку Mods игры.
Затем включи мод в игре и открой консоль модов.
Создай/заспавни тестовый объект и посмотри, появляются ли строки
NEW OBJECT CREATED, Spawner и Object.

Если мод выдаст ошибку, пришли точный текст ошибки и версию игры.
"""

(folder / "hook.txt").write_text(hook_text, encoding="utf-8")
(folder / "info.txt").write_text(info_text, encoding="utf-8")
(folder / "README.txt").write_text(readme_text, encoding="utf-8")

zip_path = Path("/mnt/data/SpawnProbe_GoreBox15.zip")
with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as archive:
    for file_path in sorted(folder.iterdir()):
        archive.write(file_path, f"SpawnProbe/{file_path.name}")

print(f"Создан архив: {zip_path}")
print(f"Состав: {', '.join(p.name for p in sorted(folder.iterdir()))}")
}
