"""ToriGraph Python Automation SDK (JSON-RPC 2.0 over loopback TCP / Named Pipe)."""

from .client import (
    GraphProxy,
    MatrixProxy,
    RpcError,
    Session,
    TableProxy,
    ToriGraphError,
    ToriGraphRpcError,
    TransportError,
    connect,
    launch,
)

__version__ = "2.5.0"

__all__ = [
    "__version__",
    "ToriGraphError",
    "ToriGraphRpcError",
    "TransportError",
    "RpcError",
    "Session",
    "TableProxy",
    "MatrixProxy",
    "GraphProxy",
    "connect",
    "launch",
]
