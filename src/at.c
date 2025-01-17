/*!*****************************************************************
 * \file    at.c
 * \brief   AT command manager.
 *******************************************************************
 * \copyright
 *
 * Copyright (c) 2022, UnaBiz SAS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1 Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  2 Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  3 Neither the name of UnaBiz SAS nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *******************************************************************/

#include "at.h"

#include "at_hw_api.h"
#include "stdio.h"
#include "string.h"

/*** AT local macros ***/

#define AT_BUFFER_SIZE                      128

#define AT_COMMAND_LIST_SIZE                64

#define AT_HEADER                           "AT"

#define AT_COMMAND_MARKER_END               '\r'
#define AT_COMMAND_MARKER_EXECUTION         '\0'
#define AT_COMMAND_MARKER_READ_HELP         '?'
#define AT_COMMAND_MARKER_WRITE             '='

#define AT_COMMAND_HEADER_BASIC             '\0'
#define AT_COMMAND_HEADER_EXTENDED          '$'
#define AT_COMMAND_HEADER_DEBUG             '!'
#define AT_COMMAND_HEADER_HELP              "        -> "

#define AT_COMMAND_PARAMETER_SEPARATOR      ','
#define AT_COMMAND_PARAMETER_MAX_NUMBER     10

#define AT_REPLY_END                        "\r\n"

/*** AT local structures ***/

/*******************************************************************/
typedef union {
    struct {
        uint8_t process :1;
        uint8_t pending :1;
        uint8_t quiet :1;
        uint8_t verbose :1;
        uint8_t echo :1;
    } field;
    uint8_t all;
} AT_flags_t;

/*******************************************************************/
typedef struct {
    AT_process_cb_t process_callback;
    volatile AT_flags_t flags;
    char rx_buffer[AT_BUFFER_SIZE];
    uint8_t rx_buffer_size;
    const AT_command_t *current_command;
    const AT_command_t *commands_list[AT_COMMAND_LIST_SIZE];
    uint8_t commands_count[AT_COMMAND_TYPE_LAST];
} AT_context_t;

/*** AT local functions declaration ***/

AT_status_t _echo_execution_callback(int32_t *error_code);
AT_status_t _echo_write_callback(uint32_t argc, char *argv[], int32_t *error_code);

AT_status_t _verbose_execution_callback(int32_t *error_code);
AT_status_t _verbose_write_callback(uint32_t argc, char *argv[], int32_t *error_code);

AT_status_t _quiet_execution_callback(int32_t *error_code);
AT_status_t _quiet_write_callback(uint32_t argc, char *argv[], int32_t *error_code);

/*** AT local global variables ***/

static const char AT_COMMAND_HEADER[AT_COMMAND_TYPE_LAST] = {
    AT_COMMAND_HEADER_BASIC,
    AT_COMMAND_HEADER_EXTENDED,
    AT_COMMAND_HEADER_DEBUG,
};

static const AT_command_t AT_COMMAND_ECHO = {
    .syntax = "E",
    .type = AT_COMMAND_TYPE_BASIC,
    .help = "Interface echo control",
    .execution_callback = &_echo_execution_callback,
    .execution_help = "Disable echo",
    .read_callback = NULL,
    .read_help = NULL,
    .write_callback = &_echo_write_callback,
    .write_arguments = "<enable>",
    .write_help = "Enable (1) or disable (0) echo",
};

static const AT_command_t AT_COMMAND_VERBOSE = {
    .syntax = "V",
    .type = AT_COMMAND_TYPE_BASIC,
    .help = "Interface verbosity level",
    .execution_callback = &_verbose_execution_callback,
    .execution_help = "Disable verbose mode",
    .read_callback = NULL,
    .read_help = NULL,
    .write_callback = &_verbose_write_callback,
    .write_arguments = "<enable>",
    .write_help = "Enable (1) or disable (0) verbose mode",
};

static const AT_command_t AT_COMMAND_QUIET = {
    .syntax = "Q",
    .type = AT_COMMAND_TYPE_BASIC,
    .help = "Interface quiet mode control",
    .execution_callback = &_quiet_execution_callback,
    .execution_help = "Disable quiet mode",
    .read_callback = NULL,
    .read_help = NULL,
    .write_callback = &_quiet_write_callback,
    .write_arguments = "<enable>",
    .write_help = "Enable (1) or disable (0) quiet mode",
};

static AT_context_t at_ctx = {
    .process_callback = NULL,
    .flags.all = 0,
    .rx_buffer = {0x00},
    .rx_buffer_size = 0,
    .current_command = NULL,
    .commands_list = {NULL},
    .commands_count = {0},
};

/*** AT local functions ***/

/*******************************************************************/
static void _rx_irq_callback(uint8_t data) {
    // Ignore null data and if processing pending.
    if (data == 0x00 || at_ctx.flags.field.pending == 1) {
        goto errors;
    }
    // Check end marker.
    if (data == AT_COMMAND_MARKER_END) {
        // Set flag.
        at_ctx.flags.field.process = 1;
        at_ctx.flags.field.pending = 1;
        // Ask for processing.
        if (at_ctx.process_callback != NULL) {
            at_ctx.process_callback();
        }
    } else {
        // Store new byte in buffer.
        at_ctx.rx_buffer[at_ctx.rx_buffer_size] = (char) data;
        at_ctx.rx_buffer_size = (at_ctx.rx_buffer_size + 1) % AT_BUFFER_SIZE;
    }
errors:
    return;
}

/*******************************************************************/
AT_status_t _parse_bit(uint32_t argc, char *argv[], uint8_t *bit) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    int tmp_int = 0;
    // Check number of arguments.
    if (argc != 1) {
        status = AT_ERROR_EXTERNAL_COMMAND_BAD_PARAMETER_NUMBER;
        goto errors;
    }
    // Parse parameter.
    if (sscanf((argv[0]), "%d", &tmp_int) != 1) {
        status = AT_ERROR_EXTERNAL_COMMAND_BAD_PARAMETER_PARSING;
        goto errors;
    }
    if (tmp_int > 1) {
        status = AT_ERROR_EXTERNAL_COMMAND_BAD_PARAMETER_VALUE;
        goto errors;
    }
    (*bit) = (uint8_t) tmp_int;
    return AT_SUCCESS;
errors:
    return status;
}

/*******************************************************************/
AT_status_t _echo_execution_callback(int32_t *error_code) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    // Reset error code.
    (*error_code) = 0;
    // Disable echo.
    at_ctx.flags.field.echo = 0;
    return status;
}

/*******************************************************************/
AT_status_t _echo_write_callback(uint32_t argc, char *argv[], int32_t *error_code) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    uint8_t enable = 0;
    // Reset error code.
    (*error_code) = 0;
    // Check number of arguments.
    status = _parse_bit(argc, argv, &enable);
    if (status != AT_SUCCESS) {
        goto errors;
    }
    // Update echo.
    at_ctx.flags.field.echo = enable;
    return AT_SUCCESS;
errors:
    return status;
}

/*******************************************************************/
AT_status_t _verbose_execution_callback(int32_t *error_code) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    // Reset error code.
    (*error_code) = 0;
    // Disable verbose.
    at_ctx.flags.field.verbose = 0;
    return status;
}

/*******************************************************************/
AT_status_t _verbose_write_callback(uint32_t argc, char *argv[], int32_t *error_code) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    uint8_t enable = 0;
    // Reset error code.
    (*error_code) = 0;
    // Check number of arguments.
    status = _parse_bit(argc, argv, &enable);
    if (status != AT_SUCCESS) {
        goto errors;
    }
    // Update echo.
    at_ctx.flags.field.verbose = enable;
    return AT_SUCCESS;
errors:
    return status;
}

/*******************************************************************/
AT_status_t _quiet_execution_callback(int32_t *error_code) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    // Reset error code.
    (*error_code) = 0;
    // Disable verbose.
    at_ctx.flags.field.quiet = 0;
    return status;
}

/*******************************************************************/
AT_status_t _quiet_write_callback(uint32_t argc, char *argv[], int32_t *error_code) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    uint8_t enable = 0;
    // Reset error code.
    (*error_code) = 0;
    // Check number of arguments.
    status = _parse_bit(argc, argv, &enable);
    if (status != AT_SUCCESS) {
        goto errors;
    }
    // Update echo.
    at_ctx.flags.field.quiet = enable;
    return AT_SUCCESS;
errors:
    return status;
}

/*******************************************************************/
static AT_status_t _print_tab(char *tab, uint32_t tab_size) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    // Check quiet flag.
    if ((at_ctx.flags.field.quiet != 0) || (tab_size == 0)) {
        goto errors;
    }
    // Write text.
    status = AT_HW_API_write((uint8_t *) tab, tab_size);
    if (status != AT_SUCCESS) {
        goto errors;
    }
errors:
    return status;
}

/*******************************************************************/
static AT_status_t _print(const char *text) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    // Write end marker.
    status = _print_tab((char *) text, strlen(text));
    return status;
}

/*******************************************************************/
static AT_status_t _end_line(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    // Write end marker.
    status = _print_tab(AT_REPLY_END, 2);
    return status;
}

/*******************************************************************/
static AT_status_t _print_line(const char *line) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    // Write text.
    status = _print(line);
    if (status != AT_SUCCESS) {
        goto errors;
    }
    // Write end marker.
    status = _end_line();
errors:
    return status;
}

/*******************************************************************/
static void _print_command_status(AT_status_t at_status, int32_t error_code) {
    // Local variables.
    uint32_t reply_size = 0;
    char tx_buffer[AT_BUFFER_SIZE] = {0x00};
    // Check verbose flag.
    if (at_ctx.flags.field.verbose == 0) {
        // Print status as numerical value.
        reply_size = snprintf(tx_buffer, AT_BUFFER_SIZE, "%d", at_status);
    } else {
        // Print header.
        if (at_status == AT_SUCCESS) {
            _print("OK");
        } else {
            _print("ERROR:");
            switch (at_status) {
            case AT_ERROR_INTERNAL_COMMAND_PARSING:
                reply_size = snprintf(tx_buffer, AT_BUFFER_SIZE, "COMMAND_PARSING");
                break;
            case AT_ERROR_INTERNAL_COMMAND_NOT_FOUND:
                reply_size = snprintf(tx_buffer, AT_BUFFER_SIZE, "COMMAND_NOT_FOUND");
                break;
            case AT_ERROR_INTERNAL_COMMAND_MARKER_NOT_DEFINED:
                reply_size = snprintf(tx_buffer, AT_BUFFER_SIZE, "COMMAND_MARKER_NOT_DEFINED");
                break;
            case AT_ERROR_INTERNAL_COMMAND_EXECUTION_NOT_DEFINED:
                reply_size = snprintf(tx_buffer, AT_BUFFER_SIZE, "COMMAND_EXECUTION_NOT_DEFINED");
                break;
            case AT_ERROR_INTERNAL_COMMAND_WRITE_NOT_DEFINED:
                reply_size = snprintf(tx_buffer, AT_BUFFER_SIZE, "COMMAND_WRITE_NOT_DEFINED");
                break;
            case AT_ERROR_INTERNAL_COMMAND_READ_NOT_DEFINED:
                reply_size = snprintf(tx_buffer, AT_BUFFER_SIZE, "COMMAND_READ_NOT_DEFINED");
                break;
            case AT_ERROR_EXTERNAL_COMMAND_BAD_PARAMETER_NUMBER:
                reply_size = snprintf(tx_buffer, AT_BUFFER_SIZE, "COMMAND_BAD_PARAMETER_NUMBER:%ld", error_code);
                break;
            case AT_ERROR_EXTERNAL_COMMAND_BAD_PARAMETER_PARSING:
                reply_size = snprintf(tx_buffer, AT_BUFFER_SIZE, "COMMAND_BAD_PARAMETER_PARSING:%ld", error_code);
                break;
            case AT_ERROR_EXTERNAL_COMMAND_BAD_PARAMETER_VALUE:
                reply_size = snprintf(tx_buffer, AT_BUFFER_SIZE, "COMMAND_BAD_PARAMETER_VALUE:%ld", error_code);
                break;
            case AT_ERROR_EXTERNAL_COMMAND_CORE_ERROR:
                if (at_ctx.current_command->enum_to_str_callback != NULL) {
                    reply_size = snprintf(tx_buffer, AT_BUFFER_SIZE, "COMMAND_CORE_ERROR:%s", at_ctx.current_command->enum_to_str_callback(error_code));
                } else {
                    reply_size = snprintf(tx_buffer, AT_BUFFER_SIZE, "COMMAND_CORE_ERROR:0x%02X", (unsigned int) error_code);
                }
                break;
            default:
                reply_size = snprintf(tx_buffer, AT_BUFFER_SIZE, "UNKNOWN:%d", at_status);
                break;
            }
        }
    }
    _print_tab(tx_buffer, reply_size);
    _end_line();
}

/*******************************************************************/
static AT_status_t _parse_and_execute_command(char *input_command, AT_command_type_t type, int32_t *command_return_code) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    uint32_t command_size = 0;
    char *content = input_command;
    char *parameter;
    const char *separators = ",\0";
    char *command_argv[AT_COMMAND_PARAMETER_MAX_NUMBER] = {NULL};
    uint32_t command_argc = 0;
    uint32_t idx = 0;
    // Reset current command.
    at_ctx.current_command = NULL;
    // Search command in list.
    for (idx = 0; idx < AT_COMMAND_LIST_SIZE; idx++) {
        // Check existence.
        if (at_ctx.commands_list[idx] == NULL) {
            continue;
        }
        // Check type.
        if ((at_ctx.commands_list[idx]->type) != type) {
            continue;
        }
        // Check syntax.
        command_size = strlen(at_ctx.commands_list[idx]->syntax);
        if (memcmp(input_command, (at_ctx.commands_list[idx]->syntax), command_size) == 0) {
            // Update local pointer.
            at_ctx.current_command = at_ctx.commands_list[idx];
            break;
        }
    }
    if (at_ctx.current_command == NULL) {
        status = AT_ERROR_INTERNAL_COMMAND_NOT_FOUND;
        goto errors;
    }
    // Check marker.
    if (input_command[command_size] == AT_COMMAND_MARKER_EXECUTION) {
        // Check if read command exists.
        if ((at_ctx.current_command->execution_callback) == NULL) {
            status = AT_ERROR_INTERNAL_COMMAND_EXECUTION_NOT_DEFINED;
            goto errors;
        }
        // Execute command.
        status = (at_ctx.current_command)->execution_callback(command_return_code);
        if (status != AT_SUCCESS) {
            goto errors;
        }
    } else if (input_command[command_size] == AT_COMMAND_MARKER_READ_HELP) {
        // Check if read command exists.
        if ((at_ctx.current_command->read_callback) == NULL) {
            status = AT_ERROR_INTERNAL_COMMAND_READ_NOT_DEFINED;
            goto errors;
        }
        // Execute command.
        status = (at_ctx.current_command)->read_callback(command_return_code);
        if (status != AT_SUCCESS) {
            goto errors;
        }
    } else if ((input_command[command_size] == AT_COMMAND_MARKER_WRITE) || (type == AT_COMMAND_TYPE_BASIC)) {
        // Check if write command exists.
        if ((at_ctx.current_command->write_callback) == NULL) {
            status = AT_ERROR_INTERNAL_COMMAND_WRITE_NOT_DEFINED;
            goto errors;
        }
        if (type == AT_COMMAND_TYPE_BASIC) {
            content = &input_command[command_size];
        } else {
            content = &input_command[command_size + 1];
        }
        // Search parameters.
        while (*content == AT_COMMAND_PARAMETER_SEPARATOR) {
            command_argv[command_argc++] = NULL;
            content++;
        }
        // Parse parameter.
        while ((parameter = strtok_r(content, separators, &content))) {
            command_argv[command_argc++] = parameter;
            while (*content == AT_COMMAND_PARAMETER_SEPARATOR) {
                command_argv[command_argc++] = NULL;
                content++;
            }
        }
        // Execute command.
        status = (at_ctx.current_command)->write_callback(command_argc, command_argv, command_return_code);
        if (status != AT_SUCCESS) {
            goto errors;
        }
    } else {
        status = AT_ERROR_INTERNAL_COMMAND_MARKER_NOT_DEFINED;
        goto errors;
    }
    return AT_SUCCESS;
errors:
    return status;
}

/*******************************************************************/
AT_status_t _print_command_header(AT_command_type_t type) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    char header[2] = {0x00};
    // Check type.
    if (type >= AT_COMMAND_TYPE_LAST) {
        status = AT_ERROR_COMMAND_TYPE;
        goto errors;
    }
    header[0] = AT_COMMAND_HEADER[type];
    status = _print(header);
errors:
    return status;
}

/*******************************************************************/
AT_status_t _print_help(AT_command_type_t type) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    uint32_t idx = 0;
    char marker[2] = {0x00};
    // Check number of registered commands.
    if (at_ctx.commands_count[type] == 0) {
        status = _print_line("    None");
        if (status != AT_SUCCESS) {
            goto errors;
        }
    } else {
        // Commands loop.
        for (idx = 0; idx < (sizeof(at_ctx.commands_list) / sizeof(AT_command_t *)); idx++) {
            // Check existence and type.
            if ((at_ctx.commands_list[idx] != NULL) && ((at_ctx.commands_list[idx]->type) == type)) {
                // Print common syntax.
                status = _print("    ");
                if (status != AT_SUCCESS) {
                    goto errors;
                }
                status = _print(at_ctx.commands_list[idx]->syntax);
                if (status != AT_SUCCESS) {
                    goto errors;
                }
                status = _print(" : ");
                if (status != AT_SUCCESS) {
                    goto errors;
                }
                status = _print_line(at_ctx.commands_list[idx]->help);
                if (status != AT_SUCCESS) {
                    goto errors;
                }
                // Execution callback.
                if (at_ctx.commands_list[idx]->execution_callback != NULL) {
                    status = _print(AT_COMMAND_HEADER_HELP);
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                    status = _print(AT_HEADER);
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                    status = _print_command_header(type);
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                    status = _print(at_ctx.commands_list[idx]->syntax);
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                    status = _print(" : ");
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                    status = _print_line(at_ctx.commands_list[idx]->execution_help);
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                }
                // Write callback.
                if ((at_ctx.commands_list[idx]->write_callback) != NULL) {
                    status = _print(AT_COMMAND_HEADER_HELP);
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                    status = _print(AT_HEADER);
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                    status = _print_command_header(type);
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                    status = _print(at_ctx.commands_list[idx]->syntax);
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                    if (type != AT_COMMAND_TYPE_BASIC) {
                        marker[0] = AT_COMMAND_MARKER_WRITE;
                        status = _print(marker);
                        if (status != AT_SUCCESS) {
                            goto errors;
                        }
                    }
                    status = _print(at_ctx.commands_list[idx]->write_arguments);
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                    status = _print(" : ");
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                    status = _print_line(at_ctx.commands_list[idx]->write_help);
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                }
                // Read callback.
                if ((at_ctx.commands_list[idx]->read_callback) != NULL) {
                    status = _print(AT_COMMAND_HEADER_HELP);
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                    status = _print(AT_HEADER);
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                    status = _print_command_header(type);
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                    status = _print(at_ctx.commands_list[idx]->syntax);
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                    marker[0] = AT_COMMAND_MARKER_READ_HELP;
                    status = _print(marker);
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                    status = _print(" : ");
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                    status = _print_line(at_ctx.commands_list[idx]->read_help);
                    if (status != AT_SUCCESS) {
                        goto errors;
                    }
                }
            }
        }
    }
errors:
    return status;
}

/*** AT local functions ***/

/*******************************************************************/
AT_status_t AT_init(AT_config_t *config) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    AT_HW_API_config_t hw_config;
    // Check parameter.
    if (config->process_callback == NULL) {
        status = AT_ERROR_NULL_PARAMETER;
        goto errors;
    }
    // Init context.
    at_ctx.flags.field.quiet = ((config->default_quiet_flag) == 0) ? 0 : 1;
    at_ctx.flags.field.verbose = ((config->default_verbose_flag) == 0) ? 0 : 1;
    at_ctx.flags.field.echo = ((config->default_echo_flag) == 0) ? 0 : 1;
    at_ctx.process_callback = config->process_callback;
    // Init hardware interface.
    hw_config.rx_irq_callback = &_rx_irq_callback;
    status = AT_HW_API_init(&hw_config);
    if (status != AT_SUCCESS) {
        goto errors;
    }
    // Register basic commands.
    status = AT_register_command(&AT_COMMAND_ECHO);
    if (status != AT_SUCCESS) {
        goto errors;
    }
    status = AT_register_command(&AT_COMMAND_VERBOSE);
    if (status != AT_SUCCESS) {
        goto errors;
    }
    status = AT_register_command(&AT_COMMAND_QUIET);
    if (status != AT_SUCCESS) {
        goto errors;
    }
    return AT_SUCCESS;
errors:
    return status;
}

/*******************************************************************/
AT_status_t AT_de_init(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    // Release hardware interface.
    status = AT_HW_API_de_init();
    if (status != AT_SUCCESS) {
        goto errors;
    }
errors:
    return status;
}

/*******************************************************************/
AT_status_t AT_register_command(const AT_command_t *command) {
    // Local variables.
    AT_status_t status = AT_ERROR_COMMANDS_LIST_FULL;
    uint32_t idx;
    // Check parameter.
    if (command == NULL) {
        status = AT_ERROR_NULL_PARAMETER;
        goto errors;
    }
    // Check write arguments.
    if (((command->write_callback) != NULL) && ((command->write_arguments) == NULL)) {
        status = AT_ERROR_WRITE_CALLBACK_WITHOUT_PARAMETER;
        goto errors;
    }
    // Check type.
    if ((command->type) >= AT_COMMAND_TYPE_LAST) {
        status = AT_ERROR_COMMAND_TYPE;
        goto errors;
    }
    // Check if command is already registered.
    for (idx = 0; idx < (sizeof(at_ctx.commands_list) / sizeof(AT_command_t *)); idx++) {
        // Compare commands.
        if (at_ctx.commands_list[idx] == command) {
            status = AT_ERROR_COMMAND_ALREADY_REGISTERED;
            goto errors;
        }
    }
    // Search free location in list.
    for (idx = 0; idx < (sizeof(at_ctx.commands_list) / sizeof(AT_command_t *)); idx++) {
        // Check free index.
        if (at_ctx.commands_list[idx] == NULL) {
            // Register command and exit.
            at_ctx.commands_list[idx] = command;
            at_ctx.commands_count[command->type]++;
            status = AT_SUCCESS;
            break;
        }
    }
    return AT_SUCCESS;
errors:
    return status;
}

/*******************************************************************/
AT_status_t AT_unregister_command(const AT_command_t *command) {
    // Local variables.
    AT_status_t status = AT_ERROR_COMMAND_NOT_REGISTERED;
    uint32_t idx = 0;
    // Search command in list.
    for (idx = 0; idx < (sizeof(at_ctx.commands_list) / sizeof(AT_command_t *)); idx++) {
        // Check pointer.
        if (at_ctx.commands_list[idx] == command) {
            // Release index and exit.
            at_ctx.commands_list[idx] = NULL;
            at_ctx.commands_count[command->type]--;
            status = AT_SUCCESS;
            break;
        }
    }
    return status;
}

/*******************************************************************/
AT_status_t AT_process(void) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    int32_t command_return_code = 0;
    uint32_t command_start_idx = (sizeof(AT_HEADER) - 1);
    uint32_t idx = 0;
    // Check process flag.
    if (at_ctx.flags.field.process == 0) {
        goto end;
    }
    // Clear flag.
    at_ctx.flags.field.process = 0;
    // Echo.
    if (at_ctx.flags.field.echo != 0) {
        _print_line(at_ctx.rx_buffer);
    }
    if (status != AT_SUCCESS) {
        goto errors;
    }
    // Check header.
    if (memcmp((uint8_t *) at_ctx.rx_buffer, AT_HEADER, command_start_idx) == 0) {
        // Check marker.
        switch (at_ctx.rx_buffer[command_start_idx]) {
        case AT_COMMAND_MARKER_EXECUTION: // Ping commands AT.
            break;
            // Read.
        case AT_COMMAND_MARKER_READ_HELP: // Help commands AT?.
            // Check last character is a execution marker.
            if (at_ctx.rx_buffer[command_start_idx + 1] == AT_COMMAND_MARKER_EXECUTION) {
                status = _print_line("Basic commands");
                if (status != AT_SUCCESS) {
                    goto errors;
                }
                status = _print_help(AT_COMMAND_TYPE_BASIC);
                if (status != AT_SUCCESS) {
                    goto errors;
                }
                status = _print_line("Extended commands");
                if (status != AT_SUCCESS) {
                    goto errors;
                }
                status = _print_help(AT_COMMAND_TYPE_EXTENDED);
                if (status != AT_SUCCESS) {
                    goto errors;
                }
                status = _print_line("Debug commands");
                if (status != AT_SUCCESS) {
                    goto errors;
                }
                status = _print_help(AT_COMMAND_TYPE_DEBUG);
                if (status != AT_SUCCESS) {
                    goto errors;
                }
            } else {
                status = AT_ERROR_INTERNAL_COMMAND_PARSING;
            }
            break;
        case AT_COMMAND_HEADER_EXTENDED: // Extended commands AT$.
            status = _parse_and_execute_command(&at_ctx.rx_buffer[command_start_idx + 1], AT_COMMAND_TYPE_EXTENDED, &command_return_code);
            if (status != AT_SUCCESS) {
                goto errors;
            }
            break;
        case AT_COMMAND_HEADER_DEBUG: // Debug commands AT!.
            status = _parse_and_execute_command(&at_ctx.rx_buffer[command_start_idx + 1], AT_COMMAND_TYPE_DEBUG, &command_return_code);
            if (status != AT_SUCCESS) {
                goto errors;
            }
            break;
        default: // Basic command AT.
            status = _parse_and_execute_command(&at_ctx.rx_buffer[command_start_idx], AT_COMMAND_TYPE_BASIC, &command_return_code);
            if (status != AT_SUCCESS) {
                goto errors;
            }
            break;
        }
    } else {
        status = AT_ERROR_INTERNAL_COMMAND_PARSING;
    }
errors:
    // Print status.
    _print_command_status(status, command_return_code);
    at_ctx.flags.field.pending = 0;
    // Reset buffer.
    for (idx = 0; idx < AT_BUFFER_SIZE; idx++) {
        at_ctx.rx_buffer[idx] = 0x00;
    }
    at_ctx.rx_buffer_size = 0;
end:
    return status;
}

/*******************************************************************/
AT_status_t AT_send_reply(const AT_command_t *command, char *reply) {
    // Local variables.
    AT_status_t status = AT_SUCCESS;
    const AT_command_t *command_ptr = NULL;
    // Check parameters.
    if (reply == NULL) {
        status = AT_ERROR_NULL_PARAMETER;
        goto errors;
    }
    if (reply[0] == '\0') {
        status = AT_ERROR_NULL_PARAMETER;
        goto errors;
    }
    // Update current command pointer.
    command_ptr = (command != NULL) ? command : at_ctx.current_command;
    // Check pointer.
    if (command_ptr != NULL) {
        // Print header.
        status = _print_command_header(command_ptr->type);
        if (status != AT_SUCCESS) {
            goto errors;
        }
        status = _print(command_ptr->syntax);
        if (status != AT_SUCCESS) {
            goto errors;
        }
        status = _print(":");
        if (status != AT_SUCCESS) {
            goto errors;
        }
    }
    status = _print(reply);
    if (status != AT_SUCCESS) {
        goto errors;
    }
    status = _end_line();
    if (status != AT_SUCCESS) {
        goto errors;
    }
    return AT_SUCCESS;
errors:
    return status;
}
