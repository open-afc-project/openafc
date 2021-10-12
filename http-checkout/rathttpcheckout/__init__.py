''' This package is a pure collection of unittest cases.

The test configuration can be controlled by environment variables:

`HTTPCHECKOUT_BASEURL`
    as the base URL to access the host-under-test. Make sure this has a trailing slash
`HTTPCHECKOUT_READONLY`
    any non-empty value will skip all tests which modify the CPO Archive state.

An example of running this checkout is:

HTTPCHECKOUT_BASEURL=http://localhost:5000/ \
XDG_DATA_DIRS=$PWD/testroot/share \
PYTHONPATH=$PWD/http-checkout \
nosetests -v rathttpcheckout

'''

from .aaa import *
from .paws import *
from .ratapi import *
from .www import *
