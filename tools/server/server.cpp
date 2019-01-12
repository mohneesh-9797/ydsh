/*
 * Copyright (C) 2018-2019 Nagisa Sekiguchi
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

#include "server.h"

namespace ydsh {
namespace lsp {

// #######################
// ##     LSPServer     ##
// #######################

void LSPServer::bindAll() {
    auto voidIface = this->interface();

    this->bind("shutdown", voidIface, &LSPServer::shutdown);
    this->bind("exit", voidIface, &LSPServer::exit);

    auto clientCap = this->interface("ClientCapabilities", {
        field("workspace", any, false),
        field("textDocument", any, false)
    });
    this->bind("initialize",
            this->interface("InitializeParams", {
                field("processId", integer),
                field("rootPath", string | null ,false),
                field("rootUri", string | null),
                field("initializationOptions", any, false),
                field("capabilities", object(clientCap)),
                field("trace", string, false)
            }), &LSPServer::initialize
    );
}

void LSPServer::run() {
    while(true) {
        this->transport.dispatch(*this);
    }
}

Reply<ServerCapabilities> LSPServer::initialize(const ClientCapabilities &cap) {
    this->logger(LogLevel::INFO, "initialize server ....");
    this->init = true;

    (void) cap; //FIXME: currently not used

    ServerCapabilities scap;    //FIXME: set supported capabilites
    return scap;
}

Reply<void> LSPServer::shutdown() {
    this->logger(LogLevel::INFO, "try to shutdown ....");
    // currently do nothing.
    return nullptr;
}

void LSPServer::exit() {
    int s = 0;
    this->logger(LogLevel::INFO, "exit server: %d", s);
    std::exit(s);   // always success
}

} // namespace lsp
} // namespace ydsh