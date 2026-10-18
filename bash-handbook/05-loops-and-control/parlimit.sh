#!/usr/bin/env bash
# ============================================================================
# parlimit.sh —— 用循环限制并发数（脚本里最常写错的那个 while）
#
#   demo.sh 里这样调：bash parlimit.sh
#   安全：任务全是本目录下的 sleep（0.2 秒），每支都当场 wait；不起监听端口、不碰别人的进程；
#         结果文件都写进 par/ 子目录，由 trap 删掉。
#   故意不加 set -e：第 5 段要演「某个任务失败」时这套并发循环怎么记账。
#
# ★ 本脚本**不打印**任何耗时、完成顺序、pid —— 它们每跑必变。
#   能被当成证据的只有「跑完了几个」「失败几个」「并发峰值 ≤ 2 吗」这类每次都成立的事实。
# ============================================================================

rm -rf par
mkdir -p par/out par/out2 par/out3
trap 'rm -rf par' EXIT

NAMES=(alpha bravo charlie delta echo foxtrot)   # 六个「任务」，名字故意带一个会撞的 echo
LIMIT=2

job() {                                  # 一个任务：睡 0.2 秒再把名字落到结果文件里
    local name=$1 dir=$2 fail=${3:-0}
    sleep 0.2
    if [ "$fail" = 1 ]; then
        printf '  ★ 任务 %s 故意失败\n' "$name" >&2
        return 1
    fi
    printf '%s\n' "$name" > "$dir/$name.done"
}

echo "=== 1. 先把「串行」作为基准：跑完几个、用了几个进程位 ==="
for name in "${NAMES[@]}"; do job "$name" par/out; done
printf '  串行跑完 → 结果文件数=%s（六个任务，%s 个成功）\n' "$(ls par/out | wc -l)" "$(ls par/out | wc -l)"
echo "  → 串行是**正确性**的基准线：后面两种并发改法都必须得到同样这 6 个文件，否则就是并发写坏了。"

echo
echo "=== 2. 最常用（也最危险）的一版：一把全扔到后台 ==="
peak_all=0
for name in "${NAMES[@]}"; do
    job "$name" par/out2 &
    running=$(jobs -rp | wc -l)
    [ "$running" -gt "$peak_all" ] && peak_all=$running
done
wait
printf '  全部 & 起来 → 观察到的并发峰值=%s ｜ 结果文件数=%s\n' "$peak_all" "$(ls par/out2 | wc -l)"
echo "  → 6 个任务就 6 个并发；60 个就 60 个。数据库连接、磁盘 IO、被限流的 API 都会在这里爆炸，"
echo "    而脚本一声不响 —— 因为它确实「跑完了」。这就是为什么第 3 段那个闸门必须写。"

echo
echo "=== 3. ★ 正解：起新任务之前先数闸门（jobs -rp | wc -l 就是那个闸） ==="
peak=0
for name in "${NAMES[@]}"; do
    while [ "$(jobs -rp | wc -l)" -ge "$LIMIT" ]; do
        sleep 0.05                       # ★ 没有这一行就是忙等：CPU 100% 换 0 点几秒的延迟
    done
    job "$name" par/out3 &
    running=$(jobs -rp | wc -l)
    [ "$running" -gt "$peak" ] && peak=$running
done
wait
printf '  限并发 %s → 观察到的峰值=%s ｜ 结果文件数=%s ｜ 前 3 个结果=%s\n' \
    "$LIMIT" "$peak" "$(ls par/out3 | wc -l)" "$(ls par/out3 | sed 's/\.done//' | sort | head -3 | tr '\n' ' ')"
echo "  → 三处细节，错一处整套就废：① 用 -r（只数还在跑的，否则已完成的僵尸会把闸门顶死）；"
echo "    ② 循环条件用命令替换**重新数一次**，不能把 running 存成变量再用（那是上一轮的数）；"
echo "    ③ 闸门里必须有 sleep 0.05，见第 6 段。"
printf '  三版结果对不对得上（串行=全并=限流）：%s ｜ 有没有超过上限 %s：%s\n' \
    "$([ "$(ls par/out | wc -l)" = "$(ls par/out3 | wc -l)" ] && echo 一致 || echo 不一致)" \
    "$LIMIT" "$([ "$peak" -le "$LIMIT" ] && echo 没有（闸门有效）|| echo ★超了)"

echo
echo "=== 4. 另一把闸门：wait -n（bash 4.3+，等到「任意一个」结束就走，不用轮询） ==="
peak_n=0
for name in "${NAMES[@]}"; do
    while [ "$(jobs -rp | wc -l)" -ge "$LIMIT" ]; do wait -n; done
    job "$name" par/out3 &
    running=$(jobs -rp | wc -l)
    [ "$running" -gt "$peak_n" ] && peak_n=$running
done
wait
printf '  wait -n 版 → 峰值=%s ｜ 结果文件数=%s ｜ 和 sleep 轮询版数量一致吗 → %s\n' \
    "$peak_n" "$(ls par/out3 | wc -l)" "$([ "$peak_n" -le "$LIMIT" ] && echo 是 || echo 否)"
echo "  → 差别不在快慢，在**谁在等**：sleep 0.05 版是你每 0.05 秒问一次；wait -n 是内核结束那一刻推你一下。"
echo "    闸门里的 wait -n 还有个副作用：它顺手把结束的作业**回收**了并返回它的退出码（第 5 段要用）。"

echo
echo "=== 5. ★ 记账：并发时谁失败了？（不 wait 就等于把失败咽下去） ==="
rm -rf par/out4; mkdir -p par/out4
fails=0
for name in "${NAMES[@]}"; do
    while [ "$(jobs -rp | wc -l)" -ge "$LIMIT" ]; do
        wait -n || fails=$((fails + 1))          # ← 失败计数只能在这里加
    done
    if [ "$name" = "charlie" ]; then job "$name" par/out4 1 & else job "$name" par/out4 & fi
done
wait
printf '  故意让 1 个任务失败 → 结果文件数=%s ｜ wait -n 抓到的失败数=%s\n' "$(ls par/out4 | wc -l)" "$fails"
echo "  → 两条都要看清：文件数少 1（**证据在磁盘上**），计数靠 wait -n 的返回值。"
echo "    如果这里写成裸 wait（不带 -n、不接返回值），失败就永远看不见 —— 脚本还是退 0。"

echo
echo "=== 6. 忙等 vs sleep 轮询 vs wait -n：本机不打耗时，只打能复现的东西 ==="
printf '  三种闸门各自的「等待方式」：\n'
printf '    A 无 sleep 的 while [ jobs -rp ... ]  → 每轮都重新起一次 jobs 和 wc，CPU 空转\n'
printf '    B 有 sleep 0.05                       → 最多晚 0.05 秒知道，代价可控\n'
printf '    C wait -n                             → 不轮询，被结束的那个作业直接唤醒\n'
printf '  闸门检查这件事，B 版每轮要起几个外部进程？jobs 是内建（0 个进程），wc 是外部（1 个）：\n'
type jobs wc wait | sed 's/^/    /'
echo "  → 所以「A 版忙等」的真实形状是：**每轮 fork 一个 wc**。这是第 22 节要算的那笔账的一个预告。"

echo
echo "=== 7. 收口：四条能带走的话 ==="
echo "  ① 限并发的最小正确写法只有三行：数闸门（jobs -rp | wc -l）→ 超了就睡一下 → 再 & 起下一个。"
echo "     闸门必须在**起新任务之前**数，不是起完之后；每轮重新数，别把上一次的数存进变量。"
echo "  ② 一定要 -r：已结束的进程在被回收前仍会出现在 jobs 里，闸门会被僵尸顶死（这就是「卡住不动」的常见原因）。"
echo "  ③ 有 wait -n 就别轮询：它同时解决「等得晚」和「失败没人记账」两件事（第 4、5 段）。"
echo "  ④ 并发版的合格判据永远和串行版**同一份对账**：结果文件数、失败数。别只看「跑完了」。"
echo "     第 08 章的锁与幂等讲的是同一件事的另一半：多个 writer 抢同一个文件时怎么办。"
