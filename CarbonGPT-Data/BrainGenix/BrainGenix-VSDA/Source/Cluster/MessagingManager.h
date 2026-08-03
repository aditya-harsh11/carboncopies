#pragma once

#include "BidirectionalRPC.h"
#include "Discovery.h"
#include <BG/Common/Logger/Logger.h>

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <thread>
#include <unordered_map>
#include <atomic>
#include <mutex>

class MessagingManager {
public:
    using RpcCallback = std::function<void(const std::string& method, const std::string& params)>;
    using LeaderLoop = std::function<void(std::atomic_bool* shouldContinue)>;

    MessagingManager(DiscoveryManager* discovery, 
                    BG::Common::Logger::LoggingSystem* logger, 
                    int basePort = 5555);
    ~MessagingManager();

    bool Start();
    void Stop();
    bool IsRunning() const;

    std::string GetNodeId() const;
    bool IsLeader();
    int GetBasePort() const;

    // Register methods that can be called remotely
    template<typename Func>
    void RegisterMethod(const std::string& methodName, Func func);

    // Call a method on a specific node
    template<typename ReturnType, typename... Args>
    ReturnType CallNode(const std::string& nodeId, const std::string& methodName, Args... args);

    // Call a method on all nodes (including self)
    template<typename ReturnType, typename... Args>
    std::unordered_map<std::string, ReturnType> CallAllNodes(const std::string& methodName, Args... args);

    // Register leader-specific loops
    void RegisterLeaderLoop(LeaderLoop loop);

    // returns the current host address of this node
    std::string GetAdvertisedHost();

    BG::Common::Logger::LoggingSystem* GetLogger() const;


private:
    void NodeJoinCallback(NodeInfo info);
    void NodeLeaveCallback(NodeInfo info);
    void LeaderChangeCallback(NodeInfo oldLeader, NodeInfo newLeader);
    void RoleChangeCallback(ClusterRole oldRole, ClusterRole newRole, const ClusterState& state);

    void UpdateConnections();
    void StartLeaderLoops();
    void StopLeaderLoops();
    void CleanupStaleConnections();

    DiscoveryManager* discovery_;
    BG::Common::Logger::LoggingSystem* logger_;
    int basePort_;

    std::unique_ptr<BidirectionalRpc> selfServer_;
    std::unordered_map<std::string, std::unique_ptr<BidirectionalRpc>> nodeClients_;
    
    std::vector<LeaderLoop> leaderLoops_;
    std::vector<std::thread> leaderThreads_;
    std::atomic_bool shouldContinue_{false};
    std::atomic_bool isRunning_{false};
    
    mutable std::mutex mutex_;
};

// Template method implementations
template<typename Func>
void MessagingManager::RegisterMethod(const std::string& methodName, Func func) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (selfServer_) {
        selfServer_->Bind(methodName, func);
    }
}

template<typename ReturnType, typename... Args>
ReturnType MessagingManager::CallNode(const std::string& nodeId, const std::string& methodName, Args... args) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (nodeClients_.find(nodeId) != nodeClients_.end()) {
        try {
            return nodeClients_[nodeId]->Call<ReturnType>(methodName, args...);
        } catch (const std::exception& e) {
            logger_->Log("RPC call to node " + nodeId + " failed: " + e.what(), 8);
            throw;
        }
    }
    
    throw std::runtime_error("No connection to node " + nodeId);
}

template<typename ReturnType, typename... Args>
std::unordered_map<std::string, ReturnType> MessagingManager::CallAllNodes(const std::string& methodName, Args... args) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_map<std::string, ReturnType> results;
    
    for (const auto& pair : nodeClients_) {
        try {
            results[pair.first] = pair.second->Call<ReturnType>(methodName, args...);
        } catch (const std::exception& e) {
            logger_->Log("RPC call to node " + pair.first + " failed: " + e.what(), 7);
            // Optionally rethrow or handle differently
        }
    }
    
    return results;
}