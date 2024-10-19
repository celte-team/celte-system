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
  for (auto &entry : _replicatedData) {
    entry.second.hasChanged = false;
  }
}
Replicator::ReplBlob Replicator::GetBlob() {
  ReplBlob blob;
  msgpack::sbuffer sbuf;
  msgpack::packer<msgpack::sbuffer> packer(sbuf);

  for (const auto &entry : _replicatedData) {
    if (entry.second.hasChanged) {
      packer.pack(entry.first);
      packer.pack(entry.second.dataSize);
      packer.pack(msgpack::type::raw_ref(
          static_cast<char *>(entry.second.dataPtr), entry.second.dataSize));
    }
  }

  blob.assign(sbuf.data(), sbuf.size()); // Assign the serialized data to blob
  return blob;
}

void Replicator::Overwrite(const ReplBlob &blob) {

  size_t offset = 0;
  msgpack::unpacker unpacker;
  unpacker.reserve_buffer(blob.size());
  std::memcpy(unpacker.buffer(), blob.data(), blob.size());
  unpacker.buffer_consumed(blob.size());

  msgpack::object_handle oh;
  while (unpacker.next(oh)) {
    msgpack::object obj = oh.get();
    std::string key;
    size_t dataSize;
    msgpack::type::raw_ref rawData;

    obj.convert(key); // Unpack the key
    unpacker.next(oh);
    oh.get().convert(dataSize); // Unpack the size of the data
    unpacker.next(oh);
    oh.get().convert(rawData); // Unpack the data

    auto it = _replicatedData.find(key);
    if (it != _replicatedData.end()) {
      std::memcpy(it->second.dataPtr, rawData.ptr, dataSize);
      it->second.hasChanged = false;
    }
  }
}

} // namespace runtime
} // namespace celte