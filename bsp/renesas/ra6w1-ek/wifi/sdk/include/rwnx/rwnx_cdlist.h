/**********************************************************************************************************
* Copyright (c) 2020 - 2026, Renesas Electronics Corporation and/or its affiliates
*
*
* By installing, copying, downloading, accessing, or otherwise using this software
* or any part thereof and the related documentation from Renesas Electronics Corporation
* and/or its affiliates ("Renesas"), You, either individually  or on behalf of an entity
* employing or engaging You, agree to be bound by this Software License Agreement.
* If you do not agree or no longer agree, you are not permitted to use this software or
* related documentation.
*
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
*    list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form, except as embedded into a Renesas
*    integrated circuit in a product or a software update for
*    such product, must reproduce the above copyright notice, this list of
*    conditions and the following disclaimer in the documentation and/or other
*    materials provided with the distribution.
*
* 3. Neither the name of Renesas nor the names of its
*    contributors may be used to endorse or promote products derived from this
*    software without specific prior written permission.
*
* 4. This software, with or without modification, must only be used with a
*    Renesas integrated circuit, or other such integrated circuit permitted by Renesas in writing.
*
* 5. Any software provided in binary form under this license must not be reverse
*    engineered, decompiled, modified and/or disassembled.
*
* THIS SOFTWARE IS PROVIDED BY RENESAS "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL RENESAS OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
* GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********************************************************************************************************/

#ifndef _RWNX_CDLIST_H
#define _RWNX_CDLIST_H

#include "rwnx_types.h"

#define CDLIST_NODE_INIT(x) \
        { .next = &(x), .prev = &(x) }

#define CDLIST_DECLARE(x) \
        struct cdlist_node x = CDLIST_NODE_INIT(x)

static inline void cdlist_init(struct cdlist_node *head)
{
    head->next = head;
    head->prev = head;
}

static inline void cdlist_add_between(struct cdlist_node *new_node,
                                      struct cdlist_node *prev,
                                      struct cdlist_node *next)
{
    next->prev = new_node;
    new_node->next = next;
    new_node->prev = prev;
    prev->next = new_node;
}

static inline void cdlist_add(struct cdlist_node *new_node,
                              struct cdlist_node *head)
{
    cdlist_add_between(new_node, head, head->next);
}

static inline void cdlist_del_between(struct cdlist_node *prev,
                                      struct cdlist_node *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void cdlist_del_entry(struct cdlist_node *node)
{
    cdlist_del_between(node->prev, node->next);

    node->next = NULL;
    node->prev = NULL;
}


#define cdlist_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define cdlist_first_entry(head, type, member) \
    cdlist_entry((head)->next, type, member)

#define cdlist_last_entry(head, type, member) \
    cdlist_entry((head)->prev, type, member)

#define cdlist_prev_entry(pos, member) \
    cdlist_entry((pos)->member.prev, __typeof(*(pos)), member)

#define cdlist_for_each_entry(cur, head, member) \
    for (cur = cdlist_entry((head)->next, __typeof(*cur), member); \
         &cur->member != (head); \
         cur = cdlist_entry(cur->member.next, __typeof(*cur), member))

#define cdlist_for_each_entry_safe(cur, n, head, member) \
    for (cur = cdlist_entry((head)->next, __typeof(*cur), member), \
         n   = cdlist_entry(cur->member.next, __typeof(*cur), member); \
         &cur->member != (head); \
         cur = n, n = cdlist_entry(n->member.next, __typeof(*n), member))

static inline void cdlist_del(struct cdlist_node *node)
{
    cdlist_del_entry(node);
}

static inline void cdlist_del_init(struct cdlist_node *node)
{
    cdlist_del_entry(node);
    cdlist_init(node);
}

static inline int cdlist_empty(const struct cdlist_node *head)
{
    return (head->next == head);
}

static inline void cdlist_add_tail(struct cdlist_node *new_node,
                                   struct cdlist_node *head)
{
    cdlist_add_between(new_node, head->prev, head);
}

#endif /* _RWNX_CDLIST_H */
