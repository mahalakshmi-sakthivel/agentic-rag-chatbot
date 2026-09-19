file(GLOB_RECURSE ORCHESTRATOR_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_library(orchestrator STATIC ${ORCHESTRATOR_SOURCES})
target_include_directories(orchestrator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(orchestrator PUBLIC nlohmann_json::nlohmann_json)
