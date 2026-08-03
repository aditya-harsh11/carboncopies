#!/bin/bash

# Navigate to project root
cd "$(dirname "$0")/.." || exit 1

# Check if test executable exists
if [ ! -f "./Binaries/BrainGenix-VSDA-Test" ]; then
    echo "Error: Test executable not found. Please build first with: ./Tools/Build.sh"
    exit 1
fi

# Run the tests
./Binaries/BrainGenix-VSDA-Test

if [ $? -eq 0 ]; then
    echo "Tests completed successfully."
else
    echo "Tests failed with exit code: $?"
    exit 1
fi