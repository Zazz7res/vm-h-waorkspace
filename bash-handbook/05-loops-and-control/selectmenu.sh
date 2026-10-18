#!/usr/bin/env bash
# ============================================================================
# selectmenu.sh —— select 菜单的真实行为（本章唯一由「喂进来的输入」驱动的一节）
#
#   demo.sh 里这样调：bash selectmenu.sh
#   安全：全程不写盘（只读输入、只往屏幕打），所以这个脚本没有沙箱目录要清理。
#   故意不加 set -e：第 5 段要拿 select 循环自己的退出码当证据。
#
# select 是 bash 里少见的「自带交互 UI」的关键字，于是它的坑全在**输入**这一侧：
#   它读的是编号不是内容、非法输入不报错、菜单和提示符走的是 stderr、
#   以及最要紧的一条 —— stdin 一旦 EOF，循环当场结束（脚本被管道/cron/CI 跑时就是这个局面）。
#
# 写法说明：每个 select 都用 <<< 自带一份输入，**不共用脚本的 stdin**。
#   这不是偷懒：同一个 stdin 会被上一个 select 吃到 EOF，后面的 select 就一次都跑不起来
#   —— 那正是第 5 段要认真演一遍的事故，所以前面几段必须先各自把输入管够。
# ============================================================================

# 注意：select 的菜单和 PS3 提示都打在 **stderr**，循环体的 echo 打在 stdout。
# demo 用 runc 跑（2>&1 合并），所以屏幕上是一份完整的混排；这个归属本身就是第 3 段的证据。

echo "=== 1. 最小可用：菜单、编号从 1 开始、变量里进的是「内容」不是编号 ==="
PS3='挑一个 > '
rounds=0
select item in 备份 归档 清理; do
    rounds=$((rounds + 1))
    printf '  循环体：item=[%s] ｜ REPLY=[%s]（你按的那个数字原样留在这里）\n' "$item" "$REPLY"
done <<< $'2\n1\n'
echo "  → 菜单三项对应编号 1 备份 / 2 归档 / 3 清理，select 把**名字**赋给 item；"
echo "    输入喂完（EOF）循环就结束，一共跑了 $rounds 轮 —— 喂几个合法编号就跑几轮。"

echo
echo "=== 2. 非法输入：不报错、不退出，只把 item 变成空字符串 ==="
select mode in fast safe; do
    if [ -z "$mode" ]; then
        printf '  ★ 输入 [%s] 不在菜单里：mode 为空、循环没结束、也没任何报错\n' "$REPLY"
    else
        printf '  合法：mode=[%s]（REPLY=[%s]）\n' "$mode" "$REPLY"
    fi
done <<< $'9\nzzz\nfast\n1\n'
echo "  → 判据只有一条：**变量为空 = 刚才那下没选上**。别指望它抛错、也别指望它自己退出。"
echo "    注意 'fast' 是菜单里的**名字**，按名字输入同样算非法（REPLY=[fast] → mode 为空）：只认编号。"
echo "    ★ 顺便看一眼提示符：这一段还是「挑一个 > 」—— PS3 是**全局变量**，第 1 段设的会一直跟着后面的"
echo "    每一个 select（默认值是 '#? '）。想换就得每次自己设，别以为它跟着菜单走。"

echo
echo "=== 3. 空行 = 重打菜单、循环体不进；stderr 里才是菜单和提示符 ==="
printf '  先数一下：这一段的 stdout 有几行？（下面这条 printf 走 stdout）\n'
select x in a b; do
    printf '    进了循环体：x=[%s] REPLY=[%s]\n' "$x" "$REPLY"
done <<< $'\n\n1\n'
echo "  → 两个空行让菜单重打了两遍，可循环体只进了一次：空行 = 「重显菜单」，不是「选了个空值」。"

echo
echo "=== 4. 拿「变量为空」做校验 + 重试上界：select 在生产里唯一的正确姿势 ==="
attempts=0
choice=""
select pick in 保留最近3份 保留最近7份 全部删除; do
    if [ -n "$pick" ]; then
        choice="$pick"
        break
    fi
    attempts=$((attempts + 1))
    printf '  第 %s 次非法输入（REPLY=[%s]），再问一次\n' "$attempts" "$REPLY"
    if [ "$attempts" -ge 3 ]; then
        printf '  ★ 连错 3 次，放弃退出（生产脚本必须有这个上界）\n'
        break
    fi
done <<< $'7\n9\n2\n'
printf '  最终选择=[%s] ｜ 非法次数=%s ｜ 有没有落进「全部删除」→ %s\n' \
    "$choice" "$attempts" "$([ "$choice" = "全部删除" ] && echo 有 || echo 没有)"

echo
echo "=== 5. ★ 输入不够 / 立刻 EOF：循环安静地一次都不跑，还返回非 0 ==="
ran=0
select y in p q; do
    ran=$((ran + 1))
done <<< "" 
printf '  喂一个空行：循环体跑了 %s 轮\n' "$ran"
select z in p q; do :; done < /dev/null
rc=$?
printf '  直接 EOF（< /dev/null）：循环退出码 = %s\n' "$rc"
echo "  → ★ 这一条是给非交互场景准备的：CI / cron / 管道里跑 select 脚本，stdin 不是终端，"
echo "    循环会因为 EOF 结束而不是因为用户选完了；退出码 $rc 会被 set -e 当成失败。"
echo "    所以「脚本里有 select」和「脚本会被自动跑」这两件事不能同时想当然。"

echo
echo "=== 6. 收口：select 的四条规矩 ==="
echo "  ① 菜单 + PS3 提示 → stderr；循环体输出 → stdout。想只留一个就得分别重定向，"
echo "     而随手写 2>/dev/null 会把整个菜单抹掉（屏幕上什么都不剩，看着像卡死）。"
echo "  ② item 是名字、REPLY 是你按的原始字符；非法输入（包括按名字）→ item 为空、不报错、不退出，"
echo "     校验代码必须自己写（第 4 段那个 attempts 上界就是最小实现）。"
echo "  ③ 编号固定从 1 开始，菜单项顺序一变，「按 2」的含义就跟着变 —— 别把编号写进文档或配置。"
echo "  ④ 非交互喂 stdin 完全可行，但要管好 EOF 与退出码；真正需要「选择」的场合，"
echo "     更稳的做法是把它做成参数（getopts，第 08 章），select 只当交互糖衣。"
