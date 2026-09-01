# tinyIoT

<div>
  <p align="center">
    <img src="https://github.com/seslabSJU/tinyIoT/blob/main/images/tinyIoT_logo1.png?raw=true" width="40%" height="50%" alt="tinyIoT logo">
  </p>
</div>

<p align="center">
  Get started with oneM2M service projects using <b>tinyIoT</b>, a lightweight oneM2M service-layer platform. 🎮
</p>

<p align="center">
  <img src="https://img.shields.io/badge/License-BSD%203--Clause-blue.svg" alt="BSD 3-Clause License">&nbsp;
  <img src="https://img.shields.io/badge/Linux-FCC624?&logo=linux&logoColor=white" alt="Linux">&nbsp;
  <img src="https://img.shields.io/badge/Ubuntu-E95420?&logo=Ubuntu&logoColor=white" alt="Ubuntu">&nbsp;
  <img src="https://img.shields.io/badge/C-A8B9CC?&logo=c&logoColor=white" alt="C">&nbsp;
  <img src="https://img.shields.io/badge/SQLite-003B57?&logo=sqlite&logoColor=white" alt="SQLite">&nbsp;
  <img src="https://img.shields.io/badge/PostgreSQL-316192?&logo=postgresql&logoColor=white" alt="PostgreSQL">&nbsp;
  <img src="https://img.shields.io/badge/JSON-000000?&logo=json&logoColor=white" alt="JSON">&nbsp;
  <img src="https://img.shields.io/badge/MQTT-660066?&logo=mqtt&logoColor=white" alt="MQTT">&nbsp;
  <img src="https://img.shields.io/badge/HTTP-6600?&logoColor=white" alt="HTTP">&nbsp;
  <img src="https://img.shields.io/badge/WebSocket-010101?&logo=socketdotio&logoColor=white" alt="WebSocket">
</p>

<br>

# What is tinyIoT?

<b>tinyIoT</b> is a lightweight, C-based oneM2M Common Services Entity (CSE) for research, prototyping, and resource-conscious deployments.

It implements oneM2M service-layer resource management and protocol handling using C and lightweight components such as SQLite, PostgreSQL, cJSON, pico/foxweb, wolfMQTT, libwebsockets, and the bundled libcoap sources.

tinyIoT supports Infrastructure Node CSE (IN-CSE) and Middle Node CSE (MN-CSE) deployments, ACP-based authorization, subscriptions and notifications, group management, discovery, CSE-to-CSE forwarding, and resource announcement.

<p align="center">
  <img src="https://github.com/seslabSJU/tinyIoT/blob/main/images/tinyIoT_overview.png?raw=true&v=2" alt="tinyIoT overview">
</p>

## oneM2M support scope

tinyIoT primarily targets the oneM2M Release 2a service-layer model. The release-version indicator is configured in `source/server/config.h`:

```c
#define CSE_RVI RVI_2a
```

The codebase contains selected behavior for later release versions, including release-version-dependent flexContainerInstance or announcement handling. This does not imply complete conformance with every feature in a later oneM2M release.

The main specifications relevant to tinyIoT are:

- TS-0001: Functional Architecture
- TS-0004: Service Layer Core Protocol
- TS-0008: CoAP Protocol Binding
- TS-0009: HTTP Protocol Binding
- TS-0010: MQTT Protocol Binding
- TS-0020: WebSocket Protocol Binding

The specifications are available from the [oneM2M published specifications](https://www.onem2m.org/technical/published-specifications) page.

# Why tinyIoT?

### Seamless installation

tinyIoT is built directly from source using `make`. SQLite can be selected for a local deployment without installing a separate database server.

### Portability

The server is written in C and uses common POSIX libraries. Its build currently targets Linux and macOS environments with GCC or a compatible compiler, pthreads, OpenSSL, pkg-config, and libwebsockets.

### Resource-conscious design

tinyIoT is intended to keep its service-layer implementation compact and suitable for research, gateways, and systems where application and database overhead should remain limited.

### Alignment with oneM2M

The resource model, request primitives, response status codes, protocol bindings, and CSE-to-CSE procedures are implemented with reference to the oneM2M specifications listed above.

# Architecture

Requests received through a protocol binding are converted into a common oneM2M primitive and passed to the central request router. The router applies authorization and dispatches the request to the appropriate resource, discovery, group, notification, forwarding, or announcement handler.

```text
HTTP / MQTT / CoAP / ebSocket
          |
          v
Protocol binding and primitive parsing
          |
          v
oneM2M request router
          |
          +---- ACP-based authorization
          |
          +---- Resource handlers
          |
          +---- Discovery and group fan-out
          |
          +---- Subscription notification
          |
          +---- CSE-to-CSE forwarding and announcement
          |
          v
SQLite or PostgreSQL

CoAP binding: under development
```

The main implementation is located in `source/server`:

```text
source/server/
├── main.c                    Server initialization and binding startup
├── onem2m.c                  Core oneM2M request routing
├── httpd.c                   HTTP binding
├── mqttClient.c              MQTT and MQTT-over-WebSocket handling
├── coap.c                    CoAP binding under development
├── filterCriteria.c          Discovery filter processing
├── rtManager.c               In-memory resource tree
├── sqlite_implement.c        SQLite backend
├── postgresql_implement.c    PostgreSQL backend
├── websocket/                WebSocket integration
├── resources/                Resource-specific handlers
├── sdt_definitions/          FlexContainer specialization profiles
└── config.h                  Server configuration
```

# Supported features

- **Registration**: AE registration and remoteCSE registration for MN-CSE deployments
- **CSE types**: Infrastructure Node CSE (IN-CSE) and Middle Node CSE (MN-CSE)
- **Resources**: cseBase, ACP, AE, CNT, CIN, SUB, FCNT, FCIN, TS, TSI, GRP, and CSR
- **Announced resources**: cbA, acpA, aeA, cntA, cinA
- **Resource operations**: Create, Retrieve, Update, and Delete subject to the lifecycle rules of each resource type
- **Access control**: ACP pv/pvs, acpi, originator and ACOP evaluation, group macp
- **Subscription and notification**: notification URI verification, subscription updates, resource and direct-child event notifications, and subscription deletion notifications
- **Group management**: mid validation, member type checking, consistency strategy (csy), nested groups, macp, and fan-OutPoint (fopt) requests
- **Discovery**: structured and unstructured identifiers, filter criteria, result limiting, level and offset processing, and applyRelativePath handling
- **Announcement**: remote announce/de-announce flows and Uni-direxctional, Bi-directional announcement
- **CSE-to-CSE operations**: remoteCSE registration, forwarding, and remote notification delivery
- **Protocol bindings**: HTTP, MQTT, WebSocket, CoAP(under development)
- **Databases**: SQLite and PostgreSQL

## Resource operation overview

| Resource | Create | Retrieve | Update | Delete | Notes |
|---|:---:|:---:|:---:|:---:|---|
| `cseBase` | — | Yes | — | — | Created and managed internally by the CSE |
| ACP, AE, CNT, SUB, GRP, CSR | Yes | Yes | Yes | Yes | Resource-specific validation is applied |
| CIN | Yes | Yes | No | Yes | Maintained as a child of a container |
| TS | Yes | Yes | Yes | Yes | Manages time-series instances and metadata |
| TSI | Yes | Yes | No | Yes | Maintained as a child of a time series |
| FCNT | Yes | Yes | Yes | Yes | Validated using loaded specialization profiles |
| FCIN | RVI-dependent | Yes | RVI-dependent | Yes | For RVI 4 and later, creation is handled internally by FCNT and direct create/update is rejected |
| Announced resources | RVI-dependent | RVI-dependent | RVI-dependent | RVI-dependent | Subject to announcement origin and synchronization rules |

## Discovery filters

The discovery implementation includes processing for commonly used criteria such as:

- creation, modification, and expiration time
- state tag and content size
- resource labels, child labels, and parent labels
- resource type, child resource type, and parent resource type
- filter operation (fo)
- result limit (lim), level (lvl), and offset (ofst)
- apply-relative-path (arp)

# Build and run tinyIoT

Server identity, database, protocol, and operational settings are configured in source/server/config.h. The default HTTP address and port are 127.0.0.1:3000.

## 1. Install build dependencies

The Makefile uses GCC, pthreads, OpenSSL, pkg-config, and libwebsockets. PostgreSQL builds additionally require libpq.

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install build-essential libssl-dev pkg-config libwebsockets-dev
```

macOS with Homebrew:

```bash
xcode-select --install
brew install openssl@3 pkg-config libwebsockets
```

## 2. Clone the repository

```bash
git clone https://github.com/seslabSJU/tinyIoT.git
cd tinyIoT/source/server
```

## 3. Configure the CSE

Open `config.h` and select the CSE type.

For an Infrastructure Node CSE:

```c
#define SERVER_TYPE IN_CSE
```

For a Middle Node CSE:

```c
#define SERVER_TYPE MN_CSE

#define REMOTE_CSE_ID "/id-in"
#define REMOTE_CSE_NAME "registrar-cse"
#define REMOTE_CSE_HOST "127.0.0.1"
#define REMOTE_CSE_SP_ID "example.com"
#define REMOTE_CSE_PORT 3000
```

An MN-CSE registers with its configured Registrar CSE during startup.

Basic server settings:

```c
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT "3000"
#define CSE_BASE_NAME "TinyIoT"
#define CSE_BASE_RI "tinyiot"
#define CSE_BASE_SP_ID "tinyiot.example.com"
#define CSE_RVI RVI_3
```

## 4. Select a database

### SQLite

For the simplest local setup, select SQLite:

```c
#define DB_TYPE DB_SQLITE
// #define DB_TYPE DB_POSTGRESQL
```

SQLite is compiled from the source included in the repository and does not require a separate database server.

### PostgreSQL

Install PostgreSQL and libpq in addition to the common prerequisites.

Ubuntu/Debian:

```bash
sudo apt install postgresql libpq-dev
sudo systemctl start postgresql
```

macOS:

```bash
brew install postgresql libpq
brew services start postgresql
```

Create the database and its owner:

```sql
CREATE USER tinyuser WITH PASSWORD 'tinypass';
CREATE DATABASE tinydb OWNER tinyuser;
```

Configure `config.h`:

```c
// #define DB_TYPE DB_SQLITE
#define DB_TYPE DB_POSTGRESQL

#if DB_TYPE == DB_POSTGRESQL
#define PG_HOST "localhost"
#define PG_PORT 5432
#define PG_USER "tinyuser"
#define PG_PASSWORD "tinypass"
#define PG_DBNAME "tinydb"

#define PG_SCHEMA_VARCHAR 0
#define PG_SCHEMA_TEXT 1
#define PG_SCHEMA_TYPE PG_SCHEMA_TEXT
#endif
```

## 5. Build

```bash
make
```

Run `make clean && make` after changing the database backend or a conditional protocol option.

## 6. Run

```bash
./server
```

To override the HTTP port:

```bash
./server -p 3000
```

Other server identity and network settings are configured in `config.h`.

## Additional configuration

The following settings are also available in `config.h`:

| Setting | Purpose |
|---|---|
| `ADMIN_AE_ID` | Privileged administrative originator |
| `DEFAULT_ACOP` | Default access-control operation mask |
| `ALLOWED_REMOTE_CSE_ID` | Allowed remote CSE identifiers |
| `MONO_THREAD` | Mono-thread or multi-thread request processing |
| `DEFAULT_MAX_NR_INSTANCES` | Default container instance limit |
| `DEFAULT_MAX_BYTE_SIZE` | Default container byte-size limit |
| `DEFAULT_DISCOVERY_LIMIT` | Default discovery result count limit |
| `LOG_LEVEL` | Server log verbosity |

# Protocol bindings

| Protocol | Implementation/tool | Description |
|---|---|---|
| HTTP | foxweb/pico | Main HTTP request and response binding |
| MQTT | wolfMQTT and Mosquitto | oneM2M MQTT binding; requires an MQTT broker |
| WebSocket | libwebsockets | WebSocket transport integration |
| MQTT over WebSocket | wolfMQTT and Mosquitto | MQTT transport through a Mosquitto WebSocket listener |
| CoAP | bundled libcoap | Under development |

<br>
If you would like to add a new protocol binding to tinyIoT, you can follow the steps outlined in the guide below.<br>
<a href = "https://github.com/seslabSJU/tinyIoT/blob/main/images/tinyIoT%20protocol%20binding%20guide.pdf" target="-blank"><b>protocol binding guide</b></a>
<br><br>

## Using MQTT

Install and start a compatible MQTT broker such as Mosquitto, then enable MQTT in `config.h`:

```c
#define ENABLE_MQTT

#define MQTT_HOST "127.0.0.1"
#define MQTT_QOS MQTT_QOS_0
#define MQTT_CLIENT_ID "TinyIoT"
#define MQTT_USERNAME "your-mqtt-user"
#define MQTT_PASSWORD "your-mqtt-password"
```

MQTT/TLS can be enabled separately by defining `ENABLE_MQTT_TLS` and configuring the broker and client for its TLS listener.

## Using MQTT over WebSocket

To use MQTT over WebSocket, enable MQTT and its WebSocket transport in tinyIoT and configure a WebSocket listener in Mosquitto.

### 1. Configure Mosquitto

Add the following listeners to `mosquitto.conf`:

```conf
# Local development example. Configure authentication as appropriate
# before exposing the broker to other hosts.
allow_anonymous true

# MQTT over TCP
listener 1883
protocol mqtt

# MQTT over WebSocket
listener 9001
protocol websockets
```

Common Mosquitto configuration paths:

- macOS with Homebrew: `/opt/homebrew/etc/mosquitto/mosquitto.conf`
- Linux: `/etc/mosquitto/mosquitto.conf`

### 2. Start Mosquitto

macOS with Homebrew:

```bash
mosquitto -c /opt/homebrew/etc/mosquitto/mosquitto.conf
```

Linux:

```bash
sudo mosquitto -c /etc/mosquitto/mosquitto.conf
```

### 3. Enable MQTT over WebSocket in tinyIoT

In `config.h`:

```c
#define ENABLE_MQTT
#define ENABLE_MQTT_WEBSOCKET
#define MQTT_OVER_WS_PORT 9001
```

The MQTT client connects to the broker through:

```text
ws://<MQTT_HOST>:9001/mqtt
```

# FlexContainer specialization profiles

tinyIoT loads FCP definitions from `source/server/sdt_definitions` during startup. The repository includes profiles for several oneM2M SDT domains.

Custom SDT XML files can be placed in `source/server/sdt_sources` and converted with:

```bash
make sdt
```

The conversion target uses the official oneM2M SDTTool. Before adding custom XML sources, configure `SDT_PYTHON` in the Makefile so that it points to a Python environment containing the tool.

# License

tinyIoT is distributed under the [BSD 3-Clause License](LICENSE).
