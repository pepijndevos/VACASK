import os
from ._vacask.parser_output import PTParameterValue
from ._vacask.parser_output import PTParameterExpression
from ._vacask.parser_output import PTParsedIdentifier
from ._vacask.id import Id
from ._vacask.value import Value
from ._vacask.rpnexpr import Rpn
from ._vacask.compiler import OpenvafCompiler

def PV(ident:str, value) -> PTParameterValue:
    """
    Create a parameter value object.

    Args:
        ident (str): The identifier of the parameter.
        value: The value of the parameter.

    Returns:
        PTParameterValue: A parameter value object.
    """
    return PTParameterValue(Id(ident), Value(value))

def PE(ident:str, expr: Rpn) -> PTParameterExpression:
    """
    Create a parameter expression object.

    Args:
        ident (str): The identifier of the parameter.
        expr (Rpn): The RPN expression of the parameter.

    Returns:
        PTParameterExpression: A parameter expression object.
    """
    return PTParameterExpression(Id(ident), expr)

def PTIds(ids: list[str]) -> list[PTParsedIdentifier]:
    """
    Convert a list of string identifiers to a list of PTParsedIdentifier objects.

    Args:
        ids (list[str]): A list of string identifiers.

    Returns:
        list[PTParsedIdentifier]: A list of PTParsedIdentifier objects.
    """
    return [PTParsedIdentifier(Id(i)) for i in ids]

def OpenvafCompilerBuiltin() -> OpenvafCompiler:
    """
    Returns the pre-build in-packagen openvaf compililer object.
    
    Args: /

    Returns:
        OpenvafCompiler: object
    """
    this_file = os.path.abspath(__file__)
    this_dir  = os.path.dirname(this_file)
    _def_openvaf_path = os.path.normpath(os.path.join(this_dir, "bin", "openvaf-r"))
    return OpenvafCompiler(_def_openvaf_path)
