# tinyIoT

<div>
	<a href = "홈페이지url" target="_blank">
	<p align = "center">
	  <img src = "https://private-user-images.githubusercontent.com/136944859/313384740-901c8832-0181-42fd-8078-e3710b91e19b.png?jwt=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3MTA1NzM3NjAsIm5iZiI6MTcxMDU3MzQ2MCwicGF0aCI6Ii8xMzY5NDQ4NTkvMzEzMzg0NzQwLTkwMWM4ODMyLTAxODEtNDJmZC04MDc4LWUzNzEwYjkxZTE5Yi5wbmc_WC1BbXotQWxnb3JpdGhtPUFXUzQtSE1BQy1TSEEyNTYmWC1BbXotQ3JlZGVudGlhbD1BS0lBVkNPRFlMU0E1M1BRSzRaQSUyRjIwMjQwMzE2JTJGdXMtZWFzdC0xJTJGczMlMkZhd3M0X3JlcXVlc3QmWC1BbXotRGF0ZT0yMDI0MDMxNlQwNzE3NDBaJlgtQW16LUV4cGlyZXM9MzAwJlgtQW16LVNpZ25hdHVyZT02YjJhYjJmN2ViZGNjODRlMjRjMTQwYmYzZjRjYTkyYTQ4MTdkYWI1ZDMxMjRiNjc0MDZlOWY0Mzk5OWQwMzA4JlgtQW16LVNpZ25lZEhlYWRlcnM9aG9zdCZhY3Rvcl9pZD0wJmtleV9pZD0wJnJlcG9faWQ9MCJ9.AWt6zSWhqxd4cCu1ivSUsDiwnD0e-SrhyVIjh38c_Qw" width = "40%" height = "50%">
	</p></a>
</div>

<p align = "center"> Let's get started your oneM2M service projects by using 'oneM2M service layer platform', <a href = "홈페이지url" target="_blank"><b>TinyIoT</b></a>!🎮 </p> <br>
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

<b>tinyIoT</b> is a secure, fast, lightweight and very flexible <blockquote>oneM2M service layer platform</blockquote> compliant with oneM2M specifications.💪

<b>tinyIoT</b> uses memory  and CPU efficiently and has low resource use.

<b>tinyIoT</b> is implemented in C and uses lightweight open source components for database (SQLite), JSON parser (cJSON), http server (pico) and mqtt client (wolfMQTT)

<b>tinyIoT</b> also comes with a web-based dashboard implemented using Vue.

<br><br>

# tinyIoT supports the following features

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


