"""WAL AST definitions shim for walpp"""


class WalEvalError(Exception):
    """WAL evaluation error"""
    pass


class WalError(Exception):
    """Generic WAL error"""
    pass


class Symbol:
    """S-expression symbol"""
    def __init__(self, name):
        self.name = name
    
    def __eq__(self, other):
        if isinstance(other, Symbol):
            return self.name == other.name
        return False
    
    def __repr__(self):
        return f"Symbol({self.name!r})"
    
    def __str__(self):
        return self.name


class Operator:
    """Operator call in AST"""
    def __init__(self, name, args=None):
        self.name = name
        self.args = args or []
    
    def __eq__(self, other):
        if isinstance(other, Operator):
            return self.name == other.name and self.args == other.args
        return False
    
    def __repr__(self):
        return f"Operator({self.name!r}, {self.args!r})"
