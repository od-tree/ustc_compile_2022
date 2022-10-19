#include "BasicBlock.h"
#include "Constant.h"
#include "Function.h"
#include "IRBuilder.h"
#include "Module.h"
#include "Type.h"

#include <iostream>
#include <memory>

#ifdef DEBUG // 用于调试信息,大家可以在编译过程中通过" -DDEBUG"来开启这一选项
#define DEBUG_OUTPUT std::cout << __LINE__ << std::endl; // 输出行号的简单示例
#else
#define DEBUG_OUTPUT
#endif

#define CONST_INT(num) ConstantInt::get(num, module)

#define CONST_FP(num) ConstantFP::get(num, module) // 得到常数值的表示,方便后面多次用到

int main() {
    auto module = new Module("Cminus code");  // module name是什么无关紧要
    auto builder = new IRBuilder(nullptr, module);
    Type *Int32Type = Type::get_int32_type(module);
    Type *FloatType = Type::get_float_type(module);

    auto mainFun = Function::create(FunctionType::get(Int32Type, {}),
                                    "main", module);
    auto bb = BasicBlock::create(module, "entry", mainFun);
    builder->set_insert_point(bb);

    auto retAlloca = builder->create_alloca(Int32Type);
    builder->create_store(CONST_INT(0), retAlloca);

    auto a=builder->create_alloca(Int32Type);
    auto i=builder->create_alloca(Int32Type);
    builder->create_store(CONST_INT(10),a);
    builder->create_store(CONST_INT(0),i);

    auto whileBB=BasicBlock::create(module,"while",mainFun);
    auto retBB=BasicBlock::create(module,"return",mainFun);
    auto iLoad=builder->create_load(i);
    auto com=builder->create_icmp_lt(iLoad, CONST_INT(10));
    auto br=builder->create_cond_br(com,whileBB,retBB);

    builder->set_insert_point(whileBB);
    auto iValue=builder->create_load(i);
    auto addValue=builder->create_iadd(iValue, CONST_INT(1));
    builder->create_store(addValue,i);
    auto Avalue=builder->create_load(a);
    auto addA=builder->create_iadd(addValue,Avalue);
    builder->create_store(addA,a);
    auto judge=builder->create_icmp_lt(addValue, CONST_INT(10));
    auto whileBR=builder->create_cond_br(judge,whileBB,retBB);

    builder->set_insert_point(retBB);
    auto aAns=builder->create_load(a);
    builder->create_ret(aAns);

    std::cout << module->print();
    delete module;
    return 0;
}
