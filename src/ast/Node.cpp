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

#include <core/builtin_variable.h>
#include <core/magic_method.h>
#include <ast/Node.h>
#include <ast/node_utils.h>

#include <assert.h>

// ##################
// ##     Node     ##
// ##################

Node::Node(int lineNum) :
        lineNum(lineNum) {
}

Node::~Node() {
    // TODO Auto-generated destructor stub
}

int Node::getLineNum() {
    return this->lineNum;
}

// ######################
// ##     ExprNode     ##
// ######################

ExprNode::ExprNode(int lineNum) :
        Node(lineNum), type(0) {
}

ExprNode::~ExprNode() {
    delete this->type;
    this->type = 0;
}

void ExprNode::setType(DSType *type) {
    this->type = type;
}

DSType *ExprNode::getType() {
    return this->type;
}

// ##########################
// ##     IntValueNode     ##
// ##########################

IntValueNode::IntValueNode(int lineNum, long value) :
        ExprNode(lineNum), value(value) {
}

long IntValueNode::getValue() {
    return this->value;
}

int IntValueNode::accept(NodeVisitor *visitor) {
    return visitor->visitIntValueNode(this);
}

// ############################
// ##     FloatValueNode     ##
// ############################

FloatValueNode::FloatValueNode(int lineNum, double value) :
        ExprNode(lineNum), value(value) {
}

double FloatValueNode::getValue() {
    return this->value;
}

int FloatValueNode::accept(NodeVisitor *visitor) {
    return visitor->visitFloatValueNode(this);
}

// ##############################
// ##     BooleanValueNode     ##
// ##############################

BooleanValueNode::BooleanValueNode(int lineNum, bool value) :
        ExprNode(lineNum), value(value) {
}

bool BooleanValueNode::getValue() {
    return this->value;
}

int BooleanValueNode::accept(NodeVisitor *visitor) {
    return visitor->visitBooleanValueNode(this);
}

// ############################
// ##     StringValueNode    ##
// ############################

StringValueNode::StringValueNode(std::string &&value) :
        ExprNode(0), value(std::move(value)) {
}

StringValueNode::StringValueNode(int lineNum, char *value, bool isSingleQuoteStr) :	//TODO:
        ExprNode(lineNum) {
    // parser original value.

    /*			StringBuilder sBuilder = new StringBuilder();
     String text = token.getText();
     int startIndex = isSingleQuoteStr ? 1 : 0;
     int endIndex = isSingleQuoteStr ? text.length() - 1 : text.length();
     for(int i = startIndex; i < endIndex; i++) {
     char ch = text.charAt(i);
     if(ch == '\\') {
     char nextCh = text.charAt(++i);
     switch(nextCh) {
     case 't' : ch = #include <ast/node_utils.h>'\t'; break;
     case 'b' : ch = '\b'; break;
     case 'n' : ch = '\n'; break;
     case 'r' : ch = '\r'; break;
     case 'f' : ch = '\f'; break;
     case '\'': ch = '\''; break;
     case '"' : ch = '"' ; break;
     case '\\': ch = '\\'; break;
     case '`' : ch = '`' ; break;
     case '$' : ch = '$' ; break;
     }
     }
     sBuilder.append(ch);
     }
     return sBuilder.toString();
     */
}

//StringValueNode::StringValueNode(int lineNum, char *value):	//FIXME:
//		StringValueNode(lineNum, value, false) {
//}

const std::string &StringValueNode::getValue() {
    return this->value;
}

int StringValueNode::accept(NodeVisitor *visitor) {
    return visitor->visitStringValueNode(this);
}

// ############################
// ##     StringExprNode     ##
// ############################

StringExprNode::StringExprNode(int lineNum) :
        ExprNode(lineNum), nodes() {
}

StringExprNode::~StringExprNode() {
    for(ExprNode *e : this->nodes) {
        delete e;
    }
    this->nodes.clear();
}

void StringExprNode::addExprNode(ExprNode *node) {	//TODO:
    this->nodes.push_back(node);
}

const std::vector<ExprNode*> &StringExprNode::getExprNodes() {
    return this->nodes;
}

int StringExprNode::accept(NodeVisitor *visitor) {
    return visitor->visitStringExprNode(this);
}

// #######################
// ##     ArrayNode     ##
// #######################

ArrayNode::ArrayNode(int lineNum) :
        ExprNode(lineNum), nodes() {
}

ArrayNode::~ArrayNode() {
    for(ExprNode *e : this->nodes) {
        delete e;
    }
    this->nodes.clear();
}

void ArrayNode::addExprNode(ExprNode *node) {
    this->nodes.push_back(node);
}

const std::vector<ExprNode*> &ArrayNode::getExprNodes() {
    return this->nodes;
}

int ArrayNode::accept(NodeVisitor *visitor) {
    return visitor->visitArrayNode(this);
}

// #####################
// ##     MapNode     ##
// #####################

MapNode::MapNode(int lineNum) :
        ExprNode(lineNum), keyNodes(), valueNodes() {
}

MapNode::~MapNode() {
    for(ExprNode *e : this->keyNodes) {
        delete e;
    }
    this->keyNodes.clear();
    for(ExprNode *e : this->valueNodes) {
        delete e;
    }
    this->valueNodes.clear();
}

void MapNode::addEntry(ExprNode *keyNode, ExprNode *valueNode) {
    this->keyNodes.push_back(keyNode);
    this->valueNodes.push_back(valueNode);
}

const std::vector<ExprNode*> &MapNode::getKeyNodes() {
    return this->keyNodes;
}

const std::vector<ExprNode*> &MapNode::getValueNodes() {
    return this->valueNodes;
}

int MapNode::accept(NodeVisitor *visitor) {
    return visitor->visitMapNode(this);
}

// ######################
// ##     PairNode     ##
// ######################

PairNode::PairNode(int lineNum, ExprNode *leftNode, ExprNode *rightNode) :
        ExprNode(lineNum), leftNode(leftNode), rightNode(rightNode) {
}

PairNode::~PairNode() {
    delete this->leftNode;
    this->leftNode = 0;

    delete this->rightNode;
    this->rightNode = 0;
}

ExprNode *PairNode::getLeftNode() {
    return this->leftNode;
}

ExprNode *PairNode::getRightNode() {
    return this->rightNode;
}

int PairNode::accept(NodeVisitor *visitor) {
    return visitor->visitPairNode(this);
}

// ############################
// ##     AssignableNode     ##
// ############################

AssignableNode::AssignableNode(int lineNum) :
        ExprNode(lineNum) {
}

AssignableNode::~AssignableNode() {
}

// #####################
// ##     VarNode     ##
// #####################

VarNode::VarNode(int lineNum, std::string &&varName) :
        AssignableNode(lineNum), varName(std::move(varName)), handle(0) {
}

const std::string &VarNode::getVarName() {
    return this->varName;
}

void VarNode::setHandle(FieldHandle *handle) {
    this->handle = handle;
}

FieldHandle *VarNode::getHandle() {
    return this->handle;
}

bool VarNode::isReadOnly() {
    return this->handle->isReadOnly();
}

int VarNode::accept(NodeVisitor *visitor) {
    return visitor->visitVarNode(this);
}

bool VarNode::isGlobal() {
    return this->handle->isGlobal();
}

int VarNode::getVarIndex() {
    return this->handle->getFieldIndex();
}

// ########################
// ##     AccessNode     ##
// ########################

AccessNode::AccessNode(ExprNode *recvNode, std::string &&fieldName) :
        AssignableNode(recvNode->getLineNum()), recvNode(recvNode), fieldName(std::move(fieldName)),
        handle(0), additionalOp(NOP) {
}

AccessNode::~AccessNode() {
    delete this->recvNode;
    this->recvNode = 0;
}

ExprNode *AccessNode::getRecvNode() {
    return this->recvNode;
}

void AccessNode::setFieldName(const std::string &fieldName) {
    this->fieldName = fieldName;
}

const std::string &AccessNode::getFieldName() {
    return this->fieldName;
}

void AccessNode::setHandle(FieldHandle *handle) {
    this->handle = handle;
}

FieldHandle *AccessNode::getHandle() {
    return this->handle;
}

int AccessNode::getFieldIndex() {
    return this->handle->getFieldIndex();
}

bool AccessNode::isReadOnly() {
    return this->handle->isReadOnly();
}

void AccessNode::setAdditionalOp(int op) {
    switch(op) {
    case NOP:
    case DUP_RECV:
    case DUP_RECV_AND_SWAP:
        this->additionalOp = op;
    }
    this->additionalOp = NOP;
}

int AccessNode::getAdditionnalOp() {
    return this->additionalOp;
}

int AccessNode::accept(NodeVisitor *visitor) {
    return visitor->visitAccessNode(this);
}

// ######################
// ##     CastNode     ##
// ######################

CastNode::CastNode(int lineNum, ExprNode *targetNode, TypeToken *type) :
        ExprNode(lineNum), targetNode(targetNode), targetTypeToken(type) {
}

CastNode::~CastNode() {
    delete this->targetNode;
    this->targetNode = 0;

    delete this->targetTypeToken;
    this->targetTypeToken = 0;
}

ExprNode *CastNode::getTargetNode() {
    return this->targetNode;
}

TypeToken *CastNode::getTargetTypeToken() {
    return this->targetTypeToken;
}

int CastNode::accept(NodeVisitor *visitor) {
    return visitor->visitCastNode(this);
}

// ############################
// ##     InstanceOfNode     ##
// ############################

InstanceOfNode::InstanceOfNode(int lineNum, ExprNode *targetNode, TypeToken *type) :
        ExprNode(lineNum), targetNode(targetNode), targetTypeToken(type),
        targetType(0), opKind(ALWAYS_FALSE) {
}

InstanceOfNode::~InstanceOfNode() {
    delete this->targetNode;
    this->targetNode = 0;

    delete this->targetTypeToken;
    this->targetTypeToken = 0;
}

ExprNode *InstanceOfNode::getTargetNode() {
    return this->targetNode;
}

TypeToken *InstanceOfNode::getTargetTypeToken() {
    return this->targetTypeToken;
}

TypeToken *InstanceOfNode::removeTargetTypeToken() {
    TypeToken *t = this->targetTypeToken;
    this->targetTypeToken = 0;
    return t;
}

void InstanceOfNode::setTargetType(DSType *targetType) {
    this->targetType = targetType;
}

DSType *InstanceOfNode::getTargetType() {
    return this->targetType;
}

void InstanceOfNode::resolveOpKind(int opKind) {
    switch(opKind) {
    case ALWAYS_FALSE:
    case INSTANCEOF:
        this->opKind = opKind;
        break;
    }
}

int InstanceOfNode::getOpKind() {
    return this->opKind;
}

int InstanceOfNode::accept(NodeVisitor *visitor) {
    return visitor->visitInstanceOfNode(this);
}

// ##############################
// ##     OperatorCallNode     ##
// ##############################

OperatorCallNode::OperatorCallNode(ExprNode *leftNode, int op, ExprNode *rightNode) :
        ExprNode(leftNode->getLineNum()), argNodes(2), op(op), handle() {
    this->argNodes.push_back(leftNode);
    this->argNodes.push_back(rightNode);
}

OperatorCallNode::OperatorCallNode(int op, ExprNode *rightNode) :
        ExprNode(rightNode->getLineNum()), argNodes(1), op(op), handle() {
    this->argNodes.push_back(rightNode);
}

OperatorCallNode::~OperatorCallNode() {
}

const std::vector<ExprNode*> OperatorCallNode::getArgNodes() {
    return this->argNodes;
}

int OperatorCallNode::getOp() {
    return this->op;
}

void OperatorCallNode::setHandle(FunctionHandle *handle) {
    this->handle = handle;
}

FunctionHandle *OperatorCallNode::getHandle() {
    return this->handle;
}

int OperatorCallNode::accept(NodeVisitor *visitor) {
    return visitor->visitOperatorCallNode(this);
}

// ######################
// ##     ArgsNode     ##
// ######################

ArgsNode::ArgsNode(int lineNum) :
        ExprNode(lineNum), argPairs(), paramIndexMap(0), paramSize(0) {
}
ArgsNode::ArgsNode(std::string &&paramName, ExprNode* argNode) :
        ArgsNode(argNode->getLineNum()) {
    this->argPairs.push_back(
            std::pair<std::string, ExprNode*>(std::move(paramName), argNode));
}

ArgsNode::ArgsNode(ExprNode *argNode) :
        ArgsNode(std::string(""), argNode) {
}

ArgsNode::~ArgsNode() {
    delete this->paramIndexMap;
    this->paramIndexMap = 0;
}

void ArgsNode::addArgPair(std::string &&paramName, ExprNode *argNode) {
    this->argPairs.push_back(
            std::pair<std::string, ExprNode*>(std::move(paramName), argNode));
}

void ArgsNode::addArg(ExprNode *argNode) {
    this->addArgPair(std::string(""), argNode);
}

void ArgsNode::initIndexMap() {
    this->paramIndexMap = new unsigned int[this->argPairs.size()];
}

void ArgsNode::addParamIndex(unsigned int index, unsigned int value) {
    this->paramIndexMap[index] = value;
}

unsigned int *ArgsNode::getParamIndexMap() {
    return this->paramIndexMap;
}

void ArgsNode::setParamSize(unsigned int size) {
    this->paramSize = size;
}

unsigned int ArgsNode::getParamSize() {
    return this->paramSize;
}
const std::vector<std::pair<std::string, ExprNode*>> &ArgsNode::getArgPairs() {
    return this->argPairs;
}

int ArgsNode::accept(NodeVisitor *visitor) {
    return visitor->visitArgsNode(this);
}

// #######################
// ##     ApplyNode     ##
// #######################

ApplyNode::ApplyNode(ExprNode *recvNode, ArgsNode *argsNode) :
        ExprNode(recvNode->getLineNum()), recvNode(recvNode),
        argsNode(argsNode), asFuncCall(false) {
}

ApplyNode::~ApplyNode() {
    delete this->recvNode;
    this->recvNode = 0;

    delete this->argsNode;
    this->argsNode = 0;
}

ExprNode *ApplyNode::getRecvNode() {
    return this->recvNode;
}

ArgsNode *ApplyNode::getArgsNode() {
    return this->argsNode;
}

void ApplyNode::setFuncCall(bool asFuncCall) {
    this->asFuncCall = asFuncCall;
}

bool ApplyNode::isFuncCall() {
    return this->asFuncCall;
}

int ApplyNode::accept(NodeVisitor *visitor) {
    return visitor->visitApplyNode(this);
}

// #######################
// ##     IndexNode     ##
// #######################

IndexNode::IndexNode(ExprNode *recvNode, ExprNode *indexNode) :
        ApplyNode(new AccessNode(recvNode, std::string(GET)), new ArgsNode(indexNode)) {
}

IndexNode::~IndexNode() {
}

ExprNode *IndexNode::getIndexNode() {
    return this->getArgsNode()->getArgPairs()[0].second;
}

ApplyNode *IndexNode::treatAsAssignment(ExprNode *rightNode) {
    AccessNode *accessNode = dynamic_cast<AccessNode*>(this->recvNode);
    accessNode->setFieldName(std::string(SET));
    this->getArgsNode()->addArg(rightNode);
    return this;
}

// #####################
// ##     NewNode     ##
// #####################

NewNode::NewNode(int lineNum, TypeToken *targetTypeToken, ArgsNode *argsNode) :
        ExprNode(lineNum), targetTypeToken(targetTypeToken),
        argsNode(argsNode) {
}

NewNode::~NewNode() {
    delete this->targetTypeToken;
    this->targetTypeToken = 0;
}

TypeToken *NewNode::getTargetTypeToken() {
    return this->targetTypeToken;
}

TypeToken *NewNode::removeTargetTypeToken() {
    TypeToken *t = this->targetTypeToken;
    this->targetTypeToken = 0;
    return t;
}

ArgsNode *NewNode::getArgsNode() {
    return this->argsNode;
}

int NewNode::accept(NodeVisitor *visitor) {
    return visitor->visitNewNode(this);
}

// ########################
// ##     CondOpNode     ##
// ########################

CondOpNode::CondOpNode(int lineNum, ExprNode *leftNode, ExprNode *rightNode, bool isAndOp) :
        ExprNode(lineNum), leftNode(leftNode), rightNode(rightNode), andOp(isAndOp) {
}

CondOpNode::~CondOpNode() {
    delete this->leftNode;
    this->leftNode = 0;

    delete this->rightNode;
    this->rightNode = 0;
}

ExprNode *CondOpNode::getLeftNode() {
    return this->leftNode;
}

ExprNode *CondOpNode::getRightNode() {
    return this->rightNode;
}

bool CondOpNode::isAndOp() {
    return this->andOp;
}

int CondOpNode::accept(NodeVisitor *visitor) {
    return visitor->visitCondOpNode(this);
}

// #########################
// ##     ProcessNode     ##
// #########################

ProcessNode::ProcessNode(int lineNum, std::string &&commandName) :
        ExprNode(lineNum), commandName(std::move(commandName)), argNodes(), redirOptions() {
}

ProcessNode::~ProcessNode() {
    for(ProcArgNode *e : this->argNodes) {
        delete e;
    }
    this->argNodes.clear();

    for(const std::pair<int, ExprNode*> &pair : this->redirOptions) {
        delete pair.second;
    }
    this->redirOptions.clear();
}

const std::string &ProcessNode::getCommandName() {
    return this->commandName;
}

void ProcessNode::addArgNode(ProcArgNode *node) {
    this->argNodes.push_back(node);
}

const std::vector<ProcArgNode*> &ProcessNode::getArgNodes() {
    return this->argNodes;
}

void ProcessNode::addRedirOption(std::pair<int, ExprNode*> &&optionPair) {
    this->redirOptions.push_back(std::move(optionPair));
}

const std::vector<std::pair<int, ExprNode*>> &ProcessNode::getRedirOptions() {
    return this->redirOptions;
}

int ProcessNode::accept(NodeVisitor *visitor) {
    return visitor->visitProcessNode(this);
}

// #########################
// ##     ProcArgNode     ##
// #########################

ProcArgNode::ProcArgNode(int lineNum) :
        ExprNode(lineNum), segmentNodes() {
}

ProcArgNode::~ProcArgNode() {
    for(ExprNode *e : this->segmentNodes) {
        delete e;
    }
    this->segmentNodes.clear();
}

void ProcArgNode::addSegmentNode(ExprNode *node) {
    ProcArgNode *argNode = dynamic_cast<ProcArgNode*>(node);
    if(argNode != 0) {
        int size = argNode->getSegmentNodes().size();
        for(int i = 0; i < size; i++) {
            ExprNode *s = argNode->segmentNodes[i];
            argNode->segmentNodes[i] = 0;
            this->segmentNodes.push_back(s);
        }
        delete argNode;
        return;
    }
    this->segmentNodes.push_back(node);
}

const std::vector<ExprNode*> &ProcArgNode::getSegmentNodes() {
    return this->segmentNodes;
}

int ProcArgNode::accept(NodeVisitor *visitor) {
    return visitor->visitProcArgNode(this);
}

// #############################
// ##     SpecialCharNode     ##
// #############################

SpecialCharNode::SpecialCharNode(int lineNum) :
        ExprNode(lineNum) {
}

SpecialCharNode::~SpecialCharNode() {
}

int SpecialCharNode::accept(NodeVisitor *visitor) {
    return visitor->visitSpecialCharNode(this);
}

// ######################
// ##     TaskNode     ##
// ######################

TaskNode::TaskNode() :
        ExprNode(0), procNodes(), background(false) {
}

TaskNode::~TaskNode() {
    for(ProcessNode *p : this->procNodes) {
        delete p;
    }
    this->procNodes.clear();
}

void TaskNode::addProcNodes(ProcessNode *node) {
    this->procNodes.push_back(node);
}

const std::vector<ProcessNode*> &TaskNode::getProcNodes() {
    return this->procNodes;
}

bool TaskNode::isBackground() {
    return this->background;
}

int TaskNode::accept(NodeVisitor *visitor) {
    return visitor->visitTaskNode(this);
}

// ###########################
// ##     InnerTaskNode     ##
// ###########################

InnerTaskNode::InnerTaskNode(ExprNode *exprNode) :
        ExprNode(0), exprNode(exprNode) {
}

InnerTaskNode::~InnerTaskNode() {
    delete this->exprNode;
    this->exprNode = 0;
}

ExprNode *InnerTaskNode::getExprNode() {
    return this->exprNode;
}

int InnerTaskNode::accept(NodeVisitor *visitor) {
    return visitor->visitInnerTaskNode(this);
}

// ########################
// ##     AssertNode     ##
// ########################

AssertNode::AssertNode(int lineNum, ExprNode *exprNode) :
        Node(lineNum), exprNode(exprNode) {
}

AssertNode::~AssertNode() {
    delete this->exprNode;
    this->exprNode = 0;
}

ExprNode *AssertNode::getExprNode() {
    return this->exprNode;
}

int AssertNode::accept(NodeVisitor *visitor) {
    return visitor->visitAssertNode(this);
}

// #######################
// ##     BlockNode     ##
// #######################

BlockNode::BlockNode() :
        Node(0), nodeList() {
}

BlockNode::~BlockNode() {
    for(Node *n : this->nodeList) {
        delete n;
    }
    this->nodeList.clear();
}

void BlockNode::addNode(Node *node) {
    this->nodeList.push_back(node);
}

void BlockNode::insertNodeToFirst(Node *node) {
    this->nodeList.push_front(node);
}

const std::list<Node*> &BlockNode::getNodeList() {
    return this->nodeList;
}

int BlockNode::accept(NodeVisitor *visitor) {
    return visitor->visitBlockNode(this);
}

// ######################
// ##     BlockEnd     ##
// ######################

BlockEndNode::BlockEndNode(int lineNum) :
        Node(lineNum) {
}

// #######################
// ##     BreakNode     ##
// #######################

BreakNode::BreakNode(int lineNum) :
        BlockEndNode(lineNum) {
}

int BreakNode::accept(NodeVisitor *visitor) {
    return visitor->visitBreakNode(this);
}

// ##########################
// ##     ContinueNode     ##
// ##########################

ContinueNode::ContinueNode(int lineNum) :
        BlockEndNode(lineNum) {
}

int ContinueNode::accept(NodeVisitor *visitor) {
    return visitor->visitContinueNode(this);
}

// ###########################
// ##     ExportEnvNode     ##
// ###########################

ExportEnvNode::ExportEnvNode(int lineNum, std::string &&envName, ExprNode *exprNode) :
        Node(lineNum), envName(std::move(envName)), exprNode(exprNode) {
}

ExportEnvNode::~ExportEnvNode() {
    delete this->exprNode;
    this->exprNode = 0;
}

const std::string &ExportEnvNode::getEnvName() {
    return this->envName;
}

ExprNode *ExportEnvNode::getExprNode() {
    return this->exprNode;
}

int ExportEnvNode::accept(NodeVisitor *visitor) {
    return visitor->visitExportEnvNode(this);
}

// ###########################
// ##     ImportEnvNode     ##
// ###########################

ImportEnvNode::ImportEnvNode(int lineNum, std::string &&envName) :
        Node(lineNum), envName(std::move(envName)) {
}

const std::string &ImportEnvNode::getEnvName() {
    return this->envName;
}

int ImportEnvNode::accept(NodeVisitor *visitor) {
    return visitor->visitImportEnvNode(this);
}

// ######################
// ##     LoopNode     ##
// ######################

LoopNode::LoopNode(int lineNum) :
        Node(lineNum) {
}

// #####################
// ##     ForNode     ##
// #####################

ForNode::ForNode(int lineNum, Node *initNode, Node *condNode, Node *iterNode, BlockNode *blockNode) :
        LoopNode(lineNum), initNode(initNode != 0 ? initNode : new EmptyNode()),
        condNode(condNode != 0 ? condNode : new VarNode(lineNum, std::string(TRUE))),
        iterNode(iterNode != 0 ? iterNode : new EmptyNode()), blockNode(blockNode) {
}

ForNode::~ForNode() {
    delete this->initNode;
    this->initNode = 0;

    delete this->condNode;
    this->condNode = 0;

    delete this->iterNode;
    this->iterNode = 0;

    delete this->blockNode;
    this->blockNode = 0;
}

Node *ForNode::getInitNode() {
    return this->initNode;
}

Node *ForNode::getCondNode() {
    return this->condNode;
}

Node *ForNode::getIterNode() {
    return this->iterNode;
}

BlockNode *ForNode::getBlockNode() {
    return this->blockNode;
}

int ForNode::accept(NodeVisitor *visitor) {
    return visitor->visitForNode(this);
}

// #######################
// ##     WhileNode     ##
// #######################

WhileNode::WhileNode(int lineNum, ExprNode *condNode, BlockNode *blockNode, bool asDoWhile) :
        LoopNode(lineNum), condNode(condNode), blockNode(blockNode), asDoWhile(asDoWhile) {
}

WhileNode::~WhileNode() {
    delete this->condNode;
    this->condNode = 0;

    delete this->blockNode;
    this->blockNode = 0;
}

ExprNode *WhileNode::getCondNode() {
    return this->condNode;
}

BlockNode *WhileNode::getBlockNode() {
    return this->blockNode;
}

bool WhileNode::isDoWhile() {
    return this->asDoWhile;
}

int WhileNode::accept(NodeVisitor *visitor) {
    return visitor->visitWhileNode(this);
}

// ####################
// ##     IfNode     ##
// ####################

IfNode::IfNode(int lineNum, ExprNode *condNode, BlockNode *thenNode, BlockNode *elseNode) :
        Node(lineNum), condNode(condNode), thenNode(thenNode),
        elseNode(elseNode != 0 ? elseNode : new BlockNode()) {
}

IfNode::~IfNode() {
    delete this->condNode;
    this->condNode = 0;

    delete this->thenNode;
    this->thenNode = 0;

    delete this->elseNode;
    this->elseNode = 0;
}

ExprNode *IfNode::getCondNode() {
    return this->condNode;
}

BlockNode *IfNode::getThenNode() {
    return this->thenNode;
}

BlockNode *IfNode::getElseNode() {
    return this->elseNode;
}

int IfNode::accept(NodeVisitor *visitor) {
    return visitor->visitIfNode(this);
}

// ########################
// ##     ReturnNode     ##
// ########################

ReturnNode::ReturnNode(int lineNum, ExprNode *exprNode) :
        BlockEndNode(lineNum), exprNode(exprNode) {
}

ReturnNode::ReturnNode(int lineNum) :
        BlockEndNode(lineNum), exprNode() {
}

ReturnNode::~ReturnNode() {
    delete this->exprNode;
    this->exprNode = 0;
}

ExprNode *ReturnNode::getExprNode() {
    return this->exprNode;
}

int ReturnNode::accept(NodeVisitor *visitor) {
    return visitor->visitReturnNode(this);
}

// #######################
// ##     ThrowNode     ##
// #######################

ThrowNode::ThrowNode(int lineNum, ExprNode *exprNode) :
        BlockEndNode(lineNum), exprNode(exprNode) {
}

ThrowNode::~ThrowNode() {
    delete this->exprNode;
    this->exprNode = 0;
}

ExprNode *ThrowNode::getExprNode() {
    return this->exprNode;
}

int ThrowNode::accept(NodeVisitor *visitor) {
    return visitor->visitThrowNode(this);
}

// #######################
// ##     CatchNode     ##
// #######################

CatchNode::CatchNode(int lineNum, std::string &&exceptionName, TypeToken *type, BlockNode *blockNode) :
        Node(lineNum), exceptionName(std::move(exceptionName)), typeToken(type), exceptionType(0),
        blockNode(blockNode) {
}

CatchNode::~CatchNode() {
    delete this->typeToken;
    this->typeToken = 0;

    delete this->blockNode;
    this->blockNode = 0;
}

const std::string &CatchNode::getExceptionName() {
    return this->exceptionName;
}

TypeToken *CatchNode::getTypeToken() {
    return this->typeToken;
}

TypeToken *CatchNode::removeTypeToken() {
    TypeToken *t = this->typeToken;
    this->typeToken = 0;
    return t;
}

void CatchNode::setExceptionType(DSType *type) {
    this->exceptionType = type;
}

DSType *CatchNode::getExceptionType() {
    return this->exceptionType;
}

BlockNode *CatchNode::getBlockNode() {
    return this->blockNode;
}

int CatchNode::accept(NodeVisitor *visitor) {
    return visitor->visitCatchNode(this);
}

// #####################
// ##     TryNode     ##
// #####################

TryNode::TryNode(int lineNum, BlockNode *blockNode) :
        Node(lineNum), blockNode(blockNode), catchNodes(), finallyNode() {
}

TryNode::~TryNode() {
    delete this->blockNode;
    this->blockNode = 0;

    for(CatchNode *n : this->catchNodes) {
        delete n;
    }
    this->catchNodes.clear();

    delete this->finallyNode;
    this->finallyNode = 0;
}

BlockNode *TryNode::getBlockNode() {
    return this->blockNode;
}

void TryNode::addCatchNode(CatchNode *catchNode) {
    this->catchNodes.push_back(catchNode);
}

const std::vector<CatchNode*> &TryNode::getCatchNodes() {
    return this->catchNodes;
}

void TryNode::addFinallyNode(Node *finallyNode) {
    if(this->finallyNode != 0) {
        delete this->finallyNode;
    }
    this->finallyNode = finallyNode;
}

Node *TryNode::getFinallyNode() {
    if(this->finallyNode == 0) {
        this->finallyNode = new EmptyNode();
    }
    return this->finallyNode;
}

int TryNode::accept(NodeVisitor *visitor) {
    return visitor->visitTryNode(this);
}

// #########################
// ##     FinallyNode     ##
// #########################

FinallyNode::FinallyNode(int lineNum, BlockNode *block) :
        Node(lineNum), blockNode(block) {
}

FinallyNode::~FinallyNode() {
    delete this->blockNode;
    this->blockNode = 0;
}

BlockNode *FinallyNode::getBlockNode() {
    return this->blockNode;
}

int FinallyNode::accept(NodeVisitor *visitor) {
    return visitor->visitFinallyNode(this);
}

// #########################
// ##     VarDeclNode     ##
// #########################

VarDeclNode::VarDeclNode(int lineNum, std::string &&varName, ExprNode *initValueNode, bool readOnly) :
        Node(lineNum), varName(std::move(varName)), readOnly(readOnly), global(false),
        initValueNode(initValueNode) {
}

VarDeclNode::~VarDeclNode() {
    delete this->initValueNode;
    this->initValueNode = 0;
}

const std::string &VarDeclNode::getVarName() {
    return this->varName;
}

bool VarDeclNode::isReadOnly() {
    return this->readOnly;
}

void VarDeclNode::setGlobal(bool global) {
    this->global = global;
}

bool VarDeclNode::isGlobal() {
    return this->global;
}

ExprNode *VarDeclNode::getInitValueNode() {
    return this->initValueNode;
}

int VarDeclNode::accept(NodeVisitor *visitor) {
    return visitor->visitVarDeclNode(this);
}

// ########################
// ##     AssignNode     ##
// ########################

AssignNode::AssignNode(ExprNode *leftNode, ExprNode *rightNode) :
        ExprNode(leftNode->getLineNum()), leftNode(leftNode), rightNode(rightNode) {
}

AssignNode::~AssignNode() {
    delete this->leftNode;
    this->leftNode = 0;

    delete this->rightNode;
    this->rightNode  = 0;
}

ExprNode *AssignNode::getLeftNode() {
    return this->leftNode;
}

ExprNode *AssignNode::getRightNode() {
    return this->rightNode;
}

int AssignNode::accept(NodeVisitor *visitor) {
    return visitor->visitAssignNode(this);
}

// #################################
// ##     FieldSelfAssignNode     ##
// #################################

FieldSelfAssignNode::FieldSelfAssignNode(ApplyNode *applyNode) :
    ExprNode(applyNode->getLineNum()), applyNode(applyNode) {
}

FieldSelfAssignNode::~FieldSelfAssignNode() {
    delete this->applyNode;
    this->applyNode = 0;
}

ApplyNode *FieldSelfAssignNode::getApplyNode() {
    return this->applyNode;
}

int FieldSelfAssignNode::accept(NodeVisitor *visitor) {
    return visitor->visitFieldSelfAssignNode(this);
}

// ##########################
// ##     FunctionNode     ##
// ##########################

FunctionNode::FunctionNode(int lineNum, std::string &&funcName) :
        Node(lineNum), funcName(std::move(funcName)), paramNodes(), paramTypeTokens(), returnTypeToken(), returnType(
                0), blockNode() {
}

FunctionNode::~FunctionNode() {
    for(VarNode *n : this->paramNodes) {
        delete n;
    }
    this->paramNodes.clear();

    for(TypeToken *t : this->paramTypeTokens) {
        delete t;
    }
    this->paramTypeTokens.clear();

    delete this->returnTypeToken;
    this->returnTypeToken = 0;

    delete this->blockNode;
    this->blockNode = 0;
}

const std::string &FunctionNode::getFuncName() {
    return this->funcName;
}

void FunctionNode::addParamNode(VarNode *node, TypeToken *paramType) {
    this->paramNodes.push_back(node);
    this->paramTypeTokens.push_back(paramType);
}

const std::vector<VarNode*> &FunctionNode::getParamNodes() {
    return this->paramNodes;
}

const std::vector<TypeToken*> &FunctionNode::getParamTypeTokens() {
    return this->paramTypeTokens;
}

void FunctionNode::setReturnTypeToken(TypeToken *typeToken) {
    this->returnTypeToken = typeToken;
}

TypeToken *FunctionNode::getReturnTypeToken() {
    return this->returnTypeToken;
}

void FunctionNode::setReturnType(DSType *returnType) {
    this->returnType = returnType;
}

DSType *FunctionNode::getReturnType() {
    return this->returnType;
}

void FunctionNode::setBlockNode(BlockNode *blockNode) {
    this->blockNode = blockNode;
}

BlockNode *FunctionNode::getBlockNode() {
    return this->blockNode;
}

int FunctionNode::accept(NodeVisitor *visitor) {
    return visitor->visitFunctionNode(this);
}

// #######################
// ##     EmptyNode     ##
// #######################

EmptyNode::EmptyNode() :
        ExprNode(0) {
}

int EmptyNode::accept(NodeVisitor *visitor) {
    return visitor->visitEmptyNode(this);
}

// ######################
// ##     RootNode     ##
// ######################

RootNode::RootNode() :
        nodeList() {
}

RootNode::~RootNode() {
    for(Node *n : this->nodeList) {
        delete n;
    }
    this->nodeList.clear();
}

void RootNode::addNode(Node *node) {
    this->nodeList.push_back(node);
}

const std::list<Node*> &RootNode::getNodeList() {
    return this->nodeList;
}
