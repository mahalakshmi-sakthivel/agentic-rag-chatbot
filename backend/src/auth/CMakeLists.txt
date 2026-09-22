file(GLOB_RECURSE AUTH_SOURCES
    "*.cpp"
    "../db/*.cpp"
)

# Remove any crow-specific middleware if they exist
list(FILTER AUTH_SOURCES EXCLUDE REGEX "middleware/.*")

add_library(auth OBJECT ${AUTH_SOURCES})

target_include_directories(auth
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../../config
    PRIVATE
        ${OPENSSL_INCLUDE_DIR}
        ${SQLITE3_INCLUDE_DIRS}
)

target_link_libraries(auth
    PUBLIC
        Drogon::Drogon
        nlohmann_json::nlohmann_json
        Threads::Threads
        OpenSSL::SSL
        OpenSSL::Crypto
        ${SQLITE3_LIBRARIES}
        bcrypt
        jwt-cpp::jwt-cpp
)
