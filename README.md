# tinyIoT

<div>
	<p align = "center">
	  <img src = "https://github.com/seslabSJU/tinyIoT/blob/main/images/tinyIoT_logo1.png?raw=true" width = "40%" height = "50%">
	</p>
</div>

<p align = "center"> Let's get started your oneM2M service projects by using 'oneM2M service layer platform', <b>tinyIoT</b>!🎮 </p> <br>
<p align = "center"> <img src="https://img.shields.io/badge/Node-339933?style=flat-square&logo=nodedotjs&logoColor=white"/>&nbsp 
<img src="https://img.shields.io/badge/Linux-FCC624?style=flat-square&logo=linux&logoColor=white"/>&nbsp 
<img src="https://img.shields.io/badge/Ubuntu-E95420?style=falt-square&logo=ubuntu&logoColors=white"/>&nbsp
<img src="https://img.shields.io/badge/C-A8B9CC?style=flat-square&logo=c&logoColor=white"/>&nbsp
<img src="https://img.shields.io/badge/SQLite-003B57?style=flat-square&logo=sqlite&logoColor=white"/>&nbsp
<img src="https://img.shields.io/badge/json-000000?style=flat-square&logo=json&logoColor=white"/>&nbsp
<img src="https://img.shields.io/badge/MQTT-660066?style=flat-square&logo=mqtt&logoColor=white"/>&nbsp
<img src="https://img.shields.io/badge/Vue-4FC08D?style=flat-square&logo=vuedotjs&logoColor=white"/>&nbsp
</p>
<br><br>


# What is the tinyIoT?

<b>tinyIoT</b> is a secure, fast, lightweight and very flexible ```oneM2M service layer platform``` compliant with oneM2M specifications.💪

<b>tinyIoT</b> uses memory  and CPU efficiently and has low resource use.

<b>tinyIoT</b> is implemented in C and uses lightweight open source components for database (SQLite), JSON parser (cJSON), http server (pico) and mqtt client (wolfMQTT)

<b>tinyIoT</b> also comes with a web-based dashboard implemented using Vue.
<br>
<p align = "center">
<img src = "https://github.com/seslabSJU/tinyIoT/blob/main/images/tinyIoT_overview.png?raw=true" >
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

 <br>
 
# tinyIoT supports the following features

- Registration of IoT 
- AcessControlPolicy (ACP), Application Entity (AE), Container (CNT), contentInstance (CIN) resources
- CRUD operations
- Subscription and Notification
- Discovery
- Bindings: HTTP, MQTT (CoAP & Websocket will be added)
- Group management 
<br>

# Installation & Development Environment
Configure your running environment with the config.h file. You can configure IP address, Port number, supported binding protocols, etc. 
Default IP address and port number are 127.0.0.1 and 3000. 

### Tested Operating System
  - Linux Ubuntu 22.04

### Install with script
1. Clone our repository!
	```
	git clone --recursive https://github.com/seslabSJU/tinyIoT.git
	```
    
2. Move to the server directory      
	```
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

	For more detailed configuration options, please refer to the comments in `config.h` file.

    If you want to use PostgreSQL, follow the settings below.

    ```bash
    sudo apt update
    sudo apt install libpq-dev
    sudo apt install postgresql
    sudo systemctl start postgresql
    # You can create it with the same information as config.h. 
    psql postgres
    createdb [dbname]
    createuser --interactive --pwprompt
    ```


 4. To make an excutable tinyIoT server, simply execute the make file.       
	```
	$ make
	```   
        
 5. Use the following command to run tinyIoT server:
	```	
	./server
	```
 
 6. You can configure port number and ip address as parameters, for example, 
	```
	$ ./server -p [port] (port = 3000 by default)
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

