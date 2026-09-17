cmake_minimum_required(VERSION 3.16)

# ── Catch2 (header-only v2) ───────────────────────────────────────────────────
# Download Catch2 header automatically if not present
set(CATCH2_SINGLE_INCLUDE ${CMAKE_SOURCE_DIR}/../third_party/catch2/catch.hpp)
if(NOT EXISTS ${CATCH2_SINGLE_INCLUDE})
    message(STATUS "Downloading Catch2 single-header...")
    file(DOWNLOAD
        "https://github.com/catchorg/Catch2/releases/download/v2.13.10/catch.hpp"
        ${CATCH2_SINGLE_INCLUDE}
        SHOW_PROGRESS
    )
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/../src
    ${CMAKE_SOURCE_DIR}/../config
    ${CMAKE_SOURCE_DIR}/../third_party/catch2
    ${CMAKE_SOURCE_DIR}/../third_party/crow/include
    ${CMAKE_SOURCE_DIR}/../third_party/jwt-cpp/include
    ${CMAKE_SOURCE_DIR}/../third_party/nlohmann
    ${CMAKE_SOURCE_DIR}/../third_party/bcrypt/include
    ${OPENSSL_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

# ── Helper macro to add a test ────────────────────────────────────────────────
macro(add_auth_test test_name source_file)
    add_executable(${test_name} auth/${source_file})
    target_link_libraries(${test_name} PRIVATE auth_lib)
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES TIMEOUT 30)
endmacro()

# ── Register all auth tests ───────────────────────────────────────────────────
add_auth_test(test_password      password_tests.cpp)
add_auth_test(test_login         login_tests.cpp)
add_auth_test(test_token         token_tests.cpp)
add_auth_test(test_session       session_tests.cpp)
add_auth_test(test_authorization authorization_tests.cpp)
add_auth_test(test_ownership     ownership_tests.cpp)
add_auth_test(test_integration   integration_tests.cpp)
add_auth_test(test_auth_integration auth_integration_test.cpp)
