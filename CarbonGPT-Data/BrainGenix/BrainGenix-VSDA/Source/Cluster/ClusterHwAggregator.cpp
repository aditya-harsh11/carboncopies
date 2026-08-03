#include "ClusterHwAggregator.h"
#include <nlohmann/json.hpp>
#include <BG/Common/Logger/Logger.h>

#include <fstream>
#include <sstream>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <iomanip>

/* ===============================================================
 * Private helpers (translation-unit-local)
 * =============================================================== */
namespace
{
// ---------- tiny POSIX helpers ---------------------------------
std::string ReadWholeFile(const std::string& path)
{
    std::ifstream f(path);
    return std::string(std::istreambuf_iterator<char>(f),
                       std::istreambuf_iterator<char>());
}

int64_t ParseMeminfoKB(const std::string& key)
{
    std::ifstream mem("/proc/meminfo");
    std::string line;
    while (std::getline(mem, line))
    {
        if (line.compare(0, key.size(), key) == 0)
        {
            std::istringstream iss(line.substr(key.size()));
            int64_t kb = 0;
            iss >> kb;
            return kb * 1024; // bytes
        }
    }
    return -1;
}

int GetLogicalCpuCount()
{
    return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
}
} // anonymous namespace

/* ===============================================================
 * ClusterHwAggregator implementation
 * =============================================================== */
ClusterHwAggregator::ClusterHwAggregator(MessagingManager* mm, const std::vector<SYCLDevice>* syclDevices)
    : mm_(mm), syclDevices_(syclDevices)
{
    // Expose RPC method so other nodes can request our HW snapshot
    mm_->RegisterMethod("GetHwSnapshot", [this]() {
        return BuildLocalHwSnapshot(mm_->GetNodeId()).dump();
    });
}

nlohmann::json ClusterHwAggregator::SyclDevicesToJson() const
{
    std::lock_guard<std::mutex> lock(syclMutex_);
    nlohmann::json syclArray = nlohmann::json::array();
    
    if (syclDevices_ == nullptr) {
        return syclArray;
    }
    
    for (const auto& device : *syclDevices_) {
        nlohmann::json deviceInfo;
        deviceInfo["name"] = device.Name;
        deviceInfo["vendor"] = device.Vendor;
        deviceInfo["vram_mb"] = device.VRAM_MB;
        
        // Convert device type to string
        switch (device.Type) {
            case SYCL_DEVICE_TYPE_GPU:
                deviceInfo["type"] = "GPU";
                break;
            case SYCL_DEVICE_TYPE_CPU:
                deviceInfo["type"] = "CPU";
                break;
            default:
                deviceInfo["type"] = "OTHER";
                break;
        }
        
        syclArray.push_back(deviceInfo);
    }
    
    return syclArray;
}

nlohmann::json ClusterHwAggregator::BuildLocalHwSnapshot(const std::string& nodeId)
{
    nlohmann::json j;
    j["node_id"]        = nodeId;
    j["cpu_logical"]    = GetLogicalCpuCount();
    j["memory_total_B"] = ParseMeminfoKB("MemTotal:");

    char hostname[256]{};
    gethostname(hostname, sizeof(hostname));
    j["hostname"] = hostname;
    
    // Add SYCL information if available
    auto syclInfo = SyclDevicesToJson();
    if (!syclInfo.empty()) {
        j["sycl_devices"] = syclInfo;
    }

    return j;
}

std::string ClusterHwAggregator::GetClusterHwInfo()
{
    if (!mm_->IsLeader())
        throw std::runtime_error("GetClusterHwInfo() must be invoked on the leader");

    // Multicast to all live nodes (including self)
    auto raw = mm_->CallAllNodes<std::string>("GetHwSnapshot");

    // Aggregate results into a single JSON array
    nlohmann::json aggregate = nlohmann::json::array();
    for (const auto& [nodeId, jsonStr] : raw)
    {
        try
        {
            aggregate.push_back(nlohmann::json::parse(jsonStr));
        }
        catch (const std::exception& ex)
        {
            nlohmann::json fail;
            fail["node_id"] = nodeId;
            fail["error"]   = ex.what();
            aggregate.push_back(fail);
        }
    }
    return aggregate.dump(2); // pretty-printed JSON string
}

void ClusterHwAggregator::UpdateSyclDevices(const std::vector<SYCLDevice>* syclDevices)
{
    std::lock_guard<std::mutex> lock(syclMutex_);
    syclDevices_ = syclDevices;
}

void ClusterHwAggregator::LogClusterInfo(const nlohmann::json& clusterInfo)
{
    if (!clusterInfo.is_array()) return;

    // Get logger from MessagingManager (assuming it has access)
    auto logger = mm_->GetLogger(); // You'll need to add this method to MessagingManager
    
    logger->Log("===== Cluster Hardware Report =====", 4);
    
    for (const auto& node : clusterInfo) {
        if (node.contains("error")) {
            logger->Log("Node " + node["node_id"].get<std::string>() + " - ERROR: " + 
                       node["error"].get<std::string>(), 7);
            continue;
        }

        logger->Log("Node: " + node["node_id"].get<std::string>(), 4);
        logger->Log("  Hostname: " + node["hostname"].get<std::string>(), 4);
        logger->Log("  CPU Cores: " + std::to_string(node["cpu_logical"].get<int>()), 4);
        
        // Format memory nicely
        int64_t memoryBytes = node["memory_total_B"].get<int64_t>();
        double memoryGB = memoryBytes / (1024.0 * 1024.0 * 1024.0);
        logger->Log("  Memory: " + std::to_string(memoryGB) + " GB", 4);
        
        // Log SYCL devices if available
        if (node.contains("sycl_devices") && !node["sycl_devices"].empty()) {
            logger->Log("  SYCL Devices:", 4);
            for (const auto& device : node["sycl_devices"]) {
                logger->Log("    - " + device["name"].get<std::string>(), 4);
                logger->Log("      Type: " + device["type"].get<std::string>(), 4);
                logger->Log("      Vendor: " + device["vendor"].get<std::string>(), 4);
                logger->Log("      VRAM: " + std::to_string(device["vram_mb"].get<size_t>()) + " MB", 4);
            }
        } else {
            logger->Log("  SYCL Devices: None detected", 4);
        }
        logger->Log("", 4); // Empty line for separation
    }
    
    logger->Log("===================================", 4);
}

std::function<void(std::atomic_bool*)> ClusterHwAggregator::GetLeaderLoop()
{
    return [this](std::atomic_bool* shouldContinue) {
        auto logger = mm_->GetLogger();
        logger->Log("ClusterHwAggregator leader loop started", 3);
        
        while (*shouldContinue) {
            try {
                // Break the operation into smaller chunks with shouldContinue checks
                if (!*shouldContinue) break;
                
                std::string jsonStr = GetClusterHwInfo(); // This might hang
                
                if (!*shouldContinue) break;
                
                nlohmann::json clusterInfo = nlohmann::json::parse(jsonStr);
                
                if (!*shouldContinue) break;
                
                // LogClusterInfo(clusterInfo);
                
            } catch (const std::exception& e) {
                logger->Log("Failed to get cluster HW info: " + std::string(e.what()), 7);
            }
            
            // Wait with frequent shouldContinue checks
            for (int i = 0; i < 100 && *shouldContinue; i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        
        logger->Log("ClusterHwAggregator leader loop exiting", 3);
    };
}