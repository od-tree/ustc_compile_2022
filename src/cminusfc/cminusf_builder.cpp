/*
 * 声明：本代码为 2020 秋 中国科大编译原理（李诚）课程实验参考实现。
 * 请不要以任何方式，将本代码上传到可以公开访问的站点或仓库
 */

#include "cminusf_builder.hpp"

#define CONST_FP(num) ConstantFP::get((float)num, module.get())
#define CONST_INT(num) ConstantInt::get(num, module.get())


// TODO: Global Variable Declarations 
// You can define global variables here
// to store state. You can expand these
// definitions if you need to.

// function that is being built
Function *cur_fun = nullptr;

// types
Type *VOID_T;
Type *INT1_T;
Type *INT32_T;
Type *INT32PTR_T;
Type *FLOAT_T;
Type *FLOATPTR_T;

/*
 * use CMinusfBuilder::Scope to construct scopes
 * scope.enter: enter a new scope
 * scope.exit: exit current scope
 * scope.push: add a new binding to current scope
 * scope.find: find and return the value bound to the name
 */

void CminusfBuilder::visit(ASTProgram &node) {
    VOID_T = Type::get_void_type(module.get());
    INT1_T = Type::get_int1_type(module.get());
    INT32_T = Type::get_int32_type(module.get());
    INT32PTR_T = Type::get_int32_ptr_type(module.get());
    FLOAT_T = Type::get_float_type(module.get());
    FLOATPTR_T = Type::get_float_ptr_type(module.get());

    for (auto decl : node.declarations) {
        decl->accept(*this);
    }
}

void CminusfBuilder::visit(ASTNum &node) {
    //!TODO: This function is empty now.
    // Add some code here.
}

void CminusfBuilder::visit(ASTVarDeclaration &node) {
    //!TODO: This function is empty now.
    // Add some code here.
}

void CminusfBuilder::visit(ASTFunDeclaration &node) {
    FunctionType *fun_type;
    Type *ret_type;
    std::vector<Type *> param_types;
    if (node.type == TYPE_INT)
        ret_type = INT32_T;
    else if (node.type == TYPE_FLOAT)
        ret_type = FLOAT_T;
    else
        ret_type = VOID_T;

    for (auto &param : node.params) {
        //!TODO: Please accomplish param_types.



    }

    fun_type = FunctionType::get(ret_type, param_types);
    auto fun = Function::create(fun_type, node.id, module.get());
    scope.push(node.id, fun);
    cur_fun = fun;
    auto funBB = BasicBlock::create(module.get(), "entry", fun);
    builder->set_insert_point(funBB);
    scope.enter();
    std::vector<Value *> args;
    for (auto arg = fun->arg_begin(); arg != fun->arg_end(); arg++) {
        args.push_back(*arg);
    }
    for (int i = 0; i < node.params.size(); ++i) {
        //!TODO: You need to deal with params
        // and store them in the scope.



    }
    node.compound_stmt->accept(*this);
    if (builder->get_insert_block()->get_terminator() == nullptr) 
    {
        if (cur_fun->get_return_type()->is_void_type())
            builder->create_void_ret();
        else if (cur_fun->get_return_type()->is_float_type())
            builder->create_ret(CONST_FP(0.));
        else
            builder->create_ret(CONST_INT(0));
    }
    scope.exit();
}

void CminusfBuilder::visit(ASTParam &node) {
    //!TODO: This function is empty now.
    // Add some code here.
}

void CminusfBuilder::visit(ASTCompoundStmt &node) {
    //!TODO: This function is not complete.
    // You may need to add some code here
    // to deal with complex statements. 
    

    for (auto &decl : node.local_declarations) {
        decl->accept(*this);
    }

    for (auto &stmt : node.statement_list) {
        stmt->accept(*this);
        if (builder->get_insert_block()->get_terminator() != nullptr)
            break;
    }
}

void CminusfBuilder::visit(ASTExpressionStmt &node) {
    //!TODO: This function is empty now.
    // Add some code here.
}

void CminusfBuilder::visit(ASTSelectionStmt &node) {
    //!TODO: This function is empty now.
    // Add some code here.
}

void CminusfBuilder::visit(ASTIterationStmt &node) {
    //!TODO: This function is empty now.
    // Add some code here.
}

void CminusfBuilder::visit(ASTReturnStmt &node) {
    if (node.expression == nullptr) {
        builder->create_void_ret();
    } else {
        //!TODO: The given code is incomplete.
        // You need to solve other return cases (e.g. return an integer).


    }
}

void CminusfBuilder::visit(ASTVar &node) {
    //!TODO: This function is empty now.
    // Add some code here.
}

void CminusfBuilder::visit(ASTAssignExpression &node) {
    //!TODO: This function is empty now.
    // Add some code here.
}

void CminusfBuilder::visit(ASTSimpleExpression &node) {
    //!TODO: This function is empty now.
    // Add some code here.
}

void CminusfBuilder::visit(ASTAdditiveExpression &node) {
    //!TODO: This function is empty now.
    // Add some code here.
}

void CminusfBuilder::visit(ASTTerm &node) {
    //!TODO: This function is empty now.
    // Add some code here.
}

void CminusfBuilder::visit(ASTCall &node) {
    //!TODO: This function is empty now.
    // Add some code here.
}
