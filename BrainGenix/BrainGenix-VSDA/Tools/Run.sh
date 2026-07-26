#!/bin/bash
# Navigate to project root
cd "$(dirname "$0")/.." || exit 1
# Check if executable exists
if [ ! -f "./Binaries/BrainGenix-VSDA" ]; then
    echo "Error: Executable not found. Please build first with: ./Tools/Build.sh"
    exit 1
fi
# Determine mode based on argument
if [ "$1" = "omp" ] || [ "$1" = "cpu" ]; then
    echo "CPU-only mode requested"
    MODE="--cpu"
    # Set environment variables for CPU backend
    export ACPP_DEVICE="omp"
    export SYCL_DEVICE_FILTER="opencl:cpu"
    export ONEAPI_DEVICE_SELECTOR="opencl:cpu"
else
    echo "GPU-preferred mode (automatic fallback to CPU if no GPUs)"
    MODE=""
    # Let SYCL automatically choose the best available devices
    unset ACPP_DEVICE
    unset SYCL_DEVICE_FILTER
    unset ONEAPI_DEVICE_SELECTOR
fi
echo "Running with mode: $MODE"
echo "Environment variables:"
env | grep -E "(ACPP_DEVICE|SYCL_DEVICE_FILTER|ONEAPI_DEVICE_SELECTOR)" || echo "No specific device environment variables set"
echo "============================="
# Run the stress test, forwarding all arguments
./Binaries/BrainGenix-VSDA $MODE "$@"
if [ $? -eq 0 ]; then
    echo "Program invoked clean exit."
else
    echo "Program failed with exit code: $?"
    exit 1
fi
