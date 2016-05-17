/*
 * Copyright (C) 2016 Nagisa Sekiguchi
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

#ifndef YDSH_CORE_OPCODE_H
#define YDSH_CORE_OPCODE_H

namespace ydsh {
namespace core {

/**
 * see (doc/opcode.md)
 */
#define OPCODE_LIST(OP) \
    OP(NOP, 0) \
    OP(STOP_EVAL, 0) \
    OP(ASSERT, 0) \
    OP(PRINT, 8) \
    OP(INSTANCE_OF, 8) \
    OP(CHECK_CAST, 8) \
    OP(PUSH_TRUE, 0) \
    OP(PUSH_FALSE, 0) \
    OP(PUSH_ESTRING, 0) \
    OP(LOAD_CONST, 2) \
    OP(LOAD_FUNC, 2) \
    OP(LOAD_GLOBAL, 2) \
    OP(STORE_GLOBAL, 2) \
    OP(LOAD_LOCAL, 2) \
    OP(STORE_LOCAL, 2) \
    OP(LOAD_FIELD, 2) \
    OP(STORE_FIELD, 2) \
    OP(IMPORT_ENV, 1) \
    OP(LOAD_ENV, 0) \
    OP(STORE_ENV, 0) \
    OP(POP, 0) \
    OP(DUP, 0) \
    OP(DUP2, 0) \
    OP(SWAP, 0) \
    OP(NEW_STRING, 0) \
    OP(APPEND_STRING, 0) \
    OP(NEW_ARRAY, 8) \
    OP(APPEND_ARRAY, 0) \
    OP(NEW_MAP, 8) \
    OP(APPEND_MAP, 0) \
    OP(NEW_TUPLE, 8) \
    OP(NEW, 8) \
    OP(CALL_INIT, 2) \
    OP(CALL_METHOD, 4) \
    OP(CALL_FUNC, 2) \
    OP(RETURN, 0) \
    OP(RETURN_V, 0) \
    OP(BRANCH, 2) \
    OP(GOTO, 4) \
    OP(THROW, 0) \
    OP(ENTER_FINALLY, 4) \
    OP(EXIT_FINALLY, 0)

enum class OpCode : unsigned char {
#define GEN_OPCODE(CODE, N) CODE,
    OPCODE_LIST(GEN_OPCODE)
#undef GEN_OPCODE
};

} // namespace core
} // namespace ydsh

#endif //YDSH_CORE_OPCODE_H
