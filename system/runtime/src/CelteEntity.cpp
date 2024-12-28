#include "CelteEntity.hpp"
#include "CelteChunk.hpp"
#include "CelteEntityManagementSystem.hpp"
#include "CelteGrapeManagementSystem.hpp"
#include "CelteHooks.hpp"
#include "CelteRuntime.hpp"
#include "Logger.hpp"
#include "base64.hpp"
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <iostream>

namespace celte {
    void CelteEntity::SetInformationToLoad(const std::string& info)
    {
        _informationToLoad = std::string();

        for (int i = 0; i < info.size(); i++) {
            _informationToLoad += info[i];
        }
    }

    void CelteEntity::OnSpawn(float x, float y, float z, const std::string& uuid)
    {
        if (uuid.empty()) {
            _uuid = boost::uuids::to_string(boost::uuids::random_generator()());
        } else {
            _uuid = uuid;
        }

        ENTITIES.RegisterEntity(shared_from_this());
        _isSpawned = true; // will cause errors if OnSpawn is called but the entity is
                           // not actually spawned in the game.
    }

    void CelteEntity::OnDestroy()
    {
        ENTITIES.UnregisterEntity(shared_from_this());
        // TODO: Notify all peers of the destruction if in server mode and entity is
        // locally owned.
    }

    void CelteEntity::OnChunkTakeAuthority(celte::chunks::Chunk& chunk)
    {
        ENTITIES.UnquaranteenEntity(_uuid);
        if (_ownerChunk != nullptr) {
            _ownerChunk->IncNOwnedEntities(-1);
        }
        _ownerChunk = &chunk;
        _ownerChunk->IncNOwnedEntities(1);
    }

    void CelteEntity::Tick()
    {
        while (not _engineLoopCallbackQueue.empty()) {
            auto callback = _engineLoopCallbackQueue.pop();
            if (callback)
                callback();
        }
#ifdef CELTE_SERVER_MODE_ENABLED
        if (_ownerChunk == nullptr) {
            return; // not in the network yet
        }
        auto& ownerGrape = GRAPES.GetGrape(_ownerChunk->GetGrapeId());
        if (ownerGrape.IsLocallyOwned()) {
            ownerGrape.GetReplicationGraph().AssignEntityByAffinity(*this);
        }
#endif
    }

#ifdef CELTE_SERVER_MODE_ENABLED
    void CelteEntity::UploadReplicationData()
    {
        if (not(_ownerChunk and GRAPES.GetGrape(_ownerChunk->GetGrapeId()).GetOptions().isLocallyOwned)) {
            return;
        }

        if (not _ownerChunk) {
            return;
        }

        std::string blob = _replicator.GetBlob(false);

        if (not blob.empty()) {
            blob = base64_encode(reinterpret_cast<const unsigned char*>(blob.c_str()),
                blob.size());
            _ownerChunk->ScheduleReplicationDataToSend(_uuid, blob);
        }
    }
#endif

    void CelteEntity::RegisterReplicatedValue(
        const std::string& name, std::function<std::string()> get,
        std::function<void(std::string)> set)
    {
        _replicator.RegisterReplicatedValue(name, get, set);
    }

    const std::string& CelteEntity::GetInformationToLoad() const
    {
        return _informationToLoad;
    }

    void CelteEntity::DownloadReplicationData(const std::string& blob)
    {
        if (blob.empty()) {
            return;
        }
        std::string blobDecoded = base64_decode(blob);
        try {
            _replicator.Overwrite(blobDecoded);
        } catch (std::exception& e) {
            std::cerr << "Error while downloading replication data: " << e.what()
                      << std::endl;
        }
    }

    std::string CelteEntity::GetProps() { return _replicator.GetBlob(true); }

    void CelteEntity::sendInputToKafka(std::string inputName, bool pressed, float x, float y)
    {
        if (_ownerChunk == nullptr) {
            return; // can't send inputs if not owned by a chunk
        }
        std::string chunkId = _ownerChunk->GetCombinedId();
        std::string cp = chunkId + "." + tp::INPUT;
        celte::runtime::CelteInputSystem::InputUpdate_s req = {
            .name = inputName, .pressed = pressed, .uuid = _uuid, .x = x, .y = y
        };

        CINPUT.GetWriterPool().Write<celte::runtime::CelteInputSystem::InputUpdate_s>(
            cp, req);
    }

    bool CelteEntity::IsOwnedByCurrentPeer() const
    {
        if (not _ownerChunk) {
            return false;
        }
        return _ownerChunk->GetConfig().isLocallyOwned;
    }
} // namespace celte
