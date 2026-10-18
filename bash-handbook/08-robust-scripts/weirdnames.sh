#!/usr/bin/env bash
# ============================================================================
# weirdnames.sh —— 名字里带空格 / 中文 / 换行 / 前导 - 时，怎么一路走完整条链而不丢
#
#   demo.sh 里这样调：bash weirdnames.sh
#   安全：只在当前目录建 wn/（放怪名字）和 wdata/（放台账）两个子目录，自己造自己删；
#         不碰 ~、不写绝对 /tmp、不联网、不装包、不 kill、不起后台进程。
#   故意不开 set -e：这一章要看的就是「rc=0 但事情没做成」的形状，脚本必须活到收账那一行。
#
# ★ 这一章不把「加引号就对了」当口号：每一格只报**实测个数**（造了几个 / 循环几段 /
#   内层切几段 / 外部命令收到几条 / 删完剩几个），坏写法与正解相邻两跑，数字差就是证据。
#   链路的形状是真实的：父脚本数组 → "${arr[@]}" 交出去 → 内层用 "$@" 接 → 循环 / 外部命令。
#   计数一律用 NUL（find -print0 | tr -dc '\0' | wc -c）而不是 ls -A | wc -l ——
#   wc -l 数的是换行符，而这批名字里恰好有一个自带换行，第 1 格当场把两个数都打出来。
#   工具报错原文一字不改（中文还是英文跟着 LC_ALL 走，见 demo 第 1 节）；屏幕上只出现 wn/ 这种相对路径，不出现沙箱绝对路径。
# ============================================================================

rm -rf wn wdata
mkdir -p wn wdata
trap 'rm -rf wn wdata' EXIT

n() { find wn -maxdepth 1 -type f -print0 | tr -dc '\0' | wc -c; }
mk4() {
    touch -- "wn/有 空格.txt" "wn/中文 名.txt" "wn/-dash.txt"
    touch -- "wn/$(printf '带\n换行.txt')"
}
ledger() { printf '%s\n' "$1" >> wdata/rcs.txt; }
run_arr() {
    cmd=$1
    shift
    printf '  内层敲的是：%s\n' "$cmd"
    printf '  交出去的名字：%s 个 →%s\n' "$#" "$(printf '|[%s]' "$@")"
    out=$(bash -c "$cmd" _ "$@" 2>&1)
    code=$?
    if [ -n "$out" ]; then printf '%s\n' "$out" | sed 's/^/    /'; else printf '    （一声没出）\n'; fi
    printf '  rc=%s\n' "$code"
    ledger "$code"
}

echo '=== 1. 先造四个怪名字，并先证明「数出来几个」这件事本身数得对 ==='
b1=$(n)
mk4
a1=$(n)
printf '  造之前 wn/ 里 %s 个文件，造之后 %s 个 —— 计数函数会跟着事实变，不是恒零的摆设\n' "$b1" "$a1"
printf '  同一批换成 ls -A | wc -l 来数，得到 %s 个\n' "$(ls -A wn | wc -l)"
printf '  → 直觉以为「%s 和 %s 里必有一个写错了」：两个都没错 —— wc -l 数的是**换行符**，\n' "$a1" "$(ls -A wn | wc -l)"
printf '    而这四个名字里有一个自带换行，它一行变两行。要「几条记录」就得数 NUL。\n'
printf '  ★ 四个名字分别是：带空格、中文里带空格、自带换行、以 - 开头。下面每一格拿它们跑全链。\n'

echo '=== 2. 数组 → 循环：加引号走 4 次，去掉引号走 7 次 ==='
arr=("有 空格.txt" "中文 名.txt" "$(printf '带\n换行.txt')" "-dash.txt")
c0=0
for f in "${arr[@]}"; do c0=$((c0 + 1)); done
c1=0
for f in ${arr[@]}; do c1=$((c1 + 1)); done
printf '  for f in "${arr[@]}"      → 循环体走 %s 次（数组元素个数 ${#arr[@]} = %s）\n' "$c0" "${#arr[@]}"
printf '  for f in ${arr[@]}（去引号）→ 循环体走 %s 次，比上面多 %s 段\n' "$c1" "$((c1 - c0))"
printf '  多出来的段长这样（循环体实际拿到的名字）：%s\n' "$(for f in ${arr[@]}; do printf '[%s]' "$f"; done)"
printf '  → 直觉以为「少个引号顶多多跑几次，无所谓」：拿到的是「有」「空格.txt」这种半截名字 ——\n'
printf '    它不报错，只是安静地对着并不存在的文件干活。\n'
printf '  ★ 元素里带空格还是换行都不影响 ${#arr[@]}（%s）；只有**展开时没引号**才会被 IFS 和换行劈开。\n' "${#arr[@]}"

echo '=== 3. 交给外部命令：内层用 "$@" 接 与 用 $@ 接 ==='
q=$(bash -c 'echo $#' _ "${arr[@]}")
qp=$(bash -c 'printf "一个名字 →[%s]\n" "$@"' _ "${arr[@]}" | wc -l)
u=$(bash -c 'k=0; for a in $@; do k=$((k + 1)); done; echo $k' _ "${arr[@]}")
run_arr 'echo "内层参数个数 = $#"; for a in "$@"; do echo "  一个名字 →[$a]"; done' "${arr[@]}"
run_arr 'k=0; for a in $@; do k=$((k + 1)); echo "  一段 →[$a]"; done; echo "切成 $k 段"' "${arr[@]}"
printf '  → 内层参数个数 %s，可「一个名字 →[…]」在屏幕上占了 %s 行 —— 那个自带换行的名字是**一个**参数。\n' "$q" "$qp"
printf '  ★ 同一批名字、同一种交法，内层只把 "$@" 换成 $@，参数就当场变 %s 段（父侧的引号白加了）。\n' "$u"

echo '=== 4. "$*" 与 "$@"：一个拼成一根串，一个永远是多个参数 ==='
printf '  默认 IFS 下 "${arr[*]}" =[%s]\n' "${arr[*]}"
IFS=:
printf '  把 IFS 改成冒号，同一条 "${arr[*]}" =[%s]\n' "${arr[*]}"
m4=$(set -- "${arr[@]}"; echo $#)
printf '  同一个 IFS 下 "${arr[@]}" 交给 set --，参数个数还是 %s\n' "$m4"
unset IFS
w4=$(set -- ${arr[@]}; echo $#)
printf '  默认 IFS 下**去掉引号** set -- ${arr[@]} → %s 个\n' "$w4"
printf '  → "$*" 按 IFS 把名字**粘成一根串**：默认 IFS 里本来就含空格，所以第一格看着像对的，那是运气；\n'
printf '    IFS 一变（第二格的冒号），粘合剂跟着变，名字边界当场没了。\n'
printf '  ★ "$@" 不受 IFS 影响（第三格仍是 %s 个）：只要这批名字还可能被再展开一次，就用 "$@"。\n' "${#arr[@]}"

echo '=== 5. 把 IFS 换成换行符的错觉：4 个名字照样数出 5 段，还顺手改掉了别的展开 ==='
list=$(printf '%s\n' "${arr[@]}")
d0=0
for f in $list; do d0=$((d0 + 1)); done
sp='a b  c'
set -- $sp; s1=$#
IFS=$'\n'
d1=0
for f in $list; do d1=$((d1 + 1)); done
set -- $sp; s2=$#
unset IFS
nf=$(n)
printf '  list = 命令替换的结果（一行一个名字）。默认 IFS：for f in $list → %s 段\n' "$d0"
printf '  改成 IFS=换行：同一条循环 → %s 段，而 wn/ 里真实文件数是 %s —— 两头都不对\n' "$d1" "$nf"
printf '  → 直觉以为「按行分就稳了」：那个自带换行的名字本身就跨两行，按行数必然多算一段。\n'
printf '  ★ 代价是**全局**的：同一串 sp（a b 两个空格 c），默认 IFS 下 set -- $sp → %s 个，\n' "$s1"
printf '    设了 IFS=换行之后同一条 → %s 个。为一处改 IFS，脚本里后面所有未加引号的展开都跟着换规则。\n' "$s2"

echo '=== 6. find | xargs：不带 -0 收到 7 条，带 -0 收到 4 条 ==='
bads=$(find wn -maxdepth 1 -type f -print | xargs printf '收到[%s]\n' 2>/dev/null)
badrc=$?
bad=$(printf '%s\n' "$bads" | grep -c '收到')
good=$(find wn -maxdepth 1 -type f -print0 | xargs -0 printf '收到[%s]\n' | grep -c '收到')
tshow=$(find wn -maxdepth 1 -type f -print | xargs -t -n1 /bin/true 2>&1 | LC_ALL=C sort)
tnc=$(printf '%s\n' "$tshow" | grep -c '/bin/true')
lost=$(printf '%s\n' "$tshow" | grep -vc 'wn/')
printf '  find -print  | xargs printf  → 外部命令收到 %s 条，rc=%s（它不报错，只是把名字改了）\n' "$bad" "$badrc"
printf '  find -print0 | xargs -0      → 外部命令收到 %s 条（wn/ 里真实文件数 = %s）\n' "$good" "$nf"
printf '  xargs -t -n1 把它实际执行的每条命令原样吐到 stderr，实测 %s 行：\n' "$tnc"
printf '%s\n' "$tshow" | sed 's/^/    /'
printf '  → 这 %s 行里有 %s 行的名字前面连 wn/ 前缀都没有了 —— 半截名指的不是「不存在」，是**另一个位置**。\n' "$tnc" "$lost"
printf '  ★ 配对写：-print0 必须配 -0。靠「我这批名字里没有空格」这种自律，撑不过同事新建的一个目录。\n'

echo '=== 7. 一路删干净：引号管「不被劈开」，-- 管「不被当选项」，两件事各自独立 ==='
before=$(n)
printf '  删之前 wn/ 实测 %s 个\n' "$before"
run_arr 'cd wn && rm -f "$@"' "${arr[@]}"
rc_a=$code
after_a=$(n)
run_arr 'cd wn && rm -f -- $@' "${arr[@]}"
rc_b=$code
after_b=$(n)
if [ -e "wn/-dash.txt" ]; then d3="还在"; else d3="已经没了"; fi
run_arr 'cd wn && rm -f -- "$@"' "${arr[@]}"
rc_c=$code
after_c=$(n)
printf '  三跑的码：%s / %s / %s，三跑之后的剩余文件数：%s → %s → %s\n' \
    "$rc_a" "$rc_b" "$rc_c" "$after_a" "$after_b" "$after_c"
printf '  → 第一跑撞在 -dash.txt 上：rc=%s，当场报错，一个也没删（看得见，所以好办）；\n' "$rc_a"
printf '    第二跑 rc=%s、一声没出，却只删掉 %s 个，剩下 %s 个原样躺着。\n' "$rc_b" "$((before - after_b))" "$after_b"
printf '    被删掉的正是那个唯一没被劈开的名字 -dash.txt（现在：[%s]）—— 剩下三个带空格的一个没动。\n' "$d3"
printf '  ★ 计数守卫：删之前 %s → 坏写法之后 %s → 正解之后 %s，三个数全是现场量的，不是写死的。\n' "$before" "$after_b" "$after_c"

echo '=== 8. 收账：上面每一次 run_arr 的真实退出码 ==='
printf '  按出场顺序的码：[%s]\n' "$(paste -sd ' ' wdata/rcs.txt)"
total=$(wc -l < wdata/rcs.txt)
z=$(grep -c '^0$' wdata/rcs.txt)
nz=$(grep -vc '^0$' wdata/rcs.txt)
printf '  条数=%s ｜ 码 0 的=%s ｜ 非 0 的=%s ｜ 两个加起来等于条数吗：%s\n' \
    "$total" "$z" "$nz" "$([ "$((z + nz))" -eq "$total" ] && echo 等于 || echo 不等于)"
printf '  → %s 次跑里非 0 的只有 %s 次，其余 %s 次全静默 —— 不报错正是它难查的原因。\n' "$total" "$nz" "$z"
printf '    第 7 格中间那一跑就是这个形状：码 0、屏幕上啥也没说，事情没做成。\n'
printf '  ★ 能带走的一句话：名字从头到尾只该以「数组元素」的形态存在 —— "${arr[@]}" 交出去、"$@" 接住、\n'
printf '    循环里用 "$f"、外部命令前放 --、跨进程用 -print0 配 -0；中间任何一步退回字符串，\n'
printf '    就得赌这批名字里没有空白 —— 而这一格刚证明：赌输的时候不报错。\n'
exit 0
