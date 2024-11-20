#include "Logger.hpp"
#include "Replicator.hpp"
#include <cstring>
#include <msgpack.hpp>

namespace celte {
namespace runtime {

void Replicator::notifyDataChanged(const std::string &name) {
  auto it = _replicatedData.find(name);
  if (it != _replicatedData.end()) {
    it->second.hasChanged = true;
  }
}

void Replicator::ResetDataChanged() {
  if (_replicatedData.size() ==
      0) { // plz rework this language wtf. why do i need that before the for
           // below (i checked, it's not a memory leak. feel free to double
           // check though)
    return;
  }
  for (auto &entry : _replicatedData) {
    entry.second.hasChanged = false;
  }
}

void Replicator::Overwrite(const ReplBlob &blob, bool active) {
  size_t offset = 0;
  msgpack::unpacker unpacker;
  unpacker.reserve_buffer(blob.size());
  std::memcpy(unpacker.buffer(), blob.data(), blob.size());
  unpacker.buffer_consumed(blob.size());

  if (active) {
    __overwriteActiveData(blob, unpacker);
  } else {
    __overwriteData(blob, unpacker);
  }
}

void Replicator::__overwriteData(const ReplBlob &blob,
                                 msgpack::unpacker &unpacker) {
  msgpack::object_handle oh;
  while (unpacker.next(oh)) {
    msgpack::object obj = oh.get();
    std::string key;
    std::string rawData;

    obj.convert(key);
    unpacker.next(oh);
    oh.get().convert(rawData);

    auto it = _replicatedData.find(key);
    if (it != _replicatedData.end()) {
      std::memcpy(it->second.dataPtr, rawData.data(), rawData.size());
      it->second.hasChanged = false;
    } else {
      throw std::runtime_error("Key not found in data: " + key);
    }
  }
}

Replicator::ReplBlob Replicator::GetBlob() {
  ReplBlob blob;
  msgpack::sbuffer sbuf;
  msgpack::packer<msgpack::sbuffer> packer(sbuf);

  if (_replicatedData.size() == 0) { // wtf cpp why do i need that
    return blob;
  }
  for (const auto &entry : _replicatedData) {
    if (entry.second.hasChanged) {
      packer.pack(entry.first);
      std::string binData(static_cast<char *>(entry.second.dataPtr),
                          entry.second.dataSize);
      packer.pack(binData);
    }
  }
  blob.assign(sbuf.data(), sbuf.size()); // Assign the serialized data to blob
  return blob;
}

Replicator::ReplBlob Replicator::GetActiveBlob() {
  ReplBlob blob;
  msgpack::sbuffer sbuf;
  msgpack::packer<msgpack::sbuffer> packer(sbuf);

  if (_activeReplicatedData.size() == 0) {
    return blob;
  }
  for (auto &[key, replData] : _activeReplicatedData) {
    int checksum = __computeCheckSum(replData.dataPtr, replData.dataSize);
    if (replData.hash != checksum) {
      replData.hash = checksum;
      packer.pack(key);
      std::string binData(static_cast<char *>(replData.dataPtr),
                          replData.dataSize);
      packer.pack(binData);
    }
  }
  blob.assign(sbuf.data(), sbuf.size());
  return blob;
};

void Replicator::__overwriteActiveData(const ReplBlob &blob,
                                       msgpack::unpacker &unpacker) {

  msgpack::object_handle oh;
  while (unpacker.next(oh)) {
    msgpack::object obj = oh.get();
    std::string key;
    std::string rawData;

    obj.convert(key);
    unpacker.next(oh);
    oh.get().convert(rawData);

    auto it = _activeReplicatedData.find(key);
    if (it != _activeReplicatedData.end()) {
      std::memcpy(it->second.dataPtr, rawData.data(), rawData.size());
      it->second.hash = __computeCheckSum(it->second.dataPtr, rawData.size());
    } else {
      throw std::runtime_error("Key not found in active data: " + key);
    }
  }
}

int Replicator::__computeCheckSum(void *dataPtr, size_t size) {
  int hash = 0;
  for (size_t i = 0; i < size; i++) {
    hash = 31 * hash + reinterpret_cast<char *>(dataPtr)[i];
  }
  return hash;
}

} // namespace runtime
} // namespace celte