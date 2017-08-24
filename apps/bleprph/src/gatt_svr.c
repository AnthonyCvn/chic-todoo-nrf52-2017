/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */


#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bsp/bsp.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "bleprph.h"

#include "hal/hal_uart.h"

#define BUFSIZE 100

// Static value for write and read process 
static uint32_t gatt_svr_data_trans;
static uint8_t buffer[BUFSIZE];
static uint8_t data_pos=0;

/*** UUID for custom Bluetooth service and characteristic */ 
// Service  497e3f64-2806-11e7-93ae-92361f002671
static const ble_uuid128_t gatt_svr_todoo_uuid =
        BLE_UUID128_INIT(0x71, 0x26, 0x00, 0x1f, 0x36, 0x92, 0xae, 0x93,
                         0xe7 ,0x11, 0x06, 0x28, 0x64, 0x3f, 0x7e, 0x49);
// Characteristic 497e41e4-2806-11e7-93ae-92361f002671 
static const ble_uuid128_t gatt_svr_chr_todoo_trans_uuid =
        BLE_UUID128_INIT(0x71, 0x26, 0x00, 0x1f, 0x36, 0x92, 0xae, 0x93,
                         0xe7 ,0x11, 0x06, 0x28, 0xe4, 0x41, 0x7e, 0x49);

// Call back function for the GATT custom service to transfer data
static int
gatt_svr_chr_trans_data(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
         //  GATT Service to transfer data (smartphone to device)
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_todoo_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) { {
             //Characteristic write data 
            .uuid = &gatt_svr_chr_todoo_trans_uuid.u,
            .access_cb = gatt_svr_chr_trans_data,
            .flags = BLE_GATT_CHR_F_READ |
                     BLE_GATT_CHR_F_WRITE,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    {
        0,  /* No more services. */
    },
};

static int
gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
                   void *dst, uint16_t *len)
{
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

// Call back function for the custom GATT Service
static int
gatt_svr_chr_trans_data(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg)
{
    const ble_uuid_t *uuid;
    int rc;

    uuid = ctxt->chr->uuid;

    /* Determine which characteristic is being accessed by examining its
     * 128-bit UUID.
     */

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_todoo_trans_uuid.u) == 0) {
        switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR :
            rc = os_mbuf_append(ctxt->om, &gatt_svr_data_trans,
                                sizeof gatt_svr_data_trans);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        case BLE_GATT_ACCESS_OP_WRITE_CHR :
            rc = gatt_svr_chr_write(ctxt->om,
                                    sizeof gatt_svr_data_trans,
                                    sizeof gatt_svr_data_trans,
                                    &gatt_svr_data_trans, NULL);
            /* Save data in buffer */
            buffer[data_pos]=gatt_svr_data_trans;
            /* write to UART with blocking */
            hal_uart_blocking_tx(0, buffer[data_pos]);
            ++data_pos;
            if (data_pos==BUFSIZE)
            {
                data_pos=0;
            }
            return rc;

        default:
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
        }
    }

    /* Unknown characteristic; the nimble stack should not have called this
     * function.
     */
          
    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

void
gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        BLEPRPH_LOG(DEBUG, "registered service %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        BLEPRPH_LOG(DEBUG, "registering characteristic %s with "
                           "def_handle=%d val_handle=%d\n",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        BLEPRPH_LOG(DEBUG, "registering descriptor %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

int
gatt_svr_init(void)
{
    int rc;

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}