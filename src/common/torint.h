/* Copyright 2003 Roger Dingledine
 * Copyright 2004-2007 Roger Dingledine, Nick Mathewson */
/* See LICENSE for licensing information */
/* $Id$ */

/**
 * \file torint.h
 * \brief Header file to define uint32_t and friends
 **/

#ifndef __TORINT_H
#define __TORINT_H
#define TORINT_H_ID "$Id$"

#include "orconfig.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_SYS_LIMITS_H
#include <sys/limits.h>
#endif
#ifdef HAVE_MACHINE_LIMITS_H
#if !defined(__FreeBSD__) && !defined(__FreeBSD_kernel__)
  /* FreeBSD has a bug where it complains that this file is obsolete,
     and I should migrate to using sys/limits. It complains even when
     I include both.
     __FreeBSD_kernel__ is defined by Debian GNU/kFreeBSD which
      does the same thing (but doesn't defined __FreeBSD__).
     */
#include <machine/limits.h>
#endif
#endif
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifdef MS_WINDOWS
/* Windows likes to capitalize ssize_t.  Charming. */
#define ssize_t SSIZE_T
#endif

#if (SIZEOF_INT8_T != 0)
#define HAVE_INT8_T
#endif
#if (SIZEOF_INT16_T != 0)
#define HAVE_INT16_T
#endif
#if (SIZEOF_INT32_T != 0)
#define HAVE_INT32_T
#endif
#if (SIZEOF_INT64_T != 0)
#define HAVE_INT64_T
#endif
#if (SIZEOF_UINT8_T != 0)
#define HAVE_UINT8_T
#endif
#if (SIZEOF_UINT16_T != 0)
#define HAVE_UINT16_T
#endif
#if (SIZEOF_UINT32_T != 0)
#define HAVE_UINT32_T
#endif
#if (SIZEOF_UINT64_T != 0)
#define HAVE_UINT64_T
#endif
#if (SIZEOF_INTPTR_T != 0)
#define HAVE_INTPTR_T
#endif
#if (SIZEOF_UINTPTR_T != 0)
#define HAVE_UINTPTR_T
#endif

#if (SIZEOF_CHAR == 1)
#ifndef HAVE_INT8_T
typedef signed char int8_t;
#define HAVE_INT8_T
#endif
#ifndef HAVE_UINT8_T
typedef unsigned char uint8_t;
#define HAVE_UINT8_T
#endif
#endif

#if (SIZEOF_SHORT == 2)
#ifndef HAVE_INT16_T
typedef signed short int16_t;
#define HAVE_INT16_T
#endif
#ifndef HAVE_UINT16_T
typedef unsigned short uint16_t;
#define HAVE_UINT16_T
#endif
#endif

#if (SIZEOF_INT == 2)
#ifndef HAVE_INT16_T
typedef signed int int16_t;
#define HAVE_INT16_T
#endif
#ifndef HAVE_UINT16_T
typedef unsigned int uint16_t;
#define HAVE_UINT16_T
#endif
#elif (SIZEOF_INT == 4)
#ifndef HAVE_INT32_T
typedef signed int int32_t;
#define HAVE_INT32_T
#endif
#ifndef HAVE_UINT32_T
typedef unsigned int uint32_t;
#define HAVE_UINT32_T
#endif
#ifndef UINT32_MAX
#define UINT32_MAX 0xffffffffu
#endif
#ifndef INT32_MAX
#define INT32_MAX 0x7fffffffu
#endif
#endif

#if (SIZEOF_LONG == 4)
#ifndef HAVE_INT32_T
typedef signed long int32_t;
#define HAVE_INT32_T
#endif
#ifndef HAVE_UINT32_T
typedef unsigned long uint32_t;
#define HAVE_UINT32_T
#ifndef UINT32_MAX
#define UINT32_MAX 0xfffffffful
#endif
#endif
#elif (SIZEOF_LONG == 8)
#ifndef HAVE_INT64_T
typedef signed long int64_t;
#define HAVE_INT64_T
#endif
#ifndef HAVE_UINT32_T
typedef unsigned long uint64_t;
#define HAVE_UINT32_T
#endif
#ifndef UINT64_MAX
#define UINT64_MAX 0xfffffffffffffffful
#endif
#endif

#if (SIZEOF_LONG_LONG == 8)
#ifndef HAVE_INT64_T
typedef signed long long int64_t;
#define HAVE_INT64_T
#endif
#ifndef HAVE_UINT64_T
typedef unsigned long long uint64_t;
#define HAVE_UINT64_T
#endif
#ifndef UINT64_MAX
#define UINT64_MAX 0xffffffffffffffffull
#endif
#ifndef INT64_MAX
#define INT64_MAX 0x7fffffffffffffffll
#endif
#endif

#if (SIZEOF___INT64 == 8)
#ifndef HAVE_INT64_T
typedef signed __int64 int64_t;
#define HAVE_INT64_T
#endif
#ifndef HAVE_UINT64_T
typedef unsigned __int64 uint64_t;
#define HAVE_UINT64_T
#endif
#ifndef UINT64_MAX
#define UINT64_MAX 0xffffffffffffffffui64
#endif
#ifndef INT64_MAX
#define INT64_MAX 0x7fffffffffffffffi64
#endif
#endif

#if (SIZEOF_VOID_P > 4 && SIZEOF_VOID_P <= 8)
#ifndef HAVE_INTPTR_T
typedef int64_t intptr_t;
#endif
#ifndef HAVE_UINTPTR_T
typedef uint64_t uintptr_t;
#endif
#elif (SIZEOF_VOID_P > 2 && SIZEOF_VOID_P <= 4)
#ifndef HAVE_INTPTR_T
typedef int32_t intptr_t;
#endif
#ifndef HAVE_UINTPTR_T
typedef uint32_t uintptr_t;
#endif
#else
#error "void * is either >8 bytes or <= 2.  In either case, I am confused."
#endif

#ifndef HAVE_INT8_T
#error "Missing type int8_t"
#endif
#ifndef HAVE_UINT8_T
#error "Missing type uint8_t"
#endif
#ifndef HAVE_INT16_T
#error "Missing type int16_t"
#endif
#ifndef HAVE_UINT16_T
#error "Missing type uint16_t"
#endif
#ifndef HAVE_INT32_T
#error "Missing type int32_t"
#endif
#ifndef HAVE_UINT32_T
#error "Missing type uint32_t"
#endif
#ifndef HAVE_INT64_T
#error "Missing type int64_t"
#endif
#ifndef HAVE_UINT64_T
#error "Missing type uint64_t"
#endif

/* This assumes a sane (2's-complement) representation.  But if you
 * aren't 2's complement, and you don't define LONG_MAX, then you're so
 * bizarre that I want nothing to do with you. */
#ifndef USING_TWOS_COMPLEMENT
#error "Seems that your platform doesn't use 2's complement arithmetic. Argh."
#endif
#ifndef LONG_MAX
#if (SIZEOF_LONG == 4)
#define LONG_MAX 0x7fffffffL
#elif (SIZEOF_LONG == 8)
#define LONG_MAX 0x7fffffffffffffffL
#else
#error "Can't define LONG_MAX"
#endif
#endif

#ifndef INT_MAX
#if (SIZEOF_INT == 4)
#define INT_MAX 0x7fffffffL
#elif (SIZEOF_INT == 8)
#define INT_MAX 0x7fffffffffffffffL
#else
#error "Can't define INT_MAX"
#endif
#endif

#ifndef UINT_MAX
#if (SIZEOF_INT == 2)
#define UINT_MAX 0xffffu
#elif (SIZEOF_INT == 4)
#define UINT_MAX 0xffffffffu
#elif (SIZEOF_INT == 8)
#define UINT_MAX 0xffffffffffffffffu
#else
#error "Can't define UINT_MAX"
#endif
#endif

#ifndef TIME_MAX

#ifdef TIME_T_IS_SIGNED

#if (SIZEOF_TIME_T == SIZEOF_INT)
#define TIME_MAX ((time_t)INT_MAX)
#elif (SIZEOF_TIME_T == SIZEOF_LONG)
#define TIME_MAX ((time_t)LONG_MAX)
#else
#error "Can't define (signed) TIME_MAX"
#endif

#else
/* Unsigned case */
#if (SIZEOF_TIME_T == 4)
#define TIME_MAX ((time_t)UINT32_MAX)
#elif (SIZEOF_TIME_T == 8)
#define TIME_MAX ((time_t)UINT64_MAX)
#else
#error "Can't define (unsigned) TIME_MAX"
#endif
#endif /* time_t_is_signed */
#endif /* ifndef(TIME_MAX) */

#ifndef SIZE_T_MAX
#if (SIZEOF_SIZE_T == 4)
#define SIZE_T_MAX UINT32_MAX
#elif (SIZEOF_SIZE_T == 8)
#define SIZE_T_MAX UINT64_MAX
#else
#error "Can't define SIZE_T_MAX"
#endif
#endif

/* Any size_t larger than this amount is likely to be an underflow. */
#define SIZE_T_CEILING (sizeof(char)<<(sizeof(size_t)*8 - 1))

#endif /* __TORINT_H */

