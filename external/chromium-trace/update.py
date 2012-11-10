#!/usr/bin/python2.6

import config, httplib, json, urllib, subprocess, sys

# Read all the Javascript files.
js_code = [('js_code', open(f).read()) for f in config.js_in_files]

# Read all the CSS files and concatenate them.
css_code = ''.join(open(f).read() for f in config.css_in_files)

# Define the parameters for the POST request and encode them in
# a URL-safe format.
params = urllib.urlencode(js_code + [
  ('language', 'ECMASCRIPT5'),
  ('compilation_level', 'SIMPLE_OPTIMIZATIONS'),
  ('output_format', 'json'),
  ('output_info', 'errors'),
  ('output_info', 'compiled_code'),
])

# Always use the following value for the Content-type header.
headers = { "Content-type": "application/x-www-form-urlencoded" }
conn = httplib.HTTPConnection('closure-compiler.appspot.com')
conn.request('POST', '/compile', params, headers)
response = conn.getresponse()
data = response.read()
conn.close

if response.status != 200:
  print sys.stderr, "error returned from JS compile service: %d" % response.status
  sys.exit(1)

result = json.loads(data)
if 'errors' in result:
  for e in result['errors']:
    filenum = int(e['file'][6:])
    filename = config.js_in_files[filenum]
    lineno = e['lineno']
    charno = e['charno']
    err = e['error']
    print '%s:%d:%d: %s' % (filename, lineno, charno, err)
  print 'Failed to generate %s.' % config.js_out_file
  sys.exit(1)

open(config.js_out_file, 'wt').write(result['compiledCode'] + '\n')
print 'Generated %s' % config.js_out_file

yuic_args = ['yui-compressor', '--type', 'css', '-o', config.css_out_file]
p = subprocess.Popen(yuic_args, stdin=subprocess.PIPE)
p.communicate(input=css_code)
if p.wait() != 0:
  print 'Failed to generate %s.' % config.css_out_file
  sys.exit(1)

print 'Generated %s' % config.css_out_file
