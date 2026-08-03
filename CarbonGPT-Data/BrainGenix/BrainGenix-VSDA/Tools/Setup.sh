#!/bin/bash
set -e

# Detect if running as root
SUDO_PREFIX=""
echo "Detecting If Script Running As root"
if [ "$EUID" -ne 0 ]; then
    echo "Detected Install Script Is Not Running As Root, Adding 'sudo' Prefix To Commands"
    SUDO_PREFIX="sudo"
else
    echo "Command Running As Root, Running Commands Without 'sudo'"
fi

# Get project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
THIRDPARTY_DIR="$PROJECT_ROOT/ThirdParty"
TOOLS_DIR="$PROJECT_ROOT/Tools"
BINARIES_DIR="$PROJECT_ROOT/Binaries"

# Create necessary directories
mkdir -p "$THIRDPARTY_DIR" "$BINARIES_DIR"

# Setting up package manager depending on Linux Distro
DISTRO=$(cat /etc/os-release | grep -w ID | awk -F'=' '{print $2}' | tr -d '"')
PACKAGE_MANAGER_INSTALL_CMD=""
PACKAGE_MANAGER_UPDATE_CMD=""

case $DISTRO in
    "arch") PACKAGE_MANAGER_INSTALL_CMD="pacman -S --noconfirm";
            PACKAGE_MANAGER_UPDATE_CMD="pacman -Sy --noconfirm";;
    "ubuntu") PACKAGE_MANAGER_INSTALL_CMD="apt install -y";
              PACKAGE_MANAGER_UPDATE_CMD="apt update -y";;
    "fedora") PACKAGE_MANAGER_INSTALL_CMD="dnf install -y";
              PACKAGE_MANAGER_UPDATE_CMD="dnf update -y";;
    *) PACKAGE_MANAGER_INSTALL_CMD="apt install -y";
       PACKAGE_MANAGER_UPDATE_CMD="apt update -y";;
esac

# Detect version
Var=$(lsb_release -r 2>/dev/null || echo "Release: 0.0")
NumOnly=$(cut -f2 <<< "$Var")

# Define Python dependencies based on distro
PYTHON_DEPS=""
case $DISTRO in
    "arch") PYTHON_DEPS="python" ;;  # Arch includes venv in the python package
    "ubuntu") PYTHON_DEPS="python3 python3-venv" ;;
    "fedora") PYTHON_DEPS="python3 python3-virtualenv" ;;
    *) PYTHON_DEPS="python3 python3-venv" ;;
esac

# Setup, update package registry
echo "Setting Up Packages Needed For Compilation"
$SUDO_PREFIX $PACKAGE_MANAGER_UPDATE_CMD

# Install Compiler Tools
COMPILER_DEPS="git wget cmake"

if [ "$DISTRO" = "arch" ]; then
    COMPILER_DEPS="$COMPILER_DEPS gcc"
else
    COMPILER_DEPS="$COMPILER_DEPS g++"   
fi

# vcpkg Dependencies
VCPKG_DEPS="curl zip unzip tar pkg-config autoconf flex bison"

# Vulkan Dependencies
if [ "$DISTRO" = "arch" ]; then
    VULKAN_DEPS="vulkan-devel"
elif [ "$DISTRO" = "fedora" ]; then
    VULKAN_DEPS="libvulkan-dev vulkan-validationlayers-dev vulkan-tools libxcb-xfixes0-dev libx11-dev libxrandr-dev"  
else
    # Check if it's ubuntu 24.04 since they changed the library name
    if [ "$NumOnly" = "24.04" ]; then
        VULKAN_DEPS="libvulkan-dev vulkan-utility-libraries-dev vulkan-tools libxcb-xfixes0-dev libx11-dev libxrandr-dev"  
    else
        VULKAN_DEPS="libvulkan-dev vulkan-validationlayers-dev vulkan-tools libxcb-xfixes0-dev libx11-dev libxrandr-dev"  
    fi
fi

# Install Everything (add PYTHON_DEPS)
INSTALL_COMMAND="$SUDO_PREFIX $PACKAGE_MANAGER_INSTALL_CMD $VCPKG_DEPS $COMPILER_DEPS $VULKAN_DEPS $PYTHON_DEPS"
echo "Running Install Command: $INSTALL_COMMAND"
$INSTALL_COMMAND

# # Create Python virtual environment and install igneous-pipeline
# echo "Creating Python virtual environment..."
# python3 -m venv "$PROJECT_ROOT/venv" || exit 1
# echo "Installing igneous-pipeline in the virtual environment..."
# "$PROJECT_ROOT/venv/bin/pip" install igneous-pipeline || exit 1

# Update Submodules
echo "Updating Submodules"
git submodule update --init

# Setup vcpkg
echo "Setting Up vcpkg"
cd "$THIRDPARTY_DIR/vcpkg"
./bootstrap-vcpkg.sh -disableMetrics
cd "$PROJECT_ROOT"

# Setup AdaptiveCpp
echo "Setting up AdaptiveCpp..."
ADAPTIVECPP_DIR="$THIRDPARTY_DIR/AdaptiveCpp"
ADAPTIVECPP_INSTALL_DIR="$THIRDPARTY_DIR/adaptivecpp-install"
ADAPTIVECPP_BUILD_DIR="$THIRDPARTY_DIR/build-adaptivecpp"

mkdir -p "$ADAPTIVECPP_INSTALL_DIR" "$ADAPTIVECPP_BUILD_DIR"

# Clone or update AdaptiveCpp
if [ ! -d "$ADAPTIVECPP_DIR" ]; then
    git clone https://github.com/AdaptiveCpp/AdaptiveCpp "$ADAPTIVECPP_DIR"
else
    cd "$ADAPTIVECPP_DIR"
    git pull
    cd "$PROJECT_ROOT"
fi

# Install AdaptiveCpp dependencies
echo "Installing AdaptiveCpp dependencies..."

install_adaptivecpp_deps() {
    if command -v apt-get >/dev/null 2>&1; then
        # Ubuntu/Debian
        wget https://apt.llvm.org/llvm.sh
        chmod +x llvm.sh
        $SUDO_PREFIX ./llvm.sh 18
        $SUDO_PREFIX apt-get update
        $SUDO_PREFIX apt-get install -y git cmake ninja-build build-essential \
            libboost-all-dev \
            libclang-18-dev clang-18 llvm-18-dev llvm-18-runtime \
            libffi-dev libedit-dev libxml2-dev zlib1g-dev libzstd-dev
        rm -f llvm.sh
        
    elif command -v dnf >/dev/null 2>&1; then
        # Fedora
        $SUDO_PREFIX dnf install -y git cmake ninja-build gcc-c++ boost-devel llvm-devel
    elif command -v pacman >/dev/null 2>&1; then
        # Arch
        $SUDO_PREFIX pacman -S --noconfirm git cmake ninja gcc boost llvm clang
    else
        echo "Warning: Could not automatically install dependencies. Please ensure you have:"
        echo "  - git, cmake, ninja-build, build-essential"
        echo "  - libboost-all-dev, libclang-18-dev, llvm-18-dev"
        echo "  - libffi-dev, libedit-dev, libxml2-dev, zlib1g-dev, libzstd-dev"
        read -p "Press enter to continue once dependencies are installed..."
    fi
}

install_adaptivecpp_deps

# Build AdaptiveCpp
echo "Building AdaptiveCpp..."
cd "$ADAPTIVECPP_BUILD_DIR"

# Detect available backends
BACKEND_OPTIONS=""
if [ -d "/opt/rocm" ]; then
    echo "ROCm detected at /opt/rocm - enabling HIP backend"
    BACKEND_OPTIONS="$BACKEND_OPTIONS -DACPP_ENABLE_HIP_BACKEND=ON"
else
    echo "ROCm not found - HIP backend will be disabled"
    BACKEND_OPTIONS="$BACKEND_OPTIONS -DACPP_ENABLE_HIP_BACKEND=OFF"
fi

# Check common CUDA installation locations
if [ -d "/usr/local/cuda" ]; then
    CUDA_ROOT="/usr/local/cuda"
    CUDA_DETECTED=true
elif command -v nvcc >/dev/null 2>&1; then
    # If nvcc is in PATH, get its directory and find the root
    NVCC_PATH=$(command -v nvcc)
    CUDA_ROOT=$(dirname "$(dirname "$NVCC_PATH")")
    CUDA_DETECTED=true
elif [ -d "/opt/cuda" ]; then
    CUDA_ROOT="/opt/cuda"
    CUDA_DETECTED=true
elif [ -d "/usr/lib/cuda" ]; then
    CUDA_ROOT="/usr/lib/cuda"
    CUDA_DETECTED=true
fi

if [ "$CUDA_DETECTED" = true ]; then
    echo "CUDA detected at $CUDA_ROOT - enabling CUDA backend"
    BACKEND_OPTIONS="$BACKEND_OPTIONS -DACPP_ENABLE_CUDA_BACKEND=ON -DCUDA_TOOLKIT_ROOT_DIR=$CUDA_ROOT"
else
    echo "CUDA not found - CUDA backend will be disabled"
    BACKEND_OPTIONS="$BACKEND_OPTIONS -DACPP_ENABLE_CUDA_BACKEND=OFF"
fi

# Find LLVM cmake directory
LLVM_CMAKE_DIR=$(llvm-config --cmakedir 2>/dev/null || echo "/usr/lib/llvm-18/lib/cmake/llvm")

# Configure and build AdaptiveCpp
cmake "$ADAPTIVECPP_DIR" \
    -DCMAKE_INSTALL_PREFIX="$ADAPTIVECPP_INSTALL_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DACPP_COMPILER_FEATURE_PROFILE=full \
    -DLLVM_DIR="$LLVM_CMAKE_DIR" \
    -DWITH_OPENCL_BACKEND=OFF \
    -DWITH_LEVEL_ZERO_BACKEND=OFF \
    $BACKEND_OPTIONS

make -j$(nproc)
make install

# Create environment setup script
cat > "$ADAPTIVECPP_INSTALL_DIR/setup_env.sh" << EOF
#!/bin/bash
export PATH="$ADAPTIVECPP_INSTALL_DIR/bin:\$PATH"
export LD_LIBRARY_PATH="$ADAPTIVECPP_INSTALL_DIR/lib:\$LD_LIBRARY_PATH"
export ACPP_INSTALL_PREFIX="$ADAPTIVECPP_INSTALL_DIR"
export CMAKE_PREFIX_PATH="$ADAPTIVECPP_INSTALL_DIR/lib/cmake/AdaptiveCpp:\$CMAKE_PREFIX_PATH"
EOF

chmod +x "$ADAPTIVECPP_INSTALL_DIR/setup_env.sh"

echo "Setup completed successfully!"
echo "AdaptiveCpp installed to: $ADAPTIVECPP_INSTALL_DIR"
echo "You can now run Build.sh to compile BrainGenix-VSDA"