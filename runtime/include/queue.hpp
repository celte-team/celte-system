#pragma once

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>

/*

MIT License

Copyright (c) 2021 Unbinilium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

 */

namespace ubn {
constexpr std::size_t const hardware_constructive_interference_size{
    2 * sizeof(std::max_align_t)};

template <typename T> class queue {
public:
  constexpr explicit queue(const std::size_t capacity = UINT16_MAX)
      : m_capacity(capacity), m_allocator(std::allocator<container_t<T>>()),
        m_head(0ul), m_tail(0ul) {
    m_container = m_allocator.allocate(m_capacity + 1);
    for (const auto &i : std::views::iota(0ul, m_capacity)) {
      new (&m_container[i]) container_t<T>();
    }
  }

  constexpr queue(const queue &) = delete;
  constexpr queue &operator=(const queue &) = delete;

  ~queue(void) noexcept {
    for (const auto &i : std::views::iota(0ul, m_capacity)) {
      m_container[i].~container_t();
    }
    m_allocator.deallocate(m_container, m_capacity + 1);
  }

  constexpr void push(const T &v) noexcept { emplace(v); }
  constexpr void push(std::convertible_to<T> auto &&v) noexcept {
    emplace(std::forward<decltype(v)>(v));
  }

  constexpr T pop() noexcept {
    std::optional<T> res;

    const auto tail{m_tail.fetch_add(1, std::memory_order::acquire)};
    const auto head{(tail / m_capacity) * 2 + 1};
    auto *container{&m_container[tail % m_capacity]};

    while (true) {
      const auto now{container->m_idx_.load(std::memory_order::acquire)};
      if (now == head) {
        break;
      }
      container->m_idx_.wait(now, std::memory_order::relaxed);
    }

    res = std::move(container->move());
    container->destruct();

    container->m_idx_.store(head + 1, std::memory_order::release);
    container->m_idx_.notify_all();

    return std::move(*res);
  }

  // added by Celte
  bool empty() const noexcept {
    return m_head.load(std::memory_order::acquire) ==
           m_tail.load(std::memory_order::acquire);
  }

protected:
  template <typename... Args> constexpr void emplace(Args &&...args) noexcept {
    const auto tail{m_head.fetch_add(1, std::memory_order::acquire)};
    const auto head{(tail / m_capacity) * 2};
    auto *container{&m_container[tail % m_capacity]};

    while (true) {
      const auto now{container->m_idx_.load(std::memory_order::acquire)};
      if (now == head) {
        break;
      }
      container->m_idx_.wait(now, std::memory_order::relaxed);
    }

    container->construct(std::forward<Args>(args)...);

    container->m_idx_.store(head + 1, std::memory_order::release);
    container->m_idx_.notify_all();
  }

  template <typename V> struct container_t {
  public:
    ~container_t() noexcept {
      if (m_idx_.load(std::memory_order::acquire)) {
        destruct();
      }
    }

    template <typename... Args>
    constexpr void construct(Args &&...args) noexcept {
      new (&storage_t) V(std::forward<Args>(args)...);
    }
    constexpr void destruct() noexcept {
      reinterpret_cast<V *>(&storage_t)->~V();
    }
    constexpr V &&move() noexcept { return reinterpret_cast<V &&>(storage_t); }

    alignas(hardware_constructive_interference_size) std::atomic_size_t
        mutable m_idx_{0};

  private:
    typename std::aligned_storage_t<sizeof(V), alignof(V)> storage_t;
  };

private:
  const std::size_t m_capacity;
  std::allocator<container_t<T>> m_allocator;
  container_t<T> *m_container;

  alignas(hardware_constructive_interference_size) std::atomic_size_t m_head;
  alignas(hardware_constructive_interference_size) std::atomic_size_t m_tail;
};
} // namespace ubn