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
  3.1 In config_coord.py, for images other than Ubuntu, set the path to simulator.py to match the user’s environment.
   <img width="759" height="212" alt="image" src="https://github.com/user-attachments/assets/e635e4ca-203e-4902-a43b-ac1fd094a273" />

   3.2 Modify the contents of config_coord.py shown in the image below to suit your needs
  <img width="681" height="100" alt="image" src="https://github.com/user-attachments/assets/817704a5-d6ce-45de-9e15-e155f9d4f32b" />
  <img width="907" height="121" alt="image" src="https://github.com/user-attachments/assets/bf000381-91d1-47bb-af20-2f15d8dbe9e2" />

3. option setting in coordinator.py
- Users can add or remove sensors here to suit their setup.
<img width="1029" height="249" alt="image" src="https://github.com/user-attachments/assets/1e821675-30fe-4b6c-9a57-e1947088ae05" />


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
