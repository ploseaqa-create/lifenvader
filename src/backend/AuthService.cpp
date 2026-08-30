#include "backend/AuthService.h"

#include <windows.h>

#include <fstream>
#include <vector>

namespace {

std::string Trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

}  // namespace

std::string AuthService::SettingsPath() {
    std::vector<wchar_t> buffer(MAX_PATH);
    DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (length == buffer.size()) {
        buffer.resize(buffer.size() * 2);
        length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }

    std::wstring path(buffer.data(), length);
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        path.erase(slash);
    }
    path += L"\\lifenvader.session";

    const int size = ::WideCharToMultiByte(CP_UTF8, 0, path.data(), static_cast<int>(path.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, path.data(), static_cast<int>(path.size()), utf8.data(), size,
                          nullptr, nullptr);
    return utf8;
}

bool AuthService::VerifyCredentials(const std::string& username, const std::string& password,
                                    User& out) const {
    // ---------------------------------------------------------------------
    // PLACEHOLDER -- swap this for your real authentication.
    // Never ship an app that compares passwords in plain text; hash them
    // (bcrypt/argon2) or delegate to a server that does.
    // ---------------------------------------------------------------------
    if (username == "demo" && password == "demo") {
        out.username = username;
        out.displayName = "CYKA BLYAT";
        out.balance = 12500;
        return true;
    }
    return false;
}

LoginResult AuthService::Login(const std::string& rawUsername, const std::string& password,
                               bool remember) {
    LoginResult result;
    const std::string username = Trim(rawUsername);

    if (username.empty() || password.empty()) {
        result.message = "Bitte Username und Passwort eingeben.";
        return result;
    }

    User user;
    if (!VerifyCredentials(username, password, user)) {
        result.message = "Username oder Passwort ist falsch.";
        return result;
    }

    current_ = user;
    signedIn_ = true;
    WriteRememberedUsername(remember ? username : std::string{});

    result.ok = true;
    result.user = user;
    return result;
}

void AuthService::Logout() {
    current_ = {};
    signedIn_ = false;
}

std::string AuthService::RememberedUsername() const {
    std::ifstream file(SettingsPath());
    if (!file) return {};

    std::string username;
    std::getline(file, username);
    return Trim(username);
}

void AuthService::WriteRememberedUsername(const std::string& username) const {
    const std::string path = SettingsPath();
    if (username.empty()) {
        ::DeleteFileA(path.c_str());
        return;
    }
    std::ofstream file(path, std::ios::trunc);
    if (file) {
        file << username << '\n';
    }
}
