/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "osal.h"
#include "rm_wifi_event.h"
#include "rm_wifi_api.h"
#include "rm_wifi.h"


/***********************************************************************************************************************
 * Externs
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Enumerations
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Defines
 **********************************************************************************************************************/
#define WIFI_EVENT_THREAD_NAME          "wifi_evt_hdlr"
#define	WIFI_EVENT_THREAD_STACK_SIZE    1024 * OS_STACK_WORD_SIZE
#define WIFI_EVENT_THREAD_PRIORITY      OS_TASK_PRIORITY_NORMAL
#define	WIFI_EVENT_QUEUE_SIZE           64

#define WIFI_EVENT_HANDLER_MAX          eWiFiEventExtMax
#define WIFI_EVENT_BIT(event)           ((1) << (event))
#define WIFI_EVENT_MASK                 (WIFI_EVENT_BIT(eWiFiEventConnected) | \
                                         WIFI_EVENT_BIT(eWiFiEventDisconnected) | \
                                         WIFI_EVENT_BIT(eWiFiEventAPStationDisconnected) | \
                                         WIFI_EVENT_BIT(eWiFiEventExtTWT))
#define WIFI_EVENT_THREAD_DELETE_EV     eWiFiEventExtMax

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef union {
    WIFIEvent_t     evt;
    WIFIEventExt_t  evt_ext;
} rm_wifi_event_t;

/***********************************************************************************************************************
 * Static Globals
 **********************************************************************************************************************/
static rm_wifi_event_handler_t wifi_event_handlers[WIFI_EVENT_HANDLER_MAX] = { NULL };
static OS_QUEUE wifi_event_queue = NULL;
static OS_MUTEX wifi_event_mutex = NULL;
static OS_BASE_TYPE wifi_event_thread_is_closing = pdFALSE;

/***********************************************************************************************************************
 * Global variables
 **********************************************************************************************************************/
OS_TASK wifi_event_thread = NULL;

/***********************************************************************************************************************
 * Function Definitions
 **********************************************************************************************************************/
static void rm_wifi_event_handler_thread(void *params)
{
    FSP_PARAMETER_NOT_USED(params);
    OS_BASE_TYPE status;
    int event_type;
    rm_wifi_event_t *p_wifi_event = NULL;
    rm_wifi_event_handler_t handler = NULL;

    while (1)
    {
        status = OS_QUEUE_GET(wifi_event_queue, &p_wifi_event, OS_QUEUE_FOREVER);

        if (status != OS_QUEUE_OK)
        {
            break;
        }

        event_type = p_wifi_event->evt.xEventType;

        if (event_type == WIFI_EVENT_THREAD_DELETE_EV)
        {
            OS_FREE_FUNC(p_wifi_event);
            break;
        }

        if (event_type < WIFI_EVENT_HANDLER_MAX)
        {
            if (xSemaphoreTake(wifi_event_mutex, OS_MUTEX_FOREVER) == OS_MUTEX_TAKEN)
            {
                handler = wifi_event_handlers[event_type];

                if (xSemaphoreGive(wifi_event_mutex) != pdTRUE)
                {
                    printf("%s:%d Failed to give mutex.\n", __func__, __LINE__);
                }
            }
            else
            {
                printf("%s:%d Failed to take mutex.\n", __func__, __LINE__);
            }

            if (handler)
            {
                handler((WIFIEvent_t *) p_wifi_event);
                handler = NULL;
            }
        }

        OS_FREE_FUNC(p_wifi_event);
    }

    if (wifi_event_queue)
    {
        while (OS_QUEUE_GET(wifi_event_queue, &p_wifi_event, OS_QUEUE_NO_WAIT))
        {
            OS_FREE_FUNC(p_wifi_event);
        }

        OS_QUEUE_DELETE(wifi_event_queue);
        wifi_event_queue = NULL;
    }

    wifi_event_thread = NULL;
    OS_TASK_DELETE(NULL);
}

static rm_wifi_event_t *rm_wifi_event_alloc(int event_type)
{
    rm_wifi_event_t *p_wifi_event = NULL;

    p_wifi_event = OS_MALLOC(sizeof(*p_wifi_event));

    if (p_wifi_event)
    {
        memset(p_wifi_event, 0, sizeof(*p_wifi_event));
        p_wifi_event->evt.xEventType = event_type;
    }
    else
    {
        printf("%s:%d wifi event alloc fail! (event_type %d)\n", __func__, __LINE__, event_type);
    }

    return p_wifi_event;
}

static OS_BASE_TYPE rm_wifi_event_signal(rm_wifi_event_t *p_wifi_event)
{
    OS_BASE_TYPE status;

    OS_ASSERT(p_wifi_event != NULL);

    if (wifi_event_thread_is_closing == pdTRUE || wifi_event_thread == NULL || wifi_event_queue == NULL)
    {
        OS_FREE_FUNC(p_wifi_event);
        return OS_QUEUE_EMPTY;
    }

    status = OS_QUEUE_PUT(wifi_event_queue, &p_wifi_event, OS_QUEUE_NO_WAIT);

    if (status != OS_QUEUE_OK)
    {
        OS_FREE_FUNC(p_wifi_event);
        printf("%s:%d Error: wifi_event_queue full!\n", __func__, __LINE__);
    }

    return status;
}

fsp_err_t rm_wifi_event_register(int event_type, rm_wifi_event_handler_t handler)
{
    if (event_type >= WIFI_EVENT_HANDLER_MAX)
    {
        return FSP_ERR_UNSUPPORTED;
    }

    if (!(WIFI_EVENT_BIT(event_type) & WIFI_EVENT_MASK))
    {
        return FSP_ERR_UNSUPPORTED;
    }

    if (wifi_event_mutex != NULL && wifi_event_thread != NULL)
    {
        if (xSemaphoreTake(wifi_event_mutex, OS_MUTEX_FOREVER) != OS_MUTEX_TAKEN)
        {
            printf("%s:%d Failed to take mutex.\n", __func__, __LINE__);
        }

        wifi_event_handlers[event_type] = handler;

        if (xSemaphoreGive(wifi_event_mutex) != pdTRUE)
        {
            printf("%s:%d Failed to give mutex.\n", __func__, __LINE__);
        }
    }
    else
    {
        wifi_event_handlers[event_type] = handler;
    }

    return FSP_SUCCESS;
}

void rm_wifi_event_handler_init(void)
{
    OS_BASE_TYPE status;

    wifi_event_thread_is_closing = pdFALSE;

    if (wifi_event_queue == NULL)
    {
        OS_QUEUE_CREATE(wifi_event_queue, sizeof(rm_wifi_event_t *), WIFI_EVENT_QUEUE_SIZE);
        OS_ASSERT(wifi_event_queue != NULL);
    }

    if (wifi_event_mutex == NULL)
    {
        wifi_event_mutex = xSemaphoreCreateMutex();
        OS_ASSERT(wifi_event_mutex != NULL);
    }

    if (wifi_event_thread == NULL)
    {
        status = OS_TASK_CREATE(WIFI_EVENT_THREAD_NAME,
                                rm_wifi_event_handler_thread,
                                (void *)NULL,
                                WIFI_EVENT_THREAD_STACK_SIZE,
                                WIFI_EVENT_THREAD_PRIORITY,
                                wifi_event_thread);
        OS_ASSERT(status == OS_TASK_CREATE_SUCCESS);
    }
}

void rm_wifi_event_handler_deinit(void)
{
    OS_BASE_TYPE status;
    
    wifi_event_thread_is_closing = pdTRUE;

    if (wifi_event_thread)
    {
        WIFIEvent_t *p_wifi_event = (WIFIEvent_t *)rm_wifi_event_alloc(WIFI_EVENT_THREAD_DELETE_EV);
        OS_ASSERT(p_wifi_event != NULL);
        
        status = OS_QUEUE_PUT(wifi_event_queue, &p_wifi_event, OS_QUEUE_FOREVER);

        if (status != OS_QUEUE_OK)
        {
            OS_TASK_DELETE(wifi_event_thread);
            wifi_event_thread = NULL;
        }

        while (wifi_event_thread != NULL)
        {
            OS_DELAY(1);
        }

        if (wifi_event_mutex != NULL)
        {
            OS_MUTEX_DELETE(wifi_event_mutex);
            wifi_event_mutex = NULL;
        }
    }
}

void rm_wifi_event_signal_connected(u8 *ssid, u8 ssid_len, u8 *bssid, int security, u8 chan_info)
{
    WIFIEvent_t *evt = (WIFIEvent_t *)rm_wifi_event_alloc(eWiFiEventConnected);
    u8 ssid_copy_len = MIN(ssid_len, wificonfigMAX_SSID_LEN);

    if (!evt)
    {
        return;
    }

    memcpy(evt->xInfo.xConnected.xConnectionInfo.ucSSID, ssid, ssid_copy_len);
    evt->xInfo.xConnected.xConnectionInfo.ucSSIDLength = ssid_copy_len;
    memcpy(evt->xInfo.xConnected.xConnectionInfo.ucBSSID, bssid, wificonfigMAX_BSSID_LEN);
    evt->xInfo.xConnected.xConnectionInfo.xSecurity = security;
    evt->xInfo.xConnected.xConnectionInfo.ucChannel = chan_info;

    rm_wifi_event_signal((rm_wifi_event_t *)evt);
}

void rm_wifi_event_signal_disconnected(u16 reason)
{
    WIFIEvent_t *evt = (WIFIEvent_t *)rm_wifi_event_alloc(eWiFiEventDisconnected);

    if (!evt)
    {
        return;
    }

    evt->xInfo.xDisconnected.xReason = reason;

    rm_wifi_event_signal((rm_wifi_event_t *)evt);
}

void rm_wifi_event_signal_ap_sta_disconnected(u16 reason, u8 *ucMac)
{
    WIFIEvent_t *evt = (WIFIEvent_t *)rm_wifi_event_alloc(eWiFiEventAPStationDisconnected);

    if (!evt)
    {
        return;
    }

    evt->xInfo.xAPStationDisconnected.xReason = reason;
    memcpy(evt->xInfo.xAPStationDisconnected.ucMac, ucMac, wificonfigMAX_BSSID_LEN);
    rm_wifi_event_signal((rm_wifi_event_t *) evt);
}

void rm_wifi_event_signal_twt(WIFI_TWTEvent_t twt_event)
{
    WIFIEventExt_t *evt = (WIFIEventExt_t *)rm_wifi_event_alloc(eWiFiEventExtTWT);

    if (!evt)
    {
        return;
    }

    evt->xInfo.xTWT.xEvent = twt_event;

    rm_wifi_event_signal((rm_wifi_event_t *)evt);
}
