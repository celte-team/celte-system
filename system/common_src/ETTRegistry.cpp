/*
** CELTE, 2025
** refacto

** Team Members:
** Eliot Janvier
** Clement Toni
** Ewen Briand
** Laurent Jiang
** Thomas Laprie

** File description:
** ETTRegistry
*/

#include "ETTRegistry.hpp"
using namespace celte;

ETTRegistry& ETTRegistry::GetInstance()
{
    static ETTRegistry instance;
    return instance;
}

void ETTRegistry::RegisterEntity(const Entity& e)
{
    accessor acc;
    if (_entities.insert(acc, e.id)) {
        acc->second = e;
    } else {
        throw std::runtime_error("Entity with id " + e.id + " already exists.");
    }
}

void ETTRegistry::UnregisterEntity(const std::string& id)
{
    accessor acc;
    if (_entities.find(acc, id)) {
        if (acc->second.isValid) {
            throw std::runtime_error(
                "Cannot unregister a valid entity as it is still in use.");
        }
        _entities.erase(acc);
    }
}

void ETTRegistry::PushTaskToEngine(const std::string& id,
    std::function<void()> task)
{
    accessor acc;
    if (_entities.find(acc, id)) {
        acc->second.executor.PushTaskToEngine(task);
    }
}

std::optional<std::function<void()>>
ETTRegistry::PollEngineTask(const std::string& id)
{
    accessor acc;
    if (_entities.find(acc, id)) {
        return acc->second.executor.PollEngineTask();
    }
    return std::nullopt;
}

std::string_view ETTRegistry::GetEntityOwner(const std::string& id)
{
    accessor acc;
    if (_entities.find(acc, id)) {
        return acc->second.ownerSN;
    }
    return "";
}

void ETTRegistry::SetEntityOwner(const std::string& id,
    const std::string& owner)
{
    accessor acc;
    if (_entities.find(acc, id)) {
        acc->second.ownerSN = owner;
    }
}

std::string_view ETTRegistry::GetEntityOwnerContainer(const std::string& id)
{
    accessor acc;
    if (_entities.find(acc, id)) {
        return acc->second.ownerContainerId;
    }
    return "";
}

void ETTRegistry::SetEntityOwnerContainer(const std::string& id,
    const std::string& ownerContainer)
{
    accessor acc;
    if (_entities.find(acc, id)) {
        acc->second.ownerContainerId = ownerContainer;
    }
}

bool ETTRegistry::IsEntityQuarantined(const std::string& id)
{
    accessor acc;
    if (_entities.find(acc, id)) {
        return acc->second.quarantine;
    }
    return false;
}

void ETTRegistry::SetEntityQuarantined(const std::string& id, bool quarantine)
{
    accessor acc;
    if (_entities.find(acc, id)) {
        acc->second.quarantine = quarantine;
    }
}

bool ETTRegistry::IsEntityValid(const std::string& id)
{
    accessor acc;
    if (_entities.find(acc, id)) {
        return acc->second.isValid;
    }
    return false;
}

void ETTRegistry::SetEntityValid(const std::string& id, bool isValid)
{
    accessor acc;
    if (_entities.find(acc, id)) {
        acc->second.isValid = isValid;
    }
}

void ETTRegistry::Clear() { _entities.clear(); }

void ETTRegistry::PushReplToEntity(const std::string& id, Replicator::ReplBlob blob)
{
    accessor acc;
    if (_entities.find(acc, id))
        acc->second._replPushed.push(blob);
}

Replicator::ReplBlob ETTRegistry::PullReplFromEntity(const std::string& id)
{
    accessor acc;
    Replicator::ReplBlob repl;

    if (_entities.find(acc, id))
        if (!acc->second._replPushed.try_pop(repl))
            return repl;

    return "";
}
