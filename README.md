# tinyIoT
<p align = "center">
  <img src = "https://private-user-images.githubusercontent.com/136944859/313384740-901c8832-0181-42fd-8078-e3710b91e19b.png?jwt=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3MTA1NzM3NjAsIm5iZiI6MTcxMDU3MzQ2MCwicGF0aCI6Ii8xMzY5NDQ4NTkvMzEzMzg0NzQwLTkwMWM4ODMyLTAxODEtNDJmZC04MDc4LWUzNzEwYjkxZTE5Yi5wbmc_WC1BbXotQWxnb3JpdGhtPUFXUzQtSE1BQy1TSEEyNTYmWC1BbXotQ3JlZGVudGlhbD1BS0lBVkNPRFlMU0E1M1BRSzRaQSUyRjIwMjQwMzE2JTJGdXMtZWFzdC0xJTJGczMlMkZhd3M0X3JlcXVlc3QmWC1BbXotRGF0ZT0yMDI0MDMxNlQwNzE3NDBaJlgtQW16LUV4cGlyZXM9MzAwJlgtQW16LVNpZ25hdHVyZT02YjJhYjJmN2ViZGNjODRlMjRjMTQwYmYzZjRjYTkyYTQ4MTdkYWI1ZDMxMjRiNjc0MDZlOWY0Mzk5OWQwMzA4JlgtQW16LVNpZ25lZEhlYWRlcnM9aG9zdCZhY3Rvcl9pZD0wJmtleV9pZD0wJnJlcG9faWQ9MCJ9.AWt6zSWhqxd4cCu1ivSUsDiwnD0e-SrhyVIjh38c_Qw" width = "90%" height = "90%">
</p>

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


Tested Operating System: 
  - Linux Ubuntu 22.04

Configure your running environment with the config.h file. You can configure IP address, Port number, supported binding protocols, etc. 
Default IP address and port number are 127.0.0.1 and 3000. 

To make an excutable tinyIoT server, simply execute the make file. 

  $ make

Use the following command to run tinyIoT server: 

  $ ./server 
 
You can configure port number and ip address as parameters, for example, 

  $ ./server -p [port] (port = 3000 by default)
   

For binding protocols, tinyIoT supports HTTP and MQTT. CoAP bindings will be supported later. 
In order to run MQTT, Mosquitto MQTT broker should be installed on your machine. 

  - HTTP: foxweb/pico
  - MQTT: Mosquitto
  - CoAP: To be added


