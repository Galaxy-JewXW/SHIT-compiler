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

#include "Utils/Log.h"
#include <chrono>

constexpr int MAX_CALLBACKS = 32;

static std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();

typedef struct {
    log_LogFn fn;
    void *udata;
    int level;
} Callback;

static struct {
    void *udata;
    log_LockFn lock;
    int level;
    bool quiet;
    Callback callbacks[MAX_CALLBACKS];
} L;

static const char *level_strings[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {
    "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"
};
#endif

static void stdout_callback(log_Event *ev) {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();

#ifdef LOG_USE_COLOR
    fprintf(
        static_cast<FILE *>(ev->udata), "[%5ldms] %s%-5s\x1b[0m \x1b[90m%s:%d:\x1b[0m ",
        elapsed_ms, level_colors[ev->level], level_strings[ev->level],
        ev->file, ev->line);
#else
    fprintf(
        static_cast<FILE *>(ev->udata), "[%5ldms] %-5s %s:%d: ",
        elapsed_ms, level_strings[ev->level], ev->file, ev->line);
#endif
    vfprintf(static_cast<FILE *>(ev->udata), ev->fmt, ev->ap);
    fprintf(static_cast<FILE *>(ev->udata), "\n");
    fflush(static_cast<FILE *>(ev->udata));
}

static void lock() {
    if (L.lock) {
        L.lock(true, L.udata);
    }
}

static void unlock() {
    if (L.lock) {
        L.lock(false, L.udata);
    }
}

const char *log_level_string(const int level) {
    return level_strings[level];
}

void log_set_lock(const log_LockFn fn, void *udata) {
    L.lock = fn;
    L.udata = udata;
}

void log_set_level(const int level) {
    L.level = level;
}

void log_set_quiet(const bool enable) {
    L.quiet = enable;
}

int log_add_callback(const log_LogFn fn, void *udata, const int level) {
    for (auto &callback: L.callbacks) {
        if (!callback.fn) {
            callback = (Callback){fn, udata, level};
            return 0;
        }
    }
    return -1;
}

static void init_event(log_Event *ev, void *udata) {
    if (!ev->time) {
        const time_t t = time(nullptr);
        ev->time = localtime(&t);
    }
    ev->udata = udata;
}

void log_log(const int level, const char *file, const int line, const char *fmt, ...) {
    log_Event ev = {
        .fmt = fmt,
        .file = file,
        .line = line,
        .level = level,
    };
    lock();
    if (!L.quiet && level >= L.level) {
        init_event(&ev, stdout);
        va_start(ev.ap, fmt);
        stdout_callback(&ev);
        va_end(ev.ap);
    }
    for (int i = 0; i < MAX_CALLBACKS && L.callbacks[i].fn; i++) {
        if (const Callback *cb = &L.callbacks[i]; level >= cb->level) {
            init_event(&ev, cb->udata);
            va_start(ev.ap, fmt);
            cb->fn(&ev);
            va_end(ev.ap);
        }
    }
    unlock();
}
