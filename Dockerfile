FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive \
    RAW_STORAGE_DIR=/app/data/uploads

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libdrogon-dev \
    libjsoncpp-dev \
    nlohmann-json3-dev \
    uuid-dev \
    libpq-dev \
    libsqlite3-dev \
    libmariadb-dev \
    libgtest-dev \
    libzip-dev \
    libxml2-dev \
    libpoppler-cpp-dev \
    libssl-dev \
    libbrotli-dev \
    libhiredis-dev \
    libyaml-cpp-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN cmake -S . -B build -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j"$(nproc)"

RUN ctest --test-dir build --output-on-failure

EXPOSE 8080

VOLUME ["/app/data/uploads"]

CMD ["./build/agentic_rag_backend"]
