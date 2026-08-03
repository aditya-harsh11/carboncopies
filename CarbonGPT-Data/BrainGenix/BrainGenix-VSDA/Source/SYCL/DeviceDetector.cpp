//=================================//
// This file is part of BrainGenix //
//=================================//

/*
    Description:
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


#include <SYCL/DeviceDetector.h>


std::vector<SYCLDevice> DetectDevices(BG::Common::Logger::LoggingSystem* _Logger) {
    _Logger->Log("Starting SYCL Device Detection", 5);
    
    std::vector<sycl::device> devices = sycl::device::get_devices();

    if (devices.empty()) {
        _Logger->Log("No SYCL devices found!", 2);
        throw std::runtime_error("No SYCL devices found!");
    }

    // Log all available devices
    _Logger->Log("Available SYCL Devices:", 4);
    for (size_t i = 0; i < devices.size(); i++) {
        auto& dev = devices[i];
        std::string deviceType;
        if (dev.is_gpu()) deviceType = "GPU";
        else if (dev.is_cpu()) deviceType = "CPU";
        else deviceType = "Other";
        
        _Logger->Log("Device " + std::to_string(i) + ": " + 
                    dev.get_info<sycl::info::device::name>() + " (" + deviceType + ")", 4);
    }

    std::vector<SYCLDevice> selectedDevices;
    
    // First try to find all GPUs
    std::vector<sycl::device> gpuDevices;
    for (auto& dev : devices) {
        if (dev.is_gpu()) {
            gpuDevices.push_back(dev);
        }
    }

    if (!gpuDevices.empty()) {
        _Logger->Log("Found " + std::to_string(gpuDevices.size()) + " GPU device(s), using them", 3);
        for (auto& gpu : gpuDevices) {
            selectedDevices.push_back(InitSYCLDevice(gpu));
        }
        return selectedDevices;
    }

    // Fallback to CPU if no GPUs found
    _Logger->Log("No GPU devices found, falling back to CPU", 3);
    for (auto& dev : devices) {
        if (dev.is_cpu()) {
            selectedDevices.push_back(InitSYCLDevice(dev));
            break; // Only use one CPU device as fallback
        }
    }

    // If no CPU found, use first available device
    if (selectedDevices.empty()) {
        _Logger->Log("No CPU devices found, using first available device", 2);
        selectedDevices.push_back(InitSYCLDevice(devices[0]));
    }

    return selectedDevices;
}


/**
 * @brief Logs detailed information about a collection of SYCL devices.
 * 
 * This function iterates through the provided vector of SYCLDevice structures
 * and logs comprehensive information about each device including name, vendor,
 * VRAM capacity, device type, and other relevant details. The logging is
 * performed at informational level (level 4) for normal operation.
 * 
 * @param[in] _Logger Pointer to the logging system instance for outputting
 *                    device information. Must not be null.
 * @param[in] _Devices Pointer to a vector of SYCLDevice structures to be logged.
 *                     The function will handle null pointers and empty vectors gracefully.
 * 
 * @note This function is useful for debugging and system information reporting
 *       after device detection has been completed.
 * 
 * @see DetectDevices
 * @see SYCLDevice
 */
void LogDevices(BG::Common::Logger::LoggingSystem* _Logger, std::vector<SYCLDevice>* _Devices) {
    if (_Devices == nullptr) {
        _Logger->Log("No devices to log - devices pointer is null", 4);
        return;
    }
    
    if (_Devices->empty()) {
        _Logger->Log("No devices to log - devices vector is empty", 4);
        return;
    }

    _Logger->Log("Logging information for " + std::to_string(_Devices->size()) + " SYCL device(s)", 4);
    
    for (size_t i = 0; i < _Devices->size(); i++) {
        const auto& device = (*_Devices)[i];
        
        std::string deviceTypeStr;
        switch (device.Type) {
            case SYCL_DEVICE_TYPE_GPU:
                deviceTypeStr = "GPU";
                break;
            case SYCL_DEVICE_TYPE_CPU:
                deviceTypeStr = "CPU";
                break;
            case SYCL_DEVICE_TYPE_OTHER:
                deviceTypeStr = "Other";
                break;
            default:
                deviceTypeStr = "Unknown";
                break;
        }

        _Logger->Log("Device " + std::to_string(i) + ":", 4);
        _Logger->Log("  Name: " + device.Name, 4);
        _Logger->Log("  Vendor: " + device.Vendor, 4);
        _Logger->Log("  Type: " + deviceTypeStr, 4);
        _Logger->Log("  VRAM: " + std::to_string(device.VRAM_MB) + " MB", 4);
        
        // Log additional device information if available
        try {
            auto maxWorkGroupSize = device.Device.get_info<sycl::info::device::max_work_group_size>();
            _Logger->Log("  Max Work Group Size: " + std::to_string(maxWorkGroupSize), 4);
        } catch (...) {
            _Logger->Log("  Max Work Group Size: Unavailable", 4);
        }
        
        _Logger->Log("", 4); // Empty line for readability
    }
    
    _Logger->Log("Finished logging device information", 4);
}