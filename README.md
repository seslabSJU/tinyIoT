# tinyIoT

oneM2M is a global partnership project founded in 2012 to create a global technical standard for interoperability concerning the architecture, API specifications, security and enrolments solutions for Internet of Things (IoT) technologies based on requirements contributed by its members. The standardised specifications produced by oneM2M enable an Eco-System to support a wide range of applications and services such as smart cities, smart grid, connected car, home automation, public safety, and health.

tinyIoT is a secure, fast, lightweight and very flexible oneM2M service layer platform compliant with oneM2M specifications. tinyIoT uses memory and CPU efficiently and has low resource use. tinyIoT is implemented in C and uses lightweight open source components for database (Berkeley DB), JSON parser (cJSON), and http server (lightHTTP). tinyIoT also comes with a web-based dashboard implemented using Vue. 

tinyIoT supports the following features: 

- Registration of IoT 
- Application Entity (AE), Container (CNT), contentInstance (CIN) resources
- CRUD operations
- Subscription and Notification
- Discovery
- Bindings: HTTP and MQTT (CoAP will be added in 2023 first quarter)
- Group management (to be added in 2023 first quarter)


Tested Operating System: 
  - Linux Ubuntu 22.04

Configure your running environment with the config.h file. You can configure IP address, Port number, supported binding protocols, etc. 
Default IP address and port number are 127.0.0.1 and 3000. 

Use the following command to run tinyIoT server: 

  Run ./server 
 
You can configure port number and ip address as parameters, for example, 

  ./server [port] (port = 3000 by default)
   
To make an excutable tinyIoT server, simply execute the make file. 

  make
  
Dependencies: 

  - Database: BerkeleyDB ==> Berkeley DB 18.1.32 → https://mindlestick.github.io/TinyIoT/build/html/installation.html (guide) Or you can read README_DB.md
  - JSON parser: CJSon

For binding protocols, tinyIoT supports HTTP and MQTT. CoAP bindings will be supported later. 
In order to run MQTT, Mosquitto MQTT broker should be installed on your machine. 

  - HTTP: lighthttpd
  - MQTT: Mosquitto
  - CoAP: To be added


