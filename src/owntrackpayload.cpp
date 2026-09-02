#include "owntrackpayload.hpp"

#include <expected>
#include <format>
#include <vector>
#include <string>

static inline double knotsToKilometersPerHour(double knots) {
    // according to wikipedia 1kn is defined as *exactly* 1.852 km/h
    return knots * 1.852;
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
    std::vector<const char*> missing{};
    for (const auto &key : {"tid", "lat", "lon", "tst"}) {
        if (p.location[key].is_null()) missing.emplace_back(key);
    }

    if (!missing.empty()) {
        std::string err{"Missing required fields: "};
        for (auto it = missing.begin(); it != missing.end(); it++) {
            err.append(*it);
            if (it + 1 != missing.end()) err.append(", ");
        }
        return std::unexpected{err};
    }

    p.stripNull();

    return std::move(p);
}

void OwntracksPayload::stripNull() {
    nlohmann::json stripped = location;

    for (auto it = location.begin(); it != location.end(); it++) {
        if (it.value().is_null()) {
            stripped.erase(it.key());
        }
    }

    location = stripped;
}
