#ifndef MY_SPIN_LOCK_HPP
#define MY_SPIN_LOCK_HPP

#include "atomic.hpp"

namespace my {

/**
 * @brief 使用 my::atomic 实现的自旋锁
 *
 * 自旋锁的基本原理：
 * - 锁空闲时 locked_ = 0
 * - 锁被持有时 locked_ = 1
 * - 获取锁：尝试将 0 改为 1 (CAS 操作)
 * - 释放锁：将 1 改为 0
 *
 * 注意：
 * - 这是一个不公平锁，可能导致线程饥饿
 * - 在自旋等待时使用 pause 指令优化 CPU 性能
 */
class spin_lock {
private:
    atomic locked_;  // 0 = 未锁定, 1 = 已锁定

    /**
     * @brief CPU 暂停指令，优化自旋等待
     *
     * pause 指令的作用（x86）：
     * 1. 提示 CPU 当前处于自旋等待状态
     * 2. 避免内存序违规导致的流水线清空
     * 3. 降低功耗
     * 4. 让出流水线资源给其他超线程
     *
     * ARM 架构使用 yield 指令达到类似效果
     */
    static inline void cpu_pause() {
        // ==================== 学生填写区域 开始 ====================
        // 提示：
        // - x86/x86_64: 使用 pause 指令
        // - ARM: 使用 yield 指令
        // - 可以使用 #if defined(__x86_64__) || defined(__i386__) 等宏判断架构
        // - 使用 __asm__ __volatile__("指令" ::: "memory");

        // TODO: 在此处填写不同架构的 pause/yield 指令

        // ==================== 学生填写区域 结束 ====================
    }

public:
    spin_lock() : locked_(0) {}

    // 禁止拷贝和移动
    spin_lock(const spin_lock&) = delete;
    spin_lock& operator=(const spin_lock&) = delete;

    /**
     * @brief 获取锁
     *
     * 使用 TAS (Test-And-Set) 方式获取锁：
     * 尝试用 exchange 将 locked_ 从 0 变为 1
     * 如果返回的旧值是 0，说明获取成功
     * 如果返回的旧值是 1，说明锁已被占用，继续自旋
     */
    void lock() {
        // ==================== 学生填写区域 开始 ====================
        // 方法1：使用 exchange (TAS)
        // 尝试将 locked_ 设为 1，如果返回 0 表示之前未锁定，获取成功
        //
        // 优化提示：在外层 exchange 失败后，可以先用 load() 检查锁状态
        // 这样在等待时避免频繁的原子操作（TTAS: Test-and-Test-and-Set）

        // TODO: 在此处实现自旋锁的 lock 逻辑

        // ==================== 学生填写区域 结束 ====================
    }

    /**
     * @brief 尝试获取锁（非阻塞）
     * @return 如果成功获取锁返回 true，否则返回 false
     */
    bool try_lock() {
        // ==================== 学生填写区域 开始 ====================
        // 只尝试一次 exchange，不自旋

        // TODO: 在此处实现 try_lock 逻辑
        return false;

        // ==================== 学生填写区域 结束 ====================
    }

    /**
     * @brief 释放锁
     */
    void unlock() {
        // ==================== 学生填写区域 开始 ====================
        // 将 locked_ 设为 0，表示锁已释放

        // TODO: 在此处实现 unlock 逻辑

        // ==================== 学生填写区域 结束 ====================
    }
};

/**
 * @brief RAII 风格的锁守卫
 * 构造时自动获取锁，析构时自动释放锁
 */
template<typename Lock>
class lock_guard {
private:
    Lock& lock_;
public:
    explicit lock_guard(Lock& lock) : lock_(lock) {
        lock_.lock();
    }
    ~lock_guard() {
        lock_.unlock();
    }
    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;
};

} // namespace my

#endif // MY_SPIN_LOCK_HPP
