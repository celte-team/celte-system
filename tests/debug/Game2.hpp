#pragma once
#include "CelteEntity.hpp"
#include "CelteRuntime.hpp"
#include <iostream>
#include <unordered_map>
#include <vector>

class GameObject {
public:
  char repr;
  std::shared_ptr<celte::CelteEntity> entity;
  int x;
  int y;

  GameObject(char repr, std::shared_ptr<celte::CelteEntity> entity, int x,
             int y)
      : repr(repr), entity(entity), x(x), y(y) {}
};

/**
 * @brief The world is a 2D grid of characters.
 * Each time a zone is loaded, the world is updated with the new zone, and
 * scaled up to fit the size of the zone. If a coordinate has no value, it is
 * left empty.
 */
class World {
public:
  void LoadZone(int x, int y, int size, char repr) {
    if (size < 0) {
      throw std::invalid_argument("Size must be a positive integer.");
    }
    if (x < 0 || y < 0) {
      throw std::invalid_argument("Coordinates must be positive integers.");
    }
    if (x + size > _world.size() || y + size > _world[0].size()) {
      // Resize the world to fit the new zone and fill new cells with spaces
      for (int i = 0; i < x + size; i++) {
        if (i >= _world.size()) {
          _world.push_back(std::vector<char>(_world[0].size(), ' '));
        }
        for (int j = 0; j < y + size; j++) {
          if (j >= _world[i].size()) {
            _world[i].push_back(' ');
          }
        }
      }
      for (int i = x; i < x + size; i++) {
        for (int j = y; j < y + size; j++) {
          _world[i][j] = repr;
        }
      }
    }
  }

  void
  Dump(std::unordered_map<std::string, std::shared_ptr<GameObject>> &objects) {
    std::cout << ">>> world dump " << _world.size() << "x" << _world[0].size()
              << std::endl;
    for (int i = 0; i < _world.size(); i++) {
      for (int j = 0; j < _world[i].size(); j++) {
        bool objectFound = false;
        for (const auto &pair : objects) {
          if (pair.second->x == i && pair.second->y == j) {
            std::cout << pair.second->repr;
            objectFound = true;
            break;
          }
        }
        if (!objectFound) {
          std::cout << _world[i][j];
        }
      }
      std::cout << std::endl;
    }
    std::cout << "<<< world dump" << std::endl;
  }

  int GetXDim() { return _world.size(); }
  int GetYDim() { return _world[0].size(); }

private:
  std::vector<std::vector<char>> _world = {{' '}};
};

class Game {
public:
  World world;
  std::unordered_map<std::string, std::shared_ptr<GameObject>> objects;

  int GetNPlayers() { return objects.size(); }

  void LoadArea(const std::string name, bool locallyOwned) {
    int x, y;
    if (name == "LeChateauDuMechant") {
      world.LoadZone(0, 0, 10, '.');
      x = 5;
      y = 5;
    } else {
      world.LoadZone(0, 10, 10, '~');
      x = 5;
      y = 15;
    }

    // if the client is loading the area where the player will be spawning, then
    // we should call the request spawn RP after the zone is done loading. This
    // is done in the .then of the grape registration to ensure that the client
    // is done connecting to the grape's topics before requesting to spawn.
    std::function<void()> then = nullptr;
#ifndef CELTE_SERVER_MODE_ENABLED
    if (name == "LeChateauDuMechant") { // spawn zone is hard coded here but
                                        // it's technically just the zone called
                                        // in the loadGrape hook.
      then = [name]() {
        std::cout << ">> CLIENT IS READY TO SPAWN <<" << std::endl;
        RUNTIME.RequestSpawn(RUNTIME.GetUUID(), name, 0, 0, 0);
      };
    }
#endif

    glm::vec3 grapePosition(x, y, 0);
    auto grapeOptions =
        celte::chunks::GrapeOptions{.grapeId = name,
                                    .subdivision = 1,
                                    .position = grapePosition,
                                    .size = glm::vec3(10, 10, 10),
                                    .localX = glm::vec3(1, 0, 0),
                                    .localY = glm::vec3(0, 1, 0),
                                    .localZ = glm::vec3(0, 0, 1),
                                    .isLocallyOwned = locallyOwned,
                                    .then = then};
    celte::chunks::CelteGrapeManagementSystem::GRAPE_MANAGER().RegisterGrape(
        grapeOptions);
    std::cout << ">> Grape " << name
              << " loaded, owned locally: " << ((locallyOwned) ? "yes" : "no")
              << " <<" << std::endl;
  }

  void AddObject(const std::string &uuid, char repr, int x, int y) {
    auto entity = std::make_shared<celte::CelteEntity>();
    entity->SetInformationToLoad(std::to_string(repr));
    entity->OnSpawn(x, y, 0, uuid);
    objects[uuid] = std::make_shared<GameObject>(repr, entity, x, y);
    // entity->RegisterActiveProperty("x", &objects[uuid]->x);
    // entity->RegisterActiveProperty("y", &objects[uuid]->y);
    entity->RegisterReplicatedValue(
        "x",
        [uuid, this]() -> std::string {
          return std::to_string(objects[uuid]->x);
        },
        [uuid, this](std::string value) {
          objects[uuid]->x = std::stoi(value);
        });

    entity->RegisterReplicatedValue(
        "y",
        [uuid, this]() -> std::string {
          return std::to_string(objects[uuid]->y);
        },
        [uuid, this](std::string value) {
          objects[uuid]->y = std::stoi(value);
        });
  }
};