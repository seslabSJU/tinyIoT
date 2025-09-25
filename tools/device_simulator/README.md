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

<img width="684" height="241" alt="image" src="https://github.com/user-attachments/assets/705a3ac5-4dec-4bbc-b35a-976ae12d600b" />

2. Uncomment `#define ENABLE_MQTT`.

<img width="641" height="397" alt="image" src="https://github.com/user-attachments/assets/6b856bbc-0dc7-46b9-bcd9-9a606407592f" />

### MQTT Broker (Mosquitto) Setup

- **Mosquitto installation required**
1. Visit the website
    
    [https://mosquitto.org](https://mosquitto.org/)
    
2. Click **Download** on the site


3. Edit `mosquitto.conf`:
- Add `#listener 1883`, `#protocol mqtt`

<img width="2043" height="282" alt="image" src="https://github.com/user-attachments/assets/6c97477f-eb28-4d64-b71a-f249ff1336cb" />

4. Run

```bash
sudo systemctl start mosquitto
sudo systemctl status mosquitto
```

<img width="2162" height="632" alt="image" src="https://github.com/user-attachments/assets/bad124a0-6891-43fe-8ed4-8a9bef79c4ea" />

If it shows “active (running)” as in the image above, the broker setup is complete.

## Running the Device Simulator

1. Setting
- Modify the contents of config_sim.py shown in the image below to suit your needs.
<img width="675" height="157" alt="image" src="https://github.com/user-attachments/assets/24d65372-75af-45b0-82e2-5ecf8468ba6a" /> 
<img width="665" height="463" alt="image" src="https://github.com/user-attachments/assets/79af566b-066e-4f91-8c09-b2b434e60415" />
<img width="827" height="777" alt="image" src="https://github.com/user-attachments/assets/2af7587c-e3d7-4402-a77f-d72329e34e32" />


2. Move to the device_simulator directory

```bash
cd tinyIoT/tools/device_simulator   
```

3. Run

```bash
python3 simulator.py --sensor {sensor} --protocol {http / mqtt} --mode {csv / random} --frequency {seconds} --registration {1 / 0}
```

- For detailed explanations of each option, please refer to the Features section above.
