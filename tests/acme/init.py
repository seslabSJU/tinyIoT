#
#	init.py (tinyIoT port)
#
#	Ported from ACME-oneM2M-CSE/tests/init.py
#	Original: (c) 2020 by Andreas Kraft, BSD 3-Clause License
#
#	HTTP-only version. MQTT, WebSocket, CoAP, acme package dependencies removed.
#

from __future__ import annotations
from typing import Any, Callable, Tuple, cast, Optional, TypeAlias, Type
from dataclasses import dataclass
from urllib.parse import ParseResult, urlparse, parse_qs
import sys, io, atexit, base64
import unittest
from datetime import timedelta, datetime, timezone
from threading import Thread
from http.server import HTTPServer, BaseHTTPRequestHandler
import requests, requests.adapters
import json, time, ssl, urllib3, random, re, importlib

from rich.console import Console

from config import *


###############################################################################
#
#	Type aliases (replaces acme.etc.Types)
#

JSON:TypeAlias       = dict
Parameters:TypeAlias = dict
STRING:TypeAlias     = str


###############################################################################
#
#	Enums (replaces acme.etc.Types)
#

from enum import IntEnum

class Operation(IntEnum):
	CREATE   = 1
	RETRIEVE = 2
	UPDATE   = 3
	DELETE   = 4
	NOTIFY   = 5


class NotificationEventType(IntEnum):
	resourceUpdate                = 1
	resourceDelete                = 2
	createDirectChild             = 3
	deleteDirectChild             = 4
	retrieveCNTNoChild            = 5
	triggerReceivedForAE          = 6
	blockingUpdate                = 7
	missingData                   = 8


class ResultContentType(IntEnum):
	nothing                           = 0
	attributes                        = 1
	hierarchicalAddress               = 2
	hierarchicalAddressAttributes     = 3
	attributesAndChildResources       = 4
	attributesAndChildResourceReferences = 5
	childResourceReferences           = 6
	originalResource                  = 7
	childResources                    = 8
	modifiedAttributes                = 9
	semanticContent                   = 10
	discoveryResultReferences         = 11
	permissions                       = 12


class ResourceTypes(IntEnum):
	MIXED      = -2
	UNKNOWN    = -1
	ALL        = 0
	ACP        = 1
	AE         = 2
	CNT        = 3
	CIN        = 4
	CSEBase    = 5
	GRP        = 9
	ACI        = 13
	FCNTAnnc   = 10
	SUB        = 23
	FCNT       = 28
	TS         = 29
	TSI        = 30
	NOD        = 14
	REQ        = 17
	SCH        = 18
	SMD        = 24
	ACTR       = 36
	DEPR       = 57
	LCP        = 58
	NTP        = 60
	NTPR       = 61
	PCH        = 15
	PDR        = 16
	CRS        = 50
	TSB        = 33


class ResponseStatusCode(IntEnum):
	OK                                 = 2000
	CREATED                            = 2001
	DELETED                            = 2002
	UPDATED                            = 2004
	NO_CONTENT                         = 204
	BAD_REQUEST                        = 4000
	RELEASE_VERSION_NOT_SUPPORTED      = 4001
	NOT_FOUND                          = 4004
	OPERATION_NOT_ALLOWED              = 4005
	REQUEST_TIMEOUT                    = 4008
	INVALID_CHILD_RESOURCE_TYPE        = 4009
	ORIGINATOR_HAS_NO_PRIVILEGE        = 4103
	ALREADY_EXISTS                     = 4104
	CONFLICT                           = 4105
	ORIGINATOR_HAS_ALREADY_REGISTERED  = 4106
	SECURITY_ASSOCIATION_REQUIRED      = 4107
	INVALID_CHILD_RESOURCE_TYPE2       = 4108
	INTERNAL_SERVER_ERROR              = 5000
	NOT_IMPLEMENTED                    = 5001
	TARGET_NOT_REACHABLE               = 6003
	NO_PRIVILEGE                       = 6104
	ORIGINATOR_NOT_REGISTERED          = 5104

RC = ResponseStatusCode
T  = ResourceTypes


###############################################################################
#
#	HTTP Header Constants (replaces acme.etc.Constants)
#

class C:
	hfOrigin  = 'X-M2M-Origin'
	hfRI      = 'X-M2M-RI'
	hfRVI     = 'X-M2M-RVI'
	hfRSC     = 'X-M2M-RSC'
	hfOT      = 'X-M2M-OT'
	hfVSI     = 'X-M2M-VSI'
	hfRET     = 'X-M2M-RET'
	hfOET     = 'X-M2M-OET'
	hfRST     = 'X-M2M-RST'
	hfRTU     = 'X-M2M-RTU'

	# Upper Tester (unused but kept for compatibility)
	hfUTCMD   = 'X-M2M-UTCMD'
	hfUTRSP   = 'X-M2M-UTRSP'


###############################################################################
#
#	Global state
#

verifyCertificate     = False
verboseRequests       = False
testCaseNames:Optional[list[str]] = None
excludedTestNames:Optional[list[str]] = []
enableTearDown        = True
initialRequestTimeout = 10.0
localNotificationServer = False

timeDelta             = 0
notificationDelay     = NOTIFICATIONDELAY
expirationCheckDelay  = 2
expirationSleep       = expirationCheckDelay * 3
requestETDuration     = f'PT{expirationCheckDelay:d}S'
requestETDuration2    = f'PT{expirationCheckDelay*2:d}S'
requestETDurationInteger = expirationCheckDelay * 1000
requestCheckDelay     = 1
requestExpirationDelay = 3.0
timeSeriesInterval    = 2.0
tsbPeriodicInterval   = 1.0
crsTimeWindowSize     = 4.0
actionPeriod          = 1 * 1000
testVerbosity         = 2

futureTimestamp = '88881231T235959'

console = Console()

# HTTP Session
httpSession:requests.Session = None

requestCount:int = 0


###############################################################################
#
#	Resource names and URLs
#

actrRN  = 'testACTR'
aeRN    = 'testAE'
acpRN   = 'testACP'
batRN   = 'testBAT'
cinRN   = 'testCIN'
cntRN   = 'testCNT'
crsRN   = 'testCRS'
csrRN   = 'testCSR'
deprRN  = 'testDEPR'
fcntRN  = 'testFCNT'
grpRN   = 'testGRP'
lcpRN   = 'testLCP'
nodRN   = 'testNOD'
ntpRN   = 'testNTP'
ntprRN  = 'testNTPR'
pchRN   = 'testPCH'
pdrRN   = 'testPDR'
reqRN   = 'testREQ'
schRN   = 'testSCH'
smdRN   = 'testSMD'
subRN   = 'testSUB'
tsRN    = 'testTS'
tsbRN   = 'testTSB'
tsiRN   = 'testTSI'
memRN   = 'testMEM'

cseURL  = f'{CSEURL}{CSERN}'
csiURL  = f'{CSEURL}{CSEID}'
aeURL   = f'{cseURL}/{aeRN}'
acpURL  = f'{cseURL}/{acpRN}'
cntURL  = f'{aeURL}/{cntRN}'
cinURL  = f'{cntURL}/{cinRN}'
crsURL  = f'{aeURL}/{crsRN}'
csrURL  = f'{cseURL}/{csrRN}'
fcntURL = f'{aeURL}/{fcntRN}'
grpURL  = f'{aeURL}/{grpRN}'
lcpURL  = f'{aeURL}/{lcpRN}'
nodURL  = f'{cseURL}/{nodRN}'
ntpURL  = f'{cseURL}/{ntpRN}'
pchURL  = f'{aeURL}/{pchRN}'
pdrURL  = f'{ntpURL}/{pdrRN}'
pcuURL  = f'{pchURL}/pcu'
smdURL  = f'{aeURL}/{smdRN}'
subURL  = f'{cntURL}/{subRN}'
tsURL   = f'{aeURL}/{tsRN}'
tsBURL  = f'{aeURL}/{tsbRN}'
actrURL = f'{cntURL}/{actrRN}'
deprURL = f'{actrURL}/{deprRN}'


###############################################################################
#
#	Shutdown
#

@atexit.register
def shutdown() -> None:
	global httpSession
	if httpSession:
		httpSession.close()
		httpSession = None


###############################################################################
#
#	Requests
#

def _RETRIEVE(url:str, originator:str, timeout:float=None, headers:Parameters={}) -> Tuple[str|JSON, int]:
	return sendRequest(Operation.RETRIEVE, url, originator, timeout=timeout, headers=headers)

def RETRIEVESTRING(url:str, originator:str, timeout:float=None, headers:Parameters={}) -> Tuple[str, int]:
	x, rsc = _RETRIEVE(url=url, originator=originator, timeout=timeout, headers=headers)
	return str(x, 'utf-8'), rsc  # type: ignore

def RETRIEVE(url:str, originator:str, timeout:float=None, headers:Parameters={}) -> Tuple[JSON, int]:
	x, rsc = _RETRIEVE(url=url, originator=originator, timeout=timeout, headers=headers)
	return cast(JSON, x), rsc

def CREATE(url:str, originator:str, ty:ResourceTypes=None, data:JSON=None, headers:Parameters={}) -> Tuple[JSON, int]:
	x, rsc = sendRequest(Operation.CREATE, url, originator, ty, data, headers=headers)
	return cast(JSON, x), rsc

def NOTIFY(url:str, originator:str, data:JSON=None, headers:Parameters={}) -> Tuple[JSON, int]:
	x, rsc = sendRequest(Operation.NOTIFY, url, originator, data=data, headers=headers)
	return cast(JSON, x), rsc

def _UPDATE(url:str, originator:str, data:JSON|str, headers:Parameters={}) -> Tuple[str|JSON, int]:
	return sendRequest(Operation.UPDATE, url, originator, data=data, headers=headers)

def UPDATESTRING(url:str, originator:str, data:str, headers:Parameters={}) -> Tuple[str, int]:
	x, rsc = _UPDATE(url=url, originator=originator, data=data, headers=headers)
	return str(x, 'utf-8'), rsc  # type: ignore

def UPDATE(url:str, originator:str, data:JSON, headers:Parameters={}) -> Tuple[JSON, int]:
	x, rsc = _UPDATE(url=url, originator=originator, data=data, headers=headers)
	return cast(JSON, x), rsc

def DELETE(url:str, originator:str, headers:Parameters={}) -> Tuple[JSON, int]:
	x, rsc = sendRequest(Operation.DELETE, url, originator, headers=headers)
	return cast(JSON, x), rsc


def sendRequest(operation:Operation,
				url:str,
				originator:str,
				ty:ResourceTypes=None,
				data:JSON|str=None,
				ct:str='application/json',
				timeout:float=None,
				headers:Parameters={}) -> Tuple[STRING|JSON, int]:
	global requestCount, httpSession
	requestCount += 1

	if not httpSession:
		httpSession = requests.Session()
		httpAdapter = requests.adapters.HTTPAdapter(pool_connections=100, pool_maxsize=100, max_retries=3)
		httpSession.mount('http://', httpAdapter)
		httpSession.mount('https://', httpAdapter)

	match operation:
		case Operation.CREATE:
			return sendHttpRequest('post', url=url, originator=originator, ty=ty, data=data, ct=ct, timeout=timeout, headers=headers)
		case Operation.RETRIEVE:
			return sendHttpRequest('get', url=url, originator=originator, ty=ty, data=data, ct=ct, timeout=timeout, headers=headers)
		case Operation.UPDATE:
			return sendHttpRequest('put', url=url, originator=originator, ty=ty, data=data, ct=ct, timeout=timeout, headers=headers)
		case Operation.DELETE:
			return sendHttpRequest('delete', url=url, originator=originator, ty=ty, data=data, ct=ct, timeout=timeout, headers=headers)
		case Operation.NOTIFY:
			return sendHttpRequest('post', url=url, originator=originator, ty=ty, data=data, ct=ct, timeout=timeout, headers=headers)

	return None, 5103


def addHttpAuthorizationHeader(headers:Parameters) -> Optional[Tuple[str, int]]:
	if doHttpBasicAuth:
		_t = f'{httpUserName}:{httpPassword}'
		headers['Authorization'] = f'Basic {base64.b64encode(_t.encode("utf-8")).decode("utf-8")}'
	elif doHttpTokenAuth:
		headers['Authorization'] = f'Bearer {httpAuthToken}'
	return None


def sendHttpRequest(method:str, url:str, originator:str, ty:ResourceTypes=None, data:JSON|str=None, ct:str=None, timeout:float=None, headers:Parameters=None) -> Tuple[STRING|JSON, int]:
	global httpSession

	ct = 'application/json'

	if isinstance(ty, (ResourceTypes, int)):
		tys = f';ty={int(ty)}'
	elif ty is not None:
		tys = f';ty={ty}'
	else:
		tys = ''

	hds = {
		'Content-Type'  : f'{ct}{tys}',
		'Accept'        : ct,
		C.hfRI          : (rid := uniqueID()),
		C.hfRVI         : RELEASEVERSION,
	}
	if originator is not None:
		hds[C.hfOrigin] = originator

	if headers is not None:
		if C.hfRVI in headers:
			hds[C.hfRVI] = headers[C.hfRVI]
			del headers[C.hfRVI]
		if C.hfRSC in headers:
			hds[C.hfRSC] = str(headers[C.hfRSC])
			del headers[C.hfRSC]
		if C.hfOT in headers:
			hds[C.hfOT] = str(headers[C.hfOT])
			del headers[C.hfOT]
		hds.update(headers)

	if (_r := addHttpAuthorizationHeader(hds)) is not None:
		return _r

	if verboseRequests:
		console.print('\n[b u]Request')
		console.print(f'[dark_orange]{method}[/dark_orange] {url}')
		console.print('\n'.join([f'{h}: {v}' for h, v in hds.items()]))
		if isinstance(data, dict):
			console.print_json(data=data)

	setLastRequestID(rid)
	try:
		sendData:Optional[str] = None
		if data is not None:
			sendData = json.dumps(data) if isinstance(data, dict) else data
		r = httpSession.request(method=method, url=url, data=sendData, headers=hds, verify=verifyCertificate, timeout=timeout)
	except Exception as e:
		return f'Failed to send request: {str(e)}', 5103

	rc = int(r.headers.get(C.hfRSC, r.status_code))
	if rc == 204:
		rc = ResponseStatusCode.NO_CONTENT

	setLastHeaders(r.headers)  # type: ignore

	if verboseRequests:
		console.print(f'\n[b u]Response - {r.status_code}')
		console.print('\n'.join([f'{h}: {v}' for h, v in r.headers.items()]))
		if r.content:
			console.print(r.json())

	if (ct := r.headers.get('Content-Type')) is not None and ct.startswith('text/plain'):
		return r.content, rc
	elif ct is not None and ct.startswith(('application/json', 'application/vnd.onem2m-res+json')):
		return r.json() if len(r.content) > 0 else None, rc

	return r.content, rc


###############################################################################
#
#	Request/Response tracking
#

_lastRequstID = None

def setLastRequestID(rid:str) -> None:
	global _lastRequstID
	_lastRequstID = rid

def lastRequestID() -> str:
	return _lastRequstID

_lastHeaders:Parameters = None

def setLastHeaders(hds:Parameters) -> None:
	global _lastHeaders
	_lastHeaders = hds

def lastHeaders() -> Parameters:
	return _lastHeaders


###############################################################################
#
#	Connection / Upper Tester
#

def connectionPossible(url:str) -> Tuple[bool, int]:
	try:
		result = RETRIEVE(url, ORIGINATOR, timeout=initialRequestTimeout)
		return result[1] == ResponseStatusCode.OK, result[1]
	except Exception as e:
		print(e)
		return False, 5103

def checkUpperTester() -> None:
	pass  # Upper Tester not supported in tinyIoT

def disableUpperTester() -> None:
	pass

def testCaseStart(name:str) -> None:
	if verboseRequests:
		console.print(f'[dim][ Start {name} ]')

def testCaseEnd(name:str) -> None:
	if verboseRequests:
		console.print(f'[dim][ End {name} ]')


###############################################################################
#
#	Expiration / Reconfiguration (stubs - Upper Tester required)
#

_orgExpCheck = -1
_maxExpiration = -1
_tooLargeResourceExpirationDelta = -1
_orgRequestExpirationDelta = -1.0

def enableShortResourceExpirations() -> None:
	pass  # requires Upper Tester

def disableShortResourceExpirations() -> None:
	pass

def isTestResourceExpirations() -> bool:
	return _orgExpCheck != -1

def tooLargeResourceExpirationDelta() -> int:
	return _tooLargeResourceExpirationDelta

def enableShortRequestExpirations() -> None:
	pass

def disableShortRequestExpirations() -> None:
	pass

def isShortRequestExpirations() -> bool:
	return _orgRequestExpirationDelta != -1.0


###############################################################################
#
#	Notification Server
#

nextNotificationResult:ResponseStatusCode = ResponseStatusCode.OK
_lastNotification:JSON = None
_lastNotificationHeaders:Parameters = None
_lastNotificationArguments:Parameters = None
_notificationServer:HTTPServer = None
_notificationServerThread:Thread = None


class SimpleHTTPRequestHandler(BaseHTTPRequestHandler):

	def do_POST(self) -> None:
		global nextNotificationResult

		self.send_response(nextNotificationResult if isinstance(nextNotificationResult, int) else 200)
		self.send_header(C.hfRSC, str(int(nextNotificationResult)))
		self.send_header(C.hfOT, getResourceDate())
		self.send_header(C.hfOrigin, ORIGINATORNotifResp)
		if C.hfRI in self.headers:
			self.send_header(C.hfRI, self.headers[C.hfRI])
		self.end_headers()
		nextNotificationResult = ResponseStatusCode.OK

		length = int(self.headers.get('Content-Length', 0))
		post_data = self.rfile.read(length)
		decoded_data = ''
		if len(post_data) > 0:
			contentType = (self.headers.get('Content-Type') or '').lower()
			if contentType.startswith(('application/json', 'application/vnd.onem2m-res+json')):
				setLastNotification(decoded_data := json.loads(post_data.decode('utf-8')))

		setLastNotificationHeaders(dict(self.headers))
		setLastNotificationArguments(parse_qs(urlparse(self.path).query))  # type: ignore

		if verboseRequests and self.headers.get(C.hfOrigin):
			console.print('\n[b u]Received Notification Request')
			console.print('\n'.join([f'{h}: {v}' for h, v in self.headers.items()]))

	def log_message(self, format:str, *args:Any) -> None:
		pass  # suppress access log


def getResourceDate() -> str:
	return datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')


def setNotificationServerURL() -> None:
	pass  # URL is set in config.py


def runNotificationServer() -> None:
	global _notificationServer
	_notificationServer.serve_forever()


def startNotificationServer() -> None:
	global _notificationServer, _notificationServerThread
	if _notificationServer:
		return
	_notificationServer = HTTPServer(('', NOTIFICATIONPORT), SimpleHTTPRequestHandler)
	_notificationServerThread = Thread(target=runNotificationServer, daemon=True)
	_notificationServerThread.start()


def stopNotificationServer() -> None:
	global _notificationServer, _notificationServerThread
	if _notificationServer:
		_notificationServer.shutdown()
		_notificationServer = None
		_notificationServerThread = None


def isNotificationServerRunning() -> bool:
	return _notificationServer is not None


def setLastNotification(notification:JSON) -> None:
	global _lastNotification
	_lastNotification = notification

def getLastNotification(clear:bool=False, wait:float=0.0) -> JSON:
	global _lastNotification
	if wait > 0.0:
		testSleep(wait)
	n = _lastNotification
	if clear:
		_lastNotification = None
	return n

def clearLastNotification(nextResult:ResponseStatusCode=ResponseStatusCode.OK) -> None:
	global _lastNotification, nextNotificationResult
	_lastNotification = None
	nextNotificationResult = nextResult

def setLastNotificationHeaders(headers:Parameters) -> None:
	global _lastNotificationHeaders
	_lastNotificationHeaders = headers

def getLastNotificationHeaders() -> Parameters:
	return _lastNotificationHeaders

def setLastNotificationArguments(arguments:Parameters) -> None:
	global _lastNotificationArguments
	_lastNotificationArguments = arguments

def getLastNotificationArguments() -> Parameters:
	return _lastNotificationArguments


###############################################################################
#
#	Timing
#

_sleepTimeCount:float = 0.0

def testSleep(ti:float) -> None:
	global _sleepTimeCount
	_sleepTimeCount += ti
	time.sleep(ti)

def clearSleepTimeCount() -> None:
	global _sleepTimeCount
	_sleepTimeCount = 0

def getSleepTimeCount() -> float:
	return _sleepTimeCount

def utcNow() -> datetime:
	return datetime.now(tz=timezone.utc)

def utcTimestamp() -> float:
	return utcNow().timestamp()

def createScheduleString(range:int, delay:int=0) -> str:
	dts = datetime.now(tz=timezone.utc) + timedelta(seconds=delay)
	dte = dts + timedelta(seconds=range)
	return f'{dts.second}-{dte.second} {dts.minute}-{dte.minute} {dts.hour}-{dte.hour} * * * *'


###############################################################################
#
#	Utilities
#

def printResult(result:unittest.TestResult) -> None:
	for f in result.failures:
		console.print(f'\n[bold][red]{f[0]}')
		console.print(f'[dim]{f[0].shortDescription()}')
		console.print(f[1])

def waitMessage(msg:str, delay:float) -> None:
	if delay:
		with console.status(f'[bright_blue]{msg}'):
			testSleep(delay)
	else:
		console.print(f'[bright_blue]{msg}')

def uniqueID() -> str:
	return str(random.randint(1, sys.maxsize))

def uniqueRN(prefix:str='') -> str:
	return f'{prefix}{round(time.time() * 1000)}-{uniqueID()}'

def toSPRelative(originator:str) -> str:
	if not isSPRelative(originator):
		return f'{CSEID}/{originator}'
	return originator

def isSPRelative(uri:str) -> bool:
	return uri is not None and len(uri) >= 2 and uri[0] == '/' and uri[1] != '/'

def getIPAddress() -> str:
	import socket
	try:
		s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		s.connect(('8.8.8.8', 80))
		return s.getsockname()[0]
	except Exception:
		return '127.0.0.1'
	finally:
		s.close()

def addTest(suite:unittest.TestSuite, case:unittest.TestCase) -> None:
	global testCaseNames
	if testCaseNames is None:
		suite.addTest(case)
	elif testCaseNames and case._testMethodName in testCaseNames:
		testCaseNames.remove(case._testMethodName)
		suite.addTest(case)

def addTests(suite:unittest.TestSuite, cls:Type[unittest.TestCase], cases:list[str]) -> None:
	def _addTest(case:str) -> None:
		if case and case not in excludedTestNames:
			try:
				suite.addTest(cls(case))
			except ValueError:
				console.print(f'[red]Test case "{case}" not found - skipping[/red]')

	if testCaseNames is None:
		for case in cases:
			_addTest(case)
	else:
		for case in testCaseNames:
			_addTest(case)

def isTearDownEnabled() -> bool:
	return enableTearDown


decimalMatch = re.compile(r'{(\d+)}')

def findXPath(dct:JSON, key:str, default:Any=None) -> Any:
	if not key or not dct:
		return default
	if key in dct:
		return dct[key]

	paths = key.split('/')
	data:Any = dct
	for i in range(0, len(paths)):
		if not data:
			return default
		pathElement = paths[i]
		if len(pathElement) == 0:
			return default
		elif (m := decimalMatch.search(pathElement)) is not None:
			idx = int(m.group(1))
			if not isinstance(data, (list, dict)) or idx >= len(data):
				return default
			if isinstance(data, dict):
				data = data[list(data)[i]]
			else:
				data = data[idx]
		elif pathElement == '{}':
			if not isinstance(data, (list, dict)):
				return default
			if i == len(paths) - 1:
				return data
			return [findXPath(d, '/'.join(paths[i+1:]), default) for d in data]
		elif pathElement == '{*}':
			if isinstance(data, dict):
				if keys := list(data.keys()):
					data = data[keys[0]]
				else:
					return default
			else:
				return default
		elif pathElement not in data:
			return default
		else:
			data = data[pathElement]
	return data


###############################################################################
#
#	Suppress SSL warnings
#

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)


###############################################################################
#
#	Startup check - is the CSE running?
#

_r, status = connectionPossible(cseURL)
if status == 401:
	console.print('[red]CSE requires authorization')
	quit(-1)
noCSE = not _r
noRemote = True  # Remote CSE not used
