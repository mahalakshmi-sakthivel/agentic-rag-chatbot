import ast
import operator as op
from typing import Any
from app.tools.base import BaseTool
from app.schemas.models import Identity, ToolResult, ErrorCode

# Supported operators
OPERATORS = {
    ast.Add: op.add,
    ast.Sub: op.sub,
    ast.Mult: op.mul,
    ast.Div: op.truediv,
    ast.FloorDiv: op.floordiv,
    ast.Mod: op.mod,
    ast.Pow: op.pow,
    ast.USub: op.neg,
    ast.UAdd: op.pos
}

# Supported functions
FUNCTIONS = {
    'abs': abs,
    'round': round,
    'min': min,
    'max': max
}

class SafeEvaluator:
    def __init__(self, max_length: int = 100):
        self.max_length = max_length

    def evaluate(self, expr: str) -> Any:
        if not isinstance(expr, str):
            raise TypeError("Expression must be a string")
        if len(expr) > self.max_length:
            raise ValueError("Expression too long")
            
        try:
            tree = ast.parse(expr, mode='eval')
        except SyntaxError as e:
            raise ValueError(f"Syntax error: {e}")
            
        return self._eval_node(tree.body)

    def _eval_node(self, node: ast.AST) -> Any:
        if isinstance(node, ast.Constant):
            if isinstance(node.value, (int, float)):
                return node.value
            raise ValueError(f"Unsupported constant type: {type(node.value)}")
        elif isinstance(node, ast.BinOp):
            left = self._eval_node(node.left)
            right = self._eval_node(node.right)
            op_type = type(node.op)
            
            if op_type not in OPERATORS:
                raise ValueError(f"Unsupported operator: {op_type}")
                
            if op_type == ast.Pow:
                # Guard against huge exponents
                if right > 100 or right < -100:
                    raise ValueError("Exponent too large (must be between -100 and 100)")
                    
            try:
                return OPERATORS[op_type](left, right)
            except ZeroDivisionError:
                raise ValueError("Division by zero")
        elif isinstance(node, ast.UnaryOp):
            operand = self._eval_node(node.operand)
            op_type = type(node.op)
            if op_type not in OPERATORS:
                raise ValueError(f"Unsupported unary operator: {op_type}")
            return OPERATORS[op_type](operand)
        elif isinstance(node, ast.Call):
            if not isinstance(node.func, ast.Name):
                raise ValueError("Only simple function calls are allowed")
            func_name = node.func.id
            if func_name not in FUNCTIONS:
                raise ValueError(f"Unsupported function: {func_name}")
            args = [self._eval_node(arg) for arg in node.args]
            return FUNCTIONS[func_name](*args)
        elif isinstance(node, ast.Name):
            # No variables allowed, but names appear as function identifiers which are checked above
            raise ValueError(f"Variables are not allowed: {node.id}")
        else:
            raise ValueError(f"Unsupported node type: {type(node)}")

class CalculatorTool(BaseTool):
    name = "calculator"

    def __init__(self):
        self.evaluator = SafeEvaluator()

    def execute(self, input_data: Any, identity_context: Identity) -> ToolResult:
        if not isinstance(input_data, dict):
            return ToolResult(success=False, error=ErrorCode.TOOL_INPUT_INVALID.value)
            
        expression = input_data.get("expression")
        if not expression or not isinstance(expression, str):
            return ToolResult(success=False, error=ErrorCode.TOOL_INPUT_INVALID.value)

        try:
            result = self.evaluator.evaluate(expression)
            return ToolResult(success=True, data={"result": result})
        except ValueError as e:
            # Controlled errors
            return ToolResult(success=False, error=str(e))
        except Exception as e:
            return ToolResult(success=False, error=ErrorCode.TOOL_EXECUTION_FAILED.value)
