import ast
import operator
import re
from .base import Skill, SkillResult


class CalculatorSkill(Skill):
    name = "calculator"
    priority = 90

    OPS = {
        ast.Add: operator.add,
        ast.Sub: operator.sub,
        ast.Mult: operator.mul,
        ast.Div: operator.truediv,
        ast.Mod: operator.mod,
        ast.Pow: operator.pow,
        ast.USub: operator.neg,
        ast.UAdd: operator.pos,
    }

    def _normalize(self, text):
        text = (text or "").lower().strip()

        text = re.sub(
            r"\b(bằng bao nhiêu|bằng mấy|kết quả là bao nhiêu|"
            r"kết quả bằng bao nhiêu|bao nhiêu|hãy tính|tính)\b",
            " ",
            text,
        )

        text = re.sub(r"\bnhân\s+với\b", "*", text)
        text = re.sub(r"\bnhân\b", "*", text)
        text = re.sub(r"\bchia\s+cho\b", "/", text)
        text = re.sub(r"\bchia\b", "/", text)
        text = re.sub(r"\bcộng\s+với\b", "+", text)
        text = re.sub(r"\bcộng\b", "+", text)
        text = re.sub(r"\btrừ\s+đi\b", "-", text)
        text = re.sub(r"\btrừ\b", "-", text)
        text = re.sub(r"(?<=\d)\s*[x×]\s*(?=\d)", "*", text)

        text = text.replace(",", ".")
        text = re.sub(r"[?!;:]+", " ", text)
        return re.sub(r"\s+", " ", text).strip()

    def _eval(self, node):
        if isinstance(node, ast.Expression):
            return self._eval(node.body)

        if isinstance(node, ast.Constant):
            if isinstance(node.value, (int, float)):
                return node.value
            raise ValueError

        if isinstance(node, ast.BinOp):
            op = type(node.op)
            if op not in self.OPS:
                raise ValueError

            left = self._eval(node.left)
            right = self._eval(node.right)

            if op in (ast.Div, ast.Mod) and right == 0:
                raise ZeroDivisionError

            return self.OPS[op](left, right)

        if isinstance(node, ast.UnaryOp):
            op = type(node.op)
            if op not in self.OPS:
                raise ValueError
            return self.OPS[op](self._eval(node.operand))

        raise ValueError

    async def handle(self, intent, ctx):
        text = intent.slots.get("text", "")
        expression = self._normalize(text).replace("%", "/100")

        try:
            if not re.fullmatch(
                r"[\d\s\.\+\-\*\/\(\)%]+",
                expression,
            ):
                return SkillResult(True, "Tôi chưa đọc được phép tính.")

            tree = ast.parse(expression, mode="eval")
            result = self._eval(tree)

        except ZeroDivisionError:
            return SkillResult(True, "Không thể chia cho 0.")
        except Exception:
            return SkillResult(True, "Tôi chưa tính được phép tính đó.")

        if isinstance(result, float):
            result = int(result) if result.is_integer() else round(result, 6)

        return SkillResult(True, str(result))


MANIFEST = {
    "name": "calculator",
    "intents": {
        "calculator.calculate": {
            "phrases": [
                "tính",
                "hãy tính",
                "bằng bao nhiêu",
                "bằng mấy",
            ],
            "patterns": [
                {
                    "regex": r"\d+\s+(?:cộng|trừ|nhân|chia)\s+(?:cho\s+)?\d+"
                },
                {
                    "regex": r"\d+\s*[\+\-\*/x×]\s*\d+"
                },
            ],
            "copy_text": True,
        }
    },
}
