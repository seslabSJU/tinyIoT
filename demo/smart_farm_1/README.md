# Simulator and Coordinator Using smartfarm Data

## Overview

This project provides:

- **Device simulators** that send Temperature, Humidity, CO₂, and Soil data to a tinyIoT server
- A **coordinator script** that controls and manages all processes
- CSV data files leveraging smartfarm data (Provide approximately 1,000 real data points from September 24–27, 2025)

The simulators follow the **oneM2M standard** and support both **HTTP** and **MQTT** as data transport protocols.

## File Structure

- `coordinator.py` – Main script to control the tinyIoT server and simulators
- `simulator.py` – Temperature, Humidity, CO₂, and Soil sensor simulator
- `config_coord.py` – Global configuration values (for `coordination.py`)
- `config_sim.py` – Global configuration values (for `simulator.py`)
- `temperatuer_data.csv` – Smartfarm temperature CSV data
- `humidity_data.csv` – Smartfarm humidity CSV data
- `co2_data.csv` – Smartfarmco2 CSV data
- `soil_data.csv` – Smartfarm soil CSV data

## Features

- **Unified control** via `coordination.py`
- **Simulation of Temperature, Humidity, CO₂, and Soil sensor**
- **Protocols**: choose **HTTP** or **MQTT**
- **Modes**
    - `csv`: send smartfarm data sequentially from a CSV file
    - `random`: generate random data within configured ranges
- **Automatic registration**: create oneM2M AE/CNT resources with the `--registration` option

## Environment

- **OS**: Ubuntu 24.04.2 LTS
- **Language**: Python 3.8+
- **IoT**: tinyIoT
- **MQTT Broker**: Mosquitto

## Installation

### 1. tinyIoT Setup
1. Go to the relevant URL and set up the environment according to the README on the tinyIoT’s GitHub. 
```bash
https://github.com/seslabSJU/tinyIoT
```


2. tinyIoT `config.h` settings
- Verify that the following settings are correctly configured (Configure the IP and port as appropriate for your setup):

```c
// #define NIC_NAME "eth0"
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT "3000"
#define CSE_BASE_NAME "TinyIoT"
#define CSE_BASE_RI "tinyiot"
#define CSE_BASE_SP_ID "tinyiot.example.com"
#define CSE_RVI RVI_2a

```

- Uncomment `#define ENABLE_MQTT`.

```c
// To enable MQTT, uncomment the line below
#define ENABLE_MQTT

#ifdef ENABLE_MQTT
#define MQTT_HOST "127.0.0.1"
#define MQTT_QOS MQTT_QOS_0
#define MQTT_KEEP_ALIVE_SEC 60
#define MQTT_CMD_TIMEOUT_MS 30000
#define MQTT_CON_TIMEOUT_MS 5000
#define MQTT_CLIENT_ID "TinyIoT"
#define MQTT_USERNAME "test"
#define MQTT_PASSWORD "mqtt"

```


### 2. smart_farm_1 repository clone
1. Another tinyIoT clone; set a folder name that does not conflict with the existing tinyIoT directory.

```bash
git clone https://github.com/seslabSJU/tinyIoT.git {folder_name}
```

2. Enter the save directory.

```bash
cd folder_name
```

3. Switch to the dev branch.

```bash
git switch dev
```


### 2. Simulator Setup

1. Setting
- Modify the contents of **config_sim.py** to suit your needs.

```python

# CSE identifiers shared by HTTP and MQTT transports.
CSE_NAME = "tinyiot"
CSE_RN = "TinyIoT"

# HTTP endpoint settings.
HTTP_HOST = "127.0.0.1"
HTTP_PORT = 3000
HTTP_BASE = f"http://{HTTP_HOST}:{HTTP_PORT}"
BASE_URL_RN = f"{HTTP_BASE}/{CSE_RN}"

# MQTT broker connection settings.
MQTT_HOST = "127.0.0.1"
MQTT_PORT = 1883

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

```


### 3. coordinator setup

1. In `config_coord.py`, set the paths for `simulator.py` and tinyIoT (enter paths excluding “Ubuntu” in the string, per your environment).

```python
# IMPORTANT: SERVER_EXEC is an external dependency.
# Please configure the absolute path to the tinyIoT server executable for your environment.
SERVER_EXEC = "/home/parks/tinyIoT/source/server/server"  #tinyIoT

# Health-check target served by the oneM2M CSE.
CSE_URL = "http://127.0.0.1:3000/tinyiot"

```

2. You can modify the header resource settings and various other configurations in `config_coord.py` to suit your environment.

```python
# Headers forwarded with the health-check GET call.
HEALTHCHECK_HEADERS = {
    "X-M2M-Origin": "CAdmin",
    "X-M2M-RVI": "2a",
    "X-M2M-RI": "healthcheck",
    "Accept": "application/json",
}
```

3. Set options in `coordinator.py`
- You can add or remove sensors as needed for your environment. (However, since this demo simulates sensors using smartfarm data, you should not modify the sensors.)
- In this demo we use smartfarm data, so set the mode to `csv`.

```python
# Sensor Configuration
# Users can add or remove sensors here to suit their setup.
# Sensor names must not contain spaces and only lowercase.
# --sensor: "temp" | "humid" | "co2" | "soil" | etc
# --protocol: "http" | "mqtt"
# --mode: "csv" | "random"
# --frequency: seconds
# --registration: 0 | 1 (Select 1: Enable AE/CNT auto-registration)
SENSORS = [
    {"sensor": "temperature", "protocol": "http", "mode": "csv", "frequency": 3, "registration": 1},
    {"sensor": "humidity", "protocol": "http", "mode": "csv", "frequency": 3, "registration": 1},
    {"sensor": "co2", "protocol": "mqtt", "mode": "csv", "frequency": 3, "registration": 1},
    {"sensor": "soil", "protocol": "mqtt", "mode": "csv", "frequency": 3, "registration": 1},
]
```

### 4. MQTT Broker (Mosquitto) Setup

- **Mosquitto installation required**
1. Visit the website
    
    [https://mosquitto.org](https://mosquitto.org/)
    
2. Click **Download** on the site


3. Edit `mosquitto.conf`:
- Add `listener 1883`, `protocol mqtt`, `allow_anonymous true`

```conf
# listener port-number [ip address/host name/unix socket path]
listener 1883
protocol mqtt
```

```conf
# the local machine.
allow_anonymous false
```

4. Run

```bash
sudo systemctl start mosquitto
sudo systemctl status mosquitto
```

**result:**
```bash
● mosquitto.service - Mosquitto MQTT Broker
     Loaded: loaded (/lib/systemd/system/mosquitto.service; enabled; vendor preset: enabled)
     Active: active (running) since Wed 2024-01-01 12:00:00 KST; 1min 30s ago
       Docs: man:mosquitto.conf(5)
             man:mosquitto(8)
   Main PID: 1234 (mosquitto)
      Tasks: 1 (limit: 4915)
     Memory: 1.2M
        CPU: 50ms
     CGroup: /system.slice/mosquitto.service
             └─1234 /usr/sbin/mosquitto -c /etc/mosquitto/mosquitto.conf

Jan 01 12:00:00 ubuntu systemd[1]: Starting Mosquitto MQTT Broker...
Jan 01 12:00:00 ubuntu systemd[1]: Started Mosquitto MQTT Broker.
```

If it shows "active (running)" as shown above, the broker setup is complete.

### 5. Python Libraries

```bash
pip install requests
pip install paho-mqtt
```

## Run the Four Sensor Simulators (temperature, humidity, CO₂, soil) Using the Coordinator Script

### 1. Move to the directory where you want to clone on Ubuntu:

```bash
cd tinyIoT/demo/smart_farm_1    # example path

```


### 2. Run the coordinator script

```bash
python3 coordinator.py

```
