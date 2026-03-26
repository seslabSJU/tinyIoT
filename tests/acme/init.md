# ACME init.py 포팅 계획

## 목표
ACME-oneM2M-CSE의 테스트 프레임워크(`init.py`)를 tinyIoT CI/CD에서 사용할 수 있도록
`acme` 패키지 의존성 없이 HTTP-only로 재작성한다.

결과물: `tinyIoT/tests/acme/init.py` + `config.py`

---

## 제거 대상 (HTTP-only이므로 불필요)

| 항목 | 이유 |
|------|------|
| `MQTTTopics`, `MQTTClientHandler` class | MQTT 전용 |
| `sendMqttRequest()` | MQTT 전용 |
| `sendWsRequest()` | WebSocket 전용 |
| `sendCoapRequest()` | CoAP 전용 |
| `_packRequest()` | MQTT/WS/CoAP용 request packing (HTTP는 불필요) |
| `coapthon` imports | CoAP 전용 |
| `websockets` imports | WebSocket 전용 |
| `cbor2` import | CBOR 직렬화 (JSON만 사용) |
| `acme.helpers.MQTTConnection` | MQTT 전용 |
| `acme.helpers.CoAPthonTools` | CoAP 전용 |
| `acme.helpers.OAuth` | OAuth 인증 (CI에서 불필요) |
| `acme.etc.RequestUtils` | MQTT/WS용 직렬화만 사용 |
| `acme.etc.DateUtils` | Notification 서버에서만 사용 (직접 대체) |
| `isRaspberrypi()` | 타임스탬프 조정용, 불필요 |
| Remote CSE 관련 URL/변수 | 단일 CSE 테스트만 필요 |

---

## 대체 구현 대상 (acme 모듈 → 직접 정의)

### 1. Type Aliases
```python
# acme.etc.Types에서 가져오던 것들
JSON = dict
Parameters = dict
STRING = str
```

### 2. Enums
```python
# acme.etc.Types.Operation
class Operation(IntEnum):
    CREATE   = 1
    RETRIEVE = 2
    UPDATE   = 3
    DELETE   = 4
    NOTIFY   = 5

# acme.etc.Types.ResourceTypes (테스트에서 쓰는 것만)
class ResourceTypes(IntEnum):
    CSEBase = 5
    AE      = 2
    CNT     = 3
    CIN     = 4
    GRP     = 9
    ACP     = 1
    SUB     = 23
    # 필요시 추가

# acme.etc.Types.ResponseStatusCode (테스트에서 쓰는 것만)
class ResponseStatusCode(IntEnum):
    OK                              = 2000
    CREATED                         = 2001
    DELETED                         = 2002
    UPDATED                         = 2004
    NO_CONTENT                      = 204   # HTTP only
    BAD_REQUEST                     = 4000
    NOT_FOUND                       = 4004
    ORIGINATOR_HAS_NO_PRIVILEGE     = 4103
    CONFLICT                        = 4105
    ORIGINATOR_HAS_ALREADY_REGISTERED = 4106
    INVALID_CHILD_RESOURCE_TYPE     = 4009
    INTERNAL_SERVER_ERROR           = 5000
    # 필요시 추가
```

### 3. HTTP 헤더 상수
```python
# acme.etc.Constants.Constants에서 가져오던 것들
class C:
    hfOrigin = 'X-M2M-Origin'
    hfRI     = 'X-M2M-RI'
    hfRVI    = 'X-M2M-RVI'
    hfRSC    = 'X-M2M-RSC'
    hfOT     = 'X-M2M-OT'
    hfVSI    = 'X-M2M-VSI'
    hfRET    = 'X-M2M-RET'
    hfOET    = 'X-M2M-OET'
    hfRST    = 'X-M2M-RST'
    hfRTU    = 'X-M2M-RTU'
```

### 4. 유틸리티 함수
```python
# DateUtils.getResourceDate() 대체
def getResourceDate() -> str:
    return datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')

# RequestUtils.toHttpUrl() 대체 → url 그대로 반환
```

---

## 유지할 함수 목록

| 함수 | 비고 |
|------|------|
| `RETRIEVE`, `CREATE`, `UPDATE`, `DELETE`, `NOTIFY` | 핵심 요청 함수 |
| `RETRIEVESTRING`, `UPDATESTRING` | 문자열 반환 변형 |
| `sendRequest()` | HTTP 분기만 유지 |
| `sendHttpRequest()` | 핵심, 그대로 유지 |
| `addHttpAuthorizationHeader()` | Basic/Token auth 유지 (OAuth 제거) |
| `findXPath()` | 응답 파싱 핵심 |
| `uniqueID()`, `uniqueRN()` | 리소스 이름 생성 |
| `testCaseStart()`, `testCaseEnd()` | Upper Tester 호출 제거, 로그만 출력 |
| `isTearDownEnabled()` | 그대로 |
| `connectionPossible()` | 서버 연결 확인 |
| `checkUpperTester()` | 비활성화 (항상 skip) |
| `shutdown()` | HTTP session만 닫기 |
| Notification Server 관련 | `SimpleHTTPRequestHandler`, `startNotificationServer()` 등 유지 |
| `testSleep()`, `clearSleepTimeCount()` 등 | 유지 |
| `addTest()`, `addTests()`, `printResult()` | 유지 |
| `setLastHeaders()`, `lastHeaders()` 등 | 유지 |
| `setLastNotification()` 관련 | 유지 |

---

## config.py 변경

환경변수로 포트/호스트 주입 가능하게:
```python
import os
TARGETHOST = os.environ.get('CSE_HOST', 'localhost')
CSEPORT    = int(os.environ.get('CSE_PORT', '3000'))   # tinyIoT 기본 포트
CSERN      = os.environ.get('CSE_RN', 'onem2m')        # tinyIoT 기본 CSE 이름
CSEID      = os.environ.get('CSE_ID', '/id-in')

UPPERTESTERENABLED    = False
RECONFIGURATIONENABLED = False
```

---

## 작업 순서

- [x] 1. `config.py` 작성 (환경변수 주입)
- [x] 2. `init.py` 재작성
  - [x] 2-1. imports 정리 (acme 제거, 표준 라이브러리만)
  - [x] 2-2. 상수/enum 직접 정의 (C, Operation, ResourceTypes, ResponseStatusCode)
  - [x] 2-3. HTTP 요청 함수 유지 (sendHttpRequest, RETRIEVE/CREATE/UPDATE/DELETE)
  - [x] 2-4. MQTT/WS/CoAP 코드 제거
  - [x] 2-5. Notification 서버 유지
  - [x] 2-6. 유틸 함수 유지
- [x] 3. tinyIoT 서버 설정 확인 (CSE RN, ID, 포트) → PORT=3050, RN=TinyIoT, RI=tinyiot
- [x] 4. `test.yml` 에 ACME 테스트 단계 추가

---

## 필요한 pip 패키지 (acmecse 대신)
```
requests
rich
isodate
```
cbor2, websockets, coapthon3 불필요.

---

## 예상 결과물 크기
- 원본 init.py: ~1650줄
- 포팅 후: ~400줄
