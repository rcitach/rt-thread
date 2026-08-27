#ifndef __PING_H__                                                          
#define __PING_H__                                                          

#include "FreeRTOS.h"
#include "task.h"
                                                                            
/**                                                                         
 * PING_USE_SOCKETS: Set to 1 to use sockets, otherwise the raw api is used 
 */                                                                         
#ifndef PING_USE_SOCKETS                                                    
#define PING_USE_SOCKETS    0	//LWIP_SOCKET                                     
#endif                                                                      

#include <stdbool.h>

#define PING_RECV_EVT_BITS	1

void ping_init(int if_idx);                                                      

void ping_init_for_console(void * param);

uint32_t get_ping_send_cnt(void);
uint32_t get_ping_recv_cnt(void);
uint32_t get_ping_loss_cnt(void);
u8_t get_ping_loss_percent(void);
uint32_t  get_ping_total_time_ms(void);
u32_t get_ping_min_time_ms(void);
u32_t get_ping_max_time_ms(void);
u32_t get_ping_avg_time_ms(void);

#if !PING_USE_SOCKETS                                                      
void ping_send_now(void);                                                  
#endif /* !PING_USE_SOCKETS */                                             
                                                                           
                                                                           
#endif /* __PING_H__ */                                                    
