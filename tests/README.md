# Kernel Test Helpers

`tests/` contains shared helpers for app-based kernel regression tests.

Test apps live in `tests/<name>_test.c` and can be selected with the existing
`APP=<name>` build flow:

```sh
make APP=destroy_test
```

Include the helper with `#include "test.h"`.

Recommended test shape:

```c
void app_main(void)
{
    test_begin("feature");
    test_check("object created", handle != TRT_HANDLE_INVALID);
    test_check_result("operation", result == ERR_OK, result);
    test_summary();
}
```

Use one test file per feature. Keep each check explicit and end long-running
tests with a summary task so serial logs show `total` and `failed`.
