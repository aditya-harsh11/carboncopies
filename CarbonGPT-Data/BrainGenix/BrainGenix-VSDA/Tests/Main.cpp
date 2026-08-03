#include <gtest/gtest.h>
#include <sycl/sycl.hpp>

int main(int argc, char** argv) {
    // Initialize Google Test first to get proper logging
    ::testing::InitGoogleTest(&argc, argv);
    
    // Print SYCL device info for debugging
    try {
        sycl::device dev;
        testing::internal::CaptureStdout();
        GTEST_LOG_(INFO) << "Running tests on: " << dev.get_info<sycl::info::device::name>() << std::endl;
        testing::internal::GetCapturedStdout();
    } catch (const sycl::exception& e) {
        GTEST_LOG_(ERROR) << "SYCL device error: " << e.what();
    }
    
    return RUN_ALL_TESTS();
}