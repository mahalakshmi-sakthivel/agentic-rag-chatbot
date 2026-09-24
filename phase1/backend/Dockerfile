# syntax=docker/dockerfile:1
#
# Phase 1 — C++ backend Dockerfile.
#
# Two stages:
#   1. build   — full Ubuntu 24.04 + build toolchain, compiles the server
#                and the GoogleTest suite, and RUNS the unit tests as part
#                of `docker build` (the image build fails if a test fails).
#   2. runtime — slim Ubuntu 24.04 with only the shared libraries the
#                compiled binary actually needs (resolved via `ldd` against
#                the real binary — see DOCKER_VERIFICATION.md), running as
#                a non-root user.
#
# Nothing about the Phase 1 contract, APIs, schemas, or interfaces changes
# here — this only packages the existing build (see README.md "1. Install
# dependencies" / "3. Build" / "5. Testing", which this Dockerfile mirrors
# step-for-step).

########################################
# Stage 1: build
########################################
FROM ubuntu:24.04 AS build

# Same Drogon build pinned in .github/workflows/ci.yml (DROGON_APT_VERSION)
# and recorded in BUILD_VERIFICATION.md. Overridable via --build-arg if the
# team deliberately bumps it; fails loudly if that exact version is no
# longer in Ubuntu's archive, rather than silently building against a
# different Drogon than what was verified.
ARG DROGON_APT_VERSION=1.8.7+ds-1.1build1

ENV DEBIAN_FRONTEND=noninteractive

# Exact apt package list from README.md section 1 ("Ubuntu/Debian (apt) —
# fastest path, what this was actually tested with").
RUN apt-get update && apt-get install -y --no-install-recommends \
      "libdrogon-dev=${DROGON_APT_VERSION}" \
      nlohmann-json3-dev libgtest-dev \
      libjsoncpp-dev libpq-dev libsqlite3-dev libmariadb-dev-compat \
      libhiredis-dev libyaml-cpp-dev cmake build-essential ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Copy only what the build needs first, so source-only edits don't bust the
# apt-get layer cache above.
COPY CMakeLists.txt CMakePresets.json ./
COPY src ./src
COPY tests ./tests

# Same commands as README.md section 3 ("Build"), non-preset form.
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON \
    && cmake --build build -j"$(nproc)"

# README.md section 5 ("Testing") — the 40 GoogleTest unit tests. This is
# what "run the phase's required tests inside the container" means for the
# pure-logic suite: it runs as a normal build step, so a failing test fails
# `docker build` and no image is produced. (The live-server smoke test in
# examples/smoke_test.sh needs a *running* server + curl/jq against it — see
# "Docker" in README.md for how to run that against this same image.)
RUN cd build && ctest --output-on-failure

########################################
# Stage 2: runtime
########################################
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Runtime shared libraries only — no compiler, no -dev/-headers packages.
# This list is exactly what `ldd build/agentic_rag_backend` reports for
# Phase 1's own direct link dependencies (their transitive deps, e.g.
# libssl/libgnutls/libkrb5, are pulled in automatically by apt); curl is
# for the HEALTHCHECK below.
RUN apt-get update && apt-get install -y --no-install-recommends \
      libdrogon1t64 libtrantor1 libjsoncpp25 libyaml-cpp0.8 libuuid1 \
      libbrotli1 libpq5 libmariadb3 libsqlite3-0 libhiredis1.1.0 \
      ca-certificates curl \
    && rm -rf /var/lib/apt/lists/*

# Non-root runtime user (Section 19.1 discipline extends naturally to not
# running the process itself as root in a shared/staging/production image).
RUN groupadd --system app && useradd --system --gid app --no-create-home app

WORKDIR /app
COPY --from=build /src/build/agentic_rag_backend ./agentic_rag_backend

# Matches FILE_STORAGE_PATH's role as a pre-agreed convention (config.h) —
# created and owned by the non-root user up front so a future Phase 3
# module mounted at this path can write to it.
RUN mkdir -p /app/storage/uploads && chown -R app:app /app

USER app

# Defaults mirror .env.example. Override with `docker run -e` / --env-file
# / compose — never bake real secrets or non-default values into the image.
# ENVIRONMENT defaults to production here (unlike the .env.example local-dev
# default) so an operator must opt into AUTH_DEV_BYPASS=true explicitly
# rather than a container silently starting with it on.
ENV BACKEND_PORT=8080 \
    FILE_STORAGE_PATH=/app/storage/uploads \
    LOG_LEVEL=info \
    ENVIRONMENT=production \
    MAX_UPLOAD_SIZE_MB=50 \
    AUTH_DEV_BYPASS=false

EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD curl -sf "http://localhost:${BACKEND_PORT}/v1/health" || exit 1

ENTRYPOINT ["./agentic_rag_backend"]
