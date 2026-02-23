#!/bin/bash
#
#   testGRP.sh
#
#   Auto-generated from testGRP_capture.json
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
do_request POST "${CSE_URL}/${CSE_RN}" "CTester1" "2" "{\"m2m:ae\": {\"rn\": \"${aeRN}\", \"api\": \"${APPID}\", \"rr\": true, \"srv\": [\"3\"], \"acpi\": [\"TinyIoT/testACP\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "(setup)"
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "3" "{\"m2m:cnt\": {\"rn\": \"${cntRN}\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "(setup)"
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "3" "{\"m2m:cnt\": {\"rn\": \"testCNT2\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "(setup)"
_RI_1=$(jq_get "$RESP_BODY" "m2m:cnt/ri")

# ----------------------------------------------------------------------
# test_createGRP
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "9" "{\"m2m:grp\": {\"rn\": \"${grpRN}\", \"mt\": 0, \"mnm\": 10, \"mid\": [\"3-20260223T1204280157\", \"${_RI_1}\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_createGRP"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/ri")" "test_createGRP: m2m:grp/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/rn")" "test_createGRP: m2m:grp/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/ty")" "test_createGRP: m2m:grp/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/ct")" "test_createGRP: m2m:grp/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/lt")" "test_createGRP: m2m:grp/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/et")" "test_createGRP: m2m:grp/et not null"

# ----------------------------------------------------------------------
# test_retrieveGRP
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_retrieveGRP"
echo "  done: test_createGRP"

# ----------------------------------------------------------------------
# test_retrieveGRPWithWrongOriginator
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "Cwrong" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4103" "test_retrieveGRPWithWrongOriginator"
echo "  done: test_retrieveGRP"

# ----------------------------------------------------------------------
# test_attributesGRP
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_attributesGRP"
echo "  done: test_retrieveGRPWithWrongOriginator"

# ----------------------------------------------------------------------
# test_updateGRP
do_request PUT "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "CTester1" "" "{\"m2m:grp\": {\"mnm\": 15}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2004" "test_updateGRP"
echo "  done: test_attributesGRP"

# ----------------------------------------------------------------------
# test_updateGRPwithCNT
do_request PUT "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "CTester1" "" "{\"m2m:cnt\": {\"lbl\": [\"wrong\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_updateGRPwithCNT"
echo "  done: test_updateGRP"

# ----------------------------------------------------------------------
# test_addCNTtoGRP
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_addCNTtoGRP"
echo "  done: test_updateGRPwithCNT"
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "3" "{\"m2m:cnt\": {\"rn\": \"testCNT3\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_addCNTtoGRP"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ri")" "test_addCNTtoGRP: m2m:cnt/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/rn")" "test_addCNTtoGRP: m2m:cnt/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ty")" "test_addCNTtoGRP: m2m:cnt/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ct")" "test_addCNTtoGRP: m2m:cnt/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/lt")" "test_addCNTtoGRP: m2m:cnt/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/et")" "test_addCNTtoGRP: m2m:cnt/et not null"
_RI_2=$(jq_get "$RESP_BODY" "m2m:cnt/ri")
do_request PUT "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "CTester1" "" "{\"m2m:grp\": {\"mid\": [\"TinyIoT/testAE/testCNT\", \"TinyIoT/testAE/testCNT2\", \"${_RI_2}\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2004" "test_addCNTtoGRP"

# ----------------------------------------------------------------------
# test_addCINviaFOPT
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}/fopt" "CTester1" "4" "{\"m2m:cin\": {\"cnf\": \"text/plain:0\", \"con\": \"aValue\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_addCINviaFOPT"
echo "  done: test_addCNTtoGRP"
do_request GET "${CSE_URL}/${CSE_RN}4-20260223T1204280161" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4004" "test_addCINviaFOPT"

# ----------------------------------------------------------------------
# test_retrieveLAviaFOPT
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}/fopt/la" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_retrieveLAviaFOPT"
echo "  done: test_addCINviaFOPT"
do_request GET "${CSE_URL}/${CSE_RN}4-20260223T1204280161" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4004" "test_retrieveLAviaFOPT"

# ----------------------------------------------------------------------
# test_updateCNTviaFOPT
do_request PUT "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}/fopt" "CTester1" "" "{\"m2m:cnt\": {\"lbl\": [\"aTag\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_updateCNTviaFOPT"
echo "  done: test_retrieveLAviaFOPT"
do_request GET "${CSE_URL}/${CSE_RN}3-20260223T1204280157" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4004" "test_updateCNTviaFOPT"

# ----------------------------------------------------------------------
# test_addExistingCNTtoGRP
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_addExistingCNTtoGRP"
echo "  done: test_updateCNTviaFOPT"
do_request PUT "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "CTester1" "" "{\"m2m:grp\": {\"mid\": [\"TinyIoT/testAE/testCNT\", \"TinyIoT/testAE/testCNT2\", \"TinyIoT/testAE/testCNT3\", \"3-20260223T1204280157\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2004" "test_addExistingCNTtoGRP"

# ----------------------------------------------------------------------
# test_deleteCNTviaFOPT
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}/fopt" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_deleteCNTviaFOPT"
echo "  done: test_addExistingCNTtoGRP"

# ----------------------------------------------------------------------
# test_deleteGRPByUnknownOriginator
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "Cwrong" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4103" "test_deleteGRPByUnknownOriginator"
echo "  done: test_deleteCNTviaFOPT"

# ----------------------------------------------------------------------
# test_deleteGRPByAssignedOriginator
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "test_deleteGRPByAssignedOriginator"
echo "  done: test_deleteGRPByUnknownOriginator"

# ----------------------------------------------------------------------
# test_createGRP2
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "3" "{\"m2m:cnt\": {\"rn\": \"${cntRN}\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_createGRP2"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ri")" "test_createGRP2: m2m:cnt/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/rn")" "test_createGRP2: m2m:cnt/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ty")" "test_createGRP2: m2m:cnt/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ct")" "test_createGRP2: m2m:cnt/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/lt")" "test_createGRP2: m2m:cnt/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/et")" "test_createGRP2: m2m:cnt/et not null"
echo "  done: test_deleteGRPByAssignedOriginator"
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "3" "{\"m2m:cnt\": {\"rn\": \"testCNT2\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_createGRP2"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ri")" "test_createGRP2: m2m:cnt/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/rn")" "test_createGRP2: m2m:cnt/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ty")" "test_createGRP2: m2m:cnt/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ct")" "test_createGRP2: m2m:cnt/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/lt")" "test_createGRP2: m2m:cnt/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/et")" "test_createGRP2: m2m:cnt/et not null"
_RI_3=$(jq_get "$RESP_BODY" "m2m:cnt/ri")
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "9" "{\"m2m:grp\": {\"rn\": \"${grpRN}\", \"mt\": 0, \"mnm\": 2, \"mid\": [\"3-20260223T1204280164\", \"${_RI_3}\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_createGRP2"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/ri")" "test_createGRP2: m2m:grp/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/rn")" "test_createGRP2: m2m:grp/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/ty")" "test_createGRP2: m2m:grp/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/ct")" "test_createGRP2: m2m:grp/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/lt")" "test_createGRP2: m2m:grp/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/et")" "test_createGRP2: m2m:grp/et not null"

# ----------------------------------------------------------------------
# test_addTooManyCNTToGRP2
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "3" "{\"m2m:cnt\": {\"rn\": \"testCNT3\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_addTooManyCNTToGRP2"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ri")" "test_addTooManyCNTToGRP2: m2m:cnt/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/rn")" "test_addTooManyCNTToGRP2: m2m:cnt/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ty")" "test_addTooManyCNTToGRP2: m2m:cnt/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ct")" "test_addTooManyCNTToGRP2: m2m:cnt/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/lt")" "test_addTooManyCNTToGRP2: m2m:cnt/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/et")" "test_addTooManyCNTToGRP2: m2m:cnt/et not null"
_RI_4=$(jq_get "$RESP_BODY" "m2m:cnt/ri")
echo "  done: test_createGRP2"
do_request PUT "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "CTester1" "" "{\"m2m:grp\": {\"mid\": [\"3-20260223T1204280164\", \"${_RI_3}\", \"${_RI_4}\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "6010" "test_addTooManyCNTToGRP2"

# ----------------------------------------------------------------------
# test_attributesGRP2
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_attributesGRP2"
echo "  done: test_addTooManyCNTToGRP2"

# ----------------------------------------------------------------------
# test_createGRPWithCreatorWrong
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "9" "{\"m2m:grp\": {\"mnm\": 10, \"mid\": [], \"cr\": \"wrong\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "4000" "test_createGRPWithCreatorWrong"
echo "  done: test_attributesGRP2"

# ----------------------------------------------------------------------
# test_createGRPWithCreator
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "9" "{\"m2m:grp\": {\"mnm\": 10, \"mid\": [], \"cr\": null}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_createGRPWithCreator"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/ri")" "test_createGRPWithCreator: m2m:grp/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/rn")" "test_createGRPWithCreator: m2m:grp/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/ty")" "test_createGRPWithCreator: m2m:grp/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/ct")" "test_createGRPWithCreator: m2m:grp/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/lt")" "test_createGRPWithCreator: m2m:grp/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/et")" "test_createGRPWithCreator: m2m:grp/et not null"
_RI_5=$(jq_get "$RESP_BODY" "m2m:grp/ri")
echo "  done: test_createGRPWithCreatorWrong"
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${_RI_5}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_createGRPWithCreator"

# ----------------------------------------------------------------------
# test_deleteGRPByAssignedOriginator
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "test_deleteGRPByAssignedOriginator"
echo "  done: test_createGRPWithCreator"

# ----------------------------------------------------------------------
# test_createGRP
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "9" "{\"m2m:grp\": {\"rn\": \"${grpRN}\", \"mt\": 0, \"mnm\": 10, \"mid\": [\"3-20260223T1204280164\", \"${_RI_3}\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_createGRP"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/ri")" "test_createGRP: m2m:grp/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/rn")" "test_createGRP: m2m:grp/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/ty")" "test_createGRP: m2m:grp/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/ct")" "test_createGRP: m2m:grp/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/lt")" "test_createGRP: m2m:grp/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/et")" "test_createGRP: m2m:grp/et not null"

# ----------------------------------------------------------------------
# test_addDeleteContainerCheckMID
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_addDeleteContainerCheckMID"
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "3" "{\"m2m:cnt\": {\"rn\": \"testCNT4\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_addDeleteContainerCheckMID"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ri")" "test_addDeleteContainerCheckMID: m2m:cnt/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/rn")" "test_addDeleteContainerCheckMID: m2m:cnt/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ty")" "test_addDeleteContainerCheckMID: m2m:cnt/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/ct")" "test_addDeleteContainerCheckMID: m2m:cnt/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/lt")" "test_addDeleteContainerCheckMID: m2m:cnt/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:cnt/et")" "test_addDeleteContainerCheckMID: m2m:cnt/et not null"
_RI_6=$(jq_get "$RESP_BODY" "m2m:cnt/ri")
do_request PUT "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "CTester1" "" "{\"m2m:grp\": {\"mid\": [\"TinyIoT/testAE/testCNT\", \"TinyIoT/testAE/testCNT2\", \"${_RI_6}\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2004" "test_addDeleteContainerCheckMID"
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}/${cntRN}4" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "test_addDeleteContainerCheckMID"
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_addDeleteContainerCheckMID"

# ----------------------------------------------------------------------
# test_deleteGRPByAssignedOriginator
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "test_deleteGRPByAssignedOriginator"
echo "  done: test_addDeleteContainerCheckMID"

# ----------------------------------------------------------------------
# test_createGRP
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}" "CTester1" "9" "{\"m2m:grp\": {\"rn\": \"${grpRN}\", \"mt\": 0, \"mnm\": 10, \"mid\": [\"3-20260223T1204280164\", \"${_RI_3}\"]}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2001" "test_createGRP"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/ri")" "test_createGRP: m2m:grp/ri not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/rn")" "test_createGRP: m2m:grp/rn not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/ty")" "test_createGRP: m2m:grp/ty not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/ct")" "test_createGRP: m2m:grp/ct not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/lt")" "test_createGRP: m2m:grp/lt not null"
assert_not_null "$(jq_get "$RESP_BODY" "m2m:grp/et")" "test_createGRP: m2m:grp/et not null"

# ----------------------------------------------------------------------
# test_createCNTviaFopt
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}/fopt" "CTester1" "3" "{\"m2m:cnt\": {\"rn\": \"container\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_createCNTviaFopt"

# ----------------------------------------------------------------------
# test_retrieveCNTviaFopt
do_request GET "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}/fopt/container" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_retrieveCNTviaFopt"
echo "  done: test_createCNTviaFopt"

# ----------------------------------------------------------------------
# test_createCNTCNTviaFopt
do_request POST "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}/fopt/container" "CTester1" "3" "{\"m2m:cnt\": {\"rn\": \"container\"}}" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2000" "test_createCNTCNTviaFopt"
echo "  done: test_retrieveCNTviaFopt"

# ----------------------------------------------------------------------
# test_deleteGRPByAssignedOriginator
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}/${grpRN}" "CTester1" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "test_deleteGRPByAssignedOriginator"
echo "  done: test_createCNTCNTviaFopt"

# ----------------------------------------------------------------------
# (teardown)
do_request DELETE "${CSE_URL}/${CSE_RN}/${aeRN}" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "(teardown)"
do_request DELETE "${CSE_URL}/${CSE_RN}/${acpRN}" "CAdmin" "" "3"
RESP_RSC=$RESP_RSC
assert_eq "$RESP_RSC" "2002" "(teardown)"

##########################################################################
echo ""
echo "Results: PASS=$PASS, FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
