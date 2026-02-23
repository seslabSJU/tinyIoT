#
#	config.py
#
#	tinyIoT 서버 접속 설정 (config.h 값 기반)
#

CSEHOST = 'localhost'
CSEPORT = 3000
CSERN   = 'TinyIoT'
CSEID   = '/tinyiot'
CSEURL  = f'http://{CSEHOST}:{CSEPORT}/{CSERN}'

ORIGINATOR      = 'CAdmin'
RELEASEVERSION  = '3'
APPID           = 'NMyapp3'
BINDING         = 'http'

# Resource names
aeRN  = 'testAE'
acpRN = 'testACP'
cntRN = 'testCNT'
cinRN = 'testCIN'
grpRN = 'testGRP'

# Resource URLs (소문자 변수명 - testXXX.py에서 사용)
cseURL = CSEURL
aeURL  = f'{CSEURL}/{aeRN}'
acpURL = f'{CSEURL}/{acpRN}'
cntURL = f'{aeURL}/{cntRN}'
cinURL = f'{cntURL}/{cinRN}'
grpURL = f'{aeURL}/{grpRN}'
