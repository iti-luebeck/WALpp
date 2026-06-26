"""WAL reader shim - parses S-expressions and wraps walpp evaluator"""
import re
from .ast_defs import Symbol, Operator


def read_wal_sexpr(source):
    """Parse a WAL S-expression string into an AST"""
    tokens = tokenize(source)
    result, _ = parse_expr(tokens, 0)
    return result


def tokenize(source):
    """Tokenize a WAL S-expression"""
    # Remove comments
    source = re.sub(r';.*$', '', source, flags=re.MULTILINE)
    
    # Simple tokenization
    tokens = []
    i = 0
    while i < len(source):
        # Skip whitespace
        if source[i].isspace():
            i += 1
            continue
        
        # Quoted strings
        if source[i] == '"':
            j = i + 1
            while j < len(source) and source[j] != '"':
                j += 1
            tokens.append(('STRING', source[i+1:j]))
            i = j + 1
            continue
        
        # Parentheses
        if source[i] in '()':
            tokens.append(('PAREN', source[i]))
            i += 1
            continue
        
        # Quoted symbol '
        if source[i] == "'":
            tokens.append(('QUOTE', "'"))
            i += 1
            continue
        
        # Numbers and symbols
        j = i
        while j < len(source) and not source[j].isspace() and source[j] not in '()"\'"':
            j += 1
        
        token = source[i:j]
        if token:
            # Try to parse as number
            try:
                if '.' in token:
                    tokens.append(('FLOAT', float(token)))
                else:
                    tokens.append(('INT', int(token)))
            except ValueError:
                tokens.append(('SYMBOL', token))
        
        i = j
    
    return tokens


def parse_expr(tokens, pos):
    """Parse an expression starting at position pos"""
    if pos >= len(tokens):
        raise ValueError("Unexpected end of input")
    
    token_type, value = tokens[pos]
    
    # Number literal
    if token_type == 'INT':
        return value, pos + 1
    
    if token_type == 'FLOAT':
        return value, pos + 1
    
    # String literal
    if token_type == 'STRING':
        return value, pos + 1
    
    # Quoted expression
    if token_type == 'QUOTE':
        expr, next_pos = parse_expr(tokens, pos + 1)
        # Convert to (quote expr)
        return Operator('quote', [expr]), next_pos
    
    # Symbol
    if token_type == 'SYMBOL':
        return Symbol(value), pos + 1
    
    # List / Operator call
    if token_type == 'PAREN' and value == '(':
        pos += 1
        args = []
        
        while pos < len(tokens):
            if tokens[pos][0] == 'PAREN' and tokens[pos][1] == ')':
                # Empty list
                if not args:
                    return Operator(None, []), pos + 1
                
                # First element should be the operator/function name
                first = args[0]
                if isinstance(first, Symbol):
                    # Function call: (+ 1 2) becomes Operator('+', [1, 2])
                    return Operator(first.name, args[1:]), pos + 1
                else:
                    # Other cases: (list 1 2 3) etc
                    return Operator(None, args), pos + 1
            
            expr, pos = parse_expr(tokens, pos)
            args.append(expr)
        
        raise ValueError("Unclosed parenthesis")
    
    raise ValueError(f"Unexpected token: {token_type}")
