/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2012,2013,2014,2015,2017 The GROMACS development team.
 * Copyright (c) 2018,2019,2020, by the GROMACS development team, led by
 * Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
 * and including many others, as listed in the AUTHORS file in the
 * top-level source directory and at http://www.gromacs.org.
 *
 * GROMACS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * GROMACS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GROMACS; if not, see
 * http://www.gnu.org/licenses, or write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *
 * If you want to redistribute modifications to GROMACS, please
 * consider that scientific software is very special. Version
 * control is crucial - bugs must be traceable. We will be happy to
 * consider code for inclusion in the official distribution, but
 * derived work must not be called official GROMACS. Details are found
 * in the README & COPYING files - if they are missing, get the
 * official version at http://www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the research papers on the package. Check out http://www.gromacs.org.
 */
/*! \internal \file
 *  \brief Defines the implementations of the DevicesManager class that are common for CPU, CUDA and OpenCL.
 *
 *  \author Artem Zhmurov <zhmurov@gmail.com>
 *
 * \ingroup module_hardware
 */
#include "gmxpre.h"

#include "config.h"

#include "gromacs/hardware/device_information.h"
#include "gromacs/hardware/device_management.h"
#include "gromacs/utility/fatalerror.h"

bool canPerformDeviceDetection(std::string* errorMessage)
{
    if (GMX_GPU && getenv("GMX_DISABLE_GPU_DETECTION") == nullptr)
    {
        return isDeviceDetectionFunctional(errorMessage);
    }
    else
    {
        return false;
    }
}

bool canComputeOnDevice()
{
    bool canComputeOnDevice = false;
    if (canPerformDeviceDetection(nullptr))
    {
        std::vector<std::unique_ptr<DeviceInformation>> devInfos = findDevices();
        canComputeOnDevice = !getCompatibleDevices(devInfos).empty();
    }
    return canComputeOnDevice;
}

std::vector<int> getCompatibleDevices(const std::vector<std::unique_ptr<DeviceInformation>>& deviceInfos)
{
    // Possible minor over-allocation here, but not important for anything
    std::vector<int> compatibleGpus;
    compatibleGpus.reserve(deviceInfos.size());
    for (const auto& deviceInfo : deviceInfos)
    {
        if (deviceInfo->status == DeviceStatus::Compatible)
        {
            compatibleGpus.push_back(deviceInfo->id);
        }
    }
    return compatibleGpus;
}

std::string getDeviceCompatibilityDescription(const std::vector<std::unique_ptr<DeviceInformation>>& deviceInfos,
                                              int deviceId)
{
    return (deviceId >= static_cast<int>(deviceInfos.size())
                    ? c_deviceStateString[DeviceStatus::Nonexistent]
                    : c_deviceStateString[deviceInfos[deviceId]->status]);
}

void serializeDeviceInformations(const std::vector<std::unique_ptr<DeviceInformation>>& deviceInfos,
                                 gmx::ISerializer*                                      serializer)
{
    int numDevices = deviceInfos.size();
    serializer->doInt(&numDevices);
    for (auto& deviceInfo : deviceInfos)
    {
        serializer->doOpaque(reinterpret_cast<char*>(deviceInfo.get()), sizeof(DeviceInformation));
    }
}

std::vector<std::unique_ptr<DeviceInformation>> deserializeDeviceInformations(gmx::ISerializer* serializer)
{
    int numDevices = 0;
    serializer->doInt(&numDevices);
    std::vector<std::unique_ptr<DeviceInformation>> deviceInfos(numDevices);
    for (int i = 0; i < numDevices; i++)
    {
        deviceInfos[i] = std::make_unique<DeviceInformation>();
        serializer->doOpaque(reinterpret_cast<char*>(deviceInfos[i].get()), sizeof(DeviceInformation));
    }
    return deviceInfos;
}
