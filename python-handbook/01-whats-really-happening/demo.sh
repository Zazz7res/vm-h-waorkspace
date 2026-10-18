#!/usr/bin/env bash
# ============================================================================
# 第 01 章《解释器到底做了什么》—— 整章证据一键复现
#
#   bash demo.sh                 跑完自动删沙箱（默认）
#   bash demo.sh --keep          跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt 2>&1  重定向时自动关颜色，得到和本章 README 一样的纯文本
#
# 这个脚本按 README 的小节顺序，把每一段的真实终端输出打出来。
# 屏幕上出现的每一行，都是这条命令在这台机器上的真实结果 —— 没有一行是手写的。
#
# 安全说明：
#   * 示例源码会被整份复制进 mktemp 出来的沙箱再运行，所以本手册目录里**不会**
#     留下 __pycache__/、*.pyc、venv 之类的垃圾（第 10 节当场用 find 证明这件事）。
#   * 只用标准库；不 pip install、不联网、不改 ~/.bashrc / ~/.profile / ~/.config。
#   * 故意不加 `set -e`：本章一半内容是「失败现场」（ModuleNotFoundError、缩进错误、
#     venv 建不起来），必须让报错原样打出来并且继续往下跑。
#   * 也故意不加 `set -o pipefail`：加了它之后 `cmd | head -3` 会因为 head 提前关掉
#     管道而报 141，屏幕上会冒出一堆和教学无关的退出码。
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：$arg（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

set -u

# 固定语言。Python 自己的报错恒为英文，但 `command not found` 这类消息是 bash 翻译的；
# 设成 C.UTF-8 后整份 transcript 才能逐字比对（你手敲时看到中文完全正常）。
export LC_ALL=C.UTF-8
export LANG=C.UTF-8
# 清掉可能干扰结论的环境变量（只影响本脚本的这个进程和它的子进程，不碰你的配置文件）。
unset PYTHONPATH PYTHONSTARTUP PYTHONOPTIMIZE PYTHONUNBUFFERED PYTHONDONTWRITEBYTECODE

# 只有输出到终端才上色；重定向到文件时是干净纯文本
if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/py01-XXXXXX")
export SB                                   # 让 runc 里起的子 bash 也能拿到沙箱路径

banner() { printf '\n%s══════ %s %s\n' "$B" "$1" "══════════════════════════════════$R"; }
note()   { printf '%s\n' "$1"; }

# 打印命令 → 执行（stderr 合并进来）→ 打印退出码
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1
    printf '[退出码 %d]\n' "$?"
}

# 计时类命令的数字每次都会抖：先原样打一遍（这是真实输出），
# 再打一行 [形状]（把所有数字换成 N）。README 只引用形状行，所以你重跑一定能对上。
runc_norm() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    raw=$(bash -c "$1" 2>&1)
    code=$?
    printf '%s\n' "$raw"
    printf '[退出码 %d]\n' "$code"
    printf '%s\n' "$raw" | sed -E 's/[0-9]+([.][0-9]+)?/N/g' | sed 's/^/[形状] /'
}

cleanup() {
    rm -rf /tmp/py01-venv-probe /tmp/py01-first.pyc      # 沙箱之外创建的东西也一并清掉
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去接着敲： cd "%s/work"\n  看完自己删： rm -rf "%s"\n' \
            "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$SB"
    fi
}
trap cleanup EXIT

# ============================================================================
banner "0. 环境体检：这台机器上的 python3 是谁"
# ============================================================================
# 把示例源码的 mtime 固定下来 —— 时间戳会被写进 __pycache__ 里 pyc 的头部，
# 不固定的话每次跑出来的头部字节都不一样。
# 注意 `-d` 后面必须带时区偏移：`touch -d '2026-01-01 10:00:00'` 会被当成 UTC。
mkdir -p "$SB/work"
cp -R "$HERE"/. "$SB/work"/
find "$SB/work" -name '*.py' -exec touch -d '2026-01-01T10:00:00+08:00' {} +
cd "$SB/work" || exit 1

runc 'python3 --version'
runc 'which python3'
runc 'type -a python3'
runc 'ls -l /usr/bin/python3'
runc 'python --version'
note '↑ 上面那条报错是 bash 给的，不是你 Python 装坏了：本机根本没有 python 这个命令。'
runc 'echo $PATH'
runc 'python3 -c "import sys; print(sys.executable, sys.prefix, sys.base_prefix)"'
runc 'python3 -c "import sys; print(sys.version)"'
runc 'python3 -c "import sys; print(len(sys.path), sys.path[0])"'

# ============================================================================
banner "1. 场景 1：python3 -c 一行式 vs 脚本文件，sys.argv 从哪来"
# ============================================================================
runc 'python3 -c "import sys; print(sys.argv)"'
runc 'python3 -c "import sys; print(sys.argv)" 甲 乙'
runc 'python3 -c "import sys; print(sys.argv)" -- 甲 乙'
note '↑ 第二条里 `--` 不是「参数分隔惯例」，它就是被当成一个普通参数收进 argv 的。'
runc 'python3 a_argv_and_main.py 甲 乙'
runc 'python3 a_argv_and_main.py'
runc 'python3 -m a_argv_and_main 甲 乙'
note '↑ 对比 -m 和脚本方式下 sys.argv[0] 和 __file__ 的取值差别。'
runc 'python3 < a_argv_and_main.py'
note '↑ 最后一条是从标准输入喂脚本：看 sys.argv[0] 变成什么、__file__ 还在不在。'
runc "printf 'import sys\nprint(sys.argv)\n' | python3"
note '退出码这件事顺手验证一下（07 章会正式讲，但这里先对上号）：'
runc 'python3 -c "import sys; sys.exit(3)"; echo "shell 看到的 $? = $?"'
runc 'python3 -c "raise RuntimeError(1)" 2>/dev/null; echo "shell 看到的 $? = $?"'
runc 'python3 -c "import sys; sys.exit()"; echo "shell 看到的 $? = $?"'

# ============================================================================
banner "2. 场景 2：字节码长什么样，__pycache__ 里到底存了什么"
# ============================================================================
runc 'python3 b_bytecode.py'
note ''
note '--- 2.1 让 import 真的产生一个 pyc，然后看目录 ---'
note '# 先破一个常见误解：marshal 没有 __main__，`python3 -m marshal` 是跑不起来的 ——'
note '# 所以下面只能按 PEP 552 的格式手工解析 pyc。'
runc 'python3 -m marshal'
runc 'rm -rf __pycache__ && find . -maxdepth 1 -name "*.py" | sort'
runc 'printf "import demo_module\n" > import_once.py && touch -d "2026-01-01T10:00:00+08:00" import_once.py && python3 import_once.py'
runc 'ls __pycache__'
runc 'for f in __pycache__/*.pyc; do printf "%s  %s 字节\n" "$f" "$(wc -c < "$f")"; done'
runc 'python3 c_pyc_inspect.py __pycache__/demo_module.cpython-314.pyc'
note ''
note '--- 2.2 再 import 一次：pyc 不会被重写（时间戳对得上就直接拿来用）---'
runc 'cp __pycache__/demo_module.cpython-314.pyc /tmp/py01-first.pyc && python3 import_once.py && cmp -s __pycache__/demo_module.cpython-314.pyc /tmp/py01-first.pyc && echo "两次跑完 pyc 逐字节相同 → 第二次没有重新编译"'
note ''
note '--- 2.3 改了源码，pyc 什么时候作废？---'
runc "printf 'print(\"改过的版本\")\n' >> demo_module.py && touch -d '2026-01-02T10:00:00+08:00' demo_module.py && python3 import_once.py"
runc 'for f in __pycache__/*.pyc; do printf "%s  %s 字节\n" "$f" "$(wc -c < "$f")"; done'
note '↑ 源码变新 → 解释器重编译并覆盖 pyc。判断依据就是 pyc 头部那 8 个字节。'
note ''
note '--- 2.4 用 -m dis 直接反汇编一个源文件（不用写一行代码）---'
runc 'python3 -m dis demo_module.py | head -16'
note ''
note '--- 2.5 另一种失效法：hash-based pyc（compileall 的 -f + --invalidation-mode）---'
mkdir -p "$SB/hashdemo"
runc 'cd "$SB/hashdemo" && printf "VALUE = 1\n" > hmod.py && touch -d "2026-01-01T10:00:00+08:00" hmod.py && python3 -m compileall -q -f --invalidation-mode checked-hash hmod.py && ls __pycache__'
runc 'cd "$SB/hashdemo" && python3 "$SB/work/c_pyc_inspect.py" __pycache__/hmod.cpython-314.pyc'
runc 'cd "$SB/hashdemo" && touch -d "2033-12-24T21:00:00+08:00" hmod.py && python3 -c "import hmod; print(\"mtime 被改到 2033 年，import 照样成功：VALUE =\", hmod.VALUE)"'
runc 'cd "$SB/hashdemo" && python3 "$SB/work/c_pyc_inspect.py" __pycache__/hmod.cpython-314.pyc | grep -E "flags|头部第 9~16|现算"'
runc 'cd "$SB/hashdemo" && printf "VALUE = 2\n" > hmod.py && python3 -c "import hmod; print(\"内容改了 1 个字节 → VALUE =\", hmod.VALUE)" && python3 "$SB/work/c_pyc_inspect.py" __pycache__/hmod.cpython-314.pyc | grep -E "flags|头部第 9~16|现算"'
runc 'cd "$SB/hashdemo" && rm -rf __pycache__ && python3 -m compileall -q -f --invalidation-mode unchecked-hash hmod.py && python3 "$SB/work/c_pyc_inspect.py" __pycache__/hmod.cpython-314.pyc | grep -E "^flags|头部第 9~16"'
note '↑ flags=3（checked-hash）：每次 import 都现算源码哈希；头部那 8 字节就是 `_imp.source_hash` 的结果，'
note '  和现算值完全相同；改 mtime 不生效，改内容才生效。'
note '↑ flags=1（unchecked-hash）：解释器连源码都不读 —— wheel 发行版爱用这个模式，'
note '  代价是你手改 site-packages 里的 .py 它毫无反应。'

# ============================================================================
banner "3. 场景 3：sys.path 怎么拼出来的，当前目录什么时候算数"
# ============================================================================
runc 'python3 d_path_probe.py'
note ''
note '--- 3.1 四种启动方式，sys.path[0] 各不相同（本章最容易踩的地雷）---'
runc 'python3 -c "import sys; print(\"-c    :\", repr(sys.path[0]), repr(sys.argv[0]))"'
runc 'python3 path0.py'
runc 'python3 -m path0'
runc 'python3 < path0.py'
note '# 同一个脚本，换到上级目录再用相对路径跑 —— 结果完全不受影响：'
runc 'cd "$SB" && python3 work/path0.py'
note '# 但 -c 换到上级目录，找模块的地方就跟着变了：'
runc 'cd "$SB" && python3 -c "import sys; print(\"-c 在上级目录:\", repr(sys.path[0]))"'
runc 'cd "$SB" && python3 -m path0'
note '↑ -m 那条会失败：-m 是按**当前目录**找模块的，上级目录里没有 path0 这个模块。'
note ''
note '--- 3.2 PYTHONPATH 插在哪，能不能压过标准库 ---'
runc 'PYTHONPATH=/tmp:/var python3 -c "import sys; print(sys.path)"'
runc 'PYTHONPATH=/usr/lib/python3.14:/tmp python3 -c "import os, sys; print(os.__file__)"'
note '↑ PYTHONPATH 排在标准库**前面**，同名的 .py 能把标准库顶掉 —— 这是它的用途，也是它的危险。'
runc 'PYTHONSTARTUP=/tmp/none.py python3 -c "import sys; print(sys.flags.ignore_environment)"'
note ''
note '--- 3.3 真实踩坑现场：同一个脚本，换个目录跑 ---'
runc 'cd e_import_trap && rm -rf __pycache__ && python3 main.py'
runc 'python3 e_import_trap/main.py'
runc 'cd "$SB" && python3 -c "import helper"'
runc 'cd "$SB/work/e_import_trap" && python3 -c "import random; print(random.__file__); print(random.roll())"'
runc 'cd "$SB/work/e_import_trap" && python3 -c "import random; random.randint(1, 6)"'
note '↑ 最后这条的报错里，解释器直接劝你把那个文件改名 —— 3.14 的报错质量值得专门看一眼。'

# ============================================================================
banner "4. 场景 4：python3 / python / sys.executable / sys.prefix 对照"
# ============================================================================
runc 'command -v python python3 python3.14 py'
runc 'ls -l /usr/bin/python3 /usr/bin/python3.14'
runc 'python3 -c "import sys, os; print(sys.executable); print(os.path.realpath(sys.executable))"'
runc 'python3 -c "import sys; print(sys.prefix, sys.base_prefix, sys.exec_prefix)"'
runc 'python3 f_where_python.py'
note ''
note '--- 4.1 本机想建 venv 的真实下场（09 章的地基，这里先立此存照）---'
runc 'mkdir -p /tmp/py01-venv-probe && python3 -m venv /tmp/py01-venv-probe/v'
runc 'python3 -m venv --without-pip /tmp/py01-venv-probe/v2 && ls /tmp/py01-venv-probe/v2/bin'
runc '/tmp/py01-venv-probe/v2/bin/python3 -c "import sys; print(sys.prefix); print(sys.base_prefix); print(len(sys.path))"'
runc 'rm -rf /tmp/py01-venv-probe && echo "临时 venv 已删除"'
note '↑ 结论：本机 venv 能建出目录结构，但**没有 pip**（发行版缺 ensurepip）。'
note '  本章不安装任何东西；09 章会按这个真实情况讲 venv 为什么必须存在。'

# ============================================================================
banner "5. 场景 5：stdout 缓冲 —— 管道和终端为什么不一样"
# ============================================================================
runc 'python3 g_buffer_probe.py'
note ''
note '--- 5.1 shell 侧的同一件事：把 python3 接到管道上 ---'
runc 'python3 -c "import sys; print(sys.stdout.isatty(), sys.stdout.line_buffering)"'
runc 'python3 -c "import sys; print(sys.stdout.isatty(), sys.stdout.line_buffering)" | cat'
runc 'python3 -u -c "import sys; print(sys.stdout.isatty(), sys.stdout.line_buffering, sys.stdout.write_through)" | cat'
runc 'python3 -c "import sys; sys.stdout.reconfigure(line_buffering=True); print(sys.stdout.line_buffering)" | cat'
note '↑ 最后一条证明：代码里 reconfigure 一下，管道里也能变回行缓冲。'

# ============================================================================
banner "6. 场景 6：交互式解释器和脚本的差别（>>> / _ / 表达式回显）"
# ============================================================================
runc 'python3 h_repl_probe.py'

# ============================================================================
banner "7. 场景 7：报错原文 —— 新手最该练的是怎么读它"
# ============================================================================
runc 'python3 i_errors/name_error.py'
runc 'python3 i_errors/module_not_found.py 1'
runc 'python3 i_errors/module_not_found.py 2'
runc 'python3 i_errors/module_not_found.py 3'
runc 'python3 i_errors/indent_error.py'
runc 'python3 i_errors/tab_error.py'
runc 'python3 i_errors/syntax_error.py'
runc 'cat -A i_errors/tab_error.py | sed -n "17,21p"'
note '↑ ^I 就是制表符：这个文件的缩进真的混用了 tab 和空格，不是注释里说说而已。'
runc 'python3 i_errors/deep_traceback.py'

# ============================================================================
banner "8. 场景 8：PYTHONPATH / -O / -OO / -B 各留下什么证据"
# ============================================================================
note '--- 8.1 默认：被 import 的模块会编译并写进 __pycache__ ---'
runc 'rm -rf __pycache__ && python3 opt_run.py && ls __pycache__'
note '↑ __pycache__ 里只有 opt_target，**没有 opt_run.py**：'
note '  被当 __main__ 直接跑的那个脚本永远不写 pyc（写了也没人按这个文件名去找它）。'
note '--- 8.2 -B：什么都不写 ---'
runc 'rm -rf __pycache__ && python3 -B opt_run.py >/dev/null && (ls __pycache__ 2>/dev/null || echo "(__pycache__ 根本没被创建)")'
runc 'PYTHONDONTWRITEBYTECODE=1 python3 -c "import sys; print(sys.dont_write_bytecode)"'
note '--- 8.3 -O / -OO：assert 和文档字符串的命运，以及 pyc 文件名的变化 ---'
runc 'rm -rf __pycache__ && python3 -O opt_run.py && ls __pycache__'
runc 'rm -rf __pycache__ && python3 -OO opt_run.py && ls __pycache__'
runc 'python3 -c "import sys; print(sys.flags.optimize, sys.flags.dont_write_bytecode, sys.flags.verbose)"'
runc 'python3 -O -c "assert False, \"这条在 -O 下不执行\"; print(\"跑到这里说明 assert 真的没生成字节码\")"'
runc 'python3 -c "assert False, \"默认级别下这条会炸\""'
note '--- 8.4 不同优化级别的 pyc 互不覆盖 ---'
runc 'rm -rf __pycache__ && python3 -O opt_run.py >/dev/null && ls __pycache__ && python3 opt_run.py >/dev/null && ls __pycache__'
note '↑ opt-1 和不带后缀的是两个文件，各归各的级别用 —— 文件名里那点信息量就是干这个的。'
note '--- 8.5 其他会留下可见痕迹的开关 ---'
runc 'python3 -S -c "import sys; print(len(sys.path), sys.path)"'
runc 'python3 -S -c "exit()"'
runc 'python3 -E -c "import sys; print(sys.flags.ignore_environment)"'
runc 'PYTHONPATH=/tmp python3 -E -c "import sys; print(\"-E 让 PYTHONPATH 被无视:\", sys.path)"'
runc 'python3 -P -c "import sys; print(sys.path[0], sys.flags.safe_path)"'
runc 'python3 -W error -c "import warnings; warnings.warn(\"只是一个警告\"); print(\"这行不会打印\")"'
runc 'python3 -X importtime -c "import json" 2>&1 | tail -4'
runc 'rm -rf __pycache__ && python3 -m py_compile demo_module.py && ls __pycache__'
note '↑ py_compile 是「只编译不运行」：它照样往 __pycache__ 写 pyc，但一行 print 都没执行。'

# ============================================================================
banner "9. 场景 9：python3 -m timeit 真实跑一组小数据"
# ============================================================================
note '# 每段先打真实输出（数字每次都会抖），再打一行 [形状]：把数字全换成 N，'
note '# 形状行是稳定可复现的，README 引用的就是它。'
runc_norm 'python3 -m timeit -s "d = {i: i for i in range(1000)}" "500 in d"'
runc_norm 'python3 -m timeit -s "s = {i for i in range(1000)}" "500 in s"'
runc_norm 'python3 -m timeit -s "l = list(range(1000))" "500 in l"'
note '↑ dict / set 的存在性检查走哈希，list 要逐个比 → 数据一大就是数量级差别（04 章展开）。'
runc_norm 'python3 -m timeit -n 100000 -r 3 -s "l = []" "l.append(1)"'
runc_norm "python3 -m timeit -s \"
def build():
    a = []
    for i in range(1000):
        a.append(i)
    return a
\" \"build()\""
note '↑ -n 每轮跑几次、-r 跑几轮取最好值、-s 是只执行一次的准备工作（setup）。不写就是自动选次数。'
runc 'python3 -m timeit -h | head -12'
note ''
note '--- 9.1 timeit 之外，还有一个当场看耗时的办法 ---'
runc "python3 -c \"
import time
t0 = time.perf_counter()
total = sum(range(1000))
dt = time.perf_counter() - t0
print(f'sum(range(1000)) 结果 {total}')
print(f'耗时 {dt:.9f} 秒（这个数字每次都不同）')
\""

# ============================================================================
banner "10. 收尾：证明这一章没在手册目录里留下垃圾"
# ============================================================================
runc "find \"$HERE\" \\( -name '__pycache__' -o -name '*.pyc' \\) -print | sed 's|^|手册目录里发现: |'"
note '↑ 上面这一条必须一行输出都没有 —— 所有 pyc 都生在沙箱里。'
runc "find . \\( -name '*.pyc' -o -name '__pycache__' \\) | wc -l"
note "沙箱位置：$SB（默认跑完就删；想留着继续敲就用 bash demo.sh --keep）"

printf '\n%s[全部跑完] 上面每一段都是真实执行结果；README.md 引用的输出全部出自这一次运行。%s\n' "$B" "$R"
exit 0
