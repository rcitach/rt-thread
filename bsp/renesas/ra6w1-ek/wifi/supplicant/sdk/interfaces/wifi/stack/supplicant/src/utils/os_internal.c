/*
 * wpa_supplicant/hostapd / Internal implementation of OS specific functions
 * Copyright (c) 2005-2006, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * This file is an example of operating system specific  wrapper functions.
 * This version implements many of the functions internally, so it can be used
 * to fill in missing functions from the target system C libraries.
 *
 * Some of the functions are using standard C library calls in order to keep
 * this file in working condition to allow the functions to be tested on a
 * Linux target. Please note that OS_NO_C_LIB_DEFINES needs to be defined for
 * this file to work correctly. Note that these implementations are only
 * examples and are not optimized for speed.
 */

#include "includes.h"
#if 0
#include <time.h>
#include <sys/wait.h>
#endif

#include "osal.h"
#include "os.h"
#include "supp_eloop.h"
#include "time.h"

#ifdef  BUILD_OPT_FC9000_ROMNDK
#include "rom_environ.h"
#endif  /* BUILD_OPT_FC9000_ROMNDK */


#undef OS_REJECT_C_LIB_FUNCTIONS
#include "supp_common.h"

void os_sleep(os_time_t sec, os_time_t usec)
{
	if (sec)
		vTaskDelay(portCONVERT_MS_2_TICKS(sec * 1000));

	if (usec)
		vTaskDelay(portCONVERT_MS_2_TICKS(usec / 1000));
}


/*
TICK2SEC_ROUND_DOWN:
Convert ticks to seconds.
Convert nowTick to sec. + Convert (MAX Tick * overflowCount) to sec.
  + Convert remainder to sec. [nowTick(remainder ) + MAX Tick * overflowCount (remainder ) + wraparound offset * overFlowCount] 
*/

#define TICK2SEC_ROUND_DOWN(nowTick, overflowCount) \
             ((nowTick / configTICK_RATE_HZ) + ((portMAX_DELAY / configTICK_RATE_HZ) * overflowCount) \
          + (((nowTick % configTICK_RATE_HZ) + ((portMAX_DELAY % configTICK_RATE_HZ) * overflowCount) + (1 * overflowCount)) / configTICK_RATE_HZ))

/*
TICK2SEC_MOD2USEC:
After converting remaining ticks to seconds, convert remainder to microseconds.
Convert remainder to usec.  [nowTick(remainder) + MAX Tick * overflowCount (remainder) + wraparound offset * overFlowCount]
*/
#define TICK2SEC_MOD2USEC(nowTick, overflowCount) \
          (portCONVERT_TICKS_2_MS(((nowTick % configTICK_RATE_HZ) + ((portMAX_DELAY % configTICK_RATE_HZ) * overflowCount) + (1 * overflowCount)) % configTICK_RATE_HZ) * 1000)


int os_get_time(struct os_time *t)
{
//MODIFY_SUPPLICANT_FOR_FREERTOS - chg
#if 1 // Calculation of tick overflow
    TimeOut_t   xNow;

    vTaskSetTimeOutState(&xNow); // Read for xNumOfOverflows, xTickCount

    t->sec  = (os_time_t) TICK2SEC_ROUND_DOWN(xNow.xTimeOnEntering, (unsigned long) xNow.xOverflowCount);
    t->usec = (os_time_t) TICK2SEC_MOD2USEC(xNow.xTimeOnEntering, (unsigned long) xNow.xOverflowCount);

#if 0 // Debug
    printf("[%s] sec=%u.%06u xOverflowCount=%d tick=%lu(0x%x)\n", __func__,
            t->sec, t->usec,
            xNow.xOverflowCount, xNow.xTimeOnEntering, xNow.xTimeOnEntering);
#endif /* Debug */
#else
	TickType_t now = xTaskGetTickCount();

	t->sec = (now / configTICK_RATE_HZ);
	t->usec = (portCONVERT_TICKS_2_MS(now % configTICK_RATE_HZ) * 1000);
	
#endif /* Calculation of tick overflow */

	return 0;
}


int os_get_reltime(struct os_reltime *t)
{
	//MODIFY_SUPPLICANT_FOR_FREERTOS - chg
	return os_get_time((struct os_time *)t);
}


//MODIFY_SUPPLICANT_FOR_FREERTOS - chg
#ifdef  BUILD_OPT_FC9000_ROMNDK
int os_mktime(int year, int month, int day, int hour, int min, int sec, os_time_t *t)
{
	return ra6wx_os_mktime(year, month, day, hour, min, sec, (void *)t);
}

#else

int os_mktime(int year, int month, int day, int hour, int min, int sec, os_time_t *t)
{
	struct tm tm;

	if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 ||
		hour < 0 || hour > 23 || min < 0 || min > 59 || sec < 0 ||
		sec > 60)
		return -1;

	os_memset(&tm, 0, sizeof(tm));
	tm.tm_year = year - 1900;
	tm.tm_mon = month - 1;
	tm.tm_mday = day;
	tm.tm_hour = hour;
	tm.tm_min = min;
	tm.tm_sec = sec;

	*t = (os_time_t) mktime(&tm);
	*t = *t + (os_time_t) (0x83AA7E80 - 0x7E90);  /* Synchronize to ThreadX NTP */

	return 0;
}
#endif  /* BUILD_OPT_FC9000_ROMNDK */


#ifdef  BUILD_OPT_FC9000_ROMNDK	//MODIFY_SUPPLICANT_FOR_FREERTOS - chg
int os_gmtime(os_time_t t, struct os_tm *tm)
{
	return ra6wx_os_gmtime((void *)(&t), (void *)tm);
}

#else

int os_gmtime(os_time_t t, struct os_tm *tm)
{
	struct tm *tm2;
	time_t t2 = t;

	tm2 = gmtime(&t2);
	if (tm2 == NULL)
		return -1;
	tm->sec = tm2->tm_sec;
	tm->min = tm2->tm_min;
	tm->hour = tm2->tm_hour;
	tm->day = tm2->tm_mday;
	tm->month = tm2->tm_mon + 1;
	tm->year = tm2->tm_year + 1900;
	return 0;
}
#endif	//  BUILD_OPT_FC9000_ROMNDK


#if 0 //MODIFY_SUPPLICANT_FOR_FREERTOS
int os_daemonize(const char *pid_file)
{
	if (daemon(0, 0)) {
		wpa_printf(MSG_ERROR, "daemon error");
		return -1;
	}

	if (pid_file) {
		FILE *f = fopen(pid_file, "w");
		if (f) {
			fprintf(f, "%u\n", getpid());
			fclose(f);
		}
	}

	return -0;
}


void os_daemonize_terminate(const char *pid_file)
{
	if (pid_file)
		unlink(pid_file);
}
#endif // 0

int os_get_random(unsigned char *buf, size_t len)
{
	//MODIFY_SUPPLICANT_FOR_FREERTOS - chg
	unsigned int i;
	u8 *tmp_nonce;

	tmp_nonce = os_zalloc(len);

	for (i = 0; i < len; i++)
		tmp_nonce[i] = (u8) (rand() % 256);

	memcpy(buf, tmp_nonce, len);

	os_free(tmp_nonce);
	return 0;
}



unsigned long os_random(void)
{
	return (unsigned long) rand();
}

#if 0 //MODIFY_SUPPLICANT_FOR_FREERTOS
char * os_rel2abs_path(const char *rel_path)
{
	char *buf = NULL, *cwd, *ret;
	size_t len = 128, cwd_len, rel_len, ret_len;

	if (rel_path[0] == '/')
		return os_strdup(rel_path);

	for (;;) {
		buf = os_malloc(len);
		if (buf == NULL)
			return NULL;
		cwd = getcwd(buf, len);
		if (cwd == NULL) {
			os_free(buf);
#if 0
			if (errno != ERANGE) {
				return NULL;
			}
#endif
			len *= 2;
		} else {
			break;
		}
	}

	cwd_len = os_strlen(cwd);
	rel_len = os_strlen(rel_path);
	ret_len = cwd_len + 1 + rel_len + 1;
	ret = os_malloc(ret_len);
	if (ret) {
		os_memcpy(ret, cwd, cwd_len);
		ret[cwd_len] = '/';
		os_memcpy(ret + cwd_len + 1, rel_path, rel_len);
		ret[ret_len - 1] = '\0';
	}
	os_free(buf);
	return ret;
}


int os_program_init(void)
{
	return 0;
}


void os_program_deinit(void)
{
}


int os_setenv(const char *name, const char *value, int overwrite)
{
	return setenv(name, value, overwrite);
}


int os_unsetenv(const char *name)
{
#if defined(__FreeBSD__) || defined(__NetBSD__)
	unsetenv(name);
	return 0;
#else
	return unsetenv(name);
#endif
}
#endif


char * os_readfile(const char *name, size_t *len)
{
	FILE *f;
	char *buf;

	f = fopen(name, "rb");
	if (f == NULL)
		return NULL;

	fseek(f, 0, SEEK_END);
	*len = (size_t) ftell(f);
	fseek(f, 0, SEEK_SET);

	buf = os_malloc(*len);
	if (buf == NULL) {
		fclose(f);
		return NULL;
	}

	if (fread(buf, 1, *len, f) != *len) {
		fclose(f);
		os_free(buf);
		return NULL;
	}

	fclose(f);

	return buf;
}


int os_fdatasync(FILE *stream)
{
	(void) stream;
	return 0;
}


void * os_zalloc(size_t size)
{
	void *n = os_malloc(size);
	if (n)
		os_memset(n, 0, size);
	return n;
}


void * os_malloc(size_t size)
{
	//MODIFY_SUPPLICANT_FOR_FREERTOS
	return pvPortMalloc(size);
}


void * os_realloc(void *ptr, size_t size)
{
	//MODIFY_SUPPLICANT_FOR_FREERTOS
	void *result;

	if (ptr == NULL) {
		return os_zalloc(size);
	} else {
		result = os_zalloc(size);
		if (result == NULL)
			return NULL;
	}

	memcpy(result, ptr, size);

	os_free(ptr);
	return result;
}


void os_free(void *ptr)
{
	//MODIFY_SUPPLICANT_FOR_FREERTOS
	if (ptr) {
		vPortFree(ptr);
	}
}


void * os_memcpy_no_use(void *dest, const void *src, size_t n);

void * os_memcpy_no_use(void *dest, const void *src, size_t n)
{
	char *d = dest;
	const char *s = src;
	while (n--)
		*d++ = *s++;
	return dest;
}


void * os_memmove_no_use(void *dest, const void *src, size_t n);

void * os_memmove_no_use(void *dest, const void *src, size_t n)
{
	if (dest < src)
		os_memcpy(dest, src, n);
	else {
		/* overlapping areas */
		char *d = (char *) dest + n;
		const char *s = (const char *) src + n;
		while (n--)
			*--d = *--s;
	}
	return dest;
}


void * os_memset_no_use(void *s, int c, size_t n);

__attribute__((optimize("no-tree-loop-distribute-patterns")))
void * os_memset_no_use(void *s, int c, size_t n)
{
	char *p = s;
	while (n--)
		*p++ = (char) c;
	return s;
}


int os_memcmp_no_use(const void *s1, const void *s2, size_t n);

int os_memcmp_no_use(const void *s1, const void *s2, size_t n)
{
	const unsigned char *p1 = s1, *p2 = s2;

	if (n == 0)
		return 0;

	while (*p1 == *p2) {
		p1++;
		p2++;
		n--;
		if (n == 0)
			return 0;
	}

	return *p1 - *p2;
}


char * os_strdup(const char *s)
{
	char *res;
	size_t len;
	if (s == NULL)
		return NULL;
	len = os_strlen(s);
	res = os_malloc(len + 1);
	if (res)
		os_memcpy(res, s, len + 1);
	return res;
}


size_t os_strlen(const char *s)
{
	const char *p = s;
	while (*p)
		p++;
	return (size_t) (p - s);
}


int os_strcasecmp(const char *s1, const char *s2)
{
	register const unsigned char *cm = (const unsigned char *)charmap,

	*us1 = (const unsigned char *)s1,
	*us2 = (const unsigned char *)s2;

	while (cm[*us1] == cm[*us2++]) {
		if (*us1++ == '\0')
			return (0);
	}

	return (cm[*us1] - cm[*--us2]);
}


int os_strncasecmp(const char *s1, const char *s2, size_t n)
{
	if (n != 0) {
		register const unsigned char *cm = (const unsigned char *)charmap,
				*us1 = (const unsigned char *)s1,
				*us2 = (const unsigned char *)s2;

		do {
			if (cm[*us1] != cm[*us2++])
				return (cm[*us1] - cm[*--us2]);
			if (*us1++ == '\0')
				break;
		} while (--n != 0);
	}

	return (0);
}


char * os_strchr(const char *s, int c)
{
	while (*s) {
		if (*s == c)
			return (char *) s;
		s++;
	}
	return NULL;
}


char * os_strrchr(const char *s, int c)
{
	const char *p = s;
	while (*p)
		p++;
	p--;
	while (p >= s) {
		if (*p == c)
			return (char *) p;
		p--;
	}
	return NULL;
}


int os_strcmp(const char *s1, const char *s2)
{
	while (*s1 == *s2) {
		if (*s1 == '\0')
			break;
		s1++;
		s2++;
	}

	return *s1 - *s2;
}


int os_strncmp(const char *s1, const char *s2, size_t n)
{
	if (n == 0)
		return 0;

	while (*s1 == *s2) {
		if (*s1 == '\0')
			break;
		s1++;
		s2++;
		n--;
		if (n == 0)
			return 0;
	}

	return *s1 - *s2;
}


size_t os_strlcpy(char *dest, const char *src, size_t siz)
{
	const char *s = src;
	size_t left = siz;

	if (left) {
		/* Copy string up to the maximum size of the dest buffer */
		while (--left != 0) {
			if ((*dest++ = *s++) == '\0')
				break;
		}
	}

	if (left == 0) {
		/* Not enough room for the string; force NUL-termination */
		if (siz != 0)
			*dest = '\0';
		while (*s++)
			; /* determine total src string length */
	}

	return (size_t) (s - src - 1);
}


int os_memcmp_const(const void *a, const void *b, size_t len)
{
	const u8 *aa = a;
	const u8 *bb = b;
	size_t i;
	u8 res;

	for (res = 0, i = 0; i < len; i++)
		res |= aa[i] ^ bb[i];

	return res;
}


char * os_strstr(const char *haystack, const char *needle)
{
	size_t len = os_strlen(needle);
	while (*haystack) {
		if (os_strncmp(haystack, needle, len) == 0)
			return (char *) haystack;
		haystack++;
	}

	return NULL;
}


int os_snprintf(char *str, size_t size, const char *format, ...)
{
	va_list ap;
	int ret;

	/* See http://www.ijs.si/software/snprintf/ for portable
	 * implementation of snprintf.
	 */

	va_start(ap, format);
	ret = vsnprintf(str, size, format, ap);
	va_end(ap);
	if (size > 0)
		str[size - 1] = '\0';
	return ret;
}


#if 0 //MODIFY_SUPPLICANT_FOR_FREERTOS
int os_exec(const char *program, const char *arg, int wait_completion)
{
	pid_t pid;
	int pid_status;

	pid = fork();
	if (pid < 0) {
		wpa_printf(MSG_ERROR, "fork error");
		return -1;
	}

	if (pid == 0) {
		/* run the external command in the child process */
		const int MAX_ARG = 30;
		char *_program, *_arg, *pos;
		char *argv[MAX_ARG + 1];
		int i;

		_program = os_strdup(program);
		_arg = os_strdup(arg);

		argv[0] = _program;

		i = 1;
		pos = _arg;
		while (i < MAX_ARG && pos && *pos) {
			while (*pos == ' ')
				pos++;
			if (*pos == '\0')
				break;
			argv[i++] = pos;
			pos = os_strchr(pos, ' ');
			if (pos)
				*pos++ = '\0';
		}
		argv[i] = NULL;

		execv(program, argv);
		wpa_printf(MSG_ERROR, "execv error");
		os_free(_program);
		os_free(_arg);
		exit(0);
		return -1;
	}

	if (wait_completion) {
		/* wait for the child process to complete in the parent */
		waitpid(pid, &pid_status, 0);
	}

	return 0;
}
#endif // 0
