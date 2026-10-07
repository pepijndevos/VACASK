# _vacask exists only in the scikit-build package, without it only the helper scripts are available
import importlib.util
if importlib.util.find_spec(__name__+"._vacask") is not None:
    from . import _vacask
else:
    _vacask = None

if _vacask is not None:
    def _conf_vacask():
        import os
        this_file = os.path.abspath(__file__)
        this_dir  = os.path.dirname(this_file)
        osdi_dir   = os.path.normpath(os.path.join(this_dir, "mod"))
        _vacask.simulator.prependModulePath([osdi_dir])

    _conf_vacask()

    status = _vacask.status
    from ._vacask.status import *

    parser = _vacask.parser
    from ._vacask.parser import *

    parser_output = _vacask.parser_output
    from ._vacask.parser_output import *

    compiler = _vacask.compiler
    from ._vacask.compiler import *

    circuit = _vacask.circuit
    from ._vacask.circuit import *

    analysis = _vacask.analysis
    from ._vacask.analysis import *

    id = _vacask.id
    from ._vacask.id import *

    value = _vacask.value
    from ._vacask.value import *

    simulator = _vacask.simulator
    from ._vacask.simulator import *

    options = _vacask.options
    from ._vacask.options import *

    from .helpers import *

    # Monkeypath the setup function because it always overwrites the "default module path" set above.
    _original_setup = simulator.setup
    def _setup(*args, **kwargs):
        result = _original_setup(*args, **kwargs)
        _conf_vacask()
        return result
    simulator.setup = _setup
