#!/usr/bin/env bash
# ============================================================================
# untilretry.sh —— until 的真实用法：等一个「还不知道什么时候好」的东西
#
#   demo.sh 里这样调：bash untilretry.sh
#   安全：只在**当前目录**（= demo 的沙箱 work/）里建 until/ 子目录，跑完由 trap 删掉；
#         唯一的后台进程是一个 0.2 秒的 sleep（负责把 ready 文件 touch 出来），当场 wait。
#   故意不加 set -e：超时那一支就是要拿非 0 退出码当证据。
#
# ★ 本机不拿「真实等了多久」当证据：耗时每跑必抖，所以第 3 段用**确定性的计数**
#   模拟「第 3 轮才 ready」，而真实轮询那一支只报「成功/超时」这两种可复现的事实。
# ============================================================================

rm -rf until
mkdir -p until
trap 'rm -rf until' EXIT

echo "=== 1. until 和 while 是镜像：条件互为反面，轮数才一样 ==="
i=0
while [ "$i" -lt 3 ]; do i=$((i + 1)); done
printf '  while [ i -lt 3 ]（条件真才进）：跑了 %s 轮，i=%s\n' "$i" "$i"
j=0
until [ "$j" -ge 3 ]; do j=$((j + 1)); done
printf '  until  [ j -ge 3 ]（条件假才进）：跑了 %s 轮，j=%s\n' "$j" "$j"
w=0
until [ "$w" -lt 3 ]; do w=$((w + 1)); done
printf '  ★ 反面写法 until [ w -lt 3 ]：只跑了 %s 轮就停了（0 -lt 3 一进来就真 → 立刻满足停止条件）\n' "$w"
echo "  → 一句话：until = while not。它俩的语法差别只是「第一次判断用哪个真假」，"
echo "    读代码时的差别是**意图**：until 后面跟的是「我等到什么就算了」的停止条件 —— 写反一次就是"
echo "    上面第三行那种「一次都不干就收工」，或者第 2 段那种停不下来的死循环。"

echo
echo "=== 2. 一次都不跑的情形，和它的退出码（这条最容易被想当然） ==="
k=0
until [ "$k" -ge 0 ]; do k=$((k + 1)); done
rc=$?
printf '  停止条件一进来就成立（0 -ge 0 为真）：跑了 %s 轮，循环整体退出码=%s\n' "$k" "$rc"
echo "  → 循环体一轮都没跑，退出码仍是 0：bash 的循环**不会**因为「什么都没干」而报错。"
echo "    所以重试类循环必须自己带上「等到了吗」的判据（看第 4 段拿文件存在性判断，而不是拿退出码）。"

echo
echo "=== 3. 确定性的重试环：第 3 轮才 ready（用计数模拟，保证每次跑逐字一样） ==="
tries=0
until [ -e until/ready.flag ] || [ "$tries" -ge 5 ]; do
    tries=$((tries + 1))
    # 「服务」本机由计数扮演：到第 3 轮才把 flag 立起来
    [ "$tries" -eq 3 ] && : > until/ready.flag
    printf '  第 %s 轮：ready.flag 在吗 → %s\n' "$tries" "$([ -e until/ready.flag ] && echo 在 || echo 不在)"
done
printf '  收工：tries=%s ｜ 等到了吗 → %s ｜ 有没有撞上上界 → %s\n' \
    "$tries" "$([ -e until/ready.flag ] && echo 是 || echo 否)" "$([ "$tries" -ge 5 ] && echo 撞了 || echo 没撞)"
echo "  → 注意循环条件里的两半：**等的是什么** + **最多等多久**。少了后半句就是一个死循环。"

echo
echo "=== 4. 撞上界的那一支：东西永远不 ready，循环必须自己认输 ==="
rm -f until/ready.flag
limit=4
tries=0
until [ -e until/ready.flag ] || [ "$tries" -ge "$limit" ]; do
    tries=$((tries + 1))
done
if [ -e until/ready.flag ]; then
    printf '  等到了（%s 轮）\n' "$tries"
else
    printf '  ★ 到上界还是没等到：轮询了 %s 轮，按失败处理（下面把退出码交出去）\n' "$tries"
fi
printf '  这一支的判定结果 = %s（%s 轮 × 上界 %s）\n' \
    "$([ -e until/ready.flag ] && echo 成功 || echo 超时)" "$tries" "$limit"

echo
echo "=== 5. 真·异步版：后台进程 0.2 秒后立 flag，循环带 sleep 轮询 ==="
( sleep 0.2; : > until/ready.flag ) &
producer=$!
tries=0
until [ -e until/ready.flag ]; do
    tries=$((tries + 1))
    sleep 0.05                          # ★ 这一行决定你是「等」还是「把 CPU 点燃」
    [ "$tries" -ge 200 ] && { printf '  ★ 撞上 200 轮上界，认输\n'; break; }
done
wait "$producer"
printf '  等到了吗 → %s ｜ 轮询了 %s 次（这个数字每次跑都可能不一样，★ 所以本机只当过程看，不印进 README 的证据里）\n' \
    "$([ -e until/ready.flag ] && echo 是 || echo 否)" "<每跑必变>"
echo "  → 屏幕上不打轮询次数（它取决于调度），只打「等到了 = 是」这种每次都成立的事实。"

echo
echo "=== 6. 忙等 vs 带 sleep 的轮询：代价是能用数字看出来的（这里打的是可复现的量） ==="
printf '  忙等写法（条件里不放 sleep）：循环体每轮都只做一次 [ -e ]，'
printf '但 CPU 一直被占满 —— 本机用 0.2 秒的时间窗测一下轮询次数：\n'
( sleep 0.2; : > until/busy.flag ) &
p2=$!
busy=0
until [ -e until/busy.flag ]; do busy=$((busy + 1)); done
wait "$p2"
printf '    忙等这一支在这 0.2 秒里空转了 %s 次（次数巨大且每跑必变 → ★ 本机不印具体数字）\n' "<很多>"
sl=0
( sleep 0.2; : > until/slow.flag ) &
p3=$!
until [ -e until/slow.flag ]; do sl=$((sl + 1)); sleep 0.05; done
wait "$p3"
printf '    带 sleep 0.05 的这一支：轮询次数是「耗时 / 0.05」量级（同样每跑必变，不印数字）\n'
echo "  → 这一段的结论不靠数字：**忙等 = 用一整个核换一个不确定的延迟**；"
echo "    只要循环体里有 sleep，你的轮询就变成了「最多晚半秒知道」，代价可控。"
echo "    真要知道「进程结束」这件事，压根不用轮询：wait 它（第 18 节），或者用 inotifywait（不在本章范围）。"

echo
echo "=== 7. 收口：三条能带走的话 ==="
echo "  ① until 用来写「等到 X 为止」，读起来比 while [ ! -e X ] 顺，仅此而已 —— 语义上和 while not 等价。"
echo "  ② 任何重试环都必须**两个条件一起写**：等什么 + 最多等多久（第 3、4 段那对 $tries / $limit）。"
echo "     只有前一半的 until 是本机见过的最常见死循环；循环退出码是 0，不会替你报错。"
echo "  ③ 每跑必变的数字（耗时、轮询次数、pid）不要印成证据；改成「等到了吗」「撞没撞上界」这类布尔，"
echo "     它们才是能逐字核对、也是换台机器还成立的东西。"
