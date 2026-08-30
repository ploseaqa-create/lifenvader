#pragma once

#include <windows.h>
#include <wrl/client.h>
#include <WebView2.h>

#include <memory>
#include <string>

#include "Bridge.h"
#include "UiResources.h"

// Owns the native window and the embedded WebView2 that renders the UI.
class App {
public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool Initialize(HINSTANCE instance, int showCommand);
    int Run();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool CreateMainWindow(HINSTANCE instance, int showCommand);
    HRESULT CreateWebView();
    HRESULT OnWebViewReady(ICoreWebView2Controller* controller);
    void ResizeWebView();

    // Points the virtual origin at the ui/ folder when one sits next to the
    // executable, otherwise at the copy embedded in the binary.
    HRESULT ServeUserInterface();
    HRESULT ServeFromFolder(const std::wstring& uiDir);
    HRESULT ServeFromEmbeddedResources();
    HRESULT OnResourceRequested(ICoreWebView2WebResourceRequestedEventArgs* args);
    HRESULT RespondNotFound(ICoreWebView2WebResourceRequestedEventArgs* args);

    // Maps a request URI onto an embedded file, or nullptr when unknown.
    static const UiResource* FindEmbeddedResource(const std::wstring& uri);

    // Bridge plumbing.
    void OnWebMessage(const std::wstring& json);
    void SendToWeb(const std::string& json);
    void HandleWindowCommand(const std::string& action, int width, int height);

    // Resizes the frameless window to the panel the page is showing and
    // reapplies the rounded corner region. Sizes are CSS pixels.
    void FitWindowToContent(int cssWidth, int cssHeight);
    void ApplyRoundedCorners();

    // Absolute path of the folder holding index.html.
    static std::wstring ResolveUiDirectory();

    HWND window_ = nullptr;
    HINSTANCE instance_ = nullptr;

    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment_;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webview_;

    std::unique_ptr<Bridge> bridge_;
};
