"""Run the Dashboard host sanity suite (tests/host_sanity.py).

Host-side regression baseline: every production rule that can be mirrored
deterministically (registry, control pending machine, rule engine, skills,
LLM provider request/response, API surface, node client boundaries) is
mirrored here and must stay green with the firmware build.
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def main():
    script = os.path.join(ROOT, "tests", "host_sanity.py")
    print("[dashboard-tests] %s" % script)
    return subprocess.call([sys.executable, script])


if __name__ == "__main__":
    sys.exit(main())