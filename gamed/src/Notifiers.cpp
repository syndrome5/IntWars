#include "stdafx.h"
#include "Game.h"
#include "Packets.h"

#include <iostream>

using namespace std;

void Game::notifyMinionSpawned(Minion* m) {
   MinionSpawn ms(m);
   sendPacket(currentPeer, ms, CHL_S2C);
   notifySetHealth(m);
}

void Game::notifySetHealth(Unit* u) {
   SetHealth sh(u);
   sendPacket(currentPeer, sh, CHL_S2C);
}

void Game::notifyUpdatedStats(Unit* u) {
   UpdateStats us(u);
   sendPacket(currentPeer, us, CHL_LOW_PRIORITY, 2);
}

void Game::notifyMovement(Object* o) {
   const std::vector<MovementVector>& waypoints = o->getWaypoints();
   MovementAns *answer = MovementAns::create(waypoints.size()*2);
   
   for(int i = 0; i < waypoints.size(); i++) {
      printf("     Vector %i, x: %f, y: %f\n", i, 2.0 * waypoints[i].x + MAP_WIDTH, 2.0 * waypoints[i].y + MAP_HEIGHT);
   }
   
   answer->nbUpdates = 1;
   answer->netId = o->getNetId();
   for(int i = 0; i < waypoints.size(); i++) {
      answer->getVector(i)->x = waypoints[i].x;
      answer->getVector(i)->y = waypoints[i].y;
   }
   
   broadcastPacket(reinterpret_cast<uint8 *>(answer), answer->size(), 4);
   MovementAns::destroy(answer);
}
