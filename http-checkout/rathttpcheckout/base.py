''' Non-test support objects and classes used by the actual test cases.
'''

import datetime
from io import BytesIO
import logging
import lxml.etree as etree
import os
import re
import requests
import shutil
import tempfile
import unittest
from urlparse import urljoin
import werkzeug.datastructures
import werkzeug.http
import werkzeug.urls
from nose import SkipTest

#: Logger for this module
LOGGER = logging.getLogger(__name__)

#: Regex to match application/xml and applicaiton/sub+xml
XML_CONTENT_RE = re.compile(r'^application/(.+\+)?xml$')

#: Time format for ISO 8601 "basic" time used by XML Schema.
#: This string is usable by datetime.strftime and datetime.strptime
TIME_FORMAT_BASIC = '%Y%m%dT%H%M%SZ'
#: Time format for ISO 8601 "extended" time used by XML Schema.
#: This string is usable by datetime.strftime and datetime.strptime
TIME_FORMAT_EXTENDED = '%Y-%m-%dT%H:%M:%SZ'

#: Absolute path to this package directory
PACKAGE_PATH = os.path.abspath(os.path.dirname(__file__))


def get_xml_parser(schema):
    ''' Generate a function to extract an XML DOM tree from an encoded document.
    
    :param schema: Iff not None, the document will be validated against
        this schema object.
    :type use_schema: bool
    :return: The parser function which takes a file-like parameter and
        returns a tree object of type :py:cls:`lxml.etree.ElementTree`.
    '''
    xmlparser = etree.XMLParser(schema=schema)

    def func(infile):
        try:
            return etree.parse(infile, parser=xmlparser)
        except etree.XMLSyntaxError as err:
            infile.seek(0)
            with tempfile.NamedTemporaryFile(delete=False) as outfile:
                shutil.copyfileobj(infile, outfile)
                raise ValueError('Failed to parse XML with error {0} in file {1}'.format(err, outfile.name))

    return func


def extract_metadict(doc):
    ''' Extract a server metadata dictionary from its parsed XML document.
    
    :param doc: The document to read from.
    :return: The metadata URL map.
    '''
    metadict = {}
    for el_a in doc.findall('//{http://www.w3.org/1999/xhtml}a'):
        m_id = el_a.attrib.get('id')
        m_href = el_a.attrib.get('href')
        if m_id is None or m_href is None:
            continue
        metadict[m_id] = m_href
    return metadict


def merged(base, delta):
    ''' Return a merged dictionary contents.
    
    :param base: The initial contents to merge.
    :param delta: The modifications to apply.
    :return: A dictionary containing the :py:obj:`base` updated 
        by the :py:obj:`delta`.
    '''
    mod = dict(base)
    mod.update(delta)
    return mod


def limit_count(iterable, limit):
    ''' Wrap an iterable/generator with a count limit to only yield the first
    :py:obj:`count` number of items.
    
    :param iterable: The source iterable object.
    :param limit: The maximum number of items available from the generator.
    :return A generator with a count limit.
    '''
    count = 0
    for item in iterable:
        yield item
        count += 1
        if count >= limit:
            return


def modify_etag(orig):
    ''' Given a base ETag value, generate a modified ETag which is 
    guaranteed to not match the original.
    
    :param str orig: The original ETag.
    :return: A different ETag
    '''
    # Inject characters near the end
    mod = orig[:-1] + '-eh' + orig[-1:]
    return mod


class ValidateJsonResponse(object):
    ''' Validate an expected JSON file response.
    '''

    def __call__(self, resp):
        import json
        json.loads(resp.content)


class ValidateHtmlResponse(object):
    ''' Validate an HTML response with a loose parser.
    '''

    def __call__(self, resp):
        import bs4
        kwargs = dict(
            markup=resp.content,
            features='lxml',
        )
        bs4.BeautifulSoup(**kwargs)


class ValidateXmlResponse(object):
    ''' Validate an expected XML file response.
    
    :param parser: A function to take a file-like input and output an
        XML element tree :py:cls:`lxml.etree.ElementTree`.
    :param require_root: If not None, the root element must be this value.
    '''

    def __init__(self, parser, require_root=None):
        self._parser = parser
        if not callable(self._parser):
            raise ValueError('ValidateXmlResponse parser invalid')
        self._require_root = require_root

    def __call__(self, resp):
        xml_tree = self._parser(BytesIO(resp.content))
        if self._require_root is not None:
            root_tag = xml_tree.getroot().tag
            if self._require_root != root_tag:
                raise ValueError('Required root element "{0}" not present, got "{1}"'.format(self._require_root, root_tag))


class BaseTestCase(unittest.TestCase):
    ''' Common access and helper functions which use the :py:mod:`unittest` 
    framework but this class defines no test functions itself.
    
    :ivar httpsession: An :py:class:`requests.Session` instance for test use.
    :ivar xmlparser: An :py:class:`etree.XMLParser` instance for test use.
    '''

    #: Cached URL to start at and to resolve from
    BASE_URL = os.environ.get('HTTPCHECKOUT_BASEURL')
    #: True if the editing tests should be skipped
    READONLY = bool(os.environ.get('HTTPCHECKOUT_READONLY'))
    #: Keep any created resources in the tearDown() method which are left behind
    KEEP_TESTITEMS = bool(os.environ.get('HTTPCHECKOUT_KEEP_TESTITEMS'))

    def setUp(self):
        unittest.TestCase.setUp(self)
        self.maxDiff = 10e3

        self.assertIsNotNone(self.BASE_URL, 'Missing environment HTTPCHECKOUT_BASEURL')
        self.httpsession = requests.Session()

        ca_roots = os.environ.get('HTTPCHECKOUT_CACERTS')
        LOGGER.info('HTTPCHECKOUT_CACERTS is "%s"', ca_roots)
        if ca_roots == '':
            import warnings
            from urllib3.exceptions import InsecureRequestWarning
            self.httpsession.verify = False
            warnings.filterwarnings("ignore", category=InsecureRequestWarning)

    def tearDown(self):
        self.httpsession = None
        unittest.TestCase.tearDown(self)

    def _assertWriteTest(self):
        ''' Skip the current test if HTTPCHECKOUT_READONLY is set.
        '''
        if self.READONLY:
            self.skipTest('Not running editing tests because of HTTPCHECKOUT_READONLY')

    def _resolve_url(self, url):
        ''' Resolve a URL relative to the original base URL.
        
        :param url: The URL to resolve.
        :type url: str
        :return: The resolved absolute URL to request on.
        :rtype: str
        '''
        return urljoin(self.BASE_URL, url)

    def assertSameUrlPath(self, first, second):
        ''' Assert that two URLs are equal except for their query/fragment parts.
        '''
        f_url = werkzeug.urls.url_parse(first)
        s_url = werkzeug.urls.url_parse(second)

        for attr in ('scheme', 'netloc', 'path'):
            self.assertEqual(
                getattr(f_url, attr),
                getattr(s_url, attr),
                'Mismatched URL {0}'.format(attr)
            )

    def _get_xml_parser(self, use_schema=None):
        ''' Generate a function to extract an XML DOM tree from an encoded document.
        
        :return: The parser function which takes a file-like parameter and
            returns a tree object of type :py:cls:`lxml.etree.ElementTree`.
        '''
        return get_xml_parser(schema=use_schema)

    def _get_xml_encoder(self):
        ''' Generate a function to encode a document from an XML DOM tree.
        
        :return: The parser function which takes a parameter of a
            tree object of type :py:cls:`lxml.etree.ElementTree` and
            returns a file-like object.
        '''

        def func(doc, outfile=None):
            ''' Encode a document.

            :parm doc: The document to encode.
            :type doc: :py:cls:`lxml.etree.ElementTree`
            :param outfile: An optional file-like object to encode into.
                This must be None if the encoder is used multiple times.
            :type outfile: file-like or None
            :return: The encoded file-like object.
            '''
            if outfile is None:
                outfile = BytesIO()

            doc.write(outfile, encoding='UTF-8', xml_declaration=True)
            if hasattr(outfile, 'seek'):
                try:
                    outfile.seek(0)
                except:
                    pass
            return outfile

        return func

    def _test_working_links(self, text):
        ''' Verify that html has well formed links

        :param text: html doc with href's
        '''

        import bs4
        html = bs4.BeautifulSoup(text, 'html.parser')
        for l in [ a['href'] for a in html.find_all('a')]:
            self._test_working_link(l)

    def _test_working_link(self, url):
        ''' Verify that a url returns a 200 response
        
        :param url: The URL to be checked
        '''

        resolved_url = self._resolve_url(url)
        resp = self.httpsession.get(resolved_url)
        self.assertEqual(200, resp.status_code)

    def _test_options_allow(self, url, methods):
        ''' Verify that the OPTIONS response for a URL matches a specific set.
        
        :param url: The URL to pass to :py:mod:`requests`
        :type url: str
        :param methods: The method names which must be identical to the response.
        :type methods: iterable
        :param params: URL parameter dictionary to pass to :py:mod:`requests`.
        :type params: dict or None
        '''
        methods = set([str(m).upper() for m in methods])
        methods.add('OPTIONS')
        if 'GET' in methods:
            methods.add('HEAD')

        resolved_url = self._resolve_url(url)
        resp = self.httpsession.options(resolved_url)
        self.assertEqual(200, resp.status_code)
        got_allow = werkzeug.http.parse_set_header(resp.headers['allow'])
        self.assertEqual(methods, set(got_allow))

    def _test_path_contents(self, url, params=None, base_headers=None,
                            validate_response_pre=None, validate_response_post=None,
                            must_authorize=True,
                            valid_status=None, content_type=None,
                            valid_encodings=None,
                            require_length=True,
                            require_vary=None, require_etag=True,
                            require_lastmod=True, require_cacheable=True,
                            cache_must_revalidate=False):
        ''' Common assertions for static resources.
        
        :param url: The URL to pass to :py:mod:`requests`
        :type url: str
        :param params: URL parameter dictionary to pass to :py:mod:`requests`.
        :type params: dict or None
        :param base_headers: A dictionary of headers to send with every request.
        :type base_headers: dict or None
        :param validate_response_pre: A callable which takes a single argument of
            the response object and performs its own validation of the headers
            and/or body for each non-cached response.
            This is performed before any of the parametric checks.
        :type validate_response_pre: callable or None
        :param validate_response_post: A callable which takes a single argument of
            the response object and performs its own validation of the headers
            and/or body for each non-cached response.
            This is performed after any of the parametric checks.
        :type validate_response_post: callable or None
        :param must_authorize: Access to the resource without an Authorization
            header is attempted and compared against this value.
        :type must_authorize: bool
        :param valid_status: A set of valid status codes to allow.
            If not provided, only code 200 is valid.
        :type valid_status: set or None
        :param content_type: If not None, the required Content-Type header.
        :type content_type: bool or None
        :param valid_encodings: If not None, a list of content encodings to check for.
            The resource must provide each of the non-identity encodings listed.
        :type valid_encodings: list or None
        :param require_length: If either true or false, assert that the 
            content-length header is present or not.
        :type require_length: bool or None
        :param require_vary: A set of Vary reults required to be present
            in the response.
            If the :py:obj:`valid_encodings` list has more than the identity 
            encoding present, then 'accept-encoding' will be automatically 
            added to this vary list.
        :type require_vary: list or None
        :param require_etag: If not None, whether the ETag is required present 
            or not present (True or False) or a specific string value.
        :type require_etag: str or bool or None
        :param require_lastmod: If not None, whether the Last-Modified is 
            required present or not present (True or False) or a specific value.
        :type require_lastmod: str or bool or None
        :param require_cacheable: If true, the resource is checked for its cacheability.
            Not all resources should be cacheable (even if not explicitly marked no-cache).
        :type require_cacheable: bool
        :param cache_must_revalidate: If True, the response must have its
            'must-revalidate' cache control header set.
        :type cache_must_revalidate: bool
        :raises: raises unittest exceptions if an assertion fails
        '''

        if base_headers is None:
            base_headers = {}

        if valid_status is None:
            valid_status = [200]
        valid_status = set(valid_status)

        # Set of valid encodings to require
        if valid_encodings is None:
            valid_encodings = []
        valid_encodings = set(valid_encodings)
        valid_encodings.add('identity')
        # Force as ordered list with identity encoding first and gzip always attempted
        try_encodings = set(valid_encodings)
        try_encodings.discard('identity')
        try_encodings = sorted(list(try_encodings))
        try_encodings.insert(0, 'identity')
        # Cached identity-encoded contents
        identity_body = None

        if require_vary is None:
            require_vary = []
        require_vary = set(require_vary)
        if len(valid_encodings) > 1:
            require_vary.add('accept-encoding')

        resolved_url = self._resolve_url(url)

        # Options on the resource itself
        resp = self.httpsession.options(
            resolved_url, params=params,
            allow_redirects=False,
            headers=base_headers,
        )
        self.assertEqual(200, resp.status_code)
        got_allow = werkzeug.http.parse_set_header(resp.headers.get('allow'))
        self.assertIn('options', got_allow)
        if 404 not in valid_status:
            self.assertIn('head', got_allow)
            self.assertIn('get', got_allow)

        # Options without authentication
        resp = self.httpsession.options(
            resolved_url, params=params,
            allow_redirects=False,
            headers=merged(base_headers, {
                'authorization': None,
            }),
        )
        if must_authorize:
            self.assertEqual(401, resp.status_code)
        else:
            self.assertEqual(200, resp.status_code)

        for try_encoding in try_encodings:
            #initial non-cache response
            enc_headers = merged(
                base_headers,
                {
                    'accept-encoding': try_encoding,
                }
            )
            resp = self.httpsession.get(
                resolved_url, params=params,
                allow_redirects=False,
                headers=enc_headers,
                stream=True,
            )
            # External validation first
            if validate_response_pre:
                try:
                    validate_response_pre(resp)
                except Exception as err:
                    self.fail('Failed pre-validation: {0}'.format(err))

            # Now parametric validation
            self.assertIn(resp.status_code, valid_status)

            got_content_type = werkzeug.http.parse_options_header(resp.headers['content-type'])
            if content_type is not None:
                self.assertEqual(content_type.lower(), got_content_type[0].lower())

            # Encoding comparison compared to valid
            got_encoding = resp.headers.get('content-encoding', 'identity')
            if try_encoding in valid_encodings:
                self.assertEqual(try_encoding, got_encoding)
            else:
                self.assertEqual(
                    'identity', got_encoding,
                    msg='"{0}" was supposed to be a disallowed content-encoding but it was accepted'.format(try_encoding)
                )

            got_length = resp.headers.get('content-length')
            if require_length is True:
                self.assertIsNotNone(got_length, msg='Content-Length missing')
            elif require_length is False:
                self.assertIsNone(got_length, msg='Content-Length should not be present')

            # Guarantee type is correct also
            if got_length is not None:
                try:
                    got_length = int(got_length)
                except ValueError:
                    self.fail('Got a non-integer Content-Length: {0}'.format(got_length))

            got_vary = werkzeug.http.parse_set_header(resp.headers.get('vary'))
            for item in require_vary:
                LOGGER.debug("headers: %s", resp.headers)
                self.assertIn(item, got_vary, msg='Vary header missing item "{0}" got {1}'.format(item, got_vary))

            got_etag = resp.headers.get('etag')
            got_lastmod = resp.headers.get('last-modified')
            if resp.status_code != 204:
                if require_etag is True:
                    self.assertIsNotNone(got_etag, msg='ETag header missing')
                elif require_etag is False:
                    self.assertIsNone(got_etag, msg='ETag header should not be present')
                elif require_etag is not None:
                    self.assertEqual(require_etag, got_etag)

                if require_lastmod is True:
                    self.assertIsNotNone(got_lastmod, msg='Last-Modified header missing')
                elif require_lastmod is False:
                    self.assertIsNone(got_lastmod, msg='Last-Modified header should not be present')
                elif require_lastmod is not None:
                    self.assertEqual(require_lastmod, got_lastmod)

            # Caching headers
            cache_control = werkzeug.http.parse_cache_control_header(
                resp.headers.get('cache-control'),
                cls=werkzeug.datastructures.ResponseCacheControl,
            )
            # The resource must define its domain
            if False:
                self.assertTrue(
                    cache_control.no_cache
                    or cache_control.public # pylint: disable=no-member
                    or cache_control.private, # pylint: disable=no-member
                    msg='Missing cache public/private assertion for {0}'.format(resolved_url)
                )
            if require_cacheable is not False and cache_must_revalidate is True:
                self.assertTrue(cache_control.must_revalidate) # pylint: disable=no-member
            if require_cacheable is True:
                self.assertFalse(cache_control.no_cache)
                self.assertFalse(cache_control.no_store)
#                self.assertLessEqual(0, cache_control.max_age)
            elif require_cacheable is False:
                #FIXME not always true
                self.assertTrue(cache_control.no_cache)

            # Actual body content itself
            got_body = str(resp.content)
            if resp.status_code == 204:
                self.assertEqual('', got_body)
            else:
                # Ensure decoded body is identical
                if got_encoding == 'identity':
                    identity_body = got_body
                    self.assertIsNotNone(identity_body)
                    if got_length is not None:
                        self.assertEqual(len(identity_body), got_length)
                else:
                    self.assertEqual(identity_body, got_body)

                # XML specific decoding
                if XML_CONTENT_RE.match(got_content_type[0]) is not None and validate_response_post is None:
                    validate_response_post = ValidateXmlResponse(self._get_xml_parser(use_schema=True))

                # After all parametric tests on this response
                if validate_response_post:
                    try:
                        validate_response_post(resp)
                    except Exception as err:
                        self.fail('Failed post-validation: {0}'.format(err))

            # Check the unauthorized view of same URL
            for method in ('GET', 'HEAD'):
                resp = self.httpsession.request(
                    method,
                    resolved_url, params=params,
                    allow_redirects=False,
                    headers=merged(enc_headers, {
                        'authorization': None,
                    }),
                )
                if must_authorize:
                    self.assertEqual(401, resp.status_code, msg='For {0} on {1}: Expected 401 status got {2}'.format(method, resolved_url, resp.status_code))
                else:
                    self.assertIn(resp.status_code, valid_status, msg='For {0} on {1}: Expected valid status got {2}'.format(method, resolved_url, resp.status_code))

            # Any resource with cache control header
            resp = self.httpsession.head(
                resolved_url, params=params,
                allow_redirects=False,
                headers=merged(enc_headers, {
                    'if-match': '*',
                }),
            )
            self.assertIn(resp.status_code, valid_status)
            # Caching with ETag
            if got_etag is not None:
                self.assertIn(resp.status_code, valid_status)
                # Existing resource
                resp = self.httpsession.head(
                    resolved_url, params=params,
                    allow_redirects=False,
                    headers=merged(enc_headers, {
                        'if-match': got_etag,
                    }),
                )
                self.assertIn(resp.status_code, valid_status)
                # Client cache response
                resp = self.httpsession.head(
                    resolved_url, params=params,
                    allow_redirects=False,
                    headers=merged(enc_headers, {
                        'if-none-match': got_etag,
                    }),
                )
                self.assertIn(resp.status_code, [304] if require_cacheable else valid_status)
                # With adjusted ETag
                mod_etag = modify_etag(got_etag)
                resp = self.httpsession.head(
                    resolved_url, params=params,
                    allow_redirects=False,
                    headers=merged(enc_headers, {
                        'if-none-match': mod_etag,
                    }),
                )
                self.assertIn(resp.status_code, valid_status)

            # Caching with Last-Modified
            if got_lastmod is not None:
                # No changes here so normal response
                resp = self.httpsession.head(
                    resolved_url, params=params,
                    allow_redirects=False,
                    headers=merged(enc_headers, {
                        'if-unmodified-since': got_lastmod,
                    }),
                )
#                self.assertIn(resp.status_code, valid_status)

                # An earlier time will give a 412
                new_time = werkzeug.http.parse_date(got_lastmod) - datetime.timedelta(seconds=5)
                resp = self.httpsession.head(
                    resolved_url, params=params,
                    allow_redirects=False,
                    headers=merged(enc_headers, {
                        'if-unmodified-since': werkzeug.http.http_date(new_time),
                    }),
                )
                self.assertIn(resp.status_code, [412] if require_cacheable else valid_status)

                # An later time is normal response
                new_time = werkzeug.http.parse_date(got_lastmod) + datetime.timedelta(seconds=5)
                resp = self.httpsession.head(
                    resolved_url, params=params,
                    allow_redirects=False,
                    headers=merged(enc_headers, {
                        'if-unmodified-since': werkzeug.http.http_date(new_time),
                    }),
                )
                self.assertIn(resp.status_code, valid_status)

                # Client cache response
                resp = self.httpsession.head(
                    resolved_url, params=params,
                    allow_redirects=False,
                    headers=merged(enc_headers, {
                        'if-modified-since': got_lastmod,
                    }),
                )
#                self.assertIn(resp.status_code, [304] if require_cacheable else valid_status)

                # A later time should also give a 304 response
                new_time = werkzeug.http.parse_date(got_lastmod) + datetime.timedelta(seconds=5)
                resp = self.httpsession.head(
                    resolved_url, params=params,
                    allow_redirects=False,
                    headers=merged(enc_headers, {
                        'if-modified-since': werkzeug.http.http_date(new_time),
                    }),
                )
                self.assertIn(resp.status_code, [304] if require_cacheable else valid_status)

                # An earlier time will give a 200 response
                new_time = werkzeug.http.parse_date(got_lastmod) - datetime.timedelta(seconds=5)
                resp = self.httpsession.head(
                    resolved_url, params=params,
                    allow_redirects=False,
                    headers=merged(enc_headers, {
                        'if-modified-since': werkzeug.http.http_date(new_time),
                    }),
                )
                self.assertIn(resp.status_code, valid_status)

    def _test_modify_request(self, url, up_data, method='POST', params=None, **kwargs):
        ''' Common assertions for static resources.
        
        :param url: The URL to pass to :py:mod:`requests`
        :type url: str
        :param up_data: The request body data.
        :type up_data: str or file-like
        :param str method: The method to request the modification.
        :param params: URL parameter dictionary to pass to :py:mod:`requests`.
        :type params: dict or None
        :param base_headers: A dictionary of headers to send with every request.
        :type base_headers: dict or None
        :param must_authorize: Access to the resource without an Authorization
            header is attempted and compared against this value.
        :type must_authorize: bool
        :param valid_status: A set of valid status codes to allow.
            If not provided, only code 201 is valid for new resources and 204 for existing ones.
        :type valid_status: set or None
        :param empty_is_valid: Whether or not an empty document is a valid modification.
            The default is False.
        :type empty_is_valid: bool
        :param is_idempotent: Whether or not sending the same request should 
            not change the resource (except for the modify time).
            The default is true if the method is PUT.
        :type is_idempotent: bool
        :param require_etag: If not None, whether the ETag is required present or not present.
        :type require_etag: bool or None
        '''
        method = method.lower()
        # Arguments not passed to _assert_modify_response()
        base_headers = kwargs.pop('base_headers', None)
        if base_headers is None:
            base_headers = {}
        must_authorize = kwargs.pop('must_authorize', True)
        empty_is_valid = kwargs.pop('empty_is_valid', False)
        is_idempotent = kwargs.pop('is_idempotent', method == 'put')

        resolved_url = self._resolve_url(url)

        if hasattr(up_data, 'seek'):

            def reset_up_data():
                up_data.seek(0)
                return up_data

        else:

            def reset_up_data():
                return up_data

        # Options on the resource itself
        resp = self.httpsession.options(
            resolved_url, params=params,
            allow_redirects=False,
            headers=base_headers,
        )
        self.assertEqual(200, resp.status_code)
        got_allow = werkzeug.http.parse_set_header(resp.headers['allow'])
        self.assertIn('options', got_allow)
        self.assertIn(method, got_allow)
        # Options without authentication
        resp = self.httpsession.options(
            resolved_url, params=params,
            allow_redirects=False,
            headers=merged(base_headers, {
                'authorization': None,
            }),
        )
        if must_authorize:
            self.assertEqual(401, resp.status_code)
        else:
            self.assertEqual(200, resp.status_code)

        # Initial state for conditions
        resp_head = self.httpsession.head(
            resolved_url, params=params,
            headers={'accept-encoding': 'identity'},
        )
        init_status = resp_head.status_code
        self.assertIn(init_status, {200, 404})
        init_etag = resp_head.headers.get('etag')

        if init_status == 200:
            # Replacing resource
            if kwargs.get('valid_status') is None:
                kwargs['valid_status'] = [204]
            match_etag = init_etag if init_etag else '*'
            add_headers_fail = {'if-none-match': match_etag}
            add_headers_good = {'if-match': match_etag}

        elif init_status == 404:
            # New resource
            if kwargs.get('valid_status') is None:
                kwargs['valid_status'] = [201]
            add_headers_fail = {'if-match': '*'}
            add_headers_good = {'if-none-match': '*'}

        if not empty_is_valid:
            # Invalid header content
            resp = self.httpsession.request(
                method, resolved_url, params=params,
            )
            self.assertEqual(415, resp.status_code)
            # Invalid (empty) body content
            resp = self.httpsession.request(
                method, resolved_url, params=params,
                headers=base_headers,
            )
            self.assertEqual(415, resp.status_code)

        # Check precondition failure
        resp = self.httpsession.request(
            method, resolved_url, params=params,
            headers=merged(base_headers, add_headers_fail),
            data=reset_up_data(),
        )
        self.assertEqual(412, resp.status_code)

        if must_authorize:
            # Unauthorized access with otherwise valid request
            resp = self.httpsession.request(
                method, resolved_url, params=params,
                headers=merged(base_headers, {
                    'authorization': None,
                }),
                data=reset_up_data(),
            )
            self.assertEqual(401, resp.status_code)

        # Actual modifying request
        resp_mod = self.httpsession.request(
            method, resolved_url, params=params,
            headers=merged(base_headers, add_headers_good),
            data=reset_up_data(),
        )
        self._assert_modify_response(resp_mod, **kwargs)
        got_modtime = resp_mod.headers.get('last-modified')
        got_etag = resp_mod.headers.get('etag')

        # Verify the same info is present in new HEAD reply
        resp_head = self.httpsession.head(
            resolved_url, params=params,
            headers={'accept-encoding': 'identity'},
        )
        self.assertEqual(200, resp_head.status_code)
        self.assertEqual(got_modtime, resp_head.headers.get('last-modified'))
        self.assertEqual(got_etag, resp_head.headers.get('etag'))

        if is_idempotent:
            # Check a duplicate request
            add_headers_good = {'if-match': got_etag}
            kwargs['valid_status'] = [204]
            resp_mod = self.httpsession.request(
                method, resolved_url, params=params,
                headers=merged(base_headers, add_headers_good),
                data=reset_up_data(),
            )
            self._assert_modify_response(resp_mod, **kwargs)
            self.assertEqual(got_etag, resp_mod.headers.get('etag'))

        # Give back the final valid response
        return resp_mod

    def _assert_modify_response(self, resp, valid_status=None,
                                require_etag=True, require_lastmod=True,
                                old_etag=None):
        ''' Verify the contents of a response to HTTP modification with no body.
        
        :param resp: The response object to check.
        :type resp: :py:cls:`requests.Response`
        :param valid_status: A set of valid status codes to allow.
            If not provided, only codes (200, 201, 204) are valid.
        :type valid_status: set or None
        :param require_etag: If not None, whether the ETag is required present 
            or not present (True or False) or a specific string value.
        :type require_etag: str or bool or None
        :param require_lastmod: If not None, whether the Last-Modified is 
            required present or not present (True or False) or a specific value.
        :type require_lastmod: str or bool or None
        :param old_etag: An optional old ETag value to compare against.
            The new response must have a different ETag value than this.
        :type old_etag: str or None
        '''
        if valid_status is None:
            valid_status = [200, 201, 204]
        valid_status = set(valid_status)

        self.assertIn(resp.status_code, valid_status)
        got_lastmod = resp.headers.get('last-modified')
        got_etag = resp.headers.get('etag')

        if require_etag is True:
            self.assertIsNotNone(got_etag, msg='ETag header missing')
        elif require_etag is False:
            self.assertIsNone(got_etag, msg='ETag header should not be present')
        elif require_etag is not None:
            self.assertEqual(require_etag, got_etag)

        if require_lastmod is True:
            self.assertIsNotNone(got_lastmod, msg='Last-Modified header missing')
        elif require_lastmod is False:
            self.assertIsNone(got_lastmod, msg='Last-Modified header should not be present')
        elif require_lastmod is not None:
            self.assertEqual(require_lastmod, got_lastmod)

        if old_etag is not None:
            self.assertNotEqual(old_etag, got_etag)

        # Empty body
        self.assertFalse(bool(resp.content))

class UserLoginBaseTestCase(BaseTestCase):
    """Wraps tests in login/logout flow
    
    Encapsulates login/logout wrapping of tests. 
    Tests that require authentication will need to use the saved login_token and cookies as headers in their requests
    """
    #: User name to test as this assumes a user HTTPCHECKOUT_ACCTNAME already exists in the User DB
    VALID_ACCTNAME = os.environ.get('HTTPCHECKOUT_ACCTNAME', 'admin').decode('utf8')
    VALID_PASSPHRASE = os.environ.get('HTTPCHECKOUT_PASSPHRASE', 'admin').decode('utf8')

    def __init__(self, *args, **kwargs):
        BaseTestCase.__init__(self, *args, **kwargs)
        self.login_token = None
        self.cookies = None

    def setUp(self):
        BaseTestCase.setUp(self)
        resp = self.httpsession.post(self._resolve_url('auth/login'), json={'email': self.VALID_ACCTNAME, 'password': self.VALID_PASSPHRASE})
        LOGGER.debug('code: %s, login headers: %s', resp.status_code, resp.content)
        self.cookies = resp.cookies
        if resp.status_code == 404:
            self.fail(msg="{} not found on this server.".format(self._resolve_url('auth/login')))
        try:
            self.login_token = resp.json()["token"]
        except ValueError:
            raise SkipTest("Could not login as {}".format(self.VALID_ACCTNAME))

    def tearDown(self):
        resp = self.httpsession.post(self._resolve_url('auth/logout'), params={'Authorization': self.login_token})
        LOGGER.debug('response code: %d\nbody: %s', resp.status_code, resp.content)
        self.login_token = None
        self.cookies = None
        BaseTestCase.tearDown(self)