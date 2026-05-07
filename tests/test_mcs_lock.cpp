/**
 * @file test_mcs_lock.cpp
 * @brief 测试 my::mcs_lock 的正确性
 * 
 * 测试内容：
 * 1. 基本的 lock/unlock
 * 2. try_lock
 * 3. 多线程并发保护临界区
 * 4. RAII 守卫
 */

#include <my/mcs_lock.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include <chrono>
#include <atomic>

// 全局变量用于测试
int global_counter = 0;
my::mcs_lock global_lock;

// 测试基本操作
void test_basic_operations() {
    std::cout << "=== 测试基本操作 ===" << std::endl;
    
    my::mcs_lock lock;
    my::mcs_node node;
    
    // 测试 lock/unlock
    lock.lock(&node);
    std::cout << "  成功获取锁" << std::endl;
    lock.unlock(&node);
    std::cout << "  成功释放锁" << std::endl;
    std::cout << "  [PASS] lock/unlock" << std::endl;
    
    // 测试 try_lock
    node.reset();
    bool got_lock = lock.try_lock(&node);
    assert(got_lock);
    std::cout << "  [PASS] try_lock 成功" << std::endl;
    lock.unlock(&node);
}

// 测试 lock_guard
void test_lock_guard() {
    std::cout << "=== 测试 mcs_lock_guard ===" << std::endl;
    
    my::mcs_lock lock;
    
    {
        my::mcs_lock_guard guard(lock);
        std::cout << "  mcs_lock_guard 构造完成，锁已获取" << std::endl;
    }
    std::cout << "  mcs_lock_guard 析构完成，锁已释放" << std::endl;
    
    // 验证锁已释放
    my::mcs_node node;
    bool got_lock = lock.try_lock(&node);
    assert(got_lock);
    lock.unlock(&node);
    std::cout << "  [PASS] mcs_lock_guard" << std::endl;
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
            my::mcs_node node;  // 每个线程有自己的节点
            for (int j = 0; j < iterations; ++j) {
                global_lock.lock(&node);
                global_counter++;
                global_lock.unlock(&node);
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

// 使用 RAII 守卫的多线程测试
void test_concurrent_with_guard() {
    std::cout << "=== 测试多线程（使用 mcs_lock_guard）===" << std::endl;
    
    const int num_threads = 8;
    const int iterations = 50000;
    
    my::mcs_lock lock;
    int counter = 0;
    
    std::vector<std::thread> threads;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < iterations; ++j) {
                my::mcs_lock_guard guard(lock);
                counter++;
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
    std::cout << "  实际值: " << counter << std::endl;
    std::cout << "  耗时: " << duration.count() << " ms" << std::endl;
    
    assert(counter == expected);
    std::cout << "  [PASS] 多线程使用 mcs_lock_guard" << std::endl;
}

// 公平性测试
void test_fairness() {
    std::cout << "=== 测试公平性 ===" << std::endl;
    
    const int num_threads = 4;
    const int iterations = 1000;
    
    my::mcs_lock lock;
    
    // 记录每个线程获取锁的次数
    std::vector<std::atomic<int>> lock_counts(num_threads);
    for (auto& count : lock_counts) {
        count.store(0);
    }
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&lock, &lock_counts, i]() {
            my::mcs_node node;
            for (int j = 0; j < iterations; ++j) {
                lock.lock(&node);
                lock_counts[i].fetch_add(1);
                // 短暂持有锁
                for (volatile int k = 0; k < 100; ++k);
                lock.unlock(&node);
                // 让出 CPU
                std::this_thread::yield();
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // 检查每个线程获取锁的次数
    std::cout << "  各线程获取锁的次数:" << std::endl;
    int total = 0;
    int min_count = iterations;
    int max_count = 0;
    
    for (int i = 0; i < num_threads; ++i) {
        int count = lock_counts[i].load();
        std::cout << "    线程 " << i << ": " << count << std::endl;
        total += count;
        min_count = std::min(min_count, count);
        max_count = std::max(max_count, count);
    }
    
    std::cout << "  总计: " << total << std::endl;
    
    assert(total == num_threads * iterations);
    std::cout << "  [PASS] 公平性测试" << std::endl;
}

// 节点重用测试
void test_node_reuse() {
    std::cout << "=== 测试节点重用 ===" << std::endl;
    
    my::mcs_lock lock;
    my::mcs_node node;
    
    const int iterations = 1000;
    int counter = 0;
    
    for (int i = 0; i < iterations; ++i) {
        lock.lock(&node);
        counter++;
        lock.unlock(&node);
    }
    
    assert(counter == iterations);
    std::cout << "  [PASS] 单线程节点重用" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "       my::mcs_lock 测试程序" << std::endl;
    std::cout << "========================================" << std::endl;
    
    test_basic_operations();
    test_lock_guard();
    test_concurrent_counter();
    test_concurrent_with_guard();
    test_fairness();
    test_node_reuse();
    
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "         所有测试通过！" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
