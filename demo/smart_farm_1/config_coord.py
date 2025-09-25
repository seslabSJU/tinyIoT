"""Configuration values for the coordination launcher and child device simulators."""

# --------------- Duplicate Handling ---------------
# Coordination runs are non-interactive, so pick an explicit policy.
#   'yes' -> auto-reuse existing resources without prompting
#   'no'  -> abort immediately on duplicates
SIMULATOR_REUSE_EXISTING = "yes"

# ----------------------- Paths -----------------------
# Absolute or relative path to the tinyIoT server binary
SERVER_EXEC = "/home/parks/tinyIoT/source/server/server"

# Absolute or relative path to the simulator entrypoint
# e.g., "simulator.py" or "/opt/iot-sim/bin/simulator.py"
SIMULATOR_PATH = "/home/parks/tinyIoT/simulator/simulator.py"

# Python interpreter used to launch the simulator
PYTHON_EXEC = "python3"

# ------------------- Health Check URL -------------------
# OneM2M CSE HTTP endpoint used for server readiness checks.
# NOTE: CSI is lowercase ("tinyiot"). Change only if your CSE path differs.
CSE_URL = "http://127.0.0.1:3000/tinyiot"

# ---------------- Timeouts & Retries ----------------
# Number of attempts for server readiness polling (1 attempt ~ 1 second)
WAIT_SERVER_TIMEOUT = 30  # seconds

# Number of attempts for process detection (1 attempt ~ 1 second)
WAIT_PROCESS_TIMEOUT = 10  # seconds

# Per-request HTTP timeout for the health check call
REQUEST_TIMEOUT = 2  # seconds
