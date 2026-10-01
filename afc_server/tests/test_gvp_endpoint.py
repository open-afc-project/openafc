""" Unit tests for the GVP Exclusion Zone Inquiry endpoint. """

import copy

import fastapi
import pytest
from fastapi.testclient import TestClient

import afc_server_gvp
from afc_server_gvp_models import Rest_Gvp_RespMsg

URL = "/fbrat/ap-gvp/exclusionZoneInquiry"


@pytest.fixture
def client():
    """ Test client over an app carrying only the GVP routes. """
    app = fastapi.FastAPI()
    app.include_router(afc_server_gvp.router)
    return TestClient(app)


@pytest.fixture
def request_body(load_fixture):
    """ The spec's Appendix A request, safe for a test to mutate. """
    return load_fixture("appendix_a_request.json")


def _codes(payload):
    """ (requestId, responseCode) of each response, in order. """
    return [(r["requestId"], r["response"]["responseCode"])
            for r in payload["exclusionZoneInquiryResponses"]]


def test_appendix_a_request_succeeds(client, request_body):
    response = client.post(URL, json=request_body)
    assert response.status_code == 200
    assert _codes(response.json()) == [("wfa-gvp-request-0001", 0)]


def test_success_response_is_a_valid_response_message(client, request_body):
    """ The endpoint's own output parses back through the response models. """
    payload = client.post(URL, json=request_body).json()
    assert Rest_Gvp_RespMsg(**payload).version == "1.0"


def test_success_carries_zones_and_expiry(client, request_body):
    """ Table 18 requires both if and only if the code is SUCCESS. """
    entry = client.post(URL, json=request_body).json()[
        "exclusionZoneInquiryResponses"][0]
    assert entry["exclusionZoneInfo"]
    assert entry["availabilityExpireTime"].endswith("Z")


def test_unused_optional_fields_are_omitted(client, request_body):
    """ Section 3.2: an optional field that is not used shall be omitted. """
    payload = client.post(URL, json=request_body).json()
    assert "vendorExtensions" not in payload


def test_malformed_json_is_rejected(client):
    """ Section 3.4.1: 400 when the payload is not an inquiry message. """
    assert client.post(URL, content=b"{{{").status_code == 400


def test_wrong_object_is_rejected(client):
    """ Section 3.4.1: valid JSON that is not an inquiry message. """
    assert client.post(URL, json={"hello": "world"}).status_code == 400


def test_invalid_value_reports_103(client, request_body):
    request_body["exclusionZoneInquiryRequests"][0][
        "areaOfIntendedOperation"]["ellipse"]["center"]["latitude"] = 137.4
    payload = client.post(URL, json=request_body).json()
    assert _codes(payload) == [("wfa-gvp-request-0001", 103)]
    assert payload["exclusionZoneInquiryResponses"][0]["response"][
        "supplementalInfo"]["invalidParams"]


def test_missing_param_reports_102(client, request_body):
    del request_body["exclusionZoneInquiryRequests"][0]["deviceDescriptor"][
        "serialNumber"]
    assert _codes(client.post(URL, json=request_body).json()) == \
        [("wfa-gvp-request-0001", 102)]


def test_inquiry_without_request_id_still_gets_a_response(
        client, request_body):
    """ Table 18 requires the field; an absent one has nothing to echo. """
    del request_body["exclusionZoneInquiryRequests"][0]["requestId"]
    assert _codes(client.post(URL, json=request_body).json()) == [("", 102)]


def test_one_response_per_inquiry(client, request_body):
    """ Section 3.2 requires the same number of responses as requests. """
    second = copy.deepcopy(request_body["exclusionZoneInquiryRequests"][0])
    second["requestId"] = "wfa-gvp-request-0002"
    del second["deviceDescriptor"]["serialNumber"]
    request_body["exclusionZoneInquiryRequests"].append(second)
    assert _codes(client.post(URL, json=request_body).json()) == \
        [("wfa-gvp-request-0001", -1), ("wfa-gvp-request-0002", 102)]


def test_duplicate_request_ids_get_a_single_response(client, request_body):
    """ Table 6 requires unique requestIds, and Table 18 correlates a
    response to an inquiry by requestId alone, so a message that breaks
    uniqueness is rejected as a whole. """
    request_body["exclusionZoneInquiryRequests"].append(
        copy.deepcopy(request_body["exclusionZoneInquiryRequests"][0]))
    assert _codes(client.post(URL, json=request_body).json()) == [("", 103)]


def test_version_is_echoed_from_the_request(client, request_body):
    """ Table 17 requires the response version to match the request. """
    payload = client.post(URL, json=request_body).json()
    assert payload["version"] == request_body["version"]
