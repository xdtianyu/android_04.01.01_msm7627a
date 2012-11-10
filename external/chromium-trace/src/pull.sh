#!/bin/bash

set -e

rm -rf shared tracing
svn co http://src.chromium.org/chrome/trunk/src/chrome/browser/resources/shared/
svnversion shared > UPSTREAM_REVISION
svn co http://src.chromium.org/chrome/trunk/src/chrome/browser/resources/tracing/@`cat UPSTREAM_REVISION`
rm -rf `find -name ".svn"`
