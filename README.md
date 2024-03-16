# tinyIoT
<img src = "https://private-user-images.githubusercontent.com/136944859/313384460-c95f8f03-e7ff-48aa-bd31-bdb51e05af3d.png?jwt=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3MTA1NzMyODIsIm5iZiI6MTcxMDU3Mjk4MiwicGF0aCI6Ii8xMzY5NDQ4NTkvMzEzMzg0NDYwLWM5NWY4ZjAzLWU3ZmYtNDhhYS1iZDMxLWJkYjUxZTA1YWYzZC5wbmc_WC1BbXotQWxnb3JpdGhtPUFXUzQtSE1BQy1TSEEyNTYmWC1BbXotQ3JlZGVudGlhbD1BS0lBVkNPRFlMU0E1M1BRSzRaQSUyRjIwMjQwMzE2JTJGdXMtZWFzdC0xJTJGczMlMkZhd3M0X3JlcXVlc3QmWC1BbXotRGF0ZT0yMDI0MDMxNlQwNzA5NDJaJlgtQW16LUV4cGlyZXM9MzAwJlgtQW16LVNpZ25hdHVyZT0yY2QxODQ0N2ZjZWQ1ZWQxYzQ4ZTU2MTEyYzQwNTE4M2ZjNTA1ZTE5ZDg0Zjg3ZDA5MjgwMTk4YTFkOGE1MWYwJlgtQW16LVNpZ25lZEhlYWRlcnM9aG9zdCZhY3Rvcl9pZD0wJmtleV9pZD0wJnJlcG9faWQ9MCJ9.XHndEy8VUx2JmF0fzNwc411dl7TsK5veGORYkfzutJk" width = "50%" height="50%"></img>

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


