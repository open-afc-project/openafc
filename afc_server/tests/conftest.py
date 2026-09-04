""" Shared pytest setup for the afc_server unit tests. """

import json
import os
import sys

import pytest

# afc_server modules import each other flat (import afc_server_models), which
# works because the Dockerfile copies them all into /wd/ side by side. Mirror
# that layout here so tests import them the same way production does.
AFC_SERVER_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if AFC_SERVER_DIR not in sys.path:
    sys.path.insert(0, AFC_SERVER_DIR)

FIXTURE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "fixtures")


@pytest.fixture
def load_fixture():
    """ Load a JSON fixture by filename from tests/fixtures. """
    def _load(name):
        with open(os.path.join(FIXTURE_DIR, name)) as f:
            return json.load(f)
    return _load
