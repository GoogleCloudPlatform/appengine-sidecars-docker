#!/usr/bin/python
#
# Copyright 2014 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


"""Verify that App Engine APIs are available."""
import os
import sys
import time
import urllib2

# add the vm meta app to the path for api libraries
agent_path = os.environ.get('AGENT_HOME', '/home/vmagent/python_vm_runtime')
new_path = ('%s:%s/lib:%s/lib/yaml/lib' % tuple([agent_path] * 3)).split(':')
sys.path += new_path

# pylint: disable=g-import-not-at-top
from google.appengine.api import apiproxy_stub_map
from google.appengine.api import modules
from google.appengine.ext.vmruntime import vmstub


def GetMetadata(attr):
  """Get metadata by key."""
  req = urllib2.Request(
      'http://metadata/computeMetadata/v1/instance/attributes/%s' % attr)
  req.add_header('Metadata-Flavor', 'Google')
  return urllib2.urlopen(req).read()


def DefaultTicket():
  """Default ticket specified by env vars, falling back to metadata."""
  appid = os.environ.get('GAE_LONG_APP_ID') or GetMetadata('gae_project')
  escaped_appid = appid.replace(':', '_').replace('.', '_')
  major_version = os.environ.get('GAE_MODULE_VERSION')
  if not major_version:
    # 'gae_backend_version' could have <major version> or
    # <major version>_<minor version>
    version = GetMetadata('gae_backend_version')
    major_version = version.rsplit('_')[0]
  module = (os.environ.get('GAE_MODULE_NAME') or
            GetMetadata('gae_backend_name'))
  instance = (os.environ.get('GAE_MODULE_INSTANCE') or
              GetMetadata('gae_backend_instance'))
  return '%s/%s.%s.%s' % (escaped_appid, module, major_version, instance)


def VerifyApi():
  """Verify that App Engine APIs are available.

  It keeps trying one of App Engine modules api
  (i.e. modules.get_num_instances()), until it gets valid results or timeout
  (2 minutes).
  """
  stub = vmstub.VMStub()
  apiproxy_stub_map.apiproxy = apiproxy_stub_map.APIProxyStubMap(stub)
  start = time.time()
  while True:
    try:
      num_instances = modules.get_num_instances()
      print 'number of instances: ', num_instances
      sys.exit(0)
    except Exception as e:  # pylint: disable=broad-except
      print 'API is not available yet because of exception:', e
    time.sleep(1)
    if time.time() > start + 900:
      sys.exit(1)


if __name__ == '__main__':
  if 'DEFAULT_TICKET' not in os.environ:
    os.environ['DEFAULT_TICKET'] = DefaultTicket()
  VerifyApi()
