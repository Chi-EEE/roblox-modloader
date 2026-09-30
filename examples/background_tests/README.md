# Background Studio tests

`background_tests.c` is an early Win32 mod for unattended Studio test processes.
Studio-Offline's WebView2 proxy loads it before Studio creates its windows. It
activates only with `RML_BACKGROUND_TESTS=1`; interactive Studio stays unaffected.

The mod removes visibility from top-level windows at creation, blocks showing and
activation APIs, prevents taskbar flashing, and suppresses console allocation and
Win32 sound notifications. Child windows keep their normal styles so Qt can build
its widget tree. Hooks apply inside the test process, on the normal display.

Build with a Windows C compiler and MinHook 1.3.4:

```sh
x86_64-w64-mingw32-gcc -O2 -shared -o background_tests.dll background_tests.c \
  minhook/src/buffer.c minhook/src/hook.c minhook/src/trampoline.c \
  minhook/src/hde/hde64.c -I minhook/include -luser32 -lwinmm
```

Install at `RobloxModLoader/background_tests.dll` in the test-only Studio install.
This is an early DLL loaded by the proxy, rather than a `ModBase` plugin: ordinary
mod lifecycle callbacks happen too late to reliably prevent startup focus changes.

`smoke.c` checks that explicit show/activate calls leave its window hidden and the
foreground window unchanged. It also checks console suppression and
`WS_EX_NOACTIVATE`. Build it with the same compiler and pass the mod's Windows path
as its only argument, with `RML_BACKGROUND_TESTS=1`.

If any required hook cannot be installed, the test process exits with code 86.
`RML_BACKGROUND_READY_FILE` can name a Windows file path to receive a `ready`
marker only after every hook is installed.
