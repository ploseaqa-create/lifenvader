#pragma once

#include <windows.h>
#include <wrl/client.h>
#include <WebView2.h>

#include <memory>
#include <string>

#include "Bridge.h"

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

    // Bridge plumbing.
    void OnWebMessage(const std::wstring& json);
    void SendToWeb(const std::string& json);
    void HandleWindowCommand(const std::string& action);

    // Absolute path of the folder holding index.html.
    static std::wstring ResolveUiDirectory();

    HWND window_ = nullptr;
    HINSTANCE instance_ = nullptr;

    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webview_;

    std::unique_ptr<Bridge> bridge_;
};
