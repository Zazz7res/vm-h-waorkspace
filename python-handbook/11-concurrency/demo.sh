#!/usr/bin/env bash
# CHECK: RUN=就地   理由：本章的证据全是「**七支取证脚本当场跑一遍**」，而它们必须待在同一目录里
#        （脚本之间会互相 import、forkserver 会重跑主模块）。校验器把脚本拷进沙箱就跑不到兄弟文件，
#        所以按全书约定标成就地跑两遍。章目录本身全程只读：脚本整份 cp 进 mktemp -d 的沙箱再跑，
#        两趟之间目录清单必须一个字都不多 —— 这条就在第 10 节当场数给你看。
# ============================================================================
# 第 11 章《并发：谁在真正同时跑》—— 整章证据一键复现
#
#   bash demo.sh                 跑完自动删沙箱（默认）
#   bash demo.sh --keep          跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt 2>&1  重定向时自动关颜色，得到纯文本（README 引用的就是这份）
#
# 本章四条主线（每节的 note 都指回其中一条）：
#   ① GIL 是「一次只让一个线程跑字节码」的锁，不是「不能并发」—— 等待时它放手，算死时它收税。
#   ② threading / multiprocessing / asyncio / concurrent.futures 换的不是并行度，是**谁在什么时候让出**。
#   ③ 本机 3.14 的 multiprocessing 默认 start method 是 forkserver，它会重新 import 主模块
#      —— 没有 main 守卫的脚本被整份重跑，这一格是当场炸出来的（c 篇第 2 格）。
#   ④ 有 GIL ≠ 线程安全，await 了 ≠ 并发（b 篇第 5~7 格、d 篇第 4 格）。
#
# 十节各自的分工：
#   第 1 节   地板：解释器身份、GIL 的编译期/运行期、start method、在场与缺席名单
#   第 2~8 节 七支取证脚本各一节（a 身份 → b 线程 → c 进程 → d 协程 → e 池与 select → f 误区 → g 收口）
#   第 9 节   章级互相对账：七支脚本的 rc 与「N/N 条判据」必须齐；四块地板在全场只许出现一个值
#   第 10 节  红线自证：沙箱产物、章目录只读、家目录、监听端口、子进程归零、屏幕上七类禁串
#             —— 每一类都**先造一个真样本让它响**，再报 0
#
# 屏幕上是两种行，别混：
#   `$ 命令` 底下到 `[退出码 N]` 为止的输出，全是这台机器当场跑出来的原文（过 fold 折身份之前除外）；
#   以 `#` 开头的是我写给下一格的说明，不是屏幕产物。
#
# 安全说明：
#   * 章目录永远只读：七支取证脚本整份 cp 进 mktemp -d 的沙箱里再跑，章目录不会留下
#     __pycache__/、*.pyc、*-demo/。第 10 节先把这四样故意造出来、数到非 0，再清掉报 0。
#   * 只用标准库；不 pip install、不联网、**不开任何监听端口**（第 1 节记一次监听 socket 数，
#     第 10 节再记一次，两次必须一样）。起的子进程全部由脚本自己 join/wait/shutdown 收尾，
#     第 10 节再把「本 demo 的直接子进程」数一遍，必须是 0。
#   * 每支脚本外面套 `timeout 600`：并发脚本最坏的失败模式是**卡住**（少一个哨兵、少一次 join），
#     而卡住不会打印任何东西。rc=124 就是当场把「卡住」这件事印出来，不是把观众晾在那儿。
#   * 故意不加 `set -e`：本章一半是失败现场（RuntimeError、ValueError、PicklingError、CancelledError…），
#     那些「炸」被脚本里的 eq() 接住当判据用；一遇非 0 就停，屏幕上只剩第一格。
#   * 屏幕上不出现真实家目录 / 用户名 / 主机名 / 沙箱路径 / 对象地址 / **pid / 秒与毫秒 /
#     丢更新后的具体计数**：取证脚本自带 mask()，本 demo 另有一道 fold 把这四类精确串折成占位符，
#     第 10 节把整份屏幕账扫七遍，七类模式各数一次。
#     ★ 本章比别的章多两类禁串：耗时数字（`数字.数字s`、`耗时`、`N 秒`、`毫秒`）和 pid 形状 ——
#       并发这一章里「快了多少倍」每跑必变，只有「有没有明显快过串行」这种带大余量的布尔才配当证据。
#   * ★ 沙箱名里那 6 个随机字符每跑必变，所以默认收尾只印「沙箱已删除」而不印路径；
#     否则两遍 transcript 永远不可能逐字一致（--keep 是给人看的，不参与对账）。
#   * 本脚本自己的退出码恒为 0，这样才能当验收闸门用。
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：${arg}（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

export LC_ALL=C.UTF-8 LANG=C.UTF-8 LANGUAGE=C
unset PYTHONPATH PYTHONHOME PYTHONSAFEPATH PYTHONSTARTUP PYTHONINSPECT PYTHONDEBUG
unset PYTHONDONTWRITEBYTECODE PYTHONOPTIMIZE PYTHONWARNINGS PYTHONDEVMODE PYTHONFAULTHANDLER
# 取证脚本里要数「几个不同 pid / 几个不同线程 / 集合大小」，哈希种子固定下来这些计数才不漂。
export PYTHONHASHSEED=0

if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
# ★ 章目录清单必须在 cd 进沙箱**之前**取，否则第 10 节拿去对照的就是沙箱 work/ 的清单了。
#   排除 README.md：这一章的书页写完之后还要重跑本 demo，那不算产物。
LS_BEFORE=$(cd -- "$HERE" && ls -1 | grep -v '^README\.md$' | tr '\n' '|')
HOME_N_BEFORE=$(ls -A "$HOME" | wc -l)
LISTEN_BEFORE=$(ss -ltn 2>/dev/null | tail -n +2 | wc -l)

SB=$(mktemp -d "${TMPDIR:-/tmp}/py11-XXXXXX")
MAIN_PID=$$                      # 第 10 节要数「还挂在 demo 名下的孩子」，得先把这个 pid 钉死
export MAIN_PID HERE SB LS_BEFORE HOME_N_BEFORE LISTEN_BEFORE
: > "$SB/all.out"                       # 屏幕账：第 9、10 节的对账与扫描吃的就是这一份（已折身份）
: > "$SB/foldhits"                      # 每次 fold 折中几行，逐笔记账；第 10 节要它 >0
mkdir -p "$SB/work"
cp "$HERE"/*.py "$SB/work/"
cd "$SB/work" || exit 1
umask 022

banner() {
    printf '\n%s══════ 第 %s 节 %s ═══════════════════════════════%s\n' "$B" "$1" "$2" "$R"
    printf '══════ 第 %s 节 %s ═══════════════════════════════\n' "$1" "$2" >> "$SB/all.out"
}
note() {
    printf '%s\n' "$1"
    printf '%s\n' "$1" >> "$SB/all.out"
}

# fold_id：上屏前把真实身份折成占位符；每折中一行就往 foldhits 记一笔（第 10 节要证明它真挡过东西）。
# ★ 模式串全部当场取（$SB、$HOME、主机名、$USER），源码里一个字都没写死 —— 写死的话这份 demo
#   自己就成了「带着模式串的文件」，任何字面扫描都会自匹配。折的顺序也是算过的：
#   主机名必须排在用户名**前面**，否则形如「用户名-机型」的主机名会被折成「<用户>-机型」漏半截。
fold_id() {
    local tmp="$SB/.foldtmp" hn hits
    hn=$(hostname)
    cat > "$tmp"
    hits=$(grep -c -F -e "$SB" -e "$HOME" -e "$hn" -e "$USER" "$tmp")
    printf '%s\n' "${hits:-0}" >> "$SB/foldhits"
    sed -E "s#$SB#<沙箱>#g; s#$HOME#<家目录>#g; s#\\b$hn\\b#<主机名>#g; s#\\b$USER\\b#<用户>#g" "$tmp"
    rm -f "$tmp"
}

# runc：打印命令 → 执行（stderr 合并、先过 fold）→ 打印退出码。三样都进屏幕账。
# ★ rc 取的是 PIPESTATUS[0]，而且这条管道**不能包在 $( ) 里**：一旦包进命令替换，
#   父 shell 的 PIPESTATUS 就只剩「这段替换自己」的状态（等于 fold 的 rc，恒 0），
#   屏幕上每一条 [退出码 0] 都会变成假凭证。所以先落到临时文件，再分两路输出。
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    printf '$ %s\n' "$1" >> "$SB/all.out"
    local rc
    bash -c "$1" 2>&1 | fold_id > "$SB/.line.out"; rc=${PIPESTATUS[0]}
    cat "$SB/.line.out"
    cat "$SB/.line.out" >> "$SB/all.out"
    printf '[退出码 %d]\n' "$rc"
    printf '[退出码 %d]\n' "$rc" >> "$SB/all.out"
}

# runscript <脚本名.py>：套 timeout 跑一支取证脚本。屏幕账留一份，另外单独留一份正文和 rc，
# 第 9 节就按这两份对账（文件名里的脚本名带 .py，两边必须同一个写法，别一边加 .txt 一边不加）。
runscript() {
    printf '\n%s$ timeout 600 python3 %s%s\n' "$C" "$1" "$R"
    printf '$ timeout 600 python3 %s\n' "$1" >> "$SB/all.out"
    local rc
    timeout 600 python3 "$1" 2>&1 | fold_id > "$SB/.line.out"; rc=${PIPESTATUS[0]}
    cat "$SB/.line.out"
    cat "$SB/.line.out" >> "$SB/all.out"
    printf '[退出码 %d]\n' "$rc"
    printf '[退出码 %d]\n' "$rc" >> "$SB/all.out"
    cat "$SB/.line.out" > "$SB/one-$1"
    printf 'rc=%s\n' "$rc" > "$SB/rc-$1"
}

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去接着敲： cd "%s/work"\n  看完自己删： rm -rf "%s"\n' "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] <本次 mktemp -d 出来的目录，名字每跑随机，所以不打出来>\n'
    fi
}
trap cleanup EXIT

# ============================================================================
banner 1 "地板：这台机器的解释器到底是怎么并发它的"
# ============================================================================
note '本章后面每一个判定都钉在这五行上。先当场探，别背 —— 换一台机器（有 free-threading 版、'
note '或者默认 start method 不是 forkserver）这些结论就得重测，本章不替那台机器说话。'
runc 'python3 -c "
import sys, sysconfig, multiprocessing as mp, os, selectors
print(\"版本 →\", sys.version.split()[0], \"｜ 实现 →\", sys.implementation.name)
print(\"sys._is_gil_enabled() →\", sys._is_gil_enabled(), \"｜ 编译期 Py_GIL_DISABLED →\", sysconfig.get_config_var(\"Py_GIL_DISABLED\"))
print(\"sys.getswitchinterval() →\", sys.getswitchinterval(), \"｜ os.cpu_count() →\", os.cpu_count(), \"｜ sched_getaffinity →\", len(os.sched_getaffinity(0)))
print(\"multiprocessing 全部 start method →\", mp.get_all_start_methods(), \"｜ 默认 →\", mp.get_start_method())
print(\"selectors.DefaultSelector 是谁 →\", type(selectors.DefaultSelector()).__name__)
"'
note '★ 第三行是本章的地板：GIL **编译期带着、运行时开着**。所以「3.14 可以把 GIL 去掉」这种话'
note '  在本机兑现不了 —— 下面两条现场把这条路堵死（没那个解释器、也没那个编译开关）。'
runc 'python3 -c "
import glob, importlib.util as u, os, sysconfig
print(\"自由线程解释器 python3.14t 在场吗 →\", any(glob.glob(p + \"/python3.14t\") for p in os.environ[\"PATH\"].split(os.pathsep)))
print(\"编译期开关 Py_GIL_DISABLED →\", sysconfig.get_config_var(\"Py_GIL_DISABLED\"), \"（非 1 就是带着 GIL 编的：本机这个值当场印出来）\")
print(\"pip 在场吗 →\", u.find_spec(\"pip\") is not None)
for name in (\"uvloop\", \"trio\", \"curio\"):
    print(f\"  {name:7s} 在场吗 →\", u.find_spec(name) is not None)
"'
note '↑ 除编译期那个数值外全是 False —— 这就是本章的边界：没有 free-threading 解释器、没有第三方'
note '  事件循环、连 pip 都不在场（装不了东西），所以本章只讲标准库那四种「谁在调度」，'
note '  一句「装个 xx 就好了」都不许出现。'
runc 'python3 -c "
import importlib.util as u
for m in (\"threading\",\"multiprocessing\",\"asyncio\",\"concurrent.futures\",\"selectors\",\"queue\",\"signal\",\"_thread\",\"subprocess\",\"socket\"):
    print(f\"  {m:22s} →\", \"在场\" if u.find_spec(m) else \"缺席\")
import os, asyncio
print(\"  os.fork 有吗 →\", hasattr(os, \"fork\"), \"｜ asyncio.to_thread 有吗 →\", asyncio.to_thread is not None)
"'
note '  → 判据：上面这几行是「本章能演什么」的名单。名单外的（去 GIL、uvloop、装包）本章一律只字不提。'

# ============================================================================
banner 2 "a 篇：GIL 到底是什么 —— 身份、粒度、什么时候放手"
# ============================================================================
note '主线① 的现场。屏幕上要盯三格：第 2 格「GIL 的粒度比你的语句细得多」（字节码条数）、'
note '第 4 格「等待时它放手」（主线程睡在自己的 Lock 上，旁观线程一路推进）、'
note '第 6 格「GIL 管执行权，Lock 管你的临界区」—— 有 GIL ≠ 互斥，这是主线④的前半。'
runscript a_gil_identity.py
note '  → 判据：a 篇把「一次只让一个线程跑字节码」演成了可数的东西：等待能叠、算死不能叠，'
note '    而这两件事同时成立，才叫理解了 GIL。'

# ============================================================================
banner 3 "b 篇：线程的两副面孔 —— 等的时候是帮手，算的时候是排队"
# ============================================================================
note '主线① 的另一半 + 主线④ 的主现场。盯四格：第 1~3 格（CPU 密集不加速、IO 密集真并发）、'
note '第 4 格（setswitchinterval 是公平性旋钮，不是并行度旋钮）、'
note '第 5~7 格（丢更新三联：纯 += 本机不丢 / 读→让出→写必丢 / 加锁一条不丢）、第 8 格（daemon 被硬切）。'
runscript b_threads_two_faces.py
note '  ★ 第 5 格那句「本机演不出来」是本章最值钱的一行：它挡住的是「GIL 下 += 一定丢」这句'
note '    到处都在抄的说法。屏幕上只有布尔，没有「还剩多少」—— 那个数每跑必变，不配当证据。'
note '  → 判据：选型只问一句「你的活卡在什么上面」；线程数、倍数都不许背。'

# ============================================================================
banner 4 "c 篇：forkserver 是默认，而它会把你的主模块重跑一遍"
# ============================================================================
note '主线③ 的主现场。第 2 格是当场炸出来的：一个没有 if __name__ == "__main__" 守卫的样本脚本'
note '被 forkserver 子进程整份重跑，RuntimeError 原文逐字抄回屏幕、退出码 1。'
note '第 5 格更反直觉：只是**问了一句** get_start_method()，默认就再也 set 不动了（force=True 才掰得开）。'
runscript c_start_methods.py
note '  ★ 这一节屏幕上没有任何秒数：三种 start method 的启动代价只用「同一动作连做三次取最快」'
note '    比一个带大余量的布尔，因为绝对值每跑必变（机器上还有别的并发脚本在抢核）。'
note '  → 判据：进程那条路的门票是「可 pickle」+「有 main 守卫」，两样缺一样都会当场炸给你看。'

# ============================================================================
banner 5 "d 篇：协程不是自动并发 —— 不 await 就不跑，不让出就全冻"
# ============================================================================
note '主线② 与主线④ 的后半。第 1 格：调用协程函数只生产一个协程对象，屏幕上就有那条'
note 'RuntimeWarning: coroutine ... was never awaited 的原文。第 4 格是本章的量法：'
note '主协程里放一句同步 sleep，那一刻 ticker 一拍都没跳（await 版已经跳过了拍）——'
note '总拍数两种写法都满，所以「任务没死，只是全部迟到」。'
runscript d_asyncio_scheduling.py
note '  → 判据：量「循环有没有被冻住」只能数结构（跳了几拍、拍数是不是 0），数秒就废了。'

# ============================================================================
banner 6 "e 篇：池与 select —— map 保序、as_completed 保先后、异常攒到 result()"
# ============================================================================
note '主线② 的落地面。盯三格：第 3 格 submit 之后异常不当场抛、在 .result() 才抛（原文照抄）；'
note '第 6 格 ex.map 按投递顺序出货、as_completed 按完成先后 —— 屏幕上问的是'
note '「回来的先后是不是发出的先后」，不是「谁花了多久」。第 7 格用 subprocess + selectors'
note '同时收 N 路输出，按谁先吐字收谁。'
runscript e_futures_and_selectors.py
note '  ★ 「线程池 vs 进程池」的选型判据在这一节是**同一件活跑两遍**量出来的，不是背出来的。'
note '  → 判据：顺序与计数能复现，时长不能；所以这一节的每条判定都只问形状。'

# ============================================================================
banner 7 "f 篇：误区合集 —— 这些坑本机真炸过，炸不出来的写「没演」"
# ============================================================================
note '第 1 格：子线程里装信号，ValueError 原文。第 2 格：循环里再 asyncio.run，RuntimeError 原文。'
note '第 4 格：局部函数/闭包丢进进程池 —— 本机印的是 PicklingError 原文（契约里预告的是'
note 'AttributeError，本机给的是哪一个就抄哪一个，不猜）。第 5 格：list.append 与 dict[k]+=1 的原子性边界。'
note '第 7 格：queue 收尾不塞哨兵就是永久等待，屏幕上用 timeout 把「卡住」这件事印出来。'
runscript f_pitfalls.py
note '  ★ queue.Empty 在本机的全名印作 _queue.Empty —— 那是同一个对象的两个名字，屏幕上把这条'
note '    判据也一起给了，免得读者以为捡到了两个不同的异常。'
note '  → 判据：这一节所有「本机演不出来」的说法都留在屏上，没被抹成一句好看的结论。'

# ============================================================================
banner 8 "g 篇：收口 —— 同一件活，四种写法各跑一遍"
# ============================================================================
note '前七节各讲一种「谁在调度」，这一节把它们摆成一张表：活只有两种 —— 「等别人」（起 6 个'
note '子进程各睡一个固定参数）和「自己算死」（对几百万个整数求和）。每种写法在两种活上的表现'
note '当场判出来，屏幕上**一个秒数都没有**。'
note '★ 这一节还顺手把「池子实际派出几个 worker」那个数**从屏幕上撤了**：max_workers 只是上限，'
note '  本机三趟里 6 份活有一趟只被 5 个线程干完 —— 每跑必变的数不许当证据，所以只印「多于一个吗」。'
runscript g_four_ways_same_job.py
note '  → 判据：表里几行的差别不在「快多少」，在**代价** —— 线程版 0 个额外进程、asyncio 版 0 个额外'
note '    线程、进程版为并发要多起进程（上限就是你声明的 worker 数）且每份活都要 pickle 进出。'
note '    选型的唯一判据是「你的时间花在等还是花在算」。'

# ============================================================================
banner 9 "章级对账：七支脚本的账必须齐，四块地板全场只许一个值"
# ============================================================================
note '这一节不跑新东西，只把前面八节的屏幕账拿出来对。两件必须同时成立，且都由同一个解析器判：'
note '① 每支脚本的 rc 必须是 0（124 就是卡住，单独印出来），且各自的「N/N 条判据成立」里两个数'
note '   必须一样、N 必须 > 0（少一条就不是 N/N；g 篇那条印作「→ 判据 2/2 条成立」，形状不同但同一回事）。'
note '② 解释器版本 / GIL 运行期 / cpu_count / 默认 start method 这四块地板，在**第 9 节之前的整份屏幕账**里'
note '   各只许出现一个值 —— 跑着跑着变了，说明有脚本改了全局状态没还回去（setswitchinterval 那种）。'
note '   ★ 命中行数也进判据：只有一行命中不算「对上了」，那叫检查在空转。'
cat > "$SB/py11_check.py" <<'PYCHK'
"""读屏幕账做两件对账：rows = 每支脚本的 rc 与 N/N；floor = 四块地板全场只许一个值。

解析器只有一份：正式版和弄响版走的是同一段代码，所以「它会不会响」这件事可当场证。
账本目录由 PY11_LEDGER 指（默认 $SB）；判据不成立就 exit 1，让屏幕上的 [退出码 N] 自己说话。
"""
import os
import re
import sys

BASE = os.environ.get("PY11_LEDGER", os.environ["SB"])
NAMES = ["a_gil_identity.py", "b_threads_two_faces.py", "c_start_methods.py",
         "d_asyncio_scheduling.py", "e_futures_and_selectors.py", "f_pitfalls.py",
         "g_four_ways_same_job.py"]
# 汇总行有两种写法（a~c 一种、d~f 一种），g 篇用的是收尾自证那条 —— 三种都要认，
# 但认完之后判据一样：两个数相等，且不是 0/0。
SUM_RX = [r"全篇 (\d+)/(\d+) 条判据成立", r"全篇判据合计 (\d+)/(\d+) 条成立",
          r"→ 判据 (\d+)/(\d+) 条成立"]
FLOOR = [("解释器版本", r"(?:python →|版本 →) (\d+\.\d+\.\d+)", 2),
         ("GIL 运行期", r"(?:_is_gil_enabled\(\)|GIL 开着吗|GIL 在场吗|GIL 仍然在场)[^｜]*?(?:实测|→) (True|False)", 3),
         ("cpu_count", r"cpu_count\(\)[^｜]*?(?:实测|→|=) (\d+)", 3),
         ("默认 start method", r"默认[^｜\[]*?→ '?\w*(forkserver|fork|spawn)", 2)]


def rows():
    bad = 0
    for n in NAMES:
        body = open(os.path.join(BASE, "one-" + n), encoding="utf-8", errors="replace").read()
        rc = int(re.search(r"rc=(\d+)",
                           open(os.path.join(BASE, "rc-" + n), encoding="utf-8").read()).group(1))
        hits = []
        for rx in SUM_RX:
            hits += re.findall(rx, body)
        ok = rc == 0 and hits and all(a == b and int(b) > 0 for a, b in hits)
        print("  {:s} rc={:d} 汇总 {:d} 条：{:s} → {:s}".format(
            n.ljust(29), rc, len(hits),
            " ".join("{}/{}".format(a, b) for a, b in hits) if hits else "（没找到汇总行）",
            "成立" if ok else "[不成立！]"))
        bad += 0 if ok else 1
    print("  → 判据：七支脚本 rc 全 0、各自 N/N 齐且 N>0 →",
          "成立" if bad == 0 else "不成立（{:d} 支有毛病）".format(bad))
    return bad == 0


def floor():
    lines = open(os.path.join(BASE, "floor.out"), encoding="utf-8", errors="replace").read().splitlines()
    bad = 0
    for key, rx, want in FLOOR:
        vals = []
        for ln in lines:
            m = re.search(rx, ln)
            if m:
                vals.append(m.group(1))
        uniq = sorted(set(vals))
        ok = len(uniq) == 1 and len(vals) >= want
        print("  {:s} 命中 {:d} 行（要求 ≥{:d}），出现过的值 → {:s} → {:s}".format(
            key.ljust(18), len(vals), want, ",".join(uniq) if uniq else "一行都没命中",
            "成立" if ok else "[不成立！]"))
        bad += 0 if ok else 1
    print("  → 判据：四块地板各只许一个值，且命中行数够 —— 只有一行命中说明这道检查在空转。")
    return bad == 0


ok = rows() if sys.argv[1] == "rows" else floor()
sys.exit(0 if ok else 1)
PYCHK
note '★ 先让这套对账**响两次**：造一份假账本 —— 一支脚本的 rc 改成 124、另一支的汇总改成 43/44、'
note '  再把四块地板各塞一个不同的值进去。同一份解析器必须报不成立、退出码非 0。'
note '  ★ 地板那份扫的是第 9 节开跑**之前**的账本快照 floor.out —— 不这么切的话，本节自己印出来的'
note '    「出现过的值 → 3.14.4,9.9.9」这类汇总行会被下一遍扫描读进去，等于让检查读到自己的排泄物。'
note '    假值也是用 printf 的 %s 参数当场拼出来的：命令行原文里留的是「→ %s」，扫描器不会自己命中它。'
runc 'cp "$SB/all.out" "$SB/floor.out"
mkdir -p "$SB/poison-rows" "$SB/poison-floor"
cp "$SB"/one-* "$SB"/rc-* "$SB/floor.out" "$SB/poison-rows/"
cp "$SB"/one-* "$SB"/rc-* "$SB/floor.out" "$SB/poison-floor/"
echo "rc=124" > "$SB/poison-rows/rc-a_gil_identity.py"
printf "  全篇 43/44 条判据成立（假的）\n" > "$SB/poison-rows/one-b_threads_two_faces.py"
printf "%s → %s ｜ %s → %s ｜ %s → %s ｜ %s → %s\n" "本机 python" "9.9.9" "GIL 开着吗" "False" "os.cpu_count()" "99" "默认 start method" "spawn" >> "$SB/poison-floor/floor.out"
echo "两份假账本各造好一条（真账本一个字节没动）"'
runc 'PY11_LEDGER="$SB/poison-rows" python3 "$SB/py11_check.py" rows'
runc 'PY11_LEDGER="$SB/poison-floor" python3 "$SB/py11_check.py" floor'
note '↑ 上面两条退出码必须都非 0 —— 能响，下面这两条真判才有意义。'
runc 'python3 "$SB/py11_check.py" rows'
runc 'python3 "$SB/py11_check.py" floor'
note '  ★ floor 那四条正则扫的是**第 1~8 节的整份屏幕账**（第 1 节自己的地板行 + 七支脚本各自的正文都在'
note '    里面），所以「七支脚本互相看不见」这件事骗不过它：谁偷偷改了 setswitchinterval、或者中途换了'
note '    一个解释器，账本里就会多出第二个值，判据立刻翻成 [不成立！]。'

# ============================================================================
banner 10 "红线自证：沙箱里的事只留沙箱，屏幕上没有每跑必变的东西"
# ============================================================================
note '此刻当前目录就是沙箱 work/。先把这一条印出来 —— 它同时是「fold 真在挡东西」的那一笔账：'
runc 'echo "当前工作目录（屏幕上的这一行会被折成 <沙箱>/work）= $(pwd)"'
note '守卫不会响就等于没检查。先数「此刻沙箱里该有什么」，再故意造四样残留让它响：'
runc 'echo "沙箱 work/ 里的 .py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)（这 7 个是章目录副本，不是产物）  非 .py 文件数=$(find . -maxdepth 1 -type f ! -name "*.py" | wc -l)"'
runc 'mkdir -p gil-demo; touch 残留.tmp; printf "q = 1\n" > probe_mod.py; python3 -c "import probe_mod"; echo "塞进假产物之后：子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  .py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)  非 .py 文件数=$(find . -maxdepth 1 -type f ! -name "*.py" | wc -l)  __pycache__ 个数=$(find . -name __pycache__ | wc -l)  *.pyc 个数=$(find . -name "*.pyc" | wc -l)  ← 五项都会响"'
runc 'rm -rf gil-demo 残留.tmp probe_mod.py probe_mod.pyc __pycache__; echo "清掉之后：子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  .py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)  非 .py 文件数=$(find . -maxdepth 1 -type f ! -name "*.py" | wc -l)  __pycache__ 个数=$(find . -name __pycache__ | wc -l)  *.pyc 个数=$(find . -name "*.pyc" | wc -l)"'
note '↑ 那一次「响」正是本章的现场复演：只有 **import** 会造 __pycache__；七支脚本跑的是'
note '  `python3 名字.py`（直跑脚本不产缓存），它们自己 import 出去的样本模块全在各自的 *-demo/ 里，收尾各自 rmtree。'
runc 'python3 -c "
import pathlib
d = sorted(p.name for p in pathlib.Path(\".\").iterdir() if p.is_dir())
print(\"  沙箱 work/ 里剩下的子目录 →\", d if d else \"无\")
print(\"  → 判据：一个都不该剩（每支脚本自建子目录、自己 rmtree）→\", len(d) == 0)
"'
note '章目录（脚本本体在这里，demo 全程只读它）。清单在第 1 节之前、还在章目录里的时候就取好了：'
runc 'now=$(cd -- "$HERE" && ls -1 | grep -v "^README\\.md$" | tr "\\n" "|"); printf "章目录清单 跑前=[%s]\n现在  =[%s]\n一个字都没多吗 → %s\n" "$LS_BEFORE" "$now" "$([ "$now" = "$LS_BEFORE" ] && echo 是 || echo 否)"'
runc 'cd -- "$HERE" && echo "章目录 .py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)  demo.sh 个数=$(find . -maxdepth 1 -name "demo.sh" | wc -l)  子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  __pycache__ 个数=$(find . -name __pycache__ | wc -l)"'
note '家目录与监听端口各再数一遍，和第 1 节之前记下的那份对照 —— 两次一样就是「没写过、没开过」的凭证：'
runc 'printf "家目录条目数 跑前=%s 现在=%s → 一样吗 %s\n" "$HOME_N_BEFORE" "$(ls -A "$HOME" | wc -l)" "$([ "$(ls -A "$HOME" | wc -l)" = "$HOME_N_BEFORE" ] && echo 是 || echo 否)"'
runc 'printf "监听中的 TCP socket 数 跑前=%s 现在=%s → 一样吗 %s（本章 socket 只用 socketpair/unix，不开监听）\n" "$LISTEN_BEFORE" "$(ss -ltn 2>/dev/null | tail -n +2 | wc -l)" "$([ "$(ss -ltn 2>/dev/null | tail -n +2 | wc -l)" = "$LISTEN_BEFORE" ] && echo 是 || echo 否)"'
note '本 demo 亲手起的东西必须归零。这里数的是**当前还挂在 demo 进程名下的直接子进程**和'
note '还在跑的取证脚本 —— 并发章节最怕的就是「屏幕都印完了，后台还挂着六个 worker」。'
note '★ 这一行自己也是 demo 起的直接孩子（每一格都靠 `bash -c` 外壳跑），所以判据里排除掉外壳本身；'
note '  剩下的 python / timeout / sleep 一个都不许在。★ 先挂一个不许留下的 sleep 进来，让它响一次 ——'
note '  这个 sleep 由 demo 自己的 shell 起（不是 runc 的外壳），所以它真就挂在 MAIN_PID 名下。'
sleep 30 & POISON_CHILD=$!
runc 'printf "故意挂一个后台 sleep 进来 → [%s] → 一个都不剩吗 %s（这条该报「否」，否则守卫是死的）\n" "$(ps -o comm= --ppid "$MAIN_PID" 2>/dev/null | grep -v "^bash$" | tr "\\n" " ")" "$([ -z "$(ps -o comm= --ppid "$MAIN_PID" 2>/dev/null | grep -v "^bash$")" ] && echo 是 || echo 否)"'
kill "$POISON_CHILD" 2>/dev/null
wait "$POISON_CHILD" 2>/dev/null
runc 'printf "杀掉之后再数一遍 → [%s] → 一个都不剩吗 %s\n" "$(ps -o comm= --ppid "$MAIN_PID" 2>/dev/null | grep -v "^bash$" | tr "\\n" " ")" "$([ -z "$(ps -o comm= --ppid "$MAIN_PID" 2>/dev/null | grep -v "^bash$")" ] && echo 是 || echo 否)"'
runc 'c=$(pgrep -cf "python3 [a-g]_[a-z_]*\\.py" || true); printf "还在跑的取证脚本（按脚本名找）= %s 个 → 该是 0\n" "${c:-0}"'
note '★ 最后扫整份屏幕账。七类「不该上屏的东西」，前五类是全书纪律，后两类是**本章额外红线**：'
note '  家目录路径 / 用户名 / 主机名 / 沙箱路径（四类精确串）、对象地址形态 0x…、'
note '  ⑥一切时长读数（小数紧跟单位字母、数字后跟那个时间汉字、以及专门标时长的两个词）'
note '  ⑦进程号赋值形状。'
note '  ★ 这两条 note 故意不写出那几个词的原文：note 也会被记进屏幕账，说明行里带上那些字面，'
note '    扫描器就会自己扫到自己 —— 模式原文只写在单独那个 .py 里，那里不上屏。'
note '  扫描器写在单独一个 .py 里（不走 runc），先造一份必然全命中的样本让它响，再扫正式账 ——'
note '  只有正式版报 0 而样本也报 0，那才叫摆设。'
cat > "$SB/py11_scan.py" <<'PYSCAN'
"""把整份屏幕账（已折身份的那份）扫七遍，七类模式各数一次。

先扫一份**弄响版**：在真账后面追加七样真东西（家目录、登录名、主机名、沙箱路径、
一个对象的 repr、一句耗时、一句 pid）。七类必须全 > 0，否则说明某一类扫描是死的。
再扫正式账：七类必须全 = 0。两个判据都要成立才叫干净。
模式串一律当场取值来构造，源码里没有写死任何一条精确串。
"""
import os
import re
import socket

raw = open(os.path.join(os.environ["SB"], "all.out"), encoding="utf-8", errors="replace").read()
exact = [("家目录绝对路径", os.path.expanduser("~")),
         ("登录名", os.environ.get("USER", "")),
         ("主机名", socket.gethostname()),
         ("本次沙箱路径", os.environ["SB"])]
addr_rx = r"0x[0-9a-fA-F]{6,}"
time_rx = r"\d+\.\d+\s*[smh]\b|\d+\s*秒|毫秒|耗时"
pid_rx = r"pid\s*[=:] ?\d+|getpid\(\) ?[=] ?\d"


def scan(t):
    hits = [t.count(v) for _, v in exact]
    hits += [len(re.findall(addr_rx, t)), len(re.findall(time_rx, t)), len(re.findall(pid_rx, t))]
    return hits


labels = [k for k, _ in exact] + ["对象地址形态", "耗时数字形态", "pid 形态"]
poison = raw + "\n".join([v for _, v in exact] + [repr(object()), "耗时 0.123s", "pid=4242"]) + "\n"
ph = scan(poison)
print("  弄响版（副本里塞了七样真东西）：" + " ｜ ".join("{}={}".format(l, n) for l, n in zip(labels, ph)))
print("  → 判据：弄响版七类必须全 > 0 →", all(n > 0 for n in ph), "（有 0 就说明那类扫描是死的）")
h = scan(raw)
print("  正式版（整份屏幕账）：" + " ｜ ".join("{}={}".format(l, n) for l, n in zip(labels, h)))
print("  → 判据：正式版七类必须全 = 0 →", all(n == 0 for n in h))
tot = sum(int(x) for x in open(os.path.join(os.environ["SB"], "foldhits"), encoding="utf-8").read().split())
print("  上屏前被 fold 折掉的行数合计 =", tot, "→ >0 才证明折叠真在挡东西：", tot > 0)
ok = all(n > 0 for n in ph) and all(n == 0 for n in h) and tot > 0
if not ok:
    raise SystemExit(1)
PYSCAN
runc 'python3 "$SB/py11_scan.py"'
note '  ★ 三条判据是一条链：弄响版全 > 0（检查是活的）→ 正式版全 = 0（屏幕上真的干净）→'
note '    foldhits > 0（这份干净是「折掉了东西」换来的，不是「压根没东西可挡」）。'
note '    三条里断任何一条，这一章都不许交付：前者是检查死了，中间是真泄漏，后者是这条红线没在办事。'
runc 'printf "屏幕账行数（含命令行、退出码、说明行与七支脚本正文）= %s；节标题数 = %s → 该是 10；被记进账里的取证脚本 = %s 支 → 该是 7\n" "$(wc -l < "$SB/all.out")" "$(grep -c "^══════ 第 " "$SB/all.out")" "$(grep -c "^[$] timeout 600 python3 " "$SB/all.out")"'
note '  → 判据：这一节全部成立，才允许说「本章的沙箱只是它自己的沙箱、屏幕上的数只是能复现的数」。'

exit 0
