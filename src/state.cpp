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

#include "state.h"

namespace ydsh {

bool VMState::wind(unsigned int stackTopOffset, unsigned int paramSize, const DSCode * code) {
    const unsigned int maxVarSize = code->is(CodeKind::NATIVE) ? paramSize :
                                    static_cast<const CompiledCode *>(code)->getLocalVarNum();
    const unsigned int localVarOffset = this->frame.stackTopIndex - paramSize + 1;
    const unsigned int operandSize = code->is(CodeKind::NATIVE) ? 4 :
                                     static_cast<const CompiledCode *>(code)->getStackDepth();

    if(this->frames.size() == MAX_FRAME_SIZE) {
        return false;
    }

    // save current control frame
    this->frame.stackTopIndex -= stackTopOffset;
    this->frames.push_back(this->getFrame());
    this->frame.stackTopIndex += stackTopOffset;

    // reallocate stack
    this->reserve(maxVarSize - paramSize + operandSize);

    // prepare control frame
    this->frame.code = code;
    this->frame.stackTopIndex += maxVarSize - paramSize;
    this->frame.stackBottomIndex = this->frame.stackTopIndex;
    this->frame.localVarOffset = localVarOffset;
    this->frame.pc = code->getCodeOffset() - 1;
    return true;
}

void VMState::unwind() {
    auto savedFrame = this->frames.back();

    this->frame.code = savedFrame.code;
    this->frame.stackBottomIndex = savedFrame.stackBottomIndex;
    this->frame.localVarOffset = savedFrame.localVarOffset;
    this->frame.pc = savedFrame.pc;

    unsigned int oldStackTopIndex = savedFrame.stackTopIndex;
    while(this->frame.stackTopIndex > oldStackTopIndex) {
        this->popNoReturn();
    }
    this->frames.pop_back();
}

void VMState::resize(unsigned int afterSize) {
    unsigned int newSize = this->operandsSize;
    do {
        newSize += (newSize >> 1u);
    } while(newSize < afterSize);

    auto newStack = new DSValue[newSize];
    for(unsigned int i = 0; i < this->frame.stackTopIndex + 1; i++) {
        newStack[i] = std::move(this->operands[i]);
    }
    delete[] this->operands;
    this->operands = newStack;
    this->operandsSize = newSize;
}

std::vector<StackTraceElement> VMState::createStackTrace() const {
    std::vector<StackTraceElement> stackTrace;
    auto curFrame = this->frame;
    for(unsigned int callDepth = this->frames.size(); callDepth > 0; curFrame = frames[--callDepth]) {
        auto &callable = curFrame.code;
        if(!callable->is(CodeKind::NATIVE)) {
            const auto *cc = static_cast<const CompiledCode *>(callable);

            // create stack trace element
            const char *sourceName = cc->getSourceName();
            unsigned int lineNum = cc->getLineNum(curFrame.pc);

            std::string callableName;
            switch(callable->getKind()) {
            case CodeKind::TOPLEVEL:
                callableName += "<toplevel>";
                break;
            case CodeKind::FUNCTION:
                callableName += "function ";
                callableName += cc->getName();
                break;
            case CodeKind::USER_DEFINED_CMD:
                callableName += "command ";
                callableName += cc->getName();
                break;
            default:
                break;
            }
            stackTrace.emplace_back(sourceName, lineNum, std::move(callableName));
        }
    }
    return stackTrace;
}

} // namespace ydsh