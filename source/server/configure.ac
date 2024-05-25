AC_INIT([tinyiot], [1.0], [your-email@example.com])

AC_PROG_CC

# SERVER_TYPE
AC_ARG_WITH([server-type],
            AS_HELP_STRING([--with-server-type=TYPE],
                           [Set the server type to MN-CSE or IN-CSE]),
            [case "${withval}" in
                mn) server_type=MN_CSE ;;
                *) server_type=IN_CSE ;;
             esac],
            [server_type=IN_CSE])  # Default to IN-CSE if not specified

# ENABLE_MQTT      
AC_ARG_ENABLE([mqtt], 
              AS_HELP_STRING([--enable-mqtt],       
                             [Enable MQTT support]), 
              [case "${enableval}" in
                  yes) enable_mqtt=yes ;;
                  *) enable_mqtt=no ;;
               esac], 
              [enable_mqtt=no])

# ENALBE_COAP
AC_ARG_ENABLE([coap], 
              AS_HELP_STRING([--enable-coap], 
                             [Enable COAP support]), 
              [case "${enableval}" in
                  yes) enable_coap=yes ;;
                  *) enable_coap=no ;;
               esac], 
              [enable_coap=no])

# ENABLE_COAPS
AC_ARG_ENABLE([coaps],
              AS_HELP_STRING([--enable-coaps], 
                             [Enable COAPS support (requires COAP support)]), 
              [case "${enableval}" in
                  yes) enable_coaps=yes ;;
                  *) enable_coaps=no ;;
               esac],
              [enable_coaps=no])

# Check for openssl and crypto libraries if CoAP or CoAPS is enabled
if test "x$enable_coap" = "xyes" || test "x$enable_coaps" = "xyes"; then
    AC_CHECK_LIB([ssl], [SSL_library_init])
    AC_CHECK_LIB([crypto], [CRYPTO_new])
fi

# Define config.h settings
AC_DEFINE_UNQUOTED([SERVER_TYPE], [$server_type])
if test "x$enable_mqtt" = "xyes"; then
    AC_DEFINE([ENABLE_MQTT])
fi
if test "x$enable_coap" = "xyes"; then
    AC_DEFINE([ENABLE_COAP])
fi
if test "x$enable_coaps" = "xyes"; then
    AC_DEFINE([ENABLE_COAPS])
fi

# Define Makefile settings
if test "x$enable_mqtt" = "xyes"; then
    ENABLE_MQTT="yes"
else
    ENABLE_MQTT="no"
fi
AC_SUBST(ENABLE_MQTT)

if test "x$enable_coap" = "xyes"; then
    ENABLE_COAP="yes"
else
    ENABLE_COAP="no"
fi
AC_SUBST(ENABLE_COAP)

AC_CONFIG_FILES([Makefile])
AC_CONFIG_HEADERS([config.h])
AC_OUTPUT