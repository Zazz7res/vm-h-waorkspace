#!/usr/bin/env bash
# ============================================================================
# slowpipe.sh —— 管道缓冲实测：中间那一段「多久才把字吐给下游」
#
#   demo.sh 里这样调：bash slowpipe.sh
#   看点：上游先写 A、睡 0.35 秒、再写 B。下游记录每一行的到达时刻（毫秒）。
#     · 中间段「块缓冲」→ A、B 两条几乎同时到达（都挤在 350ms 之后）
#     · 中间段「行缓冲」→ A 在 ~0ms 就到，B 在 ~350ms 到
#   同一份数据、同一个下游，只换中间那一段的缓冲方式，时序就变了。
#
#   本文件不起后台进程、不写文件（除了 case 标签里的临时比较），只在当前目录跑。
# ============================================================================

SLEEP=0.35

# 把 $EPOCHREALTIME（形如 1791175962.627304）换成整数毫秒，方便做减法
to_ms() {
    local t=$1
    printf '%d' $(( ${t%.*} * 1000 + 10#${t#*.} / 1000 ))
}

run_case() {
    local tag=$1 mid=$2
    local base
    base=$(to_ms "$EPOCHREALTIME")
    printf '  --- %s（中间段命令：%s）---\n' "$tag" "$mid"
    ( echo A; sleep "$SLEEP"; echo B ) | eval "$mid" | while IFS= read -r line; do
        local now
        now=$(to_ms "$EPOCHREALTIME")
        printf '      收到 [%s] 距开始 %d 毫秒\n' "$line" $(( now - base ))
    done
}

printf '下游是 while read（一收到行就打印时刻），上游是「A → 睡 %s 秒 → B」。\n' "$SLEEP"
run_case '①  中间只有 cat：它不缓冲，来多少搬多少' 'cat'
run_case '②  中间放 sed -n p：stdout 不是终端 → 整块攒满才吐' 'sed -n p'
run_case '③  中间放 stdbuf -oL sed -n p：强制行缓冲' 'stdbuf -oL sed -n p'
run_case '④  中间放 sed -u：sed 自己的行缓冲开关，效果同上' 'sed -u -n p'
run_case '⑤  中间放 grep .：和 sed 一样块缓冲' 'grep .'
run_case '⑥  中间放 grep --line-buffered .：grep 的自带开关' 'grep --line-buffered .'

printf '\n对照结论：②⑤ 的 A 都被推迟到 350ms 之后；①③④⑥ 的 A 几乎立刻到达。\n'
printf '这就是「明明上游在动，下游一个字都不出」的根源：缓冲在中间那一段，不在管道本身。\n'
printf '代价：行缓冲会让每一行都触发一次 write，长管道上会明显变慢（本节只看时序，不测吞吐）。\n'

printf '\n== 另一半真相：管道容量有限，下游不读的话上游会被堵住 ==\n'
printf '   下游一律先睡 0.6 秒再开始读；比较「上游写完的时刻」和「下游读完的时刻」。\n'
printf '   （两段时间都走 stderr 打印：如果走 stdout 就会被下一段的 cat 吞掉，屏幕上什么都看不见。）\n'
block_case() {
    local label=$1 count=$2 base
    base=$(to_ms "$EPOCHREALTIME")
    { seq 1 "$count"
      printf '   %s：上游（seq）写完于 +%d 毫秒\n' "$label" $(( $(to_ms "$EPOCHREALTIME") - base )) >&2
    } | { sleep 0.6
      cat >/dev/null
      printf '   %s：下游（读完）于 +%d 毫秒\n' "$label" $(( $(to_ms "$EPOCHREALTIME") - base )) >&2
    }
}
block_case '小数据(50 行, 约 150 字节)' 50
block_case '大数据(120000 行, 远超管道缓冲)' 120000
printf '   读法：小数据那两行时间差 ≈ 600 毫秒（上游一开机就写完了，剩下的时间在下游睡觉）；\n'
printf '         大数据那两行时间差 ≈ 0（上游被管道堵住，只能等下游开始读才写得完）。\n'
printf '   结论：管道是有容量的缓冲区，写满就堵；第 17 节的 SIGPIPE 就是「堵到没人接」的另一种死法。\n'
