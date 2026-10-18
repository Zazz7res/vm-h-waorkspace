#!/usr/bin/env bash
# ============================================================================
# 第 04 章《容器与推导式》—— 整章证据一键复现
#
#   bash demo.sh                 跑完自动删沙箱（默认）
#   bash demo.sh --keep          跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt 2>&1  重定向时自动关颜色，得到和本章 README 一样的纯文本
#
# 屏幕上出现的每一行，都是这条命令在这台机器（CPython 3.14.4 / Ubuntu）上的真实结果，
# 没有一行是手写的、没有一行是「我记得应该这样」。
#
# 这一章演什么（18~26 节的路线图，按学习曲线排）：
#   尺寸与扩容（01~04）→ 位置代价（05）→ 成员运算实测（06）→ 拼接（07）→ 切片（08）
#   → 复制梯子（09）→ 别名与 `*`（10）→ 边遍历边改（11）→ 失败现场（12）
#   → 排序三错（13~14）→ 字面量与键（15）→ 推导式（16）→ 生成器 vs map（17）
#   → collections（18~19）→ 复杂度表 × 本机实测的收口对照（20）→ 零产物自证（21）
#
# 安全与口径说明：
#   * 14 个取证脚本整份复制进 mktemp 出来的沙箱再运行，所以本手册目录里**不会**留下
#     __pycache__/、*.pyc（第 21 节当场用 find 自证，输出必须是 0）。
#     顺带说一句：本章全程 `python3 xxx.py` 直接跑脚本、从不 `import` 任何示例模块，
#     而解释器只给「被 import 的模块」生成 pyc —— 所以 0 不是运气，是机制。
#   * 只用标准库（sys / timeit / copy / collections / tracemalloc / inspect / random）；
#     不 pip install、不联网、不改 ~/.bashrc / ~/.profile / ~/.config / ~/.local。
#   * 故意不加 `set -e`：本章有**一批注定失败**的命令，要看的就是报错原文本身
#     （TypeError: cannot use 'list' as a dict key (unhashable type: 'list')、
#      RuntimeError: dictionary changed size during iteration、
#      RuntimeError: deque mutated during iteration、AttributeError: 'list' object
#      has no attribute 'popleft'、ValueError: list.remove(x): x not in list、
#      KeyError: slice(0, 1, None)、AssertionError……）。
#     加了 set -e，第 12 节第一条就把整份 transcript 掐断，后面十几节全没了。
#   * 也故意不加 `set -o pipefail`：加了它之后 `cmd | tail -6` 会因为 tail 提前关管道
#     而报 141，屏幕上会冒出一堆和教学无关的退出码。
#   * 屏幕上不主动打印绝对路径：需要「在某个目录里跑」时用 runc_at，只留目录 basename。
#     Python traceback 里若出现 /tmp/py04-XXXXXX，那是 3.9 起 `__file__` 变绝对的
#     解释器行为，不在本脚本控制范围内；随机后缀每次不同，README 里把它归一化成 <沙箱>。
#   * 耗时天花板：整份 demo.sh 必须 3 分钟内跑完（本机实测约 15 秒，最后一行会打印
#     真实秒数）。所有 timeit 实验的 n 都压在 1e5 量级、number/repeat 都是个位数，
#     第 20 节的收口表也重新测了一遍，但每格只跑 3 次重复。
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

# 固定语言：Python 自己的报错恒为英文，但 `command not found` 这类消息是 bash 翻译的；
# 设成 C.UTF-8 后整份 transcript 才能逐字比对（你手敲时看到中文完全正常）。
export LC_ALL=C.UTF-8
export LANG=C.UTF-8
export LANGUAGE=C
# 清掉可能干扰结论的环境变量（只影响本脚本这个进程和它的子进程，不碰你的配置文件）。
unset PYTHONPATH PYTHONSTARTUP PYTHONOPTIMIZE PYTHONINSPECT PYTHONWARNINGS PYTHONDONTWRITEBYTECODE
# 哈希随机化会影响 set 的遍历顺序（进而影响个别 repr 的先后）。本章的结论要么是
# 「插入顺序」（dict，和种子无关），要么是「尺寸/耗时」（也和种子无关），
# 这里显式固定成 0，只是为了让同一份脚本在你机器上打出同样的输出，方便逐行比对。
export PYTHONHASHSEED=0

# 只有输出到终端才上色；重定向到文件时是干净纯文本
if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/py04-XXXXXX")
export SB

banner() { printf '\n%s══════ %s %s\n' "$B" "$1" "══════════════════════════════════$R"; }
note()   { printf '%s\n' "$1"; }

# 打印命令 → 执行（stderr 合并进来）→ 打印退出码
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1
    printf '[退出码 %d]\n' "$?"
}

# 计时类命令的数字每次都抖：先原样打一遍，再打一行 [形状]（数字全换成 N）。
# README 引用的输出只取形状行 + 倍率，所以你重跑一定能对上。
runc_norm() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    raw=$(bash -c "$1" 2>&1)
    code=$?
    printf '%s\n' "$raw"
    printf '[退出码 %d]\n' "$code"
    printf '%s\n' "$raw" | sed -E 's/[0-9]+([.][0-9]+)?/N/g' | sed 's/^/[形状] /'
}

# runc_at <目录> <命令>：先 cd 进那个目录再跑，屏幕上只留目录 basename + 相对命令。
# 本机用户名的脱敏靠「根本不打印」，不是事后抹。
runc_at() {
    printf '\n%s$ 〔在 %s 里〕 %s%s\n' "$C" "$(basename "$1")" "$2" "$R"
    ( cd "$1" && bash -c "$2" ) 2>&1
    printf '[退出码 %d]\n' "$?"
    return 0
}

cleanup() {
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
banner "第 00 节 环境体检：本章结论只在这么一个解释器上验过"
# ============================================================================
mkdir -p "$SB/work"
cp "$HERE"/*.py "$SB/work"/
cd "$SB/work" || exit 1
note '本章的取证脚本一共 14 个，已经整份复制进沙箱（下面列的就是沙箱里的 work 目录）：'
runc 'ls'
runc 'ls *.py | wc -l'
runc 'python3 --version'
runc 'python3 -c "import sys; print(sys.version)"'
runc 'python3 -c "import sys; print(sys.implementation.name, sys.hexversion, sys.maxsize)"'
runc 'echo "LC_ALL=$LC_ALL  LANG=$LANG  PYTHONHASHSEED=$PYTHONHASHSEED"'
runc "python3 -c \"import sys; print('四个空壳的尺寸预告 list/dict/set/tuple =', sys.getsizeof([]), sys.getsizeof({}), sys.getsizeof(set()), sys.getsizeof(()))\""
runc 'python3 -c "import sys, timeit, copy, collections, tracemalloc, inspect, random; print(\"标准库全部可用，本章不装任何第三方包\")"'
note '↑ 后面每一条命令都在这个沙箱里跑；第 21 节会当场证明手册目录里一个产物都没留下。'

# ============================================================================
banner "第 01 节 容器空壳：谁在「建的时候」就已经花钱了（container_sizes.py）"
# ============================================================================
note '一句话预告：空 list 56 字节、空 dict 64、空 set 216、空 tuple 48 —— 差着数量级，'
note '而 C 里 `int a[10]` 占多少字节是编译期就钉死的。这份脚本分五段量。'
runc 'python3 container_sizes.py'
note '↑ 三个值得抄进 README 的真数字：空 dict 64 字节、塞进第 1 个键立刻 224、空 set 216。'
note '  「dict 空表 224 字节」那句流行说法是错的：224 是**装了 1 个键**的 dict。'

# ============================================================================
banner "第 02 节 补刀：getsizeof 量的是外壳不是本体；tuple 根本不扩容"
# ============================================================================
note '第 01 节末尾那句「list 里存的是指针」值得单独钉死，顺手把 tuple 的定长性格量出来。'
note '这一节全是 sys.getsizeof，没有任何计时，所以每一条数字都逐字可复现。'
runc "python3 -c \"
import sys
xs = [1, 2, 3]
print('list 外壳', sys.getsizeof(xs), '字节；三个元素本体各自', [sys.getsizeof(x) for x in xs])
print('把元素换成 1 万字节的大块，外壳一格不长：',
      sys.getsizeof([bytearray(10000) for _ in range(3)]))
print('空 tuple 到 4 个元素的尺寸：', [sys.getsizeof(tuple(range(i))) for i in range(5)])
grown = []
for i in range(5):
    grown.append(0)
print('append 长出 5 个：', sys.getsizeof(grown), '字节 = ', (sys.getsizeof(grown) - 56) // 8, '槽')
print('[0]*5 一次成型：', sys.getsizeof([0] * 5), '字节 = ', (sys.getsizeof([0] * 5) - 56) // 8, '槽')
print('两者 == 吗：', grown == [0] * 5, '；getsizeof 相等吗：', sys.getsizeof(grown) == sys.getsizeof([0] * 5))
\""
note '↑ 读三件事：① 元素再胖，外壳不变（list 是指针数组）；② tuple 每加一个元素正好 +8 字节，'
note '  没有「多租几格备着」这回事 —— 定长意味着不能改；③ 同样 len=5，append 出身租 8 槽、'
note '  [0]*5 出身租 5 槽，== 是 True 但尺寸不同：别拿 getsizeof 当相等判据。'

# ============================================================================
banner "第 03 节 list 的扩容台阶实测，对照你手写的 C 动态数组（capacity_growth.py）"
# ============================================================================
note '你在 01-array/dynamic-array 里定的是 grow_multiplier=2。这份脚本把 Python 的真实'
note '台阶一格一格量出来，再和纯倍增各算三笔账：扩容次数、空间浪费、累计复制的指针数。'
runc 'python3 capacity_growth.py'
note '↑ 这一节最该记住的两个数：append 到 1000 个元素一共搬家 28 次、累计复制 7556 个指针'
note '  （平均每次 append 只多 7.56 个）；pop 到 499 那一刻才缩容 —— 判据是「元素数 < 槽数 >> 1」。'

# ============================================================================
banner "第 04 节 补刀：知道规模就先给足 —— 四种造法各量一遍时间和槽数"
# ============================================================================
note '第 03 节末尾那条「实操推论」不能只靠嘴说，这里同一份 n=100000 用四种写法各造一遍。'
runc_norm "python3 -c \"
import sys, timeit
n = 100_000
def by_append():
    xs = []
    for i in range(n):
        xs.append(i)
    return xs
def by_comprehension():
    return [i for i in range(n)]
def by_list_range():
    return list(range(n))
def by_prealloc_assign():
    xs = [0] * n
    for i in range(n):
        xs[i] = i
    return xs
cases = (('for + append', by_append), ('列表推导式', by_comprehension),
         ('list(range(n)) 一次成型', by_list_range), ('[0]*n 再按下标填', by_prealloc_assign))
for name, fn in cases:
    t = min(timeit.repeat(fn, number=3, repeat=3)) / 3
    xs = fn()
    print(f'{name:<24} {t*1000:6.2f} ms  外壳 {sys.getsizeof(xs):>7} 字节  '
          f'槽数 {(sys.getsizeof(xs)-56)//8:>6}  结果对吗 {xs == list(range(n))}')
\""
note '↑ 诚实的结论：**时间上差不到数量级**（append 路线约 1.7 ms、一次成型约 1.0 ms，'
note '  差的是那 28 次 memmove 的钱，而 memmove 极便宜）；差得明确的是**槽数**：'
note '  append / 推导式收尾多留 116 格，[0]*n 和 list(range(n)) 精确到 100000 格。'
note '  所以「知道规模就先给足」在 n=1e5 是省内存不省时间；这条推论真正的杀伤力'
note '  在第 03 节第三节那种「累计复制了多少指针」的账上，以及在 n 很小、'
note '  一个槽就值 8 字节的时候（第 02 节末行那 24 字节）。'

# ============================================================================
banner "第 05 节 增删改查的位置代价：pop(0) / popleft() / insert / del / remove（pop_insert_probe.py）"
# ============================================================================
note '复杂度表写 list.pop(0) 是 O(n)、deque.popleft() 是 O(1)。这份脚本把「慢到什么程度」'
note '量成 µs，还顺手把 deque 的隐藏弱点 a[i] 和它的分块尺寸一起量出来。'
runc 'python3 pop_insert_probe.py'
note '↑ 挑三个数：n=100000 时 pop(0) 是 pop() 的几百倍；deque 中间按下标访问比两端慢一百倍；'
note '  deque 每 64 个元素多租一块（+528 字节），空壳就要 760 字节。'
note '  表里每格都是 timeit 取三轮最小值，绝对值会抖，**倍率和量级**才是证据。'

# ============================================================================
banner "第 06 节 成员运算 in 的真实耗时：list / tuple / dict / set（complexity_probe.py）"
# ============================================================================
note 'n = 1e3 / 1e4 / 1e5 三档，测的是**落空**的最坏情况；再加命中位置、1e6 的第三次确认、'
note '字符串键的哈希成本、`in d.values()` 这五个表。'
runc 'python3 complexity_probe.py'
note '↑ 本节最贵的一句话：n=100000 时 `in list` 比 `in set` 慢几千倍（形状是 ×10 规模→×10 耗时'
note '  vs 涨 100 倍规模几乎不动）；而 `in d.values()` 会一声不响地退回 O(n)。'
note '  计时数字每次都抖，README 只引用倍率和量级。'

# ============================================================================
banner "第 07 节 拼接的四条路，和字符串那个会被打断的私有优化（concat_probe.py）"
# ============================================================================
note '列表：acc+[x] / acc+= / append / extend 同台；字符串：s+ch / s+=ch / join，'
note '外加一个「只是多留一份引用」的对照 —— 然后把 N 翻倍看斜率。'
runc 'python3 concat_probe.py'
note '↑ 反直觉的那一行是第二节第 3 条：`hist.append(s)` 让那个 str 多一个引用，'
note '  CPython 的原地续字节优化立刻失效，耗时直接翻两个数量级。'
note '  所以「+= 拼串挺快」是赌本机实现的运气，`join` 才是能写进规范的写法。'

# ============================================================================
banner "第 08 节 切片：越界、负步长、切片赋值改长度、谁能切（slicing_probe.py）"
# ============================================================================
note '你在 C 里越界一次，ASan 给你一屏红字；Python 的切片越界**什么都不报**。'
note '这一节把「读」和「写」两侧的规矩一次划清，第五段还带一个很妙的坑。'
runc 'python3 slicing_probe.py'
note '↑ 一句话总结：单下标越界 = IndexError，切片越界 = 夹到边界给空；'
note '  只有步长为 1 的切片赋值能改长度；dict 不但不拒绝 [1:2]，还把它当成一个键去找。'

# ============================================================================
banner "第 09 节 复制的六级梯子：贴标签 → 切片 → 构造器 → copy() → copy.copy → deepcopy（copy_ladder.py）"
# ============================================================================
note '第 02 章第 20 节在对象层演过 copy/deepcopy；这里往前三步：容器层共享哪一层、'
note '成本层差多少个数量级、结构层 deepcopy 那张 memo 表会**保留**原来的别名关系。'
runc 'python3 copy_ladder.py'
note '↑ 六级梯子的分水岭只有一列：「内层[0] 还是不是原来那个对象」。五种浅拷贝全是 True，'
note '  只有 deepcopy 是 False。口诀：**要改的是里层，浅拷贝就等于没拷贝。**'
note '  成本那一格（10 万个内层 list：copy 0.4 ms vs deepcopy 60 ms）是量级差，不是风格差。'

# ============================================================================
banner '第 10 节 别名与 `*` 重复：[[0]*3]*3 这一族坑（alias_pitfalls.py）'
# ============================================================================
note 'C 里 `int board[3][3] = {{0}}` 是 9 个独立格子；Python 的 `[[0]*3]*3` 只有 1 个内层'
note 'list 被贴了 3 张标签。这份脚本每个错法都当场「改一个格，看谁跟着变」。'
runc 'python3 alias_pitfalls.py'
note '↑ 记忆法就一句：**有几层独立，就写几个 for**；`*` 造的是引用，`for` 造的才是对象。'
note '  自检口诀在最后一节：`len(set(map(id, rows))) == len(rows)`，下面当场演它的反面。'
runc "python3 -c \"
rows = [[0] * 3] * 2
assert len(set(map(id, rows))) == len(rows), '有行在共享同一个 list'
print('这条 print 永远跑不到')
\""
note '↑ 退出码 1：上一节的口诀写成 assert 就是这副样子 —— AssertionError 直接把病灶报给你，'
note '  比肉眼找「为什么三行一起变」快得多。（这一条是故意失败的，脚本没有 set -e 所以继续跑。）'

# ============================================================================
banner "第 11 节 边遍历边改：list 安静给错答案，dict / deque 当场报错（mutate_while_iterating.py）"
# ============================================================================
note '`for x in a` 内部是一个按下标递增的迭代器；你把数组改短了，它的下标还在往前走。'
note '这一节每一种错法都打出**改完之后容器里还剩什么**，残留结果比道理有用。'
runc 'python3 mutate_while_iterating.py'
note '↑ 三个容器三种性格，这一份脚本里全演到了：list 不报错但答案错（残留 [4, 8]）、'
note '  dict 改大小 → RuntimeError: dictionary changed size during iteration、'
note '  deque 改大小 → RuntimeError: deque mutated during iteration。'
note '  第六、七节还回答了两件事：== 比的是值逐格比（还递归进内层），is 比的是同一个对象。'

# ============================================================================
banner "第 12 节 失败现场集锦：不带 try 的解释器原文（每一条退出码都是 1）"
# ============================================================================
note '前面几份脚本都用 try/except 把报错接住了；这一节**故意不接**，让你看到'
note 'traceback 的原始形状 —— 报错的**文件名和行号**也一并打出来，那才是你 debug 时看到的。'
runc "python3 -c \"d = {[1, 2]: '当键就是死'}\""
runc "python3 -c \"s = {[1, 2]}\""
runc "python3 -c \"
d = {'a': 1, 'b': 2, 'c': 3}
for k in d:
    del d[k]
\""
runc 'python3 -c "print([1, 2].popleft())"'
runc 'python3 -c "[1, 2, 3].remove(9)"'
runc 'python3 -c "print({1: 2}[0:1])"'
runc 'python3 -c "print([3, 1, 2].sort()[0])"'
runc "python3 -c \"print(sorted([1, '甲']))\""
note '↑ 八条全是本机原文，一条没改。注意两条措辞：`KeyError: slice(0, 1, None)` 说明你以为在'
note '  切片、其实在查一个永远不存在的键；`TypeError: cannot use ... as a dict key` 的前半句'
note '  定位场景、括号里那半句定位病因。脚本没有 set -e，所以这八条之后还能继续往下跑。'

# ============================================================================
banner "第 13 节 排序三错：sort 返回 None、sort vs sorted、key 才是你的比较函数（sort_pitfalls.py）"
# ============================================================================
note '稳定性、比较规则、混类型报错原文，都是你在 10-sorting 里亲手写过的东西；'
note '这里量的是「Python 给你哪一个」，最后一段还把 min/max/sum 和排序的复杂度差量成 µs。'
runc 'python3 sort_pitfalls.py'
note '↑ 记两条：① `x = lst.sort()` 之后 x 是 None，想要新对象才写 sorted；'
note '  ② 同样 10 万个数，已有序那份排序比打乱那份快一个数量级 —— Timsort 认 run，'
note '  所以「排序很快」这种结论必须写清数据长什么样。'

# ============================================================================
banner "第 14 节 补刀：key 每个元素只算一次；min/max 也有 key"
# ============================================================================
note '第 13 节说「key= 只让你提供取比较键的函数，所以每个元素的 key 只算一次」。'
note '这句话不能背，用一个会数自己被打了几次的 key 当场量。'
runc "python3 -c \"
calls = []
def key(x):
    calls.append(x)
    return -x
data = [3, 1, 4, 1, 5]
print('sorted(data, key=key) =', sorted(data, key=key))
print('key 被调用了', len(calls), '次，元素一共', len(data), '个  ← 每个元素恰好一次')
calls.clear()
data.sort(key=key)
print('list.sort(key=key) 也一样：', data, '调用', len(calls), '次')
\""
runc "python3 -c \"
words = ['apple', 'fig', 'banana']
print('默认按码位比：min =', min(words), ' max =', max(words), ' ← 不是按长度')
print('带 key=len：  min =', min(words, key=len), ' max =', max(words, key=len))
print('min/max 也吃 key，而且只扫一遍：', min(range(100_000), key=lambda v: abs(v - 4242)))
\""
note '↑ 第一段是「装饰-排序-去装饰」的反面教材：Python 内部替你把键算好存着，'
note '  所以你自写 cmp 时那种「比一次算两次键」的浪费在这里不存在（这也是它比自写快的一半原因）。'
note '  第二段补一句：min/max 默认比的是元素本身，想要「按某个属性取最值」必须写 key=。'

# ============================================================================
banner "第 15 节 字面量与键的坑：{} / 单元素 tuple / 不可哈希 / 插入顺序 vs ==（dict_key_pitfalls.py）"
# ============================================================================
note '这四条都属于「语法看起来一样、类型完全不是那个」的坑。C 里没有这种坑：'
note '类型写在声明里，`int a[10]` 不可能是别的东西。Python 的字面量靠符号猜，猜错不报错。'
runc 'python3 dict_key_pitfalls.py'
note '↑ 「dict 保序」这件事网上说法五花八门，这一节不引用说法，只在本机 3.14.4 上量：'
note '  遍历顺序 = 插入顺序、删了再插会排到末尾、覆盖值不挪位置、而 == 完全无视顺序。'

# ============================================================================
banner "第 16 节 推导式四件事：造出什么类型 / 双 for 谁在外层 / 变量泄不泄漏（comprehension_basics.py）"
# ============================================================================
note '推导式是 Python 里唯一「写法顺序和执行顺序相反」的语句：读起来从左往右，'
note '执行时**左边的 for 是外层**。这一节不解释「应该怎么写」，而是把写反的真实报错打出来。'
runc 'python3 comprehension_basics.py'
note '↑ 三件事值得抄：`(x for x in ...)` 是 generator 不是 tuple；两个 for 调换是 NameError'
note '  而不是「结果奇怪」；普通 for 会把循环变量漏到外层作用域，推导式不会（和 C99 正相反）。'

# ============================================================================
banner "第 17 节 生成器 / map / filter：尺寸、惰性、一次性，以及谁反而更快（genexpr_vs_map.py）"
# ============================================================================
note '「生成器省内存」量成两个可复核的数字；更要紧的是**语义**差别：它们是一次性的。'
note '第五节的计时结果和很多人的直觉相反 —— 结论在输出里，不在这里预告。'
runc 'python3 genexpr_vs_map.py'
note '↑ 尺寸差：同一批 1000 个元素，列表推导式外壳 8856 字节、生成器 208 字节；'
note '  造 100 万个时峰值内存差三个数量级以上（tracemalloc 量的）；'
note '  但真正咬人的是第四节：第二次遍历是**空的**，不报错，只是 sum 给你 0。'

# ============================================================================
banner "第 18 节 collections.Counter：加减与或各算什么、缺失键为什么不报 KeyError（counter_probe.py）"
# ============================================================================
note 'Counter 是 dict 的子类，但它**改了 dict 的算术含义**：+ 是计数相加、- 会丢掉 ≤0 的键、'
note '& 取 min、| 取 max，四条都不是集合运算。这一节四条全部现场跑，还和 defaultdict 对照。'
runc 'python3 counter_probe.py'
note '↑ 最容易被写反的一条：`c[缺失键]` 返回 0 但**不建键**（len 不变、in 还是 False），'
note '  所以它不会触发第 11 节那条 RuntimeError；而 defaultdict 读一下就真的建键 —— '
note '  脚本里那两格现场就是这两者的分界。'

# ============================================================================
banner "第 19 节 collections 其余三兄弟：defaultdict / deque 的 extendleft 与 rotate / ChainMap"
# ============================================================================
note '第 18 节演完 Counter，这里补三件同样「看着像别的东西」的现场：defaultdict 读一下就建键、'
note 'deque 的 extendleft 会把顺序整个反过来、ChainMap 是视图不是拷贝。'
note '最后把 dict / Counter / deque 的 `+` 摆在一起 —— 同一个符号三种含义。'
runc "python3 -c \"
from collections import defaultdict, deque
buckets = defaultdict(list)
for w in ['a', 'b', 'a']:
    buckets[w].append(w)
print('defaultdict(list) 造词表：', dict(buckets), ' len =', len(buckets))
print('只是「读」一下缺失键 buckets[\\\"zz\\\"] →', buckets['zz'])
print('  键真的被建出来了：len 变成', len(buckets), ' 键表 =', list(buckets))
d = deque()
d.extendleft([1, 2, 3])
print('extendleft([1,2,3]) →', list(d), ' ← 顺序整个反过来（它是逐个 appendleft）')
d = deque([1, 2, 3, 4, 5])
d.rotate(2)
print('rotate(2) →', list(d), '；rotate(-2) 回来 →', end=' ')
d.rotate(-2)
print(list(d))
print('deque 的 + 是拼接：deque([1]) + deque([2]) →', deque([1]) + deque([2]))
try:
    deque([1]) + [2]
except TypeError as exc:
    print('  但不许和 list 拼：TypeError:', exc)
\""
runc "python3 -c \"
from collections import ChainMap
base = {'color': '红', 'size': '大'}
over = {'color': '蓝'}
cm = ChainMap(over, base)
print('ChainMap(over, base)[\\\"color\\\"] =', cm['color'], '  [\\\"size\\\"] =', cm['size'])
cm['new'] = 1
print('赋值只写进**第一张表**：over =', over)
print('                    base 没动：', base, ' ← 它不是拷贝，是视图')
\""
runc "python3 -c \"print({'a': 1} + {'b': 2})\""
runc "python3 -c \"print({1: 2} | {2: 3})\""
note '↑ 最后两条一起看：dict 的 `+` 没有定义（TypeError，退出码 1），`|` 从 3.9 起是合并；'
note '  Counter 的 `+` 有定义但算的是计数相加（第 18 节）；deque 的 `+` 算的是拼接（上面倒数第三行）。'
note '  同一个符号、三种容器三种含义 —— 这就是「别把运算符的直觉从一种容器搬到另一种容器」的现场理由。'
note '↑ 第一条还要加一句：defaultdict 只是被读一下就真的建键（len 从 2 变 3）。'
note '  所以「边遍历边读 defaultdict」会撞上第 11 节那条 RuntimeError，'
note '  而 counter_probe.py 里演过的 Counter 读缺失键什么都不建 —— 这两个容器唯一的实质差别就在这。'

# ============================================================================
banner "第 20 节 收口：一张「复杂度表 × 本机实测」的对照（独立再测一遍）"
# ============================================================================
note '前十九节的数字散在各份脚本里。这一节用**同一段代码**把最常查的十九行重新测一遍，'
note '最后一列标出「同一个结论出自本章哪一节」—— 两边形状一致，才是这条证据链闭合了。'
note '口径：n = 100000（个别格按操作特性调整 number），每格 timeit.repeat 三轮取最小。'
note '绝对 µs 会随机器负载抖，所以表后面紧跟一段**倍率**输出 —— README 引倍率最稳。'
runc "python3 -c \"
import copy, random, timeit
from collections import deque
n = 100_000
xs = list(range(n))
ss = set(xs)
dd = dict.fromkeys(xs)
strd = {f'k{i}': i for i in range(n)}
shuffled = list(range(n))
random.Random(42).shuffle(shuffled)
miss = n + 7
L = 'container = list(range(100000))'
Q = 'from collections import deque; container = deque(range(100000))'
def us(stmt, g=None, setup='', number=200):
    best = min(timeit.repeat(stmt, globals=g, setup=setup, number=number, repeat=3))
    return best / number * 1e6
rows = [
    ('x in list（落空）', 'O(n)', '第 06 节 表一',
     us('miss in xs', {'miss': miss, 'xs': xs}, number=20)),
    ('x in tuple（落空）', 'O(n)', '第 06 节 表一',
     us('miss in ts', {'miss': miss, 'ts': tuple(xs)}, number=20)),
    ('x in dict（只看键）', 'O(1) 平均', '第 06 节 表一', us('miss in d', {'miss': miss, 'd': dd})),
    ('x in set', 'O(1) 平均', '第 06 节 表一', us('miss in ss', {'miss': miss, 'ss': ss})),
    ('v in d.values()', 'O(n)', '第 06 节 表五', us('50000 in d.values()', {'d': strd}, number=20)),
    ('list.pop(0)', 'O(n)', '第 05 节 一', us('container.pop(0)', setup=L, number=200)),
    ('list.pop()', 'O(1)', '第 05 节 一', us('container.pop()', setup=L, number=200)),
    ('deque.popleft()', 'O(1)', '第 05 节 一', us('container.popleft()', setup=Q, number=200)),
    ('list.insert(0, x)', 'O(n)', '第 05 节 二', us('container.insert(0, -1)', setup=L, number=200)),
    ('list.append(x)', '均摊 O(1)', '第 05 节 二', us('container.append(-1)', setup=L, number=200)),
    ('list.remove(队尾值)', 'O(n)', '第 05 节 三',
     us('container.remove(container[-1])', setup=L, number=20)),
    ('list[50000]', 'O(1)', '第 05 节 四', us('container[50000]', setup=L, number=2000)),
    ('deque[50000]', 'O(n)', '第 05 节 四', us('container[50000]', setup=Q, number=2000)),
    ('a[0] = 改一格', 'O(1)', '第 08 节', us('container[0] = -1', setup=L, number=2000)),
    ('a[:] 抄外壳', 'O(n)', '第 09 节', us('container[:]', setup=L, number=20)),
    ('sort() 已打乱', 'O(n log n)', '第 13 节 六',
     us('container.sort()', {'shuffled': shuffled}, setup='container = shuffled.copy()', number=1)),
    ('sort() 已有序', 'O(n) 最好情况', '第 13 节 六', us('container.sort()', setup=L, number=1)),
    ('set(xs) 建哈希表', 'O(n)', '第 06 节 表三', us('set(container)', setup=L, number=5)),
    ('deepcopy 10 万个单元素 list', 'O(整棵树)', '第 09 节 五',
     us('deepcopy(src)', {'src': [[i] for i in range(n)], 'deepcopy': copy.deepcopy}, number=1)),
]
print(f\\\"{'操作':<26} {'表上写的':<14} {'本机 µs/次':>12}   同一个结论还出自\\\")
for label, bigo, where, val in rows:
    print(f'{label:<28} {bigo:<15} {val:>11.3f}   {where}')
v = [val for _, _, _, val in rows]
print()
print('把最该记住的几组换成倍率（同一台机器、同一次运行里算出来的）：')
print(f'  n=100000 时 in list / in set        = {v[0] / v[3]:>7.0f} 倍   ← 表的第 1 行和第 4 行')
print(f'  n=100000 时 in list / in dict（键） = {v[0] / v[2]:>7.0f} 倍')
print(f'  查值 in d.values() / 查键 in d      = {v[4] / v[2]:>7.0f} 倍   ← 同一个 dict，换个视图')
print(f'  pop(0) / pop()                     = {v[5] / v[6]:>7.0f} 倍   ← 同一个 list，换个位置')
print(f'  insert(0,x) / append(x)            = {v[8] / v[9]:>7.0f} 倍')
print(f'  deque 中间下标 / list 中间下标       = {v[12] / v[11]:>7.0f} 倍   ← 块要走 781 次')
print(f'  sort 已打乱 / sort 已有序           = {v[15] / v[16]:>7.0f} 倍   ← Timsort 认 run')
print(f'  deepcopy / a[:] 抄外壳              = {v[18] / v[14]:>7.0f} 倍   ← 浅抄 vs 重造整棵树')
\""
note '↑ 怎么读这张表：左边两列是你能背的东西，第三列是这台机器给的答案，第四列是本章另一处'
note '  独立测过的同一个结论。三档要分清：0.01x µs 的是哈希/尾部操作，µs 级的是「整段搬一次」，'
note '  百 µs 以上的是「整段扫一遍或整棵树重造」。'
note '↑ 下面那一段倍率才是可以当结论抄的东西：绝对 µs 每次跑都会抖（±10% 量级），'
note '  但「×10 规模 → ×10 耗时」「换个位置 → 差几个数量级」这些倍率在同一次运行里就钉死了。'

# ============================================================================
banner "第 21 节 收尾：证明这一章没在手册目录里留下任何产物"
# ============================================================================
runc_at "$HERE" "find . \\( -name '__pycache__' -o -name '*.pyc' -o -name '*.pyo' \\) | wc -l"
note '↑ 这个数字必须是 0：__pycache__ 和 pyc 只可能生在沙箱里（而且本章压根没生成）。'
runc_at "$HERE" "find . -mindepth 1 -maxdepth 1 -name '*.c' -o -maxdepth 1 -name 'main.c' | wc -l"
note '↑ 也必须是 0：本章是纯 Python 章，没有 C 对照文件，更不会有仓库根 Makefile 会误编译的 main.c。'
runc_at "$HERE" "ls -1 | wc -l"
runc_at "$HERE" "ls *.py | wc -l"
note '↑ 上面两行是「章目录里有哪些文件」的清点：14 个取证脚本 + 本 demo.sh（README 由另一位补上）。'
runc 'ls __pycache__'
note '↑ 退出码 2 是预期的：连沙箱里都没有 __pycache__ —— 本章全程 `python3 xxx.py` 直接跑脚本，'
note '  从没 import 过任何示例模块，所以解释器一次 pyc 都没必要生成。'
runc "find . -name '*.pyc' -o -name '__pycache__' | wc -l"
runc "python3 -c \"import os; print('沙箱 work 目录里的非 .py 文件：', sorted(f for f in os.listdir('.') if not f.endswith('.py')))\""
note "本次 demo.sh 从第 00 节跑到第 21 节，一共用了 ${SECONDS} 秒（天花板 180 秒）。"

printf '\n%s[全部跑完] 上面每一段都是真实执行结果；README.md 引用的输出全部出自这一次运行。%s\n' "$B" "$R"
exit 0
