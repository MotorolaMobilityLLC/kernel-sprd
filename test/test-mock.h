#include <test/test.h>
#include <test/mock.h>

DECLARE_STRUCT_CLASS_MOCK_PREREQS(test);

DECLARE_STRUCT_CLASS_MOCK_VOID_RETURN(METHOD(fail), CLASS(test),
				      PARAMS(struct test *,
					     struct test_stream *));

DECLARE_STRUCT_CLASS_MOCK_VOID_RETURN(METHOD(mock_vprintk), CLASS(test),
				      PARAMS(const struct test *,
					     const char *,
					     struct va_format *));

DECLARE_STRUCT_CLASS_MOCK_INIT(test);
