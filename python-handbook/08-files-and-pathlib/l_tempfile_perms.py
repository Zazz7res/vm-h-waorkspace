#!/usr/bin/env python3
# ============================================================================
# 取证脚本 l_tempfile_perms.py —— 第 08 章第 13 讲：临时文件的四种粒度，和它们的权限位
#
# 只证一件事：临时文件的 0600 / 0700 **不是风格，是安全属性**。/tmp 人人可写、人人能列出目录名，普通
# open() 建出来是 0644，任何本地用户都能在你写之前把它读走；tempfile 建出来永远 0600，而且名字不是
# 「算出来的」，是用 O_EXCL **抢来的**。两条合起来才叫安全。
#
# 屏幕纪律：tempfile 的随机名字一个都不上屏（只打「形状」和布尔判断），所以两遍跑 diff 为空；异常原文里
# 带沙箱绝对路径的那种，只打类型 / errno / strerror。实验全在 temp-demo/ 现造现删，umask 只改本进程。
# ============================================================================
import inspect
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import warnings
from pathlib import Path

WORK = "temp-demo"
OUTSTANDING = []                  # 还欠着清理的路径（用来抓「忘了删」）
REGISTERED = []                   # 本次向 tempfile 要过的每个路径，只增不减，末尾对账
TEMPDIR_REAL = os.path.realpath(tempfile.gettempdir())


def track(p):                     # 登记 tempfile 交给我们的路径：真名永不上屏，只进清单
    OUTSTANDING.append(p)
    REGISTERED.append(p)


def untrack(p):
    OUTSTANDING.remove(p)


def perm(p):                      # 权限只打 oct(S_IMODE(...))：滤掉会变动的字段，两遍跑才比得上
    return oct(stat.S_IMODE(os.stat(p).st_mode))


def shape(p):                     # 随机名字不上屏：只回答「长得像什么、形状对不对」
    base = os.path.basename(p)
    m = re.fullmatch(r"(tmp|[a-z]+)([A-Za-z0-9_]{8})(\.\w+)?", base)
    return f"{base[:3]}+8位随机{(m.group(3) or '') if m else ''}", bool(m)


def inner_of(root):               # 在临时目录里自建一层、塞一个文件：0500 目录删不掉，是因为**里面**还有东西
    d = os.path.join(root, "inner")
    os.makedirs(d)
    Path(d, "f.txt").write_text("占位", encoding="utf-8")
    return d


print("=== 1. 地基：tempdir 是哪个目录、本机有哪几个 API、名字长什么样 ===")
shutil.rmtree(WORK, ignore_errors=True)
os.mkdir(WORK)
print("  tempfile.gettempdir() =", tempfile.gettempdir(), "｜ 它等于 /tmp 吗 →", TEMPDIR_REAL == "/tmp")
print("  tempfile.gettempprefix() =", repr(tempfile.gettempprefix()), "｜ 随机段固定 8 位（字母数字下划线）")
apis = ("mktemp", "mkstemp", "mkdtemp", "NamedTemporaryFile", "TemporaryFile", "TemporaryDirectory")
print("  本机这六个都在吗 →", {a: hasattr(tempfile, a) for a in apis})
print("  → 粒度四种：mkstemp（fd+路径，自己管删）、NamedTemporaryFile（有名字，自动删）、TemporaryFile（连名字都没有）、TemporaryDirectory（一整份目录）。mktemp 不算粒度，它是反面教材，第 2 段专门演它。")

print("\n=== 2. ★ mktemp：本机还留着它，而且一声不响——这才是它危险的原因 ===")
print("  hasattr(tempfile, 'mktemp') →", hasattr(tempfile, "mktemp"))
name_only = tempfile.mktemp()                 # 真调一次，但**只**拿返回值，绝不拿它去建文件
print("  真调一次：返回类型 =", type(name_only).__name__, "｜ 名字形状 =", shape(name_only)[0], "｜ 这个路径上已有文件吗 →", os.path.exists(name_only), "← 它只算名字，不建文件")
doc = " ".join(tempfile.mktemp.__doc__.split())   # 折叠空白，取原话才不会被源码里的换行切断
print("  docstring 原话（不是我们的评价）：", (doc[doc.find("THIS FUNCTION"):] if "THIS FUNCTION" in doc else doc).split(". ")[0] + ".")
with warnings.catch_warnings(record=True) as got:
    warnings.simplefilter("always")
    tempfile.mktemp()
print("  本机有没有 DeprecationWarning → 警告条数 =", len(got), "（3.14 实测：一条都没有，既没被移除也没被警告）")
print("  ★「不报警告」不等于「能用」：从它算出名字到你 open 它，中间别的进程可以先占，这个空档叫竞态；第 5 段用 O_EXCL 现场把空档堵死。")

print("\n=== 3. 粒度一：mkstemp —— 返回 (fd, path)，创建那一刻就已经是 0600 ===")
fd, path = tempfile.mkstemp(suffix=".tmp"); track(path)
print("  解包出来：type(fd) =", type(fd).__name__, "｜ type(path) =", type(path).__name__, "｜ 名字形状 =", shape(path)[0], "｜ 是 tmp+8位随机+后缀吗 →", shape(path)[1])
print("  权限：从 fd 上 stat =", oct(stat.S_IMODE(os.fstat(fd).st_mode)), "｜ 从 path 再 stat =", perm(path), "｜ fd 与 path 是同一个文件吗 →", os.fstat(fd).st_ino == os.stat(path).st_ino)
print("  它此刻在磁盘上吗 →", os.path.exists(path), "← mkstemp **真的建了文件**，这点和 mktemp 正相反")
os.close(fd)
print("  os.close(fd) 之后还在吗 →", os.path.exists(path), "｜ 自己 unlink 之后 →", (os.unlink(path), untrack(path), os.path.exists(path))[2])
print("  ★ 退出作用域不替你删：mkstemp 给你的是裸 fd，删除责任 100% 在自己手上。")

print("\n=== 4. ★ 兑现第 1 节那行 umask 对照组：同一目录，mkstemp 0600、普通 open 0644 ===")
ambient = os.umask(0o022)                     # 一次调用：把 umask 钉成 0022，同时拿到继承来的原值
print("  本进程入口继承到的 umask =", oct(ambient), "（外层 shell 给的，各机器不同）；对照组显式钉在 0022/0077 两档，好跟第 1 节那行 printf 同一口径：")
for pin, remark in ((0o022, "← 0644 是全机器可读，0600 才是「只有我能读」"), (0o077, "← 普通 open 跟着变了，mkstemp **不吃 umask**")):
    os.umask(pin)
    plain = os.path.join(WORK, f"plain_{pin:o}.txt")
    Path(plain).write_text("普通 open", encoding="utf-8")     # 普通写法走的就是 open(p, 'w')
    fdn, pn = tempfile.mkstemp(dir=WORK); track(pn)
    print(f"  umask={pin:04o}：普通 open =", perm(plain), "｜ mkstemp =", perm(pn), "｜ 两者相等吗 →", perm(plain) == perm(pn), remark)
    os.close(fdn); os.unlink(pn); untrack(pn); os.unlink(plain)
print("  → mkstemp 无视 umask 的原因：内部是 os.open(..., O_CREAT|O_EXCL|O_RDWR, 0o600)，权限是第三个参数**给定**的，umask 只能往上收、不能放开。别指望 umask 替你保密：umask 是调用者当时的心情，0600 是 API 的承诺。")
os.umask(ambient)                             # 先恢复；再把同一个值设回去，返回的就是此刻生效的那个
now = os.umask(ambient)
print("  ★ 已恢复：现在生效的 umask =", oct(now), "｜ 入口原值 =", oct(ambient), "｜ 相等吗 →", now == ambient, "（上面只改**本进程**的 umask：不写配置文件、不改 shell、不影响别的进程。）")

print("\n=== 5. O_EXCL 就是那个「抢注」动作：撞一次，看竞态是怎么被堵死的 ===")
victim = os.path.join(WORK, "抢注.txt"); Path(victim).write_text("别人先建的", encoding="utf-8")
print("  先让「别人」把这个名字占掉：存在 →", os.path.exists(victim))
try:
    os.open(victim, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    print("  O_EXCL 竟然成功了（本机行为与预期不符，这段要重读）")
except FileExistsError as exc:
    print(f"  再以 O_CREAT|O_EXCL 打开同一个名字 → {type(exc).__name__}: {exc} ｜ errno = {exc.errno}")
print("  别人的内容被改掉了吗 →", Path(victim).read_text(encoding="utf-8") == "别人先建的", "← 撞失败是**原子**的：既没打开也没截断，一个字节没动")
fa = os.open(os.path.join(WORK, "无EXCL.txt"), os.O_CREAT | os.O_WRONLY, 0o600)
fb = os.open(os.path.join(WORK, "无EXCL.txt"), os.O_CREAT | os.O_WRONLY, 0o600)
print("  反证：不带 O_EXCL、对同一个名字连开两次 → 两次都成功、落在同一个文件上 =", os.fstat(fa).st_ino == os.fstat(fb).st_ino)
print("  → 没有 O_EXCL，内核就不替你判断「这个名字刚才有没有被占」，两次都算成功。mkstemp 的安全就建在 O_EXCL 上：它让「确认不存在」和「创建」成为同一次内核操作，所以没有「先算名字、后创建」的空档——这就是它和 mktemp 的全部区别。")
os.close(fa); os.close(fb); os.unlink(victim); os.unlink(os.path.join(WORK, "无EXCL.txt"))

print("\n=== 6. 粒度二：NamedTemporaryFile（默认 delete=True）—— 名字是它的，不是你借的 ===")
ntf = tempfile.NamedTemporaryFile(mode="w", suffix=".txt"); track(ntf.name)
print("  对象类型 =", type(ntf).__name__, "｜ 名字形状 =", shape(ntf.name)[0], "｜ 权限 =", perm(ntf.name), "｜ 在磁盘上吗 →", os.path.exists(ntf.name), "｜ 有 .name 吗 →", hasattr(ntf, "name"))
ntf.write("已经写进去了")
ntf.flush()                                   # 不 flush，二次 open 读到的是缓冲区之外的旧状态
try:
    with open(ntf.name, encoding="utf-8") as g:
        again = repr(g.read())
    print("  ★ 拿它的名字**再 open 一次**读得到吗（Linux 实测）→ True ｜ 读到 =", again)
except OSError as exc:
    print("  再 open 一次 →", f"{type(exc).__name__} errno={exc.errno}")
ntf.close(); untrack(ntf.name)
print("  close() 之后还在吗 →", os.path.exists(ntf.name), "← delete=True 就是在这一瞬间删的")

print("\n=== 7. 粒度二·续：delete_on_close=False；粒度三 TemporaryFile：它根本没有名字 ===")
params = list(inspect.signature(tempfile.NamedTemporaryFile).parameters)
print("  本机参数表 =", params, "｜ 有 delete_on_close 吗 →", "delete_on_close" in params, "（3.12 起才有）")
ntf2 = tempfile.NamedTemporaryFile(mode="w", dir=WORK, delete_on_close=False); track(ntf2.name)
ntf2.write("写了但先别删")
ntf2.close()
print("  delete_on_close=False：close() 后还在吗 →", os.path.exists(ntf2.name), "｜ 名字形状 =", shape(ntf2.name)[0], "｜ 内容已经落盘了吗 →", Path(ntf2.name).read_text(encoding="utf-8") == "写了但先别删")
print("  → close 只关句柄，**删**这个动作留给你自己：os.unlink(obj.name)。包装对象自己没有 unlink 方法，写 `ntf2.unlink()` 本机直接 AttributeError（这一条是踩出来的）。")
os.unlink(ntf2.name); untrack(ntf2.name)
tf = tempfile.TemporaryFile(dir=WORK)
print("  TemporaryFile：对象类型 =", type(tf).__name__, "｜ .name 的类型 =", type(tf.name).__name__, "← 是个**文件描述符整数**，不是路径")
try:
    os.path.isabs(tf.name)                    # 拿它当路径用一用：真炸才叫「没有名字」，不靠猜
    print("  当路径用竟然没报错（说明本机 .name 不是整数，这段要重读）")
except (TypeError, ValueError) as exc:
    print("  当路径用 →", f"{type(exc).__name__}: {exc}")
try:                                          # Linux 才有 /proc/self/fd，别的平台这段跳过、不装作物证
    print("  /proc/self/fd/<n> 指的东西末尾带 (deleted) 吗 →", os.readlink(f"/proc/self/fd/{tf.fileno()}").endswith("(deleted)"), "← 名字已从目录里摘掉，只剩句柄")
except OSError as exc:
    print("  /proc 不可用，这条本机验不了 →", f"{type(exc).__name__}: {exc}")
print("  → 没有名字 = 别人撞不上，你也没机会「忘了删」；代价是不能用路径访问它（不能交给子进程、不能 mmap、也不能在另一个 open() 里读）。close 即删，所以不用登记。")
tf.close()

print("\n=== 8. 粒度四：TemporaryDirectory（0700），和 0500 子目录会不会把清理炸掉 ===")
td = tempfile.TemporaryDirectory(dir=WORK); track(td.name)
inner = inner_of(td.name)
print("  对象类型 =", type(td).__name__, "｜ 名字形状 =", shape(td.name)[0], "｜ 权限 =", perm(td.name), "（0700 的执行位 = 能进去，别的用户连 ls 都 ls 不了里面）")
os.chmod(inner, 0o500)                        # 只 chmod 本脚本刚在临时目录里自建的那一层
try:
    shutil.rmtree(td.name)
    print("  inner 收成 0500 后交给 shutil.rmtree → 竟然成功了（和本机预期不符）")
except PermissionError as exc:
    print(f"  inner 收成 0500 后交给 shutil.rmtree → {type(exc).__name__}: errno={exc.errno}, strerror={exc.strerror!r}（str(exc) 带沙箱绝对路径，只打这三样）")
os.chmod(inner, 0o700)                        # 删不掉就先把权限改回来，绝不留下删不掉的目录
td.cleanup(); untrack(td.name)
print("  chmod 回 0700 之后 cleanup() → 目录还在吗 →", os.path.exists(td.name))
td2 = tempfile.TemporaryDirectory(dir=WORK); track(td2.name)
os.chmod(inner_of(td2.name), 0o500)           # 同样 0500，这次不预先改回来，直接交给它自己的 cleanup
try:
    td2.cleanup()
    healed = os.path.exists(td2.name)
except OSError as exc:                        # 万一本机不自愈：接住，绝不让脚本非 0 退出
    healed = f"没自愈：{type(exc).__name__} errno={exc.errno}"
print("  ★ 反直觉的一半：形状完全一样，只把 rmtree 换成 td2.cleanup() → 不抛异常 ｜ 目录还在吗 →", healed)
if os.path.exists(td2.name):                  # 兜底：改回权限再删，绝不在别人机器上留残骸
    os.chmod(os.path.join(td2.name, "inner"), 0o700)
    shutil.rmtree(td2.name)
untrack(td2.name)
src = inspect.getsource(tempfile.TemporaryDirectory._rmtree)
print("    证据在源码里：它的 _rmtree 挂了 onexc，遇 PermissionError 先 _resetperms 再重试 =", ("PermissionError" in src) and ("_resetperms" in src))
print("  → 别据此写「反正 cleanup 会自愈」：它自愈的只有**它自己建的**那棵树；你自己的 0500 项目目录交给 shutil.rmtree，照样是上面那条 PermissionError。")

print("\n=== 9. gettempdir() 从哪来：TMPDIR / TEMP / env= 全在子进程里改，本进程一动不动 ===")
Path(WORK, "tp.py").write_text("import tempfile, os\np = tempfile.gettempdir()\nprint('IS_TMP' if p == '/tmp' else os.path.basename(p))\n", encoding="utf-8")
choice = os.path.join(os.path.abspath(WORK), "自选目录"); os.mkdir(choice)
for label, extra in (("① 三个变量全清掉", {}), ("② TMPDIR=我们的目录", {"TMPDIR": choice}), ("③ TEMP=我们的目录", {"TEMP": choice}), ("④ 给一个没用的变量", {"X": "1"})):
    env = {k: v for k, v in os.environ.items() if k not in ("TMPDIR", "TEMP", "TMP", "PYTHONUTF8")}
    env.update(extra)
    out = subprocess.run([sys.executable, "tp.py"], cwd=WORK, env=env, capture_output=True, text=True).stdout.strip()
    print(f"  {label:18} → 子进程只报形状：{out} ｜ 命中的是我们指的那个吗 →", out == os.path.basename(choice))
print("  本进程自己的 gettempdir() 仍然是 =", tempfile.gettempdir(), "（上面全是子进程的事，父进程的环境一个字没改）")
print("  → 候选顺序：TMPDIR → TEMP → TMP → 内置（/tmp、/var/tmp、/usr/tmp）→ 当前目录；结果还被缓存在模块里，所以环境变量必须在**进程起来之前**设好才有效——subprocess 的 env= 正是这个用法。")
print("  ★ 判据：要把临时文件关进自己的沙箱，传 dir= 最稳，改子进程环境其次，靠默认值最危险——默认值是 /tmp，那是公共地盘。")

print("\n=== 10. ★ 未验证：Windows 语义，这台 Linux 机器一条都复现不了 ===")
print("  下面几条是文档/常识里的 Windows 行为，本机**无法验证**，别当事实引用：")
for line in ["NamedTemporaryFile 在 Windows 上通常不能拿同一个名字二次 open（共享模式不同）；",
             "delete=True 的删除时机受句柄释放影响，close() 之后能否立刻重建同名文件不保证；",
             "0600 在 Windows 上是「只有当前用户有 ACL」的近似，不是 POSIX 的那 9 个比特位；",
             "临时目录候选在 Windows 上一般命中 %LOCALAPPDATA%\\Temp，而不是 /tmp。"]:
    print("    ·", line)
print("  → 第 6 段那句「再 open 一次读得到 = True」**只在 Linux 成立**。要跨平台拿到「有名、别人撞不上」的临时文件，标准写法是 mkstemp + os.close(fd) + 自己 unlink，别依赖 NamedTemporaryFile。")

print("\n=== 11. 收口：临时文件的权限为什么是安全属性 ===")
print("  ① 名字要**抢**不能算：mkstemp 内部 O_CREAT|O_EXCL，撞了就换下一个（第 5 段实测 FileExistsError）；mktemp 只算不抢，中间有空档——本机它还在、一声不响，正因如此更不能当粒度用。")
print("  ② 权限要 API 给、不能环境给：mkstemp=0600、TemporaryDirectory=0700，与继承来的 umask 无关（第 4 段实测 0022/0077 两档）；普通 open 的 0644 落在 /tmp = 全机器可读。")
print("  ③ delete 的边界背清楚：NamedTemporaryFile(delete=True) 在 close() 那一刻删；delete_on_close=False 只关不删（得自己 unlink）；TemporaryFile 连名字都没有；mkstemp 谁都不替你删；cleanup 会自愈 0500，")
print("     但只管它自己建的那棵树。")
print("  ④ 要保密 → O_EXCL + 0600 + 尽早 close；要可控 → 传 dir=。这两条本机实测过，Windows 那一段没测过——屏幕上必须让读者分得清哪条是证据、哪条是听说。")
in_root = [p for p in REGISTERED if os.path.dirname(os.path.realpath(p)) == TEMPDIR_REAL]
print("  本次向 tempfile 要过的路径共", len(REGISTERED), "个 ｜ 直接落在 gettempdir() 根下的 =", len(in_root))
print("  未销账的活跃对象 =", len(OUTSTANDING), "｜ 还没删掉的 =", len([p for p in REGISTERED if os.path.exists(p)]), "｜ tempdir 里本次留下的个数 =", len([p for p in in_root if os.path.exists(p)]))
cur = os.umask(ambient)
print("  umask 收尾核对：当前生效 =", oct(cur), "｜ 入口原值 =", oct(ambient), "｜ 相等吗 →", cur == ambient)
shutil.rmtree(WORK)
print(f"  收尾自证：{WORK}/ 还在吗 →", os.path.exists(WORK))
