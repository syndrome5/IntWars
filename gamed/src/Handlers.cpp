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
#include "ItemManager.h"
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

   if(userId != keyCheck->userId) {
      return false;
   }

   uint32 playerNo = 0;

   for(ClientInfo* player : players) {
      if(player->userId == userId) {
         peer->data = player;
         KeyCheck response;
         response.userId = keyCheck->userId;
         response.playerNo = playerNo;
         bool bRet = sendPacket(peer, reinterpret_cast<uint8 *>(&response), sizeof(KeyCheck), CHL_HANDSHAKE);
         handleGameNumber(peer, NULL);//Send 0x91 Packet?
         return true;
      }
      ++playerNo;
   }

   return false;

}

bool Game::handleGameNumber(ENetPeer *peer, ENetPacket *packet) {
    WorldSendGameNumber world;
    world.gameId = 1;
    strcpy((char *)world.data1, "EUW1");
    memcpy(world.data, peerInfo(peer)->getName().c_str(), peerInfo(peer)->getName().length());
    return sendPacket(peer, reinterpret_cast<uint8 *>(&world), sizeof(WorldSendGameNumber), CHL_S2C);
}

bool Game::handleSynch(ENetPeer *peer, ENetPacket *packet) {
    SynchVersion *version = reinterpret_cast<SynchVersion *>(packet->data);
    //Logging->writeLine("Client version: %s\n", version->version);
    SynchVersionAns answer(players, "Version 4.13.0.262 [PUBLIC]", "CLASSIC");
    printPacket(reinterpret_cast<uint8 *>(&answer), sizeof(answer));
    return sendPacket(peer, answer, 3);
}

bool Game::handleMap(ENetPeer *peer, ENetPacket *packet) {
    LoadScreenPlayerName loadName(*peerInfo(peer));
    LoadScreenPlayerChampion loadChampion(*peerInfo(peer));
    //Builds team info
    LoadScreenInfo screenInfo(players);
    bool pInfo = sendPacket(peer, screenInfo, CHL_LOADING_SCREEN);
    //For all players send this info
    bool pName = sendPacket(peer, loadName, CHL_LOADING_SCREEN);
    bool pHero = sendPacket(peer, loadChampion, CHL_LOADING_SCREEN);

    return (pInfo && pName && pHero);
}

//building the map
bool Game::handleSpawn(ENetPeer *peer, ENetPacket *packet) {
   StatePacket2 start(PKT_S2C_StartSpawn);
   bool p1 = sendPacket(peer, reinterpret_cast<uint8 *>(&start), sizeof(StatePacket2), CHL_S2C);
   printf("Spawning map\r\n");

   int playerId = 0;

   for(auto p : players) {
      HeroSpawn spawn(p, playerId++);
      sendPacket(peer, spawn, CHL_S2C);
      
      HeroSpawn2 h2(p->getChampion());
      sendPacket(peer, h2, CHL_S2C);
      
      PlayerInfo info(p);
      sendPacket(peer, info, CHL_S2C); 
   }

   const std::map<uint32, Object*>& objects = map->getObjects();
 
   for(auto kv : objects) {
      Turret* t = dynamic_cast<Turret*>(kv.second);
      if(t) {
         TurretSpawn turretSpawn(t);
         sendPacket(peer, turretSpawn, CHL_S2C);
         continue;
      }
   
      LevelProp* lp = dynamic_cast<LevelProp*>(kv.second);
      if(lp) {
         LevelPropSpawn lpsPacket(lp);
         sendPacket(peer, lpsPacket, CHL_S2C);
      }
   }

   StatePacket end(PKT_S2C_EndSpawn);
   bool p3 = sendPacket(peer, reinterpret_cast<uint8 *>(&end), sizeof(StatePacket), CHL_S2C);
   BuyItemAns recall;
   recall.header.netId = peerInfo(peer)->getChampion()->getNetId();
   recall.itemId = 2001;
   recall.slotId = 7;
   recall.stack = 1;
   
   bool p4 = sendPacket(peer, reinterpret_cast<uint8 *>(&recall), sizeof(BuyItemAns), CHL_S2C); //activate recall slot
   GameTimer timer(0); //0xC0
   sendPacket(peer, reinterpret_cast<uint8 *>(&timer), sizeof(GameTimer), CHL_S2C);
   GameTimer timer2(0.4f); //0xC0
   sendPacket(peer, reinterpret_cast<uint8 *>(&timer2), sizeof(GameTimer), CHL_S2C);
   GameTimerUpdate timer3(0.4f); //0xC1
   sendPacket(peer, reinterpret_cast<uint8 *>(&timer3), sizeof(GameTimerUpdate), CHL_S2C);

   return true;
}

bool Game::handleStartGame(HANDLE_ARGS) {

   if(++playersReady == players.size()) {
      StatePacket start(PKT_S2C_StartGame);
      broadcastPacket(reinterpret_cast<uint8 *>(&start), sizeof(StatePacket), CHL_S2C);
   
      _started = true;
   }
   
   FogUpdate2 test;
   test.x = 0;
   test.y = 0;
   test.radius = 1;
   test.unk1 = 2;

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
       //TODO anticheat, currently it trusts client 100%
       
      peerInfo(peer)->getChampion()->setPosition(request->x, request->y);
      float x = ((request->x) - MAP_WIDTH)/2;
      float y = ((request->y) - MAP_HEIGHT)/2;
      
      for(int i=0;i<vMoves.size(); i++){
          vMoves.at(i).x = x;
          vMoves.at(i).y = y;
      }

      printf("Stopped at x:%f , y: %f\n", x,y);
      break;
   }
   case EMOTE:
      //Logging->writeLine("Emotion\n");
      return true;
   }
   
   peerInfo(peer)->getChampion()->setWaypoints(vMoves);
   Unit* u = dynamic_cast<Unit*>(map->getObjectById(request->targetNetId));
   if(!u) {
      peerInfo(peer)->getChampion()->setUnitTarget(0);
      return true;
   }
   
   peerInfo(peer)->getChampion()->setUnitTarget(u);

   return true;
}

bool Game::handleLoadPing(ENetPeer *peer, ENetPacket *packet) {
   PingLoadInfo *loadInfo = reinterpret_cast<PingLoadInfo *>(packet->data);
   PingLoadInfo response;
   memcpy(&response, packet->data, sizeof(PingLoadInfo));
   response.header.cmd = PKT_S2C_Ping_Load_Info;
   response.userId = peerInfo(peer)->userId;
   //Logging->writeLine("loaded: %f, ping: %f, %f\n", loadInfo->loaded, loadInfo->ping, loadInfo->f3);
   return broadcastPacket(reinterpret_cast<uint8 *>(&response), sizeof(PingLoadInfo), CHL_LOW_PRIORITY, UNRELIABLE);
}

bool Game::handleQueryStatus(HANDLE_ARGS) {
   QueryStatus response;
   return sendPacket(peer, reinterpret_cast<uint8 *>(&response), sizeof(QueryStatus), CHL_S2C);
}

bool Game::handleClick(HANDLE_ARGS) {
   Click *click = reinterpret_cast<Click *>(packet->data);
   printf("Object %u clicked on %u\n", peerInfo(peer)->getChampion()->getNetId(),click->targetNetId);

   return true;
}

bool Game::handleCastSpell(HANDLE_ARGS) {
   CastSpell *spell = reinterpret_cast<CastSpell *>(packet->data);

   printf("Spell Cast : Slot %d, coord %f ; %f, coord2 %f, %f, target NetId %08X\n", spell->spellSlot & 0x3F, spell->x, spell->y, spell->x2, spell->y2, spell->targetNetId);

   uint32 futureProjNetId = GetNewNetID();
   Spell* s = peerInfo(peer)->getChampion()->castSpell(spell->spellSlot & 0x3F, spell->x, spell->y, 0, futureProjNetId);

   if(!s) {
      return false;
   }
   
   CastSpellAns response(s, spell->x, spell->y, futureProjNetId);
   broadcastPacket(response, CHL_S2C);

   return true;
}

bool Game::handleChatBoxMessage(HANDLE_ARGS) {
   ChatMessage *message = reinterpret_cast<ChatMessage *>(packet->data);
   //Lets do commands
   if(message->msg == '.') {
      const char *cmd[] = { ".set", ".gold", ".speed", ".health", ".xp", ".ap", ".ad", ".mana", ".model", ".help", ".spawn", ".size", ".junglespawn", ".skillpoints", ".level" };
      
      // help command
      if (strncmp(message->getMessage(), cmd[9], strlen(cmd[9])) == 0) {
         return true;
      }
      
      // set
      if(strncmp(message->getMessage(), cmd[0], strlen(cmd[0])) == 0) {
         uint32 blockNo, fieldNo;
         float value;
         sscanf(&message->getMessage()[strlen(cmd[0])+1], "%u %u %f", &blockNo, &fieldNo, &value);
         blockNo = 1 << (blockNo - 1);
         uint32 mask = 1 << (fieldNo - 1);
         peerInfo(peer)->getChampion()->getStats().setStat(blockNo, mask, value);
         return true;
      }
      
      // gold
      if(strncmp(message->getMessage(), cmd[1], strlen(cmd[1])) == 0) {
         float gold = (float)atoi(&message->getMessage()[strlen(cmd[1]) + 1]);
         peerInfo(peer)->getChampion()->getStats().setGold(gold);
         return true;
      }

      // speed
      if(strncmp(message->getMessage(), cmd[2], strlen(cmd[2])) == 0)
      {
         float data = (float)atoi(&message->getMessage()[strlen(cmd[2])+1]);

         printf("Setting speed to %f\n", data);

         peerInfo(peer)->getChampion()->getStats().setMovementSpeed(data);
         return true;
      }

      // spawn
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

      // xp
      if(strncmp(message->getMessage(), cmd[4], strlen(cmd[4])) == 0)
      {
         float data = (float)atoi(&message->getMessage()[strlen(cmd[5])+1]);

         printf("Setting experience to %f\n", data);

         peerInfo(peer)->getChampion()->getStats().setExp(data);
         return true;
      }
      
      // ap
      if(strncmp(message->getMessage(), cmd[5], strlen(cmd[5])) == 0)
      {
         float data = (float)atoi(&message->getMessage()[strlen(cmd[5])+1]);

         printf("Setting AP to %f\n", data);

         peerInfo(peer)->getChampion()->getStats().setBaseAp(data);
         return true;
      }
      
      // ad
      if(strncmp(message->getMessage(), cmd[6], strlen(cmd[6])) == 0)
      {
         float data = (float)atoi(&message->getMessage()[strlen(cmd[5])+1]);

         printf("Setting AD to %f\n", data);

         peerInfo(peer)->getChampion()->getStats().setBaseAd(data);
         return true;
      }
      
      // Mana
      if(strncmp(message->getMessage(), cmd[7], strlen(cmd[7])) == 0)
      {
         float data = (float)atoi(&message->getMessage()[strlen(cmd[7])+1]);

         printf("Setting Mana to %f\n", data);

         peerInfo(peer)->getChampion()->getStats().setCurrentMana(data);
         peerInfo(peer)->getChampion()->getStats().setMaxMana(data);
         return true;
      }
      
      // Model
      if(strncmp(message->getMessage(), cmd[8], strlen(cmd[8])) == 0) {
         std::string sModel = (char *)&message->getMessage()[strlen(cmd[8]) + 1];
         peerInfo(peer)->getChampion()->setModel(sModel);
         return true;
      }
      
      // Size
      if(strncmp(message->getMessage(), cmd[11], strlen(cmd[11])) == 0) {
         float data = (float)atoi(&message->getMessage()[strlen(cmd[11])+1]);

         printf("Setting size to %f\n", data);

         peerInfo(peer)->getChampion()->getStats().setSize(data);
         return true;
      }

      // Mob Spawning-Creating, updated
      if(strncmp(message->getMessage(), cmd[12], strlen(cmd[12])) == 0) {
         const char *cmd[] = { "c baron" , "c wolves", "c red", "c blue", "c dragon", "c wraiths", "c golems"};
         return true;
      }

      // Skillpoints
      if(strncmp(message->getMessage(), cmd[13], strlen(cmd[13])) == 0) {
         peerInfo(peer)->getChampion()->setSkillPoints(17);

         SkillUpResponse skillUpResponse(peerInfo(peer)->getChampion()->getNetId(), 0, 0, 17);
         sendPacket(peer, skillUpResponse, CHL_GAMEPLAY);
         return true;
      }
      
      // Level
      if(strncmp(message->getMessage(), cmd[14], strlen(cmd[14])) == 0) {
         float data = (float)atoi(&message->getMessage()[strlen(cmd[14])+1]);

         peerInfo(peer)->getChampion()->getStats().setLevel(data);
         return true;
      }

   }

   switch(message->type) {
   case CMT_ALL:
      return broadcastPacket(packet->data, packet->dataLength, CHL_COMMUNICATION);
   case CMT_TEAM:
      //!TODO make a team class and foreach player in the team send the message
      return sendPacket(peer, packet->data, packet->dataLength, CHL_COMMUNICATION);
   default:
      //Logging->errorLine("Unknown ChatMessageType\n");
      return sendPacket(peer, packet->data, packet->dataLength, CHL_COMMUNICATION); 
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
    sendPacket(peer, reinterpret_cast<uint8 *>(&stats), sizeof(stats)-2, CHL_LOW_PRIORITY, 2);
    
    return true;
}

bool Game::handleBuyItem(HANDLE_ARGS) {
   
   BuyItemReq *request = reinterpret_cast<BuyItemReq *>(packet->data);
   
   const ItemTemplate* itemTemplate = ItemManager::getInstance()->getItemTemplateById(request->id);
   if(!itemTemplate) {
      return false;
   }
   
   if(peerInfo(peer)->getChampion()->getStats().getGold() < itemTemplate->getPrice()) {
      return true;
   }
   
   int slot;
   
   if((slot = peerInfo(peer)->getChampion()->getInventory().addItem(itemTemplate)) == -1) {
      return true;
   }
   
   peerInfo(peer)->getChampion()->getStats().setGold(peerInfo(peer)->getChampion()->getStats().getGold()-itemTemplate->getPrice());
   notifyInventory(peerInfo(peer)->getChampion());
   
   return true;
}

bool Game::handleSwapItems(HANDLE_ARGS) {
   SwapItemsReq* request = reinterpret_cast<SwapItemsReq*>(packet->data);
   
   if(request->slotFrom > 6 || request->slotTo > 6) {
      return false;
   }
   
   peerInfo(peer)->getChampion()->getInventory().swapItems(request->slotFrom, request->slotTo);
   notifyInventory(peerInfo(peer)->getChampion());
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
    EmotionResponse response;
    response.header.netId = emotion->header.netId;
    response.id = emotion->id;
    return broadcastPacket(reinterpret_cast<uint8 *>(&response), sizeof(response), CHL_S2C);
}
