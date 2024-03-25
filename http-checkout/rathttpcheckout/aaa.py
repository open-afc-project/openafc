''' Test cases related to AAA functions (not the APIs that relate to them).
'''
import logging
from .base import (ValidateHtmlResponse, BaseTestCase)
import os

#: Logger for this module
LOGGER = logging.getLogger(__name__)


class TestUserLogin(BaseTestCase):

    def setUp(self):
        BaseTestCase.setUp(self)

    def tearDown(self):
        BaseTestCase.tearDown(self)

    def test_login_options(self):
        self._test_options_allow(
            self._resolve_url('user/sign-in'),
            {'GET', 'POST'}
        )

    def test_login_request(self):
        resp = self.httpsession.get(self._resolve_url('user/sign-in'))
        # now a location, managed by flask_login
        self.assertEqual(200, resp.status_code)
        self.assertTrue("csrf_token" in resp.content)

    def test_login_success(self):
        resp = self.httpsession.post(
            self._resolve_url('user/sign-in'),
        )
        self.assertEqual(200, resp.status_code)
        encoding = resp.headers.get("Content-Type")
        LOGGER.debug("Mah: %s", encoding)
        self.assertEqual("text/html; charset=utf-8", encoding)
        self.assertTrue('form' in resp.content)
        try:
            ValidateHtmlResponse()(resp)
        except Exception as err:
            self.fail('body is not valid html: {0}'.format(err))
