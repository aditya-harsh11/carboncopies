#include "Discovery.h"


#include <udp_discovery_peer.hpp>
#include <BG/Common/Logger/Logger.h>
#include <Cluster/MutexHelper.h>

#include <thread>
#include <mutex>
#include <chrono>
#include <random>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <unordered_set>

// Implementation details hidden from users of the class
class DiscoveryManager::Impl {
public:
    Impl(
        uint64_t ApplicationId,
        unsigned int MulticastAddress,
        int Port,
        const std::string& NodeId,
        BG::Common::Logger::LoggingSystem* Logger,
        int LeaderElectionTimeoutMs,
        int LeaderHeartbeatIntervalMs,
        int NodeTimeoutMs,
        int HealthCheckIntervalMs,
        int MaxBackoffMs,
        int LeaderStabilityGracePeriodMs,
        bool PreferLongestRunningLeader,
        bool UseBroadcast,
        bool UseMulticast,
        std::string AdvertisedHost
    )
        : ApplicationId_(ApplicationId),
        MulticastAddress_(MulticastAddress),
        Port_(Port),
        NodeId_(NodeId),
        Logger_(Logger),
        LeaderElectionTimeoutMs_(LeaderElectionTimeoutMs),
        LeaderHeartbeatIntervalMs_(LeaderHeartbeatIntervalMs),
        NodeTimeoutMs_(NodeTimeoutMs),
        HealthCheckIntervalMs_(HealthCheckIntervalMs),
        MaxBackoffMs_(MaxBackoffMs),
        LeaderStabilityGracePeriodMs_(LeaderStabilityGracePeriodMs),
        PreferLongestRunningLeader_(PreferLongestRunningLeader),
        UseBroadcast_(UseBroadcast),
        UseMulticast_(UseMulticast),
        IsRunning_(false),
        CurrentRole_(ClusterRole::FOLLOWER),
        LastLeaderNodeInfo_() {
        
        // Initialize user data as follower with timestamp
        UpdateUserData();
        

        // Autodetect LAN IP
        Logger_->Log("[NodeDiscovery] Using '" + AdvertisedHost + "' As Advertised Host Address Of This Node", 4);

        // Add self to known nodes
        NodeInfo selfInfo;
        selfInfo.NodeId = NodeId_;
        selfInfo.Address = AdvertisedHost;
        selfInfo.Port = Port_;
        selfInfo.Role = "ROLE_FOLLOWER";
        selfInfo.LastSeenMs = GetCurrentTimeMs();
        selfInfo.LeaderSinceMs = 0;
        selfInfo.IsActive = true;
        KnownNodes_[NodeId_] = selfInfo;
        
        if (Logger_) {
            Logger_->Log("[NodeDiscovery] Discovery manager initialized as follower", 4);
        }
    }

    std::string GetAdvertisedHost() {
        return KnownNodes_[NodeId_].Address;
    }

    bool Start() {
        std::lock_guard<std::timed_mutex> lock = lockWithTimeout(Mutex_, "Discovery.Start");
        
        if (IsRunning_) {
            return true;
        }

        // Configure discovery parameters
        udpdiscovery::PeerParameters parameters;
        parameters.set_port(Port_);
        parameters.set_application_id(ApplicationId_);
        
        if (UseBroadcast_) {
            parameters.set_can_use_broadcast(true);
        }
        
        if (UseMulticast_) {
            parameters.set_multicast_group_address(MulticastAddress_);
            parameters.set_can_use_multicast(true);
        }
        
        parameters.set_can_discover(true);
        parameters.set_can_be_discovered(true);
        parameters.set_same_peer_mode(udpdiscovery::PeerParameters::SamePeerMode::kSamePeerIpAndPort);

        // Start the discovery peer
        if (!Peer_.Start(parameters, UserData_)) {
            if (Logger_) {
                Logger_->Log("[NodeDiscovery] Failed to start UDP discovery peer", 10);
            }
            return false;
        }

        IsRunning_ = true;
        
        // Start the management thread
        ManagementThread_ = std::thread(&Impl::ManagementThreadFunction, this);
        
        if (Logger_) {
            Logger_->Log("[NodeDiscovery] Discovery manager started successfully", 4);
        }
        
        return true;
    }

    void Stop() {
        std::lock_guard<std::timed_mutex> lock = lockWithTimeout(Mutex_, "Discovery.Stop");
        
        if (!IsRunning_) {
            return;
        }
        
        IsRunning_ = false;
        
        if (ManagementThread_.joinable()) {
            ManagementThread_.join();
        }
        
        Peer_.Stop(true);
        
        if (Logger_) {
            Logger_->Log("[NodeDiscovery] Discovery manager stopped", 4);
        }
    }

    bool IsLeader(bool _Lock) const {
        if (_Lock) std::lock_guard<std::timed_mutex> lock = lockWithTimeout(Mutex_, "Discovery.IsLeader");
        return CurrentRole_ == ClusterRole::LEADER;
    }

    std::string GetLeaderNodeId() const {
        std::lock_guard<std::timed_mutex> lock = lockWithTimeout(Mutex_, "Discovery.GetLeaderNodeID");
        return LeaderNodeId_;
    }

    std::string GetLeaderAddress(bool _LockMutex) const {
        if (_LockMutex) std::lock_guard<std::timed_mutex> lock = lockWithTimeout(Mutex_, "Discovery.GetLeaderAddress");
        
        if (LeaderIpPort_.ip() == 0) {
            return "";
        }
        
        return udpdiscovery::IpToString(LeaderIpPort_.ip());
    }

    std::vector<NodeInfo> GetKnownNodes() const {
        std::lock_guard<std::timed_mutex> lock = lockWithTimeout(Mutex_, "Discovery.GetKnownNodes");
        
        std::vector<NodeInfo> nodes;
        nodes.reserve(KnownNodes_.size());
        
        for (const auto& pair : KnownNodes_) {
            nodes.push_back(pair.second);
        }
        
        return nodes;
    }

    std::vector<NodeInfo> GetActiveNodes(bool _Lock) const {
        if (_Lock) std::lock_guard<std::timed_mutex> lock = lockWithTimeout(Mutex_, "Discovery.GetActiveNodes");
        
        std::vector<NodeInfo> activeNodes;
        auto now = GetCurrentTimeMs();
        
        for (const auto& pair : KnownNodes_) {
            if (now - pair.second.LastSeenMs < NodeTimeoutMs_) {
                activeNodes.push_back(pair.second);
            }
        }
        
        return activeNodes;
    }

    ClusterState GetClusterState(bool _Lock) const {
        if (_Lock) std::lock_guard<std::timed_mutex> lock = lockWithTimeout(Mutex_, "Discovery.GetClusterState");
        
        ClusterState state;
        state.CurrentRole = CurrentRole_;
        state.LeaderNodeId = LeaderNodeId_;
        state.LeaderAddress = GetLeaderAddress(false);
        state.LeaderSinceMs = 0;
        
        // Find leader since time if available
        if (!LeaderNodeId_.empty() && KnownNodes_.count(LeaderNodeId_)) {
            state.LeaderSinceMs = KnownNodes_.at(LeaderNodeId_).LeaderSinceMs;
        }
        
        state.ActiveNodes = GetActiveNodes(false);
        
        return state;
    }

    void StepDown() {
        std::lock_guard<std::timed_mutex> lock = lockWithTimeout(Mutex_, "Discovery.StepDown");
        
        if (CurrentRole_ == ClusterRole::LEADER) {
            SetRole(ClusterRole::FOLLOWER);
            if (Logger_) {
                Logger_->Log("[NodeDiscovery] Stepped down from leadership", 3);
            }
        }
    }

    std::string GetNodeId() const {
        return NodeId_;
    }

    void RegisterNodeJoinCallback(NodeJoinCallback callback) {
        std::lock_guard<std::timed_mutex> lock = lockWithTimeout(Mutex_, "Discovery.RegisterNodeJoinCallback");
        NodeJoinCallbacks_.push_back(callback);
    }

    void RegisterNodeLeaveCallback(NodeLeaveCallback callback) {
        std::lock_guard<std::timed_mutex> lock = lockWithTimeout(Mutex_, "Discovery.RegisterNodeLeaveCallback");
        NodeLeaveCallbacks_.push_back(callback);
    }

    void RegisterLeaderChangeCallback(LeaderChangeCallback callback) {
        std::lock_guard<std::timed_mutex> lock = lockWithTimeout(Mutex_, "Discovery.RegisterLeaderChangeCallback");
        LeaderChangeCallbacks_.push_back(callback);
    }

    void RegisterRoleChangeCallback(RoleChangeCallback callback) {
        std::lock_guard<std::timed_mutex> lock = lockWithTimeout(Mutex_, "Discovery.RegisterRoleChangeCallback");
        RoleChangeCallbacks_.push_back(callback);
    }

    size_t GetClusterSize() const {
        std::lock_guard<std::timed_mutex> lock = lockWithTimeout(Mutex_, "Discovery.GetClusterSize");
        auto now = GetCurrentTimeMs();
        size_t count = 0;
        
        for (const auto& pair : KnownNodes_) {
            if (now - pair.second.LastSeenMs < NodeTimeoutMs_) {
                count++;
            }
        }
        
        return count;
    }

    NodeInfo GetThisNodeInfo() {
        return KnownNodes_[NodeId_];
    }

    void ForceHealthCheck() {
        std::lock_guard<std::timed_mutex> lock = lockWithTimeout(Mutex_, "Discovery.ForceHealthCheck");
        CheckNodeHealth(true);
    }

private:
    void ManagementThreadFunction() {
        auto lastHeartbeat = std::chrono::steady_clock::now();
        auto lastHealthCheck = std::chrono::steady_clock::now();
        auto lastStatusLog = std::chrono::steady_clock::now();
        auto lastDiscovery = std::chrono::steady_clock::now();
        
        while (IsRunning_) {
            auto now = std::chrono::steady_clock::now();
            
            // Leader heartbeat
            if (CurrentRole_ == ClusterRole::LEADER) {
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHeartbeat).count() > 
                    LeaderHeartbeatIntervalMs_) {
                    UpdateUserData();
                    lastHeartbeat = now;
                }
            }
            
            // Health checks
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHealthCheck).count() > 
                HealthCheckIntervalMs_) {
                CheckNodeHealth(false);
                lastHealthCheck = now;
            }
            
            // Discovery - do this less frequently to avoid blocking
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDiscovery).count() > 1000) {
                CheckForLeader();
                lastDiscovery = now;
            }
            
            // Status logging
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastStatusLog).count() > 15) {
                LogStatus();
                lastStatusLog = now;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void CheckNodeHealth(bool force) {
        std::lock_guard<std::timed_mutex> lock = lockWithTimeout(Mutex_, "Discovery.CheckNodeHealth");
        
        auto now = GetCurrentTimeMs();
        std::vector<std::string> nodesToRemove;
        
        // Check for inactive nodes (skip self)
        for (const auto& pair : KnownNodes_) {
            if (pair.first != NodeId_ && now - pair.second.LastSeenMs > NodeTimeoutMs_) {
                nodesToRemove.push_back(pair.first);
            }
        }
        
        // Remove inactive nodes and trigger callbacks
        for (const auto& nodeId : nodesToRemove) {
            NodeInfo removedNode = KnownNodes_[nodeId];
            KnownNodes_.erase(nodeId);
            
            if (Logger_) {
                Logger_->Log("[NodeDiscovery] Node " + nodeId + " timed out and was removed", 6);
            }
            
            // Trigger leave callbacks
            for (const auto& callback : NodeLeaveCallbacks_) {
                callback(removedNode);
            }
        }
        
        // Log cluster status periodically
        if (force || now % 30000 < 500) {
            size_t activeCount = 0;
            for (const auto& pair : KnownNodes_) {
                if (now - pair.second.LastSeenMs < NodeTimeoutMs_) {
                    activeCount++;
                }
            }
            
            if (Logger_) {
                Logger_->Log("[NodeDiscovery] Cluster status: " + std::to_string(activeCount) + 
                            " active nodes, " + std::to_string(KnownNodes_.size() - activeCount) +
                            " inactive nodes", 2);
            }
        }
    }

    void CheckForLeader() {
        // Get discovered peers without holding the mutex (this might block)
        auto discoveredPeers = Peer_.ListDiscovered();
        
        std::lock_guard<std::timed_mutex> lock = lockWithTimeout(Mutex_, "Discovery.CheckForLeader");
        auto now = GetCurrentTimeMs();
        
        // Always update self's timestamp
        if (KnownNodes_.find(NodeId_) != KnownNodes_.end()) {
            KnownNodes_[NodeId_].LastSeenMs = now;
            KnownNodes_[NodeId_].Role = (CurrentRole_ == ClusterRole::LEADER) ? "ROLE_LEADER" : "ROLE_FOLLOWER";
            if (CurrentRole_ == ClusterRole::LEADER && KnownNodes_[NodeId_].LeaderSinceMs == 0) {
                KnownNodes_[NodeId_].LeaderSinceMs = now;
            } else if (CurrentRole_ != ClusterRole::LEADER) {
                KnownNodes_[NodeId_].LeaderSinceMs = 0;
            }
        }
        
        // Update known nodes from discovered peers and log warnings for missing nodes
        std::unordered_set<std::string> currentPeers;
        for (const auto& peer : discoveredPeers) {
            const std::string& userData = peer.user_data();
            std::string peerNodeId;
            std::string peerRole;
            uint64_t peerLeaderSinceMs = 0;
            uint64_t peerTimestampMs = 0;
            
            size_t rolePos = userData.find("ROLE_");
            if (rolePos != std::string::npos) {
                size_t colonPos = userData.find(':', rolePos);
                if (colonPos != std::string::npos) {
                    peerRole = userData.substr(rolePos, colonPos - rolePos);
                    peerNodeId = userData.substr(colonPos + 1);
                    
                    // Handle timestamp and leader since if present
                    size_t tsPos = peerNodeId.find(":TS:");
                    if (tsPos != std::string::npos) {
                        size_t lsPos = peerNodeId.find(":LS:", tsPos + 4);
                        if (lsPos != std::string::npos) {
                            peerTimestampMs = std::stoull(peerNodeId.substr(tsPos + 4, lsPos - tsPos - 4));
                            peerLeaderSinceMs = std::stoull(peerNodeId.substr(lsPos + 4));
                            peerNodeId = peerNodeId.substr(0, tsPos);
                        } else {
                            peerTimestampMs = std::stoull(peerNodeId.substr(tsPos + 4));
                            peerNodeId = peerNodeId.substr(0, tsPos);
                        }
                    }
                }
            }
            
            if (!peerNodeId.empty() && peerNodeId != NodeId_) {
                currentPeers.insert(peerNodeId);
                
                NodeInfo nodeInfo;
                nodeInfo.NodeId = peerNodeId;
                nodeInfo.Address = udpdiscovery::IpToString(peer.ip_port().ip());
                nodeInfo.Port = peer.ip_port().port();
                nodeInfo.Role = peerRole;
                nodeInfo.LastSeenMs = now;
                nodeInfo.LeaderSinceMs = peerLeaderSinceMs;
                nodeInfo.IsActive = true;
                
                // Check if this is a new node
                bool isNewNode = (KnownNodes_.find(peerNodeId) == KnownNodes_.end());
                bool wasInactive = false;
                
                if (!isNewNode) {
                    // Check if node was previously inactive
                    wasInactive = (now - KnownNodes_[peerNodeId].LastSeenMs > NodeTimeoutMs_);
                    
                    // For leaders, preserve the earliest LeaderSinceMs value
                    if (peerRole == "ROLE_LEADER") {
                        if (KnownNodes_[peerNodeId].LeaderSinceMs > 0 && 
                            KnownNodes_[peerNodeId].LeaderSinceMs < peerLeaderSinceMs) {
                            // Use the earlier timestamp if we have it
                            nodeInfo.LeaderSinceMs = KnownNodes_[peerNodeId].LeaderSinceMs;
                        } else if (peerLeaderSinceMs == 0) {
                            // If we didn't receive a timestamp, use what we have
                            nodeInfo.LeaderSinceMs = KnownNodes_[peerNodeId].LeaderSinceMs;
                        }
                    }
                } else if (peerRole == "ROLE_LEADER" && peerLeaderSinceMs == 0) {
                    // New leader discovered but no timestamp, use current time
                    nodeInfo.LeaderSinceMs = now;
                }
                
                KnownNodes_[peerNodeId] = nodeInfo;
                
                if ((isNewNode || wasInactive)) {
                    if (Logger_) {
                        if (isNewNode) {
                            Logger_->Log("[NodeDiscovery] Discovered new node: " + peerNodeId + " (" + nodeInfo.Address + 
                                        ":" + std::to_string(nodeInfo.Port) + ")", 5);
                        } else {
                            Logger_->Log("[NodeDiscovery] Node " + peerNodeId + " became active again", 5);
                        }
                    }
                    
                    // Trigger join callbacks
                    for (const auto& callback : NodeJoinCallbacks_) {
                        callback(nodeInfo);
                    }
                }
                
                // Log if we received a leader without a timestamp
                if (peerRole == "ROLE_LEADER" && peerLeaderSinceMs == 0 && Logger_) {
                    Logger_->Log("[NodeDiscovery] WARNING: Leader " + peerNodeId + 
                                " is not broadcasting leader timestamp", 8);
                }
            }
        }
        
        // Log warnings for missing nodes (including leaders)
        for (const auto& pair : KnownNodes_) {
            if (pair.first != NodeId_ && currentPeers.find(pair.first) == currentPeers.end()) {
                uint64_t timeSinceLastSeen = now - pair.second.LastSeenMs;
                
                if (timeSinceLastSeen > NodeTimeoutMs_ / 2) {
                    if (Logger_) {
                        if (pair.second.Role == "ROLE_LEADER") {
                            Logger_->Log("[NodeDiscovery] WARNING: Leader " + pair.first + 
                                        " not seen for " + std::to_string(timeSinceLastSeen / 1000) + "s", 8);
                        } else {
                            Logger_->Log("[NodeDiscovery] WARNING: Node " + pair.first + 
                                        " not seen for " + std::to_string(timeSinceLastSeen / 1000) + "s", 7);
                        }
                    }
                }
            }
        }
        
        // Find all active leaders
        std::vector<std::pair<std::string, uint64_t>> activeLeaders;
        for (const auto& pair : KnownNodes_) {
            if (pair.second.Role == "ROLE_LEADER" && 
                now - pair.second.LastSeenMs < NodeTimeoutMs_) {
                activeLeaders.emplace_back(pair.first, pair.second.LeaderSinceMs);
            }
        }
        
        if (activeLeaders.empty()) {
            // No leader found - consider election
            if (CurrentRole_ != ClusterRole::LEADER) {
                if (now - LastElectionCheck_ > LeaderElectionTimeoutMs_) {
                    // Don't hold mutex during backoff sleep
                    Mutex_.unlock();
                    
                    // Random backoff to reduce collisions
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> distrib(0, MaxBackoffMs_);
                    int backoff = distrib(gen);
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
                    
                    Mutex_.lock();
                    
                    // Re-check after backoff
                    auto currentPeers = Peer_.ListDiscovered();
                    bool stillNoLeader = true;
                    
                    for (const auto& peer : currentPeers) {
                        const std::string& userData = peer.user_data();
                        if (userData.find("ROLE_LEADER:") == 0) {
                            stillNoLeader = false;
                            break;
                        }
                    }
                    
                    if (stillNoLeader) {
                        SetRole(ClusterRole::LEADER);
                        if (Logger_) {
                            Logger_->Log("[NodeDiscovery] Elected as new leader", 4);
                        }
                    }
                    
                    LastElectionCheck_ = now;
                }
            }
            
            // Reset conflict detection when no leader is found
            LastLeaderConflictDetection_ = 0;
            return;
        }
        
        // Sort leaders by tenure (longest first)
        std::sort(activeLeaders.begin(), activeLeaders.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        
        // The most mature leader is the one with the smallest LeaderSinceMs value
        std::string mostMatureLeader = activeLeaders[0].first;
        uint64_t mostMatureLeaderSince = activeLeaders[0].second;
        
        // If we have multiple leaders, resolve the conflict
        if (activeLeaders.size() > 1 && Logger_) {
            std::stringstream ss;
            ss << "[NodeDiscovery] Multiple leaders detected: ";
            for (const auto& leader : activeLeaders) {
                uint64_t tenure = (now - leader.second) / 1000;
                ss << leader.first << " (" << tenure << "s), ";
            }
            ss << "most mature: " + mostMatureLeader;
            Logger_->Log(ss.str(), 6);
        }
        
        // Get old leader info before updating
        NodeInfo oldLeaderInfo;
        if (!LeaderNodeId_.empty() && KnownNodes_.count(LeaderNodeId_)) {
            oldLeaderInfo = KnownNodes_.at(LeaderNodeId_);
        }
        
        LeaderNodeId_ = mostMatureLeader;
        
        // Find the leader's IP port
        if (KnownNodes_.find(mostMatureLeader) != KnownNodes_.end()) {
            LeaderIpPort_ = {StringToIp(KnownNodes_[mostMatureLeader].Address), 
                             static_cast<uint16_t>(KnownNodes_[mostMatureLeader].Port)};
        }
        
        // Handle leadership conflict if we're a leader but not the most mature
        if (CurrentRole_ == ClusterRole::LEADER && mostMatureLeader != NodeId_) {
            uint64_t myLeaderSince = KnownNodes_[NodeId_].LeaderSinceMs;
            
            if (PreferLongestRunningLeader_) {
                // We should step down if we're not the most mature leader
                if (myLeaderSince > mostMatureLeaderSince) {
                    // The other leader has been leader longer
                    if (LastLeaderConflictDetection_ == 0) {
                        LastLeaderConflictDetection_ = now;
                        if (Logger_) {
                            uint64_t otherTenure = (now - mostMatureLeaderSince) / 1000;
                            uint64_t myTenure = (now - myLeaderSince) / 1000;
                            Logger_->Log("[NodeDiscovery] Leadership conflict detected. Other leader " + 
                                        mostMatureLeader + " has been leader longer (" + 
                                        std::to_string(otherTenure) + "s vs " + std::to_string(myTenure) + 
                                        "s). Will step down after grace period.", 4);
                        }
                    }
                    
                    // Step down after grace period
                    if (now - LastLeaderConflictDetection_ > LeaderStabilityGracePeriodMs_) {
                        SetRole(ClusterRole::FOLLOWER);
                        if (Logger_) {
                            Logger_->Log("[NodeDiscovery] Stepping down as leader in favor of more mature leader " + 
                                        mostMatureLeader, 4);
                        }
                        LastLeaderConflictDetection_ = 0;
                    }
                } else {
                    // We're the more mature leader, maintain leadership
                    LastLeaderConflictDetection_ = 0;
                    if (Logger_ && now % 5000 < 100) {
                        uint64_t myTenure = (now - myLeaderSince) / 1000;
                        Logger_->Log("[NodeDiscovery] Maintaining leadership as most mature leader (" + 
                                    std::to_string(myTenure) + "s tenure)", 3);
                    }
                }
            } else {
                // Original behavior: step down after grace period regardless of tenure
                if (LastLeaderConflictDetection_ == 0) {
                    LastLeaderConflictDetection_ = now;
                    if (Logger_) {
                        Logger_->Log("[NodeDiscovery] Leadership conflict detected with " + 
                                    mostMatureLeader + ", waiting for stability period", 4);
                    }
                }
                
                if (now - LastLeaderConflictDetection_ > LeaderStabilityGracePeriodMs_) {
                    SetRole(ClusterRole::FOLLOWER);
                    if (Logger_) {
                        Logger_->Log("[NodeDiscovery] Stepping down as leader after grace period", 4);
                    }
                    LastLeaderConflictDetection_ = 0;
                }
            }
        } else {
            // Reset conflict detection if we're not in conflict
            LastLeaderConflictDetection_ = 0;
        }
        
        // Trigger leader change callback if needed
        if (oldLeaderInfo.NodeId != mostMatureLeader) {
            if (Logger_) {
                Logger_->Log("[NodeDiscovery] Leader changed from " + 
                            (oldLeaderInfo.NodeId.empty() ? "None" : oldLeaderInfo.NodeId) + 
                            " to " + mostMatureLeader, 4);
            }
            
            NodeInfo newLeaderInfo;
            if (KnownNodes_.count(mostMatureLeader)) {
                newLeaderInfo = KnownNodes_.at(mostMatureLeader);
            }
            
            for (const auto& callback : LeaderChangeCallbacks_) {
                callback(oldLeaderInfo, newLeaderInfo);
            }
            
            // Update last leader info
            LastLeaderNodeInfo_ = newLeaderInfo;
        }
        
        LastLeaderDiscovery_ = now;
    }

    void SetRole(ClusterRole NewRole) {
        if (CurrentRole_ == NewRole) return;
        
        ClusterRole oldRole = CurrentRole_;
        CurrentRole_ = NewRole;
        UpdateUserData();
        
        // Update self role in known nodes
        if (KnownNodes_.find(NodeId_) != KnownNodes_.end()) {
            KnownNodes_[NodeId_].Role = (NewRole == ClusterRole::LEADER) ? "ROLE_LEADER" : "ROLE_FOLLOWER";
            if (NewRole == ClusterRole::LEADER) {
                KnownNodes_[NodeId_].LeaderSinceMs = GetCurrentTimeMs();
            } else {
                KnownNodes_[NodeId_].LeaderSinceMs = 0;
            }
        }
        
        if (NewRole == ClusterRole::LEADER) {
            LeaderNodeId_ = NodeId_;
            LeaderIpPort_ = {GetCurrentIp(), static_cast<uint16_t>(Port_)};
            
            NodeInfo newLeaderInfo;
            if (KnownNodes_.find(NodeId_) != KnownNodes_.end()) {
                newLeaderInfo = KnownNodes_.at(NodeId_);
            }
            
            // Trigger leader change callback
            for (const auto& callback : LeaderChangeCallbacks_) {
                callback(NodeInfo(), newLeaderInfo); // Empty NodeInfo for old leader
            }
            
            // Update last leader info
            LastLeaderNodeInfo_ = newLeaderInfo;
        } else {
            // Store the current leader info before clearing
            NodeInfo oldLeaderInfo = LastLeaderNodeInfo_;
            
            LeaderNodeId_.clear();
            LeaderIpPort_ = {0, 0};
            
            // Trigger leader change callback with empty new leader
            for (const auto& callback : LeaderChangeCallbacks_) {
                callback(oldLeaderInfo, NodeInfo());
            }
            
            // Clear last leader info
            LastLeaderNodeInfo_ = NodeInfo();
        }
        
        // Trigger role change callbacks
        ClusterState state = GetClusterState(false);
        for (const auto& callback : RoleChangeCallbacks_) {
            callback(oldRole, NewRole, state);
        }
    }

    void UpdateUserData() {
        std::string roleStr = (CurrentRole_ == ClusterRole::LEADER) ? "ROLE_LEADER:" : "ROLE_FOLLOWER:";
        uint64_t currentTime = GetCurrentTimeMs();
        
        // Always include timestamp
        UserData_ = roleStr + NodeId_ + ":TS:" + std::to_string(currentTime);
        
        // Add leader since timestamp if we're the leader
        if (CurrentRole_ == ClusterRole::LEADER) {
            uint64_t leaderSince = KnownNodes_[NodeId_].LeaderSinceMs;
            if (leaderSince == 0) {
                leaderSince = currentTime;
                KnownNodes_[NodeId_].LeaderSinceMs = leaderSince;
            }
            UserData_ += ":LS:" + std::to_string(leaderSince);
            
            if (Logger_ && currentTime % 10000 < 100) {
                uint64_t tenure = (currentTime - leaderSince) / 1000;
                Logger_->Log("[NodeDiscovery] Broadcasting leadership tenure: " + 
                            std::to_string(tenure) + " seconds", 3);
            }
        }
        
        Peer_.SetUserData(UserData_);
    }

    void LogStatus() {
        if (!Logger_) return;
        
        std::stringstream ss;
        ss << "[NodeDiscovery] Cluster status - ";
        ss << "Role: " << (CurrentRole_ == ClusterRole::LEADER ? "Leader" : "Follower") << ", ";
        ss << "Leader: " << (LeaderNodeId_.empty() ? "None" : LeaderNodeId_) << ", ";
        
        if (CurrentRole_ == ClusterRole::LEADER && KnownNodes_.find(NodeId_) != KnownNodes_.end()) {
            uint64_t leaderDuration = (GetCurrentTimeMs() - KnownNodes_[NodeId_].LeaderSinceMs) / 1000;
            ss << "Leader for: " << leaderDuration << "s, ";
        }
        
        ss << "Active nodes: " << GetClusterSize() << ", ";
        ss << "Total known: " << KnownNodes_.size();
        
        Logger_->Log(ss.str(), 2);
    }

    uint64_t GetCurrentTimeMs() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    uint32_t StringToIp(const std::string& ipStr) const {
        uint32_t ip = 0;
        int octet = 0;
        int shift = 24;
        
        for (char c : ipStr) {
            if (c == '.') {
                ip |= (octet << shift);
                shift -= 8;
                octet = 0;
            } else if (c >= '0' && c <= '9') {
                octet = octet * 10 + (c - '0');
            }
        }
        ip |= octet;
        
        return ip;
    }

    uint32_t GetCurrentIp() const {
        return 0;
    }



    // Member variables
    uint64_t ApplicationId_;
    unsigned int MulticastAddress_;
    int Port_;
    std::string NodeId_;
    
    BG::Common::Logger::LoggingSystem* Logger_;
    
    int LeaderElectionTimeoutMs_;
    int LeaderHeartbeatIntervalMs_;
    int NodeTimeoutMs_;
    int HealthCheckIntervalMs_;
    int MaxBackoffMs_;
    int LeaderStabilityGracePeriodMs_;
    bool PreferLongestRunningLeader_;
    bool UseBroadcast_;
    bool UseMulticast_;
    
    mutable std::timed_mutex Mutex_;
    bool IsRunning_;
    ClusterRole CurrentRole_;
    
    std::string UserData_;
    std::string LeaderNodeId_;
    udpdiscovery::IpPort LeaderIpPort_;
    
    std::unordered_map<std::string, NodeInfo> KnownNodes_;
    
    std::vector<NodeJoinCallback> NodeJoinCallbacks_;
    std::vector<NodeLeaveCallback> NodeLeaveCallbacks_;
    std::vector<LeaderChangeCallback> LeaderChangeCallbacks_;
    std::vector<RoleChangeCallback> RoleChangeCallbacks_;
    
    udpdiscovery::Peer Peer_;
    std::thread ManagementThread_;
    
    uint64_t LastElectionCheck_ = 0;
    uint64_t LastLeaderDiscovery_ = 0;
    uint64_t LastLeaderConflictDetection_ = 0;
    NodeInfo LastLeaderNodeInfo_;
};

// Public method implementations
DiscoveryManager::DiscoveryManager(
    uint64_t ApplicationId,
    unsigned int MulticastAddress,
    int Port,
    const std::string& NodeId,
    BG::Common::Logger::LoggingSystem* Logger,
    int LeaderElectionTimeoutMs,
    int LeaderHeartbeatIntervalMs,
    int NodeTimeoutMs,
    int HealthCheckIntervalMs,
    int MaxBackoffMs,
    int LeaderStabilityGracePeriodMs,
    bool PreferLongestRunningLeader,
    bool UseBroadcast,
    bool UseMulticast,
    std::string AdvertisedHost
)
    : Pimpl_(std::make_unique<Impl>(
        ApplicationId,
        MulticastAddress,
        Port,
        NodeId,
        Logger,
        LeaderElectionTimeoutMs,
        LeaderHeartbeatIntervalMs,
        NodeTimeoutMs,
        HealthCheckIntervalMs,
        MaxBackoffMs,
        LeaderStabilityGracePeriodMs,
        PreferLongestRunningLeader,
        UseBroadcast,
        UseMulticast,
        AdvertisedHost
    )) {
}

DiscoveryManager::~DiscoveryManager() = default;

bool DiscoveryManager::Start() { return Pimpl_->Start(); }
void DiscoveryManager::Stop() { Pimpl_->Stop(); }
bool DiscoveryManager::IsLeader(bool _Lock) const { return Pimpl_->IsLeader(_Lock); }
std::string DiscoveryManager::GetLeaderNodeId() const { return Pimpl_->GetLeaderNodeId(); }
std::string DiscoveryManager::GetLeaderAddress(bool _LockMutex) const { return Pimpl_->GetLeaderAddress(_LockMutex); }
std::vector<NodeInfo> DiscoveryManager::GetKnownNodes() const { return Pimpl_->GetKnownNodes(); }
std::vector<NodeInfo> DiscoveryManager::GetActiveNodes(bool _Lock = true) const { return Pimpl_->GetActiveNodes(_Lock); }
void DiscoveryManager::StepDown() { Pimpl_->StepDown(); }
std::string DiscoveryManager::GetNodeId() const { return Pimpl_->GetNodeId(); }
size_t DiscoveryManager::GetClusterSize() const { return Pimpl_->GetClusterSize(); }
void DiscoveryManager::ForceHealthCheck() { Pimpl_->ForceHealthCheck(); }
ClusterState DiscoveryManager::GetClusterState(bool _Lock) const { return Pimpl_->GetClusterState(_Lock); }
NodeInfo DiscoveryManager::GetThisNodeInfo() {return Pimpl_->GetThisNodeInfo(); }
std::string DiscoveryManager::GetAdvertisedHost() {return Pimpl_->GetAdvertisedHost();}

void DiscoveryManager::RegisterNodeJoinCallback(NodeJoinCallback callback) {
    Pimpl_->RegisterNodeJoinCallback(callback);
}

void DiscoveryManager::RegisterNodeLeaveCallback(NodeLeaveCallback callback) {
    Pimpl_->RegisterNodeLeaveCallback(callback);
}

void DiscoveryManager::RegisterLeaderChangeCallback(LeaderChangeCallback callback) {
    Pimpl_->RegisterLeaderChangeCallback(callback);
}

void DiscoveryManager::RegisterRoleChangeCallback(RoleChangeCallback callback) {
    Pimpl_->RegisterRoleChangeCallback(callback);
}