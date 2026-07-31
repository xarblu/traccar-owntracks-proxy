#include "owntrackpayload.hpp"

#include <expected>
#include <format>

static inline double knotsToKilometersPerHour(double knots) {
    constexpr auto multiplier{3.6 / 1.94384};
    return knots * multiplier;
}

std::expected<OwntracksPayload, std::string> OwntracksPayload::fromParams(const httplib::Params &params) {
    OwntracksPayload p{};

    // remap based on
    // https://owntracks.org/booklet/tech/json/#_typelocation

    try {
        for (const auto &[key, value] : params) {
            // extra keys handled by us
            if (key == "api_key") p.apiKey = value;

            // traccar keys parsed in order of
            // https://github.com/traccar/traccar-client-sdk/blob/v1.0.8/core/src/commonMain/kotlin/org/traccar/client/HttpUploader.kt
            if (key == "id") p.location["tid"] = value;
            if (key == "lat") p.location["lat"] = std::stod(value);
            if (key == "lon") p.location["lon"] = std::stod(value);
            if (key == "timestamp") p.location["tst"] = std::stod(value);
            if (key == "accuracy") p.location["acc"] = std::stod(value);
            if (key == "altitude") p.location["alt"] = std::stod(value);
            // for some reason traccar has speed in knots
            if (key == "speed") p.location["vel"] = knotsToKilometersPerHour(std::stod(value));
            if (key == "bearing") p.location["cog"] = std::stod(value);
            if (key == "batt") p.location["batt"] = std::stod(value);
            if (key == "charge") p.location["bs"] = value == "true" ? 2 : 1;
            // unhandled "alarm"
        }
    } catch (...) {
        return std::unexpected{"Failed to parse number value"};
    }

    // required fields
    for (const auto &key : {"tid", "lat", "lon", "tst"}) {
        if (p.location[key].is_null()) return std::unexpected{std::format("Missing required field: {}", key)};
    }

    return std::move(p);
}
