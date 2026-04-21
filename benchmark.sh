#!/bin/bash

#==============================================================================
# benchmark.sh - glibc malloc 基准测试脚本
#
# 用法:
#   ./benchmark.sh --init                    # 初始化glibc环境
#   ./benchmark.sh --preload <path>          # 测试自定义malloc
#   ./benchmark.sh --init --preload <path>   # 初始化并测试
#   ./benchmark.sh --help                    # 显示帮助
#==============================================================================

set -e

#==============================================================================
# 全局变量
#==============================================================================

GLIBC_VERSION="2.42"
GLIBC_URL="https://ftp.gnu.org/gnu/glibc/glibc-${GLIBC_VERSION}.tar.xz"
GLIBC_SRC="$HOME/glibc"
GLIBC_BUILD="$HOME/glibc-build"
BENCHSET="malloc-simple malloc-tcache malloc-thread"

# 参数标志
SHOULD_INIT=0
PRELOAD_PATH=""

#==============================================================================
# 辅助函数
#==============================================================================

log_info() {
    echo -e "\033[32m[INFO]\033[0m $1"
}

log_warn() {
    echo -e "\033[33m[WARN]\033[0m $1"
}

log_error() {
    echo -e "\033[31m[ERROR]\033[0m $1"
}

check_command() {
    if ! command -v "$1" &> /dev/null; then
        log_error "命令 '$1' 未找到，请先安装"
        exit 1
    fi
}

check_dependencies() {
    log_info "检查依赖..."
    check_command wget
    check_command tar
    check_command make
    check_command gcc
    log_info "依赖检查通过"
}

show_help() {
    cat << EOF
用法: $0 [选项]

选项:
    -i, --init              初始化glibc环境（下载、编译、配置）
    -p, --preload <path>    指定自定义malloc库路径进行测试
    -h, --help              显示此帮助信息

示例:
    # 首次使用：初始化glibc环境
    $0 --init

    # 测试自定义malloc
    $0 --preload $PWD/libmymalloc_solution.so

    # 初始化并测试
    $0 --init --preload $PWD/libmymalloc_solution.so

环境变量:
    GLIBC_SRC       glibc源码目录 (默认: \$HOME/glibc)
    GLIBC_BUILD     glibc构建目录 (默认: \$HOME/glibc-build)
EOF
}

#==============================================================================
# 初始化流程函数 (步骤 1-6)
#==============================================================================

# 步骤1: 下载glibc
step1_download() {
    log_info "步骤1: 下载 glibc ${GLIBC_VERSION}..."

    local tarball="$HOME/glibc-${GLIBC_VERSION}.tar.xz"

    wget -O "$tarball" "$GLIBC_URL"

    if [ ! -f "$tarball" ]; then
        log_error "下载失败"
        rm -rf "$tmp_dir"
        exit 1
    fi

    echo "$tarball"
}

# 步骤2: 解压glibc
step2_extract() {
    local tarball="$HOME/glibc-${GLIBC_VERSION}.tar.xz"

    log_info "步骤2: 解压到 $GLIBC_SRC..."

    # 如果目录存在则删除
    if [ -d "$GLIBC_SRC" ]; then
        log_warn "删除已存在的 $GLIBC_SRC"
        rm -rf "$GLIBC_SRC"
    fi

    # 解压到$HOME，然后重命名
    tar -xf "$tarball" -C "$HOME"
    mv "$HOME/glibc-${GLIBC_VERSION}" "$GLIBC_SRC"

    # 清理临时文件
    rm -f "$tarball"
    rmdir "$(dirname "$tarball")" 2>/dev/null || true

    log_info "解压完成"
}

# 步骤3: 准备构建目录
step3_prepare_build() {
    log_info "步骤3: 准备构建目录 $GLIBC_BUILD..."

    if [ -d "$GLIBC_BUILD" ]; then
        log_warn "清空已存在的 $GLIBC_BUILD"
        rm -rf "$GLIBC_BUILD"/*
    else
        mkdir -p "$GLIBC_BUILD"
    fi

    log_info "构建目录准备完成"
}

# 步骤4: 配置glibc
step4_configure() {
    log_info "步骤4: 配置 glibc..."

    pushd "$GLIBC_BUILD"
    "$GLIBC_SRC/configure" --prefix=/usr/custom_glibc
    popd

    log_info "配置完成"
}

# 步骤5: 编译glibc
step5_build() {
    log_info "步骤5: 编译 glibc (使用 $(nproc) 个线程)..."

    pushd "$GLIBC_BUILD"
    make -j$(nproc)
    popd
    log_info "编译完成"
}

# 步骤6: 修改benchtests/Makefile
step6_patch_makefile() {
    log_info "步骤6: 修改 benchtests/Makefile..."

    local makefile="$GLIBC_SRC/benchtests/Makefile"

    if [ ! -f "$makefile" ]; then
        log_error "Makefile 不存在: $makefile"
        exit 1
    fi

    # 检查是否已经修改过
    if grep -q 'CUSTOM_ENV' "$makefile"; then
        log_warn "Makefile 已经修改过，跳过"
        return 0
    fi

    # 使用sed进行替换
    # 原始内容:
    # run-bench = $(test-wrapper-env) \
    #             $(run-program-env) \
    #             $($*-ENV) $(test-via-rtld-prefix) $${run}
    # 替换为:
    # run-bench = $(test-wrapper-env) \
    #             $(run-program-env) \
    #             $(CUSTOM_ENV) $($*-ENV) $(test-via-rtld-prefix) $${run}

    sed -i 's/\$(run-program-env) \\\n\t\t    \$(\$\*-ENV)/$(run-program-env) \\\n\t\t    $(CUSTOM_ENV) $($*-ENV)/' "$makefile"

    # 验证修改
    if grep -q 'CUSTOM_ENV' "$makefile"; then
        log_info "Makefile 修改成功"
    else
        # 尝试另一种sed模式
        log_warn "第一种sed模式失败，尝试备用方案..."

        # 备用方案：使用perl进行多行替换
        perl -i -p0e 's/(\$\(run-program-env\)\s*\\\s*\n\s*)(\$\(\$\*-ENV\))/$1\$(CUSTOM_ENV) $2/g' "$makefile"

        if grep -q 'CUSTOM_ENV' "$makefile"; then
            log_info "Makefile 修改成功 (备用方案)"
        else
            log_error "Makefile 修改失败，请手动修改"
            log_error "文件位置: $makefile"
            log_error "请将 '\$(\$*-ENV)' 前添加 '\$(CUSTOM_ENV) '"
            exit 1
        fi
    fi
}

# 执行完整初始化流程
do_init() {
    log_info "开始初始化 glibc 环境..."

    check_dependencies

    step1_download
    step2_extract
    step3_prepare_build
    step4_configure
    step5_build
    step6_patch_makefile

    log_info "初始化完成!"
}

#==============================================================================
# 基准测试流程函数 (步骤 7-10)
#==============================================================================

# 步骤7: 测试自定义malloc
step7_bench_custom() {
    local preload_path="$1"

    log_info "步骤7: 测试自定义 malloc (LD_PRELOAD=$preload_path)..."

    # 转换为绝对路径
    if [[ "$preload_path" != /* ]]; then
        preload_path="$(pwd)/$preload_path"
    fi

    if [ ! -f "$preload_path" ]; then
        log_error "库文件不存在: $preload_path"
        exit 1
    fi

    pushd "$GLIBC_BUILD"
    make bench BENCHSET="$BENCHSET" CUSTOM_ENV="LD_PRELOAD=$preload_path"
    popd

    log_info "自定义 malloc 测试完成"
}

# 步骤8: 重命名输出文件
step8_rename_outputs() {
    log_info "步骤8: 重命名输出文件..."

    local benchtest_dir="$GLIBC_BUILD/benchtests"

    if [ ! -d "$benchtest_dir" ]; then
        log_error "benchtests 目录不存在: $benchtest_dir"
        exit 1
    fi

    pushd "$benchtest_dir"

    local count=0
    # 使用 find 查找所有符合格式的文件并重命名
    while IFS= read -r -d '' file; do
        local basename=$(basename "$file")
        local newname="${basename/bench-malloc-/bench-my_malloc-}"
        mv "$file" "$newname"
        log_info "  $basename -> $newname"
        count=$((count + 1))
    done < <(find . -maxdepth 1 -type f \( -name 'bench-malloc-simple-*.out' -o -name 'bench-malloc-tcache-*.out' -o -name 'bench-malloc-thread-*.out' \) -print0)

    if [ $count -eq 0 ]; then
        log_warn "没有找到需要重命名的文件"
    else
        log_info "共重命名 $count 个文件"
    fi
    popd
}

# 步骤9: 测试glibc malloc
step9_bench_glibc() {
    log_info "步骤9: 测试 glibc malloc..."

    pushd "$GLIBC_BUILD"
    make bench BENCHSET="$BENCHSET"
    popd
    log_info "glibc malloc 测试完成"
}

# 步骤10: 对比并计算得分
step10_compare() {
    log_info "步骤10: 对比性能指标..."

    local benchtest_dir="$GLIBC_BUILD/benchtests"
    local score_script="$PWD/scripts/score_benchmark.py"

    # 检查评分脚本是否存在
    if [ ! -f "$score_script" ]; then
        log_error "评分脚本不存在: $score_script"
        exit 1
    fi

    # 检查 Python3
    if ! command -v python3 &> /dev/null; then
        log_error "python3 未安装，无法运行评分脚本"
        exit 1
    fi

    echo ""

    # 运行评分脚本
    python3 "$score_script" --benchdir "$benchtest_dir"

    echo ""
    echo "详细结果文件位于: $benchtest_dir"
    echo ""
}

# 执行基准测试流程
do_benchmark() {
    local preload_path="$1"

    log_info "开始基准测试..."

    # 检查glibc环境是否已初始化
    if [ ! -d "$GLIBC_SRC" ]; then
        log_error "glibc 源码目录不存在: $GLIBC_SRC"
        log_error "请先运行 '$0 --init' 初始化环境"
        exit 1
    fi

    if [ ! -d "$GLIBC_BUILD" ]; then
        log_error "glibc 构建目录不存在: $GLIBC_BUILD"
        log_error "请先运行 '$0 --init' 初始化环境"
        exit 1
    fi

    step7_bench_custom "$preload_path"
    step8_rename_outputs
    step9_bench_glibc
    step10_compare

    log_info "基准测试完成!"
}

#==============================================================================
# 参数解析
#==============================================================================

parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -i|--init)
                SHOULD_INIT=1
                shift
                ;;
            -p|--preload)
                if [[ -z "$2" || "$2" == -* ]]; then
                    log_error "--preload 需要指定库路径"
                    exit 1
                fi
                PRELOAD_PATH="$2"
                shift 2
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                log_error "未知参数: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

#==============================================================================
# 主函数
#==============================================================================

main() {
    parse_args "$@"

    # 如果没有指定任何参数，显示帮助
    if [[ $SHOULD_INIT -eq 0 && -z "$PRELOAD_PATH" ]]; then
        show_help
        exit 0
    fi

    # 执行初始化
    if [[ $SHOULD_INIT -eq 1 ]]; then
        do_init
    fi

    # 执行基准测试
    if [[ -n "$PRELOAD_PATH" ]]; then
        do_benchmark "$PRELOAD_PATH"
    fi
}

main "$@"
