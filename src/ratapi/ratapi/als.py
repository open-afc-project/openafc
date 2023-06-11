# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

""" AFC and JSON logging to PostgreSQL """

import collections
import datetime
import json
import logging
import os
import re
import sys
import threading

# pylint: disable=global-statement

LOGGER = logging.getLogger(__name__)

import_failure = None
try:
    import dateutil.tz
    import kafka
    import kafka.errors
except ImportError as ex:
    import_failure = str(ex)

# Topic name for AFC request/response logging
ALS_TOPIC = "ALS"
# AFC Config/Request/Response logging record format version
ALS_FORMAT_VERSION = "1.0"
# Data type value for AFC Config record
ALS_DT_CONFIG = "AFC_CONFIG"
# Data type value for AFC Request record
ALS_DT_REQUEST = "AFC_REQUEST"
# Data type value for AFC Response record
ALS_DT_RESPONSE = "AFC_RESPONSE"

# Version field of AFC Config/Request/Response record
ALS_FIELD_VERSION = "version"
# AFC Server ID field of AFC Config/Request/Response record
ALS_FIELD_SERVER_ID = "afcServer"
# Timetag field of AFC Config/Request/Response record
ALS_FIELD_TIME = "time"
# Data type (ALS_DT_...) field of AFC Config/Request/Response record
ALS_FIELD_DATA_TYPE = "dataType"
# JSON data field of AFC Config/Request/Response record
ALS_FIELD_DATA = "jsonData"
# Customer name field of AFC Config/Request/Response record
ALS_FIELD_CUSTOMER = "customer"
# Geodetic data version field of AFC Config/Request/Response record
ALS_FIELD_GEO_DATA = "geoDataVersion"
# ULS data ID field of AFC Config/Request/Response record
ALS_FIELD_ULS_ID = "ulsId"
# Request indices field of AFC Config/Request/Response record
ALS_FIELD_REQ_INDICES = "requestIndexes"

# JSON log record format version
JSON_LOG_VERSION = "1.0"

# Version field of JSON log record
JSON_LOG_FIELD_VERSION = "version"
# AFC Server ID field of JSON log record
JSON_LOG_FIELD_SERVER_ID = "source"
# Timetag field of JSON log record
JSON_LOG_FIELD_TIME = "time"
# JSON data field of JSON log record
JSON_LOG_FIELD_DATA = "jsonData"

# Delay between connection attempts
CONNECT_ATTEMPT_INTERVAL = datetime.timedelta(seconds=10)

# Regular expression for topic name check (Postgre requirement to table names)
TOPIC_NAME_REGEX = re.compile(r"^[_a-zA-Z][0-9a-zA-Z_]{,62}$")

# True for Python 3, False for Python 2
IS_PY3 = sys.version_info.major == 3


def to_bytes(s):
    """ Converts string to bytes """
    if s is None:
        return None
    return s.encode("utf-8") if IS_PY3 else s


def to_str(s):
    """ Converts bytes to string """
    if (s is None) or isinstance(s, str):
        return s
    return s.decode("utf-8", errors="backslashreplace") if IS_PY3 else s


def timetag():
    """ Timetag with timezone """
    return datetime.datetime.now(tz=dateutil.tz.tzlocal()).isoformat()


def random_hex(n):
    """ Generates strongly random n-byte sequence and returns its hexadecimal
    representation """
    return "".join("%02X" % (b if IS_PY3 else ord(b)) for b in os.urandom(n))


class ArgDsc:
    """ KafkaProducer constructor argument descriptor

    Private attributes:
    _env_var        -- Environment variable containing value for parameter
    _arg            -- Parameter name
    _type_conv      -- Function to convert parameter value from string
    _list_separator -- None for scalar argument, separator for list argument
    _required       -- True if this parameter required for KafkaProducer
                       initialization and its absence will lead to no logging
    """
    def __init__(self, env_var, arg, type_conv=str, list_separator=None,
                 required=False, default=None):
        """ Constructor

        Arguments:
        env_var        -- Environment variable containing value for parameter
        arg            -- Parameter name
        type_conv      -- Function to convert parameter value from string
        list_separator -- None for scalar argument, separator for list argument
        required       -- True if this parameter required for KafkaProducer
                          initialization and its absence will lead to no
                          logging
        default        -- None or default value
        """
        self._env_var = env_var
        self._arg = arg
        self._type_conv = type_conv
        self._list_separator = list_separator
        self._required = required
        self._default = default

    @property
    def env_var(self):
        """ Name of environment variable """
        return self._env_var

    def from_env(self, kwargs):
        """ Tries to read argument from environment

        Arguments:
        kwargs -- Argument dictionary for KafkaProducer constructor to add
                  argument to
        Returns True if there is no reason to abandon reading arguments from
        environment
        """
        env_val = os.environ.get(self._env_var)
        if not env_val:
            if self._default is not None:
                kwargs[self._arg] = self._default
            return not self._required
        kwargs[self._arg] = \
            [self._type_conv(v) for v in env_val.split(self._list_separator)] \
            if self._list_separator else self._type_conv(env_val)
        return True

    @classmethod
    def str_or_int_type_conv(cls, str_val):
        """ Type converter for argument that can be string or integer """
        try:
            return int(str_val)
        except (TypeError, ValueError):
            return str_val

    @classmethod
    def bool_type_conv(cls, str_val):
        """ Type converter for boolean argument """
        if str_val.lower() in ("+", "1", "yes", "true", "on"):
            return True
        if str_val.lower() in ("-", "0", "no", "false", "off"):
            return False
        raise ValueError("Wrong format of boolean Kafka parameter")


# Descriptors of KafkaProducer arguments
arg_dscs = [
    # Default value for stem part of "client_id" parameter passed to
    # KafkaProducer and server ID used in messages
    ArgDsc("ALS_KAFKA_SERVER_ID", "client_id"),
    # Comma-separated list of Kafka (bootstrap) servers in form of 'hostname'
    # or 'hostname:port' (default port is 9092). If not specified ALS logging
    # is not performed
    ArgDsc("ALS_KAFKA_CLIENT_BOOTSTRAP_SERVERS", "bootstrap_servers",
           list_separator=",", required=True),
    # Number of Kafka confirmations before operation completion.
    # Valid values: '0' (fire and forget), '1', 'all'. Default is 0 (different
    # from correspondent KafkaProducer constructor parameter - which is 1)
    ArgDsc("ALS_KAFKA_CLIENT_ACKS", "acks",
           type_conv=ArgDsc.str_or_int_type_conv, default=0),
    # Number of retries. Default is 0
    ArgDsc("ALS_KAFKA_CLIENT_RETRIES", "retries", type_conv=int),
    # Time to wait for batching. Default is 0 (send immediately)
    ArgDsc("ALS_KAFKA_CLIENT_LINGER_MS", "linger_ms", type_conv=int),
    # Request timeout in milliseconds. Default is 30000
    ArgDsc("ALS_KAFKA_CLIENT_REQUEST_TIMEOUT_MS", "request_timeout_ms",
           type_conv=int),
    # Maximum number of unconfirmed requests in flight. Default is 5
    ArgDsc("ALS_KAFKA_CLIENT_MAX_UNCONFIRMED_REQS",
           "max_in_flight_requests_per_connection", type_conv=int),
    # Security protocol: 'PLAINTEXT', 'SSL' (hope we do not need SASL_...).
    # Default is 'PLAINTEXT'
    ArgDsc("ALS_KAFKA_CLIENT_SECURITY_PROTOCOL", "security_protocol"),
    # SSL. 'True' ('yes,', '1', '+') to check Kafka server identity,
    # 'False' ('no, '-', '0') to not check. Default is to check
    ArgDsc("ALS_KAFKA_CLIENT_SSL_CHECK_HOSTNAME", "ssl_check_hostname",
           type_conv=ArgDsc.bool_type_conv),
    # SSL. CA file for certificate verification
    ArgDsc("ALS_KAFKA_CLIENT_SSL_CAFILE", "ssl_cafile"),
    # SSL. Client certificate PEM file name
    ArgDsc("ALS_KAFKA_CLIENT_SSL_CERTFILE", "ssl_certfile"),
    # SSL. Client private key file
    ArgDsc("ALS_KAFKA_CLIENT_SSL_KEYFILE", "ssl_keyfile"),
    # SSL. File name for CRL certificate expiration check
    ArgDsc("ALS_KAFKA_CLIENT_SSL_CRLFILE", "ssl_crlfile"),
    # SSL. Available ciphers in OpenSSL cipher list format
    ArgDsc("ALS_KAFKA_CLIENT_SSL_CIPHERS", "ssl_ciphers")]


class Als:
    """ Kafka client for ALS and JSON logs

    Private attributes:
    _server_id       -- Unique AFC server identity string. Computed by
                        appending random suffix to 'client_id' constructor
                        parameter or (if not specified) to ALS_KAFKA_CLIENT_ID
                        environment variable value or (if not specified) to
                        "Unknown"
    _producer_kwargs -- Dictionary of parameters for KafkaProducer(). None if
                        parameters are invalid
    _producer        -- KafkaProducer. None if initialization was not
                        successful (yet?)
    _config_cache    -- None (if configs are not consolidated) or two-level
                        dictionary req_id->
                        (config, customer, geo_version, uls_id)->
                        request_indices
    _req_idx         -- Request message index
    _lock            -- Lock that guards thread-unsafe stuff
    _last_attempt    -- Datetime of last connection attempt
    """
    def __init__(self, client_id=None, consolidate_configs=True):
        """ Constructor

        Arguments:
        client_id           -- Client identity string. Better be unique, but
                               this is not necessary (as unique random sugffix
                               is apended anyway). If not specified, value of
                               ALS_KAFKA_CLIENT_ID is used. If neither
                               specified, "Unknown" is assumed
        consolidate_configs -- False to send configs as they arrive, True to
                               collect them and then send consolidated
        """
        self._producer = None
        self._config_cache = {} if consolidate_configs else None
        self._req_idx = 0
        self._lock = threading.Lock()
        self._producer_kwargs = None
        self._last_attempt = None
        kwargs = {}
        has_parameters = True
        for argdsc in arg_dscs:
            try:
                if not argdsc.from_env(kwargs):
                    has_parameters = False
                    break
            except (TypeError, ValueError, LookupError) as ex:
                has_parameters = False
                LOGGER.error(
                    "Parameter environment variable '%s' has invalid value of "
                    "'%s': %s",
                    argdsc.env_var, os.environ.get(argdsc.env_var), str(ex))
                break
        self._server_id = kwargs["client_id"] = \
            (client_id or kwargs.get("client_id", "Unknown")) + \
            "_" + random_hex(10)
        if not has_parameters:
            LOGGER.warning(
                "Parameters for Kafka ALS server connection not specified. "
                "No ALS logging will be performed")
            return
        if import_failure is not None:
            LOGGER.error("Some module(s) not installed: %s. "
                         "No ALS/JSON logging will be performed",
                         import_failure)
            return
        self._producer_kwargs = kwargs
        self._try_connect()

    def _try_connect(self):
        """ Tryeing to connect Kafka server, returns True if connection
        established
        """
        if self._producer is not None:
            return True
        if self._producer_kwargs is None:
            return False
        with self._lock:
            if self._producer is not None:
                return True
            if (self._last_attempt is not None) and \
                    ((datetime.datetime.now() - self._last_attempt) <
                     CONNECT_ATTEMPT_INTERVAL):
                return False
            self._last_attempt = datetime.datetime.now()
            try:
                self._producer = kafka.KafkaProducer(**self._producer_kwargs)
                LOGGER.info("Kafka server successfully connected")
                return True
            except kafka.errors.KafkaError as ex:
                LOGGER.error(
                    "Kafka ALS server connection initialization error: %s",
                    str(ex))
                return False

    @property
    def initialized(self):
        """ True if KafkaProducer initialization was successful """
        return self._producer is not None

    @property
    def connected(self):
        """ True if Kafka server connected """
        return self.initialized and self._producer.bootstrap_connected

    def afc_req_id(self):
        """ Returns Als-instance unique request ID """
        with self._lock:
            self._req_idx += 1
            return str(self._req_idx)

    def afc_request(self, req_id, req):
        """ Send AFC Request

        Arguments:
        req_id -- Unique for this Als object instance request ID string (e.g.
                  returned by req_id())
        req    -- Request message JSON dictionary
        """
        if not self._try_connect():
            return
        self._producer.send(
            topic=ALS_TOPIC, key=self._als_key(req_id),
            value=self._als_value(data_type=ALS_DT_REQUEST, data=req))

    def afc_response(self, req_id, resp):
        """ Send AFC Response

        Arguments:
        req_id -- Unique for this AlS object instance request ID string (e.g.
                  returned by req_id())
        req    -- Response message JSON dictionary
        """
        if not self._try_connect():
            return
        self._flush_afc_configs(req_id)
        self._producer.send(
            topic=ALS_TOPIC, key=self._als_key(req_id),
            value=self._als_value(data_type=ALS_DT_RESPONSE, data=resp))

    def afc_config(self, req_id, config_text, customer, geo_data_version,
                   uls_id, req_indices=None):
        """ Send or cache AFC Config

        Arguments:
        req_id --           Unique for this Als object instance request ID
                            string (e.g. returned by req_id())
        config_text      -- Config file contents
        customer         -- Customer name
        geo_data_version -- Version of Geodetic data
        uls_id           -- ID of ULS data
        req_indices      -- List of 0-based indices of individual requests
                            within message to which this information is
                            related. None if to all
        """
        if not self._try_connect():
            return
        if (self._config_cache is None) or (req_indices is None):
            self._send_afc_config(
                req_id=req_id, config_text=config_text, customer=customer,
                geo_data_version=geo_data_version, uls_id=uls_id,
                req_indices=req_indices)
        else:
            with self._lock:
                indices_for_config = \
                    self._config_cache.setdefault(req_id, {}).\
                        setdefault((config_text, customer,
                                    geo_data_version, uls_id), [])
                indices_for_config += req_indices

    def json_log(self, topic, record):
        """ Send JSON log record

        Arguments
        topic  -- Log type (message format) identifier - name of table to
                  which this log will be placed
        record -- JSON dictionary with record data
        """
        assert topic != ALS_TOPIC
        assert TOPIC_NAME_REGEX.match(topic)
        if not self._try_connect():
            return
        json_dict = \
            collections.OrderedDict(
                [(JSON_LOG_FIELD_VERSION, JSON_LOG_VERSION),
                 (JSON_LOG_FIELD_SERVER_ID, self._server_id),
                 (JSON_LOG_FIELD_TIME, timetag()),
                 (JSON_LOG_FIELD_DATA, json.dumps(record))])
        self._producer.send(topic=topic, value=to_bytes(json.dumps(json_dict)))

    def _flush_afc_configs(self, req_id):
        """ Send all AFC Config records, collected for given request

        Arguments:
        req_id -- Unique for this Als object instance request ID string (e.g.
                  returned by req_id())
        """
        if self._config_cache is None:
            return
        with self._lock:
            configs = self._config_cache.get(req_id)
            if configs is None:
                return
            del self._config_cache[req_id]
        for (config_text, customer, geo_data_version, uls_id), indices \
                in configs.items():
            self._send_afc_config(
                req_id=req_id, config_text=config_text, customer=customer,
                geo_data_version=geo_data_version, uls_id=uls_id,
                req_indices=indices if len(configs) != 1 else None)

    def _send_afc_config(self, req_id, config_text, customer, geo_data_version,
                         uls_id, req_indices):
        """ Actual AFC Config sending

        Arguments:
        req_id           -- Unique (on at least server level) request ID string
        config_text      -- Config file contents
        customer         -- Customer name
        geo_data_version -- Version of Geodetic data
        uls_id           -- ID of ULS data
        req_indices      -- List of 0-based indices of individual requests
                            within message to which this information is
                            related. None if to all
        """
        extra_fields = \
            collections.OrderedDict(
                [(ALS_FIELD_CUSTOMER, customer),
                 (ALS_FIELD_GEO_DATA, geo_data_version),
                 (ALS_FIELD_ULS_ID, uls_id)])
        if req_indices:
            extra_fields[ALS_FIELD_REQ_INDICES] = req_indices
        self._producer.send(
            topic=ALS_TOPIC, key=self._als_key(req_id),
            value=self._als_value(data_type=ALS_DT_CONFIG, data=config_text,
                                  extra_fields=extra_fields))

    def _als_key(self, req_id):
        """ ALS record key for given request ID """
        return to_bytes("%s|%s" % (self._server_id, req_id))

    def _als_value(self, data_type, data, extra_fields=None):
        """ ALS record value

        Arguments:
        data_type    -- Data type string
        data         -- Data - string or JSON dictionary
        extra_fields -- None or dictionary of message-type-specific fields
        """
        json_dict = \
            collections.OrderedDict(
                [(ALS_FIELD_VERSION, ALS_FORMAT_VERSION),
                 (ALS_FIELD_SERVER_ID, self._server_id),
                 (ALS_FIELD_TIME, timetag()),
                 (ALS_FIELD_DATA_TYPE, data_type),
                 (ALS_FIELD_DATA,
                  to_str(data) if isinstance(data, (str, bytes))
                  else json.dumps(data))])
        for k, v in (extra_fields or {}).items():
            json_dict[k] = v
        return to_bytes(json.dumps(json_dict))


# STATIC INTERFACE FOR THIS MODULE FUNCTIONALITY

# Als instance creation lock
_als_instance_lock = threading.Lock()
# Als instance, built after first initialization
_als_instance = None


def als_initialize(client_id=None, consolidate_configs=True):
    """ Initialization
    May be called several times, but all nonfirst calls are ignored

    Arguments:
    client_id           -- Client identity string. Better be unique, but this
                           is not necessary (as unique random sugffix is
                           apended anyway). If not specified, value of
                           ALS_KAFKA_CLIENT_ID is used. If neither specified,
                           "Unknown" is assumed
    consolidate_configs -- False to send configs as they arrive, True to
                           collect them and then send consolidated
    """
    global _als_instance, _als_instance_lock
    if _als_instance is not None:
        return
    with _als_instance_lock:
        if _als_instance is None:
            _als_instance = Als(client_id=client_id,
                                consolidate_configs=consolidate_configs)


def als_is_initialized():
    """ True if ALS was successfully initialized """
    return (_als_instance is not None) and _als_instance.initialized()


def als_is_connected():
    """ True if Kafka server connected """
    return (_als_instance is not None) and _als_instance.connected


def als_afc_req_id():
    """ Returns Als-instance unique request ID """
    return _als_instance.afc_req_id() if _als_instance is not None else None


def als_afc_request(req_id, req):
    """ Send AFC Request

    Arguments:
    req_id -- Unique (on at least server level) request ID string
    req    -- Request message JSON dictionary
    """
    if (_als_instance is not None) and (req_id is not None):
        _als_instance.afc_request(req_id, req)


def als_afc_response(req_id, resp):
    """ Send AFC Response

    Arguments:
    req_id -- Unique (on at least server level) request ID string
    req    -- Response message JSON dictionary
    """
    if (_als_instance is not None) and (req_id is not None):
        _als_instance.afc_response(req_id, resp)


def als_afc_config(req_id, config_text, customer, geo_data_version, uls_id,
                   req_indices=None):
    """ Send AFC Config

    Arguments:
    req_id           -- Unique (on at least server level) request ID string
    config_text      -- Config file contents
    customer         -- Customer name
    geo_data_version -- Version of Geodetic data
    uls_id           -- ID of ULS data
    req_indices      -- List of 0-based indices of individual requests within
                        message to which this information is related. None if
                        to all
    """
    if (_als_instance is not None) and (req_id is not None):
        _als_instance.afc_config(req_id, config_text, customer,
                                 geo_data_version, uls_id, req_indices)


def als_json_log(topic, record):
    """ Send JSON log record

    Arguments
    topic  -- Log type (message format) identifier - name of table to which
              this log will be placed
    record -- JSON dictionary with record data
    """
    if _als_instance is not None:
        _als_instance.json_log(topic, record)
