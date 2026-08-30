#include "backend/FeedService.h"

std::vector<Advert> FeedService::Adverts() const {
    return {
        {"CYKA BLYAT", "Suche zuverlässigen Fahrer für Lieferungen. Gute Bezahlung.", "048598564",
         "28.02.2026", "Fahrer"},
        {"Anonym", "Verkaufe Fahrzeug, nur seriöse Anfragen. Preis verhandelbar.", "", "28.02.2026",
         ""},
    };
}

Quest FeedService::DailyQuest() const {
    return {2, "Der Angler", "Du musst 10 Fische fangen", 5, 10};
}
