#!/bin/bash
#
#   testCNT.sh
#
#   Auto-generated from testCNT_capture.json
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
do_request POST "${CSE_URL}/${CSE_RN}" "CAdmin" "1" "{\"m2m:acp\": {\"rn\": \"${acpRN}\", \"pv\": {\"acr\": [{\"acor\": [\"CTester1\", \"CTester2\"], \"acop\": 63}]}, \"pvs\": {\"acr\": [{\"acor\": [\"CAdmin\"], \"acop\": 63}]}}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "(setup)"
do_request POST "${CSE_URL}/${CSE_RN}" "CTester1" "2" "{\"m2m:ae\": {\"rn\": \"${aeRN}\", \"api\": \"${APPID}\", \"rr\": false, \"srv\": [\"3\"], \"acpi\": [\"TinyIoT/testACP\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "(setup)"

# ----------------------------------------------------------------------
# test_createCNT
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "3" "{\"m2m:cnt\": {\"rn\": \"${cntRN}\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_createCNT"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ri")" "test_createCNT: m2m:cnt/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/rn")" "test_createCNT: m2m:cnt/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ty")" "test_createCNT: m2m:cnt/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ct")" "test_createCNT: m2m:cnt/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/lt")" "test_createCNT: m2m:cnt/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/et")" "test_createCNT: m2m:cnt/et not null"

# ----------------------------------------------------------------------
# test_retrieveCNT
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_retrieveCNT"
echo "  done: test_createCNT"

# ----------------------------------------------------------------------
# test_retrieveCNTWithWrongOriginator
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "Cwrong" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4103" "test_retrieveCNTWithWrongOriginator"
echo "  done: test_retrieveCNT"

# ----------------------------------------------------------------------
# test_attributesCNT
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_attributesCNT"
echo "  done: test_retrieveCNTWithWrongOriginator"

# ----------------------------------------------------------------------
# test_updateCNT
do_request PUT "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "" "{\"m2m:cnt\": {\"lbl\": [\"aTag\"], \"mni\": 10, \"mbs\": 9999}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2004" "test_updateCNT"
echo "  done: test_attributesCNT"
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_updateCNT"

# ----------------------------------------------------------------------
# test_updateCNTTy
do_request PUT "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "" "{\"m2m:cnt\": {\"ty\": 5}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_updateCNTTy"
echo "  done: test_updateCNT"

# ----------------------------------------------------------------------
# test_updateCNTempty
do_request PUT "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "" "{\"m2m:cnt\": {}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2004" "test_updateCNTempty"
echo "  done: test_updateCNTTy"

# ----------------------------------------------------------------------
# test_updateCNTPi
do_request PUT "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "" "{\"m2m:cnt\": {\"pi\": \"wrongID\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_updateCNTPi"
echo "  done: test_updateCNTempty"

# ----------------------------------------------------------------------
# test_updateCNTUnknownAttribute
do_request PUT "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "" "{\"m2m:cnt\": {\"unknown\": \"unknown\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_updateCNTUnknownAttribute"
echo "  done: test_updateCNTPi"

# ----------------------------------------------------------------------
# test_updateCNTWrongMNI
do_request PUT "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "" "{\"m2m:cnt\": {\"mni\": -1}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_updateCNTWrongMNI"
echo "  done: test_updateCNTUnknownAttribute"

# ----------------------------------------------------------------------
# test_createCNTUnderCNT
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "3" "{\"m2m:cnt\": {\"rn\": \"${cntRN}\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_createCNTUnderCNT"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ri")" "test_createCNTUnderCNT: m2m:cnt/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/rn")" "test_createCNTUnderCNT: m2m:cnt/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ty")" "test_createCNTUnderCNT: m2m:cnt/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ct")" "test_createCNTUnderCNT: m2m:cnt/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/lt")" "test_createCNTUnderCNT: m2m:cnt/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/et")" "test_createCNTUnderCNT: m2m:cnt/et not null"
echo "  done: test_updateCNTWrongMNI"

# ----------------------------------------------------------------------
# test_retrieveCNTUnderCNT
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}/testCNT" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_retrieveCNTUnderCNT"
echo "  done: test_createCNTUnderCNT"

# ----------------------------------------------------------------------
# test_deleteCNTUnderCNT
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}/testCNT" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "test_deleteCNTUnderCNT"
echo "  done: test_retrieveCNTUnderCNT"

# ----------------------------------------------------------------------
# test_createCNTWithCreatorWrong
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "3" "{\"m2m:cnt\": {\"cr\": \"wrong\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_createCNTWithCreatorWrong"
echo "  done: test_deleteCNTUnderCNT"

# ----------------------------------------------------------------------
# test_createCNTWithCreator
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "3" "{\"m2m:cnt\": {\"cr\": null}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_createCNTWithCreator"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ri")" "test_createCNTWithCreator: m2m:cnt/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/rn")" "test_createCNTWithCreator: m2m:cnt/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ty")" "test_createCNTWithCreator: m2m:cnt/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ct")" "test_createCNTWithCreator: m2m:cnt/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/lt")" "test_createCNTWithCreator: m2m:cnt/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/et")" "test_createCNTWithCreator: m2m:cnt/et not null"
_RI_1=$(jq_get "$RESP_BODY" "m2m:cnt/ri")
echo "  done: test_createCNTWithCreatorWrong"
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${_RI_1}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_createCNTWithCreator"

# ----------------------------------------------------------------------
# test_deleteCNTByUnknownOriginator
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "Cwrong" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4103" "test_deleteCNTByUnknownOriginator"
echo "  done: test_createCNTWithCreator"

# ----------------------------------------------------------------------
# test_deleteCNTByAssignedOriginator
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "test_deleteCNTByAssignedOriginator"
echo "  done: test_deleteCNTByUnknownOriginator"

# ----------------------------------------------------------------------
# test_createCNTUnderCSE
do_request POST "${CSE_URL}/${CSE_RN}" "CAdmin" "3" "{\"m2m:cnt\": {\"rn\": \"${cntRN}\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_createCNTUnderCSE"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ri")" "test_createCNTUnderCSE: m2m:cnt/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/rn")" "test_createCNTUnderCSE: m2m:cnt/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ty")" "test_createCNTUnderCSE: m2m:cnt/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ct")" "test_createCNTUnderCSE: m2m:cnt/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/lt")" "test_createCNTUnderCSE: m2m:cnt/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/et")" "test_createCNTUnderCSE: m2m:cnt/et not null"
echo "  done: test_deleteCNTByAssignedOriginator"

# ----------------------------------------------------------------------
# test_retrieveCNTUnderCSE
do_request GET "${CSE_URL}/${CSE_RN}/testCNT" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_retrieveCNTUnderCSE"
echo "  done: test_createCNTUnderCSE"

# ----------------------------------------------------------------------
# test_deleteCNTUnderCSE
do_request DELETE "${CSE_URL}/${CSE_RN}/testCNT" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "test_deleteCNTUnderCSE"
echo "  done: test_retrieveCNTUnderCSE"

# ----------------------------------------------------------------------
# test_createCNTWithoutOriginator
do_request POST "${CSE_URL}/${CSE_RN}" "" "3" "{\"m2m:cnt\": {\"rn\": \"${cntRN}\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_createCNTWithoutOriginator"
echo "  done: test_deleteCNTUnderCSE"

# ----------------------------------------------------------------------
# test_createCNTwithWrongTypeShortname
do_request POST "${CSE_URL}/${CSE_RN}" "CAdmin" "3" "{\"wrong\": {\"rn\": \"${cntRN}\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_createCNTwithWrongTypeShortname"
echo "  done: test_createCNTWithoutOriginator"

# ----------------------------------------------------------------------
# (teardown)
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "(teardown)"
echo "  done: test_createCNTwithWrongTypeShortname"
do_request DELETE "${CSE_URL}/${CSE_RN}/testCNT" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4004" "(teardown)"
do_request DELETE "${CSE_URL}/${CSE_RN}/${acpRN}" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "(teardown)"

##########################################################################
echo ""
echo "Results: PASS=$PASS, FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
