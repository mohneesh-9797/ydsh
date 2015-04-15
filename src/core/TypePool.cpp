/*
 * Copyright (C) 2015 Nagisa Sekiguchi
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

#include <core/TypePool.h>
#include <core/TypeLookupError.h>
#include <core/bind.h>

#define NEW_ID() this->idCount++

namespace ydsh {
namespace core {

// ######################
// ##     TypePool     ##
// ######################

TypePool::TypePool(char **envp) :
        idCount(0), typeMap(16), typeNameTable(),
        anyType(), voidType(), valueType(),
        int8Type(), uint8Type(), int16Type(), uint16Type(),
        int32Type(), uint32Type(), floatType(), boolType(), stringType(),
        errorType(), taskType(), baseFuncType(),
        arithmeticErrorType(), outOfIndexErrorType(),
        keyNotFoundErrorType(), typeCastErrorType(),
        templateMap(8),
        arrayTemplate(), mapTemplate(), tupleTemplate(),
        stringArrayType(), envp(envp), envSet() {

    // initialize type
    this->anyType = this->initBuiltinType("Any", true, 0, info_AnyType());
    this->voidType = this->initBuiltinType("Void", false, 0, info_VoidType(), true);

    /**
     * hidden from script.
     */
    this->valueType = this->initBuiltinType("%Value%", true, this->anyType, info_Dummy());

    this->int8Type = this->initBuiltinType("Int8", false, this->valueType, info_Int8Type());
    this->uint8Type = this->initBuiltinType("Uint8", false, this->valueType, info_Uint8Type());
    this->int16Type = this->initBuiltinType("Int16", false, this->valueType, info_Int16Type());
    this->uint16Type = this->initBuiltinType("Uint16", false, this->valueType, info_Uint16Type());
    this->int32Type = this->initBuiltinType("Int", false, this->valueType, info_IntType());
    this->uint32Type = this->initBuiltinType("Uint32", false, this->valueType, info_Uint32Type());
    this->floatType = this->initBuiltinType("Float", false, this->valueType, info_FloatType());
    this->boolType = this->initBuiltinType("Boolean", false, this->valueType, info_BooleanType());
    this->stringType = this->initBuiltinType("String", false, this->valueType, info_StringType());
    this->errorType = this->initBuiltinType("Error", true, this->anyType, info_ErrorType());
    this->taskType = this->initBuiltinType("Task", false, this->anyType, info_Dummy());

    /**
     * hidden from script
     */
    this->baseFuncType = this->initBuiltinType("%BaseFunc%", false, this->anyType, info_Dummy());

    // initialize type template
    std::vector<DSType*> elements;
    elements.push_back(this->anyType);
    this->arrayTemplate = this->initTypeTemplate("Array", std::move(elements), info_ArrayType());

    elements = std::vector<DSType*>();
    elements.push_back(this->valueType);
    elements.push_back(this->anyType);
    this->mapTemplate = this->initTypeTemplate("Map", std::move(elements), info_MapType());

    elements = std::vector<DSType*>();
    this->tupleTemplate = this->initTypeTemplate("Tuple", std::move(elements), 0);   // pseudo template.

    // init string array type(for command argument)
    std::vector<DSType *> types(1);
    types[0] = this->stringType;
    this->stringArrayType = this->createAndGetReifiedTypeIfUndefined(this->arrayTemplate, types);

    // init some error type
    this->arithmeticErrorType = this->initBuiltinType("ArithmeticError", true, this->errorType, info_ArithmeticErrorType());
    this->outOfIndexErrorType = this->initBuiltinType("OutOfIndexError", true, this->errorType, info_OutOfIndexErrorType());
    this->keyNotFoundErrorType = this->initBuiltinType("KeyNotFoundError", true, this->errorType, info_KeyNotFoundErrorType());
    this->typeCastErrorType = this->initBuiltinType("TypeCastError", true, this->errorType, info_TypeCastErrorType());
}

TypePool::~TypePool() {
    for(const std::pair<std::string, DSType *> &pair : this->typeMap) {
        if((long) pair.second > -1) {
            delete pair.second;
        }
    }
    this->typeMap.clear();

    for(const std::pair<std::string, TypeTemplate *> &pair : this->templateMap) {
        delete pair.second;
    }
    this->templateMap.clear();
}

DSType *TypePool::getAnyType() {
    return this->anyType;
}

DSType *TypePool::getVoidType() {
    return this->voidType;
}

DSType *TypePool::getValueType() {
    return this->valueType;
}

DSType *TypePool::getIntType() {
    return this->getInt32Type();
}

DSType *TypePool::getInt8Type() {
    return this->int8Type;
}

DSType *TypePool::getUint8Type() {
    return this->uint8Type;
}

DSType *TypePool::getInt16Type() {
    return this->int16Type;
}

DSType *TypePool::getUint16Type() {
    return this->uint16Type;
}

DSType *TypePool::getInt32Type() {
    return this->int32Type;
}

DSType *TypePool::getUint32Type() {
    return this->uint32Type;
}

DSType *TypePool::getFloatType() {
    return this->floatType;
}

DSType *TypePool::getBooleanType() {
    return this->boolType;
}

DSType *TypePool::getStringType() {
    return this->stringType;
}

DSType *TypePool::getErrorType() {
    return this->errorType;
}

DSType *TypePool::getTaskType() {
    return this->taskType;
}

DSType *TypePool::getBaseFuncType() {
    return this->baseFuncType;
}

DSType *TypePool::getStringArrayType() {
    return this->stringArrayType;
}

DSType *TypePool::getArithmeticErrorType() {
    return this->arithmeticErrorType;
}

DSType *TypePool::getOutOfIndexErrorType() {
    return this->outOfIndexErrorType;
}

DSType *TypePool::getKeyNotFoundErrorType() {
    return this->keyNotFoundErrorType;
}

DSType *TypePool::getTypeCastErrorType() {
    return this->typeCastErrorType;
}

TypeTemplate *TypePool::getArrayTemplate() {
    return this->arrayTemplate;
}

TypeTemplate *TypePool::getMapTemplate() {
    return this->mapTemplate;
}

TypeTemplate *TypePool::getTupleTemplate() {
    return this->tupleTemplate;
}

DSType *TypePool::getType(const std::string &typeName) {
    static const unsigned long mask = ~(1L << 63);
    auto iter = this->typeMap.find(typeName);
    if(iter != this->typeMap.end()) {
        DSType *type = iter->second;
        if((long) type < 0) {   // if tagged pointer, mask tag
            return (DSType *) (mask & (unsigned long) type);
        }
        return type;
    }
    return 0;
}

DSType *TypePool::getTypeAndThrowIfUndefined(const std::string &typeName) {
    DSType *type = this->getType(typeName);
    if(type == 0) {
        E_UndefinedType(typeName);
    }
    return type;
}

TypeTemplate *TypePool::getTypeTemplate(const std::string &typeName, int elementSize) {
    auto iter = this->templateMap.find(typeName);
    if(iter == this->templateMap.end()) {
        E_NotGenericBase(typeName);
    }
    return iter->second;
}

DSType *TypePool::createAndGetReifiedTypeIfUndefined(TypeTemplate *typeTemplate,
                                                     const std::vector<DSType *> &elementTypes) {
    if(this->tupleTemplate->getName() == typeTemplate->getName()) {
        return this->createAndGetTupleTypeIfUndefined(elementTypes);
    }
    this->checkElementTypes(typeTemplate, elementTypes);

    if(typeTemplate->getElementTypeSize() != elementTypes.size()) {
        E_UnmatchElement(typeTemplate->getName(),
                         std::to_string(typeTemplate->getElementTypeSize()), std::to_string(elementTypes.size()));
    }

    std::string typeName(this->toReifiedTypeName(typeTemplate, elementTypes));
    auto iter = this->typeMap.find(typeName);
    if(iter == this->typeMap.end()) {
        return this->addType(std::move(typeName),
                             newReifiedType(NEW_ID(), typeTemplate->getInfo(), this->anyType, elementTypes));
    }
    return iter->second;
}

DSType *TypePool::createAndGetTupleTypeIfUndefined(const std::vector<DSType *> &elementTypes) {
    this->checkElementTypes(elementTypes);

    if(elementTypes.size() < 2) {
        E_TupleElement();
    }

    std::string typeName(this->toTupleTypeName(elementTypes));
    auto iter = this->typeMap.find(typeName);
    if(iter == this->typeMap.end()) {
        return this->addType(std::move(typeName), newTupleType(NEW_ID(), this->anyType, elementTypes));
    }
    return iter->second;
}

FunctionType *TypePool::createAndGetFuncTypeIfUndefined(DSType *returnType,
                                                        const std::vector<DSType *> &paramTypes) {
    this->checkElementTypes(paramTypes);

    std::string typeName(toFunctionTypeName(returnType, paramTypes));
    auto iter = this->typeMap.find(typeName);
    if(iter == this->typeMap.end()) {
        FunctionType *funcType =
                new FunctionType(NEW_ID(), this->getBaseFuncType(), returnType, paramTypes);
        this->addType(std::move(typeName), funcType);
        return funcType;
    }

    return dynamic_cast<FunctionType *>(iter->second);
}

void TypePool::setAlias(const std::string &alias, DSType *targetType) {
    static const unsigned long tag = 1L << 63;

    /**
     * use tagged pointer to prevent double free.
     */
    DSType *taggedPtr = (DSType *) (tag | (unsigned long) targetType);
    auto pair = this->typeMap.insert(std::make_pair(alias, taggedPtr));
    if(!pair.second) {
        E_DefinedType(alias);
    }
}

const std::string &TypePool::getTypeName(const DSType &type) {
    return *this->typeNameTable[type.getTypeId()];
}

std::string TypePool::toReifiedTypeName(TypeTemplate *typeTemplate, const std::vector<DSType *> &elementTypes) {
    return this->toReifiedTypeName(typeTemplate->getName(), elementTypes);
}

std::string TypePool::toReifiedTypeName(const std::string &name, const std::vector<DSType *> &elementTypes) {
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


std::string TypePool::toTupleTypeName(const std::vector<DSType *> &elementTypes) {
    return toReifiedTypeName("Tuple", elementTypes);
}

std::string TypePool::toFunctionTypeName(DSType *returnType, const std::vector<DSType *> &paramTypes) {
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

bool TypePool::hasEnv(const std::string &envName) {
    if(this->envSet.empty()) {
        this->initEnvSet();
    }
    return this->envSet.find(envName) != this->envSet.end();
}

void TypePool::addEnv(const std::string &envName) {
    if(this->envSet.empty()) {
        this->initEnvSet();
    }
    this->envSet.insert(envName);
}

void TypePool::initEnvSet() {
    for(unsigned int i = 0; this->envp[i] != NULL; i++) {
        char *env = this->envp[i];
        std::string str;
        for(unsigned int j = 0; env[j] != '\0'; j++) {
            char ch = env[j];
            if(ch == '=') {
                break;
            }
            str += ch;
        }
        this->envSet.insert(std::move(str));
    }
}

DSType *TypePool::addType(std::string &&typeName, DSType *type) {
    auto pair = this->typeMap.insert(std::make_pair(std::move(typeName), type));
    this->typeNameTable.push_back(&pair.first->first);
    return type;
}

DSType *TypePool::initBuiltinType(const char *typeName, bool extendable,
                                  DSType *superType, native_type_info_t *info, bool isVoid) {
    // create and register type
    return this->addType(
            std::string(typeName), newBuiltinType(NEW_ID(), extendable, superType, info, isVoid));
}

TypeTemplate *TypePool::initTypeTemplate(const char *typeName,
                                         std::vector<DSType*> &&elementTypes, native_type_info_t *info) {
    return this->templateMap.insert(
            std::make_pair(typeName, new TypeTemplate(std::string(typeName),
                                                      std::move(elementTypes), info))).first->second;
}

void TypePool::checkElementTypes(const std::vector<DSType *> &elementTypes) {
    for(DSType *type : elementTypes) {
        if(*type == *this->voidType) {
            E_InvalidElement(this->getTypeName(*type));
        }
    }
}

void TypePool::checkElementTypes(TypeTemplate *t, const std::vector<DSType *> &elementTypes) {
    unsigned int size = elementTypes.size();
    for(unsigned int i = 0; i < size; i++) {
        if(!t->getAcceptableTypes()[i]->isAssignableFrom(elementTypes[i])) {
            E_InvalidElement(this->getTypeName(*elementTypes[i]));
        }
    }
}

} // namespace core
} // namespace ydsh
