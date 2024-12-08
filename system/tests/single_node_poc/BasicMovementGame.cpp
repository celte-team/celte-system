#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "CelteEntity.hpp"

namespace dummy {
    class IComponent {
    public:
        virtual void Update(float deltaTime) = 0;
    };

    class GameObject {
    public:
        std::string Name = "GameObject";

        template <typename T> void AddComponent(std::shared_ptr<T> component)
        {
            _components.push_back(component);
        }

        template <typename T> std::shared_ptr<T> GetComponent()
        {
            for (auto component : _components) {
                if (auto casted = std::dynamic_pointer_cast<T>(component)) {
                    return casted;
                }
            }
            return nullptr;
        }

        void Update(float deltaTime)
        {
            for (auto component : _components) {
                component->Update(deltaTime);
            }
        }

    private:
        std::vector<std::shared_ptr<IComponent>> _components;
    };

    /**
     * A Dummy game engine. Very basic, very bad code but easy to
     * understand.
     */
    class Engine {
    public:
        /**
         * Use this method to register a new callback to be called every
         * frame.
         */
        void RegisterGameLoopStep(std::function<void(float)> step)
        {
            _gameLoopSteps.push_back(step);
        }

        int AddGameObject(std::shared_ptr<GameObject> gameObject)
        {
            for (int i = 0; i < _gameObjects.size(); i++) {
                if (_gameObjects[i] == nullptr) {
                    _gameObjects[i] = gameObject;
                    return i;
                }
            }
            _gameObjects.push_back(gameObject);
            return _gameObjects.size() - 1;
        }

        void RemoveGameObject(int index) { _gameObjects[index] = nullptr; }

        std::shared_ptr<GameObject> GetGameObject(int index)
        {
            return _gameObjects[index];
        }

        void Run()
        {
            std::chrono::milliseconds frameTime(
                1000 / 24); // Set frame time to achieve 24 fps
            auto lastTime = std::chrono::steady_clock::now();

            while (true) {
                auto startTime = std::chrono::steady_clock::now();
                auto deltaTime
                    = std::chrono::duration_cast<std::chrono::duration<float>>(
                        startTime - lastTime)
                          .count();
                lastTime = startTime;

                for (auto step : _gameLoopSteps) {
                    step(deltaTime);
                }
                for (auto gameObject : _gameObjects) {
                    if (gameObject != nullptr) {
                        gameObject->Update(deltaTime);
                    }
                }

                auto endTime = std::chrono::steady_clock::now();
                auto elapsedTime
                    = std::chrono::duration_cast<std::chrono::milliseconds>(
                        endTime - startTime);

                if (elapsedTime < frameTime) {
                    std::this_thread::sleep_for(frameTime - elapsedTime);
                }
            }
        }

    private:
        std::vector<std::function<void(float)>> _gameLoopSteps;
        std::vector<std::shared_ptr<GameObject>> _gameObjects;
    };
}

namespace movement_game {
    class Transform : public dummy::IComponent {
    public:
        Transform(std::shared_ptr<dummy::GameObject> gameObject)
            : _gameObject(gameObject)
        {
        }

        void Update(float deltaTime) override
        {
            _x += _velocityX * deltaTime;
            _y += _velocityY * deltaTime;
        }

        void SetVelocity(float x, float y)
        {
            _velocityX = x;
            _velocityY = y;
        }

    private:
        std::shared_ptr<dummy::GameObject> _gameObject;
        int _x = 0;
        int _y = 0;
        float _velocityX = 1;
        float _velocityY = 1;
    };

    class PlayerController : public dummy::IComponent {
    public:
        PlayerController(std::shared_ptr<dummy::GameObject> gameObject)
            : _gameObject(gameObject)
        {
        }

        void Update(float deltaTime) override
        {
            // This is where the player controller logic would go
        }

        void Input(std::string direction)
        {
            if (direction == "up") {
                _gameObject->GetComponent<Transform>()->SetVelocity(0, 1);
            } else if (direction == "down") {
                _gameObject->GetComponent<Transform>()->SetVelocity(0, -1);
            } else if (direction == "left") {
                _gameObject->GetComponent<Transform>()->SetVelocity(-1, 0);
            } else if (direction == "right") {
                _gameObject->GetComponent<Transform>()->SetVelocity(1, 0);
            }
        }

    private:
        std::shared_ptr<dummy::GameObject> _gameObject;
    };

    class CelteEntityWrapper : public dummy::IComponent {
    public:
        CelteEntityWrapper(std::shared_ptr<dummy::GameObject> gameObject,
            std::shared_ptr<celte::CelteEntity> celteEntity)
            : _gameObject(gameObject)
            , _celteEntity(celteEntity)
        {
        }

        // This method should be called when the game object is spawned
        void Start() {
        }

        void Update(float deltaTime) override
        {

        }

        private:
            std::shared_ptr<dummy::GameObject> _gameObject;
            std::shared_ptr<celte::CelteEntity> _celteEntity;
        };
}