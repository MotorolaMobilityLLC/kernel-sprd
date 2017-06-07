#include <test/test.h>

static void example_simple_test(struct test *test)
{
	EXPECT_EQ(test, 1, 1);
}

static int example_test_init(struct test *test)
{
	test_info(test, "initializing");

	return 0;
}

static struct test_case example_test_cases[] = {
	TEST_CASE(example_simple_test),
	{},
};

static struct test_module example_test_module = {
	.name = "example",
	.init = example_test_init,
	.test_cases = example_test_cases,
};
module_test(example_test_module);
