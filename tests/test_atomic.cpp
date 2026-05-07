/**
 * @file test_atomic.cpp
 * @brief 测试 my::atomic 的正确性
 * 
 * 测试内容：
 * 1. 基本操作：load, store, exchange
 * 2. CAS 操作
 * 3. fetch_add/fetch_sub 操作
 * 4. 多线程并发测试
 */

#include <my/atomic.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>

// 测试基本操作
void test_basic_operations() {
    std::cout << "=== 测试基本操作 ===" << std::endl;
    
    // 测试构造和 load
    my::atomic a(42);
    assert(a.load() == 42);
    std::cout << "  [PASS] 构造和 load" << std::endl;
    
    // 测试 store
    a.store(100);
    assert(a.load() == 100);
    std::cout << "  [PASS] store" << std::endl;
    
    // 测试 exchange
    int32_t old = a.exchange(200);
    assert(old == 100);
    assert(a.load() == 200);
    std::cout << "  [PASS] exchange" << std::endl;
    
    // 测试隐式转换
    int32_t val = a;
    assert(val == 200);
    std::cout << "  [PASS] 隐式转换" << std::endl;
}

// 测试 CAS 操作
void test_cas_operations() {
    std::cout << "=== 测试 CAS 操作 ===" << std::endl;
    
    my::atomic a(100);
    
    // 成功的 CAS
    int32_t expected = 100;
    bool success = a.compare_exchange_strong(expected, 200);
    assert(success);
    assert(a.load() == 200);
    assert(expected == 100);  // expected 不变
    std::cout << "  [PASS] CAS 成功" << std::endl;
    
    // 失败的 CAS
    expected = 100;  // 错误的期望值
    success = a.compare_exchange_strong(expected, 300);
    assert(!success);
    assert(a.load() == 200);  // 值没有改变
    assert(expected == 200);  // expected 被更新为实际值
    std::cout << "  [PASS] CAS 失败并更新 expected" << std::endl;
}

// 测试 fetch_add 和 fetch_sub
void test_fetch_operations() {
    std::cout << "=== 测试 fetch_add/fetch_sub ===" << std::endl;
    
    my::atomic a(100);
    
    // fetch_add
    int32_t old = a.fetch_add(50);
    assert(old == 100);
    assert(a.load() == 150);
    std::cout << "  [PASS] fetch_add" << std::endl;
    
    // fetch_sub
    old = a.fetch_sub(30);
    assert(old == 150);
    assert(a.load() == 120);
    std::cout << "  [PASS] fetch_sub" << std::endl;
    
    // 运算符
    a.store(0);
    assert(++a == 1);      // 前置++
    assert(a++ == 1);      // 后置++，返回旧值
    assert(a.load() == 2);
    assert(--a == 1);      // 前置--
    assert(a-- == 1);      // 后置--，返回旧值
    assert(a.load() == 0);
    std::cout << "  [PASS] ++/-- 运算符" << std::endl;
}

// 多线程并发测试：多个线程同时对同一个 atomic 变量进行 fetch_add
void test_concurrent_increment() {
    std::cout << "=== 测试多线程并发 fetch_add ===" << std::endl;
    
    const int num_threads = 8;
    const int iterations = 100000;
    
    my::atomic counter(0);
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&counter]() {
            for (int j = 0; j < iterations; ++j) {
                counter.fetch_add(1);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    int32_t expected = num_threads * iterations;
    int32_t actual = counter.load();
    
    std::cout << "  期望值: " << expected << std::endl;
    std::cout << "  实际值: " << actual << std::endl;
    
    assert(actual == expected);
    std::cout << "  [PASS] 多线程并发 fetch_add" << std::endl;
}

// 多线程并发测试：CAS 竞争
void test_concurrent_cas() {
    std::cout << "=== 测试多线程并发 CAS ===" << std::endl;
    
    const int num_threads = 8;
    const int iterations = 10000;
    
    my::atomic counter(0);
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&counter]() {
            for (int j = 0; j < iterations; ++j) {
                // 使用 CAS 实现安全的自增
                int32_t current = counter.load();
                while (!counter.compare_exchange_strong(current, current + 1)) {
                    // CAS 失败，current 已被更新为实际值，重试
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    int32_t expected = num_threads * iterations;
    int32_t actual = counter.load();
    
    std::cout << "  期望值: " << expected << std::endl;
    std::cout << "  实际值: " << actual << std::endl;
    
    assert(actual == expected);
    std::cout << "  [PASS] 多线程并发 CAS" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "       my::atomic 测试程序" << std::endl;
    std::cout << "========================================" << std::endl;
    
    test_basic_operations();
    test_cas_operations();
    test_fetch_operations();
    test_concurrent_increment();
    test_concurrent_cas();
    
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "         所有测试通过！" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
