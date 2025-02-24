#pragma once
#include <memory>

namespace celte {
class TrashBin; // forward declaration
class ITrashable {
public:
  /// @brief Virtual destructor to ensure proper cleanup of derived classes.
  virtual ~ITrashable() = default;

protected:
  /// @brief Starts the cleanup process of the object. This method should block
  /// until the object can safely be deleted.
  /// @warning This method MUST be thread safe.
  virtual void __cleanup() = 0;

  friend TrashBin;
};

/// @brief The TrashBin class is used to store objects scheduled for deletion
/// until they have been cleaned up and can be deleted correctly.
class TrashBin {
public:
  /// @brief Sends an item to the trash bin. Takes ownership of the item, which
  /// will no longer be usable by the previous owner. The item will be deleted
  /// when all of its resources have been cleaned up.
  /// @note The item must inherit publicly from the ITrashable class.
  /// @param item
  template <typename T>
  typename std::enable_if<std::is_base_of<ITrashable, T>::value>::type
  TrashItem(T &&item) {
    auto ptr = std::make_shared<T>(std::move(item));
    __asyncCleanup(std::move(ptr));
  }

  template <typename T>
  typename std::enable_if<std::is_base_of<ITrashable, T>::value>::type
  TrashItem(std::shared_ptr<T> item) {
    __asyncCleanup(std::move(item));
  }

  void TrashItem(ITrashable *item);

private:
  /// @brief Asynchronously starts cleaning up an object, whose destructor will
  /// be called after the cleanup is done.
  void __asyncCleanup(std::shared_ptr<ITrashable> item);
};
} // namespace celte
