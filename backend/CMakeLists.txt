cmake_minimum_required(VERSION 3.16)
project(AgenticRAGChatbot VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Output directories ────────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ─── Compiler flags ────────────────────────────────────────────────────────────
if (MSVC)
    add_compile_options(/W4 /WX- /MP)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O2)
endif()

# ─── Find packages ────────────────────────────────────────────────────────────
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)

# ─── Fetch Dependencies ─────────────────────────────────────────────────────────
include(FetchContent)

# Crow
FetchContent_Declare(
  Crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
  GIT_TAG        v1.2.0
)
FetchContent_MakeAvailable(Crow)

# jwt-cpp
FetchContent_Declare(
  jwt-cpp
  GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
  GIT_TAG        v0.7.0
)
# jwt-cpp uses JWT_BUILD_EXAMPLES etc. disable them
set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JWT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(jwt-cpp)

# nlohmann_json
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# bcrypt
FetchContent_Declare(
  bcrypt
  GIT_REPOSITORY https://github.com/trusch/libbcrypt.git
  GIT_TAG        master
)
FetchContent_GetProperties(bcrypt)
if(NOT bcrypt_POPULATED)
  FetchContent_Populate(bcrypt)
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/config
    ${OPENSSL_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
    ${bcrypt_SOURCE_DIR}/include
)

# ─── bcrypt static lib ────────────────────────────────────────────────────────
file(GLOB BCRYPT_SOURCES ${bcrypt_SOURCE_DIR}/src/*.cpp
                         ${bcrypt_SOURCE_DIR}/src/*.c)
if(BCRYPT_SOURCES)
    add_library(bcrypt_lib STATIC ${BCRYPT_SOURCES})
endif()

# ─── Auth library ─────────────────────────────────────────────────────────────
file(GLOB_RECURSE AUTH_SOURCES
    src/auth/*.cpp
    src/middleware/*.cpp
    src/db/*.cpp
)

add_library(auth_lib STATIC ${AUTH_SOURCES})
target_link_libraries(auth_lib
    PUBLIC
        Threads::Threads
        OpenSSL::SSL
        OpenSSL::Crypto
        ${SQLite3_LIBRARIES}
        $<$<TARGET_EXISTS:bcrypt_lib>:bcrypt_lib>
)

# ─── Main server executable ───────────────────────────────────────────────────
add_executable(chatbot_server src/main.cpp)
target_link_libraries(chatbot_server PRIVATE auth_lib)

# ─── Tests ────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
