#pragma once

#include <Cluster/MessagingManager.h>
#include <Cluster/BidirectionalRPC.h>
#include <BG/Common/Logger/Logger.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <functional>
#include <unordered_map>
#include <nlohmann/json.hpp>

/**
 * @class APIHook
 * @brief Establishes bidirectional RPC communication with API server when node is leader.
 *        Handles batch JSON queries through the /VSDA route.
 */
class APIHook {
public:
    using QueryHandler = std::function<std::string(const std::string&)>;
    
    APIHook(BG::Common::Logger::LoggingSystem* _Logger, MessagingManager* _Manager);
    ~APIHook();

    void SetTimeout(int _Timeout_ms);
    void SetAPIHost(std::string _Host, int _Port);
    void SetAPIPort(int _Port);
    void Setup();
    BidirectionalRpc* GetRPC();
    
    // Register a handler for specific query types
    void RegisterQueryHandler(const std::string& queryType, QueryHandler handler);

private:
    void LeaderLoop(std::atomic_bool* _ShouldContinue);
    std::string HandleBatchQuery(const std::string& batchRequestStr);
    
    BG::Common::Logger::LoggingSystem* Logger_;
    MessagingManager* MessagingManager_;
    std::unique_ptr<BidirectionalRpc> ApiRpc_;
    std::string ApiHost_;
    int ApiPort_;
    int ApiListenerPort_;
    int TimeoutMs_;
    std::mutex Mutex_;
    std::atomic<bool> isRegistered_{false};
    
    // Map of query type to handler functions
    std::unordered_map<std::string, QueryHandler> queryHandlers_;
};