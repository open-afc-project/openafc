""" Prometheus AFC traffic metrics """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

import platform
import prometheus_client
from typing import Union

__all__ = ["message_processed", "request_processed"]

# Hostname
hostname = platform.node()

# Histogram duration buckets in seconds
DURATION_BUCKETS = (0.05, 0.1, 1., 10., 60., 120.)

# Message processing duration histogram
message_duration_hist = \
    prometheus_client.Histogram(name="afc_message_duration",
                                documentation="Message duration histogram",
                                labelnames=["host"], buckets=DURATION_BUCKETS)
message_duration_hist = message_duration_hist.labels(host=hostname)

# AFC response processing duration histogram
request_duration_hist = \
    prometheus_client.Histogram(name="afc_request_duration",
                                documentation="Request duration histogram",
                                labelnames=["host"], buckets=DURATION_BUCKETS)
request_duration_hist = request_duration_hist.labels(host=hostname)

# Message HTTP status codes
http_status_counter = \
    prometheus_client.Counter(name="afc_http_status",
                              documentation="REST HTTP status codes",
                              labelnames=["host", "status"])

# Request status codes
afc_response_counter = \
    prometheus_client.Counter(
        name="afc_response_code",
        documentation="AFC response codes",
        labelnames=["host", "response"])


def message_processed(duration_sec: float, status: Union[int, str]) -> None:
    """ Called upon AFC message processing completion

    Arguments:
    duration_sec -- Message processing duration in seconds
    status       -- HTTP status code or string with its moral equivalent
    """
    message_duration_hist.observe(duration_sec)
    http_status_counter.labels(host=hostname, status=str(status)).inc()


def request_processed(duration_sec: float, response: Union[int, str],
                      n: int = 1) -> None:
    """ Called upon AFC request processing completion

    Arguments:
    duration_sec -- Request processing duration in seconds
    response     -- AFC response code or string with its moral equivalent
    n            -- Number of responses ended this way
    """
    for _ in range(n):
        request_duration_hist.observe(duration_sec)
    afc_response_counter.labels(host=hostname, response=str(response)).inc(n)
