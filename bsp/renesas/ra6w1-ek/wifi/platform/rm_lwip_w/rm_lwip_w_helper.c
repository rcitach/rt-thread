/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include <string.h>
#include "lwip/inet.h"
#include "FreeRTOS.h"
#include <stdlib.h>
#include "rm_lwip_w_helper.h"
#ifdef R_RTC_W_HELPER_H
 #include "r_rtc_w_helper.h"
#endif                                 /* R_RTC_W_HELPER_H */
#ifdef R_RTC_W_H
 #include "r_rtc_w.h"
#endif                                 /* R_RTC_W_H */

/**
 * x^n square root function
 */
long pow_long (long x, int order)
{
    long result = 1;
    int  bit    = 1;

    while (bit <= order)
    {
        if (bit & order)
        {
            result *= x;
        }

        x    *= x;
        bit <<= 1;
    }

    return result;
}

#if LWIP_TESTCASE == 0

// returns the amount of bits used in a mask
static int calcbits (long mask)
{
    int b, bits = 0;

    for (b = 0; b < 32; b++)
    {
        if (mask & (long) pow_long(2, b))
        {
            bits++;
        }
    }

    return bits;
}

#endif

#if defined(__SUPPORT_IPV4__)

// returns the digit of an ip address. eg: digit 2 of 193.19.136.1 = 19
static int getipdigit (long ipaddress, int digit) // __SUPPORT_IPV4__
{
    char * ips, * ipe;
    int    c;
    int    status;

    ips = pvPortMalloc(16);

    memset(ips, 0, 16);

    longtoip(ipaddress, ips);

    if ((digit > 4) || (digit < 1))
    {
        vPortFree(ips);

        return 0;
    }

    for (c = 1; c < digit; c++)
    {
        while (*ips != '.')
        {
            ips++;
        }

        ips++;
    }

    ipe = ips;

    while (*ipe != '.' && *ipe != 0)
    {
        ipe++;
    }

    *ipe = 0;

    status = atoi(ips);

    vPortFree(ips);

    return status;
}

/*checks the ip format and value. returns 1 for ok, 0 for not ok */
int isvalidip (char * theip)           // __SUPPORT_IPV4__
{
    int       x, pcnt = 0;
    long      newip;
    char      ipdig[32];
    ip_addr_t tmp_addr;

    memset(ipdig, 0, 32);
    if (strlen(theip) < 7)             // minimum ip address lengh: 7
    {
        return pdFALSE;                // ex) "1.0.0.0": 7
    }

    for (x = 0; x <= (int) strlen(theip); x++)
    {
        if ((((theip[x] < '0') || (theip[x] > '9')) ? 0 : 1))
        {
            ipdig[strlen(ipdig)] = theip[x];
        }
        else
        {
            if (strlen(ipdig) == 0)
            {
                return pdFALSE;
            }

            if ((theip[x] != '.') && (theip[x] != 0))
            {
                return pdFALSE;
            }

            if ((atoi(ipdig) > 255) || (atoi(ipdig) < 0))
            {
                return pdFALSE;
            }

            pcnt++;

            if (pcnt > 4)
            {
                return pdFALSE;
            }

            memset(ipdig, 0, 32);
        }

        if (strlen(ipdig) > 3)
        {
            return pdFALSE;
        }
    }

    ipaddr_aton(theip, &tmp_addr);
    newip = (long) lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));

    if ((newip == 0) || (newip == -1) || (getipdigit(newip, 1) == 0))
    {
        return pdFALSE;
    }

    return pdTRUE;
}

/* check validity of ip first and then check ip class : only class A, B, and C allowed */
int is_in_valid_ip_class (char * theip) // __SUPPORT_IPV4__
{
    int           ret     = pdTRUE;
    unsigned long ip_addr = 0;
    ip_addr_t     tmp_addr;

    if (isvalidip(theip))
    {
        ipaddr_aton(theip, &tmp_addr);
        ip_addr = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
    }
    else
    {
        return pdFALSE;
    }

    if (IN_CLASSA(ip_addr) || IN_CLASSB(ip_addr) || IN_CLASSC(ip_addr))
    {
        ret = pdTRUE;
    }
    else
    {
        ret = pdFALSE;
    }

    return ret;
}

/**
 * Start IP address of Subnet Mask
 */
long subnetRangeFirstIP (long ip, long subnet) // __SUPPORT_IPV4__
{
    long startip = (ip & subnet) + 1;

    return startip;
}

/**
 * Broadcast IP address of Subnet Mask
 */
static long subnetBCIP (long ip, long subnet)
{
    long bits  = subnet ^ 0xffffffff;
    long endip = (ip | bits);

    return endip;
}

/**
 * End IP address of Subnet Mask
 */
long subnetRangeLastIP (long ip, long subnet) // __SUPPORT_IPV4__
{
    return subnetBCIP(ip, subnet) - 1;
}

/**
 * API to check the validity of Subnet mask
 */
int isvalidIPsubnetRange (long ip, long subnetip, long subnet) // __SUPPORT_IPV4__
{
    int  status;
    long firstIP;
    long lastIP;

    firstIP = subnetRangeFirstIP(subnetip, subnet);
    lastIP  = subnetRangeLastIP(subnetip, subnet);

    if ((firstIP <= ip) && (lastIP >= ip))
    {
        status = pdTRUE;
    }
    else
    {
        status = pdFALSE;
    }

    return status;
}

 #if LWIP_TESTCASE == 0

/*checks the subnetmask format and value. returns 1 for ok, 0 for not ok */
int isvalidmask (char * theip)         // __SUPPORT_IPV4__
{
    long      themask;
    int       mbits, b = 31;
    ip_addr_t tmp_addr;

    ipaddr_aton(theip, &tmp_addr);
    themask = (long) lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));

    if ((themask == 0) || (themask == -1))
    {
        return pdFALSE;
    }

    mbits = calcbits(themask);

    if (mbits > (32 - 2))
    {
        return pdFALSE;
    }

    // test consecutive bits with value 1
    while ((themask & (long) pow_long(2, b)) && b >= 0)
    {
        b--;
    }

    // test consecutive bits with value 0
    while (!(themask & (long) pow_long(2, b)) && b >= 0)
    {
        b--;
    }

    // if all the bits were not tested then invalid mask. ok = 111110000000... error: 1111100001000...
    if (b != -1)
    {
        return pdFALSE;
    }

    return pdTRUE;
}

 #endif

/**
 * API to check the validity of IP address range
 */
int isvalidIPrange (long ip, long firstIP, long lastIP) // __SUPPORT_IPV4__
{
    int status;
    if ((firstIP <= ip) && (lastIP >= ip))
    {
        status = pdTRUE;
    }
    else
    {
        status = pdFALSE;
    }

    return status;
}

// returns the ip notation (eg 127.0.0.1) from a long
void longtoip (long ip, char * ipbuf)  // __SUPPORT_IPV4__
{
    int w, x, y, z;

    memset(ipbuf, 0, 16);
    w = (ip >> 24) & 0xFF;
    x = (ip >> 16) & 0xFF;
    y = (ip >> 8) & 0xFF;
    z = ip & 0xFF;
    sprintf(ipbuf, "%d.%d.%d.%d", w, x, y, z);
}

#endif                                 /* __SUPPORT_IPV4__ */

#if defined(__SUPPORT_IPV6__)

static unsigned int _parseDecimal (const char ** pchCursor)
{
    unsigned int nVal = 0;             /* Accumulator for parsed decimal value */
    char         chNow;                /* Current character being evaluated */

    /* Loop through characters as long as they are digits (0–9) */
    while ((chNow = **pchCursor) >= '0' && chNow <= '9')
    {
        /* Shift the previous result by one decimal place (multiply by 10) */
        nVal *= 10;

        /* Add the numeric value of the current character */
        nVal += chNow - '0';

        /* Advance the string cursor to the next character */
        ++*pchCursor;
    }

    /* Return the parsed decimal number */
    return nVal;
}

static unsigned int _parseHex (const char ** pchCursor)
{
    unsigned int nVal = 0;             /* Accumulator for parsed hexadecimal value */
    char         chNow;                /* Current character being evaluated */

    /*
     * Loop through the input string while characters are valid hexadecimal digits.
     * The expression ( **pchCursor & 0x5F ) forces letters to uppercase
     * (by masking off lowercase bit), ensuring 'a'–'f' are treated as 'A'–'F'.
     */
    while ((chNow = **pchCursor & 0x5F),
           (chNow >= ('0' & 0x5F) && chNow <= ('9' & 0x5F)) || (chNow >= 'A' && chNow <= 'F'))
    {
        unsigned char nybbleValue;     /* Stores numeric value of current hex digit */

        /*
         * Adjust ASCII value:
         *   - For '0'–'9', subtraction yields values 0–9.
         *   - For 'A'–'F', further adjust to yield 10–15.
         */
        chNow      -= 0x10;            /* Scootch numeric values down; hex now offset by 0x31 */
        nybbleValue = (chNow > 9 ? chNow - (0x31 - 0x0A) : chNow);

        /* Shift previously parsed nybbles left (multiply by 16) and add new nybble */
        nVal <<= 4;
        nVal  += nybbleValue;

        /* Advance the string cursor to the next character */
        ++*pchCursor;
    }

    /* Return the parsed hexadecimal number */
    return nVal;
}

/*
 * Parse an IPv4 or IPv6 address (optionally with port) into binary form.
 *
 * - ppszText: NUL-terminated ASCII string (updated to point after parsed text).
 * - abyAddr : 16-byte buffer for parsed address (may be NULL).
 * - pnPort  : Output port in network order, set to 0 if not present (may be NULL).
 * - pbIsIPv6: Set to 1 if IPv6, 0 if IPv4 (may be NULL).
 *
 * Returns 1 on success, 0 on failure.
 * Notes:
 *   - No leading/internal whitespace allowed (but may terminate parsing).
 *   - Address and port values are stored in network byte order.
 */
static int ParseIPv4OrIPv6 (const char ** ppszText, unsigned char * abyAddr, int * pnPort, int * pbIsIPv6)
{
    unsigned char * abyAddrLocal;
    unsigned char   abyDummyAddr[16];

    /* find first colon, dot, and open bracket */
    const char * pchColon        = strchr(*ppszText, ':');
    const char * pchDot          = strchr(*ppszText, '.');
    const char * pchOpenBracket  = strchr(*ppszText, '[');
    const char * pchCloseBracket = NULL;

    /* Treat as IPv6 if: starts with '[', has no dots, or a ':' appears before any '.' */
    int bIsIPv6local = NULL != pchOpenBracket || NULL == pchDot ||
                       (NULL != pchColon && (NULL == pchDot || pchColon < pchDot));

    /* OK, now do a little further sanity check our initial guess... */
    if (bIsIPv6local)
    {
        pchCloseBracket = strchr(*ppszText, ']');
        if ((NULL != pchOpenBracket) && ((NULL == pchCloseBracket) || (pchCloseBracket < pchOpenBracket)))
        {
            return 0;
        }
    }
    else
    {
        if ((NULL == pchDot) || ((NULL != pchColon) && (pchColon < pchDot)))
        {
            return 0;
        }
    }

    if (NULL != pbIsIPv6)
    {
        *pbIsIPv6 = bIsIPv6local;
    }

    abyAddrLocal = abyAddr;
    if (NULL == abyAddrLocal)
    {
        abyAddrLocal = abyDummyAddr;
    }

    if (!bIsIPv6local)
    {
        unsigned char * pbyAddrCursor = abyAddrLocal;
        unsigned int    nVal;
        const char    * pszTextBefore = *ppszText;

        nVal = _parseDecimal(ppszText);
        if (('.' != **ppszText) || (nVal > 255) || (pszTextBefore == *ppszText))
        {
            return 0;
        }

        *(pbyAddrCursor++) = (unsigned char) nVal;
        ++(*ppszText);

        pszTextBefore = *ppszText;
        nVal          = _parseDecimal(ppszText);
        if (('.' != **ppszText) || (nVal > 255) || (pszTextBefore == *ppszText))
        {
            return 0;
        }

        *(pbyAddrCursor++) = (unsigned char) nVal;
        ++(*ppszText);

        pszTextBefore = *ppszText;
        nVal          = _parseDecimal(ppszText);
        if (('.' != **ppszText) || (nVal > 255) || (pszTextBefore == *ppszText))
        {
            return 0;
        }

        *(pbyAddrCursor++) = (unsigned char) nVal;
        ++(*ppszText);

        pszTextBefore = *ppszText;
        nVal          = _parseDecimal(ppszText);
        if ((nVal > 255) || (pszTextBefore == *ppszText))
        {
            return 0;
        }

        *(pbyAddrCursor++) = (unsigned char) nVal;
        if ((':' == **ppszText) && (NULL != pnPort))
        {
            unsigned short usPortNetwork;
            ++(*ppszText);
            pszTextBefore = *ppszText;
            nVal          = _parseDecimal(ppszText);
            if ((nVal > 65535) || (pszTextBefore == *ppszText))
            {
                return 0;
            }

            ((unsigned char *) &usPortNetwork)[0] = (nVal & 0xff00) >> 8;
            ((unsigned char *) &usPortNetwork)[1] = (nVal & 0xff);
            *pnPort = usPortNetwork;

            return 1;
        }
        else
        {
            if (NULL != pnPort)
            {
                *pnPort = 0;
            }

            return 1;
        }
    }
    else
    {
        unsigned char * pbyAddrCursor;
        unsigned char * pbyZerosLoc;
        int             bIPv4Detected;
        int             nIdx;

        if (NULL != pchOpenBracket)
        {
            *ppszText = pchOpenBracket + 1;
        }

        pbyAddrCursor = abyAddrLocal;
        pbyZerosLoc   = NULL;
        bIPv4Detected = 0;
        for (nIdx = 0; nIdx < 8; ++nIdx)
        {
            const char * pszTextBefore = *ppszText;
            unsigned     nVal          = _parseHex(ppszText);
            if (pszTextBefore == *ppszText)
            {
                if (NULL != pbyZerosLoc)
                {
                    if (pbyZerosLoc == pbyAddrCursor)
                    {
                        --nIdx;
                        break;
                    }

                    return 0;
                }

                if (':' != **ppszText)
                {
                    return 0;
                }

                if (0 == nIdx)
                {
                    ++(*ppszText);
                    if (':' != **ppszText)
                    {
                        return 0;
                    }
                }

                pbyZerosLoc = pbyAddrCursor;
                ++(*ppszText);
            }
            else
            {
                if ('.' == **ppszText)
                {
                    const char  * pszTextlocal = pszTextBefore;
                    unsigned char abyAddrlocal[16];
                    int           bIsIPv6loc;
                    int           bParseResultlocal = ParseIPv4OrIPv6(&pszTextlocal, abyAddrlocal, NULL, &bIsIPv6loc);
                    *ppszText = pszTextlocal;

                    if (!bParseResultlocal || bIsIPv6loc)
                    {
                        return 0;
                    }

                    *(pbyAddrCursor++) = abyAddrlocal[0];
                    *(pbyAddrCursor++) = abyAddrlocal[1];
                    *(pbyAddrCursor++) = abyAddrlocal[2];
                    *(pbyAddrCursor++) = abyAddrlocal[3];
                    ++nIdx;
                    bIPv4Detected = 1;
                    break;
                }

                if (nVal > 65535)
                {
                    return 0;
                }

                *(pbyAddrCursor++) = nVal >> 8;
                *(pbyAddrCursor++) = nVal & 0xff;
                if (':' == **ppszText)
                {
                    ++(*ppszText);
                }
                else
                {
                    break;
                }
            }
        }

        if (NULL != pbyZerosLoc)
        {
            int nHead  = (int) (pbyZerosLoc - abyAddrLocal);
            int nTail  = nIdx * 2 - (int) (pbyZerosLoc - abyAddrLocal);
            int nZeros = 16 - nTail - nHead;
            memmove(&abyAddrLocal[16 - nTail], pbyZerosLoc, nTail);
            memset(pbyZerosLoc, 0, nZeros);
        }

        if (bIPv4Detected)
        {
            static const unsigned char abyPfx[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
            if (0 != memcmp(abyAddrLocal, abyPfx, sizeof(abyPfx)))
            {
                return 0;
            }
        }

        if (NULL != pchOpenBracket)
        {
            if (']' != **ppszText)
            {
                return 0;
            }

            ++(*ppszText);
        }

        if ((':' == **ppszText) && (NULL != pnPort))
        {
            const char   * pszTextBefore;
            unsigned int   nVal;
            unsigned short usPortNetwork;

            ++(*ppszText);
            pszTextBefore = *ppszText;
            pszTextBefore = *ppszText;
            nVal          = _parseDecimal(ppszText);
            if ((nVal > 65535) || (pszTextBefore == *ppszText))
            {
                return 0;
            }

            ((unsigned char *) &usPortNetwork)[0] = (nVal & 0xff00) >> 8;
            ((unsigned char *) &usPortNetwork)[1] = (nVal & 0xff);
            *pnPort = usPortNetwork;

            return 1;
        }
        else
        {
            if (NULL != pnPort)
            {
                *pnPort = 0;
            }

            return 1;
        }
    }
}

static int hexaInt_strlen (int value)
{
    char tmp_str[9];

    memset(tmp_str, 0, 9);
    sprintf(tmp_str, "%x", value);

    return strlen(tmp_str);
}

/*
 * simple Type : 4  Display short address with 4-digit '0'
 * ex)  FE80::A1E6:0000:E145:F6E0
 *
 * simple Type : 1  Display short address with 1-digit '0'
 * ex)  FE80::A1E6:0:E145:F6E0
 *
 * simple Type : 0 : Display all 4-digit '0'
 * ex)  FE80:0000:0001:0000:A1E6:0000:E145:F6E0
 */
static void bin2ipstr (unsigned char * pbyBin, int nLen, int simple, char * ip_str)
{
    int          i;
    unsigned int ipv6_uint[8];
    int          idx = 0;

    if (nLen == 16)
    {
        for (i = 0; i < nLen / 2; ++i)
        {
            ipv6_uint[i] = (pbyBin[i * 2] << 8) | pbyBin[(i * 2) + 1];
        }

        if (simple)
        {
            for (i = 0; i < nLen / 2; ++i)
            {
                /* PRINT Value  */
                if (((i > 0) && (ipv6_uint[i - 1] == 0x0) && (ipv6_uint[i] == 0x0)) ||
                    ((i < nLen / 2) && (ipv6_uint[i] == 0x0) && (ipv6_uint[i + 1] == 0x0)))
                {
                    /* SKIP */;
                }
                else
                {
                    sprintf((char *) ip_str + idx, (simple == 4) ? "%04x" : "%x", ipv6_uint[i]);
                    idx = idx + (simple == 4 ? 4 : hexaInt_strlen(ipv6_uint[i]));

                    // printf ("(%d)", i);
                    // printf ("inx(%d)=%x(len=%d)\n", i, ipv6_uint[i], hexaInt_strlen(ipv6_uint[i]));
                }

                /* PRINT ":" */
                if (((ipv6_uint[i - 1] == 0x0) && (ipv6_uint[i] == 0x0)) ||
                    ((ipv6_uint[i] == 0x0) && (ipv6_uint[i + 1] == 0x0)))
                {
                    if ((ipv6_uint[i + 1] != 0x0) || ((ipv6_uint[i] == 0x0) && (i == 0)))
                    {
                        strcat((char *) ip_str, ":");
                        idx++;

                        // printf("{%d}", i);
                    }

                    /* SKIP */
                }
                else if (i < (nLen / 2) - 1)
                {
                    strcat((char *) ip_str, ":");
                    idx++;

                    // printf("{%d}", i);
                }
            }
        }
        else                           /* FULL Address */
        {
            for (i = 0; i < (nLen / 2); i++)
            {
                sprintf((char *) ip_str + idx, "%04x", ipv6_uint[i]);
                idx = idx + 4;

                if (i < 7)
                {
                    strcat((char *) ip_str, ":");
                    idx++;
                }
            }
        }
    }
    else                               /* IPv4 Address */
    {
        sprintf((char *) ip_str, "%d.%d.%d.%d", pbyBin[0], pbyBin[1], pbyBin[2], pbyBin[3]);
    }
}

/* unsigned char[16] ==> LONG[4] */
static void ipv6uchar2Long (unsigned char * ipv6_uchar, unsigned long * ipv6_long)
{
    for (int i = 0; i < 4; i++)
    {
        ipv6_long[i] = (ipv6_uchar[i * 4] << 24) |
                       (ipv6_uchar[(i * 4) + 1] << 16) |
                       (ipv6_uchar[(i * 4) + 2] << 8) |
                       (ipv6_uchar[(i * 4) + 3]);
    }
}

/* LONG[4] ==> unsigned char[16] */
static void ipv6Long2uchar (unsigned long * ipv6_long, unsigned char * ipv6_uchar)
{
    ipv6_uchar[0] = ipv6_long[0] >> 24;
    ipv6_uchar[1] = ipv6_long[0] >> 16 & 0x000000FF;
    ipv6_uchar[2] = ipv6_long[0] >> 8 & 0x000000FF;
    ipv6_uchar[3] = ipv6_long[0] & 0x000000FF;

    ipv6_uchar[4] = ipv6_long[1] >> 24;
    ipv6_uchar[5] = ipv6_long[1] >> 16 & 0x000000FF;
    ipv6_uchar[6] = ipv6_long[1] >> 8 & 0x000000FF;
    ipv6_uchar[7] = ipv6_long[1] & 0x000000FF;

    ipv6_uchar[8]  = ipv6_long[2] >> 24;
    ipv6_uchar[9]  = ipv6_long[2] >> 16 & 0x000000FF;
    ipv6_uchar[10] = ipv6_long[2] >> 8 & 0x000000FF;
    ipv6_uchar[11] = ipv6_long[2] & 0x000000FF;

    ipv6_uchar[12] = ipv6_long[3] >> 24;
    ipv6_uchar[13] = ipv6_long[3] >> 16 & 0x000000FF;
    ipv6_uchar[14] = ipv6_long[3] >> 8 & 0x000000FF;
    ipv6_uchar[15] = ipv6_long[3] & 0x000000FF;
}

/* LONG[4] ==> String */
void ipv6long2str (unsigned long * ipv6_long, char * ipv6_str)
{
    unsigned char ipv6_uchar[16];

    ipv6Long2uchar(ipv6_long, ipv6_uchar);
    bin2ipstr(ipv6_uchar, 16, 1, ipv6_str);
}

/* Stirng ==> LONG[4] */
int parse_IPv6_to_long (const char * pszText, unsigned long * ipv6addr, int * pnPort)
{
    (void) pnPort;

    unsigned char abyAddr[16];
    int           bIsIPv6;
    int           nPort;
    int           bSuccess;

    const char * pszTextCursor = pszText;
    bSuccess = ParseIPv4OrIPv6(&pszTextCursor, abyAddr, &nPort, &bIsIPv6);

    if (!bSuccess || !bIsIPv6)
    {
        return 0;
    }

    if (ipv6addr)
    {
        ipv6uchar2Long(abyAddr, ipv6addr);
    }

    return 1;
}

 #ifdef R_RTC_W_HELPER_H
void sntp_calendar_system_time_set (__time64_t * corrtime)
{
    struct tm ts;
    memset(&ts, 0x00, sizeof(struct tm));

    rtc_w_lock_take();
    ts = *((struct tm *) ra6w1_localtime64(corrtime));
    rtc_w_lock_give();

  #ifdef R_RTC_W_H
    R_RTC_W_CalendarTimeSet(R_RTC_W_GetCtrl(), &ts);
  #endif                               /* R_RTC_W_H */
}

 #endif                                /* R_RTC_W_HELPER_H */

#endif                                 /* __SUPPORT_IPV6__ */
