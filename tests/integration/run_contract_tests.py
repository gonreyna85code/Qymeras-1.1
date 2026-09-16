"""Run the v1 contract tests against the mock Node.

Validates docs/api-contract.yaml over real HTTP: bare /calib schema, /firmware,
toggle/dimmer wire semantics and HTTP-status-only error mapping.
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def main():
    script = os.path.join(ROOT, "tests", "integration", "test_contract.py")
    print("[contract-tests] %s" % script)
    return subprocess.call([sys.executable, script])


if __name__ == "__main__":
    sys.exit(main())