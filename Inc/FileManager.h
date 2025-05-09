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

#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <iostream>
#include <cstring>
#include <vector>
#include <regex>
#include <fstream>
#include <cstdint>
#include"DisplayManager.h"
#include "Error.h"

constexpr uint8_t TSV_NB_COLUMNS = 7;

constexpr uint32_t IH_MAGIC = 0x27051956;
constexpr uint8_t IH_TYPE_SCRIPT = 6;
constexpr uint8_t SCRIPT_LAYOUT_HEADER_SIZE = 64;
constexpr uint8_t SCRIPT_INFO_HEADER_SIZE = 8;
constexpr uint16_t FLASHLAYOUT_HEADER_SIZE = 256 ;

struct partitionInfo
{
    std::string opt;
    int phaseID;
    std::string partName;
    std::string partType;
    std::string partIp;
    std::string offset;
    std::string binary;
};

struct fileTSV
{
    unsigned char* scriptUbootTsvData;
    uint16_t scriptUbootTsvDataSize;
    std::vector<partitionInfo> partitionsList;
};

struct scriptLayoutHeader {
    uint32_t	sMagic;     /* Image Header Magic Number	*/
    uint32_t	sHcrc;      /* Image Header CRC Checksum	*/
    uint32_t	sTime;      /* Image Creation Timestamp	*/
    uint32_t	sSize;      /* Image Data Size		*/
    uint32_t	sLoad;      /* Data	 Load  Address		*/
    uint32_t	sEp;        /* Entry Point Address		*/
    uint32_t	sDcrc;      /* Image Data CRC Checksum	*/
    uint8_t		sOs;        /* Operating System		*/
    uint8_t		sArch;      /* CPU architecture		*/
    uint8_t		sType;      /* Image Type			*/
    uint8_t		sComp;	    /* Compression Type		*/
    uint8_t		sName[32];	/* Image Name		*/
} ;

struct scriptDataInfoHeader {
    uint32_t	iSize;      /* Script Data lenght */
    uint32_t	iReserved ; /* reserved, set it to 0 */
} ;

class FileManager
{
public:
    static FileManager& getInstance() ;
    int openTsvFile(const std::string &fileName, fileTSV **parsedFile, bool isStartFastboot = true);
    bool isValidTsvFile(fileTSV *myTsvFile, bool isBoot_PRGFW_UTIL) ;
    int saveTemproryScriptFile(const fileTSV parsedTsvFile, std::string &outTempFile) ;
    int removeTemproryFile(const std::string tempFile) ;
    int getTemproryFile(std::string &outTempFile) ;

private:
    FileManager();
    int parseTsvFile(const std::string tsvFolderPath, std::ifstream *inFile, fileTSV* parsedTSV, bool isStartFastboot = true);
    int splitStdString(std::string str, std::regex, std::vector<std::string>& substrings) ;
    uint32_t getChecksumCrc32(unsigned char* data, uint32_t size) ;
    int prepareUbootScriptFile(fileTSV & parsedTsvFile) ;
    int prepareUbootScriptHeader(fileTSV &parsedTsvFile);
    int prepareUbootFlashlayoutFile(fileTSV &parsedTsvFile) ;
    void createSTM32HeadredImage(std::string& data);

    DisplayManager displayManager = DisplayManager::getInstance() ;

};

#endif // FILEMANAGER_H
