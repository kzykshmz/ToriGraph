"""ToriGraph Python Automation SDK (JSON-RPC 2.0 over loopback TCP / Named Pipe)."""

from .client import (
    BatchContext,
    BatchStepRef,
    GraphProxy,
    MatrixProxy,
    RpcError,
    Session,
    TableProxy,
    ToriGraphError,
    ToriGraphRpcError,
    TransportError,
    connect,
    connect_or_launch,
    launch,
)

Graph3DProxy = GraphProxy

__version__ = "2.6.0"

__all__ = [
    "__version__",
    "ToriGraphError",
    "ToriGraphRpcError",
    "TransportError",
    "RpcError",
    "BatchStepRef",
    "BatchContext",
    "Session",
    "TableProxy",
    "MatrixProxy",
    "GraphProxy",
    "Graph3DProxy",
    "connect",
    "connect_or_launch",
    "launch",
]
