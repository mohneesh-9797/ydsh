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

#ifndef AST_NODE_H_
#define AST_NODE_H_

#include <utility>
#include <list>

#include <core/DSType.h>
#include <core/FieldHandle.h>
#include <core/DSObject.h>
#include <core/RuntimeContext.h>
#include <ast/NodeVisitor.h>
#include <ast/TypeToken.h>
#include <parser/Token.h>

class Writer;

typedef enum {
    EVAL_SUCCESS,
    EVAL_BREAK,
    EVAL_CONTINUE,
    EVAL_THROW,
    EVAL_RETURN,
} EvalStatus;


class Node {
protected:
    unsigned int lineNum;

    /**
     * initial value is null.
     */
    DSType *type;

public:
    Node(unsigned int lineNum);
    virtual ~Node();

    unsigned int getLineNum() const;
    virtual void setType(DSType *type);

    /**
     * return null, before type checking
     */
    DSType *getType() const;
    virtual Node *convertToStringNode();
    virtual Node *convertToCmdArg();
    virtual void dump(Writer &writer) const = 0;
    virtual void accept(NodeVisitor *visitor) = 0;
    virtual EvalStatus eval(RuntimeContext &ctx) = 0;
};

// expression definition

class IntValueNode: public Node {
private:
    int tempValue;

    /**
     * initialized after type check.
     */
    std::shared_ptr<DSObject> value;

public:
    IntValueNode(unsigned int lineNum, int value);

    /**
     * before type check, return empty pointer.
     */
    std::shared_ptr<DSObject> getValue();

    void setType(DSType *type); // override
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class FloatValueNode: public Node {
private:
    double tempValue;

    /**
     * initialized after type check.
     */
    std::shared_ptr<DSObject> value;

public:
    FloatValueNode(unsigned int lineNum, double value);

    /**
     * before type check, return empty pointer.
     */
    std::shared_ptr<DSObject> getValue();

    void setType(DSType *type); // override
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class StringValueNode: public Node {
private:
    /**
     * after type checking, is broken.
     */
    std::string tempValue;

    /**
     * initialized after type check.
     */
    std::shared_ptr<DSObject> value;

public:
    /**
     * used for CommandNode. lineNum is always 0.
     */
    StringValueNode(std::string &&value);

    StringValueNode(unsigned int lineNum, std::string &&value);

    /**
     * before type check, return empty pointer.
     */
    std::shared_ptr<DSObject> getValue();

    void setType(DSType *type); // override
    Node *convertToStringNode(); // override
    Node *convertToCmdArg(); // override
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class StringExprNode: public Node {
private:
    std::vector<Node*> nodes;

public:
    StringExprNode(unsigned int lineNum);
    ~StringExprNode();

    void addExprNode(Node *node);
    const std::vector<Node*> &getExprNodes();
    Node *convertToStringNode(); // override
    Node *convertToCmdArg(); // override
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class ArrayNode: public Node {
private:
    std::vector<Node*> nodes;

public:
    ArrayNode(unsigned int lineNum, Node *node);
    ~ArrayNode();

    void addExprNode(Node *node);
    void setExprNode(unsigned int index, Node *node);
    const std::vector<Node*> &getExprNodes();
    void dump(Writer &writer) const; // override
    void accept(NodeVisitor *visitor);	//override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class MapNode: public Node {
private:
    std::vector<Node*> keyNodes;
    std::vector<Node*> valueNodes;

public:
    MapNode(unsigned int lineNum, Node *keyNode, Node *valueNode);
    ~MapNode();

    void addEntry(Node *keyNode, Node *valueNode);
    void setKeyNode(unsigned int index, Node *keyNode);
    const std::vector<Node*> &getKeyNodes();
    void setValueNode(unsigned int index, Node *valueNode);
    const std::vector<Node*> &getValueNodes();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class TupleNode: public Node {
private:
    /**
     * at least two nodes
     */
    std::vector<Node*> nodes;

public:
    TupleNode(unsigned int lineNum, Node *leftNode, Node *rightNode);
    ~TupleNode();

    void addNode(Node *node);
    const std::vector<Node*> &getNodes();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

/**
 * base class for VarNode, AccessNode
 */
class AssignableNode: public Node {
protected:
    bool readOnly;

    /**
     * if node is VarNode, treat as var index.
     * if node is AccessNode, treat as field index.
     */
    int index;

public:
    AssignableNode(unsigned int lineNum);
    virtual ~AssignableNode();

    bool isReadOnly();
    int getIndex();
};

class VarNode: public AssignableNode {
private:
    std::string varName;
    bool global;

public:
    VarNode(unsigned int lineNum, std::string &&varName);

    const std::string &getVarName();
    void setAttribute(FieldHandle *handle);
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    bool isGlobal();
    int getVarIndex();
    EvalStatus eval(RuntimeContext &ctx); // override

    // for ArgsNode
    /**
     * extract varName from varNode.
     * after extracting, delete varNode.
     */
    static std::string extractVarNameAndDelete(VarNode *node);
};

class AccessNode: public AssignableNode {
public:
    typedef enum {
        NOP,
        DUP_RECV,
        DUP_RECV_AND_SWAP,
    } AdditionalOp;

private:
    Node* recvNode;
    std::string fieldName;
    AdditionalOp additionalOp;

public:
    AccessNode(Node *recvNode, std::string &&fieldName);
    ~AccessNode();

    Node *getRecvNode();
    void setFieldName(const std::string &fieldName);
    const std::string &getFieldName();
    void setAttribute(FieldHandle *handle);
    int getFieldIndex();
    void setAdditionalOp(AdditionalOp op);
    AdditionalOp getAdditionnalOp();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class CastNode: public Node {
public:
    typedef enum {
        NOP,
        INT_TO_FLOAT,
        FLOAT_TO_INT,
        TO_STRING,
        CHECK_CAST,
    } CastOp;

private:
    Node *exprNode;
    TypeToken *targetTypeToken;
    CastOp opKind;

    /**
     * for string cast
     */
    int fieldIndex;

public:
    CastNode(Node *exprNode, TypeToken *type);
    ~CastNode();

    Node *getExprNode();
    TypeToken *getTargetTypeToken();

    /**
     * remove type token, and return removed it.
     */
    TypeToken *removeTargetTypeToken();

    void setOpKind(CastOp opKind);
    CastOp getOpKind();
    void setFieldIndex(int index);
    int getFieldIndex();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	//override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class InstanceOfNode: public Node {
public:
    typedef enum {
        ALWAYS_FALSE,
        ALWAYS_TRUE,
        INSTANCEOF,
    } InstanceOfOp;

private:
    Node* targetNode;
    TypeToken* targetTypeToken;
    DSType *targetType;
    InstanceOfOp opKind;

public:
    InstanceOfNode(Node *targetNode, TypeToken *tyep);
    ~InstanceOfNode();

    Node *getTargetNode();
    TypeToken *getTargetTypeToken();

    /**
     * remove type token, and return removed it.
     */
    TypeToken *removeTargetTypeToken();

    void setTargetType(DSType *targetType);
    DSType *getTargetType();
    void setOpKind(InstanceOfOp opKind);
    InstanceOfOp getOpKind();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	//override
    EvalStatus eval(RuntimeContext &ctx); // override
};

/**
 * binary operator call.
 */
class BinaryOpNode : public Node {
private:
    /**
     * after call this->createApplyNode(), will be null.
     */
    Node *leftNode;

    /**
     * after call this->createApplyNode(), will be null.
     */
    Node *rightNode;

    TokenKind op;

    /**
     * before call this->createApplyNode(), it is null.
     */
    ApplyNode *applyNode;

public:
    BinaryOpNode(Node *leftNode, TokenKind op, Node *rightNode);
    ~BinaryOpNode();

    Node *getLeftNode();
    void setLeftNode(Node *leftNode);
    Node *getRightNode();
    void setRightNode(Node *rightNode);

    /**
     * create ApplyNode and set to this->applyNode.
     * leftNode and rightNode will be null.
     */
    ApplyNode *creatApplyNode();

    /**
     * return null, before call this->createApplyNode().
     */
    ApplyNode *getApplyNode();

    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);   // override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class ArgsNode : public Node {
private:
    std::vector<std::pair<std::string, Node*>> argPairs;

    /**
     * may be null, if not has named parameter.
     * size is equivalent to argsPair.size().
     * key is order of arg, value is parameter index.
     */
    unsigned int *paramIndexMap;

    /**
     * size of all parameter of callee.
     * may be not equivalent to argPairs.size() if has default parameter.
     */
    unsigned int paramSize;

public:
    ArgsNode();
    ~ArgsNode();

    /**
     * if argNode is AssignNode and left hand side node is VarNode,
     * treat as named argument.
     */
    void addArg(Node *argNode);

    void setArg(unsigned int index, Node *argNode);
    void initIndexMap();
    void addParamIndex(unsigned int index, unsigned int value);
    unsigned int *getParamIndexMap();
    void setParamSize(unsigned int size);
    unsigned int getParamSize();
    const std::vector<std::pair<std::string, Node*>> &getArgPairs();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);   // override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class ApplyNode: public Node {
private:
    Node *recvNode;
    ArgsNode *argsNode;

    unsigned char attributeSet;

public:
    ApplyNode(Node *recvNode, ArgsNode *argsNode);
    ~ApplyNode();

    Node *getRecvNode();
    ArgsNode *getArgsNode();
    void setAttribute(unsigned char attribute);
    void unsetAttribute(unsigned char attribute);
    bool hasAttribute(unsigned char attribute);
    void setFuncCall(bool asFuncCall);
    bool isFuncCall();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override

    const static unsigned char FUNC_CALL = 1 << 0;
    const static unsigned char INDEX     = 1 << 1;
};

/**
 * allocate new DSObject and call constructor.
 */
class NewNode : public Node {
private:
    TypeToken *targetTypeToken;
    ArgsNode *argsNode;

public:
    NewNode(unsigned int lineNum, TypeToken *targetTypeToken, ArgsNode *argsNode);
    ~NewNode();

    TypeToken *getTargetTypeToken();

    /**
     * remove type token and return removed type token.
     */
    TypeToken *removeTargetTypeToken();

    ArgsNode *getArgsNode();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);   // override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class CondOpNode: public Node {
private:
    Node* leftNode;
    Node* rightNode;

    /**
     * if true, conditional and. otherwise, conditional or
     */
    bool andOp;

public:
    CondOpNode(Node *leftNode, Node *rightNode, bool isAndOp);
    ~CondOpNode();

    Node *getLeftNode();
    Node *getRightNode();
    bool isAndOp();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	//override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class CmdNode: public Node {	//FIXME: redirect option
private:
    std::string commandName;
    std::vector<CmdArgNode*> argNodes;
    std::vector<std::pair<int, Node*>> redirOptions;

public:
    CmdNode(unsigned int lineNum, std::string &&commandName);
    ~CmdNode();

    const std::string &getCommandName();
    void addArgNode(CmdArgNode *node);
    const std::vector<CmdArgNode*> &getArgNodes();
    void addRedirOption(std::pair<int, Node*> &&optionPair);
    const std::vector<std::pair<int, Node*>> &getRedirOptions();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	//override
    EvalStatus eval(RuntimeContext &ctx); // override
};

/**
 * for command argument
 */
class CmdArgNode: public Node {
private:
    std::vector<Node*> segmentNodes;

public:
    CmdArgNode(Node *segmentNode);
    ~CmdArgNode();

    void addSegmentNode(Node *node);
    const std::vector<Node*> &getSegmentNodes();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class SpecialCharNode: public Node {	//FIXME:
private:
    std::string name;

public:
    SpecialCharNode(unsigned int lineNum, std::string &&name);
    ~SpecialCharNode();

    const std::string &getName();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	//override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class PipedCmdNode: public Node {	//TODO: background ...etc
private:
    std::vector<CmdNode*> cmdNodes;

public:
    PipedCmdNode(CmdNode *node);
    ~PipedCmdNode();

    void addCmdNodes(CmdNode *node);
    const std::vector<CmdNode*> &getCmdNodes();
    void dump(Writer &writer) const; // override
    void accept(NodeVisitor *visitor);	//override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class CmdContextNode: public Node {
public:
    typedef enum {
        VOID,   // not return
        BOOL,   // return bool status
        STR,    // return stdout as string
        ARRAY,  // reutrn stdout as string array
        TASK,   // return task ctx
    } CmdRetKind;

private:
    /**
     * may PipedCmdNode, CondOpNode, CmdNode
     */
    Node* exprNode;

    CmdRetKind retKind;
    unsigned char attributeSet;

public:
    CmdContextNode(Node *exprNode);
    ~CmdContextNode();

    Node *getExprNode();
    void setAttribute(unsigned char attribute);
    void unsetAttribute(unsigned char attribute);
    bool hasAttribute(unsigned char attribute);
    void setRetKind(CmdRetKind kind);
    CmdRetKind getRetKind();
    Node *convertToStringNode(); // override
    Node *convertToCmdArg(); // override
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	//override
    EvalStatus eval(RuntimeContext &ctx); // override

    const static unsigned char BACKGROUND = 1 << 0;
    const static unsigned char FORK       = 1 << 1;
};

// statement definition

class AssertNode: public Node {
private:
    Node *exprNode;

public:
    AssertNode(unsigned int lineNum, Node *exprNode);
    ~AssertNode();

    Node *getExprNode();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class BlockNode: public Node {
private:
    std::list<Node*> nodeList;

public:
    BlockNode(unsigned int lineNum);
    ~BlockNode();

    void addNode(Node *node);
    void insertNodeToFirst(Node *node);
    const std::list<Node*> &getNodeList();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

/**
 * base class for break, continue, return, throw node
 */
class BlockEndNode: public Node {
public:
    BlockEndNode(unsigned int lineNum);
};

class BreakNode: public BlockEndNode {
public:
    BreakNode(unsigned int lineNum);

    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class ContinueNode: public BlockEndNode {
public:
    ContinueNode(unsigned int lineNum);

    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class ExportEnvNode: public Node {
private:
    std::string envName;
    Node* exprNode;

public:
    ExportEnvNode(unsigned int lineNum, std::string &&envName, Node *exprNode);
    ~ExportEnvNode();

    const std::string &getEnvName();
    Node *getExprNode();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class ImportEnvNode: public Node {
private:
    std::string envName;
    bool global;
    int varIndex;

public:
    ImportEnvNode(unsigned int lineNum, std::string &&envName);

    const std::string &getEnvName();
    void setAttribute(FieldHandle *handle);
    bool isGlobal();
    int getVarIndex();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class ForNode: public Node {
private:
    /**
     * may be empty node
     */
    Node *initNode;

    /**
     * may be empty node
     */
    Node *condNode;

    /**
     * may be empty node
     */
    Node *iterNode;

    BlockNode *blockNode;

public:
    /**
     * initNode may be null.
     * condNode may be null.
     * iterNode may be null.
     */
    ForNode(unsigned int lineNum, Node *initNode, Node *condNode, Node *iterNode, BlockNode *blockNode);
    ~ForNode();

    Node *getInitNode();
    Node *getCondNode();
    Node *getIterNode();
    BlockNode *getBlockNode();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class WhileNode: public Node {
private:
    Node *condNode;
    BlockNode *blockNode;

public:
    WhileNode(unsigned int lineNum, Node *condNode, BlockNode *blockNode);
    ~WhileNode();

    Node *getCondNode();
    BlockNode *getBlockNode();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	//override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class DoWhileNode : public Node {
private:
    BlockNode *blockNode;
    Node *condNode;

public:
    DoWhileNode(unsigned int lineNum, BlockNode *blockNode, Node *condNode);
    ~DoWhileNode();

    BlockNode *getBlockNode();
    Node *getCondNode();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);   //override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class IfNode: public Node {
private:
    Node *condNode;
    BlockNode *thenNode;

    std::vector<Node*> elifCondNodes;
    std::vector<BlockNode*> elifThenNodes;

    /**
     * may be null, if has no else block
     */
    BlockNode *elseNode;

public:
    /**
     * elseNode may be null
     */
    IfNode(unsigned int lineNum, Node *condNode, BlockNode *thenNode);
    ~IfNode();

    Node *getCondNode();
    BlockNode *getThenNode();
    void addElifNode(Node *condNode, BlockNode *thenNode);
    const std::vector<Node*> &getElifCondNodes();
    const std::vector<BlockNode*> &getElifThenNodes();
    void addElseNode(BlockNode *elseNode);

    /*
     * return EmptyBlockNode, if elseNode is null.
     */
    BlockNode *getElseNode();

    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class ReturnNode: public BlockEndNode {
private:
    /**
     * may be null, if has no return value
     */
    Node* exprNode;

public:
    ReturnNode(unsigned int lineNum, Node *exprNode);
    ReturnNode(unsigned int lineNum);
    ~ReturnNode();

    /**
     * return null if has no return value
     */
    Node *getExprNode();

    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class ThrowNode: public BlockEndNode {
private:
    Node* exprNode;

public:
    ThrowNode(unsigned int lineNum, Node *exprNode);
    ~ThrowNode();

    Node *getExprNode();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class CatchNode: public Node {
private:
    std::string exceptionName;
    TypeToken *typeToken;

    /**
     * may be null, if has no type annotation.
     */
    DSType *exceptionType;

    BlockNode *blockNode;

public:
    CatchNode(unsigned int lineNum, std::string &&exceptionName,
            BlockNode *blockNode);
    CatchNode(unsigned int lineNum, std::string &&exceptionName,
            TypeToken *type, BlockNode *blockNode);
    ~CatchNode();

    const std::string &getExceptionName();
    TypeToken *getTypeToken();

    /**
     * get type token and set 0.
     */
    TypeToken *removeTypeToken();

    void setExceptionType(DSType *type);

    /**
     * return null if has no exception type
     */
    DSType *getExceptionType();

    BlockNode *getBlockNode();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class TryNode: public Node {	//TODO: finallyNode
private:
    BlockNode* blockNode;

    /**
     * may be empty
     */
    std::vector<CatchNode*> catchNodes;

    /**
     * may be EmptyNdoe
     */
    Node* finallyNode;

public:
    TryNode(unsigned int lineNum, BlockNode *blockNode);
    ~TryNode();

    BlockNode *getBlockNode();
    void addCatchNode(CatchNode *catchNode);
    const std::vector<CatchNode*> &getCatchNodes();
    void addFinallyNode(Node *finallyNode);
    Node *getFinallyNode();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class FinallyNode: public Node {
private:
    BlockNode* blockNode;

public:
    FinallyNode(unsigned int lineNum, BlockNode *block);
    ~FinallyNode();

    BlockNode *getBlockNode();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class VarDeclNode: public Node {
private:
    std::string varName;
    bool readOnly;
    bool global;
    int varIndex;
    Node* initValueNode;

public:
    VarDeclNode(unsigned int lineNum, std::string &&varName, Node *initValueNode, bool readOnly);
    ~VarDeclNode();

    const std::string &getVarName();
    bool isReadOnly();
    void setAttribute(FieldHandle *handle);
    bool isGlobal();
    Node *getInitValueNode();
    int getVarIndex();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

/**
 * for assignment, self assignment or named parameter
 * assignment is statement.
 * so, after type checking, type is always VoidType
 */
class AssignNode: public Node {
private:
    /**
     * must be VarNode or AccessNode
     */
    Node* leftNode;

    Node* rightNode;

    /**
     * if true, treat as self assignment.
     */
    bool selfAssign;

public:
    AssignNode(Node *leftNode, Node *rightNode, bool selfAssign = false);
    ~AssignNode();

    Node *getLeftNode();
    void setRightNode(Node *rightNode);
    Node *getRightNode();
    bool isSelfAssignment();
    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);   // override
    EvalStatus eval(RuntimeContext &ctx); // override

    /**
     * for ArgsNode
     * split AssignNode to leftNode and rightNode.
     * after splitting, delete AssignNode.
     */
    static std::pair<Node*, Node*> split(AssignNode *node);
};

class FunctionNode: public Node {	//FIXME: named parameter
private:
    std::string funcName;

    /**
     * for parameter definition.
     */
    std::vector<VarNode*> paramNodes;

    /**
     * type token of each parameter
     */
    std::vector<TypeToken*> paramTypeTokens;

    TypeToken *returnTypeToken;

    DSType *returnType;

    BlockNode *blockNode;

public:
    FunctionNode(unsigned int lineNum, std::string &&funcName);
    ~FunctionNode();

    const std::string &getFuncName();
    void addParamNode(VarNode *node, TypeToken *paramType);
    const std::vector<VarNode*> &getParamNodes();

    /**
     * get unresolved types
     */
    const std::vector<TypeToken*> &getParamTypeTokens();

    void setReturnTypeToken(TypeToken *typeToken);
    TypeToken *getReturnTypeToken();
    void setReturnType(DSType *returnType);

    /**
     * return null, if has no return type.
     */
    DSType *getReturnType();

    void setBlockNode(BlockNode *blockNode);

    /**
     * return null before call setBlockNode()
     */
    BlockNode *getBlockNode();

    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

// class ClassNode
// class ConstructorNode

class EmptyNode: public Node {
public:
    EmptyNode();
    EmptyNode(unsigned int lineNum);

    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);	// override
    EvalStatus eval(RuntimeContext &ctx); // override
};

class DummyNode: public Node {
public:
    DummyNode();

    void dump(Writer &writer) const;  // override
    void accept(NodeVisitor *visitor);   // override
    EvalStatus eval(RuntimeContext &ctx); // override
};

/**
 * Root Node of AST.
 * this class is not inheritance of Node
 */
class RootNode {	//FIXME:
private:
    std::list<Node*> nodeList;

public:
    RootNode();
    ~RootNode();

    void addNode(Node *node);
    const std::list<Node*> &getNodeList() const;
    EvalStatus eval(RuntimeContext &ctx, bool repl = false);
};

// helper function for node creation

std::string resolveUnaryOpName(TokenKind op);
std::string resolveBinaryOpName(TokenKind op);
TokenKind resolveAssignOp(TokenKind op);

ApplyNode *createApplyNode(Node *recvNode, std::string &&methodName);

ForNode *createForInNode(unsigned int lineNum, VarNode *varNode, Node *exprNode, BlockNode *blockNode);

Node *createSuffixNode(Node *leftNode, TokenKind op);

Node *createAssignNode(Node *leftNode, TokenKind op, Node *rightNode);

Node *createIndexNode(Node *recvNode, Node *indexNode);

Node *createUnaryOpNode(TokenKind op, Node *recvNode);

Node *createBinaryOpNode(Node *leftNode, TokenKind op, Node *rightNode);

#endif /* AST_NODE_H_ */
