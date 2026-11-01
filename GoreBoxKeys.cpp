// GoreBoxKeys v3: инжектируемая DLL без загрузчика и без RVA.
// Вызывает встроенные режимы игры (GameMaster.ToggleXxx) через il2cpp API по именам.
// Клавиши: F1 God, F2 Infinite Ammo, F3 Stamina, F4 NoClip, F5 Ignore AI, F6 SlowMo, F7 Hide UI, F8 Cash
// Лог: %TEMP%\GoreBoxKeys.log
//
// Сборка (x64!):
//   MinGW : g++ -shared -O2 -static -o GoreBoxKeys.dll GoreBoxKeys.cpp -luser32
//   MSVC  : cl /LD /O2 /EHsc GoreBoxKeys.cpp user32.lib /Fe:GoreBoxKeys.dll
#include <windows.h>
#include <string.h>
#include <stdio.h>
 
static void LogS(const char* a, const char* b = "")
{
    char path[MAX_PATH]; GetTempPathA(MAX_PATH, path); strcat(path, "GoreBoxKeys.log");
    FILE* f = fopen(path, "a"); if (!f) return;
    fprintf(f, "%s%s\n", a, b); fclose(f);
}
static void LogN(const char* a, long n)
{
    char buf[64]; sprintf(buf, "%ld", n); LogS(a, buf);
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
typedef void* (*f_class_get_fields)(void* klass, void** iter);
typedef int (*f_field_get_flags)(void* field);
typedef void* (*f_field_get_type)(void* field);
typedef const char* (*f_field_get_name)(void* field);
typedef void* (*f_class_from_type)(void* type);
typedef void (*f_field_static_get_value)(void* field, void* value);
typedef unsigned int (*f_array_length)(void* arr);
 
static f_domain_get p_domain_get; static f_domain_get_assemblies p_assemblies;
static f_assembly_get_image p_image; static f_image_get_name p_image_name;
static f_class_from_name p_class; static f_class_get_method p_method;
static f_class_get_type p_class_type; static f_type_get_object p_type_obj;
static f_runtime_invoke p_invoke;
static f_class_get_fields p_fields; static f_field_get_flags p_field_flags;
static f_field_get_type p_field_type; static f_field_get_name p_field_name;
static f_class_from_type p_class_from_type; static f_field_static_get_value p_static_get;
static f_array_length p_array_len;
 
static WNDPROC g_orig; static HWND g_wnd;
static void *g_gmClass, *g_findMethod, *g_findAllMethod, *g_objClass;
static bool g_resolved, g_failed;
 
static bool Resolve()
{
    if (g_resolved) return true;
    if (g_failed) return false;
    HMODULE m = GetModuleHandleA("GameAssembly.dll");
    if (!m) { LogS("[resolve] GameAssembly.dll not found"); return false; }
#define NEED(v, n) v = (decltype(v))GetProcAddress(m, n); if (!v) { LogS("[resolve] export missing: ", n); g_failed = true; return false; }
#define WANT(v, n) v = (decltype(v))GetProcAddress(m, n);
    NEED(p_domain_get, "il2cpp_domain_get") NEED(p_assemblies, "il2cpp_domain_get_assemblies")
    NEED(p_image, "il2cpp_assembly_get_image") NEED(p_image_name, "il2cpp_image_get_name")
    NEED(p_class, "il2cpp_class_from_name") NEED(p_method, "il2cpp_class_get_method_from_name")
    NEED(p_class_type, "il2cpp_class_get_type") NEED(p_type_obj, "il2cpp_type_get_object")
    NEED(p_invoke, "il2cpp_runtime_invoke")
    WANT(p_fields, "il2cpp_class_get_fields") WANT(p_field_flags, "il2cpp_field_get_flags")
    WANT(p_field_type, "il2cpp_field_get_type") WANT(p_field_name, "il2cpp_field_get_name")
    WANT(p_class_from_type, "il2cpp_class_from_type") WANT(p_static_get, "il2cpp_field_static_get_value")
    WANT(p_array_len, "il2cpp_array_length")
 
    size_t n = 0; void** asms = p_assemblies(p_domain_get(), &n);
    void *csharp = nullptr, *core = nullptr;
    for (size_t i = 0; i < n; i++) {
        void* img = p_image(asms[i]); const char* nm = p_image_name(img);
        if (!strcmp(nm, "Assembly-CSharp.dll")) csharp = img;
        else if (!strcmp(nm, "UnityEngine.CoreModule.dll")) core = img;
    }
    if (!csharp || !core) { LogS("[resolve] Assembly-CSharp / CoreModule not found"); g_failed = true; return false; }
    g_gmClass = p_class(csharp, "", "GameMaster");
    g_objClass = p_class(core, "UnityEngine", "Object");
    void* resCls = p_class(core, "UnityEngine", "Resources");
    if (!g_gmClass || !g_objClass) { LogS("[resolve] class GameMaster/Object not found"); g_failed = true; return false; }
    g_findMethod = p_method(g_objClass, "FindObjectOfType", 1);
    if (!g_findMethod) g_findMethod = p_method(g_objClass, "FindFirstObjectByType", 1);
    if (resCls) g_findAllMethod = p_method(resCls, "FindObjectsOfTypeAll", 1);
    LogS("[resolve] FindObjectOfType: ", g_findMethod ? "yes" : "NO");
    LogS("[resolve] FindObjectsOfTypeAll: ", g_findAllMethod ? "yes" : "NO");
    g_resolved = true;
    return true;
}
 
// 1) статическое поле типа GameMaster (синглтон), имя поля зашифровано, поэтому ищем по типу
static void* FindStatic()
{
    if (!p_fields || !p_field_flags || !p_field_type || !p_class_from_type || !p_static_get) return nullptr;
    void* it = nullptr; void* f;
    while ((f = p_fields(g_gmClass, &it)) != nullptr) {
        if (!(p_field_flags(f) & 0x10)) continue;               // FIELD_ATTRIBUTE_STATIC
        void* fc = p_class_from_type(p_field_type(f));
        if (fc != g_gmClass) continue;
        void* val = nullptr; p_static_get(f, &val);
        LogS("[find] static field of type GameMaster: ", p_field_name ? p_field_name(f) : "?");
        if (val) return val;
    }
    return nullptr;
}
 
static void* FindInstance()
{
    void* inst = FindStatic();
    if (inst) { LogS("[find] via static field"); return inst; }
 
    void* typeObj = p_type_obj(p_class_type(g_gmClass));
    void* args[1] = { typeObj }; void* exc = nullptr;
    if (g_findMethod) {
        inst = p_invoke(g_findMethod, nullptr, args, &exc);
        LogS("[find] FindObjectOfType exception: ", exc ? "YES" : "no");
        if (inst) { LogS("[find] via FindObjectOfType"); return inst; }
    }
    if (g_findAllMethod && p_array_len) {
        exc = nullptr;
        void* arr = p_invoke(g_findAllMethod, nullptr, args, &exc);
        LogS("[find] FindObjectsOfTypeAll exception: ", exc ? "YES" : "no");
        if (arr) {
            unsigned len = p_array_len(arr);
            LogN("[find] FindObjectsOfTypeAll count: ", (long)len);
            if (len > 0) { inst = ((void**)((char*)arr + 32))[0]; LogS("[find] via FindObjectsOfTypeAll"); return inst; }
        }
    }
    return nullptr;
}
 
static void CallGM(const char* method)
{
    LogS("[call] ", method);
    if (!Resolve()) { LogS("[call] resolve failed"); MessageBeep(MB_ICONERROR); return; }
    void* inst = FindInstance();
    void* mth = p_method(g_gmClass, method, 0);
    LogS("[call] instance: ", inst ? "found" : "NOT FOUND");
    LogS("[call] method: ", mth ? "found" : "NOT FOUND");
    if (!inst || !mth) { MessageBeep(MB_ICONERROR); return; }
    void* exc = nullptr;
    p_invoke(mth, inst, nullptr, &exc);
    LogS("[call] exception: ", exc ? "YES" : "no");
    MessageBeep(exc ? MB_ICONERROR : MB_OK);
}
 
// Выполняется в потоке окна Unity = главный поток игры.
static LRESULT CALLBACK Proc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
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
    LogS("[init] v3 window hooked: ", g_orig ? "yes" : "NO");
    return 0;
}
 
BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(mod); CreateThread(nullptr, 0, Init, nullptr, 0, nullptr); }
    return TRUE;
}
 
