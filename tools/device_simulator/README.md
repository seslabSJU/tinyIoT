# Device_simulator_for_tinyIoT


## Overview

This project provides:

- A **device simulator** that sends random temperature and humidity data to the tinyIoT server
- You can freely add any sensors you want.

The simulator follows the **oneM2M standard** and supports both **HTTP** and **MQTT** protocols for data transmission.

## File Structure

- `simulator.py` – Device simulator
- `config_sim.py` – Global configuration values
- `test data/test_data_temp.csv` – Temperature CSV data
- `test data/test_data_humid.csv` – Humidity CSV data

## Features

- You can also use simulator.py via a script called coordination.py, which controls tinyIoT and the simulator.

- **Single sensor device simulation**
- **Sensor**: Users can configure and use any sensor they want.
- **Protocol**: Choose between **HTTP** or **MQTT**
- **Mode**
    - `csv`: Sequentially sends data from a CSV file
    - `random`: Generates random data within the configured range
- **Frequency**: Set the transmission interval in seconds for data sent to tinyIoT
- **Auto registration**: Create oneM2M AE/CNT resources using the `Registration` option

## Environment

- **OS**: Ubuntu 24.04.2 LTS
- **Language**: Python 3.8+
- **IoT**: tinyIoT
- **MQTT Broker**: Mosquitto

## Installation

### Python Libraries

```bash
pip install requests
pip install paho-mqtt
```

### tinyIoT Setup

- tinyIoT `config.h` settings
1. Make sure the following settings are correctly configured (Configure the IP and port as appropriate for your setup):

```c
// #define NIC_NAME "eth0"
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT "3000"
#define CSE_BASE_NAME "TinyIoT"
#define CSE_BASE_RI "tinyiot"
#define CSE_BASE_SP_ID "tinyiot.example.com"
#define CSE_RVI RVI_2a
```


2. Uncomment `#define ENABLE_MQTT`.

```c
// To enable MQTT, de-comment the following line
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

### MQTT Broker (Mosquitto) Setup

- **Mosquitto installation required**
1. Visit the website
    
    [https://mosquitto.org](https://mosquitto.org/)
    
2. Click **Download** on the site


3. Edit `mosquitto.conf`:
- Add `#listener 1883`, `#protocol mqtt`

```conf
# listener port-number [ip address/host name/unix socket path]
# listener 1883
# protocol mqtt
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

## Running the Device Simulator

1. Setting
- Modify the contents of config_sim.py to suit your needs.

```python
"""Configuration for the oneM2M device simulator"""

# HTTP (REST) endpoint configuration
HTTP_HOST = "127.0.0.1"
HTTP_PORT = 3000
HTTP_BASE = f"http://{HTTP_HOST}:{HTTP_PORT}"
BASE_URL_RN = f"{HTTP_BASE}/{CSE_RN}"

# Baseline HTTP headers used for registration requests
HTTP_DEFAULT_HEADERS = {
    "Accept": "application/json",
    "X-M2M-Origin": "CAdmin",
    "X-M2M-RVI": "2a",
    "X-M2M-RI": "req",
    "Content-Type": "application/json",
}

# Headers for tree inspection calls (no payload type)
HTTP_GET_HEADERS = {
    "Accept": "application/json",
    "X-M2M-Origin": "CAdmin",
    "X-M2M-RVI": "2a",
    "X-M2M-RI": "check",
}

# Per-sensor resource registration metadata used by build_sensor_meta
SENSOR_RESOURCES = {
    "temp": {
        "ae": "CtempSensor",
        "cnt": "temperature",
        "api": "N.temp",
        "origin": "CtempSensor",
    },
    "humid": {
        "ae": "ChumidSensor",
        "cnt": "humidity",
        "api": "N.humid",
        "origin": "ChumidSensor",
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


2. Move to the device_simulator directory

```bash
cd tinyIoT/tools/device_simulator   
```

3. Run

```bash
python3 simulator.py --sensor {sensor} --protocol {http / mqtt} --mode {csv / random} --frequency {seconds} --registration {1 / 0}
```

- For detailed explanations of each option, please refer to the Features section above.
