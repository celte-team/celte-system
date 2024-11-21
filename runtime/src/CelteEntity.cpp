#include "CelteEntity.hpp"
#include "CelteChunk.hpp"
#include "CelteEntityManagementSystem.hpp"
#include "CelteGrapeManagementSystem.hpp"
#include "CelteHooks.hpp"
#include "CelteRuntime.hpp"
#include "Logger.hpp"
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
        try {
            auto& chunk = chunks::CelteGrapeManagementSystem::GRAPE_MANAGER()
                              .GetGrapeByPosition(x, y, z)
                              .GetChunkByPosition(x, y, z);
            OnChunkTakeAuthority(chunk);
        } catch (std::out_of_range& e) {
            RUNTIME.Err() << "Entity is not in any grape: " << e.what() << std::endl;
        }

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
        _ownerChunk = &chunk;
    }

    void CelteEntity::Tick()
    {
        // nothing yet :)
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

        // lazy replication, only send data if user notified of change using
        // NotifyDataChanged
        std::string blob = _replicator.GetBlob();
        // active replication, send all data that has changed
        std::string activeBlob = _replicator.GetActiveBlob();

        if (not blob.empty()) {
            _ownerChunk->ScheduleReplicationDataToSend(_uuid, blob);
        }

        if (not activeBlob.empty()) {
            _ownerChunk->ScheduleReplicationDataToSend(_uuid, activeBlob, true);
        }
    }
#endif

    const std::string& CelteEntity::GetInformationToLoad() const
    {
        return _informationToLoad;
    }

    void CelteEntity::DownloadReplicationData(const std::string& blob,
        bool active)
    {
        if (blob.empty()) {
            return;
        }
        _replicator.Overwrite(blob, active);
    }

    void CelteEntity::sendInputToKafka(std::string inputName, bool pressed)
    {

        msgpack::sbuffer sbuf;
        msgpack::packer<msgpack::sbuffer> packer(sbuf);

        packer.pack(std::make_tuple(inputName, pressed, _uuid));
        // packer.pack(pressed);

        std::string value(sbuf.data(), sbuf.size());
        std::cout << ("Inside test logic\n") << std::flush;

        std::string chunkId = _ownerChunk->GetCombinedId();
        std::string cp = chunkId + "." + tp::INPUT;

        std::cout << "JE SUIS PASSER [" << chunkId << "]\n"
                  << std::flush;

        std::cout << "Send message to " << cp << std::endl
                  << std::flush;

        KPOOL.Send({ .topic = cp, .value = value });
    }

} // namespace celte
