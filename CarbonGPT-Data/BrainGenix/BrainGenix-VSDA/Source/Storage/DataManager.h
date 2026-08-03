/**
 * @file  DataManager.h
 * @brief In-memory object storage facade around MinIO (S3-compatible).
 *
 * Thread-safety:  All public methods are safe to call from any thread
 *                 provided each DataManager instance is used by one thread at a time.
 *                 Create one instance per thread if you need concurrent access.
 */

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>

#include <BG/Common/Logger/Logger.h>


/**
 * @brief Thin, RAII wrapper for MinIO/S3 interactions.
 *
 * Usage:
 * @code
 * DataManager mgr(logger);
 * mgr.Setup("localhost", 9000, "minioadmin", "minioadmin", false);
 * mgr.UploadObject("images", "user/42/2025-09-01/profile.jpg", buffer);
 * auto data = mgr.DownloadObject("images", "user/42/2025-09-01/profile.jpg");
 * @endcode
 */
class DataManager
{
public:
    /**
     * @brief Construct a DataManager instance.
     * @param _Logger Pointer to the logging subsystem (must outlive this object).
     */
    explicit DataManager(BG::Common::Logger::LoggingSystem* _Logger);

    /// Rule-of-five: non-copyable, non-movable (due to mutex).
    DataManager(const DataManager&)            = delete;
    DataManager& operator=(const DataManager&) = delete;
    DataManager(DataManager&&)                 = delete;
    DataManager& operator=(DataManager&&)      = delete;
    ~DataManager();

    /**
     * @brief Configure the underlying MinIO/S3 client.
     *
     * @param _Host       Server hostname or IP address (e.g. "localhost").
     * @param _Port       TCP port (e.g. 9000).
     * @param _AccessKey  Access key / username.
     * @param _SecretKey  Secret key / password.
     * @param _UseSSL     Whether to connect over HTTPS (true) or HTTP (false).
     *
     * @throw std::runtime_error if the client could not be initialized.
     */
    void Setup(const std::string& _Host,
               unsigned short     _Port,
               const std::string& _AccessKey,
               const std::string& _SecretKey,
               bool               _UseSSL);

    /**
     * @brief Upload an object from memory.
     *
     * @param Bucket Destination bucket name.
     * @param Key    Object key (may contain slashes).
     * @param Data   Raw bytes to upload.
     *
     * @throw std::runtime_error on network or server-side failure.
     */
    void UploadObject(const std::string&                    Bucket,
                      const std::string&                    Key,
                      const std::vector<std::byte>&         Data);

    /**
     * @brief Download an object into memory.
     *
     * @param Bucket Source bucket name.
     * @param Key    Object key.
     * @return       Byte vector containing the object payload.
     *
     * @throw std::runtime_error if the object does not exist or another error occurs.
     */
    std::vector<std::byte> DownloadObject(const std::string& Bucket,
                                          const std::string& Key);

    /**
     * @brief Check if the DataManager has been set up and is ready to use.
     * @return true if Setup() has been called successfully, false otherwise.
     */
    bool IsReady() const;

private:
    class Impl;                       ///< PIMPL idiom to hide MinIO includes
    std::unique_ptr<Impl> m_impl;
    
    BG::Common::Logger::LoggingSystem* Logger_;
    mutable std::mutex SetupMutex_;   ///< Protects initialization
    bool IsSetup_;                    ///< Tracks if Setup() was called successfully
    
    // Helper to safely log (in case Logger_ is null)
    #define SAFE_LOG(Msg, Level) \
        do { \
            if (Logger_) Logger_->Log((Msg), (Level)); \
        } while(0)
};

