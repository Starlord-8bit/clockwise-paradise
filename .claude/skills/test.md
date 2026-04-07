---
description: Run native unit tests via PlatformIO Unity and report pass/fail results with file references. Use /test to execute the test suite without needing hardware.
---

Run the Clockwise Paradise native unit test suite.

## Steps

1. Run native tests:
```bash
cd firmware && pio test -e native 2>&1
```

2. Parse Unity test output:
   - Extract each test result: `[PASS]`, `[FAIL]`, `[IGNORE]`
   - For failures: show `[file:line](file:line) — assertion message`
   - Count totals: N ran, N passed, N failed, N ignored

3. Report results:

**If all pass:**
```
Tests: N/N passed
Suite: [test file name]
```

**If any fail:**
```
Tests: N passed, N FAILED, N ignored

FAILURES:
- [TestSuite::test_name](firmware/test/test_native/file.cpp#Lxx) — expected X was Y
- ...

Root cause analysis: [if multiple failures share a root cause, note it]
```

4. If the test binary fails to compile: treat as a build failure — run `/build` first and fix
   compilation errors before re-running tests.

5. If `firmware/test/test_native/CMakeLists.txt` does not exist, report:
   > No native test suite found. Tests must be added to `firmware/test/test_native/` before
   > they can run.

## Notes

- Native tests run on host (no ESP32 hardware required)
- Tests in `firmware/test/test_embedded/` require a connected device — not run by this skill
- Test files use Unity framework: `TEST_ASSERT_*` macros
- If a test is marked `[IGNORE]`: note it but do not count as failure
