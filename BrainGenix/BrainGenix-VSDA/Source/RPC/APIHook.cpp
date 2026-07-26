// APIHook.cpp - Updated HandleBatchQuery function
#include "APIHook.h"
#include <chrono>
#include <thread>
#include <stdexcept>
#include <rpc/client.h>
#include <nlohmann/json.hpp>

APIHook::APIHook(BG::Common::Logger::LoggingSystem* _Logger, 
                 MessagingManager* _Manager)
    : Logger_(_Logger),
      MessagingManager_(_Manager),
      ApiPort_(0),
      ApiListenerPort_(9090),
      TimeoutMs_(5000) {}

APIHook::~APIHook() {
    std::lock_guard<std::mutex> lock(Mutex_);
    if (ApiRpc_) {
        ApiRpc_->Stop();
    }
}

void APIHook::SetTimeout(int _Timeout_ms) {
    TimeoutMs_ = _Timeout_ms;
}

void APIHook::SetAPIHost(std::string _Host, int _Port) {
    ApiHost_ = _Host;
    ApiPort_ = _Port;
}

void APIHook::SetAPIPort(int _Port) {
    ApiListenerPort_ = _Port;
}

void APIHook::RegisterQueryHandler(const std::string& queryType, QueryHandler handler) {
    std::lock_guard<std::mutex> lock(Mutex_);
    queryHandlers_[queryType] = handler;
}

void APIHook::Setup() {
    MessagingManager_->RegisterLeaderLoop(
        [this](std::atomic_bool* _ShouldContinue) {
            this->LeaderLoop(_ShouldContinue);
        });
}

BidirectionalRpc* APIHook::GetRPC() {
    std::lock_guard<std::mutex> lock(Mutex_);
    return ApiRpc_.get();
}

std::string APIHook::HandleBatchQuery(const std::string& batchRequestStr) {
    try {
        nlohmann::json batchRequest = nlohmann::json::parse(batchRequestStr);
        nlohmann::json batchResponse = nlohmann::json::array();
        
        if (!batchRequest.is_array()) {
            throw std::runtime_error("Batch request must be an array");
        }
        
        for (const auto& query : batchRequest) {
            nlohmann::json response;
            
            try {
                // Extract ReqID from the query (provided by client)
                if (query.contains("ReqID") && query["ReqID"].is_number()) {
                    uint64_t reqId = query["ReqID"].get<uint64_t>();
                    response["ReqID"] = reqId;
                } else {
                    // If client didn't provide ReqID, we'll still process but include error
                    response["ReqID"] = -1;
                    response["error"] = "Missing ReqID in query";
                    batchResponse.push_back(response);
                    continue;
                }
                
                // Find the method name (key that isn't "ReqID")
                std::string methodName;
                for (auto it = query.begin(); it != query.end(); ++it) {
                    if (it.key() != "ReqID") {
                        methodName = it.key();
                        break;
                    }
                }
                
                if (methodName.empty()) {
                    response["error"] = "No method specified in query";
                } else if (queryHandlers_.find(methodName) == queryHandlers_.end()) {
                    response["error"] = "Unknown method: " + methodName;
                } else {
                    // Get the parameters for this method as JSON string
                    nlohmann::json params = query.value(methodName, nlohmann::json::object());
                    std::string paramsStr = params.dump();
                    
                    // Call the handler with the parameters string
                    std::string resultStr = queryHandlers_[methodName](paramsStr);
                    
                    // Parse the result string back to JSON and add to response
                    response["result"] = nlohmann::json::parse(resultStr);
                    response["success"] = true;
                }
            } catch (const std::exception& e) {
                response["error"] = std::string("Exception processing query: ") + e.what();
            }
            
            batchResponse.push_back(response);
        }
        
        return batchResponse.dump();
    } catch (const std::exception& e) {
        nlohmann::json errorResponse;
        errorResponse["error"] = std::string("Failed to process batch request: ") + e.what();
        return errorResponse.dump();
    }
}

void APIHook::LeaderLoop(std::atomic_bool* _ShouldContinue) {
    std::chrono::steady_clock::time_point lastConnectionTime;
    bool wasConnected = false;
    
    while (*_ShouldContinue) {
        {
            std::lock_guard<std::mutex> lock(Mutex_);
            
            // Create bidirectional RPC server if it doesn't exist
            if (!ApiRpc_) {
                try {
                    // Create bidirectional RPC server that listens on our API port
                    ApiRpc_ = std::make_unique<BidirectionalRpc>(ApiListenerPort_, true, Logger_, TimeoutMs_, "APIServer");
                    ApiRpc_->SetAdvertisedHost(MessagingManager_->GetAdvertisedHost());
                    
                    // Bind the batch query handler - takes string, returns string
                    ApiRpc_->Bind("VSDA", [this](const std::string& jsonRequest) -> std::string {
                        return this->HandleBatchQuery(jsonRequest);
                    });
                    
                    // Start the server (this makes us listen on ApiListenerPort_)
                    ApiRpc_->Start();
                    
                    if (Logger_) {
                        Logger_->Log("[APIHook] Listening for API server connections on port " + 
                                    std::to_string(ApiListenerPort_), 4);
                    }
                    
                    // Reset registration and connection tracking
                    isRegistered_ = false;
                    wasConnected = false;
                } catch (const std::exception& e) {
                    if (Logger_) {
                        Logger_->Log("[APIHook] Failed to start RPC server on port " + 
                                    std::to_string(ApiListenerPort_) + ": " + e.what(), 8);
                    }
                    ApiRpc_.reset();
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    continue;
                }
            }
            
            // Register with API server if we have a server but haven't registered
            if (ApiRpc_ && !isRegistered_ && !ApiRpc_->IsConnected()) {
                try {
                    // Create a temporary client to register with the API server
                    rpc::client registrationClient(ApiHost_, ApiPort_);
                    registrationClient.set_timeout(TimeoutMs_);
                    
                    // Register ourselves with the API server using JSON format
                    std::string nodeId = MessagingManager_->GetNodeId();
                    std::string advertisedHost = MessagingManager_->GetAdvertisedHost();
                    
                    nlohmann::json registerRequest;
                    registerRequest["nodeId"] = nodeId;
                    registerRequest["host"] = advertisedHost;
                    registerRequest["port"] = ApiListenerPort_;
                    
                    std::string registerRequestStr = registerRequest.dump();
                    
                    // Call the JSON endpoint with the JSON string as a single parameter
                    std::string registerResponse = registrationClient.call("RegisterVSDANode", registerRequestStr).as<std::string>();
                    
                    nlohmann::json responseJson = nlohmann::json::parse(registerResponse);
                    if (!responseJson["success"].get<bool>()) {
                        throw std::runtime_error("Registration failed: " + responseJson["error"].get<std::string>());
                    }
                    
                    // Announce ourselves as the leader using JSON format
                    nlohmann::json leaderRequest;
                    leaderRequest["nodeId"] = nodeId;
                    
                    std::string leaderRequestStr = leaderRequest.dump();
                    
                    // Call the JSON endpoint with the JSON string as a single parameter
                    std::string leaderResponse = registrationClient.call("SetVSDALeader", leaderRequestStr).as<std::string>();
                    
                    nlohmann::json leaderResponseJson = nlohmann::json::parse(leaderResponse);
                    if (!leaderResponseJson["success"].get<bool>()) {
                        throw std::runtime_error("Leader announcement failed: " + leaderResponseJson["error"].get<std::string>());
                    }
                    
                    isRegistered_ = true;
                    lastConnectionTime = std::chrono::steady_clock::now();
                    
                    if (Logger_) {
                        Logger_->Log("[APIHook] Successfully registered with API server at " +
                                    ApiHost_ + ":" + std::to_string(ApiPort_) + 
                                    " as node: " + nodeId, 4);
                    }
                } catch (const std::exception& e) {
                    if (Logger_) {
                        Logger_->Log("[APIHook] Failed to register with API server: " + 
                                    std::string(e.what()), 8);
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            }
            
            // Check if our bidirectional connection is established
            if (ApiRpc_ && ApiRpc_->IsConnected()) {
                // Connection is healthy
                wasConnected = true;
                lastConnectionTime = std::chrono::steady_clock::now();
                
                if (Logger_) {
                    Logger_->Log("[APIHook] Bidirectional connection with API server is active", 4);
                }
            } else if (ApiRpc_ && isRegistered_) {
                // Connection is lost but we were registered
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastConnectionTime).count();
                
                // If we've been disconnected for too long, reset everything
                if (elapsed > 30) { // 30 second timeout
                    if (Logger_) {
                        Logger_->Log("[APIHook] API server connection lost for " + 
                                    std::to_string(elapsed) + " seconds. Re-registering.", 8);
                    }
                    
                    // Reset the RPC server to trigger re-registration
                    ApiRpc_->Stop();
                    ApiRpc_.reset();
                    isRegistered_ = false;
                    wasConnected = false;
                } else if (wasConnected) {
                    // We had a connection but lost it recently
                    if (Logger_) {
                        Logger_->Log("[APIHook] API server connection lost, waiting for reconnect (" + 
                                    std::to_string(30 - elapsed) + "s remaining)", 5);
                    }
                } else {
                    // Waiting for initial connection after registration
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastConnectionTime).count();
                    
                    if (elapsed > 30) {
                        if (Logger_) {
                            Logger_->Log("[APIHook] Timeout waiting for API server connection. Re-registering.", 8);
                        }
                        // Reset registration to trigger re-registration
                        ApiRpc_->Stop();
                        ApiRpc_.reset();
                        isRegistered_ = false;
                    } else {
                        if (Logger_) {
                            Logger_->Log("[APIHook] Waiting for API server to connect to port " + 
                                        std::to_string(ApiListenerPort_) + 
                                        " (" + std::to_string(30 - elapsed) + "s remaining)", 5);
                        }
                    }
                }
            }
        }
        
        // Sleep before next check
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    // Cleanup when leadership is lost
    std::lock_guard<std::mutex> lock(Mutex_);
    if (ApiRpc_) {
        if (Logger_) {
            Logger_->Log("[APIHook] Stopping bidirectional RPC server", 4);
        }
        ApiRpc_->Stop();
        ApiRpc_.reset();
    }
    isRegistered_ = false;
    
    if (Logger_) {
        Logger_->Log("[APIHook] Leader loop stopped", 4);
    }
}