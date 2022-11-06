/*
 * 声明：本代码为 2020 秋 中国科大编译原理（李诚）课程实验参考实现。
 * 请不要以任何方式，将本代码上传到可以公开访问的站点或仓库
 */

#include "cminusf_builder.hpp"

#define CONST_FP(num) ConstantFP::get((float)num, module.get())
#define CONST_INT(num) ConstantInt::get(num, module.get())


// TODO: Global Variable Declarations
Value *val= nullptr;
// You can define global variables here
// to store state. You can expand these
// definitions if you need to.

// function that is being built
Function *cur_fun = nullptr;

bool pre_enter_scope = false;
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
    if(node.type==TYPE_INT)
    {
        val= CONST_INT(node.i_val);
    }
    else
    {
        val= CONST_FP(node.f_val);
    }
}

void CminusfBuilder::visit(ASTVarDeclaration &node) {
    //!TODO: This function is empty now.
    // Add some code here.
    Type *val_type;
    if((node.type)==TYPE_INT)
    {
        val_type=INT32_T;
    }
    else
    {
        val_type=FLOAT_T;
    }
    if(scope.in_global())
    {
        if(node.num== nullptr)
        {
//            auto init=(val_type==INT32_T)?: CONST_INT(0):CONST_FP(0);
            if(val_type==INT32_T)
            {
                auto init=CONST_INT(0);
                auto var=GlobalVariable::create(node.id,module.get(),val_type, false,init);
                scope.push(node.id,var);
            }
            else
            {
                auto init= CONST_FP(0);
                auto var=GlobalVariable::create(node.id,module.get(),val_type, false,init);
                scope.push(node.id,var);
            }
        }
        else
        {
//  才发现有constantzero
            auto real_type=ArrayType::get(val_type,node.num->i_val);
            auto init=ConstantZero::get(real_type,module.get());
            auto var=GlobalVariable::create(node.id,module.get(),real_type, false,init);
            scope.push(node.id,var);
        }
    }
    else
    {
        if(node.num== nullptr)
        {
            auto var=builder->create_alloca(val_type);
            scope.push(node.id,var);
        }
        else
        {
            auto real_type=ArrayType::get(val_type,node.num->i_val);
            auto var=builder->create_alloca(real_type);
            scope.push(node.id,var);
        }
    }
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
        if(param->type==TYPE_INT)
        {
            if(!(param->isarray))
            {
                param_types.push_back(INT32_T);
            }
            else
            {
                param_types.push_back(INT32PTR_T);
            }
        }
        else
        {
            if(!(param->isarray))
            {
                param_types.push_back(FLOAT_T);
            }
            else
            {
                param_types.push_back((FLOATPTR_T));
            }
        }
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
        auto param_alloc=builder->create_alloca(param_types[i]);
//        builder->create_store(args[i],param_alloc);
        scope.push(node.params[i]->id,param_alloc);
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
    bool need_exit_scope = !pre_enter_scope;
    if (pre_enter_scope)
    {
        pre_enter_scope = false;
    }
    else
    {
        scope.enter();
    }
    for (auto &decl : node.local_declarations) {
        decl->accept(*this);
    }

    for (auto &stmt : node.statement_list) {
        stmt->accept(*this);
        if (builder->get_insert_block()->get_terminator() != nullptr)
            break;
    }
    if (need_exit_scope) {
        scope.exit();
    }
}

void CminusfBuilder::visit(ASTExpressionStmt &node) {
    //!TODO: This function is empty now.
    // Add some code here.
    if(node.expression!= nullptr)
    {
        node.expression->accept(*this);
    }
}

void CminusfBuilder::visit(ASTSelectionStmt &node) {
    //!TODO: This function is empty now.
    // Add some code here.
    node.expression->accept(*this);
    auto trueBB=BasicBlock::create(module.get(),"",cur_fun);
    auto contBB=BasicBlock::create(module.get(),"",cur_fun);
    Value* cond_val;
    if(val->get_type()->is_integer_type())
    {
        cond_val=builder->create_icmp_ne(val, CONST_INT(0));
    }
    else
    {
        cond_val=builder->create_fcmp_ne(val, CONST_FP(0));
    }
    auto falseBB=BasicBlock::create(module.get(),"",cur_fun);
    if(node.else_statement==nullptr)
    {
        builder->create_cond_br(cond_val,trueBB,contBB);
        falseBB->erase_from_parent();
    }
    else
    {
        builder->create_cond_br(cond_val,trueBB,falseBB);
    }
    builder->set_insert_point(trueBB);
    node.if_statement->accept(*this);
    if(trueBB->get_terminator()== nullptr)
    {
        builder->create_br(contBB);
    }
    if(node.else_statement!= nullptr)
    {
        builder->set_insert_point(falseBB);
        node.else_statement->accept(*this);
        if(falseBB->get_terminator()== nullptr)
        {
            builder->create_br(contBB);
        }
    }
    builder->set_insert_point(contBB);
}

void CminusfBuilder::visit(ASTIterationStmt &node) {
    //!TODO: This function is empty now.
    // Add some code here.
    auto judgeBB=BasicBlock::create(module.get(),"",cur_fun);
    auto whileBB=BasicBlock::create(module.get(),"",cur_fun);
    auto contBB=BasicBlock::create(module.get(), "", cur_fun);
    if(builder->get_insert_block()->get_terminator()==nullptr)
    {
        builder->create_br(judgeBB);
    }
    builder->set_insert_point(judgeBB);
    node.expression->accept(*this);
    Value* cond;
    if(val->get_type()==INT32_T)
    {
        cond=builder->create_icmp_ne(val, CONST_INT(0));
    }
    else
    {
        cond=builder->create_icmp_ne(val, CONST_FP(0));
    }
    builder->create_cond_br(cond, whileBB, contBB);
    builder->set_insert_point(whileBB);
    node.statement->accept(*this);
    if(whileBB->get_terminator()== nullptr)
    {
        builder->create_br(whileBB);
    }
    builder->set_insert_point(contBB);
}

void CminusfBuilder::visit(ASTReturnStmt &node) {
    if (node.expression == nullptr) {
        builder->create_void_ret();
    } else {
        //!TODO: The given code is incomplete.
        // You need to solve other return cases (e.g. return an integer).
        node.expression->accept(*this);
        if(cur_fun->get_return_type()==INT32_T)
        {
            if(val->get_type()==INT32_T)
            {
                builder->create_ret(val);
            }
            else
            {
                auto tmp_val=builder->create_fptosi(val,INT32_T);
                builder->create_ret(tmp_val);
            }
        }
        else
        {
            if(val->get_type()==FLOAT_T)
            {
                builder->create_ret(val);
            }
            else
            {
                auto tmp_val=builder->create_sitofp(val,FLOAT_T);
                builder->create_ret(tmp_val);
            }
        }
    }
}

void CminusfBuilder::visit(ASTVar &node) {
    //!TODO: This function is empty now.
    // Add some code here.
}

void CminusfBuilder::visit(ASTAssignExpression &node) {
    //!TODO: This function is empty now.
    // Add some code here.
    node.expression->accept(*this);
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
