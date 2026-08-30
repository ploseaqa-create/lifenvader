#include <windows.h>

#include "App.h"

int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int showCommand) {
    // WebView2 requires an initialised COM apartment on the UI thread.
    const HRESULT com = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com)) {
        return 1;
    }

    int exitCode = 1;
    {
        App app;
        if (app.Initialize(instance, showCommand)) {
            exitCode = app.Run();
        }
    }

    ::CoUninitialize();
    return exitCode;
}
