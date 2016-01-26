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

#include <sys/stat.h>

#include <libxml/SAX2.h>

#include <iostream>
#include <fstream>
#include <list>
#include <unordered_set>
#include <cassert>

#include <misc/argv.hpp>
#include <misc/fatal.h>

namespace {

class Config {
private:
    /**
     * if true, use system bus.
     * if false, use session bus.
     * default is false(session bus).
     */
    bool systemBus;

    /**
     * if true, search child object.
     * default is false.
     */
    bool recusive;

    /**
     * destination service. must be well known name.
     * default is org.freedesktop.DBus
     */
    std::string dest;

    /**
     * target object path list.
     * must not be empty
     */
    std::vector<std::string> pathList;

    /**
     * introspection xml file.
     * if this is not empty, ignore pathList.
     */
    std::string xmlFileName;

    /**
     * output directory for generated interface file.
     */
    std::string outputDir;

    /**
     * if generated interface files have already existed, overwrite file.
     * default is false
     */
    bool overwrite;

    /**
     * for currently processing object path.
     */
    std::string currentPath;

    /**
     * if true, generate standard interface.
     */
    bool allowStd;

public:
    Config() : systemBus(false), recusive(false),
               dest("org.freedekstop.DBus"), pathList(), xmlFileName(),
               outputDir(), overwrite(false), currentPath(), allowStd(false) {}

    ~Config() = default;

    void setSystemBus(bool systemBus) {
        this->systemBus = systemBus;
    }

    bool isSystemBus() const {
        return this->systemBus;
    }

    void setRecursive(bool recursive) {
        this->recusive = recursive;
    }

    bool isRecursive() const {
        return this->recusive;
    }

    void setDest(const char *dest) {
        this->dest = dest;
    }

    const std::string &getDest() const {
        return this->dest;
    }

    void addPath(const char *path) {
        this->pathList.push_back(path);
    }

    const std::vector<std::string> &getPathList() const {
        return this->pathList;
    }

    void setXmlFileName(const char *fileName) {
        this->xmlFileName = fileName;
    }

    const std::string &getXmlFileName() const {
        return this->xmlFileName;
    }

    void setOutputDir(const char *outputDir) {
        this->outputDir = outputDir;
        if(this->outputDir[this->outputDir.size() - 1] == '/') {
            this->outputDir.erase(this->outputDir.size() - 1);
        }
    }

    const std::string &getOutputDir() const {
        return this->outputDir;
    }

    void setOverwrite(bool overwrite) {
        this->overwrite = overwrite;
    }

    bool isOverwrite() const {
        return this->overwrite;
    }

    void setCurrentPath(std::string &&path) {
        this->currentPath = std::move(path);
    }

    const std::string &getCurrentPath() const {
        return this->currentPath;
    }

    void setAllowStd(bool allowStd) {
        this->allowStd = allowStd;
    }

    bool isAllowStd() const {
        return this->allowStd;
    }
};

#define STR(x) "`" #x "'"
#define check(expr) do { if(!(expr)) { fatal("assertion fail => %s\n", STR(expr)); } } while(0)

/**
 * xmlChar is actually unsigned char(UTF8)
 */
static std::string str(const xmlChar *str) {
    return std::string((char *)str);
}

static bool existFile(const std::string &fileName) {
    struct stat st;
    int result = stat(fileName.c_str(), &st);
    return result == 0;
}

class MethodBuilder {
private:
    std::string methodName;
    std::vector<std::string> args;
    std::vector<std::string> returnTypes;

public:
    MethodBuilder() = default;
    ~MethodBuilder() = default;

    void write(FILE *fp) {
        fprintf(fp, "    function %s(", this->methodName.c_str());

        // write arg
        unsigned int size = this->args.size();
        for(unsigned int i = 0; i < size; i++) {
            if(i > 0) {
                fputs(", ", fp);
            }
            fputs(this->args[i].c_str(), fp);
        }

        fputs(")", fp);

        // write return type
        size = this->returnTypes.size();
        for(unsigned int i = 0; i < size; i++) {
            if(i == 0) {
                fputs(" : ", fp);
            } else {
                fputs(", ", fp);
            }
            fputs(this->returnTypes[i].c_str(), fp);
        }

        fputs("\n", fp);
    }

    void clear() {
        this->methodName.clear();
        this->args.clear();
        this->returnTypes.clear();
    }

    void setMethodName(std::string &&methodName) {
        this->methodName = std::move(methodName);
    }

    void appendArg(std::string &&argName, const std::string &argTypeName) {
        this->args.push_back(std::move(argName));
        this->args.back() += " : ";
        this->args.back() += argTypeName;
    }

    void appendReturnType(const std::string &typeName) {
        this->returnTypes.push_back(typeName);
    }

    bool empty() const {
        return this->methodName.empty() && this->args.empty() && this->returnTypes.empty();
    }
};

class SignalBuilder {
private:
    std::string signalName;
    std::vector<std::string> argTypes;

public:
    SignalBuilder() = default;
    ~SignalBuilder() = default;

    void write(FILE *fp) {
        fprintf(fp, "    function %s($hd : Func<Void", this->signalName.c_str());

        // write arg type
        unsigned int size = this->argTypes.size();
        for(unsigned int i = 0; i < size; i++) {
            if(i == 0) {
                fputs(",[", fp);
            } else {
                fputs(",", fp);
            }
            fputs(this->argTypes[i].c_str(), fp);

            if(i == size - 1) {
                fputs("]", fp);
            }
        }

        fputs(">)\n", fp);
    }

    void clear() {
        this->signalName.clear();
        this->argTypes.clear();
    }

    void setSignalName(std::string &&name) {
        this->signalName = std::move(name);
    }

    void appendArgType(const std::string &type) {
        this->argTypes.push_back(type);
    }

    bool empty() const {
        return this->signalName.empty() && this->argTypes.empty();
    }
};


struct SAXHandlerContext {
    const Config &config;
    std::list<std::string> &pathList;

    unsigned int nodeCount;
    FILE *fp;
    MethodBuilder mBuilder;
    SignalBuilder sBuilder;
    std::unordered_set<std::string> foundIfaceSet;

    SAXHandlerContext(const Config &config, std::list<std::string> &pathList) :
            config(config), pathList(pathList), nodeCount(0), fp(nullptr),
            mBuilder(), sBuilder(), foundIfaceSet() {
        if(!this->config.isAllowStd()) {
            this->generatedPreviously("org.freedesktop.DBus.Peer");
            this->generatedPreviously("org.freedesktop.DBus.Introspectable");
            this->generatedPreviously("org.freedesktop.DBus.Properties");
            this->generatedPreviously("org.freedesktop.DBus.ObjectManager");
        }
    }

    ~SAXHandlerContext() = default;

    bool generatedPreviously(const std::string &ifaceName) {
        return !this->foundIfaceSet.insert(ifaceName).second;
    }
};

static void decodeImpl(const std::string &desc, unsigned int &index, std::string &out) {
    assert(index < desc.size());

    char ch = desc[index++];
    switch(ch) {
    case 'y':
        out += "Byte";
        break;
    case 'b':
        out += "Boolean";
        break;
    case 'n':
        out += "Int16";
        break;
    case 'q':
        out += "Uint16";
        break;
    case 'i':
        out += "Int32";
        break;
    case 'u':
        out += "Uint32";
        break;
    case 'x':
        out += "Int64";
        break;
    case 't':
        out += "Uint64";
        break;
    case 'd':
        out += "Float";
        break;
    case 's':
        out += "String";
        break;
    case 'o':
        out += "ObjectPath";
        break;
    case 'a': {
        if(index < desc.size() && desc[index] == '{') { // as Map
            out += "Map<";
            index++; // comsume '{'
            decodeImpl(desc, index, out);
            out += ",";
            decodeImpl(desc, index, out);
            index++;    // consume '}'
            out += ">";
        } else {    // as array
            out += "Array<";
            decodeImpl(desc, index, out);
            out += ">";
        }
        break;
    }
    case '(': {
        out += "Tuple<";
        unsigned int count = 0;
        do {
            if(count++ > 0) {
                out += ",";
            }
            decodeImpl(desc, index, out);
        } while(desc[index] != ')');
        out += ">";
        break;
    }
    case 'v':
        out += "Variant";
        break;
    case 'h':
        // currently not supported
    default:
        fatal("unsupported type signature: %c, %s\n", ch, desc.c_str());
        break;
    }
}

static std::string decode(const std::string &desc) {
    std::string str;
    unsigned int index = 0;
    decodeImpl(desc, index, str);
    assert(index == desc.size());
    return str;
}

class AttributeMap {
private:
    ydsh::misc::CStringHashMap<std::string> map;

public:
    AttributeMap(int nb_attributes, const xmlChar **attributes) : map() {
        for(int i = 0; i < nb_attributes; i++) {
            int index = i * 5;
            const char *name = (const char *)attributes[index];
            std::string value(attributes[index + 3], attributes[index + 4]);
            if(!this->map.insert(std::make_pair(name, std::move(value))).second) {
                fatal("duplicated attribute: %s\n", name);
            }
        }
    }

    ~AttributeMap() = default;

    const std::string &getAttr(const char *name) {
        auto iter = this->map.find(name);
        if(iter == this->map.end()) {
            fatal("not found attribute: %s\n", name);
        }
        return iter->second;
    }
};



/**
 * ctx is actually
 */
static void handler_startElementNs(void *ctx, const xmlChar *localname,
                                   const xmlChar *, const xmlChar *,
                                   int, const xmlChar **,
                                   int nb_attributes, int, const xmlChar **attributes) {
    SAXHandlerContext *hctx = reinterpret_cast<SAXHandlerContext *>(ctx);

    std::string elementName(str(localname));
    AttributeMap attrMap(nb_attributes, attributes);

    if(elementName == "node" && hctx->nodeCount++ != 0) {
        if(hctx->config.isRecursive()) {
            check(nb_attributes == 1);
            std::string nodeName(hctx->config.getCurrentPath());
            nodeName += "/";
            nodeName += attrMap.getAttr("name");
            hctx->pathList.push_back(std::move(nodeName));
        }
    } else if(elementName == "interface") {
        check(nb_attributes == 1);
        const std::string &ifaceName(attrMap.getAttr("name"));

        if(hctx->generatedPreviously(ifaceName)) {
            hctx->fp = fopen("/dev/null", "w");
            if(hctx->fp == nullptr) {
                std::cerr << "cannot open file: /dev/null" << strerror(errno) << std::endl;
                exit(1);
            }
        } else if(hctx->config.getOutputDir().empty()) {
            hctx->fp = stdout;
        } else {
            std::string fileName(hctx->config.getOutputDir());
            fileName += '/';
            fileName += ifaceName;

            // open target file
            const char *fileNameStr = fileName.c_str();
            if(!hctx->config.isOverwrite() && existFile(fileName)) {    // check file existence
                fileNameStr = "/dev/null";
            }
            hctx->fp = fopen(fileNameStr, "w");
            if(hctx->fp == nullptr) {
                std::cerr << "cannot open file: " << fileNameStr << strerror(errno) << std::endl;
                exit(1);
            }
        }
        // write
        fprintf(hctx->fp, "interface %s {\n", ifaceName.c_str());
    } else if(elementName == "method") {
        check(nb_attributes == 1);
        hctx->mBuilder.setMethodName(std::string(attrMap.getAttr("name")));
    } else if(elementName == "signal") {
        check(nb_attributes == 1);
        hctx->sBuilder.setSignalName(std::string(attrMap.getAttr("name")));
    } else if(elementName == "property") {
        if(attrMap.getAttr("access").find("write") != std::string::npos) {
            fprintf(hctx->fp, "    var ");
        } else {
            fprintf(hctx->fp, "    let ");
        }
        fputs(attrMap.getAttr("name").c_str(), hctx->fp);
        fputs(" : ", hctx->fp);

        fputs(decode(attrMap.getAttr("type")).c_str(), hctx->fp);
        fputs("\n", hctx->fp);
    } else if(elementName == "arg") {
        std::string type = decode(attrMap.getAttr("type"));
        if(!hctx->mBuilder.empty()) {
            if(attrMap.getAttr("direction") == "in") {
                std::string argName("$");
                argName += attrMap.getAttr("name");
                hctx->mBuilder.appendArg(std::move(argName), type);
            } else {
                hctx->mBuilder.appendReturnType(type);
            }
        } else if(!hctx->sBuilder.empty()) {
            hctx->sBuilder.appendArgType(type);
        } else {
            fatal("broken arg\n");
        }
    }
}

/**
 * ctx is actually
 */
static void handler_endElementNs(void *ctx, const xmlChar* localname, const xmlChar*, const xmlChar*) {
    SAXHandlerContext *hctx = reinterpret_cast<SAXHandlerContext *>(ctx);

    std::string elementName(str(localname));
    if(elementName == "node") {
        hctx->nodeCount--;
    } else if(elementName == "interface") {
        fprintf(hctx->fp, "}\n\n");
        fflush(hctx->fp);
        if(!hctx->config.getOutputDir().empty()) {
            fclose(hctx->fp);
        }
    } else if(elementName == "method") {
        hctx->mBuilder.write(hctx->fp);
        hctx->mBuilder.clear();
    } else if(elementName == "signal") {
        hctx->sBuilder.write(hctx->fp);
        hctx->sBuilder.clear();
    }
}


static void handler_characters(void *, const xmlChar *, int) {  // do nothing
}

static std::string introspect(const Config &config) {
    std::string cmd("dbus-send --print-reply=literal");
    cmd += (config.isSystemBus() ? " --system" : " --session");
    cmd += " --dest=";
    cmd += config.getDest();
    cmd += " ";
    cmd += config.getCurrentPath();
    cmd += " org.freedesktop.DBus.Introspectable.Introspect";

    FILE *fp = popen(cmd.c_str(), "r");
    if(fp == nullptr) {
        perror("dbus-send execution failed\n");
        exit(1);
    }

    std::string msg;

    const unsigned int size = 256;
    char buf[size + 1];
    int readSize;
    while((readSize = fread(buf, sizeof(char), size, fp)) > 0) {
        buf[readSize] = '\0';
        msg += buf;
    }

    pclose(fp);

    return msg;
}

static void parse(SAXHandlerContext &handlerCtx, xmlSAXHandler &saxHander, const std::string &xmlStr) {
    xmlParserCtxtPtr ctxt =
            xmlCreatePushParserCtxt(&saxHander, &handlerCtx, nullptr, 0, "source");
    int s = xmlParseChunk(ctxt, xmlStr.c_str(), xmlStr.size(), 1);
    if(s != 0) {
        xmlParserError(ctxt, "xml parse error");
        exit(1);
    }

    xmlFreeParserCtxt(ctxt);
    xmlCleanupParser();
}

static void generateIface(Config &config) {
    std::list<std::string> pathList;
    SAXHandlerContext handlerCtx(config, pathList);

    // init sax handler
    xmlSAXHandler saxHander;
    memset(&saxHander, 0, sizeof(xmlSAXHandler));

    saxHander.initialized = XML_SAX2_MAGIC;
    saxHander.startElementNs = handler_startElementNs;
    saxHander.endElementNs = handler_endElementNs;
    saxHander.characters = handler_characters;

    // for specifying xml file
    if(!config.getXmlFileName().empty()) {
        FILE *fp = fopen(config.getXmlFileName().c_str(), "r");
        if(fp == nullptr) {
            fprintf(stderr, "%s: %s\n", config.getXmlFileName().c_str(), strerror(errno));
            exit(1);
        }
        unsigned int size = 256;
        char buf[size + 1];
        int readSize;
        std::string xmlStr;
        while((readSize = fread(buf, sizeof(char), size, fp)) > 0) {
            buf[readSize] = '\0';
            xmlStr += buf;
        }
        if(xmlStr.empty()) {
            fprintf(stderr, "broken xml\n");
            exit(1);
        }
        fclose(fp);
        parse(handlerCtx, saxHander, xmlStr);
        return;
    }


    // init path list
    for(auto &e : config.getPathList()) {
        pathList.push_back(e);
    }

    while(!pathList.empty()) {
        config.setCurrentPath(std::move(pathList.front()));
        pathList.pop_front();

        std::string xmlStr(introspect(config));
        if(xmlStr.empty()) {
            fprintf(stderr, "broken xml\n");
            exit(1);
        }
        parse(handlerCtx, saxHander, xmlStr);
    }
}

} // namespace

#define EACH_OPT(OP) \
    OP(SYSTEM_BUS,  "--system",    0,       "use system bus") \
    OP(SESSION_BUS, "--session",   0,       "use session bus (default)") \
    OP(RECURSIVE,   "--recursive", 0,       "search interface from child object") \
    OP(DEST,        "--dest",      HAS_ARG, "destination service name (must be well-known name)") \
    OP(OUTPUT,      "--output",    HAS_ARG, "specify output directory (default is standard output)") \
    OP(OVERWRITE,   "--overwrite", 0,       "if generated interface files have already existed, overwrite them") \
    OP(HELP,        "--help",      0,       "show this help message") \
    OP(XML_FILE,    "--xml",       HAS_ARG | IGNORE_REST, "generate interface from specified xml. no introspection.(ignore some option)") \
    OP(ALLOW_STD,   "--allow-std", 0,       "allow standard D-Bus interface generation")

enum class OptionSet : unsigned int {
#define GEN_ENUM(E, S, F, D) E,
    EACH_OPT(GEN_ENUM)
#undef GEN_ENUM
};

int main(int argc, char **argv) {
    using namespace ydsh::argv;

    static const Option<OptionSet> options[] = {
#define GEN_OPT(E, S, F, D) {OptionSet::E, S, (F), D},
            EACH_OPT(GEN_OPT)
#undef GEN_OPT
    };

    std::vector<std::pair<OptionSet, const char *>> cmdLines;
    int restIndex = argc;
    try {
        restIndex = ydsh::argv::parseArgv(argc, argv, options, cmdLines);
    } catch(const ParseError &e) {
        std::cerr << e.getMessage() << std::endl;
        std::cerr << options << std::endl;
        return 1;
    }

    // set option
    Config config;
    bool foundDest = false;
    bool foundXml = false;

    for(auto &cmdLine : cmdLines) {
        switch(cmdLine.first) {
        case OptionSet::SYSTEM_BUS:
            config.setSystemBus(true);
            break;
        case OptionSet::SESSION_BUS:
            config.setSystemBus(false);
            break;
        case OptionSet::RECURSIVE:
            config.setRecursive(true);
            break;
        case OptionSet::DEST:
            config.setDest(cmdLine.second);
            foundDest = true;
            break;
        case OptionSet::OUTPUT:
            config.setOutputDir(cmdLine.second);
            break;
        case OptionSet::OVERWRITE:
            config.setRecursive(true);
            break;
        case OptionSet::HELP:
            std::cout << options << std::endl;
            return 0;
        case OptionSet::XML_FILE:
            config.setXmlFileName(cmdLine.second);
            foundXml = true;
            break;
        case OptionSet::ALLOW_STD:
            config.setAllowStd(true);
            break;
        }
    }

    if(!foundDest && !foundXml) {
        std::cerr << "require --dest [destination name]" << std::endl;
        std::cout << options << std::endl;
        return 1;
    }

    const int size = argc - restIndex;
    if(size == 0 && !foundXml) {
        std::cerr << "require atleast one object path" << std::endl;
        std::cout << options << std::endl;
        return 1;
    }
    
    for(int i = restIndex; i < argc; i++) {
        config.addPath(argv[i]);
    }

    generateIface(config);

    return 0;
}