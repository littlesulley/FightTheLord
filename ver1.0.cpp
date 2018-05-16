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

// 我是几号玩家（0-地主，1-农民甲，2-农民乙）
int myPosition;

// 上一次出牌的玩家
int lastPlayer;

struct CardCombo;
vector<CardCombo> allCombos[20]; // 当前手牌的所有可能组合 

								 // 递归地找牌,实现放在后面 
void SearchCard(int&, int, short, short, int&, vector<short>&, vector<short>&, short*, short*, set<short>&);

enum class CardComboType
{
	PASS, // 过
	SINGLE, // 单张
	PAIR, // 对子
	STRAIGHT, // 顺子
	STRAIGHT2, // 双顺
	TRIPLET, // 三条
	TRIPLET1, // 三带一
	TRIPLET2, // 三带二
	BOMB, // 炸弹
	QUADRUPLE2, // 四带二（只）
	QUADRUPLE4, // 四带二（对）
	PLANE, // 飞机
	PLANE1, // 飞机带小翼
	PLANE2, // 飞机带大翼
	SSHUTTLE, // 航天飞机
	SSHUTTLE2, // 航天飞机带小翼
	SSHUTTLE4, // 航天飞机带大翼
	ROCKET, // 火箭
	INVALID // 非法牌型
};

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
using Card = short;
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
	* 下面是对当前/某次出牌后的手牌的估值函数，是对当前手牌好坏程度及获胜概率的评价
	* 目前先放在此处待用
	*/

	/*
	template <typename CARD_ITERATOR>
	int EvaluateCard(CARD_ITERATOR begin,CARD_ITERATOR end) const
	{

	}
	*/

	/**
	* 下面找出当前所有可能的牌型的组合
	*/
	template <typename CARD_ITERATOR>
	void findAllCombos(CARD_ITERATOR begin, CARD_ITERATOR end) const
	{
		for (int i = 0; i < 20; i++)
			allCombos[i].clear();
		auto deck = vector<Card>(begin, end);
		vector<Card> tmp;
		short counts[MAX_LEVEL + 1] = {};
		short beginOfCounts[MAX_LEVEL + 1] = {}; //  每个等级牌在手牌中的起始位置
		int currentType;   // 当前枚举类型 
		int Size = deck.size();// 当前手牌数量

		sort(deck.begin(), deck.end()); //先排个序再说吧... 
		for (Card c : deck)
			counts[card2level(c)]++;  // 数一下各等级的牌有多少

		for (int i = 1; i < 13; i++)
			beginOfCounts[i] = beginOfCounts[i - 1] + counts[i - 1];

		// 下面开始枚举，因为同等级的牌是没有区别的，所以只考虑该等级的前k张
		// 首先枚举单张(包括大小王)
		currentType = (int)CardComboType::SINGLE;
		for (int i = 0; i<13; i++)
			if (counts[i])
			{
				tmp.push_back(deck[beginOfCounts[i]]);
				allCombos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
				tmp.clear();
			}
		if (counts[13]) // 小王
		{
			tmp.push_back(deck[Size - 2]);
			allCombos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
			tmp.clear();
		}
		if (counts[14]) // 大王
		{
			tmp.push_back(deck[Size - 1]);
			allCombos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
			tmp.clear();
		}

		// 下面枚举对子
		currentType = (int)CardComboType::PAIR;
		for (int i = 0; i <13; i++) // 最多到一对2
			if (counts[i] >= 2)
			{
				int l = beginOfCounts[i];  // 等级i的牌在deck中的起始位置l
				tmp.push_back(deck[l]);
				tmp.push_back(deck[l + 1]);
				allCombos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
				tmp.clear();
			}

		// 下面枚举单顺
		currentType = (int)CardComboType::STRAIGHT;
		for (int l = 0; l < 8; l++) // 枚举顺子起点
			for (int r = l + 4; r < 12; r++) // 枚举顺子终点
			{
				int haveSeq = 1;
				for (int i = l; i <= r; r++)
					if (!counts[i])
					{
						haveSeq = 0;
						break;
					}
				if (!haveSeq) break; // 在[l,r]中间某处不满足了，r再增大也不会满足
				for (int i = l; i <= r; i++) // 每个选第一张就行了
					tmp.push_back(deck[beginOfCounts[i]]);
				allCombos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
				tmp.clear();
			}

		// 下面枚举双顺
		currentType = (int)CardComboType::STRAIGHT2;
		for (int l = 0; l<10; l++)
			for (int r = l + 2; r < 12; r++)
			{
				int haveSeq = 1;
				for (int i = l; i <= r; i++)
					if (counts[i] < 2)
					{
						haveSeq = 0;
						break;
					}
				if (!haveSeq) break;
				for (int i = l; i <= r; i++)
					for (int j = beginOfCounts[i]; j < beginOfCounts[i] + 2; j++)
						tmp.push_back(deck[j]);
				allCombos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
				tmp.clear();
			}

		// 下面枚举三条
		currentType = (int)CardComboType::TRIPLET;
		for (int i = 0; i<13; i++)
			if (counts[i] >= 3)
			{
				int l = beginOfCounts[i];
				tmp.push_back(deck[l]);
				tmp.push_back(deck[l + 1]);
				tmp.push_back(deck[l + 2]);
				allCombos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
				tmp.clear();
			}

		// 下面枚举炸弹
		currentType = (int)CardComboType::BOMB;
		for (int i = 0; i<13; i++)
			if (counts[i] == 4)
			{
				int l = beginOfCounts[i];
				for (int j = l; j < l + 4; j++)
					tmp.push_back(deck[j]);
				allCombos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
				tmp.clear();
			}

		// 下面枚举组合牌，直接利用上面我们求出的主牌即可，为此需要先得出每种牌型的数量
		int Triple = (int)CardComboType::TRIPLET;
		int TripleSize = allCombos[Triple].size();
		int Quad = (int)CardComboType::BOMB;
		int QuadSize = allCombos[Quad].size();

		// 下面枚举三带一张
		currentType = (int)CardComboType::TRIPLET1;
		for (int i = 0; i < TripleSize; i++)
		{
			vector<Card> mainCards = allCombos[Triple][i].cards;
			int mainLevel = allCombos[Triple][i].comboLevel;
			for (int j = 0; j < 15; j++)
				if (j != mainLevel && counts[j]) // 三带一不能带相同的，不然就成炸弹了
				{
					tmp = mainCards;
					tmp.push_back(deck[beginOfCounts[j]]);
					allCombos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
					tmp.clear();
				}
		}

		// 下面枚举三带二(一对)
		currentType = (int)CardComboType::TRIPLET2;
		for (int i = 0; i < TripleSize; i++)
		{
			vector<Card> mainCards = allCombos[Triple][i].cards;
			int mainLevel = allCombos[Triple][i].comboLevel;
			for (int j = 0; j < 13; j++)
				if (j != mainLevel && counts[j] >= 2)
				{
					tmp = mainCards;
					tmp.push_back(deck[beginOfCounts[j]]);
					tmp.push_back(deck[beginOfCounts[j] + 1]);
					allCombos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
					tmp.clear();
				}
		}

		// 下面枚举四带二单，直接用上面枚举的炸弹
		currentType = (int)CardComboType::QUADRUPLE2;
		for (int i = 0; i < QuadSize; i++)
		{
			vector<Card> mainCards = allCombos[Quad][i].cards;
			int mainLevel = allCombos[Quad][i].comboLevel;
			for (int j = 0; j < 12; j++)
				for (int k = j + 1; k < 13; k++)
					if (j != mainLevel && k != mainLevel && counts[j] && counts[k])
					{
						tmp = mainCards;
						tmp.push_back(deck[beginOfCounts[j]]);
						tmp.push_back(deck[beginOfCounts[k]]);
						allCombos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
						tmp.clear();
					}
		}

		// 下面枚举四带二对
		currentType = (int)CardComboType::QUADRUPLE4;
		for (int i = 0; i < QuadSize; i++)
		{
			vector<Card> mainCards = allCombos[Quad][i].cards;
			int mainLevel = allCombos[Quad][i].comboLevel;
			for (int j = 0; j < 12; j++)
				for (int k = j + 1; k < 13; k++)
					if (j != mainLevel && k != mainLevel && counts[j] >= 2 && counts[k] >= 2)
					{
						tmp = mainCards;
						tmp.push_back(deck[beginOfCounts[j]]);
						tmp.push_back(deck[beginOfCounts[j] + 1]);
						tmp.push_back(deck[beginOfCounts[k]]);
						tmp.push_back(deck[beginOfCounts[k] + 1]);
						allCombos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
						tmp.clear();
					}
		}

		// 下面枚举飞机不带翼
		currentType = (int)CardComboType::PLANE;
		int lastLevel;
		for (int i = 0; i < TripleSize; i++)
		{
			lastLevel = allCombos[Triple][i].comboLevel;
			tmp = allCombos[Triple][i].cards;
			for (int j = i + 1; j < TripleSize; j++)
			{
				int currentLevel = allCombos[Triple][j].comboLevel;
				if (lastLevel + 1 == currentLevel && currentLevel != 12) // 不能有2
				{
					lastLevel = currentLevel;
					tmp.insert(tmp.end(), allCombos[Triple][j].cards.begin(), allCombos[Triple][j].cards.end());
					allCombos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
				}
				else
					break;
			}
			tmp.clear();
		}

		// 下面枚举航天飞机不带翼
		currentType = (int)CardComboType::SSHUTTLE;
		for (int i = 0; i < QuadSize; i++)
		{
			lastLevel = allCombos[Quad][i].comboLevel;
			tmp = allCombos[Quad][i].cards;
			for (int j = i + 1; j < QuadSize; j++)
			{
				int currentLevel = allCombos[Quad][j].comboLevel;
				if (lastLevel + 1 == currentLevel)
				{
					lastLevel = currentLevel;
					tmp.insert(tmp.end(), allCombos[Quad][j].cards.begin(), allCombos[Quad][j].cards.end());
					allCombos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
				}
				else
					break;
			}
			tmp.clear();
		}

		int Plane=(int)CardComboType::PLANE;
		int PlaneSize = allCombos[Plane].size();
		int SShuttle = (int)CardComboType::SSHUTTLE;
		int SShuttleSize = allCombos[SShuttle].size();

		// 下面枚举飞机带小翼
		currentType = (int)CardComboType::PLANE1;
		for (int i = 0; i < PlaneSize; i++)
		{
			tmp = allCombos[Plane][i].cards;
			set<Level> levels;   // 当前飞机中所有三条的等级
			for (Card c : tmp)
				levels.insert(card2level(c));
			int levelCount = levels.size(); // 一共需要这么多张单张
			// 下面枚举所有可能的从牌
			SearchCard(currentType, 0, 1, 0, levelCount, tmp, deck, counts, beginOfCounts, levels);
			tmp.clear();
		}

		// 下面枚举飞机带大翼
		currentType = (int)CardComboType::PLANE2;
		for (int i = 0; i < PlaneSize; i++)
		{
			tmp = allCombos[Plane][i].cards;
			set<Level> levels;
			for (Card c : tmp)
				levels.insert(card2level(c));
			int levelCount = levels.size(); // 一共需要这么多对子
			SearchCard(currentType, 0, 2, 0, levelCount, tmp, deck, counts, beginOfCounts, levels);
			tmp.clear();
		}

		// 下面枚举航天飞机带小翼
		currentType = (int)CardComboType::SSHUTTLE2;
		for (int i = 0; i < SShuttleSize; i++)
		{
			tmp = allCombos[SShuttle][i].cards;
			set<Level> levels;   // 当前航天飞机中所有四条的等级
			for (Card c : tmp)
				levels.insert(card2level(c));
			int levelCount = 2 * levels.size();
			SearchCard(currentType, 0, 1, 0, levelCount, tmp, deck, counts, beginOfCounts, levels);
			tmp.clear();
		}

		// 下面枚举航天飞机带大翼
		currentType = (int)CardComboType::SSHUTTLE4;
		for (int i = 0; i < SShuttleSize; i++)
		{
			tmp = allCombos[SShuttle][i].cards;
			set<Level> levels; 
			for (Card c : tmp)
				levels.insert(card2level(c));
			int levelCount = 2 * levels.size();
			SearchCard(currentType, 0, 1, 0, levelCount, tmp, deck, counts, beginOfCounts, levels);
			tmp.clear();
		}

		// 下面考虑火箭
		currentType = (int)CardComboType::ROCKET;
		if (counts[13] == 1 && counts[14] == 1)
		{
			tmp.push_back(deck[Size - 1]);
			tmp.push_back(deck[Size - 2]);
			allCombos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
		}

		// 好了，现在都考虑完了，在下面愉快地使用allCombos作出决策吧！
	}

	/**
	* 此处为出牌策略，按照我们的讨论(贪心法)需要分为地主和农民(因此把下面的myPosition放到了开始定义)
	* 地主：按照分数从大到小出牌，炸弹、航天飞机、火箭除外；
	* 农民：尽量出比较小的复杂的(权值大的)牌，和牌数比地主大的牌(也就是尽量减少手牌数)；
	*/
	template <typename CARD_ITERATOR>
	CardCombo myGivenCombo(CARD_ITERATOR begin, CARD_ITERATOR end) const
	{
		// 首先找到所有可能的组合
		findAllCombos(begin, end);
		// 现在vector<vector<Card> > allCombos[20]已经完成赋值，下面对它进行选择 

		// 农民出牌 
		if (myPosition != 0)
		{

		}

		// 地主出牌 
		else
		{

		}
	}

	// 下面为接牌策略 
	template <typename CARD_ITERATOR> //传入当前要出牌玩家手牌的迭代器 
	CardCombo findFirstValid(CARD_ITERATOR begin, CARD_ITERATOR end) const
	{
		if (comboType == CardComboType::PASS) // 如果不需要大过谁，就轮到自己出牌 
		{
			CardCombo myCombo = myGivenCombo(begin, end);
			return myCombo;
		}

		if (myPosition * lastPlayer != 0) // 如果我是地主，那么我总要出牌；如果我是农民且上次出牌的是队友，就不出 
			return CardCombo();

		// 下面是该我接牌 
		// 然后先看一下是不是火箭，是的话就过(因为打不过...) 
		if (comboType == CardComboType::ROCKET)
			return CardCombo();

		// 现在打算从手牌中凑出同牌型的牌(无论地主OR农民)
		auto deck = vector<Card>(begin, end); // 手牌
		short counts[MAX_LEVEL + 1] = {};//和上面的构造函数是同样的，即每个等级牌的个数 

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

		// 否则不断增大当前牌组的主牌，看看能不能找到匹配的牌组
		// 主牌：该牌型中占据主要地位的牌，比如三带一的三就是主牌，88877764的888777就是主牌
		// 同样地可以把64定义为从牌，也即主牌附带的牌 
		// 找到了主牌，这个牌型整体就确定了 
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
			/**
			* 这里的意思是，如果当前牌型为：888777666
			* 那么你就要找如下的牌：999888777/101010999888/.../AAAKKKQQQ
			* 从而也就是对当前牌型去枚举一个“增量”，如上例增量为1对应999888777
			*/

			// 设置数组用来存所有可行解
			vector<vector<short> > myCombos;

			for (Level i = 1; ; i++) // 增大多少
			{
				for (int j = 0; j < mainPackCount; j++)
				{
					int level = packs[j].level + i; // 增加后对应的牌 
													// 各种连续牌型的主牌不能到2，非连续牌型的主牌不能到小王，单张的主牌不能超过大王
					if ((comboType == CardComboType::SINGLE && level > MAX_LEVEL) ||
						(isSequential && level > MAX_STRAIGHT_LEVEL) ||
						(comboType != CardComboType::SINGLE && !isSequential && level >= level_joker))
						goto findSol;

					// 如果手牌中这种牌不够，就不用继续增了
					if (counts[level] < packs[j].count)
						goto next;
				}

				/**
				* 接牌的原则:
				* 地主：出刚好可以接的牌，就是这里的策略；
				* 农民：足够大(最大的那张牌的level)的时候就出最大的那一种，否则就按照此处的策略。
				* 无论如何，都需要先把所有解都保存下来再说。
				*/
				{
					// 如果手牌的种类数不够，那从牌的种类数就不够，也不行
					if (kindCount < packs.size())
						continue;


					// 好终于可以了
					// 计算每种牌的要求数目吧
					short requiredCounts[MAX_LEVEL + 1] = {};
					for (int j = 0; j < mainPackCount; j++)  // 主牌 
						requiredCounts[packs[j].level + i] = packs[j].count;
					for (unsigned j = mainPackCount; j < packs.size(); j++) // 从牌 
						for (Level k = 0; k <= MAX_LEVEL; k++)
						{
							if (requiredCounts[k] || counts[k] < packs[j].count)//不能和主牌相同，牌数不够也不行 
								continue;
							requiredCounts[k] = packs[j].count;
							break;
						}
					// 将这个可行解放入可行解数组 
					myCombos.push_back(vector<short>(requiredCounts, requiredCounts + MAX_LEVEL + 1));

					/**
					* 对于从牌的选择问题？ 此处是从小到大依次选择。
					* 不拆炸弹、连续性的牌？使选择后留下的牌更连续(权值最大)? 这是以后要考虑的问题。
					*/
				}

			next:
				; // 再增大
			}

		findSol:
			int sumOfSol = myCombos.size();
			if (sumOfSol == 0) //没找到可行解，直接去找炸弹 
				goto failure;

			// 下面对所有可能解进行判断
			{
				vector<short> bestSol;
				if (myPosition != 0 && comboLevel >= 7) // 我是农民，大于等于10点就要出最大的牌压制了 
					bestSol = myCombos[sumOfSol - 1];
				else
					bestSol = myCombos[0];  // 否则(地主或最大的牌比较小)就按顺序出 
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
		}

	failure:
		// 最后看一下能不能炸吧
		for (Level i = 0; i < level_joker; i++)
			if (counts[i] == 4) // 存在炸弹 
			{
				/**
				* 这里的问题就是，如果存在炸弹，是不是一定要炸？
				* 如果有多个炸弹，那应该选哪一个去炸？
				* 地主：能接就接，需要拆就拆；
				* 农民：不打队友，地主出的牌较大的时候直接炸
				*/
				Card bomb[] = { Card(i * 4), Card(i * 4 + 1), Card(i * 4 + 2), Card(i * 4 + 3) };
				if ((myPosition != 0 && getWeight() >= 6) ||
					(myPosition == 0)) // 我是农民，地主牌权值比较大(>=6)才炸;我是地主，能炸就炸！ 
					return CardCombo(bomb, bomb + 4);
			}

		// 有没有火箭？
		if (counts[level_joker] + counts[level_JOKER] == 2)
		{
			/**
			* 和上面炸弹同理，有双王是否一定要炸？还是拆？
			*/
			Card rocket[] = { card_joker, card_JOKER };
			if ((myPosition != 0 && (getWeight() >= 4 || (comboType == CardComboType::PAIR && packs[0].level == 12))) ||
				(myPosition == 0)) // 我是农民，地主出的牌比较大或者如果出的是对子，那么出一对2才炸；我是地主能炸就炸！ 
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

   // SearchCard()的具体实现 
   /**
   * 参数解释：
   * comboType:当前所要搜索的牌型；currentLevel:当前搜到的手牌中的等级；cardType:要搜的牌类型(1为单张2为对子)；
   * already:已经搜到的数量；target:目标数量；tmp:当前考虑的可能牌型；deck:手牌；counts: 某个等级牌的数量
   * beginOfCounts:某等级在手牌中的起始位置；levelOf_:三(四)条的等级(我们要搜的牌不能是这个等级的)
   */
void SearchCard(int& comboType, int currentLevel, short cardType, short already, int& target, vector<short>& tmp, vector<short>& deck, short* counts, short* beginOfCounts, set<Level>& levelOf_)
{
	if (already == target)
	{
		allCombos[comboType].push_back(CardCombo(tmp.begin(), tmp.end()));
		return;
	}
	short off = target - already; //还差多少张
	for (int i = currentLevel; i <= 13 - off; i++)
		// 当前等级的牌必须1.张数>=cardType,2.不能是levelOf_里的
		if (counts[i] >= cardType && !levelOf_.count(i))
		{
			int l = beginOfCounts[i];   // 找到等级i的牌在deck中的位置
			for (int j = 0; j < cardType; j++)
				tmp.push_back(deck[l + j]);
			SearchCard(comboType, i, cardType, already + 1, target, tmp, deck, counts, beginOfCounts, levelOf_);
			for (int j = 0; j < cardType; j++)
				tmp.pop_back(); // 回溯
		}
}

// 我的牌有哪些
set<Card> myCards;

// 地主被明示的牌有哪些
set<Card> landlordPublicCards;

// 大家从最开始到现在都出过什么
vector<vector<Card>> whatTheyPlayed[PLAYER_COUNT];

// 当前要出的牌需要大过什么牌 
CardCombo lastValidCombo;

// 大家还剩多少牌
short cardRemaining[PLAYER_COUNT] = { 20, 17, 17 };


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

	// 做出决策（你只需修改以下部分）

	// findFirstValid 函数可以用作修改的起点
	CardCombo myAction = lastValidCombo.findFirstValid(myCards.begin(), myCards.end());

	// 是合法牌
	assert(myAction.comboType != CardComboType::INVALID);

	assert(
		// 在上家没过牌的时候过牌
		(lastValidCombo.comboType != CardComboType::PASS && myAction.comboType == CardComboType::PASS) ||
		// 在上家没过牌的时候出打得过的牌
		(lastValidCombo.comboType != CardComboType::PASS && lastValidCombo.canBeBeatenBy(myAction)) ||
		// 在上家过牌的时候出合法牌
		(lastValidCombo.comboType == CardComboType::PASS && myAction.comboType != CardComboType::INVALID)
	);

	// 决策结束，输出结果（你只需修改以上部分）

	BotzoneIO::output(myAction.cards.begin(), myAction.cards.end());
}
