/* coap_config.h.  Generated from coap_config.h.in by configure.  */
/* coap_config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Define to build support for Unix socket packets. */
#define COAP_AF_UNIX_SUPPORT 1

/* Define to 1 to build with support for async separate responses. */
#define COAP_ASYNC_SUPPORT 1

/* Define to 1 if libcoap supports client mode code. */
#define COAP_CLIENT_SUPPORT 1

/* Define to 1 if the system has small stack size. */
/* #undef COAP_CONSTRAINED_STACK */

/* Define to 1 to build without TCP support. */
#define COAP_DISABLE_TCP 1

/* Define to 1 if the system has epoll support. */
// #define COAP_EPOLL_SUPPORT 1

/* Define to build support for IPv4 packets. */
#define COAP_IPV4_SUPPORT 1

/* Define to build support for IPv6 packets. */
// #define COAP_IPV6_SUPPORT 1

/* Define to level if max logging level is not 8 */
/* #undef COAP_MAX_LOGGING_LEVEL */

/* Define to 1 to build with OSCORE support. */
/* #undef COAP_OSCORE_SUPPORT */

/* Define to 1 to build with Q-Block support. */
#define COAP_Q_BLOCK_SUPPORT 1

/* Define to 1 if libcoap supports server mode code. */
#define COAP_SERVER_SUPPORT 1

/* Define to 1 if the system has libgnutls28. */
/* #undef COAP_WITH_LIBGNUTLS */

/* Define to 1 if the system has libmbedtls2.7.10. */
/* #undef COAP_WITH_LIBMBEDTLS */

/* Define to 1 if the system has libssl1.1. */
#define COAP_WITH_LIBOPENSSL 1

/* Define to 1 if the system has libtinydtls. */
/* #undef COAP_WITH_LIBTINYDTLS */

/* Define to 1 to build support for persisting observes. */
/* #undef COAP_WITH_OBSERVE_PERSIST */

/* Define to 1 to build with WebSockets support. */
/* #undef COAP_WS_SUPPORT */

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 1

/* Define to 1 if you have the <assert.h> header file. */
#define HAVE_ASSERT_H 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if TinyDTLS has dtls_set_log_handler. */
/* #undef HAVE_DTLS_SET_LOG_HANDLER */

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define to 1 if you have the `getaddrinfo' function. */
#define HAVE_GETADDRINFO 1

/* Define to 1 if you have the `getrandom' function. */
// #define HAVE_GETRANDOM 1

/* Define to 1 if you have the <ifaddrs.h> header file. */
#define HAVE_IFADDRS_H 1

/* Define to 1 if you have the `if_nametoindex' function. */
#define HAVE_IF_NAMETOINDEX 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if the system has libcunit. */
/* #undef HAVE_LIBCUNIT */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the `malloc' function. */
#define HAVE_MALLOC 1

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the <net/if.h> header file. */
#define HAVE_NET_IF_H 1

/* Define to 1 if libcoap has no tls library support. */
/* #undef HAVE_NOTLS */

/* Define to 1 if you have the <pthread.h> header file. */
#define HAVE_PTHREAD_H 1

/* Define to 1 if you have the `pthread_mutex_lock' function. */
#define HAVE_PTHREAD_MUTEX_LOCK 1

/* Define to 1 if you have the `random' function. */
#define HAVE_RANDOM 1

/* Define to 1 if you have the `select' function. */
#define HAVE_SELECT 1

/* Define to 1 if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strcasecmp' function. */
#define HAVE_STRCASECMP 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strnlen' function. */
#define HAVE_STRNLEN 1

/* Define to 1 if you have the `strrchr' function. */
#define HAVE_STRRCHR 1

/* Define to 1 if the system has the type `struct cmsghdr'. */
// #define HAVE_STRUCT_CMSGHDR 1

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/unistd.h> header file. */
#define HAVE_SYS_UNISTD_H 1

/* Define to 1 if you have the <time.h> header file. */
#define HAVE_TIME_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <winsock2.h> header file. */
/* #undef HAVE_WINSOCK2_H */

/* Define to 1 if you have the <ws2tcpip.h> header file. */
/* #undef HAVE_WS2TCPIP_H */

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Define to 1 if assertions should be disabled. */
/* #undef NDEBUG */

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "libcoap-developers@lists.sourceforge.net"

/* Define to the full name of this package. */
#define PACKAGE_NAME "libcoap"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "libcoap 4.3.4"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "libcoap"

/* Define to the home page for this package. */
#define PACKAGE_URL "https://libcoap.net/"

/* Define to the version of this package. */
#define PACKAGE_VERSION "4.3.4"

/* Define to 1 if all of the C90 standard headers exist (not just the ones
   required in a freestanding environment). This macro is provided for
   backward compatibility; new code need not use it. */
#define STDC_HEADERS 1

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
#if defined __BIG_ENDIAN__
#define WORDS_BIGENDIAN 1
#endif
#else
#ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
#endif
#endif

/* Define this to 1 for ancillary data on MacOS */
/* #undef __APPLE_USE_RFC_3542 */
#define __APPLE_USE_RFC_3542

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef ssize_t */
