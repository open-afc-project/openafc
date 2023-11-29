#!/usr/bin/env python3
# ALS Client workbench

# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

import argparse
from collections.abc import Sequence
import csv
import datetime
import json
import kafka
import kafka.errors
import logging
import multiprocessing
import os
import random
import re
import signal
import string
import sys
import time
import traceback
from typing import Any, Callable, Dict, List, NamedTuple, Optional, TextIO,\
    Tuple, Union

# This script version
VERSION = "0.1"


def error(msg: str) -> None:
    """ Prints given msg as error message and exit abnormally """
    logging.error(msg)
    sys.exit(1)


def error_if(cond: Any, msg: str) -> None:
    """ If condition evaluates to true prints given msg as error message and
    exits abnormally """
    if cond:
        error(msg)


def random_str(length: int) -> str:
    """ Generates random uppercase alphanumeric string of given length """
    return "".join(random.choices(string.ascii_uppercase + string.digits,
                                  k=length))


def random_uniform(a: float, b: float, ndigits: int = 7) -> float:
    """ Rounded to 'ndigits' random value in ['a', 'b'] interval """
    return round(random.uniform(a, b), ndigits)


class RandomContext:
    """ Random generator context for replaying certain seeds.

    Intended to be used as context (with 'with')

    Private attributes:
    _seed       -- currently used random generator seed
    _rand_state -- State saved at context creation, restored after exit
    """
    def __init__(self, seed: Optional[int]) -> None:
        """ Constructor

        Arguments:
        seed -- Seed to use (None to take random seed)
        """
        self._seed = \
            seed if seed is not None else random.randint(0, 1000000000)
        self._rand_state = random.getstate()
        random.seed(self._seed)

    @property
    def seed(self) -> int:
        """ Seed from which random generator in the context was initialized """
        return self._seed

    def __enter__(self) -> "RandomContext":
        """ Entering context. Returns self """
        return self

    def __exit__(self, exc_type: Any, exc_value: Any, exc_tb: Any) -> None:
        """ Exiting context. Restores state before construction """
        random.setstate(self._rand_state)


class Region:
    """ Region rectangle

    Attributes:
    top_lat    -- North-positive top latitude in degrees
    left_lon   -- East-positive left longitude in degrees
    bottom_lat -- North-positive bottom latitude in degrees
    right_lon  -- East-positive right longitude in degrees
    """
    # Default region for AP coordinates' generation
    # "TOP_LAT_DEG;LEFT_LON_DEG;BOTTOM_LAT_DEG;RIGHT_LON_DEG" - all in
    #  north/east positive degrees
    CONUS = "49,-124,30,-81"

    def __init__(self, region_param: str) -> None:
        """ Constructor

        Arguments:
        region_param -- Command line argument representation """
        self.top_lat: float = 0
        self.left_lon: float = 0
        self.bottom_lat: float = 0
        self.right_lon: float = 0
        parts = region_param.split(",")
        error_if(len(parts) != 4, "Wrong region specification format")
        for attr in ("top_lat", "left_lon", "bottom_lat", "right_lon"):
            try:
                setattr(self, attr, float(parts[0]))
                parts.pop(0)
            except ValueError:
                error("Wrong region specification format")
        error_if(not (-90 <= self.bottom_lat < self.top_lat <= 90),
                 "Wrong region specification format")
        error_if(not (-180 <= self.left_lon < self.right_lon <= 180),
                 "Wrong region specification format")


class JsonBase:
    """ Base class for JSON data being sent to Kafka.

    Derived objects intended to be logically immutable

    Protected attributes:
    _json_dict -- Dictionary representation of JSON data
    """
    def __init__(self) -> None:
        """ Default constructor """
        self._json_dict: Dict[str, Any] = {}

    def to_str(self, indent: Optional[int] = None) -> str:
        """ JSON in string representation

        Arguments:
        indent - Visual indent (none for no indent)
        Returns string representation
        """
        return json.dumps(self._json_dict, indent=indent, sort_keys=True)

    def to_bytes(self) -> bytes:
        """ JSON in byte string representation """
        return self.to_str().encode("ascii")

    def _from_str(self, s: Union[str, bytes, bytearray]) -> None:
        """ Read-in JSON in string representation """
        self._json_dict = json.loads(s)

    def __eq__(self, other: Any) -> bool:
        """ Equality comparison """
        return isinstance(other, self.__class__) and \
            (other._json_dict == self._json_dict)

    def __hash__(self) -> int:
        """ Hash """
        return hash(self.to_str())


class AfcConfig(JsonBase):
    """ AFC Config JSON data """

    # DataType to use in ALS message
    DATA_TYPE = "AFC_CONFIG"

    __TEMPLATE = """
{
  "APUncertainty": {
    "horizontal": 30,
    "height": 5
  },
  "bodyLoss": {
    "kind": "Fixed Value",
    "valueIndoor": 0,
    "valueOutdoor": 0
  },
  "polarizationMismatchLoss": {
    "kind": "Fixed Value",
    "value": 3
  },
  "ITMParameters": {
    "polarization": "Vertical",
    "ground": "Good Ground",
    "dielectricConst": 25,
    "conductivity": 0.02,
    "minSpacing": 30,
    "maxPoints": 1500
  },
  "regionStr": "CONUS",
  "buildingPenetrationLoss": {
    "kind": "Fixed Value",
    "value": 20.5
  },
  "ulsDatabase": "CONUS_ULS_LATEST.sqlite3",
  "freqBands": [
    {
      "name": "UNII-5",
      "startFreqMHz": 5925,
      "stopFreqMHz": 6425
    },
    {
      "name": "UNII-7",
      "startFreqMHz": 6525,
      "stopFreqMHz": 6875
    }
  ],
  "rasDatabase": "RASdatabase.csv",
  "fsReceiverNoise": {
    "UNII5": -110,
    "UNII7": -109.5,
    "other": -109
  },
  "maxLinkDistance": 130,
  "clutterAtFS": true,
  "antennaPattern": {
    "kind": "None",
    "value": ""
  },
  "propagationEnv": "NLCD Point",
  "propagationModel": {
    "kind": "FCC 6GHz Report & Order",
    "win2ConfidenceCombined": 16,
    "win2UseGroundDistance": false,
    "fsplUseGroundDistance": false,
    "winner2HgtFlag": false,
    "winner2HgtLOS": 15,
    "win2ConfidenceLOS": 16,
    "win2ConfidenceNLOS": 50,
    "winner2LOSOption": "BLDG_DATA_REQ_TX",
    "itmConfidence": 5,
    "itmReliability": 20,
    "p2108Confidence": 25,
    "buildingSource": "None",
    "terrainSource": "3DEP (30m)"
  },
  "receiverFeederLoss": {
    "UNII5": 3,
    "UNII7": 3,
    "other": 3
  },
  "threshold": -6,
  "maxEIRP": 36,
  "minEIRP": 21,
  "ulsDefaultAntennaType": "WINNF-AIP-07",
  "scanPointBelowGroundMethod": "truncate",
  "rlanITMTxClutterMethod": "FORCE_TRUE",
  "fsClutterModel": {
    "p2108Confidence": 5,
    "maxFsAglHeight": 6
  },
  "nlcdFile": "federated_nlcd.tif",
  "enableMapInVirtualAp": false,
  "channelResponseAlgorithm": "psd",
  "visibilityThreshold": -6,
  "version": "3.3.17-192"
}
"""

    def __init__(self, s: Optional[str] = None, seed: Optional[int] = None) \
            -> None:
        """ Constructor

        Arguments:
        s    -- String representation. None to generate from seed
        seed -- Seed to generate from. None if string representation is used or
                if random seed should be chosen
        """
        super().__init__()
        if s is not None:
            assert seed is None
            self._from_str(s)
        else:
            self._from_str(self.__TEMPLATE)
            with RandomContext(seed):
                self._json_dict["ulsDatabase"] = random_str(10) + ".sqlite3"


class AfcRequest(JsonBase):
    """ AFC Request JSON data

    Attributes:
    req_id -- Request ID
    """

    # DataType to use in ALS message
    DATA_TYPE = "AFC_REQUEST"

    __TEMPLATE = """
{
  "availableSpectrumInquiryRequests": [
    {
      "deviceDescriptor": {
        "serialNumber": "XXX",
        "certificationId": [
          {
            "rulesetId": "US_47_CFR_PART_15_SUBPART_E",
            "id": "FCCID-SRS1"
          }
        ],
        "rulesetIds": [ "US_47_CFR_PART_15_SUBPART_E" ],
        "serialNumber": "REG123"
      },
      "inquiredChannels": [
        { "globalOperatingClass": 131 },
        { "globalOperatingClass": 132 },
        { "globalOperatingClass": 133 },
        { "globalOperatingClass": 134 },
        { "globalOperatingClass": 136 }
      ],
      "inquiredFrequencyRange": [
        {
          "highFrequency": 6425,
          "lowFrequency": 5925
        },
        {
          "highFrequency": 6875,
          "lowFrequency": 6525
        }
      ],
      "location": {
        "elevation": {
          "height": 3,
          "heightType": "AGL",
          "verticalUncertainty": 2
        },
        "ellipse": {
          "center": {
            "latitude": 64.203,
            "longitude": -149.3355
          },
          "majorAxis": 30,
          "minorAxis": 30,
          "orientation": 0
        },
        "indoorDeployment": 2
      },
      "requestId": "REQ-FSP58"
    }
  ],
  "version": "1.4"
}
"""
    def __init__(self, s: Optional[str] = None, seed: Optional[int] = None,
                 region: Optional[Region] = None) -> None:
        """ Constructor

        Arguments:
        s      -- String representation. None to generate from seed
        seed   -- Seed to generate from. None if string representation is used
                  or if random seed should be chosen
        region -- Region to generate position in. Must be specified if
                  generation is from seed
        """
        super().__init__()
        if s is not None:
            assert seed is None
            self._from_str(s)
        else:
            assert region is not None
            self._from_str(self.__TEMPLATE)
            with RandomContext(seed):
                request: Dict[str, Any] = \
                    self._json_dict["availableSpectrumInquiryRequests"][0]
                request["requestId"] = random_str(10)
                request["deviceDescriptor"]["serialNumber"] = random_str(10)
                min_height = \
                    request["location"]["elevation"]["verticalUncertainty"]
                request["location"]["elevation"]["height"] = \
                    random_uniform(min_height, min_height + 1000)
                request["location"]["ellipse"]["center"]["latitude"] = \
                    random_uniform(region.bottom_lat, region.top_lat)
                request["location"]["ellipse"]["center"]["longitude"] = \
                    random_uniform(region.left_lon, region.right_lon)
        self.req_id: str = \
            self._json_dict["availableSpectrumInquiryRequests"][0]["requestId"]


class AfcResponse(JsonBase):
    """ AFC Response JSON data """

    # DataType to use in ALS message
    DATA_TYPE = "AFC_RESPONSE"

    __TEMPLATE = """
{
 "availableSpectrumInquiryResponses": [
  {
   "availabilityExpireTime": "0",
   "availableChannelInfo": [
    {
     "channelCfi": [1, 5, 9, 13, 17, 21, 25, 29, 33, 37, 41, 45, 49, 53, 57,
     61, 65, 69, 73, 77, 81, 85, 89, 93, 117, 121, 125, 129, 133, 137, 141,
     145, 149, 153, 157, 161, 165, 169, 173, 177, 181],
     "globalOperatingClass": 131,
     "maxEirp": []
    },
    {
     "channelCfi": [3, 11, 19, 27, 35, 43, 51, 59, 67, 75, 83, 91, 123, 131,
     139, 147, 155, 163, 171, 179],
     "globalOperatingClass": 132,
     "maxEirp": []
    },
    {
     "channelCfi": [ 7, 23, 39, 55, 71, 87, 135, 151, 167 ],
     "globalOperatingClass": 133,
     "maxEirp": []
    },
    {
     "channelCfi": [ 15, 47, 79, 143 ],
     "globalOperatingClass": 134,
     "maxEirp": []
    },
    {
     "channelCfi": [ 2 ],
     "globalOperatingClass": 136,
     "maxEirp": []
    }
   ],
   "availableFrequencyInfo": [
    {"frequencyRange":{"highFrequency":5945,"lowFrequency":5925},"maxPsd":0},
    {"frequencyRange":{"highFrequency":5965,"lowFrequency":5945},"maxPsd":0},
    {"frequencyRange":{"highFrequency":5985,"lowFrequency":5965},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6005,"lowFrequency":5985},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6045,"lowFrequency":6005},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6065,"lowFrequency":6045},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6085,"lowFrequency":6065},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6105,"lowFrequency":6085},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6125,"lowFrequency":6105},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6145,"lowFrequency":6125},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6165,"lowFrequency":6145},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6185,"lowFrequency":6165},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6205,"lowFrequency":6185},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6225,"lowFrequency":6205},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6245,"lowFrequency":6225},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6265,"lowFrequency":6245},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6285,"lowFrequency":6265},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6305,"lowFrequency":6285},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6325,"lowFrequency":6305},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6345,"lowFrequency":6325},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6365,"lowFrequency":6345},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6385,"lowFrequency":6365},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6405,"lowFrequency":6385},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6425,"lowFrequency":6405},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6545,"lowFrequency":6525},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6565,"lowFrequency":6545},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6585,"lowFrequency":6565},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6605,"lowFrequency":6585},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6625,"lowFrequency":6605},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6645,"lowFrequency":6625},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6665,"lowFrequency":6645},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6685,"lowFrequency":6665},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6705,"lowFrequency":6685},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6725,"lowFrequency":6705},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6745,"lowFrequency":6725},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6765,"lowFrequency":6745},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6785,"lowFrequency":6765},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6805,"lowFrequency":6785},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6825,"lowFrequency":6805},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6845,"lowFrequency":6825},"maxPsd":0},
    {"frequencyRange":{"highFrequency":6865,"lowFrequency":6845},"maxPsd":0}
   ],
   "requestId": "REQ-FSP37",
   "response": {
    "responseCode": 0,
    "shortDescription": "Success"
   },
   "rulesetId": "US_47_CFR_PART_15_SUBPART_E"
  }
 ],
 "version": "1.4"
}
"""
    def __init__(self, s: Optional[str] = None, seed: Optional[int] = None,
                 req_id: Optional[str] = None) -> None:
        """ Constructor

        Arguments:
        s      -- String representation. None to generate from seed
        seed   -- Seed to generate from. None if string representation is used
                  or if random seed should be chosen
        req_id -- Request ID of generated request. Must be specified if
                  generated from seed
        """
        super().__init__()
        if s is not None:
            assert seed is None
            self._from_str(s)
        else:
            assert req_id is not None
            self._from_str(self.__TEMPLATE)

            with RandomContext(seed):
                response: Dict[str, Any] = \
                    self._json_dict["availableSpectrumInquiryResponses"][0]
                response["requestId"] = req_id
                for oc_data in response["availableChannelInfo"]:
                    oc_data["maxEirp"] = \
                        [random_uniform(21, 36)
                         for _ in range(len(oc_data["channelCfi"]))]
                for fr_data in response["availableFrequencyInfo"]:
                    fr_data["maxPsd"] = random_uniform(21, 36)
                response["availabilityExpireTime"] = \
                    (datetime.datetime.now(datetime.timezone.utc) +
                     datetime.timedelta(days=2)).isoformat()


class KafkaProducerParams:
    """ Kafka producer constructort parameters.
    Subset of the whole set of producer parameters. Values set to None are not
    passed to constructor - defaults are used

    Attributes:
    bootstrap_servers  -- List of 'server[:port]' Kafka server addresses
    acks               -- Required server confirmations before operation
                          completes (0, 1, 'all'). Default (None) is 1
    compression_type   -- Compression type: None, 'gzip', 'snappy', 'lz4'.
                          Default (None) is None
    retries            -- Number of retries. Default (None) is 0 (no retries)
    linger_ms          -- Time to wait (for batching) before sending in
                          milliseconds. Default (None) is 0 (send immediately)
    request_timeout_ms -- Request timeout in milliseconds. Default (None) is
                          30000 (30 seconds)
    max_in_flight_requests_per_connection -- Maximum number of unconfirmed
                          requests. Default (None) is 5
    security_protocol  -- Security protocol ('PLAINTEXT', 'SSL', 'SASL',
                          'SASL_PLAINTEXT', 'SASL_SSL'). Default (None) is
                          'PLAINTEXT'
    ssl_check_hostname -- True to verify server identity. Default (None) is
                          True
    ssl_cafile         -- CA file for cedrtifivcate verification. Default
                          (None) is None
    ssl_certfile       -- Client certificate PEM file name. Default (None) is
                          None
    ssl_keyfile        -- Client proivate key file. Default (None) is None
    ssl_crlfile        -- File name for CRL certificate expiration check.
                          Default (None) is None
    ssl_ciphers        -- Available ciphers in OpenSSL cipher list format.
                          Default (None) is None

    """
    def __init__(self, servers: Union[str, Sequence[str]], **kwargs) -> None:
        self.bootstrap_servers: Sequence[str] = \
            [servers] if isinstance(servers, str) else servers
        self.acks: Optional[Union[int, str]] = None
        self.compression_type: Optional[str] = None
        self.retries: Optional[int] = None
        self.linger_ms: Optional[int] = None
        self.request_timeout_ms: Optional[int] = None
        self.security_protocol: Optional[str] = None
        self.ssl_check_hostname: Optional[bool] = None
        self.ssl_cafile: Optional[str] = None
        self.ssl_certfile: Optional[str] = None
        self.ssl_keyfile: Optional[str] = None
        self.ssl_crlfile: Optional[str] = None
        self.ssl_ciphers: Optional[str] = None
        for k, v in kwargs.items():
            assert k in self.__dict__
            setattr(self, k, v)

    def constructor_kwargs(self) -> Dict[str, Any]:
        """ Kwargs to pass to constructor. Only keys with non-None values are
        used """
        return {k: v for k, v in self.__dict__.items() if v is not None}


class KafkaClient:
    """ Kafka client

    Private attributes:
    _producer -- KafkaProducer object
    """
    TOPIC = "ALS"

    def __init__(self, params: KafkaProducerParams,
                 client_id: Optional[str] = None) -> None:
        """ Constructor

        Arguments:
        params    -- Common producer parameters
        client_id -- Client ID string or None
        """
        self._producer = kafka.KafkaProducer(client_id=client_id,
                                             **(params.constructor_kwargs()))

    def send(self, topic: str, key: Optional[bytes] = None,
             value: Optional[bytes] = None) -> None:
        """ Send to kafka

        Arguments:
        topic -- Topic
        key   -- Optional key bytestring
        value -- Optional value bytestring
        """
        self._producer.send(topic=topic, value=value, key=key)

    def metrics(self) -> Any:
        """ Returns KAFKA metrics, whatever is it """
        return self._producer.metrics()


class ProcessSet:
    """ Collection of processes, used as flood clients

    Private attributes:
    _processes -- List of multiprocessing.Process objects
    """
    def __init__(self) -> None:
        """ Constructor """
        self._processes: List[multiprocessing.Process] = []

    def add_process(self, name, func: Callable, kwargs: Dict[str, Any]) \
            -> None:
        """ Adds process to collection and starts it

        Arguments:
        name   -- Process name
        func   -- Process function
        kwargs -- Process function keyword arguments
        """
        self._processes.append(
            multiprocessing.Process(name=name, target=func, kwargs=kwargs))
        self._processes[-1].start()

    def any_running(self) -> bool:
        """ True if there are running processes """
        return any(p.is_alive() for p in self._processes)

    def join(self) -> None:
        """ Wait for completion of all processes """
        for proc in self._processes:
            proc.join()

    def terminate(self) -> None:
        """ Terminate all processes """
        for proc in self._processes:
            proc.terminate()


class BatchedQueue:
    """ Batching extension of multiprocessing.Queue.
    Mitifates time losses on context switches

    Private attributes:
    _queue        -- Underlying multiprocessing.Queue
    _batch_size   -- Output batch maximum size
    _out_batch    -- List containing output batch
    _in_batch     -- List containing input batch
    _in_batch_ptr -- Index of first unread element in input batch
    _closed       -- True if queue is closed (no mpre puts)
    """
    def __init__(self, batch_size: int = 100, max_size: Optional[int] = None) \
            -> None:
        """ Constructor

        Arguments:
        batch_size -- Maximum size of output batch
        max_size   -- Maximum queue capacity (in number of elements, not
                      batches), Nonefor unlimited
        """
        queue_args = () if max_size is None else \
            (((max_size + batch_size - 1) // batch_size), )
        self._queue: multiprocessing.Queue = multiprocessing.Queue(*queue_args)
        self._batch_size = batch_size
        self._out_batch: List[Any] = []
        self._in_batch: List[Any] = []
        self._in_batch_ptr = 0
        self._closed = False

    def put(self, obj: Any) -> None:
        """ Put given object to queue """
        assert not self._closed
        self._out_batch.append(obj)
        if len(self._out_batch) < self._batch_size:
            return
        self.flush()

    def get(self) -> Any:
        """ Get next object from queue. Blockjs if queue is empty """
        if self._in_batch_ptr >= len(self._in_batch):
            self._in_batch = self._queue.get()
            self._in_batch_ptr = 0
        assert len(self._in_batch) > 0
        self._in_batch_ptr += 1
        return self._in_batch[self._in_batch_ptr - 1]

    def flush(self) -> None:
        """ Pushed unfinished output batch (if any) to queue """
        if self._out_batch:
            self._queue.put(self._out_batch)
            self._out_batch = []

    def rx_empty(self) -> bool:
        """ True if queue is empty (from receiver viewpoint) """
        return (self._in_batch_ptr >= len(self._in_batch)) and \
            self._queue.empty()

    def tx_full(self) -> bool:
        """ True if queue is full (from transmitter viewpoint """
        return (len(self._out_batch) == (self._batch_size - 1)) and \
            self._queue.full()

    def close(self) -> None:
        """ Closes queue (guarantees that there will be no more puts """
        self.flush()
        self._queue.close()
        self._closed = True

    def join(self) -> None:
        """ Wait for all put element will be passed to queue.
        Must be closed before """
        assert self._closed
        self._queue.join_thread()

    def terminate(self) -> None:
        """ Terminate queue without wait """
        self._queue.cancel_join_thread()


class AlsSerializerBase:
    """ Base class for Kafka messages' serializers.
    Each class responsible for particular message type

    Private attributes:
    _data_type -- Content for messages' 'DataType' field
    """
    # Content for messages' "Version" field
    FORMAT_VERSION = "1.0"

    def __init__(self, data_type: str, client_id: str) -> None:
        """ Constructor

        Arguments:
        data_type -- Content for messages' 'dataType' field
        client_id -- Content for messages' 'afcServer' field
        """
        self._data_type = data_type
        self._client_id = client_id

    def _serialize(self, data: str,
                   extra_fields: Optional[Dict[str, Any]] = None) \
            -> Tuple[str, bytes]:
        """ Common serialization code

        Arguments:
        data         -- Contant for messges' 'Data' field
        extra_fields -- Additional fields and their values or None
        Returns (timetag, serialized_message) tuple
        """
        timetag_str = datetime.datetime.now().isoformat()
        json_dict: Dict[str, Any] = {
            "version": self.FORMAT_VERSION,
            "afcServer": self._client_id,
            "time": timetag_str,
            "dataType": self._data_type,
            "jsonData": data}
        if extra_fields:
            json_dict.update(extra_fields)
        return (timetag_str, json.dumps(json_dict).encode("ascii"))


class AlsSerializerConfig(AlsSerializerBase):
    """ Serializer for "AlsConfig" messages """

    def __init__(self, client_id: str) -> None:
        """ Constructor

        Arguments:
        client_id -- Content for messages' 'AfcServer' field
        """
        super().__init__(data_type=AfcConfig.DATA_TYPE, client_id=client_id)

    def serialize(self, data: str, uls_data_id: str) -> Tuple[str, bytes]:
        """ Serialization

        Arguments:
        data        -- Content for messsages' 'Data' field
        uls_data_id -- Content for messages' 'UlsDataId' field
        Returns (timetag, serialized_message) tuple
        """
        return self._serialize(data=data,
                               extra_fields={"customer": "Broadcom",
                                             "geoDataVersion": "1.0",
                                             "ulsId": uls_data_id,
                                             "requestIndexes": []})


class AlsSerializerRequest(AlsSerializerBase):
    """ Serializer for "AlsRequest" messages """
    def __init__(self, client_id: str) -> None:
        """ Constructor

        Arguments:
        client_id -- Content for messages' 'AfcServer' field
        """
        super().__init__(data_type=AfcRequest.DATA_TYPE, client_id=client_id)

    def serialize(self, data: str) -> Tuple[str, bytes]:
        """ Serialization

        Arguments:
        data        -- Content for messsages' 'Data' field
        Returns (timetag, serialized_message) tuple
        """
        return self._serialize(data=data)


class AlsSerializerResponse(AlsSerializerBase):
    """ Serializer for "AlsResponse" messages """

    def __init__(self, client_id: str) -> None:
        """ Constructor

        Arguments:
        client_id -- Content for messages' 'AfcServer' field
        """
        super().__init__(data_type=AfcResponse.DATA_TYPE, client_id=client_id)

    def serialize(self, data: str) -> Tuple[str, bytes]:
        """ Serialization

        Arguments:
        data        -- Content for messsages' 'Data' field
        Returns (timetag, serialized_message) tuple
        """
        return self._serialize(data=data)


class AlsTransactionGenerator:
    """ Generator of ALS Transactions

    Private attributes:
    _configs           -- List of unique AFC Configs. Empty if all should be
                          unique
    _request_responses -- List of unique AFC request/response pairs. Empty if
                          all should be unique
    _uls_data_ids      -- List of unique ULS IDs. Empty if all should be unique
    _region            -- Region for coordinates' generation
    """
    class JsonDataParam(NamedTuple):
        """ Data on single JSON to send

        Attributes:
        seed       -- Seed data was generated from
        json_bytes -- JSON content serialized to bytes
        """
        seed: int
        json_str: str

    class AlsTransaction:
        """ All data for ALS transaction

        Attributes:
        uls_data_id       -- ULS Data ID string. None if AFC Config not being
                             sent
        afc_config_data   -- AFC Config JSON. None if AFC Config no being sent
        afc_request_data  -- AFC Request JSON. None if AFC Request not being
                             sent
        afc_response_data -- AFC Response JSON. None if AFC Response not being
                             sent
        """
        def __init__(self) -> None:
            """ Constructor """
            self.uls_data_id: Optional[str] = None
            self.afc_config_data: \
                Optional["AlsTransactionGenerator.JsonDataParam"] = None
            self.afc_request_data: \
                Optional["AlsTransactionGenerator.JsonDataParam"] = None
            self.afc_response_data: \
                Optional["AlsTransactionGenerator.JsonDataParam"] = None

    class _RequestResponse(NamedTuple):
        """ AFC Request/response pair """
        request: Optional["AlsTransactionGenerator.JsonDataParam"] = None
        response: Optional["AlsTransactionGenerator.JsonDataParam"] = None

    def __init__(self, unique_configs: int, unique_requests: int,
                 unique_uls: int, region: Region) -> None:
        """ Constructor

        Arguments:
        unique_configs  -- Number of unique AFC Configs. 0 if all should be
                           unique
        unique_requests -- Number of unique AFC request/response pairs. 0 if
                           all should be unique
        unique_uls      -- Number of unique ULS IDs pairs. 0 if all should be
                           unique
        region          -- Region for coordinates' generation
        """
        self._configs: \
            List[Optional[AlsTransactionGenerator.JsonDataParam]] = \
            unique_configs * [None]
        self._request_responses: \
            List[Optional[AlsTransactionGenerator._RequestResponse]] = \
            unique_requests * [None]
        self._uls_data_ids: List[Optional[str]] = unique_uls * [None]
        self._region = region

    def generate(self) -> "AlsTransactionGenerator.AlsTransaction":
        """ Generate request transaction data """
        ret = self.AlsTransaction()
        idx: int = 0
        if self._configs:
            idx = random.randrange(len(self._configs))
            ret.afc_config_data = self._configs[idx]
        if ret.afc_config_data is None:
            seed = random.randrange(1000000000)
            ret.afc_config_data = \
                self.JsonDataParam(
                    seed=seed,
                    json_str=AfcConfig(seed=seed).to_str())
            if self._configs:
                self._configs[idx] = ret.afc_config_data

        request_response: \
            Optional[AlsTransactionGenerator._RequestResponse] = None
        if self._request_responses:
            idx = random.randrange(len(self._request_responses))
            request_response = self._request_responses[idx]
        if request_response is None:
            seed = random.randrange(1000000000)
            areq = AfcRequest(seed=seed, region=self._region)
            request = self.JsonDataParam(seed=seed, json_str=areq.to_str())
            response = \
                self.JsonDataParam(
                    seed=seed,
                    json_str=AfcResponse(seed=seed,
                                         req_id=areq.req_id).to_str())
            request_response = self._RequestResponse(request=request,
                                                     response=response)
            if self._request_responses:
                self._request_responses[idx] = request_response
        ret.afc_request_data = request_response.request
        ret.afc_response_data = request_response.response

        if self._uls_data_ids:
            idx = random.randrange(len(self._uls_data_ids))
            ret.uls_data_id = self._uls_data_ids[idx]
        if ret.uls_data_id is None:
            ret.uls_data_id = random_str(10)
            if self._uls_data_ids:
                self._uls_data_ids[idx] = ret.uls_data_id
        return ret


class FloodProcess:
    """ Static class with stuff, related to flood processes """

    class JsonDataResult(NamedTuple):
        """ Data about single sent JSON

        Attributes:
        seed        -- Seed data was geenerated from
        timetag_str -- ISO-formatted timetag used for transmission
        """
        seed: int
        timetag_str: str

    class Result:
        """ Result -- message passed through result queue

        Attributes:
        client_id     -- Client ID string
        afc_config    -- Information about sent AFC config or None
        afc_request   -- Information about sent AFC Request or None
        afc_response  -- Information about sent AFC Responce or None
        uls_data_id   -- ULS Data ID or None
        exception     -- Exception message or None
        """
        def __init__(self, client_id: str, exception: Optional[str] = None) \
                -> None:
            """ Constructor

            Arguments:
            client_id -- Client ID String
            exception -- Formatted exception for exception, None for
                         transaction result
            """
            self.client_id = client_id
            self.afc_config: Optional["FloodProcess.JsonDataResult"] = None
            self.afc_request: Optional["FloodProcess.JsonDataResult"] = None
            self.afc_response: Optional["FloodProcess.JsonDataResult"] = None
            self.uls_data_id: Optional[str] = None
            self.exception: Optional[str] = exception

        def csv_row(self) -> List[Union[str, int]]:
            """ Row for flood log CSV file """
            return [
                self.client_id,
                self._value_or_empty(self.afc_request, "timetag_str"),
                self._value_or_empty(self.afc_request, "seed"),
                self._value_or_empty(self.afc_config, "timetag_str"),
                self._value_or_empty(self.afc_config, "seed"),
                self.uls_data_id if self.uls_data_id is not None else "",
                self._value_or_empty(self.afc_response, "timetag_str"),
                self._value_or_empty(self.afc_response, "seed")]

        @classmethod
        def csv_heading_row(cls) -> List[str]:
            """ Header row for flood log CSV file """
            return ["Client ID", "Request Timetag", "Request Seed",
                    "Config Timetag", "Config Seed", "ULS ID",
                    "Response Timetag", "Response Seed"]

        def _value_or_empty(self, obj: Optional[Any], attr: str) -> Any:
            """ Returns attribute value or empty string

            Arguments:
            obj  -- Object or None
            attr -- Attribute name
            Returns value of given object's attribute if oject is not None,
            empty string if object is None
            """
            return getattr(obj, attr) if obj is not None else ""

    @staticmethod
    def process(client_id: str, kafka_params: KafkaProducerParams,
                als_generator: AlsTransactionGenerator,
                max_count: Optional[int],
                max_duration: Optional[datetime.timedelta],
                result_queue: BatchedQueue, delay_ms: float) -> None:
        """ Process function

        Arguments:
        client_id     -- Client ID for messages
        kafka_params  -- Common Kafka client parameters
        als_generator -- ALS Transaction generator
        max_count     -- None or maximum number of transactions
        max_duration  -- None or maximum duration
        result_queue  -- Queue for sending results
        delay_ms      -- Mean value for artificial delay between messages
        """
        try:
            random.seed()
            kafka_client = \
                KafkaClient(params=kafka_params, client_id=client_id)
            config_serializer = AlsSerializerConfig(client_id)
            request_serializer = AlsSerializerRequest(client_id)
            response_serializer = AlsSerializerResponse(client_id)
            start_time = datetime.datetime.now()
            count = 0
            while True:
                count += 1
                if ((max_count is not None) and (count > max_count)) or \
                        ((max_duration is not None) and
                         ((datetime.datetime.now() - start_time) >
                          max_duration)):
                    break
                als_transaction = als_generator.generate()
                result = FloodProcess.Result(client_id=client_id)
                key = f"{client_id}|{count}".encode("ascii")
                if als_transaction.afc_request_data:
                    if delay_ms:
                        time.sleep(random.uniform(0, delay_ms / 1000))
                    timetag_str, value = \
                        request_serializer.serialize(
                            als_transaction.afc_request_data.json_str)
                    kafka_client.send(topic=KafkaClient.TOPIC, key=key,
                                      value=value)
                    result.afc_request = \
                        FloodProcess.JsonDataResult(
                            seed=als_transaction.afc_request_data.seed,
                            timetag_str=timetag_str)
                if als_transaction.afc_config_data:
                    if delay_ms:
                        time.sleep(random.uniform(0, delay_ms / 1000))
                    assert als_transaction.uls_data_id is not None
                    timetag_str, value = \
                        config_serializer.serialize(
                            als_transaction.afc_config_data.json_str,
                            uls_data_id=als_transaction.uls_data_id)
                    kafka_client.send(
                        topic=KafkaClient.TOPIC, key=key, value=value)
                    result.afc_config = \
                        FloodProcess.JsonDataResult(
                            seed=als_transaction.afc_config_data.seed,
                            timetag_str=timetag_str)
                    result.uls_data_id = als_transaction.uls_data_id
                if als_transaction.afc_response_data:
                    if delay_ms:
                        time.sleep(random.uniform(0, delay_ms / 1000))
                    timetag_str, value = \
                        response_serializer.serialize(
                            als_transaction.afc_response_data.json_str)
                    kafka_client.send(topic=KafkaClient.TOPIC, key=key,
                                      value=value)
                    result.afc_response = \
                        FloodProcess.JsonDataResult(
                            seed=als_transaction.afc_response_data.seed,
                            timetag_str=timetag_str)
                result_queue.put(result)
        except Exception:
            exc_type, exc, exc_traceback = sys.exc_info()
            result_queue.put(
                FloodProcess.Result(
                    client_id=client_id,
                    exception="" if isinstance(exc, KeyboardInterrupt) else
                    "".join(traceback.format_exception(exc_type, exc,
                                                       exc_traceback))))
        result_queue.flush()


def do_flood(args):
    """ Execute "flood" command.

    Arguments:
    args -- Parsed command line arguments
    """
    als_generator = \
        AlsTransactionGenerator(
            unique_configs=args.unique_configs,
            unique_requests=args.unique_requests, unique_uls=args.unique_uls,
            region=Region(args.region))

    max_duration: Optional[datetime.timedelta] = None
    if args.duration:
        m = re.match(r"((?<h>\d+)h)?((?<m>\d+)h)?((?<s>\d+)h)?",
                     args.duration, re.IGNORECASE)
        error_if(not m, "Invalid duration format")
        max_duration = \
            datetime.timedelta(
                hours=int(m.group("h")) if m.group("h") else 0,
                minutes=int(m.group("m")) if m.group("m") else 0,
                seconds=int(m.group("s")) if m.group("s") else 0)

    max_count: Optional[int] = None
    if args.number is not None:
        max_count = (args.number + args.streams - 1) // args.streams

    kafka_params = KafkaProducerParams(servers=args.kafka,
                                       compression_type=args.compression)

    streams = ProcessSet()
    flood_log_file: Optional[TextIO] = None
    start_time = datetime.datetime.now()
    results_received = 0
    result_queue = BatchedQueue()
    try:
        flood_log_csv: Optional[Any] = None
        original_sigint_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
        for i in range(args.streams):
            streams.add_process(
                name=f"ALS_Stream_{i}", func=FloodProcess.process,
                kwargs={"client_id": f"ALS_CLIENT_{i}",
                        "kafka_params": kafka_params,
                        "als_generator": als_generator,
                        "max_count": max_count,
                        "max_duration": max_duration,
                        "result_queue": result_queue,
                        "delay_ms": args.processing_delay_ms})
        signal.signal(signal.SIGINT, original_sigint_handler)

        if args.flood_log:
            flood_log_file = \
                open(args.flood_log, "w", newline="", encoding="ascii")
            flood_log_csv = csv.writer(flood_log_file)
            flood_log_csv.writerow(FloodProcess.Result.csv_heading_row())
        while True:
            if (not streams.any_running()) and result_queue.rx_empty():
                break

            take_a_break = True
            flood_log_rows: List[List[Union[str, int]]] = []
            while not result_queue.rx_empty():
                take_a_break = False
                result: FloodProcess.Result = result_queue.get()
                if result.exception is not None:
                    if result.exception == "":
                        break
                    error(f"{result.client_id}:\n{result.exception}")
                results_received += 1
                if (not args.quiet) and ((results_received % 1000) == 0):
                    total_duration = datetime.datetime.now() - start_time
                    rate = results_received / \
                        (total_duration.total_seconds() or 1)
                    print(f"{results_received} ALS Transactions made in "
                          f"{total_duration}. {rate} requests/sec    \r",
                          end="")
                if args.flood_log:
                    flood_log_rows.append(result.csv_row())
            if flood_log_rows:
                flood_log_csv.writerows(flood_log_rows)
            if take_a_break:
                time.sleep(0.1)
    except KeyboardInterrupt:
        pass
    finally:
        if streams:
            streams.terminate()
        result_queue.terminate()
        if flood_log_file:
            flood_log_file.close()
        if not args.quiet:
            print("")

    total_duration = datetime.datetime.now() - start_time
    logging.info(f"{results_received} ALS transactions made in "
                 f"{total_duration}. "
                 f"{results_received / (total_duration.total_seconds() or 1)} "
                 f"transactions/sec")


class LogSerializer:
    """ Serializer for JSON Log messages

    Private attributes:
    _client_id -- Content for messages' 'afcServer' field
    """

    # Content for messages' "version" field
    FORMAT_VERSION = "1.0"

    def __init__(self, client_id: str) -> None:
        """ Constructor

        Arguments:
        client_id -- Content for messages' 'afcServer' field
        """
        self._client_id = client_id

    def serialize(self, data: Union[str, Dict[str, Any], List[Any]]) -> bytes:
        """ Serialize log message with given data

        Arguments:
        data -- Log record in form of either dictionary/list to serialize via
                JSON or already serialized data
        Returns Kafka record value
        """
        json_dict: Dict[str, Any] = \
            {"version": self.FORMAT_VERSION,
             "afcServer": self._client_id,
             "time": datetime.datetime.now().isoformat(),
             "jsonData": data if isinstance(data, str) else json.dumps(data)}
        return json.dumps(json_dict).encode("ascii")


def do_log_test(args):
    """ Execute "log_test" command.

    Arguments:
    args -- Parsed command line arguments
    """
    kafka_params = KafkaProducerParams(servers=args.kafka,
                                       compression_type=args.compression)
    kafka_client = KafkaClient(params=kafka_params, client_id="ALS_LOG_CLIENT")
    serializer = LogSerializer("LogClient")
    for topic, records in \
            [("LOG1", [{}, [], {"key1": "value1"}]),
             ("LOG2", ['{"bad_syntax"', {"a": {"b": [1, 2, 3]}}])]:
        for record in records:
            kafka_client.send(topic=topic, value=serializer.serialize(record))


def do_help(args: Any) -> None:
    """Execute "help" command.

    Arguments:
    args -- Parsed command line arguments (also contains 'argument_parser' and
            'subparsers' fields)
    """
    if args.subcommand is None:
        args.argument_parser.print_help()
    else:
        args.subparsers.choices[args.subcommand].print_help()


def main(argv: List[str]) -> None:
    """Do the job.

    Arguments:
    argv -- Program arguments
    """
    # Kafka server connection switches
    switches_server = argparse.ArgumentParser(add_help=False)
    switches_server.add_argument(
        "--kafka", "-k", metavar="SERVER[:PORT]",
        help="Kafka bootstrap server[:port]. Default port is 9092, default "
        "host is localhost")
    switches_server.add_argument(
        "--compression", metavar="COMPRESSION",
        choices=["gzip", "snappy", "lz4"],
        help="Compression type (one of 'gzip', 'snappy', 'lz4'). Default is "
        "no compression")

    # Position generation switches
    switches_generator = argparse.ArgumentParser(add_help=False)
    switches_generator.add_argument(
        "--region",
        metavar="TOP_LAT_DEG,LEFT_LON_DEG,BOTTOM_LAT_DEG,RIGHT_LON_DEG",
        default=Region.CONUS,
        help=f"Region for coordinate generation. Latitudes are north-positive "
        f"degrees, longitudes are east-positive degrees. Default is "
        f"'{Region.CONUS}'")
    switches_generator.add_argument(
        "--flood_log", "-l", metavar="FILENAME",
        help="Log with stuff output during flood")

    # Top level parser
    argument_parser = argparse.ArgumentParser(
        description=f"ALS Client Workbench. V{VERSION}")
    subparsers = argument_parser.add_subparsers(dest="subcommand",
                                                metavar="SUBCOMMAND")

    # Subparser for 'flood' command
    parser_flood = subparsers.add_parser(
        "flood", parents=[switches_server, switches_generator],
        help="Flood Kafka server with requests")
    parser_flood.add_argument(
        "--number", "-n", metavar="NUMBER", type=int,
        help="Number of requests to send")
    parser_flood.add_argument(
        "--duration", "-d", metavar="[<HOURS>h][<MINUTES>m][<SECONDS>s]",
        help="Flood duration. e.g. 3s or 5h10m")
    parser_flood.add_argument(
        "--streams", "-s", metavar="STREAMS", type=int, default=1,
        help="Number of parallel streams (default is 1)")
    parser_flood.add_argument(
        "--unique_requests", metavar="UNIQUE_REQ_COUNT", type=int, default=0,
        help="Number of unique requests per stream. 0 for all requests "
        "unique. Default is 0")
    parser_flood.add_argument(
        "--unique_configs", metavar="UNIQUE_CONFIG_COUNT", type=int, default=1,
        help="Number of unique configs per stream. 0 for all configs unique. "
        "Default is 1")
    parser_flood.add_argument(
        "--unique_uls", metavar="UNIQUE_ULS_COUNT", type=int, default=1,
        help="Number of unique ULS datasets per stream. 0 for all ULS "
        "detasets unique. Default is 1")
    parser_flood.add_argument(
        "--processing_delay_ms", metavar="MICROSECONDS", type=float, default=0,
        help="Average request processing delay in microseconds. Default is no "
        "delay")
    parser_flood.add_argument(
        "--quiet", "-q", action="store_true",
        help="Do not print progress information")
    parser_flood.set_defaults(func=do_flood)

    parser_log_test = subparsers.add_parser(
        "log_test", parents=[switches_server],
        help="Sends a couple of log records")
    parser_log_test.set_defaults(func=do_log_test)

    # Subparser for 'help' command
    parser_help = subparsers.add_parser(
        "help", add_help=False, usage="%(prog)s subcommand",
        help="Prints help on given subcommand")
    parser_help.add_argument(
        "subcommand", metavar="SUBCOMMAND", nargs="?",
        choices=subparsers.choices,
        help="Name of subcommand to print help about (use " +
        "\"%(prog)s --help\" to get list of all subcommands)")
    parser_help.set_defaults(func=do_help, subparsers=subparsers,
                             argument_parser=argument_parser)

    if not argv:
        argument_parser.print_help()
        sys.exit(1)
    args = argument_parser.parse_args(argv)

    # Set up logging
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(
        logging.Formatter(
            f"{os.path.basename(__file__)}. %(levelname)s: %(message)s"))
    logging.getLogger().addHandler(console_handler)
    logging.getLogger().setLevel(logging.INFO)

    # Do the needful
    args.func(args)


if __name__ == "__main__":
    main(sys.argv[1:])
