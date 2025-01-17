/*!*****************************************************************
 * \file    at_hw_api.h
 * \brief   AT low level interface.
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

#ifndef __AT_HW_API_H__
#define __AT_HW_API_H__

#include "at.h"
#include "stdint.h"

/*** AT HW API structures ***/

/*!******************************************************************
 * \brief AT callback functions.
 * \fn AT_HW_API_rx_irq_cb_t:         Will be called on byte reception interrupt.
 *******************************************************************/
typedef void (*AT_HW_API_rx_irq_cb_t)(uint8_t data);

/*!******************************************************************
 * \struct AT_HW_API_config_t
 * \brief AT hardware interface configuration structure.
 *******************************************************************/
typedef struct {
    AT_HW_API_rx_irq_cb_t rx_irq_callback;
} AT_HW_API_config_t;

/*** AT HW API functions ***/

/*!******************************************************************
 * \fn AT_status_t AT_HW_API_init(AT_HW_API_config_t *hw_api_config)
 * \brief Initialize AT hardware interface.
 * \param[in]   hw_api_config: Pointer to the hardware configuration.
 * \param[out]  none
 * \retval      Function execution status.
 *******************************************************************/
AT_status_t AT_HW_API_init(AT_HW_API_config_t *hw_api_config);

/*!******************************************************************
 * \fn AT_status_t AT_HW_API_de_init(void)
 * \brief Release AT hardware interface.
 * \param[in]   none
 * \param[out]  none
 * \retval      Function execution status.
 *******************************************************************/
AT_status_t AT_HW_API_de_init(void);

/*!******************************************************************
 * \fn AT_status_t AT_HW_API_send(uint8_t *data, uint32_t data_size_bytes)
 * \brief Send data over AT hardware interface.
 * \param[in]   data: Byte array to send.
 * \param[in]   data_size_bytes: Number of bytes to send.
 * \param[out]  none
 * \retval      Function execution status.
 *******************************************************************/
AT_status_t AT_HW_API_write(uint8_t *data, uint32_t data_size_bytes);

#endif /* __AT_HW_API_H__ */
