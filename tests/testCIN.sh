#!/bin/bash
#
#   testCIN.sh
#
#   Auto-generated from testCIN_capture.json
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
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "3" "{\"m2m:cnt\": {\"rn\": \"${cntRN}\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "(setup)"

# ----------------------------------------------------------------------
# test_createCIN
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "4" "{\"m2m:cin\": {\"rn\": \"${cinRN}\", \"cnf\": \"text/plain:0\", \"con\": \"AnyValue\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_createCIN"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cin/ri")" "test_createCIN: m2m:cin/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cin/rn")" "test_createCIN: m2m:cin/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cin/ty")" "test_createCIN: m2m:cin/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cin/ct")" "test_createCIN: m2m:cin/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cin/lt")" "test_createCIN: m2m:cin/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cin/et")" "test_createCIN: m2m:cin/et not null"

# ----------------------------------------------------------------------
# test_retrieveCIN
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}/${cinRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_retrieveCIN"
echo "  done: test_createCIN"

# ----------------------------------------------------------------------
# test_attributesCIN
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}/${cinRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_attributesCIN"
echo "  done: test_retrieveCIN"

# ----------------------------------------------------------------------
# test_updateCINFail
do_request PUT "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}/${cinRN}" "CTester1" "" "{\"m2m:cin\": {\"con\": \"NewValue\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4005" "test_updateCINFail"
echo "  done: test_attributesCIN"

# ----------------------------------------------------------------------
# test_createCINUnderAE
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "4" "{\"m2m:cin\": {\"rn\": \"${cinRN}\", \"cnf\": \"text/plain:0\", \"con\": \"AnyValue\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4108" "test_createCINUnderAE"
echo "  done: test_updateCINFail"

# ----------------------------------------------------------------------
# test_createCINwithString
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "4" "{\"m2m:cin\": {\"con\": \"AnyValue\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_createCINwithString"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cin/ri")" "test_createCINwithString: m2m:cin/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cin/rn")" "test_createCINwithString: m2m:cin/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cin/ty")" "test_createCINwithString: m2m:cin/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cin/ct")" "test_createCINwithString: m2m:cin/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cin/lt")" "test_createCINwithString: m2m:cin/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cin/et")" "test_createCINwithString: m2m:cin/et not null"
_RI_1=$(jq_get "$RESP_BODY" "m2m:cin/ri")
echo "  done: test_createCINUnderAE"

# ----------------------------------------------------------------------
# test_deleteCIN
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}/${cinRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "test_deleteCIN"
echo "  done: test_createCINwithString"

# ----------------------------------------------------------------------
# test_createCINWithCreatorWrong
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "4" "{\"m2m:cin\": {\"cr\": \"wrong\", \"con\": \"AnyValue\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_createCINWithCreatorWrong"
echo "  done: test_deleteCIN"

# ----------------------------------------------------------------------
# test_createCINWithCnfWrong1
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "4" "{\"m2m:cin\": {\"cnf\": \"text\", \"con\": \"AnyValue\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_createCINWithCnfWrong1"
echo "  done: test_createCINWithCreatorWrong"

# ----------------------------------------------------------------------
# test_createCINWithCnfWrong2
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "4" "{\"m2m:cin\": {\"cnf\": \"text:0\", \"con\": \"AnyValue\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_createCINWithCnfWrong2"
echo "  done: test_createCINWithCnfWrong1"

# ----------------------------------------------------------------------
# test_createCINWithCnfWrong3
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "4" "{\"m2m:cin\": {\"cnf\": \"text/plain\", \"con\": \"AnyValue\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_createCINWithCnfWrong3"
echo "  done: test_createCINWithCnfWrong2"

# ----------------------------------------------------------------------
# test_createCINWithCnfWrong4
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "4" "{\"m2m:cin\": {\"cnf\": \"text/plain:0:0:0\", \"con\": \"AnyValue\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_createCINWithCnfWrong4"
echo "  done: test_createCINWithCnfWrong3"

# ----------------------------------------------------------------------
# test_createCINWithCnfWrong5
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "4" "{\"m2m:cin\": {\"cnf\": \"text/plain:9\", \"con\": \"AnyValue\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_createCINWithCnfWrong5"
echo "  done: test_createCINWithCnfWrong4"

# ----------------------------------------------------------------------
# test_createCINWithCreator
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "4" "{\"m2m:cin\": {\"con\": \"AnyValue\", \"cr\": null}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_createCINWithCreator"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cin/ri")" "test_createCINWithCreator: m2m:cin/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cin/rn")" "test_createCINWithCreator: m2m:cin/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cin/ty")" "test_createCINWithCreator: m2m:cin/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cin/ct")" "test_createCINWithCreator: m2m:cin/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cin/lt")" "test_createCINWithCreator: m2m:cin/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cin/et")" "test_createCINWithCreator: m2m:cin/et not null"
_RI_2=$(jq_get "$RESP_BODY" "m2m:cin/ri")
echo "  done: test_createCINWithCnfWrong5"
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}/${_RI_2}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_createCINWithCreator"

# ----------------------------------------------------------------------
# test_createCINwithAcpi
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "4" "{\"m2m:cin\": {\"rn\": \"dcntTest\", \"con\": \"AnyValue\", \"acpi\": [\"someACP\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_createCINwithAcpi"
echo "  done: test_createCINWithCreator"

# ----------------------------------------------------------------------
# (teardown)
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "(teardown)"
echo "  done: test_createCINwithAcpi"
do_request DELETE "${CSE_URL}/${CSE_RN}/${acpRN}" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "(teardown)"

##########################################################################
echo ""
echo "Results: PASS=$PASS, FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
