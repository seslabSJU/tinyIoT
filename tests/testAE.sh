#!/bin/bash
#
#   testAE.sh
#
#   Auto-generated from testAE_capture.json
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
do_request GET "${CSE_URL}/${CSE_RN}" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "(setup)"
do_request POST "${CSE_URL}/${CSE_RN}" "CAdmin" "1" "{\"m2m:acp\": {\"rn\": \"${acpRN}\", \"pv\": {\"acr\": [{\"acor\": [\"CTester1\", \"CTester2\"], \"acop\": 63}]}, \"pvs\": {\"acr\": [{\"acor\": [\"CAdmin\"], \"acop\": 63}]}}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "(setup)"

# ----------------------------------------------------------------------
# test_createAE
do_request POST "${CSE_URL}/${CSE_RN}" "CTester1" "2" "{\"m2m:ae\": {\"rn\": \"${aeRN}\", \"api\": \"${APPID}\", \"rr\": false, \"srv\": [\"3\"], \"acpi\": [\"TinyIoT/testACP\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_createAE"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/ri")" "test_createAE: m2m:ae/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/rn")" "test_createAE: m2m:ae/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/ty")" "test_createAE: m2m:ae/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/ct")" "test_createAE: m2m:ae/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/lt")" "test_createAE: m2m:ae/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/et")" "test_createAE: m2m:ae/et not null"

# ----------------------------------------------------------------------
# test_createAEUnderAEFail
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester2" "2" "{\"m2m:ae\": {\"rn\": \"testAE1\", \"api\": \"${APPID}\", \"rr\": false, \"srv\": [\"3\"], \"acpi\": [\"TinyIoT/testACP\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4108" "test_createAEUnderAEFail"
echo "  done: test_createAE"

# ----------------------------------------------------------------------
# test_createAEAgainFail
do_request POST "${CSE_URL}/${CSE_RN}" "CTester2" "2" "{\"m2m:ae\": {\"rn\": \"${aeRN}\", \"api\": \"${APPID}\", \"rr\": false, \"srv\": [\"3\"], \"acpi\": [\"TinyIoT/testACP\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4105" "test_createAEAgainFail"
echo "  done: test_createAEUnderAEFail"

# ----------------------------------------------------------------------
# test_createAEWithExistingOriginatorFail
do_request POST "${CSE_URL}/${CSE_RN}" "CTester1" "2" "{\"m2m:ae\": {\"api\": \"${APPID}\", \"rr\": false, \"srv\": [\"3\"], \"acpi\": [\"TinyIoT/testACP\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4117" "test_createAEWithExistingOriginatorFail"
echo "  done: test_createAEAgainFail"

# ----------------------------------------------------------------------
# test_createAECSIoriginatorFail
do_request POST "${CSE_URL}/${CSE_RN}" "tinyiot" "2" "{\"m2m:ae\": {\"api\": \"${APPID}\", \"rr\": false, \"srv\": [\"3\"], \"acpi\": [\"TinyIoT/testACP\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4107" "test_createAECSIoriginatorFail"
echo "  done: test_createAEWithExistingOriginatorFail"

# ----------------------------------------------------------------------
# test_retrieveAE
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_retrieveAE"
echo "  done: test_createAECSIoriginatorFail"

# ----------------------------------------------------------------------
# test_retrieveAEWithWrongOriginator
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}" "Cwrong" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4103" "test_retrieveAEWithWrongOriginator"
echo "  done: test_retrieveAE"

# ----------------------------------------------------------------------
# test_attributesAE
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_attributesAE"
echo "  done: test_retrieveAEWithWrongOriginator"

# ----------------------------------------------------------------------
# test_updateAELbl
do_request PUT "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "" "{\"m2m:ae\": {\"lbl\": [\"aTag\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2004" "test_updateAELbl"
echo "  done: test_attributesAE"
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_updateAELbl"

# ----------------------------------------------------------------------
# test_updateAETy
do_request PUT "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "" "{\"m2m:ae\": {\"ty\": 5}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_updateAETy"
echo "  done: test_updateAELbl"

# ----------------------------------------------------------------------
# test_updateAEPi
do_request PUT "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "" "{\"m2m:ae\": {\"pi\": \"wrongID\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_updateAEPi"
echo "  done: test_updateAETy"

# ----------------------------------------------------------------------
# test_updateAEUnknownAttribute
do_request PUT "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "" "{\"m2m:ae\": {\"unknown\": \"unknown\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_updateAEUnknownAttribute"
echo "  done: test_updateAEPi"

# ----------------------------------------------------------------------
# test_deleteAEByUnknownOriginator
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}" "Cwrong" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4103" "test_deleteAEByUnknownOriginator"
echo "  done: test_updateAEUnknownAttribute"

# ----------------------------------------------------------------------
# test_deleteAEByAssignedOriginator
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "test_deleteAEByAssignedOriginator"
echo "  done: test_deleteAEByUnknownOriginator"

# ----------------------------------------------------------------------
# test_createAENoAPI
do_request POST "${CSE_URL}/${CSE_RN}" "Creg" "2" "{\"m2m:ae\": {\"rn\": \"${aeRN}\", \"rr\": false, \"srv\": [\"3\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_createAENoAPI"
echo "  done: test_deleteAEByAssignedOriginator"

# ----------------------------------------------------------------------
# test_createAEAPIWrongPrefix
do_request POST "${CSE_URL}/${CSE_RN}" "Creg" "2" "{\"m2m:ae\": {\"rn\": \"${aeRN}\", \"api\": \"Xwrong\", \"rr\": false, \"srv\": [\"3\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_createAEAPIWrongPrefix"
echo "  done: test_createAENoAPI"

# ----------------------------------------------------------------------
# test_createAEAPICorrectR
do_request POST "${CSE_URL}/${CSE_RN}" "CTester3" "2" "{\"m2m:ae\": {\"rn\": \"${aeRN}\", \"api\": \"Rabc.com.example.acme\", \"rr\": false, \"srv\": [\"3\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_createAEAPICorrectR"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/ri")" "test_createAEAPICorrectR: m2m:ae/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/rn")" "test_createAEAPICorrectR: m2m:ae/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/ty")" "test_createAEAPICorrectR: m2m:ae/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/ct")" "test_createAEAPICorrectR: m2m:ae/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/lt")" "test_createAEAPICorrectR: m2m:ae/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/et")" "test_createAEAPICorrectR: m2m:ae/et not null"
echo "  done: test_createAEAPIWrongPrefix"
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "test_createAEAPICorrectR"

# ----------------------------------------------------------------------
# test_createAEAPICorrectN
do_request POST "${CSE_URL}/${CSE_RN}" "CTester4" "2" "{\"m2m:ae\": {\"rn\": \"${aeRN}\", \"api\": \"Nacme\", \"rr\": false, \"srv\": [\"3\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_createAEAPICorrectN"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/ri")" "test_createAEAPICorrectN: m2m:ae/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/rn")" "test_createAEAPICorrectN: m2m:ae/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/ty")" "test_createAEAPICorrectN: m2m:ae/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/ct")" "test_createAEAPICorrectN: m2m:ae/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/lt")" "test_createAEAPICorrectN: m2m:ae/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/et")" "test_createAEAPICorrectN: m2m:ae/et not null"
echo "  done: test_createAEAPICorrectR"
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "test_createAEAPICorrectN"

# ----------------------------------------------------------------------
# test_createAEAPIRVI3LowerCaseR
do_request POST "${CSE_URL}/${CSE_RN}" "CTester5" "2" "{\"m2m:ae\": {\"rn\": \"${aeRN}\", \"api\": \"racme\", \"rr\": false, \"srv\": [\"3\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_createAEAPIRVI3LowerCaseR"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/ri")" "test_createAEAPIRVI3LowerCaseR: m2m:ae/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/rn")" "test_createAEAPIRVI3LowerCaseR: m2m:ae/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/ty")" "test_createAEAPIRVI3LowerCaseR: m2m:ae/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/ct")" "test_createAEAPIRVI3LowerCaseR: m2m:ae/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/lt")" "test_createAEAPIRVI3LowerCaseR: m2m:ae/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:ae/et")" "test_createAEAPIRVI3LowerCaseR: m2m:ae/et not null"
echo "  done: test_createAEAPICorrectN"
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "test_createAEAPIRVI3LowerCaseR"

# ----------------------------------------------------------------------
# test_createAEInvalidRNFail
do_request POST "${CSE_URL}/${CSE_RN}" "" "2" "{\"m2m:ae\": {\"rn\": \"test?\", \"api\": \"Nacme\", \"rr\": false, \"srv\": [\"3\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_createAEInvalidRNFail"
echo "  done: test_createAEAPIRVI3LowerCaseR"
do_request POST "${CSE_URL}/${CSE_RN}" "" "2" "{\"m2m:ae\": {\"rn\": \"test wrong\", \"api\": \"Nacme\", \"rr\": false, \"srv\": [\"3\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_createAEInvalidRNFail"

# ----------------------------------------------------------------------
# test_createAEWithCreatorFail
do_request POST "${CSE_URL}/${CSE_RN}" "" "2" "{\"m2m:ae\": {\"api\": \"Nacme\", \"rr\": false, \"srv\": [\"3\"], \"cr\": null}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_createAEWithCreatorFail"
echo "  done: test_createAEInvalidRNFail"

# ----------------------------------------------------------------------
# (teardown)
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4004" "(teardown)"
echo "  done: test_createAEWithCreatorFail"
do_request DELETE "${CSE_URL}/${CSE_RN}/${acpRN}" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "(teardown)"

##########################################################################
echo ""
echo "Results: PASS=$PASS, FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
