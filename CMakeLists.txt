cmake_minimum_required(VERSION 3.21)
project(Lifenvader VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR
        "Lifenvader targets Windows only.\n"
        "Build it on Windows with Visual Studio 2022 (Desktop development with C++).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static CRT so the .exe runs without a Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)

# --- nlohmann/json ------------------------------------------------------
# Single-header JSON, used for the C++ <-> JavaScript message protocol.
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d)
FetchContent_MakeAvailable(nlohmann_json)

# --- WebView2 SDK -------------------------------------------------------
# A .nupkg is a plain zip, so we can pull the SDK without needing nuget.exe.
# Bump this to pick up a newer SDK; see https://www.nuget.org/packages/Microsoft.Web.WebView2
# The URL_HASH below is tied to this version -- update both together.
set(WEBVIEW2_VERSION "1.0.2903.40" CACHE STRING "Microsoft.Web.WebView2 SDK version")

FetchContent_Declare(webview2
    URL "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VERSION}"
    # The URL has no file extension, so CMake cannot infer the archive type.
    # DOWNLOAD_NAME gives it one, otherwise extraction is skipped.
    DOWNLOAD_NAME "webview2-${WEBVIEW2_VERSION}.zip"
    URL_HASH SHA256=ef128016dd1e51c59178c827ed5b8aa3322c57afa8675d930f8109505542ad74)
FetchContent_MakeAvailable(webview2)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WEBVIEW2_ARCH "x64")
else()
    set(WEBVIEW2_ARCH "x86")
endif()

add_library(webview2 INTERFACE)
target_include_directories(webview2 INTERFACE "${webview2_SOURCE_DIR}/build/native/include")
target_link_libraries(webview2 INTERFACE
    "${webview2_SOURCE_DIR}/build/native/${WEBVIEW2_ARCH}/WebView2LoaderStatic.lib")

# --- embedded user interface --------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(EmbedUi)
embed_ui_directory("${CMAKE_CURRENT_SOURCE_DIR}/ui" UI_RC UI_TABLE)

# --- application --------------------------------------------------------
add_executable(lifenvader WIN32
    src/main.cpp
    src/App.cpp
    src/Bridge.cpp
    src/backend/AuthService.cpp
    src/backend/FeedService.cpp
    ${UI_RC}
    ${UI_TABLE}
)

target_include_directories(lifenvader PRIVATE src)
target_link_libraries(lifenvader PRIVATE
    webview2
    nlohmann_json::nlohmann_json
    version      # required by the WebView2 static loader
    shlwapi
)

target_compile_definitions(lifenvader PRIVATE
    UNICODE _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(lifenvader PRIVATE /W4 /permissive- /utf-8)
endif()

# The interface is compiled into the binary, so lifenvader.exe ships as a
# single file. Copying ui/ next to the .exe still overrides the embedded copy,
# which is handy while iterating on CSS without rebuilding.
