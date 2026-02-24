# tinyIoT

<div>
	<p align = "center">
	  <img src = "https://github.com/seslabSJU/tinyIoT/blob/main/images/tinyIoT_logo1.png?raw=true" width = "40%" height = "50%">
	</p>
</div>

<p align = "center"> Let's get started your oneM2M service projects by using 'oneM2M service layer platform', <b>tinyIoT</b>!🎮 </p> <br>
<p align = "center"> <img src="https://img.shields.io/badge/License-BSD%203--Clause-blue.svg"/>&nbsp 
<img src="https://img.shields.io/badge/Node-339933?&logo=nodedotjs&logoColor=white"/>&nbsp 
<img src="https://img.shields.io/badge/Linux-FCC624?&logo=linux&logoColor=white"/>&nbsp 
<img src="https://img.shields.io/badge/Ubuntu-E95420?&logo=Ubuntu&logoColor=white"/>&nbsp
<img src="https://img.shields.io/badge/C-A8B9CC?&logo=c&logoColor=white"/>&nbsp
<img src="https://img.shields.io/badge/SQLite-003B57?&logo=sqlite&logoColor=white"/>&nbsp
<img src="https://img.shields.io/badge/PostgreSQL-316192?&logo=postgresql&logoColor=white"/>&nbsp
<img src="https://img.shields.io/badge/json-000000?&logo=json&logoColor=white"/>&nbsp
<img src="https://img.shields.io/badge/MQTT-660066?&logo=mqtt&logoColor=white"/>&nbsp
<img src="https://img.shields.io/badge/HTTP-6600?&logoColor=white"/>&nbsp
<img src="https://img.shields.io/badge/Vue-4FC08D?&logo=vuedotjs&logoColor=white"/>&nbsp
</p>
<br><br>

# What is the tinyIoT?

<b>tinyIoT</b> is a ```Lightweight C-based oneM2M CSE``` for research & embedded deployments

<b>tinyIoT</b> is a secure, fast, lightweight and very flexible ```oneM2M service layer platform``` compliant with oneM2M specifications.💪

<b>tinyIoT</b> uses memory and CPU efficiently and has low resource use.

<b>tinyIoT</b> is implemented in C and uses lightweight open source components for database (SQLite, PostgreSQL), JSON parser (cJSON), http server (pico) and mqtt client (wolfMQTT)

<b>tinyIoT</b> also comes with a web-based dashboard implemented using Vue.
<br>
<p align = "center">
<img src = "https://github.com/seslabSJU/tinyIoT/blob/main/images/tinyIoT_overview.png?raw=true&v=2" >
</p>
<br><br>

# Why tinyIoT?

### For Seamless Installation
tinyIoT emphasizes simplicity in getting started. Users can easily set up the system by cloning the repository and executing the make command, which streamlines the installation process and reduces initial setup hurdles.

### For Exceptional Portabilit
tinyIoT is designed with universal compatibility in mind:
- It is coded in C, ensuring that it does not rely on platform-specific libraries such as those found in apt repositories, allowing it to run on all Linux distributions.
- Development of a Windows version is also underway, which will extend its usability across more operating systems.

### For Rapid Performance
tinyIoT utilizes the speed of C programming to offer fast performance which is crucial for systems with intensive data processing needs
- The architecture is optimized to minimize database access, enhancing performance especially in environments with limited I/O capabilities.

### For Adherence to Standards:
tinyIoT is committed to maintaining high standards in technology implementation
- It is developed in compliance with the latest oneM2M standards, ensuring that it meets global criteria for IoT communications and services.

<br><br>
 
# What features are supported in tinyIoT?
- Registration of IoT 
- Supporting Resources: cseBase, ACP, AE, CNT, CIN, SUB, GRP, CSR, cbA, acpA, aeA, cntA, cinA
- CRUD operations for supporting resources 
- CSE Types: IN-CSE, MN-CSE
- Access Control: ACP resource based privilege scanning
- Subscription and Notification: support subscription creation/verification/transfer flow
- Group management: midlist management and fopt support
- Discovery: DesiredResultIdType (DRT) supported with filter criteria 
- Announcement: Uni-directional support(BI-directional is under development)
- Protocol Bindings: HTTP, MQTT (CoAP & Websocket will be added)
- Database: SQLite and PostgreSQL   
<br><br>

# How to install and develop tinyIoT?
Configure your running environment with the config.h file. You can configure IP address, Port number, supported binding protocols, etc. 
Default IP address and port number are 127.0.0.1 and 3000. 

### Tested Operating System

  - Linux Ubuntu 22.04
  - macOS 25.0

### Install with script

1. Clone our repository!
    ```bash
	git clone --recursive https://github.com/seslabSJU/tinyIoT.git
	```
    
2. Move to the server directory
	```bash
	cd tinyIoT/source/server
	```   

3. Configuration

 	You can configure the server by editing `config.h` file directly. Open `config.h` and modify the following settings:
	
	```c
	// Server Type (IN_CSE or MN_CSE)
	#define SERVER_TYPE IN_CSE

	// Basic Server Configuration
	#define SERVER_IP "127.0.0.1"      // Server IP address
	#define SERVER_PORT "3000"         // Server port number
	#define CSE_BASE_NAME "TinyIoT"    // CSE base name
	#define CSE_BASE_RI "tinyiot"      // CSE base resource identifier
	#define CSE_BASE_SP_ID "tinyiot.example.com"  // CSE base SP ID

	// Remote CSE Configuration (for MN-CSE only)
	#if SERVER_TYPE == MN_CSE
	#define REMOTE_CSE_ID ""           // Remote CSE ID
	#define REMOTE_CSE_NAME ""         // Remote CSE name
	#define REMOTE_CSE_HOST ""         // Remote CSE host
	#define REMOTE_CSE_SP_ID ""        // Remote CSE SP ID
	#define REMOTE_CSE_PORT 0          // Remote CSE port
	#endif

	// Protocol Support
	// To enable MQTT, uncomment the following line
	// #define ENABLE_MQTT

	// To enable CoAP, uncomment the following line
	// #define ENABLE_COAP

     // Database Settings
    #define DB_SQLITE 1
    #define DB_POSTGRESQL 2

    // Select Database Type: DB_SQLITE or DB_POSTGRESQL
    #define DB_TYPE DB_SQLITE
    // #define DB_TYPE DB_POSTGRESQL


    #if DB_TYPE == DB_POSTGRESQL
    // PostgreSQL connection settings
    #define PG_HOST "localhost"
    #define PG_PORT 5432
    #define PG_USER "user"
    #define PG_PASSWORD "password"
    #define PG_DBNAME "tinydb"

    // PostgreSQL Schema Types
    #define PG_SCHEMA_VARCHAR 0  // Use VARCHAR for string fields
    #define PG_SCHEMA_TEXT 1     // Use TEXT for string fields

    // Select PostgreSQL Schema Type: PG_SCHEMA_VARCHAR or PG_SCHEMA_TEXT
    // #define PG_SCHEMA_TYPE PG_SCHEMA_VARCHAR
    #define PG_SCHEMA_TYPE PG_SCHEMA_TEXT
    #endif
    ```

      ### Essential Settings
      #### 1. Server Type Selection
      Choose your CSE type based on your deployment scenario:

      - **IN_CSE**: Independent CSE (default) - Standalone oneM2M service platform
      - **MN_CSE**: Middle Node CSE - Registers to a remote CSE

      **For IN_CSE**: No additional configuration required.

      **For MN_CSE**: You must configure the remote CSE connection details(example):
      ```c
      #define REMOTE_CSE_ID "your-remote-cse-id"
      #define REMOTE_CSE_NAME "your-remote-cse-name" 
      #define REMOTE_CSE_HOST "remote-cse-host-ip"
      #define REMOTE_CSE_SP_ID "remote-cse-sp-id"
      #define REMOTE_CSE_PORT your-remote-cse-port number
      ```

      #### 2. Database Type Selection
      Choose your database backend:

      - **SQLite**: File-based database (default) - No additional setup required
      - **PostgreSQL**: Server-based database - Requires separate installation and configuration

      #### 3. PostgreSQL Setup (Required if DB_TYPE = DB_POSTGRESQL)
   
      - Ubuntu
      ```bash
      sudo apt update
      sudo apt install libpq-dev
      sudo apt install postgresql
      sudo systemctl start postgresql
      sudo -u postgres psql
      CREATE DATABASE tinydb;
      CREATE USER tinyuser WITH PASSWORD 'tinypass';
      GRANT ALL PRIVILEGES ON DATABASE tinydb TO tinyuser;
      \q
      ```

      - macOS
      ```bash
      brew install postgresql
      # You can create it with the same information as config.h. 
      psql postgres
      createdb [dbname]
      createuser --interactive --pwprompt
      ```
	  
      ### Additional Settings
	  For security settings (ADMIN_AE_ID, default_ACOP, etc.), logging configuration, and other advanced options, please refer to the comments in `config.h` file

4. Installation of required modules
    - Ubuntu
    ```bash
	sudo apt install build-essential openssl 
	sudo apt install libssl-dev
	```
    If error occurs when install openssl
	```bash
	sudo apt update
	sudo apt-get install openssl
	```
 
   - macOS
    ```bash
    brew install openssl@3
    ```
    
5. To make an excutable tinyIoT server, simply execute the make file.       
	```bash
	make
	```
 
6. Use the following command to run tinyIoT server:

    ```bash
    ./server
    ```

7. You can configure port number and ip address as parameters, for example, 

    ```bash
    ./server -p [port] (port = 3000 by default)
    ```

### For binding protocols
tinyIoT supports HTTP and MQTT. CoAP bindings will be supported later.<br> 
In order to run MQTT, Mosquitto MQTT broker should be installed on your machine. 

|Protocol|Implementation/Tool|Description|
|--------|-------------------|-----------|
|HTTP|foxweb/pico|Server/library for HTTP|
|MQTT|Mosquitto|Message broker for MQTT protocol|
|CoAP|libcoap|Lightweight protocol for constrained devices|
|Websocket|libwebsockets|Full-duplex communication protocol over a single TCP connection |

<br>
If you would like to add a new protocol binding to tinyIoT, you can follow the steps outlined in the guide below.<br>
<a href = "https://github.com/seslabSJU/tinyIoT/blob/main/images/tinyIoT%20protocol%20binding%20guide.pdf" target="-blank"><b>protocol binding guide</b></a>
<br><br>


## Using MQTT over WebSocket

If you want to use MQTT over WebSocket, you must enable and configure the WebSocket listener in Mosquitto and update the project configuration.
1. Enable WebSocket listener in Mosquitto

	Edit the Mosquitto configuration file and add a WebSocket listener.
	Add the following lines to your config file:

    ```conf
    allow_anonymous true
    
    # MQTT over TCP
    listener 1883
    protocol mqtt

    # MQTT over WebSocket (default: 9001)
    listener 9001
    protocol websockets
    ```
    
    - mac
	```bash
    /opt/homebrew/etc/mosquitto/mosquitto.conf
    ```
    - linux
	```bash
    /etc/mosquitto/mosquitto.conf
    ```
2. Run Mosquitto with the modified configuration file
	
    Start Mosquitto using your modified config file:
    - mac
	```bash
    sudo mosquitto -c /opt/homebrew/etc/mosquitto/mosquitto.conf
    ```
    - linux
	```bash
    sudo mosquitto -c /etc/mosquitto/mosquitto.conf
    ```
    

3. Enable MQTT over WebSocket in config.h

	After completing these steps, the client will connect to Mosquitto using:
	```bash
	ws://<SERVER_IP>:9001/mqtt
	```