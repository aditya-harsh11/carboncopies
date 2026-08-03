# DataManager - S3-Compatible Storage for BrainGenix VSDA

## Overview

The `DataManager` class provides a thread-safe, in-memory object storage facade around MinIO (S3-compatible storage). It allows you to upload and download objects directly from memory without requiring files on disk.

## Features

✅ **Memory-based operations**: Upload/download objects directly from RAM  
✅ **Thread-safe**: Safe for concurrent access from multiple threads  
✅ **Automatic bucket creation**: Creates buckets automatically if they don't exist  
✅ **Comprehensive logging**: Integrated with BrainGenix logging system  
✅ **Error handling**: Robust error handling with detailed error messages  
✅ **PIMPL pattern**: Clean interface hiding AWS SDK implementation details  

## Quick Start

### 1. Start MinIO Server

```bash
# Using the provided script
./Tools/MinIOServer.sh start

# Or manually
./minio server minio-data --console-address ":9001"
```

### 2. Basic Usage

```cpp
#include "DataManager.h"
#include <BG/Common/Logger/Logger.h>

// Initialize logger
BG::Common::Logger::LoggingSystem Logger;
Logger.SetKeepVectorLogs(false);

// Create DataManager instance
BG::Storage::DataManager DataMgr(&Logger);

// Setup MinIO connection
DataMgr.Setup("localhost", 9000, "minioadmin", "minioadmin", false);

// Upload data from memory
std::string TestData = "Hello, MinIO!";
std::vector<std::byte> TestBytes;
for (char c : TestData) {
    TestBytes.push_back(static_cast<std::byte>(c));
}

DataMgr.UploadObject("my-bucket", "test-object.txt", TestBytes);

// Download data to memory
auto DownloadedData = DataMgr.DownloadObject("my-bucket", "test-object.txt");
```

## API Reference

### Constructor

```cpp
DataManager(BG::Common::Logger::LoggingSystem* _Logger)
```

Creates a new DataManager instance. The logger must outlive the DataManager instance.

### Setup

```cpp
void Setup(const std::string& _Host,
           unsigned short     _Port,
           const std::string& _AccessKey,
           const std::string& _SecretKey,
           bool               _UseSSL)
```

Configures the MinIO/S3 client connection.

- `_Host`: Server hostname or IP address (e.g., "localhost")
- `_Port`: TCP port (e.g., 9000)
- `_AccessKey`: Access key / username
- `_SecretKey`: Secret key / password
- `_UseSSL`: Whether to connect over HTTPS (true) or HTTP (false)

### Upload Object

```cpp
void UploadObject(const std::string&                    Bucket,
                  const std::string&                    Key,
                  const std::vector<std::byte>&         Data)
```

Uploads an object from memory to S3.

- `Bucket`: Destination bucket name (will be created if it doesn't exist)
- `Key`: Object key (may contain slashes for hierarchical structure)
- `Data`: Raw bytes to upload

### Download Object

```cpp
std::vector<std::byte> DownloadObject(const std::string& Bucket,
                                      const std::string& Key)
```

Downloads an object from S3 into memory.

- `Bucket`: Source bucket name
- `Key`: Object key
- Returns: Byte vector containing the object payload

### Check Ready Status

```cpp
bool IsReady() const
```

Returns `true` if the DataManager has been set up and is ready to use.

## Thread Safety

The DataManager is designed to be thread-safe:

- **Single instance per thread**: Each DataManager instance should be used by one thread at a time
- **Concurrent access**: If you need concurrent access, create one DataManager instance per thread
- **Internal synchronization**: The class uses internal mutexes to protect critical sections

## Error Handling

All methods throw `std::runtime_error` with descriptive error messages on failure:

```cpp
try {
    DataMgr.UploadObject("bucket", "key", data);
} catch (const std::runtime_error& e) {
    std::cerr << "Upload failed: " << e.what() << std::endl;
}
```

## Logging

The DataManager integrates with the BrainGenix logging system:

- **Log level 0**: Critical errors (upload/download failures)
- **Log level 4**: Important events (setup completion, successful operations)
- **Log level 5**: General information (instance creation, operation start)
- **Log level 6**: Warnings (already set up, etc.)

## MinIO Server Management

Use the provided script to manage the MinIO server:

```bash
# Start server
./Tools/MinIOServer.sh start

# Check status
./Tools/MinIOServer.sh status

# Stop server
./Tools/MinIOServer.sh stop

# Test connection
./Tools/MinIOServer.sh test

# Clean all data
./Tools/MinIOServer.sh clean
```

## Testing

Run the included test to verify everything works:

```bash
# Build and run test
cd Build
make DataManagerTest
./Binaries/DataManagerTest
```

The test verifies:
- Basic upload/download functionality
- Data integrity
- Binary data handling
- Thread safety with concurrent uploads

## Dependencies

- AWS SDK for C++ (S3 components)
- BrainGenix Common Logger
- C++17 or later

## Default MinIO Credentials

- **Access Key**: `minioadmin`
- **Secret Key**: `minioadmin`
- **S3 API**: `http://localhost:9000`
- **Web Console**: `http://localhost:9001`

## Example: Thread-Safe Concurrent Uploads

```cpp
std::vector<std::thread> Threads;

for (int i = 0; i < 5; ++i) {
    Threads.emplace_back([&DataMgr, i]() {
        std::string ThreadData = "Thread " + std::to_string(i) + " data";
        std::vector<std::byte> ThreadBytes;
        for (char c : ThreadData) {
            ThreadBytes.push_back(static_cast<std::byte>(c));
        }
        
        try {
            DataMgr.UploadObject("test-bucket", 
                               "thread-" + std::to_string(i) + ".txt", 
                               ThreadBytes);
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
```

## Troubleshooting

### Connection Issues
- Ensure MinIO server is running: `./Tools/MinIOServer.sh status`
- Check firewall settings for ports 9000 and 9001
- Verify credentials are correct

### Build Issues
- Ensure AWS SDK is properly installed via vcpkg
- Check that all dependencies are available

### Runtime Issues
- Check logs for detailed error messages
- Verify MinIO server is accessible from your application
- Ensure sufficient disk space for MinIO data directory
