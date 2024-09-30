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


#include "FileManager.h"
#include <iomanip>
#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#endif

#include <experimental/filesystem>

FileManager::FileManager()
{

}

FileManager & FileManager::getInstance()
{
    static FileManager instance;
    return instance;
}

/**
 * @brief FileManager::openTsvFile : Open and parse a TSV file containing the list of memory partitions.
 * @param fileName: The TSV file path.
 * @param parsedFile: Output variable to store the parsed file information.
 * @param isStartFastboot: flag to select the mode to apply.
 * @return 0 if the operation is performed successfully, otherwise an error occurred.
 */
int FileManager::openTsvFile(const std::string &fileName, fileTSV **parsedFile, bool isStartFastboot)
{
    fileTSV* parsedTSV = nullptr;
    std::ifstream inFile(fileName);

    if(inFile.is_open() == false)
    {
        displayManager.print(MSG_ERROR, L"The file does not exist :  %s",  fileName.data());
        return TOOLBOX_DFU_ERROR_NO_FILE;
    }

    try
    {
        parsedTSV = new fileTSV;
    }
    catch(const std::bad_alloc&)
    {
        displayManager.print(MSG_ERROR, L"Cannot allocate memory to read file : %s",  fileName.data());
        inFile.close();
        return TOOLBOX_DFU_ERROR_NO_MEM;
    }

    std::string tsvFolderPath = "" ;
    try
    {
        tsvFolderPath = std::experimental::filesystem::path(fileName).parent_path().string() ;
    }
    catch(...)
    {
        delete parsedTSV;
        inFile.close();
        return TOOLBOX_DFU_ERROR_OTHER;
    }

    if(parseTsvFile(std::move(tsvFolderPath), &inFile, parsedTSV, isStartFastboot) == 0)
    {
        *parsedFile = parsedTSV;
        inFile.close();
    }
    else
    {
        delete parsedTSV;
        inFile.close();
        return TOOLBOX_DFU_ERROR_OTHER;
    }

    return 0;
}


/**
 * @brief FileManager::parseTsvFile : The engine part of the methode "openTsvFile"
 * @param tsvFolderPath: The folder that contains the TSV file.
 * @param inFile: The TSV file path.
 * @param parsedTSV: Output variable to store the parsed file information.
 * @param isStartFastboot: flag to select the mode to apply.
 * @return 0 if the operation is performed successfully, otherwise an error occurred.
 */
int FileManager::parseTsvFile(const std::string tsvFolderPath, std::ifstream *inFile, fileTSV* parsedTSV, bool isStartFastboot)
{   
    inFile->seekg(0, std::ios::end) ;
    int fSize = inFile->tellg() ;
    if(fSize == 0)
    {
        displayManager.print(MSG_ERROR, L"TSV file is empty !") ;
        return TOOLBOX_DFU_ERROR_NO_FILE ;
    }
    else
    {
        inFile->seekg(0, std::ios::beg) ;
    }

    while (inFile->eof() == false)
    {
        partitionInfo tempPartition;
        std::string line ;
        std::getline(*inFile, line) ;

        if(line.empty() == true)
            continue;

        if(line.at(0) == '#') /* filter the header which starts with "#" */
            continue;

        std::vector<std::string> infomartionList ;
        try
        {
            std::regex delimiter("\\t+");
            if(splitStdString(std::move(line), std::move(delimiter), infomartionList))
                return TOOLBOX_DFU_ERROR_OTHER ;
        }
        catch (const std::regex_error& e)
        {
            displayManager.print(MSG_ERROR, L"Regex error: %s", e.what());
            return TOOLBOX_DFU_ERROR_NO_MEM;
        }

        if(infomartionList.size() != TSV_NB_COLUMNS )
        {
            displayManager.print(MSG_ERROR, L"TSV file is not conform, it may miss some columns or fields");
            return TOOLBOX_DFU_ERROR_WRONG_PARAM;
        }

        tempPartition.opt   = infomartionList[0];
        tempPartition.phaseID = strtoul(infomartionList[1].c_str(), 0, 16);
        tempPartition.partName = infomartionList[2];
        tempPartition.partType = infomartionList[3];
        tempPartition.partIp = infomartionList[4];
        tempPartition.offset  = infomartionList[5];
        tempPartition.binary = infomartionList[6];

        if(tempPartition.binary != "none")
        {
            std::ifstream binaryFile(tempPartition.binary);
            if(binaryFile.is_open() == false) // file does not exist
            {
                /* Try to search from the folder that contains the TSV file */
                std::string tmpPath = "" ;
                tmpPath.append(tsvFolderPath).append("/").append(tempPartition.binary) ;
                tempPartition.binary = std::move(tmpPath)  ;

                binaryFile.close();
                binaryFile.open(tempPartition.binary);
                if(binaryFile.is_open() == false)
                {
                    displayManager.print(MSG_ERROR, L"File %s does not exist !", tempPartition.binary.c_str());
                    return TOOLBOX_DFU_ERROR_WRONG_PARAM;
                }
            }
            tempPartition.binary = "\"" + tempPartition.binary + "\""; //To take into account the paths with white spaces;
        }

        parsedTSV->partitionsList.push_back(tempPartition);
    }

    int ret = 0;
    if(isStartFastboot)
        prepareUbootScriptFile(*parsedTSV) ; // for Fastboot context
    else
        prepareUbootFlashlayoutFile(*parsedTSV) ; // for DFU context

    return ret ;
}

/**
 * @brief FileManager::splitStdString : Split an input string basiong on a specifc format and delimiter.
 * @param str: The input string.
 * @param delimiter: the separator to apply like "\\t+"...
 * @param stringsList: The output variable to store the splited strings.
 * @return 0 if the operation is performed successfully, otherwise an error occurred.
 */
int FileManager::splitStdString(std::string str, std::regex delimiter, std::vector<std::string>& stringsList)
{
    if(str.empty())
    {
        displayManager.print(MSG_ERROR, L"Cannot split string, input is empty") ;
        return TOOLBOX_DFU_ERROR_WRONG_PARAM;
    }

    std::vector<std::string> substrings(
                std::sregex_token_iterator(str.begin(), str.end(), delimiter, -1),
                std::sregex_token_iterator()
                );

    stringsList = std::move(substrings);

    return TOOLBOX_DFU_NO_ERROR ;
}

/**
 * @brief FileManager::prepareUbootScriptHeader : Prepare the header of U-Boot script.
 * @param parsedTsvFile: the input/output variable to manage the TSV parsed data.
 * @return 0 if the operation is performed successfully, otherwise an error occurred.
 */
int FileManager::prepareUbootScriptHeader(fileTSV &parsedTsvFile)
{
    if(parsedTsvFile.scriptUbootTsvDataSize == 0)
        return TOOLBOX_DFU_ERROR_NO_FILE ;

    scriptLayoutHeader header ;
    uint32_t auxValue;

    header.sMagic = ((IH_MAGIC >> 24) & 0xff) | ((IH_MAGIC << 8) & 0xff0000) | ((IH_MAGIC >> 8) & 0xff00) | ((IH_MAGIC << 24) & 0xff000000);
    header.sHcrc = 0 ;
    header.sTime = 0 ;

    auxValue = (parsedTsvFile.scriptUbootTsvDataSize - SCRIPT_LAYOUT_HEADER_SIZE) ;
    header.sSize = ((auxValue >> 24) & 0xff) | ((auxValue << 8) & 0xff0000) | ((auxValue >> 8) & 0xff00) | ((auxValue << 24) & 0xff000000) ;

    header.sLoad = 0 ;
    header.sEp = 0 ;

    /* Script Data CRC Checksum */
    auxValue = getChecksumCrc32(parsedTsvFile.scriptUbootTsvData + SCRIPT_LAYOUT_HEADER_SIZE, parsedTsvFile.scriptUbootTsvDataSize - SCRIPT_LAYOUT_HEADER_SIZE) ;
    header.sDcrc = ((auxValue >> 24) & 0xff) | ((auxValue << 8) & 0xff0000) | ((auxValue >> 8) & 0xff00) | ((auxValue << 24) & 0xff000000) ;

    header.sOs = 0 ;
    header.sArch = 0 ;
    header.sType = IH_TYPE_SCRIPT ;
    header.sComp = 0 ;

    for(uint8_t id=0; id < sizeof (header.sName) ; id++)
        header.sName[id] = 0;

    unsigned char* ptr  ;
    ptr = (unsigned char *)&header ;

    /* Script Header CRC Checksum */
    auxValue = getChecksumCrc32(ptr, SCRIPT_LAYOUT_HEADER_SIZE) ;
    header.sHcrc = ((auxValue >> 24) & 0xff) | ((auxValue << 8) & 0xff0000) | ((auxValue >> 8) & 0xff00) | ((auxValue << 24) & 0xff000000);

    memcpy(parsedTsvFile.scriptUbootTsvData, ptr, SCRIPT_LAYOUT_HEADER_SIZE) ;

    return TOOLBOX_DFU_NO_ERROR;
}

/**
 * @brief FileManager::prepareUbootScriptFile : Prepare the U-Boot script needed to start the fastboot mode automatically.
 * @param parsedTsvFile: the input/output variable to manage the TSV parsed data.
 * @return 0 if the operation is performed successfully, otherwise an error occurred.
 */
int FileManager::prepareUbootScriptFile(fileTSV &parsedTsvFile)
{
    if(parsedTsvFile.partitionsList.empty())
        return TOOLBOX_DFU_ERROR_NO_FILE ;

    /* https://wiki.st.com/stm32mpu/wiki/STM32CubeProgrammer_flashlayout#Block_device_GPT_partition-_SD_card_-2F_e-E2-80-A2MMC */
    std::map<std::string, std::string> guid ;
    guid.insert(std::make_pair("Binary", "8DA63339-0007-60C0-C436-083AC8230908")) ;
    guid.insert(std::make_pair("ENV", "3DE21764-95DB-54BD-A5C3-4ABE786F38A8")) ;
    guid.insert(std::make_pair("FWU_MDATA", "8A7A84A0-8387-40F6-AB41-A8B9A5A60D23")) ;
    guid.insert(std::make_pair("FIP", "19D5DF83-11B0-457b-BE2C-7559C13142A5")) ;
    guid.insert(std::make_pair("FileSystem", "0FC63DAF-8483-4772-8E79-3D69D8477DE4")) ;
    guid.insert(std::make_pair("ESP", "C12A7328-F81F-11D2-BA4B-00A0C93EC93B")) ;

    /* https://wiki.st.com/stm32mpu/wiki/STM32CubeProgrammer_flashlayout#GPT_partuuid */
    std::map<std::string, std::string> uuid ;
    uuid.insert(std::make_pair("fip-a", "4FD84C93-54EF-463F-A7EF-AE25FF887087")) ;
    uuid.insert(std::make_pair("fip-b", "09C54952-D5BF-45AF-ACEE-335303766FB3")) ;
    uuid.insert(std::make_pair("mmc0", "e91c4e10-16e6-4c0e-bd0e-77becf4a3582")) ;
    uuid.insert(std::make_pair("mmc1", "491f6117-415d-4f53-88c9-6e0de54deac6")) ;
    uuid.insert(std::make_pair("mmc2", "fd58f1c7-be0d-4338-8ee9-ad8f050aeb18")) ;

    /* Start preparing ... */
    std::string script = "env set partitions " ;
    for(uint8_t i=0;  i < parsedTsvFile.partitionsList.size() ; i++)
    {
        if((parsedTsvFile.partitionsList.at(i).partIp.compare("none") == 0) || (parsedTsvFile.partitionsList.at(i).offset.substr(0, 4)  == "boot"))
            continue;

        std::string line = "" ;
        line.append("name=").append(parsedTsvFile.partitionsList.at(i).partName);

        if((i+1) >= (uint8_t) parsedTsvFile.partitionsList.size())
        {
            /* Decode type field */
            if(parsedTsvFile.partitionsList.at(i).partType.compare("FileSystem") == 0)
                line.append(",type=").append(guid.at("FileSystem")) ;

            /* All remaining memory space */
            line.append(",size=-") ;
        }
        else
        {
            /* Calculate the partition size */
            uint64_t partSize = std::stoull(parsedTsvFile.partitionsList.at(i+1).offset, nullptr, 16)  - std::stoull(parsedTsvFile.partitionsList.at(i).offset, nullptr, 16)  ;
            std::stringstream sstream;
            sstream << std::hex << partSize;
            line.append(",size=0x").append(sstream.str()) ;

            /* Decode type field */
            if( (parsedTsvFile.partitionsList.at(i).partType.compare("FileSystem") == 0) || (parsedTsvFile.partitionsList.at(i).partType.compare("System") == 0))
                line.append(",type=").append(guid.at("FileSystem")) ;
            else if(parsedTsvFile.partitionsList.at(i).partType.compare("Binary") == 0)
                line.append(",type=").append(guid.at("Binary")) ;
            else if(parsedTsvFile.partitionsList.at(i).partType.compare("FWU_MDATA") == 0)
                line.append(",type=").append(guid.at("FWU_MDATA")) ;
            else if(parsedTsvFile.partitionsList.at(i).partType.compare("ENV") == 0)
                line.append(",type=").append(guid.at("ENV")) ;
            else if(parsedTsvFile.partitionsList.at(i).partType.compare("FIP") == 0)
                line.append(",type=").append(guid.at("FIP")) ;
            else if(parsedTsvFile.partitionsList.at(i).partType.compare("ESP") == 0)
                line.append(",type=").append(guid.at("ESP")) ;
            else
            {
                //Nothing - random GUID will be attributed by U-Boot
            }

            /* Decode uuid field */
            if(parsedTsvFile.partitionsList.at(i).partType.compare("FIP") == 0)
            {
                if(parsedTsvFile.partitionsList.at(i).partName.compare("fip-a") == 0)
                    line.append(",uuid=").append(uuid.at("fip-a")) ;
                if(parsedTsvFile.partitionsList.at(i).partName.compare("fip-b") == 0)
                    line.append(",uuid=").append(uuid.at("fip-b")) ;
            }

            if(parsedTsvFile.partitionsList.at(i).partName.compare("rootfs") == 0)
            {
                if(parsedTsvFile.partitionsList.at(i).partIp.compare("mmc0") == 0)
                    line.append(",uuid=").append(uuid.at("mmc0")) ;
                if(parsedTsvFile.partitionsList.at(i).partIp.compare("mmc1") == 0)
                    line.append(",uuid=").append(uuid.at("mmc1")) ;
                if(parsedTsvFile.partitionsList.at(i).partIp.compare("mmc2") == 0)
                    line.append(",uuid=").append(uuid.at("mmc2")) ;
            }

            if(parsedTsvFile.partitionsList.at(i).partName.find("bootfs") == 0)
                line.append(",bootable");

            line.append("\\;") ;
        }
        script.append(line) ;
    }

    script.append(";fastboot usb 0") ;

    parsedTsvFile.scriptUbootTsvData = (unsigned char*) calloc(script.size() + SCRIPT_LAYOUT_HEADER_SIZE + SCRIPT_INFO_HEADER_SIZE +1, sizeof (unsigned char));
    memcpy(parsedTsvFile.scriptUbootTsvData + SCRIPT_LAYOUT_HEADER_SIZE + SCRIPT_INFO_HEADER_SIZE, script.c_str(), script.size()) ;

    parsedTsvFile.scriptUbootTsvDataSize = script.size() + SCRIPT_LAYOUT_HEADER_SIZE + SCRIPT_INFO_HEADER_SIZE;

    scriptDataInfoHeader infoScript ;
    infoScript.iSize = ((script.size() >> 24) & 0xff) | ((script.size() << 8) & 0xff0000) | ((script.size() >> 8) & 0xff00) | ((script.size() << 24) & 0xff000000) ;
    infoScript.iReserved = 0;

    unsigned char* ptr  ;
    ptr = (unsigned char *)&infoScript ;
    memcpy(parsedTsvFile.scriptUbootTsvData + SCRIPT_LAYOUT_HEADER_SIZE, ptr, SCRIPT_INFO_HEADER_SIZE) ;

    int sattus = prepareUbootScriptHeader(parsedTsvFile);
    return sattus ;
}

/**
 * @brief FileManager::getChecksumCrc32 : Calculate the CRC32 of a given data array.
 * @param data: The buffuer containing the data.
 * @param size: The length of data to apply.
 * @return integer value of the result.
 */
uint32_t FileManager::getChecksumCrc32(unsigned char* data, uint32_t size)
{
    unsigned long reg, tmp, j, k;
    reg = 0xffffffff;
    for (j = 0; j < size; j++)
    {
        reg ^= data[j];
        for (k = 0; k < 8; k++)
        {
            tmp = reg & 0x01;
            reg >>= 1;
            if (tmp)
            {
                reg ^= 0xedb88320;
            }
        }
    }

    return ~reg;
}

/**
 * @brief FileManager::saveTemproryScriptFile : search for a temprory path and save the data.
 * @param parsedTsvFile: The input parsed TSV file containing the script data.
 * @param outTempFile: Output variable to give the temprory path.
 * @return 0 if the operation is performed successfully, otherwise an error occurred.
 */
int FileManager::saveTemproryScriptFile(const fileTSV parsedTsvFile, std::string &outTempFile)
{
    displayManager.print(MSG_NORMAL, L"Preparing U-Boot Script/Flashlayout...");

#ifdef _WIN32
    char temp_dir[MAX_PATH];
    DWORD result = GetTempPathA(MAX_PATH, temp_dir);
    if ((result == 0) || (result > MAX_PATH))
    {
        displayManager.print(MSG_ERROR, L"Could not get temporary directory!");
        return TOOLBOX_DFU_ERROR_NO_FILE;
    }

    char tempFile[MAX_PATH];
    result = GetTempFileNameA(temp_dir, "STM32", 0, tempFile);
    if (result == 0)
    {
        displayManager.print(MSG_ERROR, L"Could not get temporary directory!");
        return TOOLBOX_DFU_ERROR_NO_FILE;
    }
#else // Linux & MacOS
    const char* temp_dir = std::getenv("TMPDIR");
    if (temp_dir == nullptr)
    {
        temp_dir = "/tmp";
    }

    std::string tempFile = std::string(temp_dir) + "/STM32";
#endif

#ifdef _WIN32
    std::ofstream outfile(tempFile, std::ios::binary | std::ios::out | std::ios::trunc);
#else
    std::ofstream outfile(tempFile.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
#endif

    if (outfile.is_open())
    {
        outfile.write((char*)parsedTsvFile.scriptUbootTsvData, parsedTsvFile.scriptUbootTsvDataSize);
        outfile.close();
    }
    else
    {
        displayManager.print(MSG_ERROR, L"Could not open temporary file!");
        return TOOLBOX_DFU_ERROR_NOT_SUPPORTED ;
    }

    outTempFile.erase();
    outTempFile +=  tempFile ;

    return TOOLBOX_DFU_NO_ERROR;
}

/**
 * @brief FileManager::removeTemproryScriptFile
 * @param tempFile: The path of temprory file to be removed.
 */

/**
 * @brief FileManager::removeTemproryScriptFile
 * @param tempFile: The path of temprory file to be removed.
 * @return 0 if there is no issue, otherwise an error occured.
 */
int FileManager::removeTemproryScriptFile(const std::string tempFile)
{
    displayManager.print(MSG_NORMAL, L"Removing temprory file : %s", tempFile.c_str());
    return std::remove(tempFile.c_str());
}

/**
 * @brief FileManager::isValidTsvFile: Check the validity of the given TSV file.
 * @param myTsvFile: The input parsed TSV file containing the list of partitions.
 * @param isBoot_PRGFW_UTIL: Flag fro checking between the boot application (U-Boot or STM32PRGFW-UTIL)
 * @return True if it is a valid TSV file, otherwise false
 */
bool FileManager::isValidTsvFile(fileTSV *myTsvFile, bool isBoot_PRGFW_UTIL)
{
    bool isFsblExist = false ;
    bool isFipExist = false ;

    for(auto &partition : myTsvFile->partitionsList)
    {
        if(partition.phaseID == 0x01)
            isFsblExist = true ;
        if(partition.phaseID == 0x03)
            isFipExist = true ;
    }

    if(isBoot_PRGFW_UTIL == true)
    {
        if(isFsblExist == false)
        {
            displayManager.print(MSG_ERROR, L"FSBL [0x01] firmware should be present in the TSV file !");
            return false ;
        }
    }
    else
    {
        if((isFsblExist == false) || (isFipExist == false))
        {
            displayManager.print(MSG_ERROR, L"FSBL [0x01] and FIP [0x03] firmwares should both be present in the TSV file !");
            return false ;
        }
    }

    return true ;
}

/**
 * @brief FileManager::prepareUbootFlashlayoutFile : prepare a Flashlayout data representing the Flash memory partitions.
 * @param parsedTsvFile: the TSV file that contains the list of partitions information.
 * @return 0 if there is no issue, otherwise an error occured.
 */
int FileManager::prepareUbootFlashlayoutFile(fileTSV &parsedTsvFile)
{
    /* https://wiki.st.com/stm32mpu/wiki/How_to_load_U-Boot_with_dfu-util#Generate_an_flashlayout-stm32_file */

    if(parsedTsvFile.partitionsList.empty())
        return TOOLBOX_DFU_ERROR_NO_MEM;

    std::string mdata = "" ;
    try
    {
        for(size_t idx= 0; idx < parsedTsvFile.partitionsList.size(); idx++)
        {
            const partitionInfo &part = parsedTsvFile.partitionsList.at(idx);
            char stPhaseId[10];
            sprintf(stPhaseId, "0x%02X", part.phaseID);
            mdata.append(part.opt).append("\t")
                .append(stPhaseId).append("\t")
                .append(part.partName).append("\t")
                .append(part.partType).append("\t")
                .append(part.partIp).append("\t")
                .append(part.offset).append("\n");
        }

    }
    catch (const std::exception& e)
    {
        displayManager.print(MSG_ERROR, L"Exception occured while preparing the U-Boot Flashlayout data : %s", e.what());
        return TOOLBOX_DFU_ERROR_UNSUPPORTED_FILE_FORMAT;
    }

    /* Add STM32 header to the data, it will be authenticated by U-Boot */
    createSTM32HeadredImage(mdata);
    parsedTsvFile.scriptUbootTsvData = (unsigned char*)malloc(mdata.size()+1);
    if(parsedTsvFile.scriptUbootTsvData == nullptr)
    {
        displayManager.print(MSG_ERROR, L"Unable to allocate memory for the Uboot Flashlayout TSV Data");
        return TOOLBOX_DFU_ERROR_NO_MEM;
    }

    memcpy(parsedTsvFile.scriptUbootTsvData, (unsigned char*)mdata.data(), mdata.size()+1);
    parsedTsvFile.scriptUbootTsvDataSize = mdata.size();

    return TOOLBOX_DFU_NO_ERROR;
}


/**
 * @brief FileManager::createSTM32HeadredImage : Add a STM32 header to a given data array to be decoded in next step by the U-Boot.
 * @param data: The string data to deploy.
 */
void FileManager::createSTM32HeadredImage(std::string& data)
{
    /*https://wiki.st.com/stm32mpu/wiki/STM32_header_for_binary_files */

    uint32_t checksumValue = 0;
    for(uint32_t j=0 ; j<data.size() ; j++)
    {
        /* The checksum is the sum of all the bytes in the input data */
        checksumValue += static_cast<unsigned char>(data[j]);
    }

    std::string header(FLASHLAYOUT_HEADER_SIZE, '\0'); // Initialize string with length and fill with null characters

    // Add magic number: first 4 bytes
    header[0] = 0x53; // 'S'
    header[1] = 0x54; // 'T'
    header[2] = 0x4D; // 'M'
    header[3] = 0x32; // '2'

    // Add checksum value at position 68th byte
    header[68] = static_cast<char>(checksumValue & 0xFF);
    header[69] = static_cast<char>((checksumValue >> 8) & 0xFF);
    header[70] = static_cast<char>((checksumValue >> 16) & 0xFF);
    header[71] = static_cast<char>((checksumValue >> 24) & 0xFF);

    // Add header version at position 72nd byte
    header[72] = 0x00;
    header[73] = 0x00;
    header[74] = 0x01;
    header[75] = 0x00;

    // Add image length at position 76th byte
    header[76] = static_cast<char>(data.size() & 0xFF);
    header[77] = static_cast<char>((data.size() >> 8) & 0xFF);
    header[78] = static_cast<char>((data.size() >> 16) & 0xFF);
    header[79] = static_cast<char>((data.size() >> 24) & 0xFF);

    // Add option flag at 100th byte
    header[100] = 0x01;
    header[101] = 0x00;
    header[102] = 0x00;
    header[103] = 0x00;

    data.insert(0,header);
}
