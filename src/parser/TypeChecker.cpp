/*
 * TypeChecker.cpp
 *
 *  Created on: 2015/01/07
 *      Author: skgchxngsxyz-osx
 */

#include "TypeChecker.h"
#include "TypeError.h"

TypeChecker::TypeChecker(TypePool *typePool) :
        typePool(typePool), curReturnType(0), loopContextStack(), finallyContextStack() {
}

TypeChecker::~TypeChecker() {
    this->finallyContextStack.clear();
}

void TypeChecker::checkTypeRootNode(const std::unique_ptr<RootNode> &rootNode) {	//FIXME
    int size = rootNode->getNodes().size();
    for(int i = 0; i < size; i++) {
        this->checkTypeAcceptingVoidType(rootNode->getNodes()[i].get());
    }
}

// type check entry point

void TypeChecker::checkTypeAcceptingVoidType(Node *targetNode) {
    this->checkType(0, targetNode, 0);
}

void TypeChecker::checkType(Node *targetNode) {
    this->checkType(0, targetNode, this->typePool->getVoidType());
}

void TypeChecker::checkType(DSType *requiredType, Node *targetNode) {
    this->checkType(requiredType, targetNode, 0);
}

//TODO:
void TypeChecker::checkType(DSType *requiredType, Node *targetNode, DSType *unacceptableType) {
    /**
     * if target node is statement, always check type.
     */
    ExprNode *exprNode = dynamic_cast<ExprNode*>(targetNode);
    if(exprNode == 0) {
        targetNode->accept(this);
        return;
    }

    /**
     * if target node is expr node and type is null,
     * try type check.
     */
    if(exprNode->getType() == 0) {
        //exprNode = (ExprNode) exprNode.accept(this);
        exprNode->accept(this);
    }

    /**
     * after type checking, if type is still null,
     * throw exception.
     */
    DSType *type = exprNode->getType();
    if(type == 0) {
        Unresolved->report(exprNode->getLineNum());
        return;
    }

    /**
     * do not try type matching.
     */
    if(requiredType == 0) {
        if(unacceptableType != 0 && unacceptableType->isAssignableFrom(type)) {
            Unacceptable->report(exprNode->getLineNum(), type->getTypeName());
            return;
        }
        return;
    }

    /**
     * try type matching.
     */
    if(requiredType->isAssignableFrom(type)) {
        return;
    }
    Required->report(exprNode->getLineNum(), requiredType->getTypeName(), type->getTypeName());
}

void TypeChecker::checkTypeWithNewBlockScope(BlockNode *blockNode) {
    //TODO: symbol table, push, pop
    this->checkTypeWithCurrentBlockScope(blockNode);
}

void TypeChecker::checkTypeWithCurrentBlockScope(BlockNode *blockNode) {
    blockNode->accept(this);
}

void TypeChecker::addEntryAndThrowIfDefined(Node *node, const std::string &symbolName, DSType *type,
        bool readOnly) {
    //TODO: symbol table
}

void TypeChecker::enterLoop() {
    this->loopContextStack.push_back(true);
}

void TypeChecker::exitLoop() {
    this->loopContextStack.pop_back();
}

void TypeChecker::checkAndThrowIfOutOfLoop(Node *node) {
    if(!this->loopContextStack.empty() && this->loopContextStack.back()) {
        return;
    }
    InsideLoop->report(node->getLineNum());
}

bool TypeChecker::findBlockEnd(const std::unique_ptr<BlockNode> &blockNode) {
    if(dynamic_cast<EmptyBlockNode*>(blockNode.get()) != 0) {
        return false;
    }
    int endIndex = blockNode->getNodes().size() - 1;
    if(endIndex < 0) {
        return false;
    }
    const std::unique_ptr<Node> &endNode = blockNode->getNodes()[endIndex];
    if(dynamic_cast<BlockEndNode*>(endNode.get()) != 0) {
        return true;
    }

    /**
     * if endNode is IfNode, search recursively
     */
    IfNode *ifNode = dynamic_cast<IfNode*>(endNode.get());
    if(ifNode != 0) {
        return this->findBlockEnd(ifNode->getThenNode())
                && this->findBlockEnd(ifNode->getElseNode());
    }
    return false;
}

void TypeChecker::checkBlockEndExistence(const std::unique_ptr<BlockNode> &blockNode,
        DSType *returnType) {
    int endIndex = blockNode->getNodes().size() - 1;
    const std::unique_ptr<Node> &endNode = blockNode->getNodes()[endIndex];
    if(returnType->equals(this->typePool->getVoidType())
            && dynamic_cast<BlockEndNode*>(endNode.get()) == 0) {
        /**
         * insert return node to block end
         */
        blockNode->addNode(
                std::unique_ptr < Node
                        > (new ReturnNode(0, std::unique_ptr < ExprNode > (new EmptyNode()))));
        return;
    }
    if(!this->findBlockEnd(blockNode)) {
        UnfoundReturn->report(blockNode->getLineNum());
    }
}

void TypeChecker::pushReturnType(DSType *returnType) {
    this->curReturnType = returnType;
}

DSType *TypeChecker::popReturnType() {
    DSType *returnType = this->curReturnType;
    this->curReturnType = 0;
    return returnType;
}

DSType *TypeChecker::getCurrentReturnType() {
    return this->curReturnType;
}

void TypeChecker::checkAndThrowIfInsideFinally(BlockEndNode *node) {
    if(!this->finallyContextStack.empty() && this->finallyContextStack.back()) {
        InsideFinally->report(node->getLineNum());
    }
}

void TypeChecker::recover() {
    //TODO: symbol tabel recover
    this->curReturnType = 0;
    this->loopContextStack.clear();
    this->finallyContextStack.clear();
}

// visitor api

int TypeChecker::visitIntValueNode(IntValueNode *node) {	//TODO: int8, int16 ..etc
    node->setType(this->typePool->getIntType());
    return 0;
}

int TypeChecker::visitFloatValueNode(FloatValueNode *node) {
    node->setType(this->typePool->getFloatType());
    return 0;
}

int TypeChecker::visitBooleanValueNode(BooleanValueNode *node) {
    node->setType(this->typePool->getBooleanType());
    return 0;
}

int TypeChecker::visitStringValueNode(StringValueNode *node) {
    node->setType(this->typePool->getStringType());
    return 0;
}

int TypeChecker::visitStringExprNode(StringExprNode *node) {
    for(const std::unique_ptr<ExprNode> &exprNode : node->getExprNodes()) {
        this->checkType(this->typePool->getStringType(), exprNode.get());
    }
    node->setType(this->typePool->getStringType());
    return 0;
}

int TypeChecker::visitArrayNode(ArrayNode *node) {
    return 0;
} //TODO
int TypeChecker::visitMapNode(MapNode *node) {
    return 0;
} //TODO
int TypeChecker::visitPairNode(PairNode *node) {
    return 0;
} //TODO
int TypeChecker::visitVarNode(VarNode *node) {
    return 0;
} //TODO
int TypeChecker::visitIndexNode(IndexNode *node) {
    return 0;
} //TODO
int TypeChecker::visitAccessNode(AccessNode *node) {
    return 0;
} //TODO
int TypeChecker::visitCastNode(CastNode *node) {
    return 0;
} //TODO
int TypeChecker::visitInstanceOfNode(InstanceOfNode *node) {
    return 0;
} //TODO
int TypeChecker::visitApplyNode(ApplyNode *node) {
    return 0;
} //TODO
int TypeChecker::visitConstructorCallNode(ConstructorCallNode *node) {
    return 0;
} //TODO
int TypeChecker::visitCondOpNode(CondOpNode *node) {
    return 0;
} //TODO

int TypeChecker::visitProcessNode(ProcessNode *node) {
    for(const std::unique_ptr<ProcArgNode> &argNode : node->getArgNodes()) {
        this->checkTypeAcceptingVoidType(argNode.get());    //FIXME: accept void type
    }
    // check type redirect options
    for(const std::pair<int, std::unique_ptr<ExprNode>> &optionPair : node->getRedirOptions()) {
        this->checkTypeAcceptingVoidType(optionPair.second.get());  //FIXME: accept void type
    }
    node->setType(this->typePool->getVoidType());   //FIXME: ProcessNode is always void type
    return 0;
}

int TypeChecker::visitProcArgNode(ProcArgNode *node) {
    for(const std::unique_ptr<ExprNode> &exprNode : node->getSegmentNodes()) {
        this->checkType(exprNode.get());
    }
    node->setType(this->typePool->getVoidType());   //FIXME: ProcArgNode is always void type
    return 0;
}

int TypeChecker::visitSpecialCharNode(SpecialCharNode *node) {
    return 0;
} //TODO

int TypeChecker::visitTaskNode(TaskNode *node) {    //TODO: parent node
    for(const std::unique_ptr<ProcessNode> &procNode : node->getProcNodes()) {
        this->checkTypeAcceptingVoidType(procNode.get());   //FIXME: accept void
    }

    /**
     * resolve task type
     */
    node->setType(this->typePool->getVoidType());
    return 0;
}

int TypeChecker::visitInnerTaskNode(InnerTaskNode *node) {
    return 0;
} //TODO

int TypeChecker::visitAssertNode(AssertNode *node) {
    this->checkType(this->typePool->getBooleanType(), node->getExprNode().get());
    return 0;
}

int TypeChecker::visitBlockNode(BlockNode *node) {
    int count = 0;
    int size = node->getNodes().size();
    for(const std::unique_ptr<Node> &targetNode : node->getNodes()) {
        this->checkTypeAcceptingVoidType(targetNode.get());
        if(dynamic_cast<BlockEndNode*>(targetNode.get()) != 0 && (count != size - 1)) {
            Unreachable->report(node->getLineNum());
            return -1;
        }
        count++;
    }
    return 0;
}

int TypeChecker::visitBreakNode(BreakNode *node) {
    this->checkAndThrowIfInsideFinally(node);
    this->checkAndThrowIfOutOfLoop(node);
    return 0;
}

int TypeChecker::visitContinueNode(ContinueNode *node) {
    this->checkAndThrowIfInsideFinally(node);
    this->checkAndThrowIfOutOfLoop(node);
    return 0;
}

int TypeChecker::visitExportEnvNode(ExportEnvNode *node) {
    DSType *stringType = this->typePool->getStringType();
    this->addEntryAndThrowIfDefined(node, node->getEnvName(), stringType, true);
    this->checkType(stringType, node->getExprNode().get());
    return 0;
}

int TypeChecker::visitImportEnvNode(ImportEnvNode *node) {
    DSType *stringType = this->typePool->getStringType();
    this->addEntryAndThrowIfDefined(node, node->getEnvName(), stringType, true);
    return 0;
}

int TypeChecker::visitForNode(ForNode *node) {
    return 0;
} //TODO
int TypeChecker::visitForInNode(ForInNode *node) {
    return 0;
}

int TypeChecker::visitWhileNode(WhileNode *node) {
    this->checkType(this->typePool->getBooleanType(), node->getCondNode().get());
    this->enterLoop();
    this->checkTypeWithNewBlockScope(node->getBlockNode().get());
    this->exitLoop();
    return 0;
}

int TypeChecker::visitIfNode(IfNode *node) {
    this->checkType(this->typePool->getBooleanType(), node->getCondNode().get());
    this->checkTypeWithNewBlockScope(node->getThenNode().get());
    this->checkTypeWithNewBlockScope(node->getElseNode().get());
    return 0;
}

int TypeChecker::visitReturnNode(ReturnNode *node) {
    return 0;
} //TODO

int TypeChecker::visitThrowNode(ThrowNode *node) {
    this->checkAndThrowIfInsideFinally(node);
    this->checkType(node->getExprNode().get()); //TODO: currently accept all type
    return 0;
}

int TypeChecker::visitCatchNode(CatchNode *node) {
    return 0;
} //TODO
int TypeChecker::visitTryNode(TryNode *node) {
    return 0;
} //TODO

int TypeChecker::visitFinallyNode(FinallyNode *node) {
    this->finallyContextStack.push_back(true);
    this->checkTypeWithNewBlockScope(node->getBlockNode().get());
    this->finallyContextStack.pop_back();
    return 0;
}

int TypeChecker::visitVarDeclNode(VarDeclNode *node) {
    return 0;
} //TODO
int TypeChecker::visitAssignNode(AssignNode *node) {
    return 0;
} //TODO
int TypeChecker::visitFunctionNode(FunctionNode *node) {
    return 0;
} //TODO

int TypeChecker::visitEmptyNode(EmptyNode *node) {
    node->setType(this->typePool->getVoidType());
    return 0;
}

int TypeChecker::visitEmptyBlockNode(EmptyBlockNode *node) {
    return 0;	// do nothing
}
