#
#	init.py
#
#	tinyIoT 테스트용 헬퍼 (순수 requests 기반, acmecse 의존성 없음)
#

import requests
import uuid
from config import *


# ── oneM2M 타입 상수 ──────────────────────────────────────────────────────────

class RC:
	"""oneM2M Response Status Codes"""
	OK                              = 2000
	CREATED                         = 2001
	DELETED                         = 2002
	UPDATED                         = 2004
	BAD_REQUEST                     = 4000
	OPERATION_NOT_ALLOWED           = 4005
	INVALID_CHILD_RESOURCE_TYPE     = 4004
	NOT_ACCEPTABLE                  = 4006
	ORIGINATOR_HAS_NO_PRIVILEGE     = 4103
	CONFLICT                        = 4105
	ORIGINATOR_HAS_ALREADY_REGISTERED = 4106
	SECURITY_ASSOCIATION_REQUIRED   = 4107
	SERVICE_SUBSCRIPTION_NOT_ESTABLISHED = 4109
	MAX_NUMBER_OF_MEMBER_EXCEEDED   = 6010
	NO_CONTENT                      = 2006


class T:
	"""oneM2M Resource Types"""
	ACP     = 1
	AE      = 2
	CNT     = 3
	CIN     = 4
	CSEBase = 5
	GRP     = 9
	MGMTOBJ = 13
	SUB     = 23
	TS      = 29
	TSI     = 30
	FCNT    = 28
	MIXED   = 0


class Permission:
	"""oneM2M Access Control Operations"""
	CREATE   = 1
	RETRIEVE = 2
	UPDATE   = 4
	DELETE   = 8
	NOTIFY   = 16
	DISCOVERY= 32
	ALL      = 63


class NotificationEventType:
	deleteDirectChild = 5


# ── 서버 접속 상태 ────────────────────────────────────────────────────────────

noCSE = False  # 서버 접속 불가 시 True (아래에서 체크)

try:
	r = requests.get(f'http://{CSEHOST}:{CSEPORT}/{CSERN}',
					 headers={'Accept': 'application/json',
					          'X-M2M-Origin': ORIGINATOR,
					          'X-M2M-RVI': RELEASEVERSION},
					 timeout=3)
	noCSE = (r.status_code == 0)
except Exception:
	noCSE = True


# ── HTTP 헬퍼 함수 ────────────────────────────────────────────────────────────

def _rsc_from_response(resp) -> int:
	"""응답 헤더에서 oneM2M RSC 코드 추출"""
	try:
		return int(resp.headers.get('X-M2M-RSC', resp.status_code))
	except (ValueError, TypeError):
		return resp.status_code


def _make_headers(originator: str, ty: int = None, rvi: str = None) -> dict:
	headers = {
		'Accept':       'application/json',
		'X-M2M-RVI':   rvi or RELEASEVERSION,
		'X-M2M-RI':    str(uuid.uuid4()),
	}
	if originator:
		headers['X-M2M-Origin'] = originator
	if ty is not None:
		headers['Content-Type'] = f'application/json;ty={ty}'
	else:
		headers['Content-Type'] = 'application/json'
	return headers


def _do(method, url, originator, ty=None, data=None, rvi=None, headers=None):
	"""공통 HTTP 요청. (body_dict_or_None, rsc_int) 반환"""
	hdr = _make_headers(originator, ty, rvi)
	# 호출자가 추가 헤더를 넘긴 경우 오버라이드 (예: {C.hfRVI: '4'})
	if headers:
		hdr.update(headers)
		# hfRVI 오버라이드 시 X-M2M-RVI 재설정
		if C.hfRVI in headers:
			hdr['X-M2M-RVI'] = headers[C.hfRVI]
	import json as _json
	body = _json.dumps(data) if data is not None else None
	try:
		resp = requests.request(method, url, headers=hdr,
		                        data=body, timeout=10)
	except Exception:
		return None, 0
	global _last_headers
	_last_headers = dict(resp.headers)
	rsc = _rsc_from_response(resp)
	try:
		resp_body = resp.json()
	except Exception:
		resp_body = None
	return resp_body, rsc


def CREATE(url, originator, ty, data, headers=None):
	return _do('POST', url, originator, ty=ty, data=data, headers=headers)

def RETRIEVE(url, originator, rvi=None, headers=None):
	return _do('GET', url, originator, rvi=rvi, headers=headers)

def UPDATE(url, originator, data, headers=None):
	return _do('PUT', url, originator, data=data, headers=headers)

def DELETE(url, originator, headers=None):
	return _do('DELETE', url, originator, headers=headers)


# ── 유틸리티 ─────────────────────────────────────────────────────────────────

def findXPath(dct, path, default=None):
	"""slash-separated path로 중첩 dict 탐색. 예: 'm2m:ae/aei'"""
	if dct is None:
		return default
	keys = path.split('/')
	cur = dct
	for k in keys:
		if not isinstance(cur, dict) or k not in cur:
			return default
		cur = cur[k]
	return cur


def lastHeaders():
	"""마지막 응답 헤더 반환 (stub - 캡처 모드에서 채워짐)"""
	return {}


def isTearDownEnabled() -> bool:
	return True


def testCaseStart(name: str):
	print(f'\n>>> {name}')


def testCaseEnd(name: str):
	print(f'<<< {name}')


# Notification server stubs (미구현)
NOTIFICATIONSERVER = 'http://localhost:9999'
notificationDelay  = 0.5

def startNotificationServer():
	pass

def stopNotificationServer():
	pass

def isNotificationServerRunning() -> bool:
	return False

def clearLastNotification():
	pass

def getLastNotification(wait: float = None):
	return None


# ORIGINATORs
ORIGINATOREmpty   = ''
ORIGINATORSelfReg = 'Creg'


# Header field name constants
class C:
	hfRVI    = 'X-M2M-RVI'
	hfOrigin = 'X-M2M-Origin'
	hfRSC    = 'X-M2M-RSC'
	hfRI     = 'X-M2M-RI'


# ── 테스트 프레임워크 헬퍼 ────────────────────────────────────────────────────

import unittest as _unittest

# run() 함수의 반환 타입 힌트용
TestResult = tuple  # (testsRun, errors, skipped, sleepTime)

testVerbosity = 2

def addTests(suite: _unittest.TestSuite, cls, names: list):
	"""테스트 이름 목록을 suite에 추가"""
	for name in names:
		suite.addTest(cls(name))


def printResult(result: _unittest.TestResult):
	"""테스트 결과 출력"""
	total = result.testsRun
	fails = len(result.errors) + len(result.failures)
	skips = len(result.skipped)
	print(f'\nTotal: {total}, Failures: {fails}, Skipped: {skips}')


def getSleepTimeCount() -> float:
	"""sleep 시간 합계 (stub)"""
	return 0.0


# 마지막 응답 헤더 저장 (lastHeaders()용)
_last_headers: dict = {}

def lastHeaders() -> dict:
	return _last_headers
