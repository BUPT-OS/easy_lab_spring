/**
 * @file dining_philosophers.cpp
 * @brief 哲学家进餐问题实现
 *
 * 问题描述：
 * - 5 个哲学家围坐在一张圆桌旁
 * - 每两个哲学家之间有一根筷子（共 5 根）
 * - 哲学家要么思考，要么吃饭
 * - 吃饭需要同时拿起左右两边的筷子
 * - 吃完后放下筷子继续思考
 *
 * 潜在问题：
 * - 死锁：所有哲学家同时拿起左边的筷子，然后等待右边的筷子
 * - 饥饿：某些哲学家可能长时间无法获得筷子
 *
 * 解决方案（本实现使用方案1）：
 * 1. 资源有序分配：编号较小的哲学家先拿编号小的筷子
 * 2. 限制同时进餐人数：最多允许 4 个哲学家同时尝试拿筷子
 * 3. 同时拿起两根筷子（原子操作）
 */

#include <my/semaphore.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <atomic>
#include <iomanip>

// 哲学家数量
constexpr int NUM_PHILOSOPHERS = 5;

// 每个哲学家吃饭的次数
constexpr int MEALS_PER_PHILOSOPHER = 3;

// 筷子（信号量数组）
my::semaphore chopsticks[NUM_PHILOSOPHERS] = {
    my::semaphore(1),
    my::semaphore(1),
    my::semaphore(1),
    my::semaphore(1),
    my::semaphore(1)
};

// 统计信息
std::atomic<int> meals_eaten[NUM_PHILOSOPHERS];
std::atomic<int> total_meals(0);

// 输出锁（用于防止输出交错）
my::semaphore print_mutex(1);

/**
 * @brief 线程安全的打印函数
 */
void safe_print(const std::string& msg) {
    print_mutex.wait();
    std::cout << msg << std::endl;
    print_mutex.signal();
}

/**
 * @brief 获取左边筷子的编号
 */
int left_chopstick(int philosopher_id) {
    return philosopher_id;
}

/**
 * @brief 获取右边筷子的编号
 */
int right_chopstick(int philosopher_id) {
    return (philosopher_id + 1) % NUM_PHILOSOPHERS;
}

/**
 * @brief 哲学家思考
 */
void think(int id) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100, 500);

    safe_print("[哲学家 " + std::to_string(id) + "] 正在思考...");
    std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
}

/**
 * @brief 哲学家吃饭
 */
void eat(int id) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100, 300);

    safe_print("[哲学家 " + std::to_string(id) + "] 正在吃饭...");
    std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));

    meals_eaten[id].fetch_add(1);
    total_meals.fetch_add(1);
}

/**
 * @brief 拿起筷子（使用资源有序分配避免死锁）
 *
 * 策略：总是先拿编号较小的筷子
 * 这样可以避免循环等待，从而避免死锁
 */
void pick_up_chopsticks(int id) {
    // ==================== 学生填写区域 开始 ====================
    // 提示：
    // 1. 计算左右筷子的编号: left_chopstick(id), right_chopstick(id)
    // 2. 确定先拿哪根：总是先拿编号较小的筷子，避免死锁
    //    int first = min(left, right);
    //    int second = max(left, right);
    // 3. 使用 chopsticks[first].wait() 拿起第一根
    // 4. 使用 chopsticks[second].wait() 拿起第二根
    // 5. 可以用 safe_print() 输出日志

    // TODO: 在此处实现拿起筷子的逻辑

    // ==================== 学生填写区域 结束 ====================
}

/**
 * @brief 放下筷子
 */
void put_down_chopsticks(int id) {
    // ==================== 学生填写区域 开始 ====================
    // 提示：
    // 1. 放下两根筷子
    // 2. 使用 chopsticks[i].signal() 来释放筷子
    // 3. 放下顺序不重要（不会造成死锁）

    // TODO: 在此处实现放下筷子的逻辑

    // ==================== 学生填写区域 结束 ====================
}

/**
 * @brief 哲学家线程函数
 */
void philosopher(int id) {
    for (int meal = 0; meal < MEALS_PER_PHILOSOPHER; ++meal) {
        think(id);
        pick_up_chopsticks(id);
        eat(id);
        put_down_chopsticks(id);
    }

    safe_print("[哲学家 " + std::to_string(id) + "] 已吃完所有餐点，离开餐桌。");
}

/**
 * @brief 打印餐桌状态（ASCII 艺术）
 */
void print_table_status() {
    std::cout << std::endl;
    std::cout << "         [哲学家 0]" << std::endl;
    std::cout << "        /         \\" << std::endl;
    std::cout << "     (0)           (1)" << std::endl;
    std::cout << "      /             \\" << std::endl;
    std::cout << "[哲学家 4]         [哲学家 1]" << std::endl;
    std::cout << "      |             |" << std::endl;
    std::cout << "     (4)           (2)" << std::endl;
    std::cout << "      \\             /" << std::endl;
    std::cout << "[哲学家 3] --(3)-- [哲学家 2]" << std::endl;
    std::cout << std::endl;
    std::cout << "圆括号内的数字表示筷子编号" << std::endl;
    std::cout << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "         哲学家进餐问题演示" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "哲学家数量: " << NUM_PHILOSOPHERS << std::endl;
    std::cout << "每人进餐次数: " << MEALS_PER_PHILOSOPHER << std::endl;
    std::cout << "解决方案: 资源有序分配（总是先拿编号较小的筷子）" << std::endl;

    print_table_status();

    // 初始化统计
    for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        meals_eaten[i].store(0);
    }

    std::cout << "========================================" << std::endl;
    std::cout << "               开始进餐" << std::endl;
    std::cout << "========================================" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    // 创建哲学家线程
    std::vector<std::thread> philosophers;
    for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        philosophers.emplace_back(philosopher, i);
    }

    // 等待所有哲学家完成
    for (auto& p : philosophers) {
        p.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "              运行结果" << std::endl;
    std::cout << "========================================" << std::endl;

    std::cout << "各哲学家进餐次数:" << std::endl;
    for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        std::cout << "  哲学家 " << i << ": " << meals_eaten[i].load() << " 次" << std::endl;
    }

    std::cout << "总进餐次数: " << total_meals.load() << std::endl;
    std::cout << "总耗时: " << duration.count() << " ms" << std::endl;

    // 验证
    int expected_total = NUM_PHILOSOPHERS * MEALS_PER_PHILOSOPHER;
    if (total_meals.load() == expected_total) {
        std::cout << std::endl;
        std::cout << "[SUCCESS] 哲学家进餐问题解决成功！无死锁发生。" << std::endl;
    } else {
        std::cout << std::endl;
        std::cout << "[ERROR] 进餐次数不正确！" << std::endl;
        return 1;
    }

    return 0;
}
