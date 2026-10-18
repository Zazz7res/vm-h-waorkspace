"""第 06 章第 23 节：半个上下文管理器 —— 为第 07 章埋的钩子。

跑法：python3 t_context_manager.py

with obj: body 翻译成人话，照协议就三步：
    tmp = obj.__enter__()    进场
    try: body（as 变量绑到 tmp）
    finally: obj.__exit__(exc_type, exc_val, exc_tb)   出场，异常三元组递给它
__exit__ 返回 True 可以把异常咽下去，返回 False（默认姿势）异常照常往上飞。
本章只演「自己写一个」，深水区留给 07 章。
"""


class Door:
    def __init__(self, label):
        self.label = label

    def __enter__(self):
        print(f"[{self.label}] __enter__：开门，欢迎光临")
        return self          # as 拿到的就是它

    def __exit__(self, exc_type, exc_val, exc_tb):
        if exc_type is None:
            print(f"[{self.label}] __exit__：平安出门，锁门")
        else:
            print(f"[{self.label}] __exit__：摔了个跤（{exc_type.__name__}: {exc_val}），照样锁门")
        return False         # 不咽异常


class Silence:
    """__exit__ 返回 True：异常被就地火化。"""

    def __enter__(self):
        return "Silence"

    def __exit__(self, exc_type, exc_val, exc_tb):
        print(f"[Silence] 本来有个 {exc_type.__name__}，被我 return True 咽了")
        return True


print("=" * 68)
print("一、正常走完")
print("=" * 68)
with Door("前门") as d:
    print(f"  屋里干活，手里的 as 对象是 Door 实例吗：{isinstance(d, Door)}")

print()
print("=" * 68)
print("二、屋里出异常：__exit__ 收到三元组，然后异常继续飞")
print("=" * 68)
try:
    with Door("后门"):
        raise RuntimeError("屋里水管爆了")
except RuntimeError as e:
    print("  外面还是接住了：RuntimeError:", e)

print()
print("=" * 68)
print("三、__exit__ 返回 True：异常就地消失（危险，演示用）")
print("=" * 68)
with Silence():
    raise ValueError("这货活不过 with 块")
print("跑到这说明 ValueError 真被咽了 —— 生产代码别这么干，至少打个日志")
