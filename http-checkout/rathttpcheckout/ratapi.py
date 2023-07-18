''' Test cases related to PAWS API.
'''

from urlparse import urljoin
from .base import (ValidateJsonResponse, ValidateHtmlResponse, BaseTestCase)


class TestRatApi(BaseTestCase):
    ''' Test case to verify the RAT RESTful API.
    '''

    def setUp(self):
        BaseTestCase.setUp(self)

        # Get the actual endpoint URL
        resp = self.httpsession.head(
            self._resolve_url(''),
            allow_redirects=True
        )
        self.assertIn(resp.status_code, (200,))
        index_url = resp.url

        self.guiconfig_url = urljoin(index_url, '../ratapi/v1/guiconfig')
        resp = self.httpsession.get(self.guiconfig_url)
        self.guiconfig = resp.json()

    def tearDown(self):
        BaseTestCase.tearDown(self)

    def test_guiconfig_cache(self):
        self._test_path_contents(
            self.guiconfig_url,
            must_authorize=False,
            valid_encodings=None,
            require_etag=None,
            require_lastmod=None,
            validate_response_post=ValidateJsonResponse(),
        )


class TestUlsDb(BaseTestCase):
    ''' Test case to verify the ULS DB.
    '''

    def setUp(self):
        BaseTestCase.setUp(self)

        # Get the actual endpoint URL
        resp = self.httpsession.head(
            self._resolve_url(''),
            allow_redirects=True
        )
        self.assertIn(resp.status_code, (200,))
        index_url = resp.url

        self.uls_db_url = urljoin(index_url, '../ratapi/v1/files/uls_db')
        uls_resp = self.httpsession.get(self.uls_db_url)
        self.uls_db = uls_resp

        self.uls_csv_to_sql = urljoin(
            index_url, '../ratapi/v1/convert/uls/csv/sql/')

    def tearDown(self):
        BaseTestCase.tearDown(self)

    def test_webdav(self):
        self._test_path_contents(
            self.uls_db_url,
            must_authorize=False,
            require_etag=False,
            valid_encodings=None,
            require_lastmod=False,
            validate_response_post=ValidateHtmlResponse()
        )

    def test_links(self):
        self._test_working_links(self.uls_db.text)

    def test_bad_file(self):
        uls_db_url = self.uls_db_url + "/"
        self._test_path_contents(
            urljoin(uls_db_url, 'bad_file_name.csv'),
            must_authorize=False,
            require_etag=False,
            valid_encodings=None,
            require_lastmod=False,
            valid_status=[404]
        )


class TestAntennaPattern(BaseTestCase):
    ''' Test case to verify the Antenna Pattern.
    '''

    def setUp(self):
        BaseTestCase.setUp(self)

        # Get the actual endpoint URL
        resp = self.httpsession.head(
            self._resolve_url(''),
            allow_redirects=True
        )
        self.assertIn(resp.status_code, (200,))
        index_url = resp.url

        self.antenna_url = urljoin(
            index_url, '../ratapi/v1/files/antenna_pattern')
        antenna_pattern = self.httpsession.get(self.antenna_url)
        self.antenna_pattern = antenna_pattern

    def tearDown(self):
        BaseTestCase.tearDown(self)

    def test_webdav(self):
        self._test_path_contents(
            self.antenna_url,
            must_authorize=False,
            require_etag=False,
            valid_encodings=None,
            require_lastmod=False,
            validate_response_post=ValidateHtmlResponse()
        )

    def test_links(self):
        self._test_working_links(self.antenna_pattern.text)


class TestHistory(BaseTestCase):
    ''' Test case to verify the Histories.
    '''

    def setUp(self):
        BaseTestCase.setUp(self)

        # Get the actual endpoint URL
        resp = self.httpsession.head(
            self._resolve_url(''),
            allow_redirects=True
        )
        self.assertIn(resp.status_code, (200,))
        index_url = resp.url

        self.history_url = urljoin(index_url, '../ratapi/v1/history')
        history = self.httpsession.get(self.history_url)
        self.history = history

    def tearDown(self):
        BaseTestCase.tearDown(self)

    def test_webdav(self):
        self._test_path_contents(
            self.history_url,
            must_authorize=False,
            require_etag=False,
            valid_encodings=None,
            require_lastmod=False,
            validate_response_post=ValidateHtmlResponse()
        )

    def test_links(self):
        self._test_working_links(self.history.text)
