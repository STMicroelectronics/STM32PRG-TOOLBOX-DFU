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
#include "DisplayManager.h"
#include "Error.h"
#include <thread>
#include <chrono>
#include <vector>

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
    bool isUbootDfuRunning(uint32_t msTimeout = 1000) ;
    bool isUbootFastbootRunning(uint32_t msTimeout = 1000) ;
    bool isDfuDeviceExist(uint32_t msTimeout = 1000) ;
    int getDeviceID() ;
    int readOtpPartition(const std::string filePath) ;
    int writeOtpPartition(const std::string filePath) ;
    bool isDfuUtilInstalled() ;
    int getAlternateSettingIndex(const std::string altName, uint8_t *altIndex);
    int displayDevicesList() ;

    uint16_t deviceID ;
    std::string otpPartitionName ;
    bool isSTM32PRGFW_UTIL ;
    std::string toolboxFolder = "" ;
    std::string dfuSerialNumber = "" ;
    std::vector<std::pair<int, std::string>> altSettingList ;

private:
    DisplayManager displayManager = DisplayManager::getInstance() ;
    std::string getDfuUtilProgramPath() ;
    std::string getLsUsbProgramPath() ;
    int getAlternateSettingList();
};

#endif // DFU_H
