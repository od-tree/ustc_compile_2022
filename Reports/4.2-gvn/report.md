# Lab4.2 实验报告

姓名 刘阳 学号 PB20000114

## 实验要求

本次实验中,我们会得到cminusfc已经被编译完成的,ssa的格式的在内存中的ir,我们需要实现常量折叠,并利用GVN来发现与构造等价类,以使得后续算法可以将等价类中内容替换为等价类的leader.并将冗余内容删除.

## 实验难点

+ 开头对框架的理解比较困难,要结合助教提供的接口和伪代码的含义
+ 对起始块和有前趋没有被访问之类特殊块的处理,entry块通过前趋数特判处理,前趋没被访问的块对用到的Instruct生成一个指代Instruct的expr,在后续迭代中逐渐修正,对两个前趋都没被访问的块不能保持Top,否则可能发生段错误.
+ phi指令应当在两个前趋块中处理,并在join操作中生成
+ 对各种类型\==的理解和设计.
+ join操作时的两个块的等价类可能不是同一次迭代时生成的,因此index可能不同,不能基于index判断是合并等价类还是生成phi等价类,应该用leader判断
+ 处理phi可能会导致出现空等价类,要注意删除
+ 常量传播,不但要考虑操作数是常量,还要考虑操作数在常量等价类
+ PhiExpress一定要设计成递归的形式,否则可能无法发现phi[phi,phi]+phi[phi,phi]的冗余

## 实验设计
对除phi以外的指令,在valueExpr中处理,先判断操作数是不是常数,再在已有等价类中找出操作数所在等价类的值表达式,然后判断能否常量折叠,生成该语句的值表达式,transfer得到后判断是添加到已有等价类还是新建等价类
例如对类型转换的处理
```c++
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
```
对phi的处理主要在detect函数中提前处理,通过join操作生成,并使用valuPhiExpr检测phi+phi的冗余
```c++
for(auto &nextBB:bb.get_succ_basic_blocks())
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
        {...
        }
        if(judge)
        {
            //添加到已有等价类
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
```
处理如下代码
```c
int main(void)
{
    int a;
    int b;
    int c;
    int d;
    int e;
    a = b = c = 0;
    d = input();
    e = input();
    if (a > b) {
        if (c > b) {
            a = d + e;
            b = d * e;
            c = a + b;
        } else {
            a = 1 + 9;
            b = 2 + 6;
            c = a + b;
        }
    } else {
        if (c > b) {
            a = e - d;
            b = e * 10;
            c = a + b;
        } else {
            a = 10 + 9;
            b = 10 + 6;
            c = a + b;
        }
    }
    output(c);
    output(a + b);
}
```
优化前
```llvm
declare i32 @input()

declare void @output(i32)

declare void @outputFloat(float)

declare void @neg_idx_except()

define i32 @main() {
label_entry:
  %op0 = call i32 @input()
  %op1 = call i32 @input()
  %op2 = icmp sgt i32 0, 0
  %op3 = zext i1 %op2 to i32
  %op4 = icmp ne i32 %op3, 0
  br i1 %op4, label %label5, label %label14
label5:                                                ; preds = %label_entry
  %op6 = icmp sgt i32 0, 0
  %op7 = zext i1 %op6 to i32
  %op8 = icmp ne i32 %op7, 0
  br i1 %op8, label %label18, label %label26
label9:                                                ; preds = %label22, %label34
  %op10 = phi i32 [ %op23, %label22 ], [ %op35, %label34 ]
  %op11 = phi i32 [ %op24, %label22 ], [ %op36, %label34 ]
  %op12 = phi i32 [ %op25, %label22 ], [ %op37, %label34 ]
  call void @output(i32 %op10)
  %op13 = add i32 %op12, %op11
  call void @output(i32 %op13)
  ret i32 0
label14:                                                ; preds = %label_entry
  %op15 = icmp sgt i32 0, 0
  %op16 = zext i1 %op15 to i32
  %op17 = icmp ne i32 %op16, 0
  br i1 %op17, label %label30, label %label38
label18:                                                ; preds = %label5
  %op19 = add i32 %op0, %op1
  %op20 = mul i32 %op0, %op1
  %op21 = add i32 %op19, %op20
  br label %label22
label22:                                                ; preds = %label18, %label26
  %op23 = phi i32 [ %op21, %label18 ], [ %op29, %label26 ]
  %op24 = phi i32 [ %op20, %label18 ], [ %op28, %label26 ]
  %op25 = phi i32 [ %op19, %label18 ], [ %op27, %label26 ]
  br label %label9
label26:                                                ; preds = %label5
  %op27 = add i32 1, 9
  %op28 = add i32 2, 6
  %op29 = add i32 %op27, %op28
  br label %label22
label30:                                                ; preds = %label14
  %op31 = sub i32 %op1, %op0
  %op32 = mul i32 %op1, 10
  %op33 = add i32 %op31, %op32
  br label %label34
label34:                                                ; preds = %label30, %label38
  %op35 = phi i32 [ %op33, %label30 ], [ %op41, %label38 ]
  %op36 = phi i32 [ %op32, %label30 ], [ %op40, %label38 ]
  %op37 = phi i32 [ %op31, %label30 ], [ %op39, %label38 ]
  br label %label9
label38:                                                ; preds = %label14
  %op39 = add i32 10, 9
  %op40 = add i32 10, 6
  %op41 = add i32 %op39, %op40
  br label %label34
}
```
优化后
```llvm
declare i32 @input()

declare void @output(i32)

declare void @outputFloat(float)

declare void @neg_idx_except()

define i32 @main() {
label_entry:
  %op0 = call i32 @input()
  %op1 = call i32 @input()
  br i1 false, label %label5, label %label14
label5:                                                ; preds = %label_entry
  br i1 false, label %label18, label %label26
label9:                                                ; preds = %label22, %label34
  %op10 = phi i32 [ %op23, %label22 ], [ %op35, %label34 ]
  call void @output(i32 %op10)
  call void @output(i32 %op10)
  ret i32 0
label14:                                                ; preds = %label_entry
  br i1 false, label %label30, label %label38
label18:                                                ; preds = %label5
  %op19 = add i32 %op0, %op1
  %op20 = mul i32 %op0, %op1
  %op21 = add i32 %op19, %op20
  br label %label22
label22:                                                ; preds = %label18, %label26
  %op23 = phi i32 [ %op21, %label18 ], [ 18, %label26 ]
  br label %label9
label26:                                                ; preds = %label5
  br label %label22
label30:                                                ; preds = %label14
  %op31 = sub i32 %op1, %op0
  %op32 = mul i32 %op1, 10
  %op33 = add i32 %op31, %op32
  br label %label34
label34:                                                ; preds = %label30, %label38
  %op35 = phi i32 [ %op33, %label30 ], [ 35, %label38 ]
  br label %label9
label38:                                                ; preds = %label14
  br label %label34
}
```
常量运算被进行了折叠和传播
label%op13的phi的运算被通过递归判断得出和

### 思考题
1. 请简要分析你的算法复杂度
   假设有n个块,那么最多迭代O(n)次,每次迭代需要处理每条指令,假设v个指令,每个指令主要去等价类中查找Q(Cv)次,因此时间复杂度应该是$O(nv^2)$
2. `std::shared_ptr`如果存在环形引用，则无法正确释放内存，你的 Expression 类是否存在 circular reference?
   不存在,每个shared_ptr最终都指向SingleExpr或者Constexpr或无参数的FuncExpr(input),因此不存在环形引用(应该)
3. 尽管本次实验已经写了很多代码，但是在算法上和工程上仍然可以对 GVN 进行改进，请简述你的 GVN 实现可以改进的地方
   + 我的GVN算法在一个块的前趋未被访问时访问可能会导致段错误,考虑在没有前趋被访问时跳过该块或者改变遍历顺序或许是可行的方法
   + 算法中大量基于枚举的查找可以优化

## 实验总结

加深了对C++已经编译优化的理解

## 实验反馈（可选 不会评分）

对辅助类的说明能增加一些,经常出现找不到函数之类的情况

