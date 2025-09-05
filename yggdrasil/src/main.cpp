#include <cpr/cpr.h>
#include <glm/glm.hpp>
#include <hiredis/hiredis.h>
#include <iostream>
#include <pulsar/Client.h>

int main() {
  std::cout << "Yggdrasil example starting..." << std::endl;

  // GLM usage
  glm::vec3 v(1.0f, 2.0f, 3.0f);
  std::cout << "glm vec: " << v.x << ", " << v.y << ", " << v.z << std::endl;

  // Pulsar client minimal usage (no broker available here)
  try {
    pulsar::ClientConfiguration config;
    pulsar::Client client("pulsar://localhost:6650", config);
    std::cout << "Pulsar client constructed" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Pulsar client error: " << e.what() << std::endl;
  }

  // Hiredis minimal test (non-blocking attempt)
  redisContext *c = redisConnect("127.0.0.1", 6379);
  if (c == NULL || c->err) {
    if (c) {
      std::cerr << "Redis error: " << c->errstr << std::endl;
      redisFree(c);
    } else {
      std::cerr << "Can't allocate redis context" << std::endl;
    }
  } else {
    redisReply *reply = (redisReply *)redisCommand(c, "PING");
    if (reply) {
      std::cout << "Redis PING: " << reply->str << std::endl;
      freeReplyObject(reply);
    }
    redisFree(c);
  }

  // cpr HTTP get
  try {
    auto r = cpr::Get(cpr::Url{"https://httpbin.org/get"});
    std::cout << "HTTP status: " << r.status_code << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "HTTP request failed: " << e.what() << std::endl;
  }

  std::cout << "Yggdrasil example done." << std::endl;
  return 0;
}
