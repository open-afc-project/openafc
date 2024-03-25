''' Test cases related to PAWS API.
'''

import logging
from urlparse import urljoin
from random import randint
from .base import (BaseTestCase, UserLoginBaseTestCase)
from afc import (TestAfcEngine)
from nose import SkipTest

#: Logger for this module
LOGGER = logging.getLogger(__name__)


class TestPawsApi(TestAfcEngine):
    ''' Test case to verify the PAWS JSON-RPC endpoint.
    '''

    def setUp(self):
        UserLoginBaseTestCase.setUp(self)

        # Get the actual endpoint URL
        resp = self.httpsession.head(
            self._resolve_url(''),
            allow_redirects=True
        )
        self.assertIn(resp.status_code, (200,))
        index_url = resp.url
        config_url = urljoin(index_url, '../ratapi/v1/guiconfig')
        resp = self.httpsession.get(config_url)
        self.guiconfig = resp.json()
        self.paws_url = self._resolve_url(self.guiconfig['paws_url'])

    def tearDown(self):
        UserLoginBaseTestCase.tearDown(self)

    def _call_jsonrpc(self, url, method, params, expect_status=200,
                      expect_error=None, expect_result=None):
        if not params:
            params = {}

        req_id = randint(1, 10**9)
        req_body = {
            'jsonrpc': '2.0',
            'id': req_id,
            'method': method,
            'params': params,
        }
        LOGGER.debug("request:\n%s", req_body)
        resp = self.httpsession.post(
            url,
            headers={
                'accept-encoding': 'gzip',
            },
            json=req_body,
        )

        LOGGER.debug("request code: %d body:\n%s",
                     resp.status_code, resp.content)
        self.assertEqual(expect_status, resp.status_code)
        self.assertEqual('application/json', resp.headers.get('content-type'))
        resp_body = resp.json()
        self.assertEqual('2.0', resp_body.get('jsonrpc'))
        self.assertEqual(req_id, resp_body.get('id'))

        if expect_error is not None:
            err_obj = resp_body.get('error')
            self.assertIsNotNone(err_obj)
            for (key, val) in expect_error.iteritems():
                LOGGER.debug("%s ==? %s", val, err_obj.get(key))
                self.assertEqual(val, err_obj.get(key))
        elif expect_result is not None:
            result_obj = resp_body.get('result')
            self.assertIsNotNone(
                result_obj, msg="In body {}".format(resp_body))
            for (key, val) in expect_result.iteritems():
                self.assertEqual(val, result_obj.get(key))

        return resp_body

    def test_browser_redirect(self):
        resp = self.httpsession.head(
            self.paws_url
        )
        self.assertEqual(302, resp.status_code)
        self.assertEqual(urljoin(self.paws_url, 'paws/browse/'),
                         resp.headers.get('location'))

    def test_jsonrpc_empty(self):
        resp = self.httpsession.post(
            self.paws_url,
            json={},
        )
        self.assertEqual(200, resp.status_code)
        self.assertEqual('application/json', resp.headers.get('content-type'))

        got_body = resp.json()
        self.assertEqual(u'2.0', got_body.get('jsonrpc'))
        self.assertEqual(None, got_body.get('id'))
        got_error = got_body.get('error')
        self.assertIsNotNone(got_error)
        self.assertEqual(-32602, got_error['code'])
        self.assertEqual(u'InvalidParamsError', got_error['name'])

    def test_jsonrpc_badmethod(self):
        self._call_jsonrpc(
            self.paws_url,
            method='hi',
            params={},
            expect_error={
                u'code': -32601,
                u'name': 'MethodNotFoundError',
            }
        )

    def test_jsonrpc_badargs(self):
        self._call_jsonrpc(
            self.paws_url,
            method='spectrum.paws.getSpectrum',
            params={},
            expect_error={
                u'code': -32602,
                u'name': u'InvalidParamsError',
                u'message': u'InvalidParamsError: Required parameter names: deviceDesc location antenna capabilities type version',
            })

    def test_jsonrpc_no_rulesets(self):
        req_devicedesc = {
            "serialNumber": "sn-test",
        }
        self._call_jsonrpc(
            self.paws_url,
            method='spectrum.paws.getSpectrum',
            params={
                "antenna": {
                    "height": 25,
                    "heightType": "AMSL",
                    "heightUncertainty": 5
                },
                "capabilities": {
                    "indoorOutdoor": "INDOOR"
                },
                "deviceDesc": req_devicedesc,
                "location": {
                    "point": {
                        "center": {
                            "latitude": 40.75,
                            "longitude": -74
                        },
                        "orientation": 48,
                        "semiMajorAxis": 100,
                        "semiMinorAxis": 75
                    }
                },
                "type": "AVAIL_SPECTRUM_REQ",
                "version": "1.0"
            },
            expect_status=401,
            expect_error={
                u'code': 401,
                u'name': u'InvalidCredentialsError',
                u'message': u'InvalidCredentialsError: Invalid rulesetIds: [\"AFC-6GHZ-DEMO-1.1\"] expected',
            }
        )

    def test_paws_valid(self):
        afc_loc = self.guiconfig["afcconfig_defaults"]
        LOGGER.debug("cookies: %s, token: %s", self.cookies, self.login_token)
        resp = self.httpsession.head(
            self._resolve_url(afc_loc),
            headers={
                'Authorization': self.login_token},
            cookies=self.cookies)
        code = resp.status_code
        LOGGER.debug("status: %d, url: %s", code, self._resolve_url(afc_loc))
        if code == 404:
            raise SkipTest("AFC Config does not exist.")
        req_devicedesc = {
            "serialNumber": "sn-test",
            "rulesetIds": ["AFC-6GHZ-DEMO-1.1"]
        }

        self._call_jsonrpc(
            self.paws_url,
            method='spectrum.paws.getSpectrum',
            params={
                "antenna": {
                    "height": 25,
                    "heightType": "AMSL",
                    "heightUncertainty": 5
                },
                "capabilities": {
                    "indoorOutdoor": "INDOOR"
                },
                "deviceDesc": req_devicedesc,
                "location": {
                    "point": {
                        "center": {
                            "latitude": 40.75,
                            "longitude": -74
                        },
                        "orientation": 48,
                        "semiMajorAxis": 100,
                        "semiMinorAxis": 75
                    }
                },
                "type": "AVAIL_SPECTRUM_REQ",
                "version": "1.0"
            },
            expect_result={
                'version': '1.0',
                'type': 'AVAIL_SPECTRUM_RESP',
            },
        )

    def test_paws_resp_structure(self):
        afc_loc = self.guiconfig["afcconfig_defaults"]
        LOGGER.debug("cookies: %s, token: %s", self.cookies, self.login_token)
        resp = self.httpsession.get(
            self._resolve_url(afc_loc),
            headers={
                'Authorization': self.login_token},
            cookies=self.cookies)
        code = resp.status_code
        LOGGER.debug("status: %d, url: %s", code, self._resolve_url(afc_loc))
        if code == 404:
            raise SkipTest("AFC Config does not exist.")
        req_devicedesc = {
            "serialNumber": "sn-test",
            "rulesetIds": ["AFC-6GHZ-DEMO-1.1"]
        }

        response = self._call_jsonrpc(
            self.paws_url,
            method='spectrum.paws.getSpectrum',
            params={
                "antenna": {
                    "height": 25,
                    "heightType": "AMSL",
                    "heightUncertainty": 5
                },
                "capabilities": {
                    "indoorOutdoor": "INDOOR"
                },
                "deviceDesc": req_devicedesc,
                "location": {
                    "point": {
                        "center": {
                            "latitude": 40.75,
                            "longitude": -74
                        },
                        "orientation": 80,
                        "semiMajorAxis": 500,
                        "semiMinorAxis": 400
                    }
                },
                "type": "AVAIL_SPECTRUM_REQ",
                "version": "1.0"
            },
            expect_result={
                'version': '1.0',
                'type': 'AVAIL_SPECTRUM_RESP',
            },
        )
        result = response['result']

        self._test_paws_result_valid(result, req_devicedesc)
