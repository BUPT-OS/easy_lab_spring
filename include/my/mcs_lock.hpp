#ifndef MY_MCS_LOCK_HPP
#define MY_MCS_LOCK_HPP

#include <atomic>
#include <cstdint>

namespace my {

/**
 * @brief MCS Lock - 基于链表的自旋锁
 *
 * MCS Lock 由 Mellor-Crummey 和 Scott 在 1991 年提出，
 * 解决了 Ticket Spin Lock 的两个问题：
 *
 * 1. Thundering Herd 问题：
 *    - Ticket Lock 中，所有等待线程都在 now_serving 上自旋
 *    - 当锁释放时，所有 CPU 的 L1 Cache 都会失效，需要重新从内存/L3 读取
 *    - MCS Lock 中，每个线程在自己的本地变量上自旋，释放锁只影响下一个节点
 *
 * 2. Cache Line Bouncing：
 *    - 多个线程在同一个变量上自旋，导致 Cache Line 频繁失效
 *    - MCS Lock 每个线程自旋的变量不同，减少 Cache 争用
 *
 * 实现原理：
 * - 维护一个隐式的 FIFO 队列（链表）
 * - 每个想获取锁的线程创建一个节点
 * - 通过 CAS 将自己的节点挂到队列尾部
 * - 在自己节点的 locked 字段上自旋
 * - 前驱释放锁时，将后继节点的 locked 设为 false
 *
 * 注意：
 * - 这里使用 std::atomic 而不是 my::atomic，因为我们需要原子指针操作
 * - 学生版可以改用 my::atomic 配合内联汇编实现指针的原子操作
 */

/**
 * @brief MCS 节点，每个线程需要一个
 *
 * 使用方式：
 *   mcs_lock lock;
 *   mcs_node my_node;  // 每个线程有自己的节点
 *   lock.lock(&my_node);
 *   // 临界区
 *   lock.unlock(&my_node);
 */
struct mcs_node {
    std::atomic<mcs_node*> next{nullptr};  // 指向下一个等待的节点
    std::atomic<bool> locked{false};        // 是否需要等待

    mcs_node() = default;

    // 重置节点状态，便于重用
    void reset() {
        next.store(nullptr, std::memory_order_relaxed);
        locked.store(false, std::memory_order_relaxed);
    }
};

class mcs_lock {
private:
    // 队列尾部指针，nullptr 表示锁空闲
    std::atomic<mcs_node*> tail_{nullptr};

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
    mcs_lock() = default;

    // 禁止拷贝
    mcs_lock(const mcs_lock&) = delete;
    mcs_lock& operator=(const mcs_lock&) = delete;

    /**
     * @brief 获取锁
     * @param node 调用者提供的节点（每个线程需要自己的节点）
     *
     * 步骤：
     * 1. 初始化自己的节点：next = null, locked = true
     * 2. 原子地将 tail 指向自己，获取旧的 tail
     * 3. 如果旧 tail 为 null，说明锁空闲，直接获得
     * 4. 否则，将自己挂到前驱的 next 上，在自己的 locked 上自旋
     */
    void lock(mcs_node* node) {
        // ==================== 学生填写区域 开始 ====================
        // 提示：
        // 1. 初始化节点: node->next = nullptr, node->locked = true
        // 2. 使用 tail_.exchange(node) 将自己加入队列尾部，获取前驱
        // 3. 如果前驱为空 (predecessor == nullptr)，直接获得锁，return
        // 4. 否则：
        //    a. 将前驱的 next 设为自己: predecessor->next.store(node)
        //    b. 在自己的 locked 上自旋: while (node->locked.load()) cpu_pause();
        //
        // 注意内存序: exchange 用 acq_rel, store 用 release, load 用 acquire

        // TODO: 在此处实现 MCS lock 的 lock 逻辑

        // ==================== 学生填写区域 结束 ====================
    }

    /**
     * @brief 释放锁
     * @param node 调用者的节点（与 lock 时使用的相同）
     *
     * 步骤：
     * 1. 检查是否有后继节点
     * 2. 如果没有后继，尝试 CAS 将 tail 从自己改为 null
     * 3. CAS 失败说明有人正在加入，等待 next 被设置
     * 4. 将后继的 locked 设为 false，唤醒后继
     */
    void unlock(mcs_node* node) {
        // ==================== 学生填写区域 开始 ====================
        // 提示：
        // 1. 读取 node->next
        // 2. 如果 next 为空:
        //    a. 尝试 CAS: tail_.compare_exchange_strong(expected=node, desired=nullptr)
        //    b. CAS 成功则完成（没有其他线程在等待），return
        //    c. CAS 失败说明有线程正在加入队列，等待 next 被设置
        //       while ((successor = node->next.load()) == nullptr) cpu_pause();
        // 3. 将后继的 locked 设为 false: successor->locked.store(false)

        // TODO: 在此处实现 MCS lock 的 unlock 逻辑

        // ==================== 学生填写区域 结束 ====================
    }

    /**
     * @brief 尝试获取锁（非阻塞）
     * @param node 调用者提供的节点
     * @return 如果成功获取锁返回 true，否则返回 false
     */
    bool try_lock(mcs_node* node) {
        // ==================== 学生填写区域 开始 ====================
        // 提示：
        // 只有当 tail_ 为 null 时才尝试获取
        // 1. 初始化节点
        // 2. 使用 CAS: tail_.compare_exchange_strong(expected=nullptr, desired=node)

        // TODO: 在此处实现 try_lock 逻辑
        return false;

        // ==================== 学生填写区域 结束 ====================
    }
};

/**
 * @brief MCS Lock 的 RAII 守卫
 *
 * 注意：需要在构造时提供节点，或者使用内部节点
 */
class mcs_lock_guard {
private:
    mcs_lock& lock_;
    mcs_node node_;

public:
    explicit mcs_lock_guard(mcs_lock& lock) : lock_(lock) {
        lock_.lock(&node_);
    }

    ~mcs_lock_guard() {
        lock_.unlock(&node_);
    }

    mcs_lock_guard(const mcs_lock_guard&) = delete;
    mcs_lock_guard& operator=(const mcs_lock_guard&) = delete;
};

} // namespace my

#endif // MY_MCS_LOCK_HPP
