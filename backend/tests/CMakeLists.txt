cmake_minimum_required(VERSION 3.16)

# ── Catch2 (header-only v2) ───────────────────────────────────────────────────
# Download Catch2 single-header into the build tree if not already present.
# The file is placed at: <build>/third_party/catch2/catch.hpp
# Tests include it as: #include <catch2/catch.hpp>
# So the include path must be: <build>/third_party
set(CATCH2_DIR ${CMAKE_BINARY_DIR}/third_party/catch2)
set(CATCH2_SINGLE_INCLUDE ${CATCH2_DIR}/catch.hpp)
if(NOT EXISTS ${CATCH2_SINGLE_INCLUDE})
    message(STATUS "Downloading Catch2 single-header...")
    file(DOWNLOAD
        "https://github.com/catchorg/Catch2/releases/download/v2.13.10/catch.hpp"
        ${CATCH2_SINGLE_INCLUDE}
        SHOW_PROGRESS
    )
endif()

# ── Include search paths for all auth tests ───────────────────────────────────
# - src/           → resolves auth/*, db/*, common/*
# - config/        → resolves config/auth_config.hpp
# - third_party    → resolves <catch2/catch.hpp> via third_party/catch2/catch.hpp
# NOTE: jwt-cpp and bcrypt include dirs come transitively from the 'auth' target.
set(AUTH_TEST_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/config
    ${CMAKE_BINARY_DIR}/third_party
    ${OPENSSL_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

# ── Helper macro to add a test ────────────────────────────────────────────────
macro(add_auth_test test_name source_file)
    add_executable(${test_name} auth/${source_file})
    target_include_directories(${test_name} PRIVATE ${AUTH_TEST_INCLUDE_DIRS})
    # 'auth' is the OBJECT library defined in src/auth/CMakeLists.txt.
    # It transitively provides jwt-cpp, bcrypt, OpenSSL, SQLite3, Drogon, nlohmann_json.
    target_link_libraries(${test_name} PRIVATE auth)
    # Run tests from the source root so relative path "db/migrations" resolves.
    add_test(NAME ${test_name} COMMAND ${test_name}
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
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
