from myplayer import DeepQLearning,GetAction,GetStat
import sample
import numpy as np
import random
import json
import copy
from collections import deque
import tensorflow
import ast
import os

def ordinalTransfer(poker):
        newPoker = [int(i/4)+3 for i in poker if i <= 52]
        if 53 in poker:
            newPoker += [17]
        return newPoker

def checkPokerType(poker, hasTransfer): # poker：list，表示一个人出牌的牌型
        poker.sort()
        lenPoker = len(poker)
        newPoker = [i for i in poker]
        if not hasTransfer:
            newPoker = ordinalTransfer(poker)
        typeP, mP, sP = 0, newPoker, []

        for tmp in range(2):
            if tmp == 1:
                return -1, poker, [] # 没有判断出任何牌型，出错
            if lenPoker == 0: # 没有牌，也即pass
                break
            if poker == [52, 53]:
                typeP = 16
                break
            if lenPoker == 4 and newPoker.count(newPoker[0]) == 4:
                typeP = 10
                break
            if lenPoker == 1:
                typeP = 1
                break
            if lenPoker == 2:
                if newPoker.count(newPoker[0]) == 2:
                    typeP = 2
                    break
                continue
                
            firstPoker = newPoker[0]

            if lenPoker >= 5 and 15 not in newPoker:
                singleSeq = [firstPoker+i for i in range(lenPoker)]
                if newPoker == singleSeq:
                    typeP = 6
                    break

            if lenPoker >= 6 and lenPoker % 2 == 0 and 15 not in newPoker:
                pairSeq = [firstPoker+i for i in range(int(lenPoker / 2))]
                pairSeq = [j for j in pairSeq for i in range(2)]
                if newPoker == pairSeq:
                    typeP = 6
                    break

            thirdPoker = newPoker[2]
            if lenPoker <= 5 and newPoker.count(thirdPoker) == 3:
                mP, sP = [thirdPoker for k in range(3)], [k for k in newPoker if k != thirdPoker]
                if lenPoker == 3:
                    typeP = 4
                    break
                if lenPoker == 4:
                    typeP = 4
                    break
                if lenPoker == 5:
                    typeP = 4
                    if sP[0] == sP[1]:
                        break
                    continue

            if lenPoker < 6:
                continue

            fifthPoker = newPoker[4]
            if lenPoker == 6 and newPoker.count(thirdPoker) == 4:
                typeP, mP = 8, [thirdPoker for k in range(4)]
                sP = [k for k in newPoker if k != thirdPoker]
                if sP[0] != sP[1]:
                    break
                continue
            if lenPoker == 8:
                typeP = 8
                mP, sP = [], []
                if newPoker.count(thirdPoker) == 4:
                    mP, sP = [thirdPoker for k in range(4)], [k for k in newPoker if k != thirdPoker]
                elif newPoker.count(fifthPoker) == 4:
                    mP, sP = [fifthPoker for k in range(4)], [k for k in newPoker if k != fifthPoker]
                if len(sP) == 4:
                    if sP[0] == sP[1] and sP[2] == sP[3] and sP[0] != sP[2]:
                        break

            singlePoker = list(set(newPoker)) # 表示newPoker中有哪些牌种
            singlePoker.sort()
            mP, sP = newPoker, []
            dupTime = [newPoker.count(i) for i in singlePoker] # 表示newPoker中每种牌各有几张
            singleDupTime = list(set(dupTime)) # 表示以上牌数的种类
            singleDupTime.sort()

            if len(singleDupTime) == 1 and 15 not in singlePoker: # 不带翼
                lenSinglePoker, firstSP = len(singlePoker), singlePoker[0]
                tmpSinglePoker = [firstSP+i for i in range(lenSinglePoker)]
                if singlePoker == tmpSinglePoker:
                    if singleDupTime == [3]: # 飞机不带翼
                        typeP = 8
                        break
                    if singleDupTime == [4]: # 航天飞机不带翼
                        typeP = 10
                        break

            def takeApartPoker(singleP, newP):
                m = [i for i in singleP if newP.count(i) >= 3]
                s = [i for i in singleP if newP.count(i) < 3]
                return m, s

            m, s = [], []
            if len(singleDupTime) == 2 and singleDupTime[0] < 3 and singleDupTime[1] >= 3:
                c1, c2 = dupTime.count(singleDupTime[0]), dupTime.count(singleDupTime[1])
                if c1 != c2 and not (c1 == 4 and c2 == 2): # 带牌的种类数不匹配
                    continue
                m, s = takeApartPoker(singlePoker, newPoker) # 都是有序的
                if 15 in m:
                    continue
                lenm, firstSP = len(m), m[0]
                tmpm = [firstSP+i for i in range(lenm)]
                if m == tmpm: # [j for j in pairSeq for i in range(2)]
                    m = [j for j in m for i in range(singleDupTime[1])]
                    s = [j for j in s for i in range(singleDupTime[0])]
                    if singleDupTime[1] == 3:
                        if singleDupTime[0] == 1:
                            typeP = 8
                            mP, sP = m, s
                            break
                        if singleDupTime[0] == 2:
                            typeP = 8
                            mP, sP = m, s
                            break
                    elif singleDupTime[1] == 4:
                        if singleDupTime[0] == 1:
                            typeP = 10
                            mP, sP = m, s
                            break
                        if singleDupTime[0] == 2:
                            typeP = 10
                            mP, sP = m, s
                            break
        
        omP, osP = [], []
        for i in poker:
            tmp = int(i/4)+3
            if i == 53:
                tmp = 17
            if tmp in mP:
                omP += [i]
            elif tmp in sP:
                osP += [i]
            else:
                return -1, poker, []
        return typeP, omP, osP

def calculate_rewards(card_combo):
    typeP,_,_=checkPokerType(card_combo,0)
    punish=0
    for _, card in enumerate(card_combo):
        punish+=int(card/4)*0.1
    reward=typeP/100-punish
    return reward

def init_game():
    all_card=[i for i in range(54)]
    random.shuffle(all_card)
    random.shuffle(all_card)
    card_0=all_card[0:17]
    card_1=all_card[17:34]
    card_2=all_card[34:51]
    card_public=all_card[51:]
    json_0={"requests":[{"own":card_0+card_public,"publiccard":card_public}],"responses":[]}
    json_1={"requests":[{"own":card_1,"publiccard":card_public}],"responses":[]}
    json_2={"requests":[{"own":card_2,"publiccard":card_public}],"responses":[]}
    json_list=[json_0,json_1,json_2]
    return json_list

def deal_response(json,my_comb,next_comb,prev_comb):
    temp={"history":[next_comb,prev_comb]}
    if len(json["requests"][0])==2:
        json["requests"][0]["history"]=[next_comb,prev_comb]
    else:
        json["responses"].append(my_comb)
        json["requests"].append(temp)
    return json 

def match(bot,round=10):
    win_rate=0
    buffer=deque()
    for _ in range(round):
        json_list=init_game()
        card_count=[20,17,17]
        my_comb,next_comb,prev_comb=[],[],[]
        index=0
        stat_for_train,action_for_train,now_player=[],[],[]
        while 0 not in card_count:
            json_list[index]=deal_response(json_list[index],my_comb,next_comb,prev_comb)
            now_player.append(index)
            temp=copy.deepcopy(json_list[index])
            my_comb=next_comb
            next_comb=prev_comb
            if index==0:
                stat=GetStat(temp)
                stat_for_train.append(stat)
                temp=copy.deepcopy(stat)
                action=GetAction(temp)
                bot.receive_stat(temp)
                prev_comb=bot.take_action(action.res)
            else:
                prev_comb=sample.payer(temp)
            action_for_train.append(prev_comb)
            card_count[index]-=len(prev_comb)
            index=(index+1)%3
        rewards=np.zeros(len(now_player))
        time=len(now_player)
        for i in range(time):
            if index==0:
                win_rate+=1
                rewards[np.array(now_player) == 0]=2.0*int(i/3)/int(time/3)
                rewards[np.array(now_player) !=0]=0
            else:
                rewards[np.array(now_player) != 0]=2.0*int(i/3)/int(time/3)
                rewards[np.array(now_player) ==0]=0
        for i in range(len(now_player)):
            rewards[i]+=calculate_rewards(action_for_train[i])
    buffer.extend(zip(stat_for_train,action_for_train,rewards))
    print("与样例比较：")
    print(win_rate/round)
    return buffer

def train(round=10):
    #bot=DeepQLearning()
    bot.random=True
    buffer=match(bot,round)
    bot.update_model(buffer)

def test():
    bot.random='false'
    with tensorflow.Graph().as_default() as net2_graph:
        bot_raw=DeepQLearning()
    rate=0
    bot_list=[bot_raw,bot,bot]
    for _ in range(10):
        json_list=init_game()
        json_test_list=copy.deepcopy(json_list)
        card_count=[20,17,17]
        my_comb,next_comb,prev_comb=[],[],[]
        index=0
        while 0 not in card_count:
            json_list[index]=deal_response(json_list[index],my_comb,next_comb,prev_comb)
            temp=copy.deepcopy(json_list[index])
            stat=GetStat(temp)
            temp=copy.deepcopy(stat)
            action=GetAction(temp)
            bot_list[index].receive_stat(temp)
            my_comb=next_comb
            next_comb=prev_comb
            prev_comb=bot_list[index].take_action(action.res)
            card_count[index]-=len(prev_comb)
            index=(index+1)%3
        if index==0:
            rate+=1
    bot_list=[bot,bot_raw,bot_raw]
    for _ in range(10):
        json_list=copy.deepcopy(json_test_list)
        card_count=[20,17,17]
        my_comb,next_comb,prev_comb=[],[],[]
        index=0
        while 0 not in card_count:
            json_list[index]=deal_response(json_list[index],my_comb,next_comb,prev_comb)
            temp=copy.deepcopy(json_list[index])
            stat=GetStat(temp)
            temp=copy.deepcopy(stat)
            action=GetAction(temp)
            bot_list[index].receive_stat(temp)
            my_comb=next_comb
            next_comb=prev_comb
            prev_comb=bot_list[index].take_action(action.res)
            card_count[index]-=len(prev_comb)
            index=(index+1)%3
        if index!=0:
            rate+=1 
    rate=rate/20
    print(rate)
    if(rate>=0.55):
        bot.save()

def train_with_history(file_name):
    buffer=deque()
    with open(file=file_name,mode='r',encoding='utf-8') as f:
        for temp in f.readlines():
            temp=temp.replace('false','\'false\'')
            temp=temp.replace('true','\'true\'')
            history_card=[]
            temp=eval(temp)
            history=temp['log']
            if 'errorInfo' in history[-1]['output']['display']:
                continue
            stat_for_train,action_for_train,now_player=[],[],[]
            mark=0
            public_card=[]
            own_card=[[],[],[]]
            for index,iter in enumerate(history):
                if index%2==0:
                    if iter['output']['command']=='finish':
                        break
                    my_id=int((index/2)%3)
                    now_player.append(my_id)
                    my_history=[]
                    for i,history_temp in enumerate(history_card):
                        if i%3==my_id:
                            my_history.append(history_temp)
                    other_history=[]
                    if 'own' in iter['output']['content'][str(my_id)]:
                        own_card[my_id]=(iter['output']['content'][str(my_id)]['own'])
                    own_temp=copy.deepcopy(own_card[my_id])
                    if 'publiccard' in iter['output']['content'][str(my_id)]:
                        public_card=iter['output']['content'][str(my_id)]['publiccard']
                    other_history.append(iter['output']['content'][str(my_id)]['history'][0])
                    other_history.append(iter['output']['content'][str(my_id)]['history'][1])
                    temp_data={'requests':[{'own':own_temp,'publiccard':public_card,'history':other_history}],'responses':my_history}
                    stat_for_train.append(GetStat(temp_data))
                else:
                    my_id=int(((index-1)/2)%3)
                    if (iter[str(my_id)]['verdict']=='OK'):
                        action_for_train.append(iter[str(my_id)]['response'])
                        history_card.append(iter[str(my_id)]['response'])
                    else:
                        mark=1
            if mark==1:
                continue
            rewards=np.zeros(len(now_player))
            index=temp['scores'][1]
            if index==0:
                rewards[np.array(now_player) == 0]=2.0
                rewards[np.array(now_player) !=0]=0
            else:
                rewards[np.array(now_player) != 0]=2.0
                rewards[np.array(now_player) ==0]=0
            for i in range(len(now_player)):
                rewards[i]+=calculate_rewards(action_for_train[i])
            buffer.extend(zip(stat_for_train,action_for_train,rewards))
    bot.update_model(buffer)

def main():
    i=1
    for data_file in os.listdir('/home/guoxiaobo96/data'):
        print(data_file)
        i+=1
        data_file=os.path.join('/home/guoxiaobo96/data',data_file)
        train_with_history(data_file)
        test()

bot=DeepQLearning()
main()

    