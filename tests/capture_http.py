#
#	capture_http.py
#
#	requests.Session.send를 monkey-patch해서 모든 HTTP 통신을 캡처.
#	run_capture.py에서 import해서 사용.
#

import requests

# 현재 실행 중인 테스트 이름을 추적
_current_test = ['(setup)']

# 캡처된 엔트리 목록
CAPTURE_LOG = []

_orig_send = requests.Session.send


def _patched_send(self, request, **kwargs):
	response = _orig_send(self, request, **kwargs)

	# Content-Type에서 ty 추출: application/json;ty=2 → "2"
	ct = request.headers.get('Content-Type', '')
	ty = ''
	if ';ty=' in ct:
		ty = ct.split(';ty=')[-1].strip()

	entry = {
		'test_name':    _current_test[0],
		'method':       request.method,
		'url':          request.url,
		'originator':   request.headers.get('X-M2M-Origin', ''),
		'rvi':          request.headers.get('X-M2M-RVI', ''),
		'ty':           ty,
		'req_body':     request.body if request.body else '',
		'resp_status':  response.status_code,
		'rsc':          response.headers.get('X-M2M-RSC', ''),
		'resp_body':    response.text,
		'resp_headers': dict(response.headers),
	}
	CAPTURE_LOG.append(entry)
	return response


# monkey-patch 적용
requests.Session.send = _patched_send
