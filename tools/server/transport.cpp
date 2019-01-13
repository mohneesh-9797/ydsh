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

#include <misc/num.h>

#include "transport.h"

namespace ydsh {
namespace lsp {

static constexpr const char HEADER_LENGTH[] = "Content-Length: ";

// ##########################
// ##     LSPTransport     ##
// ##########################

int LSPTransport::send(unsigned int size, const char *data) {
    std::string header = HEADER_LENGTH;
    header += std::to_string(size);
    header += "\r\n";
    header += "\r\n";

    fwrite(header.c_str(), sizeof(char), header.size(), this->output.get());
    fwrite(data, sizeof(char), size, this->output.get());
    fflush(this->output.get());

    return 0;
}

static bool isContentLength(const std::string &line) {
    return strncmp(line.c_str(), HEADER_LENGTH, arraySize(HEADER_LENGTH) - 1) == 0 &&
        line.size() == strlen(line.c_str());
}

static int parseContentLength(const std::string &line) {
    const char *ptr = line.c_str();
    ptr += arraySize(HEADER_LENGTH) - 1;
    int s;
    long value = convertToInt64(ptr, s);
    if(s == 0 && value >= 0 && value <= INT32_MAX) {
        return value;
    }
    return 0;
}

int LSPTransport::recvSize() {
    int size = 0;
    while(true) {
        std::string header;
        if(!this->readHeader(header)) {
            return -1;
        }

        if(header.empty()) {
            break;
        }
        if(isContentLength(header)) {
            this->logger(LogLevel::INFO, "length header: %s", header.c_str());
            if(size > 0) {
                this->logger(LogLevel::WARNING, "previous read message length: %d", size);
            }
            int ret = parseContentLength(header);
            if(!ret) {
                this->logger(LogLevel::ERROR, "may be broken message or empty message");
            }
            size = ret;
        } else {    // may be other header
            this->logger(LogLevel::INFO, "other header: %s", header.c_str());
        }
    }
    return size;
}

int LSPTransport::recv(unsigned int size, char *data) {
    return fread(data, sizeof(char), size, this->input.get());
}

bool LSPTransport::readHeader(std::string &header) {
    char prev = '\0';
    while(true) {
        char ch;
        if(fread(&ch, 1, 1, this->input.get()) != 1) {
            if(ferror(this->input.get()) && (errno == EINTR || errno == EAGAIN)) {
                continue;
            }
            return false;
        }

        if(ch == '\n' && prev == '\r') {
            header.pop_back();  // pop \r
            break;
        }
        prev = ch;
        header += ch;
    }
    return true;
}

} // namespace lsp
} // namespace ydsh