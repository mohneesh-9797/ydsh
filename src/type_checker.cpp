/*
 * Copyright (C) 2015-2018 Nagisa Sekiguchi
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

#include <sys/utsname.h>

#include <vector>

#include "constant.h"
#include "core.h"
#include "type_checker.h"

namespace ydsh {

// #########################
// ##     BreakGather     ##
// #########################

void BreakGather::clear() {
    delete this->entry;
}

void BreakGather::enter() {
    auto *e = new Entry(this->entry);
    this->entry = e;
}

void BreakGather::leave() {
    auto *old = this->entry->next;
    this->entry->next = nullptr;
    this->clear();
    this->entry = old;
}

void BreakGather::addJumpNode(JumpNode *node) {
    assert(this->entry != nullptr);
    this->entry->jumpNodes.push_back(node);
}

// #########################
// ##     TypeChecker     ##
// #########################

#define TRY(E) ({ auto value = E; if(!value) { return value; } value.take();})

TypeOrError TypeChecker::toTypeImpl(TypeNode &node) {
    switch(node.typeKind) {
    case TypeNode::Base: {
        return this->symbolTable.getType(cast<BaseTypeNode>(node).getTokenText());
    }
    case TypeNode::Reified: {
        auto &typeNode = cast<ReifiedTypeNode>(node);
        unsigned int size = typeNode.getElementTypeNodes().size();
        auto tempOrError = this->symbolTable.getTypeTemplate(typeNode.getTemplate()->getTokenText());
        if(!tempOrError) {
            return Err(tempOrError.takeError());
        }
        auto typeTemplate = tempOrError.take();
        std::vector<DSType *> elementTypes(size);
        for(unsigned int i = 0; i < size; i++) {
            elementTypes[i] = &this->checkTypeExactly(*typeNode.getElementTypeNodes()[i]);
        }
        return this->symbolTable.createReifiedType(*typeTemplate, std::move(elementTypes));
    }
    case TypeNode::Func: {
        auto &typeNode = cast<FuncTypeNode>(node);
        auto &returnType = this->checkTypeExactly(*typeNode.getReturnTypeNode());
        unsigned int size = typeNode.getParamTypeNodes().size();
        std::vector<DSType *> paramTypes(size);
        for(unsigned int i = 0; i < size; i++) {
            paramTypes[i] = &this->checkTypeExactly(*typeNode.getParamTypeNodes()[i]);
        }
        return this->symbolTable.createFuncType(&returnType, std::move(paramTypes));
    }
    case TypeNode::Return: {
        auto &typeNode = cast<ReturnTypeNode>(node);
        unsigned int size = typeNode.getTypeNodes().size();
        if(size == 1) {
            return Ok(&this->checkTypeExactly(*typeNode.getTypeNodes()[0]));
        }

        std::vector<DSType *> types(size);
        for(unsigned int i = 0; i < size; i++) {
            types[i] = &this->checkTypeExactly(*typeNode.getTypeNodes()[i]);
        }
        return this->symbolTable.createTupleType(std::move(types));
    }
    case TypeNode::TypeOf:
        auto &typeNode = cast<TypeOfNode>(node);
        auto &type = this->checkTypeAsSomeExpr(typeNode.getExprNode());
        return Ok(&type);
    }
    return Ok(static_cast<DSType *>(nullptr)); // for suppressing gcc warning (normally unreachable).
}

DSType &TypeChecker::checkType(const DSType *requiredType, Node &targetNode,
                               const DSType *unacceptableType, CoercionKind &kind) {
    /**
     * if target node is expr node and type is null,
     * try type check.
     */
    if(targetNode.isUntyped()) {
        this->visitingDepth++;
        targetNode.accept(*this);
        this->visitingDepth--;
    }

    /**
     * after type checking, Node is not untyped.
     */
    assert(!targetNode.isUntyped());
    auto &type = targetNode.getType();

    /**
     * do not try type matching.
     */
    if(requiredType == nullptr) {
        if(!type.isNothingType() && unacceptableType != nullptr &&
                unacceptableType->isSameOrBaseTypeOf(type)) {
            RAISE_TC_ERROR(Unacceptable, targetNode, this->symbolTable.getTypeName(type));
        }
        return type;
    }

    /**
     * try type matching.
     */
    if(requiredType->isSameOrBaseTypeOf(type)) {
        return type;
    }

    /**
     * check coercion
     */
    if(kind == CoercionKind::INVALID_COERCION && this->checkCoercion(*requiredType, type)) {
        kind = CoercionKind::PERFORM_COERCION;
        return type;
    }

    RAISE_TC_ERROR(Required, targetNode, this->symbolTable.getTypeName(*requiredType),
                   this->symbolTable.getTypeName(type));
}

DSType& TypeChecker::checkTypeAsSomeExpr(Node &targetNode) {
    auto &type = this->checkTypeAsExpr(targetNode);
    if(type.isNothingType()) {
        RAISE_TC_ERROR(Unacceptable, targetNode, this->symbolTable.getTypeName(type));
    }
    return type;
}

void TypeChecker::checkTypeWithCurrentScope(const DSType *requiredType, BlockNode &blockNode) {
    DSType *blockType = &this->symbolTable.get(TYPE::Void);
    for(auto iter = blockNode.refNodes().begin(); iter != blockNode.refNodes().end(); ++iter) {
        auto &targetNode = *iter;
        if(blockType->isNothingType()) {
            RAISE_TC_ERROR(Unreachable, *targetNode);
        }
        if(iter == blockNode.refNodes().end() - 1) {
            if(requiredType != nullptr) {
                this->checkTypeWithCoercion(*requiredType, targetNode);
            } else {
                this->checkTypeExactly(*targetNode);
            }
        } else {
            this->checkTypeWithCoercion(this->symbolTable.get(TYPE::Void), targetNode);
        }
        blockType = &targetNode->getType();

        // check empty block
        if(isa<BlockNode>(*targetNode) && cast<BlockNode>(*targetNode).getNodes().empty()) {
            RAISE_TC_ERROR(UselessBlock, *targetNode);
        }
    }

    // set base index of current scope
    blockNode.setBaseIndex(this->symbolTable.curScope().getBaseIndex());
    blockNode.setVarSize(this->symbolTable.curScope().getVarSize());
    blockNode.setMaxVarSize(this->symbolTable.getMaxVarIndex() - blockNode.getBaseIndex());

    assert(blockType != nullptr);
    blockNode.setType(*blockType);
}

void TypeChecker::checkTypeWithCoercion(const DSType &requiredType, std::unique_ptr<Node> &targetNode) {
    CoercionKind kind = CoercionKind::INVALID_COERCION;
    this->checkType(&requiredType, *targetNode, nullptr, kind);
    if(kind != CoercionKind::INVALID_COERCION && kind != CoercionKind::NOP) {
        this->resolveCoercion(requiredType, targetNode);
    }
}

bool TypeChecker::checkCoercion(const DSType &requiredType, const DSType &targetType) {
    if(requiredType.isVoidType()) {  // pop stack top
        return true;
    }

    if(requiredType.is(TYPE::Boolean)) {
        if(targetType.isOptionType()) {
            return true;
        }
        auto *handle = this->symbolTable.lookupMethod(targetType, OP_BOOL);
        if(handle != nullptr) {
            return true;
        }
    }
    return false;
}

DSType& TypeChecker::resolveCoercionOfJumpValue() {
    auto &jumpNodes = this->breakGather.getJumpNodes();
    if(jumpNodes.empty()) {
        return this->symbolTable.get(TYPE::Void);
    }

    auto &firstType = jumpNodes[0]->getExprNode().getType();
    assert(!firstType.isNothingType() && !firstType.isVoidType());

    for(auto &jumpNode : jumpNodes) {
        if(firstType != jumpNode->getExprNode().getType()) {
            this->checkTypeWithCoercion(firstType, jumpNode->refExprNode());
        }
    }
    auto ret = this->symbolTable.createOptionType(firstType);
    assert(ret);
    return *ret.take();
}

const FieldHandle *TypeChecker::addEntry(const Node &node, const std::string &symbolName,
                                   const DSType &type, FieldAttribute attribute) {
    auto pair = this->symbolTable.newHandle(symbolName, type, attribute);
    if(!pair) {
        switch(pair.asErr()) {
        case SymbolError::DEFINED:
            RAISE_TC_ERROR(DefinedSymbol, node, symbolName.c_str());
        case SymbolError::LIMIT:
            RAISE_TC_ERROR(LocalLimit, node);
        }
    }
    return pair.asOk();
}

// for ApplyNode type checking
HandleOrFuncType TypeChecker::resolveCallee(ApplyNode &node) {
    auto &exprNode = node.getExprNode();
    if(exprNode.is(NodeKind::Access) && !node.isFuncCall()) {
        auto &accessNode = cast<AccessNode>(exprNode);
        if(!this->checkAccessNode(accessNode)) {
            auto &recvType = accessNode.getRecvNode().getType();
            auto *handle = this->symbolTable.lookupMethod(recvType, accessNode.getFieldName());
            if(handle == nullptr) {
                const char *name = accessNode.getFieldName().c_str();
                RAISE_TC_ERROR(UndefinedMethod, accessNode.getNameNode(), name);
            }
            node.setKind(ApplyNode::METHOD_CALL);
            return handle;
        }
    }

    node.setKind(ApplyNode::FUNC_CALL);
    if(exprNode.is(NodeKind::Var)) {
        return this->resolveCallee(cast<VarNode>(exprNode));
    }

    auto &type = this->checkType(this->symbolTable.get(TYPE::Func), exprNode);
    if(!type.isFuncType()) {
        RAISE_TC_ERROR(NotCallable, exprNode);
    }
    return static_cast<FunctionType *>(&type);
}

HandleOrFuncType TypeChecker::resolveCallee(VarNode &recvNode) {
    auto handle = this->symbolTable.lookupHandle(recvNode.getVarName());
    if(handle == nullptr) {
        RAISE_TC_ERROR(UndefinedSymbol, recvNode, recvNode.getVarName().c_str());
    }
    recvNode.setAttribute(*handle);

    auto &type = handle->getType();
    if(!type.isFuncType()) {
        if(type.is(TYPE::Func)) {
            RAISE_TC_ERROR(NotCallable, recvNode);
        } else {
            RAISE_TC_ERROR(Required, recvNode, this->symbolTable.getTypeName(this->symbolTable.get(TYPE::Func)),
                           this->symbolTable.getTypeName(type));
        }
    }
    return static_cast<FunctionType *>(const_cast<DSType *>(&type));
}

void TypeChecker::checkTypeArgsNode(Node &node, const MethodHandle *handle, std::vector<std::unique_ptr<Node>> &argNodes) {
    unsigned int argSize = argNodes.size();
    // check param size
    unsigned int paramSize = handle->getParamSize();
    if(paramSize != argSize) {
        RAISE_TC_ERROR(UnmatchParam, node, paramSize, argSize);
    }

    // check type each node
    for(unsigned int i = 0; i < paramSize; i++) {
        this->checkTypeWithCoercion(handle->getParamTypeAt(i), argNodes[i]);
    }
}

bool TypeChecker::checkAccessNode(AccessNode &node) {
    auto &recvType = this->checkTypeAsExpr(node.getRecvNode());
    auto *handle = this->symbolTable.lookupField(recvType, node.getFieldName());
    if(handle == nullptr) {
        return false;
    }
    node.setAttribute(*handle);
    node.setType(handle->getType());
    return true;
}

void TypeChecker::resolveCastOp(TypeOpNode &node) {
    auto &exprType = node.getExprNode().getType();
    auto &targetType = node.getType();

    if(node.getType().isVoidType()) {
        node.setOpKind(TypeOpNode::TO_VOID);
        return;
    }

    /**
     * nop
     */
    if(targetType.isSameOrBaseTypeOf(exprType)) {
        return;
    }

    /**
     * number cast
     */
    int beforeIndex = exprType.getNumTypeIndex();
    int afterIndex = targetType.getNumTypeIndex();
    if(beforeIndex > -1 && afterIndex > -1) {
        assert(beforeIndex < 8 && afterIndex < 8);
        node.setOpKind(TypeOpNode::NUM_CAST);
        return;
    }

    if(exprType.isOptionType()) {
        if(targetType.is(TYPE::Boolean)) {
            node.setOpKind(TypeOpNode::CHECK_UNWRAP);
            return;
        }
    } else  {
        if(targetType.is(TYPE::String)) {
            node.setOpKind(TypeOpNode::TO_STRING);
            return;
        }
        if(targetType.is(TYPE::Boolean) && this->symbolTable.lookupMethod(exprType, OP_BOOL) != nullptr) {
            node.setOpKind(TypeOpNode::TO_BOOL);
            return;
        }
        if(!targetType.isNothingType() && exprType.isSameOrBaseTypeOf(targetType)) {
            node.setOpKind(TypeOpNode::CHECK_CAST);
            return;
        }
    }

    RAISE_TC_ERROR(CastOp, node, this->symbolTable.getTypeName(exprType), this->symbolTable.getTypeName(targetType));
}

std::unique_ptr<Node> TypeChecker::newPrintOpNode(std::unique_ptr<Node> &&node) {
    if(!node->getType().isVoidType() && !node->getType().isNothingType()) {
        auto castNode = newTypedCastNode(std::move(node), this->symbolTable.get(TYPE::Void));
        castNode->setOpKind(TypeOpNode::PRINT);
        node = std::move(castNode);
    }
    return std::move(node);
}

// visitor api
void TypeChecker::visitTypeNode(TypeNode &node) {
    auto ret = this->toTypeImpl(node);
    if(ret) {
        node.setType(*ret.take());
    } else {
        throw TypeCheckError(node.getToken(), *ret.asErr());
    }
}

void TypeChecker::visitNumberNode(NumberNode &node) {
    switch(node.kind) {
    case NumberNode::Int:
        node.setType(this->symbolTable.get(TYPE::Int));
        break;
    case NumberNode::Float:
        node.setType(this->symbolTable.get(TYPE::Float));
        break;
    case NumberNode::Signal:
        node.setType(this->symbolTable.get(TYPE::Signal));
        break;
    }
}

void TypeChecker::visitStringNode(StringNode &node) {
    node.setType(this->symbolTable.get(TYPE::String));
}

void TypeChecker::visitStringExprNode(StringExprNode &node) {
    for(auto &exprNode : node.getExprNodes()) {
        this->checkTypeAsExpr(*exprNode);
    }
    node.setType(this->symbolTable.get(TYPE::String));
}

void TypeChecker::visitRegexNode(RegexNode &node) {
    node.setType(this->symbolTable.get(TYPE::Regex));
}

void TypeChecker::visitArrayNode(ArrayNode &node) {
    unsigned int size = node.getExprNodes().size();
    assert(size != 0);
    auto &firstElementNode = node.getExprNodes()[0];
    auto &elementType = this->checkTypeAsSomeExpr(*firstElementNode);

    for(unsigned int i = 1; i < size; i++) {
        this->checkTypeWithCoercion(elementType, node.refExprNodes()[i]);
    }

    auto typeOrError = this->symbolTable.createArrayType(elementType);
    assert(typeOrError);
    node.setType(*typeOrError.take());
}

void TypeChecker::visitMapNode(MapNode &node) {
    unsigned int size = node.getValueNodes().size();
    assert(size != 0);
    auto &firstKeyNode = node.getKeyNodes()[0];
    this->checkTypeAsSomeExpr(*firstKeyNode);
    auto &keyType = this->checkType(this->symbolTable.get(TYPE::_Value), *firstKeyNode);
    auto &firstValueNode = node.getValueNodes()[0];
    auto &valueType = this->checkTypeAsSomeExpr(*firstValueNode);

    for(unsigned int i = 1; i < size; i++) {
        this->checkTypeWithCoercion(keyType, node.refKeyNodes()[i]);
        this->checkTypeWithCoercion(valueType, node.refValueNodes()[i]);
    }

    auto typeOrError = this->symbolTable.createMapType(keyType, valueType);
    assert(typeOrError);
    node.setType(*typeOrError.take());
}

void TypeChecker::visitTupleNode(TupleNode &node) {
    unsigned int size = node.getNodes().size();
    std::vector<DSType *> types(size);
    for(unsigned int i = 0; i < size; i++) {
        types[i] = &this->checkTypeAsSomeExpr(*node.getNodes()[i]);
    }
    auto typeOrError = this->symbolTable.createTupleType(std::move(types));
    assert(typeOrError);
    node.setType(*typeOrError.take());
}

void TypeChecker::visitVarNode(VarNode &node) {
    auto handle = this->symbolTable.lookupHandle(node.getVarName());
    if(handle == nullptr) {
        RAISE_TC_ERROR(UndefinedSymbol, node, node.getVarName().c_str());
    }

    node.setAttribute(*handle);
    node.setType(handle->getType());
}

void TypeChecker::visitAccessNode(AccessNode &node) {
    if(!this->checkAccessNode(node)) {
        RAISE_TC_ERROR(UndefinedField, node.getNameNode(), node.getFieldName().c_str());
    }
}

void TypeChecker::visitTypeOpNode(TypeOpNode &node) {
    auto &exprType = this->checkTypeAsExpr(node.getExprNode());
    auto &targetType = this->checkTypeExactly(*node.getTargetTypeNode());

    if(node.isCastOp()) {
        node.setType(targetType);
        this->resolveCastOp(node);
    } else {
        if(targetType.isSameOrBaseTypeOf(exprType)) {
            node.setOpKind(TypeOpNode::ALWAYS_TRUE);
        } else if(!exprType.isOptionType() && exprType.isSameOrBaseTypeOf(targetType)) {
            node.setOpKind(TypeOpNode::INSTANCEOF);
        } else {
            node.setOpKind(TypeOpNode::ALWAYS_FALSE);
        }
        node.setType(this->symbolTable.get(TYPE::Boolean));
    }
}

void TypeChecker::visitUnaryOpNode(UnaryOpNode &node) {
    auto &exprType = this->checkTypeAsExpr(*node.getExprNode());
    if(node.isUnwrapOp()) {
        if(!exprType.isOptionType()) {
            RAISE_TC_ERROR(Required, *node.getExprNode(), "Option type", this->symbolTable.getTypeName(exprType));
        }
        node.setType(*static_cast<ReifiedType *>(&exprType)->getElementTypes()[0]);
    } else {
        if(exprType.isOptionType()) {
            this->resolveCoercion(this->symbolTable.get(TYPE::Boolean), node.refExprNode());
        }
        auto &applyNode = node.createApplyNode();
        node.setType(this->checkTypeAsExpr(applyNode));
    }
}

void TypeChecker::visitBinaryOpNode(BinaryOpNode &node) {
    if(node.getOp() == COND_AND || node.getOp() == COND_OR) {
        auto &booleanType = this->symbolTable.get(TYPE::Boolean);
        this->checkTypeWithCoercion(booleanType, node.refLeftNode());
        if(node.getLeftNode()->getType().isNothingType()) {
            RAISE_TC_ERROR(Unreachable, *node.getRightNode());
        }

        this->checkTypeWithCoercion(booleanType, node.refRightNode());
        node.setType(booleanType);
        return;
    }

    if(node.getOp() == STR_CHECK) {
        this->checkType(this->symbolTable.get(TYPE::String), *node.getLeftNode());
        this->checkType(this->symbolTable.get(TYPE::String), *node.getRightNode());
        node.setType(this->symbolTable.get(TYPE::String));
        return;
    }

    if(node.getOp() == NULL_COALE) {
        auto &leftType = this->checkTypeAsExpr(*node.getLeftNode());
        if(!leftType.isOptionType()) {
            RAISE_TC_ERROR(Required, *node.getLeftNode(), "Option type", this->symbolTable.getTypeName(leftType));
        }
        auto &elementType = static_cast<ReifiedType &>(leftType).getElementTypes()[0];
        this->checkTypeWithCoercion(*elementType, node.refRightNode());
        node.setType(*elementType);
        return;
    }

    auto &leftType = this->checkTypeAsExpr(*node.getLeftNode());
    auto &rightType = this->checkTypeAsExpr(*node.getRightNode());

    // check referencial equality of func object
    if(leftType.isFuncType() && leftType == rightType
       && (node.getOp() == TokenKind::EQ || node.getOp() == TokenKind::NE)) {
        node.setType(this->symbolTable.get(TYPE::Boolean));
        return;
    }

    // string concatenation
    if(node.getOp() == TokenKind::ADD &&
                (leftType.is(TYPE::String) || rightType.is(TYPE::String))) {
        if(!leftType.is(TYPE::String)) {
            this->resolveCoercion(this->symbolTable.get(TYPE::String), node.refLeftNode());
        }
        if(!rightType.is(TYPE::String)) {
            this->resolveCoercion(this->symbolTable.get(TYPE::String), node.refRightNode());
        }
        node.setType(this->symbolTable.get(TYPE::String));
        return;
    }

    node.createApplyNode();
    node.setType(this->checkTypeAsExpr(*node.getOptNode()));
}

void TypeChecker::checkTypeAsMethodCall(ApplyNode &node, const MethodHandle *handle) {
    // check type argument
    this->checkTypeArgsNode(node, handle, node.refArgNodes());
    node.setHandle(handle);
    node.setType(handle->getReturnType());
}

void TypeChecker::visitApplyNode(ApplyNode &node) {
    /**
     * resolve handle
     */
    HandleOrFuncType hf = this->resolveCallee(node);
    if(is<const MethodHandle *>(hf)) {
        this->checkTypeAsMethodCall(node, get<const MethodHandle *>(hf));
        return;
    }

    // resolve param types
    auto &paramTypes = get<FunctionType *>(hf)->getParamTypes();
    unsigned int size = paramTypes.size();
    unsigned int argSize = node.getArgNodes().size();
    // check param size
    if(size != argSize) {
        RAISE_TC_ERROR(UnmatchParam, node, size, argSize);
    }

    // check type each node
    for(unsigned int i = 0; i < size; i++) {
        this->checkTypeWithCoercion(*paramTypes[i], node.refArgNodes()[i]);
    }

    // set return type
    node.setType(*get<FunctionType *>(hf)->getReturnType());
}

void TypeChecker::visitNewNode(NewNode &node) {
    auto &type = this->checkTypeAsExpr(node.getTargetTypeNode());
    if(type.isOptionType() ||
        this->symbolTable.getTypePool().isArrayType(type) ||
        this->symbolTable.getTypePool().isMapType(type)) {
        unsigned int size = node.getArgNodes().size();
        if(size > 0) {
            RAISE_TC_ERROR(UnmatchParam, node, 0, size);
        }
    } else {
        auto *handle = this->symbolTable.lookupConstructor(type);
        if(handle == nullptr) {
            RAISE_TC_ERROR(UndefinedInit, node, this->symbolTable.getTypeName(type));
        }
        this->checkTypeArgsNode(node, handle, node.refArgNodes());
        node.setHandle(handle);
    }

    node.setType(type);
}

void TypeChecker::visitEmbedNode(EmbedNode &node) {
    auto &exprType = this->checkTypeAsExpr(node.getExprNode());
    node.setType(exprType);

    if(node.getKind() == EmbedNode::STR_EXPR) {
        auto &type = this->symbolTable.get(TYPE::String);
        if(!type.isSameOrBaseTypeOf(exprType)) { // call __INTERP__()
            std::string methodName(OP_INTERP);
            auto *handle = exprType.isOptionType() ? nullptr :
                    this->symbolTable.lookupMethod(exprType, methodName);
            if(handle == nullptr) { // if exprType is
                RAISE_TC_ERROR(UndefinedMethod, node.getExprNode(), methodName.c_str());
            }
            assert(handle->getReturnType() == type);
            node.setHandle(handle);
        }
    } else {
        if(!this->symbolTable.get(TYPE::String).isSameOrBaseTypeOf(exprType) &&
           !this->symbolTable.get(TYPE::StringArray).isSameOrBaseTypeOf(exprType) &&
           !this->symbolTable.get(TYPE::UnixFD).isSameOrBaseTypeOf(exprType)) { // call __STR__ or __CMD__ARG
            // first try lookup __CMD_ARG__ method
            std::string methodName(OP_CMD_ARG);
            auto *handle = this->symbolTable.lookupMethod(exprType, methodName);

            if(handle == nullptr) { // if not found, lookup __STR__
                methodName = OP_STR;
                handle = exprType.isOptionType() ? nullptr :
                        this->symbolTable.lookupMethod(exprType, methodName);
                if(handle == nullptr) {
                    RAISE_TC_ERROR(UndefinedMethod, node.getExprNode(), methodName.c_str());
                }
            }
            assert(handle->getReturnType().is(TYPE::String) || handle->getReturnType().is(TYPE::StringArray));
            node.setHandle(handle);
            node.setType(handle->getReturnType());
        }
    }
}

void TypeChecker::visitCmdNode(CmdNode &node) {
    this->checkType(this->symbolTable.get(TYPE::String), node.getNameNode());
    for(auto &argNode : node.getArgNodes()) {
        this->checkTypeAsExpr(*argNode);
    }
    if(node.getNameNode().getValue() == "exit") {
        node.setType(this->symbolTable.get(TYPE::Nothing));
    } else {
        node.setType(this->symbolTable.get(TYPE::Boolean));
    }
}

void TypeChecker::visitCmdArgNode(CmdArgNode &node) {
    for(auto &exprNode : node.getSegmentNodes()) {
        this->checkTypeAsExpr(*exprNode);
        assert(exprNode->getType().is(TYPE::String) ||
            exprNode->getType().is(TYPE::StringArray) ||
            exprNode->getType().is(TYPE::UnixFD) ||
            exprNode->getType().isNothingType());
    }

    if(node.getGlobPathSize() > UINT8_MAX) {
        RAISE_TC_ERROR(GlobLimit, node);
    }

    // not allow String Array and UnixFD type
    if(node.getSegmentNodes().size() > 1) {
        for(auto &exprNode : node.getSegmentNodes()) {
            this->checkType(nullptr, *exprNode, &this->symbolTable.get(TYPE::StringArray));
            this->checkType(nullptr, *exprNode, &this->symbolTable.get(TYPE::UnixFD));
        }
    }
    assert(!node.getSegmentNodes().empty());
    node.setType(node.getGlobPathSize() > 0
            ? this->symbolTable.get(TYPE::StringArray)
            : node.getSegmentNodes()[0]->getType());
}

void TypeChecker::visitRedirNode(RedirNode &node) {
    auto &argNode = node.getTargetNode();
    this->checkTypeAsExpr(argNode);

    // not allow String Array type
    this->checkType(nullptr, argNode, &this->symbolTable.get(TYPE::StringArray));

    // not UnixFD type, if IOHere
    if(node.isHereStr()) {
        this->checkType(nullptr, argNode, &this->symbolTable.get(TYPE::UnixFD));
    }
    assert(argNode.getType().isNothingType() ||
        argNode.getType().is(TYPE::String) || argNode.getType().is(TYPE::UnixFD));
    node.setType(this->symbolTable.get(TYPE::Any)); //FIXME:
}

void TypeChecker::visitWildCardNode(WildCardNode &node) {
    node.setType(this->symbolTable.get(TYPE::String));
}

void TypeChecker::visitPipelineNode(PipelineNode &node) {
    unsigned int size = node.getNodes().size();
    if(size > 250) {
        RAISE_TC_ERROR(PipeLimit, node);
    }

    {
        auto child = this->inChild();
        for(unsigned int i = 0; i < size - 1; i++) {
            this->checkTypeExactly(*node.getNodes()[i]);
        }
    }

    {
        auto scope = this->inScope();
        if(node.isLastPipe()) { // register pipeline state
            this->addEntry(node, "%%pipe", this->symbolTable.get(TYPE::Any), FieldAttribute::READ_ONLY);
            node.setBaseIndex(this->symbolTable.curScope().getBaseIndex());
        }
        auto &type = this->checkTypeExactly(*node.getNodes()[size - 1]);
        node.setType(node.isLastPipe() ? type : this->symbolTable.get(TYPE::Boolean));
    }
}

void TypeChecker::visitWithNode(WithNode &node) {
    auto scope = this->inScope();

    // register redir config
    this->addEntry(node, "%%redir", this->symbolTable.get(TYPE::Any), FieldAttribute::READ_ONLY);

    auto &type = this->checkTypeExactly(node.getExprNode());
    for(auto &e : node.getRedirNodes()) {
        this->checkTypeAsExpr(*e);
    }

    node.setBaseIndex(this->symbolTable.curScope().getBaseIndex());
    node.setType(type);
}

void TypeChecker::visitForkNode(ForkNode &node) {
    auto child = this->inChild();
    this->checkType(nullptr, node.getExprNode(), node.isJob() ? &this->symbolTable.get(TYPE::Job) : nullptr);

    DSType *type = nullptr;
    switch(node.getOpKind()) {
    case ForkKind::STR:
        type = &this->symbolTable.get(TYPE::String);
        break;
    case ForkKind::ARRAY:
        type = &this->symbolTable.get(TYPE::StringArray);
        break;
    case ForkKind::IN_PIPE:
    case ForkKind::OUT_PIPE:
        type = &this->symbolTable.get(TYPE::UnixFD);
        break;
    case ForkKind::JOB:
    case ForkKind::COPROC:
    case ForkKind::DISOWN:
        type = &this->symbolTable.get(TYPE::Job);
        break;
    }
    node.setType(*type);
}

void TypeChecker::visitAssertNode(AssertNode &node) {
    this->checkTypeWithCoercion(this->symbolTable.get(TYPE::Boolean), node.refCondNode());
    this->checkType(this->symbolTable.get(TYPE::String), node.getMessageNode());
    node.setType(this->symbolTable.get(TYPE::Void));
}

void TypeChecker::visitBlockNode(BlockNode &node) {
    if(this->isTopLevel() && node.getNodes().empty()) {
        RAISE_TC_ERROR(UselessBlock, node);
    }
    auto scope = this->inScope();
    this->checkTypeWithCurrentScope(nullptr, node);
}

void TypeChecker::visitTypeAliasNode(TypeAliasNode &node) {
    if(!this->isTopLevel()) {   // only available toplevel scope
        RAISE_TC_ERROR(OutsideToplevel, node);
    }

    TypeNode &typeToken = node.getTargetTypeNode();
    if(!this->symbolTable.setAlias(node.getAlias(), this->checkTypeExactly(typeToken))) {
        RAISE_TC_ERROR(DefinedSymbol, node, node.getAlias().c_str());
    }
    node.setType(this->symbolTable.get(TYPE::Void));
}

void TypeChecker::visitLoopNode(LoopNode &node) {
    {
        auto scope1 = this->inScope();
        this->checkTypeWithCoercion(this->symbolTable.get(TYPE::Void), node.refInitNode());

        {
            auto scope2 = this->inScope();
            if(node.getInitNode().is(NodeKind::VarDecl)) {
                bool b = this->symbolTable.disallowShadowing(cast<VarDeclNode>(node.getInitNode()).getVarName());
                (void) b;
                assert(b);
            }

            if(node.getCondNode() != nullptr) {
                this->checkTypeWithCoercion(this->symbolTable.get(TYPE::Boolean), node.refCondNode());
            }
            this->checkTypeWithCoercion(this->symbolTable.get(TYPE::Void), node.refIterNode());

            {
                auto loop = this->inLoop();
                this->checkTypeWithCurrentScope(node.getBlockNode());
                auto &type = this->resolveCoercionOfJumpValue();
                node.setType(type);
            }
        }
    }

    if(!node.getBlockNode().getType().isNothingType()) {    // insert continue to block end
        auto jumpNode = JumpNode::newContinue({0, 0});
        jumpNode->setType(this->symbolTable.get(TYPE::Nothing));
        jumpNode->getExprNode().setType(this->symbolTable.get(TYPE::Void));
        node.getBlockNode().setType(jumpNode->getType());
        node.getBlockNode().addNode(std::move(jumpNode));
    }
}

void TypeChecker::visitIfNode(IfNode &node) {
    this->checkTypeWithCoercion(this->symbolTable.get(TYPE::Boolean), node.refCondNode());
    auto &thenType = this->checkTypeExactly(node.getThenNode());
    auto &elseType = this->checkTypeExactly(node.getElseNode());

    if(thenType.isNothingType() && elseType.isNothingType()) {
        node.setType(thenType);
    } else if(thenType.isSameOrBaseTypeOf(elseType)) {
        node.setType(thenType);
    } else if(elseType.isSameOrBaseTypeOf(thenType)) {
        node.setType(elseType);
    } else if(this->checkCoercion(thenType, elseType)) {
        this->checkTypeWithCoercion(thenType, node.refElseNode());
        node.setType(thenType);
    } else if(this->checkCoercion(elseType, thenType)) {
        this->checkTypeWithCoercion(elseType, node.refThenNode());
        node.setType(elseType);
    } else {
        this->checkTypeWithCoercion(this->symbolTable.get(TYPE::Void), node.refThenNode());
        this->checkTypeWithCoercion(this->symbolTable.get(TYPE::Void), node.refElseNode());
        node.setType(this->symbolTable.get(TYPE::Void));
    }
}

bool TypeChecker::IntPatternMap::collect(const Node &constNode) {
    if(constNode.getNodeKind() != NodeKind::Number) {
        return false;
    }
    int64_t value = cast<const NumberNode>(constNode).getIntValue();
    auto pair = this->set.insert(value);
    return pair.second;
}

bool TypeChecker::StrPatternMap::collect(const Node &constNode) {
    if(constNode.getNodeKind() != NodeKind::String) {
        return false;
    }
    const char *str = cast<const StringNode>(constNode).getValue().c_str();
    auto pair = this->set.insert(str);
    return pair.second;
}

bool TypeChecker::PatternCollector::collect(const Node &constNode) {
    if(!this->map) {
        switch(constNode.getNodeKind()) {
        case NodeKind::Number:
            this->map = std::make_unique<IntPatternMap>();
            break;
        case NodeKind::String:
            this->map = std::make_unique<StrPatternMap>();
            break;
        default:
            break;
        }
    }
    return this->map ? this->map->collect(constNode) : false;
}


void TypeChecker::visitCaseNode(CaseNode &node) {
    auto *exprType = &this->checkTypeAsExpr(node.getExprNode());

    // check pattern type
    PatternCollector collector;
    for(auto &e : node.getArmNodes()) {
        this->checkPatternType(*e, collector);
    }

    // check type expr
    node.setCaseKind(collector.getKind());
    auto *patternType = collector.getType();
    if(!patternType) {
        RAISE_TC_ERROR(NeedPattern, node);
    }
    if(exprType->isOptionType()) {
        exprType = static_cast<ReifiedType *>(exprType)->getElementTypes()[0];
    }
    if(!patternType->isSameOrBaseTypeOf(*exprType)) {
        RAISE_TC_ERROR(Required, node.getExprNode(),
                this->symbolTable.getTypeName(*patternType), this->symbolTable.getTypeName(*exprType));
    }

    // resolve arm expr type
    unsigned int size = node.getArmNodes().size();
    std::vector<DSType *> types(size);
    for(unsigned int i = 0; i < size; i++) {
        types[i] = &this->checkTypeExactly(*node.getArmNodes()[i]);
    }
    auto &type = this->resolveCommonSuperType(types);

    // apply coercion
    for(auto &armNode : node.getArmNodes()) {
        this->checkTypeWithCoercion(type, armNode->refActionNode());
        armNode->setType(type);
    }

    if(!type.isVoidType() && !collector.hasElsePattern()) {
        RAISE_TC_ERROR(NeedDefault, node);
    }
    node.setType(type);
}

void TypeChecker::visitArmNode(ArmNode &node) {
    auto &type = this->checkTypeExactly(node.getActionNode());
    node.setType(type);
}

void TypeChecker::checkPatternType(ArmNode &node, PatternCollector &collector) {
    if(node.getPatternNodes().empty()) {
        if(collector.hasElsePattern()) {
            Token token{node.getPos(), 4};
            auto elseNode = std::make_unique<StringNode>(token, "else");
            RAISE_TC_ERROR(DupPattern, *elseNode);
        }
        collector.setElsePattern(true);
    }

    for(auto &e : node.getPatternNodes()) {
        auto *type = &this->checkTypeAsExpr(*e);
        if(type->is(TYPE::Regex)) {
            collector.setKind(CaseNode::IF_ELSE);
            type = &this->symbolTable.get(TYPE::String);
        }
        if(collector.getType() == nullptr) {
            collector.setType(type);
        }
        if(*collector.getType() != *type) {
            RAISE_TC_ERROR(Required, *e, this->symbolTable.getTypeName(*collector.getType()),
                    this->symbolTable.getTypeName(*type));
        }
    }

    for(auto &e : node.refPatternNodes()) {
        this->applyConstFolding(e);
    }

    for(auto &e : node.getPatternNodes()) {
        if(e->is(NodeKind::Regex)) {
            continue;
        }
        if(!collector.collect(*e)) {
            RAISE_TC_ERROR(DupPattern, *e);
        }
    }
}

DSType& TypeChecker::resolveCommonSuperType(const std::vector<DSType *> &types) {
    for(auto &type : types) {
        unsigned int size = types.size();
        unsigned int index = 0;
        for(; index < size; index++) {
            auto &curType = types[index];
            if(type == curType) {
                continue;
            }

            if(!this->checkCoercion(*type, *curType)) {
                break;
            }
        }
        if(index == size) {
            return *type;
        }
    }
    return this->symbolTable.get(TYPE::Void);
}

static auto initConstVarMap() {
    struct utsname name{};
    if(uname(&name) == -1) {
        fatal_perror("cannot get utsname");
    }

    CStringHashMap<std::string> map = {
            {CVAR_VERSION, X_INFO_VERSION_CORE},
            {CVAR_CONFIG_DIR, SYSTEM_CONFIG_DIR},
            {CVAR_OSTYPE, name.sysname},
            {CVAR_MACHTYPE, name.machine},
    };
    return map;
}

bool TypeChecker::applyConstFolding(std::unique_ptr<Node> &node) const {
    switch(node->getNodeKind()) {
    case NodeKind::String:
    case NodeKind::Number:
    case NodeKind::Regex:
    case NodeKind::WildCard:
        return true;
    case NodeKind::UnaryOp: { // !, +, -, !
        auto &unaryNode = cast<UnaryOpNode>(*node);
        Token token = node->getToken();
        const auto op = unaryNode.getOp();
        if(node->getType().is(TYPE::Int) && (op == MINUS || op == PLUS || op == NOT)) {
            auto &applyNode = unaryNode.refApplyNode();
            assert(applyNode->getExprNode().is(NodeKind::Access));
            auto &accessNode = cast<AccessNode>(applyNode->getExprNode());
            auto &recvNode = accessNode.refRecvNode();
            if(!this->applyConstFolding(recvNode)) {
                break;
            }
            assert(isa<NumberNode>(*recvNode));
            int64_t value = cast<NumberNode>(*recvNode).getIntValue();
            if(op == MINUS) {
                value = -value;
            } else if(op == NOT) {
                uint64_t v = ~static_cast<uint64_t>(value);
                value = static_cast<int64_t>(v);
            }
            node = NumberNode::newInt(token, value);
            node->setType(this->symbolTable.get(TYPE::Int));
            return true;
        }
        break;
    }
    case NodeKind::StringExpr: {
        auto &exprNode = cast<StringExprNode>(*node);
        Token token = node->getToken();
        std::string value;
        for(auto &e : exprNode.refExprNodes()) {
            if(!this->applyConstFolding(e)) {
                break;
            }
            assert(isa<StringNode>(*e));
            value += cast<StringNode>(*e).getValue();
        }
        node = std::make_unique<StringNode>(token, std::move(value));
        node->setType(this->symbolTable.get(TYPE::String));
        return true;
    }
    case NodeKind::Embed: {
        auto &embedNode = cast<EmbedNode>(*node);
        if(!embedNode.getHandle() && this->applyConstFolding(embedNode.refExprNode())) {
            node = std::move(embedNode.refExprNode());
            return true;
        }
        break;
    }
    case NodeKind::Var: {
        assert(this->lexer);
        static const auto constMap = initConstVarMap();
        auto &varNode = cast<VarNode>(*node);
        Token token = varNode.getToken();
        std::string value;
        if(hasFlag(varNode.attr(), FieldAttribute::MOD_CONST)) {
            if(varNode.getVarName() == CVAR_SCRIPT_NAME) {
                value = this->lexer->getSourceName();
            } else if(varNode.getVarName() == CVAR_SCRIPT_DIR) {
                value = this->lexer->getScriptDir();
            } else {
                break;
            }
        } else {
            auto iter = constMap.find(varNode.getVarName().c_str());
            if(iter == constMap.end()) {
                break;
            }
            value = iter->second;
        }
        assert(varNode.getType().is(TYPE::String));
        node = std::make_unique<StringNode>(token, std::move(value));
        node->setType(this->symbolTable.get(TYPE::String));
        return true;
    }
    default:
        break;
    }
    RAISE_TC_ERROR(Constant, *node);
}

void TypeChecker::checkTypeAsBreakContinue(JumpNode &node) {
    if(this->fctx.loopLevel() == 0) {
        RAISE_TC_ERROR(InsideLoop, node);
    }

    if(this->fctx.finallyLevel() > this->fctx.loopLevel()) {
        RAISE_TC_ERROR(InsideFinally, node);
    }

    if(this->fctx.childLevel() > this->fctx.loopLevel()) {
        RAISE_TC_ERROR(InsideChild, node);
    }

    if(this->fctx.tryCatchLevel() > this->fctx.loopLevel()) {
        node.setLeavingBlock(true);
    }

    if(node.getExprNode().is(NodeKind::Empty)) {
        this->checkType(this->symbolTable.get(TYPE::Void), node.getExprNode());
    } else if(node.getOpKind() == JumpNode::BREAK_) {
        this->checkTypeAsSomeExpr(node.getExprNode());
        this->breakGather.addJumpNode(&node);
    }
    assert(!node.getExprNode().isUntyped());
}

void TypeChecker::checkTypeAsReturn(JumpNode &node) {
    if(this->fctx.finallyLevel() > 0) {
        RAISE_TC_ERROR(InsideFinally, node);
    }

    if(this->fctx.childLevel() > 0) {
        RAISE_TC_ERROR(InsideChild, node);
    }

    auto *returnType = this->getCurrentReturnType();
    if(returnType == nullptr) {
        RAISE_TC_ERROR(InsideFunc, node);
    }
    auto &exprType = this->checkType(*returnType, node.getExprNode());
    if(exprType.isVoidType()) {
        if(!node.getExprNode().is(NodeKind::Empty)) {
            RAISE_TC_ERROR(NotNeedExpr, node);
        }
    }
}

void TypeChecker::visitJumpNode(JumpNode &node) {
    switch(node.getOpKind()) {
    case JumpNode::BREAK_:
    case JumpNode::CONTINUE_:
        this->checkTypeAsBreakContinue(node);
        break;
    case JumpNode::THROW_: {
        if(this->fctx.finallyLevel() > 0) {
            RAISE_TC_ERROR(InsideFinally, node);
        }
        this->checkType(this->symbolTable.get(TYPE::Any), node.getExprNode());
        break;
    }
    case JumpNode::RETURN_: {
        this->checkTypeAsReturn(node);
        break;
    }
    }
    node.setType(this->symbolTable.get(TYPE::Nothing));
}

void TypeChecker::visitCatchNode(CatchNode &node) {
    auto &exceptionType = this->checkTypeAsSomeExpr(node.getTypeNode());
    /**
     * not allow Void, Nothing and Option type.
     */
    if(exceptionType.isOptionType()) {
        RAISE_TC_ERROR(Unacceptable, node.getTypeNode(), this->symbolTable.getTypeName(exceptionType));
    }

    {
        auto scope = this->inScope();
        /**
         * check type catch block
         */
        auto handle = this->addEntry(node, node.getExceptionName(), exceptionType, FieldAttribute::READ_ONLY);
        node.setAttribute(*handle);
        this->checkTypeWithCurrentScope(nullptr, node.getBlockNode());
    }
    node.setType(node.getBlockNode().getType());
}

void TypeChecker::visitTryNode(TryNode &node) {
    if(node.getCatchNodes().empty() && node.getFinallyNode() == nullptr) {
        RAISE_TC_ERROR(MeaninglessTry, node);
    }
    assert(node.getExprNode().is(NodeKind::Block));
    if(cast<BlockNode>(node.getExprNode()).getNodes().empty()) {
        RAISE_TC_ERROR(EmptyTry, node.getExprNode());
    }

    // check type try block
    const DSType *exprType = nullptr;
    {
        auto try1 = this->inTry();
        exprType = &this->checkTypeExactly(node.getExprNode());
    }

    // check type catch block
    for(auto &c : node.getCatchNodes()) {
        auto try1 = this->inTry();
        auto &catchType = this->checkTypeExactly(*c);
        if(!exprType->isSameOrBaseTypeOf(catchType) && !this->checkCoercion(*exprType, catchType)) {
            exprType = &this->symbolTable.get(TYPE::Void);
        }
    }

    // perform coercion
    this->checkTypeWithCoercion(*exprType, node.refExprNode());
    for(auto &c : node.refCatchNodes()) {
        this->checkTypeWithCoercion(*exprType, c);
    }

    // check type finally block, may be empty node
    if(node.getFinallyNode() != nullptr) {
        auto finally1 = this->inFinally();
        this->checkTypeWithCoercion(this->symbolTable.get(TYPE::Void), node.refFinallyNode());

        if(findInnerNode<BlockNode>(node.getFinallyNode())->getNodes().empty()) {
            RAISE_TC_ERROR(UselessBlock, *node.getFinallyNode());
        }
        if(node.getFinallyNode()->getType().isNothingType()) {
            RAISE_TC_ERROR(InsideFinally, *node.getFinallyNode());
        }
    }

    /**
     * verify catch block order
     */
    const int size = node.getCatchNodes().size();
    for(int i = 0; i < size - 1; i++) {
        auto &curType = findInnerNode<CatchNode>(node.getCatchNodes()[i].get())->getTypeNode().getType();
        auto &nextType = findInnerNode<CatchNode>(node.getCatchNodes()[i + 1].get())->getTypeNode().getType();
        if(curType.isSameOrBaseTypeOf(nextType)) {
            auto &nextNode = node.getCatchNodes()[i + 1];
            RAISE_TC_ERROR(Unreachable, *nextNode);
        }
    }
    node.setType(*exprType);
}

void TypeChecker::visitVarDeclNode(VarDeclNode &node) {
    DSType *exprType = nullptr;
    FieldAttribute attr{};
    switch(node.getKind()) {
    case VarDeclNode::CONST:
    case VarDeclNode::VAR:
        if(node.getKind() == VarDeclNode::CONST) {
            setFlag(attr, FieldAttribute::READ_ONLY);
        }
        exprType = &this->checkTypeAsSomeExpr(*node.getExprNode());
        break;
    case VarDeclNode::IMPORT_ENV:
    case VarDeclNode::EXPORT_ENV:
        setFlag(attr, FieldAttribute::ENV);
        exprType = &this->symbolTable.get(TYPE::String);
        if(node.getExprNode() != nullptr) {
            this->checkType(*exprType, *node.getExprNode());
        }
        break;
    }

    auto handle = this->addEntry(node, node.getVarName(), *exprType, attr);
    node.setAttribute(*handle);
    node.setType(this->symbolTable.get(TYPE::Void));
}

void TypeChecker::visitAssignNode(AssignNode &node) {
    if(!isAssignable(node.getLeftNode())) {
        RAISE_TC_ERROR(Assignable, node.getLeftNode());
    }
    auto &leftNode = static_cast<AssignableNode&>(node.getLeftNode());
    auto &leftType = this->checkTypeAsExpr(leftNode);
    if(hasFlag(leftNode.attr(), FieldAttribute::READ_ONLY)) {
        RAISE_TC_ERROR(ReadOnly, leftNode);
    }

    if(leftNode.is(NodeKind::Access)) {
        node.setAttribute(AssignNode::FIELD_ASSIGN);
    }
    if(node.isSelfAssignment()) {
        assert(node.getRightNode().is(NodeKind::BinaryOp));
        auto &opNode = cast<BinaryOpNode>(node.getRightNode());
        opNode.getLeftNode()->setType(leftType);
        if(isa<AccessNode>(leftNode)) {
            cast<AccessNode>(leftNode).setAdditionalOp(AccessNode::DUP_RECV);
        }
        auto &rightType = this->checkTypeAsExpr(node.getRightNode());
        if(leftType != rightType) { // convert right hand-side type to left type
            this->resolveCoercion(leftType, node.refRightNode());
        }
    } else {
        this->checkTypeWithCoercion(leftType, node.refRightNode());
    }

    node.setType(this->symbolTable.get(TYPE::Void));
}

void TypeChecker::visitElementSelfAssignNode(ElementSelfAssignNode &node) {
    auto &recvType = this->checkTypeAsExpr(node.getRecvNode());
    auto &indexType = this->checkTypeAsExpr(node.getIndexNode());

    node.setRecvType(recvType);
    node.setIndexType(indexType);

    auto &elementType = this->checkTypeAsExpr(node.getGetterNode());
    cast<BinaryOpNode>(node.getRightNode()).getLeftNode()->setType(elementType);

    // convert right hand-side type to element type
    auto &rightType = this->checkTypeAsExpr(node.getRightNode());
    if(elementType != rightType) {
        this->resolveCoercion(elementType, node.refRightNode());
    }

    node.getSetterNode().getArgNodes()[1]->setType(elementType);
    this->checkType(this->symbolTable.get(TYPE::Void), node.getSetterNode());

    node.setType(this->symbolTable.get(TYPE::Void));
}

static void addReturnNodeToLast(BlockNode &blockNode, const TypePool &pool, std::unique_ptr<Node> exprNode) {
    assert(!blockNode.isUntyped() && !blockNode.getType().isNothingType());
    assert(!exprNode->isUntyped());

    auto returnNode = JumpNode::newReturn(exprNode->getToken(), std::move(exprNode));
    returnNode->setType(*pool.get(TYPE::Nothing));
    blockNode.setType(returnNode->getType());
    blockNode.addNode(std::move(returnNode));
}

void TypeChecker::visitFunctionNode(FunctionNode &node) {
    if(!this->isTopLevel()) {   // only available toplevel scope
        RAISE_TC_ERROR(OutsideToplevel, node);
    }

    // resolve return type, param type
    auto &returnType = this->checkTypeExactly(node.getReturnTypeToken());
    unsigned int paramSize = node.getParamTypeNodes().size();
    std::vector<DSType *> paramTypes(paramSize);
    for(unsigned int i = 0; i < paramSize; i++) {
        auto &type = this->checkTypeAsSomeExpr(*node.getParamTypeNodes()[i]);
        paramTypes[i] = &type;
    }

    // register function handle
    auto typeOrError = this->symbolTable.createFuncType(&returnType, std::move(paramTypes));
    assert(typeOrError);
    auto &funcType = static_cast<FunctionType&>(*typeOrError.take());
    node.setFuncType(&funcType);
    auto handle = this->addEntry(node, node.getFuncName(), funcType,
                                 FieldAttribute::FUNC_HANDLE | FieldAttribute::READ_ONLY);
    node.setVarIndex(handle->getIndex());

    {
        auto func = this->inFunc(returnType);
        // register parameter
        for(unsigned int i = 0; i < paramSize; i++) {
            VarNode &paramNode = *node.getParamNodes()[i];
            auto fieldHandle = this->addEntry(paramNode, paramNode.getVarName(),
                                              *funcType.getParamTypes()[i], FieldAttribute());
            paramNode.setAttribute(*fieldHandle);
        }
        // check type func body
        this->checkTypeWithCurrentScope(node.getBlockNode());
        node.setMaxVarNum(this->symbolTable.getMaxVarIndex());
    }

    // insert terminal node if not found
    BlockNode &blockNode = node.getBlockNode();
    if(returnType.isVoidType() && !blockNode.getType().isNothingType()) {
        auto emptyNode = std::make_unique<EmptyNode>();
        emptyNode->setType(this->symbolTable.get(TYPE::Void));
        addReturnNodeToLast(blockNode, this->symbolTable.getTypePool(), std::move(emptyNode));
    }
    if(!blockNode.getType().isNothingType()) {
        RAISE_TC_ERROR(UnfoundReturn, blockNode);
    }

    node.setType(this->symbolTable.get(TYPE::Void));
}

void TypeChecker::visitUserDefinedCmdNode(UserDefinedCmdNode &node) {
    if(!this->isTopLevel()) {   // only available toplevel scope
        RAISE_TC_ERROR(OutsideToplevel, node);
    }

    // register command name
    auto pair = this->symbolTable.registerUdc(node.getCmdName(), this->symbolTable.get(TYPE::Any));
    if(!pair && pair.asErr() == SymbolError::DEFINED) {
        RAISE_TC_ERROR(DefinedCmd, node, node.getCmdName().c_str());
    }
    node.setUdcIndex(pair.asOk()->getIndex());

    {
        auto func = this->inFunc(this->symbolTable.get(TYPE::Int)); // pseudo return type
        // register dummy parameter (for propagating command attr)
        this->addEntry(node, "%%attr", this->symbolTable.get(TYPE::Any), FieldAttribute::READ_ONLY);

        // register dummy parameter (for closing file descriptor)
        this->addEntry(node, "%%redir", this->symbolTable.get(TYPE::Any), FieldAttribute::READ_ONLY);

        // register special characters (@, #, 0, 1, ... 9)
        this->addEntry(node, "@", this->symbolTable.get(TYPE::StringArray), FieldAttribute::READ_ONLY);
        this->addEntry(node, "#", this->symbolTable.get(TYPE::Int), FieldAttribute::READ_ONLY);
        for(unsigned int i = 0; i < 10; i++) {
            this->addEntry(node, std::to_string(i), this->symbolTable.get(TYPE::String), FieldAttribute::READ_ONLY);
        }

        // check type command body
        this->checkTypeWithCurrentScope(node.getBlockNode());
        node.setMaxVarNum(this->symbolTable.getMaxVarIndex());
    }

    // insert return node if not found
    if(node.getBlockNode().getNodes().empty() ||
            !node.getBlockNode().getNodes().back()->getType().isNothingType()) {
        auto varNode = std::make_unique<VarNode>(Token{0, 1}, "?");
        this->checkTypeAsExpr(*varNode);
        addReturnNodeToLast(node.getBlockNode(), this->symbolTable.getTypePool(), std::move(varNode));
    }

    node.setType(this->symbolTable.get(TYPE::Void));
}

void TypeChecker::visitInterfaceNode(InterfaceNode &node) {
//    if(!this->isTopLevel()) {   // only available toplevel scope
//        RAISE_TC_ERROR(OutsideToplevel, node);
//    }
    RAISE_TC_ERROR(OutsideToplevel, node);
}

void TypeChecker::visitSourceNode(SourceNode &node) {
    assert(this->isTopLevel());

    // register module placeholder
    const FieldHandle *handle = nullptr;
    if(node.isFirstAppear()) {
        auto pair = this->symbolTable.newModHandle(node.getModType());
        assert(static_cast<bool>(pair));
        handle = pair.asOk();
    }
    if(handle == nullptr) {
        handle = this->symbolTable.lookupModHandle(node.getModType());
        assert(handle != nullptr);
    }

    // register actual module handle
    node.setModIndex(handle->getIndex());
    if(node.getName().empty()) {    // global import
        const char *ret = this->symbolTable.import(node.getModType());
        if(ret) {
            RAISE_TC_ERROR(ConflictSymbol, node, ret);
        }
    } else {    // scoped import
        // register actual module handle
        handle = this->addEntry(node, node.getName(), node.getModType(), FieldAttribute::READ_ONLY);
        assert(handle != nullptr);
        node.setIndex(handle->getIndex());
    }
    node.setType(this->symbolTable.get(node.isNothing() ? TYPE::Nothing : TYPE::Void));
}

void TypeChecker::resolvePathList(SourceListNode &node) {
    std::vector<std::string> ret;
    if(node.getPathNode().getGlobPathSize() == 0) {
        std::string path;
        for(auto &e : node.getPathNode().getSegmentNodes()) {
            assert(isa<StringNode>(*e));
            path += cast<StringNode>(*e).getValue();
        }

        auto &first = *node.getPathNode().getSegmentNodes()[0];
        if(isa<StringNode>(first) && cast<StringNode>(first).isTilde()) {
            expandTilde(path);
        }
        ret.push_back(std::move(path));
    } else {
        fatal("unsupported\n");
    }
    node.setPathList(std::move(ret));
}

void TypeChecker::visitSourceListNode(SourceListNode &node) {
    if(!this->isTopLevel()) {   // only available toplevel scope
        RAISE_TC_ERROR(OutsideToplevel, node);
    }
    this->checkType(this->symbolTable.get(TYPE::String), node.getPathNode());
    for(auto &e : node.getPathNode().refSegmentNodes()) {
        this->applyConstFolding(e);
        assert(isa<StringNode>(*e) || isa<WildCardNode>(*e));
        if(isa<StringNode>(*e)) {
            auto ref = StringRef(cast<StringNode>(*e).getValue());
            if(ref.find(StringRef("\0",  1)) != StringRef::npos) {
                RAISE_TC_ERROR(NullInPath, node.getPathNode());
            }
        }
    }
    this->resolvePathList(node);
    node.setType(this->symbolTable.get(TYPE::Void));
}

void TypeChecker::visitEmptyNode(EmptyNode &node) {
    node.setType(this->symbolTable.get(TYPE::Void));
}

static bool mayBeCmd(const Node &node) {
    if(node.is(NodeKind::Cmd)) {
        return true;
    }
    if(node.is(NodeKind::Pipeline)) {
        if(cast<const PipelineNode>(node).getNodes().back()->is(NodeKind::Cmd)) {
            return true;
        }
    }
    return false;
}

std::unique_ptr<Node> TypeChecker::operator()(const DSType *prevType, std::unique_ptr<Node> &&node) {
    if(prevType != nullptr && prevType->isNothingType()) {
        RAISE_TC_ERROR(Unreachable, *node);
    }

    if(this->toplevelPrinting && this->symbolTable.isRootModule() && !mayBeCmd(*node)) {
        this->checkTypeExactly(*node);
        node = this->newPrintOpNode(std::move(node));
    } else {
        this->checkTypeWithCoercion(this->symbolTable.get(TYPE::Void), node);// pop stack top
    }
    return std::move(node);
}

} // namespace ydsh
