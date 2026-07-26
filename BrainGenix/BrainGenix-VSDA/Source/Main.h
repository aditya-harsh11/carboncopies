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

#pragma once

// Standard Libraries (BG convention: use <> instead of "")

// Third-Party Libraries (BG convention: use <> instead of "")

// Internal Libraries (BG convention: use <> instead of "")
#include <BG/Common/Logger/Logger.h>

#include <SYCL/DeviceDetector.h>
#include <SYCL/DeviceBenchmark.h>

// #include <Cluster/DistributedManager.h>
#include <Cluster/Discovery.h>
#include <Cluster/MessagingManager.h>

#include <VSDA/Common/Structs/VoxelArray.h>

#include <ConfigParser/ConfigParser.h>