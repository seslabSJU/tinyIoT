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
	
 	You can set up configuration through `./autogen.sh` that makes `config.h` and `Makefile`.
	This will ask you the following and it will be automatically forwarded as an option through `./configure`.

	```
 	./autogen.sh 
	Enter server type [mn/in]: in
	Enter server IP (default: lo)): 127.0.0.1
	Enter server port (default: 3000): 3000
	Enter CSE base name (default: TinyIoT): TinyIoT
	Enter CSE base RI (default: tinyiot): tinyiot
	Do you want to enable MQTT support? [yes/no] (default: no): no
	Do you want to enable COAP support? [yes/no] (default: no): no
 	```
 
	- You can input CSE information:
		- `--with-server-type select` selects the server type.
		- `--with-server-ip` sets the IP address of the server.
		- `--with-server-port` sets the port of the server
		- `--with-cse-base-name` sets the base name of the CSE.
		- `--with-cse-base-ri` sets the base resource identifier of the CSE.
	- If tinyIoT is MN-CSE, you can input RemoteCSE information:
		- `--with-remote-cse-id` sets the ID of the remote CSE.
		- `--with-remote-cse-name` sets the name of the remote CSE.
		- `--with-remote-cse-host` sets the host of the remote CSE.
		- `--with-remote-cse-port` sets the port of the remote CSE.
	- For binding protocols, tinyIoT supports HTTP basically. You can choose more protocols:
		- `--enable-mqtt` enables the MQTT protocol.
		- `--enable-coap` enables the CoAP protocol.
		- `--enable-coaps` enables the secure CoAP protocol.


 5. To make an excutable tinyIoT server, simply execute the make file.       
	```
	$ make
	```   
        
 6. Use the following command to run tinyIoT server:
	```	
	./server
	```
 
 7. You can configure port number and ip address as parameters, for example, 
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

