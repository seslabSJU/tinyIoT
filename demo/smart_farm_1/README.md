# Coordinator Script for tinyIoT and device simulator

## Overview
This project provides:
- **Device simulators** for sending arbitrary temperature and humidity data to a tinyIoT server  
- A **coordinator script** to control and manage all processes  

The simulators follow the **oneM2M standard** and support both **HTTP** and **MQTT** protocols for data transmission.  

## File Structure
- `coordinator.py` – Main script to control the tinyIoT server and simulators  
- `simulator.py` – Temperature & Humidity sensor simulator (It is possible to add sensors to suit the user’s situation.)
- `config_coord.py`- Global configuration values (coordiantion.py)    
- `config_sim.py` – Global configuration values (simulator.py)  
- `test_data_temp.csv` – Temperature CSV data  
- `test_data_humid.csv` – Humidity CSV data  

## Features
- **Integrated control** with `coordination.py`  
- **Temperature and humidity simulation**
- **Protocols**: Choose between **HTTP** or **MQTT**    
- **Modes**  
  - `csv`: Sends data from CSV files in sequence  
  - `random`: Generates random data within configured ranges  
- **Automatic registration**: Create oneM2M AE/CNT resources with `--registration` option  

## Environment
- **OS**: Ubuntu 24.04.2 LTS  
- **Language**: Python 3.8+  
- **IoT**: tinyIoT
- **MQTT Broker**: Mosquitto  

## Installation


### 1. tinyIoT Setup 

- tinyIoT config.h settings
1. Verify that the following settings are correctly configured (Configure the IP and port as appropriate for your setup):


```c
// #define NIC_NAME "eth0"
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT "3000"
#define CSE_BASE_NAME "TinyIoT"
#define CSE_BASE_RI "tinyiot"
#define CSE_BASE_SP_ID "tinyiot.example.com"
#define CSE_RVI RVI_2a
```


2. Uncomment #define ENABLE_MQTT.

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

### 2. simulator Setup

Go to the relevant URL and set up the environment according to the README on the simulator’s GitHub.


## Run the Device Simulator Using the Coordinator Script

1. Navigate to the directory where you want to clone in Ubuntu:
```bash
cd tinyIoT/script   #example path
```

2. Setting
  2.1 In config_coord.py, for images other than Ubuntu, set the path to simulator.py to match the user's environment.

```python
# ----------------------- Paths -----------------------
# Absolute or relative path to the tinyIoT server binary
SERVER_EXEC = "/home/parks/tinyIoT/source/server/server"

# Absolute or relative path to the simulator entrypoint
# e.g., "simulator.py" or "/opt/iot-sim/bin/simulator.py"
SIMULATOR_PATH = "/home/parks/tinyIoT/simulator/simulator.py"
```


  2.2 Modify the contents of config_coord.py to suit your needs

```python
"""Configuration values for the coordination launcher and child device simulators."""

# ----------------------- Paths -----------------------
# Absolute or relative path to the tinyIoT server binary
SERVER_EXEC = "/home/parks/tinyIoT/source/server/server"

# ------------------- Health Check URL -------------------
# OneM2M CSE HTTP endpoint used for server readiness checks.
# NOTE: CSI is lowercase ("tinyiot"). Change only if your CSE path differs.
CSE_URL = "http://127.0.0.1:3000/tinyiot"
```

  

3. option setting in coordinator.py
- Users can add or remove sensors here to suit their setup.

```python
# Coordinator options. Users can add or remove sensors here to suit their setup.
# Sensor names must not contain spaces.
SENSORS_TO_RUN: List[SensorConfig] = [
    SensorConfig('temp',  protocol='http', mode='random', frequency=3, registration=1),
    SensorConfig('humid', protocol='http', mode='random', frequency=3, registration=1),
    SensorConfig('co2',   protocol='http', mode='random', frequency=3, registration=1),
    SensorConfig('soil',  protocol='http', mode='random', frequency=3, registration=1)
]
```


4. Set the options in `coordinator.py`.  
More detailed option settings can be configured in `config_coord.py`.
For more detailed descriptions of the options, refer to the man page.
- protocol : HTTP or MQTT

- mode : csv or random

- frequency : Data transmission interval (seconds)

- registration : 0 or 1 (Enable AE/CNT auto-registration)


5. Enter the cloned directory

```bash
cd tinyIoT/demo/smart_farm_1/  
```

6. Run Coordinator Script

```bash
python3 coordinator.py
```
