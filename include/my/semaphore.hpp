#ifndef MY_SEMAPHORE_HPP
#define MY_SEMAPHORE_HPP

#include <cstdint>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <limits.h>
#include <cerrno>

namespace my {

/**
 * @brief 使用 Linux futex 系统调用实现的信号量
 *
 * futex (Fast Userspace muTEX) 是 Linux 提供的一种用户空间同步机制：
 * - 在无竞争时，完全在用户空间操作，无系统调用开销
 * - 在有竞争时，才陷入内核进行等待/唤醒
 *
 * 主要操作：
 * - FUTEX_WAIT: 如果 *addr == val，则睡眠等待
 * - FUTEX_WAKE: 唤醒最多 n 个在 addr 上等待的线程
 */
class semaphore {
private:
    // 信号量的值，使用 volatile 防止编译器优化
    // 注意：这里没有使用 my::atomic，因为 futex 要求的是普通整数地址
    volatile int32_t value_;

    /**
     * @brief 封装 futex 系统调用
     */
    static long futex(volatile int32_t* uaddr, int futex_op, int val,
                      const struct timespec* timeout = nullptr,
                      int* uaddr2 = nullptr, int val3 = 0) {
        return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
    }

    /**
     * @brief 原子比较并交换
     * 使用 GCC 内置函数，因为我们需要和 futex 配合使用
     */
    static bool atomic_cas(volatile int32_t* ptr, int32_t expected, int32_t desired) {
        return __sync_bool_compare_and_swap(ptr, expected, desired);
    }

    /**
     * @brief 原子加法
     */
    static int32_t atomic_add(volatile int32_t* ptr, int32_t val) {
        return __sync_fetch_and_add(ptr, val);
    }

public:
    /**
     * @brief 构造函数
     * @param initial_value 信号量初始值（默认为0）
     */
    explicit semaphore(int32_t initial_value = 0) : value_(initial_value) {}

    // 禁止拷贝
    semaphore(const semaphore&) = delete;
    semaphore& operator=(const semaphore&) = delete;

    /**
     * @brief P 操作 / wait() / down()
     *
     * 如果信号量值 > 0，则减 1 并继续
     * 如果信号量值 <= 0，则阻塞等待
     */
    void wait() {
        // ==================== 学生填写区域 开始 ====================
        // 提示：
        // 1. 循环尝试将 value_ 减 1
        // 2. 如果 value_ > 0，使用 atomic_cas 尝试减 1
        //    - CAS 成功则 return（成功获取资源）
        //    - CAS 失败则重试（有竞争）
        // 3. 如果 value_ <= 0，使用 futex(&value_, FUTEX_WAIT, current) 等待
        //    - FUTEX_WAIT 会检查 value_ 是否仍等于 current，是则睡眠
        // 4. 被唤醒后重新尝试（回到循环开始）

        // TODO: 在此处实现 wait (P操作) 逻辑

        // ==================== 学生填写区域 结束 ====================
    }

    /**
     * @brief 非阻塞版本的 wait
     * @return 如果成功减 1 返回 true，否则返回 false
     */
    bool try_wait() {
        // ==================== 学生填写区域 开始 ====================
        // 如果 value_ > 0，则尝试 CAS 减 1

        // TODO: 在此处实现 try_wait 逻辑
        return false;

        // ==================== 学生填写区域 结束 ====================
    }

    /**
     * @brief V 操作 / signal() / up() / post()
     *
     * 将信号量值加 1，并唤醒一个等待的线程
     */
    void signal() {
        // ==================== 学生填写区域 开始 ====================
        // 提示：
        // 1. 使用 atomic_add(&value_, 1) 原子地将 value_ 加 1
        // 2. 如果之前的值 <= 0，说明可能有线程在等待
        //    使用 futex(&value_, FUTEX_WAKE, 1) 唤醒最多 1 个线程

        // TODO: 在此处实现 signal (V操作) 逻辑

        // ==================== 学生填写区域 结束 ====================
    }

    /**
     * @brief 获取当前信号量值（用于调试）
     */
    int32_t get_value() const {
        return value_;
    }
};

/**
 * @brief 二元信号量（互斥锁）
 * 信号量初始值为 1
 */
class binary_semaphore : public semaphore {
public:
    binary_semaphore() : semaphore(1) {}
};

/**
 * @brief 使用信号量实现的互斥锁
 */
class mutex {
private:
    semaphore sem_;
public:
    mutex() : sem_(1) {}

    void lock() { sem_.wait(); }
    bool try_lock() { return sem_.try_wait(); }
    void unlock() { sem_.signal(); }
};

} // namespace my

#endif // MY_SEMAPHORE_HPP
