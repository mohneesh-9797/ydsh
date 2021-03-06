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

#include <pwd.h>

#include <cstdarg>
#include <cerrno>

#include "constant.h"
#include "object.h"
#include "node.h"
#include "symbol_table.h"

// helper macro for node dumper
#define NAME(f) #f

#define DUMP(field) dumper.dump(NAME(field), field)
#define DUMP_PTR(field) \
    do {\
        if((field) == nullptr) {\
            dumper.dumpNull(NAME(field));\
        } else {\
            dumper.dump(NAME(field), *(field));\
        }\
    } while(false)


// not directly use it.
#define GEN_ENUM_STR(ENUM) case ENUM: ___str__ = #ENUM; break;

#define DUMP_ENUM(val, EACH_ENUM) \
    do {\
        const char *___str__ = nullptr;\
        switch(val) {\
        EACH_ENUM(GEN_ENUM_STR)\
        }\
        dumper.dump(NAME(val), ___str__);\
    } while(false)

// not directly use it.
#define GEN_FLAG_STR(FLAG) \
        if((___set__ & (FLAG))) { if(___count__++ > 0) { ___str__ += " | "; } ___str__ += #FLAG; }

#define DUMP_BITSET(val, EACH_FLAG) \
    do {\
        unsigned int ___count__ = 0;\
        std::string ___str__;\
        auto ___set__ = val;\
        EACH_FLAG(GEN_FLAG_STR)\
        dumper.dump(NAME(val), ___str__);\
    } while(false)



namespace ydsh {

// ##################
// ##     Node     ##
// ##################

void Node::accept(NodeVisitor &visitor) {
    switch(this->nodeKind) {
#define DISPATCH(K) case NodeKind::K: visitor.visit ## K ## Node(*cast<K ## Node>(this)); break;
    EACH_NODE_KIND(DISPATCH)
#undef DISPATCH
    }
}

// ######################
// ##     TypeNode     ##
// ######################

void TypeNode::dump(NodeDumper &dumper) const {
    DUMP_ENUM(typeKind, EACH_TYPE_NODE_KIND);
}


// ##########################
// ##     BaseTypeNode     ##
// ##########################

void BaseTypeNode::dump(NodeDumper &dumper) const {
    TypeNode::dump(dumper);
    DUMP(typeName);
}

// #############################
// ##     ReifiedTypeNode     ##
// #############################

void ReifiedTypeNode::dump(NodeDumper &dumper) const {
    TypeNode::dump(dumper);
    DUMP_PTR(templateTypeNode);
    DUMP(elementTypeNodes);
}

// ##########################
// ##     FuncTypeNode     ##
// ##########################

void FuncTypeNode::dump(NodeDumper &dumper) const {
    TypeNode::dump(dumper);
    DUMP_PTR(returnTypeNode);
    DUMP(paramTypeNodes);
}

// ############################
// ##     ReturnTypeNode     ##
// ############################

void ReturnTypeNode::dump(NodeDumper &dumper) const {
    TypeNode::dump(dumper);
    DUMP(typeNodes);
}

// ########################
// ##     TypeOfNode     ##
// ########################

void TypeOfNode::dump(NodeDumper &dumper) const {
    TypeNode::dump(dumper);
    DUMP_PTR(exprNode);
}


// ########################
// ##     NumberNode     ##
// ########################

void NumberNode::dump(NodeDumper &dumper) const {
    DUMP_ENUM(kind, EACH_NUMBER_NODE_KIND);

    switch(this->kind) {
    case Int:
        DUMP(intValue);
        break;
    case Float:
        DUMP(floatValue);
        break;
    case Signal:
        DUMP(intValue);
        break;
    }
}

// #######################
// ##     StringNode    ##
// #######################

void StringNode::dump(NodeDumper &dumper) const {
#define EACH_ENUM(OP) \
    OP(STRING) \
    OP(TILDE)

    DUMP_ENUM(kind, EACH_ENUM);

#undef EACH_ENUM

    DUMP(value);
}

// ############################
// ##     StringExprNode     ##
// ############################

void StringExprNode::dump(NodeDumper &dumper) const {
    DUMP(nodes);
}

// #######################
// ##     RegexNode     ##
// #######################

void RegexNode::dump(NodeDumper &dumper) const {
    DUMP(reStr);
}

// #######################
// ##     ArrayNode     ##
// #######################

void ArrayNode::dump(NodeDumper &dumper) const {
    DUMP(nodes);
}

// #####################
// ##     MapNode     ##
// #####################

void MapNode::addEntry(std::unique_ptr<Node> &&keyNode, std::unique_ptr<Node> &&valueNode) {
    this->keyNodes.push_back(std::move(keyNode));
    this->valueNodes.push_back(std::move(valueNode));
}

void MapNode::dump(NodeDumper &dumper) const {
    DUMP(keyNodes);
    DUMP(valueNodes);
}

// #######################
// ##     TupleNode     ##
// #######################

void TupleNode::dump(NodeDumper &dumper) const {
    DUMP(nodes);
}

// ############################
// ##     AssignableNode     ##
// ############################

void AssignableNode::dump(NodeDumper &dumper) const {
    DUMP(index);
    dumper.dump("attribute", toString(this->attribute));
}

// #####################
// ##     VarNode     ##
// #####################

void VarNode::dump(NodeDumper &dumper) const {
    DUMP(varName);
    AssignableNode::dump(dumper);
}

// ########################
// ##     AccessNode     ##
// ########################

void AccessNode::dump(NodeDumper &dumper) const {
    DUMP_PTR(recvNode);
    DUMP_PTR(nameNode);
    AssignableNode::dump(dumper);

#define EACH_ENUM(OP) \
    OP(NOP) \
    OP(DUP_RECV)

    DUMP_ENUM(additionalOp, EACH_ENUM);
#undef EACH_ENUM
}

// ########################
// ##     TypeOpNode     ##
// ########################

std::unique_ptr<TypeOpNode> newTypedCastNode(std::unique_ptr<Node> &&targetNode, const DSType &type) {
    assert(!targetNode->isUntyped());
    auto castNode = std::make_unique<TypeOpNode>(std::move(targetNode), nullptr, TypeOpNode::NO_CAST);
    castNode->setType(type);
    return castNode;
}

void TypeOpNode::dump(NodeDumper &dumper) const {
    DUMP_PTR(exprNode);
    auto *targetTypeToken = this->getTargetTypeNode();
    DUMP_PTR(targetTypeToken);

#define EACH_ENUM(OP) \
    OP(NO_CAST) \
    OP(TO_VOID) \
    OP(NUM_CAST) \
    OP(TO_STRING) \
    OP(TO_BOOL) \
    OP(CHECK_CAST) \
    OP(CHECK_UNWRAP) \
    OP(PRINT) \
    OP(ALWAYS_FALSE) \
    OP(ALWAYS_TRUE) \
    OP(INSTANCEOF)

    DUMP_ENUM(opKind, EACH_ENUM);
#undef EACH_ENUM
}

// #######################
// ##     ApplyNode     ##
// #######################

ApplyNode::ApplyNode(std::unique_ptr<Node> &&exprNode, std::vector<std::unique_ptr<Node>> &&argNodes, Kind kind) :
        WithRtti(exprNode->getToken()),
        exprNode(std::move(exprNode)), argNodes(std::move(argNodes)), kind(kind) {
    if(!this->argNodes.empty()) {
        this->updateToken(this->argNodes.back()->getToken());
    }
}

std::unique_ptr<ApplyNode> ApplyNode::newMethodCall(std::unique_ptr<Node> &&recvNode, Token token, std::string &&methodName) {
    auto exprNode = std::make_unique<AccessNode>(
            std::move(recvNode), std::make_unique<VarNode>(token, std::move(methodName)));
    return std::make_unique<ApplyNode>(std::move(exprNode), std::vector<std::unique_ptr<Node>>(), METHOD_CALL);
}

void ApplyNode::dump(NodeDumper &dumper) const {
    DUMP_PTR(exprNode);
    DUMP(argNodes);
    DUMP_PTR(handle);

#define EACH_ENUM(OP) \
    OP(UNRESOLVED) \
    OP(FUNC_CALL) \
    OP(METHOD_CALL) \
    OP(INDEX_CALL)

    DUMP_ENUM(kind, EACH_ENUM);
#undef EACH_ENUM
}

// #####################
// ##     NewNode     ##
// #####################

NewNode::NewNode(unsigned int startPos, std::unique_ptr<TypeNode> &&targetTypeNode,
        std::vector<std::unique_ptr<Node>> &&argNodes) :
        WithRtti({startPos, 0}), targetTypeNode(std::move(targetTypeNode)), argNodes(std::move(argNodes)) {
    if(!this->argNodes.empty()) {
        this->updateToken(this->argNodes.back()->getToken());
    }
}

void NewNode::dump(NodeDumper &dumper) const {
    DUMP_PTR(targetTypeNode);
    DUMP(argNodes);
    DUMP_PTR(handle);
}

// #######################
// ##     EmbedNode     ##
// #######################

void EmbedNode::dump(ydsh::NodeDumper &dumper) const {
#define EACH_ENUM(OP) \
    OP(STR_EXPR) \
    OP(CMD_ARG)

    DUMP_ENUM(kind, EACH_ENUM);
#undef EACH_ENUM

    DUMP_PTR(exprNode);
    DUMP_PTR(handle);
}


// #########################
// ##     UnaryOpNode     ##
// #########################

ApplyNode &UnaryOpNode::createApplyNode() {
    this->methodCallNode = ApplyNode::newMethodCall(
            std::move(this->exprNode), this->opToken, resolveUnaryOpName(this->op));
    return *this->methodCallNode;
}

void UnaryOpNode::dump(NodeDumper &dumper) const {
    DUMP(op);
    DUMP_PTR(exprNode);
    DUMP_PTR(methodCallNode);
}


// ##########################
// ##     BinaryOpNode     ##
// ##########################

void BinaryOpNode::createApplyNode() {
    auto applyNode = ApplyNode::newMethodCall(std::move(this->leftNode), this->opToken, resolveBinaryOpName(this->op));
    applyNode->refArgNodes().push_back(std::move(this->rightNode));
    this->setOptNode(std::move(applyNode));
}

void BinaryOpNode::dump(NodeDumper &dumper) const {
    DUMP_PTR(leftNode);
    DUMP_PTR(rightNode);
    DUMP(op);
    DUMP_PTR(optNode);
}


// ########################
// ##     CmdArgNode     ##
// ########################

void CmdArgNode::addSegmentNode(std::unique_ptr<Node> &&node) {
    if(isa<WildCardNode>(*node)) {
        if(this->globPathSize == 0 && !this->segmentNodes.empty()) {
            this->globPathSize++;
        }
        this->globPathSize++;
    } else if(!this->segmentNodes.empty() &&
        isa<WildCardNode>(*this->segmentNodes.back())) {
        this->globPathSize++;
    }
    this->updateToken(node->getToken());
    this->segmentNodes.push_back(std::move(node));
}

void CmdArgNode::dump(NodeDumper &dumper) const {
    DUMP(globPathSize);
    DUMP(segmentNodes);
}

bool CmdArgNode::isIgnorableEmptyString() const {
    return this->segmentNodes.size() > 1 ||
            (!isa<StringNode>(*this->segmentNodes.back()) &&
                    !isa<StringExprNode>(*this->segmentNodes.back()));
}

// #######################
// ##     RedirNode     ##
// #######################

void RedirNode::dump(NodeDumper &dumper) const {
    DUMP(op);
    DUMP_PTR(targetNode);
}

// ##########################
// ##     WildCardNode     ##
// ##########################

void WildCardNode::dump(NodeDumper &dumper) const {
    dumper.dump("meta", toString(meta));
}

// #####################
// ##     CmdNode     ##
// #####################

void CmdNode::addArgNode(std::unique_ptr<CmdArgNode> &&node) {
    this->updateToken(node->getToken());
    this->argNodes.push_back(std::move(node));
}

void CmdNode::addRedirNode(std::unique_ptr<RedirNode> &&node) {
    this->updateToken(node->getToken());
    this->argNodes.push_back(std::move(node));
    this->redirCount++;
}

void CmdNode::dump(NodeDumper &dumper) const {
    DUMP_PTR(nameNode);
    DUMP(argNodes);
    DUMP(redirCount);
    DUMP(inPipe);
}

// ##########################
// ##     PipelineNode     ##
// ##########################

void PipelineNode::addNode(std::unique_ptr<Node> &&node) {
    if(isa<PipelineNode>(*node)) {
        auto &pipe = cast<PipelineNode>(*node);
        for(auto &e : pipe.nodes) {
            this->addNodeImpl(std::move(e));
        }
    } else {
        this->addNodeImpl(std::move(node));
    }
}

void PipelineNode::dump(NodeDumper &dumper) const {
    DUMP(nodes);
    DUMP(baseIndex);
}

void PipelineNode::addNodeImpl(std::unique_ptr<Node> &&node) {
    if(isa<CmdNode>(*node)) {
        cast<CmdNode>(*node).setInPipe(true);
    }
    this->updateToken(node->getToken());
    this->nodes.push_back(std::move(node));
}

// ######################
// ##     WithNode     ##
// ######################

void WithNode::dump(NodeDumper &dumper) const {
    DUMP_PTR(exprNode);
    DUMP(redirNodes);
    DUMP(baseIndex);
}

// ######################
// ##     ForkNode     ##
// ######################

void ForkNode::dump(NodeDumper &dumper) const {
    DUMP_PTR(exprNode);

#define EACH_ENUM(OP) \
    OP(ForkKind::STR) \
    OP(ForkKind::ARRAY) \
    OP(ForkKind::IN_PIPE) \
    OP(ForkKind::OUT_PIPE) \
    OP(ForkKind::JOB) \
    OP(ForkKind::DISOWN) \
    OP(ForkKind::COPROC)

    DUMP_ENUM(opKind, EACH_ENUM);
#undef EACH_ENUM
}

// ########################
// ##     AssertNode     ##
// ########################

void AssertNode::dump(NodeDumper &dumper) const {
    DUMP_PTR(condNode);
    DUMP_PTR(messageNode);
}

// #######################
// ##     BlockNode     ##
// #######################

void BlockNode::insertNodeToFirst(std::unique_ptr<Node> &&node) {
    this->nodes.insert(this->nodes.begin(), std::move(node));
}

void BlockNode::dump(NodeDumper &dumper) const {
    DUMP(nodes);
    DUMP(baseIndex);
    DUMP(varSize);
    DUMP(maxVarSize);
}

// ###########################
// ##     TypeAliasNode     ##
// ###########################

void TypeAliasNode::dump(NodeDumper &dumper) const {
    DUMP(alias);
    DUMP_PTR(targetTypeNode);
}

// ######################
// ##     LoopNode     ##
// ######################

LoopNode::LoopNode(unsigned int startPos, std::unique_ptr<Node> &&initNode,
                   std::unique_ptr<Node> &&condNode, std::unique_ptr<Node> &&iterNode,
                   std::unique_ptr<BlockNode> &&blockNode, bool asDoWhile) :
        WithRtti({startPos, 0}), initNode(std::move(initNode)), condNode(std::move(condNode)),
        iterNode(std::move(iterNode)), blockNode(std::move(blockNode)), asDoWhile(asDoWhile) {
    if(this->initNode == nullptr) {
        this->initNode = std::make_unique<EmptyNode>();
    }

    if(this->iterNode == nullptr) {
        this->iterNode = std::make_unique<EmptyNode>();
    }

    this->updateToken(this->asDoWhile ? this->condNode->getToken() : this->blockNode->getToken());
}

void LoopNode::dump(NodeDumper &dumper) const {
    DUMP_PTR(initNode);
    DUMP_PTR(condNode);
    DUMP_PTR(iterNode);
    DUMP_PTR(blockNode);
    DUMP(asDoWhile);
}

// ####################
// ##     IfNode     ##
// ####################

/**
 * if condNode is InstanceOfNode and targetNode is VarNode, insert VarDeclNode to blockNode.
 */
static void resolveIfIsStatement(Node &condNode, BlockNode &blockNode) {
    if(!isa<TypeOpNode>(condNode) || !cast<TypeOpNode>(condNode).isInstanceOfOp()) {
        return;
    }
    auto &isNode = cast<TypeOpNode>(condNode);

    if(!isa<VarNode>(isNode.getExprNode())) {
        return;
    }
    auto &varNode = cast<VarNode>(isNode.getExprNode());

    auto exprNode = std::make_unique<VarNode>(Token{isNode.getPos(), 1}, std::string(varNode.getVarName()));
    auto castNode = std::make_unique<TypeOpNode>(std::move(exprNode), *isNode.getTargetTypeNode(), TypeOpNode::NO_CAST);
    auto declNode = std::make_unique<VarDeclNode>(
            isNode.getPos(), std::string(varNode.getVarName()), std::move(castNode), VarDeclNode::CONST);
    blockNode.insertNodeToFirst(std::move(declNode));
}

IfNode::IfNode(unsigned int startPos, std::unique_ptr<Node> &&condNode,
               std::unique_ptr<Node> &&thenNode, std::unique_ptr<Node> &&elseNode) :
        WithRtti({startPos, 0}), condNode(std::move(condNode)),
        thenNode(std::move(thenNode)), elseNode(std::move(elseNode)) {

    if(isa<BlockNode>(*this->thenNode)) {
        resolveIfIsStatement(*this->condNode, cast<BlockNode>(*this->thenNode));
    }
    this->updateToken(this->thenNode->getToken());
    if(this->elseNode != nullptr) {
        this->updateToken(this->elseNode->getToken());
    }
    if(this->elseNode == nullptr) {
        this->elseNode = std::make_unique<EmptyNode>(this->getToken());
    }
}

void IfNode::dump(NodeDumper &dumper) const {
    DUMP_PTR(condNode);
    DUMP_PTR(thenNode);
    DUMP_PTR(elseNode);
}

// ######################
// ##     CaseNode     ##
// ######################

bool CaseNode::hasDefault() const {
    for(auto &e : this->armNodes) {
        if(e->isDefault()) {
            return true;
        }
    }
    return false;
}

void CaseNode::dump(NodeDumper &dumper) const {
    DUMP_PTR(exprNode);
    DUMP(armNodes);

#define EACH_ENUM(OP) \
    OP(MAP) \
    OP(IF_ELSE)

    DUMP_ENUM(caseKind, EACH_ENUM);
#undef EACH_ENUM
}

// #####################
// ##     ArmNode     ##
// #####################

void ArmNode::dump(ydsh::NodeDumper &dumper) const {
    DUMP(this->patternNodes);
    DUMP_PTR(this->actionNode);
}


// ######################
// ##     JumpNode     ##
// ######################

JumpNode::JumpNode(Token token, OpKind kind, std::unique_ptr<Node> &&exprNode) :
        WithRtti(token), opKind(kind), exprNode(std::move(exprNode)) {
    if(this->exprNode == nullptr) {
        this->exprNode = std::make_unique<EmptyNode>(token);
    } else {
        this->updateToken(this->exprNode->getToken());
    }
}

void JumpNode::dump(NodeDumper &dumper) const {
#define EACH_ENUM(OP) \
    OP(BREAK_) \
    OP(CONTINUE_) \
    OP(THROW_) \
    OP(RETURN_)

    DUMP_ENUM(opKind, EACH_ENUM);
#undef EACH_ENUM

    DUMP_PTR(exprNode);
    DUMP(leavingBlock);
}

// #######################
// ##     CatchNode     ##
// #######################

void CatchNode::dump(NodeDumper &dumper) const {
    DUMP(exceptionName);
    DUMP_PTR(typeNode);
    DUMP_PTR(blockNode);
    DUMP(varIndex);
}

// #####################
// ##     TryNode     ##
// #####################

void TryNode::dump(NodeDumper &dumper) const {
    DUMP_PTR(exprNode);
    DUMP(catchNodes);
    DUMP_PTR(finallyNode);
}

// #########################
// ##     VarDeclNode     ##
// #########################

VarDeclNode::VarDeclNode(unsigned int startPos, std::string &&varName,
        std::unique_ptr<Node> &&exprNode, Kind kind) :
        WithRtti({startPos, 0}),
        varName(std::move(varName)), kind(kind), exprNode(std::move(exprNode)) {
    if(this->exprNode != nullptr) {
        this->updateToken(this->exprNode->getToken());
    }
}

void VarDeclNode::setAttribute(const FieldHandle &handle) {
    this->global = hasFlag(handle.attr(), FieldAttribute::GLOBAL);
    this->varIndex = handle.getIndex();
}

void VarDeclNode::dump(NodeDumper &dumper) const {
    DUMP(varName);
    DUMP(global);
    DUMP(varIndex);
    DUMP_PTR(exprNode);

#define EACH_ENUM(OP) \
    OP(VAR) \
    OP(CONST) \
    OP(IMPORT_ENV) \
    OP(EXPORT_ENV)

    DUMP_ENUM(kind, EACH_ENUM);
#undef EACH_ENUM
}

// ########################
// ##     AssignNode     ##
// ########################

void AssignNode::dump(NodeDumper &dumper) const {
    DUMP_PTR(leftNode);
    DUMP_PTR(rightNode);

#define EACH_FLAG(OP) \
    OP(SELF_ASSIGN) \
    OP(FIELD_ASSIGN)

    DUMP_BITSET(attributeSet, EACH_FLAG);
#undef EACH_FLAG
}

// ###################################
// ##     ElementSelfAssignNode     ##
// ###################################

ElementSelfAssignNode::ElementSelfAssignNode(std::unique_ptr<ApplyNode> &&leftNode,
                                             std::unique_ptr<BinaryOpNode> &&binaryNode) :
        WithRtti(leftNode->getToken()), rightNode(std::move(binaryNode)) {
    this->updateToken(this->rightNode->getToken());

    // init recv, indexNode
    assert(leftNode->isIndexCall());
    auto opToken = cast<AccessNode>(leftNode->getExprNode()).getNameNode().getToken();
    auto pair = ApplyNode::split(std::move(leftNode));
    this->recvNode = std::move(pair.first);
    this->indexNode = std::move(pair.second);

    // init getter node
    this->getterNode = ApplyNode::newMethodCall(std::make_unique<EmptyNode>(), opToken, std::string(OP_GET));
    this->getterNode->refArgNodes().push_back(std::make_unique<EmptyNode>());

    // init setter node
    this->setterNode = ApplyNode::newMethodCall(std::make_unique<EmptyNode>(), opToken, std::string(OP_SET));
    this->setterNode->refArgNodes().push_back(std::make_unique<EmptyNode>());
    this->setterNode->refArgNodes().push_back(std::make_unique<EmptyNode>());
}

void ElementSelfAssignNode::setRecvType(DSType &type) {
    this->getterNode->getRecvNode().setType(type);
    this->setterNode->getRecvNode().setType(type);
}

void ElementSelfAssignNode::setIndexType(DSType &type) {
    this->getterNode->refArgNodes()[0]->setType(type);
    this->setterNode->refArgNodes()[0]->setType(type);
}

void ElementSelfAssignNode::dump(NodeDumper &dumper) const {
    DUMP_PTR(recvNode);
    DUMP_PTR(indexNode);
    DUMP_PTR(getterNode);
    DUMP_PTR(setterNode);
    DUMP_PTR(rightNode);
}


// ##########################
// ##     FunctionNode     ##
// ##########################

void FunctionNode::dump(NodeDumper &dumper) const {
    DUMP(funcName);
    DUMP(paramNodes);
    DUMP(paramTypeNodes);

    DUMP_PTR(returnTypeNode);
    DUMP_PTR(blockNode);
    DUMP(maxVarNum);
    DUMP(varIndex);
    DUMP_PTR(funcType);
}

// ###########################
// ##     InterfaceNode     ##
// ###########################

InterfaceNode::~InterfaceNode() {
    for(auto &node : this->methodDeclNodes) {
        delete node;
    }

    for(auto &node : this->fieldDeclNodes) {
        delete node;
    }

    for(auto &t : this->fieldTypeNodes) {
        delete t;
    }
}

void InterfaceNode::addMethodDeclNode(FunctionNode *methodDeclNode) {
    this->methodDeclNodes.push_back(methodDeclNode);
}

void InterfaceNode::addFieldDecl(VarDeclNode *node, TypeNode *typeToken) {
    this->fieldDeclNodes.push_back(node);
    this->fieldTypeNodes.push_back(typeToken);
    this->updateToken(typeToken->getToken());
}

void InterfaceNode::dump(NodeDumper &dumper) const {
    DUMP(interfaceName);
    DUMP(methodDeclNodes);
    DUMP(fieldDeclNodes);
    DUMP(fieldTypeNodes);
}

// ################################
// ##     UserDefinedCmdNode     ##
// ################################

void UserDefinedCmdNode::dump(NodeDumper &dumper) const {
    DUMP(cmdName);
    DUMP(udcIndex);
    DUMP_PTR(blockNode);
    DUMP(maxVarNum);
}

// ########################
// ##     SourceNode     ##
// ########################

void SourceNode::dump(NodeDumper &dumper) const {
    DUMP(name);
    DUMP(modType);
    DUMP(firstAppear);
    DUMP(nothing);
    DUMP(modIndex);
    DUMP(index);
    DUMP(maxVarNum);
}

// ############################
// ##     SourceListNode     ##
// ############################

void SourceListNode::dump(NodeDumper &dumper) const {
    DUMP_PTR(pathNode);
    DUMP(name);
    DUMP(optional);
    DUMP(curIndex);
    DUMP(pathList);
}

// #######################
// ##     EmptyNode     ##
// #######################

void EmptyNode::dump(NodeDumper &) const {
} // do nothing

// for node creation

const char *resolveUnaryOpName(TokenKind op) {
    switch(op) {
    case PLUS:  // +
        return OP_PLUS;
    case MINUS: // -
        return OP_MINUS;
    case NOT:   // not
        return OP_NOT;
    default:
        fatal("unsupported unary op: %s\n", TO_NAME(op));
    }
}

const char *resolveBinaryOpName(TokenKind op) {
    switch(op) {
    case ADD:
        return OP_ADD;
    case SUB:
        return OP_SUB;
    case MUL:
        return OP_MUL;
    case DIV:
        return OP_DIV;
    case MOD:
        return OP_MOD;
    case EQ:
        return OP_EQ;
    case NE:
        return OP_NE;
    case LT:
        return OP_LT;
    case GT:
        return OP_GT;
    case LE:
        return OP_LE;
    case GE:
        return OP_GE;
    case AND:
        return OP_AND;
    case OR:
        return OP_OR;
    case XOR:
        return OP_XOR;
    case MATCH:
        return OP_MATCH;
    case UNMATCH:
        return OP_UNMATCH;
    default:
        fatal("unsupported binary op: %s\n", TO_NAME(op));
    }
}

TokenKind resolveAssignOp(TokenKind op) {
    switch(op) {
    case INC:
        return ADD;
    case DEC:
        return SUB;
    case ADD_ASSIGN:
        return ADD;
    case SUB_ASSIGN:
        return SUB;
    case MUL_ASSIGN:
        return MUL;
    case DIV_ASSIGN:
        return DIV;
    case MOD_ASSIGN:
        return MOD;
    case STR_ASSIGN:
        return STR_CHECK;
    default:
        fatal("unsupported assign op: %s\n", TO_NAME(op));
    }
}

std::unique_ptr<LoopNode> createForInNode(unsigned int startPos, std::string &&varName,
                                          std::unique_ptr<Node> &&exprNode, std::unique_ptr<BlockNode> &&blockNode) {
    Token dummy = {startPos, 1};

    // create for-init
    auto call_iter = ApplyNode::newMethodCall(std::move(exprNode), std::string(OP_ITER));
    std::string reset_var_name = "%reset_";
    reset_var_name += std::to_string(startPos);
    auto reset_varDecl = std::make_unique<VarDeclNode>(startPos, std::string(reset_var_name),
            std::move(call_iter), VarDeclNode::CONST);

    // create for-cond
    auto reset_var = std::make_unique<VarNode>(dummy, std::string(reset_var_name));
    auto call_hasNext = ApplyNode::newMethodCall(std::move(reset_var), std::string(OP_HAS_NEXT));

    // create forIn-init
    reset_var = std::make_unique<VarNode>(dummy, std::string(reset_var_name));
    auto call_next = ApplyNode::newMethodCall(std::move(reset_var), std::string(OP_NEXT));
    auto init_var = std::make_unique<VarDeclNode>(startPos, std::move(varName), std::move(call_next), VarDeclNode::VAR);

    // insert init to block
    blockNode->insertNodeToFirst(std::move(init_var));

    return std::make_unique<LoopNode>(startPos, std::move(reset_varDecl),
            std::move(call_hasNext), nullptr, std::move(blockNode));
}

std::unique_ptr<Node> createAssignNode(std::unique_ptr<Node> &&leftNode, TokenKind op, Token token, std::unique_ptr<Node> &&rightNode) {
    /*
     * basic assignment
     */
    if(op == ASSIGN) {
        // assign to element(actually call SET)
        if(isa<ApplyNode>(*leftNode) && cast<ApplyNode>(*leftNode).isIndexCall()) {
            auto &indexNode = cast<ApplyNode>(*leftNode);
            indexNode.setMethodName(std::string(OP_SET));
            indexNode.refArgNodes().push_back(std::move(rightNode));
            return std::move(leftNode);
        }
        // assign to variable or field
        return std::make_unique<AssignNode>(std::move(leftNode), std::move(rightNode));
    }

    /**
     * self assignment
     */
    // assign to element
    auto opNode = std::make_unique<BinaryOpNode>(
            std::make_unique<EmptyNode>(rightNode->getToken()), resolveAssignOp(op), token, std::move(rightNode));
    if(isa<ApplyNode>(*leftNode) && cast<ApplyNode>(*leftNode).isIndexCall()) {
        auto *indexNode = cast<ApplyNode>(leftNode.release());
        return std::make_unique<ElementSelfAssignNode>(std::unique_ptr<ApplyNode>(indexNode), std::move(opNode));
    }
    // assign to variable or field
    return std::make_unique<AssignNode>(std::move(leftNode), std::move(opNode), true);
}

const Node *findInnerNode(NodeKind kind, const Node *node) {
    while(!node->is(kind)) {
        assert(isa<TypeOpNode>(node));
        node = &cast<const TypeOpNode>(node)->getExprNode();
    }
    return node;
}

// ########################
// ##     NodeDumper     ##
// ########################

void NodeDumper::dump(const char *fieldName, const Node &node) {
    // write field name
    this->writeName(fieldName);

    // write node body
    this->newline();
    this->enterIndent();
    this->dump(node);
    this->leaveIndent();
}

void NodeDumper::dump(const char *fieldName, const DSType &type) {
    this->dump(fieldName, this->symbolTable.getTypeName(type));
}

void NodeDumper::dump(const char *fieldName, TokenKind kind) {
    this->dump(fieldName, toString(kind));
}

void NodeDumper::dump(const char *fieldName, const MethodHandle &handle) {
    this->dump(fieldName, std::to_string(handle.getMethodIndex()));
}

void NodeDumper::dump(const Node &node) {
    this->indent();
    this->dumpNodeHeader(node);
    node.dump(*this);
}

void NodeDumper::indent() {
    for(unsigned int i = 0; i < this->bufs.back().indentLevel; i++) {
        this->append("  ");
    }
}

static const char *toString(NodeKind kind) {
    const char *table[] = {
#define GEN_STR(E) #E,
            EACH_NODE_KIND(GEN_STR)
#undef GEN_STR
    };
    return table[static_cast<unsigned char>(kind)];
}

void NodeDumper::dumpNodeHeader(const Node &node, bool inArray) {
    this->appendAs("nodeKind: %s\n", toString(node.getNodeKind()));

    if(inArray) {
        this->enterIndent();
    }

    this->indent(); this->appendAs("token:\n");
    this->enterIndent();
    this->indent(); this->appendAs("pos: %d\n", node.getPos());
    this->indent(); this->appendAs("size: %d\n", node.getSize());
    this->leaveIndent();
    this->indent();
    if(node.isUntyped()) {
        this->append("type:\n");
    } else {
        this->appendAs("type: %s\n", this->symbolTable.getTypeName(node.getType()));
    }

    if(inArray) {
        this->leaveIndent();
    }
}

void NodeDumper::append(int ch) {
    this->bufs.back().value += static_cast<char>(ch);
}

void NodeDumper::append(const char *str) {
    this->bufs.back().value += str;
}

void NodeDumper::appendEscaped(const char *value) {
    this->append('"');
    while(*value != 0) {
        int ch = *(value++);
        bool escape = true;
        switch(ch) {
        case '\t':
            ch = 't';
            break;
        case '\r':
            ch = 'r';
            break;
        case '\n':
            ch = 'n';
            break;
        case '"':
            ch = '"';
            break;
        case '\\':
            ch = '\\';
            break;
        default:
            escape = false;
            break;
        }
        if(escape) {
            this->append('\\');
        }
        this->append(ch);
    }
    this->append('"');
}

void NodeDumper::appendAs(const char *fmt, ...) {
    va_list arg;

    va_start(arg, fmt);
    char *str = nullptr;
    if(vasprintf(&str, fmt, arg) == -1) {
        fatal_perror("");
    }
    va_end(arg);

    this->append(str);
    free(str);
}

void NodeDumper::writeName(const char *fieldName) {
    this->indent(); this->appendAs("%s:", fieldName);
}

void NodeDumper::enterModule(const char *sourceName, const char *header) {
    this->bufs.emplace_back();

    if(header) {
        this->appendAs("%s\n", header);
    }
    this->dump("sourceName", sourceName);
    this->writeName("nodes");
    this->newline();

    this->enterIndent();
}

void NodeDumper::leaveModule() {
    auto b = std::move(this->bufs.back());
    this->bufs.pop_back();
    this->bufs.push_front(std::move(b));
}

void NodeDumper::operator()(const Node &node) {
    this->indent();
    this->append("- ");
    this->dumpNodeHeader(node, true);
    this->enterIndent();
    node.dump(*this);
    this->leaveIndent();
}

void NodeDumper::finalize() {
    this->leaveIndent();

    this->dumpRaw("maxVarNum", std::to_string(this->symbolTable.getMaxVarIndex()).c_str());
    this->dumpRaw("maxGVarNum", std::to_string(this->symbolTable.getMaxGVarIndex()).c_str());

    this->leaveModule();

    assert(this->fp != nullptr);
    for(auto &e : this->bufs) {
        fputs(e.value.c_str(), this->fp);
        fputc('\n', this->fp);
        fflush(this->fp);
    }
}

} // namespace ydsh