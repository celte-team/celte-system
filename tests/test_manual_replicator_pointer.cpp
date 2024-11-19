#include "Replicator.hpp"
#include <iostream>

int main() {
  celte::runtime::Replicator replicator;
  int x = 41;

  std::cout << "address of x is " << &x << std::endl;

  replicator.registerActiveValue("property", &x);
  x++;
  auto blob = replicator.GetActiveBlob();
  x = 234234;
  replicator.Overwrite(blob, true);

  std::cout << "final x: " << x << std::endl;

  return 0;
}