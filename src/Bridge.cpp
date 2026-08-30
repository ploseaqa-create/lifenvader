#include "Bridge.h"

#include <stdexcept>

using nlohmann::json;

void Bridge::Handle(const std::string& rawJson) {
    json message;
    try {
        message = json::parse(rawJson);
    } catch (const json::exception&) {
        return;  // not addressed to us
    }
    if (!message.is_object()) {
        return;
    }

    const std::string id = message.value("id", std::string{});
    const std::string channel = message.value("channel", std::string{});
    const json payload = message.value("payload", json::object());

    if (channel.empty()) {
        return;
    }

    // Window controls are fire-and-forget and never produce a reply.
    if (channel == "window:command") {
        if (windowCommand_) {
            windowCommand_(payload.value("action", std::string{}));
        }
        return;
    }

    json response = {{"id", id}};
    try {
        response["ok"] = true;
        response["data"] = Dispatch(channel, payload);
    } catch (const std::exception& error) {
        response["ok"] = false;
        response["error"] = error.what();
        response.erase("data");
    }
    Send(response);
}

json Bridge::Dispatch(const std::string& channel, const json& payload) {
    if (channel == "auth:login") {
        const auto result = auth_.Login(payload.value("username", std::string{}),
                                        payload.value("password", std::string{}),
                                        payload.value("remember", false));
        if (!result.ok) {
            throw std::runtime_error(result.message);
        }
        return {
            {"username", result.user.username},
            {"displayName", result.user.displayName},
            {"balance", result.user.balance},
        };
    }

    if (channel == "auth:rememberedUser") {
        return {{"username", auth_.RememberedUsername()}};
    }

    if (channel == "auth:logout") {
        auth_.Logout();
        return json::object();
    }

    if (channel == "feed:adverts") {
        json items = json::array();
        for (const auto& advert : feed_.Adverts()) {
            items.push_back({
                {"author", advert.author},
                {"body", advert.body},
                {"phone", advert.phone},
                {"date", advert.date},
                {"tag", advert.tag},
            });
        }
        return {{"items", items}};
    }

    if (channel == "feed:quest") {
        const auto quest = feed_.DailyQuest();
        return {
            {"index", quest.index},   {"title", quest.title},      {"description", quest.description},
            {"progress", quest.progress}, {"goal", quest.goal},
        };
    }

    throw std::runtime_error("Unknown channel: " + channel);
}

void Bridge::Emit(const std::string& channel, const json& data) {
    Send({{"channel", channel}, {"data", data}});
}

void Bridge::Send(const json& message) {
    if (sender_) {
        sender_(message.dump());
    }
}
