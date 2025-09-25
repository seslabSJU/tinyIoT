"""Single-sensor oneM2M device simulator for HTTP and MQTT transports."""

import argparse
import csv
import json
import random
import select
import socket
import string
import sys
import threading
import time
import uuid
from dataclasses import dataclass
from typing import Dict, Optional, Set

import paho.mqtt.client as mqtt
import requests

import config_sim as config

HTTP = requests.Session()
HTTP_TIMEOUT = (config.CONNECT_TIMEOUT, config.READ_TIMEOUT)

@dataclass
class DuplicateContext:
    protocol: str
    resource_path: str
    resource_url: Optional[str] = None
    status_code: Optional[int] = None
    rsc: Optional[str] = None

# ---------- Common utilities ----------

# Fetch and print the current state of the resource tree for context.
def display_resource_tree(resource_url: Optional[str] = None, resource_path: Optional[str] = None):
    """Dump the current resource tree to aid duplicate investigations."""
    tree_url = resource_url or config.BASE_URL_RN
    label = resource_path or (
        tree_url.replace(f"{config.HTTP_BASE}/", "") if tree_url.startswith(f"{config.HTTP_BASE}/") else tree_url
    )
    try:
        resp = HTTP.get(tree_url, headers=config.HTTP_GET_HEADERS, timeout=HTTP_TIMEOUT)
        print(f"[INFO] HTTP GET {label} -> {resp.status_code} ({tree_url})")
        if resp.status_code == 200:
            try:
                payload = resp.json()
                print(json.dumps(payload, indent=2, ensure_ascii=False))
            except Exception:
                print(resp.text)
        else:
            print(f"[WARN] Failed to fetch resource tree: {resp.status_code} {resp.text}")
    except Exception as exc:
        print(f"[ERROR] Exception while fetching resource tree: {exc}")


# Print a concise duplicate warning and any known status details.
def log_duplicate(kind: str, ctx: Optional[DuplicateContext]) -> None:
    """Log a high-level duplicate warning with protocol/status context."""
    if ctx:
        path_info = ctx.resource_path or ctx.resource_url or "(unknown path)"
        status_details = []
        if ctx.status_code is not None:
            status_details.append(f"HTTP {ctx.status_code}")
        if ctx.rsc:
            status_details.append(f"RSC {ctx.rsc}")
        status_txt = " / ".join(status_details) if status_details else "duplicate detected"
        print(f"[{kind}] Existing resource {path_info} ({ctx.protocol.upper()} {status_txt}).")
    else:
        print(f"[{kind}] Existing resource detected.")


# Optionally dump the resource tree when duplicates are detected.
def show_duplicate_tree(ctx: Optional[DuplicateContext]) -> None:
    """Display the resource tree when configured to assist troubleshooting."""
    if getattr(config, "SHOW_DUPLICATE_RESOURCE_TREE", True):
        url = ctx.resource_url if ctx and ctx.resource_url else None
        path = ctx.resource_path if ctx else None
        print("[INFO] Resource tree:")
        display_resource_tree(url, path)
    else:
        print("[INFO] Resource tree skipped (config).")

# Prompt the operator to decide whether to reuse an existing resource.
def prompt_continue(kind: str, ctx: Optional[DuplicateContext] = None) -> bool:
    """Ask the operator whether an existing resource should be reused."""
    log_duplicate(kind, ctx)
    show_duplicate_tree(ctx)
    print()
    print("[INFO] Enter 'y' to reuse the existing resource or 'n' to terminate the device simulator.")

    if not sys.stdin or not sys.stdin.isatty():
        print("[WARN] No interactive input available; aborting by default.")
        return False

    timeout_sec = float(getattr(config, "PROMPT_TIMEOUT_SECONDS", 30.0) or 0)
    while True:
        try:
            prompt = "Continue? (y/n): " if timeout_sec <= 0 else f"Continue? (y/n) [timeout {int(timeout_sec)}s]: "
            print()
            print(prompt, end="", flush=True)
            if timeout_sec > 0:
                ready, _, _ = select.select([sys.stdin], [], [], timeout_sec)
                if not ready:
                    print()
                    print("[ERROR] No response within the allotted time. Terminating.")
                    return False
                answer_raw = sys.stdin.readline()
            else:
                answer_raw = sys.stdin.readline()
            if answer_raw == "":
                print("[WARN] Input stream closed; aborting.")
                return False
            answer = answer_raw.strip().lower()
        except KeyboardInterrupt:
            print("\n[INFO] Stopping per user request (interrupt).")
            return False
        except Exception as exc:
            print(f"[ERROR] Input error ({exc}); aborting.")
            return False

        if not answer:
            print("[WARN] Empty response received; please enter y or n.")
            continue
        if answer in ("y", "yes"):
            print("[INFO] Continuing per user request.")
            return True
        if answer in ("n", "no"):
            print("[INFO] Stopping per user request. The device simulator will terminate.")
            return False
        print("[WARN] Please enter y or n.")

# Best-effort check that the HTTP endpoint is accepting connections.
def check_http_reachable() -> bool:
    """Return True when the HTTP endpoint is reachable at connect time."""
    try:
        with socket.create_connection((config.HTTP_HOST, int(config.HTTP_PORT)), timeout=config.CONNECT_TIMEOUT):
            return True
    except Exception:
        return False

# Remember duplicate-handling decisions per sensor to avoid repeat prompts.
class DuplicateGuard:
    """Cache duplicate decisions per sensor so repeat prompts are avoided."""
    def __init__(self, sensor_name: str, default_decision: Optional[bool] = None,
                 remember_decision: bool = True):
        self.sensor_name = sensor_name
        self.decision: Optional[bool] = default_decision
        self._locked = default_decision is not None
        self._remember = remember_decision
        self._locked_resources: Set[str] = set()

    def _notify_locked(self, resource_desc: str, ctx: Optional[DuplicateContext]) -> None:
        """Emit the cached duplicate decision once for each resource id."""
        if resource_desc in self._locked_resources:
            return
        self._locked_resources.add(resource_desc)
        log_duplicate(resource_desc, ctx)
        show_duplicate_tree(ctx)
        print()
        mode = "yes" if self.decision else "no"
        action = "Auto-approved" if self.decision else "Auto-rejected"
        suffix = "" if self.decision else " The device simulator will terminate."
        print(f"[{resource_desc}] {action} (reuse-existing={mode}).{suffix}")

    def confirm(self, resource_desc: str, ctx: Optional[DuplicateContext] = None) -> bool:
        """Return whether the guard allows reuse of an existing resource."""
        if self._locked:
            self._notify_locked(resource_desc, ctx)
            return bool(self.decision)
        if self.decision is None:
            ok = prompt_continue(resource_desc, ctx)
            if self._remember:
                self.decision = ok
            return ok
        if self.decision:
            print(f"[{resource_desc}] Previously approved for sensor '{self.sensor_name}'. Continuing.")
            return True
        print(f"[{resource_desc}] Previously rejected for sensor '{self.sensor_name}'. The device simulator will terminate.")
        return False


# Helper that builds HTTP headers with correct origin and content-type.
class Headers:
    """Build HTTP headers with the desired origin and resource type."""
    def __init__(self, content_type: Optional[str] = None, origin: str = "CAdmin", ri: str = "req"):
        self.headers = dict(config.HTTP_DEFAULT_HEADERS)
        self.headers["X-M2M-Origin"] = origin
        self.headers["X-M2M-RI"] = ri
        if content_type:
            self.headers["Content-Type"] = f"application/json;ty={self.get_content_type(content_type)}"

    @staticmethod
    def get_content_type(content_type: str):
        return config.HTTP_CONTENT_TYPE_MAP.get(content_type)


def url_ae(ae: str) -> str:
    return f"{config.BASE_URL_RN}/{ae}"


def url_cnt(ae: str, cnt: str) -> str:
    return f"{config.BASE_URL_RN}/{ae}/{cnt}"

# Issue an HTTP POST and classify the response for registration flows.
def request_post(url, headers, body, kind=""):
    """Issue an HTTP POST and classify outcomes for registration flows."""
    try:
        r = HTTP.post(url, headers=headers, json=body, timeout=HTTP_TIMEOUT)
        if r.status_code in (200, 201):
            return "ok", r
        x_rsc = str(r.headers.get("X-M2M-RSC", r.headers.get("x-m2m-rsc", ""))).strip()
        if r.status_code == 409 or x_rsc == "4105":
            header_info = x_rsc or "N/A"
            print(f"[WARN] POST {kind} {url} -> duplicate detected (status={r.status_code}, x-m2m-rsc={header_info})")
            return "duplicate", r
        try:
            text = r.json()
        except Exception:
            text = r.text
        if any(k in str(text).lower() for k in ("duplicat", "already exist", "exists")):
            print(f"[WARN] POST {kind} {url} -> duplicate hint in payload")
            return "duplicate", r
        print(f"[ERROR] POST {kind} {url} -> {r.status_code} {text}")
        return "error", r
    except requests.exceptions.ReadTimeout as e:
        print(f"[WARN] POST timeout on {url}: {e}")
        return "timeout", None
    except Exception as e:
        print(f"[ERROR] POST {url} failed: {e}")
        return "error", None


def extract_rsc_header(response) -> str:
    if not response:
        return ""
    return str(response.headers.get("X-M2M-RSC") or response.headers.get("x-m2m-rsc") or "").strip()

# Fetch the latest content instance value to confirm storage after timeouts.
def get_latest_con(ae, cnt) -> Optional[str]:
    """Fetch the latest content instance value for timeout verification."""
    la = f"{url_cnt(ae, cnt)}/la"
    try:
        r = HTTP.get(la, headers=config.HTTP_GET_HEADERS, timeout=HTTP_TIMEOUT)
        if r.status_code == 200:
            js = r.json()
            return js.get("m2m:cin", {}).get("con")
    except Exception:
        pass
    return None

# Publish a contentInstance via HTTP and retry on intermittent failures.
def send_cin_http(ae, cnt, value) -> bool:
    """Send a contentInstance over HTTP and survive transient failures."""
    hdr = Headers(content_type="cin", origin=ae).headers
    body = {"m2m:cin": {"con": value}}
    u = url_cnt(ae, cnt)
    try:
        r = HTTP.post(u, headers=hdr, json=body, timeout=HTTP_TIMEOUT)
        if r.status_code in (200, 201):
            return True
        try:
            text = r.json()
        except Exception:
            text = r.text
        print(f"[ERROR] POST {u} -> {r.status_code} {text}")
        return False
    except requests.exceptions.ReadTimeout:
        latest = get_latest_con(ae, cnt)
        if latest == str(value):
            print("[WARN] POST timed out but verified via /la (stored).")
            return True
        print("[WARN] POST timed out and not verified; will retry.")
        return False
    except Exception as e:
        print(f"[ERROR] POST {u} failed: {e}")
        return False

# Create AE/CNT resources over HTTP while honoring duplicate guard decisions.
def ensure_registration_http_postonly(ae, cnt, api, guard: DuplicateGuard) -> bool:
    """Create AE/CNT resources via HTTP while consulting the duplicate guard."""
    status_ae, resp_ae = request_post(
        config.BASE_URL_RN,
        Headers("ae", origin=ae).headers,
        {"m2m:ae": {"rn": ae, "api": api, "rr": True}},
        "AE"
    )
    if status_ae == "duplicate":
        ctx = DuplicateContext(
            protocol="HTTP",
            resource_path=f"{config.CSE_RN}/{ae}",
            resource_url=url_ae(ae),
            status_code=resp_ae.status_code if resp_ae else None,
            rsc=extract_rsc_header(resp_ae),
        )
        if not guard.confirm(f"AE '{ae}'", ctx=ctx):
            return False
    elif status_ae != "ok":
        return False

    status_cnt, resp_cnt = request_post(
        url_ae(ae),
        Headers("cnt", origin=ae).headers,
        {"m2m:cnt": {"rn": cnt, "mni": config.CNT_MNI, "mbs": config.CNT_MBS}},
        "CNT"
    )
    if status_cnt == "duplicate":
        ctx = DuplicateContext(
            protocol="HTTP",
            resource_path=f"{config.CSE_RN}/{ae}/{cnt}",
            resource_url=url_cnt(ae, cnt),
            status_code=resp_cnt.status_code if resp_cnt else None,
            rsc=extract_rsc_header(resp_cnt),
        )
        if not guard.confirm(f"CNT '{ae}/{cnt}'", ctx=ctx):
            return False
    else:
        if status_cnt != "ok":
            return False

    return True

# Return a random value within the configured profile bounds.
def generate_random_value_from_profile(profile: Dict) -> str:
    """Return a random value constrained by the provided profile."""
    dt = profile.get("data_type", "float")
    if dt == "int":
        return str(random.randint(int(profile.get("min", 0)), int(profile.get("max", 100))))
    if dt == "float":
        return f"{random.uniform(float(profile.get("min", 0.0)), float(profile.get("max", 100.0))):.2f}"
    if dt == "string":
        length = int(profile.get("length", 8))
        return "".join(random.choices(string.ascii_letters + string.digits, k=length))
    return "0"

# ---------- Sensor metadata (flexible) ----------

# Resolve dynamic sensor metadata from config templates and overrides.
def build_sensor_meta(name: str) -> Dict:
    """Assemble per-sensor metadata using config entries and fallbacks."""
    sensor_key = name.lower()
    upper = sensor_key.upper()

    resources = getattr(config, "SENSOR_RESOURCES", {})
    meta = dict(resources.get(sensor_key, {}))

    if not meta:
        template = getattr(config, "GENERIC_SENSOR_TEMPLATE", {})
        if template:
            meta = {k: v.format(sensor=sensor_key) for k, v in template.items()}
        else:
            meta = {
                "ae": f"C{sensor_key}Sensor",
                "cnt": sensor_key,
                "api": f"N.{sensor_key}",
                "origin": f"C{sensor_key}Sensor",
            }

    meta.setdefault("ae", f"C{sensor_key}Sensor")
    meta.setdefault("cnt", sensor_key)
    meta.setdefault("api", f"N.{sensor_key}")
    meta.setdefault("origin", meta.get("ae") or f"C{sensor_key}Sensor")

    meta["profile"] = meta.get(
        "profile",
        getattr(config, f"{upper}_PROFILE", {"data_type": "float", "min": 0, "max": 100})
    )
    meta["csv"] = meta.get("csv", getattr(config, f"{upper}_CSV", None))

    return meta

# ---------- MQTT oneM2M client (req/resp) ----------

# Minimal MQTT client for oneM2M request/response interactions.
class MqttOneM2MClient:
    """Lightweight MQTT helper implementing oneM2M request/response."""
    def __init__(self, broker, port, origin, cse_csi, cse_rn="TinyIoT", duplicate_guard: Optional[DuplicateGuard] = None):
        self.broker = broker
        self.port = int(port)
        self.origin = origin
        self.cse_csi = cse_csi
        self.cse_rn = cse_rn
        self.response_received = threading.Event()
        self.last_response = None
        self.duplicate_guard = duplicate_guard

        self.client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect

        self.req_topic = f"/oneM2M/req/{self.origin}/{self.cse_csi}/json"
        self.resp_topic = f"/oneM2M/resp/{self.origin}/{self.cse_csi}/json"

    def on_connect(self, client, userdata, flags, reason_code, properties=None):
        code = getattr(reason_code, "value", reason_code)
        code = 0 if code is None else code
        print("[MQTT] Connection successful." if code == 0 else f"[MQTT] Connection failed: rc={code}")

    def on_disconnect(self, client, userdata, reason_code, properties=None):
        code = getattr(reason_code, "value", reason_code)
        code = 0 if code is None else code
        print(f"[MQTT] Disconnected rc={code}")

    def on_message(self, client, userdata, msg):
        try:
            payload_txt = msg.payload.decode()
            self.last_response = json.loads(payload_txt)
            self.response_received.set()
            print(f"[MQTT][RECV] {payload_txt}")
        except Exception as e:
            print(f"[ERROR] Failed to parse MQTT response: {e}")

    def connect(self) -> bool:
        try:
            self.client.connect(self.broker, self.port, keepalive=60)
            self.client.loop_start()
            self.client.subscribe(self.resp_topic, qos=0)
            print(f"[MQTT] SUB {self.resp_topic}")
            print(f"[MQTT] Connected to broker {self.broker}:{self.port}")
            return True
        except Exception as e:
            print(f"[ERROR] Failed to connect to MQTT broker: {e}")
            return False

    def disconnect(self):
        try:
            self.client.loop_stop()
        finally:
            self.client.disconnect()
        print("[MQTT] Disconnected.")

    def _send_request(self, body, ok_rsc=(2000, 2001, 2004)):
        """Send a oneM2M MQTT request and wait for the matching response."""
        request_id = str(uuid.uuid4())
        message = {
            "fr": self.origin,
            "to": body["to"],
            "op": body["op"],
            "rqi": request_id,
            "ty": body.get("ty"),
            "pc": body.get("pc", {}),
            "rvi": "3"
        }
        print(f"[MQTT][SEND] {json.dumps(message, ensure_ascii=False)}")
        self.response_received.clear()
        self.client.publish(self.req_topic, json.dumps(message))
        if self.response_received.wait(timeout=5):
            response = self.last_response or {}
            rsc_raw = response.get("rsc")
            try:
                rsc = int(rsc_raw)
            except Exception:
                rsc = 0
            if rsc == 4105:
                return "duplicate", response
            if rsc in ok_rsc:
                return "ok", response
            return "error", response
        print("[ERROR] No MQTT response within timeout.")
        return "timeout", None

    def create_ae(self, ae_name):
        """Create an AE over MQTT, invoking the duplicate guard if needed."""
        status, response = self._send_request({
            "to": f"{self.cse_rn}",
            "op": 1,
            "ty": 2,
            "pc": {"m2m:ae": {"rn": ae_name, "api": "N.device", "rr": True}}
        }, ok_rsc=(2001,))
        if status == "duplicate":
            guard = self.duplicate_guard or DuplicateGuard(ae_name)
            ctx = DuplicateContext(
                protocol="MQTT",
                resource_path=f"{self.cse_rn}/{ae_name}",
                resource_url=f"{config.BASE_URL_RN}/{ae_name}",
                rsc=str(response.get("rsc")) if response else None,
            )
            return guard.confirm(f"AE '{ae_name}'", ctx=ctx)
        if status == "ok":
            return True
        if response:
            print(f"[ERROR] AE create failed rsc={response.get('rsc')} msg={response}")
        return False

    def create_cnt(self, ae_name, cnt_name):
        """Create a CNT over MQTT, invoking the duplicate guard if needed."""
        status, response = self._send_request({
            "to": f"{self.cse_rn}/{ae_name}",
            "op": 1,
            "ty": 3,
            "pc": {"m2m:cnt": {"rn": cnt_name}}
        }, ok_rsc=(2001,))
        if status == "duplicate":
            guard = self.duplicate_guard or DuplicateGuard(ae_name)
            ctx = DuplicateContext(
                protocol="MQTT",
                resource_path=f"{self.cse_rn}/{ae_name}/{cnt_name}",
                resource_url=f"{config.BASE_URL_RN}/{ae_name}/{cnt_name}",
                rsc=str(response.get("rsc")) if response else None,
            )
            return guard.confirm(f"CNT '{ae_name}/{cnt_name}'", ctx=ctx)
        if status == "ok":
            return True
        if response:
            print(f"[ERROR] CNT create failed rsc={response.get('rsc')} msg={response}")
        return False

    def send_cin(self, ae_name, cnt_name, value):
        """Publish a CIN over MQTT, tolerating duplicate acknowledgements."""
        status, response = self._send_request({
            "to": f"{self.cse_rn}/{ae_name}/{cnt_name}",
            "op": 1,
            "ty": 4,
            "pc": {"m2m:cin": {"con": value}}
        }, ok_rsc=(2001,))
        if status == "ok":
            return True
        if status == "duplicate":
            print("[WARN] Unexpected duplicate response while sending CIN; continuing.")
            return True
        if status == "timeout":
            print("[ERROR] No MQTT response within timeout during CIN send.")
        elif response:
            print(f"[ERROR] CIN send failed rsc={response.get('rsc')} msg={response}")
        return False

# ---------- Single-sensor worker ----------

# Own the lifecycle of a single sensor simulator instance.
class SensorWorker:
    """Manage the lifecycle of a single simulated sensor."""
    def __init__(self, sensor_name: str, protocol: str, mode: str,
                 period_sec: float, registration: int, reuse_existing: str):
        self.meta = build_sensor_meta(sensor_name)
        self.sensor_name = sensor_name
        self.protocol = protocol
        self.mode = mode
        self.period_sec = float(period_sec)
        self.registration = registration
        self.stop_flag = threading.Event()
        self.csv_data, self.csv_index, self.err = [], 0, 0
        self.mqtt = None
        default_choice: Optional[bool] = None
        remember_decision = True
        if reuse_existing == "yes":
            default_choice = True
        elif reuse_existing == "no":
            default_choice = False
        else:
            remember_decision = False
        self.duplicate_guard = DuplicateGuard(
            sensor_name,
            default_decision=default_choice,
            remember_decision=remember_decision
        )

    def setup(self):
        """Prepare transports, register resources, and load any CSV data."""
        if self.protocol == "http":
            if not check_http_reachable():
                print(f"[ERROR] Cannot connect HTTP: {config.HTTP_HOST}:{config.HTTP_PORT}")
                raise SystemExit(1)
            if self.registration == 1:
                if not ensure_registration_http_postonly(self.meta["ae"], self.meta["cnt"], self.meta["api"], self.duplicate_guard):
                    print("[ERROR] HTTP registration failed.")
                    raise SystemExit(1)
        else:
            self.mqtt = MqttOneM2MClient(config.MQTT_HOST, config.MQTT_PORT,
                                         self.meta["origin"], config.CSE_NAME, config.CSE_RN,
                                         duplicate_guard=self.duplicate_guard)
            if not self.mqtt.connect():
                raise SystemExit(1)
            if self.registration == 1:
                if not self.mqtt.create_ae(self.meta["ae"]):
                    raise SystemExit(1)
                if not self.mqtt.create_cnt(self.meta["ae"], self.meta["cnt"]):
                    raise SystemExit(1)

        if self.mode == "csv":
            path = self.meta.get("csv")
            if not path:
                print(f"[{self.sensor_name.upper()}][ERROR] CSV path not configured in config for '{self.sensor_name}'.")
                raise SystemExit(1)
            try:
                with open(path, "r") as f:
                    rows = list(csv.reader(f))
                    self.csv_data = [r[0].strip() for r in rows if r]
            except Exception as e:
                print(f"[{self.sensor_name.upper()}][ERROR] CSV open failed: {e}")
                raise SystemExit(1)
            if not self.csv_data:
                print(f"[{self.sensor_name.upper()}][ERROR] CSV empty.")
                raise SystemExit(1)

    def stop(self):
        """Signal the worker loop to exit."""
        self.stop_flag.set()

    def _next_value(self) -> str:
        """Return the next payload from CSV or from the random generator."""
        if self.mode == "csv":
            v = self.csv_data[self.csv_index]
            self.csv_index += 1
            return v
        return generate_random_value_from_profile(self.meta["profile"])

    def run(self):
        """Main send loop honoring the chosen period, mode, and retry policy."""
        print(f"[{self.sensor_name.upper()}] run (protocol={self.protocol}, mode={self.mode}, period={self.period_sec}s)")
        next_send = time.time() + self.period_sec
        try:
            while not self.stop_flag.is_set():
                remaining = next_send - time.time()
                if remaining > 0:
                    time.sleep(remaining)
                if self.stop_flag.is_set():
                    break

                value = self._next_value()
                ok = (
                    send_cin_http(self.meta["ae"], self.meta["cnt"], value)
                    if self.protocol == "http"
                    else self.mqtt.send_cin(self.meta["ae"], self.meta["cnt"], value)
                )

                if ok:
                    tag = self.sensor_name.upper()
                    print(f"[{tag}] Sent value: {value}\n")
                    self.err = 0
                else:
                    self.err += 1
                    print(f"[{self.sensor_name.upper()}][ERROR] send failed: {value} (retry in {config.RETRY_WAIT_SECONDS}s)")
                    time.sleep(config.RETRY_WAIT_SECONDS)
                    if self.err >= config.SEND_ERROR_THRESHOLD:
                        print(f"[{self.sensor_name.upper()}][ERROR] repeated failures. stop.")
                        break

                if self.mode == "csv" and self.csv_index >= len(self.csv_data):
                    print(f"[{self.sensor_name.upper()}] CSV done. stop.")
                    break

                next_send += self.period_sec
        finally:
            if self.mqtt:
                try:
                    self.mqtt.disconnect()
                except Exception:
                    pass
            print(f"[{self.sensor_name.upper()}] stopped.")

# ---------- CLI ----------

# Parse CLI arguments into a simple namespace.
def parse_args(argv):
    """Parse CLI arguments into a simple namespace."""
    p = argparse.ArgumentParser(
        description="Single-sensor oneM2M simulator (HTTP/MQTT)."
    )
    p.add_argument("--sensor", required=True)
    p.add_argument("--protocol", choices=["http", "mqtt"], required=True)
    p.add_argument("--mode", choices=["csv", "random"], required=True)
    p.add_argument("--frequency", type=float, required=True)
    p.add_argument("--registration", type=int, choices=[0, 1], required=True)
    p.add_argument(
        "--reuse-existing",
        choices=["ask", "yes", "no"],
        default="ask",
        help="Duplicate handling policy: 'ask' (default) prompts, 'yes' auto-reuses, 'no' aborts."
    )
    return p.parse_args(argv)

def main():
    args = parse_args(sys.argv[1:])
    worker = SensorWorker(
        sensor_name=args.sensor,
        protocol=args.protocol,
        mode=args.mode,
        period_sec=args.frequency,
        registration=args.registration,
        reuse_existing=args.reuse_existing
    )
    worker.setup()
    try:
        worker.run()
    except KeyboardInterrupt:
        worker.stop()
        print("\n[SIM] Interrupted.")

if __name__ == "__main__":
    main()
