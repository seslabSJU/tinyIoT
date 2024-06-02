#!/bin/sh

# Check for autoconf
which autoconf > /dev/null || {
    echo "You need to install autoconf"
    exit 1
}

# Generate configuration scripts
aclocal     # Set up an m4 environment
autoconf    # Generate configure from configure.ac
# automake --add-missing

read -p "Enter server type [mn/in]: " server_type

read -p "Enter server IP (default: lo)): " server_ip
read -p "Enter server port (default: 3000): " server_port
read -p "Enter CSE base name (default: TinyIoT): " cse_base_name
read -p "Enter CSE base RI (default: tinyiot): " cse_base_ri

if [ "$server_type" = "mn" ]; then
    read -p "Enter remote CSE ID: " remote_cse_id
    read -p "Enter remote CSE name: " remote_cse_name
    read -p "Enter remote CSE host: " remote_cse_host
    read -p "Enter remote CSE port: " remote_cse_port
fi

read -p "Do you want to enable MQTT support? [yes/no] (default: no): " mqtt
read -p "Do you want to enable COAP support? [yes/no] (default: no): " coap
if [ "$coap" = "yes" ]; then
    read -p "Do you want to enable COAPS support? [yes/no] (default: no): " coaps
fi
echo

server_ip=${server_ip:-127.0.0.1}
server_port=${server_port:-3000}
cse_base_name=${cse_base_name:-TinyIoT}
cse_base_ri=${cse_base_ri:-tinyiot}

set -e

# Run configure with user input
./configure \
  --with-server-type=$server_type \
  --enable-mqtt=$mqtt \
  --enable-coap=$coap \
  --enable-coaps=$coaps \
  --with-remote-cse-id=$remote_cse_id \
  --with-remote-cse-name=$remote_cse_name \
  --with-remote-cse-host=$remote_cse_host \
  --with-remote-cse-port=$remote_cse_port \
  --with-server-ip=$server_ip \
  --with-server-port=$server_port \
  --with-cse-base-name=$cse_base_name \
  --with-cse-base-ri=$cse_base_ri

echo
echo "Configuration is done. You can now run 'make'"
echo "The configuration has generated 'config.h' and 'Makefile'. You can modify these files if necessary."