#include <atomic>
#include "shared_ptr.h"

namespace neatlib {
    namespace unsafe {
        template<typename T>
        class atomic_shared_ptr {
            template<typename U> using shared_ptr = ::neatlib::unsafe::shared_ptr<U>;
        public:
            atomic_shared_ptr() noexcept: ptr(nullptr) {}

            atomic_shared_ptr(const ::neatlib::unsafe::shared_ptr<T> &desired) noexcept: ptr(desired.get()) {}

            atomic_shared_ptr(const atomic_shared_ptr &) = delete;

            void operator=(::neatlib::unsafe::shared_ptr<T> desired) noexcept { ptr = desired.get(); }

            void operator=(const atomic_shared_ptr &) = delete;

            bool is_lock_free() const noexcept { return true; }

            void store(::neatlib::unsafe::shared_ptr<T> desired,
                       std::memory_order order = std::memory_order_seq_cst) noexcept {
                ptr.store(desired.get(), order);
            }

            ::neatlib::unsafe::shared_ptr<T>
            load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
                T *p = ptr.load(order);
                return shared_ptr<T>(p);
            }

            ::neatlib::unsafe::shared_ptr<T> exchange(::neatlib::unsafe::shared_ptr<T> desired,
                                                      std::memory_order order = std::memory_order_seq_cst) noexcept {
                ptr.exchange(desired.get(), order);
            }

            bool compare_exchange_weak(::neatlib::unsafe::shared_ptr<T> &expected,
                                       const ::neatlib::unsafe::shared_ptr<T> &desired,
                                       std::memory_order success,
                                       std::memory_order failure) noexcept {
                return ptr.compare_exchange_weak(expected.ptr, desired.ptr, success, failure);
            }

            bool compare_exchange_weak(shared_ptr<T> &expected, shared_ptr<T> &&desired,
                                       std::memory_order success, std::memory_order failure) noexcept {
                return ptr.compare_exchange_weak(expected.ptr, desired.ptr, success, failure);
            }

            bool compare_exchange_weak(shared_ptr<T> &expected, const shared_ptr<T> &desired,
                                       std::memory_order order = std::memory_order_seq_cst) noexcept {
                return ptr.compare_exchange_weak(expected.ptr, desired.ptr, order);
            }

            bool compare_exchange_weak(shared_ptr<T> &expected, shared_ptr<T> &&desired,
                                       std::memory_order order = std::memory_order_seq_cst) noexcept {
                return ptr.compare_exchange_weak(expected.ptr, desired.ptr, order);
            }

            bool compare_exchange_strong(shared_ptr<T> &expected, const shared_ptr<T> &desired,
                                         std::memory_order success, std::memory_order failure) noexcept {
                return ptr.compare_exchange_strong(expected.ptr, desired.ptr, success, failure);
            }

            bool compare_exchange_strong(shared_ptr<T> &expected, shared_ptr<T> &&desired,
                                         std::memory_order success, std::memory_order failure) noexcept {
                return ptr.compare_exchange_strong(expected.ptr, desired.ptr, success, failure);
            }

            bool compare_exchange_strong(shared_ptr<T> &expected, const shared_ptr<T> &desired,
                                         std::memory_order order = std::memory_order_seq_cst) noexcept {
                return ptr.compare_exchange_strong(expected.ptr, desired.ptr, order);
            }

            bool compare_exchange_strong(shared_ptr<T> &expected, shared_ptr<T> &&desired,
                                         std::memory_order order = std::memory_order_seq_cst) noexcept {
                return ptr.compare_exchange_strong(expected.ptr, desired.ptr, order);
            }

        private:
            std::atomic<T *> ptr;
        };

    } // unsafe
} // neatlib