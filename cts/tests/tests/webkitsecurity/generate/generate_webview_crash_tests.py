#! /usr/bin/env python

"""
generate_webview_crash_tests.py

generate_webview_crash_tests.py <path_to_tests>

Specializes WebViewSecurityBaseTest.java with the appropriate values.

This script exists as an ugly hack around the lack of real parameterized testing
support. Its basic flow is:

  1. Get the path to a directory containing the tests you want.
  2. Copy those files to ../assets/
  3. Filter those tests so that we only load the .html files (test roots).
  4. For each of those, generate a test from WebViewBaseTest.java
  5. Place the generated tests under src/android/webkitsecurity/cts

The process of generating the tests is very simple:

  1. From each test root generate a Java-style name
  2. Replace the class name and constructor name with the Java name
  3. From the Java-style name generate a logtag
  4. Replace the logtag with the generated logtag value
  5. Replace the base path with the on-device path to the test root
  6. TODO: remove the comment at the top of the base file.

"""

import re
import sys
import string
import shutil
import os.path

BASE_FILE = "WebViewBaseTest"
HOST_ASSETS_PATH = os.path.join("..", "assets")
HOST_SOURCE_PATH = os.path.join("..", "src", "android", "webkitsecurity", "cts")
DEVICE_ASSETS_PATH = ""


def print_help():
  """Prints the docstring, which is also our help text"""
  print(sys.modules[__name__].__doc__)


def get_arguments():
  """Processes the only argument, ensuring it is sane before returning"""

  # if we don't get the correct number of arguments
  if len(sys.argv) != 2:
    print("Sorry, but we need tests to run! Please see the help text below.\n")
    print_help()
    exit(1)

  # get the argument
  argument = os.path.realpath(sys.argv[1])

  # if the given argument isn't a valid directory
  if not os.path.isdir(argument):
    print("Given path is not a directory. Please see the help text below.\n")
    print_help()
    exit(1)

  # TODO: if the directory is empty or contains no .html files, error out.

  # go home
  return argument

def is_crashing_test(path):
  """Checks for the string 'crash' in the file name"""
  if not path.endswith('expected.txt'):
    if 'crash' in path.lower():
      if 'svn' not in path.lower():
        return True
  return False

def get_test_paths(directory):
  """Returns a list of canonical pathnames to the files in the directory"""

  canonical_test_paths = []

  for top, dirs, files in os.walk(directory):
    for fname in files:
      name = os.path.join(top, fname)
      if is_crashing_test(name):
        canonical_test_paths.append(os.path.join(directory, name))

  # make sure there are some
  if not canonical_test_paths:
    print("No test files found! Please specify some and try again.\n")
    print_help()
    exit(1)

  # otherwise, head home
  return canonical_test_paths


def copy_assets(test_files):
  """Copies each of the given test files to the host asset directory"""
  for path in test_files:
    shutil.copy(path, HOST_ASSETS_PATH)


def get_test_roots(test_files):
  """Filters out non-HTML files from the list, returning only entry points"""

  # get all the test roots
  test_roots = [t for t in test_files if t.endswith('.html')]

  # if there aren't any, die
  if not test_roots:
    print("No test roots found! Please specify some tests.\n")
    print_help()
    exit(1)

  # otherwise, go home
  return test_roots


def read_base_test(base_file):
  """Reads in the base test file"""
  with open(base_file) as f:
    contents = f.read()
  return contents


def get_asset_path(test):
  """Get the path to the test file on the device"""
  return DEVICE_ASSETS_PATH + os.path.basename(test)


def get_java_name(test):
  """Returns a Java-style name based on the path of the test.

  This is actually a surprisingly ugly thing to do. The Java convention is:

    * Must start with an uppercase letter
    * Must not contain the $ or whitespace
    * Should be CamelCased
    * Should not contain any other punctuation

  To which we add the following requirements:

    * Must start with 'Webkit'
    * Must end with 'Test'
    * Should not contain any other numerals
    * Should not conflict with any other test
    * Should not duplicate any words

  Unfortunately, the names of the tests we're given are all across the board,
  which makes this code a bit of a mess. Our basic process is to:

    1. Gather the basename of the test
    2. Strip its extension
    3. Replace all numeric characters (9) with literal counterparts (Nine)
    4. Stripping split on any undesired character
    5. Capitalize the fragments
    6. Verify that the first fragment is not "Webkit"
    7. Verify that the last fragment is not "Test"
    8. Join them

  I'm sure there are remaining issues here, but this should do for now.
  """

  basename = os.path.basename(test)

  # note that this is fragile, but we're being conservative here
  name = basename.split('.')[0]

  # build the numeral-number table
  nums = {'0' : "Zero",
          '1' : "One",
          '2' : "Two",
          '3' : "Three",
          '4' : "Four",
          '5' : "Five",
          '6' : "Six",
          '7' : "Seven",
          '8' : "Eight",
          '9' : "Nine"}

  # do our replacement
  for k, v in nums.items():
    name = name.replace(k, v)

  # do the stripping split to obtain our fragments
  undesired_chars = '[' + string.whitespace + string.punctuation + ']'
  fragments = re.split(undesired_chars, name)

  # capitalize each fragment
  fragments = [f.capitalize() for f in fragments]

  # check the first and last fragments
  if fragments[0] != 'Webkit':
    fragments.insert(0, 'Webkit')

  if fragments[-1] != 'Test':
    fragments.append('Test')

  # join the results
  return ''.join(fragments)


def get_device_path(test):
  """Gets the path to the asset as it will be seen from the device"""
  return DEVICE_ASSETS_PATH + os.path.basename(test)


def get_host_test_path(java_name):
  """Returns the path that the generated test should be stored at"""
  return os.path.join(HOST_SOURCE_PATH, java_name + '.java')


def write_test(test_contents, new_test_host_path):
  """Writes the given contents at the given path"""
  with open(new_test_host_path, 'w') as f:
    f.write(test_contents)


if __name__ == "__main__":

  # TODO: check our location
  # check_sanity()

  # get our aguments
  directory = get_arguments()

  # turn that into a list of test pathnames
  test_paths = get_test_paths(directory)

  # copy those paths over to the assets dir
  copy_assets(test_paths)

  # filter for the .html files
  test_roots = get_test_roots(test_paths)

  # read the base test in
  base_test = read_base_test(BASE_FILE)

  # the test roots are the entry points for the tests we care about
  # let's iterate over them and build the accompanying test files.
  for test in test_roots:

    # get the values for substitution
    asset_path = get_asset_path(test)
    java_name = get_java_name(test)

    # get the on-device path to the test
    device_test_path = get_device_path(test)

    # do the actual substitution
    new_test = base_test % (java_name, java_name, device_test_path, java_name)

    # get the destination to write to
    new_test_host_path = get_host_test_path(java_name)

    # write the test
    write_test(new_test, new_test_host_path)

  # and we're done!
