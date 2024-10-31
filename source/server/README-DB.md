
BerkeleyDB Installation

- Installation environment: Ubuntu 22.04 LTS , WSL2

The BerkeleyDB version we used : db-18.1.32


Download Berkeley DB

libdb (https://github.com/berkeleydb/libdb)
or 
db-18.1.32.tar.gz (https://github.com/Mindlestick/TinyIoT/raw/main/db-18.1.32.tar.gz)


If you downloaded the db-18.1.32.tar.gz file, unzip it.

    tar xvfz db-18.1.32.tar.gz

Then go to the build_unix directory.

    cd db-18.1.32
    cd build_unix

Then configure with the folloing options. 

../dist/configure \
--enable-dbm \
--enable-sql \
--enable-o_direct \
--enable-localization \
--enable-umrw \
--enable-cxx \
--enable-atomicfileread \
--enable-sql_compat \
CFLAGS="-I/usr/local/ssl/include -L/usr/local/ssl/lib -DSQLITE_DIRECT_OVERFLOW_READ -DSQLITE_ENABLE_DBSTAT_VTAB -DSQLITE_ENABLE_DBPAGE_VTAB -DSQLITE_ENABLE_RTREE -DSQLITE_USE_ALLOCA -DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1 -DSQLITE_ENABLE_COLUMN_METADATA -DSQLITE_ENABLE_RBU"

You may need to specify your machine, if your ubuntu doesn't detect it automatically. 

    --build=aarch64-unknown-linux-gnu

Tnen build with the following as usual: 

    make
    sudo make install


You have to export this new libration to LD_LIBRARY_PATH

    LD_LIBRARY_PATH=/usr/local/BerkeleyDB.18.1/lib:$LD_LIBRARY_PATH
    export LD_LIBRARY_PATH

Still you will get an error about 'db.h'. 
Then install the following pacakges: 

    sudo apt install libdb-dev
    sudo apt-get install libdb4o-cil-dev

If you run the above code and still get an error install follwoing packages: 

    sudo apt install libdb4-dev
    sudo apt-get install libc6-i386 lib32gcc1
    sudo apt-get install ia32-libs g++-multilib






