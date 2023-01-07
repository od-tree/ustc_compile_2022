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
    return {};
}

std::shared_ptr<CongruenceClass> GVN::intersect(std::shared_ptr<CongruenceClass> Ci,
                                                std::shared_ptr<CongruenceClass> Cj) {
    // TODO
    return {};
}

void GVN::detectEquivalences() {
    bool changed{};
    // initialize pout with top
    // iterate until converge
    BasicBlock *entry=func_->get_entry_block();
    pin_[entry]={};
//    pout_[entry]= transferFunction()
    for(auto &bb:func_->get_basic_blocks())
    {
//        pout_[&bb]=
    }
    do {
        changed= false;
        // see the pseudo code in documentation
        for (auto &bb : func_->get_basic_blocks()) { // you might need to visit the blocks in depth-first order
            // get PIN of bb by predecessor(s)
            auto p= clone(pin_[&bb]);
            // iterate through all instructions in the block
            // and the phi instruction in all the successors
            for(auto &instr:bb.get_instructions())
            {
                p= transferFunction(&instr,&instr,p);
            }
            if(p!=pout_[&bb])
            {
                changed=true;
            }
            // check changes in pout
            pout_[&bb]=std::move(p);
        }
    } while (changed);
}

shared_ptr<Expression> GVN::valueExpr(Instruction *instr,partitions pin) {
    // TODO
    if(instr->isBinary())
    {
        auto oprands=instr->get_operands();
        auto lconst=dynamic_cast<Constant*>(oprands[0]);
        auto rconst=dynamic_cast<Constant*>(oprands[1]);
        if(lconst!= nullptr)
        {
            if(rconst!= nullptr)
            {
                auto cons=folder_->compute(instr,lconst,rconst);
                auto consExp=ConstantExpression::create(cons);
                return consExp;
            }
        }

        shared_ptr<Expression> lop= nullptr;
        shared_ptr<Expression> rop= nullptr;
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
            auto cons=folder_->compute(instr,lconst, std::dynamic_pointer_cast<ConstantExpression>(rop)->get_cons());
            auto consExp=ConstantExpression::create(cons);
            return consExp;
        }
        if((lop!= nullptr)&&(lop->get_expr_type()==Expression::e_constant)&&(rconst!= nullptr))
        {
            auto cons=folder_->compute(instr,std::dynamic_pointer_cast<ConstantExpression>(lop)->get_cons(),rconst);
            auto consExp=ConstantExpression::create(cons);
            return consExp;
        }
        if((lop!= nullptr)&&(lop->get_expr_type()==Expression::e_constant)&&(rop!= nullptr)&&(rop->get_expr_type()==Expression::e_constant))
        {
            auto cons=folder_->compute(instr,std::dynamic_pointer_cast<ConstantExpression>(lop)->get_cons(),std::dynamic_pointer_cast<ConstantExpression>(rop)->get_cons());
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
    if(instr->is_call())
    {
        return FuncExpression::create(instr->get_operands(),func_info_->is_pure_function(dynamic_cast<Function *>(instr->get_operand(0))),instr);
    }
    return {};
}

// instruction of the form `x = e`, mostly x is just e (SSA), but for copy stmt x is a phi instruction in the
// successor. Phi values (not copy stmt) should be handled in detectEquiv
/// \param bb basic block in which the transfer function is called
GVN::partitions GVN::transferFunction(Instruction *x,Value  *e, partitions pin) {
    partitions pout = clone(pin);
    if(x->is_void())
    {
        return pout;
    }
    // TODO: get different ValueExpr by Instruction::OpID, modify pout
    for(auto &i:pout)
    {
       if( i->members_.find(x)!=i->members_.end());
       {
           i->members_.erase(x);
       }
    }
    auto ve= valueExpr(x,pin);
    auto vpf= valuePhiFunc(ve,pin);
    bool judge= false;


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
                case Expression::e_phi:
                {
                    auto ve_phi=std::dynamic_pointer_cast<PhiExpression>(ve);

                    break;
                }
                case Expression::e_single: break;
                case Expression::e_func: {
                    auto ve_func=std::dynamic_pointer_cast<FuncExpression>(ve);
                    if((i->value_func!= nullptr)&&(i->value_func->equiv(ve_func.get())))
                    {
                        judge =true;
                        i->members_.insert(x);
                    }
                    break;
                }
            }
        }
    }

    if(ve!= nullptr) {
        if (!judge) {
            auto cc = createCongruenceClass(next_value_number_++);

            cc->members_ = {x};
            cc->value_expr_ = ve;
//            if(x->isBinary())
//            {
//                auto operands=x->get_operands();
//
//            }
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

                case Expression::e_single: break;
                case Expression::e_func:{
                    auto ve_func=std::dynamic_pointer_cast<FuncExpression>(ve);
                    cc->leader_=x;
                    cc->value_func=ve_func;
                    break;
                }
            }
            pout.insert(cc);
        }
    }
    if(ve== nullptr)
    {
        auto cc = createCongruenceClass(next_value_number_++);
        cc->leader_ = x;
        cc->members_ = {x};
        cc->value_expr_ = valueExpr(dynamic_cast<Instruction*>(e),pin);
//        cc->value_single
        pout.insert(cc);
    }
    return pout;
}

shared_ptr<PhiExpression> GVN::valuePhiFunc(shared_ptr<Expression> ve, const partitions &P) {
    // TODO
    return {};
}

shared_ptr<Expression> GVN::getVN(const partitions &pout, shared_ptr<Expression> ve) {
    // TODO: return what?
    for (auto it = pout.begin(); it != pout.end(); it++)
        if ((*it)->value_expr_ and *(*it)->value_expr_ == *ve)
            return {};
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
    for(auto iter1=p1.begin(),iter2=p2.begin();iter1!=p1.end();iter1++,iter2++)
    {
        if(!((**iter1)==(**iter2)))
        {
            return false;
        }
    }
    return true;
}

bool CongruenceClass::operator==(const CongruenceClass &other) const {
    // TODO: which fields need to be compared?
    return this->members_==other.members_;

//    return false;
}
