cmake_minimum_required(VERSION 3.10)

# Set Dirs
set(LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty")
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Binaries")
set(PROJECT_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake/Utils)

set(INSTALL_PREFIX "/etc/BrainGenix/VSDA")

# Setup vcpkg script with CMake (note: should be placed before project() call)
set(CMAKE_TOOLCHAIN_FILE ${LIB_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(BrainGenix-VSDA)

# Add AdaptiveCpp module path (important!)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ACPP_SOURCE_ROOT}/cmake)

find_package(AdaptiveCpp CONFIG REQUIRED)

# Internal Deps
message(STATUS "Finding nlohmann_json Package")
find_package(nlohmann_json CONFIG REQUIRED)

message(STATUS "Setting Up yaml-cpp Library")
find_package(yaml-cpp CONFIG REQUIRED)

message(STATUS "Finding rpclib Package")
find_package(rpclib CONFIG REQUIRED)

message(STATUS "Finding cppzmq Package")
find_package(cppzmq CONFIG REQUIRED)

message(STATUS "Finding bg-common-logger Package")
find_package(bg-common-logger CONFIG REQUIRED)

message(STATUS "Finding AWS SDK Package")
find_package(AWSSDK REQUIRED COMPONENTS s3)

# ADD THIRD PARTY SUBDIR LIBRARIES
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/udp-discovery-cpp)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Automatically glob all source files in Source directory (excluding tests)
file(GLOB_RECURSE SOURCES
  "Source/*.cpp"
  "Source/*.h"
  "Source/*.hpp"
)

# Exclude test files from main sources
list(FILTER SOURCES EXCLUDE REGEX ".*\\.test\\.cpp$")

# Automatically glob all test files
file(GLOB_RECURSE TEST_SOURCES
  "Source/*.test.cpp"
  "Tests/*.cpp"
  "Tests/*.h"
)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Source)



# Create main library from all sources (except tests)
add_library(lib${PROJECT_NAME} STATIC ${SOURCES})
target_compile_features(lib${PROJECT_NAME} PRIVATE cxx_std_17)

# Fix yaml-cpp warning by using correct target
target_link_libraries(lib${PROJECT_NAME} PUBLIC
  bg-common-logger
  nlohmann_json::nlohmann_json
  rpclib::rpc
  yaml-cpp::yaml-cpp
  udp-discovery
  cppzmq
  cppzmq-static
  ${AWSSDK_LINK_LIBRARIES}
)

# Add SYCL compilation to library
add_sycl_to_target(TARGET lib${PROJECT_NAME} SOURCES ${SOURCES})

# Create main executable that links to the library
add_executable(${PROJECT_NAME} Source/Main.cpp)
add_sycl_to_target(TARGET ${PROJECT_NAME} SOURCES Source/Main.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_link_libraries(${PROJECT_NAME} PRIVATE lib${PROJECT_NAME})


# DATA MANAGER TEST (REMOVE LATER)
# ===============================
# Create DataManager test executable
add_executable(DataManagerTest Source/Storage/DataManagerExample.cpp)
target_compile_features(DataManagerTest PRIVATE cxx_std_17)
target_link_libraries(DataManagerTest PRIVATE lib${PROJECT_NAME})
# ===============================

# Set output directory for binaries
set_target_properties(${PROJECT_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR}
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR}
)

# DATA MANAGER TEST (REMOVE LATER)
# ===============================
set_target_properties(DataManagerTest PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR}
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR}
)
# ===============================

# Optional: Set debug level if needed
if(NOT ACPP_DEBUG_LEVEL)
  set(ACPP_DEBUG_LEVEL 2 CACHE STRING "Debug level for AdaptiveCpp")
endif()
add_definitions(-DHIPSYCL_DEBUG_LEVEL=${ACPP_DEBUG_LEVEL})

# Enable testing and find GTest
enable_testing()
find_package(GTest REQUIRED)

# Create test executable if test sources exist
if(TEST_SOURCES)
  message(STATUS "Found test sources: ${TEST_SOURCES}")
  
  add_executable(${PROJECT_NAME}-Test ${TEST_SOURCES})
  target_compile_features(${PROJECT_NAME}-Test PRIVATE cxx_std_17)
  target_include_directories(${PROJECT_NAME}-Test PRIVATE Tests/)
  
  target_link_libraries(${PROJECT_NAME}-Test PRIVATE
    lib${PROJECT_NAME}
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
  )
  
  # Add SYCL compilation for tests
  add_sycl_to_target(TARGET ${PROJECT_NAME}-Test SOURCES ${TEST_SOURCES})
  
  # Discover and register tests
  gtest_discover_tests(${PROJECT_NAME}-Test
    EXTRA_ARGS "--gtest_output=xml:${BIN_DIR}/test_results/"
  )
  
  # Set test output directory to main binaries directory
  set_target_properties(${PROJECT_NAME}-Test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR}
  )
else()
  message(STATUS "No test sources found")
endif()

# Install targets
install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
  COMPONENT runtime
)

install(TARGETS lib${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  COMPONENT runtime
)

# Install test executable if it exists
if(TARGET ${PROJECT_NAME}-Test)
  install(TARGETS ${PROJECT_NAME}-Test
    RUNTIME DESTINATION bin
    COMPONENT tests
  )
endif()