#!/usr/bin/env python3
#
#	capture_to_sh.py
#
#	run_capture.py로 생성된 *_capture.json을 bash+curl .sh 파일로 변환.
#
#	Usage:
#	    python3 capture_to_sh.py testAE_capture.json
#	    python3 capture_to_sh.py testAE_capture.json testCNT_capture.json ...
#	    python3 capture_to_sh.py --all
#

import sys
import json
import re
from pathlib import Path

# ── config.h 파싱 (URL 역치환용) ─────────────────────────────────────────────

CONFIG_H_PATH = Path(__file__).parent.parent / 'source' / 'server' / 'config.h'

def parse_config_h(path: Path) -> dict:
	cfg = {}
	try:
		text = path.read_text()
	except FileNotFoundError:
		return {}
	for m in re.finditer(r'#define\s+(\w+)\s+"([^"]*)"', text):
		cfg[m.group(1)] = m.group(2)
	for m in re.finditer(r'#define\s+(\w+)\s+(\d+)\b', text):
		cfg[m.group(1)] = m.group(2)
	rvi_map = {'RVI_1': '1', 'RVI_2': '2', 'RVI_2a': '2a', 'RVI_3': '3', 'RVI_4': '4'}
	m = re.search(r'#define\s+CSE_RVI\s+(\w+)', text)
	if m:
		cfg['CSE_RVI'] = rvi_map.get(m.group(1), '3')
	return cfg

CONFIG   = parse_config_h(CONFIG_H_PATH)
CSE_HOST = 'localhost'
CSE_PORT = CONFIG.get('SERVER_PORT', '3000')
CSE_RN   = CONFIG.get('CSE_BASE_NAME', 'TinyIoT')
CSE_RI   = CONFIG.get('CSE_BASE_RI', 'tinyiot')
ORIGINATOR = CONFIG.get('ADMIN_AE_ID', 'CAdmin')
RVI      = CONFIG.get('CSE_RVI', '3')

CSE_BASE_URL = f'http://{CSE_HOST}:{CSE_PORT}/{CSE_RN}'

# ── URL → bash 변수 역치환 ─────────────────────────────────────────────────

# 구체적인 것부터 먼저 매핑 (긴 것 먼저)
URL_REPLACE_PATTERNS = [
	# (regex_pattern, bash_replacement)
	(rf'http://{re.escape(CSE_HOST)}:{re.escape(CSE_PORT)}/{re.escape(CSE_RN)}/testAE/testCNT/testCIN', '${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}/${cinRN}'),
	(rf'http://{re.escape(CSE_HOST)}:{re.escape(CSE_PORT)}/{re.escape(CSE_RN)}/testAE/testCNT',        '${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}'),
	(rf'http://{re.escape(CSE_HOST)}:{re.escape(CSE_PORT)}/{re.escape(CSE_RN)}/testAE/testGRP',        '${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}'),
	(rf'http://{re.escape(CSE_HOST)}:{re.escape(CSE_PORT)}/{re.escape(CSE_RN)}/testAE',                '${CSE_URL}/${CSE_RN}/${aeRN}'),
	(rf'http://{re.escape(CSE_HOST)}:{re.escape(CSE_PORT)}/{re.escape(CSE_RN)}/testACP',               '${CSE_URL}/${CSE_RN}/${acpRN}'),
	(rf'http://{re.escape(CSE_HOST)}:{re.escape(CSE_PORT)}/{re.escape(CSE_RN)}',                       '${CSE_URL}/${CSE_RN}'),
	(rf'http://{re.escape(CSE_HOST)}:{re.escape(CSE_PORT)}',                                           '${CSE_URL}'),
]

def url_to_bash(url: str) -> str:
	"""URL을 bash 변수로 역치환"""
	for pattern, replacement in URL_REPLACE_PATTERNS:
		url = re.sub(pattern, replacement, url)
	return url


# ── 요청 body에서 bash 변수 역치환 ────────────────────────────────────────

BODY_REPLACE = [
	# (literal_value, bash_variable)
	('testAE',   '${aeRN}'),
	('testACP',  '${acpRN}'),
	('testCNT',  '${cntRN}'),
	('testCIN',  '${cinRN}'),
	('testGRP',  '${grpRN}'),
	('NMyapp3',  '${APPID}'),
]

def body_to_bash(body: str) -> str:
	"""JSON body 내 리터럴 값을 bash 변수로 역치환"""
	if not body:
		return ''
	for literal, var in BODY_REPLACE:
		body = body.replace(f'"{literal}"', f'"{var}"')
	# CSE_ID 역치환: "/tinyiot" → "${CSE_ID}"
	body = body.replace(f'"/{CSE_RI}"', '"${CSE_ID}"')
	body = body.replace(f'"{CSE_RN}/{{}}"', '"${CSE_RN}/${acpRN}"')
	return body


# ── 응답 body에서 주요 필드 assert 생성 ───────────────────────────────────

def make_assert_from_response(entry: dict, test_name: str) -> list[str]:
	"""응답 body를 파싱해서 assert 구문 생성"""
	lines = []
	rsc = entry.get('rsc', '')
	if not rsc:
		return lines

	# RSC assert
	lines.append(f'assert_eq "$RESP_RSC" "{rsc}" "{test_name}: RSC"')

	# 2001 Created: 주요 필드 not-null 체크
	if rsc == '2001':
		try:
			body = json.loads(entry.get('resp_body', ''))
		except Exception:
			return lines
		# 응답의 최상위 키 (예: m2m:ae, m2m:cnt 등)
		if not body:
			return lines
		root_key = next(iter(body), None)
		if root_key:
			obj = body[root_key]
			for field in ('ri', 'rn', 'ty', 'ct', 'lt', 'et'):
				if field in obj:
					lines.append(
						f'assert_not_null "$(jq_get "$RESP_BODY" "{root_key}/{field}")" '
						f'"{test_name}: {root_key}/{field} not null"'
					)

	return lines


# ── 엔트리 → bash 명령 변환 ────────────────────────────────────────────────

def entry_to_bash_url(entry: dict, resolved_url: str, prev_test: str, ri_map: dict = None) -> list[str]:
	"""URL이 이미 resolve된 경우 사용"""
	lines = []

	test_name = entry.get('test_name', '')
	method    = entry.get('method', 'GET')
	url       = resolved_url
	orig      = entry.get('originator', '')
	ty        = entry.get('ty', '')
	body_raw  = entry.get('req_body', '')
	rvi       = entry.get('rvi', RVI)
	rsc       = entry.get('rsc', '')
	ri_map    = ri_map or {}

	# 테스트 구분선
	if test_name != prev_test:
		lines.append('')
		lines.append('# ' + '-' * 70)
		lines.append(f'# {test_name}')

	# RVI 오버라이드 (서버 기본값과 다를 때만)
	rvi_arg = f' "{rvi}"' if rvi and rvi != RVI else ''

	# body 처리 (ri_map으로 동적 ri도 치환)
	body = body_to_bash_with_ri(body_raw, ri_map)
	if body:
		# 단일 인용부호 내 변수 치환을 위해 double-quote + escape 사용
		body_escaped = body.replace('\\', '\\\\').replace('"', '\\"')
		data_arg = f' "{body_escaped}"'
	else:
		data_arg = ''

	# do_request 호출
	# do_request METHOD URL ORIGINATOR [TY] [DATA] [RVI]
	line = f'do_request {method} "{url}" "{orig}" "{ty}"{data_arg}{rvi_arg}'
	lines.append(line)
	lines.append('RESP_RSC=$RESP_RSC')  # 명시적 대입 (가독성)

	# RSC assert
	if rsc:
		lines.append(f'assert_eq "$RESP_RSC" "{rsc}" "{test_name}"')

	# setup/teardown은 추가 assert 생략
	if test_name not in ('(setup)', '(teardown)'):
		extra = make_assert_from_response(entry, test_name)
		for line in extra[1:]:  # RSC assert는 이미 위에서 추가함
			lines.append(line)

		# 테스트 마지막 요청인지 판별 어려우므로 echo는 생략
		# (같은 테스트 내 여러 요청이 있을 수 있음)

	return lines


# ── .sh 파일 헤더 ─────────────────────────────────────────────────────────

SH_HEADER = '''#!/bin/bash
#
#   {name}.sh
#
#   Auto-generated from {name}_capture.json
#

CSE_HOST="{cse_host}"
CSE_PORT="{cse_port}"
CSE_URL="http://${{CSE_HOST}}:${{CSE_PORT}}"
CSE_RN="{cse_rn}"
CSE_ID="/{cse_ri}"
ORIGINATOR="{originator}"
RVI="{rvi}"
APPID="NMyapp3"
RELEASEVERSION="{rvi}"

# Resource names
aeRN="testAE"
acpRN="testACP"
cntRN="testCNT"
cinRN="testCIN"
grpRN="testGRP"

PASS=0
FAIL=0
RESP_BODY=""
RESP_RSC=""

pass() {{ echo "[PASS] $1"; PASS=$((PASS+1)); }}
fail() {{ echo "[FAIL] $1 (got: $2, expected: $3)"; FAIL=$((FAIL+1)); }}

# HTTP request helper
# Usage: do_request METHOD URL ORIGINATOR [TY] [DATA] [RVI_OVERRIDE]
do_request() {{
    local method="$1" url="$2" orig="$3" ty="$4" data="$5" rvi="${{6:-${{RVI}}}}"
    local ct="application/json"
    [ -n "$ty" ] && ct="application/json;ty=${{ty}}"
    local args=(-s -D /tmp/resp_headers -o /tmp/resp_body)
    args+=(-X "$method")
    args+=(-H "Accept: application/json")
    args+=(-H "Content-Type: ${{ct}}")
    args+=(-H "X-M2M-RVI: ${{rvi}}")
    [ -n "$orig" ] && args+=(-H "X-M2M-Origin: ${{orig}}")
    [ -n "$data" ] && args+=(-d "$data")
    curl "${{args[@]}}" "$url" > /dev/null 2>&1
    RESP_BODY=$(cat /tmp/resp_body)
    RESP_RSC=$(grep -i "x-m2m-rsc:" /tmp/resp_headers | tr -d "\\r" | awk "{{print \\$2}}")
}}

# JSON 필드 추출
jq_get() {{
    local body="$1" path="$2"
    printf "%s" "$body" | python3 -c "
import sys, json
path = sys.argv[1]
try:
    d = json.load(sys.stdin)
    for k in path.split('/'):
        d = d[k]
    v = d
    if isinstance(v, bool): print(str(v).lower())
    elif isinstance(v, (dict, list)): print(json.dumps(v))
    else: print(v)
except: print('')
" "$path"
}}

# assert helpers
assert_eq() {{
    local got="$1" exp="$2" msg="$3"
    if [ "$got" = "$exp" ]; then pass "$msg"; else fail "$msg" "$got" "$exp"; fi
}}
assert_ne() {{
    local got="$1" exp="$2" msg="$3"
    if [ "$got" != "$exp" ]; then pass "$msg"; else fail "$msg (should differ)" "$got" "$exp"; fi
}}
assert_not_null() {{
    local val="$1" msg="$2"
    if [ -n "$val" ] && [ "$val" != "None" ] && [ "$val" != "null" ]; then pass "$msg"; else fail "$msg" "(null)" "(non-null)"; fi
}}
assert_null() {{
    local val="$1" msg="$2"
    if [ -z "$val" ] || [ "$val" = "None" ] || [ "$val" = "null" ]; then pass "$msg"; else fail "$msg" "$val" "(null)"; fi
}}

##########################################################################
'''

SH_FOOTER = '''
##########################################################################
echo ""
echo "Results: PASS=$PASS, FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
'''


# ── 동적 RI 추적: 캡처된 응답에서 ri 값을 추적해서 URL 역치환 ──────────────

def build_ri_map(entries: list) -> dict:
	"""캡처 엔트리를 순서대로 보며 응답 ri를 수집.
	{ri_value: bash_var_name} 반환. 알려진 rn이 없는 ri만 대상으로 함."""
	ri_map = {}  # {ri_literal: varname}
	counter = [0]

	# 알려진 고정 rn 목록 (이미 bash 변수로 처리되는 것들)
	known_rns = {'testAE', 'testACP', 'testCNT', 'testCIN', 'testGRP'}

	for entry in entries:
		rsc = entry.get('rsc', '')
		if rsc != '2001':
			continue
		try:
			body = json.loads(entry.get('resp_body', ''))
		except Exception:
			continue
		if not body:
			continue
		root_key = next(iter(body), None)
		if not root_key:
			continue
		obj = body.get(root_key, {})
		ri = obj.get('ri', '')
		rn = obj.get('rn', '')
		if ri and rn not in known_rns:
			# 동적으로 생성된 ri
			if ri not in ri_map:
				counter[0] += 1
				varname = f'_RI_{counter[0]}'
				ri_map[ri] = varname

	return ri_map


def url_to_bash_with_ri(url: str, ri_map: dict) -> str:
	"""URL을 bash 변수로 역치환 (알려진 URL 패턴 + 동적 ri)"""
	result = url_to_bash(url)
	# 동적 ri 치환: URL에 ri 값이 포함되어 있으면 변수로 대체
	for ri_val, varname in ri_map.items():
		result = result.replace(f'/{ri_val}', f'/${{{varname}}}')
	return result


def body_to_bash_with_ri(body: str, ri_map: dict) -> str:
	"""JSON body 내 동적 ri 값도 bash 변수로 역치환"""
	result = body_to_bash(body)
	for ri_val, varname in ri_map.items():
		# JSON 문자열 값으로 나타나는 ri를 치환 (예: "3-20260223T...")
		result = result.replace(f'"{ri_val}"', f'"${{{varname}}}"')
	return result


# ── 메인 변환 로직 ─────────────────────────────────────────────────────────

def convert(capture_json_path: Path) -> Path:
	with open(capture_json_path, encoding='utf-8') as f:
		entries = json.load(f)

	# 출력 파일명: testAE_capture.json → testAE.sh
	stem = capture_json_path.stem.replace('_capture', '')
	out_path = capture_json_path.parent / f'{stem}.sh'

	# 동적 ri 맵 빌드
	ri_map = build_ri_map(entries)

	lines = []

	# 헤더
	header = SH_HEADER.format(
		name       = stem,
		cse_host   = CSE_HOST,
		cse_port   = CSE_PORT,
		cse_rn     = CSE_RN,
		cse_ri     = CSE_RI,
		originator = ORIGINATOR,
		rvi        = RVI,
	)
	lines.append(header)

	# 엔트리 변환
	prev_test = None
	done_tests = set()

	for entry in entries:
		test_name = entry.get('test_name', '')

		# URL에 동적 ri 적용
		entry_url = url_to_bash_with_ri(entry.get('url', ''), ri_map)

		# 응답이 2001이면 ri 저장 bash 코드 추가
		rsc = entry.get('rsc', '')
		ri_save_lines = []
		if rsc == '2001':
			try:
				body = json.loads(entry.get('resp_body', ''))
				root_key = next(iter(body), None)
				if root_key:
					ri_val = body[root_key].get('ri', '')
					if ri_val and ri_val in ri_map:
						varname = ri_map[ri_val]
						ri_save_lines.append(
							f'{varname}=$(jq_get "$RESP_BODY" "{root_key}/ri")'
						)
			except Exception:
				pass

		bash_lines = entry_to_bash_url(entry, entry_url, prev_test, ri_map)
		lines.extend(bash_lines)
		lines.extend(ri_save_lines)

		# 테스트가 바뀌면 echo done 추가
		if test_name != prev_test and prev_test and prev_test not in ('(setup)', '(teardown)'):
			if prev_test not in done_tests:
				lines.append(f'echo "  done: {prev_test}"')
				done_tests.add(prev_test)

		prev_test = test_name

	# 마지막 테스트 echo done
	if prev_test and prev_test not in ('(setup)', '(teardown)') and prev_test not in done_tests:
		lines.append(f'echo "  done: {prev_test}"')

	# 푸터
	lines.append(SH_FOOTER)

	out_path.write_text('\n'.join(lines), encoding='utf-8')
	out_path.chmod(0o755)
	print(f'  -> Written: {out_path}  ({len(entries)} requests)')
	return out_path


def main():
	args = sys.argv[1:]
	if not args:
		print('Usage: python3 capture_to_sh.py testAE_capture.json [...]')
		print('       python3 capture_to_sh.py --all')
		sys.exit(1)

	tests_dir = Path(__file__).parent

	if '--all' in args:
		files = sorted(tests_dir.glob('*_capture.json'))
	else:
		files = [Path(a) if Path(a).exists() else tests_dir / a for a in args]

	for f in files:
		if not f.exists():
			print(f'[WARN] Not found: {f}')
			continue
		print(f'\nConverting {f.name} ...')
		convert(f)

	print('\nDone.')


if __name__ == '__main__':
	main()
