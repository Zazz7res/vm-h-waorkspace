#!/usr/bin/env bash
# ============================================================================
# file_tests.sh —— 文件测试运算符全表，每个都用沙箱里**真造出来的对象**实测
#
# 这个文件在演什么：
#   -e -f -d -L -r -w -x -s -p -S -b -c -u -g -k -t -nt -ot -ef -O -G
#   这二十来个开关，手册里通常只写「-f 判断是否是文件」，但真正会咬人的是三件事：
#     ① 它们**全部跟随符号链接**（除 -L），断链的符号链接 -e/-f 都是假；
#     ② -r/-w/-x 判的是「你这个进程能不能」，不是「文件权限位里有没有」；
#     ③ -nt/-ot 只看 mtime，-ef 比的是「同一个文件」（设备号+inode），跟内容无关。
#   所以每个都要拿真对象跑一遍，把 rc 打出来。
#
# 怎么造对象：
#   全部在当前目录的 objs/ 子目录里现造：touch / mkdir / ln -s / ln（硬链接）/
#   mkfifo / python3 绑一个 unix socket 文件 / chmod 出 setuid、setgid、sticky 位。
#   设备类用系统里本来就有的 /dev/null（字符设备）、/dev/loop0（块设备）—— 只读判断，不写。
#
# 安全说明：不碰 objs/ 以外的任何东西；setuid 那个文件只造出来**判断位**，从不执行它。
# ============================================================================
set -u

O=objs
rm -rf "$O"; mkdir -p "$O"
cd "$O" || exit 1

# ---- 造对象 ----------------------------------------------------------------
printf 'hello\nworld\n' > hello.txt          # 有内容的普通文件
: > empty.txt                                # 0 字节普通文件
touch plain                                  # 0 字节普通文件（后面 -s 用）
mkdir -p adir giddir stickydir               # 三个目录
ln -s plain link_ok                          # 指向存在目标的符号链接
ln -s nowhere_at_all link_broken             # 断链符号链接（目标不存在）
ln hello.txt hardlink                        # 硬链接：和 hello.txt 同一个 inode
mkfifo afifo                                 # 命名管道
python3 -c 'import socket,sys; s=socket.socket(socket.AF_UNIX); s.bind(sys.argv[1])' ./asock
cp plain suid_file; chmod 4755 suid_file     # setuid 位文件（只用来判断位，从不执行）
cp plain sgid_file; chmod 2755 sgid_file     # setgid 位文件
chmod 1777 stickydir                         # sticky 位（/tmp 就是这一类）
chmod 2750 giddir                            # setgid 位目录
printf 'x\n' > no_read; chmod 000 no_read    # 谁都读不了（我们不是 root）
printf 'x\n' > ro_file;  chmod 400 ro_file   # 只读
printf 'x\n' > exe_file; chmod 755 exe_file  # 可执行
printf 'x\n' > no_exe;   chmod 644 no_exe    # 不可执行
touch old_file; sleep 2; touch new_file      # 差 2 秒的两个 mtime

# ---- 证据：这些对象到底长什么样（用 stat，不用 ls -l，免得把用户名打到屏幕上）
printf '\n──── 0. 沙箱里造出来的对象（stat 证据）\n'
for f in hello.txt empty.txt adir link_ok link_broken hardlink afifo asock \
         suid_file sgid_file stickydir giddir no_read ro_file exe_file no_exe \
         old_file new_file /dev/null /dev/loop0; do
    printf '  %s\n' "$(stat -c '%A %h 大小=%s  inode=%i  mtime=%y  %n' "$f")"
done

# ---- 逐个测 ----------------------------------------------------------------
ftest() {   # ftest '测试表达式'  → 把表达式原样打印，再打印它的退出码
    local expr=$1 rc
    eval "[ $expr ]"; rc=$?
    printf '  %-40s rc=%d\n' "[ $expr ]" "$rc"
}

printf '\n──── 1. 存在性 / 类型（注意谁跟随符号链接）\n'
ftest '-e plain'
ftest '-e link_ok'
ftest '-e link_broken'
ftest '-f plain'
ftest '-f adir'
ftest '-f link_ok'
ftest '-f link_broken'
ftest '-d adir'
ftest '-d plain'
ftest '-L link_ok'
ftest '-L link_broken'
ftest '-L plain'
ftest '-e hello.txt'

printf '\n──── 2. 权限三兄弟：判的是「我这个进程能不能」\n'
ftest '-r hello.txt'
ftest '-r no_read'
ftest '-w no_read'
ftest '-r ro_file'
ftest '-w ro_file'
ftest '-x exe_file'
ftest '-x no_exe'
ftest '-x adir'
ftest '-r /etc/shadow'

printf '\n──── 3. 大小 / 时间戳 / 同一文件\n'
ftest '-s hello.txt'
ftest '-s empty.txt'
printf 'y\n' > oneline.txt
ftest '-s oneline.txt'
ftest 'new_file -nt old_file'
ftest 'old_file -nt new_file'
ftest 'old_file -ot new_file'
ftest 'plain -nt plain'
ftest 'hello.txt -ef hardlink'
ftest 'hello.txt -ef link_ok'
ftest 'link_ok -ef plain'
ftest 'hello.txt -ef plain'
ftest '-O hello.txt'
ftest '-G hello.txt'

printf '\n──── 3b. -nt 的精度实测：两条紧挨着的 touch 也分得出新旧\n'
touch t_a; touch t_b
printf '  stat 证据：t_a mtime=%s   t_b mtime=%s\n' \
    "$(stat -c '%.9Y' t_a)" "$(stat -c '%.9Y' t_b)"
ftest 't_b -nt t_a'
ftest 't_a -nt t_b'
touch t_c; touch -r t_c t_d                  # 用 touch -r 把 mtime 复制成完全相同
printf '  stat 证据：t_c mtime=%s   t_d mtime=%s（一模一样）\n' \
    "$(stat -c '%.9Y' t_c)" "$(stat -c '%.9Y' t_d)"
ftest 't_d -nt t_c'
ftest 't_d -ot t_c'
touch -d '2020-01-01 00:00:00' backdated     # 改旧 mtime，但 inode 是新的
printf '  stat 证据：backdated mtime=%s（比 hello.txt 旧）\n' "$(stat -c '%.9Y' backdated)"
ftest 'backdated -nt hello.txt'

printf '\n──── 4. 特殊文件类型（每一行都是沙箱里的真对象或 /dev 里的真设备）\n'
ftest '-p afifo'
ftest '-p hello.txt'
ftest '-S asock'
ftest '-S afifo'
ftest '-c /dev/null'
ftest '-b /dev/null'
ftest '-b /dev/loop0'
ftest '-c /dev/loop0'
ftest '-L /dev/stdin'
ftest '-e /dev/stdin'
ftest '-c /dev/stdin'

printf '\n──── 5. 三个「位」：setuid / setgid / sticky\n'
ftest '-u suid_file'
ftest '-u plain'
ftest '-g sgid_file'
ftest '-g giddir'
ftest '-g plain'
ftest '-k stickydir'
ftest '-k plain'
ftest '-k /tmp'

printf '\n──── 6. -t：这个 fd 是不是终端（脚本里几乎永远是假）\n'
ftest '-t 1'
ftest '-t 0'
ftest '-t 2'

printf '\n──── 7. 顺带三个高频误用：单参数形式的陷阱\n'
xe=''
printf '  （现在 xe 是空串）\n'
ftest '-n $xe'
ftest '-n "$xe"'
ftest '-z $xe'
ftest '-z "$xe"'
xe='有值'
printf '  （现在 xe 是「有值」）\n'
ftest '-n $xe'
ftest '-z $xe'
printf '  （单参数的通用规则：只要那一个词非空就是真）\n'
ftest '0'
ftest '"0"'
ftest '-o'
ftest '-v'
