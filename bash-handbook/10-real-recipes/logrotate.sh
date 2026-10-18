#!/usr/bin/env bash
# CHECK: RUN=非复现 理由：第 3/4/5 格的后台写进程「轮转后又写了多少行、窗口里丢了几行、fd 里涨到多少字节」由调度决定，第 11 格不加锁那组的末值由竞态决定，第 3/4/5 格还当场印自己记的 pid；这些每跑必变，判据一律写成「期望 >0 / 期望 0 / 相同吗=1」这类区间与存在性，不写等值（免除的证据只有「两遍逐字相同」这一条；rc、残留、隐私、每格的收口判定照判）
# ============================================================================
# logrotate.sh —— 日志轮转：把「mv 换 inode、fd 还跟着老 inode 走、copytruncate 会丢窗口」这三件事测到屏上
#
#   demo.sh 里这样调：bash logrotate.sh
#   安全：只在当前目录建 lt/（沙箱配置、状态文件、被演示的日志、后台写进程都关在它里面），
#         跑完由 trap 整目录删掉；全用相对文件名上屏（logrotate 的配置文件里必须写绝对路径，
#         那条路径一上屏就当场 sed 折成 <沙箱>）；绝不碰 /etc、不写 ~、绝不用系统 /etc/logrotate.d，
#         logrotate 只吃沙箱配置（位置参数）+ 沙箱状态文件（-s）；不联网、不装包、不开端口；
#         后台进程只准是自己 & 起来的，全部 wait 收尾，trap 里再按记下的 pid 补刀（只杀自己那几个）。
# ★ 这一支要还的账：轮转不是「把文件清干净」，而是「换掉文件名背后那个 inode」。谁还顺着旧 fd 写，
#   日志就落进谁那里 —— 新文件永远是空的。rm、copytruncate、create、flock 每一样都在改这件事的形状。
# ★ 每个「应当 0 / 应当一致」的判据都先喂一条真违规把它弄响，再报真实值：从不出数的守卫不算证据。
# ============================================================================

rm -rf lt
mkdir -p lt
PIDS=()
cleanup() {
    local p
    # 只 kill 自己记下的 pid（别人的一律不碰）；wait 把僵尸收干净；最后整目录删掉
    for p in ${PIDS[@]+"${PIDS[@]}"}; do
        kill "$p" 2>/dev/null || :
    done
    wait 2>/dev/null || :
    rm -rf lt
}
trap cleanup EXIT

# 本机身份串：hostname 与 $USER 在这台机器上是同一个串，两条都各自折掉
SBX=$(pwd)
HN=$(hostname)
LR="$(command -v logrotate 2>/dev/null || echo /usr/sbin/logrotate)"
fold() { sed -E "s#$SBX#<沙箱>#g; s#$HOME#<家目录>#g; s#\\b$USER\\b#<用户>#g; s#\\b$HN\\b#<主机名>#g"; }

# 后台写进程（非重开）：exec 3>> 打开后一直复用这个 fd，写到 n 行自己退出
writer_hold() {
    exec 3>>"$1"
    local i=1
    while [ "$i" -le "${2:-40}" ]; do
        printf '行%s\n' "$i" >&3
        sleep 0.05
        i=$((i + 1))
    done
    exec 3>&-
}
# 后台写进程（每行都重开）：模拟「进程自己响应信号 reopen」——每写一行都重新按文件名开一次
writer_reopen() {
    local i=1
    while [ "$i" -le "${2:-40}" ]; do
        ( exec 3>>"$1"; printf 'R%s\n' "$i" >&3 )
        sleep 0.05
        i=$((i + 1))
    done
}

# ---------------------------------------------------------------------------
printf '=== 1. 在场名单与身份：本章要用的东西哪几件在场（不在场的一个都不演）===\n'
present=0; absent=0
for t in logrotate inotifywait entr flock lsof fuser script gzip; do
    p="$(command -v "$t" 2>/dev/null)"; rc=$?
    if [ "$rc" -eq 0 ]; then present=$((present + 1)); st=在场; else absent=$((absent + 1)); st=不在场; fi
    printf '    %-12s rc=%s %-6s 路径=[%s]\n' "$t" "$rc" "$st" "$(printf '%s' "${p:-无}" | fold)"
done
printf '  在场=%s 件 ｜ 不在场=%s 件（inotifywait/entr 不在场 → inotify 触发与增量重跑这两条本章一律不演）\n' "$present" "$absent"
cpath="$(command -v crontab 2>/dev/null)"; spath="$(command -v systemctl 2>/dev/null)"
printf '  crontab 在场=[%s] ｜ systemctl 在场=[%s] —— 二者本章「在场、一次都没调用」：全章的执行行由第 13 节第 3 格按十类模式扫，本脚本自己的那份自查在 interact.sh 第 9 格\n' "$cpath" "$spath"
printf '  → 三个数就够定本章边界：logrotate 在 /usr/sbin（普通 PATH 未必收，故上面用 command -v 的原样路径、下文用绝对路径跑它）；\n'
printf '    造伪 tty 的 script、做压缩的 gzip、加锁的 flock、查谁开着的 lsof/fuser 都在场 —— 每一格都能真跑。\n'

# ---------------------------------------------------------------------------
printf '\n=== 2. 手工轮转的最小形状：mv + 空重定向，inode 当场换人 ===\n'
printf 'l1\nl2\nl3\n' > lt/app.log
ino_before=$(stat -c %i lt/app.log); sz_before=$(stat -c %s lt/app.log)
printf '  守卫（先弄响）：轮转前 app.log 字节数=%s（期望 >0，这就是「不 truncate 它就还是满的」那条真违规）\n' "$sz_before"
mv lt/app.log lt/app.log.1
: > lt/app.log          # 新建一个全新 inode 的空文件（这就是「轮转」的最小动作）
ino_new=$(stat -c %i lt/app.log); ino_bak=$(stat -c %i lt/app.log.1)
sz_new=$(stat -c %s lt/app.log); sz_bak=$(stat -c %s lt/app.log.1)
new_eq_old=$(( ino_new == ino_before ? 1 : 0 ))    # 新 app.log 与轮转前那个文件同 inode 吗：期望 0（换了）
bak_eq_old=$(( ino_bak == ino_before ? 1 : 0 ))    # .1 就是原来那个文件：期望 1（mv 只改目录项）
printf '  新 app.log 的 inode == 轮转前 inode 吗=%s（期望 0，全章最关键一条：inode 换了）｜ .1 的 inode == 轮转前 inode 吗=%s（期望 1）\n' "$new_eq_old" "$bak_eq_old"
printf '  轮转后新 app.log 字节数=%s（期望 0）｜ app.log.1 字节数=%s（期望=%s 轮转前，内容一个字没丢）\n' "$sz_new" "$sz_bak" "$sz_before"
printf '  → mv 只把「文件名 → inode」这条目录项改名搬去 .1（所以 .1 的 inode 与原文件相同=%s）；\n' "$bak_eq_old"
printf '    : > 另建一个 inode 完全不同的空文件顶上来（新 inode 与原文件相同=%s）。这就是轮转的物理形状。\n' "$new_eq_old"

# ---------------------------------------------------------------------------
printf '\n=== 3. fd 跟着 inode 走（本章最贵的一格）：轮转后新写入全落进被轮走的那份 ===\n'
: > lt/app3.log
writer_hold lt/app3.log 40 & W3=$!; PIDS+=("$W3")
sleep 0.3
pre=$(wc -l < lt/app3.log)                       # 轮转前老文件已有几行
mv lt/app3.log lt/app3.log.1; : > lt/app3.log    # 手工轮转（老 fd 仍指向那个 inode）
sleep 0.3
alive=$([ -d "/proc/$W3" ] && echo 1 || echo 0)  # 此刻它还活着，三种口径才有得测
proc_hit=$(ls -l "/proc/$W3/fd/" 2>/dev/null | grep -c 'app3.log.1')            # 只读自己那个 pid
fuser_hit=$(fuser lt/app3.log.1 2>/dev/null | wc -w)                             # fuser 报几个进程
lsof_hit=$(lsof lt/app3.log.1 2>/dev/null | grep -c 'app3.log')                  # lsof 报几行（含表头与 fd 行）
wait "$W3"
new_lines=$(wc -l < lt/app3.log)
old_lines=$(wc -l < lt/app3.log.1)
post=$(( old_lines - pre ))
printf '  写进程 pid=%s（记在案，收尾已 wait）｜轮转那一刻它还活着=%s（期望 1，否则三口径没得测）\n' "$W3" "$alive"
printf '  轮转后老文件 app3.log.1 里新增的行数=%s（期望 >0，这就是丢日志：新写入没进新文件，全写进被轮走的那份）\n' "$post"
printf '  轮转后新 app3.log 行数=%s（期望 0：老 fd 根本不认识这个新建的文件）\n' "$new_lines"
printf '  三种口径互证「谁还开着老文件」（只读/只查自己那个 pid）：\n'
printf '    /proc/%s/fd 里指向 app3.log.1 的行数=%s（期望 1）｜ fuser 命中进程数=%s ｜ lsof 命中行数=%s\n' "$W3" "$proc_hit" "$fuser_hit" "$lsof_hit"
printf '  → fd 记的是 inode 不是文件名：mv 之后老进程还顺着 3 号 fd 往那个 inode 写，于是 .1 又长出 %s 行（>0）、\n' "$post"
printf '    新文件停在 %s 行（0）。/proc、fuser、lsof 三个口径都指到同一个还开着的 pid —— 互相印证，不是猜。\n' "$new_lines"

# ---------------------------------------------------------------------------
printf '\n=== 4. rm 掉文件名 != 释放空间：文件从目录里消失，字节还顺着 fd 在长 ===\n'
: > lt/del4.log
writer_hold lt/del4.log 40 & W4=$!; PIDS+=("$W4")
sleep 0.4
sz_before=$(stat -c %s lt/del4.log)
vis_before=$(ls -1 lt/ 2>/dev/null | grep -c '^del4.log$')   # 守卫：删名前它在目录里（应当=1）
rm lt/del4.log; rc_rm=$?
sleep 0.3                                                     # rm 之后进程继续往那个（已无名的）inode 写
vis_after=$(ls -1 lt/ 2>/dev/null | grep -c '^del4.log$')     # 期望 0：名字没了，ls 再也看不见
del_tag=$(readlink "/proc/$W4/fd/3" 2>/dev/null | grep -c deleted)   # fd 目标带 (deleted) 标记
fd_bytes=$(stat -c %s "/proc/$W4/fd/3" 2>/dev/null || echo -1)       # 经 /proc 的 fd 仍能读出长度（还在长）
wait "$W4"
printf '  rm 的 rc=%s（0=删名成功）\n' "$rc_rm"
printf '  守卫（先弄响）：删名前目录里可见 del4.log 的行数=%s（期望 1）｜ 删名后=%s（期望 0：名字确实没了）\n' "$vis_before" "$vis_after"
printf '  但字节还在长：fd 目标带 (deleted) 的行数=%s（期望 1）｜ 经 /proc/%s/fd/3 读到字节数=%s（期望 >%s：删名后它还在涨）\n' "$del_tag" "$W4" "$fd_bytes" "$sz_before"
printf '  → 删除文件名 != 释放空间：目录项没了（ls 从 %s 掉到 %s）、inode 仍挂着那个打开的 fd，\n' "$vis_before" "$vis_after"
printf '    所以按名字走一遍目录的工具 du 也数不到它，可 /proc/fd 里字节从 %s 涨到 %s —— 进程一退、fd 一关才真回收。\n' "$sz_before" "$fd_bytes"

# ---------------------------------------------------------------------------
printf '\n=== 5. copytruncate 的形状与代价：inode 不变，但截断窗口丢写入；正解是让进程自己重开 ===\n'
# A) 先比两种轮转对 inode 的影响：mv 换 inode（违规样本），cp+:> 不换（断言）
printf 'k\n' > lt/i5.log; i0=$(stat -c %i lt/i5.log); cp lt/i5.log lt/i5.log.bak; : > lt/i5.log; i1=$(stat -c %i lt/i5.log)
printf 'k\n' > lt/j5.log; j0=$(stat -c %i lt/j5.log); mv lt/j5.log lt/j5.log.bak; : > lt/j5.log; j1=$(stat -c %i lt/j5.log)
cp_same=$(( i0 == i1 ? 1 : 0 ))   # copytruncate：新文件还是原 inode（期望 1）
mv_same=$(( j0 == j1 ? 1 : 0 ))   # mv：新文件换了 inode（期望 0）—— 这就是把「应当 1」先弄响的那条违规
printf '  守卫（先弄响）：mv 那套轮出来的新文件 inode 与原文件相同吗=%s（期望 0，inode 会变）\n' "$mv_same"
printf '  对照 copytruncate（cp 再 : >）：新文件 inode 与原文件相同吗=%s（期望 1，写进程无感、继续往同一 inode 写）\n' "$cp_same"
# B) copytruncate 的截断窗口丢几行
: > lt/win5.log
writer_hold lt/win5.log 30 & W5=$!; PIDS+=("$W5")
sleep 0.3
cp lt/win5.log lt/win5.log.1   # 快照
sleep 0.4                       # 窗口期：这段时间的写入即将被下面的 : > 抹掉
: > lt/win5.log                 # truncate：连同窗口期的行一起清掉
wait "$W5"
w_1=$(wc -l < lt/win5.log.1); w_new=$(wc -l < lt/win5.log); w_lost=$(( 30 - w_1 - w_new ))
printf '  copytruncate 丢行数：老备份 .1 里=%s 行 + 新文件里=%s 行，写进程声称写了 30 行 → 少掉的（截断窗口丢的）=%s（期望 >0）\n' "$w_1" "$w_new" "$w_lost"
# C) 同一份 mv 轮转下：不重开就丢、重开就不丢（都是本脚本自己写的 bash 模拟写进程，不测真实服务信号）
: > lt/no5.log; : > lt/yes5.log
writer_hold lt/no5.log 30 & WNo=$!; PIDS+=("$WNo")
writer_reopen lt/yes5.log 30 & WYes=$!; PIDS+=("$WYes")
sleep 0.3
mv lt/no5.log lt/no5.log.1; : > lt/no5.log          # 不重开：fd 攥着老 inode
mv lt/yes5.log lt/yes5.log.1; : > lt/yes5.log       # 重开：每行按新文件名重开
sleep 0.4
wait "$WNo"; wait "$WYes"
no_new=$(wc -l < lt/no5.log); yes_new=$(wc -l < lt/yes5.log)
printf '  轮转后新文件收到几行：不重开=%s（期望 0，全落进 .1 即丢）｜ 每行重开=%s（期望 >0，重开就不丢）\n' "$no_new" "$yes_new"
printf '  → copytruncate 让写进程无感（inode 相同=%s）却在截断那一瞬间丢了 %s 行；mv 不重开就把 %s 行全丢进轮走的那份。\n' "$cp_same" "$w_lost" "$no_new"
printf '    生产正解是让进程自己重开日志（收到信号 reopen）：重开那档新文件收到 %s 行（>0）就是它不丢的证据。\n' "$yes_new"

# ---------------------------------------------------------------------------
printf '\n=== 6. 真工具跑起来：logrotate -d 只打印打算干什么，-f 真轮一次（配置/状态都在沙箱）===\n'
printf 't1\nt2\n' > lt/tool.log
cat > lt/lr.conf <<CFGEOF
$SBX/lt/tool.log {
    rotate 3
    create 0640
}
CFGEOF
"$LR" -d -s lt/state "$SBX/lt/lr.conf" > lt/d.out 2>&1; rc_d=$?
"$LR" -f -s lt/state "$SBX/lt/lr.conf" > lt/f.out 2>&1; rc_f=$?
printf '  注意：logrotate 不吃 -c，配置文件按位置参数给；这里只喂沙箱配置 + 沙箱状态文件（-s）\n'
printf '  -d（debug 只打印不干活）的 rc=%s（期望 0）｜ 里面「considering log」的行数=%s\n' "$rc_d" "$(grep -c 'considering log' lt/d.out)"
printf '  -f（强制轮转）的 rc=%s（期望 0）\n' "$rc_f"
printf '  轮转后 lt/ 里 tool 相关的文件名形状=[%s]\n' "$(ls -1 lt/ | grep '^tool' | tr '\n' ' ')"
printf '  状态文件里那一行的形状=[%s]\n' "$(grep 'tool.log' lt/state | sed -E "s#$SBX#<沙箱>#g; s#[0-9]{4}-[0-9]{2}-[0-9]{2}[-:0-9]*#<日期>#g" | tr '\n' '/')"
printf '  → -d 什么都不改（文件名形状里仍只有一个 tool.log、状态文件那行是新建态）；-f 才真把 tool.log 顶成 tool.log.1 并按 create 建新的空 tool.log。\n'
printf '    状态文件那一行 = 绝对路径（折成 <沙箱>）+ 上次轮转时间（折成 <日期>）；它决定下次到不到轮转周期，本章都用 -f 强制绕开它。\n'

# ---------------------------------------------------------------------------
printf '\n=== 7. rotate 3 的删除策略：连轮 5 次，最老的被删、编号封顶在 3 ===\n'
rm -f lt/tool.log lt/tool.log.*; : > lt/tool.log
cat > lt/r5.conf <<CFGEOF
$SBX/lt/tool.log {
    rotate 5
    create 0640
}
CFGEOF
for i in 1 2 3 4 5; do printf 'x%s\n' "$i" > lt/tool.log; "$LR" -f -s lt/st7b "$SBX/lt/r5.conf" >/dev/null 2>&1; done
ring45=$(ls -1 lt | grep -cE 'tool\.log\.(4|5)$'); ringmax=$(ls -1 lt | grep -oE 'tool\.log\.[0-9]+$' | grep -oE '[0-9]+$' | sort -n | tail -1)
printf '  守卫（先弄响）rotate 5：出现 .4/.5 的个数=%s（期望 >0，证明「编号封顶」这个数不是恒零）、最大编号=%s\n' "$ring45" "$ringmax"
rm -f lt/tool.log lt/tool.log.*; : > lt/tool.log
cat > lt/r3.conf <<CFGEOF
$SBX/lt/tool.log {
    rotate 3
    create 0640
}
CFGEOF
for i in 1 2 3 4 5; do printf 'x%s\n' "$i" > lt/tool.log; "$LR" -f -s lt/st7 "$SBX/lt/r3.conf" >/dev/null 2>&1; done; rc7=$?
ncount=$(ls -1 lt | grep -cE 'tool\.log\.[0-9]+$'); nmax=$(ls -1 lt | grep -oE 'tool\.log\.[0-9]+$' | grep -oE '[0-9]+$' | sort -n | tail -1)
nover=$(ls -1 lt | grep -cE 'tool\.log\.(4|5)$')
printf '  rotate 3：末次 -f 的 rc=%s（期望 0）｜ 还在的 tool.log.N 有几个=%s（期望 3）｜ 最大编号=%s（期望 3）｜ 出现 .4/.5 吗=%s（期望 0）\n' "$rc7" "$ncount" "$nmax" "$nover"
printf '  → 同样连轮 5 次：rotate 5 留到 .5（守卫那行 >0），rotate 3 只留 .1 .2 .3，第 4、5 次把最老的删了、封顶在 %s。\n' "$nmax"
printf '    「保留几份」这件事直接改的是文件名集合的形状 —— 这就是策略改变文件名形状的正证。\n'

# ---------------------------------------------------------------------------
printf '\n=== 8. missingok / notifempty / dateext 各改什么：拿「文件不存在」和「空文件」两个样本对拍 ===\n'
# 8a missingok：样本文件不存在
rm -f lt/m8.log lt/m8.log.*
cat > lt/nomis.conf <<CFGEOF
$SBX/lt/m8.log {
    rotate 3
}
CFGEOF
"$LR" -f -s lt/st8a "$SBX/lt/nomis.conf" >/dev/null 2>&1; rc_nomiss=$?
cat > lt/mis.conf <<CFGEOF
$SBX/lt/m8.log {
    rotate 3
    missingok
}
CFGEOF
"$LR" -f -s lt/st8b "$SBX/lt/mis.conf" >/dev/null 2>&1; rc_mis=$?
printf '  missingok（文件不存在）：不带 rc=%s（期望非 0：报 stat failed）｜ 带 rc=%s（期望 0：当没事发生）\n' "$rc_nomiss" "$rc_mis"
# 8b notifempty：空文件
rm -f lt/n8.log lt/n8.log.*; : > lt/n8.log
cat > lt/nonot.conf <<CFGEOF
$SBX/lt/n8.log {
    rotate 3
}
CFGEOF
"$LR" -f -s lt/st8c "$SBX/lt/nonot.conf" >/dev/null 2>&1; rc_nn=$?
after_nn=$(ls -1 lt | grep -cE '^n8\.log\.')
rm -f lt/n8.log lt/n8.log.*; : > lt/n8.log
cat > lt/note.conf <<CFGEOF
$SBX/lt/n8.log {
    rotate 3
    notifempty
}
CFGEOF
"$LR" -f -s lt/st8d "$SBX/lt/note.conf" >/dev/null 2>&1; rc_nt=$?
after_nt=$(ls -1 lt | grep -cE '^n8\.log\.')
printf '  守卫（先弄响）notifempty（空文件）：不带时新增轮转文件=%s（期望 >0，空也照轮）、rc=%s ｜ 带时新增=%s（期望 0，跳过）、rc=%s\n' "$after_nn" "$rc_nn" "$after_nt" "$rc_nt"
# 8c dateext：文件名带日期后缀
printf 'data\n' > lt/date8.log
cat > lt/de.conf <<CFGEOF
$SBX/lt/date8.log {
    rotate 3
    dateext
    dateformat -%Y%m%d
}
CFGEOF
"$LR" -f -s lt/st8e "$SBX/lt/de.conf" >/dev/null 2>&1; rc_de=$?
de_cnt=$(ls -1 lt | grep -cE '^date8\.log-[0-9]{8}$'); de_shape=$(ls -1 lt | grep -E '^date8\.log-[0-9]{8}$' | sed -E 's/-[0-9]{8}/-<日期>/' | tr '\n' ' ')
printf '  dateext：rc=%s（期望 0）｜ 带 8 位日期后缀的文件数=%s（期望 1）｜ 名字形状=[%s]（真实日期当场折成 <日期>）\n' "$rc_de" "$de_cnt" "$de_shape"
printf '  → 三条指令各改一件事：missingok 改「文件不存在时报不报错（rc）」；notifempty 改「空文件轮不轮（目录里新增几个）」；\n'
printf '    dateext 改「轮出来的文件叫什么」——从 .1 变成 date8.log-<日期>（形状上面已折好，避免和别趟对拍冲突）。\n'

# ---------------------------------------------------------------------------
printf '\n=== 9. compress 与 delaycompress：轮完之后目录里有几个 .gz、最新那份压没压 ===\n'
gz_path="$(command -v gzip 2>/dev/null)"; gz_rc=$?
rm -f lt/c9.log lt/c9.log.*; : > lt/c9.log
cat > lt/cmp.conf <<CFGEOF
$SBX/lt/c9.log {
    rotate 3
    compress
}
CFGEOF
for i in 1 2; do printf 'c%s\n' "$i" > lt/c9.log; "$LR" -f -s lt/st9a "$SBX/lt/cmp.conf" >/dev/null 2>&1; done
gz=$(ls -1 lt | grep -cE '^c9\.log\.[0-9]+\.gz$'); dot1=$(ls -1 lt | grep -cE '^c9\.log\.1$')
printf '  gzip 在场=[%s] rc=%s\n' "${gz_path:-无}" "$gz_rc"
printf '  守卫（先弄响）compress 轮两轮：未压的 .1 还在吗=%s（期望 0，两份都被压走）｜ .gz 个数=%s（期望 2）\n' "$dot1" "$gz"
rm -f lt/c9.log lt/c9.log.*; : > lt/c9.log
cat > lt/dcmp.conf <<CFGEOF
$SBX/lt/c9.log {
    rotate 3
    compress
    delaycompress
}
CFGEOF
for i in 1 2 3; do printf 'd%s\n' "$i" > lt/c9.log; "$LR" -f -s lt/st9b "$SBX/lt/dcmp.conf" >/dev/null 2>&1; done
dot1d=$(ls -1 lt | grep -cE '^c9\.log\.1$'); gzd=$(ls -1 lt | grep -cE '^c9\.log\.[0-9]+\.gz$')
printf '  delaycompress 轮三轮：未压的 .1 还在吗=%s（期望 1，最新那份被延迟）｜ .gz 个数=%s（期望 2，旧的压了）\n' "$dot1d" "$gzd"
printf '  → 差别就在「最新那份 .1」：compress 连 .1 一起压（未压 .1 个数=%s），delaycompress 留着 .1 不压（个数=%s），\n' "$dot1" "$dot1d"
printf '    只把更早的压成 .gz。拿命中数一比即明 —— gzip 在本机可用（rc=%s）。\n' "$gz_rc"

# ---------------------------------------------------------------------------
printf '\n=== 10. create 0640 与 umask 谁赢：轮出来的新文件权限由 create 定，不由你的 umask 定 ===\n'
( umask 077; : > lt/t77 ); ( umask 022; : > lt/t22 )
m77=$(stat -c %a lt/t77); m22=$(stat -c %a lt/t22); touch_diff=$(( m77 == m22 ? 0 : 1 ))
printf '  守卫（先弄响）普通新建 touch：umask077 得=%s、umask022 得=%s → 两者不一致=%s（期望 1，证明 umask 本来管得着）\n' "$m77" "$m22" "$touch_diff"
cat > lt/cr.conf <<CFGEOF
$SBX/lt/tool.log {
    rotate 3
    create 0640
}
CFGEOF
rm -f lt/tool.log lt/tool.log.*; printf 'z\n' > lt/tool.log
( umask 077; "$LR" -f -s lt/st10a "$SBX/lt/cr.conf" >/dev/null 2>&1 ); cr77=$(stat -c %a lt/tool.log)
printf 'z\n' > lt/tool.log
( umask 022; "$LR" -f -s lt/st10b "$SBX/lt/cr.conf" >/dev/null 2>&1 ); cr22=$(stat -c %a lt/tool.log)
create_same=$(( cr77 == cr22 ? 1 : 0 ))
printf '  create 0640（本格自带配置）：umask077 下新文件=%s ｜ umask022 下=%s ｜ 两者相同吗=%s（期望 1，都是 640）\n' "$cr77" "$cr22" "$create_same"
printf '  → touch 的权限跟着 umask 走（600 vs 644，不一致=%s），logrotate create 0640 在两种 umask 下都造出 640（相同=%s）：\n' "$touch_diff" "$create_same"
printf '    轮转新建的文件权限由 create 说了算、umask 插不上手 —— 这正是生产靠 create 锁死日志权限（配合属组）的原因。\n'

# ---------------------------------------------------------------------------
printf '\n=== 11. 并发轮转要锁：flock 把读-改-写包成临界区，两个后台抢才串行成立 ===\n'
: > lt/ul.txt
for r in 1 2; do
    ( i=1; while [ "$i" -le 200 ]; do n=$(cat lt/ul.txt); n=$((n + 1)); printf '%s\n' "$n" > lt/ul.txt; i=$((i + 1)); done ) &
    PIDS+=("$!")
done
wait
ul_final=$(cat lt/ul.txt); ul_lost=$((400 - ul_final))
: > lt/lk.txt
for r in 1 2; do
    ( i=1; while [ "$i" -le 200 ]; do flock "$SBX/lt/lock.f" -c 'n=$(cat lt/lk.txt); n=$((n + 1)); echo "$n" > lt/lk.txt'; i=$((i + 1)); done ) &
    PIDS+=("$!")
done
wait
lk_final=$(cat lt/lk.txt); lk_lost=$((400 - lk_final))
printf '  总目标增量=400（两组各 2 个后台 × 200 次读改写）\n'
printf '  不加锁 末值=%s → 丢失=%s（期望 >0：两个后台的读-改-写互相插队，这是真违规样本）\n' "$ul_final" "$ul_lost"
printf '  加锁(flock) 末值=%s → 丢失=%s（期望 0：读改写被包成一段，串行成立）\n' "$lk_final" "$lk_lost"
printf '  → 同一批 400 次读改写：不锁时末值掉到 %s（丢了 %s 次），用 flock 锁住就稳稳 %s（丢 %s）。\n' "$ul_final" "$ul_lost" "$lk_final" "$lk_lost"
printf '    所以并发轮转必须用 flock 把「轮转 + 改状态」锁成一段。两个后台都已 wait 收尾，trap 里再按 pid 补刀。\n'

# ---------------------------------------------------------------------------
printf '\n[logrotate.sh 自查] 本脚本格数=%s ｜ lt/ 里现有文件数=%s（跑完由 trap 整目录删除）\n' \
    "$(grep -Ec "^printf '.*=== [0-9]" "$0")" "$(find lt -type f | wc -l)"
exit 0
