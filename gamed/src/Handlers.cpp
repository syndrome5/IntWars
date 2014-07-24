/*
IntWars playground server for League of Legends protocol testing
Copyright (C) 2012  Intline9 <Intline9@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "stdafx.h"
#include "Game.h"
#include "Packets.h"
#include "ChatBox.h"

#include <vector>
#include <string>

using namespace std;

bool Game::handleNull(HANDLE_ARGS) {
    return true;
}

bool Game::handleKeyCheck(ENetPeer *peer, ENetPacket *packet) {
    KeyCheck *keyCheck = (KeyCheck *)packet->data;
    uint64 userId = _blowfish->Decrypt(keyCheck->checkId);
    /*
    uint64 enc = _blowfish->Encrypt(keyCheck->userId);
    char buffer[255];
    unsigned char *p = (unsigned char*)&enc;
    for(int i = 0; i < 8; i++)
    {
    sprintf(&buffer[i*3], "%02X ", p[i]);
    }
    PDEBUG_LOG_LINE(//Logging," Enc id: %s\n", buffer);*/
    if(userId == keyCheck->userId) {
       // PDEBUG_LOG_LINE(//Logging, " User got the same key as i do, go on!\n");
        peerInfo(peer)->keyChecked = true;
        peerInfo(peer)->userId = userId;
    } else {
        //Logging->errorLine(" WRONG KEY, GTFO!!!\n");
        return false;
    }
    //Send response as this is correct (OFC DO SOME ID CHECKS HERE!!!)
    KeyCheck response;
    response.userId = keyCheck->userId;
    bool bRet = sendPacket(peer, reinterpret_cast<uint8 *>(&response), sizeof(KeyCheck), CHL_HANDSHAKE);
    handleGameNumber(peer, NULL);//Send 0x91 Packet?
    return bRet;
}

bool Game::handleGameNumber(ENetPeer *peer, ENetPacket *packet) {
    WorldSendGameNumber world(1, "EUW1", peerInfo(peer)->getName());
    return sendPacket(peer, world, CHL_S2C);
}

bool Game::handleSynch(ENetPeer *peer, ENetPacket *packet) {
    SynchVersion *version = reinterpret_cast<SynchVersion *>(packet->data);
    //Logging->writeLine("Client version: %s\n", version->version);
    SynchVersionAns answer;
    answer.mapId = 1;
    answer.players[0].userId = peerInfo(peer)->userId;
    answer.players[0].skill1 = SPL_Ignite;
    answer.players[0].skill2 = SPL_Flash;
    return sendPacket(peer, reinterpret_cast<uint8 *>(&answer), sizeof(SynchVersionAns), 3);
}

bool Game::handleMap(ENetPeer *peer, ENetPacket *packet) {
    LoadScreenPlayerName loadName(*peerInfo(peer));
    LoadScreenPlayerChampion loadChampion(*peerInfo(peer));
    //Builds team info
    LoadScreenInfo screenInfo;
    screenInfo.bluePlayerNo = 1;
    screenInfo.redPlayerNo = 0;
    screenInfo.bluePlayerIds[0] = peerInfo(peer)->userId;
    bool pInfo = sendPacket(peer, reinterpret_cast<uint8 *>(&screenInfo), sizeof(LoadScreenInfo), CHL_LOADING_SCREEN);
    //For all players send this info
    bool pName = sendPacket(peer, loadName, CHL_LOADING_SCREEN);
    bool pHero = sendPacket(peer, loadChampion, CHL_LOADING_SCREEN);

    return (pInfo && pName && pHero);
}

//building the map
bool Game::handleSpawn(ENetPeer *peer, ENetPacket *packet) {
    StatePacket2 start(PKT_S2C_StartSpawn);
    bool p1 = sendPacket(peer, start, CHL_S2C);
    printf("Spawning map\r\n");
    
    HeroSpawn spawn(peerInfo(peer)->getChampion()->getNetId(), 0, peerInfo(peer)->getName(), peerInfo(peer)->getChampion()->getType(), peerInfo(peer)->getSkinNo());
    bool p2 = sendPacket(peer, spawn, CHL_S2C);
    
    PlayerInfo info(peerInfo(peer)->getChampion()->getNetId(), SPL_Ignite, SPL_Flash);
    sendPacket(peer, info, CHL_S2C);
    
    HeroSpawn2 h2(peerInfo(peer)->getChampion()->getNetId());
    sendPacket(peer, h2, CHL_S2C);
	
    notifySetHealth(peerInfo(peer)->getChampion());
    //Spawn Turrets
    vector<string> szTurrets = {
        "@Turret_T1_R_03_A",
        "@Turret_T1_R_02_A",
        "@Turret_T1_C_07_A",
        "@Turret_T2_R_03_A",
        "@Turret_T2_R_02_A",
        "@Turret_T2_R_01_A",
        "@Turret_T1_C_05_A",
        "@Turret_T1_C_04_A",
        "@Turret_T1_C_03_A",
        "@Turret_T1_C_01_A",
        "@Turret_T1_C_02_A",
        "@Turret_T2_C_05_A",
        "@Turret_T2_C_04_A",
        "@Turret_T2_C_03_A",
        "@Turret_T2_C_01_A",
        "@Turret_T2_C_02_A",
        "@Turret_OrderTurretShrine_A",
        "@Turret_ChaosTurretShrine_A",
        "@Turret_T1_L_03_A",
        "@Turret_T1_L_02_A",
        "@Turret_T1_C_06_A",
        "@Turret_T2_L_03_A",
        "@Turret_T2_L_02_A",
        "@Turret_T2_L_01_A"
    };
    for(unsigned int i = 0; i < 24; i++) {
        TurretSpawn turretSpawn(GetNewNetID(), szTurrets[i]);
        sendPacket(peer, turretSpawn, CHL_S2C);
    }
    //Spawn Props
    LevelPropSpawn lpSpawn(GetNewNetID(), "LevelProp_Yonkey", "Yonkey", 12465, 14422.257f, 101);
    sendPacket(peer, lpSpawn, CHL_S2C);
    LevelPropSpawn lpSpawn2(GetNewNetID(), "LevelProp_Yonkey1", "Yonkey", -76, 1769.1589f, 94);
    sendPacket(peer, lpSpawn2, CHL_S2C);
    LevelPropSpawn lpSpawn3(GetNewNetID(), "LevelProp_ShopMale", "ShopMale", 13374, 14245.673f, 194);
    sendPacket(peer, lpSpawn3, CHL_S2C);
    LevelPropSpawn lpSpawn4(GetNewNetID(), "LevelProp_ShopMale1", "ShopMale", -99, 855.6632f, 191);
    sendPacket(peer, lpSpawn4, CHL_S2C);
    
    StatePacket end(PKT_S2C_EndSpawn);
    bool p3 = sendPacket(peer, end, CHL_S2C);
    BuyItemAns recall(peerInfo(peer)->getChampion()->getNetId(), 2001, 7, 1);
    bool p4 = sendPacket(peer, recall, CHL_S2C); //activate recall slot
    GameTimer timer(0);
    sendPacket(peer, timer, CHL_S2C);
    GameTimer timer2(0.4f);
    sendPacket(peer, timer2, CHL_S2C);
    GameTimerUpdate timer3(0.4f);
    sendPacket(peer, timer3, CHL_S2C);
    for(int i = 0; i < 4; i++) {
        SpellSet spell(peerInfo(peer)->getChampion()->getNetId(), i, 1);
        sendPacket(peer, spell, CHL_S2C);
    }
    return p1 & p2 & p3;
}

bool Game::handleStartGame(HANDLE_ARGS) {
   StatePacket start(PKT_S2C_StartGame);
   sendPacket(peer, start, CHL_S2C);
   
   _started = true;
   
   /*
   FogUpdate2 test(peerInfo(peer)->getChampion()->getNetId(), 0, 0, 2);
   sendPacket(peer, test, CHL_S2C);
   TODO : Create class Turret (like Champion or Minion) to change Fog (and Masks)
   */
   return true;
}

bool Game::handleAttentionPing(ENetPeer *peer, ENetPacket *packet) {
   AttentionPing *ping = reinterpret_cast<AttentionPing *>(packet->data);
   AttentionPingAns response(peerInfo(peer), ping);
   return broadcastPacket(response, CHL_S2C);
}

bool Game::handleView(ENetPeer *peer, ENetPacket *packet) {
   ViewRequest *request = reinterpret_cast<ViewRequest *>(packet->data);
   ViewAnswer answer(request);
   if (request->requestNo == 0xFE)
   {
      answer.setRequestNo(0xFF);
   }
   else
   {
      answer.setRequestNo(request->requestNo);
   }
   sendPacket(peer, answer, CHL_S2C, UNRELIABLE);
   return true;
}

inline void SetBitmaskValue(uint8 mask[], int pos, bool val) {
    if(pos < 0)
    { return; }
    if(val)
    { mask[pos / 8] |= 1 << (pos % 8); }
    else
    { mask[pos / 8] &= ~(1 << (pos % 8)); }
}

inline bool GetBitmaskValue(uint8 mask[], int pos) {
    return pos >= 0 && ((1 << (pos % 8)) & mask[pos / 8]) != 0;
}

std::vector<MovementVector> readWaypoints(uint8 *buffer, int coordCount) {
    unsigned int nPos = (coordCount + 5) / 8;
    if(coordCount % 2)
    { nPos++; }
    int vectorCount = coordCount / 2;
    std::vector<MovementVector> vMoves;
    MovementVector lastCoord;
    for(int i = 0; i < vectorCount; i++) {
        if(GetBitmaskValue(buffer, (i - 1) * 2)) {
            lastCoord.x += *(char *)&buffer[nPos++];
        } else {
            lastCoord.x = *(short *)&buffer[nPos];
            nPos += 2;
        }
        if(GetBitmaskValue(buffer, (i - 1) * 2 + 1)) {
            lastCoord.y += *(char *)&buffer[nPos++];
        } else {
            lastCoord.y = *(short *)&buffer[nPos];
            nPos += 2;
        }
        vMoves.push_back(lastCoord);
    }
    return vMoves;
}

bool Game::handleMove(ENetPeer *peer, ENetPacket *packet) {
   MovementReq *request = reinterpret_cast<MovementReq *>(packet->data);
   std::vector<MovementVector> vMoves = readWaypoints(&request->moveData, request->vectorNo);
    
   switch(request->type) {
   //TODO, Implement stop commands
   case STOP:
   {
      float x = ((request->x) - MAP_WIDTH)/2;
      float y = ((request->y) - MAP_HEIGHT)/2;

      printf("Stopped at x:%f , y: %f\n", x,y);
      break;
   }
   case EMOTE:
      //Logging->writeLine("Emotion\n");
      return true;
   }
   
   peerInfo(peer)->getChampion()->setWaypoints(vMoves);

   return true;
}

bool Game::handleLoadPing(ENetPeer *peer, ENetPacket *packet) {
    PingLoadInfo *loadInfo = reinterpret_cast<PingLoadInfo *>(packet->data);
    PingLoadInfo response;
    memcpy(&response, packet->data, sizeof(PingLoadInfo));
    response.header.cmd = PKT_S2C_Ping_Load_Info;
    response.userId = peerInfo(peer)->userId;
    //Logging->writeLine("loaded: %f, ping: %f, %f\n", loadInfo->loaded, loadInfo->ping, loadInfo->f3);
    bool bRet = broadcastPacket(reinterpret_cast<uint8 *>(&response), sizeof(PingLoadInfo), CHL_LOW_PRIORITY, UNRELIABLE);
    static bool bLoad = false;
    if(!bLoad) {
        handleMap(peer, NULL);
        bLoad = true;
    }
    return bRet;
}

bool Game::handleQueryStatus(HANDLE_ARGS) {
    QueryStatusAns response;
    return sendPacket(peer, response, CHL_S2C);
}

bool Game::handleClick(HANDLE_ARGS) {
   Click *click = reinterpret_cast<Click *>(packet->data);
   printf("Object %u clicked on %u\n", peerInfo(peer)->getChampion()->getNetId(),click->targetNetId);
   Unk response(peerInfo(peer)->getChampion()->getNetId(), 0, 0, click->targetNetId);
   return sendPacket(peer, reinterpret_cast<uint8 *>(&response), sizeof(response), CHL_S2C);
}

bool Game::handleCastSpell(HANDLE_ARGS) {
   CastSpell *spell = reinterpret_cast<CastSpell *>(packet->data);

   printf("Spell Cast : Slot %d, coord %f ; %f, coord2 %f, %f, target NetId %08X\n", spell->spellSlot & 0x7F, spell->x, spell->y, spell->x2, spell->y2, spell->targetNetId);

   Spell* s = peerInfo(peer)->getChampion()->castSpell(spell->spellSlot & 0x7F, spell->x, spell->y, 0);

   if(!s) {
      return false;
   }

   /*Unk unk(peerInfo(peer)->getChampion()->getNetId(), spell->x, spell->y, spell->targetNetId);
   sendPacket(peer, reinterpret_cast<uint8 *>(&unk), sizeof(unk), CHL_S2C);*/

   CastSpellAns response(s, spell->x, spell->y);
   sendPacket(peer, response, CHL_S2C);

   SpawnProjectile sp(GetNewNetID(), peerInfo(peer)->getChampion(), spell->x, spell->y);
   sendPacket(peer, sp, CHL_S2C);

   return true;
}

bool Game::handleChatBoxMessage(HANDLE_ARGS) {
    ChatMessage *message = reinterpret_cast<ChatMessage *>(packet->data);
    //Lets do commands
    if(message->msg == '.') {
        const char *cmd[] = { ".set", ".gold", ".speed", ".health", ".xp", ".ap", ".ad", ".mana", ".model", ".help", ".spawn" };
        //Set field
        if(strncmp(message->getMessage(), cmd[0], strlen(cmd[0])) == 0) {
            uint32 blockNo, fieldNo;
            float value;
            sscanf(&message->getMessage()[strlen(cmd[0])+1], "%u %u %f", &blockNo, &fieldNo, &value);
            blockNo = 1 << (blockNo - 1);
            uint32 mask = 1 << (fieldNo - 1);
            CharacterStats stats(blockNo, peerInfo(peer)->getChampion()->getNetId(), mask, value);
            sendPacket(peer, stats, CHL_LOW_PRIORITY, 2);
            return true;
        }
        // Set Gold
        if(strncmp(message->getMessage(), cmd[1], strlen(cmd[1])) == 0) {
            float gold = (float)atoi(&message->getMessage()[strlen(cmd[1]) + 1]);
            CharacterStats stats(MM_One, peerInfo(peer)->getChampion()->getNetId(), FM1_Gold, gold);
            sendPacket(peer, stats, CHL_LOW_PRIORITY, 2);
            /*CharacterStats stats2(MM_One, peerInfo(peer)->netId, FM1_Gold_2, gold);
            sendPacket(peer, stats2, CHL_LOW_PRIORITY, 2);*/
            return true;
        }
       
        //movement
        if(strncmp(message->getMessage(), cmd[2], strlen(cmd[2])) == 0)
        {
           float data = (float)atoi(&message->getMessage()[strlen(cmd[2])+1]);
           
           printf("Setting speed to %f\n", data);
           
           peerInfo(peer)->getChampion()->getStats().setMovementSpeed(data);
           return true;
        }
        
         //spawn
         if(strncmp(message->getMessage(), cmd[10], strlen(cmd[10])) == 0)
         {
            static const MinionSpawnPosition positions[] = {   SPAWN_BLUE_TOP,
                                                               SPAWN_BLUE_BOT,
                                                               SPAWN_BLUE_MID,
                                                               SPAWN_RED_TOP,
                                                               SPAWN_RED_BOT,
                                                               SPAWN_RED_MID, 
                                                            };
                          
            for(int i = 0; i < 6; ++i) {                                     
               Minion* m = new Minion(map, GetNewNetID(), MINION_TYPE_MELEE, positions[i]);
               map->addObject(m);
               notifyMinionSpawned(m);
            }
            return true;
         }
         
        //health
        if(strncmp(message->getMessage(), cmd[3], strlen(cmd[3])) == 0)
        {
           float data = (float)atoi(&message->getMessage()[strlen(cmd[3])+1]);
           
           peerInfo(peer)->getChampion()->getStats().setCurrentHealth(data);
           peerInfo(peer)->getChampion()->getStats().setMaxHealth(data);
           
           notifySetHealth(peerInfo(peer)->getChampion());
           
           return true;
        }
        
        /*
        //experience
        if(strncmp(message->getMessage(), cmd[4], strlen(cmd[4])) == 0)
        {
        float data = (float)atoi(&message->getMessage()[strlen(cmd[4])+1]);

        charStats.statType = STI_Exp;
        charStats.statValue = data;
        //Logging->writeLine("set champ exp to %f\n", data);
        sendPacket(peer,reinterpret_cast<uint8*>(&charStats),sizeof(charStats), CHL_LOW_PRIORITY, 2);
        return true;
        }
        //AbilityPower
        if(strncmp(message->getMessage(), cmd[5], strlen(cmd[5])) == 0)
        {
        float data = (float)atoi(&message->getMessage()[strlen(cmd[5])+1]);

        charStats.statType = STI_AbilityPower;
        charStats.statValue = data;
        //Logging->writeLine("set champ abilityPower to %f\n", data);
        sendPacket(peer,reinterpret_cast<uint8*>(&charStats),sizeof(charStats), CHL_LOW_PRIORITY, 2);
        return true;
        }
        //Attack damage
        if(strncmp(message->getMessage(), cmd[6], strlen(cmd[6])) == 0)
        {
        float data = (float)atoi(&message->getMessage()[strlen(cmd[6])+1]);

        charStats.statType = STI_AttackDamage;
        charStats.statValue = data;
        //Logging->writeLine("set champ attack damage to %f\n", data);
        sendPacket(peer,reinterpret_cast<uint8*>(&charStats),sizeof(charStats), CHL_LOW_PRIORITY, 2);
        return true;
        }
        //Mana
        if(strncmp(message->getMessage(), cmd[7], strlen(cmd[7])) == 0)
        {
        float data = (float)atoi(&message->getMessage()[strlen(cmd[7])+1]);

        charStats.statType = STI_Mana;
        charStats.statValue = data;
        //Logging->writeLine("set champ mana to %f\n", data);
        sendPacket(peer,reinterpret_cast<uint8*>(&charStats),sizeof(charStats), CHL_LOW_PRIORITY, 2);
        return true;
        }
        */
        //Model
        if(strncmp(message->getMessage(), cmd[8], strlen(cmd[8])) == 0) {
            std::string sModel = (char *)&message->getMessage()[strlen(cmd[8]) + 1];
			int32 skinNo = (int32)atoi(&message->getMessage()[strlen(cmd[8]) + sModel.length()]);
			sModel.erase(sModel.find_first_of(' '), sModel.length() - sModel.find_first_of(' '));
            UpdateModel modelPacket(peerInfo(peer)->getChampion()->getNetId(), sModel, skinNo); //96
            broadcastPacket(modelPacket, CHL_S2C);
            return true;
        }
    }
    switch(message->type) {
        case CMT_ALL:
            return broadcastPacket(packet->data, packet->dataLength, CHL_COMMUNICATION);
            break;
        case CMT_TEAM:
            //!TODO make a team class and foreach player in the team send the message
            return sendPacket(peer, packet->data, packet->dataLength, CHL_COMMUNICATION);
            break;
        default:
            //Logging->errorLine("Unknown ChatMessageType\n");
            return sendPacket(peer, packet->data, packet->dataLength, CHL_COMMUNICATION);
            break;
    }
    return false;
}

bool Game::handleSkillUp(HANDLE_ARGS) {
    SkillUpPacket *skillUpPacket = reinterpret_cast<SkillUpPacket *>(packet->data);
    //!TODO Check if can up skill? :)
    
    Spell*s = peerInfo(peer)->getChampion()->levelUpSpell(skillUpPacket->skill);
    
    if(!s) {
      return false;
    }
    
    SkillUpResponse skillUpResponse(peerInfo(peer)->getChampion()->getNetId(), skillUpPacket->skill, s->getLevel(), peerInfo(peer)->getChampion()->getSkillPoints());
    sendPacket(peer, skillUpResponse, CHL_GAMEPLAY);
    
    CharacterStats stats(MM_One, peerInfo(peer)->getChampion()->getNetId(), FM1_SPELL, (unsigned short)(0x108F)); // activate all the spells
    sendPacket(peer, stats, CHL_LOW_PRIORITY, 2);
    
    return true;
}

bool Game::handleBuyItem(HANDLE_ARGS) {
	// TODO : Add to player a system to check slot open or not (and stacks)
    static int slot = 0;
    BuyItemReq *request = reinterpret_cast<BuyItemReq *>(packet->data);
    BuyItemAns response(peerInfo(peer)->getChampion()->getNetId(), request->id, slot++, 1);
    return broadcastPacket(response, CHL_S2C);
}

bool Game::handleEmotion(HANDLE_ARGS) {
    EmotionPacket *emotion = reinterpret_cast<EmotionPacket *>(packet->data);
    //for later use -> tracking, etc.
    switch(emotion->id) {
        case 0:
            //dance
            //Logging->writeLine("dance");
            break;
        case 1:
            //taunt
            //Logging->writeLine("taunt");
            break;
        case 2:
            //laugh
            //Logging->writeLine("laugh");
            break;
        case 3:
            //joke
            //Logging->writeLine("joke");
            break;
    }
    EmotionResponse response(peerInfo(peer)->getChampion()->getNetId(), emotion->id);
    return broadcastPacket(response, CHL_S2C);
}
