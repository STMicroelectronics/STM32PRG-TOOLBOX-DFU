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

#ifndef ERROR_H
#define ERROR_H

enum ToolboxError {
    /** Success (no error) */
    TOOLBOX_DFU_NO_ERROR = 0,

    /** Device not connected */
    TOOLBOX_DFU_ERROR_NOT_CONNECTED = -1,

    /** Device not found */
    TOOLBOX_DFU_ERROR_NO_DEVICE = -2,

    /** Device connection error */
    TOOLBOX_DFU_ERROR_CONNECTION = -3,

    /** No such file  */
    TOOLBOX_DFU_ERROR_NO_FILE = -4,

    /** Operation not supported or unimplemented on this interface */
    TOOLBOX_DFU_ERROR_NOT_SUPPORTED = -5,

    /** Interface not supported or unimplemented on this plateform */
    TOOLBOX_DFU_ERROR_INTERFACE_NOT_SUPPORTED = -6,

    /** Insufficient memory */
    TOOLBOX_DFU_ERROR_NO_MEM = -7,

    /** Wrong parameters */
    TOOLBOX_DFU_ERROR_WRONG_PARAM = -8,

    /** Memory read failure */
    TOOLBOX_DFU_ERROR_READ = -9,

    /** Memory write failure */
    TOOLBOX_DFU_ERROR_WRITE = -10,

    /** File format not supported for this kind of device */
    TOOLBOX_DFU_ERROR_UNSUPPORTED_FILE_FORMAT = -11,

    /** Other error */
    TOOLBOX_DFU_ERROR_OTHER = -99,
};

#endif // ERROR_H
