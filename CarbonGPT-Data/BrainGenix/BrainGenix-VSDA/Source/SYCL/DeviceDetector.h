//=================================//
// This file is part of BrainGenix //
//=================================//

/*
    Description:
    Additional Notes: None
    Date Created: 2025-08-29
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

#pragma once

#include <sycl/sycl.hpp>
#include <string>
#include <memory>
#include <vector>

#include <SYCL/Device.h>

#include <BG/Common/Logger/Logger.h>




/**
 * @brief Detects and initializes available SYCL devices with logging support.
 * 
 * This function scans the system for available SYCL devices, logs information about
 * all discovered devices, and returns a vector of initialized SYCLDevice structures.
 * The function prioritizes GPU devices and falls back to CPU devices if no GPUs
 * are available. If no suitable devices are found, an exception is thrown.
 * 
 * @param[in] _Logger Pointer to the logging system instance for outputting device
 *                    detection information and status messages. The logger should
 *                    be properly initialized before calling this function.
 * 
 * @return std::vector<SYCLDevice> Vector containing initialized SYCLDevice structures
 *         for all detected and selected devices. The vector will contain:
 *         - All available GPU devices (preferred)
 *         - Or a single CPU device (fallback)
 *         - Or the first available device (last resort)
 * 
 * @throws std::runtime_error If no SYCL devices are found on the system.
 * 
 * @note The function logs detailed information about the detection process at
 *       various log levels (2-5) using the provided logging system.
 * 
 * @see SYCLDevice
 * @see InitSYCLDevice
 * 
 * @par Example:
 * @code
 * BG::Common::Logger::LoggingSystem Logger;
 * std::vector<SYCLDevice> devices = DetectDevices(&Logger);
 * for (const auto& device : devices) {
 *     std::cout << "Using device: " << device.Name << std::endl;
 * }
 * @endcode
 */
std::vector<SYCLDevice> DetectDevices(BG::Common::Logger::LoggingSystem* _Logger);


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
 * 
 * @par Example:
 * @code
 * std::vector<SYCLDevice> devices = DetectDevices(&Logger);
 * LogDevices(&Logger, &devices);
 * @endcode
 */
void LogDevices(BG::Common::Logger::LoggingSystem* _Logger, std::vector<SYCLDevice>* _Devices);