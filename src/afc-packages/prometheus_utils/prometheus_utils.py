#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
""" Prometheus client utility stuff for Flask

This module defines a couple of wrappers around some stuff, defined in
prometheus_client Python module. Specifically:

- PrometheusTimeBase class for time measurements
- multiprocess_prometheus_configured() and multiprocess_flask_metrics() for
  defining Prometheus metrics endpoint in multiprocess Flask application (see
  'SETUP' section below)

PrometheusTimeBase is a base class for defining Summary (mapped to Prometheus
'counter' data type) for measuring time spent inside code (at function level
one can use prometheus_client.Summary directly).

PrometheusTimeBase is a base class. Each derived class defines a single metric
with following labels:
- type    -- can be 'cpu' or 'wallclock' for measuring CPU and wallclock time
             spent respectively. Any may be individually disabled if deemed
             costly
- scope   -- name of code span being measured
- segment -- name of segment within scope. This labes was introduced to avoid
             moving code blocks right/left multiple times (see examples below)

Metric derived class must have 'metric' static member. Here is how to define
metric 'foo':
    class FooMetric(prometheus_utils.PrometheusTimeBase):
      metric = \
        prometheus_utils.PrometheusTimeBase.make_metric(
            "foo", "Description of 'foo'")

Metric usage.
Context style:
    import prometheus_utils
    ...
    def bar():
        ...
        with FooMetric("bar_scope1") as foo_metric:
            ...
            foo_metric.seg_end("s1")
            ...
            foo_metric.seg_end("s2")
            ...
        ...
        ...
        ...
        with FooMetric("bar_scope2"):
            ...
        ...

Call style (more awkward, but does not require moving code left/right at all):
    import prometheus_utils
    ...
    def bar():
        ...
        foo_metric1 = FooMetric("bar_scope1")
        foo_metric1.start()
        ...
        foo_metric.seg_end("s1")
        ...
        foo_metric.seg_end("s2")
        ...
        foo_metric1.stop()
        ...
        ...
        ...
        foo_metric2 = FooMetric("bar_scope2", start=True)
        ...
        foo_metric2.stop()
        ...

Code above will create following time series:
- Time spent in the whole 'bar_scope1':
  - foo_sum{type="cpu", scope="bar_scope1", segment="entire"}
  - foo_sum{type="wallclock", scope="bar_scope1", segment="entire"}
- Time spent in in first segment of 'bar_scope1':
  - foo_sum{type="cpu", scope="bar_scope1", segment="s1"}
  - foo_sum{type="wallclock", scope="bar_scope1", segment="s1"}
- Time spent in in second segment of 'bar_scope1':
  - foo_sum{type="cpu", scope="bar_scope1", segment="s2"}
  - foo_sum{type="wallclock", scope="bar_scope1", segment="s2"}
- Time spent in in last segment of 'bar_scope1':
  - foo_sum{type="cpu", scope="bar_scope1", segment="tail"}
  - foo_sum{type="wallclock", scope="bar_scope1", segment="tail"}
- Time spent in the whole 'bar_scope2':
  - foo_sum{type="cpu", scope="bar_scope2", segment="entire"}
  - foo_sum{type="wallclock", scope="bar_scope2", segment="entire"}
- Number of times correspondent codespans were executed:
  - foo_count - same labels as above


Defining Flask metrics endpoint. This example is, in fact, not good, as
/metrics endpoint better be defined at top level (not in blueprint), but as of
time of this writing I do not know how to achieve this and anyway achieving
this has nothing to do with Prometheus. So use '__metrics_path__' in Prometheus
target definition
...
module = flask.Blueprint(...)
...
class PrometheusMetrics(MethodView):
    def get(self):
        return prometheus_utils.multiprocess_flask_metrics()
...
if prometheus_utils.multiprocess prometheus_configured():
    module.add_url_rule(
        '/metrics', view_func=PrometheusMetrics.as_view('PrometheusMetrics'))


Defining multiprocess FastAPI metrics endpoint

app = fastapi.FastAPI()
...
app.mount("/metrics", prometheus_utils.multiprocess_fastapi_metrics())

SETUP

For Prometheus client to work properly in multiprocess Flask applivation the
following things need to be done
- 'PROMETHEUS_MULTIPROC_DIR' environment variable must be defined and point to
  empty directory
- Gunicorn should be given a setup file (--config in gunicorn command line)
  with following stuff present:
    ...
    import prometheus_client.multiprocess
    ...
    def child_exit(server, worker):
        ...
        prometheus_client.multiprocess.mark_process_dead(worker.pid)
        ...
"""

# pylint: disable=too-many-instance-attributes, wrong-import-order
# pylint: disable=invalid-name

import os
try:
    import flask
except ImportError:
    pass
import prometheus_client
import prometheus_client.multiprocess
import prometheus_client.core
import sys
import time
from typing import Callable, Optional


class PrometheusTimeBase:
    """ Abstract base class for Summary-based metrics.

    Derived classes must define static 'metric' static field, initialized with
    make_metric(). This field defines metric name

    Private attributes:
    _metric                     -- Metric name
    _scope                      -- Name of scope ('scope' label in generated
                                   time series)
    _no_cpu                     -- True to not generate 'type="cpu"' label
    _no_wallclock               -- True to not generate 'type="wallclock"'
                                   label
    _segmented                  -- True if there were segments since start (to
                                   generate 'tail' segment time series)
    _started                    -- True if measurement started and not stopped
    _cpu_start_ns               -- CPU NS counter at scope start
    _wallclock_start_ns         -- Wallclock NS counter at scope start
    _segment_cpu_start_ns       -- CPU NS counter at segment start
    _segment_wallclock_start_ns -- Wallclock NS counter at segment start
    """
    # Segment label value for last segment
    TAIL_SEG_NAME = "tail"
    # Segment label value for full scope
    ENTIRE_SEG_NAME = "entire"

    def __init__(self, scope: str, no_cpu: bool = False,
                 no_wallclock: bool = False, start: bool = False) -> None:
        """ Constructor

        Arguments:
        scope        -- Segment name
        no_cpu       -- True to not generate 'type="cpu"' label
        no_wallclock -- True to not generate 'type="wallclock"' label
        """
        # 'metric' static attribute must be defined in derived class
        self._metric = self.__class__.metric  # pylint: disable=no-member
        self._scope = scope
        self._no_cpu = no_cpu
        self._no_wallclock = no_wallclock
        self._segmented = False
        self._started = False
        self._cpu_start_ns = 0
        self._wallclock_start_ns = 0
        self._segment_cpu_start_ns = 0
        self._segment_wallclock_start_ns = 0
        if start:
            self.start()

    def start(self) -> None:
        """ Start scope measurement """
        assert not self._started
        self._started = True
        self._segmented = False
        if not self._no_cpu:
            self._cpu_start_ns = self._segment_cpu_start_ns = \
                time.process_time_ns()
        if not self._no_wallclock:
            self._wallclock_start_ns = self._segment_wallclock_start_ns = \
                time.perf_counter_ns()

    def stop(self) -> None:
        """ Ens scope measurement """
        if not self._started:
            return
        self._started = False
        if not self._no_cpu:
            ns = time.process_time_ns()
            if self._segmented:
                self._metric.\
                    labels("cpu", self._scope, self.TAIL_SEG_NAME).\
                    observe((ns - self._segment_cpu_start_ns) * 1e-9)
            self._metric.\
                labels("cpu", self._scope, self.ENTIRE_SEG_NAME).\
                observe((ns - self._cpu_start_ns) * 1e-9)
        if not self._no_cpu:
            ns = time.perf_counter_ns()
            if self._segmented:
                self._metric.\
                    labels("wallclock", self._scope, self.TAIL_SEG_NAME).\
                    observe((ns - self._segment_wallclock_start_ns) * 1e-9)
            self._metric.\
                labels("wallclock", self._scope, self.ENTIRE_SEG_NAME).\
                observe((ns - self._wallclock_start_ns) * 1e-9)

    def seg_end(self, seg_name: str) -> None:
        """ End of segment within scope

        Arguments:
        seg_name -- Segment name
        """
        if not self._started:
            return
        self._segmented = True
        if not self._no_cpu:
            ns = time.process_time_ns()
            self._metric.\
                labels("cpu", self._scope, seg_name).\
                observe((ns - self._segment_cpu_start_ns) * 1e-9)
            self._segment_cpu_start_ns = ns
        if not self._no_cpu:
            ns = time.perf_counter_ns()
            self._metric.\
                labels("wallclock", self._scope, seg_name).\
                observe((ns - self._segment_wallclock_start_ns) * 1e-9)
            self._segment_wallclock_start_ns = ns

    def __enter__(self) -> "PrometheusTimeBase":
        """ 'with' start. Starts scope measurement """
        self.start()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        """ 'with' end. Ends scope (and last segment) measurement """
        self.stop()

    @classmethod
    def make_metric(cls, name: str, dsc: Optional[str] = None) \
            -> prometheus_client.core.Summary:
        """ Returns Summary metrics of given name and description """
        return prometheus_client.core.Summary(name, dsc or name,
                                              ["type", "scope", "segment"])


def multiprocess_prometheus_configured() -> bool:
    """ True if multiprocess Priometheus client support was (hopefully)
    configured, False if definitely not """
    return os.environ.get("PROMETHEUS_MULTIPROC_DIR") is not None


def multiprocess_flask_metrics() -> "flask.Response":
    """ Returns return value for 'get()' on metrics endpoint """
    assert "flask" in sys.modules
    registry = prometheus_client.CollectorRegistry()
    prometheus_client.multiprocess.MultiProcessCollector(registry)
    ret = flask.make_response(prometheus_client.generate_latest(registry))
    ret.mimetype = prometheus_client.CONTENT_TYPE_LATEST
    return ret


def multiprocess_fastapi_metrics() -> Callable:
    """ Creating ASGI metrics app """
    registry = prometheus_client.CollectorRegistry()
    prometheus_client.multiprocess.MultiProcessCollector(registry)
    return prometheus_client.make_asgi_app(registry=registry)


