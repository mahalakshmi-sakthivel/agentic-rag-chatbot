find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

pkg_check_modules(LIBZIP REQUIRED libzip)
pkg_check_modules(LIBXML2 REQUIRED libxml-2.0)
pkg_check_modules(POPPLER REQUIRED poppler-cpp)

add_library(ingestion
    handoff/Phase4ChunkHandoff.h
    IngestionHandler.cpp
    repository/DocumentRepository.cpp
    validation/FileValidator.cpp
    parsers/CsvParser.cpp
    parsers/JsonParser.cpp
    parsers/XlsxParser.cpp
    parsers/PdfParser.cpp
    cleaning/TextCleaner.cpp
    chunking/TextChunker.cpp
    metadata/MetadataExtractor.cpp
    metadata/ChunkIdGenerator.cpp
    status/IngestionStatus.cpp
    status/IngestionStatus.h
    repository/PersistentRawFileRepository.cpp
    repository/PersistentRawFileRepository.h
    parsers/ParserFactory.cpp
    parsers/ParserFactory.h
)

target_include_directories(ingestion
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${LIBZIP_INCLUDE_DIRS}
        ${LIBXML2_INCLUDE_DIRS}
        ${POPPLER_INCLUDE_DIRS}
        
)

target_link_libraries(ingestion
    PUBLIC
        Drogon::Drogon
        nlohmann_json::nlohmann_json
        OpenSSL::Crypto
        ${LIBZIP_LIBRARIES}
        ${LIBXML2_LIBRARIES}
        ${POPPLER_LIBRARIES}
)
