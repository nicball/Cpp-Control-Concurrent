#pragma once

#include "Box.hpp"
#include <exception>
#include <optional>
#include <thread>
#include <variant>

template <class T>
class Async {
private:
    struct except_wrapper {
        std::exception_ptr ep;
        except_wrapper(std::exception_ptr p): ep{p} {}
    };

    bool delivered;
    Box<std::variant<except_wrapper, T>> result;
public:
    template <class Func, class... Args,
              class = std::enable_if_t<std::is_invocable_v<Func&&, Args&&...>>>
    explicit Async(Func&& func, Args&&... args): delivered{false} {
        std::thread th{
            [this](Func&& func, Args&&... args) {
                try {
                    result.emplace(
                        std::in_place_index<1>,
                        std::invoke(std::forward<Func>(func), std::forward<Args>(args)...)
                    );
                }
                catch (...) {
                    result.emplace(std::in_place_index<0>, except_wrapper{std::current_exception()});
                }
            },
            std::forward<Func>(func), std::forward<Args>(args)...
        };
        th.detach();
    }
    ~Async() {
        if (!delivered) result.discard();
    }

    template <class = std::enable_if_t<std::is_copy_constructible_v<T>>>
    T wait() {
        return std::visit([this](auto&& arg) -> T {
                delivered = true;
                using U = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<U, except_wrapper>) {
                    std::rethrow_exception(arg.ep);
                }
                else {
                    return arg;
                }
            },
            result.take()
        );
    }

    std::optional<std::exception_ptr> exec() {
        return std::visit([this](auto&& arg) -> std::optional<std::exception_ptr> {
                delivered = true;
                using U = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<U, except_wrapper>) {
                    return arg.ep;
                }
                else {
                    return std::nullopt;
                }
            },
            result.take()
        );
    }

    template <class = std::enable_if_t<std::is_copy_constructible_v<T>
                                    || std::is_move_constructible_v<T>>>
    std::optional<T> eval() {
        return std::visit([this](auto&& arg) -> std::optional<T> {
                delivered = true;
                using U = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<U, except_wrapper>) {
                    return std::nullopt;
                }
                else {
                    return arg;
                }
            },
            result.take()
        );
    }
};
