# Copyright 2010 - 2012, Qualcomm Innovation Center, Inc.
# 
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
# 
#        http://www.apache.org/licenses/LICENSE-2.0
# 
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
# 

Import('env')

# Indicate that this SConscript file has been loaded already
#
env['_ALLJOYNCORE_'] = True

# Dependent Projects
common_hdrs, common_objs = env.SConscript(['../common/SConscript'])
#if env['OS_GROUP'] == 'windows' or env['OS'] == 'android':
#    env.SConscript(['../stlport/SConscript'])

if(not(env.has_key('BULLSEYE_BIN'))):
    print('BULLSEYE_BIN not specified')
else:
    env.PrependENVPath('PATH', env.get('BULLSEYE_BIN'))

# manually add dependencies for xml to h, and for files included in the xml
env.Depends('inc/Status.h', 'src/Status.xml');
env.Depends('inc/Status.h', '../common/src/Status.xml');

# Add support for multiple build targets in the same workset
env.VariantDir('$OBJDIR', 'src', duplicate = 0)
env.VariantDir('$OBJDIR/test', 'test', duplicate = 0)
env.VariantDir('$OBJDIR/daemon', 'daemon', duplicate=0)
env.VariantDir('$OBJDIR/samples', 'samples', duplicate = 0)
env.VariantDir('$OBJDIR/alljoyn_android', 'alljoyn_android', duplicate = 0)

# AllJoyn Install
env.Install('$OBJDIR', env.File('src/Status.xml'))
env.Status('$OBJDIR/Status')
env.Install('$DISTDIR/inc', env.File('inc/Status.h'))
env.Install('$DISTDIR/inc/alljoyn', env.Glob('inc/alljoyn/*.h'))
for d,h in common_hdrs.items():
    env.Install('$DISTDIR/inc/%s' % d, h)

# Header file includes
env.Append(CPPPATH = [env.Dir('inc')])

# Make private headers available
env.Append(CPPPATH = [env.Dir('src')])

# AllJoyn Libraries
libs = env.SConscript('$OBJDIR/SConscript', exports = ['common_objs'])
ajlib = env.Install('$DISTDIR/lib', libs)
env.Append(LIBPATH = [env.Dir('$DISTDIR/lib')])

# Set the alljoyn library 
env.Prepend(LIBS = ajlib)

# AllJoyn Daemon, daemon library, and bundled daemon object file
daemon_progs, bdlib, bdobj = env.SConscript('$OBJDIR/daemon/SConscript')
daemon_lib = env.Install('$DISTDIR/lib', bdlib)
daemon_obj = env.Install('$DISTDIR/lib', bdobj)
env.Install('$DISTDIR/bin', daemon_progs)

# Test programs to have built-in bundled daemon or not
if env['BD'] == 'on':
    env.Prepend(LIBS = daemon_lib)
    env.Prepend(LIBS = daemon_obj)
    env['bdlib'] = ""
    env['bdobj'] = ""
else:
    env['bdlib'] = daemon_lib
    env['bdobj'] = daemon_obj

# Test programs 
progs = env.SConscript('$OBJDIR/test/SConscript')
env.Install('$DISTDIR/bin', progs)

# Build unit Tests
env.SConscript('unit_test/SConscript', variant_dir='$OBJDIR/unittest', duplicate=0)

# Sample programs
progs = env.SConscript('$OBJDIR/samples/SConscript')
env.Install('$DISTDIR/bin/samples', progs)

# Android daemon runner
progs = env.SConscript('$OBJDIR/alljoyn_android/SConscript')
env.Install('$DISTDIR/bin/alljoyn_android', progs)

# Release notes and misc. legals
env.Install('$DISTDIR', 'docs/ReleaseNotes.txt')
env.Install('$DISTDIR', 'docs/AllJoyn_API_Changes_cpp.txt')
env.Install('$DISTDIR', 'docs/AllJoyn_API_Changes_java.txt')
if env['OS_CONF'] == 'windows': 
    env.InstallAs('$DISTDIR/README.txt', 'docs/README.windows')
if env['OS_CONF'] == 'linux':
    env.InstallAs('$DISTDIR/README.txt', 'docs/README.linux')
if env['OS_CONF'] == 'android':
    env.InstallAs('$DISTDIR/README.txt', 'docs/README.android')

env.Install('$DISTDIR', 'README.md')
env.Install('$DISTDIR', 'NOTICE')

# Build docs
env.SConscript('docs/SConscript')
    
