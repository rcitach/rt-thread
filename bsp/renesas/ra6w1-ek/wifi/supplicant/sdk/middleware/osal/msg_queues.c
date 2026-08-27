/**
 ****************************************************************************************
 *
 * @file msg_queues.c
 *
 * @brief Message queue API
 *
 * Copyright (c) 2023-2024 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */

#if !defined(OS_BAREMETAL)

#include <stdbool.h>
#include <string.h>
#include <sdk_defs.h>
#include <msg_queues.h>


#if CONFIG_MSG_QUEUE_USE_ALLOCATORS
const content_allocator default_os_allocator = {
        .content_alloc = (MSG_ALLOC) MSG_QUEUE_MALLOC,
        .content_free = (MSG_FREE) MSG_QUEUE_FREE
};

#define QUEUE_ALLOC(queue, size) queue->allocator->content_alloc(size)
#define QUEUE_DEALLOCATOR(queue) queue->allocator->content_free

#else

#define QUEUE_ALLOC(queue, size) MSG_QUEUE_MALLOC(size)
#define QUEUE_DEALLOCATOR(queue) MSG_QUEUE_FREE

#endif

void msg_queue_create(msg_queue *queue, int queue_size, content_allocator *allocator)
{
    (void) allocator;

        OS_QUEUE_CREATE(queue->queue, sizeof(msg), (UBaseType_t) queue_size);
#if CONFIG_MSG_QUEUE_USE_ALLOCATORS
        queue->allocator = allocator;
#endif
}

void msg_queue_delete(msg_queue *queue)
{
        OS_QUEUE_DELETE(queue->queue);
}

int msg_queue_put(msg_queue *queue, msg *message, OS_TICK_TIME timeout)
{
        int retval;

        if (in_interrupt()) {
                retval = OS_QUEUE_PUT_FROM_ISR(queue->queue, message);
        } else {
                retval = OS_QUEUE_PUT(queue->queue, message, timeout);
        }

        return retval;
}

int msg_queue_get(msg_queue *queue, msg *message, OS_TICK_TIME timeout)
{
        return OS_QUEUE_GET(queue->queue, message, timeout);
}

void msg_init(msg *message, MSG_ID id, MSG_TYPE type, void *buf, MSG_SIZE size, MSG_FREE free_cb)
{
        message->id = id;
        message->type = type;
        message->data = buf;
        message->size = size;
        message->free_cb = free_cb;
}

void msg_release(msg *message)
{
        if (message->free_cb) {
                message->free_cb(message->data);
                message->free_cb = NULL;
        }
}

int msg_queue_init_msg(msg_queue *queue, msg *message, MSG_ID id, MSG_TYPE type, MSG_SIZE size)
{
#if !CONFIG_MSG_QUEUE_USE_ALLOCATORS
        (void) queue;
#endif
        uint8_t *buf;

        buf = QUEUE_ALLOC(queue, size);
        if (buf == NULL) {
                return 0;
        }
        msg_init(message, id, type, buf, size, QUEUE_DEALLOCATOR(queue));

        return 1;
}

int msg_queue_send(msg_queue *queue, MSG_ID id, MSG_TYPE type, void *buf, MSG_SIZE size,
                                                                        OS_TICK_TIME timeout)
{
        msg message;

        if (msg_queue_init_msg(queue, &message, id, type, size) == 0) {
                return OS_QUEUE_FULL;
        }

        memcpy(message.data, buf, size);
        if (msg_queue_put(queue, &message, timeout) == OS_QUEUE_FULL) {
                msg_release(&message);
                return OS_QUEUE_FULL;
        }

        return OS_QUEUE_OK;
}

int msq_queue_send_zero_copy(msg_queue *queue, MSG_ID id, MSG_TYPE type, void *buf,
                                        MSG_SIZE size, OS_TICK_TIME timeout, MSG_FREE free_cb)
{
        msg message;

        msg_init(&message, id, type, buf, size, free_cb);

        if (msg_queue_put(queue, &message, timeout) == OS_QUEUE_FULL) {
                msg_release(&message);
                return OS_QUEUE_FULL;
        }

        return OS_QUEUE_OK;
}

#endif /* !defined(OS_BAREMETAL) */
