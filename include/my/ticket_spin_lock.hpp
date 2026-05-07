#ifndef MY_TICKET_SPIN_LOCK_HPP
#define MY_TICKET_SPIN_LOCK_HPP

#include "atomic.hpp"

namespace my {

/**
 * @brief Ticket Spin Lock - 公平的自旋锁
 *
 * 解决的问题：
 * - 简单的自旋锁是不公平的，多个等待线程竞争时，谁 CAS 成功谁获得锁
 * - 可能导致某些线程"饥饿"，长时间无法获得锁
 *
 * 实现原理（类似银行取号排队）：
 * - next_ticket: 下一个要发放的号码
 * - now_serving: 当前正在服务的号码
 * - 获取锁时：原子地取一个号码，然后等待自己的号码被叫到
 * - 释放锁时：将 now_serving 加 1，叫下一个号
 *
 * 特点：
 * - 保证 FIFO 顺序，公平性好
 * - 所有等待线程在同一个变量 now_serving 上自旋
 * - 存在 Thundering Herd 问题：释放锁时，所有等待线程都会检测到变化
 */
class ticket_spin_lock {
private:
    alignas(64) atomic next_ticket_;   // 下一个要发放的号码
    alignas(64) atomic now_serving_;   // 当前正在服务的号码

    /**
     * @brief CPU 暂停指令
     */
    static inline void cpu_pause() {
#if defined(__x86_64__) || defined(__i386__)
        __asm__ __volatile__("pause" ::: "memory");
#elif defined(__aarch64__) || defined(__arm__)
        __asm__ __volatile__("yield" ::: "memory");
#else
        __asm__ __volatile__("" ::: "memory");
#endif
    }

public:
    ticket_spin_lock() : next_ticket_(0), now_serving_(0) {}

    // 禁止拷贝
    ticket_spin_lock(const ticket_spin_lock&) = delete;
    ticket_spin_lock& operator=(const ticket_spin_lock&) = delete;

    /**
     * @brief 获取锁
     *
     * 步骤：
     * 1. 原子地获取一个票号 (fetch_add)
     * 2. 等待直到 now_serving == 我的票号
     */
    void lock() {
        // ==================== 学生填写区域 开始 ====================
        // 提示：
        // 1. 使用 fetch_add 原子地获取一个票号
        // 2. 在循环中等待 now_serving_ == my_ticket

        // TODO: 在此处实现 ticket lock 的 lock 逻辑

        // ==================== 学生填写区域 结束 ====================
    }

    /**
     * @brief 尝试获取锁（非阻塞）
     * @return 如果成功获取锁返回 true，否则返回 false
     */
    bool try_lock() {
        // ==================== 学生填写区域 开始 ====================
        // 提示：
        // 1. 读取 next_ticket 和 now_serving
        // 2. 如果相等，尝试 CAS 将 next_ticket 加 1

        // TODO: 在此处实现 try_lock 逻辑
        return false;

        // ==================== 学生填写区域 结束 ====================
    }

    /**
     * @brief 释放锁
     *
     * 将 now_serving 加 1，让下一个等待的线程获得锁
     */
    void unlock() {
        // ==================== 学生填写区域 开始 ====================
        // 提示：将 now_serving_ 加 1
        // 注意：这里使用 fetch_add 而不是简单的 store，
        // 是为了确保原子性（虽然只有锁持有者会调用 unlock）

        // TODO: 在此处实现 unlock 逻辑

        // ==================== 学生填写区域 结束 ====================
    }
};

} // namespace my

#endif // MY_TICKET_SPIN_LOCK_HPP
