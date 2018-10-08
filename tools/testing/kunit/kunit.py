#!/usr/bin/python3
# SPDX-License-Identifier: GPL-2.0

# A thin wrapper on top of the KUnit Kernel

import argparse
import sys
import os
import time

import kunit_config
import kunit_kernel
import kunit_new_template
import kunit_parser

def run_tests(cli_args, linux):
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
				linux.run_kernel(
					timeout=cli_args.timeout))):
			print(line)

	test_end = time.time()

	print(kunit_parser.timestamp((
		'Elapsed time: %.3fs total, %.3fs configuring, %.3fs ' +
		'building, %.3fs running.\n') % (
				test_end - config_start,
				config_end - config_start,
				build_end - build_start,
				test_end - test_start)))

def print_test_skeletons(cli_args):
	kunit_new_template.create_skeletons_from_path(
			cli_args.path,
			namespace_prefix=cli_args.namespace_prefix,
			print_test_only=cli_args.print_test_only)

def main(argv, linux=kunit_kernel.LinuxSourceTree()):
	parser = argparse.ArgumentParser(
			description='Helps writing and running KUnit tests.')
	subparser = parser.add_subparsers(dest='subcommand')

	run_parser = subparser.add_parser('run', help='Runs KUnit tests.')
	run_parser.add_argument('--raw_output', help='don\'t format output from kernel',
				action='store_true')

	run_parser.add_argument('--timeout',
				help='maximum number of seconds to allow for all tests '
				'to run. This does not include time taken to build the '
				'tests.',
				type=int,
				default=300,
				metavar='timeout')

	new_parser = subparser.add_parser(
			'new',
			help='Prints out boilerplate for writing new tests.')
	new_parser.add_argument('--path',
				help='Path of source file to be tested.',
				type=str,
				required=True)
	new_parser.add_argument('--namespace_prefix',
				help='Namespace of the code to be tested.',
				type=str)
	new_parser.add_argument('--print_test_only',
				help='Skip Kconfig and Makefile while printing sample '
				'test.',
				action='store_true')

	cli_args = parser.parse_args(argv)

	if cli_args.subcommand == 'new':
		print_test_skeletons(cli_args)
	elif cli_args.subcommand == 'run':
		run_tests(cli_args, linux)
	else:
		parser.print_help()

if __name__ == '__main__':
	main(sys.argv[1:])
