#include "Runtime.hpp"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

using namespace celte;

static std::string make_uuid() {
  boost::uuids::random_generator gen;
  boost::uuids::uuid id = gen();
#ifdef CELTE_SERVER_MODE_ENABLED
  return "sn." + boost::uuids::to_string(id);
#else
  return "cl." + boost::uuids::to_string(id);
#endif
}

Runtime::Runtime() : _uuid(make_uuid()) {}

Runtime &Runtime::GetInstance() {
  static Runtime instance;
  return instance;
}

void Runtime::ConnectToCluster() {
  // Implementation here
}

void Runtime::ConnectToCluster(const std::string &address, int port) {
  // Implementation here
}

void Runtime::Tick() {
  // Implementation here
}