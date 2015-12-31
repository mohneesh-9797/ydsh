/*
 * Copyright (C) 2015-2016 Nagisa Sekiguchi
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

#ifndef YDSH_LOGGER_H
#define YDSH_LOGGER_H

#include "../config.h"
#include "../misc/logger_base.hpp"

DEFINE_LOGGING_POLICY("YDSH_", "APPENDER", TRACE_TOKEN, DUMP_EXEC, TRACE_SIGNAL);

#endif //YDSH_LOGGER_H
