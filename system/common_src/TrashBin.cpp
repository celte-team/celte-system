#include "Runtime.hpp"
#include "TrashBin.hpp"
#include <algorithm>
#include <memory>
#include <vector>

using namespace celte;

void TrashBin::TrashItem(ITrashable *item) {
  std::shared_ptr<ITrashable> ptr(item);
  __asyncCleanup(std::move(ptr));
}

void TrashBin::__asyncCleanup(std::shared_ptr<ITrashable> item) {
  RUNTIME.ScheduleAsyncTask(
      [item = std::move(item)]() mutable { item->__cleanup(); });
}