cmake_minimum_required(VERSION 3.16)
project(agentic_rag_backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(BUILD_TESTS "Build the Phase 1 unit test target" ON)

find_package(Drogon CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Phase 1 sources (TECHNICAL_CONTRACT.md Section 23: src/api/ + src/common/).
#
# CONFIGURE_DEPENDS makes CMake re-scan this directory and reconfigure
# automatically whenever a .cpp is added or removed under src/api/ — so
# adding a new controller/model file never silently drops out of the build
# because someone forgot to hand-edit a source list. This is the concrete
# fix for "CMake paths don't match the actual folder structure": the source
# list is now generated FROM the folder structure, not maintained separately
# from it.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE PHASE1_API_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api/*.cpp
)

set(BACKEND_SOURCES
    src/main.cpp
    ${PHASE1_API_SOURCES}
)

add_executable(agentic_rag_backend ${BACKEND_SOURCES})
target_include_directories(agentic_rag_backend PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(agentic_rag_backend PRIVATE Drogon::Drogon nlohmann_json::nlohmann_json)

# ---------------------------------------------------------------------------
# Sibling phase modules (Section 23: src/auth [Phase 2], src/ingestion
# [Phase 3], src/retrieval [Phase 4], src/orchestrator [Phase 5], src/llm
# [Phase 6]).
#
# Phase 1 does not own or build these. This block exists so that once a
# sibling phase's folder + its own CMakeLists.txt appear in the tree, the
# overall build picks it up automatically instead of silently ignoring it
# (which is what "doesn't match the actual folder structure" turns into
# once Phase 2/3 land). If the folder/CMakeLists.txt isn't there yet, this
# loop is a no-op and the build is byte-for-byte what it is today.
#
# ASSUMPTION (flag to Team Lead / Phase 2 & 3 owners for confirmation): each
# sibling phase exposes a library target with the SAME NAME as its folder
# (e.g. src/auth/CMakeLists.txt defines a target called `auth`). If a phase
# uses a different target name, either rename the target or tell Phase 1 the
# actual name so this list can be corrected — this is exactly the kind of
# cross-phase interface assumption Section 27 exists to formalize.
# ---------------------------------------------------------------------------
set(SIBLING_PHASE_MODULES auth ingestion retrieval orchestrator llm)
foreach(_module ${SIBLING_PHASE_MODULES})
    set(_module_cmake ${CMAKE_CURRENT_SOURCE_DIR}/src/${_module}/CMakeLists.txt)
    if(EXISTS ${_module_cmake})
        add_subdirectory(src/${_module})
        if(TARGET ${_module})
            target_link_libraries(agentic_rag_backend PRIVATE ${_module})
        else()
            message(WARNING
                "src/${_module}/CMakeLists.txt exists but defines no '${_module}' "
                "target. Phase 1's root CMakeLists.txt cannot auto-link it — "
                "confirm the intended target name with that phase's owner.")
        endif()
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Unit tests (Section 28). These target the pure, framework-free validation
# logic in src/api/models/*.h and src/common/envelope.h — they do NOT spin
# up a live Drogon server, so they build fast and need no network access.
# Live-endpoint verification is examples/smoke_test.sh, run against a
# running server (see README "Testing").
# ---------------------------------------------------------------------------
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)

    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/api/*.cpp
    )

    # tests/api/test_ingestion_seam.cpp exercises the real
    # UploadController::ingestionHandler static member (Phase 3 seam), whose
    # out-of-line definition lives in UploadController.cpp — pull that one
    # source in explicitly so the test binary links, without dragging in the
    # rest of the controllers (which the other test files don't need).
    add_executable(agentic_rag_backend_tests
        ${TEST_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/api/controllers/UploadController.cpp
    )
    target_include_directories(agentic_rag_backend_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(agentic_rag_backend_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        nlohmann_json::nlohmann_json
        Drogon::Drogon   # common/identity.h includes drogon/HttpRequest.h
    )

    include(GoogleTest)
    gtest_discover_tests(agentic_rag_backend_tests)
endif()

