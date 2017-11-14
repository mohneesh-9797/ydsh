/*
 * Copyright (C) 2015-2017 Nagisa Sekiguchi
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

#include <cstdarg>
#include <array>

#include "type.h"
#include "object.h"
#include "handle.h"
#include "diagnosis.h"
#include "parser.h"
#include "type_checker.h"
#include "core.h"
#include "symbol.h"
#include "misc/util.hpp"

namespace ydsh {

template <std::size_t N>
std::array<NativeCode, N> initNative(const NativeFuncInfo (&e)[N]) {
    std::array<NativeCode, N> array;
    for(unsigned int i = 0; i < N; i++) {
        const char *funcName = e[i].funcName;
        if(funcName != nullptr && strcmp(funcName, "waitSignal") == 0) {
            array[i] = createWaitSignalCode();
        } else {
            array[i] = NativeCode(e[i].func_ptr, static_cast<HandleInfo>(e[i].handleInfo[0]) != HandleInfo::Void);
        }
    }
    return array;
}

} // namespace ydsh

#include "bind.h"

namespace ydsh {

// ####################
// ##     DSType     ##
// ####################

MethodHandle *DSType::getConstructorHandle(TypePool &) {
    return nullptr;
}

const DSCode *DSType::getConstructor() {
    return nullptr;
}

unsigned int DSType::getFieldSize() {
    return this->superType != nullptr ? this->superType->getFieldSize() : 0;
}

unsigned int DSType::getMethodSize() {
    return this->superType != nullptr ? this->superType->getMethodSize() : 0;
}

FieldHandle *DSType::lookupFieldHandle(TypePool &, const std::string &) {
    return nullptr;
}

MethodHandle *DSType::lookupMethodHandle(TypePool &, const std::string &) {
    return nullptr;
}

bool DSType::isSameOrBaseTypeOf(const DSType &targetType) const {
    if(*this == targetType) {
        return true;
    }
    if(targetType.isNothingType()) {
        return true;
    }
    if(this->isOptionType()) {
        return static_cast<const ReifiedType *>(this)->getElementTypes()[0]->isSameOrBaseTypeOf(targetType);
    }
    DSType *superType = targetType.getSuperType();
    return superType != nullptr && this->isSameOrBaseTypeOf(*superType);
}

const DSCode *DSType::getMethodRef(unsigned int methodIndex) {
    return this->superType != nullptr ? this->superType->getMethodRef(methodIndex) : nullptr;
}

void DSType::copyAllMethodRef(std::vector<const DSCode *> &) {
}

// ##########################
// ##     FunctionType     ##
// ##########################

MethodHandle *FunctionType::lookupMethodHandle(TypePool &typePool, const std::string &methodName) {
    return this->superType->lookupMethodHandle(typePool, methodName);
}

FieldHandle *FunctionType::findHandle(const std::string &fieldName) {
    return this->superType->findHandle(fieldName);
}

void FunctionType::accept(TypeVisitor *visitor) {
    visitor->visitFunctionType(this);
}

// ################################
// ##     native_type_info_t     ##
// ################################

NativeFuncInfo &native_type_info_t::getMethodInfo(unsigned int index) {
    return nativeFuncInfoTable[this->offset + this->constructorSize + index];
}

/**
 * not call it if constructorSize is 0
 */
NativeFuncInfo &native_type_info_t::getInitInfo() {
    return nativeFuncInfoTable[this->offset];
}

static const NativeCode *getCode(native_type_info_t info, unsigned int index) {
    return getNativeCode(info.offset + info.constructorSize + index);
}

static const NativeCode *getCode(native_type_info_t info) {
    return getNativeCode(info.offset);
}


// #########################
// ##     BuiltinType     ##
// #########################

BuiltinType::BuiltinType(DSType *superType, native_type_info_t info, flag8_set_t attribute) :
        DSType(superType, attribute),
        info(info), constructorHandle(), constructor(),
        methodTable(superType != nullptr ? superType->getMethodSize() + info.methodSize : info.methodSize) {

    // copy super type methodRef to method table
    if(this->superType != nullptr) {
        this->superType->copyAllMethodRef(this->methodTable);
    }

    // init method handle
    unsigned int baseIndex = superType != nullptr ? superType->getMethodSize() : 0;
    for(unsigned int i = 0; i < info.methodSize; i++) {
        NativeFuncInfo *funcInfo = &info.getMethodInfo(i);
        unsigned int methodIndex = baseIndex + i;
        auto *handle = new MethodHandle(methodIndex);
        this->methodHandleMap.insert(std::make_pair(std::string(funcInfo->funcName), handle));

        // set to method table
        this->methodTable[methodIndex] = getCode(this->info, i);
    }
}

BuiltinType::~BuiltinType() {
    delete this->constructorHandle;

    for(auto &pair : this->methodHandleMap) {
        delete pair.second;
    }
}

MethodHandle *BuiltinType::getConstructorHandle(TypePool &typePool) {
    if(this->constructorHandle == nullptr && this->info.constructorSize != 0) {
        this->constructorHandle = new MethodHandle(0);
        if(!this->initMethodHandle(this->constructorHandle, typePool, this->info.getInitInfo())) {
            return nullptr;
        }
        this->constructor = getCode(this->info);
    }
    return this->constructorHandle;
}

const DSCode *BuiltinType::getConstructor() {
    return this->constructor;
}

MethodHandle *BuiltinType::lookupMethodHandle(TypePool &typePool, const std::string &methodName) {
    auto iter = this->methodHandleMap.find(methodName);
    if(iter == this->methodHandleMap.end()) {
        return this->superType != nullptr ? this->superType->lookupMethodHandle(typePool, methodName) : nullptr;
    }

    MethodHandle *handle = iter->second;
    if(!handle->initialized()) { // init handle
        unsigned int baseIndex = this->superType != nullptr ? this->superType->getMethodSize() : 0;
        unsigned int infoIndex = handle->getMethodIndex() - baseIndex;
        if(!this->initMethodHandle(handle, typePool, this->info.getMethodInfo(infoIndex))) {
            return nullptr;
        }
    }
    return handle;
}

FieldHandle *BuiltinType::findHandle(const std::string &fieldName) { // override
//    auto iter = this->methodHandleMap.find(fieldName);
//    if(iter != this->methodHandleMap.end()) {
//        return iter->second;
//    }
    return this->superType != nullptr ? this->superType->findHandle(fieldName) : nullptr;
}

void BuiltinType::accept(TypeVisitor *visitor) {
    visitor->visitBuiltinType(this);
}

unsigned int BuiltinType::getMethodSize() {
    if(this->superType != nullptr) {
        return this->superType->getMethodSize() + this->methodHandleMap.size();
    }
    return this->methodHandleMap.size();
}

const DSCode *BuiltinType::getMethodRef(unsigned int methodIndex) {
    return this->methodTable[methodIndex];
}

void BuiltinType::copyAllMethodRef(std::vector<const DSCode *> &methodTable) {
    unsigned int size = this->getMethodSize();
    assert(size <= methodTable.size());

    for(unsigned int i = 0; i < size; i++) {
        methodTable[i] = this->methodTable[i];
    }
}

bool BuiltinType::initMethodHandle(MethodHandle *handle, TypePool &typePool, NativeFuncInfo &info) {
    return handle->init(typePool, info);
}

// #########################
// ##     ReifiedType     ##
// #########################

bool ReifiedType::initMethodHandle(MethodHandle *handle, TypePool &typePool, NativeFuncInfo &info) {
    return handle->init(typePool, info, &this->elementTypes);
}

void ReifiedType::accept(TypeVisitor *visitor) {
    visitor->visitReifiedType(this);
}

// #######################
// ##     TupleType     ##
// #######################

TupleType::TupleType(native_type_info_t info, DSType *superType, std::vector<DSType *> &&types) :
        ReifiedType(info, superType, std::move(types)) {
    const unsigned int size = this->elementTypes.size();
    const unsigned int baseIndex = this->superType->getFieldSize();
    for(unsigned int i = 0; i < size; i++) {
        FieldHandle *handle = new FieldHandle(this->elementTypes[i], i + baseIndex, FieldAttributes());
        this->fieldHandleMap.insert(std::make_pair("_" + std::to_string(i), handle));
    }
}

TupleType::~TupleType() {
    for(auto pair : this->fieldHandleMap) {
        delete pair.second;
    }
}

unsigned int TupleType::getFieldSize() {
    return this->elementTypes.size();
}

FieldHandle *TupleType::lookupFieldHandle(TypePool &typePool, const std::string &fieldName) {
    auto iter = this->fieldHandleMap.find(fieldName);
    if(iter == this->fieldHandleMap.end()) {
        return this->superType->lookupFieldHandle(typePool, fieldName);
    }
    return iter->second;
}

FieldHandle *TupleType::findHandle(const std::string &fieldName) {
    auto iter = this->fieldHandleMap.find(fieldName);
    if(iter == this->fieldHandleMap.end()) {
        return this->superType->findHandle(fieldName);
    }
    return iter->second;
}

void TupleType::accept(TypeVisitor *visitor) {
    visitor->visitTupleType(this);
}


// ###########################
// ##     InterfaceType     ##
// ###########################

InterfaceType::~InterfaceType() {
    for(auto &pair : this->fieldHandleMap) {
        delete pair.second;
    }

    for(auto &pair : this->methodHandleMap) {
        delete pair.second;
    }
}

FieldHandle *InterfaceType::newFieldHandle(const std::string &fieldName, DSType &fieldType, bool readOnly) {
    // field index is always 0.
    FieldAttributes attr(FieldAttribute::INTERFACE);
    if(readOnly) {
        attr.set(FieldAttribute::READ_ONLY);
    }

    auto *handle = new FieldHandle(&fieldType, 0, attr);
    auto pair = this->fieldHandleMap.insert(std::make_pair(fieldName, handle));
    if(pair.second) {
        return handle;
    }
    delete handle;
    return nullptr;

}

MethodHandle *InterfaceType::newMethodHandle(const std::string &methodName) {
    auto *handle = new MethodHandle(0);
    handle->setAttribute(MethodHandle::INTERFACE);
    auto pair = this->methodHandleMap.insert(std::make_pair(methodName, handle));
    if(!pair.second) {
        handle->setNext(pair.first->second);
        pair.first->second = handle;
    }
    return handle;
}

unsigned int InterfaceType::getFieldSize() {
    return this->superType->getFieldSize() + this->fieldHandleMap.size();
}

unsigned int InterfaceType::getMethodSize() {
    return this->superType->getMethodSize() + this->methodHandleMap.size();
}

FieldHandle *InterfaceType::lookupFieldHandle(TypePool &typePool, const std::string &fieldName) {
    auto iter = this->fieldHandleMap.find(fieldName);
    if(iter == this->fieldHandleMap.end()) {
        return this->superType->lookupFieldHandle(typePool, fieldName);
    }
    return iter->second;
}

MethodHandle *InterfaceType::lookupMethodHandle(TypePool &typePool, const std::string &methodName) {
    auto iter = this->methodHandleMap.find(methodName);
    if(iter == this->methodHandleMap.end()) {
        return this->superType->lookupMethodHandle(typePool, methodName);
    }

    //FIXME:
    return iter->second;
}

FieldHandle *InterfaceType::findHandle(const std::string &fieldName) {
    return this->superType->findHandle(fieldName);
}

void InterfaceType::accept(TypeVisitor *visitor) {
    visitor->visitInterfaceType(this);
}

// #######################
// ##     ErrorType     ##
// #######################

ErrorType::~ErrorType() {
    delete this->constructorHandle;
}

NativeFuncInfo *ErrorType::funcInfo = nullptr;
const DSCode *ErrorType::initRef;

MethodHandle *ErrorType::getConstructorHandle(TypePool &typePool) {
    if(this->constructorHandle == nullptr) {
        this->constructorHandle = new MethodHandle(0);
        this->constructorHandle->init(typePool, *funcInfo);
        this->constructorHandle->setRecvType(*this);
    }
    return this->constructorHandle;
}

const DSCode *ErrorType::getConstructor() {
    return initRef;
}

unsigned int ErrorType::getFieldSize() {
    return this->superType->getFieldSize();
}

FieldHandle *ErrorType::lookupFieldHandle(TypePool &typePool, const std::string &fieldName) {
    return this->superType->lookupFieldHandle(typePool, fieldName);
}

MethodHandle *ErrorType::lookupMethodHandle(TypePool &typePool, const std::string &methodName) {
    return this->superType->lookupMethodHandle(typePool, methodName);
}

FieldHandle *ErrorType::findHandle(const std::string &fieldName) {
    return this->superType->findHandle(fieldName);
}

void ErrorType::accept(TypeVisitor *visitor) {
    visitor->visitErrorType(this);
}

/**
 * call only once.
 */
void ErrorType::registerFuncInfo(native_type_info_t info) {
    if(funcInfo == nullptr) {
        funcInfo = &info.getInitInfo();
        initRef = getCode(info);
    }
}


// #####################
// ##     TypeMap     ##
// #####################

static bool isAlias(const DSType *type) {
    assert(type != nullptr);
    return (reinterpret_cast<long>(type)) < 0;
}

static unsigned long asKey(const DSType *type) {
    assert(type != nullptr);
    return reinterpret_cast<unsigned long>(type);
}

TypeMap::~TypeMap() {
    for(auto pair : this->typeMapImpl) {
        if(!isAlias(pair.second)) {
            delete pair.second;
        }
    }
}

DSType *TypeMap::addType(std::string &&typeName, DSType *type) {
    assert(type != nullptr);
    auto pair = this->typeMapImpl.insert(std::make_pair(std::move(typeName), type));
    this->typeNameMap.insert(std::make_pair(asKey(type), &pair.first->first));
//    this->typeCache.push_back(&pair.first->first);
    return type;
}

DSType *TypeMap::getType(const std::string &typeName) const {
    constexpr unsigned long mask = ~(1L << 63);
    auto iter = this->typeMapImpl.find(typeName);
    if(iter != this->typeMapImpl.end()) {
        DSType *type = iter->second;
        if(isAlias(type)) {   // if tagged pointer, mask tag
            return reinterpret_cast<DSType *>(mask & (unsigned long) type);
        }
        return type;
    }
    return nullptr;
}

const std::string &TypeMap::getTypeName(const DSType &type) const {
    auto iter = this->typeNameMap.find(asKey(&type));
    assert(iter != this->typeNameMap.end());
    return *iter->second;
}

bool TypeMap::setAlias(std::string &&alias, DSType &targetType) {
    constexpr unsigned long tag = 1L << 63;

    /**
     * use tagged pointer to prevent double free.
     */
    auto *taggedPtr = reinterpret_cast<DSType *>(tag | (unsigned long) &targetType);
    auto pair = this->typeMapImpl.insert(std::make_pair(std::move(alias), taggedPtr));
//    this->typeCache.push_back(&pair.first->first);
    return pair.second;
}

void TypeMap::commit() {
    this->typeCache.clear();
}

void TypeMap::abort() {
//    for(const std::string *typeName : this->typeCache) {
//        this->removeType(*typeName);
//    }
    this->typeCache.clear();
}

void TypeMap::removeType(const std::string &typeName) {
    auto iter = this->typeMapImpl.find(typeName);
    if(iter != this->typeMapImpl.end()) {
        if(!isAlias(iter->second)) {
            this->typeNameMap.erase(asKey(iter->second));
            delete iter->second;
        }
        this->typeMapImpl.erase(iter);
    }
}


// ######################
// ##     TypePool     ##
// ######################

TypePool::TypePool() :
        typeTable(new DSType*[__SIZE_OF_DS_TYPE__]()),
        templateMap(8),
        arrayTemplate(), mapTemplate(), tupleTemplate() {

    // initialize type
    this->initBuiltinType(Any, "Any", true, info_AnyType());
    this->initBuiltinType(Void, "Void", false, info_Dummy());
    this->initBuiltinType(Nothing, "Nothing", false, info_Dummy());
    this->initBuiltinType(Variant, "Variant", false, this->getAnyType(), info_Dummy());

    /**
     * hidden from script.
     */
    this->initBuiltinType(Value__, "Value%%", true, this->getVariantType(), info_Dummy());

    this->initBuiltinType(Byte, "Byte", false, this->getValueType(), info_ByteType());
    this->initBuiltinType(Int16, "Int16", false, this->getValueType(), info_Int16Type());
    this->initBuiltinType(Uint16, "Uint16", false, this->getValueType(), info_Uint16Type());
    this->initBuiltinType(Int32, "Int32", false, this->getValueType(), info_Int32Type());
    this->initBuiltinType(Uint32, "Uint32", false, this->getValueType(), info_Uint32Type());
    this->initBuiltinType(Int64, "Int64", false, this->getValueType(), info_Int64Type());
    this->initBuiltinType(Uint64, "Uint64", false, this->getValueType(), info_Uint64Type());

    this->initBuiltinType(Float, "Float", false, this->getValueType(), info_FloatType());
    this->initBuiltinType(Boolean, "Boolean", false, this->getValueType(), info_BooleanType());
    this->initBuiltinType(String, "String", false, this->getValueType(), info_StringType());

    this->initBuiltinType(ObjectPath, "ObjectPath", false, this->getValueType(), info_ObjectPathType());
    this->initBuiltinType(UnixFD, "UnixFD", false, this->getAnyType(), info_UnixFDType());
    this->initBuiltinType(Proxy, "Proxy", false, this->getAnyType(), info_ProxyType());
    this->initBuiltinType(DBus, "DBus", false, this->getAnyType(), info_DBusType());
    this->initBuiltinType(Bus, "Bus", false, this->getAnyType(), info_BusType());
    this->initBuiltinType(Service, "Service", false, this->getAnyType(), info_ServiceType());
    this->initBuiltinType(DBusObject, "DBusObject", false, this->getProxyType(), info_DBusObjectType());

    this->initBuiltinType(Error, "Error", true, this->getAnyType(), info_ErrorType());
    this->initBuiltinType(Task, "Task", false, this->getAnyType(), info_Dummy());
    this->initBuiltinType(Func, "Func", false, this->getAnyType(), info_Dummy());
    this->initBuiltinType(StringIter__, "StringIter%%", false, this->getAnyType(), info_StringIterType());
    this->initBuiltinType(Regex, "Regex", false, this->getAnyType(), info_RegexType());
    this->initBuiltinType(Signal, "Signal", false, this->getAnyType(), info_SignalType());
    this->initBuiltinType(Signals, "Signals", false, this->getAnyType(), info_SignalsType());

    // register NativeFuncInfo to ErrorType
    ErrorType::registerFuncInfo(info_ErrorType());

    // initialize type template
    std::vector<DSType *> elements = {&this->getAnyType()};
    this->arrayTemplate = this->initTypeTemplate(TYPE_ARRAY, std::move(elements), info_ArrayType());

    elements = {&this->getValueType(), &this->getAnyType()};
    this->mapTemplate = this->initTypeTemplate(TYPE_MAP, std::move(elements), info_MapType());

    elements = std::vector<DSType *>();
    this->tupleTemplate = this->initTypeTemplate(TYPE_TUPLE, std::move(elements), info_TupleType());   // pseudo template.

    elements = std::vector<DSType *>();
    this->optionTemplate = this->initTypeTemplate(TYPE_OPTION, std::move(elements), info_OptionType()); // pseudo template

    // init string array type(for command argument)
    std::vector<DSType *> types = {&this->getStringType()};
    this->setToTypeTable(StringArray, &this->createReifiedType(this->getArrayTemplate(), std::move(types)));

    // init some error type
    this->initErrorType(ArithmeticError, "ArithmeticError", this->getErrorType());
    this->initErrorType(OutOfRangeError, "OutOfRangeError", this->getErrorType());
    this->initErrorType(KeyNotFoundError, "KeyNotFoundError", this->getErrorType());
    this->initErrorType(TypeCastError, "TypeCastError", this->getErrorType());
    this->initErrorType(DBusError, "DBusError", this->getErrorType());
    this->initErrorType(SystemError, "SystemError", this->getErrorType());
    this->initErrorType(StackOverflowError, "StackOverflowError", this->getErrorType());
    this->initErrorType(RegexSyntaxError, "RegexSyntaxError", this->getErrorType());
    this->initErrorType(UnwrapingError, "UnwrappingError", this->getErrorType());

    this->registerDBusErrorTypes();

    // init internal status type
    this->initBuiltinType(InternalStatus__, "internal status%%", false, info_Dummy());
    this->initBuiltinType(ShellExit__, "Shell Exit", false, this->getInternalStatus(), info_Dummy());
    this->initBuiltinType(AssertFail__, "Assertion Error", false, this->getInternalStatus(), info_Dummy());

    // commit generated type
    this->typeMap.commit();
}

TypePool::~TypePool() {
    delete[] this->typeTable;
    for(auto &pair : this->templateMap) {
        delete pair.second;
    }
}

DSType &TypePool::getTypeAndThrowIfUndefined(const std::string &typeName) const {
    DSType *type = this->getType(typeName);
    if(type == nullptr) {
        RAISE_TL_ERROR(UndefinedType, typeName.c_str());
    }
    return *type;
}

const TypeTemplate &TypePool::getTypeTemplate(const std::string &typeName) const {
    auto iter = this->templateMap.find(typeName);
    if(iter == this->templateMap.end()) {
        RAISE_TL_ERROR(NotTemplate, typeName.c_str());
    }
    return *iter->second;
}

DSType &TypePool::createReifiedType(const TypeTemplate &typeTemplate,
                                    std::vector<DSType *> &&elementTypes) {
    if(this->tupleTemplate->getName() == typeTemplate.getName()) {
        return this->createTupleType(std::move(elementTypes));
    }

    flag8_set_t attr = this->optionTemplate->getName() == typeTemplate.getName() ? DSType::OPTION_TYPE : 0;

    // check each element type
    if(attr != 0u) {
        auto *type = elementTypes[0];
        if(type->isOptionType() || type->isVoidType() || type->isNothingType()) {
            RAISE_TL_ERROR(InvalidElement, this->getTypeName(*type).c_str());
        }
    } else {
        this->checkElementTypes(typeTemplate, elementTypes);
    }

    std::string typeName(this->toReifiedTypeName(typeTemplate, elementTypes));
    DSType *type = this->typeMap.getType(typeName);
    if(type == nullptr) {
        DSType *superType = attr != 0u ? nullptr :
                            this->asVariantType(elementTypes) ? &this->getVariantType() : &this->getAnyType();
        return *this->typeMap.addType(std::move(typeName),
                                      new ReifiedType(typeTemplate.getInfo(), superType, std::move(elementTypes), attr));
    }
    return *type;
}

DSType &TypePool::createTupleType(std::vector<DSType *> &&elementTypes) {
    this->checkElementTypes(elementTypes);

    assert(!elementTypes.empty());

    std::string typeName(this->toTupleTypeName(elementTypes));
    DSType *type = this->typeMap.getType(typeName);
    if(type == nullptr) {
        DSType *superType = this->asVariantType(elementTypes) ? &this->getVariantType() : &this->getAnyType();
        return *this->typeMap.addType(std::move(typeName),
                                      new TupleType(this->tupleTemplate->getInfo(), superType, std::move(elementTypes)));
    }
    return *type;
}

FunctionType &TypePool::createFuncType(DSType *returnType, std::vector<DSType *> &&paramTypes) {
    this->checkElementTypes(paramTypes);

    std::string typeName(toFunctionTypeName(returnType, paramTypes));
    DSType *type = this->typeMap.getType(typeName);
    if(type == nullptr) {
        auto *funcType = new FunctionType(&this->getBaseFuncType(), returnType, std::move(paramTypes));
        this->typeMap.addType(std::move(typeName), funcType);
        return *funcType;
    }
    assert(type->isFuncType());

    return *static_cast<FunctionType *>(type);
}

InterfaceType &TypePool::createInterfaceType(const std::string &interfaceName) {
    DSType *type = this->typeMap.getType(interfaceName);
    if(type == nullptr) {
        auto *ifaceType = new InterfaceType(&this->getDBusObjectType());
        this->typeMap.addType(std::string(interfaceName), ifaceType);
        return *ifaceType;
    }
    assert(type->isInterface());

    return *static_cast<InterfaceType *>(type);
}

DSType &TypePool::createErrorType(const std::string &errorName, DSType &superType) {
    DSType *type = this->typeMap.getType(errorName);
    if(type == nullptr) {
        DSType *errorType = new ErrorType(&superType);
        this->typeMap.addType(std::string(errorName), errorType);
        return *errorType;
    }
    return *type;
}

DSType &TypePool::getDBusInterfaceType(const std::string &typeName) {
    DSType *type = this->typeMap.getType(typeName);
    if(type == nullptr) {
        // load dbus interface
        std::string ifacePath(getIfaceDir());
        ifacePath += "/";
        ifacePath += typeName;

        auto rootNode = parse(ifacePath.c_str());
        if(!rootNode) {
            RAISE_TL_ERROR(NoDBusInterface, typeName.c_str());
        }

        auto *front = rootNode->getNodes().front();
        if(!front->is(NodeKind::Interface)) {
            RAISE_TL_ERROR(NoDBusInterface, typeName.c_str());
        }

        auto *ifaceNode = static_cast<InterfaceNode *>(front);
        return TypeGenerator(*this).resolveInterface(ifaceNode);
    }
    return *type;
}

void TypePool::setAlias(const char *alias, DSType &targetType) {
    if(!this->typeMap.setAlias(std::string(alias), targetType)) {
        RAISE_TL_ERROR(DefinedType, alias);
    }
}

std::string TypePool::toReifiedTypeName(const std::string &name, const std::vector<DSType *> &elementTypes) const {
    int elementSize = elementTypes.size();
    std::string reifiedTypeName(name);
    reifiedTypeName += "<";
    for(int i = 0; i < elementSize; i++) {
        if(i > 0) {
            reifiedTypeName += ",";
        }
        reifiedTypeName += this->getTypeName(*elementTypes[i]);
    }
    reifiedTypeName += ">";
    return reifiedTypeName;
}

std::string TypePool::toFunctionTypeName(DSType *returnType, const std::vector<DSType *> &paramTypes) const {
    int paramSize = paramTypes.size();
    std::string funcTypeName("Func<");
    funcTypeName += this->getTypeName(*returnType);
    for(int i = 0; i < paramSize; i++) {
        if(i == 0) {
            funcTypeName += ",[";
        }
        if(i > 0) {
            funcTypeName += ",";
        }
        funcTypeName += this->getTypeName(*paramTypes[i]);
        if(i == paramSize - 1) {
            funcTypeName += "]";
        }
    }
    funcTypeName += ">";
    return funcTypeName;
}

constexpr int TypePool::INT64_PRECISION;
constexpr int TypePool::INT32_PRECISION;
constexpr int TypePool::INT16_PRECISION;
constexpr int TypePool::BYTE_PRECISION;
constexpr int TypePool::INVALID_PRECISION;

int TypePool::getIntPrecision(const DSType &type) const {
    const struct {
        DS_TYPE TYPE;
        int precision;
    } table[] = {
            // Int64, Uint64
            {Int64, INT64_PRECISION},
            {Uint64, INT64_PRECISION},
            // Int32, Uint32
            {Int32, INT32_PRECISION},
            {Uint32, INT32_PRECISION},
            // Int16, Uint16
            {Int16, INT16_PRECISION},
            {Uint16, INT16_PRECISION},
            // Byte
            {Byte, BYTE_PRECISION},
    };

    for(auto &e : table) {
        if(*this->typeTable[e.TYPE] == type) {
            return e.precision;
        }
    }
    return INVALID_PRECISION;
}

static const TypePool::DS_TYPE numTypeTable[] = {
        TypePool::Byte,   // 0
        TypePool::Int16,  // 1
        TypePool::Uint16, // 2
        TypePool::Int32,  // 3
        TypePool::Uint32, // 4
        TypePool::Int64,  // 5
        TypePool::Uint64, // 6
        TypePool::Float,  // 7
};

int TypePool::getNumTypeIndex(const DSType &type) const {
    for(unsigned int i = 0; i < arraySize(numTypeTable); i++) {
        if(*this->typeTable[numTypeTable[i]] == type) {
            return i;
        }
    }
    return -1;
}

DSType *TypePool::getByNumTypeIndex(unsigned int index) const {
    return index < arraySize(numTypeTable) ? this->typeTable[numTypeTable[index]] : nullptr;
}

void TypePool::setToTypeTable(DS_TYPE TYPE, DSType *type) {
    assert(this->typeTable[TYPE] == nullptr && type != nullptr);
    this->typeTable[TYPE] = type;
}

void TypePool::initBuiltinType(DS_TYPE TYPE, const char *typeName, bool extendable,
                               native_type_info_t info) {
    // create and register type
    flag8_set_t attribute = extendable ? DSType::EXTENDIBLE : 0;
    if(TYPE == Void) {
        attribute |= DSType::VOID_TYPE;
    }
    if(TYPE == Nothing) {
        attribute |= DSType::NOTHING_TYPE;
    }

    DSType *type = this->typeMap.addType(
            std::string(typeName), new BuiltinType(nullptr, info, attribute));

    // set to typeTable
    this->setToTypeTable(TYPE, type);
}

void TypePool::initBuiltinType(DS_TYPE TYPE, const char *typeName, bool extendable,
                               DSType &superType, native_type_info_t info) {
    // create and register type
    DSType *type = this->typeMap.addType(
            std::string(typeName), new BuiltinType(&superType, info, extendable ? DSType::EXTENDIBLE : 0));

    // set to typeTable
    this->setToTypeTable(TYPE, type);
}

TypeTemplate *TypePool::initTypeTemplate(const char *typeName,
                                         std::vector<DSType *> &&elementTypes, native_type_info_t info) {
    return this->templateMap.insert(
            std::make_pair(typeName, new TypeTemplate(std::string(typeName),
                                                      std::move(elementTypes), info))).first->second;
}

void TypePool::initErrorType(DS_TYPE TYPE, const char *typeName, DSType &superType) {
    DSType *type = this->typeMap.addType(std::string(typeName), new ErrorType(&superType));
    this->setToTypeTable(TYPE, type);
}

void TypePool::checkElementTypes(const std::vector<DSType *> &elementTypes) const {
    for(DSType *type : elementTypes) {
        if(type->isVoidType() || type->isNothingType()) {
            RAISE_TL_ERROR(InvalidElement, this->getTypeName(*type).c_str());
        }
    }
}

void TypePool::checkElementTypes(const TypeTemplate &t, const std::vector<DSType *> &elementTypes) const {
    const unsigned int size = elementTypes.size();

    // check element type size
    if(t.getElementTypeSize() != size) {
        RAISE_TL_ERROR(UnmatchElement, t.getName().c_str(), t.getElementTypeSize(), size);
    }

    for(unsigned int i = 0; i < size; i++) {
        auto *acceptType = t.getAcceptableTypes()[i];
        auto *elementType = elementTypes[i];
        if(acceptType->isSameOrBaseTypeOf(*elementType) && !elementType->isNothingType()) {
            continue;
        }
        if(*acceptType == this->getAnyType() && elementType->isOptionType()) {
            continue;
        }
        RAISE_TL_ERROR(InvalidElement, this->getTypeName(*elementType).c_str());
    }
}

bool TypePool::asVariantType(const std::vector<DSType *> &elementTypes) const {
    for(DSType *type : elementTypes) {
        if(!this->getVariantType().isSameOrBaseTypeOf(*type)) {
            return false;
        }
    }
    return true;
}

void TypePool::registerDBusErrorTypes() {
    const char *table[] = {
            "Failed",
            "NoMemory",
            "ServiceUnknown",
            "NameHasNoOwner",
            "NoReply",
            "IOError",
            "BadAddress",
            "NotSupported",
            "LimitsExceeded",
            "AccessDenied",
            "AuthFailed",
            "NoServer",
            "Timeout",
            "NoNetwork",
            "AddressInUse",
            "Disconnected",
            "InvalidArgs",
            "FileNotFound",
            "FileExists",
            "UnknownMethod",
            "UnknownObject",
            "UnknownInterface",
            "UnknownProperty",
            "PropertyReadOnly",
            "TimedOut",
            "MatchRuleNotFound",
            "MatchRuleInvalid",
            "Spawn.ExecFailed",
            "Spawn.ForkFailed",
            "Spawn.ChildExited",
            "Spawn.ChildSignaled",
            "Spawn.Failed",
            "Spawn.FailedToSetup",
            "Spawn.ConfigInvalid",
            "Spawn.ServiceNotValid",
            "Spawn.ServiceNotFound",
            "Spawn.PermissionsInvalid",
            "Spawn.FileInvalid",
            "Spawn.NoMemory",
            "UnixProcessIdUnknown",
            "InvalidSignature",
            "InvalidFileContent",
            "SELinuxSecurityContextUnknown",
            "AdtAuditDataUnknown",
            "ObjectPathInUse",
            "InconsistentMessage",
            "InteractiveAuthorizationRequired",
    };

    for(const auto &e : table) {
        std::string s = "org.freedesktop.DBus.Error.";
        s += e;
        this->setAlias(e, this->createErrorType(s, this->getDBusErrorType()));
    }
}

TypeLookupError createTLError(TLError, const char *kind, const char *fmt, ...) {
    va_list arg;

    va_start(arg, fmt);
    char *str = nullptr;
    if(vasprintf(&str, fmt, arg) == -1) { abort(); }
    va_end(arg);

    TypeLookupError error(kind, str);
    free(str);
    return error;
}

TypeCheckError createTCError(TCError, const Node &node, const char *kind, const char *fmt, ...) {
    va_list arg;

    va_start(arg, fmt);
    char *str = nullptr;
    if(vasprintf(&str, fmt, arg) == -1) { abort(); }
    va_end(arg);

    TypeCheckError error(node.getToken(), kind, str);
    free(str);
    return error;
}

} // namespace ydsh
