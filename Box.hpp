#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <type_traits>
#include <utility>

template <class T>
class Box {
private:
    bool is_empty;
    alignas(T) char payload[sizeof(T)];
    std::mutex mutex;
    std::condition_variable reader_cv;
    std::condition_variable writer_cv;
public:
    Box(): is_empty{true} {}
    template <class U,
              class = std::enable_if_t<std::is_constructible_v<T, U&&>>>
    explicit Box(U&& init): is_empty{false} {
        new(payload) T(std::forward<U>(init));
    }
    ~Box() {
        if (!is_empty) {
            reinterpret_cast<T*>(payload)->~T();
        }
    }

    template <class U,
              class = std::enable_if_t<std::is_assignable_v<T&, U&&>>>
    void put(U&& value) {
        {
            std::unique_lock<std::mutex> lock{mutex};
            writer_cv.wait(lock, [this]{ return is_empty; });
            *reinterpret_cast<T*>(payload) = std::forward<U>(value);
            is_empty = false;
        }
        reader_cv.notify_one();
    }

    template <class... Args>
    void emplace(Args&&... args) {
        {
            std::unique_lock<std::mutex> lock{mutex};
            writer_cv.wait(lock, [this]{ return is_empty; });
            reinterpret_cast<T*>(payload)->~T();
            new(payload) T(std::forward<Args>(args)...);
            is_empty = false;
        }
        reader_cv.notify_one();
    }

    template <class = std::enable_if_t<std::is_move_constructible_v<T>>>
    T take() {
        std::unique_lock<std::mutex> lock{mutex};
        reader_cv.wait(lock, [this]{ return !is_empty; });
        T retval = std::move(*reinterpret_cast<T*>(payload));
        is_empty = true;
        lock.unlock();
        writer_cv.notify_one();
        return retval;
    }

    void discard() {
        std::unique_lock<std::mutex> lock{mutex};
        reader_cv.wait(lock, [this]{ return !is_empty; });
        reinterpret_cast<T*>(payload)->~T();
        is_empty = true;
        lock.unlock();
        writer_cv.notify_one();
    }

    bool try_discard() {
        std::unique_lock<std::mutex> lock{mutex};
        bool retval = !is_empty;
        if (!is_empty) {
            reinterpret_cast<T*>(payload)->~T();
            lock.unlock();
            writer_cv.notify_one();
        }
        return retval;
    }

    template <class = std::enable_if_t<std::is_copy_constructible_v<T>>>
    T peek() {
        std::unique_lock<std::mutex> lock{mutex};
        reader_cv.wait(lock, [this] { return !is_empty; });
        return *reinterpret_cast<T*>(payload);
    }

    template <class Func, class = std::enable_if_t<std::is_invocable_v<Func&&, const T*>>>
    std::invoke_result_t<Func&&, const T*> peek(Func&& func) {
        std::unique_lock<std::mutex> lock{mutex};
        reader_cv.wait(lock, [this] { return !is_empty; });
        return std::invoke(std::forward<Func>(func), reinterpret_cast<const T*>(payload));
    }

    bool empty() {
        std::unique_lock<std::mutex> lock{mutex};
        return is_empty;
    }
};
