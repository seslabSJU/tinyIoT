#!/usr/bin/env python3
#
#	run_capture.py
#
#	testXXX.py를 실행하면서 HTTP 통신을 캡처해서 JSON으로 저장.
#
#	Usage:
#	    python3 run_capture.py testAE
#	    python3 run_capture.py testAE testCNT testCIN ...
#	    python3 run_capture.py --all
#

import sys
import json
import unittest
import importlib
from pathlib import Path

# capture_http를 가장 먼저 import해서 monkey-patch 적용
import capture_http
from capture_http import CAPTURE_LOG, _current_test

# testXXX.py가 'from acme.etc.Types import ...' 로 import하는데
# 실제 패키지명은 acmecse 이므로 sys.modules에 acme → acmecse 별칭 등록
import sys as _sys
try:
	import acmecse as _acmecse
	_sys.modules['acme'] = _acmecse
	# 서브모듈도 등록
	import importlib as _importlib
	for _sub in ('etc', 'etc.Types', 'etc.DateUtils'):
		try:
			_mod = _importlib.import_module(f'acmecse.{_sub}')
			_sys.modules[f'acme.{_sub}'] = _mod
		except ImportError:
			pass
except ImportError:
	pass


class CapturingTestResult(unittest.TestResult):
	"""테스트 실행 중 현재 테스트 이름을 capture_http에 알려줌"""

	@staticmethod
	def _name(test):
		return getattr(test, '_testMethodName', str(test))

	def startTest(self, test):
		_current_test[0] = self._name(test)
		super().startTest(test)

	def stopTest(self, test):
		_current_test[0] = '(teardown)'
		super().stopTest(test)

	def addSuccess(self, test):
		print(f'  [OK]   {self._name(test)}')

	def addError(self, test, err):
		print(f'  [ERR]  {self._name(test)}: {err[1]}')

	def addFailure(self, test, err):
		print(f'  [FAIL] {self._name(test)}: {err[1]}')

	def addSkip(self, test, reason):
		print(f'  [SKIP] {self._name(test)}: {reason}')


def run_test_module(module_name: str) -> str:
	"""테스트 모듈 실행 후 캡처 JSON 파일 경로 반환"""
	print(f'\n=== Capturing: {module_name} ===')

	# 이전 캡처 초기화
	CAPTURE_LOG.clear()
	_current_test[0] = '(setup)'

	# 모듈 재로드 (이전 import 캐시 무시)
	if module_name in _sys.modules:
		del _sys.modules[module_name]

	# 모듈 로드
	mod = importlib.import_module(module_name)

	# 모듈에 run() 함수가 있으면 해당 순서로 실행, 없으면 loadTestsFromModule
	if hasattr(mod, 'run'):
		# run() 함수가 내부적으로 suite를 구성하고 TextTestRunner로 실행함.
		# TextTestRunner 대신 CapturingTestResult를 사용하도록 unittest.TextTestRunner를 patch.
		import unittest as _ut
		_orig_runner = _ut.TextTestRunner

		class _CapturingRunner:
			def __init__(self, *a, **kw):
				pass
			def run(self, suite):
				result = CapturingTestResult()
				_current_test[0] = '(setup)'
				suite.run(result)
				return result

		_ut.TextTestRunner = _CapturingRunner
		try:
			mod.run(False)
		finally:
			_ut.TextTestRunner = _orig_runner
	else:
		suite = unittest.TestLoader().loadTestsFromModule(mod)
		result = CapturingTestResult()
		_current_test[0] = '(setup)'
		suite.run(result)

	# 캡처 결과 저장
	out_path = Path(__file__).parent / f'{module_name}_capture.json'
	with open(out_path, 'w', encoding='utf-8') as f:
		json.dump(CAPTURE_LOG, f, indent=2, ensure_ascii=False)

	print(f'  -> Captured {len(CAPTURE_LOG)} requests to {out_path}')
	return str(out_path)


def main():
	test_dir = Path(__file__).parent
	sys.path.insert(0, str(test_dir))

	args = sys.argv[1:]
	if not args:
		print('Usage: python3 run_capture.py testAE [testCNT ...]')
		print('       python3 run_capture.py --all')
		sys.exit(1)

	if '--all' in args:
		modules = [p.stem for p in sorted(test_dir.glob('test*.py'))
		           if p.stem not in ('testCSE',)]  # testCSE.sh는 수동 작성
	else:
		modules = [a.replace('.py', '') for a in args]

	captured_files = []
	for mod_name in modules:
		try:
			path = run_test_module(mod_name)
			captured_files.append(path)
		except Exception as e:
			print(f'[ERROR] {mod_name}: {e}')

	print(f'\nDone. Captured files:')
	for f in captured_files:
		print(f'  {f}')


if __name__ == '__main__':
	main()
