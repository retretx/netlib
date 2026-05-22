cmake_minimum_required(VERSION 3.24)
project(netlib_consumer_smoke LANGUAGES CXX)

find_package(netlib 1.0 CONFIG REQUIRED)

add_executable(consumer_smoke main.cpp)
target_link_libraries(consumer_smoke PRIVATE netlib::netlib)
target_compile_features(consumer_smoke PRIVATE cxx_std_23)
