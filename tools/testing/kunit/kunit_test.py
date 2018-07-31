#!/usr/bin/python3

import unittest

import tempfile, shutil # Handling test_tmpdir

import os

import kunit_config
import kunit_parser
import kunit

test_tmpdir = ''

def setUpModule():
	global test_tmpdir
	test_tmpdir = tempfile.mkdtemp()

def tearDownModule():
	shutil.rmtree(test_tmpdir)

def get_absolute_path(path):
	return os.path.join(os.path.dirname(__file__), path)

class KconfigTest(unittest.TestCase):

	def test_is_subset_of(self):
		kconfig0 = kunit_config.Kconfig()
		self.assertTrue(kconfig0.is_subset_of(kconfig0))

		kconfig1 = kunit_config.Kconfig()
		kconfig1.add_entry(kunit_config.KconfigEntry('CONFIG_TEST=y'))
		self.assertTrue(kconfig1.is_subset_of(kconfig1))
		self.assertTrue(kconfig0.is_subset_of(kconfig1))
		self.assertFalse(kconfig1.is_subset_of(kconfig0))

	def test_read_from_file(self):
		kconfig = kunit_config.Kconfig()
		kconfig_path = get_absolute_path(
			'test_data/test_read_from_file.kconfig')

		kconfig.read_from_file(kconfig_path)

		expected_kconfig = kunit_config.Kconfig()
		expected_kconfig.add_entry(
			kunit_config.KconfigEntry('CONFIG_UML=y'))
		expected_kconfig.add_entry(
			kunit_config.KconfigEntry('CONFIG_MMU=y'))
		expected_kconfig.add_entry(
			kunit_config.KconfigEntry('CONFIG_TEST=y'))
		expected_kconfig.add_entry(
			kunit_config.KconfigEntry('CONFIG_EXAMPLE_TEST=y'))
		expected_kconfig.add_entry(
			kunit_config.KconfigEntry('# CONFIG_MK8 is not set'))

		self.assertEqual(kconfig.entries(), expected_kconfig.entries())

	def test_write_to_file(self):
		kconfig_path = os.path.join(test_tmpdir, '.config')

		expected_kconfig = kunit_config.Kconfig()
		expected_kconfig.add_entry(
			kunit_config.KconfigEntry('CONFIG_UML=y'))
		expected_kconfig.add_entry(
			kunit_config.KconfigEntry('CONFIG_MMU=y'))
		expected_kconfig.add_entry(
			kunit_config.KconfigEntry('CONFIG_TEST=y'))
		expected_kconfig.add_entry(
			kunit_config.KconfigEntry('CONFIG_EXAMPLE_TEST=y'))
		expected_kconfig.add_entry(
			kunit_config.KconfigEntry('# CONFIG_MK8 is not set'))

		expected_kconfig.write_to_file(kconfig_path)

		actual_kconfig = kunit_config.Kconfig()
		actual_kconfig.read_from_file(kconfig_path)

		self.assertEqual(actual_kconfig.entries(),
				 expected_kconfig.entries())

class KUnitParserTest(unittest.TestCase):

	def assertContains(self, needle, haystack):
		for line in haystack:
			if needle in line:
				return
		raise AssertionError('"' +
			str(needle) + '" not found in "' + str(haystack) + '"!')

	def test_output_isolated_correctly(self):
		log_path = get_absolute_path(
			'test_data/test_output_isolated_correctly.log')
		file = open(log_path)
		result = kunit_parser.isolate_kunit_output(file.readlines())
		self.assertContains('kunit example: initializing\n', result)
		self.assertContains('kunit example: example_mock_test passed\n',
				    result)
		self.assertContains('kunit example: all tests passed\n', result)
		file.close()

	def test_parse_successful_test_log(self):
		all_passed_log = get_absolute_path(
			'test_data/test_is_test_passed-all_passed.log')
		file = open(all_passed_log)
		result = kunit_parser.parse_run_tests(file.readlines())
		self.assertContains(
			'Testing complete. 3 tests run. 0 failed. 0 crashed.',
			result)
		file.close()

	def test_parse_failed_test_log(self):
		failed_log = get_absolute_path(
			'test_data/test_is_test_passed-failure.log')
		file = open(failed_log)
		result = kunit_parser.parse_run_tests(file.readlines())
		self.assertContains(
			'Testing complete. 3 tests run. 1 failed. 0 crashed.',
			result)
		file.close()

	def test_broken_test(self):
		broken_log = get_absolute_path(
			'test_data/test_is_test_passed-broken_test.log')
		file = open(broken_log)
		result = kunit_parser.parse_run_tests(
			kunit_parser.isolate_kunit_output(file.readlines()))
		self.assertContains(
			'Before the crash: 3 tests run. 0 failed. 0 crashed.',
			result)
		file.close()

	def test_no_tests(self):
		empty_log = get_absolute_path(
			'test_data/test_is_test_passed-no_tests_run.log')
		file = open(empty_log)
		result = kunit_parser.parse_run_tests(
			kunit_parser.isolate_kunit_output(file.readlines()))
		self.assertContains(
			'Testing complete. 0 tests run. 0 failed. 0 crashed.',
			result)
		file.close()

	def test_crashed_test(self):
		crashed_log = get_absolute_path(
			'test_data/test_is_test_passed-crash.log')
		file = open(crashed_log)
		result = kunit_parser.parse_run_tests(file.readlines())
		self.assertContains(
			'Testing complete. 3 tests run. 0 failed. 1 crashed.',
			result)
		file.close()

if __name__ == '__main__':
	unittest.main()