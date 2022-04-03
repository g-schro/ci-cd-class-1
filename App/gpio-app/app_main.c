/*
 * @brief GPIO app.
 *
 * This simple app provides control of discrete/digital GPIO via a console serial
 * connection. Here are the commands and responses:
 *
 * dc <port> <pin> {i|o} {up|down|nopull|pushpull}
 *   Configure discrete GPIO, returns OK or error message.
 * dr <port> <pin>
 *   Reads discrete GPIO, returns 0, 1, or error message.
 * dw <port> <pin> {0|1
 *   Writes discrete GPIO, returns OK or error message.
 * reset
 *   Resets MCU.
 * version
 *   Writes software version ID.
 *
 * There is a help command that prints the information above.
 *
 * The design of this app is not great - it is "demo grade". It was chosen to
 * make the code as simple as possible, and easier to port to other STM32 MCUs.
 *
 * It does polling for UART input and output using the HAL library, rather than
 * use interrupts and an input buffer.  Because it polls for UART input, there
 * is a chance that UART overrun will occur, although it is currently OK (CPU
 * running at 72 MHz).
 *
 * MIT License
 * 
 * Copyright (c) 2021 Eugene R Schroeder
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include CONFIG_STM32_HAL_HDR

#include "version.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

struct port_info
{
    GPIO_TypeDef* gpio_port;
    const char port_name;
};

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

static void process_cmd(int argc, char** argv);
static void get_cmd_line(char* cmd_bfr, int cmd_bfr_size);
static int parse_cmd_line(char* cmd_bfr, char** cmd_tokens, int cmd_tokens_max);
static struct port_info* get_port_info(char port_name);
static const char* config_gpio(struct port_info* port_info, uint32_t pin_mask,
                               char* direction, char* pin_circuit);
static const char* enable_gpio_clock(char port_name);

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

static struct port_info ports_info[] = {

#ifdef GPIOA
    {GPIOA, 'A'},
#endif
#ifdef GPIOB
    {GPIOB, 'B'},
#endif
#ifdef GPIOC
    {GPIOC, 'C'},
#endif
#ifdef GPIOD
    {GPIOD, 'D'},
#endif
#ifdef GPIOE
    {GPIOE, 'E'},
#endif
#ifdef GPIOF
    {GPIOF, 'F'},
#endif
#ifdef GPIOG
    {GPIOG, 'G'},
#endif
#ifdef GPIOH
    {GPIOH, 'H'},
#endif
#ifdef GPIOI
    {GPIOI, 'I'},
#endif
#ifdef GPIOJ
    {GPIOJ, 'J'},
#endif
#ifdef GPIOK
    {GPIOK, 'K'},
#endif
};

////////////////////////////////////////////////////////////////////////////////
// Public (global) variables and externs
////////////////////////////////////////////////////////////////////////////////

extern UART_HandleTypeDef CONFIG_CONSOLE_UART;

////////////////////////////////////////////////////////////////////////////////
// Public (global) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Main function for the application.
 */
void app_main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0); // Remove buffering on stdout.
    printf("\nStarting\n");

    while (1) {
        const int CMD_BFR_SIZE = 80;
        char cmd_bfr[CMD_BFR_SIZE];
        const int MAX_ARGC = 8;
        int argc;
        char* argv[MAX_ARGC];

        get_cmd_line(cmd_bfr, CMD_BFR_SIZE);
        argc = parse_cmd_line(cmd_bfr, argv, MAX_ARGC);
        if (argc > 0)
            process_cmd(argc, argv);
    }
}

/*
 * @brief Print a character to the console serial connection.
 *
 * @param[in] c Character to print.
 *
 * @note This function will block.
 *
 * This function overrides the "weak" version that is called by the _write()
 * pseudo-system-call.
 */
void __io_putchar(char c)
{
    HAL_UART_Transmit(&CONFIG_CONSOLE_UART, (uint8_t*)&c, 1, HAL_MAX_DELAY);
    // Line conditioning to automatically do a CR after a NL.
    if (c == '\n') __io_putchar('\r');
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Process a command.
 *
 * @param[out] cmd_bfr Buffer to place characters (will be null terminated).
 */

void process_cmd(int argc, char** argv)
{
    const char* output_msg = "OK";
    const uint32_t PIN_NUM_MAX = 15;
    char* base_cmd;
    struct port_info* port_info;
    uint32_t pin_num;
    uint32_t pin_mask;
    enum {
        d_op_config,
        d_op_read,
        d_op_write
    } d_op;
    int required_argc;
    char* end_ptr;

    base_cmd = argv[0];
    if (strcasecmp(base_cmd, "help") == 0) {
        printf("dc <port> <pin> {i|o} {up|down|nopull|pushpull}\n  Configure dio, returns OK or error message\n"
               "dr <port> <pin>\n  Reads dio, returns 0, 1, or error message\n"
               "dw <port> <pin> {0|1}\n  Writes dio, returns 0, 1, or error message\n"
               "reset\n  Resets MCU\n"
               "version\n  Writes version string\n"
            );
        return;
    }
    if (strcasecmp(base_cmd, "reset") == 0) {
        NVIC_SystemReset();
        return;
    }
    if (strcasecmp(base_cmd, "version") == 0) {
        static const char* build_time = __DATE__ " " __TIME__; 
        printf("Built=\"%s\" Version=\"%s\"\n", build_time, VERSION);
        return;
    }

    if (strcasecmp(base_cmd, "dc") == 0) {
        d_op = d_op_config;
        required_argc = 5;
    } else if (strcasecmp(base_cmd, "dr") == 0) {
        d_op = d_op_read;
        required_argc = 3;
    } else if (strcasecmp(base_cmd, "dw") == 0) {
        d_op = d_op_write;
        required_argc = 4;
    } else {
        printf("Error: Unknown command\n");
        return;
    }

    if (argc != required_argc) {
        printf("Error: Invalid number of arguments\n");
    }

    // Command is dc/dr/dw, all have port and pin as first args.
    if (strlen(argv[1]) != 1) {
        printf("Error: Invalid GPIO port name\n");
    }
    port_info = get_port_info(*argv[1]);
    if (port_info == NULL) {
        printf("Error: Unknown GPIO port\n");
    }
    pin_num = strtoul(argv[2], &end_ptr, 10);
    if (*end_ptr != '\0') {
        printf("Error: Invalid GPIO pin\n");
    }
    if (pin_num > PIN_NUM_MAX) {
        printf("Error: Out of range GPIO pin\n");
    }
    pin_mask = 1 << pin_num;

    switch (d_op) {
        case d_op_config:
            output_msg = config_gpio(port_info, pin_mask, argv[3],
                                     argv[4]);
            if (output_msg == NULL)
                output_msg = "OK";
            break;

        case d_op_read:
            output_msg = HAL_GPIO_ReadPin(port_info->gpio_port, pin_mask) ==
                GPIO_PIN_RESET ? "\"0\"" : "\"1\"";
            break;

        case d_op_write:
            if (strcasecmp(argv[3], "0") == 0)
                HAL_GPIO_WritePin(port_info->gpio_port, pin_mask, GPIO_PIN_RESET);
            else if (strcasecmp(argv[3], "1") == 0)
                HAL_GPIO_WritePin(port_info->gpio_port, pin_mask, GPIO_PIN_SET);
            else
                output_msg = "Error: Invalid value";
            break;
    }
    printf("%s\n", output_msg);
}

/*
 * @brief Get a command line from the console serial connection.
 *
 * @param[out] cmd_bfr Buffer to place characters (will be null terminated).
 * @param[in]  cmd_bfr_size Size of buffer.
 *
 * @note This function will block.
 */
static void get_cmd_line(char* cmd_bfr, int cmd_bfr_size)
{
    HAL_StatusTypeDef rc;
    const int BFR_RESERVE_CHARS = 2;
    const int INACTIVE_IDX = -1;
    int bfr_put_idx = 0;
    int bfr_get_idx = 0;
    int bfr_eol_idx = INACTIVE_IDX;
    int bfr_bs_idx = INACTIVE_IDX;
    #define NUM_BS_CHARS 3
    char bs_bfr[NUM_BS_CHARS] = { '\b', ' ', '\b' };

    printf("> ");
    while (1) {
        char c;
        if (bfr_bs_idx == INACTIVE_IDX && bfr_eol_idx == INACTIVE_IDX) {
            rc =  HAL_UART_Receive(&CONFIG_CONSOLE_UART, (uint8_t*)&c, 1, 0);
            if (rc == HAL_OK) {
                if (c == '\n' || c == '\r') {
                    bfr_eol_idx = bfr_put_idx;
                    cmd_bfr[bfr_put_idx++] = '\n';
                    cmd_bfr[bfr_put_idx++] = '\r';
                } else if (c == '\b' || c == '\x7f') {
                    if (bfr_put_idx > 0 && bfr_put_idx == bfr_get_idx) {
                        bfr_put_idx--;
                        bfr_get_idx--;
                        bfr_bs_idx = 0;
                    }
                } else if (bfr_put_idx < (cmd_bfr_size - BFR_RESERVE_CHARS)) {
                    cmd_bfr[bfr_put_idx++] = c;
                }
            }
        }

        // The HAL_UART_Transmit() API has an odd behavior. If timeout is set to
        // 0, then even if it is able to transmit a character, it returns
        // HAL_TIMEOUT.  So we wait until the UART is ready to transmit, and
        // then call HAL_UART_Transmit() and assume it sent a character.

        if (__HAL_UART_GET_FLAG(&CONFIG_CONSOLE_UART, UART_FLAG_TXE)) {
            if (bfr_bs_idx != INACTIVE_IDX) {
                HAL_UART_Transmit(&CONFIG_CONSOLE_UART,
                                  (uint8_t*)&bs_bfr[bfr_bs_idx], 1, 0);
                if (++bfr_bs_idx >= NUM_BS_CHARS) {
                    bfr_bs_idx = INACTIVE_IDX;
                }
            } else if (bfr_put_idx != bfr_get_idx) {
                HAL_UART_Transmit(&CONFIG_CONSOLE_UART,
                                  (uint8_t*)&cmd_bfr[bfr_get_idx], 1, 0);
                bfr_get_idx++;
            } else if (bfr_eol_idx != INACTIVE_IDX) {
                cmd_bfr[bfr_eol_idx] = '\0';
                break;
            }
        }
    }
}

/*
 * @brief Parse a command line into tokens.
 *
 * @param[in]  cmd_bfr The raw command line.
 * @param[out] cmd_tokens Array to place pointers to the tokens.
 * @param[in]  cmd_tokens_max Size of cmd_tokens array.
 *
 * @return Number of tokens, or -1 in case of error.
 */
static int parse_cmd_line(char* cmd_bfr, char** cmd_tokens, int cmd_tokens_max)
{
    char* p = cmd_bfr;
    int num_tokens = 0;

    while (1) {
        // Find start of token.
        while (*p && isspace((unsigned char)*p))
            p++;
        if (*p == '\0') {
            // Found end of line.
            break;
        } else {
            if (num_tokens >= cmd_tokens_max) {
                printf("Error: Too many tokens\n");
                return -1;
            }
            // Record pointer to token and find its end.
            cmd_tokens[num_tokens++] = p;
            while (*p && !isspace((unsigned char)*p))
                p++;
            if (*p) {
                // Terminate token.
                *p++ = '\0';
            } else {
                // Found end of line.
                break;
            }
        }
    }
    return num_tokens;
}


/*
 * @brief Get information about GPIO port.
 *
 * @param[in]  port_name The single-letter port name.
 *
 * @return Pointer to information, or NULL if port does not exist.
 */
static struct port_info* get_port_info(char port_name)
{
    struct port_info* port_info;
    int idx;

    for (idx = 0; idx < ARRAY_SIZE(ports_info); idx++) {
        port_info = &ports_info[idx];
        if (port_info->port_name == toupper(port_name))
            return port_info;
    }
    return NULL;
}

/*
 * @brief Configure a GPIO pin.
 *
 * @param[in] port_info   Information about the port of the pin.
 * @param[in] pin_mask    The pin.
 * @param[in] direction   The direction (as expressed in the command).
 * @param[in] pin_circuit Pin circuit info (as expressed in the command).
 *
 * @return NULL is success, else an error message.
 */
static const char* config_gpio(struct port_info* port_info, uint32_t pin_mask,
                               char* direction, char* pin_circuit)
{
    const char *rc;

    GPIO_InitTypeDef init_data = {
        .Pin = pin_mask,
        .Speed = GPIO_SPEED_LOW,
#if CONFIG_GPIO_TYPE != 1
        .Alternate = 0,
#endif
    };

    rc = enable_gpio_clock(port_info->port_name);
    if (rc != NULL)
        return rc;

    if (strcasecmp(direction, "i") == 0)
        init_data.Mode = GPIO_MODE_INPUT;
    else if (strcasecmp(direction, "o") == 0)
        init_data.Mode = GPIO_MODE_OUTPUT_OD;
    else
        return "invalid direction";

    if (strcasecmp(pin_circuit, "up") == 0)
        init_data.Pull = GPIO_PULLUP;
    else if (strcasecmp(pin_circuit, "down") == 0)
        init_data.Pull = GPIO_PULLDOWN;
    else if (strcasecmp(pin_circuit, "nopull") == 0)
        init_data.Pull = GPIO_NOPULL;
    else if (strcasecmp(pin_circuit, "pushpull") == 0) {
        init_data.Pull = GPIO_NOPULL;
        init_data.Mode = GPIO_MODE_OUTPUT_PP;
    } else
        return "invalid mode";

    HAL_GPIO_Init(port_info->gpio_port, &init_data);
    return NULL;
}

/*
 * @brief Enable clock for GPIO.
 *
 * @param[in] port_name   Port name (single letter)
 *
 * @return NULL is success, else an error message.
 */
static const char* enable_gpio_clock(char port_name)
{
    switch (toupper(port_name))
    {
#ifdef GPIOA
        case 'A':
            __HAL_RCC_GPIOA_CLK_ENABLE();
            break;
#endif
#ifdef GPIOB
        case 'B':
            __HAL_RCC_GPIOB_CLK_ENABLE();
            break;
#endif
#ifdef GPIOC
        case 'C':
            __HAL_RCC_GPIOC_CLK_ENABLE();
            break;
#endif
#ifdef GPIOD
        case 'D':
            __HAL_RCC_GPIOD_CLK_ENABLE();
            break;
#endif
#ifdef GPIOE
        case 'E':
            __HAL_RCC_GPIOE_CLK_ENABLE();
            break;
#endif
#ifdef GPIOF
        case 'F':
            __HAL_RCC_GPIOF_CLK_ENABLE();
            break;
#endif
#ifdef GPIOG
        case 'G':
            __HAL_RCC_GPIOG_CLK_ENABLE();
            break;
#endif
#ifdef GPIOH
        case 'H':
            __HAL_RCC_GPIOH_CLK_ENABLE();
            break;
#endif
#ifdef GPIOI
        case 'I':
            __HAL_RCC_GPIOI_CLK_ENABLE();
            break;
#endif
#ifdef GPIOJ
        case 'J':
            __HAL_RCC_GPIOJ_CLK_ENABLE();
            break;
#endif
#ifdef GPIOK
        case 'K':
            __HAL_RCC_GPIOK_CLK_ENABLE();
            break;
#endif
        default:
            return "Invalid GPIO port";
    }
    return NULL;
}

