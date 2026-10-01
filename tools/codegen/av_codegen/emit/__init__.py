"""Code-generation backends.

Each backend is a small module exporting an ``emit(reg, out_dir)``
function that writes one or more files into ``out_dir``. The orchestrator
in ``__main__`` runs them all, in this order.

To add a new backend (say, a tracing emitter or a wire-format validator):
write a new module with an ``emit`` function and append it to
``ALL_BACKENDS`` below. Nothing declares which files a backend writes —
``--check`` compares the whole output directory, so a backend that grows
a file needs no bookkeeping here.
"""

from __future__ import annotations

from . import (
    client_dispatch,
    command_names,
    icd_entries,
    ids,
    serialize,
    server_backends,
    server_dispatch,
    supported_extensions,
    width_audit,
)

ALL_BACKENDS = (
    ids,
    command_names,
    serialize,
    client_dispatch,
    server_dispatch,
    server_backends,
    icd_entries,
    supported_extensions,
)


__all__ = [
    "ALL_BACKENDS",
    "client_dispatch",
    "command_names",
    "icd_entries",
    "ids",
    "serialize",
    "server_backends",
    "server_dispatch",
    "supported_extensions",
    "width_audit",
]
