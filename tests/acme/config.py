import os
import socket

BINDING  = 'http'
PROTOCOL = 'http'

TARGETHOST = os.environ.get('CSE_HOST', 'localhost')
CSEPORT    = int(os.environ.get('CSE_PORT', '3050'))
HTTPROOT   = '/'

CSERN  = os.environ.get('CSE_RN', 'TinyIoT')
CSERI  = os.environ.get('CSE_RI', 'tinyiot')
CSEID  = f'/{CSERI}'

TARGETHOSTIP = socket.gethostbyname(TARGETHOST)
CSEHOST      = TARGETHOSTIP
CSEURL       = f'{PROTOCOL}://{CSEHOST}:{CSEPORT}{HTTPROOT}'

SPID                  = os.environ.get('CSE_SPID', 'tinyiot.example.com')
APPID                 = 'NMyApp1Id'
ORIGINATOR            = 'CAdmin'
ORIGINATORSelfReg     = 'C'
ORIGINATOREmpty       = ''
ORIGINATORNotifResp   = 'CTester'
RELEASEVERSION        = '3'

UPPERTESTERENABLED     = False
RECONFIGURATIONENABLED = False

TESTHOSTIP = None  # auto-detect

NOTIFICATIONPORT   = 9990
NOTIFICATIONDELAY  = 0.5

doOAuth         = False
doHttpBasicAuth = False
doHttpTokenAuth = False
httpUserName    = 'test'
httpPassword    = 'test'
httpAuthToken   = 'testToken'
