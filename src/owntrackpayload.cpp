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
            // traccar keys parsed in order of
            // https://github.com/traccar/traccar-client-sdk/blob/v1.0.8/core/src/commonMain/kotlin/org/traccar/client/HttpUploader.kt
            if (key == "id") { p.location["tid"] = value; continue; }
            if (key == "lat") { p.location["lat"] = std::stod(value); continue; }
            if (key == "lon") { p.location["lon"] = std::stod(value); continue; }
            if (key == "timestamp") { p.location["tst"] = std::stod(value); continue; }
            if (key == "accuracy") { p.location["acc"] = std::stod(value); continue; }
            if (key == "altitude") { p.location["alt"] = std::stod(value); continue; }
            // for some reason traccar has speed in knots continue; }
            if (key == "speed") { p.location["vel"] = knotsToKilometersPerHour(std::stod(value)); continue; }
            if (key == "bearing") { p.location["cog"] = std::stod(value); continue; }
            if (key == "batt") { p.location["batt"] = std::stod(value); continue; }
            if (key == "charge") { p.location["bs"] = value == "true" ? 2 : 1; continue; }
            if (key == "alarm") { /* ignored */ continue; }

            // forwarded
            p.forward.emplace(key, value);
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
        for (auto it = missing.begin(); it != missing.end();) {
            err.append(*it);
            if (++it != missing.end()) err.append(", ");
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

std::string OwntracksPayload::forwardUrlEncoded() const {
    std::string s{};
    for (auto it = forward.begin(); it != forward.end();) {
        s.append(httplib::encode_uri_component(it->first));
        s.append("=");
        s.append(httplib::encode_uri_component(it->second));
        if (++it != forward.end()) s.append("&");
    }
    return s;
}
