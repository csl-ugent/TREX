from abc import ABC, abstractmethod


def _to_expr_node(node):
    if isinstance(node, ExprBase):
        return node
    else:
        return ExprLiteral(node)


class ExprBase(ABC):
    @abstractmethod
    def emit(self, node_name: str) -> str:
        pass

    # Comparison operators
    def __lt__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(self, '<', other)

    def __le__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(self, '<=', other)

    def __eq__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(self, '=', other)

    def __ne__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(self, '<>', other)

    def __gt__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(self, '>', other)

    def __ge__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(self, '>=', other)

    # Binary arithmetic operators
    def __add__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(self, '+', other)

    def __radd__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(other, '+', self)

    def __sub__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(self, '-', other)

    def __rsub__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(other, '-', self)

    def __mul__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(self, '*', other)

    def __rmul__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(other, '*', self)

    def __truediv__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(self, '/', other)

    def __rtruediv__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(other, '/', self)

    def __mod__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(self, '%', other)

    def __rmod__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(other, '%', self)

    def __pow__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(self, '^', other)

    def __rpow__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(other, '^', self)

    # Unary arithmetic operators
    def __neg__(self):
        return ExprUnaryOp('-', self)

    def __pos__(self):
        return ExprUnaryOp('+', self)

    # Bitwise operators, repurposed as logical operators.
    # Admittedly, this is a bit hacky. The main disadvantage
    # of this is that the bitwise operators have different
    # precedence than the logical operators.
    def __and__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(self, 'AND', other)

    def __rand__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(other, 'AND', self)

    def __xor__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(self, 'XOR', other)

    def __rxor__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(other, 'XOR', self)

    def __or__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(self, 'OR', other)

    def __ror__(self, other):
        other = _to_expr_node(other)
        return ExprBinOp(other, 'OR', self)

    def __invert__(self):
        return ExprUnaryOp('NOT', self)


class ExprLiteral(ExprBase):
    def __init__(self, literal):
        self.literal = literal

    def emit(self, node_name: str) -> str:
        # Quote strings
        if isinstance(self.literal, str):
            return f'"{self.literal}"'
        else:
            return f'{self.literal}'


class ExprProperty(ExprBase):
    def __init__(self, node_name, property_name):
        self.node_name = node_name
        self.property_name = property_name

    def emit(self, node_name: str) -> str:
        # Use the node_name that was passed in the constructor if not None.
        # Otherwise, use the name of the enclosing node.
        node_name = self.node_name if self.node_name is not None else node_name

        return f'{node_name}.{self.property_name}'


def P(*args):
    if len(args) == 1:
        # Simple property of enclosing node.
        node_name = None
        property_name = args[0]
    elif len(args) == 2:
        # Property with explicit node name
        node_name = args[0]
        property_name = args[1]
    else:
        raise ValueError('Expected one or two arguments')
    return ExprProperty(node_name, property_name)


class ExprBinOp(ExprBase):
    def __init__(self, lhs, op: str, rhs):
        self.lhs = lhs
        self.op = op
        self.rhs = rhs

    def emit(self, node_name: str) -> str:
        # Emit lhs and rhs
        lhs_em = self.lhs.emit(node_name)
        rhs_em = self.rhs.emit(node_name)

        return f'({lhs_em}) {self.op} ({rhs_em})'

# NOTE: While we can override "in" with __contains__, it must return a Bool,
# so we can't use it here. Hence the need for the explicit "is_in".


def is_in(property_name, lst: list):
    lst = _to_expr_node(lst)
    return ExprBinOp(property_name, 'IN', lst)


class ExprUnaryOp(ExprBase):
    def __init__(self, op: str, operand):
        self.op = op
        self.operand = operand

    def emit(self, node_name: str) -> str:
        # Emit operand
        op_em = self.operand.emit(node_name)

        return f'{self.op}({op_em})'


class ExprIsNull(ExprBase):
    def __init__(self, operand):
        self.operand = operand

    def emit(self, node_name: str) -> str:
        # Emit operand
        op_em = self.operand.emit(node_name)

        return f'({op_em}) IS NULL'


def is_null(property_name):
    return ExprIsNull(property_name)
