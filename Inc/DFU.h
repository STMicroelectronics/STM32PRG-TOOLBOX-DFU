/*
 * PRG-TOOLBOX-DFU
 *
 * (C) 2024 by STMicroelectronics.
 *
 * Integrates dfu-util v0.11 for Windows platform.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef DFU_H
#define DFU_H

#include <iostream>
#include "Inc/DisplayManager.h"
#include "Inc/Error.h"
#include <thread>
#include <chrono>

enum STM32MP_DEVICE {
    STM32MP15 = 0x500,
    STM32MP13 = 0x501,
    STM32MP25 = 0x505
};

class DFU
{
public:
    DFU();
    int flashPartition(uint8_t partitionIndex, const std::string inputFirmwarePath) ;
    int dfuDetach() ;
    bool isUbootDfuRunning() ;
    bool isUbootDfuRunningTimeout() ;
    bool isUbootFastbootRunning() ;
    bool isUbootFastbootRunningTimeout() ;
    bool isDfuDeviceExist() ;
    int getDeviceID() ;
    int readOtpPartition(const std::string filePath) ;
    int writeOtpPartition(const std::string filePath) ;
    bool isDfuUtilInstalled() ;

    uint16_t deviceID ;
    std::string otpPartitionName ;
    bool isSTM32PRGFW_UTIL ;
    std::string toolboxFolder = "" ;

private:
    DisplayManager displayManager = DisplayManager::getInstance() ;
    std::string getDfuUtilProgramPath() ;
    std::string getLsUsbProgramPath() ;
};

#endif // DFU_H
