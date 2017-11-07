#include "Box.hpp"
#include "Async.hpp"
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

int main() {
    using namespace std::chrono_literals;

    {
        Box<int> box;
        std::thread th1{[&] {
            std::this_thread::sleep_for(1s);
            box.put(1);
        }};
        std::thread th2{[&] {
            std::this_thread::sleep_for(2s);
            box.put(2);
        }};
        std::cout << box.take() << std::endl;
        std::cout << box.take() << std::endl;
        th1.join();
        th2.join();
    }

    {
        Box<std::shared_ptr<int>> box;
        std::thread th{[&] {
            std::this_thread::sleep_for(1s);
            box.put(std::make_shared<int>(1));
            box.emplace(new int{2});
        }};
        std::cout << *box.peek() << std::endl;
        std::cout << *box.peek() << std::endl;
        box.discard();
        box.peek(
            [](const std::shared_ptr<int>* pp) {
                std::cout << **pp << std::endl;
            }
        );
        box.discard();
        th.join();
    }

    {
        Async<int> action{[] {
            std::this_thread::sleep_for(1s);
            return 5;
        }};
        std::cout << action.wait() << std::endl;
    }

    {
        Async<int> action{[] {
            throw "Error!";
            return 0;
        }};
        try {
            std::cout << action.wait() << std::endl;
        }
        catch (const char* msg) {
            std::cout << msg << std::endl;
        }
    }

    {
        Async<int> action{[] {
            throw "Error again!";
            return 0;
        }};
        auto err = action.exec();
        try {
            if (err) std::rethrow_exception(err.value());
        }
        catch (const char* msg) {
            std::cout << msg << std::endl;
        }
    }

    {
        Async<int> action{[] {
            return 10;
        }};
        auto i = action.eval();
        if (i) std::cout << i.value() << std::endl;
    }
}
