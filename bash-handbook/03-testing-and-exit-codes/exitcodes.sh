#!/usr/bin/env bash
# ============================================================================
# exitcodes.sh —— 「退出码是每条命令自己定的约定」实测表
#
# 这个文件在演什么：
#   把最常碰到的六类命令/机制的退出码全部真跑一遍，一个数字一个数字地打出来：
#     grep 的 0/1/2、diff 的 0/1/2、test/[/[[ 的 0/1/2、exit 的取值域、
#     126 家族的三种坏法、127、以及「被信号杀死 = 128+signum」。
#
# 为什么要演：
#   新手背的是「0 成功、非 0 失败」。这句只对了一半：
#   grep 的 1 是「我很正常地告诉你没找到」，grep 的 2 才是「我根本没法干活」。
#   两者都叫「非 0」，但一个是业务结论、一个是故障。这种区别只能靠实测拿到，
#   背口诀背不出来。
#
# 安全说明：
#   所有文件都在**当前目录**（demo.sh 已经把 cwd 切进 mktemp 沙箱）里现造，
#   前缀统一 ec_，跑完不往外写任何东西，也不改任何已有文件。
#   信号那一段只杀「刚刚为自己 fork 出来的那个 bash」，不杀任何别人的进程。
# ============================================================================
set -u

# ----------------------------------------------------------------------------
# 两个小工具函数：row 只报退出码，show 连输出一并报（输出缩进 6 格）
# ----------------------------------------------------------------------------
row() {                          # row '说明' 命令 参数…
    local desc=$1; shift
    "$@" >/dev/null 2>&1
    printf '  %-50s rc=%d\n' "$desc" "$?"
}
show() {                         # show '说明' 命令 参数…
    local desc=$1; shift
    local out rc
    # 注意这里分成两行写（local out; out=$(...)）而不是 local out=$(...)：
    # 后者会把命令替换的退出码换成 local 的 0，详见 status_chain.sh 和 README。
    out=$("$@" 2>&1); rc=$?
    printf '  %s\n' "$desc"
    if [ -n "$out" ]; then
        printf '%s\n' "$out" | sed 's/^/        /'
    else
        printf '        （无任何输出）\n'
    fi
    printf '        → rc=%d\n' "$rc"
}

# ============================================================================
printf '\n──── A. grep：0=找到了，1=没找到（正常答复），2=没法干活\n'
# ============================================================================
printf 'alpha\nbeta\ngamma\n' > ec_a.txt
printf 'alpha\ndelta\n'       > ec_b.txt
mkdir -p ec_dir

row  'grep -q alpha ec_a.txt      命中'              grep -q alpha ec_a.txt
row  'grep -q zzz   ec_a.txt      没命中'            grep -q zzz ec_a.txt
show 'grep alpha ec_MISSING.txt   文件不存在'        grep alpha ec_MISSING.txt
show 'grep "[" ec_a.txt           正则写错'          grep '[' ec_a.txt
show 'grep -c zzz ec_a.txt        没命中时打印 0'    grep -c zzz ec_a.txt
row  'grep -q alpha ec_a.txt ec_MISSING.txt 一半好一半坏' grep -q alpha ec_a.txt ec_MISSING.txt
row  'grep -q alpha ec_dir        参数是个目录'      grep -q alpha ec_dir

# ============================================================================
printf '\n──── B. diff：0=一样，1=不一样（正常答复），2=没法干活\n'
# ============================================================================
row  'diff -q ec_a.txt ec_a.txt   完全相同'          diff -q ec_a.txt ec_a.txt
show 'diff -q ec_a.txt ec_b.txt   有差异'            diff -q ec_a.txt ec_b.txt
show 'diff -q ec_a.txt ec_MISSING.txt  文件不存在'   diff -q ec_a.txt ec_MISSING.txt
row  'cmp -s ec_a.txt ec_b.txt    有差异（-s 静音）' cmp -s ec_a.txt ec_b.txt

# ============================================================================
printf '\n──── C. test / [ ]：0=真，1=假，2=用错了（注意 2 不是「假得厉害」）\n'
# ============================================================================
row 'test 1 -eq 1              真'                   test 1 -eq 1
row 'test 1 -eq 2              假'                   test 1 -eq 2
row 'test -z ""                真'                   test -z ""
show 'test 1 -zz 2              运算符不存在'        test 1 -zz 2
show 'test -eq 1                一元运算符缺左操作数' test -eq 1
show '[ 1 -eq 2 -zz ]           多余参数'            [ 1 -eq 2 -zz ]

# ============================================================================
printf '\n──── D. exit 的取值域：只保留低 8 位（mod 256）\n'
# ============================================================================
probe() {                        # probe '描述' '要跑的代码'
    printf '  %-34s → ' "$1"
    ( eval "$2" ) 2>&1
    printf '        rc=%d\n' "$?"
}
probe '( exit 0 )'    '( exit 0 )'
probe '( exit 7 )'    '( exit 7 )'
probe '( exit 255 )'  '( exit 255 )'
probe '( exit 256 )'  '( exit 256 )'
probe '( exit 300 )'  '( exit 300 )'
probe '( exit 511 )'  '( exit 511 )'
probe '( exit 999 )'  '( exit 999 )'
probe '( exit -1 )'   '( exit -1 )'
probe '( exit abc )'  '( exit abc )'

# ============================================================================
printf '\n──── E. 126 家族（找到但执行不了）与 127（根本没找到）\n'
# ============================================================================
printf 'echo 我其实能跑\n' > ec_noexec.sh
printf '#!/bin/no_such_interp_03\necho hi\n' > ec_bad.sh
chmod +x ec_bad.sh

probe 'nosuch_program_03（不存在）' 'nosuch_program_03 --version'
probe './ec_dir（是目录）'          './ec_dir'
probe './ec_noexec.sh（没有 +x）'   './ec_noexec.sh'
probe './ec_bad.sh（shebang 指错）' './ec_bad.sh'
probe 'bash ec_noexec.sh（读就行）' 'bash ec_noexec.sh'

# ============================================================================
printf '\n──── F. 被信号杀死：退出码 = 128 + signum（反查用 kill -l）\n'
# ============================================================================
for s in HUP INT KILL SEGV PIPE USR1 TERM; do
    n=$(kill -l "$s")
    { bash -c "kill -$s \$BASHPID"; rc=$?; } 2>/dev/null
    printf '  kill -%-5s signum=%-3s 退出码=%-4s 128+signum=%-4s 反查名=%s\n' \
        "$s" "$n" "$rc" "$((128 + n))" "$(kill -l "$rc")"
done

# ============================================================================
printf '\n──── G. 同样是非 0，含义完全不同（没有「大小」可言）\n'
# ============================================================================
row 'false                     约定俗成的 1'         false
row 'grep -q zzz ec_a.txt      没找到 → 1'           grep -q zzz ec_a.txt
row 'diff -q ec_a.txt ec_b.txt 不一样 → 1'           diff -q ec_a.txt ec_b.txt
row 'grep alpha ec_MISSING.txt 干不了活 → 2'         grep alpha ec_MISSING.txt
# 用 { …; } 2>/dev/null 把「Killed」这条由父 shell 打的进程报告压掉，
# 否则每行都会带一个每次都变的 pid，没法逐行核对。
{ bash -c 'kill -KILL $BASHPID'; rc_kill=$?; } 2>/dev/null
printf '  %-44s rc=%d（=128+9）\n' '被 SIGKILL(9) 杀死 →' "$rc_kill"
row "( exit 200 )              自己定的 200"        bash -c 'exit 200'
printf '  ↑ grep -c 打印的「0」是正确答案，退出码却是 1：输出对 ≠ 退出码 0\n'
