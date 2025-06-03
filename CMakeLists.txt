cmake_minimum_required(VERSION 3.10)

cmake_policy(SET CMP0091 NEW)

project(xiaozhi-pc)

set(CMAKE_CXX_STANDARD 17)

find_package(fmt CONFIG REQUIRED)
find_package(cJSON CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(PahoMqttCpp CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(MbedTLS CONFIG REQUIRED)
find_package(Opus CONFIG REQUIRED)

add_subdirectory(src)
