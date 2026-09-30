// Loaded by Studio-Offline's WebView2 proxy before Studio creates its windows.
// This mod affects only the process with RML_BACKGROUND_TESTS=1.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "MinHook.h"

static HWND (WINAPI *original_create_a)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
static BOOL (WINAPI *original_show)(HWND, int);
static BOOL (WINAPI *original_show_async)(HWND, int);
static BOOL (WINAPI *original_position)(HWND, HWND, int, int, int, int, UINT);
static HWND (WINAPI *original_create)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
static HRESULT (WINAPI *original_folder)(HWND, int, HANDLE, DWORD, LPWSTR);
static HRESULT (WINAPI *original_subfolder)(HWND, int, HANDLE, DWORD, LPCWSTR, LPWSTR);
static HRESULT (WINAPI *original_known_folder)(const GUID *, DWORD, HANDLE, PWSTR *);
static wchar_t test_local_appdata[MAX_PATH];
static wchar_t test_documents[MAX_PATH];

// Qt and Studio use shell APIs rather than the LOCALAPPDATA environment variable.
static const wchar_t *folder_redirect(int folder) {
    return (folder & 0xff) == CSIDL_LOCAL_APPDATA ? test_local_appdata :
        (folder & 0xff) == CSIDL_PERSONAL ? test_documents : NULL;
}
static HRESULT WINAPI test_folder(HWND window, int folder, HANDLE token, DWORD flags, LPWSTR path) {
    const wchar_t *redirect = folder_redirect(folder);
    if (redirect && *redirect) {
        wcscpy(path, redirect);
        return S_OK;
    }
    return original_folder(window, folder, token, flags, path);
}
static HRESULT WINAPI test_subfolder(HWND window, int folder, HANDLE token, DWORD flags, LPCWSTR subfolder, LPWSTR path) {
    const wchar_t *redirect = folder_redirect(folder);
    if (!redirect || !*redirect) return original_subfolder(window, folder, token, flags, subfolder, path);
    size_t length = wcslen(redirect) + (subfolder ? wcslen(subfolder) + 1 : 0);
    if (length >= MAX_PATH) return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);
    wcscpy(path, redirect);
    if (subfolder && *subfolder) { wcscat(path, L"\\"); wcscat(path, subfolder); }
    if (folder & CSIDL_FLAG_CREATE) {
        int error = SHCreateDirectoryExW(NULL, path, NULL);
        if (error != ERROR_SUCCESS && error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS)
            return HRESULT_FROM_WIN32(error);
    }
    return S_OK;
}
static HRESULT WINAPI test_known_folder(const GUID *id, DWORD flags, HANDLE token, PWSTR *path) {
    const wchar_t *redirect = IsEqualGUID(id, &FOLDERID_LocalAppData) ? test_local_appdata :
        IsEqualGUID(id, &FOLDERID_Documents) ? test_documents : NULL;
    if (redirect && *redirect) {
        size_t bytes = (wcslen(redirect) + 1) * sizeof(wchar_t);
        *path = CoTaskMemAlloc(bytes);
        if (!*path) return E_OUTOFMEMORY;
        memcpy(*path, redirect, bytes);
        return S_OK;
    }
    return original_known_folder(id, flags, token, path);
}

static BOOL own_window(HWND window) {
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    return pid == GetCurrentProcessId();
}
static BOOL top_window(HWND window) {
    return own_window(window) && !(GetWindowLongPtrW(window, GWL_STYLE) & WS_CHILD);
}
static BOOL WINAPI quiet_show(HWND window, int command) {
    return original_show(window, top_window(window) ? SW_HIDE : command);
}
static BOOL WINAPI quiet_show_async(HWND window, int command) {
    return original_show_async(window, top_window(window) ? SW_HIDE : command);
}
static BOOL WINAPI quiet_position(HWND window, HWND after, int x, int y, int width, int height, UINT flags) {
    if (top_window(window)) flags = (flags & ~SWP_SHOWWINDOW) | SWP_NOACTIVATE | SWP_NOZORDER;
    return original_position(window, after, x, y, width, height, flags);
}
static HWND WINAPI quiet_create(DWORD ex, LPCWSTR klass, LPCWSTR title, DWORD style, int x, int y, int width, int height, HWND parent, HMENU menu, HINSTANCE instance, LPVOID param) {
    if (!(style & WS_CHILD)) {
        style &= ~WS_VISIBLE;
        ex = (ex & ~WS_EX_APPWINDOW) | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
    }
    return original_create(ex, klass, title, style, x, y, width, height, parent, menu, instance, param);
}
static HWND WINAPI quiet_create_a(DWORD ex, LPCSTR klass, LPCSTR title, DWORD style, int x, int y, int width, int height, HWND parent, HMENU menu, HINSTANCE instance, LPVOID param) {
    if (!(style & WS_CHILD)) {
        style &= ~WS_VISIBLE;
        ex = (ex & ~WS_EX_APPWINDOW) | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
    }
    return original_create_a(ex, klass, title, style, x, y, width, height, parent, menu, instance, param);
}
static BOOL WINAPI quiet_foreground(HWND window) { (void)window; return FALSE; }
static HWND WINAPI quiet_active(HWND window) { (void)window; return GetActiveWindow(); }
static HWND WINAPI quiet_focus(HWND window) { (void)window; return GetFocus(); }
static BOOL WINAPI quiet_bring(HWND window) { (void)window; return TRUE; }
static BOOL WINAPI quiet_flash(HWND window, BOOL invert) { (void)window; (void)invert; return FALSE; }
static BOOL WINAPI quiet_flash_ex(PFLASHWINFO info) { (void)info; return FALSE; }
static BOOL WINAPI quiet_console(void) { return FALSE; }
static BOOL WINAPI quiet_beep(UINT type) { (void)type; return TRUE; }
static BOOL WINAPI quiet_sound(LPCWSTR sound, HMODULE module, DWORD flags) { (void)sound; (void)module; (void)flags; return TRUE; }

static void hook(HMODULE module, const char *name, void *replacement, void **original) {
    void *target = (void *)GetProcAddress(module, name);
    if (!target || MH_CreateHook(target, replacement, original) != MH_OK || MH_EnableHook(target) != MH_OK) {
        // Fail closed: an unattended test must never silently fall back to visible Studio.
        ExitProcess(86);
    }
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)instance; (void)reserved;
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    char enabled[2] = {0};
    if (GetEnvironmentVariableA("RML_BACKGROUND_TESTS", enabled, sizeof(enabled)) != 1 || enabled[0] != '1') return TRUE;
    if (MH_Initialize() != MH_OK) return FALSE;
    HMODULE user = LoadLibraryW(L"user32.dll");
    HMODULE kernel = LoadLibraryW(L"kernel32.dll");
    HMODULE audio = LoadLibraryW(L"winmm.dll");
    DWORD folder_length = GetEnvironmentVariableW(L"RML_BACKGROUND_LOCALAPPDATA", test_local_appdata, MAX_PATH);
    DWORD documents_length = GetEnvironmentVariableW(L"RML_BACKGROUND_DOCUMENTS", test_documents, MAX_PATH);
    if (folder_length >= MAX_PATH || documents_length >= MAX_PATH) ExitProcess(86);
    if (folder_length || documents_length) {
        HMODULE shell = LoadLibraryW(L"shell32.dll");
        hook(shell, "SHGetFolderPathW", test_folder, (void **)&original_folder);
        hook(shell, "SHGetFolderPathAndSubDirW", test_subfolder, (void **)&original_subfolder);
        hook(shell, "SHGetKnownFolderPath", test_known_folder, (void **)&original_known_folder);
    }
    hook(user, "CreateWindowExA", quiet_create_a, (void **)&original_create_a);
    hook(user, "CreateWindowExW", quiet_create, (void **)&original_create);
    hook(user, "ShowWindow", quiet_show, (void **)&original_show);
    hook(user, "ShowWindowAsync", quiet_show_async, (void **)&original_show_async);
    hook(user, "SetWindowPos", quiet_position, (void **)&original_position);
    hook(user, "SetForegroundWindow", quiet_foreground, NULL);
    hook(user, "SetActiveWindow", quiet_active, NULL);
    hook(user, "SetFocus", quiet_focus, NULL);
    hook(user, "BringWindowToTop", quiet_bring, NULL);
    hook(user, "FlashWindow", quiet_flash, NULL);
    hook(user, "FlashWindowEx", quiet_flash_ex, NULL);
    hook(user, "MessageBeep", quiet_beep, NULL);
    hook(kernel, "AllocConsole", quiet_console, NULL);
    hook(audio, "PlaySoundW", quiet_sound, NULL);
    const char *ready = getenv("RML_BACKGROUND_READY_FILE");
    if (ready) {
        HANDLE file = CreateFileA(ready, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) { DWORD written; WriteFile(file, "ready", 5, &written, NULL); CloseHandle(file); }
    }
    return TRUE;
}
