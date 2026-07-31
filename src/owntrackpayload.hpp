#pragma once

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <string>
#include <expected>

struct OwntracksPayload {
    // api key used for forwarding
    std::string apiKey{};

    nlohmann::json location{
        // required metadata
        {"_type", "location"},

        // tracker ID
        {"tid", nullptr},

        // UNIX timestamp of the point
        {"tst", nullptr},

        // Dawarich uses this to determine whether to convert km/h to m/s
        // (https://github.com/Freika/dawarich/blob/1.10.3/app/services/own_tracks/params.rb#L88)
        // Spoof to non-empty dummy value to match Owntracks (km/h)
        {"topic", "owntracks/traccar-owntracks-proxy"},

        // battery state, 0=unknown, 1=unplugged, 2=charging, 3=full
        {"bs", 0},

        // battery level, persentage [0, 100]
        {"batt", nullptr},

        // position
        {"lat", nullptr},
        {"lon", nullptr},
        {"alt", nullptr},

        // accuracy in meters
        {"acc", nullptr},

        // velocity, in km/h (see note above for topic)
        {"vel", nullptr},
    };

    /**
     * Parse the HTTP param list for location data
     * (as in https://github.com/traccar/traccar-client-sdk/blob/v1.0.8/core/src/commonMain/kotlin/org/traccar/client/HttpUploader.kt)
     */
    static std::expected<OwntracksPayload, std::string> fromParams(const httplib::Params &params);
};
