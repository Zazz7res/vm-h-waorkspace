#!/usr/bin/env bash
# ============================================================================
# 第 07 章《报错与清理：异常不是惩罚，是你拿到的第一手证据》—— 整章证据一键复现
#
#   bash demo.sh                 跑完自动删沙箱（默认）
#   bash demo.sh --keep          跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt 2>&1  重定向时自动关颜色，得到和本章 README 一样的纯文本
#
# 屏幕上出现的每一行，都是这条命令在这台机器（CPython 3.14.4 / Ubuntu）上的真实结果，
# 没有一行是手写的、没有一行是「我记得应该这样」。
#
# 安全说明：
#   * 示例源码整份复制进 mktemp 出来的沙箱再运行，所以章目录里**不会**留下
#     __pycache__/、*.pyc、files-demo/。第 21 节不直接报 0：每条计数都先被故意弄响一次
#     （造一个 .tmp、真 import 一个模块），再报 0 —— 恒 0 的检查比没有检查更糟。
#   * 第 18 节会在**沙箱的当前目录里**真建真删文件（cfg.json / real.json / 临时文件），
#     脚本跑完自己 rmtree；章目录从头到尾不参与写。
#   * 只用标准库 + 本机自带的 python3；不 pip install、不联网、不装东西、
#     不开任何监听端口、不改 ~/.bashrc / ~/.profile / ~/.config / ~/.local。
#   * 故意不加 `set -e`：本章大半是「失败现场」（咽异常、吞 KeyboardInterrupt、
#     __exit__ 返回 True、@contextmanager 忘了 yield、半截 JSON、EBUSY 重试耗尽…），
#     加了就只能看到第一段。
#   * 故意不加 `set -o pipefail`：`cmd | head -3` 这类截取会因为下游提前关管道报 141，
#     屏幕上会冒出一堆和教学无关的退出码。
#   * 屏幕上不主动打印绝对路径：需要「在某个目录里跑」时用 runc_at，只留目录 basename。
#     第 15 节是计时：绝对值每跑必抖，那一节只当**量级和倍率**读。
#   * 子进程一律带 capture：本脚本自己的退出码必须一路是 0，才能当验收闸门用。
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

# 固定语言：Python 自己的输出恒为英文，但 `command not found` 这类消息是 bash 翻译的；
# 设成 C.UTF-8 后整份 transcript 才能逐字比对（你手敲时看到中文完全正常）。
export LC_ALL=C.UTF-8
export LANG=C.UTF-8
export LANGUAGE=C
# 清掉可能干扰结论的环境变量（只影响本脚本这个进程和它的子进程，不碰你的配置文件）。
unset PYTHONPATH PYTHONSTARTUP PYTHONOPTIMIZE PYTHONINSPECT PYTHONWARNINGS
# 本章比的是「哪一行跑了、哪一行没跑」「顺序是谁先谁后」，不吃哈希种子；固定成 0
# 只是为了让同一份脚本在你机器上打出尽量稳定的输出。
export PYTHONHASHSEED=0

# 只有输出到终端才上色；重定向到文件时是干净纯文本
if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/py07-XXXXXX")
export SB

banner() { printf '\n%s══════ 第 %s 节 %s ═══════════════════════════════%s\n' "$B" "$1" "$2" "$R"; }
note()   { printf '%s\n' "$1"; }

# 打印命令 → 执行（stderr 合并进来）→ 打印退出码
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1
    printf '[退出码 %d]\n' "$?"
}

# runc_at <目录> <命令>：先 cd 进那个目录再跑，屏幕上只留目录 basename + 相对命令。
runc_at() {
    printf '\n%s$ 〔在 %s 里〕 %s%s\n' "$C" "$(basename "$1")" "$2" "$R"
    ( cd "$1" && eval "$2" ) 2>&1
    printf '[退出码 %d]\n' "$?"
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
banner 1 "环境体检：本章结论只在这么一个解释器上验过"
# ============================================================================
mkdir -p "$SB/work"
cp -R "$HERE"/. "$SB/work"/
cd "$SB/work" || exit 1
note '本章 19 个 .py（18 个取证脚本 + 第 20 节那个对账工具）+ demo.sh，沙箱里列一遍（章目录本身不产任何文件，第 21 节自证）：'
runc 'ls'
runc 'python3 --version && python3 -c "import sys; print(sys.version)"'
runc 'echo "LC_ALL=$LC_ALL  PYTHONHASHSEED=$PYTHONHASHSEED"; python3 -c "
import sys, contextlib, traceback, json
print(\"只用标准库这四个 + 内置：\", {\"contextlib\", \"traceback\", \"json\"} <= sys.stdlib_module_names)
print(\"contextlib 里本章用到的名字：\", [n for n in (\"suppress\",\"closing\",\"nullcontext\",\"redirect_stdout\",\"ExitStack\",\"contextmanager\") if hasattr(contextlib, n)])
"'
note '↑ 本章不碰文件名编码；固定 LC_ALL 只是想让 bash 自己的报错语言可预测。'
note '  另外把「本机有几个核」记在这儿：第 15 节的计时只能读倍率，就是因为这一行。'
runc 'python3 -c "import os; print(\"os.cpu_count() =\", os.cpu_count())"'
runc 'python3 -c "import sys; print(\"递归限额 =\", sys.getrecursionlimit(), \"｜ 第 15 节的深栈成本就按它算\")"'

# ============================================================================
banner 2 "一条 try 语句的走位：else / finally 到底什么时候跑"
# ============================================================================
runc 'python3 a_minimal_shape.py'
note '↑ 第 5 小段那条「finally 里 return」是本章第一个**没有任何痕迹**的坑，解释器自己都看不下去'
note '  并发了一条 SyntaxWarning —— 那条评论在别的节还会再出现一次（第 16 节）。'

# ============================================================================
banner 3 "异常也是一棵树：家谱、以及 except Exception 接不住的那三个"
# ============================================================================
runc 'python3 b_tree.py'
note '↑ 记住唯一的例外：KeyboardInterrupt / SystemExit / GeneratorExit 是 BaseException 的儿子，'
note '  不是 Exception 的儿子。写 except Exception 时它们**不会被接住** —— 这是故意的。'

# ============================================================================
banner 4 "except 的匹配顺序：写反了不报错，只是永远轮不到"
# ============================================================================
runc 'python3 c_matching.py'
note '↑ 本节最值钱的一条：家谱写反**没有任何警告**，只有窄的那条分支永远变死代码。'
note '  顺带钉一条：except as 的名字在块结束时会被 del 掉（第 5 小段）。'

# ============================================================================
banner 5 "异常链：__cause__ 是你写的，__context__ 是解释器记的"
# ============================================================================
runc 'python3 d_chain.py'
note '↑ 以后看到 traceback 中间那句话，先认它是哪一句：direct cause = 显式 from；'
note '  during handling = 处理时又炸了一个。两句的含义完全不同，查的方向也不同。'

# ============================================================================
banner 6 "raise 的三种形态、裸 raise、以及 assert 为什么不该给用户输入"
# ============================================================================
runc 'python3 e_raise.py'
note '↑ 第 6 小段用子进程跑了 python3 -O：assert 整条消失，于是原来被它挡住的除零真发生了。'
note '  这就是「assert 只写内部不变量」的全部理由 —— 它是可以被一条命令行开关删掉的。'

# ============================================================================
banner 7 "traceback 怎么读，以及怎么不靠肉眼拿到它"
# ============================================================================
runc 'python3 f_traceback.py'
note '↑ 一句话：读 traceback 只看最后一行 + 最深那一帧；要程序化处理就用 traceback.extract_tb，'
note '  帧上有 .name 和 .line，别去 split 那段文本。'

# ============================================================================
banner 8 "报错原文集：这一节当字典查（看到什么往哪查）"
# ============================================================================
runc 'python3 g_reading.py'
note '↑ 本节没有新机制，全是在**本机现场构造**这些报错并把原文抄下来。'
note '  最有用的两条判据：TypeError 查形状 / ValueError 查值；「Did you mean」是模糊匹配不是结论。'

# ============================================================================
banner 9 "自定义异常：继承谁、带不带 payload、能不能 pickle"
# ============================================================================
runc 'python3 h_custom.py'
note '↑ 三条判据就够：要接住标准口径就挂个标准父亲；纯自家错继承 Exception；'
note '  带数据就显式挂字段，别指望 args 帮你带。'

# ============================================================================
banner 10 "with 是什么协议：手写一遍 __enter__ / __exit__"
# ============================================================================
runc 'python3 i_with_protocol.py'
note '↑ 本节第 4 小段是整章最危险的一行：__exit__ 返回**真值**就咽掉异常，写字符串也算真值。'
note '  第 5 小段是它的孪生坑：__enter__ 自己崩了，__exit__ 根本不会被调用。'

# ============================================================================
banner 11 "多个 with：逗号堆、嵌套、括号写法，和那条后进先出的顺序律"
# ============================================================================
runc 'python3 j_with_more.py'
note '↑ enter 左→右、exit 右→左，和栈一样；第 k 个 enter 失败时前 k-1 个照样被还。'
note '  还有一条容易被忽略：__exit__ 收到什么，取决于异常有没有**穿出这一层**。'

# ============================================================================
banner 12 "@contextmanager：一个 yield 把函数劈成 enter 和 exit"
# ============================================================================
runc 'python3 k_contextlib.py'
note '↑ 三条纪律：yield 之后必须 try/finally；一次 with 一个新对象；想咽异常就别默认接。'
note '  第 4 小段那个「复用」的报错原文是 AttributeError 不是 RuntimeError —— 别看猜。'

# ============================================================================
banner 13 "contextlib 的四个现成件：suppress / closing / nullcontext / redirect_stdout"
# ============================================================================
runc 'python3 l_std_tools.py'
note '↑ 第 1 小段把 suppress 的**源码**打出来了：它就是第 12 节那个形状，6 行 Python。'
note '  第 2 小段那条要记牢：suppress(BaseException) 会把 Ctrl-C 一起咽掉，永远别写。'

# ============================================================================
banner 14 "ExitStack：资源个数运行时才知道"
# ============================================================================
runc 'python3 m_exitstack.py'
note '↑ callback 和 push 的区别写在各自的 docstring 里（本节把它打印出来了）：'
note '  callback **不能**咽异常，push 收的 __exit__ 签名函数能。别记反。'

# ============================================================================
banner 15 "计时：一次 try 值多少钱、一次真抛值多少钱"
# ============================================================================
runc 'python3 n_cost.py'
note '↑ 这一节每跑必抖，只有**倍率**可读，口径全写在脚本里（timeit + 固定次数）。'
note '  能复现的顺序是：不进 except 的 try ≈ 免费 ＜ 真的抛 ＜ 深栈里抛 ＜ format 整份 traceback。'

# ============================================================================
banner 16 "静悄悄杀手：那些**不报错**的错"
# ============================================================================
runc 'python3 o_silent.py'
note '↑ 本节每一条的共同点是「下一行照常执行」。第 5 小段那个 exit(0) 是本章最贵的一条：'
note '  脚本、CI、make 全都只看退出码，你打印了「出错了」却退 0，就是向上游撒谎。'

# ============================================================================
banner 17 "异常走到程序边界：退出码、atexit、os._exit、线程里的异常"
# ============================================================================
runc 'python3 p_process.py'
note '↑ 四条边界事实：未捕获 = 退 1 + 全文进 stderr；sys.exit 就是 raise SystemExit；'
note '  atexit 在 finally 之后且逆序；子线程的未捕获异常**不改变**进程退出码。'

# ============================================================================
banner 18 "with 落到真实文件上：半截文件、原子替换、一次性的文件迭代器"
# ============================================================================
note '这一节会在沙箱当前目录里真建真删文件（files-demo/，跑完自己 rmtree）。'
runc 'python3 q_files.py'
note '↑ with open 管的是 close，不是事务。配置文件只有「同目录临时文件 + os.replace」一种安全写法。'
note '  第 6 小段那条和数据结构直接相关：文件句柄是**迭代器**，第二次读它就是空，还不报错。'

# ============================================================================
banner 19 "capstone：把本章零件装成一个真工具（安全读配置）"
# ============================================================================
runc 'python3 r_capstone.py'
note '↑ 这个工具的每一行都能回指本章某一节，对照表在它自己最后一段里打出来了。'
note '  注意第 6 段：重试只给「可能是瞬时」的那一支，ENOENT 重试三遍只是浪费 3 次系统调用。'

# ============================================================================
banner 20 "对账：这 19 个脚本是不是真按规矩写的，以及一条守卫怎么证明它会响"
# ============================================================================
runc 'python3 s_checklist.py'
note '↑ 第 2 段那张表就是本章 README 里「19 个脚本 / 139 个小节」这两个数字的出处 —— 数字是脚本数出来的，不是人写的。'
note '  第 3 段是这一节真正的看点：它现场造三个**不合格**的假脚本，让三条判据各响一次，'
note '  再放一个合格样本证明它不误报。这就是第 16 节「不响的检查等于没检查」的正面写法。'
note '  它什么时候退非 0？往参数里塞一个**不是取证脚本**的文件，闸门当场就响（下面只留不合格那一行，完整表上一条已经打过了）：'
runc 'out=$(python3 s_checklist.py demo.sh); echo "退出码 = $?（非 0 = 能当 CI / make check 用）"; grep -F demo.sh <<<"$out"'

# ============================================================================
banner 21 "自证：沙箱里的事只留在沙箱，章目录一个字都没多"
# ============================================================================
note '先把「该有什么」写成一条能判的式子，否则下面那些 0 只是「看着干净」：'
note '  本章的源码只有 *.py 和 demo.sh 两种；README.md 是文档（跑之前有没有它都不该影响结论，'
note '  所以从清单里显式放过）；除此之外多出来的任何文件都算 demo 产物 —— 本节数的就是它。'
note 'demo 的写操作全部发生在 mktemp -d 的沙箱里；此刻的当前目录就是沙箱 work/。'
runc 'echo "沙箱 work/ 里的项数（不含 README.md）=$(ls | grep -vc "^README.md$")，前 12 个："; ls | grep -v "^README.md$" | head -12'
note '↑ 每一项计数都得能响（第 20 节那条纪律用在自己身上）。先给它一个正对照：'
runc 'touch 残留.tmp; echo "塞进一个假产物之后，非源码产物数=$(find . -maxdepth 1 -type f ! -name "*.py" ! -name "demo.sh" ! -name "README.md" | wc -l) ← 会响"; rm 残留.tmp'
runc 'echo "清掉之后：非源码产物数=$(find . -maxdepth 1 -type f ! -name "*.py" ! -name "demo.sh" ! -name "README.md" | wc -l)  残留 .tmp 个数=$(find . -name "*.tmp" | wc -l)  残留子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)"'
note '↑ 第 18 节建过 files-demo/、第 19 节建过 capstone-demo/、第 20 节建过 check-demo/，这里三个子目录都是 0。'
note '__pycache__ / *.pyc 这一对也先弄响一次：随便 import 一个模块，Python 就会把这两样造出来。'
runc 'printf "x = 1\n" > probe_mod.py; python3 -c "import probe_mod"; echo "import 之后：__pycache__ 个数=$(find . -name __pycache__ | wc -l)  *.pyc 个数=$(find . -name "*.pyc" | wc -l) ← 会响"; rm -rf __pycache__ probe_mod.py'
note '↑ 顺带钉一条：只有 **import** 才产 __pycache__；`python3 xxx.py` 直接跑脚本不产 —— 本章全程跑脚本。'
runc 'echo "本章跑完之后：__pycache__ 个数=$(find . -name __pycache__ | wc -l)  *.pyc 个数=$(find . -name "*.pyc" | wc -l)"'
note '章目录（源文件本体在这里）。清单显式排除 README.md，所以「README 写完之后重跑」得到的是同一份输出：'
runc_at "$HERE" 'find . -maxdepth 1 -type f ! -name "README.md" | sort'
runc_at "$HERE" 'echo ".py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)（这 19 个是源码本身，不是产物）  非源码产物数=$(find . -maxdepth 1 -type f ! -name "*.py" ! -name "demo.sh" ! -name "README.md" | wc -l)  子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)"'
note '这一章一条网络命令都没用、没起过任何监听端口。本机在听的端口只打印**条数**——端口号和本章无关，不该写进教材：'
runc 'if command -v ss >/dev/null 2>&1; then echo "本机 LISTEN 条目数=$(ss -lnt | grep -c LISTEN)"; else echo "ss 本机没有（那就更没得起）"; fi'
runc 'pgrep -a sleep | head -3; echo "（上面这一行为空 = 系统里没有 demo 留下的后台进程）"'
note '★ 本章的取证纪律：所有「故意失败」都由 try/except 在脚本里接住并原样打印；'
note '  临时目录全在 /tmp/py07-XXXXXX，由 trap cleanup EXIT 整份删掉。'

printf '\n%s[全部跑完]%s 上面每一段都是真实执行结果；README.md 引用的输出全部出自这一次运行。\n' "$B" "$R"
printf '[一句话回顾] 异常是**控制流的另一半**：接得窄、要么记要么抛、包装就写 from、\n'
printf '              with 只是「必然执行的清理」，而 __exit__ 返回真值会把异常就地抹掉。\n'
