#include "kb/core.h"
#include "kb_test.h"

static void test_api_version(void)
{
    KB_CHECK_EQ(kb_core_api_version(), KB_CORE_API_VERSION);
}

int main(void)
{
    printf("test_core\n");
    KB_RUN(test_api_version);
    return KB_TEST_RESULT();
}
