#ifndef _CONFIG_H_
#define _CONFIG_H_

/*
 * This file handles configuration of the application, including handling
 * differences between different hardware platforms.
 *
 * A key to hardware platform support is that the STM32CubeIDE-generated makefiles
 * create a #define for the MCU, for example STM32F103xB or STM32U575xx. These
 * defines can be used to control other settings, like the names of HAL/LL header
 * files, and UART used for the console.
 *
 * STM32 MCUs have a number of different hardware implementations for
 * peripherals like UART or I2C. However, a group of MCUs may share the same
 * implementation for a particular peripheral. The scheme used here is that
 * different impelmentation are given "type" numbers (type 1, type 2, etc) and
 * then each MCU define is mapped to the periperhal types it support.
 *
 * When using the HAL libaries, different between peripheral types are sometimes
 * (but not always) hidden.
 *
 * To add a new MCU, determine the MCU define supplied in the makefile, and add
 * it to code below.
 *
 */

/*
 * Peripheral type information:
 *
 * GPIO:
 *   Type 1: Does not have "Alternate" member in GPIO_InitTypeDef.
 */

////////////////////////////////////////////////////////////////////////////////
// Below are #defines based directly on "MCU type" #define from the IDE
// (makefile).
////////////////////////////////////////////////////////////////////////////////

#if defined STM32F103xB // STM32F103C8T6 (used by Bluepill)

    #define CONFIG_STM32_HAL_HDR "stm32f1xx_hal.h"
    #define CONFIG_CONSOLE_UART huart2
    #define CONFIG_GPIO_TYPE 1

#elif defined STM32F401xE

    #define CONFIG_STM32_HAL_HDR "stm32f4xx_hal.h"
    #define CONFIG_CONSOLE_UART huart2

#elif defined STM32L452xx

    #define CONFIG_STM32_HAL_HDR "stm32l4xx_hal.h"
    #define CONFIG_CONSOLE_UART huart2

#elif defined STM32U575xx

    #define CONFIG_STM32_HAL_HDR "stm32u5xx_hal.h"
    #define CONFIG_CONSOLE_UART huart1

#else
    #error Unknown processor
#endif

#endif // _CONFIG_H_
