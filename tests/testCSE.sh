#!/bin/bash
#
#   testCSE.sh
#
#   Auto-generated from testCSE_capture.json
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
do_request GET "${CSE_URL}/${CSE_RN}" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "(setup)"

# ----------------------------------------------------------------------
# test_retrieveCSE
do_request GET "${CSE_URL}/${CSE_RN}" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_retrieveCSE"

# ----------------------------------------------------------------------
# test_retrieveCSEWithWrongOriginator
do_request GET "${CSE_URL}/${CSE_RN}" "CWrong" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4103" "test_retrieveCSEWithWrongOriginator"
echo "  done: test_retrieveCSE"

# ----------------------------------------------------------------------
# test_attributesCSE
do_request GET "${CSE_URL}/${CSE_RN}" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_attributesCSE"
echo "  done: test_retrieveCSEWithWrongOriginator"

# ----------------------------------------------------------------------
# test_CSESupportedResourceTypes
do_request GET "${CSE_URL}/${CSE_RN}" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_CSESupportedResourceTypes"
echo "  done: test_attributesCSE"

# ----------------------------------------------------------------------
# test_deleteCSEFail
do_request DELETE "${CSE_URL}/${CSE_RN}" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4005" "test_deleteCSEFail"
echo "  done: test_CSESupportedResourceTypes"

# ----------------------------------------------------------------------
# test_updateCSEFail
do_request PUT "${CSE_URL}/${CSE_RN}" "CAdmin" "" "{\"m2m:cse\": {\"lbl\": [\"aTag\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4005" "test_updateCSEFail"
echo "  done: test_deleteCSEFail"

# ----------------------------------------------------------------------
# test_CSEreleaseVersion
do_request GET "${CSE_URL}/${CSE_RN}" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_CSEreleaseVersion"
echo "  done: test_updateCSEFail"
echo "  done: test_CSEreleaseVersion"

##########################################################################
echo ""
echo "Results: PASS=$PASS, FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
