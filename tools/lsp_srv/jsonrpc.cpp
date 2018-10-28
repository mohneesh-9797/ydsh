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

#include "jsonrpc.h"

namespace rpc {

JSON newError(int code, const char *message, JSON &&data) {
    return {
            {"code", code},
            {"message", message},
            {"data", std::move(data)}
    };
}

JSON newError(int code, const char *message) {
    return {
            {"code", code},
            {"message", message},
    };
}

} // namespace rpc