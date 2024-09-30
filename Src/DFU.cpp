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

#include "DFU.h"
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
    if(this->dfuSerialNumber != "")
        utilCmd.append(" --serial ").append(this->dfuSerialNumber);

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
    std::string  utilCmd =  getDfuUtilProgramPath().append("-d 483:df11 -a 0 -e") ;
    if(this->dfuSerialNumber != "")
        utilCmd.append(" --serial ").append(this->dfuSerialNumber);

    displayManager.print(MSG_NORMAL, L"DFU-UTIL command: %s", utilCmd.data()) ;
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
 * @brief DFU::isUbootDfuRunning : Verify if there is a U-Boot device runs in DFU mode with timeout checks.
 * @param msTimeout: The timeout duration in milliseconds to discover and search for the DFU device.
 * @return True if an U-Boot in DFU mode is started, otherwise, false.
 */
bool DFU::isUbootDfuRunning(uint32_t msTimeout)
{
    std::string  utilCmd =  getDfuUtilProgramPath().append("-d 483:df11 -l") ; /* ST DFU PID:0483 VID:DF11 */
    if(this->dfuSerialNumber != "")
        utilCmd.append(" --serial ").append(this->dfuSerialNumber);

    displayManager.print(MSG_NORMAL, L"DFU-UTIL command: %s", utilCmd.data()) ;

    bool isDfuRunning = false ;
    const std::chrono::milliseconds timeout_duration(msTimeout);
    const auto start_time = std::chrono::steady_clock::now();

    while (true)
    {
        // Check if the timeout has been reached
        const auto elapsed_time = std::chrono::steady_clock::now() - start_time;
        if (elapsed_time >= timeout_duration)
        {
            displayManager.print(MSG_WARNING, L"Timeout [%d ms] is reached to discover U-Boot DFU device!", msTimeout) ;
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
                isDfuRunning = true ;
                otpPartitionName = match[1].str();
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

    if (isDfuRunning)
        displayManager.print(MSG_GREEN, L"U-Boot in DFU mode is running !") ;
    else
        displayManager.print(MSG_WARNING, L"U-Boot in DFU mode is not running !") ;

    return isDfuRunning;
}

/**
 * @brief DFU::isDfuDeviceExist: Verify if there is a plugged-in STM32 DFU device with timeout checks.
 * @param msTimeout: The timeout duration in milliseconds to discover and search for the DFU device.
 * @return True if a device is present, otherwise, false.
 */
bool DFU::isDfuDeviceExist(uint32_t msTimeout)
{
    std::string  utilCmd =  getDfuUtilProgramPath().append("-d 483:df11 -l") ; /* ST DFU PID:0483 VID:0AFB */
    if(this->dfuSerialNumber != "")
        utilCmd.append(" --serial ").append(this->dfuSerialNumber);

    bool isExist = false ;
    const std::chrono::milliseconds timeout_duration(msTimeout);
    const auto start_time = std::chrono::steady_clock::now();

    while (true)
    {
        // Check if the timeout has been reached
        const auto elapsed_time = std::chrono::steady_clock::now() - start_time;
        if (elapsed_time >= timeout_duration)
        {
            displayManager.print(MSG_WARNING, L"Timeout [%d ms] is reached to found the STM32 DFU device!", msTimeout) ;
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

        std::string searchString = "Found DFU: [0483:df11]";
        size_t pos = result.find(searchString);
        if (pos != std::string::npos)
        {
            isExist = true ;
            break ;
        }

        // Sleep for a short time to simulate work being done
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    }

    if (isExist == false)
    {
        if(this->dfuSerialNumber != "")
            displayManager.print(MSG_ERROR, L"No STM32 DFU device [%s] is detected !", this->dfuSerialNumber.data()) ;
        else
            displayManager.print(MSG_ERROR, L"No STM32 DFU device is detected !") ;
    }

    return isExist;
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
 * @brief DFU::isUbootFastbootRunning: Verify if there is a U-Boot device runs in Fastboot mode with timeout checks.
 * @param msTimeout: The timeout duration in milliseconds to discover and search for the fastboot device.
 * @return True if an U-Boot in Fastboot mode is detected, otherwise, false.
 */
bool DFU::isUbootFastbootRunning(uint32_t msTimeout)
{
    std::string  utilCmd =  getLsUsbProgramPath().append("-d 0483:0afb") ; /* ST Fastboot PID:0483 VID:0AFB */

    bool isRunning = false ;
    const std::chrono::milliseconds timeout_duration(msTimeout);
    const auto start_time = std::chrono::steady_clock::now();

    while (true)
    {
        // Check if the timeout has been reached
        const auto elapsed_time = std::chrono::steady_clock::now() - start_time;
        if (elapsed_time >= timeout_duration)
        {
            displayManager.print(MSG_WARNING, L"Timeout [%d ms] is reached to discover Fastboot device!", msTimeout) ;
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

    if (isRunning)
        displayManager.print(MSG_GREEN, L"U-Boot in Fastboot mode is running !") ;
    else
        displayManager.print(MSG_WARNING, L"No U-Boot in Fastboot mode is running !") ;

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

    displayManager.print(MSG_NORMAL, L"OTP partition name = %s", otpPartitionName.c_str()) ;

    utilCmd.append(otpPartitionName) ;
    utilCmd.append(" -U ").append(filePath) ;
    if(this->dfuSerialNumber != "")
        utilCmd.append(" --serial ").append(this->dfuSerialNumber);

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

    displayManager.print(MSG_NORMAL, L"OTP partition name = %s", otpPartitionName.c_str()) ;

    utilCmd.append(otpPartitionName) ;
    utilCmd.append(" -D ").append(filePath) ;
    if(this->dfuSerialNumber != "")
        utilCmd.append(" --serial ").append(this->dfuSerialNumber);

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

/**
 * @brief DFU::getAlternateSettingList : Get the list of the alternate settings for the current DFU device.
 * @return 0 if the operation is performed successfully, otherwise an error occurred.
 */
int DFU::getAlternateSettingList()
{
    std::string  utilCmd =  getDfuUtilProgramPath().append("-d 483:df11 -l") ; /* ST DFU PID:0483 VID:DF11 */
    if(this->dfuSerialNumber != "")
        utilCmd.append(" --serial ").append(this->dfuSerialNumber);
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

    try
    {
        std::regex altNameRegex("alt=([0-9]+).*?name=\"@([^/]+)");
        auto begin = std::sregex_iterator(result.begin(), result.end(), altNameRegex);
        auto end = std::sregex_iterator();

        for (std::sregex_iterator i = begin; i != end; ++i) {
            std::smatch match = *i;
            int alt = std::stoi(match[1].str());
            std::string name = match[2].str();
            this->altSettingList.emplace_back(alt, name);
        }
    }
    catch (const std::regex_error& e)
    {
        displayManager.print(MSG_ERROR, L"Regex error: %s", e.what());
        return TOOLBOX_DFU_ERROR_OTHER ;
    }

    return TOOLBOX_DFU_NO_ERROR;
}

/**
 * @brief DFU::getAlternateSettingIndex: Get the alternate setting index of a specific alternate name.
 * @param altName: Input string corresponding to the alternate name to be searched.
 * @param altIndex: The output Index value represneting the mentioned alternate name.
 * @return 0 if the operation is performed successfully, otherwise an error occurred.
 */
int DFU::getAlternateSettingIndex(const std::string altName, uint8_t *altIndex)
{
    int ret = TOOLBOX_DFU_NO_ERROR;
    bool isIndexFound = false ;

    if(this->altSettingList.empty())
    {
        /* Read the DFU device and get the list of the avaialble alternate setting */
        ret = getAlternateSettingList() ;
        if(ret != TOOLBOX_DFU_NO_ERROR)
            return ret ;
    }

    /* The List of alternate setting is already populated */
    for(uint8_t idx = 0 ; idx < this->altSettingList.size(); idx ++)
    {
        if(this->altSettingList.at(idx).second == altName)
        {
            *altIndex = this->altSettingList.at(idx).first ;
            isIndexFound = true ;
            displayManager.print(MSG_NORMAL, L"DFU device : Alternate name [%s] is found with alternate index [%d]", altName.c_str(), *altIndex);
            break ;
        }
    }

    if(isIndexFound == false)
    {
        displayManager.print(MSG_ERROR, L"DFU device : Alternate name [%s] does not exist !", altName.c_str());
        ret = TOOLBOX_DFU_ERROR_INTERFACE_NOT_SUPPORTED;
    }

    return ret;
}

/**
 * @brief DFU::displayDevicesList: Print the list of available STM32 DFU devices.
 * @return : 0 if the operation is performed successfully, otherwise an error occurred.
 */
int DFU::displayDevicesList()
{
    std::string  utilCmd =  getDfuUtilProgramPath().append("-d 483:df11 -l") ; /* ST DFU PID:0483 VID:DF11 */
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

    std::map<std::string, std::string> deviceMap;
    try
    {
        std::regex regex("devnum=(\\d+).*serial=\"([A-F0-9]+)\"");
        std::smatch match;
        std::string::const_iterator searchStart(result.cbegin());
        while (std::regex_search(searchStart, result.cend(), match, regex))
        {
            std::string devnum = match[1];
            std::string serial = match[2];
            deviceMap[serial] = std::move(devnum);
            searchStart = match.suffix().first;
        }
    }
    catch (const std::regex_error& e)
    {
        displayManager.print(MSG_ERROR, L"Regex error: %s", e.what());
        return false ;
    }


    // Check if any devices were found
    if (deviceMap.empty())
    {
        displayManager.print(MSG_NORMAL, L"") ;
        displayManager.print(MSG_WARNING, L"No STM32 DFU devices found.") ;
    }
    else
    {
        displayManager.print(MSG_GREEN, L"\nSTM32 DFU devices List") ;
        displayManager.print(MSG_NORMAL, L" Number of STM32 DFU devices: %d", deviceMap.size()) ;
        int deviceCount = 1;
        for (const auto& device : deviceMap)
        {
            displayManager.print(MSG_NORMAL, L" [Device %d] : ", deviceCount) ;
            displayManager.print(MSG_NORMAL, L"     Dev Num : %s", device.second.c_str()) ;
            displayManager.print(MSG_NORMAL, L"     Serial number : %s", device.first.c_str()) ;
            deviceCount++;
        }
    }

    return TOOLBOX_DFU_NO_ERROR ;
}
