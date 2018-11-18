/*
 * Copyright (C) 2018 Nagisa Sekiguchi
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef YDSH_LOGGER_BASE_HPP
#define YDSH_LOGGER_BASE_HPP

#include <unistd.h>

#include <cstring>
#include <string>
#include <ctime>
#include <cstdarg>
#include <mutex>

#include "resource.hpp"
#include "util.hpp"
#include "fatal.h"

namespace ydsh {

namespace __detail_logger {

template<bool T>
class LoggerBase {
public:
    static_assert(T, "not allowed instantiation");

    enum Severity : unsigned int {
        INFO, WARNING, ERROR, FATAL,
    };

protected:
    std::string prefix;
    FILE *fp{nullptr};
    unsigned int severity{FATAL};
    std::mutex outMutex;

    explicit LoggerBase(const char *prefix) : prefix(prefix) {
        this->syncSetting();
    }

    ~LoggerBase() {
        if(this->fp && this->fp != stderr) {
            fclose(this->fp);
        }
    }

    void log(Severity level, const char *fmt, va_list list);

public:
    const char *get(Severity severity) const {
        const char *str[] = {
                "info", "warning", "error", "fatal"
        };
        return str[severity];
    }

    void syncSetting();

    void operator()(Severity level, const char *fmt, ...) __attribute__ ((format(printf, 3, 4))) {
        va_list arg;
        va_start(arg, fmt);
        this->log(level, fmt, arg);
        va_end(arg);
    }

    bool enabled(Severity level) const {
        return level >= this->severity;
    }
};

template <bool T>
void LoggerBase<T>::log(Severity level, const char *fmt, va_list list) {
    if(!this->enabled(level)) {
        return;
    }

    // create body
    char *str = nullptr;
    if(vasprintf(&str, fmt, list) == -1) {
        fatal_perror("");
    }

    // create header
    char header[128];
    header[0] = '\0';

    time_t timer = time(nullptr);
    struct tm local{};
    tzset();
    if(localtime_r(&timer, &local)) {
        char buf[32];
        strftime(buf, arraySize(buf), "%F %T", &local);
        snprintf(header, arraySize(header), "%s [%d] [%s] ", buf, getpid(), this->get(level));
    }

    // print body
    fprintf(this->fp, "%s%s\n", header, str);
    fflush(this->fp);
    free(str);

    if(level == FATAL) {
        abort();
    }
}

template <bool T>
void LoggerBase<T>::syncSetting() {
    std::lock_guard<std::mutex> guard(this->outMutex);

    std::string key = this->prefix;
    key += "_LEVEL";
    const char *level = getenv(key.c_str());
    if(level != nullptr) {
        for(auto i = static_cast<unsigned int>(INFO); i < static_cast<unsigned int>(FATAL); i++) {
            auto s = static_cast<Severity>(i);
            if(strcasecmp(this->get(s), level) == 0) {
                this->severity = s;
                break;
            }
        }
    }

    key = this->prefix;
    key += "_APPENDER";
    const char *appender = getenv(key.c_str());
    if(appender != nullptr && strlen(appender) != 0) {
        if(this->fp && this->fp != stderr) {
            fclose(this->fp);
            this->fp = nullptr;
        }
        this->fp = fopen(appender, "w");
    }
    if(!this->fp) {
        this->fp = stderr;
    }
}

} // namespace __detail_logger

using LoggerBase = __detail_logger::LoggerBase<true>;

template <typename T>
class SingletonLogger : public LoggerBase, Singleton<T> {
protected:
    explicit SingletonLogger(const char *prefix) : LoggerBase(prefix) {}

public:
    static void Info(const char *fmt, ...) __attribute__ ((format(printf, 1, 2))) {
        va_list arg;
        va_start(arg, fmt);
        Singleton<T>::instance().log(INFO, fmt, arg);
        va_end(arg);
    }

    static void Warning(const char *fmt, ...) __attribute__ ((format(printf, 1, 2))) {
        va_list arg;
        va_start(arg, fmt);
        Singleton<T>::instance().log(WARNING, fmt, arg);
        va_end(arg);
    }

    static void Error(const char *fmt, ...) __attribute__ ((format(printf, 1, 2))) {
        va_list arg;
        va_start(arg, fmt);
        Singleton<T>::instance().log(ERROR, fmt, arg);
        va_end(arg);
    }

    static void Fatal(const char *fmt, ...) __attribute__ ((format(printf, 1, 2))) {
        va_list arg;
        va_start(arg, fmt);
        Singleton<T>::instance().log(FATAL, fmt, arg);
        va_end(arg);
    }
};

} // namespace ydsh


#endif //YDSH_LOGGER_BASE_HPP