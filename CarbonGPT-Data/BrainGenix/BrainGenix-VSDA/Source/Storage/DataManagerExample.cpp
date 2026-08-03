/**
 * @file DataManagerExample.cpp
 * @brief Example usage of DataManager for MinIO/S3 operations
 */

#include "DataManager.h"
#include <BG/Common/Logger/Logger.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <cassert>

int main() {
    try {
        // Initialize logger
        BG::Common::Logger::LoggingSystem Logger;
        Logger.SetKeepVectorLogs(false);
        
        // Create DataManager instance
        DataManager DataMgr(&Logger);
        
        // Setup MinIO connection (default credentials)
        DataMgr.Setup("localhost", 9000, "minioadmin", "minioadmin", false);
        
        std::cout << "DataManager setup completed successfully!" << std::endl;
        
        // Test data
        std::string TestData = "Hello, MinIO! This is a test object from BrainGenix VSDA.";
        std::vector<std::byte> TestBytes;
        TestBytes.reserve(TestData.size());
        for (char c : TestData) {
            TestBytes.push_back(static_cast<std::byte>(c));
        }
        
        std::string BucketName = "test-bucket";
        std::string ObjectKey = "test-object.txt";
        
        // Upload test
        std::cout << "Uploading test object..." << std::endl;
        DataMgr.UploadObject(BucketName, ObjectKey, TestBytes);
        std::cout << "Upload successful!" << std::endl;
        
        // Download test
        std::cout << "Downloading test object..." << std::endl;
        auto DownloadedData = DataMgr.DownloadObject(BucketName, ObjectKey);
        std::cout << "Download successful!" << std::endl;
        
        // Verify data integrity
        std::string DownloadedString;
        DownloadedString.reserve(DownloadedData.size());
        for (std::byte b : DownloadedData) {
            DownloadedString.push_back(static_cast<char>(b));
        }
        assert(DownloadedString == TestData);
        std::cout << "Data integrity verified!" << std::endl;
        
        // Test with binary data
        std::cout << "\nTesting with binary data..." << std::endl;
        std::vector<std::byte> BinaryData = {
            std::byte{0x48}, std::byte{0x65}, std::byte{0x6C}, std::byte{0x6C}, std::byte{0x6F},
            std::byte{0x00}, std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0xFF}
        };
        
        DataMgr.UploadObject(BucketName, "binary-test.bin", BinaryData);
        auto DownloadedBinary = DataMgr.DownloadObject(BucketName, "binary-test.bin");
        
        assert(DownloadedBinary == BinaryData);
        std::cout << "Binary data test successful!" << std::endl;
        
        // Test thread safety with multiple uploads
        std::cout << "\nTesting thread safety with multiple uploads..." << std::endl;
        std::vector<std::thread> Threads;
        
        for (int i = 0; i < 5; ++i) {
            Threads.emplace_back([&DataMgr, BucketName, i]() {
                std::string ThreadData = "Thread " + std::to_string(i) + " data";
                std::vector<std::byte> ThreadBytes;
                ThreadBytes.reserve(ThreadData.size());
                for (char c : ThreadData) {
                    ThreadBytes.push_back(static_cast<std::byte>(c));
                }
                std::string ThreadKey = "thread-test-" + std::to_string(i) + ".txt";
                
                try {
                    DataMgr.UploadObject(BucketName, ThreadKey, ThreadBytes);
                    std::cout << "Thread " << i << " upload successful" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " upload failed: " << e.what() << std::endl;
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& Thread : Threads) {
            Thread.join();
        }
        
        std::cout << "All thread safety tests completed!" << std::endl;
        
        std::cout << "\n✅ All tests passed! DataManager is working correctly." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}