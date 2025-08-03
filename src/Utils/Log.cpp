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

static std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();

static int current_level = LOG_INFO;
static bool quiet_mode = false;

static const char *level_strings[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {"\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"};
#endif

const char *log_level_string(const int level) { return level_strings[level]; }

void log_set_level(const int level) { current_level = level; }

void log_set_quiet(const bool enable) { quiet_mode = enable; }

void log_log(const int level, const char *file, const int line, const char *fmt, ...) {
    if (quiet_mode || level < current_level) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();

#ifdef LOG_USE_COLOR
    fprintf(stdout, "[%5ldms] %s%-5s\x1b[0m \x1b[90m%s:%d:\x1b[0m ", static_cast<long>(elapsed_ms), level_colors[level],
            level_strings[level], file, line);
#else
    fprintf(stdout, "[%5ldms] %-5s %s:%d: ", static_cast<long>(elapsed_ms), level_strings[level], file, line);
#endif

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fprintf(stdout, "\n");
    fflush(stdout);
}

std::string log_string_format(const char *fmt, ...) {
    va_list args1;
    va_start(args1, fmt);

    va_list args2;
    va_copy(args2, args1);

    const int size = vsnprintf(nullptr, 0, fmt, args1);
    va_end(args1);

    if (size < 0) {
        return std::string{"Error formatting the log message."};
    }

    std::string result(size, '\0');
    vsnprintf(&result[0], size + 1, fmt, args2);
    va_end(args2);

    return result;
}
