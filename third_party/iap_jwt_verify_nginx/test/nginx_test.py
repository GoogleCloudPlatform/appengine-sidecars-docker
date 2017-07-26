# Copyright (C) 2002-2016 Igor Sysoev
# Copyright (C) 2011-2016 Nginx, Inc.
# Copyright (C) 2017 Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
################################################################################

"""Tests for nginx with the IAP JWT verification module compiled in."""

import httplib
import json
import os
import shlex
import shutil
import subprocess
import time
import unittest

APP_RESPONSE = 'SUCCESS_WOOHOO'
HEALTHCHECK_RESPONSE = 'healthy'
KEY_FILE_NAME = 'keys.jwk'
NGX_CONF_PREAMBLE = """
daemon on;
worker_processes 1;
error_log stderr error;
pid nginx.pid;
events {
  worker_connections 32;
}
"""
NGX_CONF_HTTP_BLOCK_OPEN = """
http {{
  log_format custom '$remote_addr [$time_local] "$request" $status '
  '$body_bytes_sent "$http_user_agent" '
  'iap_jwt_action=$iap_jwt_action';

  access_log {path}/access.log custom;
"""
NGX_CONF_IAP_PARAMS_TEMPLATE = """
  iap_jwt_verify_project_number 1234;
  iap_jwt_verify_app_id some-app-id;
  iap_jwt_verify_key_file """ + KEY_FILE_NAME + """;
  iap_jwt_verify_iap_state_file {iap_state_file_name};
  iap_jwt_verify_state_cache_time_sec {state_cache_time_sec};
  iap_jwt_verify_key_cache_time_sec {key_cache_time_sec};
"""
NGX_CONF_SRV_BLOCK_OPEN_TEMPLATE = """
  server {{
    listen {ngx_port};
"""
NGX_CONF_HEALTHCHECK_TEMPLATE = """
    location = /healthcheck {{
      iap_jwt_verify off;
      root {root_dir};
    }}
"""
NGX_CONF_HEALTHCHECK_TEMPLATE_NO_IAP_DIRECTIVES = """
    location = /healthcheck {{
      root {root_dir};
    }}
"""
NGX_CONF_POSTAMBLE = """
  } # server
} # http
"""
CONF_FILE_NAME = 'nginx.conf'
IAP_STATE_FILE_NAME = 'iap_state'
FIVE_MINUTES_IN_SECONDS = 300
TWELVE_HOURS_IN_SECONDS = 43200
NGX_PORT = 9127
IAP_JWT_HEADER_NAME = 'x-goog-iap-jwt-assertion'
VALID_JWT = 'eyJhbGciOiJFUzI1NiIsImtpZCI6IjFhIiwidHlwIjoiSldUIn0.eyJhdWQiOiIvcHJvamVjdHMvMTIzNC9hcHBzL3NvbWUtYXBwLWlkIiwiZW1haWwiOiJub2JvZHlAZXhhbXBsZS5jb20iLCJleHAiOjEzMzcxMzM3MTMzNywiaWF0Ijo2MSwiaXNzIjoiaHR0cHM6Ly9jbG91ZC5nb29nbGUuY29tL2lhcCIsInN1YiI6ImFjY291bnRzLmdvb2dsZS5jb206MTIzNDUifQ.b9uV01RfO3qmIdrM8NsWsNcGKFcSImvS9PgZfZcxDsXZXhOAxuWzvqxD1yvbtXjrNYb_UJiIh-o_SZlxr8qSMA'
OTHER_VALID_JWT = 'eyJhbGciOiJFUzI1NiIsImtpZCI6IjJiIiwidHlwIjoiSldUIn0.eyJhdWQiOiIvcHJvamVjdHMvMTIzNC9hcHBzL3NvbWUtYXBwLWlkIiwiZW1haWwiOiJub2JvZHlAZXhhbXBsZS5jb20iLCJleHAiOjEzMzcxMzM3MTMzNywiaWF0Ijo2MSwiaXNzIjoiaHR0cHM6Ly9jbG91ZC5nb29nbGUuY29tL2lhcCIsInN1YiI6ImFjY291bnRzLmdvb2dsZS5jb206MTIzNDUifQ.5DjEO61Cnc_GrhNysG-vwy_7E6fQ15PRYT-jsbevs1UvsS4NckzsFnNBFNIDYULkNGSgkdvA2pvpHdQzyv9fpg'
INVALID_JWT = 'eyJhbGciOiJFUzI1NiIsImtpZCI6IjFhIiwidHlwIjoiSldUIn0.eyJhdWQiOiIvcHJvamVjdHMvMTIzNC9hcHBzL3NvbWUtYXBwLWlkIiwiZW1haWwiOiJub2JvZHlAZXhhbXBsZS5jb20iLCJleHAiOjEzMzcxMzM3MTMzNywiaWF0Ijo2MSwiaXNzIjoiaHR0cHM6Ly9jbG91ZC5nb29nbGUuY29tL2lhcCIsInN1YiI6ImFjY291bnRzLmdvb2dsZS5jb206MTIzNDUifQ.b9uV01RfO3qmIdrM8NsWsNcGKFcSImvS9PgZfZcxDsXZXhOAxuWzvqxD1yvbtXjrNYb_UJiIh-o_SZlxr8qSNA'


class NginxTest(unittest.TestCase):

  @classmethod
  def setUpClass(cls):
    # Create the healthcheck message
    f = open('healthcheck', 'w')
    f.write(HEALTHCHECK_RESPONSE)
    f.close()

    # Create index.html
    f = open('index.html', 'w')
    f.write(APP_RESPONSE)
    f.close()

  def setUp(self):
    self.cur_dir = os.getcwd()

    # Some tests modify the key file.
    # Make sure we always start with a clean copy.
    shutil.copyfile('test/keys.jwk', KEY_FILE_NAME)

  def tearDown(self):
    self.stopNginx()

  def createIapJwtVerifyDirective(self, on):
    return '  iap_jwt_verify ' + ('on;\n' if on else 'off;\n')

  def createLocConfBlocks(self, loc_iap_directive, omit_all_iap_directives):
    indent = '    '

    # location config for '/'
    loc_conf = indent + 'location / {\n'
    if not omit_all_iap_directives and loc_iap_directive != None:
      loc_conf += indent + self.createIapJwtVerifyDirective(loc_iap_directive)
    loc_conf += indent + '  root {0};\n'.format(self.cur_dir)
    loc_conf += indent + '}\n'

    # location config for '/healthcheck'
    hc_template = (NGX_CONF_HEALTHCHECK_TEMPLATE_NO_IAP_DIRECTIVES
                   if omit_all_iap_directives
                   else NGX_CONF_HEALTHCHECK_TEMPLATE)
    loc_conf += hc_template.format(root_dir=self.cur_dir)

    return loc_conf

  def createConfFile(self,
                     iap_on_main,
                     iap_on_srv,
                     iap_on_loc,
                     state_cache_time_sec,
                     key_cache_time_sec,
                     omit_all_iap_directives=False):
    f = open(CONF_FILE_NAME, 'w')
    f.write(NGX_CONF_PREAMBLE)
    f.write(NGX_CONF_HTTP_BLOCK_OPEN.format(path=self.cur_dir))
    if not omit_all_iap_directives:
      if iap_on_main != None:
        f.write(self.createIapJwtVerifyDirective(iap_on_main))
      f.write(NGX_CONF_IAP_PARAMS_TEMPLATE.format(
                 iap_state_file_name=IAP_STATE_FILE_NAME,
                 state_cache_time_sec=state_cache_time_sec,
                 key_cache_time_sec=key_cache_time_sec))
    f.write(NGX_CONF_SRV_BLOCK_OPEN_TEMPLATE.format(ngx_port=NGX_PORT))
    if not omit_all_iap_directives and iap_on_srv != None:
      f.write('  ' + self.createIapJwtVerifyDirective(iap_on_srv))
    f.write(self.createLocConfBlocks(iap_on_loc, omit_all_iap_directives))
    f.write(NGX_CONF_POSTAMBLE);
    f.truncate()
    f.close()

  def createConfFileSimple(self, enforce, state_cache_time, key_cache_time):
    self.createConfFile(None, None, enforce, state_cache_time, key_cache_time)

  def createConfFileSimplest(self, enforce):
    self.createConfFileSimple(
        enforce, FIVE_MINUTES_IN_SECONDS, TWELVE_HOURS_IN_SECONDS)

  def createIapStateFile(self):
    """Create the IAP state file (indicates that IAP is on)."""
    open(IAP_STATE_FILE_NAME, 'w').close()

  def deleteIapStateFile(self):
    """Delete the IAP state file (its absence indicates that IAP is off)."""
    try:
      os.remove(IAP_STATE_FILE_NAME)
    except:
      pass

  def exec_nginx(self, signal=''):
    command = 'test/nginx-iap -c ' + CONF_FILE_NAME + ' -p ' + self.cur_dir
    if signal:
      command += ' -s ' + signal
    subprocess.check_call(shlex.split(command))

  def startNginx(self):
    self.exec_nginx()

  def stopNginx(self):
    try:
      self.exec_nginx('quit')
    except:
      # In some tests, there is not an nginx process to clean up. This is not a
      # concern.
      pass

  def reloadNginxConfig(self):
    self.exec_nginx('reload')

  def assertAppResponse(self, resp):
    """Assert that nginx responded to the request with status 200 and the
    expected app response body.

    Args:
      resp: an httplib.HttpResponse object"""
    self.assertEqual(200, resp.status)
    self.assertEqual(APP_RESPONSE, resp.read())

  def assertHealthcheckResponse(self, resp):
    """Assert that nginx responded to the request with status 200 and the
    expected healthcheck response body.

    Args:
      resp: an httplib.HttpResponse object"""
    self.assertEqual(200, resp.status)
    self.assertEqual(HEALTHCHECK_RESPONSE, resp.read())

  def assertForbidden(self, resp):
    """Assert that nginx rejected the request with a 403 Forbidden.

    Args:
      resp: an httplib.HttpResponse object"""
    self.assertEqual(403, resp.status)
    self.assertIn('Forbidden', resp.read())

  def makeAndEvaluateStandardRequests(self, expect_enforcement):
    """Makes four standard requests, and asserts conditionally on the responses:
    1) Request to '/' with no JWT
      - if expect_enforcement is True, validated with assertForbidden
      - if expect_enforcement is False, validated with assertAppResponse
    2) Request to '/' with a valid JWT
      - always validated with assertAppResponse
    3) Request to '/' with an invalid JWT (bad signature)
      - if expect_enforcement is True, validated with assertForbidden
      - if expect_enforcement is False, validated with assertAppResponse
    4) Request to '/healthcheck' with no JWT
      - always validated with assertHealthcheckResponse"""
    conn = httplib.HTTPConnection('localhost', NGX_PORT)

    # a request to '/' with no JWT
    conn.request('GET', 'http://localhost/')
    if expect_enforcement:
      self.assertForbidden(conn.getresponse())
    else:
      self.assertAppResponse(conn.getresponse())

    # A request to '/' with a valid JWT should always succeed.
    conn.request('GET',
                 'http://localhost/',
                 headers = { IAP_JWT_HEADER_NAME: VALID_JWT })
    self.assertAppResponse(conn.getresponse())

    # A request to '/' with an invalid JWT should fail.
    conn.request('GET',
                 'http://localhost/',
                 headers = { IAP_JWT_HEADER_NAME: INVALID_JWT })
    if expect_enforcement:
      self.assertForbidden(conn.getresponse())
    else:
      self.assertAppResponse(conn.getresponse())

    # A request to '/healthcheck' without a JWT should always succeed.
    conn.request('GET', 'http://localhost/healthcheck')
    self.assertHealthcheckResponse(conn.getresponse())


  def test_iap_on_enforcement_on(self):
    """If IAP is on (i.e. state file present) and enforcement is on, then
    enforcement should be expected."""
    self.createConfFileSimplest(True)
    self.createIapStateFile()
    self.startNginx()
    self.makeAndEvaluateStandardRequests(True)

  def test_iap_off_enforcement_on(self):
    """If IAP is off (i.e. state file not present) and enforcement is on, then
    enforcement should NOT be expected."""
    self.createConfFileSimplest(True)
    self.deleteIapStateFile()
    self.startNginx()
    self.makeAndEvaluateStandardRequests(False)

  def test_iap_on_enforcement_off(self):
    """If IAP is on (i.e. state file present) and enforcement is off, then
    enforcement should NOT be expected."""
    self.createConfFileSimplest(False)
    self.createIapStateFile()
    self.startNginx()
    self.makeAndEvaluateStandardRequests(False)

  def test_iap_off_enforcement_off(self):
    """If IAP is off (i.e. state file not present) and enforcement is off, then
    enforcement should NOT be expected."""
    self.createConfFileSimplest(False)
    self.deleteIapStateFile()
    self.startNginx()
    self.makeAndEvaluateStandardRequests(False)

  def test_conf_merge(self):
    """LOC setting overrides SRV or MAIN, and SRV overrides MAIN. If no
    iap_jwt_verify directives are present, no verification is performed."""
    verify_directive_states = [
        None,  # directive absent
        True,  # iap_jwt_verify on;
        False  # iap_jwt_verify off;
    ]

    self.createIapStateFile()
    for main in verify_directive_states:
      for srv in verify_directive_states:
        for loc in verify_directive_states:
          self.createConfFile(
            main, srv, loc, FIVE_MINUTES_IN_SECONDS, TWELVE_HOURS_IN_SECONDS)
          self.startNginx()
          expect_enforcement = False
          if (loc == True):
            expect_enforcement = True
          elif loc == None:
            if srv == True:
              expect_enforcement = True
            elif srv == None:
              expect_enforcement = main == True
          self.makeAndEvaluateStandardRequests(expect_enforcement)
          self.stopNginx()

  def test_state_cache_time_too_large(self):
    """Ensure that nginx does not start if the maximum state cache time is
    exceeded."""
    try:
      self.createConfFileSimple(
          True, FIVE_MINUTES_IN_SECONDS + 1, TWELVE_HOURS_IN_SECONDS)
      self.startNginx()
      self.assertEqual(0, 1)
    except subprocess.CalledProcessError:
      pass

  def test_iap_disable(self):
    """Deleting the IAP state should result in JWT-less requests getting through    after a length of time equal to the state cache time."""
    state_cache_time = 3  # seconds
    self.createConfFileSimple(True, state_cache_time, TWELVE_HOURS_IN_SECONDS)
    self.createIapStateFile()
    self.startNginx()
    self.makeAndEvaluateStandardRequests(True)
    self.deleteIapStateFile()
    time.sleep(state_cache_time)
    self.makeAndEvaluateStandardRequests(False)

  def test_iap_enable(self):
    """Creating the IAP state should result in JWT-less requests getting
    rejected after a length of time equal to the state cache time."""
    state_cache_time = 3  # seconds
    self.createConfFileSimple(True, state_cache_time, TWELVE_HOURS_IN_SECONDS)
    self.deleteIapStateFile()
    self.startNginx()
    self.makeAndEvaluateStandardRequests(False)
    self.createIapStateFile()
    time.sleep(state_cache_time)
    self.makeAndEvaluateStandardRequests(True)

  def test_key_cache_time_too_large(self):
    """Ensure that nginx does not start if the maximum key cache time is
    exceeded."""
    try:
      self.createConfFileSimple(
          True, FIVE_MINUTES_IN_SECONDS, TWELVE_HOURS_IN_SECONDS + 1)
      self.startNginx()
      self.assertEqual(0, 1)
    except subprocess.CalledProcessError:
      pass

  def test_key_refresh(self):
    """The key file should be reloaded after a length of time equal to the
    key_cache_time."""
    key_cache_time = 3  # seconds
    self.createConfFileSimple(
        True, FIVE_MINUTES_IN_SECONDS, key_cache_time)
    self.createIapStateFile()
    self.startNginx()
    conn = httplib.HTTPConnection('localhost', NGX_PORT)
    conn.request('GET',
                 'http://localhost/',
                 headers = { IAP_JWT_HEADER_NAME: VALID_JWT })
    self.assertAppResponse(conn.getresponse())

    keys_file = open(KEY_FILE_NAME, 'r')
    jwks = json.load(keys_file)
    keys_file.close()

    # Delete the key used to sign VALID_JWT
    del jwks['keys'][0]

    keys_file = open(KEY_FILE_NAME, 'w')
    json.dump(jwks, keys_file)
    keys_file.close()

    time.sleep(key_cache_time)

    conn.request('GET',
                 'http://localhost/',
                 headers = { IAP_JWT_HEADER_NAME: VALID_JWT })
    self.assertForbidden(conn.getresponse())

    # OTHER_VALID_JWT is signed with key 2b, which was not removed, so it should
    # be valid.
    conn.request('GET',
                 'http://localhost/',
                 headers = { IAP_JWT_HEADER_NAME: OTHER_VALID_JWT })
    self.assertAppResponse(conn.getresponse())

  def test_no_iap_jwt_verify_directives(self):
    """nginx should start up just fine and requests should not be blocked if no
    iap_jwt_verify.* directives (including value setters) are present in the
    config"""
    # Create a config file with NO IAP directives.
    self.createConfFile(None, None, None, None, None, True)
    self.createIapStateFile()
    self.startNginx()
    self.makeAndEvaluateStandardRequests(False)

  def test_iap_jwt_action_setting(self):
    """The iap_jwt_action variable is set by the IAP JWT access handler. Its
    intended purpose is to be written to the access log for metric generation.
    This test ensures that it is set properly and writing it to the access log
    succeeds as expected."""
    # Make sure access logs written by previous tests don't mess us up.
    try:
      os.remove("access.log")
    except:
      pass

    self.createConfFileSimple(True, 0, TWELVE_HOURS_IN_SECONDS)
    self.deleteIapStateFile()
    self.startNginx()
    conn = httplib.HTTPConnection('localhost', NGX_PORT)

    conn.request('GET', 'http://localhost/healthcheck')
    conn.getresponse().read()  # Must do this prior to making another request

    conn = httplib.HTTPConnection('localhost', NGX_PORT)
    conn.request('GET', 'http://localhost/')
    conn.getresponse().read()

    self.createIapStateFile()
    conn.request('GET', 'http://localhost/')
    conn.getresponse().read()
    conn.request('GET',
                 'http://localhost/',
                 headers = { IAP_JWT_HEADER_NAME: VALID_JWT })
    conn.getresponse().read()
    conn.request('GET',
                 'http://localhost/',
                 headers = { IAP_JWT_HEADER_NAME: INVALID_JWT })
    conn.getresponse().read()

    # If the access handler doesn't get inserted because the IAP JWT module is
    # not in use anywhere, we still expect a "noop_off" action value.
    self.stopNginx()
    self.createConfFileSimple(False, 0, TWELVE_HOURS_IN_SECONDS)
    self.startNginx()
    conn = httplib.HTTPConnection('localhost', NGX_PORT)
    conn.request('GET', 'http://localhost/')
    conn.getresponse().read()

    access_log = open("access.log", 'r')
    lines = access_log.readlines()
    self.assertEqual(len(lines), 6)
    self.assertIn('iap_jwt_action=noop_off', lines[0])
    self.assertIn('iap_jwt_action=noop_iap_off', lines[1])
    self.assertIn('iap_jwt_action=deny', lines[2])
    self.assertIn('iap_jwt_action=allow', lines[3])
    self.assertIn('iap_jwt_action=deny', lines[4])
    self.assertIn('iap_jwt_action=noop_off', lines[0])


if __name__ == '__main__':
  unittest.main()
