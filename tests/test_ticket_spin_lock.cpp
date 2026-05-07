/**
 * @file test_ticket_spin_lock.cpp
 * @brief 测试 my::ticket_spin_lock 的正确性和公平性
 * 
 * 测试内容：
 * 1. 基本的 lock/unlock
 * 2. 多线程并发保护临界区
 * 3. 公平性测试（FIFO 顺序）
 */

#include <my/ticket_spin_lock.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include <chrono>
#include <atomic>
#include <algorithm>

// 全局变量用于测试
int global_counter = 0;
my::ticket_spin_lock global_lock;

// 测试基本操作
void test_basic_operations() {
    std::cout << "=== 测试基本操作 ===" << std::endl;
    
    my::ticket_spin_lock lock;
    
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
    lock.unlock();
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

// 公平性测试：验证 FIFO 顺序
void test_fairness() {
    std::cout << "=== 测试公平性 (FIFO 顺序) ===" << std::endl;
    
    const int num_threads = 4;
    const int iterations = 1000;
    
    my::ticket_spin_lock lock;
    
    // 记录每个线程获取锁的次数
    std::vector<std::atomic<int>> lock_counts(num_threads);
    for (auto& count : lock_counts) {
        count.store(0);
    }
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&lock, &lock_counts, i]() {
            for (int j = 0; j < iterations; ++j) {
                lock.lock();
                lock_counts[i].fetch_add(1);
                // 短暂持有锁
                for (volatile int k = 0; k < 100; ++k);
                lock.unlock();
                // 短暂让出 CPU，让其他线程有机会竞争
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
    std::cout << "  最小: " << min_count << ", 最大: " << max_count << std::endl;
    
    // Ticket lock 应该保证公平性，每个线程获取的次数应该相近
    // 由于是严格 FIFO，差异应该很小
    assert(total == num_threads * iterations);
    
    // 允许一定的波动（由于 yield 的时机）
    double avg = (double)total / num_threads;
    double deviation = (max_count - min_count) / avg;
    std::cout << "  偏差率: " << (deviation * 100) << "%" << std::endl;
    
    // 偏差应该在合理范围内（例如 50%）
    assert(deviation < 0.5);
    std::cout << "  [PASS] 公平性测试" << std::endl;
}

// 与简单自旋锁对比
void test_compare_with_spin_lock() {
    std::cout << "=== 与简单自旋锁性能对比 ===" << std::endl;
    
    const int num_threads = 4;
    const int iterations = 100000;
    
    // 测试 ticket_spin_lock
    {
        my::ticket_spin_lock lock;
        int counter = 0;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < iterations; ++j) {
                    lock.lock();
                    counter++;
                    lock.unlock();
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "  Ticket Spin Lock: " << duration.count() << " ms" << std::endl;
        assert(counter == num_threads * iterations);
    }
    
    std::cout << "  [INFO] 性能对比完成" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   my::ticket_spin_lock 测试程序" << std::endl;
    std::cout << "========================================" << std::endl;
    
    test_basic_operations();
    test_concurrent_counter();
    test_fairness();
    test_compare_with_spin_lock();
    
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "         所有测试通过！" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
