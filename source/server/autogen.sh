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
read -p "Do you want to enable MQTT support?  [yes/no]: " mqtt
read -p "Do you want to enable COAP support?  [yes/no]: " coap
if [ "$coap" = "yes" ]; then
    read -p "Do you want to enable COAPS support? [yes/no]: " coaps
fi
echo

set -e

# Run configure with user input
./configure --with-server-type=$server_type --enable-mqtt=$mqtt --enable-coap=$coap --enable-coaps=$coaps

echo
echo "Configuration is done. You can now run 'make'"