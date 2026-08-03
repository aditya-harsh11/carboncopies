/**
 * @file  DataManager.cpp
 * @brief Implementation of the DataManager class for MinIO/S3 storage.
 */

#include "DataManager.h"
#include <stdexcept>
#include <sstream>
#include <iostream>

// AWS SDK includes (MinIO is S3-compatible)
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/HeadBucketRequest.h>
#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>


/**
 * @brief PIMPL implementation hiding AWS SDK details
 */
class DataManager::Impl {
public:
    Impl() : IsInitialized_(false) {}
    
    ~Impl() {
        if (IsInitialized_) {
            Aws::ShutdownAPI(AwsOptions_);
        }
    }
    
    void Initialize(const std::string& _Host,
                   unsigned short _Port,
                   const std::string& _AccessKey,
                   const std::string& _SecretKey,
                   bool _UseSSL) {
        
        if (IsInitialized_) {
            return; // Already initialized
        }
        
        // Initialize AWS SDK
        AwsOptions_.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Off;
        Aws::InitAPI(AwsOptions_);
        
        // Configure client
        Aws::Client::ClientConfiguration Config;
        Config.endpointOverride = (_UseSSL ? "https://" : "http://") + _Host + ":" + std::to_string(_Port); // change for future
        Config.scheme = _UseSSL ? Aws::Http::Scheme::HTTPS : Aws::Http::Scheme::HTTP;
        Config.verifySSL = false; // For MinIO self-signed certificates
        Config.region = "us-east-1"; // MinIO doesn't care about region
        
        // Set up credentials
        Aws::Auth::AWSCredentials Credentials(_AccessKey.c_str(), _SecretKey.c_str());
        
        // Create S3 client
        S3Client_ = std::make_unique<Aws::S3::S3Client>(
            Credentials,
            Config,
            Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
            false // Use virtual addressing
        );
        
        IsInitialized_ = true;
    }
    
    void UploadObject(const std::string& Bucket,
                     const std::string& Key,
                     const std::vector<std::byte>& Data) {
        
        if (!IsInitialized_ || !S3Client_) {
            throw std::runtime_error("DataManager not properly initialized. Call Setup() first.");
        }
        
        // Ensure bucket exists
        EnsureBucketExists(Bucket);
        
        // Create stream from byte data
        auto StreamBuf = std::make_shared<Aws::Utils::Stream::PreallocatedStreamBuf>(
            reinterpret_cast<unsigned char*>(const_cast<std::byte*>(Data.data())),
            Data.size()
        );
        auto InputStream = std::make_shared<std::iostream>(StreamBuf.get());
        
        // Create upload request
        Aws::S3::Model::PutObjectRequest Request;
        Request.SetBucket(Bucket.c_str());
        Request.SetKey(Key.c_str());
        Request.SetBody(InputStream);
        Request.SetContentLength(Data.size());
        Request.SetContentType("application/octet-stream");
        
        // Perform upload
        auto Outcome = S3Client_->PutObject(Request);
        
        if (!Outcome.IsSuccess()) {
            auto Error = Outcome.GetError();
            throw std::runtime_error("Failed to upload object '" + Key + "' to bucket '" + Bucket + "': " + 
                                   Error.GetMessage().c_str());
        }
    }
    
    std::vector<std::byte> DownloadObject(const std::string& Bucket,
                                         const std::string& Key) {
        
        if (!IsInitialized_ || !S3Client_) {
            throw std::runtime_error("DataManager not properly initialized. Call Setup() first.");
        }
        
        // Create download request
        Aws::S3::Model::GetObjectRequest Request;
        Request.SetBucket(Bucket.c_str());
        Request.SetKey(Key.c_str());
        
        // Perform download
        auto Outcome = S3Client_->GetObject(Request);
        
        if (!Outcome.IsSuccess()) {
            auto Error = Outcome.GetError();
            throw std::runtime_error("Failed to download object '" + Key + "' from bucket '" + Bucket + "': " + 
                                   Error.GetMessage().c_str());
        }
        
        // Read data from stream
        auto& Stream = Outcome.GetResult().GetBody();
        std::vector<std::byte> Data;
        
        // Get content length
        auto ContentLength = Outcome.GetResult().GetContentLength();
        if (ContentLength > 0) {
            Data.reserve(ContentLength);
        }
        
        // Read data in chunks
        constexpr size_t ChunkSize = 8192;
        char Buffer[ChunkSize];
        
        while (Stream.good()) {
            Stream.read(Buffer, ChunkSize);
            auto BytesRead = Stream.gcount();
            if (BytesRead > 0) {
                auto OldSize = Data.size();
                Data.resize(OldSize + BytesRead);
                std::memcpy(Data.data() + OldSize, Buffer, BytesRead);
            }
        }
        
        return Data;
    }
    
    bool IsReady() const {
        return IsInitialized_ && S3Client_ != nullptr;
    }
    
private:
    void EnsureBucketExists(const std::string& Bucket) {
        // Check if bucket exists
        Aws::S3::Model::HeadBucketRequest HeadRequest;
        HeadRequest.SetBucket(Bucket.c_str());
        
        auto HeadOutcome = S3Client_->HeadBucket(HeadRequest);
        
        if (!HeadOutcome.IsSuccess()) {
            // Bucket doesn't exist, create it
            Aws::S3::Model::CreateBucketRequest CreateRequest;
            CreateRequest.SetBucket(Bucket.c_str());
            
            auto CreateOutcome = S3Client_->CreateBucket(CreateRequest);
            
            if (!CreateOutcome.IsSuccess()) {
                auto Error = CreateOutcome.GetError();
                throw std::runtime_error("Failed to create bucket '" + Bucket + "': " + 
                                       Error.GetMessage().c_str());
            }
        }
    }
    
    Aws::SDKOptions AwsOptions_;
    std::unique_ptr<Aws::S3::S3Client> S3Client_;
    bool IsInitialized_;
};

// DataManager implementation

DataManager::DataManager(BG::Common::Logger::LoggingSystem* _Logger)
    : m_impl(std::make_unique<Impl>())
    , Logger_(_Logger)
    , IsSetup_(false)
{
    SAFE_LOG("DataManager instance created", 5);
}

DataManager::~DataManager() {
    SAFE_LOG("DataManager instance destroyed", 5);
}

void DataManager::Setup(const std::string& _Host,
                       unsigned short _Port,
                       const std::string& _AccessKey,
                       const std::string& _SecretKey,
                       bool _UseSSL) {
    
    std::lock_guard<std::mutex> Lock(SetupMutex_);
    
    if (IsSetup_) {
        SAFE_LOG("DataManager already set up, skipping setup", 6);
        return;
    }
    
    try {
        SAFE_LOG("Setting up DataManager with host: " + _Host + ":" + std::to_string(_Port), 4);
        
        m_impl->Initialize(_Host, _Port, _AccessKey, _SecretKey, _UseSSL);
        IsSetup_ = true;
        
        SAFE_LOG("DataManager setup completed successfully", 4);
        
    } catch (const std::exception& e) {
        std::string ErrorMsg = "Failed to setup DataManager: " + std::string(e.what());
        SAFE_LOG(ErrorMsg, 0);
        throw std::runtime_error(ErrorMsg);
    }
}

void DataManager::UploadObject(const std::string& Bucket,
                              const std::string& Key,
                              const std::vector<std::byte>& Data) {
    
    // Lazy initialization on first upload if not already set up
    if (!IsSetup_) {
        SAFE_LOG("DataManager not set up. Call Setup() before uploading objects.", 0);
        throw std::runtime_error("DataManager not set up. Call Setup() before uploading objects.");
    }
    
    try {
        SAFE_LOG("Uploading object: " + Key + " to bucket: " + Bucket + " (size: " + std::to_string(Data.size()) + " bytes)", 5);
        
        m_impl->UploadObject(Bucket, Key, Data);
        
        SAFE_LOG("Successfully uploaded object: " + Key + " to bucket: " + Bucket, 4);
        
    } catch (const std::exception& e) {
        std::string ErrorMsg = "Failed to upload object '" + Key + "' to bucket '" + Bucket + "': " + std::string(e.what());
        SAFE_LOG(ErrorMsg, 0);
        throw std::runtime_error(ErrorMsg);
    }
}

std::vector<std::byte> DataManager::DownloadObject(const std::string& Bucket,
                                                   const std::string& Key) {
    
    if (!IsSetup_) {
        SAFE_LOG("DataManager not set up. Call Setup() before downloading objects.", 0);
        throw std::runtime_error("DataManager not set up. Call Setup() before downloading objects.");
    }
    
    try {
        SAFE_LOG("Downloading object: " + Key + " from bucket: " + Bucket, 5);
        
        auto Data = m_impl->DownloadObject(Bucket, Key);
        
        SAFE_LOG("Successfully downloaded object: " + Key + " from bucket: " + Bucket + " (size: " + std::to_string(Data.size()) + " bytes)", 4);
        
        return Data;
        
    } catch (const std::exception& e) {
        std::string ErrorMsg = "Failed to download object '" + Key + "' from bucket '" + Bucket + "': " + std::string(e.what());
        SAFE_LOG(ErrorMsg, 0);
        throw std::runtime_error(ErrorMsg);
    }
}

bool DataManager::IsReady() const {
    std::lock_guard<std::mutex> Lock(SetupMutex_);
    return IsSetup_ && m_impl && m_impl->IsReady();
}
