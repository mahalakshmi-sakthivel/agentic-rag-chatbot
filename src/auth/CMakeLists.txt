cmake_minimum_required(VERSION 3.16)

# Phase 2 JWT verification seam used by Phase 3 protected routes.
# Only the authentication components required by the verifier are linked here.
add_library(auth STATIC
    jwt_verifier.cpp
    token_service.cpp
    session_manager.cpp
    ../db/database.cpp
)

target_include_directories(auth
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../../config
)

target_link_libraries(auth
    PUBLIC
        Drogon::Drogon
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
        SQLite::SQLite3
        jwt-cpp::jwt-cpp
)
