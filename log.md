斗地主策略：贪心

version 1

2018/5/13

地主

1. 按照分数从大到小出牌，炸弹、航天飞机、火箭除外；
2. 接牌：出刚好可以接的牌；
3. 能接就接，需要拆牌的时候就拆牌。

农民

1. 不打队友；
2. 足够大的时候出最大的牌压制地主，其他情况跟牌；
3. 出牌：尽量出比较小的复杂的(权值大的)牌，和牌数比地主大的牌(也就是尽量减少手牌数)；
4. 只有单一牌型的时候，从大打到小，不给地主机会。

version 1.0

2018/5/15

1. 将 myPosition 定义放到了开始。
2. 在 Line 24 增加vector<vector<Card> > allCombos[20]用来计算当前手牌的所有可能组合。 
3. 在 Line 21 定义了变量 lastPlayer，表示上次出牌的玩家，主要用于农民不相互攻击。这个变量将在 input()函数里完成赋值。
4. 在 Line 27 声明了函数 SearchCard() 用来寻找主牌的从牌。其具体实现在 Line 928 处定义。
5. 在 Line 392 定义了估值函数 EvaluateCard()，对当前手牌好坏程度及获胜概率进行评价(待实现)。 
6. 在 Line 403 定义函数 findAllCombos() 用来找出当前手牌的所有可能组合，并把结果放在 vector<CardCombo> allCombos[20] 中。
7. 在 Line 716 定义函数 myGivenCombo() 用来出牌，分为农民和地主两个角色的出牌策略。一开始调用了 findAllCombos()函数。
8. 在 Line 740 处修改了出牌策略，调用 myGivenCombo()函数。
9. 在 Line 746 处的修改表示农民不打农民。
10. 在 Line 797 定义 myCombos，用来存储所有可行的接牌方案。
11. 在 Line 853 处根据不同角色出不同的牌。
12. 从 Line 880 开始修改了使用炸弹的规则。

version 1.1

2018/5/19

本次更新基本实现了ver1的所有的架构，是ver1的完全版。其主要改动如下：

1. 将 cardRemaining[] 数组放到了 Line 21 处，便于在出牌及接牌策略中直接使用。
2. 在 findAllCombos() 函数中修正了若干Bug。
3. 在 Line 710 处实现了 myGivenCombo() 函数，用来实现出牌策略。其具体方案如下：
   - 首先通过函数 findAllCombo() 找出当前手牌的所有可能组合。
   - 然后先考虑一种特殊情况：对方(和自己对立的一方)只有一张牌的时候，尽量不出单牌。如果只剩单牌，就从大出到小。
   - 对于其他情况，首先按照重载的 < 号(Line 146处)将每种可能出的牌型排序，规定一个出牌次序 order[]，按照这个出牌次序依次考虑出什么牌。
   - 对于双顺、三带、飞机、航天飞机的情况，限制最大的主牌以免造成只有小牌没有大牌的情况。
   - 对于可能出的单张，如果该牌是对子，就不拆它；如果最小的单牌都大于Q，则考虑最小的对子。如果此时最小的对子比较小或者根本就没有单牌，就转而去出对子；否则就出最小的单牌。
4. 在接牌策略方面作出了如下的考虑，分为农民和地主两个角色：
   1. 农民
   - 上次出牌的也是农民且出的牌比较大(comboLevel>=8)，就不出。
   - 上次出牌的也是农民且出的牌比较小，我就跟牌(出我能出的最小牌)。
   - 上次出牌的是地主，且出地牌比较大(comboLevel>7)，我就出2/3大的牌 (sumOfSol-1)*2/3 。
   - 上次出牌的是地主，且出的牌比较小，我就跟牌。
   - 对于炸弹(火箭)，上次出牌的是地主，出的不是单张，且本次出牌的数量>=8，或者出了一对(三个)二，就炸。
   1. 地主
   - 若农民出的牌比较小(comboLevel<7)，就跟牌。
   - 若农民出的牌比较大，就出中间的牌 (sumOfSol-1)/2 。
   - 对于炸弹，若农民出的牌数>=5，或者有一方手牌数量<=10，或者出了一对(三个)二，就炸。

version 1.2

2018/5/20

本次更新主要修正了接牌方面的一个Bug，以及对出牌作了一个优化。

1. 在 Line 899 处增加了变量 currentHave 表示当前正在考虑的主牌是否有足够多的从牌来搭配它，只有最后当 currentHave==packs.size()-mainPackCount 时才满足条件。
2. 在 Line 924 处单独考虑了接单张的情况，尽量做到不拆对子或者三条。如果除去对子或三条之外还有单张解，就把这些对子或三条接从 myCombos 中除去，只留下单牌解。这样一来，就再直接使用后面的接牌策略即可。如果没有单牌的话，就只能拆，故维持不变即可。



version 2.0

2018/5/23

本版本将对出牌策略进行较大的修改，具体实现将在2.0.1或2.0.2完成

 1.在line 12：#include<map>

 2.在line 20和line 21新增了using std::multimap和using std::make_pair

 3.在line 118至line 123把之前版本中放在后面定义的全局变量放在前面来定义了，但是只有CardCombo lastValidCombo在line 987定义，没有放到前面来定义

 4.在struct CardCombo中，在line 146新增成员变量int value用来表示每个CardCombo的权值，此处注意CardCombo看成玩家的手牌，不管这个整体是否满足某一个可打出的手牌样式，如果不能满足，那就是INVALID，但是value指的是当前CardCombo经过所有分解后所得的估值的最大值

 5.在line 149重载了运算符CardCombo operator - (const CardCombo& a) const用于dp过程，具体表示该CardCombo去除牌组a后的剩余牌组

 6.在line 165重载了运算符bool operator < (const CardCombo& a) const用于multimap的排序

 7.在line 420把CardCombo myGivenCombo(CARD_ITERATOR begin, CARD_ITERATOR end) const的定义移到了line 823而仅在此处保留声明

 8.在line 428把CardCombo findFirstValie(CARD_ITERATOR begin, CARD_ITERATOR end) const的定义移到了line 837而仅在此处保留声明

 9.在line 197声明了int getValue() 在line 445处写定义，这个函数是牌型的估值函数，对每个可打出的牌型进行估值，具体估值方案可见landlord.pdf

 10.在line 456把原来的void SearchCard函数移到此处，并且取消了全局变量allCombos，用一个参数vector<CardCombo>* allCombos代替

 11.在line 478修改函数void findAllCombos的参数列表为(CardCombo& thisdeck, vector<CardCombo>* allCombos)，解释如下：CardCombo& thisdeck传入要搜索的手牌范围，allCombos同前一版本，储存搜索出来的所有可行牌型，但是用于之后dp的时候进行枚举

 12.在line 772和line 773定义了两个multimap，分别是multimap<CardCombo, int> map_value和multimap<CardCombo, vector<CardCombo> > map_best_combos第一个用来存储每个手牌对应的最大权值，第二个用来存储每个手牌取最大权值的时候对应的分解

 13.在line 781定义了求最大估值的函数int best_combo_dp(CardCombo mydeck)采用记忆化搜索的动态规划技术，把当前手牌看成某一牌型和剩余手牌的组合，对这二者的权值求和再求最大值。其中递归地求剩余手牌的最大权值

 14.下一步任务：

- 将估值函数int CardCombo::getValue()完善，具体可参考landlord.pdf
- 将出牌策略完善，具体需要完善两个方面：
  - 修改CardCombo::myGivenCombo
  - 修改CardCombo::findFirstValid


