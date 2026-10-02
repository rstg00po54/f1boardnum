#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
TOOLCHAIN_FILE="$ROOT_DIR/cmake/gcc-arm-none-eabi.cmake"
ELF_FILE="$BUILD_DIR/f1board.elf"

MODE="${1:-all}"
JOBS="${JOBS:-$(nproc)}"
SWD_SPEED="${SWD_SPEED:-1000}"

# Prefer an ARM GCC already in PATH. If it is not there, also support the
# toolchain location used on the current Ubuntu machine.
if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    for candidate in \
        "/home/cc/disk/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi/bin" \
        "$HOME/disk/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi/bin"
    do
        if [[ -x "$candidate/arm-none-eabi-gcc" ]]; then
            export PATH="$candidate:$PATH"
            break
        fi
    done
fi

log()
{
    echo
    echo "============================================================"
    echo "$1"
    echo "============================================================"
}

check_tools()
{
    local tools=(
        cmake
        arm-none-eabi-gcc
        openocd
    )

    for tool in "${tools[@]}"; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            echo "错误：找不到命令：$tool"
            exit 1
        fi
    done

    echo "ARM GCC : $(command -v arm-none-eabi-gcc)"
    echo "OpenOCD : $(command -v openocd)"
}

configure()
{
    # 旧 build 可能缓存了已经不存在的 CMake / 编译器绝对路径。
    if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
        local cached_cmake
        local cached_cc
        cached_cmake="$(sed -n 's#^CMAKE_COMMAND:INTERNAL=##p' "$BUILD_DIR/CMakeCache.txt" | head -n1)"
        cached_cc="$(sed -n 's#^CMAKE_C_COMPILER:FILEPATH=##p; s#^CMAKE_C_COMPILER:STRING=##p' "$BUILD_DIR/CMakeCache.txt" | head -n1)"

        if [[ -n "$cached_cmake" && ! -x "$cached_cmake" ]]; then
            log "检测到旧 CMake 路径已失效，删除 build"
            rm -rf "$BUILD_DIR"
        elif [[ -n "$cached_cc" && ! -x "$cached_cc" ]]; then
            log "检测到旧 ARM GCC 路径已失效，删除 build"
            rm -rf "$BUILD_DIR"
        elif ! grep -q "arm-none-eabi-gcc" "$BUILD_DIR/CMakeCache.txt"; then
            log "检测到旧构建目录不是 ARM 编译器，删除 build"
            rm -rf "$BUILD_DIR"
        fi
    fi

    if [[ ! -f "$TOOLCHAIN_FILE" ]]; then
        echo "错误：找不到工具链文件："
        echo "$TOOLCHAIN_FILE"
        exit 1
    fi

    if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
        log "配置 CMake"

        cmake \
            -S "$ROOT_DIR" \
            -B "$BUILD_DIR" \
            -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    fi
}

build()
{
    configure

    log "编译 STM32F103，线程数：$JOBS"

    cmake --build "$BUILD_DIR" --parallel "$JOBS"

    if [[ ! -f "$ELF_FILE" ]]; then
        echo "错误：编译结束，但没有生成："
        echo "$ELF_FILE"
        exit 1
    fi

    echo
    arm-none-eabi-size "$ELF_FILE"
    echo
    file "$ELF_FILE"

    echo
    echo "编译完成：$ELF_FILE"
}

flash()
{
    if [[ ! -f "$ELF_FILE" ]]; then
        echo "错误：找不到固件：$ELF_FILE"
        echo "请先执行：$0 build"
        exit 1
    fi

    log "使用 DAPLink 烧录，SWD 速度：${SWD_SPEED} kHz"

    openocd \
        -f interface/cmsis-dap.cfg \
        -c "transport select swd" \
        -f target/stm32f1x.cfg \
        -c "adapter_khz $SWD_SPEED" \
        -c "program {$ELF_FILE} verify reset exit"

    echo
    echo "烧录完成"
}

clean()
{
    log "清理构建目录"
    rm -rf "$BUILD_DIR"
    echo "已删除：$BUILD_DIR"
}

check_tools

case "$MODE" in
    all)
        build
        flash
        ;;

    build)
        build
        ;;

    flash)
        flash
        ;;

    clean)
        clean
        ;;

    rebuild)
        clean
        build
        ;;

    *)
        echo "用法："
        echo "  $0           编译并烧录"
        echo "  $0 all       编译并烧录"
        echo "  $0 build     只编译"
        echo "  $0 flash     只烧录"
        echo "  $0 clean     清理"
        echo "  $0 rebuild   清理后重新编译"
        exit 1
        ;;
esac
