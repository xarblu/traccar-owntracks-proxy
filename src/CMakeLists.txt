add_executable(traccar-owntracks-proxy)

target_sources(traccar-owntracks-proxy
    PRIVATE
        main.cpp
        owntrackpayload.cpp
)

target_include_directories(traccar-owntracks-proxy
    PRIVATE
      ${httplib_INCLUDE_DIRS}
      ${nlohmann_json_INCLUDE_DIRS}
)

target_link_libraries(traccar-owntracks-proxy
    PRIVATE
      httplib::httplib
      nlohmann_json::nlohmann_json 
)
