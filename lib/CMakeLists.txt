add_library(ARBUZ STATIC
    application.cpp
    cachecontrol.cpp
    citycatalog.cpp
    commands.cpp
    httpclient.cpp
    models.cpp
    request.cpp
    utils.cpp
)

target_link_libraries(ARBUZ PUBLIC cpr::cpr)
target_link_libraries(ARBUZ PUBLIC nlohmann_json::nlohmann_json)
target_include_directories(ARBUZ PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
