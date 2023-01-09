# TinyIoT_Zeroconf

  Server is a demo version.

  Test whether the server can perform the Zeroconf registration.
  
  App and Device provide only simple request and response.
  
  ## Hard-Coding
  URL, File Path, Authentication key, Authentication header name.
  
  Make sure to modify it according to the environment.
    
  
  
  
# TinyIoT_Server

  ## Download packages for compiling avahi
  
  ```
  sudo apt-get install build-essential pkg-config libglib2.0-dev libevent-dev qtbase5-dev libgtk-3-dev libgdbm-dev libdaemon-dev python-gi-dev mono-mcs monodoc-http libtool libnss-mdns libcurl4-gnutls-dev
  ```
  
  
  ## Compile and install
  
  ```
  cd avahi-0.8
  ```
  
  
  ```
  ./configure
  ```
  
  
  ```
  make
  ```
  
  
  ```
  make install
  ```
  
  
  ## Start avahi-daemon
  ```
  service avahi-daemon start
  ```
  
  
  ## Set conf File
  
  Please Set avahi-daemon.conf (cd /etc/avahi)
  
  use-ipv6=yes -> use-ipv6=no
  
 # TinyIoT Device
 
  ## Make Service
  
  see https://linux.die.net/man/5/avahi.service, https://wiki.archlinux.org/title/avahi
 

# TinyIoT_Zeroconf Demo Link
  https://youtu.be/gccRKW8B-BQ

