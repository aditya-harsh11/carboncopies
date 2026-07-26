#include "MessagingManager.h"
#include <stdexcept>
#include <algorithm>

MessagingManager::MessagingManager(DiscoveryManager* discovery, 
                                 BG::Common::Logger::LoggingSystem* logger, 
                                 int basePort)
    : discovery_(discovery), logger_(logger), basePort_(basePort) {
    
    // Register callbacks with discovery manager
    discovery_->RegisterNodeJoinCallback(
        std::bind(&MessagingManager::NodeJoinCallback, this, std::placeholders::_1));
    discovery_->RegisterNodeLeaveCallback(
        std::bind(&MessagingManager::NodeLeaveCallback, this, std::placeholders::_1));
    discovery_->RegisterLeaderChangeCallback(
        std::bind(&MessagingManager::LeaderChangeCallback, this, std::placeholders::_1, std::placeholders::_2));
    discovery_->RegisterRoleChangeCallback(
        std::bind(&MessagingManager::RoleChangeCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

MessagingManager::~MessagingManager() {
    Stop();
}

bool MessagingManager::Start() {
    if (isRunning_) return true;
    
    try {
        // Create our own server with timeout
        std::string Label = "Self";
        selfServer_ = std::make_unique<BidirectionalRpc>(basePort_, true, logger_, 5000);
        selfServer_->SetAdvertisedHost(discovery_->GetThisNodeInfo().Address);
        selfServer_->Start();
        
        // Bind a health check method
        selfServer_->Bind("SocketHealthCheck", []() { return true; });
        
        // Update connections based on current cluster state
        UpdateConnections();
        
        isRunning_ = true;
        logger_->Log("[MessagingManager] MessagingManager started successfully on port " + std::to_string(basePort_), 4);
        return true;
    } catch (const std::exception& e) {
        logger_->Log("[MessagingManager] Failed to start MessagingManager: " + std::string(e.what()), 10);
        return false;
    }
}

void MessagingManager::Stop() {
    if (!isRunning_) return;
    
    StopLeaderLoops();
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        nodeClients_.clear();
        
        if (selfServer_) {
            selfServer_->Stop();
            selfServer_.reset();
        }
    }
    
    isRunning_ = false;
    logger_->Log("[MessagingManager] MessagingManager stopped", 4);
}

bool MessagingManager::IsRunning() const {
    return isRunning_;
}

std::string MessagingManager::GetNodeId() const {
    return discovery_->GetNodeId();
}

bool MessagingManager::IsLeader() {
    return discovery_->IsLeader(false);
}

int MessagingManager::GetBasePort() const {
    return basePort_;
}

void MessagingManager::RegisterLeaderLoop(LeaderLoop loop) {
    std::lock_guard<std::mutex> lock(mutex_);
    leaderLoops_.push_back(loop);
}

// Update the constructor calls to include timeout
void MessagingManager::NodeJoinCallback(NodeInfo info) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        std::string Label = "Node@" + info.Address;
        auto client = std::make_unique<BidirectionalRpc>(0, true, logger_, 5000); // 5 second timeout
        client->SetAdvertisedHost(discovery_->GetThisNodeInfo().Address);
        client->UpdatePeer(info.Address, basePort_);
        client->Start();
        
        nodeClients_[info.NodeId] = std::move(client);
        logger_->Log("[MessagingManager] Created RPC connection to node " + info.NodeId, 5);
    } catch (const std::exception& e) {
        logger_->Log("[MessagingManager] Failed to create RPC connection to node " + info.NodeId + ": " + e.what(), 8);
    }
}

void MessagingManager::NodeLeaveCallback(NodeInfo info) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (nodeClients_.find(info.NodeId) != nodeClients_.end()) {
        nodeClients_.erase(info.NodeId);
        logger_->Log("[MessagingManager] Removed RPC connection to node " + info.NodeId, 5);
    }
}

void MessagingManager::LeaderChangeCallback(NodeInfo oldLeader, NodeInfo newLeader) {
    logger_->Log("[MessagingManager] Leader changed from " + (oldLeader.NodeId.empty() ? "None" : oldLeader.NodeId) + 
                " to " + newLeader.NodeId, 5);
    
    if (newLeader.NodeId == GetNodeId()) {
        // We became the leader
        logger_->Log("[MessagingManager] We are the new leader, starting leader loops", 5);
        StartLeaderLoops();
    } else {
        // We are no longer leader
        logger_->Log("[MessagingManager] We are no longer leader, stopping leader loops", 5);
        StopLeaderLoops();
        logger_->Log("[MessagingManager] Stopped Leader Loops", 2);
    }
    
    // Update all connections
    logger_->Log("[MessagingManager] Updating all connections to new leader", 3);
    UpdateConnections();
    logger_->Log("[MessagingManager] Finished updating all connections to new leader", 2);
}

std::string ClusterRoleToStr(ClusterRole _Role) {
    if (_Role == ClusterRole::FOLLOWER) {
        return "Follower";
    } else if (_Role == ClusterRole::LEADER) {
        return "Leader";
    } else {
        return "Pending";
    }
}

void MessagingManager::RoleChangeCallback(ClusterRole oldRole, ClusterRole newRole, const ClusterState& state) {
     logger_->Log("[MessagingManager] Role changed from " + ClusterRoleToStr(oldRole) + " to " + ClusterRoleToStr(newRole), 5);
    
    // Update connections when role changes
    UpdateConnections();
}

std::string MessagingManager::GetAdvertisedHost() {
    return discovery_->GetAdvertisedHost();
}

void MessagingManager::UpdateConnections() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Get current active nodes
    auto activeNodes = discovery_->GetActiveNodes(false);
    std::unordered_set<std::string> currentNodes;
    
    // Create connections to new nodes
    for (const auto& node : activeNodes) {
        currentNodes.insert(node.NodeId);
        
        if (nodeClients_.find(node.NodeId) == nodeClients_.end()) {
            try {
                std::string Label = "Node@" + node.Address;
                auto client = std::make_unique<BidirectionalRpc>(0, true, logger_, 5000, Label); // 5 second timeout
                client->SetAdvertisedHost(discovery_->GetThisNodeInfo().Address);
                client->UpdatePeer(node.Address, basePort_);
                client->Start();
                
                nodeClients_[node.NodeId] = std::move(client);
                logger_->Log("[MessagingManager] Created RPC connection to node " + node.NodeId, 5);
            } catch (const std::exception& e) {
                logger_->Log("[MessagingManager] Failed to create RPC connection to node " + node.NodeId + ": " + e.what(), 8);
            }
        }
    }
    
    // Remove connections to nodes that are no longer active
    CleanupStaleConnections();
}


void MessagingManager::CleanupStaleConnections() {
    auto activeNodes = discovery_->GetActiveNodes(false);
    std::unordered_set<std::string> activeNodeIds;
    
    for (const auto& node : activeNodes) {
        activeNodeIds.insert(node.NodeId);
    }
    
    // Remove connections to nodes that are no longer active
    for (auto it = nodeClients_.begin(); it != nodeClients_.end(); ) {
        if (activeNodeIds.find(it->first) == activeNodeIds.end()) {
            logger_->Log("[MessagingManager] Removing stale connection to node " + it->first, 5);
            it = nodeClients_.erase(it);
        } else {
            ++it;
        }
    }
}

void MessagingManager::StartLeaderLoops() {
    StopLeaderLoops();
    shouldContinue_ = true;
    
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& loop : leaderLoops_) {
        leaderThreads_.emplace_back(loop, &shouldContinue_);
    }
    
    logger_->Log("[MessagingManager] Started " + std::to_string(leaderLoops_.size()) + " leader loops", 5);
}

void MessagingManager::StopLeaderLoops() {
    shouldContinue_ = false;
    logger_->Log("[MessagingManager] Requesting leader threads to exit", 3);

    std::lock_guard<std::mutex> lock(mutex_);
    int tid = 0;
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (auto& thread : leaderThreads_) {
        logger_->Log("[MessagingManager] Attempting to join thread " + std::to_string(tid), 2);
        
        if (thread.joinable()) {
            // Try to join with timeout
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                current_time - start_time).count();
            
            if (elapsed > 5) { // 5 second timeout
                logger_->Log("[MessagingManager] Timeout joining thread " + std::to_string(tid) + 
                           ", detaching instead", 1);
                thread.detach(); // Last resort
            } else {
                thread.join();
                logger_->Log("[MessagingManager] Joined thread " + std::to_string(tid), 2);
            }
        }
        tid++;
    }
    
    leaderThreads_.clear();
    logger_->Log("[MessagingManager] Stopped all leader loops", 5);
}


BG::Common::Logger::LoggingSystem* MessagingManager::GetLogger() const {
    return logger_;
}
