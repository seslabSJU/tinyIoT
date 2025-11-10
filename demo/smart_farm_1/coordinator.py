"""Start the tinyIoT server and a batch of sensor device simulators in sequence."""

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
    """Configuration bundle describing a sensor simulator start request."""

    def __init__(self, sensor_type: str, protocol: str = 'mqtt', mode: str = 'random',
                 frequency: int = 2, registration: int = 1) -> None:
        """Store the simulator command parameters for a single sensor.

        Args:
            sensor_type: Name of the sensor (must match simulator metadata keys).
            protocol: ``http`` or ``mqtt`` communication method.
            mode: ``random`` or ``csv`` data generation strategy.
            frequency: Sampling interval between readings in seconds.
            registration: ``1`` to perform AE/CNT registration, ``0`` to skip.
        """
        self.sensor_type = sensor_type
        self.protocol = protocol
        self.mode = mode
        self.frequency = frequency
        self.registration = registration


# Sensor definitions live in config_coord.SENSORS so deployments remain configurable
# without modifying this module. Build the resolved objects once for reuse.


def _load_sensor_configs() -> List[SensorConfig]:
    """Build ``SensorConfig`` objects from ``config_coord.SENSORS``."""
    raw_options = getattr(config, 'SENSORS', [])
    sensors: List[SensorConfig] = []

    for idx, option in enumerate(raw_options):
        if isinstance(option, SensorConfig):
            sensors.append(option)
            continue
        if isinstance(option, dict):
            try:
                sensor_kwargs = option.copy()
                if 'sensor' in sensor_kwargs and 'sensor_type' not in sensor_kwargs:
                    sensor_kwargs['sensor_type'] = sensor_kwargs.pop('sensor')
                sensors.append(SensorConfig(**sensor_kwargs))
            except TypeError as exc:
                logging.error(
                    f"[COORD] Invalid sensor option at index {idx}: {option!r} ({exc})"
                )
        else:
            logging.error(
                f"[COORD] Invalid sensor option type at index {idx}: {type(option)!r}"
            )

    if not sensors:
        logging.warning("[COORD] No valid sensor options found; nothing will be started.")
    return sensors


SENSORS_TO_RUN: List[SensorConfig] = _load_sensor_configs()


def wait_for_server(timeout: int = getattr(config, 'WAIT_SERVER_TIMEOUT', 30)) -> bool:
    """Poll the HTTP endpoint until the CSE responds with 200 OK."""
    headers = config.HEALTHCHECK_HEADERS
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



class SimulatorHandle:
    """Wrapper that watches simulator output and exposes readiness helpers."""

    def __init__(self, proc: subprocess.Popen, sensor_type: str) -> None:
        """Attach to the simulator process and start monitoring output."""
        self.proc = proc
        self.sensor_type = sensor_type
        self._ready = threading.Event()
        self._failed = threading.Event()
        self._reader = threading.Thread(target=self._pump_output, daemon=True)
        self._reader.start()

    def _pump_output(self) -> None:
        """Stream simulator stdout, echo to console, and update readiness flags."""
        tag = f"[{self.sensor_type.upper()}]"
        try:
            for line in self.proc.stdout:
                text = line.rstrip('\r\n')
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
        """Request a graceful shutdown of the simulator process."""
        if self.proc and self.proc.poll() is None:
            try:
                self.proc.terminate()
            except Exception:
                pass

    def kill(self) -> None:
        """Forcefully stop the simulator process."""
        if self.proc and self.proc.poll() is None:
            try:
                self.proc.kill()
            except Exception:
                pass

    def join_reader(self, timeout: float = 1.0) -> None:
        """Wait for the background log reader to finish."""
        if self._reader.is_alive():
            self._reader.join(timeout)


def start_simulator(sensor_config: SensorConfig, index: int) -> Optional[SimulatorHandle]:
    """Start one simulator subprocess and return a monitor handle (None on failure)."""
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
    logging.info(f"[COORD] Starting simulator [{sensor_config.sensor_type}]...")
    logging.debug(f"[COORD] Command: {' '.join(sim_args)}")
    env = os.environ.copy()
    sim_env = getattr(config, 'SIM_ENV', None)
    if isinstance(sim_env, dict):
        env.update(sim_env)
    env['PYTHONUNBUFFERED'] = '1'
    env['SKIP_HEALTHCHECK'] = '1'
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
    # Launch the server, start simulators, supervise them, then clean up.
    logging.info("[COORD] Starting tinyIoT server...")
    try:
        server_proc = subprocess.Popen([config.SERVER_EXEC])
    except Exception as e:
        logging.error(f"[COORD] Failed to start tinyIoT server: {e}")
        sys.exit(1)

    # Stop early if the server never reports healthy.
    if not wait_for_server():
        try:
            server_proc.terminate()
        except Exception:
            pass
        sys.exit(1)

    simulator_handles: List[SimulatorHandle] = []
    logging.info(f"[COORD] starting {len(SENSORS_TO_RUN)} sensor simulators...")

    try:
        # Start simulators sequentially and stop the sequence on failure.
        for i, sensor_conf in enumerate(SENSORS_TO_RUN):
            handle = start_simulator(sensor_conf, i)
            if not handle:
                logging.error(f"[COORD] Failed to start simulator [{sensor_conf.sensor_type}]")
                continue
            logging.info(f"[COORD] Waiting for simulator [{sensor_conf.sensor_type}] to finish registration...")
            if handle.wait_until_ready():
                logging.info(f"[COORD] Simulator [{sensor_conf.sensor_type}] ready.")
                simulator_handles.append(handle)
            else:
                # Abort additional launches if one simulator fails during initialization.
                logging.error(f"[COORD] Simulator [{sensor_conf.sensor_type}] failed during setup; terminating start sequence.")
                handle.terminate()
                handle.join_reader()
                break
    except KeyboardInterrupt:
        logging.info("[COORD] start interrupted by user.")

    # Shut down immediately if every simulator failed to launch.
    if not simulator_handles:
        logging.error("[COORD] No simulators were started successfully.")
        try:
            server_proc.terminate()
            server_proc.wait(timeout=getattr(config, 'SERVER_TERM_WAIT', 5))
        except Exception:
            pass
        sys.exit(1)

    ok = len(simulator_handles)
    need = len(SENSORS_TO_RUN)
    if ok != need:
        # Tear down partially started runs to avoid inconsistent state.
        ok_names = [h.sensor_type for h in simulator_handles]
        need_names = [c.sensor_type for c in SENSORS_TO_RUN]
        fail_names = [n for n in need_names if n not in ok_names]
        logging.error(f"[COORD] start failed: started {ok}/{need}. failed={fail_names}")

        for handle in simulator_handles:
            if handle.proc and handle.proc.poll() is None:
                try:
                    logging.info(f"[COORD] Terminating simulator [{handle.sensor_type}]...")
                    handle.terminate()
                    handle.proc.wait(timeout=getattr(config, 'PROC_TERM_WAIT', 5))
                except Exception:
                    logging.warning(f"[COORD] Failed to terminate simulator [{handle.sensor_type}]; killing...")
                    handle.kill()
            handle.join_reader(getattr(config, 'JOIN_READER_TIMEOUT', 1.0))

        if server_proc and server_proc.poll() is None:
            try:
                logging.info("[COORD] Terminating tinyIoT server...")
                server_proc.terminate()
                server_proc.wait(timeout=getattr(config, 'SERVER_TERM_WAIT', 5))
            except Exception:
                logging.warning("[COORD] Failed to terminate server; killing...")
                try:
                    server_proc.kill()
                except Exception:
                    pass
        sys.exit(1)

    logging.info("[COORD] Successfully started all simulators.")

    try:
        # Monitor the processes until the server stops or every simulator exits.
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
        # Terminate all processes before exiting the coordinator.
        logging.info("[COORD] Terminating all processes...")
        cleanup_interrupted = False

        for handle in simulator_handles:
            if handle.proc and handle.proc.poll() is None:
                try:
                    logging.info(f"[COORD] Terminating simulator [{handle.sensor_type}]...")
                    handle.terminate()
                    handle.proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    logging.warning(f"[COORD] Simulator [{handle.sensor_type}] did not exit in time; killing...")
                    handle.kill()
                except KeyboardInterrupt:
                    cleanup_interrupted = True
                    logging.warning(
                        f"[COORD] Cleanup interrupted while waiting for simulator [{handle.sensor_type}]; killing..."
                    )
                    handle.kill()
                except Exception as e:
                    logging.warning(f"[COORD] Failed to terminate simulator [{handle.sensor_type}]: {e}; killing...")
                    handle.kill()
            handle.join_reader()

        if server_proc and server_proc.poll() is None:
            try:
                logging.info("[COORD] Terminating tinyIoT server...")
                server_proc.terminate()
                server_proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                logging.warning("[COORD] Server did not exit in time; killing...")
                try:
                    server_proc.kill()
                except Exception:
                    pass
            except KeyboardInterrupt:
                cleanup_interrupted = True
                logging.warning("[COORD] Cleanup interrupted while waiting for server; killing...")
                try:
                    server_proc.kill()
                except Exception:
                    pass
            except Exception as e:
                logging.warning(f"[COORD] Failed to terminate server: {e}; killing...")
                try:
                    server_proc.kill()
                except Exception:
                    pass

        if cleanup_interrupted:
            logging.warning("[COORD] Cleanup was interrupted by user input; forced termination may leave child logs incomplete.")
        logging.info("[COORD] All processes terminated.")
