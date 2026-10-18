#!/usr/bin/env bash
# ============================================================================
# jobwait.sh —— 脚本里的作业控制：& / $! / jobs / wait / disown / kill -0
#
#   demo.sh 里这样调：bash jobwait.sh
#   安全：本脚本起的每一个后台进程都当场 wait 或 kill 掉，第 7 段再用 kill -0 逐个自证
#         「一个都没留下」；不起任何监听端口、不碰别人的进程（所有 pid 都来自本脚本自己的 $!）。
#   故意不加 set -e：wait 的非 0 退出码就是本章要看的证据。
#
# ★ pid 一律不上屏（打成果果 <pid>），因为它**每跑必变**：README 要逐字核对，
#   而「这个数字是多少」从来不是重点，重点全是「它和谁相等」「它退出码是几」这类关系。
#   同理第 2 段不印 jobs -n：它取决于「你什么时候问」，本机两次跑就能不一样，不配当证据。
# ============================================================================

echo "=== 1. & 起来的到底是什么：一个新进程，还是同一个 shell 里的「另一条线」？ ==="
sleep 0.4 &
bg=$!
printf '  本 shell 的 pid 与后台 sleep 的 pid 相等吗 → %s（不相等 = 真的多起了一个进程）\n' \
    "$([ "$$" = "$bg" ] && echo 相等 || echo 不相等)"
ppid=$(awk '{print $4}' "/proc/$bg/stat" 2>/dev/null || echo 读不到)
printf '  后台进程的**父进程 pid** 等于本 shell 的 $$ 吗 → %s ｜ 所以它只是「不等」，不是「换了爹」\n' \
    "$([ "$ppid" = "$$" ] && echo 是 || echo 否)"
echo "  → & 之后子进程仍挂在本 shell 下：本 shell exit，它就变成孤儿（第 5 段 disown 演的就是这个）。"
wait "$bg"

echo
echo "=== 2. jobs 的几种问法：同一份作业表，不同的输出形状 ==="
sleep 0.5 & sleep 0.5 & sleep 0.5 &
printf '  jobs 默认（带 [+]/[-] 与命令行原文）：\n'
jobs | sed 's/^/      /'
printf '  jobs -l（比默认多一列 pid；只折 pid 那一列，[1]/[2]/[3] 和命令行原文都留着）：\n'
jobs -l | sed -E 's/^(\[[^]]*\][-+]?) +[0-9]+/\1 <pid>/; s/^/      /'
printf '  jobs -p（只有 pid）行数=%s ｜ jobs -rp（只有 running 的 pid）行数=%s ｜ jobs -sp（stopped）行数=%s\n' \
    "$(jobs -p | wc -l)" "$(jobs -rp | wc -l)" "$(jobs -sp | wc -l)"
echo "  → 数并发为什么一律用 jobs -rp | wc -l：-r 滤掉已结束的、-p 只吐 pid，"
echo "    行数就是「还在跑的个数」，跟命令行文本、跟 [+]/[-] 的排版都没关系。"
echo "    （还有个 jobs -n 只报「自上次问以来状态变过的」，答案取决于你什么时候问 —— 本机不拿它当证据。）"
wait

echo
echo "=== 3. wait 的三种结局：成功 0 ｜ 作业自己失败了 ｜ 被信号杀了 ==="
sleep 0.05 & wait $!; printf '  正常跑完的后台：wait 退出码=%s\n' "$?"
( exit 7 ) & pe=$!; wait "$pe"; printf '  子进程自己 exit 7：wait 退出码=%s ← ★ 脚本里拿到后台退出码的**唯一**办法\n' "$?"
sleep 30 & pk=$!; kill "$pk"; wait "$pk"; rc_pk=$?
printf '  被 SIGTERM 杀掉：wait 退出码=%s ｜ kill -l 15 的名字=[%s] ｜ 143 = 128 + 15 → %s\n' \
    "$rc_pk" "$(kill -l 15)" "$([ "$rc_pk" = 143 ] && echo 是 || echo 否)"
echo "  → 对照：& 之后不 wait，\$? 是「起作业这条命令自己」的退出码（永远 0）；"
echo "    作业干了什么、死得多惨，全在 wait 的返回值里 —— 这一条和第 04 章的 141/143 是同一套记账。"

echo
echo "=== 4. wait 的两次「什么都没等到」：重复 wait 与 wait 一个不是自己孩子的 pid ==="
sleep 0.05 & pr=$!
wait "$pr" >/dev/null; r1=$?
wait "$pr" >/dev/null 2>&1; r2=$?
printf '  第一次 wait=%s，第二次 wait=%s（★ 同一个 pid 再 wait 一次仍返回 0，不报错 —— 它在翻旧账）\n' "$r1" "$r2"
w1=$(wait "$PPID" 2>&1 >/dev/null); r3=$?
printf '  wait 一个**不是自己孩子**的 pid（这里用 $PPID：只是问一句，不碰它）：退出码=%s ｜ 原文=[%s]\n' \
    "$r3" "${w1//pid $PPID/pid <pid>}"
w2=$(wait 4294967000 2>&1 >/dev/null); r4=$?
printf '  wait 一个大得离谱的数：退出码=%s ｜ 原文=[%s]\n' "$r4" "$w2"
echo "  → ★ 两条都是「没等到」，但 bash 给的退出码不一样：127 是「它确实是个 pid，只是不是我的孩子」，"
echo "    1 是「这个数根本不构成一个合法作业号」。写脚本时想拿这个码做分支的话，先分清这两层。"
echo "  → 三种「返回 0」要分清：真成功、翻旧账（已经 wait 过）、以及作业表空时 wait 不带参数。"
echo "    「重复 wait 得 0」特别容易被当成「等到了一次成功」，其实它什么都没等。"

echo
echo "=== 5. disown：从作业表里摘掉，但进程照跑（等于把收尾权交了出去） ==="
sleep 30 & pd=$!
disown "$pd"
printf '  disown 之后 jobs -rp 行数=%s（表里已经没它了）｜ kill -0 探它 → %s\n' \
    "$(jobs -rp | wc -l)" "$(kill -0 "$pd" 2>/dev/null && echo 还活着 || echo 没了)"
wait                                  # 不带参数：只等**作业表里**的，disown 掉的那个不在表里
printf '  wait（无参数）返回后再探一次 → %s（★ 这是 30 秒的 sleep，wait 一回来它当然还在跑 = wait 压根没等它）\n' \
    "$(kill -0 "$pd" 2>/dev/null && echo 还在跑 || echo 已经结束)"
echo "  → ★ disown 只动 bash 的作业表，不动进程表。之后 wait / jobs / %1 都再也看不见它，"
echo "    但脚本退出时它照样活着（被 init 收养）。这就是本手册要求「后台作业必须当场收尾」的原因。"
kill "$pd" 2>/dev/null; wait "$pd" 2>/dev/null

echo
echo "=== 6. kill -0：不投递信号的那次调用，是最好的探活（第 20 节循环等进程就靠它） ==="
# probe <pid> <这一行叫什么>：退出码和报错原文都要，但 pid 数字一律折成 <pid>。
# ★ 这里不能用 `$(kill ... | sed ...)` 折：管道把 $? 变成 sed 的 0，kill 的真实退出码就丢了。
probe() {
    local out rc
    out=$(kill -0 "$1" 2>&1 >/dev/null); rc=$?
    out=${out//([1-9]*)/(<pid>)}
    printf '  %s：退出码=%s ｜ 原文=[%s]\n' "$2" "$rc" "$out"
}
sleep 0.2 & pl=$!
probe "$pl" "活着时 kill -0（信号 0 只是查一下，不投递任何东西）"
wait "$pl"
probe "$pl" "自己家的进程已结束并被回收后"
probe 1 "别人的进程（pid 1，本脚本既不动它也发不到它）"
echo "  → ★ 后两条的退出码都是 1，但含义**不一样**：一条是「没这个进程」，一条是「有，但轮不到你」。"
echo "    循环等进程结束时如果只看 rc≠0 就判定「它退出了」，遇到权限类情况会提前收工 ——"
echo "    所以真要用它当循环条件，得把报错原文一起留着，或者干脆 wait 你自己的 pid（第 3 段）。"
echo "  → kill -0 答的是**内核视角**：进程刚死、还没被父进程回收时它是个僵尸，kill -0 仍然说「在」。"

echo
echo "=== 7. 收口：先自证一个进程都没留下，再给判据 ==="
left=0
seen=0
for pid in "$bg" "$pe" "$pk" "$pr" "$pd" "$pl"; do
    seen=$((seen + 1))
    kill -0 "$pid" 2>/dev/null && left=$((left + 1))
done
printf '  本脚本经手过的 pid 共 %s 个 ｜ 此刻还活着的 = %s（必须是 0）\n' "$seen" "$left"
printf '  作业表里剩下的行数 = %s ｜ jobs 输出 =[%s]\n' "$(jobs -p | wc -l)" "$(jobs)"
echo "  ① 后台作业的退出码**只有 wait 能拿到**；不 wait 就等于把失败咽下去了。"
echo "  ② wait 返回 0 有三种成因（真成功 / 翻旧账 / 没孩子可等），所以别拿「wait 0」当「活干成功了」的唯一证据，"
echo "     要么显式检查你 wait 的那个 pid，要么让作业自己把结果写进文件。"
echo "  ③ disown 是「放弃收尾权」，不是「让进程更安全」；要留长命进程就得自己写 trap EXIT。"
echo "  ④ 数并发、探存活，只用 jobs -rp 与 kill -0 这两把尺，别 grep jobs 的文本。"
