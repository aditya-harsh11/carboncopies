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

#pragma once

#include <sycl/sycl.hpp>
#include <string>
#include <memory>
#include <vector>

/**
 * @brief Enum representing the type of SYCL device
 */
enum SYCLDeviceType {
    SYCL_DEVICE_TYPE_GPU,
    SYCL_DEVICE_TYPE_CPU,
    SYCL_DEVICE_TYPE_OTHER
};

/**
 * @brief Simple SYCL device information container (C-style struct)
 *
 * Passive struct holding essential device info and queue
 */
struct SYCLDevice {
    std::unique_ptr<sycl::queue> Queue;          ///< SYCL command queue for this device
    sycl::device Device;                        ///< SYCL device handle
    std::string Name;                           ///< Device name string
    std::string Vendor;                         ///< Vendor name string
    size_t VRAM_MB;                             ///< VRAM size in megabytes
    enum SYCLDeviceType Type;                   ///< Type of the device (GPU, CPU, OTHER)
};



/**
 * @brief Initializes a SYCLDevice struct from a SYCL device
 * @param dev SYCL device to wrap
 * @return SYCLDevice struct initialized with device info
 */
SYCLDevice InitSYCLDevice(sycl::device dev);