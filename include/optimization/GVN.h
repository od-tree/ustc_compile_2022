#pragma once
#include "BasicBlock.h"
#include "Constant.h"
#include "DeadCode.h"
#include "FuncInfo.h"
#include "Function.h"
#include "Instruction.h"
#include "Module.h"
#include "PassManager.hpp"
#include "Value.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace GVNExpression {

// fold the constant value
class ConstFolder {
  public:
    ConstFolder(Module *m) : module_(m) {}
    Constant *compute(Instruction *instr, Constant *value1, Constant *value2);
    Constant *compute(Instruction *instr, Constant *value1);

    Constant *compute(Instruction::OpID op, Constant *value1, Constant *value2);

  private:
    Module *module_;
};

/**
 * for constructor of class derived from `Expression`, we make it public
 * because `std::make_shared` needs the constructor to be publicly available,
 * but you should call the static factory method `create` instead the constructor itself to get the desired data
 */
class Expression {
  public:
    // TODO: you need to extend expression types according to testcases
    enum gvn_expr_t { e_constant, e_bin, e_phi ,e_single,e_func,e_cmp,e_fcmp};
    Expression(gvn_expr_t t) : expr_type(t) {}
    virtual ~Expression() = default;
    virtual std::string print() = 0;
    gvn_expr_t get_expr_type() const { return expr_type; }

  private:
    gvn_expr_t expr_type;
};

bool operator==(const std::shared_ptr<Expression> &lhs, const std::shared_ptr<Expression> &rhs);
bool operator==(const GVNExpression::Expression &lhs, const GVNExpression::Expression &rhs);

class ConstantExpression : public Expression {
  public:
    static std::shared_ptr<ConstantExpression> create(Constant *c) { return std::make_shared<ConstantExpression>(c); }
    virtual std::string print() { return c_->print(); }
    // we leverage the fact that constants in lightIR have unique addresses
    bool equiv(const ConstantExpression *other) const { return c_ == other->c_; }
    ConstantExpression(Constant *c) : Expression(e_constant), c_(c) {}
    Constant* get_cons() const { return c_; }
  private:
    Constant *c_;
};

// arithmetic expression
class FCmpExpression : public Expression {
  public:
    static std::shared_ptr<FCmpExpression> create(FCmpInst::CmpOp op,
                                                 std::shared_ptr<Expression> lhs,
                                                 std::shared_ptr<Expression> rhs) {
        return std::make_shared<FCmpExpression>(op, lhs, rhs);
    }
    virtual std::string print() {
        return "(fcmp " + lhs_->print() + " " + rhs_->print() + ")";
    }

    bool equiv(const FCmpExpression *other) const {
        if (fcmp_op_ == other->fcmp_op_ and *lhs_ == *other->lhs_ and *rhs_ == *other->rhs_)
            return true;
        else
            return false;
    }
    gvn_expr_t get_lhs_type(){return lhs_->get_expr_type();}
    gvn_expr_t get_rhs_type(){return rhs_->get_expr_type();}
    FCmpInst::CmpOp get_op(){return fcmp_op_;}
    std::shared_ptr<Expression> get_lhs(){return lhs_;}
    std::shared_ptr<Expression> get_rhs(){return rhs_;}
    FCmpExpression(FCmpInst::CmpOp op, std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
        : Expression(e_fcmp), fcmp_op_(op), lhs_(lhs), rhs_(rhs) {}

  private:
    FCmpInst::CmpOp fcmp_op_;
    std::shared_ptr<Expression> lhs_, rhs_;
};
class CmpExpression : public Expression {
  public:
    static std::shared_ptr<CmpExpression> create(CmpInst::CmpOp op,
                                                    std::shared_ptr<Expression> lhs,
                                                    std::shared_ptr<Expression> rhs) {
        return std::make_shared<CmpExpression>(op, lhs, rhs);
    }
    virtual std::string print() {
        return "(cmp " + lhs_->print() + " " + rhs_->print() + ")";
    }

    bool equiv(const CmpExpression *other) const {
        if (cmp_op_ == other->cmp_op_ and *lhs_ == *other->lhs_ and *rhs_ == *other->rhs_)
            return true;
        else
            return false;
    }
    gvn_expr_t get_lhs_type(){return lhs_->get_expr_type();}
    gvn_expr_t get_rhs_type(){return rhs_->get_expr_type();}
    CmpInst::CmpOp get_op(){return cmp_op_;}
    std::shared_ptr<Expression> get_lhs(){return lhs_;}
    std::shared_ptr<Expression> get_rhs(){return rhs_;}
    CmpExpression(CmpInst::CmpOp op, std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
        : Expression(e_cmp), cmp_op_(op), lhs_(lhs), rhs_(rhs) {}

  private:
    CmpInst::CmpOp cmp_op_;
    std::shared_ptr<Expression> lhs_, rhs_;
};
class BinaryExpression : public Expression {
  public:
    static std::shared_ptr<BinaryExpression> create(Instruction::OpID op,
                                                    std::shared_ptr<Expression> lhs,
                                                    std::shared_ptr<Expression> rhs) {
        return std::make_shared<BinaryExpression>(op, lhs, rhs);
    }
    virtual std::string print() {
        return "(" + Instruction::get_instr_op_name(op_) + " " + lhs_->print() + " " + rhs_->print() + ")";
    }

    bool equiv(const BinaryExpression *other) const {
        if (op_ == other->op_ and *lhs_ == *other->lhs_ and *rhs_ == *other->rhs_)
            return true;
        else
            return false;
    }
    gvn_expr_t get_lhs_type(){return lhs_->get_expr_type();}
    gvn_expr_t get_rhs_type(){return rhs_->get_expr_type();}
    Instruction::OpID get_op(){return op_;}
    std::shared_ptr<Expression> get_lhs(){return lhs_;}
    std::shared_ptr<Expression> get_rhs(){return rhs_;}
    BinaryExpression(Instruction::OpID op, std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
        : Expression(e_bin), op_(op), lhs_(lhs), rhs_(rhs) {}

  private:
    Instruction::OpID op_;
    std::shared_ptr<Expression> lhs_, rhs_;
};

class PhiExpression : public Expression {
  public:
    static std::shared_ptr<PhiExpression> create(Value* lhs, Value* rhs) {
        return std::make_shared<PhiExpression>(lhs, rhs);
    }
    virtual std::string print() { return "(phi " + lhs_->print() + " " + rhs_->print() + ")"; }
    bool equiv(const PhiExpression *other) const {
        if (lhs_ == other->lhs_ and rhs_ == other->rhs_)
            return true;
        else
            return false;
    }
    PhiExpression(Value* lhs, Value* rhs)
        : Expression(e_phi), lhs_(lhs), rhs_(rhs) {}
    Value* get_lhs_(){return lhs_;}
    Value* get_rhs_(){return rhs_;}
  private:

    Value *lhs_, *rhs_;
};
class SingleExpression : public Expression {
  public:
    static std::shared_ptr<SingleExpression> create(Value* a) {
        return std::make_shared<SingleExpression>(a);
    }
    virtual std::string print() {
        return var->print();
    }
    bool equiv(const SingleExpression *other) const {
        return var==other->var;
    }
    SingleExpression(Value *a)
        : Expression(e_single),var(a){}

  private:
    Value* var;
};
class FuncExpression : public Expression {
  public:
    static std::shared_ptr<FuncExpression> create(Value* f,std::vector<std::shared_ptr<Expression>>a,bool isPure,Instruction* xx) {
        return std::make_shared<FuncExpression>(f,a,isPure,xx);
    }
    virtual std::string print() {
//        return var->print();
        return "";
    }

    FuncExpression(Value* f,std::vector<std::shared_ptr<Expression>> a,bool isPure,Instruction *xx)
        : Expression(e_func),pure(isPure),func(f),operands(std::move(a)),x(xx){}

    bool equiv(const FuncExpression *other) const {
        if(x==other->x)
        {
            return true;
        }
        if (!pure || !other->pure)
            return false;
       if(func!=other->func)
           return false;
       if(operands.size()!=other->operands.size())
           return false;
       for(int i=0;i<operands.size();i++)
       {
           if(operands[i]!=other->operands[i])
           {
               return false;
           }
       }
       return true;
    }
  private:
//    std::vector<Value*> operands;
    bool pure;
    Value* func;
    std::vector<std::shared_ptr<Expression>> operands;
    Instruction* x;
};
} // namespace GVNExpression


/**
 * Congruence class in each partitions
 * note: for constant propagation, you might need to add other fields
 * and for load/store redundancy detection, you most certainly need to modify the class
 */
struct CongruenceClass {
    size_t index_;
    // representative of the congruence class, used to replace all the members (except itself) when analysis is done
    Value *leader_;
    // value expression in congruence class
    std::shared_ptr<GVNExpression::Expression> value_expr_;
    // value φ-function is an annotation of the congruence class
    std::shared_ptr<GVNExpression::PhiExpression> value_phi_;
    // equivalent variables in one congruence class
    std::shared_ptr<GVNExpression::ConstantExpression> value_const_;
    std::shared_ptr<GVNExpression::BinaryExpression> value_bin;
    std::shared_ptr<GVNExpression::SingleExpression> value_single;
    std::shared_ptr<GVNExpression::FuncExpression> value_func;
    std::shared_ptr<GVNExpression::CmpExpression> value_cmp;
    std::shared_ptr<GVNExpression::FCmpExpression> value_fcmp;
    std::set<Value *> members_;

    CongruenceClass(size_t index) : index_(index), leader_{}, value_expr_{}, value_phi_{},value_const_{}, value_bin{},value_single{},value_func{},members_{} {}

    bool operator<(const CongruenceClass &other) const { return this->index_ < other.index_; }
    bool operator==(const CongruenceClass &other) const;
};

namespace std {
template <>
// overload std::less for std::shared_ptr<CongruenceClass>, i.e. how to sort the congruence classes
struct less<std::shared_ptr<CongruenceClass>> {
    bool operator()(const std::shared_ptr<CongruenceClass> &a, const std::shared_ptr<CongruenceClass> &b) const {
        // nullptrs should never appear in partitions, so we just dereference it
        return *a < *b;
    }
};
} // namespace std

class GVN : public Pass {
  public:
    using partitions = std::set<std::shared_ptr<CongruenceClass>>;
    GVN(Module *m, bool dump_json) : Pass(m), dump_json_(dump_json) {}
    // pass start
    void run() override;
    // init for pass metadata;
    void initPerFunction();

    // fill the following functions according to Pseudocode, **you might need to add more arguments**
    void detectEquivalences();
    partitions join(const partitions &P1, const partitions &P2);
    std::shared_ptr<CongruenceClass> intersect(std::shared_ptr<CongruenceClass>, std::shared_ptr<CongruenceClass>);
    partitions transferFunction(Instruction *x,Value *e, partitions pin);
    std::shared_ptr<GVNExpression::PhiExpression> valuePhiFunc(std::shared_ptr<GVNExpression::Expression>,
                                                               const partitions &);
    std::shared_ptr<GVNExpression::Expression> valueExpr(Instruction *instr,partitions pin);
    Value* getVN(const partitions &pout,std::shared_ptr<GVNExpression::Expression> ve);

    // replace cc members with leader
    void replace_cc_members();

    // note: be careful when to use copy constructor or clone
    partitions clone(const partitions &p);

    // create congruence class helper
    std::shared_ptr<CongruenceClass> createCongruenceClass(size_t index = 0) {
        return std::make_shared<CongruenceClass>(index);
    }

  private:
    bool dump_json_;
    std::uint64_t next_value_number_ = 1;
    Function *func_;
    std::map<BasicBlock *, partitions> pin_, pout_;
    std::unique_ptr<FuncInfo> func_info_;
    std::unique_ptr<GVNExpression::ConstFolder> folder_;
    std::unique_ptr<DeadCode> dce_;
    std::shared_ptr<GVNExpression::Expression> binValueExpr(Instruction *instr, partitions &pin);
    void globel_and_argv(partitions &p);
    std::shared_ptr<GVNExpression::Expression> funcValueExpr(Instruction *instr, partitions &pin) const;
    std::shared_ptr<GVNExpression::Expression> cmpValueExpr(Instruction *instr, partitions &pin) const;
    std::shared_ptr<GVNExpression::Expression> fcmpValueExpr(Instruction *instr, partitions &pin);
};

bool operator==(const GVN::partitions &p1, const GVN::partitions &p2);
