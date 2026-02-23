#!/bin/bash
#
#   testCNT_CIN.sh
#
#   Auto-generated from testCNT_CIN_capture.json
#

CSE_HOST="localhost"
CSE_PORT="3000"
CSE_URL="http://${CSE_HOST}:${CSE_PORT}"
CSE_RN="TinyIoT"
CSE_ID="/tinyiot"
ORIGINATOR="CAdmin"
RVI="2a"
APPID="NMyapp3"
RELEASEVERSION="2a"

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

pass() { echo "[PASS] $1"; PASS=$((PASS+1)); }
fail() { echo "[FAIL] $1 (got: $2, expected: $3)"; FAIL=$((FAIL+1)); }

# HTTP request helper
# Usage: do_request METHOD URL ORIGINATOR [TY] [DATA] [RVI_OVERRIDE]
do_request() {
    local method="$1" url="$2" orig="$3" ty="$4" data="$5" rvi="${6:-${RVI}}"
    local ct="application/json"
    [ -n "$ty" ] && ct="application/json;ty=${ty}"
    local args=(-s -D /tmp/resp_headers -o /tmp/resp_body)
    args+=(-X "$method")
    args+=(-H "Accept: application/json")
    args+=(-H "Content-Type: ${ct}")
    args+=(-H "X-M2M-RVI: ${rvi}")
    [ -n "$orig" ] && args+=(-H "X-M2M-Origin: ${orig}")
    [ -n "$data" ] && args+=(-d "$data")
    curl "${args[@]}" "$url" > /dev/null 2>&1
    RESP_BODY=$(cat /tmp/resp_body)
    RESP_RSC=$(grep -i "x-m2m-rsc:" /tmp/resp_headers | tr -d "\r" | awk "{print \$2}")
}

# JSON 필드 추출
jq_get() {
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
}

# assert helpers
assert_eq() {
    local got="$1" exp="$2" msg="$3"
    if [ "$got" = "$exp" ]; then pass "$msg"; else fail "$msg" "$got" "$exp"; fi
}
assert_ne() {
    local got="$1" exp="$2" msg="$3"
    if [ "$got" != "$exp" ]; then pass "$msg"; else fail "$msg (should differ)" "$got" "$exp"; fi
}
assert_not_null() {
    local val="$1" msg="$2"
    if [ -n "$val" ] && [ "$val" != "None" ] && [ "$val" != "null" ]; then pass "$msg"; else fail "$msg" "(null)" "(non-null)"; fi
}
assert_null() {
    local val="$1" msg="$2"
    if [ -z "$val" ] || [ "$val" = "None" ] || [ "$val" = "null" ]; then pass "$msg"; else fail "$msg" "$val" "(null)"; fi
}

##########################################################################


# ----------------------------------------------------------------------
# (setup)
do_request POST "${CSE_URL}/${CSE_RN}" "CAdmin" "1" "{\"m2m:acp\": {\"rn\": \"${acpRN}\", \"pv\": {\"acr\": [{\"acor\": [\"CTester1\", \"CTester2\"], \"acop\": 63}]}, \"pvs\": {\"acr\": [{\"acor\": [\"CAdmin\"], \"acop\": 63}]}}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "(setup)"
do_request POST "${CSE_URL}/${CSE_RN}" "CTester1" "2" "{\"m2m:ae\": {\"rn\": \"${aeRN}\", \"api\": \"${APPID}\", \"rr\": false, \"srv\": [\"3\"], \"acpi\": [\"TinyIoT/testACP\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "(setup)"
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "3" "{\"m2m:cnt\": {\"rn\": \"${cntRN}\", \"mni\": 3}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "(setup)"

##########################################################################
echo ""
echo "Results: PASS=$PASS, FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
