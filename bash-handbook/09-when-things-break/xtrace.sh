#!/usr/bin/env bash
# ============================================================================
# xtrace.sh —— set -x 打的到底是哪一行字：展开「之后」、走 stderr、只给命令替换加层级
#
#   demo.sh 里这样调：bash xtrace.sh
#   安全：只在当前目录建 xt/（几份被演示的小脚本 + 两遍 diff 的对照文件），跑完 trap 整目录删掉。
#         全部用相对文件名跑，不碰 ~、不写绝对 /tmp、不联网、不 sleep、不起后台。
# ★ 这一支的账：很多人以为 set -x 打的是「源码那一行」，实测它打的是**展开之后的那一次执行**；
#   以为它能显示调用深度，实测函数深度到 2 还是一个加号、只有命令替换才加号；
#   以为它会污染 stdout，实测它走 stderr，而 bash -c 起的子进程一个字都不 trace。
# ★ 每一格都分开打 stdout 与 stderr（前缀 1>/2>），免得把正菜错标成 trace。
# ============================================================================

rm -rf xt
mkdir -p xt
trap 'rm -rf xt' EXIT

fold() { sed -E "s#$HOME#<家目录>#g; s#\\b$USER\\b#<用户>#g; s#$(hostname)#<主机名>#g"; }
# on/stderr：把一条命令的两条流分别贴上来源标签，避免「看起来像 trace 其实是正菜」。
on()  { "$@" 2>/dev/null | sed 's/^/      1│ /'; }
sn()  { "$@" 2>&1 >/dev/null | fold | sed 's/^/      2│ /'; }

echo "=== 1. 默认 PS4 与「深度」：函数不加号，命令替换才加号 ==="
printf '  本机 PS4 的默认值（cat -A 看首尾空白）=[%s]\n' "$(bash -c 'printf %s "$PS4"' | cat -A)"
cat > xt/nest.sh <<'EOF'
f() { g() { :; }; g; }
f
EOF
printf '  被演示的脚本一共 %s 行（f 叫 g，g 叫 :，深度 3）：\n' "$(wc -l < xt/nest.sh)"
nl -ba xt/nest.sh | sed 's/^/    /'
printf '  bash -x 的 stderr：\n'
sn bash -x xt/nest.sh
printf '  再演一种嵌套：命令替换套两层（echo 里套 echo 里套 echo）\n'
cat > xt/sub.sh <<'EOF'
echo $(echo $(echo 最深))
EOF
sn bash -x xt/sub.sh
printf '  → 实测两件事分开：函数调用深度到 3，每行 trace 前面还是单个 "+"（它不表示层级）；\n'
printf '    命令替换却按层加号，而且**最里层先执行所以先打** —— 上面三行从大到小是 +++ / ++ / +。\n'
printf '    想看出函数深度只能自己往 PS4 里塞（ps4fd.sh 第 1 格），别指望默认形状。\n'

echo
echo "=== 2. set -v 与 set -x：一个打源文本，一个打展开之后 ==="
cat > xt/vx.sh <<'EOF'
a=甲
printf 'a=%s\n' "$a"
EOF
printf '  同一份脚本（%s 行）：\n' "$(wc -l < xt/vx.sh)"
nl -ba xt/vx.sh | sed 's/^/    /'
printf '  bash -v 的两条流：\n'
on bash -v xt/vx.sh
sn bash -v xt/vx.sh
printf '  bash -x 的两条流：\n'
on bash -x xt/vx.sh
sn bash -x xt/vx.sh
printf '  → 同一条 printf：-v 把你写的那一行原样吐回来（连反斜杠都还在：'"'"'a=%s\\n'"'"' 与 "$a"），\n'
printf '    -x 吐的是展开做完那一次（$a 已经变成甲，你写的那对引号不见了，因为「甲」这个字不需要引号）。\n'
printf '    两条流的 1│ 都是正菜 a=甲，只有 2│ 里才是各自打的调试行。\n'
printf '    ★ 但别读成「-x 把引号都吃了」：看下一格，同一份 trace 里带空格与带 [ ] 的词是被 bash 打上引号打印的，\n'
printf '      而前导 - 的词它不打 —— -x 的引号是打印层现加的，不是你源码里那对，拿它当「引号写对了吗」的证据会读反。\n'

echo
echo "=== 3. 一行少引号的写法，两种 trace 各现什么形 ==="
cat > xt/bug.sh <<'EOF'
a=有 空格
printf 'a=[%s]\n' "$a"
EOF
printf '  脚本第 1 行是 a=有 空格（看着像少了一对引号）\n'
printf '  bash -v：\n'
on bash -v xt/bug.sh
sn bash -v xt/bug.sh
printf '  bash -x：\n'
on bash -x xt/bug.sh
sn bash -x xt/bug.sh
printf '  → -x 把这一行拆成两条（+ a=有 ／ + 空格），随后紧跟一行「空格: 未找到命令」，脚本没停；\n'
printf '    这句是 bash 的报错原文，跟着语言环境走 —— 英文环境里同一处印 command not found。\n'
printf '    所以读 trace 只认结构（哪一条被拆成两行、哪一行是命令名），别认这句话的字面。\n'
printf '  ★ 但结论不是「少引号的赋值把值截断了」：屏上正菜是 a=[]（空！）不是 a=[有]。\n'
printf '    VAR=值 命令 这种形状里，前面那截是**给那条命令的环境前缀**，根本没写进 shell 变量；\n'
printf '    这里后面那一截恰好是个不存在的命令，于是两边都没落着 —— 少引号只是让它更容易被这么误读。\n'

echo
echo "=== 4. trace 给带空格与带 glob 字符的词补引号，给前导 - 的词不补 ==="
cat > xt/q.sh <<'EOF'
a=("有 空格.txt" "-dash.txt" "甲 乙" "a[b]")
printf '[%s]\n' "${a[@]}"
EOF
sn bash -x xt/q.sh
on bash -x xt/q.sh
printf '  → 实测：带空格的两个词、含 [ ] 的那个词都被打上引号（这些字符不引就会歧义）；\n'
printf '    -dash.txt 却原样打出来 —— 「屏上看着像选项」不等于「它是选项」，\n'
printf '    也别拿 trace 的排版当「引号加得对不对」的判据：它是打印层，不是源码层。\n'

echo
echo "=== 5. trace 走的是 stderr：三种接法各留下什么 ==="
cat > xt/mix.sh <<'EOF'
set -x
echo 正菜
EOF
printf '  只看 stdout（丢 stderr）：\n'
on bash xt/mix.sh
printf '  只看 stderr（丢 stdout）：\n'
sn bash xt/mix.sh
printf '  合起来按到达顺序看：\n'
printf '      1+2│ %s\n' "$(bash xt/mix.sh 2>&1 | fold | tr '\n' '/')"
printf '  → 把 2 并进 1 看到的是交错顺序（trace 先到），丢掉 2 得到一份干净结果；\n'
printf '    想把 trace 单独存文件就别用 2> —— 那条路会把真报错一起收进 trace 文件里。\n'

echo
echo "=== 6. 就地夹一段：{ set -x; ...; set +x; } 2>/dev/null ==="
cat > xt/clip.sh <<'EOF'
#!/usr/bin/env bash
echo 前
{ set -x; echo 中间; set +x; } 2>/dev/null
echo 后
EOF
cat > xt/clip2.sh <<'EOF'
#!/usr/bin/env bash
echo 前
{ set -x; echo 中间; set +x; }
echo 后
EOF
printf '  带 2>/dev/null 的 stdout=[%s]\n' "$(bash xt/clip.sh 2>/dev/null | tr '\n' '/')"
printf '  带 2>/dev/null 的 stderr=[%s]\n' "$(bash xt/clip.sh 2>&1 >/dev/null | fold | tr '\n' '/')"
printf '  不带 2>/dev/null 的 stdout=[%s]\n' "$(bash xt/clip2.sh 2>/dev/null | tr '\n' '/')"
printf '  不带 2>/dev/null 的 stderr=[%s]\n' "$(bash xt/clip2.sh 2>&1 >/dev/null | fold | tr '\n' '/')"
printf '  → 正菜一个字没少（中间那句照样走 stdout），少的只是那两行 +；\n'
printf '    stderr 里是「+ echo 中间 / + set +x」两行 —— 打开 xtrace 的那句自己不被 trace（那一刻还没打开）。\n'

echo
echo "=== 7. 容器与继承：( ) 和 \$( ) 跟得到 xtrace，bash -c 跟不到 ==="
cat > xt/cont.sh <<'EOF'
#!/usr/bin/env bash
set -x
echo 顶层
{ echo 花括号; }
( echo 圆括号 )
v=$(echo 命令替换)
echo 甲 | tr 甲 乙
bash -c 'echo 新bash'
EOF
printf '  正菜（stdout）：\n'
on bash xt/cont.sh
printf '  trace（stderr，按字面排序后的；原始顺序见下面那句「这一趟的实际先后」）：\n'
sn bash xt/cont.sh | LC_ALL=C sort
printf '  每种容器各出现几条 trace（判据只看有没有，不看先后）：顶层=%s ｜ 花括号=%s ｜ 圆括号=%s ｜ 命令替换=%s ｜ 管道左=%s ｜ 管道右=%s ｜ 新 bash 体内=%s\n' \
    "$(bash xt/cont.sh 2>&1 >/dev/null | grep -c '^+ echo 顶层')" \
    "$(bash xt/cont.sh 2>&1 >/dev/null | grep -c '^+ echo 花括号')" \
    "$(bash xt/cont.sh 2>&1 >/dev/null | grep -c '^+ echo 圆括号')" \
    "$(bash xt/cont.sh 2>&1 >/dev/null | grep -c '^++ echo 命令替换')" \
    "$(bash xt/cont.sh 2>&1 >/dev/null | grep -c '^+ echo 甲')" \
    "$(bash xt/cont.sh 2>&1 >/dev/null | grep -c '^+ tr 甲 乙')" \
    "$(bash xt/cont.sh 2>&1 >/dev/null | grep -c '^+ echo 新bash')"
printf '  ★ 上面那格为什么排序才敢印：管道两端是两个进程同时往同一个 stderr 写，谁先落笔由调度决定，\n'
printf '    两趟跑下来这两行的先后可能就换了个位 —— 所以这一格把顺序信息放弃掉，只保留「谁出现了、出现几条」。\n'
printf '    想留住顺序就得给每条 trace 加进程号（第 13 节 strace -f 的 pid 前缀就是干这个的），而加了 pid 又不可跨趟对拍。\n'
printf '  → 数一下：花括号 / 圆括号 / 命令替换 / 管道两端 各自的 echo 都出现了 trace 行；\n'
printf '    + bash -c ... 那一行在，而它体内的 echo 一次都没出现（上面那格「新 bash 体内=0」就是它）——\n'
printf '    前四种是同一个 bash fork 出来的子 shell，xtrace 是「选项」，跟着 fork 继承；\n'
printf '    bash -c 是全新进程，自己的选项自己定，除非你显式 bash -x -c。\n'
printf '    对照第 08/09 章的另一半：陷阱（DEBUG/ERR）不跟 fork 继承，所以 ( ) 里根本不响。\n'

echo
echo "=== 8. 泄密：赋值在 trace 里是明文，关早了才挡得住 ==="
cat > xt/sec.sh <<'EOF'
#!/usr/bin/env bash
set -x
token=$(printf 'supersecret')
{ set +x; } 2>/dev/null
echo "拿到的长度=${#token}"
EOF
printf '  整份 stderr=[%s]\n' "$(bash xt/sec.sh 2>&1 >/dev/null | fold | tr '\n' '/')"
printf '  明文 supersecret 命中：stderr=%s 处 ｜ stdout=%s 处 ｜ 正菜那句只报长度=%s\n' \
    "$(bash xt/sec.sh 2>&1 >/dev/null | grep -c supersecret)" \
    "$(bash xt/sec.sh 2>/dev/null | grep -c supersecret)" \
    "$(bash xt/sec.sh 2>/dev/null | tr '\n' '/')"
cat > xt/sec2.sh <<'EOF'
#!/usr/bin/env bash
set -x
echo 看得见
{ set +x; } 2>/dev/null
secret=supersecret
echo 关完之后
EOF
printf '  把 { set +x; } 2>/dev/null 挪到赋值**之前**：stderr=[%s]\n' "$(bash xt/sec2.sh 2>&1 >/dev/null | fold | tr '\n' '/')"
printf '  → 第一份里 ++ printf supersecret 和 + token=supersecret 两行都是明文（命令替换也被 trace）；\n'
printf '    第二份把关掉的一刻提到赋值之前，屏上就只剩 + echo 看得见 —— 遮密的要害是「赋值那一行之前就关」，\n'
printf '    事后关只保护后面的行，那一次取值早打在屏上了。真要给 trace 落盘，还得配 0600 的文件（ps4fd.sh 第 7 格）。\n'

echo
echo "=== 9. trace 的行数与源码行数不是一回事 ==="
cat > xt/count.sh <<'EOF'
#!/usr/bin/env bash
s=0
for x in 1 2 3; do
    s=$((s + x))
done
printf 's=%s\n' "$s"
EOF
printf '  源码行数=%s ｜ trace 行数=%s ｜ 正菜行数=%s\n' \
    "$(wc -l < xt/count.sh)" "$(bash -x xt/count.sh 2>&1 >/dev/null | wc -l)" "$(bash -x xt/count.sh 2>/dev/null | wc -l)"
printf '  不带 + 的 trace 行数=%s（trace 每行都规规矩矩以 + 开头）\n' "$(bash -x xt/count.sh 2>&1 >/dev/null | grep -vc '^+ ')"
printf '  for 那一行在 trace 里出现=%s 次\n' "$(bash -x xt/count.sh 2>&1 >/dev/null | grep -c '^+ for x in')"
printf '  → trace 数的是**执行次数**，源码数的是**写了多少行**：一行 for 三轮就是三行，\n'
printf '    拿「trace 行号能不能对上源码行号」当默认预期会失望（要行号得自己在 PS4 里放 $LINENO）。\n'

echo
echo "=== 10. 这套证据自己可复现吗：同一段 trace 跑两遍逐行 diff ==="
bash -x xt/count.sh 2>&1 | fold > xt/t1.txt
bash -x xt/count.sh 2>&1 | fold > xt/t2.txt
sed 's/^+ for x in/+ FORX x in/' xt/t1.txt > xt/t3.txt
printf '  ① 先看守卫能不能响：把 t3 里 for 那一行改一个词，与 t2 diff 命中=%s 行（期望非 0）\n' \
    "$(diff xt/t3.txt xt/t2.txt | grep -c '^[<>]')"
diff xt/t1.txt xt/t2.txt > xt/d.txt 2>&1
printf '  ② 撤掉改动：同一脚本两次 bash -x 的输出 diff=%s 行（期望 0）｜ 两遍各 %s/%s 行\n' \
    "$(grep -c '^[<>]' xt/d.txt)" "$(wc -l < xt/t1.txt)" "$(wc -l < xt/t2.txt)"
printf '  → 默认的 xtrace 里不含时间戳、不含 pid，所以它是本章少数「整屏可对拍」的证据；\n'
printf '    一旦往 PS4 里塞 $(date) 或 $$，这份可复现性当场作废（ps4fd.sh 第 4、5 格）。\n'

echo
printf '[xtrace.sh 自查] 本脚本格数=%s ｜ xt/ 里现有文件数=%s（跑完由 trap 整目录删除）\n' \
    "$(grep -c '^echo .=== ' "$0")" "$(find xt -type f | wc -l)"
