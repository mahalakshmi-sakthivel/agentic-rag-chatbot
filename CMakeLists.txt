cmake_minimum_required(VERSION 3.16)
project(agentic_rag_backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(BUILD_TESTS "Build the Phase 1 unit test target" ON)

find_package(Drogon CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

set(BACKEND_SOURCES
    src/main.cpp
    src/api/router.cpp
    src/api/controllers/HealthController.cpp
    src/api/controllers/QueryController.cpp
    src/api/controllers/UploadController.cpp
)

add_executable(agentic_rag_backend ${BACKEND_SOURCES})
target_include_directories(agentic_rag_backend PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(agentic_rag_backend PRIVATE Drogon::Drogon nlohmann_json::nlohmann_json)

# ---------------------------------------------------------------------------
# Unit tests (Section 14). These target the pure, framework-free validation
# logic in api/models/*.h and common/envelope.h — they do NOT spin up a live
# Drogon server, so they build fast and need no network access. Endpoint
# routing/error-envelope tests over real HTTP are covered separately via the
# curl-based smoke script in examples/ (see README "Testing").
# ---------------------------------------------------------------------------
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)

    add_executable(agentic_rag_backend_tests
        tests/api/test_query_validation.cpp
        tests/api/test_upload_validation.cpp
        tests/api/test_envelope.cpp
    )
    target_include_directories(agentic_rag_backend_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(agentic_rag_backend_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        nlohmann_json::nlohmann_json
    )

    include(GoogleTest)
    gtest_discover_tests(agentic_rag_backend_tests)
endif()
