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

# ─── Third-party headers (Crow, jwt-cpp) ──────────────────────────────────────
# Clone or copy headers into backend/third_party/
# Crow: https://github.com/CrowCpp/Crow
# jwt-cpp: https://github.com/Thalhammer/jwt-cpp
# bcrypt: https://github.com/trusch/libbcrypt

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/config
    ${CMAKE_SOURCE_DIR}/third_party/crow/include
    ${CMAKE_SOURCE_DIR}/third_party/jwt-cpp/include
    ${CMAKE_SOURCE_DIR}/third_party/nlohmann
    ${CMAKE_SOURCE_DIR}/third_party/bcrypt/include
    ${OPENSSL_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

# ─── bcrypt static lib ────────────────────────────────────────────────────────
file(GLOB BCRYPT_SOURCES ${CMAKE_SOURCE_DIR}/third_party/bcrypt/src/*.cpp
                         ${CMAKE_SOURCE_DIR}/third_party/bcrypt/src/*.c)
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
