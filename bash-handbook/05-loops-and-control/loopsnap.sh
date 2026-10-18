#!/usr/bin/env bash
# ============================================================================
# loopsnap.sh —— 证明「for 的词表在进循环之前就已经算完了」
#
#   demo.sh 里这样调：bash loopsnap.sh
#   安全：只在**当前目录**（= demo 的沙箱 work/）里建一个 snap/ 子目录，由 trap 收尾删掉。
#   故意不加 set -e：② 那一段就是要让你看见 cat 的报错原文。
#
# 这条规矩一旦记住，能一次性解释四件平时各记各的事：
#   · 循环里新建的文件本轮看不见（要下一轮 for 才看见）；
#   · 循环里删掉的文件本轮照样迭代，于是 body 里撞上「文件不存在」；
#   · for 中途改那个「装着列表的变量」，本轮不受影响；
#   · find … | while read 与 while read … < <(find …) 的差别不在权限，在「谁是过程」。
# ============================================================================

rm -rf snap
mkdir -p snap
trap 'rm -rf snap' EXIT          # 万一中间出岔子也别留垃圾

printf 'A\n'  > snap/a.txt
printf 'B\n'  > snap/b.txt
printf 'C\n'  > snap/c.txt

echo "=== 1. 词表快照：循环里新建的文件，本轮看不见 ==="
echo "  进循环前 ls 一下：$(ls snap | tr '\n' ' ')"
for f in snap/*.txt; do
    echo "  正在处理 $f"
    # 第一轮就造一个字母序排在最后的新文件：它匹配 *.txt，但本轮不会被访问
    [ "$f" = "snap/a.txt" ] && : > snap/zz_new.txt
done
echo "  → 循环体跑了 3 次，可磁盘上现在有 4 个文件：$(ls snap | tr '\n' ' ')"
echo "  → 为什么 zz_new.txt 没被处理：*.txt 只在**进 for 之前**展开了一次，"
echo "    展开结果（三个词）就是这一轮 for 的全部行程；循环体里再改磁盘改不动它。"
echo
echo "  再开一个 for，同一个glob 现在展开成几个？"
n=0
for f in snap/*.txt; do n=$((n + 1)); done
echo "  → 第二轮的 n = $n（★ 同一个通配，两轮展开结果不同 —— 因为中间有人改了磁盘）"

echo
echo "=== 2. 词表快照的另一面：循环里删掉的文件，本轮照样迭代到它 ==="
rm -rf snap/*                       # 重新铺一遍，保证起点确定
printf 'A\n' > snap/a.txt; printf 'B\n' > snap/b.txt; printf 'C\n' > snap/c.txt
for f in snap/*.txt; do
    echo "  轮到 $f；它现在还在吗 → $([ -e "$f" ] && echo 在 || echo 不在)"
    rm -f "$f"                      # 边迭代边删，删的是**当前**那个
    cat "$f"                        # 于是这一行必然报错
done
echo "  → 上面每一轮都报了 No such file or directory：词表是**名字**的快照，不是**存在性**的快照。"
echo "    「先收集名字、再逐个动手」的循环，动手前都得再 test 一次。"

echo
echo "=== 3. 改「装着列表的变量」也晚了：for 要的是展开后的词，不是那个变量 ==="
list=(one two three)
for word in "${list[@]}"; do
    echo "  word=$word ｜ 现在我把 list 改成 (x y)"
    list=(x y)                      # 改的是变量，不是这一轮 for 的词表
    echo "        改完之后 list 的实际内容 = ${list[*]}（但循环还在按 old 走）"
done
echo "  → 循环里一共打了 3 个 word（one two three），一个 x/y 都没出现。"
echo "    循环结束后 list 的长度 = ${#list[@]}、内容 = ${list[*]}，"
echo "    说明**变量确实被改了**，只是这一轮 for 早就跟它没关系了。"

echo
echo "=== 4. 收口：什么时候「快照」会变成 bug，什么时候它正是你要的 ==="
echo "  ① 想「本轮新产生的文件也一起处理」→ 别硬塞进同一个 for，外面套 while 反复跑，"
echo "     或者干脆用 find（它是边走边吐，见下面 ③ 那条区别）。"
echo "  ② 想「动手前确认它还在」→ for 的词表只保证名字、不保证存在；body 里先 [ -e \"\$f\" ] 再说。"
echo "  ③ 与本章第 13 节直接相关：find … | while read 里的 while 是**另一个进程**，"
echo "     它拿到的是「管道里当下流过来的东西」，所以边生成边消费；"
echo "     而 for 的词表是**同一个进程在开始前**算完的。两者的可见性差别是结构性的，不是快慢问题。"
