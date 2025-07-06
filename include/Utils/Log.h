/*
 * Copyright (c) 2020 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef LOG_H
#define LOG_H

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <stdexcept>

#define LOG_VERSION "0.1.0"
#define LOG_USE_COLOR

typedef struct {
    va_list ap;
    const char *fmt;
    const char *file;
    tm *time;
    void *udata;
    int line;
    int level;
} log_Event;

typedef void (*log_LogFn)(log_Event *ev);

typedef void (*log_LockFn)(bool lock, void *udata);

enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };

#define log_trace(...) log_log(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...) log_log(LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...) log_log(LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...)                                                                                                 \
    do {                                                                                                               \
        log_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__);                                                           \
        throw std::runtime_error("log error");                                                                         \
    } while (0)
#define log_fatal(...)                                                                                                 \
    do {                                                                                                               \
        log_log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__);                                                           \
        throw std::runtime_error("log fatal");                                                                         \
    } while (0)

const char *log_level_string(int level);

void log_set_lock(log_LockFn fn, void *udata);

void log_set_level(int level);

void log_set_quiet(bool enable);

int log_add_callback(log_LogFn fn, void *udata, int level);

void log_log(int level, const char *file, int line, const char *fmt, ...);

#endif
