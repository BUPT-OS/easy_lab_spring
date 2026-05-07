/**
 * @file test_spin_lock.cpp
 * @brief 测试 my::spin_lock 的正确性
 * 
 * 测试内容：
 * 1. 基本的 lock/unlock
 * 2. try_lock
 * 3. 多线程并发保护临界区
 */

#include <my/spin_lock.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include <chrono>

// 全局变量用于测试
int global_counter = 0;
my::spin_lock global_lock;

// 测试基本操作
void test_basic_operations() {
    std::cout << "=== 测试基本操作 ===" << std::endl;
    
    my::spin_lock lock;
    
    // 测试 lock/unlock
    lock.lock();
    std::cout << "  成功获取锁" << std::endl;
    lock.unlock();
    std::cout << "  成功释放锁" << std::endl;
    std::cout << "  [PASS] lock/unlock" << std::endl;
    
    // 测试 try_lock
    bool got_lock = lock.try_lock();
    assert(got_lock);
    std::cout << "  [PASS] try_lock 成功" << std::endl;
    
    // 锁已被持有，try_lock 应该失败
    // 注意：在单线程中无法测试这个，因为我们已经持有锁
    lock.unlock();
}

// 测试 lock_guard
void test_lock_guard() {
    std::cout << "=== 测试 lock_guard ===" << std::endl;
    
    my::spin_lock lock;
    
    {
        my::lock_guard<my::spin_lock> guard(lock);
        std::cout << "  lock_guard 构造完成，锁已获取" << std::endl;
        // 在 guard 作用域内，锁被持有
    }
    // guard 析构，锁已释放
    std::cout << "  lock_guard 析构完成，锁已释放" << std::endl;
    
    // 验证锁已释放
    bool got_lock = lock.try_lock();
    assert(got_lock);
    lock.unlock();
    std::cout << "  [PASS] lock_guard" << std::endl;
}

// 多线程并发测试：保护临界区
void test_concurrent_counter() {
    std::cout << "=== 测试多线程并发（保护临界区）===" << std::endl;
    
    const int num_threads = 8;
    const int iterations = 100000;
    
    global_counter = 0;
    std::vector<std::thread> threads;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([]() {
            for (int j = 0; j < iterations; ++j) {
                global_lock.lock();
                global_counter++;
                global_lock.unlock();
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    int expected = num_threads * iterations;
    
    std::cout << "  期望值: " << expected << std::endl;
    std::cout << "  实际值: " << global_counter << std::endl;
    std::cout << "  耗时: " << duration.count() << " ms" << std::endl;
    
    assert(global_counter == expected);
    std::cout << "  [PASS] 多线程并发保护临界区" << std::endl;
}

// 测试 try_lock 在竞争情况下的行为
void test_concurrent_try_lock() {
    std::cout << "=== 测试多线程 try_lock ===" << std::endl;
    
    const int num_threads = 4;
    const int iterations = 10000;
    
    my::spin_lock lock;
    int counter = 0;
    int try_lock_failures = 0;
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < iterations; ++j) {
                // 尝试获取锁
                while (!lock.try_lock()) {
                    __sync_fetch_and_add(&try_lock_failures, 1);
                    // 短暂让出 CPU
                    std::this_thread::yield();
                }
                counter++;
                lock.unlock();
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    int expected = num_threads * iterations;
    
    std::cout << "  期望值: " << expected << std::endl;
    std::cout << "  实际值: " << counter << std::endl;
    std::cout << "  try_lock 失败次数: " << try_lock_failures << std::endl;
    
    assert(counter == expected);
    std::cout << "  [PASS] 多线程 try_lock" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "      my::spin_lock 测试程序" << std::endl;
    std::cout << "========================================" << std::endl;
    
    test_basic_operations();
    test_lock_guard();
    test_concurrent_counter();
    test_concurrent_try_lock();
    
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "         所有测试通过！" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
