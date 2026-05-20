#include <nlohmann/json.hpp>
#include <httplib.h>
#include <mutex>
#include <nlohmann/json_fwd.hpp>
#include <iostream>
#include <cstdint>
#include <string>
#include <string_view>
#include <chrono>

/**
 * Very basic ISO 8601 UTC time to UNIX epoch parser
 */
static inline std::int64_t parseISO8601UTC(std::string timeString) {
    using std::operator""sv;
    constexpr size_t fullLen{("YYYY-MM-DDTHH:MM:SSZ"sv).size()};
    constexpr size_t startYear{(""sv).size()};
    constexpr size_t startMonth{("YYYY-"sv).size()};
    constexpr size_t startDay{("YYYY-MM-"sv).size()};
    constexpr size_t startHour{("YYYY-MM-DDT"sv).size()};
    constexpr size_t startMinute{("YYYY-MM-DDTHH:"sv).size()};
    constexpr size_t startSecond{("YYYY-MM-DDTHH:MM:"sv).size()};

    if (timeString.size() != fullLen) {
        return 0;
    }

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
}

int main(int argc, char *argv[]) {
    std::string server_addr{"0.0.0.0"};
    uint server_port{8080};
    std::string owntracks_url{};

    for (int i = 0; i < argc; i++) {
        std::string_view arg{argv[i]};

        if (arg == "--server-addr") {
            if (i + 1 >= argc) return 1;
            server_addr = argv[i+1];
        }

        if (arg == "--server-port") {
            if (i + 1 >= argc) return 1;
            server_port = std::stoi(argv[i+1]);
        }

        if (arg == "--owntracks-url") {
            if (i + 1 >= argc) return 1;
            owntracks_url = argv[i+1];
        }
    }

    if (owntracks_url.empty()) {
        std::cerr << "--owntracks-url is required\n";
    }

    httplib::Server server{};
    httplib::Client client{"https://dawarich.gaydragon.zone/api/v1/owntracks/points"};
    std::mutex client_mutex{};

    server.Post("/api/v1/traccar/points", [&client, &client_mutex](const httplib::Request &req, httplib::Response &res) {
        nlohmann::json traccar_json = nlohmann::json::parse(req.body);
        nlohmann::json owntracks_json{};

        std::cout << traccar_json << "\n";

        // validate rough structure
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

        // ID of the device
        owntracks_json["tid"] = traccar_json["device_id"];

        // Timestamp in Unix epoch time
        owntracks_json["location"]["tst"] = parseISO8601UTC(traccar_json["location"]["timestamp"]);
        // Human-readable timestamp of the location fix
        owntracks_json["location"]["isotst"] = traccar_json["location"]["timestamp"];

        // Longitude coordinate
        owntracks_json["location"]["lon"] = traccar_json["location"]["coords"]["longitude"];
        // Latitude coordinate
        owntracks_json["location"]["lat"] = traccar_json["location"]["coords"]["latitude"];
        // Accuracy of position in meters
        owntracks_json["location"]["acc"] = traccar_json["location"]["coords"]["accuracy"];
        // Altitude in meters
        owntracks_json["location"]["alt"] = traccar_json["location"]["coords"]["altitude"];

        // Device battery level (percentage)
        owntracks_json["location"]["batt"] = traccar_json["location"]["battery"]["level"];
        // Battery status (0=unknown, 1=unplugged, 2=charging, 3=full)
        owntracks_json["location"]["bs"] = traccar_json["location"]["battery"]["is_charging"] ? 2 : 1;

        // Motion state (0=stopped, 1=moving)
        owntracks_json["location"]["m"] = traccar_json["location"]["is_moving"];
        // Internal message type (usually "location")
        owntracks_json["location"]["_type"] = "location";

        res.set_content(owntracks_json.dump(), "application/json");
    });

    std::cout << "Starting server\n";
    server.listen("0.0.0.0", 8080);

    std::cout << "Stopped server\n";
    return 0;
}
