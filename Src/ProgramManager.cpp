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

#include "ProgramManager.h"
#include <thread>
#include <chrono>

using namespace std ;

ProgramManager::ProgramManager(const std::string toolboxFolder, const std::string dfuSerialNumber)
{
    dfuInterface = new DFU() ;
    dfuInterface->toolboxFolder = toolboxFolder ;
    dfuInterface->dfuSerialNumber = dfuSerialNumber ;
    parsedTsvFile = nullptr;
}

ProgramManager::~ProgramManager()
{
    delete dfuInterface ;
    delete parsedTsvFile ;
}

/**
 * @brief ProgramManager::startInstallService : Navigate through the partitions list and flash the appropriate boot firmwares.
 * @param inputTsvPath: The TSV file to deploy.
 * @param isStartFastboot: Ask to launch the fastboot mode or not.
 * @param isDfuFlashingCommand: Flag to handle the programming of the Flashlayout (Flash command versus Install command)
 * @return 0 if the operation is performed successfully, otherwise an error occurred.
 */
int ProgramManager::startInstallService(const std::string inputTsvPath, bool isStartFastboot, bool isDfuFlashingCommand)
{
    auto start = std::chrono::high_resolution_clock::now(); // get start time

    if(fileManager.openTsvFile(inputTsvPath, &parsedTsvFile, isStartFastboot) != 0)
    {
        displayManager.print(MSG_ERROR, L"Failed to download TSV partitions: %s", inputTsvPath.c_str());
        return TOOLBOX_DFU_ERROR_NO_FILE ;
    }

    if((parsedTsvFile->partitionsList.size() == 1) && (parsedTsvFile->partitionsList.at(0).partIp == "none")) /* support PRGFW-UTIL which contains only one boot partition used to manage OTP */
    {
        dfuInterface->isSTM32PRGFW_UTIL = true ;
    }

    displayManager.print(MSG_NORMAL, L"-----------------------------------------");
    displayManager.print(MSG_GREEN, L"TSV DFU installing...");
    displayManager.print(MSG_NORMAL, L"  TSV path           : %s", inputTsvPath.data() );
    displayManager.print(MSG_NORMAL, L"  Partitions number  : %d", parsedTsvFile->partitionsList.size() );
    displayManager.print(MSG_NORMAL, L"  U-Boot script size : %d Bytes", parsedTsvFile->scriptUbootTsvDataSize );
    displayManager.print(MSG_NORMAL, L"  Start Fastboot     : %s ",  (isStartFastboot == true) ? "Yes" : "No");
    displayManager.print(MSG_NORMAL, L"  Boot Application   : %s ",  (dfuInterface->isSTM32PRGFW_UTIL == false) ? "U-Boot" : "STM32PRGFW-UTIL");
    displayManager.print(MSG_NORMAL,L"-----------------------------------------\n" );

    if((isStartFastboot == true) && (dfuInterface->isSTM32PRGFW_UTIL == true))
    {
        displayManager.print(MSG_ERROR, L"STM32PRGFW-UTIL does not support Fastboot mode.", inputTsvPath.c_str());
        return TOOLBOX_DFU_ERROR_NOT_SUPPORTED;
    }

    if(fileManager.isValidTsvFile(parsedTsvFile, dfuInterface->isSTM32PRGFW_UTIL) == false)
        return TOOLBOX_DFU_ERROR_WRONG_PARAM ;

    int ret = -1 ;
    if(dfuInterface->isDfuUtilInstalled() == false)
        return TOOLBOX_DFU_ERROR_OTHER;

    displayManager.print(MSG_NORMAL, L"Checking if there is a Fastboot device is already running");
    bool isUbootFastbootRunning = dfuInterface->isUbootFastbootRunning() ;
    if(isStartFastboot == true)
    {
        if(isUbootFastbootRunning == true)
        {
            displayManager.print(MSG_NORMAL, L"No installing service will be performed !");
            return TOOLBOX_DFU_NO_ERROR ;
        }
    }
    else
    {
        if(isUbootFastbootRunning == true)
        {
            displayManager.print(MSG_ERROR, L"U-Boot in fastboot mode is already running, it is not possible to prepare and launch U-Boot in DFU mode.");
            displayManager.print(MSG_ERROR, L"Please reset your device and try again.");
            return TOOLBOX_DFU_ERROR_NOT_CONNECTED ;
        }
    }

    if(dfuInterface->isDfuDeviceExist() == false)
        return TOOLBOX_DFU_ERROR_CONNECTION;

    if(dfuInterface->getDeviceID() != 0)
        return TOOLBOX_DFU_ERROR_NO_DEVICE ;

    displayManager.print(MSG_NORMAL, L"Checking if there is a U-Boot in DFU mode is already running");
    if(dfuInterface->isUbootDfuRunning() == true)
        isDfuUbootRunning = true ;

    if((isStartFastboot == false) && (isDfuUbootRunning == true))
    {
        displayManager.print(MSG_NORMAL, L"No installing service will be performed !");
        return TOOLBOX_DFU_NO_ERROR ;
    }

    if(isDfuUbootRunning == false)
    {
        /* Program the boot partitions */
        /* https://wiki.st.com/stm32mpu/wiki/How_to_load_U-Boot_with_dfu-util */
        if(dfuInterface->deviceID == STM32MP15) /* STM32MP15 */
        {
            ret = dfuInterface->flashPartition(1, parsedTsvFile->partitionsList.at(0).binary) ;
            if(ret)
            {
                displayManager.print(MSG_ERROR, L"Failed to flash partition: %s",  parsedTsvFile->partitionsList.at(0).binary.c_str());
                return ret ;
            }

            if(dfuInterface->isSTM32PRGFW_UTIL == false)
            {
                ret = dfuInterface->flashPartition(3, parsedTsvFile->partitionsList.at(1).binary) ;
                if(ret)
                {
                    displayManager.print(MSG_ERROR, L"Failed to flash partition: %s",  parsedTsvFile->partitionsList.at(1).binary.c_str());
                    return ret ;
                }

                ret = dfuInterface->dfuDetach() ;
                if(ret)
                    return ret ;
            }
        }
        else if(dfuInterface->deviceID == STM32MP13)
        {
            ret = dfuInterface->flashPartition(0, parsedTsvFile->partitionsList.at(0).binary) ;
            if(ret)
            {
                displayManager.print(MSG_ERROR, L"Failed to flash partition: %s",  parsedTsvFile->partitionsList.at(0).binary.c_str());
                return ret ;
            }

            ret = dfuInterface->dfuDetach() ;
            if(ret)
                return ret ;

            if(dfuInterface->isSTM32PRGFW_UTIL == false)
            {
                if(dfuInterface->isDfuDeviceExist(3000) == false) // wait the device to reconnect after deatch
                    return TOOLBOX_DFU_ERROR_CONNECTION;

                ret = dfuInterface->flashPartition(0, parsedTsvFile->partitionsList.at(1).binary) ;
                if(ret)
                {
                    displayManager.print(MSG_ERROR, L"Failed to flash partition: %s",  parsedTsvFile->partitionsList.at(1).binary.c_str());
                    return ret ;
                }

                ret = dfuInterface->dfuDetach() ;
                if(ret)
                    return ret ;
            }
        }
        else if((dfuInterface->deviceID == STM32MP25) || (dfuInterface->deviceID == STM32MP21))
        {
            /* fsbl-boot */
            ret = dfuInterface->flashPartition(0, parsedTsvFile->partitionsList.at(0).binary) ;
            if(ret)
            {
                displayManager.print(MSG_ERROR, L"Failed to flash partition: %s",  parsedTsvFile->partitionsList.at(0).binary.c_str());
                return ret ;
            }

            ret = dfuInterface->dfuDetach() ;
            if(ret)
                return ret ;

            if(dfuInterface->isSTM32PRGFW_UTIL == false)
            {
                if(dfuInterface->isDfuDeviceExist(3000) == false) // wait the device to reconnect after deatch
                    return TOOLBOX_DFU_ERROR_CONNECTION;

                /* fip-ddr	FIP */
                ret = dfuInterface->flashPartition(0, parsedTsvFile->partitionsList.at(1).binary) ;
                if(ret)
                {
                    displayManager.print(MSG_ERROR, L"Failed to flash partition: %s",  parsedTsvFile->partitionsList.at(1).binary.c_str());
                    return ret ;
                }

                ret = dfuInterface->dfuDetach() ;
                if(ret)
                    return ret ;

                if(dfuInterface->isDfuDeviceExist(3000) == false) // wait the device to reconnect after deatch
                    return TOOLBOX_DFU_ERROR_CONNECTION;

                /* fip-boot */
                ret = dfuInterface->flashPartition(1, parsedTsvFile->partitionsList.at(2).binary) ;
                if(ret)
                {
                    displayManager.print(MSG_ERROR, L"Failed to flash partition: %s",  parsedTsvFile->partitionsList.at(2).binary.c_str());
                    return ret ;
                }

                ret = dfuInterface->dfuDetach() ;
                if(ret)
                    return ret ;
            }
        }
        else
        {
            displayManager.print(MSG_ERROR, L"Unsupported device !");
            return TOOLBOX_DFU_ERROR_NOT_SUPPORTED ;
        }
    }

    /* Flash the flash memory layout in partition 0 and start fastboot/DFU mode */
    if((isDfuFlashingCommand == true) || (isStartFastboot == true))
    {
        std::string tempFile ;
        ret = fileManager.saveTemproryScriptFile(*parsedTsvFile, tempFile) ;
        if(ret != 0)
        {
            displayManager.print(MSG_ERROR, L"Failed to prepare script flashlayout !");
            return ret ;
        }

        if(dfuInterface->isUbootDfuRunning(30000) == false) //waiting the device to detach
        {
            return TOOLBOX_DFU_ERROR_CONNECTION ;
        }

        ret = dfuInterface->flashPartition(0, tempFile) ;
        if(ret != 0)
        {
            displayManager.print(MSG_ERROR, L"Failed to program flashlayout at partition 0 !");
            if(fileManager.removeTemproryFile(tempFile.c_str()) != TOOLBOX_DFU_NO_ERROR)
            {
                displayManager.print(MSG_ERROR, L"Failed to remove the temprory file !");
                return TOOLBOX_DFU_ERROR_NO_MEM ;
            }
            return ret ;
        }

        if(fileManager.removeTemproryFile(tempFile.c_str()) != TOOLBOX_DFU_NO_ERROR)
        {
            displayManager.print(MSG_ERROR, L"Failed to remove the temprory file !");
            return TOOLBOX_DFU_ERROR_NO_MEM ;
        }

        ret = dfuInterface->dfuDetach() ;
        if(ret != 0)
            return ret ;
    }

    if(isStartFastboot == true)
    {
        if(dfuInterface->isUbootFastbootRunning(30000) == true)
        {
            auto end = std::chrono::high_resolution_clock::now(); // get end time
            auto duration = std::chrono::duration_cast< std::chrono::milliseconds>(end - start);
            displayManager.print(MSG_NORMAL, L"Time elapsed to start fastboot: %02d:%02d:%03d", (duration.count() / (1000 * 60)), ((duration.count() / 1000) % 60), (duration.count() % 1000));

            ret = TOOLBOX_DFU_NO_ERROR ;
        }
        else
        {
            displayManager.print(MSG_ERROR, L"Failed to start Fastboot !");
            ret = TOOLBOX_DFU_ERROR_CONNECTION ;
        }
    }
    else
    {
        if(dfuInterface->isUbootDfuRunning(30000) == true)
        {
            auto end = std::chrono::high_resolution_clock::now(); // get end time
            auto duration = std::chrono::duration_cast< std::chrono::milliseconds>(end - start);
            displayManager.print(MSG_NORMAL, L"Time elapsed to launch U-Boot in DFU mode: %02d:%02d:%03d", (duration.count() / (1000 * 60)), ((duration.count() / 1000) % 60), (duration.count() % 1000));
            ret = TOOLBOX_DFU_NO_ERROR ;
        }
        else
        {
            displayManager.print(MSG_ERROR, L"Failed to start U-Boot in DFU mode !");
            ret = TOOLBOX_DFU_ERROR_CONNECTION ;
        }
    }

    return ret ;
}

/**
 * @brief ProgramManager::sleep : Wait for a couple of time in millisecondes.
 * @param ms: Number of millisecondes.
 */
void ProgramManager::sleep(uint32_t ms)
{
    displayManager.print(MSG_NORMAL, L"Sleeping : %d ms", ms);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

/**
 * @brief ProgramManager::readOtpPartition : Read the OTP partition and request to save data in file.
 * @param filePath: The output binary file to store OTP data.
 * @return 0 if the operation is performed successfully, otherwise an error occurred.
 */
int ProgramManager::readOtpPartition(const std::string filePath)
{
    displayManager.print(MSG_NORMAL, L"-----------------------------------------");
    displayManager.print(MSG_GREEN, L"DFU reading...");
    displayManager.print(MSG_NORMAL, L"  OTP partition     : 0xF2");
    displayManager.print(MSG_NORMAL, L"  Output file path  : %s", filePath.data() );
    displayManager.print(MSG_NORMAL,L"-----------------------------------------\n" );

    if(dfuInterface->isDfuDeviceExist() == false)
        return TOOLBOX_DFU_ERROR_CONNECTION;

    if(dfuInterface->getDeviceID() != 0)
        return TOOLBOX_DFU_ERROR_NO_DEVICE ;

    if(dfuInterface->isUbootDfuRunning() == false)
        return TOOLBOX_DFU_ERROR_CONNECTION;

    std::string outFilePath = filePath ;
    outFilePath.erase(std::remove(outFilePath.begin(), outFilePath.end(), '\"'), outFilePath.end()) ; //remove the double quotes from the file path
    std::ifstream file(outFilePath);
    if (file.good())
    {
        file.close();
        displayManager.print(MSG_WARNING, L"The file %s already exists, it will be overwritten !", filePath.c_str());
        if (std::remove(outFilePath.c_str()) != 0)
        {
            displayManager.print(MSG_ERROR,L"Error deleting file: %s", filePath.c_str());
            return TOOLBOX_DFU_ERROR_NO_FILE;
        }
    }

    return  this->dfuInterface->readOtpPartition(filePath) ;
}

/**
 * @brief ProgramManager::writeOtpPartition : Write a binary file into the OTP partition.
 * @param filePath: The input binary file to program.
 * @return 0 if the operation is performed successfully, otherwise an error occurred.
 * @note Be careful, using a bad binary file to fuse OTP can damage your device.
 */
int ProgramManager::writeOtpPartition(const std::string filePath)
{
    displayManager.print(MSG_NORMAL, L"-----------------------------------------");
    displayManager.print(MSG_GREEN, L"DFU downloading...");
    displayManager.print(MSG_NORMAL, L"  OTP partition    : 0xF2");
    displayManager.print(MSG_NORMAL, L"  Input file path  : %s", filePath.data() );
    displayManager.print(MSG_NORMAL,L"-----------------------------------------\n" );

    if(dfuInterface->isDfuDeviceExist() == false)
        return TOOLBOX_DFU_ERROR_CONNECTION;

    if(dfuInterface->getDeviceID() != 0)
        return TOOLBOX_DFU_ERROR_NO_DEVICE ;

    if(dfuInterface->isUbootDfuRunning() == false)
        return TOOLBOX_DFU_ERROR_CONNECTION;

    return  this->dfuInterface->writeOtpPartition(filePath) ;
}

/**
 * @brief ProgramManager::startFlashingService : Navigate through the partitions list and flash all firmwares except boot partitions throught DFU interface.
 * @param inputTsvPath: The TSV file to deploy.
 * @return 0 if the operation is performed successfully, otherwise an error occurred.
 */
int ProgramManager::startFlashingService(const std::string inputTsvPath)
{
    displayManager.print(MSG_NORMAL, L"\nStart DFU flashing service...\n\n");

    auto start = std::chrono::high_resolution_clock::now(); // get start time
    if(fileManager.openTsvFile(inputTsvPath, &parsedTsvFile, false) != 0)
    {
        displayManager.print(MSG_ERROR, L"Failed to download TSV partitions: %s", inputTsvPath.c_str());
        return TOOLBOX_DFU_ERROR_NO_FILE ;
    }


    displayManager.print(MSG_NORMAL, L"-----------------------------------------");
    displayManager.print(MSG_GREEN, L"TSV DFU flashing...");
    displayManager.print(MSG_NORMAL, L"  TSV path           : %s", inputTsvPath.data() );
    displayManager.print(MSG_NORMAL, L"  Partitions number  : %d", parsedTsvFile->partitionsList.size() );
    displayManager.print(MSG_NORMAL,L"-----------------------------------------\n" );

    if(fileManager.isValidTsvFile(parsedTsvFile, dfuInterface->isSTM32PRGFW_UTIL) == false)
        return TOOLBOX_DFU_ERROR_WRONG_PARAM ;

    int ret = -1 ;
    if(dfuInterface->isDfuUtilInstalled() == false)
        return TOOLBOX_DFU_ERROR_OTHER;

    if(dfuInterface->isDfuDeviceExist() == false)
        return TOOLBOX_DFU_ERROR_CONNECTION;

    if(dfuInterface->getDeviceID() != 0)
        return TOOLBOX_DFU_ERROR_NO_DEVICE ;

    uint8_t phaseID = 0xFF ;
    bool isNeedDetach = false ;
    bool isFlashlayoutSent = false;

    while(1)
    {
        ret = getPhase(&phaseID, &isNeedDetach) ;
        if(ret != TOOLBOX_DFU_NO_ERROR)
            break ;

        if(phaseID == 0)
        {
            if(isFlashlayoutSent == true) //To fix BareMetal Flashing with STM32PRGFW-UTIL for External memory
                continue;

            displayManager.print(MSG_NORMAL, L"\nFlashlayout Programming ...");
            std::string tempFile ;
            ret = fileManager.saveTemproryScriptFile(*parsedTsvFile, tempFile) ;
            if(ret != 0)
            {
                displayManager.print(MSG_ERROR, L"Failed to prepare flashlayout !");
                break ;
            }

            ret = dfuInterface->flashPartition(0, tempFile) ;
            if(ret != 0)
            {
                displayManager.print(MSG_ERROR, L"Failed to program flashlayout at partition 0 !");
            }

            if(fileManager.removeTemproryFile(tempFile.c_str()) != TOOLBOX_DFU_NO_ERROR)
            {
                displayManager.print(MSG_ERROR, L"Failed to remove the temprory flashlayout file !");
                break;
            }

            if(ret != TOOLBOX_DFU_NO_ERROR)
                break ;

            ret = dfuInterface->dfuDetach() ;
            if(ret != 0)
                break ;

            if(dfuInterface->isDfuDeviceExist(30000) == false)
            {
                displayManager.print(MSG_ERROR, L"Failed to reconnect the device !");
                ret = TOOLBOX_DFU_ERROR_CONNECTION ;
                break ;
            }
            isFlashlayoutSent = true;
        }
        else if(phaseID == 0xFE)
        {
            displayManager.print(MSG_NORMAL, L"Flashing service completed successfully");
            break;
        }
        else if(phaseID == 0xFF)
        {
            displayManager.print(MSG_WARNING, L"Received PhaseID is 0xFF, system is going to reboot");
            break;
        }
        else
        {
            for(auto &part: parsedTsvFile->partitionsList)
            {
                std::string patternNone = "none\"";
                if((part.binary == "none") || (part.binary.size() >= patternNone.size() && part.binary.substr(part.binary.size() - patternNone.size()) == patternNone)) //ignore the field containing none keyword
                    continue ;

                if(part.phaseID == phaseID)
                {
                    uint8_t alternateIndex = 0xFF;
                    ret = this->dfuInterface->getAlternateSettingIndex(phaseID, &alternateIndex);
                    if(ret != TOOLBOX_DFU_NO_ERROR)
                        break;

                    ret = dfuInterface->flashPartition(alternateIndex, part.binary) ;
                    if(ret != 0)
                        break;

                    sleep(5);
                    if(phaseID <= 5) // To check FSBL USB enumeration for boot partitions.
                    {
                        ret = getPhase(&phaseID, &isNeedDetach) ;
                        if(ret != TOOLBOX_DFU_NO_ERROR)
                            break ;

                        if(isNeedDetach == true)
                        {
                            ret = dfuInterface->dfuDetach() ;
                            if(ret != 0)
                                break;

                            if(dfuInterface->isDfuDeviceExist(30000) == false)
                            {
                                displayManager.print(MSG_ERROR, L"Failed to reconnect the device !");
                                ret = TOOLBOX_DFU_ERROR_CONNECTION ;
                                break ;
                            }
                        }
                    }
                    break ;
                }
            }

            if(ret != TOOLBOX_DFU_NO_ERROR)
                break;
        }
    }

    if(ret == TOOLBOX_DFU_NO_ERROR)
    {
        auto end = std::chrono::high_resolution_clock::now(); // get end time
        auto duration = std::chrono::duration_cast< std::chrono::milliseconds>(end - start);
        displayManager.print(MSG_NORMAL, L"DFU Flashing service finished."),
        displayManager.print(MSG_GREEN, L"Time elapsed to flash all partitions: %ld min, %02ld s, %03ld ms", (duration.count() / (1000 * 60)), ((duration.count() / 1000) % 60), (duration.count() % 1000));
    }
    else
    {
        displayManager.print(MSG_ERROR, L"Failed to flash partitions !");
    }

    return ret ;

}

/**
 * @brief ProgramManager::getPhase : Get the acutal running phase.
 * @param phase: Output parameter indicating the phase value.
 * @param isNeedDetach: Output parameter to check if the device requesting a detach.
 * @return  0 if the operation is performed successfully, otherwise an error occurred.
 */
int ProgramManager::getPhase(uint8_t* phase, bool* isNeedDetach)
{
    /* https://wiki.st.com/stm32mp25-beta-v5/wiki/How_to_load_U-Boot_with_dfu-util#GetPhase_support_with_dfu-util */

    displayManager.print(MSG_NORMAL, L"DFU Getting Phase ID...\n");
    int status = TOOLBOX_DFU_NO_ERROR;
    if(dfuInterface->isDfuDeviceExist() == false)
        status = TOOLBOX_DFU_ERROR_CONNECTION;

    if(status != TOOLBOX_DFU_NO_ERROR)
        return status ;

    if(dfuInterface->getDeviceID() != 0)
        status =  TOOLBOX_DFU_ERROR_NO_DEVICE ;

    if(status != TOOLBOX_DFU_NO_ERROR)
        return status ;

    std::string tmpPhaseFile = "" ;
    status = fileManager.getTemproryFile(tmpPhaseFile) ;
    if(status != TOOLBOX_DFU_NO_ERROR)
        return status ;

    status = fileManager.removeTemproryFile(tmpPhaseFile) ; // Remove the temporary file to allow dfu-util to create it again
    if(status != TOOLBOX_DFU_NO_ERROR)
        return status ;

    uint8_t alternateIndexVirtual = 0xFF;
    status = this->dfuInterface->getAlternateSettingIndex("virtual", &alternateIndexVirtual);
    if(status != TOOLBOX_DFU_NO_ERROR)
        return status ;

    status = dfuInterface->readPartition(tmpPhaseFile, alternateIndexVirtual) ;
    if(status != TOOLBOX_DFU_NO_ERROR)
        return status ;

    /* The GetPhase information is now avialable inside the tmpPhaseFile */
    std::ifstream file(tmpPhaseFile, std::ios::binary);
    if (!file)
    {
        displayManager.print(MSG_ERROR, L"The file does not exist :  %s",  tmpPhaseFile.data());
        status = TOOLBOX_DFU_ERROR_NO_FILE;
    }

    if(status != TOOLBOX_DFU_NO_ERROR)
        return status ;

    GetPhaseStruct data;
    file.read(reinterpret_cast<char*>(&data), sizeof(data) - 1);

    /* Check if Phase is 0 to read the NeedDFUDetach byte */
    if (data.Phase == 0)
        file.read(reinterpret_cast<char*>(&data.NeedDFUDetach), sizeof(data.NeedDFUDetach));
    else
        data.NeedDFUDetach = 0;

    file.close();

    status = fileManager.removeTemproryFile(std::move(tmpPhaseFile) ) ; // Remove the temprory file created by dfu-util.
    if(status != TOOLBOX_DFU_NO_ERROR)
        return status ;

    *phase =  data.Phase ;
    *isNeedDetach = (data.NeedDFUDetach != 0) ? true : false;

    displayManager.print(MSG_NORMAL, L"\n + Phase ID       : 0x%02X", *phase);
    displayManager.print(MSG_NORMAL, L" + Load address   : 0x%08X", data.Address);
    displayManager.print(MSG_NORMAL, L" + Request detach : %s\n", *isNeedDetach ? "Yes" : "No");

    return status ;
}
