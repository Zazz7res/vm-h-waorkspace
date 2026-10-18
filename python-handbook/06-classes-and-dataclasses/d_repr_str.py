"""第 06 章第 6 节：__repr__ vs __str__ —— 四种场合各自走哪个。

跑法：python3 d_repr_str.py

一句话分工：
  __str__  → 给人看的（print(x)、str(x)、f"{x}" 走这里）
  __repr__ → 给程序员/调试器看的（f"{x!r}"、repr(x)、塞进 list/tuple/dict 里打印走这里）
  str() 在没定义 __str__ 时会退到 __repr__；repr() 永远不回退去叫 __str__。
脚本里不打印带内存地址的默认 repr（地址每次跑都不同），只截取它对不上的那半段。
"""


class Book:
    """同时定义两个：一眼看清谁在什么场合被叫。"""

    def __init__(self, title, pages):
        self.title = title
        self.pages = pages

    def __str__(self):
        return f"《{self.title}》共 {self.pages} 页"

    def __repr__(self):
        return f"Book(title={self.title!r}, pages={self.pages})"


class Stamp:
    """只定义 __repr__：str() 会退回来用它。"""

    def __repr__(self):
        return "Stamp('加急')"


class Memo:
    """只定义 __str__：repr() 不会退回来用它，仍走 object 的默认 repr。"""

    def __str__(self):
        return "便签内容"


b = Book("Python 手册", 420)

print("=" * 68)
print("一、Book 同时有 __str__ 和 __repr__：四种场合")
print("=" * 68)
print("1. print(b) →")
print("   实际打出：", end="")
print(b)
print("2. str(b) / f\"{b}\" →", str(b), "|", f"{b}")
print("3. repr(b) / f\"{b!r}\" →", repr(b), "|", f"{b!r}")
print("4. 塞进容器再 print：", [b], "|", (b,))
print("   ↑ list/tuple/dict 打印元素时用的全是元素的 repr，所以看到的是 __repr__")

print()
print("=" * 68)
print("二、只有 __repr__（Stamp）：str() 退回来")
print("=" * 68)
s = Stamp()
print("print(s) →", s, "　← __str__ 没定义，str() 退到 __repr__")
print("f\"{s!r}\" →", f"{s!r}")

print()
print("=" * 68)
print("三、只有 __str__（Memo）：repr() 不退回来")
print("=" * 68)
m = Memo()
print("print(m) →", m, "　← 走 __str__")
rm = repr(m)
print("repr(m) 的开头：", rm[:10], "…结尾：", rm[-1:], "← 默认那串带地址的 <...object at 0x...>，地址每次跑都不同所以只截形状")
print("repr(m) 等于 __str__ 吗：", rm == m.__str__(), "← False：repr 永远不叫 __str__")
print("把 Memo 塞进 list 打印：", "[", rm[:10], "…", "] ← 容器用的是 repr，给人看的 __str__ 根本没上场")

print()
print("=" * 68)
print("四、f-string 的转换码就这两个：!s 走 str，!r 走 repr")
print("=" * 68)
print("f\"{b!s}\" →", f"{b!s}")
print("f\"{b!r}\" →", f"{b!r}")
print("小结：写日志给人看用 str/print，写调试输出和「能复制回去重跑」的东西用 repr。")
