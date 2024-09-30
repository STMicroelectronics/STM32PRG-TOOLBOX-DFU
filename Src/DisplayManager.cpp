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

#include "DisplayManager.h"
#ifdef _WIN32
#include <windows.h>
HANDLE  console;
CONSOLE_SCREEN_BUFFER_INFO SBInfo,CurSBInfo;
WORD OriginalBgColors;
#else
#include <cstdlib>
#endif

DisplayManager::DisplayManager()
{

}

DisplayManager & DisplayManager::getInstance()
{
    static DisplayManager instance;
    return instance;
}

/**
 * @brief DisplayManager::print : display a message in variadic format.
 * @param messageType: Coloring message depending on the context.
 * @param message: The string to display.
 */
void DisplayManager::print(messageType messageType, const wchar_t* message, ...)
{
    va_list args;
    va_start(args, message);
    std::wstring msgIndicator;

    switch(messageType)
    {
    case MSG_WARNING:
        msgIndicator = L"[Info]: ";
        break;
    case MSG_ERROR:
        msgIndicator = L"[Error]: ";
        break;
     default: ;
    }

    wchar_t * ws = (wchar_t *) malloc(30*1024*sizeof (wchar_t));
    if(ws == nullptr)
    {
        std::cout << "Error : " << "Memory allocation failed " << std::endl ;
        va_end(args);
        return  ;
    }
    vswprintf(ws, 30*1024, message, args);
    std::wstring s(std::move(msgIndicator));
    s += std::wstring(ws);

    displayMessage(messageType, s.c_str()) ;

    free(ws);
    va_end(args);
}

/**
 * @brief DisplayManager::displayMessage : print a message inline.
 * @param type: Coloring message and background depending on the context.
 * @param str: the string to display.
 */
void DisplayManager::displayMessage(messageType type, const wchar_t* str)
{
#ifdef _WIN32
    console = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO Infox;
    GetConsoleScreenBufferInfo(console, &Infox);
    DWORD backAttributes = Infox.wAttributes;
    BYTE BACKGROUND_COLOR = (Infox.wAttributes) & 0xF0 ;

    switch(type)
    {
    default:
    case  MSG_NORMAL:
        SetConsoleTextAttribute(console, WHITE | BACKGROUND_COLOR);
        break;
    case  MSG_GREEN:
        SetConsoleTextAttribute(console, FOREGROUND_GREEN | FOREGROUND_INTENSITY| BACKGROUND_COLOR);
        break;

    case  MSG_WARNING:
        SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY| BACKGROUND_COLOR);
        break;

    case  MSG_ERROR:
        SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_INTENSITY| BACKGROUND_COLOR);
        break;
    }

    std::wcout << str << std::endl ;
    SetConsoleTextAttribute(console, backAttributes);

#else
    switch (type)
    {
    case MSG_GREEN:
        printf("\033[00;32m");
        break;
    case MSG_NORMAL:
        printf("\033[39;49m");
        break;
    case MSG_WARNING:
        printf("\033[00;33m");
        break;
    case MSG_ERROR:
        printf("\033[00;31m");
        break;
    }

    std::wcout << str << std::endl;
    printf("\033[39;49m");

#endif

}
