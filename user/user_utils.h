/*
 * Copyright (c) 2011-2012 LeafGrass <leafgrass.g@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 * OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 * ARM SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 */

/*
 * @file    include/sys/debug.h
 * @brief   debuging services
 * @log     2011.8 initial revision
 */

#ifndef __SYS_DEBUG_H__
#define __SYS_DEBUG_H__

/*
 * Log
 */
#define LOG_ALL		0
#define LOG_DEBUG	1
#define LOG_INFO	2
#define LOG_WARN	3
#define LOG_ERROR	4
#define LOG_FATAL	5
#define LOG_LEVEL	LOG_ALL

#define __dec		(system_get_time()/1000000)
#define __frac		(system_get_time()/1000%1000)

#define log_dbg(msg, args...) \
	do { \
		if (LOG_LEVEL <= LOG_DEBUG) { \
			os_printf("[%6u.%03u] D/%s: " msg, __dec, __frac, __func__, ##args); \
		} \
	} while (0)
#define log_info(msg, args...) \
	do { \
		if (LOG_LEVEL <= LOG_INFO) { \
			os_printf("[%6u.%03u] I/%s: " msg, __dec, __frac, __func__, ##args); \
		} \
	} while (0)
#define log_warn(msg, args...) \
	do { \
		if (LOG_LEVEL <= LOG_WARN) { \
			os_printf("[%6u.%03u] W/%s: " msg, __dec, __frac, __func__, ##args); \
		} \
	} while (0)
#define log_err(msg, args...) \
	do { \
		if (LOG_LEVEL <= LOG_ERROR) { \
			os_printf("[%6u.%03u] E/%s: " msg, __dec, __frac, __func__, ##args); \
		} \
	} while (0)
#define log_fatal(msg, args...) \
	do { \
		if (LOG_LEVEL <= LOG_FATAL) { \
			os_printf("[%6u.%03u] F/%s: " msg, __dec, __frac, __func__, ##args); \
		} \
	} while (0)

/*
 * Assertion
 */
#define ASSERT_NONE	0
#define ASSERT_ALL	1
#define ASSERT_LEVEL	ASSERT_ALL

#if (ASSERT_LEVEL >= ASSERT_ALL)
#define _OS_ASSERT(exp) \
	do { \
		if (exp) { \
		} else { \
			log_fatal("assert failed: " #exp " at %s %d!\n", __FILE__, __LINE__); \
			while (1); \
		} \
	} while (0)
#else
#define _OS_ASSERT(exp) (void)((0))
#endif

#define dbg_assert(exp)			_OS_ASSERT(exp)

/*
 * Other Utils
 */
#define bzero(s,n)	(void)memset(s,0,n)

#endif /* __SYS_DEBUG_H__ */
