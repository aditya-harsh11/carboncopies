#!/bin/bash
set -e

# Get project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
THIRDPARTY_DIR="$PROJECT_ROOT/ThirdParty"
ADAPTIVECPP_INSTALL_DIR="$THIRDPARTY_DIR/adaptivecpp-install"

# Source AdaptiveCpp environment
if [ -f "$ADAPTIVECPP_INSTALL_DIR/setup_env.sh" ]; then
    source "$ADAPTIVECPP_INSTALL_DIR/setup_env.sh"
else
    echo "Error: AdaptiveCpp environment not found. Please run Setup.sh first."
    exit 1
fi

# Configure Build Type
BuildType="Release"
if [ $# -ge 1 ]; then
    BuildType=$1
    echo "[BG BUILD HELPER] - Building in user-specified $BuildType mode."
else
    echo "[BG BUILD HELPER] - Did not get specified build configuration, building in $BuildType mode."
fi

# Enter project root
echo "[BG BUILD HELPER] - Entering Root Repo Directory"
cd "$PROJECT_ROOT"

# Delete Binary If Exists 
echo "[BG BUILD HELPER] - Checking If Binary Exists"
if [ -f "Binaries/BrainGenix-VSDA" ]; then
    echo "[BG BUILD HELPER] - Removing Last Executable"
    rm Binaries/BrainGenix-VSDA
else
    echo "[BG BUILD HELPER] - No Binaries To Clean"
fi

# Check If Build Type Correct
if [ -f "Build/BuildType" ] && grep -q "$BuildType" "Build/BuildType"; then
    echo "[BG BUILD HELPER] - Detected Matching Build Type"
else
    echo "[BG BUILD HELPER] - Build Type Mismatch, Cleaning First"
    rm -rf Build/
fi

# Check If Configuration Needs To Be Run
echo "[BG BUILD HELPER] - Checking If Build Directory Already Exists"
if [ -d "Build" ]; then 
    echo "[BG BUILD HELPER] - Build Directory Already Exists, Skipping Generation"
    cd Build
else 
    # Create Build Dir
    echo "[BG BUILD HELPER] - Creating Build Directory"
    mkdir -p Build

    # Enter Build Dir
    echo "[BG BUILD HELPER] - Entering Build Directory"
    cd Build

    # Configure with CMake
    echo "[BG BUILD HELPER] - Configuring Build Files with AdaptiveCpp"
    cmake .. -D CMAKE_BUILD_TYPE=$BuildType

    # Set Config Var
    echo "[BG BUILD HELPER] - Saving Build Type Configuration Of $BuildType"
    echo "$BuildType" > "BuildType"
fi

# Build Files
echo "[BG BUILD HELPER] - Building, Please Wait. This may take some time"
cmake --build . -j $(nproc)

# Verify the executable was created
if [ -f "$PROJECT_ROOT/Binaries/BrainGenix-VSDA" ]; then
    echo "Build successful! Executable: $PROJECT_ROOT/Binaries/BrainGenix-VSDA"
    echo ""
    echo "To run with specific device:"
    echo "  ACPP_DEVICE=omp ./Binaries/BrainGenix-VSDA    # For CPU"
    if [ -d "/usr/local/cuda" ]; then
        echo "  ACPP_DEVICE=cuda:0 ./Binaries/BrainGenix-VSDA # For NVIDIA GPU"
    fi
    if [ -d "/opt/rocm" ]; then
        echo "  ACPP_DEVICE=hip:0 ./Binaries/BrainGenix-VSDA  # For AMD GPU"
    fi
else
    echo "Error: Executable not found after build!"
    exit 1
fi

# Return to Tools directory
cd "$PROJECT_ROOT/Tools"