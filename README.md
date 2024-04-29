# tinyIoT

<div>
	<a href = "홈페이지url" target="_blank">
	<p align = "center">
	  <img src = "https://github.com/seslabSJU/tinyIoT/blob/main/images/tinyIoT_logo1.png?raw=true" width = "40%" height = "50%">
	</p></a>
</div>

<p align = "center"> Let's get started your oneM2M service projects by using 'oneM2M service layer platform', <a href = "홈페이지url" target="_blank"><b>tinyIoT</b></a>!🎮 </p> <br>
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
- Bindings: HTTP and MQTT (CoAP will be added in 2024 first quarter)
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
    
2. Please enter the pico path of the file       
	```
	cd pico
	```   
        
3. To make an excutable tinyIoT server, simply execute the make file.       
	```
	$ make
	```   
        
 4. Use the following command to run tinyIoT server:
	```	
	./server
	```
 
 5. You can configure port number and ip address as parameters, for example, 
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
|CoAP|To be added|Future CoAP bindings|

<br>

# Documentation

We are managing our development documentation through GitHub files.📂<br>
For more information, please see below.<br> 
You can view our development process and documentation within these files!👀

<a href = "https://github.com/seslabSJU/tinyIoT/tree/main/documents" target="_blank"><b>documentation</b></a>
