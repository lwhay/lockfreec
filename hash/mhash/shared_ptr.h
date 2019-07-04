#ifndef NEATLIB_UNSAFE_SHARED_PTR
#define NEATLIB_UNSAFE_SHARED_PTR

#include <utility>

namespace neatlib {
    namespace unsafe {

        template<typename T>
        class atomic_shared_ptr;

        template<typename T>
        class shared_ptr {
            friend class atomic_shared_ptr<T>;

        public:
            shared_ptr() : ptr(nullptr) {}

            shared_ptr(T *p) : ptr(p) {}

            ~shared_ptr() = default; // this is unsafe
            shared_ptr(const shared_ptr &rhs) = default; // there is no counter
            shared_ptr &operator=(const shared_ptr &rhs) = default; // like the above
            template<class Y>
            shared_ptr &operator=(const shared_ptr<Y> &r) noexcept {
                ptr = (T *) r.ptr;
            }


            template<class Y>
            shared_ptr(const shared_ptr<Y> &r) noexcept: ptr((T *) r.get()) {}

            void reset() noexcept { ptr = nullptr; }

            void reset(T *p) { ptr = p; }

            void swap(shared_ptr &rhs) {
                std::swap(ptr, rhs->ptr);
            }

            T *get() const {
                return ptr;
            }

            T &operator*() const noexcept { return *ptr; }

            T *operator->() const noexcept { return ptr; }

            operator bool() const noexcept { return ptr != nullptr; }

        private:
            T *ptr;
        };

        template<class T>
        bool operator==(const shared_ptr<T> &lhs, std::nullptr_t rhs) noexcept {
            return lhs.get() == nullptr;
        }

        template<class T, class U>
        bool operator==(const shared_ptr<T> &lhs, const shared_ptr<U> &rhs) noexcept {
            return static_cast<void *>(lhs.get()) == static_cast<void *>(rhs.get());
        }
    } // neatlib
} // unsafe

#endif // NEATLIB_UNSAFE_SHARED_PTR