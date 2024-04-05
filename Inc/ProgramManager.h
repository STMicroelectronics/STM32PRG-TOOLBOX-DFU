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

#ifndef PROGRAMMANAGER_H
#define PROGRAMMANAGER_H

#include <iostream>
#include "Inc/FileManager.h"
#include "Inc/DisplayManager.h"
#include "Inc/DFU.h"
#include "Inc/Error.h"

class ProgramManager
{
public:
    ProgramManager(const std::string toolboxFolder);
    ~ProgramManager();
    int startFlashingService(const std::string inputTsvPath, bool isStartFastboot) ;
    int readOtpPartition(const std::string filePath) ;
    int writeOtpPartition(const std::string filePath) ;

private:
    void sleep(uint32_t ms) ;

    DisplayManager displayManager = DisplayManager::getInstance() ;
    FileManager fileManager  = FileManager::getInstance() ;
    DFU *dfuInterface ;
    bool isDfuUbootRunning = false ;
    fileTSV *parsedTsvFile ;

};

#endif // PROGRAMMANAGER_H
