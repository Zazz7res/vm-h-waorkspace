#!/usr/bin/env bash
# ============================================================================
# pids.sh —— 「谁在跑我」的证据卡
#
# 这一页只做一件事：把这个脚本此刻运行在哪个进程里、它叫什么名字、
# 它是交互还是非交互，全部打印出来，并且给一个变量赋值，
# 好让外面检查「外面看得见这个变量吗」。
#
# 三种跑法都用它：bash pids.sh / ./pids.sh / source pids.sh
# 看输出的差异，比看任何文字解释都管用。
# ============================================================================

# $0：脚本「以为」自己叫什么。
#   ./pids.sh   跑 → 0=./pids.sh
#   bash pids.sh 跑 → 0=pids.sh      （bash 把你传给它的第一个参数当脚本名）
#   source       跑 → 0=bash 或 0=demo.sh（继承「执行 source 的那个 shell」的 $0）
echo "  0(名字)      = [$0]"

# $$ 是「当前 shell 的 PID」，但有个大坑：它在子 shell 里**不会变**，
# 永远是最初那个 shell 的 PID。想看真正的进程号要用 $BASHPID。
# 这一行故意把两个都打出来，让你看到顶层时它们相等。
echo "  \$\$(PID)     = [$$]      \$BASHPID = [$BASHPID]"
echo "  PPID(父进程) = [$PPID]"

# BASH_VERSION 只有 bash 才有；dash（也就是本机的 /bin/sh）里它是空的。
# 这是判断「我到底跑在哪个 shell 里」最快的土办法。
echo "  BASH_VERSION = [${BASH_VERSION:-（空：我不是 bash！）}]"

# $- 是「当前 shell 打开的开关字母」。里面有 i 就是交互式 shell。
# 脚本里跑的时候一定是非交互 —— 这决定了 alias、提示符、作业控制都不生效。
case "$-" in
    *i*) echo "  交互性       = 交互式（\$- 含 i）" ;;
    *)   echo "  交互性       = 非交互（\$- 不含 i）" ;;
esac

# 给一个普通 shell 变量赋值：只有 source 时外面才看得见。
SHELLVAR=脚本里赋的值

# 再给一个 export 变量：即使 source，也只在「那个 shell」里存在；
# 它能不能被子进程看见，见 envcheck.sh。
EXPORTEDVAR=脚本里导出的值
export EXPORTEDVAR

echo "  赋值完成     = SHELLVAR=[$SHELLVAR]"
