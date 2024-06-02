# tinyIoT

oneM2M is a global partnership project founded in 2012 to create a global technical standard for interoperability concerning the architecture, API specifications, security and enrolments solutions for Internet of Things (IoT) technologies based on requirements contributed by its members. The standardised specifications produced by oneM2M enable an Eco-System to support a wide range of applications and services such as smart cities, smart grid, connected car, home automation, public safety, and health.

tinyIoT is a secure, fast, lightweight and very flexible oneM2M service layer platform compliant with oneM2M specifications. tinyIoT uses memory and CPU efficiently and has low resource use. tinyIoT is implemented in C and uses lightweight open source components for database (SQLite), JSON parser (cJSON), http server (pico) and mqtt client (wolfMQTT) . tinyIoT also comes with a web-based dashboard implemented using Vue. 

tinyIoT supports the following features: 

- Registration of IoT 
- AcessControlPolicy (ACP), Application Entity (AE), Container (CNT), contentInstance (CIN) resources
- CRUD operations
- Subscription and Notification
- Discovery
- Bindings: HTTP and MQTT (CoAP will be added in 2024 first quarter)
- Group management 

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
  ```
  ./autogen.sh
  ./configure
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
   

For binding protocols, tinyIoT supports HTTP and MQTT. CoAP bindings will be supported later. 
In order to run MQTT, Mosquitto MQTT broker should be installed on your machine. 

  - HTTP: foxweb/pico
  - MQTT: Mosquitto
  - CoAP: To be added
