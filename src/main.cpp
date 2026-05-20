#include <nlohmann/json.hpp>
#include <httplib.h>
#include <mutex>
#include <nlohmann/json_fwd.hpp>
#include <iostream>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <chrono>
#include <optional>
#include <regex>

/**
 * Very basic ISO 8601 UTC time to UNIX epoch parser
 * with second accuracy
 */
static inline std::optional<std::int64_t> parseISO8601UTC(std::string timeString) {
    using std::operator""sv;
    constexpr size_t minLen{("YYYY-MM-DDTHH:MM:SSZ"sv).size()};
    constexpr size_t startYear{(""sv).size()};
    constexpr size_t startMonth{("YYYY-"sv).size()};
    constexpr size_t startDay{("YYYY-MM-"sv).size()};
    constexpr size_t startHour{("YYYY-MM-DDT"sv).size()};
    constexpr size_t startMinute{("YYYY-MM-DDTHH:"sv).size()};
    constexpr size_t startSecond{("YYYY-MM-DDTHH:MM:"sv).size()};

    if (timeString.size() < minLen) {
        return std::nullopt;
    }

    try {
        std::chrono::year year{std::stoi(timeString.substr(startYear, 4))};
        std::chrono::month month{static_cast<unsigned int>(std::stoi(timeString.substr(startMonth, 2)))};
        std::chrono::day day{static_cast<unsigned int>(std::stoi(timeString.substr(startDay, 2)))};

        std::chrono::year_month_day date{year, month, day};
        std::chrono::sys_days days{date};

        std::chrono::seconds seconds = days.time_since_epoch();
        seconds += std::chrono::hours{std::stoi(timeString.substr(startHour, 2))};
        seconds += std::chrono::minutes{std::stoi(timeString.substr(startMinute, 2))};
        seconds += std::chrono::seconds{std::stoi(timeString.substr(startSecond, 2))};

        return seconds.count();
    } catch (std::invalid_argument e) {
        return std::nullopt;
    }
}

int main(int argc, char *argv[]) {
    std::string server_addr{"0.0.0.0"};
    uint server_port{8080};
    std::optional<std::string> owntracks_url{};

    for (int i = 1; i < argc; i++) {
        std::string_view arg{argv[i]};

        if (arg == "--server-addr") {
            if (i + 1 >= argc) {
                std::cerr << "Argument " << arg << " requires a value\n";
                return 1;
            }
            server_addr = argv[++i];
        }

        else if (arg == "--server-port") {
            if (i + 1 >= argc) {
                std::cerr << "Argument " << arg << " requires a value\n";
                return 1;
            }
            try {
                server_port = std::stoi(argv[++i]);
            } catch (std::invalid_argument e) {
                std::cerr << "Invalid integer for argument: " << arg << "\n";
                return 1;
            }
        }

        else if (arg == "--owntracks-url") {
            if (i + 1 >= argc) {
                std::cerr << "Argument " << arg << " requires a value\n";
                return 1;
            }
            owntracks_url = argv[++i];
        }

        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return 1;
        }
    }

    if (!owntracks_url.has_value()) {
        std::cerr << "--owntracks-url is required\n";
        return 1;
    }

    std::optional<std::string> owntracks_host{};
    std::optional<std::string> owntracks_netloc{};

    std::regex url_regex{"^(https?://[^/]+)(/.+)?$"};
    std::smatch matches{};
    std::regex_match(owntracks_url.value(), matches, url_regex);

    bool skipFirst{true};
    for (auto &match : matches) {
        if (skipFirst) {
            skipFirst = false;
            continue;
        }

        if (!owntracks_host.has_value()) {
            owntracks_host = match;
            continue;
        }
        if (!owntracks_netloc.has_value()) {
            owntracks_netloc = match;
            continue;
        }
    }

    if (!owntracks_host.has_value()) {
        std::cerr << "Malformed URL: " << owntracks_url.value() << "\n";
        return 1;
    }
    if (!owntracks_netloc.has_value()) {
        std::cerr << "Using default location /\n";
        owntracks_netloc = "/";
    }

    std::cout << "Forwarding requests to: " << owntracks_host.value() << owntracks_netloc.value() << "\n";

    httplib::Server server{};
    httplib::Client client{owntracks_host.value()};
    std::mutex client_mutex{};

    server.Post("/api/v1/traccar/points", [&](const httplib::Request &req, httplib::Response &res) {
        if (!nlohmann::json::accept(req.body)) {
            res.status = httplib::StatusCode::BadRequest_400;
            res.set_content("Invalid JSON\n", "text/plain");
            return;
        }

        std::optional<std::string> api_key{};
        if (req.has_param("api_key")) {
            api_key = req.get_param_value("api_key");
        }

        nlohmann::json traccar_json = nlohmann::json::parse(req.body);
        nlohmann::json owntracks_json{};

        std::cout << "New request:\n";
        std::cout << "Traccar:\n" << traccar_json.dump(2) << "\n";

        // validate rough structure
        if (!traccar_json["device_id"].is_string()) {
            res.status = httplib::StatusCode::BadRequest_400;
            res.set_content("Bad or missing field: device_id\n", "text/plain");
            return;
        }
        if (!traccar_json["location"].is_object()) {
            res.status = httplib::StatusCode::BadRequest_400;
            res.set_content("Bad or missing field: location\n", "text/plain");
            return;
        }
        if (!traccar_json["location"]["coords"].is_object()) {
            res.status = httplib::StatusCode::BadRequest_400;
            res.set_content("Bad or missing field: location.coords\n", "text/plain");
            return;
        }
        if (!traccar_json["location"]["battery"].is_object()) {
            res.status = httplib::StatusCode::BadRequest_400;
            res.set_content("Bad or missing field: location.battery\n", "text/plain");
            return;
        }
        if (!traccar_json["location"]["activity"].is_object()) {
            res.status = httplib::StatusCode::BadRequest_400;
            res.set_content("Bad or missing field: location.activity\n", "text/plain");
            return;
        }
        if (!traccar_json["location"]["extras"].is_object()) {
            res.status = httplib::StatusCode::BadRequest_400;
            res.set_content("Bad or missing field: location.extras\n", "text/plain");
            return;
        }

        // remap based on
        // https://owntracks.org/booklet/tech/json/#_typelocation

        // required type
        owntracks_json["_type"] = "location";

        // ID of the device
        owntracks_json["tid"] = traccar_json["device_id"];

        // dawarich uses this to determine whether to convert km/h to m/s
        // (https://github.com/Freika/dawarich/blob/1.7.8/app/services/own_tracks/params.rb#L79)
        // let's just spoof it to any non-empty string
        owntracks_json["topic"] = "owntracks/tracca-owntracks-proxy/" + traccar_json["device_id"].get<std::string>();

        // TODO: remap location.activity

        // location.battery
        // Battery status | Traccar: bool | Owntracks: int, 0=unknown, 1=unplugged, 2=charging, 3=full
        owntracks_json["bs"] = traccar_json["location"]["battery"]["is_charging"] ? 2 : 1;
        // Battery level | Traccar: float [0, 1] | Owntracks: int [0, 100]
        if (auto &val = traccar_json["location"]["battery"]["level"]; val.is_number()) {
            owntracks_json["batt"] = val.get<float>() * 100.0;
        }

        // location.coords
        // see https://docs.transistorsoft.com/flutter/Coords/
        // Accuracy | Traccar: float, meters | Owntracks: int, meters
        if (auto &val = traccar_json["location"]["coords"]["accuracy"]; val.is_number()) {
            owntracks_json["acc"] = val;
        }
        // Altitude | Traccar: float, meters | Owntracks: int, meters
        if (auto &val = traccar_json["location"]["coords"]["altitude"]; val.is_number()) {
            owntracks_json["alt"] = val;
        }
        // Heading | Traccar: float, degrees | Owntracks: int, degrees
        if (auto &val = traccar_json["location"]["coords"]["heading"]; val.is_number()) {
            if (auto deg = val.get<float>(); deg >= 0.0) {
                owntracks_json["cog"] = deg;
            }
        }
        // Latitude coordinate | Traccar: float | Owntracks: float
        if (auto &val = traccar_json["location"]["coords"]["latitude"]; val.is_number()) {
            owntracks_json["lat"] = val;
        }
        // Longitude coordinate | Traccar: float | Owntracks: float
        if (auto &val = traccar_json["location"]["coords"]["longitude"]; val.is_number()) {
            owntracks_json["lon"] = val;
        }
        // Velocity | Traccar: float, m/s | Owntracks: int, km/h
        if (auto &val = traccar_json["location"]["coords"]["speed"]; val.is_number()) {
            if (auto km_h = val.get<float>() * 3.6; km_h >= 0.0) {
                owntracks_json["vel"] = km_h;
            }
        }

        // location.*
        // Timestamp of the location fix | Traccar: string, ISO8601UTC | Owntracks: int, Unix Epoch
        if (auto &val = traccar_json["location"]["timestamp"]; val.is_string()) {
            owntracks_json["isotst"] = val;
            if (auto tst = parseISO8601UTC(val); tst.has_value()) {
                owntracks_json["tst"] = tst;
            }
        }

        std::cout << "Owntracks:\n" << owntracks_json.dump(2) << "\n";

        std::string params{};
        if (api_key.has_value()) {
            params += "?api_key=" + api_key.value();
        }

        {
            std::lock_guard lock{client_mutex};
            auto client_res = client.Post(owntracks_netloc.value() + params,
                                          owntracks_json.dump(),
                                          "application/json");

            if (client_res) {
                if (client_res->status == httplib::StatusCode::OK_200) {
                    std::cerr << "Successfully forwarded to Owntracks endpoint\n";
                } else {
                    std::cerr << "Owntracks endpoint returned " << client_res->status << "\n";
                }

                // forward result back
                res.status = client_res->status;
                res.set_content(client_res->body, client_res->get_header_value("Content-Type", "text/plain"));
                return;
            } else {
                std::cerr << "Connecting to Owntracks endpoint failed: " << client_res.error() << "\n";
                res.status = httplib::StatusCode::BadGateway_502;
                res.set_content("Connecting to Owntracks endpoint failed\n", "text/plain");
                return;
            }
        }
    });

    std::cout << "Starting server\n";
    server.listen(server_addr, server_port);

    std::cout << "Stopped server\n";
    return 0;
}
