#pragma once

#include <string>

struct User {
    std::string username;
    std::string displayName;
    int balance = 0;
};

struct LoginResult {
    bool ok = false;
    std::string message;  // shown to the user when ok == false
    User user;
};

// Authentication and "remember me" persistence.
//
// The credential check below is a placeholder so the UI is usable out of the
// box. Replace VerifyCredentials() with your real backend (database, HTTP
// call, game server RPC, ...) -- nothing else needs to change.
class AuthService {
public:
    LoginResult Login(const std::string& username, const std::string& password, bool remember);
    void Logout();

    // Username stored by a previous "remember me" login, or an empty string.
    std::string RememberedUsername() const;

private:
    // The single place that decides whether a login is valid.
    bool VerifyCredentials(const std::string& username, const std::string& password, User& out) const;

    static std::string SettingsPath();
    void WriteRememberedUsername(const std::string& username) const;

    User current_;
    bool signedIn_ = false;
};
