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
vector<CardCombo> allCombos[20]; // ��ǰ���Ƶ����п������ 
const int level2value[15]={-7,-6,-5,-4,-3,-2,-1,0,1,2,3,4,5,6,7};

void SearchCard(vector<CardCombo>&,int&, int, short, short, int&, vector<short>&, vector<short>&, short*, short*, set<short>&);
void findAllCombos(vector<Card>&,vector<CardCombo>*); 
void findSeq(vector<Card>&,vector<CardCombo>&);

enum class CardComboType
{
	PASS, // ��
	SINGLE, // ����      1
	PAIR, // ����        2
	STRAIGHT, // ˳��    3
	STRAIGHT2, // ˫˳   4
	TRIPLET, // ����     5
	TRIPLET1, // ����һ  6
	TRIPLET2, // ������  7
	BOMB, // ը��        8
	QUADRUPLE2, // �Ĵ�����ֻ��   9
	QUADRUPLE4, // �Ĵ������ԣ�   10
	PLANE, // �ɻ�                11
	PLANE1, // �ɻ���С��         12
	PLANE2, // �ɻ�������         13
	SSHUTTLE, // ����ɻ�         14
	SSHUTTLE2, // ����ɻ���С��  15
	SSHUTTLE4, // ����ɻ�������  16
	ROCKET, // ���               17
	INVALID // �Ƿ�����
};

int cardComboScores[] = {
	0, // ��    
	1, // ����   
	2, // ����
	6, // ˳��
	6, // ˫˳
	4, // ����
	4, // ����һ
	4, // ������
	10, // ը��
	8, // �Ĵ�����ֻ��
	8, // �Ĵ������ԣ�
	8, // �ɻ�
	8, // �ɻ���С��
	8, // �ɻ�������
	10, // ����ɻ�����Ҫ���У�����Ϊ10�֣�����Ϊ20�֣�
	10, // ����ɻ���С��
	10, // ����ɻ�������
	16, // ���
	0 // �Ƿ�����
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

// ��0~53��54��������ʾΨһ��һ����
constexpr Card card_joker = 52;
constexpr Card card_JOKER = 53;

// ������0~53��54��������ʾΨһ���ƣ�
// ���ﻹ����һ����ű�ʾ�ƵĴ�С�����ܻ�ɫ�����Ա�Ƚϣ������ȼ���Level��
// ��Ӧ��ϵ���£�
// 3 4 5 6 7 8 9 10	J Q K	A	2	С��	����
// 0 1 2 3 4 5 6 7	8 9 10	11	12	13	    14
using Level = short;
constexpr Level MAX_LEVEL = 15;
constexpr Level MAX_STRAIGHT_LEVEL = 11;
constexpr Level level_joker = 13;
constexpr Level level_JOKER = 14;

/**
* ��Card���Level
*/
constexpr Level card2level(Card card)
{
	return card / 4 + card / 53;
}

// �Ƶ���ϣ����ڼ�������
struct CardCombo
{
	// ��ʾͬ�ȼ������ж�����
	// �ᰴ�����Ӵ�С���ȼ��Ӵ�С����
	struct CardPack
	{
		Level level;
		short count;

		bool operator< (const CardPack& b) const
		{
			if (count == b.count)
				return level > b.level;   //������ͬ��ȼ������ǰ�� 
			return count > b.count;       //������������
		}
	};
	vector<Card> cards; // ԭʼ���ƣ�δ����
	vector<CardPack> packs; // ����Ŀ�ʹ�С���������
	CardComboType comboType; // ���������
	Level comboLevel = 0; // ����Ĵ�С��

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
	* ����������CardPack�ݼ��˼���, �磺887766 ==> findMaxSeq=3; 88877734 ==> findMaxSeq=2
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
	* �������������ֵܷ�ʱ���Ȩ��
	*/
	int getWeight() const
	{
		// ����ɻ���Ҫ�����ж� 
		if (comboType == CardComboType::SSHUTTLE ||
			comboType == CardComboType::SSHUTTLE2 ||
			comboType == CardComboType::SSHUTTLE4)
			return cardComboScores[(int)comboType] + (findMaxSeq() > 2) * 10;
		// ��������ֱ�ӵõ�Ȩֵ 
		return cardComboScores[(int)comboType];
	}

	// ����һ��������
	CardCombo() : comboType(CardComboType::PASS) {}

	/**
	* ͨ��Card����short�����͵ĵ���������һ������
	* ����������ͺʹ�С���
	* ��������û���ظ����֣����ظ���Card��
	*/

	// ģ�幹�캯����ͨ��CARD�������������ǰ���� 
	template <typename CARD_ITERATOR>
	CardCombo(CARD_ITERATOR begin, CARD_ITERATOR end)
	{
		// ���У���
		if (begin == end) //���û���Ƴ��˾�PASS 
		{
			comboType = CardComboType::PASS;
			return;
		}

		// ÿ�����ж��ٸ���counts[i]��ʾ��i���ȼ��Ƶ����� 
		short counts[MAX_LEVEL + 1] = {};

		// ͬ���Ƶ��������ж��ٸ����š����ӡ���������������Щ��Combo�Ļ���Ԫ�أ�
		// countsOfCount[i]��ʾ��i��(���š�����...)�Ƶ����� 
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

		// ����ࡢ�ȼ��������������ǿ��ԱȽϴ�С��
		comboLevel = packs[0].level;

		// ��������
		// ���� ͬ���Ƶ����� �м��� ���з���
		vector<int> kindOfCountOfCount;
		for (int i = 0; i <= 4; i++)
			if (countOfCount[i])
				kindOfCountOfCount.push_back(i);
		sort(kindOfCountOfCount.begin(), kindOfCountOfCount.end());

		int curr, lesser;

		switch (kindOfCountOfCount.size())
		{
		case 1: // ֻ��һ����(���ǵ���/����/����/����) 
			curr = countOfCount[kindOfCountOfCount[0]]; //�����Ƶ�����(���š�����...) 
			switch (kindOfCountOfCount[0])
			{
			case 1: //������Ϊ ���� 
				if (curr == 1)  //ֻ��һ�ţ�����ǰ����Ϊ _����_ 
				{
					comboType = CardComboType::SINGLE;
					return;
				}
				if (curr == 2 && packs[1].level == level_joker) //�����ţ�����һ��ΪС�������ȻΪ_���_ 
				{
					comboType = CardComboType::ROCKET;
					return;
				}
				if (curr >= 5 && findMaxSeq() == curr &&  //�ж���(>=5)������_˳��_ 
					packs.rbegin()->level <= MAX_STRAIGHT_LEVEL)
				{
					comboType = CardComboType::STRAIGHT;
					return;
				}
				break;
			case 2: //������Ϊ ���� 
				if (curr == 1)  //������ _����_ 
				{
					comboType = CardComboType::PAIR;
					return;
				}
				if (curr >= 3 && findMaxSeq() == curr && //������ _����_ 
					packs.rbegin()->level <= MAX_STRAIGHT_LEVEL)
				{
					comboType = CardComboType::STRAIGHT2;
					return;
				}
				break;
			case 3: //������Ϊ���� 
				if (curr == 1)
				{
					comboType = CardComboType::TRIPLET;  //������_������_ 
					return;
				}
				if (findMaxSeq() == curr &&
					packs.rbegin()->level <= MAX_STRAIGHT_LEVEL) //����ֻ��һ���ƣ�����_�ɻ�������_ 
				{
					comboType = CardComboType::PLANE;
					return;
				}
				break;
			case 4: // ������Ϊ���� 
				if (curr == 1)  //ֻ��һ����˵��������_ը��_ 
				{
					comboType = CardComboType::BOMB;
					return;
				}
				if (findMaxSeq() == curr &&  //ֻ��һ���ƣ�����Ϊ_����ɻ�������_ 
					packs.rbegin()->level <= MAX_STRAIGHT_LEVEL)
				{
					comboType = CardComboType::SSHUTTLE;
					return;
				}
			}
			break;
		case 2: // �������ƣ���ʱ�Ͳ����ڵ��š����ӡ�˳���ˣ�ֻ���� ����?���Ĵ�?����� 
			curr = countOfCount[kindOfCountOfCount[1]];   //���ָ���(����OR����)����һ���Ƶ����� 
			lesser = countOfCount[kindOfCountOfCount[0]]; //���ָ�С(����OR����)����һ���Ƶ����� 
			if (kindOfCountOfCount[1] == 3)  //�������һ����Ϊ���� 
			{
				// ��С����һ����Ϊ���ţ�˵��������_����1_ 
				if (kindOfCountOfCount[0] == 1)
				{
					// ����һ
					if (curr == 1 && lesser == 1) //������ֻ��1��ֱ������1 
					{
						comboType = CardComboType::TRIPLET1;
						return;
					}
					if (findMaxSeq() == curr && lesser == curr &&  //������ֹһ������_�ɻ���С��_ 
						packs.rbegin()->level <= MAX_STRAIGHT_LEVEL)
					{
						comboType = CardComboType::PLANE1;
						return;
					}
				}
				// ��С����һ����Ϊ���ӣ�����2 
				if (kindOfCountOfCount[0] == 2)
				{
					// ������
					if (curr == 1 && lesser == 1)  //������Ϊ_����2_ 
					{
						comboType = CardComboType::TRIPLET2;
						return;
					}
					if (findMaxSeq() == curr && lesser == curr &&  // _�ɻ�������_ 
						packs.rbegin()->level <= MAX_STRAIGHT_LEVEL)
					{
						comboType = CardComboType::PLANE2;
						return;
					}
				}
			}
			// �������һ����Ϊ ���� 
			if (kindOfCountOfCount[1] == 4)
			{
				// ��С����һ����Ϊ���ţ���������
				if (kindOfCountOfCount[0] == 1)
				{
					// ��������ֻ * n
					if (curr == 1 && lesser == 2)  //���������ţ�����Ϊ_�Ĵ�2_ 
					{
						comboType = CardComboType::QUADRUPLE2;
						return;
					}
					if (findMaxSeq() == curr && lesser == curr * 2 &&  //�����ķɻ�����Ϊ_����ɻ���С��_ 
						packs.rbegin()->level <= MAX_STRAIGHT_LEVEL)
					{
						comboType = CardComboType::SSHUTTLE2;
						return;
					}
				}
				// ��С����һ����Ϊ���� 
				if (kindOfCountOfCount[0] == 2)
				{
					// ���������� * n
					if (curr == 1 && lesser == 2)  // ���������ӣ�_�Ĵ�����_ 
					{
						comboType = CardComboType::QUADRUPLE4;
						return;
					}
					if (findMaxSeq() == curr && lesser == curr * 2 && //�������֣���Ϊ_����ɻ�������_ 
						packs.rbegin()->level <= MAX_STRAIGHT_LEVEL)
					{
						comboType = CardComboType::SSHUTTLE4;
						return;
					}
				}
			}
		}

		comboType = CardComboType::INVALID; //��������Ч 
	}

	/**
	* �ж�ָ�������ܷ�����ǰ���飨������������ǹ��Ƶ��������
	*/
	bool canBeBeatenBy(const CardCombo& b) const
	{
		if (comboType == CardComboType::INVALID || b.comboType == CardComboType::INVALID)
			return false;
		if (b.comboType == CardComboType::ROCKET)  //˫���ܴ���κ��� 
			return true;
		if (b.comboType == CardComboType::BOMB)    //ը���ͱȴ�С 
			switch (comboType)
			{
			case CardComboType::ROCKET:
				return false;
			case CardComboType::BOMB:
				return b.comboLevel > comboLevel;
			default:
				return true;
			}
		// �������Ҫ�Ƚϴ�С���룺(1).������ͬ��(2).��������ͬ 
		return b.comboType == comboType && b.cards.size() == cards.size() && b.comboLevel > comboLevel;
	}
	/**
	* �˴�Ϊ���Ʋ���
	*/
	template <typename CARD_ITERATOR>
	CardCombo myGivenCombo(CARD_ITERATOR begin, CARD_ITERATOR end) const
	{
		auto deck=vector<Card>(begin,end);
		vector<CardCombo> allCombos[20];
		// �����ҵ����п��ܵ����
		findAllCombos(deck, allCombos);

		// �ȿ���һ������������Է�(�����Լ�������һ��)ֻ��һ�����ˣ�
		// �Ҿ�ֻ�ܳ�����>=2���ƣ���ֻ�е��ţ���ֻ�ܴӴ��С
		if ((myPosition != 0 && cardRemaining[0] == 1) ||
			(myPosition == 0 && (cardRemaining[1] == 1 || cardRemaining[2] == 1)))
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
		// ���濼������������Ȱ�������
		for (int i = 1; i < 18; i++)
			sort(allCombos[i].begin(), allCombos[i].end());

		short counts[MAX_LEVEL + 1] = {};
		for (Card c : deck)
			counts[card2level(c)]++;
		short need[19] = { 0,1,2,5,6,3,4,5,4,6,8,6,8,10,8,12,16,2 }; // ÿ��������Ҫ������
		int order[19] = { 0,4,13,12,11,6,7,5,3,1,2,8,10,9,17,14,15,16 };

		// ˫������������������
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
					// ����x���ɻ���x������ɻ���x�������Ʋ�Ҫ���� Q
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
					if (deck.size() - need[order[i]] <= 1)
						return allCombos[order[i]][0];
					else
					{
						for (int j = 0; j < Size; j++)
							if (allCombos[order[i]][j].comboLevel < 9)
								return allCombos[order[i]][j];
					}
					break;
				}
				// ���濼�ǵ��� 
				case 1:
				{
					short firstPair = -1;
					short firstSingle = -1;
					for (int j = 0; j < Size; j++)
					{
						// �����������,�������С�ĵ��Ŷ��Ƚϴ�(>Q),�ͱ��� 
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
					// �������С�ĵ��Ŷ�>Q�ˣ�������С�Ķ���
					// �����С�Ķ���<Q������û�е����ĵ��ţ��͵���һ��������
					// �����ǳ����� 
					if ((firstPair < 9 && firstPair != -1) ||
						firstSingle == -1) break;
					else
						return allCombos[1][firstSingle];
					break;
				}
				default: // �����ľ����γ����� 
					return allCombos[order[i]][0];
				}
			}
		}
		return CardCombo();
	}

	// ����Ϊ���Ʋ��� 
	template <typename CARD_ITERATOR> //���뵱ǰҪ����������Ƶĵ����� 
	CardCombo findFirstValid(CARD_ITERATOR begin, CARD_ITERATOR end) const
	{
		if (comboType == CardComboType::PASS) // �������Ҫ���˭�����ֵ��Լ����� 
			return myGivenCombo(begin, end);
		
		// �ϼ���ũ�񣬲�����ѵ������ 
		if (myPosition != 0 &&
			lastPlayer != 0 &&
			(comboLevel >= 8||
			 (int)comboType >= 5||
			 (int)comboType == 4||
			 ((int)comboType == 3 && cards.size() >= 7)))
			return CardCombo();

		// �����Ǹ��ҽ��� 
		// Ȼ���ȿ�һ���ǲ��ǻ�����ǵĻ��͹�
		if (comboType == CardComboType::ROCKET)
			return CardCombo();

		// ���ڴ���������дճ�ͬ���͵���(���۵���ORũ��)
		auto deck = vector<Card>(begin, end); // ����
		short counts[MAX_LEVEL + 1] = {};//ÿ���ȼ��Ƶĸ��� 

		unsigned short kindCount = 0;

		// ����һ��������ÿ�����ж��ٸ�
		for (Card c : deck)
			counts[card2level(c)]++;

		// ������������ã�ֱ�Ӳ��ô��ˣ������ܲ���ը��
		if (deck.size() < cards.size())
			goto failure;

		// ����һ���������ж�������
		for (short c : counts)
			if (c)
				kindCount++;

		{
			// ��ʼ��������
			int mainPackCount = findMaxSeq();
			// �����Ƿ�����������(˳�ӡ����ԡ��ɻ�������ɻ�)��Ҳ�����ǳ��ĵ��š�һ�ԡ��� 
			bool isSequential =
				comboType == CardComboType::STRAIGHT ||
				comboType == CardComboType::STRAIGHT2 ||
				comboType == CardComboType::PLANE ||
				comboType == CardComboType::PLANE1 ||
				comboType == CardComboType::PLANE2 ||
				comboType == CardComboType::SSHUTTLE ||
				comboType == CardComboType::SSHUTTLE2 ||
				comboType == CardComboType::SSHUTTLE4;

			// �����������������п��н�
			vector<vector<short> > myCombos;

			for (Level i = 1; ; i++) // �������
			{
				for (int j = 0; j < mainPackCount; j++)
				{
					int level = packs[j].level + i; // ���Ӻ��Ӧ���� 
					if ((comboType == CardComboType::SINGLE && level > MAX_LEVEL) ||
						(isSequential && level > MAX_STRAIGHT_LEVEL) ||
						(comboType != CardComboType::SINGLE && !isSequential && level >= level_joker))
						goto findSol;

					// ��������������Ʋ������Ͳ��ü�������
					if (counts[level] < packs[j].count)
						goto next;
				}

				{
					// ������Ƶ��������������Ǵ��Ƶ��������Ͳ�����Ҳ����
					if (kindCount < packs.size())
						continue;


					// �����ڿ�����
					// ����ÿ���Ƶ�Ҫ����Ŀ��
					short requiredCounts[MAX_LEVEL + 1] = {};
					for (int j = 0; j < mainPackCount; j++)  // ���� 
						requiredCounts[packs[j].level + i] = packs[j].count;

					unsigned int currentHave = 0; // ������ǰҪ��Ĵ����Ƿ��㹻 
					for (unsigned j = mainPackCount; j < packs.size(); j++) // ���ƣ������δӵ�ǰ������С��ѡ��
						for (Level k = 0; k <= MAX_LEVEL; k++)
						{
							if (requiredCounts[k] || counts[k] < packs[j].count)//���ܺ�������ͬ����������Ҳ���� 
								continue;
							requiredCounts[k] = packs[j].count;
							currentHave++;
							break;
						}
					// ������ԵĻ�����������н������н����� 
					if (currentHave == packs.size() - mainPackCount)
						myCombos.push_back(vector<short>(requiredCounts, requiredCounts + MAX_LEVEL + 1));
				}

			next:
				; // ������
			}

		findSol:
			int sumOfSol = myCombos.size();
			if (!sumOfSol) //û�ҵ����н⣬ֱ��ȥ��ը�� 
				goto failure;

			vector<short> bestSol;
		// ��������Ҫ��myCombos���ҳ�����˳�ӵ���Щ�� 
			int best=0;
			vector<vector<short> > tmp_combo;
			for(int i=0;i<sumOfSol;i++) 
			{
				vector<short> current_combo=myCombos[i];// ע������Ҫ����һ�� 
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
				int haveSingleSol = 0; // �ǵ��Ƶ����� 
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
				if (haveSingleSol) // ����е��� 
				{
					sumOfSol = haveSingleSol; // �������ķǵ��ƴ�myCombos��ȥ�� 
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
				
			// ��ʼ������
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
		// ���һ���ܲ���ը��
		for (Level i = 0; i < level_joker; i++)
			if (counts[i] == 4) // ����ը�� 
			{
				Card bomb[] = { Card(i * 4), Card(i * 4 + 1), Card(i * 4 + 2), Card(i * 4 + 3) };
				//�ϴγ��Ƶ��ǵ��������Ĳ��ǵ��ţ��ұ��γ��Ƶ�����>=8�����߳���һ�Զ�(������)����ը��
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

		// ��û�л����
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
		std::cout << "��" << cardComboStrings[(int)comboType] <<
			"��" << cards.size() << "�ţ���С��" << comboLevel << "��";
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
	short off = target - already; //���������
	for (int i = currentLevel; i < 12 - off; i++)
		// ��ǰ�ȼ����Ʊ���1.����>=cardType,2.������levelOf_���
		if (counts[i] == cardType && !levelOf_.count(i))
		{
			int l = beginOfCounts[i];   // �ҵ��ȼ�i������deck�е�λ��
			for (int j = 0; j < cardType; j++)
				tmp.push_back(deck[l + j]);
			SearchCard(Combos,comboType, i + 1, cardType, already + 1, target, tmp, deck, counts, beginOfCounts, levelOf_);
			for (int j = 0; j < cardType; j++)
				tmp.pop_back(); // ����
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
		short beginOfCounts[MAX_LEVEL + 1] = {}; //  ÿ���ȼ����������е���ʼλ��
		int currentType;   // ��ǰö������ 
		int Size = deck.size();// ��ǰ��������

		sort(deck.begin(), deck.end()); //���Ÿ�����˵��... 
		for (Card c : deck)
			counts[card2level(c)]++;  // ��һ�¸��ȼ������ж���

		for (int i = 1; i <= 14; i++)
			beginOfCounts[i] = beginOfCounts[i - 1] + counts[i - 1];

		// ���濪ʼö�٣���Ϊͬ�ȼ�������û������ģ�����ֻ���Ǹõȼ���ǰk��
		// ����ö�ٵ���(������С��)
		currentType = (int)CardComboType::SINGLE;
		for (int i = 0; i <= 14; i++)
			if (counts[i])
			{
				vector<Card> tmp;
				tmp.push_back(deck[beginOfCounts[i]]);
				Combos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
			}

		// ����ö�ٶ���
		currentType = (int)CardComboType::PAIR;
		for (int i = 0; i <13; i++) // ��ൽһ��2
			if (counts[i] == 2)
			{
				vector<Card> tmp;
				int l = beginOfCounts[i];  // �ȼ�i������deck�е���ʼλ��l
				tmp.push_back(deck[l]);
				tmp.push_back(deck[l + 1]);
				Combos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
			}

		// ����ö�ٵ�˳
		currentType = (int)CardComboType::STRAIGHT;
		for (int l = 0; l < 8; l++) // ö��˳�����
			for (int r = l + 4; r < 12; r++) // ö��˳���յ�
			{
				int haveSeq = -1;
				for (int i = l; i <= r; i++)
					if (!counts[i])
					{
						haveSeq = i; // ��haveSeq����� 
						break;
					}
				if (haveSeq != -1) // ��[l,r]�м�ĳ���������ˣ�r������Ҳ��������
				{
					l = haveSeq;
					break;
				}
				vector<Card> tmp;
				for (int i = l; i <= r; i++) // ÿ��ѡ��һ�ž�����
					tmp.push_back(deck[beginOfCounts[i]]);
				Combos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
			}

		// ����ö��˫˳
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

		// ����ö������
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

		// ����ö��ը��
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

		// ����ö������ƣ�ֱ����������������������Ƽ��ɣ�Ϊ����Ҫ�ȵó�ÿ�����͵�����
		int Triple = (int)CardComboType::TRIPLET;
		int TripleSize = Combos[Triple].size();
		int Quad = (int)CardComboType::BOMB;
		int QuadSize = Combos[Quad].size();

		// ����ö������һ��
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

		// ����ö��������(һ��)
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

		// ����ö���Ĵ�������ֱ��������ö�ٵ�ը��
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

		// ����ö���Ĵ�����
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

		// ����ö�ٷɻ�������
		currentType = (int)CardComboType::PLANE;
		int lastLevel;
		for (int i = 0; i < TripleSize; i++)
		{
			lastLevel = Combos[Triple][i].comboLevel;
			vector<Card> tmp = Combos[Triple][i].cards;
			for (int j = i + 1; j < TripleSize; j++)
			{
				int currentLevel = Combos[Triple][j].comboLevel;
				if (lastLevel + 1 == currentLevel && currentLevel != 12) // ������2
				{
					lastLevel = currentLevel;
					tmp.insert(tmp.end(), Combos[Triple][j].cards.begin(), Combos[Triple][j].cards.end());
					Combos[currentType].push_back(CardCombo(tmp.begin(), tmp.end()));
				}
				else
					break;
			}
		}

		// ����ö�ٺ���ɻ�������
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

		// ����ö�ٷɻ���С��
		currentType = (int)CardComboType::PLANE1;
		for (int i = 0; i < PlaneSize; i++)
		{
			vector<Card> tmp = Combos[Plane][i].cards;
			set<Level> levels;   // ��ǰ�ɻ������������ĵȼ�
			for (Card c : tmp)
				levels.insert(card2level(c));
			int levelCount = levels.size(); // һ����Ҫ��ô���ŵ���
			SearchCard(Combos,currentType, 0, 1, 0, levelCount, tmp, deck, counts, beginOfCounts, levels);
		}

		// ����ö�ٷɻ�������
		currentType = (int)CardComboType::PLANE2;
		for (int i = 0; i < PlaneSize; i++)
		{
			vector<Card> tmp = Combos[Plane][i].cards;
			set<Level> levels;
			for (Card c : tmp)
				levels.insert(card2level(c));
			int levelCount = levels.size(); // һ����Ҫ��ô�����
			SearchCard(Combos,currentType, 0, 2, 0, levelCount, tmp, deck, counts, beginOfCounts, levels);
		}

		// ����ö�ٺ���ɻ���С��
		currentType = (int)CardComboType::SSHUTTLE2;
		for (int i = 0; i < SShuttleSize; i++)
		{
			vector<Card> tmp = Combos[SShuttle][i].cards;
			set<Level> levels;   // ��ǰ����ɻ������������ĵȼ�
			for (Card c : tmp)
				levels.insert(card2level(c));
			int levelCount = 2 * levels.size();
			SearchCard(Combos,currentType, 0, 1, 0, levelCount, tmp, deck, counts, beginOfCounts, levels);
		}

		// ����ö�ٺ���ɻ�������
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

		// ���濼�ǻ��
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
		// �������루ƽ̨�ϵ������ǵ��У�
		string line;
		getline(cin, line);
		Json::Value input;
		Json::Reader reader;
		reader.parse(line, input);

		// ���ȴ����һ�غϣ���֪�Լ���˭������Щ��
		{
			auto firstRequest = input["requests"][0u]; // �±���Ҫ�� unsigned������ͨ�������ֺ����u������
			auto own = firstRequest["own"];
			auto llpublic = firstRequest["public"];
			auto history = firstRequest["history"];
			for (unsigned i = 0; i < own.size(); i++)  //�ҵ��� 
				myCards.insert(own[i].asInt());
			for (unsigned i = 0; i < llpublic.size(); i++) // ������������������ 
				landlordPublicCards.insert(llpublic[i].asInt());
			if (history[0u].size() == 0)
				if (history[1].size() == 0)
					myPosition = 0; // ���ϼҺ��ϼҶ�û���ƣ�˵���ǵ���
				else
					myPosition = 1; // ���ϼ�û���ƣ������ϼҳ����ˣ�˵����ũ���
			else
				myPosition = 2; // ���ϼҳ����ˣ�˵����ũ����
		}

		// history���һ����ϼң��͵ڶ���ϼң��ֱ���˭�ľ���
		int whoInHistory[] = { (myPosition - 2 + PLAYER_COUNT) % PLAYER_COUNT, (myPosition - 1 + PLAYER_COUNT) % PLAYER_COUNT };

		int turn = input["requests"].size();
		for (int i = 0; i < turn; i++)
		{
			// ��λָ����浽��ǰ
			auto history = input["requests"][i]["history"]; // ÿ����ʷ�����ϼҺ����ϼҳ�����
			int howManyPass = 0;
			for (int p = 0; p < 2; p++)
			{
				int player = whoInHistory[p]; // ��˭������
				auto playerAction = history[p]; // ������Щ��
				vector<Card> playedCards;
				for (unsigned _ = 0; _ < playerAction.size(); _++) // ѭ��ö������˳���������
				{
					int card = playerAction[_].asInt(); // �����ǳ���һ����
					playedCards.push_back(card);
				}
				whatTheyPlayed[player].push_back(playedCards); // ��¼�����ʷ
				cardRemaining[player] -= playerAction.size();

				if (playerAction.size() == 0)
					howManyPass++;
				else
				{
					lastValidCombo = CardCombo(playedCards.begin(), playedCards.end());
					lastPlayer = player;  // ������һ�γ������ 
				}

			}

			if (howManyPass == 2)  //�ϼҺ����ϼҶ�PASS�� 
				lastValidCombo = CardCombo();

			if (i < turn - 1)
			{
				// ��Ҫ�ָ��Լ�������������
				auto playerAction = input["responses"][i]; // ������Щ��
				vector<Card> playedCards;
				for (unsigned _ = 0; _ < playerAction.size(); _++) // ѭ��ö���Լ�����������
				{
					int card = playerAction[_].asInt(); // �������Լ�����һ����
					myCards.erase(card); // ���Լ�������ɾ��
					playedCards.push_back(card);
				}
				whatTheyPlayed[myPosition].push_back(playedCards); // ��¼�����ʷ
				cardRemaining[myPosition] -= playerAction.size();
			}
		}
	} // end input() 

	  /**
	  * ������ߣ�begin�ǵ�������㣬end�ǵ������յ�
	  * CARD_ITERATOR��Card����short�����͵ĵ�����
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
