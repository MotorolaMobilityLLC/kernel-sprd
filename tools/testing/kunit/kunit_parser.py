import re
from datetime import datetime

kunit_start_re = re.compile('console .* enabled')
kunit_end_re = re.compile('List of all partitions:')

class KernelCrashException(Exception):
	pass

def isolate_kunit_output(kernel_output):
	started = False
	for line in kernel_output:
		if kunit_start_re.match(line):
			started = True
		elif kunit_end_re.match(line):
			return
		elif started:
			yield line
	# Output ended without encountering end marker, kernel probably panicked
	# or crashed unexpectedly.
	raise KernelCrashException()

def raw_output(kernel_output):
	for line in kernel_output:
		print(line)

DIVIDER = "=" * 30

RESET = '\033[0;0m'

def red(text):
	return '\033[1;31m' + text + RESET

def yellow(text):
	return '\033[1;33m' + text + RESET

def green(text):
	return '\033[1;32m' + text + RESET

def print_with_timestamp(message):
	print('[%s] %s' % (datetime.now().strftime('%H:%M:%S'), message))

def print_log(log):
	for m in log:
		print_with_timestamp(m)

def parse_run_tests(kernel_output):
	test_case_output = re.compile('^kunit .*?: (.*)$')

	test_module_success = re.compile('^kunit .*: all tests passed')
	test_module_fail = re.compile('^kunit .*: one or more tests failed')

	test_case_success = re.compile('^kunit (.*): (.*) passed')
	test_case_fail = re.compile('^kunit (.*): (.*) failed')
	test_case_crash = re.compile('^kunit (.*): (.*) crashed')

	total_tests = set()
	failed_tests = set()
	crashed_tests = set()

	def get_test_name(match):
		return match.group(1) + ":" + match.group(2)

	current_case_log = []
	did_kernel_crash = False

	def end_one_test(match, log):
		log.clear()
		total_tests.add(get_test_name(match))

	print_with_timestamp(DIVIDER)
	try:
		for line in isolate_kunit_output(kernel_output):
			# Ignore module output:
			if (test_module_success.match(line) or
			test_module_fail.match(line)):
				print_with_timestamp(DIVIDER)
				continue

			match = re.match(test_case_success, line)
			if match:
				print_with_timestamp(green("[PASSED] ") +
						get_test_name(match))
				end_one_test(match, current_case_log)
				continue

			match = re.match(test_case_fail, line)
			# Crashed tests will report as both failed and crashed. We only
			# want to show and count it once.
			if match and get_test_name(match) not in crashed_tests:
				failed_tests.add(get_test_name(match))
				print_with_timestamp(red("[FAILED] " +
							get_test_name(match)))
				print_log(map(yellow, current_case_log))
				print_with_timestamp("")
				end_one_test(match, current_case_log)
				continue

			match = re.match(test_case_crash, line)
			if match:
				crashed_tests.add(get_test_name(match))
				print_with_timestamp(yellow("[CRASH] " +
							get_test_name(match)))
				print_log(current_case_log)
				print_with_timestamp("")
				end_one_test(match, current_case_log)
				continue

			# Strip off the `kunit module-name:` prefix
			match = re.match(test_case_output, line)
			if match:
				current_case_log.append(match.group(1))
			else:
				current_case_log.append(line)
	except KernelCrashException:
		did_kernel_crash = True
		print_with_timestamp(
			red("The KUnit kernel crashed unexpectedly and was " +
			    "unable to finish running tests!"))
		print_with_timestamp(red("These are the logs from the most " +
					 "recently running test:"))
		print_with_timestamp(DIVIDER)
		print_log(current_case_log)
		print_with_timestamp(DIVIDER)

	fmt = green if (len(failed_tests) + len(crashed_tests) == 0
			and not did_kernel_crash) else red
	message = ("Before the crash:" if did_kernel_crash else
		   "Testing complete.")

	print_with_timestamp(
		fmt(message + " %d tests run. %d failed. %d crashed." %
		    (len(total_tests), len(failed_tests), len(crashed_tests))))

