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

    auto callee=Function::create(FunctionType::get(Int32Type,{Int32Type}),"callee",module);
    auto bb = BasicBlock::create(module, "entry", callee);
    builder->set_insert_point(bb);                        // 一个BB的开始,将当前插入指令点的位置设在bb
    auto retAlloca = builder->create_alloca(Int32Type);   // 在内存中分配返回值的位置
    auto aAlloca = builder->create_alloca(Int32Type);
    std::vector<Value *> args;
    for (auto arg :callee->get_args()) {
        args.push_back(arg);
    }

    builder->create_store(args[0], aAlloca);
    auto retValue=builder->create_imul(args[0], CONST_INT(2));
    builder->create_store(retValue,retAlloca);
    auto ans=builder->create_load(retAlloca);
    builder->create_ret(ans);

    auto mainFun = Function::create(FunctionType::get(Int32Type, {}),
                                    "main", module);
    auto BB=BasicBlock::create(module,"start",mainFun);
    builder->set_insert_point(BB);
//    auto value=builder->create_alloca(Int32Type);
//    builder->create_store(CONST_INT(110),value);
    auto call=builder->create_call(callee,{CONST_INT(110)});
    builder->create_ret(call);

    std::cout << module->print();
    delete module;
    return 0;
}
