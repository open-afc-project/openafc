# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

""" AFC and JSON logging to PostgreSQL

This module uses Kafka producer client from Confluent.
Overview-level documentation may be found here:
https://github.com/confluentinc/confluent-kafka-python
API documentation may be found here:
https://docs.confluent.io/platform/current/clients/confluent-kafka-python/html/
Configuration parameters may be found here:
https://docs.confluent.io/platform/current/installation/configuration/
and here:
https://docs.confluent.io/platform/current/clients/confluent-kafka-python/\
html/index.html#kafka-client-configuration
"""

# pylint: disable=invalid-name, too-many-arguments, global-statement
# pylint: disable=broad-exception-caught

import collections
import datetime
import json
import logging
import os
import re
import threading
from typing import Any, Callable, cast, Dict, List, Optional, Tuple, Union

LOGGER = logging.getLogger(__name__)

import_failure: Optional[str] = None
try:
    import dateutil.tz
    import confluent_kafka
except ImportError as exc:
    import_failure = repr(exc)

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
# mTLS DN field  of AFC Request record
ALS_FIELD_MTLS_DN = "mtlsDn"

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

# Once in this number of times Producer.poll() is called (to prevent send
# report accumulation)
POLL_PERIOD = 100

# Delay between connection attempts
CONNECT_ATTEMPT_INTERVAL = datetime.timedelta(seconds=10)

# Regular expression for topic name check (Postgre requirement to table names)
TOPIC_NAME_REGEX = re.compile(r"^[_a-zA-Z][0-9a-zA-Z_]{,62}$")

# Maximum um message size (default maximum of 1MB is way too small for GUI AFC
# Requests)
MAX_MSG_SIZE = (1 << 20) * 10


def to_bytes(s: Optional[str]) -> Optional[bytes]:
    """ Converts string to bytes """
    if s is None:
        return None
    return s.encode("utf-8")


def to_str(s: Optional[Union[bytes, str]]) -> Optional[str]:
    """ Converts bytes to string """
    if (s is None) or isinstance(s, str):
        return s
    return s.decode("utf-8", errors="backslashreplace")


def timetag() -> str:
    """ Timetag with timezone """
    return datetime.datetime.now(tz=dateutil.tz.tzlocal()).isoformat()


def random_hex(n: int) -> str:
    """ Generates strongly random n-byte sequence and returns its hexadecimal
    representation """
    return "".join(f"{b:02X}" for b in os.urandom(n))


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

    def __init__(self, env_var: str, arg: str,
                 type_conv: Callable[[str], Any] = str,
                 list_separator: Optional[str] = None,
                 required: bool = False, default: Any = None) -> None:
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
    def env_var(self) -> str:
        """ Name of environment variable """
        return self._env_var

    def from_env(self, kwargs: Dict[str, Any]) -> bool:
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
    def str_or_int_type_conv(cls, str_val: str) -> Union[str, int]:
        """ Type converter for argument that can be string or integer """
        try:
            return int(str_val)
        except (TypeError, ValueError):
            return str_val

    @classmethod
    def bool_type_conv(cls, str_val: str) -> bool:
        """ Type converter for boolean argument """
        if str_val.lower() in ("+", "1", "yes", "true", "on"):
            return True
        if str_val.lower() in ("-", "0", "no", "false", "off"):
            return False
        raise ValueError("Wrong format of boolean Kafka parameter")


# Descriptors of KafkaProducer arguments
arg_dscs: List[ArgDsc] = [
    # Default value for stem part of "client_id" parameter passed to
    # KafkaProducer and server ID used in messages
    ArgDsc("ALS_KAFKA_SERVER_ID", "client.id"),
    # Comma-separated list of Kafka (bootstrap) servers in form of 'hostname'
    # or 'hostname:port' (default port is 9092). If not specified ALS logging
    # is not performed
    ArgDsc("ALS_KAFKA_CLIENT_BOOTSTRAP_SERVERS", "bootstrap.servers",
           required=True),
    # Number of Kafka confirmations before operation completion.
    # Valid values: '0' (fire and forget), '1', 'all')
    ArgDsc("ALS_KAFKA_CLIENT_ACKS", "acks",
           type_conv=ArgDsc.str_or_int_type_conv, default=1),
    # Number of retries. Default is 0
    ArgDsc("ALS_KAFKA_CLIENT_RETRIES", "retries", type_conv=int, default=5),
    # Time to wait for batching. Default is 0 (send immediately)
    ArgDsc("ALS_KAFKA_CLIENT_LINGER_MS", "linger.ms", type_conv=int),
    # Request timeout in milliseconds. Default is 30000
    ArgDsc("ALS_KAFKA_CLIENT_REQUEST_TIMEOUT_MS", "request.timeout.ms",
           type_conv=int),
    # Maximum number of unconfirmed requests in flight. Default is 5
    ArgDsc("ALS_KAFKA_CLIENT_MAX_UNCONFIRMED_REQS",
           "max.in.flight.requests.per.connection", type_conv=int),
    # Security protocol: 'PLAINTEXT', 'SSL' (hope we do not need SASL_...).
    # Default is 'PLAINTEXT'
    ArgDsc("ALS_KAFKA_CLIENT_SECURITY_PROTOCOL", "security.protocol"),
    # SSL. CA file for certificate verification
    # Maximum message size
    ArgDsc("ALS_KAFKA_MAX_REQUEST_SIZE", "message.max.bytes", type_conv=int)]


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
    _send_count      -- Number of sent records
    """

    def __init__(self, client_id: Optional[str] = None,
                 consolidate_configs: bool = True) -> None:
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
        self._producer: Optional[confluent_kafka.Producer] = None
        self._config_cache: \
            Optional[Dict[str, Dict[Tuple[str, str, str, str], List[int]]]] = \
            {} if consolidate_configs else None
        self._req_idx = 0
        self._lock = threading.Lock()
        kwargs: Dict[str, Any] = {}
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
                    argdsc.env_var, os.environ.get(argdsc.env_var), repr(ex))
                break
        self._server_id: str = \
            cast(str, (client_id or kwargs.get("client.id", "Unknown"))) + \
            "_" + random_hex(10)
        self._send_count = 0
        kwargs["client.id"] = self._server_id
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
        with self._lock:
            try:
                self._producer = \
                    confluent_kafka.Producer(kwargs)
                LOGGER.info("Kafka ALS server connection created")
            except confluent_kafka.KafkaException as ex:
                LOGGER.error(
                    "Kafka ALS server connection initialization error: %s",
                    repr(ex.args[0]))

    @property
    def initialized(self) -> bool:
        """ True if KafkaProducer initialization was successful """
        return self._producer is not None

    def afc_req_id(self) -> str:
        """ Returns Als-instance unique request ID """
        with self._lock:
            self._req_idx += 1
            return str(self._req_idx)

    def afc_request(self, req_id: str, req: Dict[str, Any],
                    mtls_dn: Optional[str] = None) -> None:
        """ Send AFC Request

        Arguments:
        req_id  -- Unique for this Als object instance request ID string (e.g.
                   returned by req_id())
        req     -- Request message JSON dictionary
        mtls_dn -- DN of client mTLS certificate or None
        """
        if self._producer is None:
            return
        extra_fields = \
            {k: v for k, v in [(ALS_FIELD_MTLS_DN, mtls_dn)] if v is not None}
        self._send(
            topic=ALS_TOPIC, key=self._als_key(req_id),
            value=self._als_value(data_type=ALS_DT_REQUEST, data=req,
                                  extra_fields=extra_fields))

    def afc_response(self, req_id: str, resp: Dict[str, Any]) -> None:
        """ Send AFC Response

        Arguments:
        req_id -- Unique for this AlS object instance request ID string (e.g.
                  returned by req_id())
        resp   -- Response message as JSON dictionary
        """
        if self._producer is None:
            return
        self._flush_afc_configs(req_id)
        self._send(
            topic=ALS_TOPIC, key=self._als_key(req_id),
            value=self._als_value(data_type=ALS_DT_RESPONSE, data=resp))

    def afc_config(self, req_id: str, config_text: str, customer: str,
                   geo_data_version: str, uls_id: str,
                   req_indices: Optional[list[int]] = None) -> None:
        """ Send or cache AFC Config

        Arguments:
        req_id --           Unique for this AlS object instance request ID
                            string (e.g. returned by req_id())
        config_text      -- Config file contents
        customer         -- Customer name
        geo_data_version -- Version of Geodetic data
        uls_id           -- ID of ULS data
        req_indices      -- List of 0-based indices of individual requests
                            within message to which this information is
                            related. None if to all
        """
        if self._producer is None:
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
                    setdefault(
                        (config_text, customer, geo_data_version, uls_id), [])
                indices_for_config += req_indices

    def json_log(self, topic: str, record: Any) -> None:
        """ Send JSON log record

        Arguments
        topic  -- Log type (message format) identifier - name of table to
                  which this log will be placed
        record -- JSON dictionary with record data
        """
        if self._producer is None:
            return
        assert topic != ALS_TOPIC
        assert TOPIC_NAME_REGEX.match(topic)
        json_dict = \
            collections.OrderedDict(
                [(JSON_LOG_FIELD_VERSION, JSON_LOG_VERSION),
                 (JSON_LOG_FIELD_SERVER_ID, self._server_id),
                 (JSON_LOG_FIELD_TIME, timetag()),
                 (JSON_LOG_FIELD_DATA, json.dumps(record))])
        self._send(topic=topic, value=to_bytes(json.dumps(json_dict)))

    def flush(self, timeout_sec: float) -> bool:
        """ Flush pending messages (if any)

        Arguments:
        timeout_sec -- timeout in seconds, None to wait for completion
        Returns True on success, False on timeout
        """
        if self._producer is None:
            return True
        return self._producer.flush(timeout=timeout_sec) == 0

    def _flush_afc_configs(self, req_id: str) -> None:
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

    def _send_afc_config(self, req_id: str, config_text: str, customer: str,
                         geo_data_version: str, uls_id: str,
                         req_indices: Optional[List[int]]) -> None:
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
        extra_fields: Dict[str, Any] = \
            collections.OrderedDict(
                [(ALS_FIELD_CUSTOMER, customer),
                 (ALS_FIELD_GEO_DATA, geo_data_version),
                 (ALS_FIELD_ULS_ID, uls_id)])
        if req_indices:
            extra_fields[ALS_FIELD_REQ_INDICES] = req_indices
        self._send(
            topic=ALS_TOPIC, key=self._als_key(req_id),
            value=self._als_value(data_type=ALS_DT_CONFIG, data=config_text,
                                  extra_fields=extra_fields))

    def _send(self, topic: str, key: Optional[bytes] = None,
              value: Optional[bytes] = None) -> None:
        """ KafkaProducer's send() method with exceptions caught """
        assert self._producer is not None
        self._send_count += 1
        if (self._send_count % POLL_PERIOD) == 0:
            try:
                self._producer.poll(0)
            except confluent_kafka.KafkaException:
                pass
        try:
            self._producer.produce(topic=topic, key=key, value=value)
        except confluent_kafka.KafkaException as ex:
            LOGGER.error(
                "Error sending to topic '%s': %s", topic,
                repr(ex.args[0]))

    def _als_key(self, req_id: str) -> bytes:
        """ ALS record key for given request ID """
        return cast(bytes, to_bytes(f"{self._server_id}|{req_id}"))

    def _als_value(self, data_type: str, data: Union[str, Dict[str, Any]],
                   extra_fields: Optional[Dict[str, Any]] = None):
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
_als_instance_lock: threading.Lock = threading.Lock()
# Als instance, built after first initialization
_als_instance: Optional[Als] = None


def als_initialize(client_id: Optional[str] = None,
                   consolidate_configs: bool = True) -> None:
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
    global _als_instance
    global _als_instance_lock
    if _als_instance is not None:
        return
    with _als_instance_lock:
        if _als_instance is None:
            _als_instance = Als(client_id=client_id,
                                consolidate_configs=consolidate_configs)


def als_is_initialized() -> bool:
    """ True if ALS was successfully initialized """
    return (_als_instance is not None) and _als_instance.initialized


def als_afc_req_id() -> Optional[str]:
    """ Returns Als-instance unique request ID """
    return _als_instance.afc_req_id() if _als_instance is not None else None


def als_afc_request(req_id: str, req: Dict[str, Any],
                    mtls_dn: Optional[str] = None) -> None:
    """ Send AFC Request

    Arguments:
    req_id  -- Unique (on at least server level) request ID string
    req     -- Request message JSON dictionary
    mtls_dn -- DN of client mTLS certificate or None
    """
    if (_als_instance is not None) and (req_id is not None):
        _als_instance.afc_request(req_id, req, mtls_dn)


def als_afc_response(req_id: str, resp: Dict[str, Any]) -> None:
    """ Send AFC Response

    Arguments:
    req_id -- Unique (on at least server level) request ID string
    req    -- Response message JSON dictionary
    """
    if (_als_instance is not None) and (req_id is not None):
        _als_instance.afc_response(req_id, resp)


def als_afc_config(req_id: str, config_text: str, customer: str,
                   geo_data_version: str, uls_id: str,
                   req_indices: Optional[List[int]] = None) -> None:
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


def als_json_log(topic: str, record: Any):
    """ Send JSON log record

    Arguments
    topic  -- Log type (message format) identifier - name of table to which
              this log will be placed
    record -- JSON dictionary with record data
    """
    if _als_instance is not None:
        _als_instance.json_log(topic, record)


def als_flush(timeout_sec: float = 2) -> bool:
    """ Flush pending messages (if any)

    Usually it is not needed, but if last log ALS/JSON write was made
    immediately before program exit, some records might become lost.
    Hence it might be beneficial to call this function before script end

    Arguments:
    timeout_sec -- timeout in seconds, None to wait for completion
    Returns True on success, False on timeout
    """
    if _als_instance is not None:
        return _als_instance.flush(timeout_sec=timeout_sec)
    return True
