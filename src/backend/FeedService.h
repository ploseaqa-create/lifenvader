#pragma once

#include <string>
#include <vector>

struct Advert {
    std::string author;
    std::string body;
    std::string phone;  // empty when the advert hides its number
    std::string date;
    std::string tag;    // empty when the advert has no category chip
};

struct Quest {
    int index = 0;
    std::string title;
    std::string description;
    int progress = 0;
    int goal = 0;
};

// Supplies the content shown on the dashboard.
//
// Returns fixed sample data today; point these methods at your database or
// game server when the backend is ready.
class FeedService {
public:
    std::vector<Advert> Adverts() const;
    Quest DailyQuest() const;
};
