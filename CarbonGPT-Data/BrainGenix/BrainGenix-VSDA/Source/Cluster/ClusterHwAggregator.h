#pragma once

#include "MessagingManager.h"
#include <SYCL/Device.h>
#include <nlohmann/json.hpp>
#include <string>
#include <atomic>
#include <functional>
#include <vector>

/* ---------------------------------------------------------------
 * Cluster-wide hardware aggregator.
 * Must be instantiated after MessagingManager::Start().
 * ------------------------------------------------------------- */
class ClusterHwAggregator
{
public:
    explicit ClusterHwAggregator(MessagingManager* mm, const std::vector<SYCLDevice>* syclDevices = nullptr);
    ~ClusterHwAggregator() = default;

    // Only call on the leader. Returns a JSON string.
    std::string GetClusterHwInfo();

    // Get leader loop function for automatic cluster info updates
    std::function<void(std::atomic_bool*)> GetLeaderLoop();

    // Update SYCL devices pointer (useful if devices are detected later)
    void UpdateSyclDevices(const std::vector<SYCLDevice>* syclDevices);

private:
    MessagingManager* mm_;
    const std::vector<SYCLDevice>* syclDevices_;
    mutable std::mutex syclMutex_;
    
    nlohmann::json BuildLocalHwSnapshot(const std::string& nodeId);
    nlohmann::json SyclDevicesToJson() const;
    void LogClusterInfo(const nlohmann::json& clusterInfo);
};