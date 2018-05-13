// ��������FightTheLandlord����������
// ���Բ���
// ���ߣ�zhouhy
// ��Ϸ��Ϣ��http://www.botzone.org/games#FightTheLandlord

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

enum class CardComboType
{
	PASS, // ��
	SINGLE, // ����
	PAIR, // ����
	STRAIGHT, // ˳��
	STRAIGHT2, // ˫˳
	TRIPLET, // ����
	TRIPLET1, // ����һ
	TRIPLET2, // ������
	BOMB, // ը��
	QUADRUPLE2, // �Ĵ�����ֻ��
	QUADRUPLE4, // �Ĵ������ԣ�
	PLANE, // �ɻ�
	PLANE1, // �ɻ���С��
	PLANE2, // �ɻ�������
	SSHUTTLE, // ����ɻ�
	SSHUTTLE2, // ����ɻ���С��
	SSHUTTLE4, // ����ɻ�������
	ROCKET, // ���
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
using Card = short;
constexpr Card card_joker = 52;
constexpr Card card_JOKER = 53;

// ������0~53��54��������ʾΨһ���ƣ�
// ���ﻹ����һ����ű�ʾ�ƵĴ�С�����ܻ�ɫ�����Ա�Ƚϣ������ȼ���Level��
// ��Ӧ��ϵ���£�
// 3 4 5 6 7 8 9 10	J Q K	A	2	С��	����
// 0 1 2 3 4 5 6 7	8 9 10	11	12	13	14
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
	 * ��ָ��������Ѱ�� ��һ�� �ܴ����ǰ���������
	 * ��������Ļ�ֻ����һ��
	 * ����������򷵻�һ��PASS������
	 *
	 * �����������Ҫ��Ҫ�޸ĵĵط� 
	 */
	template <typename CARD_ITERATOR> //���뵱ǰҪ����������Ƶĵ����� 
	CardCombo findFirstValid(CARD_ITERATOR begin, CARD_ITERATOR end) const
	{   /**
		 * 1.�������Ƿ���Ҫ���ģ��Ͼ���һ����first��... 
	     */ 
		if (comboType == CardComboType::PASS) // �������Ҫ���˭��ֻ��Ҫ����
		{                                     // ��ǰ��һ������ 
			CARD_ITERATOR second = begin;
			second++;
			return CardCombo(begin, second); // ��ô�ͳ���һ���ơ���
		}    
		/**
		 * 2.�˴�Ϊ���Ʋ��ԣ��������ǵ�����(̰�ķ�)��Ҫ��Ϊ������ũ��(��������һЩ�����ǲ�����Ҫ�õ����涨�壿) 
		 * ���������շ����Ӵ�С���ƣ�ը��������ɻ���������⣻
		 * ũ�񣺾������Ƚ�С�ĸ��ӵ�(Ȩֵ���)�ƣ��������ȵ��������(Ҳ���Ǿ�������������)�� 
		 */ 

		// Ȼ���ȿ�һ���ǲ��ǻ�����ǵĻ��͹�(��Ϊ�򲻹�...) 
		if (comboType == CardComboType::ROCKET)
			return CardCombo();

		// ���ڴ���������дճ�ͬ���͵���(���۵���ORũ��)
		auto deck = vector<Card>(begin, end); // ����
		short counts[MAX_LEVEL + 1] = {};//������Ĺ��캯����ͬ���ģ���ÿ���ȼ��Ƶĸ��� 

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

		// ���򲻶�����ǰ��������ƣ������ܲ����ҵ�ƥ�������
		// ���ƣ���������ռ����Ҫ��λ���ƣ���������һ�����������ƣ�88877764��888777��������
		// ͬ���ؿ��԰�64����Ϊ���ƣ�Ҳ�����Ƹ������� 
		// �ҵ������ƣ�������������ȷ���� 
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
			/**
			 * �������˼�ǣ������ǰ����Ϊ��888777666 
			 * ��ô���Ҫ�����µ��ƣ�999888777/101010999888/.../AAAKKKQQQ
			 * �Ӷ�Ҳ���ǶԵ�ǰ����ȥö��һ����������������������Ϊ1��Ӧ999888777 
			 */ 
			for (Level i = 1; ; i++) // �������
			{
				for (int j = 0; j < mainPackCount; j++)
				{
					int level = packs[j].level + i; // ���Ӻ��Ӧ���� 

					// �����������͵����Ʋ��ܵ�2�����������͵����Ʋ��ܵ�С�������ŵ����Ʋ��ܳ�������
					if ((comboType == CardComboType::SINGLE && level > MAX_LEVEL) ||
						(isSequential && level > MAX_STRAIGHT_LEVEL) ||
						(comboType != CardComboType::SINGLE && !isSequential && level >= level_joker))
						goto failure; //ֻҪ����û�ҵ�����ô����Ҳ�Ϳ϶������ڣ�����ֱ��ȥ��ը�� 

					// ��������������Ʋ������Ͳ��ü�������
					if (counts[level] < packs[j].count)
						goto next;
				}
					
				/**
				 * 4.�������ҵ���һ����ͷ��أ�������ڶ�����أ� ����ǽ��Ƶ�ԭ�� 
				 * ���������պÿ��Խӵ��ƣ���������Ĳ��ԣ� 
				 * ũ�񣺲�����ѣ��㹻��(��ζ���?)��ʱ��ͳ�������һ�֣�����Ͱ��մ˴��Ĳ��ԡ� 
				 */ 
				{
					// �ҵ��˺��ʵ�����(����i)����ô�����أ�
					// ������Ƶ��������������Ǵ��Ƶ��������Ͳ�����Ҳ����
					if (kindCount < packs.size())
						continue;
						 

					// �����ڿ�����
					// ����ÿ���Ƶ�Ҫ����Ŀ��
					short requiredCounts[MAX_LEVEL + 1] = {};
					for (int j = 0; j < mainPackCount; j++)  // ���� 
						requiredCounts[packs[j].level + i] = packs[j].count;
					for (unsigned j = mainPackCount; j < packs.size(); j++) // ���� 
						for (Level k = 0; k <= MAX_LEVEL; k++)
						{
							if (requiredCounts[k] || counts[k] < packs[j].count)//���ܺ�������ͬ����������Ҳ���� 
								continue;
							requiredCounts[k] = packs[j].count;
							break;
						}
					/**
					 * 5.���ڴ��Ƶ�ѡ�����⣿ �˴��Ǵ�С����ѡ�� 
					 * ʵ���ϣ�����ը���������Ե��ƣ�ʹѡ������µ��Ƹ�����(Ȩֵ���)? 
					 */ 


					// ��ʼ������
					vector<Card> solve;
					for (Card c : deck)
					{
						Level level = card2level(c);
						if (requiredCounts[level])
						{
							solve.push_back(c);
							requiredCounts[level]--;
						}
					}
					return CardCombo(solve.begin(), solve.end());
				}

			next:
				; // ������
			}
		}

	failure:
		// ���һ���ܲ���ը��
		for (Level i = 0; i < level_joker; i++)
			if (counts[i] == 4) // ����ը�� 
			{
				/**
				 * 3.�����������ǣ��������ը�����ǲ���һ��Ҫը�� 
				 * ����ж��ը������Ӧ��ѡ��һ��ȥը�� 
				 * �������ܽӾͽӣ���Ҫ��Ͳ�
				 * ũ�񣺲�����ѣ����������ƽϴ��ʱ��ֱ��ը 
				 */ 
				Card bomb[] = { Card(i * 4), Card(i * 4 + 1), Card(i * 4 + 2), Card(i * 4 + 3) };
				return CardCombo(bomb, bomb + 4);
			}

		// ��û�л����
		if (counts[level_joker] + counts[level_JOKER] == 2)
		{
			/**
			 * 3.������ը��ͬ����˫���Ƿ�һ��Ҫը�����ǲ� 
			 */ 
			Card rocket[] = { card_joker, card_JOKER };
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

// �ҵ�������Щ
set<Card> myCards;

// ��������ʾ��������Щ
set<Card> landlordPublicCards;

// ��Ҵ��ʼ�����ڶ�����ʲô
vector<vector<Card>> whatTheyPlayed[PLAYER_COUNT];

// ��ǰҪ��������Ҫ���ʲô�� 
CardCombo lastValidCombo;

// ��һ�ʣ������
short cardRemaining[PLAYER_COUNT] = { 20, 17, 17 };

// ���Ǽ�����ң�0-������1-ũ��ף�2-ũ���ң�
int myPosition;

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
					lastValidCombo = CardCombo(playedCards.begin(), playedCards.end());
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

	// �������ߣ���ֻ���޸����²��֣�

	// findFirstValid �������������޸ĵ����
	CardCombo myAction = lastValidCombo.findFirstValid(myCards.begin(), myCards.end());

	// �ǺϷ���
	assert(myAction.comboType != CardComboType::INVALID);

	assert(
		// ���ϼ�û���Ƶ�ʱ�����
		(lastValidCombo.comboType != CardComboType::PASS && myAction.comboType == CardComboType::PASS) ||
		// ���ϼ�û���Ƶ�ʱ�����ù�����
		(lastValidCombo.comboType != CardComboType::PASS && lastValidCombo.canBeBeatenBy(myAction)) ||
		// ���ϼҹ��Ƶ�ʱ����Ϸ���
		(lastValidCombo.comboType == CardComboType::PASS && myAction.comboType != CardComboType::INVALID)
	);

	// ���߽���������������ֻ���޸����ϲ��֣�

	BotzoneIO::output(myAction.cards.begin(), myAction.cards.end());
}
