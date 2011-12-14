/* Copyright (c) 2001, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef _my_stacktrace_h_
#define _my_stacktrace_h_

#include <my_global.h>

#ifdef TARGET_OS_LINUX
#if defined (__x86_64__) || defined (__i386__) || \
    (defined(__alpha__) && defined(__GNUC__))
#define HAVE_STACKTRACE 1
#endif
#elif defined(__WIN__) || defined(HAVE_PRINTSTACK)
#define HAVE_STACKTRACE 1
#endif

#if HAVE_BACKTRACE && (HAVE_BACKTRACE_SYMBOLS || HAVE_BACKTRACE_SYMBOLS_FD)
#undef HAVE_STACKTRACE
#define HAVE_STACKTRACE 1
#endif

#define HAVE_WRITE_CORE

#if HAVE_BACKTRACE && HAVE_BACKTRACE_SYMBOLS && \
    HAVE_CXXABI_H && HAVE_ABI_CXA_DEMANGLE && \
    HAVE_WEAK_SYMBOL
#define BACKTRACE_DEMANGLE 1
#endif

C_MODE_START

#if defined(HAVE_STACKTRACE) || defined(HAVE_BACKTRACE)
void my_init_stacktrace();
void my_print_stacktrace(uchar* stack_bottom, ulong thread_stack);
void my_safe_print_str(const char* val, int max_len);
void my_write_core(int sig);
#if BACKTRACE_DEMANGLE
char *my_demangle(const char *mangled_name, int *status);
#endif
#ifdef __WIN__
void my_set_exception_pointers(EXCEPTION_POINTERS *ep);
#endif
#endif

#ifdef HAVE_BFD_H
#define HAVE_MY_ADDR_RESOLVE 1
#endif

typedef struct {
  const char *file;
  const char *func;
  uint line;
} my_addr_loc;

#ifdef HAVE_MY_ADDR_RESOLVE
int my_addr_resolve(void *ptr, my_addr_loc *loc);
const char *my_addr_resolve_init();
#else
#define my_addr_resolve_init()  (0)
#define my_addr_resolve(A,B)    (1)
#endif

#ifdef HAVE_WRITE_CORE
void my_write_core(int sig);
#endif

C_MODE_END

#endif /* _my_stacktrace_h_ */
