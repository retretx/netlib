# Опциональные C++20 modules (зеркало umbrella-заголовков).

add_library(netlib_simple)
add_library(netlib::simple ALIAS netlib_simple)
target_sources(netlib_simple PUBLIC
    FILE_SET CXX_MODULES FILES
        netlib.net.simple.cppm
)
target_link_libraries(netlib_simple PUBLIC netlib::netlib)
target_compile_features(netlib_simple PUBLIC cxx_std_23)

add_library(netlib_medium)
add_library(netlib::medium ALIAS netlib_medium)
target_sources(netlib_medium PUBLIC
    FILE_SET CXX_MODULES FILES
        netlib.net.medium.cppm
)
target_link_libraries(netlib_medium PUBLIC netlib::netlib)
target_compile_features(netlib_medium PUBLIC cxx_std_23)

add_library(netlib_net_module)
add_library(netlib::net_module ALIAS netlib_net_module)
target_sources(netlib_net_module PUBLIC
    FILE_SET CXX_MODULES FILES
        netlib.net.cppm
)
target_link_libraries(netlib_net_module PUBLIC netlib::netlib)
target_compile_features(netlib_net_module PUBLIC cxx_std_23)

install(TARGETS netlib_simple netlib_medium netlib_net_module
    EXPORT netlibTargets
)
