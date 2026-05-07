#ifndef MY_ATOMIC_HPP
#define MY_ATOMIC_HPP

#include <cstdint>

namespace my {

/**
 * @brief 使用内联汇编实现的 4 字节原子类型
 *
 * 本类实现了基本的原子操作：
 * - load(): 原子读取
 * - store(): 原子写入
 * - exchange(): 原子交换
 * - compare_exchange_strong(): CAS 操作
 * - fetch_add(): 原子加法
 * - fetch_sub(): 原子减法
 */
class atomic {
private:
    // volatile 防止编译器优化掉对该变量的读写
    volatile int32_t value_;

public:
    atomic() : value_(0) {}
    explicit atomic(int32_t val) : value_(val) {}

    // 禁止拷贝和赋值
    atomic(const atomic&) = delete;
    atomic& operator=(const atomic&) = delete;

    /**
     * @brief 原子读取
     * @return 当前值
     */
    int32_t load() const {
        int32_t result = 0;
        // ==================== 学生填写区域 开始 ====================
        // 提示：使用 mov 指令读取 value_ 到 result
        // x86 的对齐内存访问本身是原子的，但需要内存屏障防止重排
        //
        // 内联汇编格式: __asm__ __volatile__("指令" : 输出 : 输入 : 破坏列表);
        // 输出约束: "=r"(result) 表示 result 放在任意通用寄存器
        // 输入约束: "m"(value_) 表示 value_ 在内存中
        // 破坏列表: "memory" 表示内存屏障

        // TODO: 在此处填写内联汇编代码

        // ==================== 学生填写区域 结束 ====================
        return result;
    }

    /**
     * @brief 原子写入
     * @param val 要写入的值
     */
    void store(int32_t val) {
        // ==================== 学生填写区域 开始 ====================
        // 提示：使用 mov 指令将 val 写入 value_
        // 输出约束: "=m"(value_) 表示 value_ 在内存中
        // 输入约束: "r"(val) 表示 val 放在任意通用寄存器

        // TODO: 在此处填写内联汇编代码

        // ==================== 学生填写区域 结束 ====================
    }

    /**
     * @brief 原子交换
     * @param val 要交换的新值
     * @return 交换前的旧值
     */
    int32_t exchange(int32_t val) {
        int32_t result = 0;
        // ==================== 学生填写区域 开始 ====================
        // 提示：使用 xchg 指令，它隐含了 lock 前缀
        // xchg 会原子地交换寄存器和内存的值
        //
        // 输出约束: "=r"(result) 交换后的旧值
        //           "+m"(value_) 内存位置（同时也是输入）
        // 输入约束: "0"(val) 表示初始时 result 对应的寄存器 = val

        // TODO: 在此处填写内联汇编代码

        // ==================== 学生填写区域 结束 ====================
        return result;
    }

    /**
     * @brief CAS (Compare-And-Swap) 操作
     * @param expected 期望的当前值（如果失败会被更新为实际值）
     * @param desired 如果当前值等于 expected，则写入 desired
     * @return 如果交换成功返回 true，否则返回 false
     */
    bool compare_exchange_strong(int32_t& expected, int32_t desired) {
        int32_t old_val = expected;
        int32_t result = value_;
        // ==================== 学生填写区域 开始 ====================
        // 提示：使用 lock cmpxchg 指令
        // cmpxchg 比较 eax 和目标操作数：
        //   - 如果相等：ZF=1，目标操作数 = 源操作数
        //   - 如果不等：ZF=0，eax = 目标操作数
        //
        // 输出约束: "=a"(result) 表示 result 必须在 eax
        //           "+m"(value_) 内存位置
        // 输入约束: "0"(old_val) 表示 eax = expected
        //           "r"(desired) 表示 desired 在寄存器中
        // 破坏列表: "memory", "cc" (标志寄存器)

        // TODO: 在此处填写内联汇编代码

        // ==================== 学生填写区域 结束 ====================

        if (result == old_val) {
            return true;  // 交换成功
        } else {
            expected = result;  // 更新 expected 为实际值
            return false;
        }
    }

    /**
     * @brief 原子加法
     * @param val 要加的值
     * @return 加法前的旧值
     */
    int32_t fetch_add(int32_t val) {
        int32_t result = 0;
        // ==================== 学生填写区域 开始 ====================
        // 提示：使用 lock xadd 指令
        // xadd 会原子地：temp = dst; dst = dst + src; src = temp
        //
        // 输出约束: "=r"(result) 加法前的旧值
        //           "+m"(value_) 内存位置（会被修改）
        // 输入约束: "0"(val) 表示初始时 result 对应的寄存器 = val
        // 破坏列表: "memory", "cc"

        // TODO: 在此处填写内联汇编代码

        // ==================== 学生填写区域 结束 ====================
        return result;
    }

    /**
     * @brief 原子减法
     * @param val 要减的值
     * @return 减法前的旧值
     */
    int32_t fetch_sub(int32_t val) {
        // fetch_sub(val) 等价于 fetch_add(-val)
        return fetch_add(-val);
    }

    // 运算符重载，方便使用
    int32_t operator++() { return fetch_add(1) + 1; }    // 前置++
    int32_t operator++(int) { return fetch_add(1); }     // 后置++
    int32_t operator--() { return fetch_sub(1) - 1; }    // 前置--
    int32_t operator--(int) { return fetch_sub(1); }     // 后置--
    int32_t operator+=(int32_t val) { return fetch_add(val) + val; }
    int32_t operator-=(int32_t val) { return fetch_sub(val) - val; }

    // 隐式转换
    operator int32_t() const { return load(); }
};

} // namespace my

#endif // MY_ATOMIC_HPP
