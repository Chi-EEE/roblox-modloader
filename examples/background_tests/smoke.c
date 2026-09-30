#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <assert.h>
#include <stdio.h>
#include <wchar.h>

int main(int argc, char **argv) {
    assert(argc == 2);
    HWND foreground = GetForegroundWindow();
    assert(LoadLibraryA(argv[1]));
    wchar_t expected[MAX_PATH], folder[MAX_PATH];
    if (GetEnvironmentVariableW(L"RML_BACKGROUND_LOCALAPPDATA", expected, MAX_PATH)) {
        assert(SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, folder)));
        assert(wcscmp(folder, expected) == 0);
        assert(SUCCEEDED(SHGetFolderPathAndSubDirW(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, L"Roblox\\Plugins", folder)));
        wcscat(expected, L"\\Roblox\\Plugins");
        assert(wcscmp(folder, expected) == 0);
        assert(GetEnvironmentVariableW(L"RML_BACKGROUND_LOCALAPPDATA", expected, MAX_PATH));
        PWSTR known = NULL;
        assert(SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &known)));
        assert(wcscmp(known, expected) == 0);
        CoTaskMemFree(known);
    }
    if (GetEnvironmentVariableW(L"RML_BACKGROUND_DOCUMENTS", expected, MAX_PATH)) {
        assert(SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, folder)));
        assert(wcscmp(folder, expected) == 0);
        PWSTR known = NULL;
        assert(SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Documents, 0, NULL, &known)));
        assert(wcscmp(known, expected) == 0);
        CoTaskMemFree(known);
    }
    WNDCLASSW wide = {0};
    wide.lpfnWndProc = DefWindowProcW;
    wide.hInstance = GetModuleHandleW(NULL);
    wide.lpszClassName = L"BackgroundTestSmoke";
    assert(RegisterClassW(&wide));
    HWND window = CreateWindowExW(WS_EX_APPWINDOW, wide.lpszClassName, L"BackgroundTestSmoke", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 10, 10, 320, 240, NULL, NULL, wide.hInstance, NULL);
    assert(window);
    ShowWindow(window, SW_SHOW);
    ShowWindowAsync(window, SW_SHOW);
    SetWindowPos(window, HWND_TOPMOST, 10, 10, 320, 240, SWP_SHOWWINDOW);
    SetForegroundWindow(window);
    SetActiveWindow(window);
    SetFocus(window);
    BringWindowToTop(window);
    FlashWindow(window, TRUE);
    assert(!IsWindowVisible(window));
    assert(GetForegroundWindow() == foreground);
    assert(!AllocConsole());
    assert(GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_NOACTIVATE);
    DestroyWindow(window);
    WNDCLASSA ansi = {0};
    ansi.lpfnWndProc = DefWindowProcA;
    ansi.hInstance = GetModuleHandleA(NULL);
    ansi.lpszClassName = "BackgroundTestSmokeA";
    assert(RegisterClassA(&ansi));
    HWND ansi_window = CreateWindowExA(WS_EX_APPWINDOW, ansi.lpszClassName, "BackgroundTestSmokeA", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 10, 10, 320, 240, NULL, NULL, ansi.hInstance, NULL);
    assert(ansi_window && !IsWindowVisible(ansi_window));
    assert(GetForegroundWindow() == foreground);
    DestroyWindow(ansi_window);
    puts("PASS: native window stays hidden; foreground unchanged; console suppressed");
    return 0;
}
