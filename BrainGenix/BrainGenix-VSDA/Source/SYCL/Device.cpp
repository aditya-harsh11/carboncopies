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


#include <SYCL/Device.h>


/**
 * @brief Initializes a SYCLDevice struct from a SYCL device
 * @param dev SYCL device to wrap
 * @return SYCLDevice struct initialized with device info
 */
SYCLDevice InitSYCLDevice(sycl::device dev) {
    SYCLDevice device_info;

    device_info.Device = dev;
    device_info.Queue = std::make_unique<sycl::queue>(dev, sycl::property::queue::in_order{});
    device_info.Name = dev.get_info<sycl::info::device::name>();
    device_info.Vendor = dev.get_info<sycl::info::device::vendor>();
    device_info.VRAM_MB = dev.get_info<sycl::info::device::global_mem_size>() / (1024 * 1024);

    if (dev.is_gpu()) {
        device_info.Type = SYCL_DEVICE_TYPE_GPU;
    } else if (dev.is_cpu()) {
        device_info.Type = SYCL_DEVICE_TYPE_CPU;
    } else {
        device_info.Type = SYCL_DEVICE_TYPE_OTHER;
    }

    return device_info;
}
