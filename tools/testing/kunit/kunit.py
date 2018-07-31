#!/usr/bin/python3

# A thin wrapper on top of the KUnit Kernel

import argparse
import sys
import os
import time

import kunit_config
import kunit_kernel
import kunit_parser

parser = argparse.ArgumentParser(description='Runs KUnit tests.')

parser.add_argument('--raw_output', help='don\'t format output from kernel',
		    action='store_true')

parser.add_argument('--timeout', help='maximum number of seconds to allow for '
		    'all tests to run. This does not include time taken to '
		    'build the tests.', type=int, default=300,
		    metavar='timeout')

cli_args = parser.parse_args()

def main(linux):

	config_start = time.time()
	success = linux.build_reconfig()
	config_end = time.time()
	if not success:
		return

	print(kunit_parser.timestamp('Building KUnit Kernel ...'))

	build_start = time.time()
	success = linux.build_um_kernel()
	build_end = time.time()
	if not success:
		return

	print(kunit_parser.timestamp('Starting KUnit Kernel ...'))
	test_start = time.time()

	if cli_args.raw_output:
		kunit_parser.raw_output(
			linux.run_kernel(timeout=cli_args.timeout))
	else:
		for line in kunit_parser.parse_run_tests(
			kunit_parser.isolate_kunit_output(
				linux.run_kernel(timeout=cli_args.timeout))):
			print(line)

	test_end = time.time()

	print(kunit_parser.timestamp((
		'Elapsed time: %.3fs total, %.3fs configuring, %.3fs ' +
		'building, %.3fs running.\n') % (test_end - config_start,
		config_end - config_start, build_end - build_start,
		test_end - test_start)))

if __name__ == '__main__':
	main(kunit_kernel.LinuxSourceTree())