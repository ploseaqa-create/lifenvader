#include "App.h"

#include <shlwapi.h>
#include <wrl/event.h>

#include <string>
#include <vector>

#include "UiResources.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace {

constexpr wchar_t kWindowClass[] = L"LifenvaderWindow";
constexpr wchar_t kWindowTitle[] = L"Lifenvader";

// The UI folder is served under this synthetic origin so that fetch(),
// ES modules and relative URLs behave like on a normal web page. Loading
// index.html from file:// would trip CORS on every module import.
// `.example` is reserved by IANA and can never resolve on the public internet,
// so a broken mapping fails loudly instead of silently hitting a real host.
constexpr wchar_t kVirtualHost[] = L"appassets.example";
constexpr wchar_t kStartUrl[] = L"https://appassets.example/index.html";

constexpr int kDefaultWidth = 1180;
constexpr int kDefaultHeight = 760;
constexpr int kMinWidth = 900;
constexpr int kMinHeight = 620;

std::string ToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size,
                          nullptr, nullptr);
    return out;
}

std::wstring ToUtf16(const std::string& text) {
    if (text.empty()) return {};
    const int size =
        ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

void ShowFatalError(const std::wstring& message) {
    ::MessageBoxW(nullptr, message.c_str(), kWindowTitle, MB_ICONERROR | MB_OK);
}

}  // namespace

App::App() : bridge_(std::make_unique<Bridge>()) {}

App::~App() = default;

std::wstring App::ResolveUiDirectory() {
    std::vector<wchar_t> buffer(MAX_PATH);
    DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (length == buffer.size()) {  // path was truncated, grow and retry
        buffer.resize(buffer.size() * 2);
        length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }

    std::wstring path(buffer.data(), length);
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        path.erase(slash);
    }
    return path + L"\\ui";
}

bool App::Initialize(HINSTANCE instance, int showCommand) {
    instance_ = instance;

    // Per-monitor DPI so the panel stays crisp on scaled displays.
    ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    if (!CreateMainWindow(instance, showCommand)) {
        ShowFatalError(L"Could not create the application window.");
        return false;
    }

    const HRESULT hr = CreateWebView();
    if (FAILED(hr)) {
        ShowFatalError(
            L"WebView2 could not be started.\n\n"
            L"Install the Microsoft Edge WebView2 Runtime:\n"
            L"https://developer.microsoft.com/microsoft-edge/webview2/");
        return false;
    }
    return true;
}

bool App::CreateMainWindow(HINSTANCE instance, int showCommand) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &App::WndProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    // Matches the darkest tone of the design so resizing never flashes white.
    wc.hbrBackground = ::CreateSolidBrush(RGB(0x0B, 0x06, 0x12));
    wc.lpszClassName = kWindowClass;

    if (!::RegisterClassExW(&wc)) {
        return false;
    }

    window_ = ::CreateWindowExW(0, kWindowClass, kWindowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                CW_USEDEFAULT, kDefaultWidth, kDefaultHeight, nullptr, nullptr,
                                instance, this);
    if (!window_) {
        return false;
    }

    ::ShowWindow(window_, showCommand);
    ::UpdateWindow(window_);
    return true;
}

HRESULT App::CreateWebView() {
    // Keep the browser profile beside the executable so the app stays portable.
    const std::wstring userDataFolder = ResolveUiDirectory() + L"\\..\\.webview2";

    return ::CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataFolder.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                if (FAILED(result) || environment == nullptr) {
                    ShowFatalError(L"WebView2 environment could not be created.");
                    ::PostQuitMessage(1);
                    return result;
                }
                // Needed later to build responses for embedded resources.
                environment_ = environment;

                return environment->CreateCoreWebView2Controller(
                    window_,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT hr, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(hr) || controller == nullptr) {
                                ShowFatalError(L"WebView2 controller could not be created.");
                                ::PostQuitMessage(1);
                                return hr;
                            }
                            return OnWebViewReady(controller);
                        })
                        .Get());
            })
            .Get());
}

HRESULT App::OnWebViewReady(ICoreWebView2Controller* controller) {
    controller_ = controller;
    controller_->get_CoreWebView2(&webview_);

    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webview_->get_Settings(&settings))) {
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_AreDevToolsEnabled(TRUE);  // F12; turn off for a release build
        settings->put_IsZoomControlEnabled(FALSE);
    }

    const HRESULT served = ServeUserInterface();
    if (FAILED(served)) {
        ::PostQuitMessage(1);
        return served;
    }

    // JS -> C++
    EventRegistrationToken token = {};
    webview_->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                LPWSTR raw = nullptr;
                if (SUCCEEDED(args->get_WebMessageAsJson(&raw)) && raw != nullptr) {
                    OnWebMessage(raw);
                    ::CoTaskMemFree(raw);
                }
                return S_OK;
            })
            .Get(),
        &token);

    // C++ -> JS
    bridge_->SetSender([this](const std::string& json) { SendToWeb(json); });
    bridge_->SetWindowCommandHandler(
        [this](const std::string& action) { HandleWindowCommand(action); });

    ResizeWebView();
    webview_->Navigate(kStartUrl);
    return S_OK;
}

HRESULT App::ServeUserInterface() {
    // A ui/ folder beside the executable wins, so the interface can be edited
    // without recompiling. Shipped builds fall back to the embedded copy.
    const std::wstring uiDir = ResolveUiDirectory();
    if (::PathFileExistsW((uiDir + L"\\index.html").c_str())) {
        return ServeFromFolder(uiDir);
    }
    return ServeFromEmbeddedResources();
}

HRESULT App::ServeFromFolder(const std::wstring& uiDir) {
    // Failures here must be reported: without the mapping WebView2 falls back
    // to real DNS and shows ERR_NAME_NOT_RESOLVED, which explains nothing.
    ComPtr<ICoreWebView2_3> webview3;
    HRESULT hr = webview_.As(&webview3);
    if (FAILED(hr)) {
        ShowFatalError(L"This WebView2 runtime is too old: it does not provide "
                       L"ICoreWebView2_3.\n\nInstall the current runtime from\n"
                       L"https://developer.microsoft.com/microsoft-edge/webview2/");
        return hr;
    }

    hr = webview3->SetVirtualHostNameToFolderMapping(
        kVirtualHost, uiDir.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
    if (FAILED(hr)) {
        ShowFatalError(L"Could not map the UI folder:\n" + uiDir);
    }
    return hr;
}

HRESULT App::ServeFromEmbeddedResources() {
    if (kUiResourceCount == 0) {
        ShowFatalError(L"This build contains no embedded user interface.");
        return E_FAIL;
    }

    HRESULT hr = webview_->AddWebResourceRequestedFilter(
        L"https://appassets.example/*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    if (FAILED(hr)) {
        ShowFatalError(L"Could not install the resource handler for the user interface.");
        return hr;
    }

    EventRegistrationToken token = {};
    return webview_->add_WebResourceRequested(
        Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                return OnResourceRequested(args);
            })
            .Get(),
        &token);
}

HRESULT App::OnResourceRequested(ICoreWebView2WebResourceRequestedEventArgs* args) {
    ComPtr<ICoreWebView2WebResourceRequest> request;
    if (FAILED(args->get_Request(&request))) {
        return S_OK;  // let WebView2 handle it
    }

    LPWSTR rawUri = nullptr;
    if (FAILED(request->get_Uri(&rawUri)) || rawUri == nullptr) {
        return S_OK;
    }
    const std::wstring uri(rawUri);
    ::CoTaskMemFree(rawUri);

    const UiResource* match = FindEmbeddedResource(uri);
    if (match == nullptr) {
        return RespondNotFound(args);
    }

    // Resource bytes stay mapped for the lifetime of the process; no free needed.
    const HRSRC info = ::FindResourceW(nullptr, MAKEINTRESOURCEW(match->id), RT_RCDATA);
    const HGLOBAL handle = info ? ::LoadResource(nullptr, info) : nullptr;
    const void* bytes = handle ? ::LockResource(handle) : nullptr;
    if (bytes == nullptr) {
        return RespondNotFound(args);
    }
    const DWORD size = ::SizeofResource(nullptr, info);

    ComPtr<IStream> stream;
    stream.Attach(::SHCreateMemStream(static_cast<const BYTE*>(bytes), size));
    if (!stream) {
        return RespondNotFound(args);
    }

    // Everything is served from the binary, so it can be cached indefinitely.
    std::wstring headers = L"Content-Type: ";
    headers += match->mime;
    headers += L"\r\nCache-Control: no-cache";

    ComPtr<ICoreWebView2WebResourceResponse> response;
    if (SUCCEEDED(environment_->CreateWebResourceResponse(stream.Get(), 200, L"OK",
                                                          headers.c_str(), &response))) {
        args->put_Response(response.Get());
    }
    return S_OK;
}

const UiResource* App::FindEmbeddedResource(const std::wstring& uri) {
    constexpr wchar_t kOrigin[] = L"https://appassets.example/";
    constexpr size_t kOriginLength = ARRAYSIZE(kOrigin) - 1;

    if (uri.compare(0, kOriginLength, kOrigin) != 0) {
        return nullptr;
    }

    std::wstring path = uri.substr(kOriginLength);
    // Drop any query string or fragment before matching.
    const size_t cut = path.find_first_of(L"?#");
    if (cut != std::wstring::npos) {
        path.erase(cut);
    }
    if (path.empty()) {
        path = L"index.html";
    }

    const std::string needle = ToUtf8(path);
    for (size_t i = 0; i < kUiResourceCount; ++i) {
        if (needle == kUiResources[i].path) {
            return &kUiResources[i];
        }
    }
    return nullptr;
}

HRESULT App::RespondNotFound(ICoreWebView2WebResourceRequestedEventArgs* args) {
    ComPtr<ICoreWebView2WebResourceResponse> response;
    if (SUCCEEDED(environment_->CreateWebResourceResponse(nullptr, 404, L"Not Found", L"",
                                                          &response))) {
        args->put_Response(response.Get());
    }
    return S_OK;
}

void App::OnWebMessage(const std::wstring& json) {
    bridge_->Handle(ToUtf8(json));
}

void App::SendToWeb(const std::string& json) {
    if (webview_) {
        webview_->PostWebMessageAsJson(ToUtf16(json).c_str());
    }
}

void App::HandleWindowCommand(const std::string& action) {
    if (!window_) return;

    if (action == "close") {
        ::PostMessageW(window_, WM_CLOSE, 0, 0);
    } else if (action == "minimize") {
        ::ShowWindow(window_, SW_MINIMIZE);
    } else if (action == "drag") {
        // Let the user move the window by dragging the panel header.
        ::ReleaseCapture();
        ::SendMessageW(window_, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }
}

void App::ResizeWebView() {
    if (!controller_ || !window_) return;
    RECT bounds = {};
    ::GetClientRect(window_, &bounds);
    controller_->put_Bounds(bounds);
}

int App::Run() {
    MSG msg = {};
    while (::GetMessageW(&msg, nullptr, 0, 0)) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    App* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<App*>(create->lpCreateParams);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<App*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT App::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            ResizeWebView();
            return 0;

        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = kMinWidth;
            info->ptMinTrackSize.y = kMinHeight;
            return 0;
        }

        case WM_DESTROY:
            controller_.Reset();
            webview_.Reset();
            ::PostQuitMessage(0);
            return 0;

        default:
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}
