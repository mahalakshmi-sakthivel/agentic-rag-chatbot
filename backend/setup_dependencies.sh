#!/usr/bin/env bash
# setup_dependencies.sh
#
# Downloads and sets up all third-party dependencies for Phase 2:
#   - Crow (C++ HTTP framework)
#   - jwt-cpp (header-only JWT library)
#   - nlohmann/json (header-only JSON library)
#   - libbcrypt (bcrypt password hashing)
#   - Catch2 v2 (unit testing, header-only)
#
# Run from: backend/
# Usage: bash setup_dependencies.sh

set -e

THIRD_PARTY="$(pwd)/third_party"
mkdir -p "$THIRD_PARTY"

echo "=== Setting up Phase 2 dependencies ==="
echo "Target: $THIRD_PARTY"

# ── 1. Crow ────────────────────────────────────────────────────────────────────
echo ""
echo "[1/5] Downloading Crow HTTP framework..."
if [ ! -d "$THIRD_PARTY/crow" ]; then
    git clone --depth 1 https://github.com/CrowCpp/Crow.git "$THIRD_PARTY/crow"
    echo "    ✓ Crow cloned"
else
    echo "    ✓ Crow already present"
fi

# ── 2. jwt-cpp ─────────────────────────────────────────────────────────────────
echo ""
echo "[2/5] Downloading jwt-cpp..."
if [ ! -d "$THIRD_PARTY/jwt-cpp" ]; then
    git clone --depth 1 https://github.com/Thalhammer/jwt-cpp.git "$THIRD_PARTY/jwt-cpp"
    echo "    ✓ jwt-cpp cloned"
else
    echo "    ✓ jwt-cpp already present"
fi

# ── 3. nlohmann/json ───────────────────────────────────────────────────────────
echo ""
echo "[3/5] Downloading nlohmann/json..."
mkdir -p "$THIRD_PARTY/nlohmann"
if [ ! -f "$THIRD_PARTY/nlohmann/json.hpp" ]; then
    curl -sSL \
        "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp" \
        -o "$THIRD_PARTY/nlohmann/json.hpp"
    echo "    ✓ nlohmann/json downloaded"
else
    echo "    ✓ nlohmann/json already present"
fi

# ── 4. libbcrypt ───────────────────────────────────────────────────────────────
echo ""
echo "[4/5] Downloading libbcrypt..."
if [ ! -d "$THIRD_PARTY/bcrypt" ]; then
    git clone --depth 1 https://github.com/trusch/libbcrypt.git "$THIRD_PARTY/bcrypt"
    echo "    ✓ libbcrypt cloned"
else
    echo "    ✓ libbcrypt already present"
fi

# ── 5. Catch2 v2 (single-header) ──────────────────────────────────────────────
echo ""
echo "[5/5] Downloading Catch2..."
mkdir -p "$THIRD_PARTY/catch2"
if [ ! -f "$THIRD_PARTY/catch2/catch.hpp" ]; then
    curl -sSL \
        "https://github.com/catchorg/Catch2/releases/download/v2.13.10/catch.hpp" \
        -o "$THIRD_PARTY/catch2/catch.hpp"
    echo "    ✓ Catch2 downloaded"
else
    echo "    ✓ Catch2 already present"
fi

# ── Done ────────────────────────────────────────────────────────────────────────
echo ""
echo "=== All dependencies ready. ==="
echo ""
echo "Next steps:"
echo "  mkdir -p build && cd build"
echo "  cmake .. -DCMAKE_BUILD_TYPE=Debug"
echo "  cmake --build . -j4"
echo "  ctest --output-on-failure"
