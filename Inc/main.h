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

#ifndef MAIN_H
#define MAIN_H

#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdint>
#include "DisplayManager.h"
#include "Error.h"

constexpr uint8_t  MAX_COMMANDS_NBR = 14 ;
constexpr uint8_t  MAX_PARAMS_NBR = 5 ;

using namespace std;

struct command
{
    string cmd; // command
    uint8_t nParams; // number of parameters
    string Params[MAX_PARAMS_NBR]; // optional parameters.
};


command argumentsList[MAX_COMMANDS_NBR];
const string supportedCommandList[MAX_COMMANDS_NBR]={"-d", "--download", "?", "-h", "--help", "-v", "-otp", "--otp", "-sn", "--serial", "-f", "--flash", "-l", "--list"} ;

DisplayManager displayManager = DisplayManager::getInstance() ;
int extractProgramCommands (int numberCommands, char* commands[]);
bool compareStrings(const std::string& str1, const std::string& str2, bool caseInsensitive) ;
void showHelp();

#endif // MAIN_H
