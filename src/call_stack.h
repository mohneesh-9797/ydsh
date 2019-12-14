/*
 * Copyright (C) 2019 Nagisa Sekiguchi
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

#ifndef YDSH_CALL_STACK_H
#define YDSH_CALL_STACK_H

#include "object.h"

namespace ydsh {

struct ControlFrame {
    /**
     * currently executed code
     */
    const DSCode *code{nullptr};

    /**
     * initial value is 0. increment index before push
     */
    unsigned int stackTopIndex{0};

    /**
     * indicate lower limit of stack top index (bottom <= top)
     */
    unsigned int stackBottomIndex{0};

    /**
     * offset of current local variable index.
     * initial value is equivalent to globalVarSize.
     */
    unsigned int localVarOffset{0};

    /**
     * indicate the index of currently evaluating op code.
     */
    unsigned int pc{0};

    /**
     * interpreter recursive depth
     */
    unsigned int recDepth{0};
};

class CallStack {
private:
    ControlFrame frame;
    std::vector<ControlFrame> frames;

    unsigned int operandsSize;

    /**
     * contains operand, global variable(may be function) or local variable.
     *
     * stack grow ==>
     * +--------+   +--------+-------+   +-------+
     * | gvar 1 | ~ | gvar N | var 1 | ~ | var N | ~
     * +--------+   +--------+-------+   +-------+
     * |   global variable   |   local variable  | operand stack
     *
     *
     * stack layout within callable
     *
     * stack grow ==>
     *   +-----------------+   +-----------------+-----------+   +-------+--------------+
     * ~ | var 1 (param 1) | ~ | var M (param M) | var M + 1 | ~ | var N | ~
     *   +-----------------+   +-----------------+-----------+   +-------+--------------+
     *   |                           local variable                      | operand stack
     */
    DSValue *operands;

    static constexpr unsigned int MAX_FRAME_SIZE = 2048;

public:
    CallStack() : operandsSize(64), operands(new DSValue[this->operandsSize]) {}

    ~CallStack() {
        delete[] this->operands;
    }

    // for stack manipulation op
    const DSValue &peek() const {
        return this->operands[this->frame.stackTopIndex];
    }

    const DSValue &peekByOffset(unsigned int offset) const {
        return this->operands[this->frame.stackTopIndex - offset];
    }

    void push(const DSValue &value) {
        this->push(DSValue(value));
    }

    void push(DSValue &&value) {
        this->operands[++this->frame.stackTopIndex] = std::move(value);
    }

    DSValue pop() {
        return std::move(this->operands[this->frame.stackTopIndex--]);
    }

    void popNoReturn() {
        this->operands[this->frame.stackTopIndex--].reset();
    }

    void dup() {
        auto v = this->peek();
        this->push(std::move(v));
    }

    void dup2() {
        auto v1 = this->peekByOffset(1);
        auto v2 = this->peek();
        this->push(std::move(v1));
        this->push(std::move(v2));
    }

    void swap() {
        this->operands[this->frame.stackTopIndex].swap(this->operands[this->frame.stackTopIndex - 1]);
    }

    void clearOperands() {
        while(this->frame.stackTopIndex > this->frame.stackBottomIndex) {
            this->popNoReturn();
        }
    }

    void reclaimLocals(unsigned char offset, unsigned char size) {
        auto *limit = this->operands + this->frame.localVarOffset + offset;
        auto *cur = limit + size - 1;
        while(cur >= limit) {
            (cur--)->reset();
        }
    }

    // for local variable access
    void setLocal(unsigned char index, const DSValue &obj) {
        this->setLocal(index, DSValue(obj));
    }

    void setLocal(unsigned char index, DSValue &&obj) {
        this->operands[this->frame.localVarOffset + index] = std::move(obj);
    }

    const DSValue &getLocal(unsigned char index) const {
        return this->operands[this->frame.localVarOffset + index];
    }

    DSValue moveLocal(unsigned char index) {
        return std::move(this->operands[this->frame.localVarOffset + index]);
    }

    void storeLocal(unsigned char index) {
        this->setLocal(index, this->pop());
    }

    void loadLocal(unsigned char index) {
        auto v = this->getLocal(index);
        this->push(std::move(v));
    }

    // for field access
    void storeField(unsigned int index) {
        auto value = this->pop();
        this->pop()->getFieldTable()[index] = std::move(value);
    }

    void loadField(unsigned int index) {
        this->operands[this->frame.stackTopIndex] =
                this->operands[this->frame.stackTopIndex]->getFieldTable()[index];
    }

    // for recursive depth count
    unsigned int recDepth() const {
        return this->frame.recDepth;
    }

    void incRecDepth() {
        this->frame.recDepth++;
    }

    void decRecDepth() {
        this->frame.recDepth--;
    }

    bool checkVMReturn() const {
        return this->frames.empty() || this->recDepth() != this->frames.back().recDepth;
    }

    const ControlFrame &getFrame() const {
        return this->frame;
    }

    const std::vector<ControlFrame> &getFrames() const {
        return this->frames;
    }

    const DSCode *code() const {
        return this->frame.code;
    }

    unsigned int &pc() noexcept {
        return this->frame.pc;
    }

    void reserve(unsigned int add) {
        unsigned int afterSize = this->frame.stackTopIndex + add;
        if(afterSize < this->operandsSize) {
            return;
        }
        this->resize(afterSize);
    }

    void reset() {
        this->frames.clear();
        this->frame.stackTopIndex = 0;
        this->frame.stackBottomIndex = 0;
        this->frame.localVarOffset = 0;
    }

    /**
     *
     * @param stackTopOffset
     * @param paramSize
     * @param code
     * @return
     * if current frames size is limit, return false.
     */
    bool wind(unsigned int stackTopOffset, unsigned int paramSize, const DSCode *code);

    void unwind();

private:
    void resize(unsigned int afterSize);
};

} // namespace ydsh

#endif //YDSH_CALL_STACK_H
