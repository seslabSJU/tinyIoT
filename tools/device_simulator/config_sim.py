"""Device simulator configuration for tinyIoT."""
import os

_CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))

# CSE identifiers shared by HTTP and MQTT transports.
CSE_NAME = "tinyiot"
CSE_RN = "TinyIoT"

# HTTP endpoint settings.
# configure for your environment
HTTP_HOST = "127.0.0.1"
HTTP_PORT = 3000
HTTP_BASE = f"http://{HTTP_HOST}:{HTTP_PORT}"
BASE_URL_RN = f"{HTTP_BASE}/{CSE_RN}"

# MQTT broker connection settings.
# configure for your environment 
MQTT_HOST = "127.0.0.1"
MQTT_PORT = 1883

# Optional broker credentials. Leave ``None`` (or empty) when Mosquitto allows anonymous clients.
MQTT_USER = "test"
MQTT_PASS = "mqtt"

# HTTP headers and content-type codes used by oneM2M requests.
HTTP_DEFAULT_HEADERS = {
    "Accept": "application/json",
    "X-M2M-Origin": "CAdmin",
    "X-M2M-RVI": "2a",
    "X-M2M-RI": "req",
}

HTTP_GET_HEADERS = {
    "Accept": "application/json",
    "X-M2M-Origin": "CAdmin",
    "X-M2M-RVI": "2a",
    "X-M2M-RI": "check",
}

HTTP_CONTENT_TYPE_MAP = {
    "ae": 2,
    "cnt": 3,
    "cin": 4,
}

# Resource metadata keyed by logical sensor name.
SENSOR_RESOURCES = {
    "temperature": {
        "ae": "CTemperatureSensor",
        "cnt": "temperature",
        "api": "N.temperature",
        "origin": "CTemperatureSensor",
    },
    "humidity": {
        "ae": "CHumiditySensor",
        "cnt": "humidity",
        "api": "N.humidity",
        "origin": "CHumiditySensor",
    },
    "co2": {
        "ae": "Cco2Sensor",
        "cnt": "co2",
        "api": "N.co2",
        "origin": "Cco2Sensor",
    },
    "soil": {
        "ae": "CsoilSensor",
        "cnt": "soil",
        "api": "N.soil",
        "origin": "CsoilSensor",
    },
}

# Fallback metadata used when a sensor definition is missing above.
GENERIC_SENSOR_TEMPLATE = {
    "ae": "C{sensor}Sensor",
    "cnt": "{sensor}",
    "api": "N.{sensor}",
    "origin": "C{sensor}Sensor",
}

# CSV fixtures used when sensors run in CSV mode.
TEMPERATURE_CSV = os.path.join(_CURRENT_DIR, "smartfarm_data", "temperature_data.csv")
HUMIDITY_CSV = os.path.join(_CURRENT_DIR, "smartfarm_data", "humidity_data.csv")
CO2_CSV = os.path.join(_CURRENT_DIR, "smartfarm_data", "co2_data.csv")
SOIL_CSV = os.path.join(_CURRENT_DIR, "smartfarm_data", "soil_data.csv")

# Random data-generation profiles for supported sensors.
# data_type can be set to int | float | string.
TEMPERATURE_PROFILE = {
    "data_type": "float",
    "min": 20.0,
    "max": 35.0,
}

HUMIDITY_PROFILE = {
    "data_type": "float",
    "min": 50.0,
    "max": 90.0,
}

CO2_PROFILE = {
    "data_type": "float",
    "min": 350.0,
    "max": 800.0,
}

SOIL_PROFILE = {
    "data_type": "float",
    "min": 20.0,
    "max": 60.0,
}

# Default random profile when a sensor is not explicitly listed above.
# data_type can be set to int | float | string.
GENERIC_RANDOM_PROFILE = {
    "data_type": "float",
    "min": 0.0,
    "max": 100.0,
}

# Timeout and retry behaviour (seconds) shared by both transports.
CONNECT_TIMEOUT = 2
READ_TIMEOUT = 10
RETRY_WAIT_SECONDS = 5
SEND_ERROR_THRESHOLD = 5
HTTP_REQUEST_TIMEOUT = (CONNECT_TIMEOUT, READ_TIMEOUT)

# Connection/response tuning.
MQTT_KEEPALIVE = 60
MQTT_CONNECT_WAIT = 10
MQTT_RESPONSE_TIMEOUT = 5

# Default container retention limits.
CNT_MNI = 1000
CNT_MBS = 10485760
