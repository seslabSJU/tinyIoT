AC_INIT([tinyiot], [1.0], [your-email@example.com])

AC_PROG_CC

# SERVER_TYPE
AC_ARG_WITH([server-type],
            AS_HELP_STRING([--with-server-type=TYPE],
                           [Set the server type to MN-CSE or IN-CSE]),
            [case "${withval}" in
                mn) server_type=MN_CSE ;;
                *) server_type=IN_CSE ;; # Default to IN-CSE if not specified
             esac])

AC_ARG_WITH([server-ip],
            AS_HELP_STRING([--with-server-ip=IP],
                           [Set the server IP]),
            [server_ip="${withval}"])
AC_ARG_WITH([server-port],
            AS_HELP_STRING([--with-server-port=PORT],
                           [Set the server port]),
            [server_port="${withval}"])
AC_ARG_WITH([cse-base-name],
            AS_HELP_STRING([--with-cse-base-name=NAME],
                           [Set the CSE base name]),
            [cse_base_name="${withval}"])
AC_ARG_WITH([cse-base-ri],
            AS_HELP_STRING([--with-cse-base-ri=RI],
                           [Set the CSE base RI]),
            [cse_base_ri="${withval}"])

# REMOTE_CSE (MN-CSE)
AC_ARG_WITH([remote-cse-id],
            AS_HELP_STRING([--with-remote-cse-id=ID],
                           [Set the remote CSE ID]),
            [remote_cse_id="${withval}"])
AC_ARG_WITH([remote-cse-name],
            AS_HELP_STRING([--with-remote-cse-name=NAME],
                           [Set the remote CSE name]),
            [remote_cse_name="${withval}"])
AC_ARG_WITH([remote-cse-host],
            AS_HELP_STRING([--with-remote-cse-host=HOST],
                           [Set the remote CSE host]),
            [remote_cse_host="${withval}"])
AC_ARG_WITH([remote-cse-port],
            AS_HELP_STRING([--with-remote-cse-port=PORT],
                           [Set the remote CSE port]),
            [if test -z "${withval}"; then
                remote_cse_port=0
             else
                remote_cse_port="${withval}"
             fi])

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
    PKG_CHECK_MODULES([OPENSSL], [openssl])
fi

# Define config.h settings
AC_DEFINE_UNQUOTED([SERVER_TYPE], [$server_type])

AC_DEFINE_UNQUOTED([SERVER_IP], ["$server_ip"])
AC_DEFINE_UNQUOTED([SERVER_PORT], ["$server_port"])
AC_DEFINE_UNQUOTED([CSE_BASE_NAME], ["$cse_base_name"])
AC_DEFINE_UNQUOTED([CSE_BASE_RI], ["$cse_base_ri"])

AC_DEFINE_UNQUOTED([REMOTE_CSE_ID], ["$remote_cse_id"])
AC_DEFINE_UNQUOTED([REMOTE_CSE_NAME], ["$remote_cse_name"])
AC_DEFINE_UNQUOTED([REMOTE_CSE_HOST], ["$remote_cse_host"])
AC_DEFINE_UNQUOTED([REMOTE_CSE_PORT], [$remote_cse_port])

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