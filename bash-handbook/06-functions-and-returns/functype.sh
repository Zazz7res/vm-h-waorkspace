#!/usr/bin/env bash
# ============================================================================
# functype.sh —— 函数到底怎么声明、这个名字最后被解析成了谁
#
#   demo.sh 里这样调：bash functype.sh
#   本章的立论是「函数只能还你一个退出码」，但那之前得先把一件事钉死：
#   `f(){ }`、`function g { }`、`function h(){ }` 三种写法进的是**同一张表**，
#   写法本身不留痕；而「跑起来到底是谁」由查找顺序决定 —— 函数压过文件、
#   内建也压不过函数。所以一个名字的真实身份只有一个问法能得到全部来源：type -a。
#   安全：只在当前目录里建自己的 funcs/ 沙箱（里面放一个**真的可执行文件** date），
#         PATH 的前置只写在需要它的那一条命令上，绝不进脚本环境；
#         跑完第 7 段先显式删掉、再由 trap 兜底；全程不起后台作业、不开端口、不联网、
#         不碰 ~/.bashrc 与 ~/.profile（要看「另一个干净的 shell」一律起子进程）。
#   故意不加 set -e：dash 撞的语法错误、type 查不到名字时的 rc=1 都是本章要看的原文，
#         加了 -e 屏幕上就只剩第一段。也不加 set -u、不加全局 pipefail。
#   ★ 需要英文报错原文的地方，都在**那一条**命令前加 env LC_ALL=C.UTF-8 起子进程，
#     本脚本自己不改 locale（外层 locale 是 zh_CN.UTF-8 时 bash 会说中文）。
#   ★ 屏幕上只印相对路径：系统里那两个同名 date 的全路径不印，只报「来源有几行」。
# ============================================================================

rm -rf funcs
mkdir -p funcs/bin
trap 'rm -rf funcs' EXIT

cat > funcs/bin/date <<'EOF'
#!/usr/bin/env bash
printf '沙箱文件版 date\n'
EOF
chmod 755 funcs/bin/date

# src_count <名字>：type -a 里「X is ...」这种来源行有几行（函数体那些行不带 " is "）。
src_count() { type -a "$1" | grep -c ' is '; }

echo "=== 1. 三种声明写法：进表之后长得一模一样 ==="
d_one() { :; }
function d_two { :; }
function d_three() { :; }
for n in d_one d_two d_three; do
    printf '  %-8s ｜ type -t = %-8s ｜ command -v = %s\n' "$n" "$(type -t "$n")" "$(command -v "$n")"
done
printf '  type -a d_one 的全文（函数体是被 bash 重新打印的，不是你敲的那行）：\n'
type -a d_one | sed 's/^/      /'
body_two=$(type -a d_two | sed 's/d_two/d_one/')
printf '  d_two 的 type -a 把名字换成 d_one 后，和 d_one 的一字不差吗 → %s\n' \
    "$([ "$(type -a d_one)" = "$body_two" ] && echo 是 || echo 否)"
printf '  d_three 同理 → %s\n' \
    "$([ "$(type -a d_one)" = "$(type -a d_three | sed 's/d_three/d_one/')" ] && echo 是 || echo 否)"
echo "  → 三种写法在 bash 眼里是同一样东西：type -t 一律 function，command -v 一律只回名字本身。"
echo "    区别只在「能不能被 sh 读懂」（第 6 段撞给你看）。"
echo "  ★ command -v 对函数只回名字、不回内容 —— 它答的是「会不会被找到」，不是「它是谁」。"

echo
echo "=== 2. 一个名字有几种身份：type -t 给分类，type -a 给全部来源 ==="
printf '  %-14s type -t = %-9s ｜ type -a 来源行数 = %s\n' "[函数 d_one]" "$(type -t d_one)" "$(src_count d_one)"
printf '  %-14s type -t = %-9s ｜ command -v = %s\n' "[内建 printf]" "$(type -t printf)" "$(command -v printf)"
printf '  %-14s type -t = %-9s ｜ command -v = %s\n' "[关键词 if]" "$(type -t if)" "$(command -v if)"
printf '  %-16s type -t = %-9s ｜ type -a 来源行数 = %s\n' "[文件(前置 PATH)]" \
    "$(PATH=./funcs/bin:$PATH type -t date)" "$(PATH=./funcs/bin:$PATH src_count date)"
notfound=$(env LC_ALL=C.UTF-8 bash -c 'type -a nosuchname_xyz' 2>&1); nrc=$?
printf '  %-14s type -t = [%s]（空串）｜ 查不到时 type -a 的原文 =[%s] rc=%s\n' \
    "[不存在]" "$(type -t nosuchname_xyz)" "$notfound" "$nrc"
echo "  → 想知道「我这么写，跑起来到底是谁」，只有 type -a 一次给全：它是 bash 内建的招牌，"
echo "    函数排第一，后面按 PATH 顺序列文件；type -t 只给一个词，command -v 只给一个名字。"
echo "  ★ type -a 是 bash 的开关，别家 shell 不认（第 6 段有 dash 的原文）—— 脚本里查身份可以靠它，"
echo "    跨 shell 的兼容代码不能靠它。"

echo
echo "=== 3. 同名遮蔽现场（上）：PATH 前置 ./funcs/bin，只借给这一条命令 ==="
printf '  不带前置时，date 的来源行数 = %s（系统里本来就有 2 个同名文件，全路径不印）\n' "$(src_count date)"
printf '  带上前置后，date 的来源行数 = %s ← 多出来的那行是 ./funcs/bin/date\n' \
    "$(PATH=./funcs/bin:$PATH src_count date)"
printf '  PATH=./funcs/bin:$PATH type -t date     → %s\n' "$(PATH=./funcs/bin:$PATH type -t date)"
printf '  PATH=./funcs/bin:$PATH command -v date  → %s\n' "$(PATH=./funcs/bin:$PATH command -v date)"
printf '  PATH=./funcs/bin:$PATH date             → %s\n' "$(PATH=./funcs/bin:$PATH date)"
printf '  裸跑 date（不带前置）的来源第一行 → %s\n' "$(type -a date | head -1)"
echo "  → 只把一个相对目录放到 PATH 最前，就换掉了「date」这个词的执行体；"
echo "    而且第 3 行那种写法（PATH=... 只挂在一条命令前面）不会污染脚本自己的环境。"

echo
echo "=== 4. 同名遮蔽现场（下）：把同名函数定义上，PATH 就再也追不上了 ==="
date() { printf '函数版 date\n'; }
printf '  加了同名函数后 type -t date → %s ｜ command -v date → %s\n' "$(type -t date)" "$(command -v date)"
printf '  不带前置实跑   → %s\n' "$(date)"
printf '  带前置实跑     → %s（★ 同一个答案：改 PATH 追不上函数）\n' "$(PATH=./funcs/bin:$PATH date)"
printf '  现在 type -a date 的来源行数 = %s（1 个函数 + 3 个文件）｜ 第一行 = %s\n' \
    "$(src_count date)" "$(type -a date | head -1)"
printf '  把函数删掉 → type -t 变回 %s ｜ 实跑变回 %s\n' \
    "$(unset -f date; type -t date)" "$(PATH=./funcs/bin:$PATH date)"
echo "  → 顺序是「函数 > 内建 > PATH 里的文件」：前一段还是 file，定义同名函数的一行之后立刻变 function，"
echo "    文件一个都没动。第 5 段把内建也拉进来对一次。"

echo
echo "=== 5. 内建也压不过函数（这段在 bash -c 子进程里跑，本脚本的 printf/true 不受影响） ==="
env LC_ALL=C.UTF-8 bash -c '
    true() { printf "函数版 true\n"; }
    printf "  type -t true      → %s（原本是 builtin）\n" "$(type -t true)"
    printf "  裸 true           → [%s]\n" "$(true)"
    printf "  command true      → [%s]（★ 空：绕过函数打到内建，内建本来就不打印）\n" "$(command true)"
    printf "  builtin true      → [%s]（同上，另一个逃生口）\n" "$(builtin true)"
    printf "  type -a true 来源行数 = %s（1 函数 + 1 内建 + 2 文件）｜ 第一行 = %s\n" \
        "$(type -a true | grep -c " is ")" "$(type -a true | head -1)"
    printf "  被顶掉之后 && 还成立吗 → %s\n" "$(true >/dev/null; [ $? = 0 ] && echo 成立 || echo 不成立)"
'
echo "  → 内建可以被同名函数整个换掉，而且换掉之后 `&&` / `if` 看到的是**函数**的退出码；"
echo "    上面最后一行是「函数 return 0」所以还成立 —— 换个 return 1 的内建名就是无声的分支反转。"
echo "  ★ 这就是为什么手册要求脚本里的函数名带前缀（本段用 true 是故意的事故现场，只在子进程里活着）。"

echo
echo "=== 6. 同一份函数定义文件在 /bin/sh 里撞的原文（本机 /bin/sh → $(readlink /bin/sh)） ==="
printf 'd_posix() { :; }\n'          > funcs/form_posix.sh
printf 'function d_kw { :; }\n'      > funcs/form_kw.sh
printf 'function d_paren() { :; }\n' > funcs/form_paren.sh
printf 'd_posix() { :; }\nfunction d_kw { :; }\nfunction d_paren() { :; }\n' > funcs/defs.sh
for n in posix kw paren; do
    bash -c "exit 0" >/dev/null 2>&1
    err=$(env LC_ALL=C.UTF-8 bash "funcs/form_$n.sh" 2>&1); brc=$?
    err2=$(env LC_ALL=C.UTF-8 sh "funcs/form_$n.sh" 2>&1); src=$?
    printf '  %-20s bash rc=%s ｜ sh rc=%s ｜ sh 原文=[%s]\n' "form_$n.sh" "$brc" "$src" "$err2"
    [ -n "$err" ] && printf '      （bash 侧也报了：[%s]）\n' "$err"
done
derr=$(env LC_ALL=C.UTF-8 sh funcs/defs.sh 2>&1); drc=$?
bnerr=$(env LC_ALL=C.UTF-8 bash funcs/defs.sh 2>&1); bnrc=$?
printf '  三种写法合在一个 funcs/defs.sh 里：bash rc=%s（原文=[%s]）｜ sh rc=%s ｜ 原文=[%s]\n' \
    "$bnrc" "$bnerr" "$drc" "$derr"
nerr=$(env LC_ALL=C.UTF-8 sh -n funcs/defs.sh 2>&1); nrc2=$?
printf '  同一个文件 sh -n（只做语法检查、一条都不执行）：rc=%s ｜ 原文=[%s]\n' "$nrc2" "$nerr"
taerr=$(env LC_ALL=C.UTF-8 sh -c 'type -a echo' 2>&1); tarcs=$?
printf '  顺带一句：dash 里没有 type -a 这个开关，原文=[%s] rc=%s\n' "$taerr" "$tarcs"
echo "  → 报错停在第 2 行：`function d_kw { :; }` 那一种。dash 只认 `名字() { }` 这一种声明，"
echo "    两种 `function` 写法都过不去；而 bash 两种都认 —— 所以 `bash x.sh` 正常、`sh x.sh` 崩。"
echo "  ★ 这就是「shebang 要写对」的物证：同一份文件，`sh -n` 在语法检查阶段就 rc=2，"
echo "    连一行都没跑。写 `function` 就得钉死 bash（`#!/usr/bin/env bash`），要能 `sh` 跑就只能用 `名字() { }`。"

echo
echo "=== 7. 收口：判据 + 当场自证 ==="
rm -rf funcs
printf '  自证：沙箱目录 funcs 还在吗 → %s（trap 只是兜底，这里显式删完再问）\n' \
    "$([ -d funcs ] && echo 在 || echo 不在了)"
printf '  自证：本脚本的 PATH 里留下过 ./funcs/bin 吗 → %s\n' \
    "$([[ :$PATH: == *":./funcs/bin:"* ]] && echo 留下了 || echo 没留（前置只挂在单条命令上）)"
printf '  自证：主进程作业表 jobs -p 行数 = %s ｜ 本脚本全程没用过 &\n' "$(jobs -p | wc -l)"
echo "     （这个 0 是在脚本自己的进程里问的；去 bash -c 子进程里问它天生是空表，那个 0 是假的。）"
echo "  ① 判据一：查一个名字的真实身份，只用 type -a —— 它一次给全部来源，且第一行就是会被执行的那个；"
echo "     type -t 只给你一个词，command -v 只给你一个名字，两者都会把「函数顶掉了谁」这件事藏起来。"
echo "  ② 判据二：写函数就写 `名字() { }`。`function` 前缀在 bash 里和它等价，但出了 bash 就是语法错误；"
echo "     要留住 bash 专属写法，就把 shebang 写成 bash，别让人家用 sh 跑你的脚本。"
echo "  ③ 判据三：函数 > 内建 > PATH 里的文件。同名遮蔽不是「谁的路径近」，是「谁在表里」；"
echo "     调试时先 `type -a 那个名字`，再决定是 unset -f 还是改 PATH。"
echo "  ④ 判据四：给脚本里的函数起名要带前缀（本手册用 d_ 打头）—— 内建和 /usr/bin 里的命令就那么多，"
echo "     随手一个 true / date / test 就撞上了，而且撞上了不报错。"
