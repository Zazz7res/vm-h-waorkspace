#!/usr/bin/env bash
# ============================================================================
# localscope.sh —— bash 是**动态作用域**：local 的影响范围比你以为的大
#
#   demo.sh 里这样调：bash localscope.sh
#   安全边界：只在**当前目录**里开 scope/ 子目录，里面放三段「只有另起进程才敢跑」的小脚本
#             （函数外面写 local、自引用的 nameref、指向已返回局部的 nameref）；跑完由
#             trap 'rm -rf scope' EXIT 删掉。不起后台进程、不开端口、绝不碰 ~/.bashrc、
#             ~/.profile、~/.config 这类用户文件 —— 作用域全在**当前这个 shell 进程**里谈，
#             要看「另一个干净 shell」一律 bash scope/xxx.sh 起子进程，开关只活在那条命令里。
#   故意不加 set -e：第 5、7 段要演的正是「bash 报了错却不停下」；一崩后面的现场就没了。
#
# 本章立场：函数只能还你一个退出码，想带回别的东西只有三条通道。这一支演通道二（全局变量）
#   和通道三（nameref），并量出各自的代价：
#     · 全局变量这条通道的代价是**命名** —— 函数里每一句没 local 的赋值都进了公共命名空间；
#     · nameref 这条通道的代价是**撞名** —— 它按名字找变量，而名字在调用栈上会重复出现。
#   local 之所以比「少用全局」更根本：它是这两条通道共同的闸门。
#   通道一（stdout）见 errmix.sh；return 与 0~255 见 retcode.sh。
# ============================================================================

rm -rf scope
mkdir -p scope
trap 'rm -rf scope' EXIT

echo "=== 1. local 的基本面：里面写的外面看不见，函数里漏写的就是全局 ==="
g_demo=外面本来就有的全局
inside_only() {
    local only_here=只活在函数里的值
    printf '    函数里读 only_here=[%s]\n' "$only_here"
}
printf '  原文：inside_only() { local only_here=只活在函数里的值; ... } ；然后 inside_only\n'
inside_only
printf '  函数返回后：only_here 还在吗 → %s ｜ g_demo 还在吗 → %s（值=[%s]）\n' \
    "$([[ -v only_here ]] && echo 在 || echo 不在)" "$([[ -v g_demo ]] && echo 在 || echo 不在)" "$g_demo"
leaky() { forgot_local=函数里忘了写local; }
leaky
printf '  ★ 代价：函数里赋值不写 local，它就是全局 —— leaky 跑完，外面直接读到 forgot_local=[%s]\n' "${forgot_local-}"
printf '  → 判据：local 不是代码风格，是「别往公共命名空间乱扔名字」唯一的开关；一个函数里有几行\n'
printf '    裸赋值，就有几个可能撞名的全局。第 4 段会看到这句话反过来也咬人（下层挡住不了上层，上层也拦不住下层）。\n'

echo
echo "=== 2. 遮蔽与恢复：把这一层撤掉之后，外面看见的还是原来那个吗 ==="
x=外面原来的值
shadow() { local x=函数内的新值; printf '    函数里 x=[%s]（同名的全局被挡在门外）\n' "$x"; }
printf '  调用前：外面 x=[%s]\n' "$x"
shadow
printf '  返回后：外面 x=[%s] ｜ 和调用前一模一样吗 → %s\n' \
    "$x" "$([ "$x" = "外面原来的值" ] && echo 是 || echo 否)"
unsetter() {
    local x=函数内的新值
    unset x
    printf '    函数里 unset 之后：x=[%s] ｜ [[ -v x ]] → %s ← ★ 没有「露出下面那个全局」这回事\n' \
        "${x-（空）}" "$([[ -v x ]] && echo 有 || echo 无)"
}
unsetter
printf '  返回后：外面 x=[%s] ｜ 还在吗 → %s\n' "$x" "$([[ -v x ]] && echo 在 || echo 不在)"
echo "  → 两句记牢：local 只在函数活着的那段时间挡全局；在函数里 unset 一个 local 是把这一层**整个抹掉**"
echo "    （当场读它是空的、-v 也说无），不是「揭开盖子露出全局」；函数一返回，被遮蔽的那个全局原封不动回来。"

echo
echo "=== 3. ★ 动态作用域：inner 一个 local 都没写，却读得到、改得动 outer 的那个 x ==="
x=全局那个x
peek() { printf '      现在 peek 读到的 x=[%s]\n' "$x"; }
poke() { x=被poke改掉; printf '      poke 把当下最近的 x 写成了 [%s]\n' "$x"; }
outer() {
    local x=外层函数的那个x
    printf '    outer 里 local x=[%s]，然后调用 peek、poke（它俩谁都不认识 outer）\n' "$x"
    peek
    poke
    printf '    poke 回来之后，outer 自己的 x=[%s] ← ★ 被一个「什么都没声明」的函数改了\n' "$x"
}
printf '  ① 先在顶层问一次 peek：\n'
peek
printf '  ② 同样这两个函数，这次是从 outer 里调下去的：\n'
outer
printf '  ③ outer 返回后，最外面那个全局 x=[%s] ｜ 一点没被改吗 → %s\n' \
    "$x" "$([ "$x" = "全局那个x" ] && echo 是 || echo 否)"
printf '  ④ 再把 poke 拿到顶层调一次（这次它手上没有别的 x 了）：\n'
poke
printf '     全局 x=[%s] ← 同一个 poke，两次改的是两个不同的变量\n' "$x"
echo "  → ★ 判据（这一章最要紧的一句）：bash 的作用域是按**调用栈**找的，不是按函数的定义位置找的。"
echo "    outer 的 local x 对所有「被 outer 调下去的函数」都可见、可写 —— 这就叫动态作用域。"
echo "    代价：底层函数里一句随手赋值，改掉的可能是几十层之上某个函数正在用的变量；而它读到什么"
echo "    取决于「谁调了它」，同一个函数换个调用者行为就变 —— 这类 bug 盯着函数本身看是看不出来的。"
x=全局那个x

echo
echo "=== 4. 反面：被调用者自己写一句 local，这条链当场断掉 ==="
leaf_shield() {
    local x
    printf '      leaf_shield 里刚写完 local x（还没赋值）：x=[%s] ｜ [[ -v x ]] → %s\n' \
        "${x-（空）}" "$([[ -v x ]] && echo 有 || echo 无)"
    printf '      → 读到的是空，不是上层的「中间层的x」：这一层已经存在了，只是没值\n'
    x=leaf_shield自己的
    shielded_after=$x          # ★ 这句本身就在往全局写，正是第 1 段说的那个动作，这里故意破一次规矩好把值拿出来印
}
leaf_open() {
    printf '      leaf_open 里没有 local，读到 x=[%s]\n' "$x"
    x=leaf_open改的
}
mid()  { local x=中间层的x; printf '    mid  的 x 初值=[%s]\n' "$x"; leaf_shield; shielded=$x; printf '    leaf 回来之后 mid 的 x=[%s] ← 没被动过\n' "$x"; }
mid2() { local x=中间层的x; printf '    mid2 的 x 初值=[%s]\n' "$x"; leaf_open;  opened=$x;   printf '    leaf 回来之后 mid2 的 x=[%s] ← 被下层的裸赋值改了\n' "$x"; }
mid
mid2
printf '  对照（同名字、同结构，只差被调用者那一句 local）：\n'
printf '     挡了的那一层结束时 x=[%s] ｜ 没挡的那一层结束时 x=[%s] ｜ 两者相等吗 → %s\n' \
    "$shielded" "$opened" "$([ "$shielded" = "$opened" ] && echo 相等 || echo 不相等)"
printf '     顺带把两个变量本身也比一下：shielded=[%s] 是 mid 自己的值吗 → %s ｜ opened=[%s] 是 leaf 写的吗 → %s\n' \
    "$shielded" "$([ "$shielded" = "中间层的x" ] && echo 是 || echo 否)" \
    "$opened" "$([ "$opened" = "leaf_open改的" ] && echo 是 || echo 否)"
echo "  → ★ 判据：被调用者挡不住上层传下来的同名变量，除非自己声明 local；反过来，上层也拦不住"
echo "    下层往全局里造名字（第 1 段的 leaky、第 3 段的 poke）。所以规矩只有一条："
echo "    函数里每一个变量，进函数先 local，宁可写多不靠猜。"

echo
echo "=== 5. 函数外面写 local：报错原文，以及它为什么不中断脚本 ==="
cat > scope/notlocal.sh <<'PEOF'
target=外面本来就有这个全局
local target=函数外想遮蔽它
printf '    local 那句报错之后，这一行照样执行了：target=[%s]\n' "$target"
PEOF
printf '  现场文件 scope/notlocal.sh 第 2 行是一句裸的 local（下面这条是当前 locale 的原文）：\n'
bash scope/notlocal.sh 2>&1
printf '    退出码=%s ← ★ 报的是错，但不致命：脚本继续跑，遮蔽也没发生，target 还是那个全局\n' "$?"
printf '  同一条命令的英文对照（只在这一步给子进程强制 LC_ALL=C.UTF-8；外层已经是 C.UTF-8 时两行会重合）：\n'
env LC_ALL=C.UTF-8 bash scope/notlocal.sh 2>&1 | sed -n '1p'
echo "  → 判据：local / export / declare 这些 builtin 只在函数体内有意义，写在函数外面 bash 就报"
echo "    「只能在函数中使用」这一句，然后接着跑下一行 —— 所以「反正我加了 set -e」在这里救不了你："
echo "    这一句根本不是让脚本停下来的错，遮蔽也没生效，代码却看起来是对的。"

echo
echo "=== 6. local -n（nameref）的正用：把值回填给调用方，不占用全局 ==="
printf '  原文：f() { local -n _out=$1; _out=计算结果; }   ← 调用方传的是**变量名本身**（不加 $）\n'
square_into() { local -n _res=$1; _res=$(( $2 * $2 )); }
box=占位
square_into box 7
printf '  ① 回填一个调用方已有的全局：square_into box 7 → box=[%s]\n' "$box"
compute() { local -n _out=$1; _out="算完了：$(( 6 * 7 ))"; }
user() { local result=还没算; compute result; printf '  ② 回填调用方的**局部**：user 里的 result=[%s]\n' "$result"; }
user
printf '     函数自己的 _out 在外面存在吗 → %s ｜ _res 呢 → %s ← nameref 是 local，不泄漏\n' \
    "$([[ -v _out ]] && echo 在 || echo 不在)" "$([[ -v _res ]] && echo 在 || echo 不在)"
filler() { local -n _slot=$1; _slot=填进去的值; }
filler slot_from_thin_air          # 传进去的名字之前根本不存在
printf '  ★ 代价：名字不存在时 nameref 不失败，它给你**凭空造一个全局**：\n'
printf '     slot_from_thin_air=[%s] ｜ declare -p 的原文=[%s]\n' \
    "${slot_from_thin_air-}" "$(declare -p slot_from_thin_air 2>&1)"
printf '     顺便：名字写成非法变量名（比如中文）时，本机也只给一句就过 ——\n'
filler 从没出现过的名字 2>&1 | sed 's/^/       /'
printf '       filler 那句的退出码=%s ← 撞名、非法名都不拦，全靠你自己保证\n' "${PIPESTATUS[0]}"
echo "  → 什么时候**才**该用 nameref：你要回填的就是调用方那一个变量，又不想为此开全局、"
echo "    也不想让调用方去抄 stdout。它比 stdout 通道干净在「不留子 shell」（errmix 第 6 段），"
echo "    比全局通道干净在「不白占名字」；但它仍然按**名字**找变量，而名字会在调用栈上重复 —— 下一段两个坑。"

echo
echo "=== 7. ★ local -n 的两个坑：自引用、指向已经返回的局部 ==="
cat > scope/circ.sh <<'PEOF'
caller_var=调用方给的变量名
calc() { local -n caller_var=$1; caller_var=算出来的值; }
user2() { local caller_var=调用方的局部值; calc caller_var; printf '    user2 的局部 caller_var=[%s] ← 一个字节都没收到\n' "$caller_var"; }
user2
printf '    全局 caller_var=[%s]\n' "$caller_var"
PEOF
printf '  坑① 自引用：函数里 nameref 自己的名字（caller_var）和要回填的名字（$1 传进来的 caller_var）撞了。\n'
bash scope/circ.sh 2>&1
printf '     退出码=%s ← ★ 三行 warning 之后 bash 当这事过去了：调用方的局部没收到值，\n' "$?"
printf '       值落到了一个**凭空出现的全局**上（上面最后一行）。\n'
cat > scope/deadref.sh <<'PEOF'
hide() { local secret=只活在函数里; declare -g -n LONG_LIVED=secret; printf '    hide 里透过 nameref 读到 LONG_LIVED=[%s]\n' "$LONG_LIVED"; }
hide
printf '    hide 返回后 LONG_LIVED=[%s] ｜ 判空 → %s ｜ [[ -v LONG_LIVED ]] → %s\n' \
    "$LONG_LIVED" "$([ -z "$LONG_LIVED" ] && echo 空 || echo 非空)" "$([[ -v LONG_LIVED ]] && echo 有 || echo 无)"
printf '    declare -p LONG_LIVED → %s ｜ 它指向的那个 secret 还在吗 → %s\n' \
    "$(declare -p LONG_LIVED 2>&1)" "$([[ -v secret ]] && echo 在 || echo 不在)"
LONG_LIVED=新值
printf '    ★ 再透过它写一次：全局 secret=[%s] ｜ nameref 读到=[%s]\n' "${secret-（本来不存在）}" "$LONG_LIVED"
PEOF
printf '  坑② 引用失效：nameref 比它指向的那个局部活得久（这里用 declare -g 把它挂成全局，好看见后果）。\n'
bash scope/deadref.sh 2>&1
printf '     退出码=%s ← 全程一句报错都没有\n' "$?"
echo "  → 判据：nameref 的入参必须是**调用方那个变量在调用那一刻可见的名字**；函数里的 nameref 名字"
echo "    要避开它可能回填的每一个名字。常见的 _out 前缀只是降低撞名概率，不消除它 —— 真正管用的是"
echo "    「函数里那句 local -n 的名字 ≠ $1 可能传进来的任何名字」，以及回填前先确认目标存在（[[ -v \$1 ]]）。"
echo "    这两个坑共同的形状：不炸、退出码 0，只把你的结果送到别处去，或者从别处读回一个空串。"

echo
echo "=== 8. 收口：数一遍还剩下的全局，再给判据 ==="
printf '  删之前 scope/ 里有 %s 个文件 ｜ 现在执行 rm -rf scope（trap 里是同一句，这里提前跑一遍好自证）\n' \
    "$(ls -A scope | wc -l)"
rm -rf scope
printf '  ① 现场目录 scope/ 还在吗 → %s ｜ ls -A 里名为 scope 的条目数 = %s\n' \
    "$([ -d scope ] && echo 在 || echo 不在了)" "$(ls -A | grep -cx scope)"
printf '  ② 后台进程：本脚本没起过后台作业，在**自己的进程里**复核 jobs -p 行数 = %s\n' "$(jobs -p | wc -l)"
left_names=
for name in g_demo only_here forgot_local x shielded opened box slot_from_thin_air _out _res caller_var LONG_LIVED target; do
    [[ -v $name ]] && left_names="$left_names $name"
done
printf '  ③ 跑完这一支，本 shell 里还剩下的全局 =%s（共 %s 个）\n' "${left_names-（无）}" "$(printf '%s\n' "$left_names" | wc -w)"
printf '     其中 only_here/_out/_res 不在名单里 = local 真的没泄漏；caller_var/LONG_LIVED/target 不在名单里 = 那三段是在**子进程**里撞的，本 shell 一个都没沾上。\n'
echo "  ④ 判据一：bash 是动态作用域 —— 函数里裸写的名字沿**调用栈**往上找，找到谁算谁；"
echo "     底层函数改掉的可能是上层函数的局部，同一个函数换个调用者就是换个结果。"
echo "  ⑤ 判据二：函数里每一个变量都先 local（包括最底层那片叶子）；被调用者不声明就挡不住上层，"
echo "     上层也拦不住下层造全局。函数里 unset 一个 local 是把这层抹掉，不是揭开盖子露出全局。"
echo "  ⑥ 判据三：回填用 local -n 时，函数里的 nameref 名字绝不能等于 $1 可能传进来的名字"
echo "     （本机撞名报 3 行 warning、退出码仍 0、值落到凭空出现的全局上）；nameref 指向的局部一旦返回就失效，"
echo "     读它给空串、连 [[ -v ]] 都说无，再写一次则凭空造一个全局（本支第 7 段实测，bash 5.3.9）。"
echo "  ⑦ 判据四：三条通道的代价各自记牢 —— stdout 要付子 shell（errmix.sh）、全局变量要抢名字（本支）、"
echo "     nameref 要防撞名（本支第 7 段）；退出码那条路永远走 return，它只能带回 0~255，见 retcode.sh。"
