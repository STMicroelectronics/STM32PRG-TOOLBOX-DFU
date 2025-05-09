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

#include "main.h"
#include "ProgramManager.h"
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

std::string PRG_TOOLBOX_DFU_VERSION = "2.1.0";
std::string toolboxRootPath = "" ;

int main(int argc, char* argv[])
{
    std::string dfuSerialNumber = "";

    displayManager.print(MSG_NORMAL, L"      -------------------------------------------------------------------") ;
    displayManager.print(MSG_NORMAL, L"                      PRG-TOOLBOX-DFU v%s                      ", PRG_TOOLBOX_DFU_VERSION.c_str()) ;
    displayManager.print(MSG_NORMAL, L"      -------------------------------------------------------------------\n\n") ;

    int nCommands  = extractProgramCommands(argc, argv);
    if((nCommands < 0 ) || (nCommands == 0))
        return EXIT_FAILURE ;

    try
    {
        toolboxRootPath = fs::path(argv[0]).parent_path().string();
        if(toolboxRootPath.empty())
            toolboxRootPath = "." ; // The working directory is the same as where the executable is located.

        displayManager.print(MSG_NORMAL, L"TOOLBOX parent path : %s ", toolboxRootPath.c_str()) ;
    }
    catch(...)
    {
        displayManager.print(MSG_ERROR, L"Failed to get the Toolbox Root Path") ;
        return EXIT_FAILURE;
    }

    /* Check the list of commands before starting the execution (Option that can be placed anywhere on the command line) */
    for (int cmdIdx=0; cmdIdx < nCommands; cmdIdx++)
    {
        if(compareStrings(argumentsList[cmdIdx].cmd , "-sn", true) || compareStrings(argumentsList[cmdIdx].cmd , "--serial", true))
        {
            if((argumentsList[cmdIdx].nParams != 1))
            {
                displayManager.print(MSG_ERROR, L"Wrong parameters for -sn/--serial command") ;
                showHelp();
                return EXIT_FAILURE;
            }

            dfuSerialNumber = argumentsList[cmdIdx].Params[0];
            displayManager.print(MSG_NORMAL, L"Selected serial number : %s", dfuSerialNumber.data()) ;
        }
    }

    /* Search and execute commands */
    for (int cmdIdx=0; cmdIdx < nCommands; cmdIdx++)
    {
        if (compareStrings(argumentsList[cmdIdx].cmd , "-?", true) || compareStrings(argumentsList[cmdIdx].cmd , "-h", true) || compareStrings(argumentsList[cmdIdx].cmd , "--help", true))
        {
            showHelp();
        }
        else if(compareStrings(argumentsList[cmdIdx].cmd , "-v", true) || compareStrings(argumentsList[cmdIdx].cmd , "--version", true))
        {
            displayManager.print(MSG_NORMAL, L"PRG-TOOLBOX-DFU version : %s",  PRG_TOOLBOX_DFU_VERSION.data()) ;
        }
        else if(compareStrings(argumentsList[cmdIdx].cmd , "-l", true) || compareStrings(argumentsList[cmdIdx].cmd , "--list", true))
        {
            if((argumentsList[cmdIdx].nParams != 0))
            {
                displayManager.print(MSG_ERROR, L"Wrong parameters for -l/--list command") ;
                showHelp();
                return EXIT_FAILURE;
            }

            DFU *dfuInterface = new DFU();
            dfuInterface->toolboxFolder = toolboxRootPath;
            int ret = dfuInterface->displayDevicesList();

            delete dfuInterface;
            if(ret)
                return EXIT_FAILURE;
        }
        else if(compareStrings(argumentsList[cmdIdx].cmd , "-sn", true) || compareStrings(argumentsList[cmdIdx].cmd , "--serial", true))
        {
            if((argumentsList[cmdIdx].nParams != 1))
            {
                displayManager.print(MSG_ERROR, L"Wrong parameters for -sn/--serial command") ;
                showHelp();
                return EXIT_FAILURE;
            }

            dfuSerialNumber = argumentsList[cmdIdx].Params[0];
        }
        else if(compareStrings(argumentsList[cmdIdx].cmd , "-d", true) || compareStrings(argumentsList[cmdIdx].cmd , "--download", true))
        {
            if((argumentsList[cmdIdx].nParams > 2) || (argumentsList[cmdIdx].nParams < 1))
            {
                displayManager.print(MSG_ERROR, L"Wrong parameters for -d/--download command") ;
                showHelp();
                return EXIT_FAILURE;
            }

            std::string tsvFilePath = argumentsList[cmdIdx].Params[0];
            if(tsvFilePath.substr(tsvFilePath.size() - 4) != ".tsv" )
            {
                displayManager.print(MSG_ERROR, L"Download command : wrong file extension !\nExpected file extension is .tsv") ;
                showHelp();
                return EXIT_FAILURE;
            }

            bool isStartFastboot = true ; /* By default we require to start fastboot mode after flhasing boot partiotions */

            if(argumentsList[cmdIdx].nParams == 2) /* If there is an option "fastboot=0/1" */
            {
                std::string sFastboot = argumentsList[cmdIdx].Params[1];
                uint8_t enableFastboot = 0 ;

                try
                {
                    std::regex fastbootOption("fastboot=(\\d+)", std::regex_constants::icase);
                    std::smatch match;
                    if (std::regex_search(sFastboot, match, fastbootOption))
                    {
                        enableFastboot = std::stoul(match[1]);
                    }
                    else
                    {
                        displayManager.print(MSG_ERROR, L"-d/--download command, wrong option : %s", sFastboot.c_str()) ;
                        showHelp();
                        return EXIT_FAILURE;
                    }
                }
                catch (const std::regex_error& e)
                {
                    displayManager.print(MSG_ERROR, L"Regex error: %s", e.what());
                    showHelp();
                    return EXIT_FAILURE;
                }

                if(enableFastboot == 0)
                    isStartFastboot = false ;
                else if(enableFastboot == 1)
                    isStartFastboot = true ;
                else
                {
                    displayManager.print(MSG_ERROR, L"-d/--download command, wrong fastboot option value: %s | possible values [0 , 1]", sFastboot.c_str()) ;
                    showHelp();
                    return EXIT_FAILURE;
                }
            }

            ProgramManager *programMng = new ProgramManager(toolboxRootPath, dfuSerialNumber);
            int ret = programMng->startInstallService(std::move(tsvFilePath), isStartFastboot);
            delete programMng;

            if(ret)
                return EXIT_FAILURE;

        }
        else if(compareStrings(argumentsList[cmdIdx].cmd , "-f", true) || compareStrings(argumentsList[cmdIdx].cmd , "--flash", true))
        {
            if(argumentsList[cmdIdx].nParams != 1)
            {
                displayManager.print(MSG_ERROR, L"Wrong parameters for -f/--flash command") ;
                showHelp();
                return EXIT_FAILURE;
            }

            std::string tsvFilePath = argumentsList[cmdIdx].Params[0];
            if(tsvFilePath.substr(tsvFilePath.size() - 4) != ".tsv" )
            {
                displayManager.print(MSG_ERROR, L"Flash command : wrong file extension !\nExpected file extension is .tsv") ;
                showHelp();
                return EXIT_FAILURE;
            }

            ProgramManager *programMng = new ProgramManager(toolboxRootPath, dfuSerialNumber);
            int ret = programMng->startFlashingService(std::move(tsvFilePath));
            delete programMng;

            if(ret)
                return EXIT_FAILURE;

        }
        else if(compareStrings(argumentsList[cmdIdx].cmd , "-otp", true) || compareStrings(argumentsList[cmdIdx].cmd , "--otp", true))
        {
            if(argumentsList[cmdIdx].nParams != 2 )
            {
                displayManager.print(MSG_ERROR, L"Wrong parameters for -otp/--otp command") ;
                showHelp();
                return EXIT_FAILURE;
            }

            std::string operationType = argumentsList[cmdIdx].Params[0];
            std::string filePath = argumentsList[cmdIdx].Params[1];
            filePath = "\"" + filePath + "\"";
            if((compareStrings(operationType, "read", true) == false) && (compareStrings(operationType, "write", true) == false))
            {
                displayManager.print(MSG_ERROR, L"OTP command, operation is not defined !") ;
                showHelp();
                return EXIT_FAILURE;
            }

            ProgramManager *programMng = new ProgramManager(toolboxRootPath, dfuSerialNumber);
            int ret = -1 ;

            if(compareStrings(operationType, "write", true))
                ret = programMng->writeOtpPartition(std::move(filePath));
            else
                ret = programMng->readOtpPartition(std::move(filePath));

            delete programMng;
            if(ret)
            {
                displayManager.print(MSG_ERROR, L"OTP command, Read/Write operation failed !") ;
                return EXIT_FAILURE;
            }
        }
        else if(compareStrings(argumentsList[cmdIdx].cmd , "-p", true) || compareStrings(argumentsList[cmdIdx].cmd , "--phase", true))
        {
            if((argumentsList[cmdIdx].nParams != 0))
            {
                displayManager.print(MSG_ERROR, L"Wrong parameters for -p/--phase command") ;
                showHelp();
                return EXIT_FAILURE;
            }

            ProgramManager *programMng = new ProgramManager(toolboxRootPath, dfuSerialNumber);
            int ret = -1 ;
            uint8_t phase = 0xFF; bool isNeedDetach = false;
            ret = programMng->getPhase(&phase, &isNeedDetach);
            delete programMng;

            if(ret)
            {
                displayManager.print(MSG_ERROR, L"Get Phase ID command, -p/--phase operation failed !") ;
                return EXIT_FAILURE;
            }
        }
        else
        {
            displayManager.print(MSG_ERROR, L"Wrong command [ %s ]: Unknown command or command missed some parameters.\nPlease refer to the help for the supported commands.", argumentsList[cmdIdx].cmd.c_str()) ;
            showHelp();
            return EXIT_FAILURE;
        }

    }

    return EXIT_SUCCESS;
}

/**
 * @brief compareStrings: Compare two strings with with case tracking.
 * @param str1: First string to compare.
 * @param str2: Second string to compare.
 * @param caseInsensitive: True to compare with insensitve case, otherwise false.
 * @return True if the two strings are equal, otherwise false.
 */
bool compareStrings(const std::string& str1, const std::string& str2, bool caseInsensitive)
{
    if(caseInsensitive == true)
    {
        std::string str1Lower = str1;
        std::transform(str1Lower.begin(), str1Lower.end(), str1Lower.begin(), ::tolower);
        std::string str2Lower = str2;
        std::transform(str2Lower.begin(), str2Lower.end(), str2Lower.begin(), ::tolower);

        // Compare the lowercase std::strings: returns true if it is equal
        return str1Lower.compare(str2Lower) == 0;
    }
    else
    {
        return str1.compare(str2) ;
    }
}

/**
 * @brief extractProgramCommands: check and extract the total of commands which are passed to the program.
 * @param numberCommands: Initial number of commands passed to the program.
 * @param commands: List of string passed as commands.
 * @return postive value indacating the number of commands , otherwise, negative value to eraise an error.
 */
int extractProgramCommands(int numberCommands, char* commands[])
{
    if(numberCommands == 1) // No argument
    {
        showHelp();
        return 0;
    }

    uint16_t nCmds  =0;
    uint16_t argumentIndex = 1;
    while(argumentIndex < numberCommands)
    {
        if (commands[argumentIndex][0] == '-')
        {
            /* Extract commands */
            argumentsList[nCmds].cmd = commands[argumentIndex];

            /* Extract parameters */
            argumentsList[nCmds].nParams=0;
            if (argumentIndex+1 < numberCommands)
            {
                while ((argumentIndex+1+argumentsList[nCmds].nParams<numberCommands) && (commands[argumentIndex+1+argumentsList[nCmds].nParams][0] != '-') && (argumentsList[nCmds].nParams < MAX_PARAMS_NBR))
                {
                    argumentsList[nCmds].Params[argumentsList[nCmds].nParams] = commands[argumentIndex+1+argumentsList[nCmds].nParams];
                    argumentsList[nCmds].nParams++;
                }
            }
            argumentIndex += argumentsList[nCmds].nParams+1;
            nCmds++;
        }
        else
        {
            displayManager.print(MSG_ERROR, L"Argument error. Use -? for help") ;
            showHelp();
            return TOOLBOX_DFU_ERROR_WRONG_PARAM;
        }
    }

    /* verify the command syntax */
    bool validCommand = false;
    for (int cmdIdx=0; cmdIdx < nCmds; cmdIdx++)
    {
        validCommand = false;
        for (int i=0; i < MAX_COMMANDS_NBR; i++)
        {
            if (compareStrings(argumentsList[cmdIdx].cmd , supportedCommandList[i], true) == true)
            {
                validCommand=true;
                break ;
            }
        }

        if (validCommand == false)
        {
            displayManager.print(MSG_ERROR, L"Invalid command : %s", argumentsList[cmdIdx].cmd.data()) ;
            showHelp();
            return TOOLBOX_DFU_ERROR_WRONG_PARAM;
        }
    }

    return nCmds ;
}

/**
 * @brief showHelp : Display the list of available commands.
 */
void showHelp()
{
    displayManager.print(MSG_GREEN, L"\nUsage :") ;
    displayManager.print(MSG_NORMAL, L"PRG-TOOLBOX-DFU [command_1] [Arguments_1] [[command_2] [Arguments_2]...]\n") ;


    displayManager.print(MSG_NORMAL, L"--help        -h   -?       : Show the help menu.") ;
    displayManager.print(MSG_NORMAL, L"--version          -v       : Display the program version.") ;
    displayManager.print(MSG_NORMAL, L"--list             -l       : Display the list of available STM32 DFU devices.") ;
    displayManager.print(MSG_NORMAL, L"--serial           -sn      : Select the USB device by serial number.") ;
    displayManager.print(MSG_NORMAL, L"--download         -d       : Prepare the device, install U-Boot and enable/disable fastboot mode.") ;
    displayManager.print(MSG_NORMAL, L"       <filePath.tsv>       : TSV file path") ;
    displayManager.print(MSG_NORMAL, L"       <fastboot=0/1>       : Optional flag to configure the fastboot, possible value [0, 1]") ;
    displayManager.print(MSG_NORMAL, L"                              [0] initiate the flashing process and Fastboot will not be launched") ;
    displayManager.print(MSG_NORMAL, L"                              [1] initiate the flashing process and launch Fastboot") ;
    displayManager.print(MSG_NORMAL, L"                              Note: if it is not specified, the default value is 1") ;

    displayManager.print(MSG_NORMAL, L"--flash            -f       : Prepare the device and flash the list of partitions through DFU interface") ;
    displayManager.print(MSG_NORMAL, L"       <filePath.tsv>       : TSV file path") ;

    displayManager.print(MSG_NORMAL, L"--otp         -otp          : Read and write the OTP partition") ;
    displayManager.print(MSG_NORMAL, L"       <operationType>      : read/write") ;
    displayManager.print(MSG_NORMAL, L"       <filePath.bin>       : The output file of the read and the input binary path of the write") ;

    displayManager.print(MSG_NORMAL, L"--phase             -p      : Get and display the running Phase ID.") ;

    displayManager.print(MSG_NORMAL, L"") ;
}
