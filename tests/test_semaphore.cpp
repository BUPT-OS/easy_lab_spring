/**
 * @file test_semaphore.cpp
 * @brief 测试 my::semaphore 的正确性
 * 
 * 测试内容：
 * 1. 基本的 wait/signal 操作
 * 2. 信号量作为计数器
 * 3. 信号量作为互斥锁
 * 4. 多线程生产者-消费者简单测试
 */

#include <my/semaphore.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include <chrono>
#include <atomic>

// 测试基本操作
void test_basic_operations() {
    std::cout << "=== 测试基本操作 ===" << std::endl;
    
    // 测试初始值
    my::semaphore sem(3);
    assert(sem.get_value() == 3);
    std::cout << "  [PASS] 初始值" << std::endl;
    
    // 测试 wait
    sem.wait();
    assert(sem.get_value() == 2);
    sem.wait();
    assert(sem.get_value() == 1);
    sem.wait();
    assert(sem.get_value() == 0);
    std::cout << "  [PASS] wait 递减" << std::endl;
    
    // 测试 signal
    sem.signal();
    assert(sem.get_value() == 1);
    std::cout << "  [PASS] signal 递增" << std::endl;
}

// 测试 try_wait
void test_try_wait() {
    std::cout << "=== 测试 try_wait ===" << std::endl;
    
    my::semaphore sem(1);
    
    // 有资源时应成功
    assert(sem.try_wait() == true);
    std::cout << "  [PASS] try_wait 有资源时成功" << std::endl;
    
    // 无资源时应失败
    assert(sem.try_wait() == false);
    std::cout << "  [PASS] try_wait 无资源时失败" << std::endl;
    
    // 释放资源后应该再次成功
    sem.signal();
    assert(sem.try_wait() == true);
    std::cout << "  [PASS] try_wait signal 后成功" << std::endl;
}

// 测试信号量作为互斥锁
void test_as_mutex() {
    std::cout << "=== 测试信号量作为互斥锁 ===" << std::endl;
    
    const int num_threads = 8;
    const int iterations = 50000;
    
    my::semaphore mutex(1);  // 二元信号量
    int counter = 0;
    
    std::vector<std::thread> threads;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < iterations; ++j) {
                mutex.wait();    // P 操作，获取锁
                counter++;
                mutex.signal();  // V 操作，释放锁
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
    std::cout << "  [PASS] 信号量作为互斥锁" << std::endl;
}

// 测试有限资源访问
void test_limited_resources() {
    std::cout << "=== 测试有限资源访问 ===" << std::endl;
    
    const int max_concurrent = 3;  // 最多 3 个线程同时访问
    const int num_threads = 10;
    
    my::semaphore sem(max_concurrent);
    std::atomic<int> current_users(0);
    std::atomic<int> max_observed(0);
    std::atomic<bool> violation(false);
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100; ++j) {
                sem.wait();
                
                // 进入临界区
                int users = current_users.fetch_add(1) + 1;
                
                // 检查是否超过限制
                if (users > max_concurrent) {
                    violation.store(true);
                }
                
                // 更新观察到的最大并发数
                int old_max = max_observed.load();
                while (users > old_max && !max_observed.compare_exchange_weak(old_max, users));
                
                // 模拟工作
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                
                // 离开临界区
                current_users.fetch_sub(1);
                
                sem.signal();
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "  最大允许并发: " << max_concurrent << std::endl;
    std::cout << "  观察到的最大并发: " << max_observed.load() << std::endl;
    
    assert(!violation.load());
    assert(max_observed.load() <= max_concurrent);
    std::cout << "  [PASS] 有限资源访问" << std::endl;
}

// 简单的生产者-消费者测试
void test_producer_consumer_simple() {
    std::cout << "=== 测试简单生产者-消费者 ===" << std::endl;
    
    const int num_items = 1000;
    
    my::semaphore items(0);    // 可消费的物品数量
    std::atomic<int> produced(0);
    std::atomic<int> consumed(0);
    
    // 生产者线程
    std::thread producer([&]() {
        for (int i = 0; i < num_items; ++i) {
            produced.fetch_add(1);
            items.signal();  // 生产一个物品
        }
    });
    
    // 消费者线程
    std::thread consumer([&]() {
        for (int i = 0; i < num_items; ++i) {
            items.wait();  // 等待物品
            consumed.fetch_add(1);
        }
    });
    
    producer.join();
    consumer.join();
    
    std::cout << "  生产数量: " << produced.load() << std::endl;
    std::cout << "  消费数量: " << consumed.load() << std::endl;
    
    assert(produced.load() == num_items);
    assert(consumed.load() == num_items);
    std::cout << "  [PASS] 简单生产者-消费者" << std::endl;
}

// 测试阻塞等待
void test_blocking_wait() {
    std::cout << "=== 测试阻塞等待 ===" << std::endl;
    
    my::semaphore sem(0);  // 初始为 0，wait 会阻塞
    std::atomic<bool> thread_waiting(false);
    std::atomic<bool> thread_done(false);
    
    // 等待者线程
    std::thread waiter([&]() {
        thread_waiting.store(true);
        sem.wait();  // 应该阻塞
        thread_done.store(true);
    });
    
    // 等待一小段时间，确保线程开始等待
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    assert(thread_waiting.load());
    assert(!thread_done.load());  // 线程应该还在阻塞
    std::cout << "  线程正在阻塞等待..." << std::endl;
    
    // 释放信号量
    sem.signal();
    
    // 等待线程结束
    waiter.join();
    
    assert(thread_done.load());
    std::cout << "  [PASS] 阻塞等待和唤醒" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "     my::semaphore 测试程序" << std::endl;
    std::cout << "========================================" << std::endl;
    
    test_basic_operations();
    test_try_wait();
    test_as_mutex();
    test_limited_resources();
    test_producer_consumer_simple();
    test_blocking_wait();
    
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "         所有测试通过！" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
