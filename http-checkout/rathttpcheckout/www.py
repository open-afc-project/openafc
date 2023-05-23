''' Test cases related to Web pages (not the APIs used by them).
'''

import logging
from .base import (ValidateHtmlResponse, BaseTestCase)

#: Logger for this module
LOGGER = logging.getLogger(__name__)


class TestWebApp(BaseTestCase):
    ''' Test case to verify the PAWS JSON-RPC endpoint.
    '''

    def setUp(self):
        BaseTestCase.setUp(self)

    def tearDown(self):
        BaseTestCase.tearDown(self)

    def test_root_redirect(self):
        resp = self.httpsession.head(self._resolve_url(''))
        self.assertEqual(302, resp.status_code)
        self.assertEqual(self._resolve_url('www/index.html'),
                         resp.headers.get('location'))

    def test_html_app(self):
        self._test_path_contents(
            self._resolve_url('www/index.html'),
            valid_encodings=None,
            content_type='text/html',
            must_authorize=False,
            require_etag=True,
            require_lastmod=True,
            require_cacheable=True,
            validate_response_post=ValidateHtmlResponse(),
        )

    def test_guiconfig(self):
        required_keys = frozenset([
            'afcconfig_defaults',
            'uls_convert_url',
            'login_url',
            'paws_url',
            'history_url',
            'admin_url',
            'user_url',
            'ap_deny_admin_url',
            'rat_api_analysis',
            'version',
            'antenna_url',
            'google_apikey',
            'uls_url'
        ])
        resp = self.httpsession.get(self._resolve_url('ratapi/v1/guiconfig'))
        encoding = resp.headers["Content-Type"]
        self.assertEqual("application/json", encoding)
        try:
            parsed_body = resp.json()
        except ValueError:
            self.fail("Body is not valid JSON.")

        missing_keys = required_keys - frozenset(parsed_body.keys())
        if missing_keys:
            self.fail('Required keys: {0}'.format(' '.join(required_keys)))

        non_200_eps = {}
        for value in parsed_body.values():
            resp = self.httpsession.options(self._resolve_url(value))
            LOGGER.debug("Verifying status of %s", self._resolve_url(value))
            if resp.status_code != 200:
                non_200_eps[value] = resp.status_code
            self.assertEqual(
                {}, non_200_eps, msg="{}, were defined in GUI config as required endpoint(s) but returned non-200 status on OPTIONS".format(non_200_eps))
