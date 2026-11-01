// GoreBoxKeys: инжектируемая DLL без загрузчика и без RVA.
// Вызывает встроенные режимы игры (GameMaster.ToggleXxx) через il2cpp API по именам.
// Клавиши: F1 God, F2 Infinite Ammo, F3 Stamina, F4 NoClip, F5 Ignore AI, F6 SlowMo, F7 Hide UI, F8 Cash
//
// Сборка (x64!):
//   MSVC  : cl /LD /O2 /EHsc GoreBoxKeys.cpp user32.lib /Fe:GoreBoxKeys.dll
//   MinGW : x86_64-w64-mingw32-g++ -shared -O2 -static -o GoreBoxKeys.dll GoreBoxKeys.cpp -luser32
#include <windows.h>
#include <string.h>
#include <stdio.h>

static void LogN(const char* fmt, long n);
static void Log(const char* fmt, const char* a = "")
{
    char path[MAX_PATH]; GetTempPathA(MAX_PATH, path); strcat(path, "GoreBoxKeys.log");
    FILE* f = fopen(path, "a"); if (!f) return;
    fprintf(f, fmt, a); fprintf(f, "\n"); fclose(f);
}

static void LogN(const char* fmt, long n)
{
    char path[MAX_PATH]; GetTempPathA(MAX_PATH, path); strcat(path, "GoreBoxKeys.log");
    FILE* f = fopen(path, "a"); if (!f) return;
    fprintf(f, fmt, n); fprintf(f, "\n"); fclose(f);
}

typedef void* (*f_domain_get)();
typedef void** (*f_domain_get_assemblies)(void* domain, size_t* count);
typedef void* (*f_assembly_get_image)(void* assembly);
typedef const char* (*f_image_get_name)(void* image);
typedef void* (*f_class_from_name)(void* image, const char* ns, const char* name);
typedef void* (*f_class_get_method)(void* klass, const char* name, int argc);
typedef void* (*f_class_get_type)(void* klass);
typedef void* (*f_type_get_object)(void* type);
typedef void* (*f_runtime_invoke)(void* method, void* obj, void** params, void** exc);

static f_domain_get p_domain_get; static f_domain_get_assemblies p_assemblies;
static f_assembly_get_image p_image; static f_image_get_name p_image_name;
static f_class_from_name p_class; static f_class_get_method p_method;
static f_class_get_type p_class_type; static f_type_get_object p_type_obj;
static f_runtime_invoke p_invoke;

static WNDPROC g_orig; static HWND g_wnd;
static void* g_gmClass; static void* g_findMethod; static void* g_objClass;

static bool Resolve()
{
    if (p_invoke) return g_gmClass != nullptr;
    HMODULE m = GetModuleHandleA("GameAssembly.dll");
    if (!m) { Log("[resolve] GameAssembly.dll not found"); return false; }
#define G(v, n) v = (decltype(v))GetProcAddress(m, n); if (!v) { Log("[resolve] export missing: %s", n); return false; }
    G(p_domain_get, "il2cpp_domain_get") G(p_assemblies, "il2cpp_domain_get_assemblies")
    G(p_image, "il2cpp_assembly_get_image") G(p_image_name, "il2cpp_image_get_name")
    G(p_class, "il2cpp_class_from_name") G(p_method, "il2cpp_class_get_method_from_name")
    G(p_class_type, "il2cpp_class_get_type") G(p_type_obj, "il2cpp_type_get_object")
    G(p_invoke, "il2cpp_runtime_invoke")

    size_t n = 0; void** asms = p_assemblies(p_domain_get(), &n);
    void *csharp = nullptr, *core = nullptr;
    for (size_t i = 0; i < n; i++) {
        void* img = p_image(asms[i]); const char* nm = p_image_name(img);
        if (!strcmp(nm, "Assembly-CSharp.dll")) csharp = img;
        else if (!strcmp(nm, "UnityEngine.CoreModule.dll")) core = img;
    }
    LogN("[resolve] assemblies: %ld", (long)n);
    if (!csharp || !core) { Log("[resolve] Assembly-CSharp or CoreModule image not found"); return false; }
    g_gmClass = p_class(csharp, "", "GameMaster");
    g_objClass = p_class(core, "UnityEngine", "Object");
    if (!g_gmClass || !g_objClass) { Log("[resolve] class GameMaster/Object not found"); return false; }
    g_findMethod = p_method(g_objClass, "FindObjectOfType", 1);
    if (!g_findMethod) g_findMethod = p_method(g_objClass, "FindFirstObjectByType", 1);  // новые версии Unity
    Log("[resolve] FindObjectOfType found: %s", g_findMethod ? "yes" : "NO");
    return g_findMethod != nullptr;
}

static void CallGM(const char* method)
{
    if (!Resolve()) { Log("[call] resolve failed"); MessageBeep(MB_ICONERROR); return; }
    void* typeObj = p_type_obj(p_class_type(g_gmClass));
    void* args[1] = { typeObj }; void* exc = nullptr;
    void* inst = p_invoke(g_findMethod, nullptr, args, &exc);
    void* mth = p_method(g_gmClass, method, 0);
    Log("[call] %s", method);
    Log("[call] instance found: %s", inst ? "yes" : "NO");
    Log("[call] method found: %s", mth ? "yes" : "NO");
    if (!inst || !mth) { MessageBeep(MB_ICONERROR); return; }   // GameMaster нет на сцене или метод переименован
    exc = nullptr;
    p_invoke(mth, inst, nullptr, &exc);
    Log("[call] exception: %s", exc ? "YES" : "no");
    MessageBeep(exc ? MB_ICONERROR : MB_OK);
}

// Выполняется в потоке окна Unity = главный поток игры.
static LRESULT CALLBACK Proc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    if (msg == WM_KEYDOWN && w >= VK_F1 && w <= VK_F8) LogN("[key] F%ld pressed", (long)(w - VK_F1 + 1));
    if (msg == WM_KEYDOWN && !(l & 0x40000000)) {   // без автоповтора
        switch (w) {
        case VK_F1: CallGM("ToggleInvincibility"); break;
        case VK_F2: CallGM("ToggleInfiniteAmmo"); break;
        case VK_F3: CallGM("ToggleInfiniteStamina"); break;
        case VK_F4: CallGM("ToggleNoClipPerms"); break;
        case VK_F5: CallGM("ToggleIgnoreAI"); break;
        case VK_F6: CallGM("ToggleSlowMoPerms"); break;
        case VK_F7: CallGM("ToggleHideUI"); break;
        case VK_F8: CallGM("SpawnCash"); break;
        }
    }
    return CallWindowProcW(g_orig, h, msg, w, l);
}

static BOOL CALLBACK Enum(HWND h, LPARAM)
{
    DWORD pid; GetWindowThreadProcessId(h, &pid);
    wchar_t cls[64]; GetClassNameW(h, cls, 64);
    if (pid == GetCurrentProcessId() && !wcscmp(cls, L"UnityWndClass")) { g_wnd = h; return FALSE; }
    return TRUE;
}

static DWORD WINAPI Init(LPVOID)
{
    for (int i = 0; i < 600 && !g_wnd; i++) { EnumWindows(Enum, 0); Sleep(100); }
    if (g_wnd) g_orig = (WNDPROC)SetWindowLongPtrW(g_wnd, GWLP_WNDPROC, (LONG_PTR)Proc);
    Log("[init] window found: %s", g_wnd ? "yes" : "NO");
    Log("[init] subclass ok: %s", g_orig ? "yes" : "NO");
    MessageBoxA(nullptr, g_orig ? "GoreBoxKeys loaded. Window hooked OK. Press F1 in game." : "GoreBoxKeys loaded, but game window NOT found!", "GoreBoxKeys", MB_OK | MB_TOPMOST);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(mod); CreateThread(nullptr, 0, Init, nullptr, 0, nullptr); }
    return TRUE;
}

