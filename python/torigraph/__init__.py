"""ToriGraph Python Automation SDK (JSON-RPC 2.0 over loopback TCP / Named Pipe)."""

from .client import (
    BatchContext,
    DEFAULT_RPC_TIMEOUT,
    BatchStepRef,
    GraphProxy,
    MatrixProxy,
    RpcError,
    Session,
    TableProxy,
    ToriGraphError,
    ToriGraphRpcError,
    ToriGraphVersionWarning,
    TransportError,
    WorkbookProxy,
    check_app_version,
    connect,
    connect_or_launch,
    launch,
)

Graph3DProxy = GraphProxy

# Must equal project(... VERSION ...) in the top-level CMakeLists.txt
# (checked at CMake configure time and by scripts/check_version.py).
__version__ = "3.0.0"

__all__ = [
    "__version__",
    "ToriGraphError",
    "ToriGraphRpcError",
    "TransportError",
    "ToriGraphVersionWarning",
    "DEFAULT_RPC_TIMEOUT",
    "check_app_version",
    "RpcError",
    "BatchStepRef",
    "BatchContext",
    "Session",
    "TableProxy",
    "MatrixProxy",
    "GraphProxy",
    "Graph3DProxy",
    "WorkbookProxy",
    "connect",
    "connect_or_launch",
    "launch",
]
