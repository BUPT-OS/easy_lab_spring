#!/usr/bin/env python3
"""
score_benchmark.py - malloc 性能基准测试评分脚本

用法: python3 score_benchmark.py --benchdir <benchtests目录>

评分规则:
  - 内存吞吐量 (20分): 基于延迟比值 (lat / golden_lat)
  - 内存利用率 (15分): 基于RSS比值 (max_rss / golden_max_rss)
"""

import argparse
import json
import os
import sys
from typing import Dict, List, Optional, Tuple

# 评分阈值
LAT_THRESHOLDS = [
    (10, 20),  # lat_ratio <= 10  -> 20分
    (50, 15),  # lat_ratio <= 50  -> 15分
    (100, 10),  # lat_ratio <= 100 -> 10分
    (1000, 5),  # lat_ratio <= 1000 -> 5分
]
LAT_DEFAULT = 2

RSS_THRESHOLDS = [
    (1.1, 15),  # rss_ratio <= 1.1 -> 15分
    (1.5, 10),  # rss_ratio <= 1.5 -> 10分
    (2.0, 5),  # rss_ratio <= 2.0 -> 5分
]
RSS_DEFAULT = 2


# 颜色输出 (已禁用)
class Colors:
    GREEN = ""
    YELLOW = ""
    RED = ""
    BLUE = ""
    BOLD = ""
    END = ""


def colored(text: str, color: str) -> str:
    return text


def score_latency(ratio: float) -> int:
    """根据延迟比值计算得分"""
    for threshold, score in LAT_THRESHOLDS:
        if ratio <= threshold:
            return score
    return LAT_DEFAULT


def score_rss(ratio: float) -> int:
    """根据RSS比值计算得分"""
    for threshold, score in RSS_THRESHOLDS:
        if ratio <= threshold:
            return score
    return RSS_DEFAULT


def load_json_file(filepath: str) -> Optional[dict]:
    """加载JSON文件"""
    try:
        with open(filepath, "r") as f:
            return json.load(f)
    except (json.JSONDecodeError, FileNotFoundError) as e:
        print(f"警告: 无法加载文件 {filepath}: {e}", file=sys.stderr)
        return None


def find_matching_files(benchdir: str, prefix: str) -> Dict[str, Tuple[str, str]]:
    """
    找到匹配的 glibc 和自定义 malloc 结果文件对
    返回: {配置参数: (glibc文件路径, 自定义malloc文件路径)}
    """
    glibc_pattern = f"bench-malloc-{prefix}-"
    custom_pattern = f"bench-my_malloc-{prefix}-"

    matches = {}

    for f in os.listdir(benchdir):
        if f.startswith(glibc_pattern) and f.endswith(".out"):
            # 提取配置参数 (如 "8", "16", "32" 等)
            param = f[len(glibc_pattern) : -4]  # 去掉前缀和 .out
            glibc_file = os.path.join(benchdir, f)
            custom_file = os.path.join(benchdir, f"{custom_pattern}{param}.out")

            if os.path.exists(custom_file):
                matches[param] = (glibc_file, custom_file)

    return matches


def process_malloc_simple(
    benchdir: str,
) -> Tuple[Optional[float], Optional[float], List[dict]]:
    """
    处理 malloc-simple 测试结果
    返回: (平均延迟比值, 平均RSS比值, 详细结果列表)
    """
    matches = find_matching_files(benchdir, "simple")
    if not matches:
        return None, None, []

    lat_fields = [
        "main_arena_st_allocs_0025_time",
        "main_arena_st_allocs_0100_time",
        "main_arena_mt_allocs_0025_time",
        "main_arena_mt_allocs_0100_time",
        "thread_arena__allocs_0025_time",
        "thread_arena__allocs_0100_time",
    ]

    all_lat_ratios = []
    all_rss_ratios = []
    details = []

    for param in sorted(matches.keys(), key=lambda x: int(x)):
        glibc_file, custom_file = matches[param]

        glibc_data = load_json_file(glibc_file)
        custom_data = load_json_file(custom_file)

        if not glibc_data or not custom_data:
            continue

        try:
            glibc_malloc = glibc_data["functions"]["malloc"][""]
            custom_malloc = custom_data["functions"]["malloc"][""]

            # 计算延迟比值
            lat_ratios = []
            for field in lat_fields:
                if field in glibc_malloc and field in custom_malloc:
                    glibc_val = glibc_malloc[field]
                    custom_val = custom_malloc[field]
                    if glibc_val > 0:
                        lat_ratios.append(custom_val / glibc_val)

            avg_lat_ratio = sum(lat_ratios) / len(lat_ratios) if lat_ratios else None

            # 计算RSS比值
            glibc_rss = glibc_malloc.get("max_rss", 0)
            custom_rss = custom_malloc.get("max_rss", 0)
            rss_ratio = custom_rss / glibc_rss if glibc_rss > 0 else None

            if avg_lat_ratio is not None:
                all_lat_ratios.append(avg_lat_ratio)
            if rss_ratio is not None:
                all_rss_ratios.append(rss_ratio)

            details.append(
                {
                    "param": param,
                    "lat_ratio": avg_lat_ratio,
                    "rss_ratio": rss_ratio,
                    "glibc_rss": glibc_rss,
                    "custom_rss": custom_rss,
                }
            )

        except KeyError as e:
            print(f"警告: malloc-simple-{param} 数据格式错误: {e}", file=sys.stderr)

    avg_lat = sum(all_lat_ratios) / len(all_lat_ratios) if all_lat_ratios else None
    avg_rss = sum(all_rss_ratios) / len(all_rss_ratios) if all_rss_ratios else None

    return avg_lat, avg_rss, details


def process_malloc_tcache(benchdir: str) -> Tuple[Optional[float], List[dict]]:
    """
    处理 malloc-tcache 测试结果
    返回: (平均延迟比值, 详细结果列表)
    """
    matches = find_matching_files(benchdir, "tcache")
    if not matches:
        return None, []

    all_lat_ratios = []
    details = []

    for param in sorted(matches.keys(), key=lambda x: int(x)):
        glibc_file, custom_file = matches[param]

        glibc_data = load_json_file(glibc_file)
        custom_data = load_json_file(custom_file)

        if not glibc_data or not custom_data:
            continue

        try:
            glibc_malloc = glibc_data["functions"]["malloc"]
            custom_malloc = custom_data["functions"]["malloc"]

            lat_ratios = []
            detail = {"param": param}

            # simple 的 time_per_iteration
            if "simple" in glibc_malloc and "simple" in custom_malloc:
                glibc_simple = glibc_malloc["simple"].get("time_per_iteration", 0)
                custom_simple = custom_malloc["simple"].get("time_per_iteration", 0)
                if glibc_simple > 0:
                    ratio = custom_simple / glibc_simple
                    lat_ratios.append(ratio)
                    detail["simple_ratio"] = ratio

            # optimized 的 time_per_iteration
            if "optimized" in glibc_malloc and "optimized" in custom_malloc:
                glibc_opt = glibc_malloc["optimized"].get("time_per_iteration", 0)
                custom_opt = custom_malloc["optimized"].get("time_per_iteration", 0)
                if glibc_opt > 0:
                    ratio = custom_opt / glibc_opt
                    lat_ratios.append(ratio)
                    detail["optimized_ratio"] = ratio

            if lat_ratios:
                avg_ratio = sum(lat_ratios) / len(lat_ratios)
                detail["avg_lat_ratio"] = avg_ratio
                all_lat_ratios.append(avg_ratio)

            details.append(detail)

        except KeyError as e:
            print(f"警告: malloc-tcache-{param} 数据格式错误: {e}", file=sys.stderr)

    avg_lat = sum(all_lat_ratios) / len(all_lat_ratios) if all_lat_ratios else None

    return avg_lat, details


def process_malloc_thread(
    benchdir: str,
) -> Tuple[Optional[float], Optional[float], List[dict]]:
    """
    处理 malloc-thread 测试结果
    返回: (平均延迟比值, 平均RSS比值, 详细结果列表)
    """
    matches = find_matching_files(benchdir, "thread")
    if not matches:
        return None, None, []

    all_lat_ratios = []
    all_rss_ratios = []
    details = []

    for param in sorted(matches.keys(), key=lambda x: int(x)):
        glibc_file, custom_file = matches[param]

        glibc_data = load_json_file(glibc_file)
        custom_data = load_json_file(custom_file)

        if not glibc_data or not custom_data:
            continue

        try:
            glibc_malloc = glibc_data["functions"]["malloc"][""]
            custom_malloc = custom_data["functions"]["malloc"][""]

            # 计算延迟比值
            glibc_lat = glibc_malloc.get("time_per_iteration", 0)
            custom_lat = custom_malloc.get("time_per_iteration", 0)
            lat_ratio = custom_lat / glibc_lat if glibc_lat > 0 else None

            # 计算RSS比值
            glibc_rss = glibc_malloc.get("max_rss", 0)
            custom_rss = custom_malloc.get("max_rss", 0)
            rss_ratio = custom_rss / glibc_rss if glibc_rss > 0 else None

            if lat_ratio is not None:
                all_lat_ratios.append(lat_ratio)
            if rss_ratio is not None:
                all_rss_ratios.append(rss_ratio)

            details.append(
                {
                    "param": param,
                    "lat_ratio": lat_ratio,
                    "rss_ratio": rss_ratio,
                    "glibc_lat": glibc_lat,
                    "custom_lat": custom_lat,
                    "glibc_rss": glibc_rss,
                    "custom_rss": custom_rss,
                }
            )

        except KeyError as e:
            print(f"警告: malloc-thread-{param} 数据格式错误: {e}", file=sys.stderr)

    avg_lat = sum(all_lat_ratios) / len(all_lat_ratios) if all_lat_ratios else None
    avg_rss = sum(all_rss_ratios) / len(all_rss_ratios) if all_rss_ratios else None

    return avg_lat, avg_rss, details


def print_separator(char: str = "=", length: int = 60):
    print(char * length)


def print_header(title: str):
    print()
    print_separator()
    print(colored(f"  {title}", Colors.BOLD))
    print_separator()


def format_ratio(ratio: Optional[float]) -> str:
    if ratio is None:
        return "N/A"
    return f"{ratio:.4f}"


def format_score(score: int, max_score: int) -> str:
    if score >= max_score * 0.8:
        color = Colors.GREEN
    elif score >= max_score * 0.5:
        color = Colors.YELLOW
    else:
        color = Colors.RED
    return colored(f"{score}/{max_score}", color)


def main():
    parser = argparse.ArgumentParser(description="malloc 性能基准测试评分脚本")
    parser.add_argument("--benchdir", required=True, help="benchtests 目录路径")
    args = parser.parse_args()

    benchdir = args.benchdir

    if not os.path.isdir(benchdir):
        print(f"错误: 目录不存在: {benchdir}", file=sys.stderr)
        sys.exit(1)

    # 处理各测试
    simple_lat, simple_rss, simple_details = process_malloc_simple(benchdir)
    tcache_lat, tcache_details = process_malloc_tcache(benchdir)
    thread_lat, thread_rss, thread_details = process_malloc_thread(benchdir)

    # ==================== 输出报告 ====================
    print()
    print_separator("=", 60)
    print(colored("           malloc 性能基准测试评分报告", Colors.BOLD + Colors.BLUE))
    print_separator("=", 60)

    # ---------- malloc-simple ----------
    print_header("malloc-simple 测试结果")
    if simple_details:
        print(
            f"  {'配置':<8} | {'延迟比值':<12} | {'RSS比值':<12} | {'glibc RSS':<10} | {'自定义 RSS':<10}"
        )
        print(f"  {'-' * 8} | {'-' * 12} | {'-' * 12} | {'-' * 10} | {'-' * 10}")
        for d in simple_details:
            print(
                f"  {d['param']:<8} | {format_ratio(d['lat_ratio']):<12} | {format_ratio(d['rss_ratio']):<12} | {d.get('glibc_rss', 'N/A'):<10} | {d.get('custom_rss', 'N/A'):<10}"
            )
        print(f"  {'-' * 8} | {'-' * 12} | {'-' * 12} | {'-' * 10} | {'-' * 10}")
        print(
            f"  {'平均':<8} | {format_ratio(simple_lat):<12} | {format_ratio(simple_rss):<12}"
        )

        simple_lat_score = score_latency(simple_lat) if simple_lat else LAT_DEFAULT
        simple_rss_score = score_rss(simple_rss) if simple_rss else RSS_DEFAULT
        print()
        print(
            f"  延迟得分: {format_score(simple_lat_score, 20)}  |  RSS得分: {format_score(simple_rss_score, 15)}"
        )
    else:
        print("  (无数据)")
        simple_lat_score = LAT_DEFAULT
        simple_rss_score = RSS_DEFAULT

    # ---------- malloc-tcache ----------
    print_header("malloc-tcache 测试结果")
    if tcache_details:
        print(
            f"  {'配置':<8} | {'simple比值':<14} | {'optimized比值':<14} | {'平均比值':<12}"
        )
        print(f"  {'-' * 8} | {'-' * 14} | {'-' * 14} | {'-' * 12}")
        for d in tcache_details:
            print(
                f"  {d['param']:<8} | {format_ratio(d.get('simple_ratio')):<14} | {format_ratio(d.get('optimized_ratio')):<14} | {format_ratio(d.get('avg_lat_ratio')):<12}"
            )
        print(f"  {'-' * 8} | {'-' * 14} | {'-' * 14} | {'-' * 12}")
        print(f"  {'平均':<8} | {'':<14} | {'':<14} | {format_ratio(tcache_lat):<12}")

        tcache_lat_score = score_latency(tcache_lat) if tcache_lat else LAT_DEFAULT
        print()
        print(
            f"  延迟得分: {format_score(tcache_lat_score, 20)}  |  RSS得分: (不参与计算)"
        )
    else:
        print("  (无数据)")
        tcache_lat_score = LAT_DEFAULT

    # ---------- malloc-thread ----------
    print_header("malloc-thread 测试结果")
    if thread_details:
        print(
            f"  {'配置':<8} | {'延迟比值':<12} | {'RSS比值':<12} | {'glibc延迟':<12} | {'自定义延迟':<12}"
        )
        print(f"  {'-' * 8} | {'-' * 12} | {'-' * 12} | {'-' * 12} | {'-' * 12}")
        for d in thread_details:
            print(
                f"  {d['param']:<8} | {format_ratio(d['lat_ratio']):<12} | {format_ratio(d['rss_ratio']):<12} | {d.get('glibc_lat', 'N/A'):<12} | {d.get('custom_lat', 'N/A'):<12}"
            )
        print(f"  {'-' * 8} | {'-' * 12} | {'-' * 12} | {'-' * 12} | {'-' * 12}")
        print(
            f"  {'平均':<8} | {format_ratio(thread_lat):<12} | {format_ratio(thread_rss):<12}"
        )

        thread_lat_score = score_latency(thread_lat) if thread_lat else LAT_DEFAULT
        thread_rss_score = score_rss(thread_rss) if thread_rss else RSS_DEFAULT
        print()
        print(
            f"  延迟得分: {format_score(thread_lat_score, 20)}  |  RSS得分: {format_score(thread_rss_score, 15)}"
        )
    else:
        print("  (无数据)")
        thread_lat_score = LAT_DEFAULT
        thread_rss_score = RSS_DEFAULT

    # ==================== 最终得分 ====================
    print()
    print_separator("=", 60)
    print(colored("                    最终得分", Colors.BOLD + Colors.BLUE))
    print_separator("=", 60)
    print()

    # 计算最终得分
    final_lat_score = (simple_lat_score + tcache_lat_score + thread_lat_score) / 3
    final_rss_score = (simple_rss_score + thread_rss_score) / 2
    final_score = final_lat_score + final_rss_score

    print("  内存吞吐量得分 (满分20):")
    print(f"    = ({simple_lat_score} + {tcache_lat_score} + {thread_lat_score}) / 3")
    print(
        f"    = {colored(f'{final_lat_score:.2f}', Colors.GREEN if final_lat_score >= 15 else Colors.YELLOW)}"
    )
    print()
    print("  内存利用率得分 (满分15):")
    print(f"    = ({simple_rss_score} + {thread_rss_score}) / 2")
    print(
        f"    = {colored(f'{final_rss_score:.2f}', Colors.GREEN if final_rss_score >= 10 else Colors.YELLOW)}"
    )
    print()
    print_separator("-", 60)
    print()
    print(
        f"  {colored('总分', Colors.BOLD)}: {colored(f'{final_score:.2f}', Colors.BOLD + Colors.GREEN)} / 35"
    )
    print()
    print_separator("=", 60)

    # 输出评分等级
    if final_score >= 30:
        grade = colored("优秀 (A)", Colors.GREEN)
    elif final_score >= 25:
        grade = colored("良好 (B)", Colors.BLUE)
    elif final_score >= 20:
        grade = colored("中等 (C)", Colors.YELLOW)
    else:
        grade = colored("需改进 (D)", Colors.RED)

    print(f"  评级: {grade}")
    print()
    print(f"performance_score: {final_score}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
