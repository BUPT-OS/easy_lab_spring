/**
 * @file producer_consumer.cpp
 * @brief 生产者-消费者模型实现
 *
 * 使用我们实现的同步原语来解决经典的生产者-消费者问题。
 *
 * 问题描述：
 * - 有一个有限容量的缓冲区
 * - 生产者向缓冲区中放入数据
 * - 消费者从缓冲区中取出数据
 * - 缓冲区满时，生产者需要等待
 * - 缓冲区空时，消费者需要等待
 *
 * 解决方案：使用三个信号量
 * - empty: 表示空闲槽位数量（初始 = 缓冲区大小）
 * - full:  表示已填充槽位数量（初始 = 0）
 * - mutex: 保护缓冲区的互斥访问
 */

#include <my/semaphore.hpp>
#include <my/spin_lock.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <chrono>
#include <random>
#include <atomic>

// 缓冲区大小
constexpr int BUFFER_SIZE = 10;

// 生产/消费的总数量
constexpr int TOTAL_ITEMS = 100;

/**
 * @brief 有界缓冲区类
 *
 * 使用信号量实现线程安全的有界队列
 */
class BoundedBuffer {
private:
    std::queue<int> buffer_;     // 缓冲区
    my::semaphore empty_;        // 空闲槽位
    my::semaphore full_;         // 已填充槽位
    my::semaphore mutex_;        // 互斥锁

public:
    BoundedBuffer(int capacity)
        : empty_(capacity),      // 初始有 capacity 个空槽位
          full_(0),              // 初始没有数据
          mutex_(1)              // 二元信号量作为互斥锁
    {}

    /**
     * @brief 生产者放入数据
     * @param item 要放入的数据
     */
    void produce(int item) {
        // ==================== 学生填写区域 开始 ====================
        // 提示：
        // 1. P(empty): 等待空槽位   -> empty_.wait()
        // 2. P(mutex): 获取互斥锁   -> mutex_.wait()
        // 3. 将数据放入缓冲区       -> buffer_.push(item)
        // 4. V(mutex): 释放互斥锁   -> mutex_.signal()
        // 5. V(full):  增加已填充槽位 -> full_.signal()
        //
        // 注意：P 操作的顺序很重要！如果先 P(mutex) 再 P(empty)，
        // 可能导致死锁（持有互斥锁时阻塞在 empty 上）

        // TODO: 在此处实现生产者逻辑

        // ==================== 学生填写区域 结束 ====================
    }

    /**
     * @brief 消费者取出数据
     * @return 取出的数据
     */
    int consume() {
        int item = 0;
        // ==================== 学生填写区域 开始 ====================
        // 提示：
        // 1. P(full):  等待有数据    -> full_.wait()
        // 2. P(mutex): 获取互斥锁    -> mutex_.wait()
        // 3. 从缓冲区取出数据        -> item = buffer_.front(); buffer_.pop()
        // 4. V(mutex): 释放互斥锁    -> mutex_.signal()
        // 5. V(empty): 增加空槽位    -> empty_.signal()

        // TODO: 在此处实现消费者逻辑

        // ==================== 学生填写区域 结束 ====================
        return item;
    }

    /**
     * @brief 获取当前缓冲区大小（仅用于调试）
     */
    size_t size() {
        mutex_.wait();
        size_t s = buffer_.size();
        mutex_.signal();
        return s;
    }
};

// 全局统计
std::atomic<int> total_produced(0);
std::atomic<int> total_consumed(0);
std::atomic<long long> sum_produced(0);
std::atomic<long long> sum_consumed(0);

/**
 * @brief 生产者线程函数
 */
void producer(int id, BoundedBuffer& buffer, int items_to_produce) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100);
    std::uniform_int_distribution<> delay(1, 10);

    for (int i = 0; i < items_to_produce; ++i) {
        int item = dis(gen);

        buffer.produce(item);

        total_produced.fetch_add(1);
        sum_produced.fetch_add(item);

        std::cout << "[生产者 " << id << "] 生产: " << item
                  << " (缓冲区大小约: " << buffer.size() << ")" << std::endl;

        // 随机延迟，模拟生产时间
        std::this_thread::sleep_for(std::chrono::milliseconds(delay(gen)));
    }

    std::cout << "[生产者 " << id << "] 完成，共生产 " << items_to_produce << " 个物品" << std::endl;
}

/**
 * @brief 消费者线程函数
 */
void consumer(int id, BoundedBuffer& buffer, int items_to_consume) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> delay(1, 15);

    for (int i = 0; i < items_to_consume; ++i) {
        int item = buffer.consume();

        total_consumed.fetch_add(1);
        sum_consumed.fetch_add(item);

        std::cout << "[消费者 " << id << "] 消费: " << item
                  << " (缓冲区大小约: " << buffer.size() << ")" << std::endl;

        // 随机延迟，模拟消费时间
        std::this_thread::sleep_for(std::chrono::milliseconds(delay(gen)));
    }

    std::cout << "[消费者 " << id << "] 完成，共消费 " << items_to_consume << " 个物品" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "        生产者-消费者模型演示" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "缓冲区大小: " << BUFFER_SIZE << std::endl;
    std::cout << "总生产/消费数量: " << TOTAL_ITEMS << std::endl;
    std::cout << "========================================" << std::endl << std::endl;

    BoundedBuffer buffer(BUFFER_SIZE);

    const int num_producers = 3;
    const int num_consumers = 2;

    // 计算每个生产者/消费者需要处理的数量
    int items_per_producer = TOTAL_ITEMS / num_producers;
    int items_per_consumer = TOTAL_ITEMS / num_consumers;

    // 处理除不尽的情况
    int extra_producer_items = TOTAL_ITEMS % num_producers;
    int extra_consumer_items = TOTAL_ITEMS % num_consumers;

    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    // 启动生产者线程
    for (int i = 0; i < num_producers; ++i) {
        int items = items_per_producer + (i < extra_producer_items ? 1 : 0);
        threads.emplace_back(producer, i, std::ref(buffer), items);
    }

    // 启动消费者线程
    for (int i = 0; i < num_consumers; ++i) {
        int items = items_per_consumer + (i < extra_consumer_items ? 1 : 0);
        threads.emplace_back(consumer, i, std::ref(buffer), items);
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "              运行结果" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "总生产数量: " << total_produced.load() << std::endl;
    std::cout << "总消费数量: " << total_consumed.load() << std::endl;
    std::cout << "生产总和: " << sum_produced.load() << std::endl;
    std::cout << "消费总和: " << sum_consumed.load() << std::endl;
    std::cout << "总耗时: " << duration.count() << " ms" << std::endl;

    // 验证正确性
    if (total_produced.load() == total_consumed.load() &&
        sum_produced.load() == sum_consumed.load()) {
        std::cout << std::endl;
        std::cout << "[SUCCESS] 生产者-消费者模型运行正确！" << std::endl;
    } else {
        std::cout << std::endl;
        std::cout << "[ERROR] 数据不一致！" << std::endl;
        return 1;
    }

    return 0;
}
