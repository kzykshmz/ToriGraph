"""ToriGraph Python Automation SDK (JSON-RPC 2.0 over loopback TCP / Named Pipe)."""

from .client import (
    GraphProxy,
    MatrixProxy,
    RpcError,
    Session,
    TableProxy,
    ToriGraphError,
    TransportError,
    connect,
    launch,
)

__version__ = "2.4.0"

__all__ = [
    "__version__",
    "ToriGraphError",
    "TransportError",
    "RpcError",
    "Session",
    "TableProxy",
    "MatrixProxy",
    "GraphProxy",
    "connect",
    "launch",
]
