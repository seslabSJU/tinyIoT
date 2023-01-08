# tinyIoT - very light oneM2M server

It is a very light oneM2M server using C

## Operating System

Linux Ubuntu 22.04

## Quick start

Run `./server` or `./server [port]` (port = 3000 by default).
If there is no executable file, Run `make`

## make Denpendency

1. Berkeley DB 18.1.32 → https://mindlestick.github.io/TinyIoT/build/html/installation.html (guide)

## Add or Remove Response Header

1. open `httpd.h`
2. modify → `#define RESPONSE_JSON_HEADER`
3. header foramt → `Connection : close\n`
