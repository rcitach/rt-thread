/**
 ****************************************************************************************
 *
 * @file net_dns_client.c
 *
 * @brief DNS Client module
 *
 * Copyright (c) 2023 Renesas Electronics. All rights reserved.
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
#include "bsp_api.h"

#if CFG_WIFI /* Compile only with WiFi stack */

#include "net_dns_client.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "util_api.h"
#include "ip_addr.h"
#include "rm_lwip_w_helper.h"
#include "rm_vee_flash_w_rrq_nvram.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif

#if LWIP_DNS
// Local variables

#if defined ( __DNS_2ND_CACHE_SUPPORT__ )
static domain_to_ip_addr_t	_url_ipaddr_table = { 0 };
static domain_to_ip_addr_t	*domain_ipaddr_table_p = (domain_to_ip_addr_t *)&_url_ipaddr_table;

static char	dns_2st_cache_loading_flag = FALSE;
static char	ipaddr_str_temp[MAX_IPADDR_LEN];
#endif

// External global variables

#define CACHE_EXPIRETIME    (3600*24) // Sec.

static int set_iface_dns_addr(int iface, bool Primary_DNS, char *ip_addr)
{
	UINT status = pdFAIL;
#ifdef __SUPPORT_IPV4__
	ip4_addr_t ipaddr;

	ipaddr.addr = ipaddr_addr(ip_addr);

	if (ipaddr.addr == IPADDR_NONE) {
		// converting fails ...
		status = pdFAIL;
	} else {
		// apply to dns immediately
		dns_setserver(Primary_DNS ? 0:2, (const ip_addr_t *)&ipaddr);

#ifdef RM_MAP_PERSISTANT_W
		// save to nvram
		if (Primary_DNS) {
			RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, iface == 0 ? WIFI_PROFILE_DNSSVR_0 : WIFI_PROFILE_DNSSVR_1, ip_addr);
		} else {
			// Secondary DNS
			RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, iface == 0 ? WIFI_PROFILE_DNSSVR_2ND_0 : WIFI_PROFILE_DNSSVR_2ND_1, ip_addr);
		}
#endif /* SET DNS */

		status = pdPASS;
	}
#endif // __SUPPORT_IPV4__
	return status;
}

int get_dns_addr(int iface, char *result_str)
{
	printf("=== [%s] Need to FreeRTOS code for this fucntion ...\n", __func__);

	RA6W1_UNUSED_ARG(iface);
	RA6W1_UNUSED_ARG(result_str);

	return pdPASS;
}

int get_dns_addr_2nd(int iface_flag, char *result_str)
{
	printf("=== [%s] Need to write FreeRTOS code for this function ...\n", __func__);

	RA6W1_UNUSED_ARG(iface_flag);
	RA6W1_UNUSED_ARG(result_str);

	return pdTRUE;
}

int set_dns_addr(int iface, char *ip_addr)
{
	return set_iface_dns_addr(iface, true, ip_addr);
}

int set_dns_addr_2nd(int iface, char *ip_addr)
{
	return set_iface_dns_addr(iface, false, ip_addr);
}

void dns_req_resolved(const char* domain, const ip_addr_t *ipaddr, void *arg)
{
	dns_req_ctx_t *ctx = (dns_req_ctx_t *)arg;
	if (!ctx)
	{
		return;
	}

#if !defined ( __DNS_2ND_CACHE_SUPPORT__ )
	RA6W1_UNUSED_ARG(domain);
#endif /* __DNS_2ND_CACHE_SUPPORT__ */

#if defined ( __DNS_2ND_CACHE_SUPPORT__ )
	char tmp_addr[IP6ADDR_LEN];
#endif /* __DNS_2ND_CACHE_SUPPORT__ */

#if defined (__SUPPORT_IPV6__) && defined (__SUPPORT_IPV4__)
	if (ip_addr_isany_val(*ipaddr))
#elif defined (__SUPPORT_IPV4__)
	if (ip4_addr_isany_val(*ip_2_ip4(ipaddr)))
#elif defined (__SUPPORT_IPV6__)
	if (ip6_addr_isany_val(*ip_2_ip6(ipaddr)))
#endif
	{
		xSemaphoreGive(ctx->sem);
		printf("[%s] Failed to query DNS. Invalid domain URL...\n", __func__);
	} else
	{
		*(ctx->out) = *ipaddr;

#if defined ( __DNS_2ND_CACHE_SUPPORT__ )
		ipaddr_ntoa_r(ipaddr, tmp_addr, IP6ADDR_LEN);
		ra6w1_dns_2nd_cache_add((const char*)domain, tmp_addr, 0);
#endif
		xSemaphoreGive(ctx->sem);
	}
}

bool dns_A_Query(char *domain_name, char *ipaddr_str, unsigned long wait_option)
{
	ip_addr_t dns_ipaddr, result;
	dns_req_ctx_t ctx = {0};
	bool resolved = pdFALSE;

	ctx.out = &result;
	ctx.sem = xSemaphoreCreateBinary();

	if (!ctx.sem)
	{
		return pdFALSE;
	}
	xSemaphoreTake(ctx.sem, 0);

	memset(ipaddr_str, 0, IPADDR_LEN);
	err_t ret = dns_gethostbyname_addrtype(domain_name, &dns_ipaddr, dns_req_resolved, &ctx , LWIP_DNS_ADDRTYPE_IPV4);

	if (ERR_OK == ret)
	{
#if defined (__SUPPORT_IPV6__) && defined (__SUPPORT_IPV4__)
		if (ip_addr_isany_val(dns_ipaddr))
		{
#elif defined (__SUPPORT_IPV4__)
		if (ip4_addr_isany_val(*ip_2_ip4(&dns_ipaddr)))
		{
#endif
			printf("[%s] Failed to query DNS. Invalid domain URL...\n", __func__);
			resolved = pdFALSE;
			goto End;
		} else
		{
			ipaddr_ntoa_r(&dns_ipaddr, ipaddr_str, IPADDR_LEN);
			resolved = pdTRUE;
			goto End;
		}
	} else if (ERR_INPROGRESS == ret)
	{
		if (xSemaphoreTake(ctx.sem, portCONVERT_MS_2_TICKS(wait_option)) != pdTRUE)
		{
			resolved = pdFALSE;
			goto End;
		}

		ipaddr_ntoa_r(&result, ipaddr_str, IPADDR_LEN);
		resolved = pdTRUE;
		goto End;
	} else
	{
		printf("> DNS query fail (%s), ret=%d\n", domain_name, ret);
		resolved = pdFALSE;
		goto End;
	}

End:
	vSemaphoreDelete(ctx.sem);
	return resolved;
}

bool dns_AAAA_Query(char *domain_name, char *ipaddr_str, unsigned long wait_option)
{
	ip_addr_t dns_ipaddr, result;
	dns_req_ctx_t ctx = {0};
	bool resolved = pdFALSE;

	ctx.out = &result;
	ctx.sem = xSemaphoreCreateBinary();

	if (!ctx.sem)
	{
		return pdFALSE;
	}
	xSemaphoreTake(ctx.sem, 0);

	memset(ipaddr_str, 0, IP6ADDR_LEN);
	err_t ret = dns_gethostbyname_addrtype(domain_name, &dns_ipaddr, dns_req_resolved, &ctx, LWIP_DNS_ADDRTYPE_IPV6);

	if (ERR_OK == ret)
	{
#if defined (__SUPPORT_IPV6__) && defined (__SUPPORT_IPV4__)
		if (ip_addr_isany_val(dns_ipaddr))
		{
#elif defined (__SUPPORT_IPV6__)
		if (ip6_addr_isany_val(*ip_2_ip6(&dns_ipaddr)))
		{
#endif
			printf("[%s] Failed to query DNS. Invalid domain URL...\n", __func__);
			resolved = pdFALSE;
			goto End;
		} else
		{
			ipaddr_ntoa_r(&dns_ipaddr, ipaddr_str, IP6ADDR_LEN);
			resolved = pdTRUE;
			goto End;
		}
	} else if (ERR_INPROGRESS == ret)
	{
		if (xSemaphoreTake(ctx.sem, portCONVERT_MS_2_TICKS(wait_option)) != pdTRUE)
		{
			resolved = pdFALSE;
			goto End;  // time out
		}

		ipaddr_ntoa_r(&result, ipaddr_str, IP6ADDR_LEN);
		resolved = pdTRUE;
		goto End;
	} else
	{
		printf("> DNS query fail (%s), ret=%d\n", domain_name, ret);
		resolved = pdFALSE;
		goto End;
	}

End:
	vSemaphoreDelete(ctx.sem);
	return resolved;
}

unsigned int dns_ALL_Query(unsigned char *domain_name,
						unsigned char *record_buffer,
						unsigned int record_buffer_size,
						unsigned int *record_count,
						unsigned long wait_option)	/* GET_MULTI_IP */
{
	RA6W1_UNUSED_ARG(wait_option);
	
#ifdef __SUPPORT_IPV4__
	int status;
	UINT rec_cnt = 0;
	UINT32* rec_buf = NULL;
	struct addrinfo hints; 
	struct addrinfo *addr_list, *cur;

	memset( &hints, 0, sizeof( hints ) );
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if (record_buffer == NULL || record_count == NULL) {
   		printf("[%s] record_buffer(NULL) / record_count(NULL) \n", __func__);
        return 1; // error
	}

    if((status = getaddrinfo((const char *)domain_name, NULL, &hints, &addr_list)) != 0) {
   		printf("[%s] DNS query error(0x%0x). [%s], see netdb.h\n",
			   __func__, (int)status, domain_name);
        return 1; // error
    }

	rec_buf = (UINT32*)record_buffer;
	
    for (cur = addr_list; (cur != NULL) && (rec_cnt <= record_buffer_size); cur = cur->ai_next) {
       *rec_buf = (UINT32)(((struct sockaddr_in*)(cur->ai_addr))->sin_addr.s_addr);
	   rec_cnt++;
	   rec_buf++;
    }
	
	*record_count = rec_cnt;
	
    freeaddrinfo( addr_list );
#endif // __SUPPORT_IPV4__
	return 0;

}


#if defined ( __DNS_2ND_CACHE_SUPPORT__ )
static bool user_sflash_is_addr_size_valid(unsigned int sflash_addr, int chk_for_rw, int rw_size)
{
    unsigned int sflash_user_area_start = 0;
    unsigned int sflash_user_area_end = 0;

    sflash_user_area_start = SF_USER_AREA;
    sflash_user_area_end = SF_USER_AREA + SF_USER_AREA_SIZE;

    /* Check address range */
    if ((sflash_addr < sflash_user_area_start) || (sflash_addr >= sflash_user_area_end)) {
        printf("- SFLASH read error : Wrong address offset (0x%x ~ 0x%x) !!!\n",
                        sflash_user_area_start, sflash_user_area_end);
        return  pdFALSE;
    }

    if (chk_for_rw == pdTRUE) {
        if ((sflash_addr + rw_size) > sflash_user_area_end) {
            printf("- SFLASH address : Wrong size (0x%x ~ 0x%x) !!!\n",
                      sflash_user_area_start, sflash_user_area_end);
            return  pdFALSE;
        }
    }

    return pdTRUE;
}

bool user_sflash_read(int sflash_addr, char *rd_buf, int rd_size)
{
    if (user_sflash_is_addr_size_valid(sflash_addr, pdTRUE, rd_size) == pdFALSE) {
        return pdFALSE;
    }
    return util_sflash_read(sflash_addr, rd_buf, rd_size);
}

bool user_sflash_write(int sflash_addr, char *wr_buf, int wr_size)
{
   if (user_sflash_is_addr_size_valid(sflash_addr, pdTRUE, wr_size) == pdFALSE) {
        return pdFALSE;
   }
   return util_sflash_write(sflash_addr, wr_buf, wr_size);
}

bool user_sflash_erase(int sflash_addr, int er_size)
{
    if (user_sflash_is_addr_size_valid(sflash_addr, pdFALSE, 0) == pdFALSE) {
        return pdFALSE;
    }
    return util_sflash_erase(sflash_addr, er_size);
}

bool user_sflash_copy(int dest_addr, int src_addr, int cp_size)
{
    if (user_sflash_is_addr_size_valid(dest_addr, pdFALSE, cp_size) == pdFALSE) {
        return pdFALSE;
    }
    return util_sflash_copy(dest_addr, src_addr, cp_size);
}
// update sflash()
static int _ra6w1_dns_2nd_cache_update_sflash(void) 
{
	return user_sflash_write(SF_USER_AREA, (char *)domain_ipaddr_table_p, sizeof(domain_to_ip_addr_t));
}

// idx find_empty_slot() : find empty slot
static int _ra6w1_dns_2nd_cache_find_empty_slot(void) 
{
	int empty_idx = -1;
	int i;

	for (i = 0; i < MAX_URL_TABLE_CNT; i++) {
		if (domain_ipaddr_table_p->table[i].ipaddr_str[0] == 0 ||
		    (isvalidip(domain_ipaddr_table_p->table[i].ipaddr_str) == pdFALSE)) {
			empty_idx = i;
			break;			
		}
	}
	
	return empty_idx;
}


static int _ra6w1_dns_2nd_cache_find_oldest_slot(void) 
{
	int idx;
    int oldest_idx = -1;

    unsigned long long current_time;
    int time_remaining;

    ra6w1_time64(NULL, &current_time); 
  
	for (idx = 0; idx < MAX_URL_TABLE_CNT; idx++) {
    	if (idx == 0) {
        	oldest_idx = idx;
        	continue;
    	}

        if (domain_ipaddr_table_p->table[idx].last_time + CACHE_EXPIRETIME < current_time) {
            time_remaining = 0;
        } else {
            time_remaining = (int)((domain_ipaddr_table_p->table[idx].last_time + CACHE_EXPIRETIME) - current_time);
        }
    	
		if ((int)((domain_ipaddr_table_p->table[idx-1].last_time + CACHE_EXPIRETIME) - current_time) > time_remaining) {
    	    oldest_idx = idx;
		}
	}
	
	return oldest_idx;
}


#ifdef __DNS_2ND_CACHE_INFO__
// res delete_row (idx) : delete row (set ip_addr=0) (+sflash update)
static int _ra6w1_dns_2nd_cache_delete(int idx) 
{
	int status;
	char* ip_str = domain_ipaddr_table_p->table[idx].ipaddr_str;
	char* domain_str = domain_ipaddr_table_p->table[idx].domain_str;

	if (domain_ipaddr_table_p->table[idx].ipaddr_str[0] != 0) {
		ip_str[0] = 0;			
		domain_str[0] = 0;
	} else {
		return pdFALSE;
	}

	status = _ra6w1_dns_2nd_cache_update_sflash();
	if (status == pdFALSE) {
		printf("[%s] SFLASH write error...\n", __func__);
		// Go ahead next operation ...
	}

	return pdTRUE;
}

// init_table() : memset(table,0,..)
static int _ra6w1_dns_2nd_cache_init(void) 
{
	int status;
	
	if (dns_2st_cache_loading_flag == TRUE) {
	    return TRUE;
    }

    memset(domain_ipaddr_table_p, 0, sizeof(domain_to_ip_addr_t));

	status = user_sflash_read(SF_USER_AREA, (char*)domain_ipaddr_table_p, sizeof(domain_to_ip_addr_t));
								
	dns_2st_cache_loading_flag = TRUE;
	return status;
}
#endif /* __DNS_2ND_CACHE_INFO__ */

// idx find_row(str) : search and return idx
static int _ra6w1_dns_2nd_cache_find(const char* domain) 
{
	int idx_found = -1;
	int i;

	for (i = 0; i < MAX_URL_TABLE_CNT; i++) {
		if (strcmp(domain_ipaddr_table_p->table[i].domain_str, domain) == 0
			&& domain_ipaddr_table_p->table[i].ipaddr_str[0] != 0
			&& (isvalidip(domain_ipaddr_table_p->table[i].ipaddr_str) == pdTRUE)) {
			idx_found = i;
			break;			
		}
	}
	
	return idx_found;
}


#ifdef __DNS_2ND_CACHE_INFO__
static void _ra6w1_dns_2nd_cache_show_table(void)
{	
	unsigned char idx, used_cnt = 0;
    unsigned long long current_time;
    int time_remaining, elapsed_time;

    ra6w1_time64(NULL, &current_time); 

	for (idx = 0; idx < MAX_URL_TABLE_CNT; idx++) {
		if ((domain_ipaddr_table_p->table[idx].ipaddr_str[0] != 0)
		    && (isvalidip(domain_ipaddr_table_p->table[idx].ipaddr_str) == pdTRUE)) {
    		if (used_cnt == 0) {
    		    printf("IDX  %19s %16s  %s\n", "TTL", "IP Address", "Domain");
    		    print_separate_bar('-', 80, 1);
    		}

            if (domain_ipaddr_table_p->table[idx].last_time + CACHE_EXPIRETIME < current_time) {
                time_remaining = 0;
                elapsed_time   = 0;
            } else {
                time_remaining = (int)((domain_ipaddr_table_p->table[idx].last_time + CACHE_EXPIRETIME) - current_time);
                elapsed_time   = (int)(current_time - domain_ipaddr_table_p->table[idx].last_time);
            }

            printf("[%02d] %6d(%5d/%5d) %16s  %s\n",
                idx,                                            // index
                time_remaining,                                 // TTL
                elapsed_time,
                CACHE_EXPIRETIME,
                domain_ipaddr_table_p->table[idx].ipaddr_str,   // IP Address
                domain_ipaddr_table_p->table[idx].domain_str);  // Domain
			used_cnt++;
		}
	}

    if (used_cnt > 0) {
        print_separate_bar('-', 80, 1);
        printf("Used: %02d/%02d\n", used_cnt,  MAX_URL_TABLE_CNT);
    } else {
    	printf("<empty>\n");
	}
}

static void _ra6w1_dns_2nd_cache_modify_row(int idx, char* ip_addr_str, uint64_t ttl)
{	
	if (domain_ipaddr_table_p->table[idx].domain_str[0] != 0) {
		strcpy(domain_ipaddr_table_p->table[idx].ipaddr_str, ip_addr_str);

        if (ttl != 0) {
            domain_ipaddr_table_p->table[idx].last_time = ttl;
		} else {
            ra6w1_time64(NULL, &domain_ipaddr_table_p->table[idx].last_time);
		}
		
		_ra6w1_dns_2nd_cache_update_sflash();
	} else {
		printf("Failed to modify IP. It doesn't exist in table.\n");
	}
}
#endif /* __DNS_2ND_CACHE_INFO__ */

#if defined ( __DNS_2ND_CACHE_INFO__ )
bool cmd_dns2cache(int argc, char *argv[])
{
	bool result = pdFALSE;
	char ip[IPADDR_LEN] = {0,};

	if (argc < 2 || argc > 4) 
		printf("dns2cache [option] [option2] [option3]\n"
                "  option: [status]\n"
                "          [query] <domain>\n"
                "          [edit] <index> <ipaddr> <ttl>\n"
                "          [erase] [<index>|all]\n");

	if (argc == 2) {
		_ra6w1_dns_2nd_cache_init();
		if (strcmp(argv[1], "status") == 0) {
			_ra6w1_dns_2nd_cache_show_table();
		}
		return pdTRUE;
	} else if (argc == 3 || (argc == 4 && strcmp(argv[1], "erase") == 0)) {
		if (strcmp(argv[1], "query") == 0) {
			printf("domain: %s \n", argv[2]);
			result = dns_A_Query(argv[2], ip, 4000);
			if (result == pdTRUE)
			{
				printf("IP addr: %s\n", ip);
			}
		} else if ((strcmp(argv[1], "erase") == 0)) {
		    if (strcmp(argv[2], "all") == 0) {
                memset(domain_ipaddr_table_p, 0, sizeof(domain_to_ip_addr_t));
		        ra6w1_dns_2nd_cache_erase_sflash();
		    } else {
    			if (_ra6w1_dns_2nd_cache_delete(atoi(argv[2])) == pdFALSE) {
        			printf("Failed to delete IP. It doesn't exist in table.(idx=%d)\n", atoi(argv[2]));
    			}
			}
		}
		return pdTRUE;
	} else if (argc == 5 && (strcmp(argv[1], "edit") == 0)) {
		_ra6w1_dns_2nd_cache_init();
    	_ra6w1_dns_2nd_cache_modify_row(atoi(argv[2]), argv[3], atoi(argv[4]));	
		return pdTRUE;
	}
	return pdFALSE;
}
#endif // __DNS_2ND_CACHE_INFO__
///////

// res add_row(url[I], ip_addr[I]): find empty slot and add (+sflash update)
int ra6w1_dns_2nd_cache_add(const char* domain, char* ip_addr, unsigned long ip_addr_long) 
{
	int idx;
	int status;

	// convert if ip_addr is in unsigned long format
	if (ip_addr == NULL) {
		longtoip(ip_addr_long, ipaddr_str_temp);
		ip_addr = ipaddr_str_temp;
	}

    if (isvalidip(ip_addr) == pdFALSE) {
        return pdFALSE;
    }
    
	// find an existing one
	idx = _ra6w1_dns_2nd_cache_find(domain);
	
	if (idx == -1) { 		// domain not found
		// find an empty slot to add
		idx = _ra6w1_dns_2nd_cache_find_empty_slot();
	    DNS_DBG_INFO("[%s] find an empty slot idx=%d\n", __func__, idx);

		if (idx == -1) {
		    DNS_DBG_INFO("[%s] full\n", __func__);
            idx = _ra6w1_dns_2nd_cache_find_oldest_slot();
            DNS_DBG_INFO("[%s] find oldest idx=%d\n", __func__, idx);
		}

		// add
		strcpy(domain_ipaddr_table_p->table[idx].domain_str, domain);
	} else {
	    DNS_DBG_INFO("[%s] found domain idx=%d\n", __func__, idx);
		// found, update only ip_addr
	}

    strcpy(domain_ipaddr_table_p->table[idx].ipaddr_str, ip_addr);

    ra6w1_time64(NULL, &domain_ipaddr_table_p->table[idx].last_time);

	/// sflash update
	status = _ra6w1_dns_2nd_cache_update_sflash();
	if (status == FALSE) {
		// SFLASH write error.. but don't need to stop the operation...
		// Go ahead next operation ...
	}

	return TRUE;
}

// update sflash()
int ra6w1_dns_2nd_cache_erase_sflash(void) 
{
	// erase 4K
	return user_sflash_erase(SF_USER_AREA, 1024*4);
}

UINT ra6w1_dns_2nd_cache_find_answer(const char* domain, unsigned long* ip_addr) 
{
	UINT	status = TRUE;

	// Read URL_to_IP_Addr table from SFLASH
	if (dns_2st_cache_loading_flag == FALSE) {
		status = user_sflash_read(SF_USER_AREA, (char*)domain_ipaddr_table_p, sizeof(domain_to_ip_addr_t));
		dns_2st_cache_loading_flag = TRUE;
	}

	if (status == FALSE) {
		// SFLASH read error ...
		DNS_DBG_ERR("[%s] SFLASH read Error ! \n", __func__);
		return ERR_VAL;
	} else {
		// ... DNS 2nd cache loading is ok
		int idx;
		
		idx = _ra6w1_dns_2nd_cache_find(domain);

		if (idx != -1) {
			// cache hit !
            unsigned long long current_time;
            int time_remaining;

            ra6w1_time64(NULL, &current_time);

            if (domain_ipaddr_table_p->table[idx].last_time + CACHE_EXPIRETIME < current_time) {
                time_remaining = 0;
            } else {
                time_remaining = (int)((domain_ipaddr_table_p->table[idx].last_time + CACHE_EXPIRETIME) - current_time);
            }

            if (time_remaining) {
				*ip_addr = (unsigned long)ipaddr_addr(domain_ipaddr_table_p->table[idx].ipaddr_str);\
				DNS_DBG_INFO("[%s] Matched : to_find=<%s>, ipaddr=<%s>\n", __func__, 
				            domain, domain_ipaddr_table_p->table[idx].ipaddr_str);
                return ERR_OK;
            } else {
                // the ip addr is not valid any more
                DNS_DBG_INFO("[%s] TTL has expired.\n", __func__);
    			return ERR_VAL;
            }
		} else {
			// cache NOT hit !
			return ERR_VAL;
		}
	}
}

#else 

UINT ra6w1_dns_2nd_cache_find_answer(const char* domain, unsigned long* ip_addr) 
{
	RA6W1_UNUSED_ARG(domain);
	RA6W1_UNUSED_ARG(ip_addr);

	return ERR_VAL;
}
#endif /* LWIP_DNS */

#endif  // __DNS_2ND_CACHE_SUPPORT__
#endif /* CFG_WIFI */

/* EOF */
