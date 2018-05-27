#include <iostream>
#include <set>
#include <string>
#include <cassert>
#include <cstring> 
#include <algorithm>
#include "jsoncpp/json.h" 

using std::vector;
using std::sort;
using std::unique;
using std::set;
using std::string;

constexpr int PLAYER_COUNT = 3;

using Card=short; 
struct CardCombo;
int myPosition;
short cardRemaining[PLAYER_COUNT] = { 20, 17, 17 };
int lastPlayer;
set<Card> myCards;
set<Card> landlordPublicCards;
vector<vector<Card>> whatTheyPlayed[PLAYER_COUNT];
vector<CardCombo> allCombos[20]; // 当前手牌的所有可能组合 
const int level2value[15]={-7,-6,-5,-4,-3,-2,-1,0,1,2,3,4,5,6,7};

void SearchCard(vector<CardCombo>&,int&, int, short, short, int&, vector<short>&, vector<short>&, short*, short*, set<short>&);
void findAllCombos(vector<Card>&,vector<CardCombo>*); 
void findSeq(vector<Card>&,vector<CardCombo>&);

enum class CardComboType
{
	PASS, // 过
	SINGLE, // 单张      1
	PAIR, // 对子        2
	STRAIGHT, // 顺子    3
	STRAIGHT2, // 双顺   4
	TRIPLET, // 三条     5
	TRIPLET1, // 三带一  6
	TRIPLET2, // 三带二  7
	BOMB, // 炸弹        8
	QUADRUPLE2, // 四带二（只）   9
	QUADRUPLE4, // 四带二（对）   10
	PLANE, // 飞机                11
	PLANE1, // 飞机带小翼         12
	PLANE2, // 飞机带大翼         13
	SSHUTTLE, // 航天飞机         14
	SSHUTTLE2, // 航天飞机带小翼  15
	SSHUTTLE4, // 航天飞机带大翼  16
	ROCKET, // 火箭               17
	INVALID // 非法牌型
}; //0,3,4,13,12,11,6,7,5,1,2,8,10,9,17,14,15,16 

int cardComboScores[] = {
	0, // 过    
	1, // 单张   
	2, // 对子
	6, // 顺子
	6, // 双顺
	4, // 三条
	4, // 三带一
	4, // 三带二
	10, // 炸弹
	8, // 四带二（只）
	8, // 四带二（对）
	8, // 飞机
	8, // 飞机带小翼
	8, // 飞机带大翼
	10, // 航天飞机（需要特判：二连为10分，多连为20分）
	10, // 航天飞机带小翼
	10, // 航天飞机带大翼
	16, // 火箭
	0 // 非法牌型
};

#ifndef _BOTZONE_ONLINE
string cardComboStrings[] = {
	"PASS",
	"SINGLE",
	"PAIR",
	"STRAIGHT",
	"STRAIGHT2",
	"TRIPLET",
	"TRIPLET1",
	"TRIPLET2",
	"BOMB",
	"QUADRUPLE2",
	"QUADRUPLE4",
	"PLANE",
	"PLANE1",
	"PLANE2",
	"SSHUTTLE",
	"SSHUTTLE2",
	"SSHUTTLE4",
	"ROCKET",
	"INVALID"
};
#endif

// 用0~53这54个整数表示唯一的一张牌
constexpr Card card_joker = 52;
constexpr Card card_JOKER = 53;

// 除了用0~53这54个整数表示唯一的牌，
// 这里还用另一种序号表示牌的大小（不管花色），以便比较，称作等级（Level）
// 对应关系如下：
// 3 4 5 6 7 8 9 10	J Q K	A	2	小王	大王
// 0 1 2 3 4 5 6 7	8 9 10	11	12	13	    14
using Level = short;
constexpr Level MAX_LEVEL = 15;
constexpr Level MAX_STRAIGHT_LEVEL = 11;
constexpr Level level_joker = 13;
constexpr Level level_JOKER = 14;

/**
* 将Card变成Level
*/
constexpr Level card2level(Card card)
{
	return card / 4 + card / 53;
}

// 牌的组合，用于计算牌型
struct CardCombo
{
	// 表示同等级的牌有多少张
	// 会按个数从大到小、等级从大到小排序
	struct CardPack
	{
		Level level;
		short count;

		bool operator< (const CardPack& b) const
		{
			if (count == b.count)
				return level > b.level;   //张数相同则等级大的排前面 
			return count > b.count;       //否则按张数排序
		}
	};
	vector<Card> cards; // 原始的牌，未排序
	vector<CardPack> packs; // 按数目和大小排序的牌种
	CardComboType comboType; // 算出的牌型
	Level comboLevel = 0; // 算出的大小序

	bool operator<(const CardCombo& c) const
	{
		if (comboLevel != c.comboLevel)
			return comboLevel < c.comboLevel;
		else return cards.size() < c.cards.size();
	}
	friend vector<Card> operator-(vector<Card> deck,const CardCombo& c)
	{
		vector<Card> tmp=deck;
		vector<Card> cards=c.cards;
		vector<Card>::iterator it2,it1;
		for(it2=cards.begin();it2!=cards.end();it2++)
		{
			it1=find(tmp.begin(),tmp.end(),*it2);
			tmp.erase(it1);
		}
		return tmp;
	}
	/**
	* 检查个数最多的CardPack递减了几个, 如：887766 ==> findMaxSeq=3; 88877734 ==> findMaxSeq=2
	*/
	int findMaxSeq() const
	{
		for (unsigned c = 1; c < packs.size(); c++)
			if (packs[c].count != packs[0].count ||
				packs[c].level != packs[c - 1].level - 1)
				return c;
		return packs.size();
	}

	/**
	* 这个牌型最后算总分的时候的权重
	*/
	int getWeight() const
	{
		// 航天飞机需要单独判断 
		if (comboType == CardComboType::SSHUTTLE ||
			comboType == CardComboType::SSHUTTLE2 ||
			comboType == CardComboType::SSHUTTLE4)
			return cardComboScores[(int)comboType] + (findMaxSeq() > 2) * 10;
		// 其他牌型直接得到权值 
		return cardComboScores[(int)comboType];
	}

	// 创建一个空牌组
	CardCombo() : comboType(CardComboType::PASS) {}

	/**
	* 通过Card（即short）类型的迭代器创建一个牌型
	* 并计算出牌型和大小序等
	* 假设输入没有重复数字（即重复的Card）
	*/

	// 模板构造函数，通过CARD迭代器计算出当前牌型 
	template <typename CARD_ITERATOR>
	CardCombo(CARD_ITERATOR begin, CARD_ITERATOR end)
	{
		// 特判：空
		if (begin == end) //如果没有牌出了就PASS 
		{
			comboType = CardComboType::PASS;
			return;
		}

		// 每种牌有多少个：counts[i]表示第i个等级牌的数量 
		short counts[MAX_LEVEL + 1] = {};

		// 同种牌的张数（有多少个单张、对子、三条、四条，这些是Combo的基本元素）
		// countsOfCount[i]表示第i种(单张、对子...)牌的数量 
		short countOfCount[5] = {};

		cards = vector<Card>(begin, end);
		for (Card c : cards)
			counts[card2level(c)]++;
		for (Level l = 0; l <= MAX_LEVEL; l++)
			if (counts[l])
			{
				packs.push_back(CardPack{ l, counts[l] });
				countOfCount[counts[l]]++;
			}
		sort(packs.begin(), packs.end());

		// 用最多、等级最大的那种牌总是可以比较大小的
		comboLevel = packs[0].level;

		// 计算牌型
		// 按照 同种牌的张数 有几种 进行分类
		vector<int> kindOfCountOfCount;
		for (int i = 0; i <= 4; i++)
			if (countOfCount[i])
				kindOfCountOfCount.push_back(i);
		sort(kindOfCountOfCount.begin(), kindOfCountOfCount.end());

		int curr, lesser;

		switch (kindOfCountOfCount.size())
		{
		case 1: // 只有一类牌(都是单张/对子/三条/四条) 
			curr = countOfCount[kindOfCountOfCount[0]]; //该类牌的数量(单张、对子...) 
			switch (kindOfCountOfCount[0])
			{
			case 1: //该类牌为 单张 
				if (curr == 1)  //只有一张，即当前牌型为 _单张_ 
				{
					comboType = CardComboType::SINGLE;
					return;
				}
				if (curr == 2 && packs[1].level == level_joker) //有两张，其中一张为小王，则必然为_火箭_ 
				{
					comboType = CardComboType::ROCKET;
					return;
				}
				if (curr >= 5 && findMaxSeq() == curr &&  //有多张(>=5)，即是_顺子_ 
					packs.rbegin()->level <= MAX_STRAIGHT_LEVEL)
				{
					comboType = CardComboType::STRAIGHT;
					return;
				}
				break;
			case 2: //该类牌为 对子 
				if (curr == 1)  //出的是 _对子_ 
				{
					comboType = CardComboType::PAIR;
					return;
				}
				if (curr >= 3 && findMaxSeq() == curr && //出的是 _连对_ 
					packs.rbegin()->level <= MAX_STRAIGHT_LEVEL)
				{
					comboType = CardComboType::STRAIGHT2;
					return;
				}
				break;
			case 3: //该类牌为三条 
				if (curr == 1)
				{
					comboType = CardComboType::TRIPLET;  //出的是_三不带_ 
					return;
				}
				if (findMaxSeq() == curr &&
					packs.rbegin()->level <= MAX_STRAIGHT_LEVEL) //由于只有一种牌，故是_飞机不带翼_ 
				{
					comboType = CardComboType::PLANE;
					return;
				}
				break;
			case 4: // 该类牌为四条 
				if (curr == 1)  //只有一个，说明出的是_炸弹_ 
				{
					comboType = CardComboType::BOMB;
					return;
				}
				if (findMaxSeq() == curr &&  //只有一种牌，牌型为_航天飞机不带翼_ 
					packs.rbegin()->level <= MAX_STRAIGHT_LEVEL)
				{
					comboType = CardComboType::SSHUTTLE;
					return;
				}
			}
			break;
		case 2: // 有两类牌，此时就不存在单张、对子、顺子了，只能是 三带?、四带?的情况 
			curr = countOfCount[kindOfCountOfCount[1]];   //牌种更大(三条OR四条)的那一类牌的数量 
			lesser = countOfCount[kindOfCountOfCount[0]]; //牌种更小(单张OR对子)的那一类牌的数量 
			if (kindOfCountOfCount[1] == 3)  //更大的那一类牌为三条 
			{
				// 更小的那一类牌为单张，说明出的是_三带1_ 
				if (kindOfCountOfCount[0] == 1)
				{
					// 三带一
					if (curr == 1 && lesser == 1) //数量都只有1，直接三带1 
					{
						comboType = CardComboType::TRIPLET1;
						return;
					}
					if (findMaxSeq() == curr && lesser == curr &&  //数量不止一，就是_飞机带小翼_ 
						packs.rbegin()->level <= MAX_STRAIGHT_LEVEL)
					{
						comboType = CardComboType::PLANE1;
						return;
					}
				}
				// 更小的那一类牌为对子，三带2 
				if (kindOfCountOfCount[0] == 2)
				{
					// 三带二
					if (curr == 1 && lesser == 1)  //出的牌为_三带2_ 
					{
						comboType = CardComboType::TRIPLET2;
						return;
					}
					if (findMaxSeq() == curr && lesser == curr &&  // _飞机带大翼_ 
						packs.rbegin()->level <= MAX_STRAIGHT_LEVEL)
					{
						comboType = CardComboType::PLANE2;
						return;
					}
				}
			}
			// 更大的那一类牌为 四条 
			if (kindOfCountOfCount[1] == 4)
			{
				// 更小的那一类牌为单张，四条带？
				if (kindOfCountOfCount[0] == 1)
				{
					// 四条带两只 * n
					if (curr == 1 && lesser == 2)  //有两个单张，牌型为_四带2_ 
					{
						comboType = CardComboType::QUADRUPLE2;
						return;
					}
					if (findMaxSeq() == curr && lesser == curr * 2 &&  //连续的飞机，则为_航天飞机带小翼_ 
						packs.rbegin()->level <= MAX_STRAIGHT_LEVEL)
					{
						comboType = CardComboType::SSHUTTLE2;
						return;
					}
				}
				// 更小的那一类牌为对子 
				if (kindOfCountOfCount[0] == 2)
				{
					// 四条带两对 * n
					if (curr == 1 && lesser == 2)  // 有两个对子，_四带两对_ 
					{
						comboType = CardComboType::QUADRUPLE4;
						return;
					}
					if (findMaxSeq() == curr && lesser == curr * 2 && //连续出现，则为_航天飞机带大翼_ 
						packs.rbegin()->level <= MAX_STRAIGHT_LEVEL)
					{
						comboType = CardComboType::SSHUTTLE4;
						return;
					}
				}
			}
		}

		comboType = CardComboType::INVALID; //否则牌无效 
	}

	/**
	* 判断指定牌组能否大过当前牌组（这个函数不考虑过牌的情况！）
	*/
	bool canBeBeatenBy(const CardCombo& b) const
	{
		if (comboType == CardComboType::INVALID || b.comboType == CardComboType::INVALID)
			return false;
		if (b.comboType == CardComboType::ROCKET)  //双王能大过任何牌 
			return true;
		if (b.comboType == CardComboType::BOMB)    //炸弹就比大小 
			switch (comboType)
			{
			case CardComboType::ROCKET:
				return false;
			case CardComboType::BOMB:
				return b.comboLevel > comboLevel;
			default:
				return true;
			}
		// 其他情况要比较大小必须：(1).牌型相同；(2).牌数量相同 
		return b.comboType == comboType && b.cards.size() == cards.size() && b.comboLevel > comboLevel;
	}
	/**
	* 此处为出牌策略
	*/
	template <typename CARD_ITERATOR>
	CardCombo myGivenCombo(CARD_ITERATOR begin, CARD_ITERATOR end) const
	{
		auto deck=vector<Card>(begin,end);
		vector<CardCombo> allCombos[20];
		// 首先找到所有可能的组合
		findAllCombos(deck, allCombos);
		for (int i = 1; i < 18; i++)
			sort(allCombos[i].begin(), allCombos[i].end());

		short counts[MAX_LEVEL + 1] = {};
		for (Card c : deck)
			counts[card2level(c)]++;
		short need[19] = { 0,1,2,5,6,3,4,5,4,6,8,6,8,10,8,12,16,2 }; // 每种牌型需要的牌数
		int order[19] = { 0,3,4,13,12,11,6,7,5,1,2,8,10,9,17,14,15,16 };
		
		// 先考虑一种特殊情况：对方(即和自己对立的一方)只有一/二张牌了，
		// 我就只能出牌数>=2的牌；若只有单张，则只能从大打到小
		if ((myPosition != 0 && cardRemaining[0] <= 2) ||
			(myPosition == 0 && (cardRemaining[1] <= 2 || cardRemaining[2] <= 2)))
		{
			if (allCombos[2].size())
				return allCombos[2][0];
			else
			{
				int Size = allCombos[1].size();
				if (Size)
					return allCombos[1][Size - 1];
			}
		}
		// 双方都按照这个次序出牌
		for (int i = 1; i < 19; i++)
		{
			int Size = allCombos[order[i]].size();
			if (Size)
			{
				switch (order[i])
				{
				case 3:
				{
					CardCombo best=allCombos[3][Size-1];
					int best_low=0,best_high=0;
					for(int i=0;i<Size;i++) 
					{
						vector<Card> tmp_deck=deck-allCombos[3][i];
						vector<CardCombo> tmp;
						findSeq(tmp_deck,tmp);
						int count=tmp.size();
						if(count)
						{
							int card_sz=allCombos[3][i].cards.size();
							int high=allCombos[3][i].packs[0].level;
							int low=allCombos[3][i].packs[card_sz-1].level;
							if(low<=best_low&&high>=best_high)
								best=allCombos[3][i];
						}
					}
					return best;
					break;
				}
				case 4:
					for (int j = Size-1; j >= 0; j--)
						if (allCombos[4][j].comboLevel < 10)
							return allCombos[4][j];
					break;
					// 三带x，飞机带x，航天飞机带x，最大的牌不要超过 Q
				case 5:
				case 6:
				case 7:
				case 11:
				case 12:
				case 13:
				case 14:
				case 15:
				case 16:
				{
					for(int j=1;j<=3;j++)
						if(deck.size()-need[order[i]]*j<=2&&need[order[i]]*j-deck.size()<=2)
							return allCombos[order[i]][0];
					for (int j = 0; j < Size; j++)
						if (allCombos[order[i]][j].comboLevel < 9)
							return allCombos[order[i]][j];
					break;
				}
				// 下面考虑单张 
				case 1:
				{
					short firstPair = -1;
					short firstSingle = -1;
					for (int j = 0; j < Size; j++)
					{
						// 尽量不拆对子,但如果最小的单张都比较大(>Q),就保留 
						Level level = allCombos[1][j].comboLevel;
						if (counts[level] == 2 && firstPair == -1)
							firstPair = j;
						else if (counts[level] == 1)
						{
							if (firstSingle == -1)
								firstSingle = j;
							if (level <= 9)
								return allCombos[1][j];
							else break;
						}
					}
					// 到这里，最小的单张都>Q了，考虑最小的对子
					// 如果最小的对子<Q，或者没有单独的单张，就到下一步出对子
					// 否则还是出单张 
					if ((firstPair < 9 && firstPair != -1) ||
						firstSingle == -1) break;
					else
						return allCombos[1][firstSingle];
					break;
				}
				default: // 其他的就依次出即可 
					return allCombos[order[i]][0];
				}
			}
		}
		return allCombos[1][0];
	}

	// 下面为接牌策略 
	template <typename CARD_ITERATOR> //传入当前要出牌玩家手牌的迭代器 
	CardCombo findFirstValid(CARD_ITERATOR begin, CARD_ITERATOR end) const
	{
		if (comboType == CardComboType::PASS) // 如果不需要大过谁，就轮到自己出牌 
			return myGivenCombo(begin, end);
		
		// 上家是农民，不打队友的情况有 
		if (myPosition != 0 &&
			lastPlayer != 0 &&
			(comboLevel >= 8||
			 (int)comboType >= 5||
			 (int)comboType == 4||
			 ((int)comboType == 3 && cards.size() >= 7)))
			return CardCombo();

		// 下面是该我接牌 
		// 然后先看一下是不是火箭，是的话就过
		if (comboType == CardComboType::ROCKET)
			return CardCombo();

		// 现在打算从手牌中凑出同牌型的牌(无论地主OR农民)
		auto deck = vector<Card>(begin, end); // 手牌
		short counts[MAX_LEVEL + 1] = {};//每个等级牌的个数 

		unsigned short kindCount = 0;

		// 先数一下手牌里每种牌有多少个
		for (Card c : deck)
			counts[card2level(c)]++;

		// 手牌如果不够用，直接不用凑了，看看能不能炸吧
		if (deck.size() < cards.size())
			goto failure;

		// 再数一下手牌里有多少种牌
		for (short c : counts)
			if (c)
				kindCount++;

		{
			// 开始增大主牌
			int mainPackCount = findMaxSeq();
			// 看看是否是连续牌型(顺子、连对、飞机、航天飞机)，也即不是出的单张、一对…… 
			bool isSequential =
				comboType == CardComboType::STRAIGHT ||
				comboType == CardComboType::STRAIGHT2 ||
				comboType == CardComboType::PLANE ||
				comboType == CardComboType::PLANE1 ||
				comboType == CardComboType::PLANE2 ||
				comboType == CardComboType::SSHUTTLE ||
				comboType == CardComboType::SSHUTTLE2 ||
				comboType == CardComboType::SSHUTTLE4;

			// 设置数组用来存所有可行解
			vector<vector<short> > myCombos;

			for (Level i = 1; ; i++) // 增大多少
			{
				for (int j = 0; j < mainPackCount; j++)
				{
					int level = packs[j].level + i; // 增加后对应的牌 
					if ((comboType == CardComboType::SINGLE && level > MAX_LEVEL) ||
						(isSequential && level > MAX_STRAIGHT_LEVEL) ||
						(comboType != CardComboType::SINGLE && !isSequential && level >= level_joker))
						goto findSol;

					// 如果手牌中这种牌不够，就不用继续增了
					if (counts[level] < packs[j].count)
						goto next;
				}

				{
					// 如果手牌的种类数不够，那从牌的种类数就不够，也不行
					if (kindCount < packs.size())
						continue;


					// 好终于可以了
					// 计算每种牌的要求数目吧
					short requiredCounts[MAX_LEVEL + 1] = {};
					for (int j = 0; j < mainPackCount; j++)  // 主牌 
						requiredCounts[packs[j].level + i] = packs[j].count;

					unsigned int currentHave = 0; // 看看当前要求的从牌是否足够 
					for (unsigned j = mainPackCount; j < packs.size(); j++) // 从牌，就依次从当前手牌最小的选起
						for (Level k = 0; k <= MAX_LEVEL; k++)
						{
							if (requiredCounts[k] || counts[k] < packs[j].count)//不能和主牌相同，牌数不够也不行 
								continue;
							requiredCounts[k] = packs[j].count;
							currentHave++;
							break;
						}
					// 如果可以的话，将这个可行解放入可行解数组 
					if (currentHave == packs.size() - mainPackCount)
						myCombos.push_back(vector<short>(requiredCounts, requiredCounts + MAX_LEVEL + 1));
				}

			next:
				; // 再增大
			}

		findSol:
			int sumOfSol = myCombos.size();
			if (!sumOfSol) //没找到可行解，直接去找炸弹 
				goto failure;

			vector<short> bestSol;
		// 下面我们要从myCombos中找出不拆顺子的那些牌 
			int best=0;
			vector<vector<short> > tmp_combo;
			for(int i=0;i<sumOfSol;i++) 
			{
				vector<short> current_combo=myCombos[i];// 注意这里要复制一个 
				vector<Card> tmp_card;
				for (Card c : deck)
				{
					Level level = card2level(c);
					if (current_combo[level])
					{
						tmp_card.push_back(c);
						current_combo[level]--;
					}
				}
				vector<Card> tmp_deck=deck-CardCombo(tmp_card.begin(),tmp_card.end());
				vector<CardCombo> tmp;
				findSeq(tmp_deck,tmp);
				int count=tmp.size();
				if(count>best)
				{
					best=count;
					tmp_combo.clear();
					tmp_combo.push_back(myCombos[i]);
				}
				else if(count==best)
					tmp_combo.push_back(myCombos[i]);
			}
			myCombos=tmp_combo;
			
			if (comboType == CardComboType::SINGLE)
			{
				int haveSingleSol = 0; // 是单牌的数量 
				for (auto it = myCombos.begin(); it != myCombos.end(); ++it)
				{
					int level = -1;
					for (int j = 0; j < MAX_LEVEL + 1; j++)
						if ((*it)[j])
						{
							level = j;
							break;
						}
					if (counts[level] == 1)
						haveSingleSol++;
				}
				if (haveSingleSol) // 如果有单牌 
				{
					sumOfSol = haveSingleSol; // 把其他的非单牌从myCombos中去除 
					for (auto it = myCombos.begin(); it != myCombos.end(); )
					{
						int level = -1;
						for (int j = 0; j < MAX_LEVEL + 1; j++)
							if ((*it)[j])
							{
								level = j;
								break;
							}
						if (counts[level] > 1)
							myCombos.erase(it);
						else ++it;
					}
				}
			}
			sumOfSol = myCombos.size();
			if (myPosition != 0 && lastPlayer == 0 && comboLevel > 7)
				bestSol = myCombos[(sumOfSol - 1) * 2 / 3];
			else if(myPosition != 0 && cardRemaining[0] <= 2)
				bestSol = myCombos[sumOfSol-1];
			else if (myPosition != 0 && lastPlayer == 0)
				bestSol = myCombos[0];
			else if (myPosition != 0 && lastPlayer != 0)
				bestSol = myCombos[0];
			else if (myPosition == 0 &&(cardRemaining[1]<=2||cardRemaining[2]<=2))
				bestSol = myCombos[sumOfSol-1];
			else if (myPosition == 0 && comboLevel < 7)
				bestSol = myCombos[0];
			else if (myPosition == 0 && comboLevel >= 7)
				bestSol = myCombos[(sumOfSol - 1) / 2];
				
			// 开始产生解
			vector<Card> solve;
			for (Card c : deck)
			{
				Level level = card2level(c);
				if (bestSol[level])
				{
					solve.push_back(c);
					bestSol[level]--;
				}
			}
			return CardCombo(solve.begin(), solve.end());
		}

	failure:
		// 最后看一下能不能炸吧
		for (Level i = 0; i < level_joker; i++)
			if (counts[i] == 4) // 存在炸弹 
			{
				Card bomb[] = { Card(i * 4), Card(i * 4 + 1), Card(i * 4 + 2), Card(i * 4 + 3) };
				//上次出牌的是地主，出的不是单张，且本次出牌的数量>=8，或者出了一对二(三个二)，就炸。
				if (myPosition != 0 && lastPlayer == 0 && comboType != CardComboType::SINGLE &&
					(cards.size() >= 8 ||
					((comboType == CardComboType::PAIR || comboType == CardComboType::TRIPLET || comboType == CardComboType::TRIPLET1 || comboType == CardComboType::TRIPLET2) && comboLevel == 12)))
					return CardCombo(bomb, bomb + 4);
				else if (myPosition == 0 &&
					(cards.size() >= 5 ||
					(cardRemaining[1] <= 10 || cardRemaining[2] <= 10) ||
						((comboType == CardComboType::PAIR || comboType == CardComboType::TRIPLET || comboType == CardComboType::TRIPLET1 || comboType == CardComboType::TRIPLET2) && comboLevel == 12)))
					return CardCombo(bomb, bomb + 4);
			}

		// 有没有火箭？
		if (counts[level_joker] + counts[level_JOKER] == 2)
		{
			Card rocket[] = { card_joker, card_JOKER };
			if (myPosition != 0 && lastPlayer == 0 && comboType != CardComboType::SINGLE &&
				(cards.size() >= 8 ||
				((comboType == CardComboType::PAIR || comboType == CardComboType::TRIPLET || comboType == CardComboType::TRIPLET1 || comboType == CardComboType::TRIPLET2) && comboLevel == 12)))
				return CardCombo(rocket, rocket + 2);
			else if (myPosition == 0 &&
				(cards.size() >= 5 ||
				(cardRemaining[1] <= 10 || cardRemaining[2] <= 10) ||
					((comboType == CardComboType::PAIR || comboType == CardComboType::TRIPLET || comboType == CardComboType::TRIPLET1 || comboType == CardComboType::TRIPLET2) && comboLevel == 12)))
				return CardCombo(rocket, rocket + 2);
		}
		return CardCombo();
	} // end FindFirstValid()

	void debugPrint()
	{
#ifndef _BOTZONE_ONLINE
		std::cout << "【" << cardComboStrings[(int)comboType] <<
			"共" << cards.size() << "张，大小序" << comboLevel << "】";
#endif
	}
}; //end class CardCombo

void SearchCard(vector<CardCombo>* Combos,int& comboType, int currentLevel, short cardType, short already, int& target, vector<short>& tmp, vector<short>& deck, short* counts, short* beginOfCounts, set<Level>& levelOf_)
{
	if (already == target)
	{
		Combos[comboType].push_back(CardCombo(tmp.begin(), tmp.end()));
		return;
	}
	short off = target - already; //还差多少张
	for (int i = currentLevel; i < 12 - off; i++)
		// 当前等级的牌必须1.张数>=cardType,2.不能是levelOf_里的
		if (counts[i] == cardType && !levelOf_.count(i))
		{
			int l = beginOfCounts[i];   // 找到等级i的牌在deck中的位置
			for (int j = 0; j < cardType; j++)
				tmp.push_back(deck[l + j]);
			SearchCard(Combos,comboType, i + 1, cardType, already + 1, target, tmp, deck, counts, beginOfCounts, levelOf_);
			for (int j = 0; j < cardType; j++)
				tmp.pop_back(); // 回溯
		}
}

void findSeq(vector<Card>& deck,vector<CardCombo>& Combos)
{
	short counts[MAX_LEVEL + 1] = {};
	short beginOfCounts[MAX_LEVEL + 1] = {};

	sort(deck.begin(), deck.end());
	for (Card c : deck)
		counts[card2level(c)]++;  
	for (int i = 1; i <= 14; i++)
		beginOfCounts[i] = beginOfCounts[i - 1] + counts[i - 1];
		
	for (int l = 0; l < 8; l++) 
			for (int r = l + 4; r < 12; r++) 
			{
				int haveSeq = -1;
				for (int i = l; i <= r; i++)
					if (!counts[i])
					{
						haveSeq = i; 
						break;
					}
				if (haveSeq != -1) 
				{
					l = haveSeq;
					break;
				}
				vector<Card> tmp;
				for (int i = l; i <= r; i++) 
					tmp.push_back(deck[beginOfCounts[i]]);
				Combos.push_back(CardCombo(tmp.begin(), tmp.end()));
			}
}

void findAllCombos(vector<Card>& deck,vector<CardCombo>* Combos)
	{
		short counts[MAX_LEVEL + 1] = {};
		short beginOfCounts[MAX_LEVEL + 1] = {}; //  每个等级牌在手牌中的起始位置
		int currentType;   // 当前枚举类型 
		int Size = deck.size();// 当前手牌数量

		sort(deck.begin(), deck.end()); //先排个序再说吧... 
		for (Card c : deck)
			counts[card2level(c)]++;  // 数一下各等级的牌有多少

		for (int i = 1; i <= 14; i++)
			beginOfCounts[i] = beginOfCounts[i - 1] + counts[i - 1];

		// 下面开始枚举，因为同等级的牌是没有区别的，所以只考虑该等级的前k张
		// 首先枚举单张(包括大小王)
		currentType = (int)CardComboType::SINGLE;
		for (int i = 0; i <= 14; i++)
			if (counts[i])
			{
				vector<Card> tmp;
				tmp.push_back(deck[beginOfCounts[i]]);
				Combos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
			}

		// 下面枚举对子
		currentType = (int)CardComboType::PAIR;
		for (int i = 0; i <13; i++) // 最多到一对2
			if (counts[i] == 2)
			{
				vector<Card> tmp;
				int l = beginOfCounts[i];  // 等级i的牌在deck中的起始位置l
				tmp.push_back(deck[l]);
				tmp.push_back(deck[l + 1]);
				Combos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
			}

		// 下面枚举单顺
		currentType = (int)CardComboType::STRAIGHT;
		for (int l = 0; l < 8; l++) // 枚举顺子起点
			for (int r = l + 4; r < 12; r++) // 枚举顺子终点
			{
				int haveSeq = -1;
				for (int i = l; i <= r; i++)
					if (!counts[i])
					{
						haveSeq = i; // 在haveSeq处间断 
						break;
					}
				if (haveSeq != -1) // 在[l,r]中间某处不满足了，r再增大也不会满足
				{
					l = haveSeq;
					break;
				}
				vector<Card> tmp;
				for (int i = l; i <= r; i++) // 每个选第一张就行了
					tmp.push_back(deck[beginOfCounts[i]]);
				Combos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
			}

		// 下面枚举双顺
		currentType = (int)CardComboType::STRAIGHT2;
		for (int l = 0; l < 10; l++)
			for (int r = l + 2; r < 12; r++)
			{
				int haveSeq = -1;
				for (int i = l; i <= r; i++)
					if (counts[i] < 2||counts[i]==4)
					{
						haveSeq = i;
						break;
					}
				if (haveSeq != -1)
				{
					l = haveSeq;
					break;
				}
				vector<Card> tmp;
				for (int i = l; i <= r; i++)
					for (int j = beginOfCounts[i]; j < beginOfCounts[i] + 2; j++)
						tmp.push_back(deck[j]);
				Combos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
			}

		// 下面枚举三条
		currentType = (int)CardComboType::TRIPLET;
		for (int i = 0; i<13; i++)
			if (counts[i] == 3)
			{
				vector<Card> tmp;
				int l = beginOfCounts[i];
				tmp.push_back(deck[l]);
				tmp.push_back(deck[l + 1]);
				tmp.push_back(deck[l + 2]);
				Combos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
			}

		// 下面枚举炸弹
		currentType = (int)CardComboType::BOMB;
		for (int i = 0; i<13; i++)
			if (counts[i] == 4)
			{
				vector<Card> tmp;
				int l = beginOfCounts[i];
				for (int j = l; j < l + 4; j++)
					tmp.push_back(deck[j]);
				Combos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
			}

		// 下面枚举组合牌，直接利用上面我们求出的主牌即可，为此需要先得出每种牌型的数量
		int Triple = (int)CardComboType::TRIPLET;
		int TripleSize = Combos[Triple].size();
		int Quad = (int)CardComboType::BOMB;
		int QuadSize = Combos[Quad].size();

		// 下面枚举三带一张
		currentType = (int)CardComboType::TRIPLET1;
		for (int i = 0; i < TripleSize; i++)
		{
			vector<Card> mainCards = Combos[Triple][i].cards;
			int mainLevel = Combos[Triple][i].comboLevel;
			for (int j = 0; j < 15; j++)
				if (j != mainLevel && counts[j] == 1)
				{
					vector<Card> tmp = mainCards;
					tmp.push_back(deck[beginOfCounts[j]]);
					Combos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
				}
		}

		// 下面枚举三带二(一对)
		currentType = (int)CardComboType::TRIPLET2;
		for (int i = 0; i < TripleSize; i++)
		{
			vector<Card> mainCards = Combos[Triple][i].cards;
			int mainLevel = Combos[Triple][i].comboLevel;
			for (int j = 0; j < 13; j++)
				if (j != mainLevel && counts[j] == 2)
				{
					vector<Card> tmp = mainCards;
					tmp.push_back(deck[beginOfCounts[j]]);
					tmp.push_back(deck[beginOfCounts[j] + 1]);
					Combos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
					tmp.clear();
				}
		}

		// 下面枚举四带二单，直接用上面枚举的炸弹
		currentType = (int)CardComboType::QUADRUPLE2;
		for (int i = 0; i < QuadSize; i++)
		{
			vector<Card> mainCards = Combos[Quad][i].cards;
			int mainLevel = Combos[Quad][i].comboLevel;
			for (int j = 0; j < 12; j++)
				for (int k = j + 1; k < 13; k++)
					if (j != mainLevel && k != mainLevel && counts[j] == 1 && counts[k] == 1)
					{
						vector<Card> tmp = mainCards;
						tmp.push_back(deck[beginOfCounts[j]]);
						tmp.push_back(deck[beginOfCounts[k]]);
						Combos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
						tmp.clear();
					}
		}

		// 下面枚举四带二对
		currentType = (int)CardComboType::QUADRUPLE4;
		for (int i = 0; i < QuadSize; i++)
		{
			vector<Card> mainCards = Combos[Quad][i].cards;
			int mainLevel = Combos[Quad][i].comboLevel;
			for (int j = 0; j < 12; j++)
				for (int k = j + 1; k < 13; k++)
					if (j != mainLevel && k != mainLevel && counts[j] == 2 && counts[k] == 2)
					{
						vector<Card> tmp = mainCards;
						tmp.push_back(deck[beginOfCounts[j]]);
						tmp.push_back(deck[beginOfCounts[j] + 1]);
						tmp.push_back(deck[beginOfCounts[k]]);
						tmp.push_back(deck[beginOfCounts[k] + 1]);
						Combos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
						tmp.clear();
					}
		}

		// 下面枚举飞机不带翼
		currentType = (int)CardComboType::PLANE;
		int lastLevel;
		for (int i = 0; i < TripleSize; i++)
		{
			lastLevel = Combos[Triple][i].comboLevel;
			vector<Card> tmp = Combos[Triple][i].cards;
			for (int j = i + 1; j < TripleSize; j++)
			{
				int currentLevel = Combos[Triple][j].comboLevel;
				if (lastLevel + 1 == currentLevel && currentLevel != 12) // 不能有2
				{
					lastLevel = currentLevel;
					tmp.insert(tmp.end(), Combos[Triple][j].cards.begin(), Combos[Triple][j].cards.end());
					Combos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
				}
				else
					break;
			}
		}

		// 下面枚举航天飞机不带翼
		currentType = (int)CardComboType::SSHUTTLE;
		for (int i = 0; i < QuadSize; i++)
		{
			lastLevel = Combos[Quad][i].comboLevel;
			vector<Card> tmp = Combos[Quad][i].cards;
			for (int j = i + 1; j < QuadSize; j++)
			{
				int currentLevel = Combos[Quad][j].comboLevel;
				if (lastLevel + 1 == currentLevel && currentLevel != 12)
				{
					lastLevel = currentLevel;
					tmp.insert(tmp.end(), Combos[Quad][j].cards.begin(), Combos[Quad][j].cards.end());
					Combos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
				}
				else
					break;
			}
		}
		int Plane = (int)CardComboType::PLANE;
		int PlaneSize = Combos[Plane].size();
		int SShuttle = (int)CardComboType::SSHUTTLE;
		int SShuttleSize = Combos[SShuttle].size();

		// 下面枚举飞机带小翼
		currentType = (int)CardComboType::PLANE1;
		for (int i = 0; i < PlaneSize; i++)
		{
			vector<Card> tmp = Combos[Plane][i].cards;
			set<Level> levels;   // 当前飞机中所有三条的等级
			for (Card c : tmp)
				levels.insert(card2level(c));
			int levelCount = levels.size(); // 一共需要这么多张单张
			SearchCard(Combos,currentType, 0, 1, 0, levelCount, tmp, deck, counts, beginOfCounts, levels);
		}

		// 下面枚举飞机带大翼
		currentType = (int)CardComboType::PLANE2;
		for (int i = 0; i < PlaneSize; i++)
		{
			vector<Card> tmp = Combos[Plane][i].cards;
			set<Level> levels;
			for (Card c : tmp)
				levels.insert(card2level(c));
			int levelCount = levels.size(); // 一共需要这么多对子
			SearchCard(Combos,currentType, 0, 2, 0, levelCount, tmp, deck, counts, beginOfCounts, levels);
		}

		// 下面枚举航天飞机带小翼
		currentType = (int)CardComboType::SSHUTTLE2;
		for (int i = 0; i < SShuttleSize; i++)
		{
			vector<Card> tmp = Combos[SShuttle][i].cards;
			set<Level> levels;   // 当前航天飞机中所有四条的等级
			for (Card c : tmp)
				levels.insert(card2level(c));
			int levelCount = 2 * levels.size();
			SearchCard(Combos,currentType, 0, 1, 0, levelCount, tmp, deck, counts, beginOfCounts, levels);
		}

		// 下面枚举航天飞机带大翼
		currentType = (int)CardComboType::SSHUTTLE4;
		for (int i = 0; i < SShuttleSize; i++)
		{
			vector<Card> tmp = Combos[SShuttle][i].cards;
			set<Level> levels;
			for (Card c : tmp)
				levels.insert(card2level(c));
			int levelCount = 2 * levels.size();
			SearchCard(Combos,currentType, 0, 2, 0, levelCount, tmp, deck, counts, beginOfCounts, levels);
		}

		// 下面考虑火箭
		currentType = (int)CardComboType::ROCKET;
		if (counts[13] == 1 && counts[14] == 1)
		{
			vector<Card> tmp;
			tmp.push_back(deck[Size - 1]);
			tmp.push_back(deck[Size - 2]);
			Combos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
		}
	}

CardCombo lastValidCombo;

namespace BotzoneIO
{
	using namespace std;
	void input()
	{
		// 读入输入（平台上的输入是单行）
		string line;
		getline(cin, line);
		Json::Value input;
		Json::Reader reader;
		reader.parse(line, input);

		// 首先处理第一回合，得知自己是谁、有哪些牌
		{
			auto firstRequest = input["requests"][0u]; // 下标需要是 unsigned，可以通过在数字后面加u来做到
			auto own = firstRequest["own"];
			auto llpublic = firstRequest["public"];
			auto history = firstRequest["history"];
			for (unsigned i = 0; i < own.size(); i++)  //我的牌 
				myCards.insert(own[i].asInt());
			for (unsigned i = 0; i < llpublic.size(); i++) // 地主被公开的三张牌 
				landlordPublicCards.insert(llpublic[i].asInt());
			if (history[0u].size() == 0)
				if (history[1].size() == 0)
					myPosition = 0; // 上上家和上家都没出牌，说明是地主
				else
					myPosition = 1; // 上上家没出牌，但是上家出牌了，说明是农民甲
			else
				myPosition = 2; // 上上家出牌了，说明是农民乙
		}

		// history里第一项（上上家）和第二项（上家）分别是谁的决策
		int whoInHistory[] = { (myPosition - 2 + PLAYER_COUNT) % PLAYER_COUNT, (myPosition - 1 + PLAYER_COUNT) % PLAYER_COUNT };

		int turn = input["requests"].size();
		for (int i = 0; i < turn; i++)
		{
			// 逐次恢复局面到当前
			auto history = input["requests"][i]["history"]; // 每个历史中有上家和上上家出的牌
			int howManyPass = 0;
			for (int p = 0; p < 2; p++)
			{
				int player = whoInHistory[p]; // 是谁出的牌
				auto playerAction = history[p]; // 出的哪些牌
				vector<Card> playedCards;
				for (unsigned _ = 0; _ < playerAction.size(); _++) // 循环枚举这个人出的所有牌
				{
					int card = playerAction[_].asInt(); // 这里是出的一张牌
					playedCards.push_back(card);
				}
				whatTheyPlayed[player].push_back(playedCards); // 记录这段历史
				cardRemaining[player] -= playerAction.size();

				if (playerAction.size() == 0)
					howManyPass++;
				else
				{
					lastValidCombo = CardCombo(playedCards.begin(), playedCards.end());
					lastPlayer = player;  // 设置上一次出牌玩家 
				}

			}

			if (howManyPass == 2)  //上家和上上家都PASS了 
				lastValidCombo = CardCombo();

			if (i < turn - 1)
			{
				// 还要恢复自己曾经出过的牌
				auto playerAction = input["responses"][i]; // 出的哪些牌
				vector<Card> playedCards;
				for (unsigned _ = 0; _ < playerAction.size(); _++) // 循环枚举自己出的所有牌
				{
					int card = playerAction[_].asInt(); // 这里是自己出的一张牌
					myCards.erase(card); // 从自己手牌中删掉
					playedCards.push_back(card);
				}
				whatTheyPlayed[myPosition].push_back(playedCards); // 记录这段历史
				cardRemaining[myPosition] -= playerAction.size();
			}
		}
	} // end input() 

	  /**
	  * 输出决策，begin是迭代器起点，end是迭代器终点
	  * CARD_ITERATOR是Card（即short）类型的迭代器
	  */
	template <typename CARD_ITERATOR>
	void output(CARD_ITERATOR begin, CARD_ITERATOR end)
	{
		Json::Value result, response(Json::arrayValue);
		for (; begin != end; begin++)
			response.append(*begin);
		result["response"] = response;

		Json::FastWriter writer;
		cout << writer.write(result) << endl;
	}
}

int main()
{
	BotzoneIO::input();
	CardCombo myAction = lastValidCombo.findFirstValid(myCards.begin(), myCards.end());
	assert(myAction.comboType != CardComboType::INVALID);
	assert(
		(lastValidCombo.comboType != CardComboType::PASS && myAction.comboType == CardComboType::PASS) ||
		(lastValidCombo.comboType != CardComboType::PASS && lastValidCombo.canBeBeatenBy(myAction)) ||
		(lastValidCombo.comboType == CardComboType::PASS && myAction.comboType != CardComboType::INVALID)
	);
	BotzoneIO::output(myAction.cards.begin(), myAction.cards.end());
}
