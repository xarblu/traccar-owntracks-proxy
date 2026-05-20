cmake_minimum_required(VERSION 4.1.0)

project(traccar-owntracks-project
    VERSION 0.1.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED 23)

# dependencies
find_package(httplib REQUIRED)
find_package(nlohmann_json REQUIRED)

# sources
add_subdirectory(src)
