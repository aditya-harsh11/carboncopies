#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <atomic>



/**
 * @struct NodeInfo
 * @brief Information about a cluster node
 */
struct NodeInfo {
    std::string NodeId;          ///< Unique node identifier
    std::string Address;         ///< IP address of the node
    int Port;                    ///< Port number
    std::string Role;            ///< Node role (leader/follower)
    uint64_t LastSeenMs;         ///< Last time this node was seen (epoch ms)
    uint64_t LeaderSinceMs;      ///< When this node became leader (0 if not leader)
    bool IsActive;               ///< Whether the node is currently active
};

/**
 * @brief Cluster role states
 */
enum class ClusterRole {
    UNKNOWN,
    LEADER,
    FOLLOWER
};

/**
 * @brief Cluster state information
 */
struct ClusterState {
    ClusterRole CurrentRole;
    std::string LeaderNodeId;
    std::string LeaderAddress;
    uint64_t LeaderSinceMs;
    std::vector<NodeInfo> ActiveNodes;
};

// Forward declaration for the UDP discovery types we need
namespace udpdiscovery {
    struct IpPort;
    class Peer;
    struct DiscoveredPeer;
    struct PeerParameters;
}

namespace BG {
namespace Common {
namespace Logger {
    class LoggingSystem;
} // namespace Logger
} // namespace Common
} // namespace BG



/**
 * @class DiscoveryManager
 * @brief Manages node discovery, leader election, and health monitoring in a distributed cluster
 */
class DiscoveryManager {
public:
    /// Callback type for node join events
    using NodeJoinCallback = std::function<void(const NodeInfo&)>;
    
    /// Callback type for node leave events
    using NodeLeaveCallback = std::function<void(const NodeInfo&)>;
    
    /// Callback type for leader change events
    using LeaderChangeCallback = std::function<void(const NodeInfo& oldLeader, const NodeInfo& newLeader)>;

    /// Callback type for role change events
    using RoleChangeCallback = std::function<void(ClusterRole oldRole, ClusterRole newRole, const ClusterState& state)>;

    /**
     * @brief Constructs a new Discovery Manager
     */
    DiscoveryManager(
        uint64_t ApplicationId,
        unsigned int MulticastAddress,
        int Port,
        const std::string& NodeId,
        BG::Common::Logger::LoggingSystem* Logger,
        int LeaderElectionTimeoutMs = 10000,
        int LeaderHeartbeatIntervalMs = 2000,
        int NodeTimeoutMs = 15000,
        int HealthCheckIntervalMs = 3000,
        int MaxBackoffMs = 3000,
        int LeaderStabilityGracePeriodMs = 5000,
        bool PreferLongestRunningLeader = true,
        bool UseBroadcast = true,
        bool UseMulticast = true,
        std::string AdvertisedHost = "127.0.0.1"
    );

    /**
     * @brief Destroys the Discovery Manager
     */
    ~DiscoveryManager();

    bool Start();
    void Stop();
    bool IsLeader(bool _Lock = true) const;
    std::string GetLeaderNodeId() const;
    std::string GetLeaderAddress(bool _LockMutex = true) const;
    std::vector<NodeInfo> GetKnownNodes() const;
    std::vector<NodeInfo> GetActiveNodes(bool _Lock) const;
    void StepDown();
    std::string GetNodeId() const;
    size_t GetClusterSize() const;
    void ForceHealthCheck();
    ClusterState GetClusterState(bool _Lock = true) const;
    std::string GetAdvertisedHost();

    NodeInfo GetThisNodeInfo();

    void RegisterNodeJoinCallback(NodeJoinCallback callback);
    void RegisterNodeLeaveCallback(NodeLeaveCallback callback);
    void RegisterLeaderChangeCallback(LeaderChangeCallback callback);
    void RegisterRoleChangeCallback(RoleChangeCallback callback);

private:
    /// Pimpl idiom to hide implementation details
    class Impl;
    std::unique_ptr<Impl> Pimpl_;
};