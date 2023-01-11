#include "GVN.h"

#include "BasicBlock.h"
#include "Constant.h"
#include "DeadCode.h"
#include "FuncInfo.h"
#include "Function.h"
#include "Instruction.h"
#include "logging.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

using namespace GVNExpression;
using std::string_literals::operator""s;
using std::shared_ptr;

static auto get_const_int_value = [](Value *v) { return dynamic_cast<ConstantInt *>(v)->get_value(); };
static auto get_const_fp_value = [](Value *v) { return dynamic_cast<ConstantFP *>(v)->get_value(); };
// Constant Propagation helper, folders are done for you
Constant *ConstFolder::compute(Instruction *instr, Constant *value1, Constant *value2) {
    auto op = instr->get_instr_type();
    switch (op) {
    case Instruction::add: return ConstantInt::get(get_const_int_value(value1) + get_const_int_value(value2), module_);
    case Instruction::sub: return ConstantInt::get(get_const_int_value(value1) - get_const_int_value(value2), module_);
    case Instruction::mul: return ConstantInt::get(get_const_int_value(value1) * get_const_int_value(value2), module_);
    case Instruction::sdiv: return ConstantInt::get(get_const_int_value(value1) / get_const_int_value(value2), module_);
    case Instruction::fadd: return ConstantFP::get(get_const_fp_value(value1) + get_const_fp_value(value2), module_);
    case Instruction::fsub: return ConstantFP::get(get_const_fp_value(value1) - get_const_fp_value(value2), module_);
    case Instruction::fmul: return ConstantFP::get(get_const_fp_value(value1) * get_const_fp_value(value2), module_);
    case Instruction::fdiv: return ConstantFP::get(get_const_fp_value(value1) / get_const_fp_value(value2), module_);

    case Instruction::cmp:
        switch (dynamic_cast<CmpInst *>(instr)->get_cmp_op()) {
        case CmpInst::EQ: return ConstantInt::get(get_const_int_value(value1) == get_const_int_value(value2), module_);
        case CmpInst::NE: return ConstantInt::get(get_const_int_value(value1) != get_const_int_value(value2), module_);
        case CmpInst::GT: return ConstantInt::get(get_const_int_value(value1) > get_const_int_value(value2), module_);
        case CmpInst::GE: return ConstantInt::get(get_const_int_value(value1) >= get_const_int_value(value2), module_);
        case CmpInst::LT: return ConstantInt::get(get_const_int_value(value1) < get_const_int_value(value2), module_);
        case CmpInst::LE: return ConstantInt::get(get_const_int_value(value1) <= get_const_int_value(value2), module_);
        }
    case Instruction::fcmp:
        switch (dynamic_cast<FCmpInst *>(instr)->get_cmp_op()) {
        case FCmpInst::EQ: return ConstantInt::get(get_const_fp_value(value1) == get_const_fp_value(value2), module_);
        case FCmpInst::NE: return ConstantInt::get(get_const_fp_value(value1) != get_const_fp_value(value2), module_);
        case FCmpInst::GT: return ConstantInt::get(get_const_fp_value(value1) > get_const_fp_value(value2), module_);
        case FCmpInst::GE: return ConstantInt::get(get_const_fp_value(value1) >= get_const_fp_value(value2), module_);
        case FCmpInst::LT: return ConstantInt::get(get_const_fp_value(value1) < get_const_fp_value(value2), module_);
        case FCmpInst::LE: return ConstantInt::get(get_const_fp_value(value1) <= get_const_fp_value(value2), module_);
        }
    default: return nullptr;
    }
}
Constant *ConstFolder::compute(Instruction::OpID op, Constant *value1, Constant *value2) {
    switch (op) {
    case Instruction::add: return ConstantInt::get(get_const_int_value(value1) + get_const_int_value(value2), module_);
    case Instruction::sub: return ConstantInt::get(get_const_int_value(value1) - get_const_int_value(value2), module_);
    case Instruction::mul: return ConstantInt::get(get_const_int_value(value1) * get_const_int_value(value2), module_);
    case Instruction::sdiv: return ConstantInt::get(get_const_int_value(value1) / get_const_int_value(value2), module_);
    case Instruction::fadd: return ConstantFP::get(get_const_fp_value(value1) + get_const_fp_value(value2), module_);
    case Instruction::fsub: return ConstantFP::get(get_const_fp_value(value1) - get_const_fp_value(value2), module_);
    case Instruction::fmul: return ConstantFP::get(get_const_fp_value(value1) * get_const_fp_value(value2), module_);
    case Instruction::fdiv: return ConstantFP::get(get_const_fp_value(value1) / get_const_fp_value(value2), module_);
    default: return nullptr;
    }
}
Constant *ConstFolder::compute(Instruction *instr, Constant *value1) {
    auto op = instr->get_instr_type();
    switch (op) {
    case Instruction::sitofp: return ConstantFP::get((float)get_const_int_value(value1), module_);
    case Instruction::fptosi: return ConstantInt::get((int)get_const_fp_value(value1), module_);
    case Instruction::zext: return ConstantInt::get((int)get_const_int_value(value1), module_);
    default: return nullptr;
    }
}

namespace utils {
static std::string print_congruence_class(const CongruenceClass &cc) {
    std::stringstream ss;
    if (cc.index_ == 0) {
        ss << "top class\n";
        return ss.str();
    }
    ss << "\nindex: " << cc.index_ << "\nleader: " << cc.leader_->print()
       << "\nvalue phi: " << (cc.value_phi_ ? cc.value_phi_->print() : "nullptr"s)
       << "\nvalue expr: " << (cc.value_expr_ ? cc.value_expr_->print() : "nullptr"s) << "\nmembers: {";
    for (auto &member : cc.members_)
        ss << member->print() << "; ";
    ss << "}\n";
    return ss.str();
}

static std::string dump_cc_json(const CongruenceClass &cc) {
    std::string json;
    json += "[";
    for (auto member : cc.members_) {
        if (auto c = dynamic_cast<Constant *>(member))
            json += member->print() + ", ";
        else
            json += "\"%" + member->get_name() + "\", ";
    }
    json += "]";
    return json;
}

static std::string dump_partition_json(const GVN::partitions &p) {
    std::string json;
    json += "[";
    for (auto cc : p)
        json += dump_cc_json(*cc) + ", ";
    json += "]";
    return json;
}

static std::string dump_bb2partition(const std::map<BasicBlock *, GVN::partitions> &map) {
    std::string json;
    json += "{";
    for (auto [bb, p] : map)
        json += "\"" + bb->get_name() + "\": " + dump_partition_json(p) + ",";
    json += "}";
    return json;
}

// logging utility for you
static void print_partitions(const GVN::partitions &p) {
    if (p.empty()) {
        LOG_DEBUG << "empty partitions\n";
        return;
    }
    std::string log;
    for (auto &cc : p)
        log += print_congruence_class(*cc);
    LOG_DEBUG << log; // please don't use std::cout
}
} // namespace utils

GVN::partitions GVN::join(const partitions &P1, const partitions &P2) {
    // TODO: do intersection pair-wise
    if(P1.empty()||P2.empty())
    {
        return {};
    }
    for(const auto& i:P1)
    {
        if(i->index_==0)
        {
            return P2;
        }
    }
    for(const auto& i:P2)
    {
        if(i->index_==0)
        {
            return P1;
        }
    }
    partitions p={};
    for(auto &i:P1)
    {
        for(auto &j:P2)
        {
            auto Ck=intersect(i,j);
            if(!Ck->members_.empty())
            {
                p.emplace(Ck);
            }
        }
    }
    return p;
}

std::shared_ptr<CongruenceClass> GVN::intersect(const std::shared_ptr<CongruenceClass>& Ci,
                                                const std::shared_ptr<CongruenceClass>& Cj) {
    // TODO
    shared_ptr<CongruenceClass> cc;
    if(Ci->value_expr_==Cj->value_expr_)
    {
        cc= createCongruenceClass(next_value_number_++);
        cc->value_expr_=Ci->value_expr_;
        cc->leader_=Ci->leader_;
        switch (Ci->value_expr_->get_expr_type()) {
        case Expression::e_constant: {
            cc->value_const_=std::dynamic_pointer_cast<ConstantExpression>(Ci->value_expr_);
            break;
        }
        case Expression::e_bin: {
            cc->value_bin=std::dynamic_pointer_cast<BinaryExpression>(Ci->value_expr_);
            break;
        }
        case Expression::e_phi: {
            cc->value_phi_=std::dynamic_pointer_cast<PhiExpression>(Ci->value_expr_);
            break;
        }
        case Expression::e_single: {
            cc->value_single=std::dynamic_pointer_cast<SingleExpression>(Ci->value_expr_);
            break;
        }
        case Expression::e_func:{
            cc->value_func=std::dynamic_pointer_cast<FuncExpression>(Ci->value_expr_);
            break;
        }
        case Expression::e_cmp: {
            cc->value_cmp=std::dynamic_pointer_cast<CmpExpression>(Ci->value_expr_);
            break;
        }
        case Expression::e_fcmp: {
            cc->value_fcmp=std::dynamic_pointer_cast<FCmpExpression>(Ci->value_expr_);
            break;
        }
        case Expression::e_trans: {
            cc->value_trans=std::dynamic_pointer_cast<TransExpression>(Ci->value_expr_);
            break;
        }
        case Expression::e_gep: {
            cc->value_gep=std::dynamic_pointer_cast<GepExpression>(Ci->value_expr_);
            break;
        }
        }
    }
    else
    {
        cc= createCongruenceClass(0);
    }
    for(auto& i:Ci->members_)
    {
        for(auto& j:Cj->members_)
        {
            if(i==j)
            {
                cc->members_.insert(i);
            }
        }
    }
    if((!cc->members_.empty())&&(cc->index_==0))
    {
        cc->index_=next_value_number_++;
        auto ve_phi=PhiExpression::create(Ci->leader_,Cj->leader_);
        cc->value_expr_=ve_phi;
        cc->value_phi_=ve_phi;
        cc->leader_=*cc->members_.begin();
    }
    return cc;
}

void GVN::detectEquivalences() {
    bool changed{};
    // initialize pout with top
    // iterate until converge
    BasicBlock *entry=func_->get_entry_block();
    pin_[entry]={};
    partitions p_top{};
    p_top.insert(createCongruenceClass(0));
    auto first=true;
    for(auto &bb:func_->get_basic_blocks())
    {
        if(first){
            first= false;
            continue;
        }
        pout_[&bb]=p_top;
    }
//    auto count=0;
    do {
        changed= false;
        // see the pseudo code in documentation
        first=true;
        for (auto &bb : func_->get_basic_blocks()) { // you might need to visit the blocks in depth-first order
            // get PIN of bb by predecessor(s)
            partitions p;
            auto pre_bolocks = bb.get_pre_basic_blocks();
            if (pre_bolocks.size() == 2) {
                pin_[&bb] = join(pout_[pre_bolocks.front()], pout_[pre_bolocks.back()]);
            }
            if(pre_bolocks.size()==1)
            {
                pin_[&bb] = pout_[pre_bolocks.front()];
            }
            if(pre_bolocks.empty())
            {
                pin_[&bb]={};
            }
            p = clone(pin_[&bb]);
            // iterate through all instructions in the block
            // and the phi instruction in all the successors
            if(first)
            {
                first= false;
                globel_and_argv(p);
            }
            for(auto &instr:bb.get_instructions())
            {
                p= transferFunction(&instr,&instr,p);
            }

            // check changes in pout
            for(auto &nextBB:bb.get_succ_basic_blocks())
            {
                for(auto &instr:nextBB->get_instructions())
                {
                    if(instr.is_phi())
                    {
                        Value* op;
                        bool judge=true;
                        for(auto &i:p)
                        {
                            i->members_.erase(&instr);

                        }
                        for(auto it=p.begin();it!=p.end();)
                        {
                            if((*it)->members_.empty())
                            {
                                it=p.erase(it);
                            }
                            else
                            {
                                it++;
                            }
                        }
                        if(instr.get_operand(1)==&bb)
                        {
                            op=instr.get_operand(0);
                        }
                        else
                        {
                            op=instr.get_operand(2);
                        }
                        for(auto &i:p)
                        {
                            for(auto &j:i->members_)
                            {
                                if(j==op)
                                {
                                    i->members_.insert(&instr);
                                    judge=false;
                                    break;
                                }
                            }
                        }
                        if(judge)
                        {
                            auto cons_op=ConstantExpression::create(dynamic_cast<Constant *>(op));
                            for(auto &i:p)
                            {
                                if((i->value_const_!= nullptr)&&(i->value_const_->equiv(cons_op.get())))
                                {
                                    i->members_.insert(&instr);
                                    judge=false;
                                    break;
                                }
                            }
                            if(judge)
                            {
                                auto cc = createCongruenceClass(next_value_number_++);
                                cc->leader_ =dynamic_cast<Constant *>(op);
                                cc->members_ = {&instr};
                                cc->value_expr_=cons_op;
                                cc->value_const_=cons_op;
                                p.insert(cc);
                            }
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
            utils::print_partitions(p);
            if(p!=pout_[&bb])
            {
                changed=true;
            }
            pout_[&bb]=std::move(p);
        }
    } while (changed);
}
void GVN::globel_and_argv(GVN::partitions &p) {
    auto &glob= m_->get_global_variable();
    for(auto &i:glob)
    {
        auto cc = createCongruenceClass(next_value_number_++);
        auto single_expr=SingleExpression::create(&i);
        cc->leader_ =&i;
        cc->members_ = {&i};
        cc->value_expr_=single_expr;
        cc->value_single=single_expr;
        p.insert(cc);
    }
    auto &argv= func_->get_args();
    for(auto &i:argv)
    {
        auto cc = createCongruenceClass(next_value_number_++);
        auto single_expr=SingleExpression::create(i);
        cc->leader_ =i;
        cc->members_ = {i};
        cc->value_expr_=single_expr;
        cc->value_single=single_expr;
        p.insert(cc);
    }
}

shared_ptr<Expression> GVN::valueExpr(Instruction *instr,partitions pin) {
    // TODO
    if(instr->isBinary())
    {
        return binValueExpr(instr, pin);
    }
    if(instr->is_call())
    {
        return funcValueExpr(instr, pin);
    }
    if(instr->is_cmp())
    {
        return cmpValueExpr(instr, pin);
    }
    if(instr->is_fcmp())
    {
        return fcmpValueExpr(instr, pin);
    }
    if(instr->is_si2fp()||instr->is_fp2si()||instr->is_zext())
    {
        return transValueExpr(instr, pin);
    }
    if(instr->is_gep())
    {
        return gepValueExpr(instr, pin);
    }
    return SingleExpression::create(instr);
}
shared_ptr<Expression> GVN::gepValueExpr(Instruction *instr, GVN::partitions &pin) const {
    std::vector<std::shared_ptr<Expression>> operands{};
    for(int i=0;i<instr->get_operands().size();i++)
    {
        if(dynamic_cast<Constant*>(instr->get_operands()[i])!= nullptr)
        {
            auto cons_op=ConstantExpression::create(dynamic_cast<Constant *>(instr->get_operand(i)));
            operands.push_back(cons_op);
        }
        else
        {
            for(auto &k:pin)
            {
                for(auto &j:k->members_)
                {
                    if(j==(instr->get_operand(i)))
                    {
                        operands.push_back(k->value_expr_);
                        break;
                    }
                }
            }
        }
    }
    auto gepInstr=dynamic_cast<GetElementPtrInst*>(instr);
    return GepExpression::create(gepInstr->get_element_type(),operands);
}
shared_ptr<Expression> GVN::transValueExpr(Instruction *instr, GVN::partitions &pin) const {
    auto consOp=dynamic_cast<Constant*>(instr->get_operand(0));
    if(consOp!= nullptr)
    {
        auto cons= folder_->compute(instr,consOp);
        auto consExp=ConstantExpression::create(cons);
        return consExp;
    }
    std::shared_ptr<Expression> operand= nullptr;
    for(auto &i:pin)
        {
            for(auto &j:i->members_)
            {
                if(j==instr->get_operand(0))
                {
                    operand=i->value_expr_;
                }
            }
    }
    if((operand!= nullptr)&&(operand->get_expr_type()==Expression::e_constant))
        {
            auto cons= folder_->compute(instr,std::dynamic_pointer_cast<ConstantExpression>(operand)->get_cons());
            auto consExpr=ConstantExpression::create(cons);
            return consExpr;
    }
    if((consOp== nullptr)&&(operand== nullptr))
        {
            operand=SingleExpression::create(instr);
    }
    return TransExpression::create(instr->get_instr_type(),operand);
}
shared_ptr<Expression> GVN::fcmpValueExpr(Instruction *instr, GVN::partitions &pin) {
    auto oprands=instr->get_operands();
    auto lconst=dynamic_cast<Constant*>(oprands[0]);
    auto rconst=dynamic_cast<Constant*>(oprands[1]);
    if(lconst!= nullptr)
    {
        if(rconst!= nullptr)
        {
            auto cons= folder_->compute(instr,lconst,rconst);
            auto consExp=ConstantExpression::create(cons);
            return consExp;
        }
    }

    std::shared_ptr<Expression> lop= nullptr;
    std::shared_ptr<Expression> rop= nullptr;
    for(auto &i:pin)
    {
        for(auto &j:i->members_)
        {
            if(j==oprands[0])
            {
                lop=i->value_expr_;
            }
            if(j==oprands[1])
            {
                rop=i->value_expr_;
            }
        }
    }
    if((lconst!= nullptr)&&(rop!=nullptr)&&(rop->get_expr_type()==Expression::e_constant))
    {
        auto cons= folder_->compute(instr,lconst, std::dynamic_pointer_cast<ConstantExpression>(rop)->get_cons());
        auto consExp=ConstantExpression::create(cons);
        return consExp;
    }
    if((lop!= nullptr)&&(lop->get_expr_type()==Expression::e_constant)&&(rconst!= nullptr))
    {
        auto cons= folder_->compute(instr,std::dynamic_pointer_cast<ConstantExpression>(lop)->get_cons(),rconst);
        auto consExp=ConstantExpression::create(cons);
        return consExp;
    }
    if((lop!= nullptr)&&(lop->get_expr_type()==Expression::e_constant)&&(rop!= nullptr)&&(rop->get_expr_type()==Expression::e_constant))
    {
        auto cons= folder_->compute(instr,std::dynamic_pointer_cast<ConstantExpression>(lop)->get_cons(),std::dynamic_pointer_cast<ConstantExpression>(rop)->get_cons());
        auto consExp=ConstantExpression::create(cons);
        return consExp;
    }
    if(lop==nullptr&&lconst== nullptr)
    {
        lop=SingleExpression::create(oprands[0]);
    }
    if(rop== nullptr&&rconst== nullptr)
    {
        rop=SingleExpression::create(oprands[1]);
    }
    if(lconst!= nullptr)
    {
        lop=ConstantExpression::create(lconst);
    }
    if(rconst!= nullptr)
    {
        rop=ConstantExpression::create(rconst);
    }
    return FCmpExpression::create(dynamic_cast<FCmpInst*>(instr)->get_cmp_op(),lop,rop);
}
shared_ptr<Expression> GVN::cmpValueExpr(Instruction *instr, GVN::partitions &pin) const {
    auto oprands=instr->get_operands();
    auto lconst=dynamic_cast<Constant*>(oprands[0]);
    auto rconst=dynamic_cast<Constant*>(oprands[1]);
    if(lconst!= nullptr)
    {
        if(rconst!= nullptr)
        {
            auto cons= folder_->compute(instr,lconst,rconst);
            auto consExp=ConstantExpression::create(cons);
            return consExp;
        }
    }

    std::shared_ptr<Expression> lop= nullptr;
    std::shared_ptr<Expression> rop= nullptr;
    for(auto &i:pin)
    {
        for(auto &j:i->members_)
        {
            if(j==oprands[0])
            {
                lop=i->value_expr_;
            }
            if(j==oprands[1])
            {
                rop=i->value_expr_;
            }
        }
    }
    if((lconst!= nullptr)&&(rop!=nullptr)&&(rop->get_expr_type()==Expression::e_constant))
    {
        auto cons= folder_->compute(instr,lconst, std::dynamic_pointer_cast<ConstantExpression>(rop)->get_cons());
        auto consExp=ConstantExpression::create(cons);
        return consExp;
    }
    if((lop!= nullptr)&&(lop->get_expr_type()==Expression::e_constant)&&(rconst!= nullptr))
    {
        auto cons= folder_->compute(instr,std::dynamic_pointer_cast<ConstantExpression>(lop)->get_cons(),rconst);
        auto consExp=ConstantExpression::create(cons);
        return consExp;
    }
    if((lop!= nullptr)&&(lop->get_expr_type()==Expression::e_constant)&&(rop!= nullptr)&&(rop->get_expr_type()==Expression::e_constant))
    {
        auto cons= folder_->compute(instr,std::dynamic_pointer_cast<ConstantExpression>(lop)->get_cons(),std::dynamic_pointer_cast<ConstantExpression>(rop)->get_cons());
        auto consExp=ConstantExpression::create(cons);
        return consExp;
    }
    if(lop==nullptr&&lconst== nullptr)
    {
        lop=SingleExpression::create(oprands[0]);
    }
    if(rop== nullptr&&rconst== nullptr)
    {
        rop=SingleExpression::create(oprands[1]);
    }
    if(lconst!= nullptr)
    {
        lop=ConstantExpression::create(lconst);
    }
    if(rconst!= nullptr)
    {
        rop=ConstantExpression::create(rconst);
    }
    return CmpExpression::create(dynamic_cast<CmpInst*>(instr)->get_cmp_op(),lop,rop);
}
shared_ptr<Expression> GVN::funcValueExpr(Instruction *instr, GVN::partitions &pin) const {
    std::vector<std::shared_ptr<Expression>> operands{};
    for(int i=1;i<instr->get_operands().size();i++)
    {
        if(dynamic_cast<Constant*>(instr->get_operands()[i])!= nullptr)
        {
            auto cons_op=ConstantExpression::create(dynamic_cast<Constant *>(instr->get_operand(i)));
            operands.push_back(cons_op);
        }
        else
        {
            for(auto &k:pin)
            {
                for(auto &j:k->members_)
                {
                    if(j==(instr->get_operand(i)))
                    {
                        operands.push_back(k->value_expr_);
                        break;
                    }
                }
            }
        }
    }
    return FuncExpression::create(instr->get_operand(0),operands,
                                  func_info_->is_pure_function(dynamic_cast<Function *>(instr->get_operand(0))),instr);
}
shared_ptr<Expression> GVN::binValueExpr(Instruction *instr, GVN::partitions &pin) {
    auto oprands=instr->get_operands();
    auto lconst=dynamic_cast<Constant*>(oprands[0]);
    auto rconst=dynamic_cast<Constant*>(oprands[1]);
    if(lconst!= nullptr)
    {
        if(rconst!= nullptr)
        {
            auto cons= folder_->compute(instr,lconst,rconst);
            auto consExp=ConstantExpression::create(cons);
            return consExp;
        }
    }

    std::shared_ptr<Expression> lop= nullptr;
    std::shared_ptr<Expression> rop= nullptr;
    for(auto &i:pin)
    {
        for(auto &j:i->members_)
        {
            if(j==oprands[0])
            {
                lop=i->value_expr_;
            }
            if(j==oprands[1])
            {
                rop=i->value_expr_;
            }
        }
    }
    if((lconst!= nullptr)&&(rop!=nullptr)&&(rop->get_expr_type()==Expression::e_constant))
    {
        auto cons= folder_->compute(instr,lconst, std::dynamic_pointer_cast<ConstantExpression>(rop)->get_cons());
        auto consExp=ConstantExpression::create(cons);
        return consExp;
    }
    if((lop!= nullptr)&&(lop->get_expr_type()==Expression::e_constant)&&(rconst!= nullptr))
    {
        auto cons= folder_->compute(instr,std::dynamic_pointer_cast<ConstantExpression>(lop)->get_cons(),rconst);
        auto consExp=ConstantExpression::create(cons);
        return consExp;
    }
    if((lop!= nullptr)&&(lop->get_expr_type()==Expression::e_constant)&&(rop!= nullptr)&&(rop->get_expr_type()==Expression::e_constant))
    {
        auto cons= folder_->compute(instr,std::dynamic_pointer_cast<ConstantExpression>(lop)->get_cons(),std::dynamic_pointer_cast<ConstantExpression>(rop)->get_cons());
        auto consExp=ConstantExpression::create(cons);
        return consExp;
    }
    if(lop==nullptr&&lconst== nullptr)
    {
        lop=SingleExpression::create(oprands[0]);
    }
    if(rop== nullptr&&rconst== nullptr)
    {
        rop=SingleExpression::create(oprands[1]);
    }
    if(lconst!= nullptr)
    {
        lop=ConstantExpression::create(lconst);
    }
    if(rconst!= nullptr)
    {
        rop=ConstantExpression::create(rconst);
    }
    return BinaryExpression::create(instr->get_instr_type(),lop,rop);
}

// instruction of the form `x = e`, mostly x is just e (SSA), but for copy stmt x is a phi instruction in the
// successor. Phi values (not copy stmt) should be handled in detectEquiv
/// \param bb basic block in which the transfer function is called
GVN::partitions GVN::transferFunction(Instruction *x,Value  *e, partitions pin) {
    partitions pout = clone(pin);
    if(x->is_void()||x->is_phi())
    {
        return pout;
    }
    // TODO: get different ValueExpr by Instruction::OpID, modify pout
    for(auto &i:pout)
    {
        i->members_.erase(x);
//        if(i->members_.empty())
//        {
//            pout.erase(i);
//        }
    }
    for(auto it=pout.begin();it!=pout.end();)
    {
        if((*it)->members_.empty())
        {
            it=pout.erase(it);
        }
        else
        {
            it++;
        }
    }
    auto ve= valueExpr(x,pin);
    auto vpf= valuePhiFunc(ve,pin);
    bool judge= false;

    if(vpf!= nullptr)
    {
        ve=vpf;
    }
    if(ve!= nullptr) {
        for (auto &i : pout) {
            switch (ve->get_expr_type()) {
                case Expression::e_constant:
                {
                    auto ve_cons=std::dynamic_pointer_cast<ConstantExpression>(ve);
                    if((i->value_const_!= nullptr)&&(i->value_const_->equiv(ve_cons.get())))
                    {
                        judge =true;
                        i->members_.insert(x);
                    }
                    break;
                }
                case Expression::e_bin:
                {
                    auto ve_bin = std::dynamic_pointer_cast<BinaryExpression>(ve);
                    if ((i->value_bin!= nullptr)&&(i->value_bin->equiv(ve_bin.get())))
                    {
                        judge = true;
                        i->members_.insert(x);
                    }
                    break;
                }
                case Expression::e_phi: {
                    auto ve_phi=std::dynamic_pointer_cast<PhiExpression>(ve);
                    if((i->value_phi_!= nullptr)&&(i->value_phi_->equiv(ve_phi.get())))
                    {
                        judge =true;
                        i->members_.insert(x);
                    }
                    break;
                }
                case Expression::e_single: {
                    auto ve_single=std::dynamic_pointer_cast<SingleExpression>(ve);
                    if((i->value_single!= nullptr)&&(i->value_single->equiv(ve_single.get())))
                    {
                        judge=true;
                        i->members_.insert(x);
                    }
                    break;
                }
                case Expression::e_func: {
                    auto ve_func=std::dynamic_pointer_cast<FuncExpression>(ve);
                    if((i->value_func!= nullptr)&&(i->value_func->equiv(ve_func.get())))
                    {
                        judge =true;
                        i->members_.insert(x);
                    }
                    break;
                }
                case Expression::e_cmp: {
                    auto ve_cmp=std::dynamic_pointer_cast<CmpExpression>(ve);
                    if((i->value_cmp!= nullptr)&&(i->value_cmp->equiv(ve_cmp.get())))
                    {
                        judge=true;
                        i->members_.insert(x);
                    }
                    break;
                }
                case Expression::e_fcmp: {
                    auto ve_fcmp=std::dynamic_pointer_cast<FCmpExpression>(ve);
                    if((i->value_fcmp!= nullptr)&&(i->value_fcmp->equiv(ve_fcmp.get())))
                    {
                        judge=true;
                        i->members_.insert(x);
                    }
                    break;
                }
                case Expression::e_trans: {
                    auto ve_trans=std::dynamic_pointer_cast<TransExpression>(ve);
                    if((i->value_trans!=nullptr)&&(i->value_trans->equiv(ve_trans.get())))
                    {
                        judge=true;
                        i->members_.insert(x);
                    }
                    break;
                }
                case Expression::e_gep: {
                    auto ve_gep=std::dynamic_pointer_cast<GepExpression>(ve);
                    if((i->value_gep!= nullptr)&&(i->value_gep->equiv(ve_gep.get())))
                    {
                        judge=true;
                        i->members_.insert(x);
                    }
                    break;
                }
            }
        }
    }
    if (!judge) {
        auto cc = createCongruenceClass(next_value_number_++);
        cc->members_ = {x};
        cc->value_expr_ = ve;
        switch (ve->get_expr_type()) {
            case Expression::e_constant:
            {
               auto ve_cons=std::dynamic_pointer_cast<ConstantExpression>(ve);
               cc->leader_ = ve_cons->get_cons();
               cc->value_const_=ve_cons;
               break;
            }
            case Expression::e_bin: {
               auto ve_bin = std::dynamic_pointer_cast<BinaryExpression>(ve);
               cc->leader_=x;
               cc->value_bin = ve_bin;
               break;
            }
            case Expression::e_phi: {
               cc->leader_=x;
               cc->value_phi_=std::dynamic_pointer_cast<PhiExpression>(ve);
               break;
            }
            case Expression::e_single: {
                auto ve_single=std::dynamic_pointer_cast<SingleExpression>(ve);
                cc->leader_=x;
                cc->value_single=ve_single;
                break;
            }
            case Expression::e_func:{
                auto ve_func=std::dynamic_pointer_cast<FuncExpression>(ve);
                cc->leader_=x;
                cc->value_func=ve_func;
                break;
            }
            case Expression::e_cmp: {
                auto ve_cmp=std::dynamic_pointer_cast<CmpExpression>(ve);
                cc->leader_=x;
                cc->value_cmp=ve_cmp;
            break;
            }
            case Expression::e_fcmp: {
                auto ve_fcmp=std::dynamic_pointer_cast<FCmpExpression>(ve);
                cc->leader_=x;
                cc->value_fcmp=ve_fcmp;
                break;
            }
            case Expression::e_trans: {
                auto ve_trans=std::dynamic_pointer_cast<TransExpression>(ve);
                cc->leader_=x;
                cc->value_trans=ve_trans;
                break;
            }
            case Expression::e_gep: {
                auto ve_gep=std::dynamic_pointer_cast<GepExpression>(ve);
                cc->leader_=x;
                cc->value_gep=ve_gep;
                break;
            }
        }
        pout.insert(cc);
    }
    return pout;
}

shared_ptr<PhiExpression> GVN::valuePhiFunc(shared_ptr<Expression> ve, const partitions &P) {
    // TODO
    auto ve_bin=std::dynamic_pointer_cast<BinaryExpression>(ve);
    if((ve_bin!= nullptr)&&(ve_bin->get_lhs_type()==Expression::e_phi)&&(ve_bin->get_rhs_type()==Expression::e_phi)){
        auto lop1=std::dynamic_pointer_cast<PhiExpression>(ve_bin->get_lhs())->get_lhs_();
        shared_ptr<Expression> lve1;
        if(dynamic_cast<Constant*>(lop1)== nullptr)
        {
            lve1= valueExpr(dynamic_cast<Instruction*>(lop1),P);
        }
        else
        {
            lve1=ConstantExpression::create(dynamic_cast<Constant*>(lop1));
        }
        auto lop2=std::dynamic_pointer_cast<PhiExpression>(ve_bin->get_rhs())->get_lhs_();
        shared_ptr<Expression> lve2;
        if(dynamic_cast<Constant*>(lop2)== nullptr)
        {
            lve2= valueExpr(dynamic_cast<Instruction*>(lop2),P);
        }
        else
        {
            lve2=ConstantExpression::create(dynamic_cast<Constant*>(lop2));
        }
        shared_ptr<Expression> lexpr;
        if((lve1->get_expr_type()!=Expression::e_constant)||(lve2->get_expr_type()!=Expression::e_constant)) {
            lexpr = BinaryExpression::create(ve_bin->get_op(), lve1, lve2);
        }
        else
        {
            lexpr = ConstantExpression::create(folder_->compute(ve_bin->get_op(),dynamic_cast<Constant*>(lop1),dynamic_cast<Constant*>(lop2)));
        }
        auto linstr= dynamic_cast<Instruction*>(getVN(P,ve_bin->get_lhs()));
        auto lbb=dynamic_cast<BasicBlock*>(linstr->get_operand(1));
        auto vi=getVN(pout_[lbb],lexpr);
        if(vi== nullptr)
        {
            auto vi_expr= valuePhiFunc(lexpr,pout_[lbb]);
            vi=getVN(pout_[lbb],vi_expr);
        }
        auto rop1=std::dynamic_pointer_cast<PhiExpression>(ve_bin->get_lhs())->get_rhs_();
        shared_ptr<Expression> rve1;
        if(dynamic_cast<Constant*>(rop1)== nullptr)
        {
            rve1= valueExpr(dynamic_cast<Instruction*>(rop1),P);
        }
        else
        {
            rve1=ConstantExpression::create(dynamic_cast<Constant*>(rop1));
        }
        auto rop2=std::dynamic_pointer_cast<PhiExpression>(ve_bin->get_rhs())->get_rhs_();
        shared_ptr<Expression> rve2;
        if(dynamic_cast<Constant*>(rop2)== nullptr)
        {
            rve2= valueExpr(dynamic_cast<Instruction*>(rop2),P);
        }
        else
        {
            rve2=ConstantExpression::create(dynamic_cast<Constant*>(rop2));
        }
       shared_ptr<Expression> rexpr;
        if((rve1->get_expr_type()!=Expression::e_constant)||(rve2->get_expr_type()!=Expression::e_constant)) {
            rexpr = BinaryExpression::create(ve_bin->get_op(), rve1, rve2);
        }
        else
        {
            rexpr = ConstantExpression::create(folder_->compute(ve_bin->get_op(),dynamic_cast<Constant*>(rop1),dynamic_cast<Constant*>(rop2)));
        }
        auto rinstr= dynamic_cast<Instruction*>(getVN(P,ve_bin->get_rhs()));
        auto rbb=dynamic_cast<BasicBlock*>(rinstr->get_operand(3));
        auto vj=getVN(pout_[rbb],rexpr);
        if(vj== nullptr)
        {
            auto vj_expr= valuePhiFunc(rexpr,pout_[rbb]);
            vj=getVN(pout_[rbb],vj_expr);
        }
        if((vi!= nullptr)&&(vj!= nullptr))
        {
            return PhiExpression::create(vi,vj);
        }
        else
        {
            return nullptr;
        }
    }
    else{
        return nullptr;
    }
}

Value* GVN::getVN(const partitions &pout, shared_ptr<Expression> ve) {
    // TODO: return what?
    if(ve== nullptr)
    {
        return nullptr;
    }
    for (auto it = pout.begin(); it != pout.end(); it++)
        if ((*it)->value_expr_ and *(*it)->value_expr_ == *ve)
            return (*it)->leader_;
    return nullptr;
}

void GVN::initPerFunction() {
    next_value_number_ = 1;
    pin_.clear();
    pout_.clear();
}

void GVN::replace_cc_members() {
    for (auto &[_bb, part] : pout_) {
        auto bb = _bb; // workaround: structured bindings can't be captured in C++17
        for (auto &cc : part) {
            if (cc->index_ == 0)
                continue;
            // if you are planning to do constant propagation, leaders should be set to constant at some point
            for (auto &member : cc->members_) {
                bool member_is_phi = dynamic_cast<PhiInst *>(member);
                bool value_phi = cc->value_phi_ != nullptr;
                if (member != cc->leader_ and (value_phi or !member_is_phi)) {
                    // only replace the members if users are in the same block as bb
                    member->replace_use_with_when(cc->leader_, [bb](User *user) {
                        if (auto instr = dynamic_cast<Instruction *>(user)) {
                            auto parent = instr->get_parent();
                            auto &bb_pre = parent->get_pre_basic_blocks();
                            if (instr->is_phi()) // as copy stmt, the phi belongs to this block
                                return std::find(bb_pre.begin(), bb_pre.end(), bb) != bb_pre.end();
                            else
                                return parent == bb;
                        }
                        return false;
                    });
                }
            }
        }
    }
    return;
}

// top-level function, done for you
void GVN::run() {
    std::ofstream gvn_json;
    if (dump_json_) {
//        gvn_json.open("/home/ly/2022fall-compiler_cminus/cmake-build-debug/gvn.json", std::ios::out);
    gvn_json.open("gvn.json", std::ios::out);
        gvn_json << "[";
    }
    m_->set_print_name();
    folder_ = std::make_unique<ConstFolder>(m_);
    func_info_ = std::make_unique<FuncInfo>(m_);
    func_info_->run();
    dce_ = std::make_unique<DeadCode>(m_);
    dce_->run(); // let dce take care of some dead phis with undef

    for (auto &f : m_->get_functions()) {
        if (f.get_basic_blocks().empty())
            continue;
        func_ = &f;
        initPerFunction();
        LOG_INFO << "Processing " << f.get_name();
        detectEquivalences();
        LOG_INFO << "===============pin=========================\n";
        for (auto &[bb, part] : pin_) {
            LOG_INFO << "\n===============bb: " << bb->get_name() << "=========================\npartitionIn: ";
            for (auto &cc : part)
                LOG_INFO << utils::print_congruence_class(*cc);
        }
        LOG_INFO << "\n===============pout=========================\n";
        for (auto &[bb, part] : pout_) {
            LOG_INFO << "\n=====bb: " << bb->get_name() << "=====\npartitionOut: ";
            for (auto &cc : part)
                LOG_INFO << utils::print_congruence_class(*cc);
        }

        if (dump_json_) {
            gvn_json << "{\n\"function\": ";
            gvn_json << "\"" << f.get_name() << "\", ";
            gvn_json << "\n\"pout\": " << utils::dump_bb2partition(pout_);
            gvn_json << "},";
        }

        replace_cc_members(); // don't delete instructions, just replace them
    }
    dce_->run(); // let dce do that for us
    if (dump_json_)
        gvn_json << "]";
}

template <typename T>
static bool equiv_as(const Expression &lhs, const Expression &rhs) {
    // we use static_cast because we are very sure that both operands are actually T, not other types.
    return static_cast<const T *>(&lhs)->equiv(static_cast<const T *>(&rhs));
}

bool GVNExpression::operator==(const Expression &lhs, const Expression &rhs) {
    if (lhs.get_expr_type() != rhs.get_expr_type())
        return false;
    switch (lhs.get_expr_type()) {
    case Expression::e_constant: return equiv_as<ConstantExpression>(lhs, rhs);
    case Expression::e_bin: return equiv_as<BinaryExpression>(lhs, rhs);
    case Expression::e_phi: return equiv_as<PhiExpression>(lhs, rhs);
    case Expression::e_single: return equiv_as<SingleExpression>(lhs,rhs);
    case Expression::e_func: return equiv_as<FuncExpression>(lhs,rhs);
    case Expression::e_cmp: return equiv_as<CmpExpression>(lhs,rhs);
    case Expression::e_fcmp: return equiv_as<FCmpExpression>(lhs,rhs);
    case Expression::e_trans: return equiv_as<TransExpression>(lhs,rhs);
    case Expression::e_gep: return equiv_as<GepExpression>(lhs,rhs);
    }
}

bool GVNExpression::operator==(const shared_ptr<Expression> &lhs, const shared_ptr<Expression> &rhs) {
    if (lhs == nullptr and rhs == nullptr) // is the nullptr check necessary here?
        return true;
    return lhs and rhs and *lhs == *rhs;
}

GVN::partitions GVN::clone(const partitions &p) {
    partitions data;
    for (auto &cc : p) {
        data.insert(std::make_shared<CongruenceClass>(*cc));
    }
    return data;
}

bool operator==(const GVN::partitions &p1, const GVN::partitions &p2) {
    // TODO: how to compare partitions?
    if(p1.size()!=p2.size())
    {
        return false;
    }
    bool judge=true;
    for(auto i:p1)
    {
        judge= false;
        for(auto j:p2)
        {
            if(*i==*j)
            {
                judge= true;
                break;
            }
        }
        return judge;
    }
    return judge;
}

bool CongruenceClass::operator==(const CongruenceClass &other) const {
    // TODO: which fields need to be compared?
    return std::tie(this->members_)==std::tie(other.members_);

//    return false;
}
