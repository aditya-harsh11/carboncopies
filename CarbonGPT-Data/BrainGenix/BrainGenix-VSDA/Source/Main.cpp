//=================================//
// This file is part of BrainGenix //
//=================================//

/*
    Description: Main entry point.
    Additional Notes: None
    Date Created: 2025-08-28
    Author(s): Thomas Liao


    Copyright (C) 2025  Thomas Liao

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/


#include <Main.h>


#include <Cluster/GetHostname.h>
#include <Cluster/IPStringToUint.h>

#include <Cluster/ClusterHwAggregator.h>

#include <RPC/APIHook.h>


int main(int argc, char** argv) {

    // Setup Logging System
    BG::Common::Logger::LoggingSystem Logger;
    Logger.SetKeepVectorLogs(false);
    // Example call: Logger.Log("Foo", 3); <- number is severity (0-10), 0 is debug, 5 info, 6 warn, 7 error, 8 critical, 9 more crit, 10 fatal

    // Log all command-line arguments
    Logger.Log("[CLI] Command-line arguments:", 1);
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--", 0) == 0) {  // Starts with "--"
            if (i + 1 < argc && argv[i + 1][0] != '-') {  // Next arg is not a flag or pair
                Logger.Log("[CLI]   " + arg + " " + argv[i + 1], 1);
                i++;  // Skip the next argument
            } else {
                Logger.Log("[CLI]   " + arg, 1);
            }
        } else if (arg.rfind("-", 0) == 0) {  // Starts with "-"
            Logger.Log("[CLI]   " + arg, 1);
        } else {  // Standalone argument
            Logger.Log("[CLI]   " + arg, 1);
        }
    }
    
    // Initialize Config Manager
    Logger.Log("[MAIN] Loading System Configuration", 5);
    ConfigParser ConfigManager("Config.yaml", argc, argv, &Logger);
    Logger.Log("[MAIN] System Configuration Loaded", 5);


    // Sycl Initialization
    std::vector<SYCLDevice> Devices = DetectDevices(&Logger);
    LogDevices(&Logger, &Devices);


    // Initialize Distributed Manager
    Logger.Log("[MAIN] Initializing Distributed Manager", 5);
    DiscoveryManager ClusterDiscoveryManager(
        ConfigManager.GetInt("Discovery.ApplicationID", 7681412),
        IpStringToUint(ConfigManager.GetString("Discovery.MulticastAddress", "224.0.0.123")),
        ConfigManager.GetInt("Discovery.Port", 12021),
        "node-" + GetHostName(),
        &Logger,
        ConfigManager.GetInt("Discovery.LeaderElectionTimeoutMs", 10000),
        ConfigManager.GetInt("Discovery.LeaderHeartbeatIntervalMs", 2000),
        ConfigManager.GetInt("Discovery.NodeTimeoutMs", 15000),
        ConfigManager.GetInt("Discovery.HealthCheckIntervalMs", 3000),
        ConfigManager.GetInt("Discovery.MaxBackoffMs", 3000),
        ConfigManager.GetInt("Discovery.LeaderStabilityGracePeriodMs", 5000),
        ConfigManager.GetBool("Discovery.PreferLongestRunningLeader", true),
        ConfigManager.GetBool("Discovery.UseBroadcast", true),
        ConfigManager.GetBool("Discovery.UseMulticast", true),
        ConfigManager.GetString("Routing.AdvertisedHost", "127.0.0.1")
    );
    
    // Create messaging manager
    MessagingManager ClusterMessageManager(&ClusterDiscoveryManager, &Logger, ConfigManager.GetInt("RPC.ClusterBasePort", 5555));



    // Now initialize and run messaging systems
    if (!ClusterMessageManager.Start()) {
        Logger.Log("[MAIN] Failed to start message manager", 1);
        return 1;
    }

    if (!ClusterDiscoveryManager.Start()) {
        Logger.Log("[MAIN] Failed to start discovery manager", 1);
        return 1;
    }

    Logger.Log("[MAIN] Node ID: " + ClusterDiscoveryManager.GetNodeId(), 4);




    // Setup Inter-Node RPC Endpoints
    ClusterHwAggregator HWAggregator(&ClusterMessageManager, &Devices);
    ClusterMessageManager.RegisterLeaderLoop(HWAggregator.GetLeaderLoop());


    // Now setup Leader-API Bidir RPC 
    APIHook Hook(&Logger, &ClusterMessageManager);
    Hook.SetTimeout(ConfigManager.GetInt("APIServer.RPCTimeout_ms", 10000));
    Hook.SetAPIHost(ConfigManager.GetString("APIServer.RPCHost", "127.0.0.1"), ConfigManager.GetInt("APIServer.RPCPort", 7999));


    // Register hardware aggregator as a query handler
    Hook.RegisterQueryHandler("GetClusterHwInfo", [&HWAggregator](const std::string& paramsStr) -> std::string {
        try {
            std::string hwInfo = HWAggregator.GetClusterHwInfo();
            return hwInfo; // Return as string directly
        } catch (const std::exception& e) {
            nlohmann::json error;
            error["error"] = std::string("Failed to get cluster hardware info: ") + e.what();
            return error.dump();
        }
    });

    Hook.Setup();


    // // Perform Device Performance Profiling
    // Logger.Log("Starting Device Performance Profiling", 5);
    // std::vector<SYCLDevicePerformanceInfo> device_performance = BenchmarkDevices(devices);
    // for (unsigned int i = 0; i < device_performance.size(); i++) {
    //     Logger.Log("Device " + std::to_string(i) + " Performance:", 4);
    //     Logger.Log("  Memory Bandwidth: " + std::to_string(device_performance[i].MemoryBandwidthGBs) + " GB/s", 4);
    //     Logger.Log("  FP32 Compute Performance: " + std::to_string(device_performance[i].GFLOPsFP32) + " GFLOPs", 4);
        
    //     if (device_performance[i].GFLOPsFP16 > 0) {
    //         Logger.Log("  FP16 Compute Performance: " + std::to_string(device_performance[i].GFLOPsFP16) + " GFLOPs", 4);
    //     } else {
    //         Logger.Log("  FP16 Compute Performance: Not supported", 4);
    //     }
        
    //     Logger.Log("", 4);
    // }
    // Logger.Log("Finished device performance profiling", 4);

    
    // Keep application running
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}