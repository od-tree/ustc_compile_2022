# Lab4.1 实验报告

## 实验要求

阅读SSA IR的材料以及助教实现的Mem2reg pass,并理解其如何实现对冗余指令的删除.

## 思考题
### Mem2reg
1. + 支配性:在入口节点为$b_0$的流图中,当且仅当$b_i$位于$b_0$到$b_j$的每条路径上时,$b_i$支配$b_j$.
   + 严格支配性:节点的支配中去除其自身的节点,对其严格支配.
   + 直接支配性:节点m的严格支配集$DOM\{m\}-\{m\}$中与m最接近的节点,对其具有直接支配性.
   + 支配边界:具有如下性质的节点n的集合是m的支配边界:
    (1):n支配m的一个前驱
    (2):n不严格支配m
2. phi节点是用来插入在具有多个前驱的程序节点起始处,对当前程序节点中定义或使用的名字,插入一个phi函数,其对支配边界中每个元素,phi有一个参数与之对应,用来确定在当前节点前执行的是哪一个前驱节点来取得相应的值.
   通过插入phi节点,可以确定前驱节点来获得值,从而可以将store/load指令精简,从而实现了代码优化.
3. 
   + 对函数参数x的使用变化了:
   ```c
   if(x > 0)
   ```
   优化前,优化后
   ```ir
    %op1 = alloca i32
    store i32 %arg0, i32* %op1
    %op2 = load i32, i32* %op1
    %op3 = icmp sgt i32 %op2, 0
   ```
   优化后
   ```ir
   %op3 = icmp sgt i32 %arg0, 0
   ```
   不再重新创建变量,而是直接使用参数
 +  返回值是,插入phi决定x的值,并将第二个值换成常数0,随后可以删除赋值语句
 +  main函数中,进行函数调用时,直接使用常量2333,随后可以删除对应的变量创建,store,load指令
 +  对全局变量和数组的store,load指令没有改变
4. 放置phi节点使用如下代码
   ```c++
   void Mem2Reg::generate_phi() 
   ```
   通过```var : global_live_var_name```找到全局名字,并为其初始化worklist```work_list.assign(live_var_2blocks[var].begin(), live_var_2blocks[var].end());```,并对worklist中一个基本块,使用```dominators_->get_dominance_frontier(bb)```通过支配者树求得支配边界.
   通过支配边界便可以其中每个元素加入phi并将该块加入worklist,重复此过程至对worklist内每个基本块完成该操作
5. 选择value(变量最新的值)来替换load指令使用```void Mem2Reg::re_name(BasicBlock *bb)```函数
   代码先用phi作为lvalue的最新定值压入栈内,再扫描指令用lvalue的最新定值替换load指令,并将store指令作为lvalue的最新定值,随后补充完整phi指令并对所有后继块递归进行替换,随后将lvaue最新定值出栈并清除冗余指令
### 代码阅读总结

通过本次阅读实验,对课堂上提及过的SSA有了更多理解,也意识到了许多之前没想到的注意点,比如phi节点需要并发执行.

### 实验反馈 （可选 不会评分）

对本次实验的建议
