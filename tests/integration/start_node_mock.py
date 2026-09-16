"""Standalone mock Node for manual Dashboard<->Node wiring (real hardware).

Launches the Qymera v1 mock Node on a fixed port so a real Dashboard board in
AP/STA mode can be pointed at it. Prints the base URL; Ctrl-C to stop.

Usage:
    python tests/integration/start_node_mock.py [port]
"""
from __future__ import print_function
import sys
from mock_node import MockNode


def main(argv):
    port = int(argv[1]) if len(argv) > 1 else 8123
    node = MockNode()
    node.serve(port)
    print("mock node up at http://127.0.0.1:%d (Ctrl-C to stop)" % port)
    try:
        import threading
        threading.Event().wait()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.path.insert(0, __file__.rsplit("/", 1)[0] if "/" in __file__ else ".")
    sys.exit(main(sys.argv))