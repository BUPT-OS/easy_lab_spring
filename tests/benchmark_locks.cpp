/**
 * @file benchmark_locks.cpp
 * @brief 性能对比测试：spin_lock vs ticket_spin_lock vs mcs_lock
 * 
 * 测试内容：
 * 1. 不同线程数下的吞吐量对比
 * 2. 锁的公平性对比
 * 3. 高竞争 vs 低竞争场景
 */

#include <my/spin_lock.hpp>
#include <my/ticket_spin_lock.hpp>
#include <my/mcs_lock.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <iomanip>
#include <atomic>
#include <algorithm>
#include <numeric>
#include <cmath>

// 测试参数
constexpr int WARMUP_ITERATIONS = 10000;      // 预热迭代次数
constexpr int BENCHMARK_ITERATIONS = 100000;  // 基准测试迭代次数
constexpr int FAIRNESS_ITERATIONS = 10000;    // 公平性测试迭代次数

// 计时器
class Timer {
public:
    void start() {
        start_ = std::chrono::high_resolution_clock::now();
    }
    
    double elapsed_ms() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }
    
private:
    std::chrono::high_resolution_clock::time_point start_;
};

// 打印分隔线
void print_separator(char c = '=', int width = 80) {
    std::cout << std::string(width, c) << std::endl;
}

// 打印标题
void print_title(const std::string& title) {
    print_separator();
    std::cout << "  " << title << std::endl;
    print_separator();
}

// ============================================================================
// 吞吐量测试
// ============================================================================

// 测试 spin_lock 吞吐量
double benchmark_spin_lock(int num_threads, int iterations_per_thread) {
    my::spin_lock lock;
    int counter = 0;
    std::vector<std::thread> threads;
    
    Timer timer;
    timer.start();
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < iterations_per_thread; ++j) {
                lock.lock();
                counter++;
                lock.unlock();
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    double elapsed = timer.elapsed_ms();
    
    // 验证正确性
    if (counter != num_threads * iterations_per_thread) {
        std::cerr << "[ERROR] spin_lock: 计数器不正确!" << std::endl;
    }
    
    return elapsed;
}

// 测试 ticket_spin_lock 吞吐量
double benchmark_ticket_spin_lock(int num_threads, int iterations_per_thread) {
    my::ticket_spin_lock lock;
    int counter = 0;
    std::vector<std::thread> threads;
    
    Timer timer;
    timer.start();
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < iterations_per_thread; ++j) {
                lock.lock();
                counter++;
                lock.unlock();
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    double elapsed = timer.elapsed_ms();
    
    if (counter != num_threads * iterations_per_thread) {
        std::cerr << "[ERROR] ticket_spin_lock: 计数器不正确!" << std::endl;
    }
    
    return elapsed;
}

// 测试 mcs_lock 吞吐量
double benchmark_mcs_lock(int num_threads, int iterations_per_thread) {
    my::mcs_lock lock;
    int counter = 0;
    std::vector<std::thread> threads;
    
    Timer timer;
    timer.start();
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            my::mcs_node node;
            for (int j = 0; j < iterations_per_thread; ++j) {
                lock.lock(&node);
                counter++;
                lock.unlock(&node);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    double elapsed = timer.elapsed_ms();
    
    if (counter != num_threads * iterations_per_thread) {
        std::cerr << "[ERROR] mcs_lock: 计数器不正确!" << std::endl;
    }
    
    return elapsed;
}

// 运行吞吐量测试
void run_throughput_benchmark() {
    print_title("吞吐量测试 (Throughput Benchmark)");
    
    std::vector<int> thread_counts = {1, 2, 4, 8, 16, 32, 64};
    int iterations_per_thread = BENCHMARK_ITERATIONS;
    
    std::cout << std::endl;
    std::cout << std::setw(10) << "线程数"
              << std::setw(18) << "spin_lock(ms)"
              << std::setw(18) << "ticket_lock(ms)"
              << std::setw(18) << "mcs_lock(ms)"
              << std::setw(16) << "最快" << std::endl;
    print_separator('-');
    
    for (int num_threads : thread_counts) {
        // 预热
        benchmark_spin_lock(num_threads, WARMUP_ITERATIONS / num_threads);
        benchmark_ticket_spin_lock(num_threads, WARMUP_ITERATIONS / num_threads);
        benchmark_mcs_lock(num_threads, WARMUP_ITERATIONS / num_threads);
        
        // 正式测试
        double spin_time = benchmark_spin_lock(num_threads, iterations_per_thread);
        double ticket_time = benchmark_ticket_spin_lock(num_threads, iterations_per_thread);
        double mcs_time = benchmark_mcs_lock(num_threads, iterations_per_thread);
        
        // 找出最快的
        std::string fastest;
        double min_time = std::min({spin_time, ticket_time, mcs_time});
        if (min_time == spin_time) fastest = "spin_lock";
        else if (min_time == ticket_time) fastest = "ticket_lock";
        else fastest = "mcs_lock";
        
        std::cout << std::setw(10) << num_threads
                  << std::setw(18) << std::fixed << std::setprecision(2) << spin_time
                  << std::setw(18) << ticket_time
                  << std::setw(18) << mcs_time
                  << std::setw(16) << fastest << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "注: 每个线程执行 " << iterations_per_thread << " 次 lock/unlock 操作" << std::endl;
}

// ============================================================================
// 公平性测试
// ============================================================================

// 测试公平性（返回标准差/平均值，越小越公平）
template<typename LockType>
double test_fairness_impl(LockType& lock, int num_threads, int total_iterations,
                          std::vector<int>& counts) {
    counts.assign(num_threads, 0);
    std::atomic<int> remaining(total_iterations);
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            while (remaining.load() > 0) {
                lock.lock();
                if (remaining.load() > 0) {
                    counts[i]++;
                    remaining.fetch_sub(1);
                }
                lock.unlock();
                std::this_thread::yield();  // 给其他线程机会
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // 计算变异系数 (CV = 标准差/平均值)
    double sum = std::accumulate(counts.begin(), counts.end(), 0.0);
    double mean = sum / num_threads;
    
    double sq_sum = 0;
    for (int c : counts) {
        sq_sum += (c - mean) * (c - mean);
    }
    double stddev = std::sqrt(sq_sum / num_threads);
    
    return stddev / mean;  // 变异系数，越小越公平
}

// MCS lock 版本的公平性测试
double test_fairness_mcs(my::mcs_lock& lock, int num_threads, int total_iterations,
                         std::vector<int>& counts) {
    counts.assign(num_threads, 0);
    std::atomic<int> remaining(total_iterations);
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            my::mcs_node node;
            while (remaining.load() > 0) {
                lock.lock(&node);
                if (remaining.load() > 0) {
                    counts[i]++;
                    remaining.fetch_sub(1);
                }
                lock.unlock(&node);
                std::this_thread::yield();
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    double sum = std::accumulate(counts.begin(), counts.end(), 0.0);
    double mean = sum / num_threads;
    
    double sq_sum = 0;
    for (int c : counts) {
        sq_sum += (c - mean) * (c - mean);
    }
    double stddev = std::sqrt(sq_sum / num_threads);
    
    return stddev / mean;
}

void run_fairness_benchmark() {
    print_title("公平性测试 (Fairness Benchmark)");
    
    const int num_threads = 4;
    const int total_iterations = FAIRNESS_ITERATIONS;
    
    std::cout << std::endl;
    std::cout << "线程数: " << num_threads << ", 总操作数: " << total_iterations << std::endl;
    std::cout << "变异系数 (CV) = 标准差/平均值，越小越公平，0 表示完全公平" << std::endl;
    std::cout << std::endl;
    
    std::vector<int> counts;
    
    // spin_lock
    {
        my::spin_lock lock;
        double cv = test_fairness_impl(lock, num_threads, total_iterations, counts);
        
        std::cout << "spin_lock:" << std::endl;
        std::cout << "  各线程获取锁次数: ";
        for (int c : counts) std::cout << c << " ";
        std::cout << std::endl;
        std::cout << "  变异系数 (CV): " << std::fixed << std::setprecision(4) << cv << std::endl;
        std::cout << std::endl;
    }
    
    // ticket_spin_lock
    {
        my::ticket_spin_lock lock;
        double cv = test_fairness_impl(lock, num_threads, total_iterations, counts);
        
        std::cout << "ticket_spin_lock:" << std::endl;
        std::cout << "  各线程获取锁次数: ";
        for (int c : counts) std::cout << c << " ";
        std::cout << std::endl;
        std::cout << "  变异系数 (CV): " << std::fixed << std::setprecision(4) << cv << std::endl;
        std::cout << std::endl;
    }
    
    // mcs_lock
    {
        my::mcs_lock lock;
        double cv = test_fairness_mcs(lock, num_threads, total_iterations, counts);
        
        std::cout << "mcs_lock:" << std::endl;
        std::cout << "  各线程获取锁次数: ";
        for (int c : counts) std::cout << c << " ";
        std::cout << std::endl;
        std::cout << "  变异系数 (CV): " << std::fixed << std::setprecision(4) << cv << std::endl;
        std::cout << std::endl;
    }
}

// ============================================================================
// 高竞争场景测试
// ============================================================================

void run_high_contention_benchmark() {
    print_title("高竞争场景测试 (High Contention)");
    
    const int num_threads = 8;
    const int iterations = 50000;
    
    std::cout << std::endl;
    std::cout << "场景: " << num_threads << " 个线程，每个线程 " << iterations << " 次操作" << std::endl;
    std::cout << "临界区内有额外工作（模拟真实场景）" << std::endl;
    std::cout << std::endl;
    
    // spin_lock
    {
        my::spin_lock lock;
        volatile int counter = 0;
        std::vector<std::thread> threads;
        
        Timer timer;
        timer.start();
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < iterations; ++j) {
                    lock.lock();
                    // 临界区内做一些工作
                    for (volatile int k = 0; k < 10; ++k) {
                        counter++;
                    }
                    lock.unlock();
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        std::cout << "spin_lock:        " << std::fixed << std::setprecision(2) 
                  << timer.elapsed_ms() << " ms" << std::endl;
    }
    
    // ticket_spin_lock
    {
        my::ticket_spin_lock lock;
        volatile int counter = 0;
        std::vector<std::thread> threads;
        
        Timer timer;
        timer.start();
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < iterations; ++j) {
                    lock.lock();
                    for (volatile int k = 0; k < 10; ++k) {
                        counter++;
                    }
                    lock.unlock();
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        std::cout << "ticket_spin_lock: " << std::fixed << std::setprecision(2) 
                  << timer.elapsed_ms() << " ms" << std::endl;
    }
    
    // mcs_lock
    {
        my::mcs_lock lock;
        volatile int counter = 0;
        std::vector<std::thread> threads;
        
        Timer timer;
        timer.start();
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&]() {
                my::mcs_node node;
                for (int j = 0; j < iterations; ++j) {
                    lock.lock(&node);
                    for (volatile int k = 0; k < 10; ++k) {
                        counter++;
                    }
                    lock.unlock(&node);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        std::cout << "mcs_lock:         " << std::fixed << std::setprecision(2) 
                  << timer.elapsed_ms() << " ms" << std::endl;
    }
}

// ============================================================================
// 低竞争场景测试
// ============================================================================

void run_low_contention_benchmark() {
    print_title("低竞争场景测试 (Low Contention)");
    
    const int num_threads = 4;
    const int iterations = 50000;
    
    std::cout << std::endl;
    std::cout << "场景: " << num_threads << " 个线程，每个线程 " << iterations << " 次操作" << std::endl;
    std::cout << "临界区外有较长的工作时间，减少竞争" << std::endl;
    std::cout << std::endl;
    
    // spin_lock
    {
        my::spin_lock lock;
        int counter = 0;
        std::vector<std::thread> threads;
        
        Timer timer;
        timer.start();
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < iterations; ++j) {
                    // 临界区外做一些工作
                    for (volatile int k = 0; k < 50; ++k);
                    
                    lock.lock();
                    counter++;
                    lock.unlock();
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        std::cout << "spin_lock:        " << std::fixed << std::setprecision(2) 
                  << timer.elapsed_ms() << " ms" << std::endl;
    }
    
    // ticket_spin_lock
    {
        my::ticket_spin_lock lock;
        int counter = 0;
        std::vector<std::thread> threads;
        
        Timer timer;
        timer.start();
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < iterations; ++j) {
                    for (volatile int k = 0; k < 50; ++k);
                    
                    lock.lock();
                    counter++;
                    lock.unlock();
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        std::cout << "ticket_spin_lock: " << std::fixed << std::setprecision(2) 
                  << timer.elapsed_ms() << " ms" << std::endl;
    }
    
    // mcs_lock
    {
        my::mcs_lock lock;
        int counter = 0;
        std::vector<std::thread> threads;
        
        Timer timer;
        timer.start();
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&]() {
                my::mcs_node node;
                for (int j = 0; j < iterations; ++j) {
                    for (volatile int k = 0; k < 50; ++k);
                    
                    lock.lock(&node);
                    counter++;
                    lock.unlock(&node);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        std::cout << "mcs_lock:         " << std::fixed << std::setprecision(2) 
                  << timer.elapsed_ms() << " ms" << std::endl;
    }
}

// ============================================================================
// 主函数
// ============================================================================

int main() {
    std::cout << std::endl;
    print_separator('*');
    std::cout << "       锁性能对比测试: spin_lock vs ticket_spin_lock vs mcs_lock" << std::endl;
    print_separator('*');
    std::cout << std::endl;
    
    std::cout << "硬件信息:" << std::endl;
    std::cout << "  CPU 核心数: " << std::thread::hardware_concurrency() << std::endl;
    std::cout << std::endl;
    
    // 运行各项测试
    run_throughput_benchmark();
    std::cout << std::endl;
    
    run_fairness_benchmark();
    std::cout << std::endl;
    
    run_high_contention_benchmark();
    std::cout << std::endl;
    
    run_low_contention_benchmark();
    std::cout << std::endl;
    
    // 总结
    print_title("总结 (Summary)");
    std::cout << std::endl;
    std::cout << "1. spin_lock (TAS):" << std::endl;
    std::cout << "   - 优点: 实现简单，低竞争时性能好" << std::endl;
    std::cout << "   - 缺点: 不公平，可能导致线程饥饿" << std::endl;
    std::cout << std::endl;
    std::cout << "2. ticket_spin_lock:" << std::endl;
    std::cout << "   - 优点: FIFO 公平性，无饥饿" << std::endl;
    std::cout << "   - 缺点: 所有线程在同一变量上自旋，Cache Line Bouncing" << std::endl;
    std::cout << std::endl;
    std::cout << "3. mcs_lock:" << std::endl;
    std::cout << "   - 优点: FIFO 公平性，每个线程在本地变量上自旋，" << std::endl;
    std::cout << "           避免 Thundering Herd 和 Cache Line Bouncing" << std::endl;
    std::cout << "   - 缺点: 实现复杂，需要额外的节点存储" << std::endl;
    std::cout << std::endl;
    
    print_separator('*');
    std::cout << "                        测试完成" << std::endl;
    print_separator('*');
    std::cout << std::endl;
    
    return 0;
}
