/*!*****************************************************************
 * \file    at.h
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

#ifndef __AT_H__
#define __AT_H__

#include "stdint.h"

/*** AT structures ***/

/*!******************************************************************
 * \enum AT_status_t
 * \brief AT driver error codes.
 *******************************************************************/
typedef enum {
    AT_SUCCESS = 0,
    // Internal command errors (printed on terminal).
    AT_ERROR_INTERNAL_COMMAND_PARSING,               /*! Parsing of the command failed. */
    AT_ERROR_INTERNAL_COMMAND_NOT_FOUND,             /*! Command not found. */
    AT_ERROR_INTERNAL_COMMAND_MARKER_NOT_DEFINED,    /*! Command marker is not defined. (not '$' or '!'). */
    AT_ERROR_INTERNAL_COMMAND_EXECUTION_NOT_DEFINED, /*! Execution command is null. */
    AT_ERROR_INTERNAL_COMMAND_WRITE_NOT_DEFINED,     /*! Write command is null. */
    AT_ERROR_INTERNAL_COMMAND_READ_NOT_DEFINED,      /*! Read command is null. */
    // External command errors (printed on terminal, only used in user command execution, read or write callbacks).
    AT_ERROR_EXTERNAL_COMMAND_BAD_PARAMETER_NUMBER,  /*! Number of parameters is incorrect. \param error_code is the expected number of parameters. */
    AT_ERROR_EXTERNAL_COMMAND_BAD_PARAMETER_PARSING, /*! Parsing of one parameter failed. \param error_code is the bad parameter position. */
    AT_ERROR_EXTERNAL_COMMAND_BAD_PARAMETER_VALUE,   /*! Value of one parameter is incorrect. \param error_code is the bad parameter position. */
    AT_ERROR_EXTERNAL_COMMAND_CORE_ERROR,            /*! The command execution failed. \param errors_code. is code execution error. */
    // Driver errors (not printed on terminal).
    AT_ERROR_NULL_PARAMETER,
    AT_ERROR_WRITE_CALLBACK_WITHOUT_PARAMETER,
    AT_ERROR_COMMAND_TYPE,
    AT_ERROR_COMMAND_ALREADY_REGISTERED,
    AT_ERROR_COMMANDS_LIST_FULL,
    AT_ERROR_COMMAND_NOT_REGISTERED,
    AT_ERROR_TX_BUFFER_SIZE,
    AT_ERROR_AT_HW_API,
    // Last index.
    AT_ERROR_LAST
} AT_status_t;

/*!******************************************************************
 * \enum AT_command_type_t
 * \brief AT command types list.
 *******************************************************************/
typedef enum {
    AT_COMMAND_TYPE_BASIC = 0,
    AT_COMMAND_TYPE_EXTENDED,
    AT_COMMAND_TYPE_DEBUG,
    AT_COMMAND_TYPE_LAST
} AT_command_type_t;

/*!******************************************************************
 * \brief AT callback functions.
 * \fn AT_process_cb_t:               Will be called each time a low level IRQ is handled by the hardware interface.
 * \fn AT_command_execution_cb_t:     AT command execution callback.
 * \fn AT_command_read_cb_t:          AT command read callback.
 * \fn AT_command_write_cb_t          AT command write callback.
 *******************************************************************/
typedef void (*AT_process_cb_t)(void);
typedef AT_status_t (*AT_command_execution_cb_t)(int32_t *error_code);
typedef AT_status_t (*AT_command_read_cb_t)(int32_t *error_code);
typedef AT_status_t (*AT_command_write_cb_t)(uint32_t argc, char *argv[], int32_t *error_code);
typedef const char *(*AT_command_error_enum_to_str_cb_t)(unsigned int error_code);

/*!******************************************************************
 * \struct AT_command_t
 * \brief AT command definition structure.
 *******************************************************************/
typedef struct {
    uint8_t default_quiet_flag;
    uint8_t default_verbose_flag;
    uint8_t default_echo_flag;
    AT_process_cb_t process_callback;
} AT_config_t;

/*!******************************************************************
 * \struct AT_command_t
 * \brief AT command definition structure.
 *******************************************************************/
typedef struct {
    const char *syntax;
    AT_command_type_t type;
    const char *help;
    AT_command_execution_cb_t execution_callback;
    const char *execution_help;
    AT_command_read_cb_t read_callback;
    const char *read_help;
    AT_command_write_cb_t write_callback;
    const char *write_arguments;
    const char *write_help;
    AT_command_error_enum_to_str_cb_t enum_to_str_callback;
} AT_command_t;

/*** AT functions ***/

/*!******************************************************************
 * \fn AT_status_t AT_init(AT_config_t *config)
 * \brief Initialize AT command manager.
 * \param[in]   config: Pointer to the configuration structure.
 * \param[out]  none
 * \retval      Function execution status.
 *******************************************************************/
AT_status_t AT_init(AT_config_t *config);

/*!******************************************************************
 * \fn AT_status_t AT_de_init(void)
 * \brief Release AT command manager.
 * \param[in]   none
 * \param[out]  none
 * \retval      Function execution status.
 *******************************************************************/
AT_status_t AT_de_init(void);

/*!******************************************************************
 * \fn AT_status_t AT_register_command(const AT_command_t *command)
 * \brief Register an AT command.
 * \param[in]   command: Pointer to the command to register.
 * \param[out]  none
 * \retval      Function execution status.
 *******************************************************************/
AT_status_t AT_register_command(const AT_command_t *command);

/*!******************************************************************
 * \fn AT_status_t AT_unregister_command(const AT_command_t *command)
 * \brief Unregister an AT command.
 * \param[in]   command: Pointer to the command to unregister.
 * \param[out]  none
 * \retval      Function execution status.
 *******************************************************************/
AT_status_t AT_unregister_command(const AT_command_t *command);

/*!******************************************************************
 * \fn AT_status_t AT_process(void)
 * \brief Process AT command driver.
 * \param[in]   none
 * \param[out]  none
 * \retval      Function execution status.
 *******************************************************************/
AT_status_t AT_process(void);

/*!******************************************************************
 * \fn AT_status_t AT_send_reply(const AT_command_t *command, char *reply)
 * \brief Send a reply over USART interface.
 * \param[in]   command: Pointer to the command to send a reply. If NULL, the actual internal command will be used for the reply.
 * \param[in]   reply: String to send.
 * \param[out]  none
 * \retval      Function execution status.
 *******************************************************************/
AT_status_t AT_send_reply(const AT_command_t *command, char *reply);

/*!******************************************************************
 * \fn void AT_check_status(error)
 * \brief Generic macro to check a MCAL function status and exit.
 * \param[in]   error: High level error code to rise.
 * \param[out]  none
 * \retval      none
 *******************************************************************/
#define AT_check_status(error) { if (at_status != AT_SUCCESS) { status = error; goto errors; } }

/*!******************************************************************
 * \fn AT_command_exit_param_number_error(expected_param_number)
 * \brief Generic macro to exit from a command execution with a parameter number error.
 * \param[in]   expected_param_number: Expected number of parameters.
 * \param[out]  none
 * \retval      none
 *******************************************************************/
#define AT_command_exit_param_number_error(expected_param_number) { *error_code = expected_param_number; status = AT_ERROR_EXTERNAL_COMMAND_BAD_PARAMETER_NUMBER; goto errors; }

/*!******************************************************************
 * \fn AT_command_exit_param_parsing_error(param_position)
 * \brief Generic macro to exit from a command execution with a parameter parsing error.
 * \param[in]   param_position: Position of the parameter.
 * \param[out]  none
 * \retval      none
 *******************************************************************/
#define AT_command_exit_param_parsing_error(param_position) { *error_code = param_position; status = AT_ERROR_EXTERNAL_COMMAND_BAD_PARAMETER_PARSING; goto errors; }

/*!******************************************************************
 * \fn AT_command_exit_param_value_error(param_position)
 * \brief Generic macro to exit from a command execution with a parameter value error.
 * \param[in]   param_position: Position of the parameter.
 * \param[out]  none
 * \retval      none
 *******************************************************************/
#define AT_command_exit_param_value_error(param_position) { *error_code = param_position; status = AT_ERROR_EXTERNAL_COMMAND_BAD_PARAMETER_VALUE; goto errors; }

/*!******************************************************************
 * \fn AT_command_exit_core_error(code)
 * \brief Generic macro to exit from a command execution with a core error.
 * \param[in]   code: Error code.
 * \param[out]  none
 * \retval      none
 *******************************************************************/
#define AT_command_exit_core_error(code) { *error_code = code; status = AT_ERROR_EXTERNAL_COMMAND_CORE_ERROR; goto errors; }

/*!******************************************************************
 * \fn AT_command_check_and_exit_param_number_error(param_number)
 * \brief Generic macro to check and exit from a command execution with a parameter number error.
 * \param[in]   param_number: Expected number of parameters.
 * \param[out]  none
 * \retval      none
 *******************************************************************/
#define AT_command_check_and_exit_param_number_error(param_number) { if (argc!= param_number) { AT_command_exit_param_number_error(param_number); } }

/*!******************************************************************
 * \fn AT_command_check_and_exit_param_parser_error(check, param_position)
 * \brief Generic macro to check and exit from a command execution with a parameter parsing error.
 * \param[in]   check: Check to perform.
 * \param[in]   param_position: Position of the parameter.
 * \param[out]  none
 * \retval      none
 *******************************************************************/
#define AT_command_check_and_exit_param_parser_error(check, param_position) { if (check) { AT_command_exit_param_parsing_error(param_position); } }

/*!******************************************************************
 * \fn AT_command_check_and_exit_param_value_error(check, param_position)
 * \brief Generic macro to check and exit from a command execution with a parameter value error.
 * \param[in]   check: Check to perform.
 * \param[in]   param_position: Position of the parameter.
 * \param[out]  none
 * \retval      none
 *******************************************************************/
#define AT_command_check_and_exit_param_value_error(check, param_position) { if (check) { AT_command_exit_param_value_error(param_position); } }

#endif /* __AT_H__ */
