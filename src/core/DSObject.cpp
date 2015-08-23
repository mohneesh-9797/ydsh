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

#include <cassert>

#include "DSObject.h"
#include "RuntimeContext.h"
#include "../ast/Node.h"
#include "../misc/debug.h"
#include "../config.h"

#ifdef USE_DBUS
#include "../dbus/DBusBind.h"
#endif

namespace ydsh {
namespace core {

using namespace ydsh::ast;

// #####################
// ##     DSValue     ##
// #####################

DSValue::DSValue(DSObject *obj) noexcept : obj(obj) {
    if(this->obj != nullptr) {
        this->obj->refCount++;
    }
}

DSValue::~DSValue() {
    if(this->obj != nullptr) {
        if(--this->obj->refCount == 0) {
            delete this->obj;
        }
        this->obj = nullptr;
    }
}

DSValue &DSValue::operator=(const DSValue &value) noexcept {
    DSValue tmp(value);
    this->swap(tmp);
    return *this;
}

DSValue &DSValue::operator=(DSValue &&value) noexcept {
    DSValue tmp(std::move(value));
    this->swap(tmp);
    return *this;
}

void DSValue::reset() noexcept {
    DSValue tmp;
    this->swap(tmp);
}


// ######################
// ##     DSObject     ##
// ######################

DSObject::DSObject(DSType *type) :
        type(type), refCount(0) {
}

DSType *DSObject::getType() {
    return this->type;
}

void DSObject::setType(DSType *type) {  // do nothing.
}

DSValue *DSObject::getFieldTable() {
    return 0;
}

std::string DSObject::toString(RuntimeContext &ctx) {
    std::string str("DSObject(");
    str += std::to_string((long) this);
    str += ")";
    return str;
}

bool DSObject::equals(const DSValue &obj) {
    return (long) this == (long) obj.get();
}

DSValue DSObject::str(RuntimeContext &ctx) {
    return DSValue::create<String_Object>(ctx.getPool().getStringType(), this->toString(ctx));
}

DSValue DSObject::interp(RuntimeContext &ctx) {
    return this->str(ctx);
}

DSValue DSObject::commandArg(RuntimeContext &ctx) {
    return this->str(ctx);
}

size_t DSObject::hash() {
    return std::hash<long>()((long) this);
}

bool DSObject::introspect(RuntimeContext &ctx, DSType *targetType) {
    return targetType->isSameOrBaseTypeOf(this->type);
}

void DSObject::accept(ObjectVisitor *visitor) {
    visitor->visitDefault(this);
}

// ########################
// ##     Int_Object     ##
// ########################

Int_Object::Int_Object(DSType *type, int value) :
        DSObject(type), value(value) {
}

int Int_Object::getValue() {
    return this->value;
}

std::string Int_Object::toString(RuntimeContext &ctx) {
    if(*this->type == *ctx.getPool().getUint32Type()) {
        return std::to_string((unsigned int) this->value);
    }
    if(*this->type == *ctx.getPool().getInt16Type()) {
        return std::to_string((short) this->value);
    }
    if(*this->type == *ctx.getPool().getUint16Type()) {
        return std::to_string((unsigned short) this->value);
    }
    if(*this->type == *ctx.getPool().getByteType()) {
        return std::to_string((unsigned char) this->value);
    }
    return std::to_string(this->value);
}

bool Int_Object::equals(const DSValue &obj) {
    return this->value == TYPE_AS(Int_Object, obj)->value;
}

size_t Int_Object::hash() {
    return std::hash<int>()(this->value);
}

void Int_Object::accept(ObjectVisitor *visitor) {
    visitor->visitInt_Object(this);
}

// #########################
// ##     Long_Object     ##
// #########################

Long_Object::Long_Object(DSType *type, long value) :
        DSObject(type), value(value) {
}

long Long_Object::getValue() {
    return this->value;
}

std::string Long_Object::toString(RuntimeContext &ctx) {
    if(*this->type == *ctx.getPool().getUint64Type()) {
        return std::to_string((unsigned long) this->value);
    }
    return std::to_string(this->value);
}

bool Long_Object::equals(const DSValue &obj) {
    return this->value == TYPE_AS(Long_Object, obj)->value;
}

size_t Long_Object::hash() {
    return std::hash<long>()(this->value);
}

void Long_Object::accept(ObjectVisitor *visitor) {
    visitor->visitLong_Object(this);
}

// ##########################
// ##     Float_Object     ##
// ##########################

Float_Object::Float_Object(DSType *type, double value) :
        DSObject(type), value(value) {
}

double Float_Object::getValue() {
    return this->value;
}

std::string Float_Object::toString(RuntimeContext &ctx) {
    return std::to_string(this->value);
}

bool Float_Object::equals(const DSValue &obj) {
    return this->value == TYPE_AS(Float_Object, obj)->value;
}

size_t Float_Object::hash() {
    return std::hash<double>()(this->value);
}

void Float_Object::accept(ObjectVisitor *visitor) {
    visitor->visitFloat_Object(this);
}


// ############################
// ##     Boolean_Object     ##
// ############################

Boolean_Object::Boolean_Object(DSType *type, bool value) :
        DSObject(type), value(value) {
}

bool Boolean_Object::getValue() {
    return this->value;
}

std::string Boolean_Object::toString(RuntimeContext &ctx) {
    return this->value ? "true" : "false";
}

bool Boolean_Object::equals(const DSValue &obj) {
    return this->value == TYPE_AS(Boolean_Object, obj)->value;
}

size_t Boolean_Object::hash() {
    return std::hash<bool>()(this->value);
}

void Boolean_Object::accept(ObjectVisitor *visitor) {
    visitor->visitBoolean_Object(this);
}

// ###########################
// ##     String_Object     ##
// ###########################

String_Object::String_Object(DSType *type, std::string &&value) :
        DSObject(type), value(std::move(value)) {
}

String_Object::String_Object(DSType *type, const std::string &value) :
        DSObject(type), value(value) {
}

String_Object::String_Object(DSType *type) :
        DSObject(type), value() {
}

const char *String_Object::getValue() const {
    return this->value.c_str();
}

unsigned int String_Object::size() const {
    return this->value.size();
}

bool String_Object::empty() const {
    return this->size() == 0;
}

std::string String_Object::toString(RuntimeContext &ctx) {
    return this->value;
}

bool String_Object::equals(const DSValue &obj) {
    return this->value == TYPE_AS(String_Object, obj)->value;
}

size_t String_Object::hash() {
    return std::hash<std::string>()(this->value);
}

void String_Object::accept(ObjectVisitor *visitor) {
    visitor->visitString_Object(this);
}

// ##########################
// ##     Array_Object     ##
// ##########################

Array_Object::Array_Object(DSType *type) :
        DSObject(type), curIndex(0), values() {
}

Array_Object::Array_Object(DSType *type, std::vector<DSValue> &&values) :
        DSObject(type), curIndex(0), values(std::move(values)) {
}

const std::vector<DSValue> &Array_Object::getValues() {
    return this->values;
}

std::string Array_Object::toString(RuntimeContext &ctx) {
    std::string str("[");
    unsigned int size = this->values.size();
    for(unsigned int i = 0; i < size; i++) {
        if(i > 0) {
            str += ", ";
        }
        str += this->values[i]->toString(ctx);
    }
    str += "]";
    return str;
}

void Array_Object::append(DSValue &&obj) {
    this->values.push_back(std::move(obj));
}

void Array_Object::append(const DSValue &obj) {
    this->values.push_back(obj);
}

void Array_Object::set(unsigned int index, const DSValue &obj) {
    this->values[index] = obj;
}

void Array_Object::initIterator() {
    this->curIndex = 0;
}

const DSValue &Array_Object::nextElement() {
    unsigned int index = this->curIndex++;
    assert(index < this->values.size());
    return this->values[index];
}

bool Array_Object::hasNext() {
    return this->curIndex < this->values.size();
}

DSValue Array_Object::interp(RuntimeContext &ctx) {
    if(this->values.size() == 1) {
        return this->values[0]->interp(ctx);
    }

    std::string str;
    unsigned int size = this->values.size();
    for(unsigned int i = 0; i < size; i++) {
        if(i > 0) {
            str += " ";
        }
        str += TYPE_AS(String_Object, this->values[i]->interp(ctx))->getValue();
    }
    return DSValue::create<String_Object>(ctx.getPool().getStringType(), std::move(str));
}

DSValue Array_Object::commandArg(RuntimeContext &ctx) {
    DSValue result(new Array_Object(ctx.getPool().getStringArrayType()));
    for(auto &e : this->values) {
        DSValue temp(e->commandArg(ctx));

        DSType *tempType = temp->getType();
        if(*tempType == *ctx.getPool().getStringType()) {
            TYPE_AS(Array_Object, result)->values.push_back(std::move(temp));
        } else if(*tempType == *ctx.getPool().getStringArrayType()) {
            Array_Object *tempArray = TYPE_AS(Array_Object, temp);
            for(auto &tempValue : tempArray->values) {
                TYPE_AS(Array_Object, result)->values.push_back(tempValue);
            }
        } else {
            fatal("illegal command argument type: %s\n", ctx.getPool().getTypeName(*tempType).c_str());
        }
    }
    return result;
}

void Array_Object::accept(ObjectVisitor *visitor) {
    visitor->visitArray_Object(this);
}

bool KeyCompare::operator()(const DSValue &x, const DSValue &y) const {
    return x->equals(y);
}

std::size_t GenHash::operator()(const DSValue &key) const {
    return key->hash();
}

// ########################
// ##     Map_Object     ##
// ########################

Map_Object::Map_Object(DSType *type) :
        DSObject(type), valueMap() {
}

const HashMap &Map_Object::getValueMap() {
    return this->valueMap;
}

void Map_Object::set(const DSValue &key, const DSValue &value) {
    this->valueMap[key] = value;
}

void Map_Object::add(std::pair<DSValue, DSValue> &&entry) {
    this->valueMap.insert(std::move(entry));
}

void Map_Object::initIterator() {
    this->iter = this->valueMap.cbegin();
}

DSValue Map_Object::nextElement(RuntimeContext &ctx) {
    std::vector<DSType *> types(2);
    types[0] = this->iter->first->getType();
    types[1] = this->iter->second->getType();

    DSValue entry(
            new Tuple_Object(ctx.getPool().createAndGetTupleTypeIfUndefined(std::move(types))));
    TYPE_AS(Tuple_Object, entry)->set(0, this->iter->first);
    TYPE_AS(Tuple_Object, entry)->set(1, this->iter->second);
    this->iter++;

    return std::move(entry);
}

bool Map_Object::hasNext() {
    return this->iter != this->valueMap.cend();
}

std::string Map_Object::toString(RuntimeContext &ctx) {
    std::string str("{");
    unsigned int count = 0;
    for(auto &iter : this->valueMap) {
        if(count++ > 0) {
            str += ", ";
        }
        str += iter.first->toString(ctx);
        str += " : ";
        str += iter.second->toString(ctx);
    }
    str += "}";
    return str;
}

void Map_Object::accept(ObjectVisitor *visitor) {
    visitor->visitMap_Object(this);
}

// ########################
// ##     BaseObject     ##
// ########################

BaseObject::BaseObject(DSType *type) :
        DSObject(type), fieldTable(new DSValue[type->getFieldSize()]) {
}

BaseObject::~BaseObject() {
    delete[] this->fieldTable;
    this->fieldTable = 0;
}

DSValue *BaseObject::getFieldTable() {
    return this->fieldTable;
}

// ##########################
// ##     Tuple_Object     ##
// ##########################

Tuple_Object::Tuple_Object(DSType *type) :
        BaseObject(type) {
}

std::string Tuple_Object::toString(RuntimeContext &ctx) {
    std::string str("(");
    unsigned int size = this->getElementSize();
    for(unsigned int i = 0; i < size; i++) {
        if(i > 0) {
            str += ", ";
        }
        str += this->fieldTable[i]->toString(ctx);
    }
    str += ")";
    return str;
}

unsigned int Tuple_Object::getElementSize() {
    return this->type->getFieldSize();
}

void Tuple_Object::set(unsigned int elementIndex, const DSValue &obj) {
    this->fieldTable[elementIndex] = obj;
}

const DSValue &Tuple_Object::get(unsigned int elementIndex) {
    return this->fieldTable[elementIndex];
}

DSValue Tuple_Object::interp(RuntimeContext &ctx) {
    std::string str;
    unsigned int size = this->getElementSize();
    for(unsigned int i = 0; i < size; i++) {
        if(i > 0) {
            str += " ";
        }
        str += TYPE_AS(String_Object, this->fieldTable[i]->str(ctx))->getValue();
    }
    return DSValue::create<String_Object>(ctx.getPool().getStringType(), std::move(str));
}

DSValue Tuple_Object::commandArg(RuntimeContext &ctx) {
    DSValue result(new Array_Object(ctx.getPool().getStringArrayType()));
    unsigned int size = this->getElementSize();
    for(unsigned int i = 0; i < size; i++) {
        DSValue temp(this->fieldTable[i]->commandArg(ctx));

        DSType *tempType = temp->getType();
        if(*tempType == *ctx.getPool().getStringType()) {
            TYPE_AS(Array_Object, result)->append(std::move(temp));
        } else if(*tempType == *ctx.getPool().getStringArrayType()) {
            Array_Object *tempArray = TYPE_AS(Array_Object, temp);
            for(auto &tempValue : tempArray->getValues()) {
                TYPE_AS(Array_Object, result)->append(tempValue);
            }
        } else {
            fatal("illegal command argument type: %s\n", ctx.getPool().getTypeName(*tempType).c_str());
        }
    }
    return result;
}

void Tuple_Object::accept(ObjectVisitor *visitor) {
    visitor->visitTuple_Object(this);
}

// ###############################
// ##     StackTraceElement     ##
// ###############################

std::ostream &operator<<(std::ostream &stream, const StackTraceElement &e) {
    return stream << "from " << e.getSourceName() << ":"
           << e.getLineNum() << " '" << e.getCallerName() << "()'";
}

unsigned int getOccuredLineNum(const std::vector<StackTraceElement> &elements) {
    return elements.front().getLineNum();
}


// ##########################
// ##     Error_Object     ##
// ##########################

Error_Object::Error_Object(DSType *type, const DSValue &message) :
        DSObject(type), message(message), name(), stackTrace() {
}

Error_Object::Error_Object(DSType *type, DSValue &&message) :
        DSObject(type), message(std::move(message)), name(), stackTrace() {
}

std::string Error_Object::toString(RuntimeContext &ctx) {
    std::string str("Error(");
    str += std::to_string((long) this);
    str += ", ";
    str += TYPE_AS(String_Object, this->message)->getValue();
    str += ")";
    return str;
}

const DSValue &Error_Object::getMessage() {
    return this->message;
}

void Error_Object::printStackTrace(RuntimeContext &ctx) {
    // print header
    std::cerr << ctx.getPool().getTypeName(*this->type) << ": "
    << TYPE_AS(String_Object, this->message)->getValue() << std::endl;

    // print stack trace
    for(auto &s : this->stackTrace) {
        std::cerr << "    " << s << std::endl;
    }
}

const DSValue &Error_Object::getName(RuntimeContext &ctx) {
    if(!this->name) {
        this->name = DSValue::create<String_Object>(
                ctx.getPool().getStringType(), ctx.getPool().getTypeName(*this->type));
    }
    return this->name;
}

const std::vector<StackTraceElement> &Error_Object::getStackTrace() {
    return this->stackTrace;
}

void Error_Object::accept(ObjectVisitor *visitor) {
    visitor->visitError_Object(this);
}

DSValue Error_Object::newError(RuntimeContext &ctx, DSType *type, const DSValue &message) {
    auto obj = DSValue::create<Error_Object>(type, message);
    TYPE_AS(Error_Object, obj)->createStackTrace(ctx);
    return std::move(obj);
}

DSValue Error_Object::newError(RuntimeContext &ctx, DSType *type, DSValue &&message) {
    auto obj = DSValue::create<Error_Object>(type, std::move(message));
    TYPE_AS(Error_Object, obj)->createStackTrace(ctx);
    return std::move(obj);
}

void Error_Object::createStackTrace(RuntimeContext &ctx) {
    ctx.fillInStackTrace(this->stackTrace);
}


// ########################
// ##     FuncObject     ##
// ########################

FuncObject::FuncObject() :
        DSObject(0) {
}

void FuncObject::setType(DSType *type) {
    if(this->type == 0) {
        assert(dynamic_cast<FunctionType *>(type) != 0);
        this->type = type;
    }
}

FunctionType *FuncObject::getFuncType() {
    return static_cast<FunctionType *>(this->type);
}


// ############################
// ##     UserFuncObject     ##
// ############################

UserFuncObject::UserFuncObject(FunctionNode *funcNode) :
        FuncObject(), funcNode(funcNode) {
}

UserFuncObject::~UserFuncObject() {
    delete this->funcNode;
    this->funcNode = 0;
}

FunctionNode *UserFuncObject::getFuncNode() {
    return this->funcNode;
}

std::string UserFuncObject::toString(RuntimeContext &ctx) {
    std::string str("function(");
    str += this->funcNode->getFuncName();
    str += ")";
    return str;
}

bool UserFuncObject::invoke(RuntimeContext &ctx) {  //TODO: default param
    // change stackTopIndex
    ctx.pushFuncContext(this->funcNode);
    ctx.reserveLocalVar(ctx.getLocalVarOffset() + this->funcNode->getMaxVarNum());

    EvalStatus s = this->funcNode->getBlockNode()->eval(ctx);
    ctx.popFuncContext();
    switch(s) {
    case EvalStatus::RETURN:
        return true;
    case EvalStatus::THROW:
        return false;
    default:
        fatal("illegal eval status: %d\n", s);
        return false;
    }
}

void UserFuncObject::accept(ObjectVisitor *visitor) {
    visitor->visitUserFuncObject(this);
}

// #############################
// ##     NativeMethodRef     ##
// #############################

NativeMethodRef::NativeMethodRef(native_func_t func_ptr) :
        MethodRef(), func_ptr(func_ptr) {
}

bool NativeMethodRef::invoke(RuntimeContext &ctx) {
    return this->func_ptr(ctx);
}

// #########################
// ##     DBus_Object     ##
// #########################

DBus_Object::DBus_Object(TypePool *typePool) :
        DSObject(typePool->getDBusType()) {
}

bool DBus_Object::getSystemBus(RuntimeContext &ctx) {
    ctx.throwError(ctx.getPool().getErrorType(), "unsupported feature");
    return false;
}

bool DBus_Object::getSessionBus(RuntimeContext &ctx) {
    ctx.throwError(ctx.getPool().getErrorType(), "unsupported feature");
    return false;
}

bool DBus_Object::waitSignal(RuntimeContext &ctx) {
    ctx.throwError(ctx.getPool().getErrorType(), "not support waitSignal method");
    return false;
}

bool DBus_Object::supportDBus() {
#ifdef USE_DBUS
    return true;
#else
    return false;
#endif
}

bool DBus_Object::getServiceFromProxy(RuntimeContext &ctx, const DSValue &proxy) {
    ctx.throwError(ctx.getPool().getErrorType(), "not support method");
    return false;
}

bool DBus_Object::getObjectPathFromProxy(RuntimeContext &ctx, const DSValue &proxy) {
    ctx.throwError(ctx.getPool().getErrorType(), "not support method");
    return false;
}

bool DBus_Object::getIfaceListFromProxy(RuntimeContext &ctx, const DSValue &proxy) {
    ctx.throwError(ctx.getPool().getErrorType(), "not support method");
    return false;
}

bool DBus_Object::introspectProxy(RuntimeContext &ctx, const DSValue &proxy) {
    ctx.throwError(ctx.getPool().getErrorType(), "not support method");
    return false;
}

DBus_Object *DBus_Object::newDBus_Object(TypePool *typePool) {
#ifdef USE_DBUS
    return newDBusObject(typePool);
#else
    return new DBus_Object(typePool);
#endif
}

// ########################
// ##     Bus_Object     ##
// ########################

Bus_Object::Bus_Object(DSType *type) : DSObject(type) {
}

bool Bus_Object::service(RuntimeContext &ctx, std::string &&serviceName) {
    ctx.throwError(ctx.getPool().getErrorType(), "not support D-Bus service object");
    return false;
}

bool Bus_Object::listNames(RuntimeContext &ctx, bool activeName) {
    ctx.throwError(ctx.getPool().getErrorType(), "not support listNames method");
    return false;
}

// ############################
// ##     Service_Object     ##
// ############################

Service_Object::Service_Object(DSType *type) : DSObject(type) {
}

bool Service_Object::object(RuntimeContext &ctx, const DSValue &objectPath) {
    ctx.throwError(ctx.getPool().getErrorType(), "not support D-Bus proxy object");
    return false;
}

} // namespace core
} // namespace ydsh
