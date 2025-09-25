"""Launch the tinyIoT server and a batch of sensor device simulators in sequence."""

import subprocess
import sys
import time
import threading
import requests
import logging
import os
import config_coord as config
from typing import List, Optional

logging.basicConfig(level=getattr(config, 'LOG_LEVEL', logging.INFO), format='[%(levelname)s] %(message)s')


class SensorConfig:
    """Configuration bundle describing a sensor simulator launch request."""
    def __init__(self, sensor_type: str, protocol: str = 'mqtt', mode: str = 'random',
                 frequency: int = 2, registration: int = 1) -> None:
        self.sensor_type = sensor_type
        self.protocol = protocol
        self.mode = mode
        self.frequency = frequency
        self.registration = registration


# Coordinator options. Users can add or remove sensors here to suit their setup.
# Sensor names must not contain spaces.
SENSORS_TO_RUN: List[SensorConfig] = [
    SensorConfig('temp',  protocol='http', mode='random', frequency=3, registration=1),
    SensorConfig('humid', protocol='http', mode='random', frequency=3, registration=1),
    SensorConfig('co2',   protocol='http', mode='random', frequency=3, registration=1),
    SensorConfig('soil',  protocol='http', mode='random', frequency=3, registration=1)
]


def wait_for_server(timeout: int = getattr(config, 'WAIT_SERVER_TIMEOUT', 30)) -> bool:
    """Poll the HTTP endpoint until the CSE responds with 200 OK."""
    headers = {'X-M2M-Origin': 'CAdmin', 'X-M2M-RVI': '3', 'X-M2M-RI': 'healthcheck'}
    url = f"{config.CSE_URL}"
    req_timeout = getattr(config, 'REQUEST_TIMEOUT', 2)
    for _ in range(timeout):
        try:
            res = requests.get(url, headers=headers, timeout=req_timeout)
            if res.status_code == 200:
                logging.info("[COORD] tinyIoT server is responsive.")
                return True
        except requests.exceptions.RequestException:
            pass
        time.sleep(1)
    logging.error("[COORD] Unable to connect to tinyIoT server.")
    return False


def wait_for_process(name: str, timeout: int = getattr(config, 'WAIT_PROCESS_TIMEOUT', 30)) -> bool:
    """Return True when a process matching *name* is detected within timeout."""
    logging.info(f"[COORD] Checking process: {name}...")
    for _ in range(timeout):
        try:
            out = subprocess.check_output(['pgrep', '-f', name])
            if out:
                logging.info(f"[COORD] {name} is running.")
                return True
        except subprocess.CalledProcessError:
            pass
        time.sleep(1)
    logging.error(f"[COORD] Failed to detect running process: {name}.")
    return False


class SimulatorHandle:
    """Wrapper that watches simulator output and exposes readiness helpers."""
    def __init__(self, proc: subprocess.Popen, sensor_type: str) -> None:
        self.proc = proc
        self.sensor_type = sensor_type
        self._ready = threading.Event()
        self._failed = threading.Event()
        self._reader = threading.Thread(target=self._pump_output, daemon=True)
        self._reader.start()

    def _pump_output(self) -> None:
        tag = f"[{self.sensor_type.upper()}]"
        try:
            for line in self.proc.stdout:
                text = line.rstrip('\n')
                print(text, flush=True)
                if text.startswith(f"{tag} run "):
                    self._ready.set()
                elif 'registration failed' in text.lower():
                    self._failed.set()
                elif '[INFO] Stopping per user request' in text:
                    self._failed.set()
        finally:
            try:
                self.proc.stdout.close()
            except Exception:
                pass
            self.proc.wait()
            if not self._ready.is_set():
                self._failed.set()

    def wait_until_ready(self) -> bool:
        """Block until the simulator prints its ready message or fails."""
        while True:
            if self._ready.wait(timeout=0.2):
                return True
            if self._failed.is_set():
                return False
            if self.proc.poll() is not None and not self._ready.is_set():
                return False

    def terminate(self) -> None:
        """Politely terminate the simulator process if still running."""
        if self.proc and self.proc.poll() is None:
            try:
                self.proc.terminate()
            except Exception:
                pass

    def kill(self) -> None:
        """Force kill the simulator process if termination failed."""
        if self.proc and self.proc.poll() is None:
            try:
                self.proc.kill()
            except Exception:
                pass

    def join_reader(self, timeout: float = 1.0) -> None:
        """Join the stdout reader thread to avoid leaking daemon threads."""
        if self._reader.is_alive():
            self._reader.join(timeout)

def launch_simulator(sensor_config: SensorConfig, index: int) -> Optional[SimulatorHandle]:
    """Spawn one simulator process and return a handle on success."""
    python_exec = getattr(config, 'PYTHON_EXEC', 'python3')
    simulator_path = config.SIMULATOR_PATH
    sim_args = [
        python_exec, simulator_path,
        '--sensor', sensor_config.sensor_type,
        '--protocol', sensor_config.protocol,
        '--mode', sensor_config.mode,
        '--frequency', str(sensor_config.frequency),
        '--registration', str(sensor_config.registration),
    ]
    reuse_existing = str(getattr(config, 'SIMULATOR_REUSE_EXISTING', 'yes')).lower()
    if reuse_existing not in {'yes', 'no'}:
        logging.warning(
            "[COORD] Invalid SIMULATOR_REUSE_EXISTING value '%s'; falling back to 'no'.",
            reuse_existing
        )
        reuse_existing = 'no'
    sim_args.extend(['--reuse-existing', reuse_existing])
    logging.info(f"[COORD] Starting simulator #{index + 1} ({sensor_config.sensor_type} sensor)...")
    logging.debug(f"[COORD] Command: {' '.join(sim_args)}")
    env = os.environ.copy()
    sim_env = getattr(config, 'SIM_ENV', None)
    if isinstance(sim_env, dict):
        env.update(sim_env)
    env['PYTHONUNBUFFERED'] = '1'
    try:
        proc = subprocess.Popen(
            sim_args,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            env=env
        )
        return SimulatorHandle(proc, sensor_config.sensor_type)
    except Exception as e:
        logging.error(f"[COORD] Failed to start simulator #{index + 1}: {e}")
        return None

if __name__ == '__main__':
    logging.info("[COORD] Starting tinyIoT server...")
    try:
        server_proc = subprocess.Popen([config.SERVER_EXEC])
    except Exception as e:
        logging.error(f"[COORD] Failed to start tinyIoT server: {e}")
        sys.exit(1)

    if not wait_for_server():
        try:
            server_proc.terminate()
        except Exception:
            pass
        sys.exit(1)

    simulator_handles: List[SimulatorHandle] = []
    logging.info(f"[COORD] Launching {len(SENSORS_TO_RUN)} sensor simulators...")

    try:
        for i, sensor_conf in enumerate(SENSORS_TO_RUN):
            handle = launch_simulator(sensor_conf, i)
            if not handle:
                logging.error(f"[COORD] Failed to launch simulator #{i + 1}")
                continue
            logging.info(f"[COORD] Waiting for simulator #{i + 1} ({sensor_conf.sensor_type}) to finish registration...")
            if handle.wait_until_ready():
                logging.info(f"[COORD] Simulator #{i + 1} ready.")
                simulator_handles.append(handle)
            else:
                logging.error(f"[COORD] Simulator #{i + 1} failed during setup; aborting launch sequence.")
                handle.terminate()
                handle.join_reader()
                break
    except KeyboardInterrupt:
        logging.info("[COORD] Launch interrupted by user.")

    if not simulator_handles:
        logging.error("[COORD] No simulators were started successfully.")
        try:
            server_proc.terminate()
        except Exception:
            pass
        sys.exit(1)

    logging.info(f"[COORD] Successfully started {len(simulator_handles)} simulator(s).")

    try:
        while True:
            if server_proc.poll() is not None:
                logging.error("[COORD] tinyIoT server has stopped unexpectedly.")
                break
            running_sims = sum(1 for h in simulator_handles if h.proc.poll() is None)
            if running_sims == 0:
                logging.info("[COORD] All simulators have stopped.")
                break
            time.sleep(5)
    except KeyboardInterrupt:
        logging.info("[COORD] Shutting down (KeyboardInterrupt)...")
    finally:
        logging.info("[COORD] Terminating all processes...")
        for i, handle in enumerate(simulator_handles):
            if handle.proc and handle.proc.poll() is None:
                try:
                    logging.info(f"[COORD] Terminating simulator #{i + 1}...")
                    handle.terminate()
                    handle.proc.wait(timeout=5)
                except Exception as e:
                    logging.warning(f"[COORD] Failed to terminate simulator #{i + 1}: {e}; killing...")
                    handle.kill()
            handle.join_reader()
        if server_proc and server_proc.poll() is None:
            try:
                logging.info("[COORD] Terminating tinyIoT server...")
                server_proc.terminate()
                server_proc.wait(timeout=5)
            except Exception as e:
                logging.warning(f"[COORD] Failed to terminate server: {e}; killing...")
                try:
                    server_proc.kill()
                except Exception:
                    pass
        logging.info("[COORD] All processes terminated.")
