#include <nlohmann/json.hpp>
#include <httplib.h>
#include <mutex>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <optional>
#include <expected>
#include <regex>

#include "owntrackpayload.hpp"

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
        std::cout << std::format("Got request with params:\n");
        for (const auto &[key, value] : req.params) {
            std::cout << "  " << key << ": ";

            if (key == "api_key") {
                std::cout << "[REDACTED]";
            } else {
                std::cout << value;
            }
            
            std::cout << "\n";
        }

        // try to parse from params
        const auto payload = OwntracksPayload::fromParams(req.params);
        if (!payload) {
            std::cout << "Failed to parse payload: " << payload.error() << "\n";
            // "Accepted" in the sense that we looked at it and won't
            // process it any further; sadly we need a 2XX code or the client
            // will resend its bad request forever
            res.status = httplib::Accepted_202;
            res.set_content(payload.error(), "text/plain");
            return;
        }

        std::cout << "As Owntracks JSON:\n" << payload->location.dump(2) << "\n";

        std::string params{};
        if (!payload->apiKey.empty()) {
            params += "?api_key=" + payload->apiKey;
        }

        {
            std::lock_guard lock{client_mutex};
            auto client_res = client.Post(owntracks_netloc.value() + params,
                                          payload->location.dump(),
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

    std::cout << std::format("Starting server on {}:{}\n", server_addr, server_port);
    server.listen(server_addr, server_port);

    std::cout << "Stopped server\n";
    return 0;
}
