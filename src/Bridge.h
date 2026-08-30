#pragma once

#include <functional>
#include <string>

#include <nlohmann/json.hpp>

#include "backend/AuthService.h"
#include "backend/FeedService.h"

// Routes JSON messages between the web UI and the C++ backend.
//
// Request   (JS  -> C++):  {"id": "42", "channel": "auth:login", "payload": {...}}
// Response  (C++ -> JS ):  {"id": "42", "ok": true,  "data": {...}}
//                          {"id": "42", "ok": false, "error": "..."}
// Push      (C++ -> JS ):  {"channel": "feed:updated", "data": {...}}
//
// To add a backend call: handle a new channel in Dispatch() and call the
// matching method from ui/js/bridge.js.
class Bridge {
public:
    using Sender = std::function<void(const std::string&)>;
    using WindowCommandHandler = std::function<void(const std::string&)>;

    void SetSender(Sender sender) { sender_ = std::move(sender); }
    void SetWindowCommandHandler(WindowCommandHandler handler) {
        windowCommand_ = std::move(handler);
    }

    // Consumes one raw message coming from the page.
    void Handle(const std::string& rawJson);

    // Sends an unsolicited message to the page.
    void Emit(const std::string& channel, const nlohmann::json& data);

private:
    // Returns the payload for a successful call, or throws std::runtime_error
    // with a message that is shown to the user.
    nlohmann::json Dispatch(const std::string& channel, const nlohmann::json& payload);

    void Send(const nlohmann::json& message);

    Sender sender_;
    WindowCommandHandler windowCommand_;

    AuthService auth_;
    FeedService feed_;
};
