import tensorflow as tf
import numpy as np
import json 
import random
from collections import deque 

class DeepQLearning():
    def __init__(self):
        self.replay_memory=deque()
        self.stat_dim=8
        self.card_number=15  #0-12加上只给大小王用的13、14
        self.card_colour=5   #大小王算一种单独的花色，分别在13、14位置
        self.action_dim=1
        self.qValue=self.creat_network()
        self.session=tf.Session()
        self.session.run(tf.global_variables_initializer())
        self.session.run(tf.local_variables_initializer())
        self.saver = tf.train.Saver(tf.global_variables())
        self.random=False
        self.random_rate=0.1
        self.restore()
        self.gamma=0.9

    def restore(self):
        self.saver.restore(self.session, "./python/model/saved_networks")

    
    def save(self):
        self.saver.save(self.session, "./python/model/saved_networks")

    def creat_network(self):
        self.stat_feats=tf.placeholder(tf.float32,[None,self.card_number,self.card_colour,self.stat_dim])
        self.action_feats=tf.placeholder(tf.float32,[None,self.card_number,self.card_colour,self.action_dim])
        self.reward=tf.placeholder(tf.float32,[None])
        with tf.variable_scope('stat_output'):
            conv1_w=tf.get_variable(shape=[3,3,self.stat_dim,32],initializer=tf.truncated_normal_initializer(),name='conv1_w')
            conv_1=tf.nn.conv2d(self.stat_feats,conv1_w,strides=[1,1,1,1],padding='SAME')
            conv_1=tf.nn.relu(conv_1)
            pool_1=tf.nn.max_pool(conv_1,ksize=[1,2,2,1],strides=[1,2,2,1],padding='SAME')
            
            conv2_w=tf.get_variable(shape=[3,3,32,64],initializer=tf.truncated_normal_initializer(),name='conv2_w')
            conv_2=tf.nn.conv2d(pool_1,conv2_w,strides=[1,1,1,1],padding='SAME')
            conv_2=tf.nn.relu(conv_2)
            pool_2=tf.nn.max_pool(conv_2,ksize=[1,2,2,1],strides=[1,2,2,1],padding='SAME')
            pool_2_flat=tf.reshape(pool_2,[-1,512])
            stat_out=tf.layers.dense(pool_2_flat,128)

        with tf.variable_scope('action_output'):
            conv1_w=tf.get_variable(shape=[3,3,self.action_dim,32],initializer=tf.truncated_normal_initializer(),name='conv1_w')
            conv_1=tf.nn.conv2d(self.action_feats,conv1_w,strides=[1,1,1,1],padding='SAME')
            conv_1=tf.nn.relu(conv_1)
            pool_1=tf.nn.max_pool(conv_1,ksize=[1,2,2,1],strides=[1,2,2,1],padding='SAME')
        
            conv2_w=tf.get_variable(shape=[3,3,32,64],initializer=tf.truncated_normal_initializer(),name='conv2_w')
            conv_2=tf.nn.conv2d(pool_1,conv2_w,strides=[1,1,1,1],padding='SAME')
            conv_2=tf.nn.relu(conv_2)
            pool_2=tf.nn.max_pool(conv_2,ksize=[1,2,2,1],strides=[1,2,2,1],padding='SAME')

            pool_3_flat=tf.reshape(pool_2,[-1,512])
            action_out=tf.layers.dense(pool_3_flat,128)

        with tf.variable_scope('StatPlusAct'):
            stat_all=tf.nn.relu(tf.concat([stat_out,action_out],axis=1))
            stat_all_out=tf.nn.relu(tf.layers.dense(stat_all,128))
            con_w=tf.get_variable(name='conv_w',shape=[stat_all_out.get_shape()[1],1])
            self.qValue=tf.matmul(stat_all_out,con_w)
            self.qAction=tf.reduce_mean(self.qValue,1)
            self.loss=tf.reduce_mean(tf.square(self.qAction-self.reward))
            self.optimizer  = tf.train.AdamOptimizer(learning_rate=1e-5)
            self.train_op=self.optimizer.minimize(self.loss)

    def get_action(self,action):
        action_feat = np.zeros((self.card_number, self.card_colour, self.action_dim))
        for _,card in enumerate(action):
                if card<52:
                    action_feat[int(card/4)+1, card%4,0] += 1
                elif card==52:
                    action_feat[13, 4,0] += 1
                else:
                    action_feat[14, 4,0] += 1
        return action_feat

    def get_stat(self,stat):
        hand_cards = stat.own
        stat_feat = np.zeros((self.card_number, self.card_colour, self.stat_dim))

        for card in hand_cards:
            if card<52:
                stat_feat[int(card/4)+1, card%4, 0] += 1
            elif card==52:
                stat_feat[13, 4, 0] += 1
            else:
                stat_feat[14, 4, 0] += 1

        for card in stat.my_history:
            if card<52:
                stat_feat[int(card/4)+1, card%4, 1] += 1
            elif card==52:
                stat_feat[13, 4, 1] += 1
            else:
                stat_feat[14, 4, 1] += 1
        
        for card in stat.pre_history:
            if card<52:
                stat_feat[int(card/4)+1, card%4, 2] += 1
            elif card==52:
                stat_feat[13, 4, 2] += 1
            else:
                stat_feat[14, 4, 2] += 1
        
        for card in stat.next_history:
            if card<52:
                stat_feat[int(card/4)+1, card%4, 3] += 1
            elif card==52:
                stat_feat[13, 4, 3] += 1
            else:
                stat_feat[14, 4, 3] += 1

        for card in stat.my_last:
            if card<52:
                stat_feat[int(card/4)+1, card%4, 0] += 1
            elif card==52:
                stat_feat[13, 4, 4] += 1
            else:
                stat_feat[14, 4, 4] += 1
        return stat_feat

        for card in stat.pre_last:
            if card<52:
                stat_feat[int(card/4)+1, card%4, 0] += 1
            elif card==52:
                stat_feat[13, 4, 5] += 1
            else:
                stat_feat[14, 4, 5] += 1

        for card in stat.next_last:
            if card<52:
                stat_feat[int(card/4)+1, card%4, 0] += 1
            elif card==52:
                stat_feat[13, 4, 6] += 1
            else:
                stat_feat[14, 4, 6] += 1
        
        for i in range(self.card_number):
            for j in range(self.card_colour):
                stat_feat[i,j, 7]=stat.my_id

    def update_model(self, experiences):
        action_feats= []
        stat_feats=[]
        reward_feats=[]
        for experience in experiences:
            stat_feats.append(self.get_stat(experience[0]))
            action_feats.append(self.get_action(experience[1]))
            reward_feats.append(experience[2])



        _, loss,q = self.session.run((self.train_op,self.loss, self.qAction), feed_dict = { self.stat_feats:stat_feats,self.action_feats:action_feats,self.reward:reward_feats})
        print(loss)

    def receive_stat(self,stat):
        self.stat = stat

    def take_action(self,action_list):
        stat = self.stat
        action_feats = []
        for _,action in enumerate(action_list):
            action_feats.append(self.get_action(action))

        stat_feat = self.get_stat(stat)
        stat_feats = []
        stat_feats = [stat_feat for i in range(len(action_list))]
        q = self.qAction.eval(session=self.session,feed_dict={self.stat_feats: stat_feats, self.action_feats: action_feats})
        idx = int(np.argmax(q))
        if self.random==True:
            if (random.random())>0.9:
                idx=random.randint(0,len(q)-1)
        return action_list[idx]

class GetStat():
    def __init__(self,all_data):
        self.all_info=all_data
        self.my_history,self.pre_history,self.next_history=[],[],[]
        self.use_info = all_data["requests"][0]
        self.history, self.publiccard =self.use_info["history"], self.use_info["publiccard"]
        self.last_history = all_data["requests"][-1]["history"]
        self.get_my_history()
        self.get_pre_history()
        self.get_next_history()
        self.get_my_card()
        self.get_my_id()
        self.get_last_history()

    def get_my_id(self):
        self.my_id = 0 # 判断自己是什么身份，地主0 or 农民甲1 or 农民乙2
        if len(self.history[0]) == 0:
            if len(self.history[1]) != 0:
                self.my_id = 1
        else:
            self.my_id = 2

    def get_my_history(self):
        my_history=self.all_info["responses"]
        for _, round in enumerate(my_history):
            for _, cards in enumerate(round):
                self.my_history.append(cards)
    
    def get_my_card(self):
        self.own=self.use_info["own"]
        for _,card in enumerate(self.my_history):
            self.own.remove(card)
        self.own.sort()
    
    def get_pre_history(self):
        self.pre_history=[]
        for _ ,round in enumerate(self.all_info["requests"]):
            self.pre_history+=round["history"][1]
    
    def get_next_history(self):
        self.next_history=[]
        for _ ,round in enumerate(self.all_info["requests"]):
            self.next_history+=round["history"][0]
    
    def get_last_history(self):
        self.pre_last=self.last_history[1]
        self.next_history=self.last_history[0]
        if len(self.all_info["responses"])!=0:
            self.my_last=self.all_info["responses"][-1]
        else:
            self.my_last=[]
      
class GetAction():
    def __init__ (self,stat):
        self.poker=stat.own
        self.lastTypeP, self.lastMP, self.lastSP, self.countPass = self.recover(stat.last_history)
        self.res=[]
        if self.countPass!=2:
            self.res=self.searchCard(self.poker, self.lastTypeP, self.lastMP, self.lastSP)
            if self.res==[]:
                self.res.append([])
        else:
            self.res=self.separate(self.poker)

    def ordinalTransfer(self,poker):
        newPoker = [int(i/4)+3 for i in poker if i <= 52]
        if 53 in poker:
            newPoker += [17]
        return newPoker

    def separate(self,poker): 
        res = []
        if len(poker) == 0:
            return res
        myPoker = [i for i in poker]
        newPoker = self.ordinalTransfer(myPoker)
        if 16 in newPoker and 17 in newPoker: # 单独列出火箭
            newPoker = newPoker[:-2]
            res += [[16, 17]]
        elif 16 in newPoker:
            newPoker = newPoker[:-1]    
            res += [[16]]
        elif 17 in newPoker:
            newPoker = newPoker[:-1] 
            res += [[17]]
        
        singlePoker = list(set(newPoker)) # 都有哪些牌
        singlePoker.sort()

        for i in singlePoker:    # 分出炸弹，其实也可以不分，优化点之一
            if newPoker.count(i) == 4:
                idx = newPoker.index(i)
                res += [ newPoker[idx:idx+4] ]
                newPoker = newPoker[0:idx] + newPoker[idx+4:]

        specialCount, specialRes = 0, []
        if 15 in newPoker:
            specialCount = newPoker.count(15)
            idx = newPoker.index(15)
            specialRes = [15 for i in range(specialCount)]
            newPoker = newPoker[:-specialCount]

        def findSeq(p, dupTime, minLen): # 这里的p是点数，找最长的顺子，返回值为牌型组合
            resSeq, tmpSeq = [], []
            singleP = list(set(p))
            singleP.sort()
            for curr in singleP:
                if p.count(curr) >= dupTime:
                    if len(tmpSeq) == 0:
                        tmpSeq = [curr]
                        continue
                    elif curr == (tmpSeq[-1] + 1):
                        tmpSeq += [curr]
                        continue
                if len(tmpSeq) >= minLen:
                    tmpSeq = [i for i in tmpSeq for j in range(dupTime)]
                    resSeq += [tmpSeq]
                tmpSeq = []
            return resSeq

        def subSeq(p, subp): # 一定保证subp是p的子集
            singleP = list(set(subp))
            singleP.sort()
            for curr in singleP:
                idx = p.index(curr)
                countP = subp.count(curr)
                p = p[0:idx] + p[idx+countP:]
            return p
    

        para = [[1,5],[2,3],[3,2]]
        validChoice = [0,1,2]
        allSeq = [[], [], []] # 分别表示单顺、双顺、三顺（飞机不带翼）
        while(True): # myPoker，这里会找完所有的最长顺子
            if len(newPoker) == 0 or len(validChoice) == 0:
                break
            dupTime = random.choice(validChoice)
            tmp = para[dupTime]
            newSeq = findSeq(newPoker, tmp[0], tmp[1])
            for tmpSeq in newSeq:
                newPoker = subSeq(newPoker, tmpSeq)
            if len(newSeq) == 0:
                validChoice.remove(dupTime)
            else:
                allSeq[dupTime] += [tmpSeq]
        res += allSeq[0] + allSeq[1] # 对于单顺和双顺没必要去改变
        plane = allSeq[2]

        allRetail = [[], [], []] # 分别表示单张，对子，三张
        singlePoker = list(set(newPoker)) # 更新目前为止剩下的牌，newPoker和myPoker是一一对应的
        singlePoker.sort()
        for curr in singlePoker:
            countP = newPoker.count(curr)
            allRetail[countP-1] += [[curr for i in range(countP)]]


        for curr in plane:
            lenKind = int(len(curr) / 3)
            tmp = curr
            for t in range(2): # 分别试探单张和对子的个数是否足够
                tmpP = allRetail[t]
                if len(tmpP) >= lenKind:
                    tmp += [i[j] for i in tmpP[0:lenKind] for j in range(t+1)]
                    allRetail[t] = allRetail[t][lenKind:]
                    break
            res += [tmp]

        if specialCount == 3:
            allRetail[2] += [specialRes]
        elif specialCount > 0 and specialCount <= 2:
            allRetail[specialCount - 1] += [specialRes]
        for curr in allRetail[2]: # curr = [1,1,1]
            tmp = curr
            for t in range(2):
                tmpP = allRetail[t]
                if len(tmpP) >= 1:
                    tmp += tmpP[0]
                    allRetail[t] = allRetail[t][1:]
                    break
            res += [tmp]
    
        res += allRetail[0] + allRetail[1]
        real_result=[]
        for _, card_comb in enumerate(res):
            temp=[]
            for _,card in enumerate(card_comb):
                if card==17:
                        temp.append(53)
                        self.poker.remove(53)
                elif card==16:
                        temp.append(52)
                        self.poker.remove(52)
                else:
                    for i in range(4):
                        if (card-3)*4+i in self.poker:
                            temp.append((card-3)*4+i)
                            self.poker.remove((card-3)*4+i)
                            break
            real_result.append(temp)
        return real_result

    def checkPokerType(self,poker, hasTransfer): # poker：list，表示一个人出牌的牌型
        poker.sort()
        lenPoker = len(poker)
        newPoker = [i for i in poker]
        if not hasTransfer:
            newPoker = self.ordinalTransfer(poker)
        typeP, mP, sP = "空", newPoker, []

        for tmp in range(2):
            if tmp == 1:
                return "错误", poker, [] # 没有判断出任何牌型，出错
            if lenPoker == 0: # 没有牌，也即pass
                break
            if poker == [52, 53]:
                typeP = "火箭"
                break
            if lenPoker == 4 and newPoker.count(newPoker[0]) == 4:
                typeP = "炸弹"
                break
            if lenPoker == 1:
                typeP = "单张"
                break
            if lenPoker == 2:
                if newPoker.count(newPoker[0]) == 2:
                    typeP = "一对"
                    break
                continue
                
            firstPoker = newPoker[0]

            if lenPoker >= 5 and 15 not in newPoker:
                singleSeq = [firstPoker+i for i in range(lenPoker)]
                if newPoker == singleSeq:
                    typeP = "单顺"
                    break

            if lenPoker >= 6 and lenPoker % 2 == 0 and 15 not in newPoker:
                pairSeq = [firstPoker+i for i in range(int(lenPoker / 2))]
                pairSeq = [j for j in pairSeq for i in range(2)]
                if newPoker == pairSeq:
                    typeP = "双顺"
                    break

            thirdPoker = newPoker[2]
            if lenPoker <= 5 and newPoker.count(thirdPoker) == 3:
                mP, sP = [thirdPoker for k in range(3)], [k for k in newPoker if k != thirdPoker]
                if lenPoker == 3:
                    typeP = "三带零"
                    break
                if lenPoker == 4:
                    typeP = "三带一"
                    break
                if lenPoker == 5:
                    typeP = "三带二"
                    if sP[0] == sP[1]:
                        break
                    continue

            if lenPoker < 6:
                continue

            fifthPoker = newPoker[4]
            if lenPoker == 6 and newPoker.count(thirdPoker) == 4:
                typeP, mP = "四带两只", [thirdPoker for k in range(4)]
                sP = [k for k in newPoker if k != thirdPoker]
                if sP[0] != sP[1]:
                    break
                continue
            if lenPoker == 8:
                typeP = "四带两对"
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
                        typeP = "飞机不带翼"
                        break
                    if singleDupTime == [4]: # 航天飞机不带翼
                        typeP = "航天飞机不带翼"
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
                            typeP = "飞机带小翼"
                            mP, sP = m, s
                            break
                        if singleDupTime[0] == 2:
                            typeP = "飞机带大翼"
                            mP, sP = m, s
                            break
                    elif singleDupTime[1] == 4:
                        if singleDupTime[0] == 1:
                            typeP = "航天飞机带小翼"
                            mP, sP = m, s
                            break
                        if singleDupTime[0] == 2:
                            typeP = "航天飞机带大翼"
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
                return "错误", poker, []
        return typeP, omP, osP

    def searchCard(self,poker, objType, objMP, objSP): # 搜索自己有没有大过这些牌的牌
        if objType == "火箭": # 火箭是最大的牌
            return []
        # poker.sort() # 要求poker是有序的，使得newPoker一般也是有序的
        newPoker = self.ordinalTransfer(poker)
        singlePoker = list(set(newPoker)) # 都有哪些牌
        singlePoker.sort()
        countPoker = [newPoker.count(i) for i in singlePoker] # 这些牌都有几张
    
        res = []
        idx = [[i for i in range(len(countPoker)) if countPoker[i] == k] for k in range(5)] # 分别有1,2,3,4的牌在singlePoker中的下标
        quadPoker = [singlePoker[i] for i in idx[4]]
        flag = 0
        if len(poker) >= 2:
            if poker[-2] == 52 and poker[-1] == 53:
                flag = 1

        newObjMP, lenObjMP = self.ordinalTransfer(objMP), len(objMP)

        if objType == "炸弹":
            for curr in quadPoker:
                if curr > newObjMP[0]:
                    res += [[(curr-3)*4+j for j in range(4)]]
            if flag:
                res += [[52,53]]
            return res

        singleObjMP = list(set(newObjMP)) # singleObjMP为超过一张的牌的点数
        singleObjMP.sort()
        countObjMP, maxObjMP = newObjMP.count(singleObjMP[0]), singleObjMP[-1]
        # countObjMP虽取首元素在ObjMP中的个数，但所有牌count应相同；countObjMP * len(singleObjMP) == lenObjMP

        newObjSP, lenObjSP = self.ordinalTransfer(objSP), len(objSP) # 只算点数的对方拥有的主牌; 对方拥有的主牌数
        singleObjSP = list(set(newObjSP)) 
        singleObjSP.sort()
        countObjSP = 0
        if len(objSP) > 0: # 有可能没有从牌，从牌的可能性为单张或双张
            countObjSP = newObjSP.count(singleObjSP[0])

        tmpMP, tmpSP = [], []

        for j in range(1, 16 - maxObjMP):
            tmpMP, tmpSP = [i + j for i in singleObjMP], []
            if all([newPoker.count(i) >= countObjMP for i in tmpMP]): # 找到一个匹配的更大解
                if j == (15 - maxObjMP) and countObjMP != lenObjMP: # 与顺子有关，则解中不能出现2（15）
                    break
                if lenObjSP != 0:
                    tmpSP = list(set(singlePoker)-set(tmpMP))
                    tmpSP.sort()
                    tmpSP = [i for i in tmpSP if newPoker.count(i) >= countObjSP] # 作为从牌有很多组合方式，是优化点
                    species = int(lenObjSP/countObjSP)
                    if len(tmpSP) < species: # 剩余符合从牌特征的牌种数少于目标要求的牌种数，比如334455->lenObjSP=6,countObjSP=2,tmpSP = [8,9]
                        continue
                    tmp = [i for i in tmpSP if newPoker.count(i) == countObjSP]
                    if len(tmp) >= species: # 剩余符合从牌特征的牌种数少于目标要求的牌种数，比如334455->lenObjSP=6,countObjSP=2,tmpSP = [8,9]
                        tmpSP = tmp
                    tmpSP = tmpSP[0:species]
                tmpRes = []
                idxMP = [newPoker.index(i) for i in tmpMP]
                idxMP = [i+j for i in idxMP for j in range(countObjMP)]
                idxSP = [newPoker.index(i) for i in tmpSP]
                idxSP = [i+j for i in idxSP for j in range(countObjSP)]
                idxAll = idxMP + idxSP
                tmpRes = [poker[i] for i in idxAll]
                res += [tmpRes]
    
        if objType == "单张": # 以上情况少了上家出2，本家可出大小王的情况
            if 52 in poker and objMP[0] < 52:
                res += [[52]]
            if 53 in poker:
                res += [[53]]

        for curr in quadPoker: # 把所有炸弹先放进返回解
            res += [[(curr-3)*4+j for j in range(4)]]
        if flag:
            res += [[52,53]]
        return res
        
    def recover(self,history): # 只考虑倒数3个，返回最后一个有效牌型及主从牌，且返回之前有几个人选择了pass；id是为了防止某一出牌人在某一牌局后又pass，然后造成连续pass
        typeP, mP, sP, countPass = "空", [], [], 0
        for i in range(-1,-3,-1):
            lastPoker = history[i]
            typeP, mP, sP = self.checkPokerType(lastPoker, 0)
            if typeP == "空":
                countPass += 1
                continue
            break
        return typeP, mP, sP, countPass

def main():
    brain=DeepQLearning()
    all_data=json.loads(input())
    stat=GetStat(all_data)
    action=GetAction(stat)
    brain.receive_stat(stat)
    res=brain.take_action(action.res)
    print(json.dumps({
        "response": res
    }))

if __name__=='__main__':
    main()