#! /usr/bin/env python

"""
webkit_security_postprocessor.py

This script postprocesses the result of a cts-tradefed run of webkit security tests to match the
failing tests to the appropriate CL in upstream WebKit.
"""

import os
import commands
import multiprocessing
from xml.etree import ElementTree

class Test:

  java_name = ""
  webkit_name = ""
  local_path = ""
  cls = None
  test_case = ""

  def __init__(self, java_name):
    self.java_name = java_name
    self.cls = []

  def __str__(self):
    data = self.java_name + '\n'
    underline = "=" * (len(data) - 1) + "\n"
    data += underline + '\n'
    data += "\t" + "to reproduce" + "\n"
    data += "\t" + "------------" + "\n"
    data += "\t" + "cts-tradefed run cts --class android.webkitsecurity.cts." + self.java_name + "\n"
    data += "\n"
    data += "\t" + "test case" + "\n"
    data += "\t" + "---------" + "\n"
    for line in self.test_case.splitlines():
      data += "\t" + line + "\n"
    data += "\n"
    data += "\t" + "revisions" + "\n"
    data += "\t" + "---------" + "\n"
    for cl in self.cls:
      name = "[" + str(cl) + "]"
      link = "http://trac.webkit.org/changeset/" + str(cl)[1:]
      data += "\t" + name + "(" + link + ")" + '\n'
    return data

# get the location of the xml output file
test_results_directory = "/usr/local/google/android/out/host/linux-x86/cts/android-cts/repository/results"


# get the location of the android webkitsecurity tests
source_dir = "/usr/local/google/android/"
android_tests_dir = source_dir + "cts/tests/tests/webkitsecurity/src/android/webkitsecurity/cts/"

# get the location of the webkit layout tests
webkit_tests_dir = "/usr/local/google/webkit/LayoutTests"

def get_test_results():
  # look in the test results directory
  test_results = []
  for root, dirs, fnames in os.walk(test_results_directory):
    for fname in fnames:
      if fname == 'testResult.xml':
        full_path = os.path.abspath(os.path.join(root, fname))
        test_results.append(full_path)

  # special cases
  if not test_results:
    raise Exception("No results found")
  elif len(test_results) == 1:
    return test_results[0]
  else:
    # find the timestamp of each file
    newest = None
    newest_fname = None
    for fname in test_results:
      timestamp_start = fname.index('20')
      timestamp_end = fname.index('/', start)
      timestamp = time.strptime(fname[timestamp_start:timestamp_end], "%Y.%m.%d_%H.%M.%S")
      if newest == None or timestamp > newest:
        newest = timestamp
        newest_fname = fname
    return newest_fname

def get_all_java_tests():
  element_tree = ElementTree.parse(get_test_results())
  for elem in element_tree.getiterator():
    if elem.tag == 'TestCase':
      java_name = elem.attrib['name']
      if java_name.startswith('Webkit') and java_name.endswith('Test'):
        for subelem in elem:
          if subelem.tag == 'Test':
            if subelem.attrib['result'] == 'fail':
              yield Test(java_name)

def extract_webkit_name_from_test(test):
  sig = "    private static final String TEST_PATH = \""
  for line in test:
    if line.startswith(sig):
      return line[len(sig):].strip()[:-2]

def get_all_web_tests(java_tests, android_test_dir):
  # for each java test
  for test in java_tests:
    test_name = os.path.join(android_test_dir, test.java_name) + '.java'
    with open(test_name) as f:
      # get the webkit test name
      webkit_test_name = extract_webkit_name_from_test(f)
      # postprocess it
      test.webkit_name = webkit_test_name.split('/')[-1]
      yield test

def find_local_files(tests, webkit_tests_dir):
  # convert the test names to a dictionary
  tests = dict((n.webkit_name, n) for n in tests)
  # for each test under LayoutTests/
  for root, dirs, files in os.walk(webkit_tests_dir):
    if '.svn' in root:
      continue
    for fname in files:
      if fname in tests:
        tests[fname].local_path = os.path.abspath(os.path.join(root, fname))
        with open(tests[fname].local_path) as f:
          tests[fname].test_case = f.read()
  # return the tests
  return tests.values()

def get_test_cls(test):
  cwd = os.getcwd()
  os.chdir(webkit_tests_dir)
  while True:
    cmd = "svn log -q %s" % test.local_path
    status, output = commands.getstatusoutput(cmd)
    if not status:
      break
  # parse the results, saving only the commit numbers
  for line in output.splitlines():
    if line.startswith('r'):
      cl = line.split()[0].strip()
      test.cls.append(cl)
  os.chdir(cwd)
  return test

def get_all_test_cls(tests):
  pool = multiprocessing.Pool(processes=12)
  return pool.map(get_test_cls, tests)

def output(tests, bugreport):
  for test in tests:
    print str(test)
  print
  print
  print bugreport

def get_crashes():
  status, output = commands.getstatusoutput("adb bugreport")
  # TODO reintroduce crash parsing
  # - this was removed due to questions about symbol resolution, but could be reintroduced if
  # - this script as a whole knew anything about branches.
  # - Alternatively, if this would fail meaningfully it could also be reintroduced
  return output

def decode_crashes(logcat):
  with open('/tmp/crashdump.txt', 'w') as f:
    f.write(logcat)
  cwd = os.getcwd()
  os.chdir('/usr/local/google/android') # TODO add branch setting
  status, output = commands.getstatusoutput("./vendor/google/tools/stack /tmp/crashdump.txt")
  return output


def post_bug(report):
  # TODO add auto bugging
  pass

if __name__ == "__main__":
  java_tests = list(get_all_java_tests())
  webkit_tests = list(get_all_web_tests(java_tests, android_tests_dir))
  local_tests = find_local_files(webkit_tests, webkit_tests_dir)
  tests = get_all_test_cls(local_tests)
  crashes = get_crashes()
  traces = decode_crashes(crashes)
  output(tests, traces)
  

  

