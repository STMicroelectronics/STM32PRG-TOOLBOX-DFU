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

#include "Inc/DFU.h"
#include <regex>
#include <iostream>
#include <experimental/filesystem>

DFU::DFU()
{
    deviceID = 0x0;
    otpPartitionName = "";
    isSTM32PRGFW_UTIL = false ;
}

/**
 * @brief DFU::flashPartition : Get the dfu-util command ready, then flash one partition.
 * @param partitionIndex: ALT index of the dedicated partition.
 * @param inputFirmwarePath: The firmware path to be programmed.
 * @return 0 if the operation is performed successfully, otherwise an error occurred.
 */
int DFU::flashPartition(uint8_t partitionIndex, const std::string inputFirmwarePath)
{
    displayManager.print(MSG_NORMAL, L"Partition index : %d", partitionIndex);
    displayManager.print(MSG_NORMAL, L"Firmware path   : %s", inputFirmwarePath.c_str());

    std::string utilCmd = getDfuUtilProgramPath().append("-d 483:df11") ;
    utilCmd.append(" -a ").append(std::to_string(partitionIndex)) ;
    utilCmd.append(" -D ").append(inputFirmwarePath) ;
#ifdef _WIN32
        utilCmd = "\"" + utilCmd + "\"" ;
#endif
    displayManager.print(MSG_NORMAL, L"DFU-UTIL command: %s", utilCmd.data()) ;

    FILE* pipe = popen(utilCmd.c_str(), "r");
    if (pipe == nullptr)
    {
        displayManager.print(MSG_ERROR, L"Failed to open pipe") ;
        return TOOLBOX_DFU_ERROR_NO_MEM;
    }

    char buffer[4096];
    std::string result = "";
    while (!feof(pipe))
    {
        if (fgets(buffer, 4096, pipe) != nullptr)
        {
            result += buffer;
        }
    }
    pclose(pipe);

    displayManager.print(MSG_NORMAL, L"OUTPUT: %s", result.data()) ;

    std::string searchString = "Download done.";
    size_t pos = result.find(searchString);
    if (pos != std::string::npos)
    {
        displayManager.print(MSG_GREEN, L"Phase ID %d : Download Done", partitionIndex) ;
        return TOOLBOX_DFU_NO_ERROR ;

    }
    else
    {
        displayManager.print(MSG_ERROR, L"Phase ID %d : Download Failed", partitionIndex) ;
        return TOOLBOX_DFU_ERROR_WRITE ;
    }
}

/**
 * @brief DFU::dfuDetach : Request to detach the device.
 * @return 0 if the operation is performed successfully, otherwise an error occurred.
 */
int DFU::dfuDetach()
{
    displayManager.print(MSG_NORMAL, L"DFU-UTIL command: -d 483:df11 -a 0 -e") ;
    std::string  utilCmd =  getDfuUtilProgramPath().append("-d 483:df11 -a 0 -e") ;
    int ret = std::system(utilCmd.c_str());
    if(ret == 0)
    {
        displayManager.print(MSG_GREEN, L"Detach Done") ;
        return TOOLBOX_DFU_NO_ERROR;
    }
    else
    {
        displayManager.print(MSG_ERROR, L"Detach Failed") ;
        return TOOLBOX_DFU_ERROR_OTHER ;
    }
}

/**
 * @brief DFU::isUbootDfutRunning : Verify if there is a U-Boot device runs in DFU mode.
 * @return True if an U-Boot in DFU mode is detected, otherwise, false.
 */
bool DFU::isUbootDfuRunning()
{
    std::string  utilCmd =  getDfuUtilProgramPath().append("-d 483:df11 -l") ; /* ST DFU PID:0483 VID:DF11 */
    displayManager.print(MSG_NORMAL, L"DFU-UTIL command: %s", utilCmd.data()) ;
    FILE* pipe = popen(utilCmd.c_str(), "r");
    if (pipe == nullptr)
    {
        displayManager.print(MSG_ERROR, L"Failed to open pipe") ;
        return false;
    }

    char buffer[4096];
    std::string result = "";

    while (!feof(pipe))
    {
        if (fgets(buffer, 4096, pipe) != nullptr)
        {
            result += buffer;
        }
    }
    pclose(pipe);

    try
    {
        std::regex pattern("name=(\"@OTP([^\\\"]*)\")");
        std::smatch match;
        if (std::regex_search(result, match, pattern))
        {
            otpPartitionName= match[1].str();
            displayManager.print(MSG_NORMAL, L"OTP partition name=%s", otpPartitionName.c_str()) ;
            displayManager.print(MSG_GREEN, L"U-Boot in DFU mode is running !") ;
        }
        else
        {
            displayManager.print(MSG_WARNING, L"U-Boot in DFU mode is not running !") ;
            return false ;
        }
    }
    catch (const std::regex_error& e)
    {
        displayManager.print(MSG_ERROR, L"Regex error: %s", e.what());
        return false ;
    }

    return true ;
}

/**
 * @brief DFU::isUbootDfuRunningTimeout : Verify if there is a U-Boot device runs in DFU mode with timeout checks.
 * @return True if an U-Boot in DFU mode is started, otherwise, false.
 */
bool DFU::isUbootDfuRunningTimeout()
{
    std::string  utilCmd =  getDfuUtilProgramPath().append("-d 483:df11 -l") ; /* ST DFU PID:0483 VID:DF11 */
    displayManager.print(MSG_NORMAL, L"DFU-UTIL command: %s", utilCmd.data()) ;

    bool isStarted = false ;
    // Set the timeout duration
    const std::chrono::seconds timeout_duration(30);
    const auto start_time = std::chrono::steady_clock::now();

    while (true)
    {
        // Check if the timeout has been reached
        const auto elapsed_time = std::chrono::steady_clock::now() - start_time;
        if (elapsed_time >= timeout_duration)
        {
             displayManager.print(MSG_WARNING, L"Timeout reached !") ;
            break;
        }

        // Perform some operation
        FILE* pipe = popen(utilCmd.c_str(), "r");
        if (pipe == nullptr)
        {
            displayManager.print(MSG_ERROR, L"Failed to open pipe") ;
            return false;
        }

        char buffer[4096];
        std::string result = "";

        while (!feof(pipe))
        {
            if (fgets(buffer, 4096, pipe) != nullptr)
            {
                result += buffer;
            }
        }
        pclose(pipe);

        try
        {
            std::regex pattern("name=(\"@OTP([^\\\"]*)\")");
            std::smatch match;
            if (std::regex_search(result, match, pattern))
            {
                isStarted = true ;
                break;
            }
        }
        catch (const std::regex_error& e)
        {
            displayManager.print(MSG_ERROR, L"Regex error: %s", e.what());
            return false ;
        }

        // Sleep for a short time to simulate work being done
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return isStarted;
}

/**
 * @brief DFU::isDfuDeviceExist : Verify the precense of a STM32 DFU device.
 * @return True if a device is present, otherwise, false.
 */
bool DFU::isDfuDeviceExist()
{
    std::string  utilCmd =  getDfuUtilProgramPath().append("-d 483:df11 -l") ; /* ST DFU PID:0483 VID:0AFB */
    FILE* pipe = popen(utilCmd.c_str(), "r");
    if (pipe == nullptr)
    {
        displayManager.print(MSG_ERROR, L"Failed to open pipe") ;
        return false;
    }

    char buffer[4096];
    std::string result = "";

    while (!feof(pipe))
    {
        if (fgets(buffer, 4096, pipe) != nullptr)
        {
            result += buffer;
        }
    }
    pclose(pipe);

    std::string searchString = "Found DFU: [0483:df11]";
    size_t pos = result.find(searchString);
    if (pos != std::string::npos)
    {
        return true ;
    }
    else
    {
        displayManager.print(MSG_ERROR, L"No STM32 DFU device is detected !") ;
        return false ;
    }
}

/**
 * @brief DFU::getDeviceID : Search and get the device of the connected DFU device.
 * @return The device ID value as STM32MP15, STM32MP13... If the value is 0, it indicates that there may be an issue with the device description.
 */
int DFU::getDeviceID()
{
    std::string  utilCmd =  getLsUsbProgramPath().append("-d 0483:df11 -v") ;
    FILE* pipe = popen(utilCmd.c_str(), "r");
    if (pipe == nullptr)
    {
        displayManager.print(MSG_ERROR, L"Failed to open pipe") ;
        return TOOLBOX_DFU_ERROR_OTHER;
    }

    char buffer[4096];
    std::string result = "";

    while (!feof(pipe))
    {
        if (fgets(buffer, 4096, pipe) != nullptr)
        {
            result += buffer;
        }
    }
    pclose(pipe);

    std::string searchString = "@Device ID /";
    size_t pos = result.find(searchString);
    if (pos != std::string::npos)
    {
        std::string deviceIdString = result.substr(pos + searchString.length(), 5) ;
        this->deviceID = std::stoul(deviceIdString, nullptr, 16);
        displayManager.print(MSG_GREEN, L"STM32 device ID = 0x%03X", this->deviceID) ;
        return TOOLBOX_DFU_NO_ERROR ;
    }
    else
    {
        displayManager.print(MSG_ERROR, L"Failed to extract the STM32 device ID") ;
        this->deviceID = 0 ;
        return TOOLBOX_DFU_ERROR_OTHER ;
    }
}

/**
 * @brief DFU::isUbootFastbootRunning . Verify if there is a U-Boot device runs in Fastboot mode.
 * @return True if an U-Boot in Fastboot mode is detected, otherwise, false.
 */
bool DFU::isUbootFastbootRunning()
{
    std::string  utilCmd =  getLsUsbProgramPath().append("-d 0483:0afb") ; /* ST Fastboot PID:0483 VID:0AFB */
    FILE* pipe = popen(utilCmd.c_str(), "r");
    if (pipe == nullptr)
    {
        displayManager.print(MSG_ERROR, L"Failed to open pipe") ;
        return false;
    }

    char buffer[4096];
    std::string result = "";

    while (!feof(pipe))
    {
        if (fgets(buffer, 4096, pipe) != nullptr)
        {
            result += buffer;
        }
    }
    pclose(pipe);

    std::string searchString = "ID 0483:0afb";
    size_t pos = result.find(searchString);
    if (pos != std::string::npos)
    {
        displayManager.print(MSG_GREEN, L"U-Boot in Fastboot mode is running !") ;
        return true ;

    }
    else
    {
        displayManager.print(MSG_WARNING, L"No U-Boot in Fastboot mode is running !") ;
        return false ;
    }
}

/**
 * @brief DFU::isUbootFastbootRunningTimeout . Verify if there is a U-Boot device runs in Fastboot mode with timeout checks.
 * @return True if an U-Boot in Fastboot mode is detected, otherwise, false.
 */
bool DFU::isUbootFastbootRunningTimeout()
{
    std::string  utilCmd =  getLsUsbProgramPath().append("-d 0483:0afb") ; /* ST Fastboot PID:0483 VID:0AFB */

    bool isRunning = false ;
    // Set the timeout duration
    const std::chrono::seconds timeout_duration(30);
    const auto start_time = std::chrono::steady_clock::now();

    while (true)
    {
        // Check if the timeout has been reached
        const auto elapsed_time = std::chrono::steady_clock::now() - start_time;
        if (elapsed_time >= timeout_duration)
        {
            displayManager.print(MSG_WARNING, L"Timeout reached !") ;
            break;
        }

        FILE* pipe = popen(utilCmd.c_str(), "r");
        if (pipe == nullptr)
        {
            displayManager.print(MSG_ERROR, L"Failed to open pipe") ;
            return false;
        }

        char buffer[4096];
        std::string result = "";

        while (!feof(pipe))
        {
            if (fgets(buffer, 4096, pipe) != nullptr)
            {
                result += buffer;
            }
        }
        pclose(pipe);

        std::string searchString = "ID 0483:0afb";
        size_t pos = result.find(searchString);
        if (pos != std::string::npos)
        {
            isRunning = true ;
            break ;
        }

        // Sleep for a short time to simulate work being done
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return isRunning;
}

/**
 * @brief DFU::readOtpPartition : Get the dfu-util command ready, then read the OTP partition and save it into file.
 * @param filePath: The output binary file to store OTP data.
 * @return 0 if the operation is performed successfully, otherwise an error occurred.
 */
int DFU::readOtpPartition(const std::string filePath)
{
    std::string  utilCmd =  getDfuUtilProgramPath().append("-d 0483:df11 -a ") ;

    if(otpPartitionName.empty()) // Then Check U-Boot and get the OTP partition name
    {
       bool state = isUbootDfuRunning() ;
       if(state == false)
           return TOOLBOX_DFU_ERROR_OTHER ;
    }
    utilCmd.append(otpPartitionName) ;
    utilCmd.append(" -U ").append(filePath) ;

#ifdef _WIN32
    utilCmd = "\"" + utilCmd + "\"" ;
#endif
    displayManager.print(MSG_NORMAL, L"DFU-UTIL command: %s", utilCmd.data()) ;

    FILE* pipe = popen(utilCmd.c_str(), "r");
    if (pipe == nullptr)
    {
        displayManager.print(MSG_ERROR, L"Failed to open pipe") ;
        return TOOLBOX_DFU_ERROR_OTHER;
    }

    char buffer[4096];
    std::string result = "";

    while (!feof(pipe))
    {
        if (fgets(buffer, 4096, pipe) != nullptr)
        {
            result += buffer;
        }
    }
    pclose(pipe);

    std::string searchString = "Upload done.";
    size_t pos = result.find(searchString);
    if (pos != std::string::npos)
    {
        displayManager.print(MSG_GREEN, L"Read OTP partition is done successfully !") ;
        return TOOLBOX_DFU_NO_ERROR ;
    }
    else
    {
        displayManager.print(MSG_ERROR, L"Read OTP partition is failed !") ;
        return TOOLBOX_DFU_ERROR_READ ;
    }
}

/**
 * @brief DFU::writeOtpPartition : Write a binary file into the OTP partition.
 * @param filePath: The input binary file to program.
 * @return 0 if the operation is performed successfully, otherwise an error occurred.
 * @note Be careful, using a bad binary file to fuse OTP can damage your device.
 */
int DFU::writeOtpPartition(const std::string filePath)
{
    std::string  utilCmd = getDfuUtilProgramPath().append("-d 0483:df11 -a ");

    if(otpPartitionName.empty()) // Then Check U-Boot and get the OTP partition name
    {
       bool state = isUbootDfuRunning() ;
       if(state == false)
           return TOOLBOX_DFU_ERROR_OTHER;
    }
    utilCmd.append(otpPartitionName) ;
    utilCmd.append(" -D ").append(filePath) ;

#ifdef _WIN32
    utilCmd = "\"" + utilCmd + "\"" ;
#endif

    displayManager.print(MSG_NORMAL, L"DFU-UTIL command: %s", utilCmd.data()) ;

    FILE* pipe = popen(utilCmd.c_str(), "r");
    if (pipe == nullptr)
    {
        displayManager.print(MSG_ERROR, L"Failed to open pipe") ;
        return TOOLBOX_DFU_ERROR_OTHER;
    }

    char buffer[4096];
    std::string result = "";

    while (!feof(pipe))
    {
        if (fgets(buffer, 4096, pipe) != nullptr)
        {
            result += buffer;
        }
    }
    pclose(pipe);

    std::string searchString = "Download done.";
    size_t pos = result.find(searchString);
    if (pos != std::string::npos)
    {
        displayManager.print(MSG_GREEN, L"Write OTP partition is done successfully !") ;
        return TOOLBOX_DFU_NO_ERROR ;
    }
    else
    {
        displayManager.print(MSG_ERROR, L"Write OTP partition is failed !") ;
        return TOOLBOX_DFU_ERROR_WRITE ;
    }
}

/**
 * @brief DFU::getDfuUtilProgramPath : Get the path of dfu-util program from the project directory.
 * @return The dfu-util executable path.
 */
std::string DFU::getDfuUtilProgramPath()
{
    std::string path = "" ;
#ifdef _WIN32
    path = this->toolboxFolder;
    path.append("\\Utilities\\Windows\\dfu-util.exe") ; //from the project tree
    path = "\"" + path + "\" " ;
    displayManager.print(MSG_NORMAL, L"dfu-util application path : %s", path.c_str()) ;
#else
    path = "dfu-util " ; //use system command
#endif

    return path;
}

/**
 * @brief DFU::getLsUsbProgramPath : Get the path of lsusb program from the project directory.
 * @return The fastboot executable path.
 */
std::string DFU::getLsUsbProgramPath()
{
    std::string path = "" ;
#ifdef _WIN32
    path = this->toolboxFolder;
    path.append("\\Utilities\\Windows\\lsusb.exe") ; //from the project tree
    path = "\"" + path + "\" " ;
    displayManager.print(MSG_NORMAL, L"lsusb path : %s", path.c_str()) ; // display this just once
#elif __APPLE__
    path = this->toolboxFolder;
    path = "/Utilities/MacOS/dfu-util/lsusb" ; //from the project tree
    path = "\"" + path + "\" " ;
    displayManager.print(MSG_NORMAL, L"lsusb application path : %s", path.c_str()) ; // display this just once
#elif __linux__
    path = "lsusb " ; // use Linux system command
#else
    path = "" ;
#endif

    return path;
}

/**
 * @brief DFU::isDfuUtilInstalled
 * @return True if dfu-util is already installed on the machine, otherwise, false.
 * @note PRG-TOOLBOX-DFU includes the dfu-util program within the project for Windows, while it relies on the pre-installed version for Linux and MacOS.
 */
bool DFU::isDfuUtilInstalled()
{
    std::string cmd =  getDfuUtilProgramPath().append("--version ") ;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe == nullptr)
    {
        displayManager.print(MSG_ERROR, L"Failed to open pipe") ;
        return false;
    }

    char buffer[4096];
    std::string result = "";

    while (!feof(pipe))
    {
        if (fgets(buffer, 4096, pipe) != nullptr)
        {
            result += buffer;
        }
    }
    pclose(pipe);

    std::string searchString = "not found";
    size_t pos = result.find(searchString);
    if ((pos != std::string::npos) || (result.empty()))
    {
        displayManager.print(MSG_ERROR, L"dfu-util is not installed or cannot be found. Please install it and try again.") ;
        displayManager.print(MSG_WARNING, L"refer to: https://dfu-util.sourceforge.net/") ;
        return false ;
    }
    else
    {
        return true ;
    }
}
