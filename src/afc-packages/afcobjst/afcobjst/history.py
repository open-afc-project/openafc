#!/usr/bin/env python3

# Copyright 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""
Provides HTTP server for getting history.
"""

import urllib.parse
from markupsafe import escape
import markupsafe
import hashlib
import hmac
import os
import logging
import io
import abc
import json
import gzip
from flask import Flask, request, helpers, abort, Response
from .objstconf import ObjstConfigInternal

NET_TIMEOUT = 60  # The amount of time, in seconds, to wait for the server response

hist_app = Flask(__name__)
hist_app.config.from_object(ObjstConfigInternal())


def _derive_objst_bearer_token(key):
    """ Purpose-bound objst/hist bearer token:
    HMAC-SHA256(key, 'afc-objst-bearer-v1') - mirrors filestorage.py; the
    raw provisioned secret must never transit the wire because the RMQ
    response-signing key is derived from the same secret.
    """
    return hmac.new(key.encode("utf-8"), b"afc-objst-bearer-v1",
                    hashlib.sha256).hexdigest()


def _load_objst_api_key():
    """ Return the expected hist bearer token (derived from
    AFC_OBJST_API_KEY_FILE) or None. """
    key_file = os.environ.get("AFC_OBJST_API_KEY_FILE")
    if key_file and os.path.isfile(key_file):
        with open(key_file) as f:
            key = f.read().strip()
        return _derive_objst_bearer_token(key) if key else None
    return None


@hist_app.before_request
def _require_hist_bearer_token():
    """ Require a bearer token on every history route.

    The sibling objst_app gates its routes the same way; hist_app previously
    had no authentication, exposing the AFC history directory unauthenticated
    to any host that could reach the container.
    """
    if request.endpoint == "healthcheck":
        return
    expected = _load_objst_api_key()
    if expected is None:
        hist_app.logger.error(
            "hist: AFC_OBJST_API_KEY_FILE not configured — "
            "all requests rejected")
        abort(503)
    auth_header = request.headers.get("Authorization", "")
    if not auth_header.startswith("Bearer "):
        abort(401)
    supplied = auth_header[len("Bearer "):]
    if not hmac.compare_digest(supplied.encode(), expected.encode()):
        abort(403)


if hist_app.config['AFC_OBJST_LOG_FILE']:
    logging.basicConfig(filename=hist_app.config['AFC_OBJST_LOG_FILE'],
                        level=hist_app.config['AFC_OBJST_LOG_LVL'])
else:
    logging.basicConfig(level=hist_app.config['AFC_OBJST_LOG_LVL'])

if hist_app.config["AFC_OBJST_MEDIA"] == "GoogleCloudBucket":
    import google.cloud.storage
    os.environ["GOOGLE_APPLICATION_CREDENTIALS"] = hist_app.config["AFC_OBJST_GOOGLE_CLOUD_CREDENTIALS_JSON"]
    client = google.cloud.storage.client.Client()
    bucket = client.bucket(hist_app.config["AFC_OBJST_GOOGLE_CLOUD_BUCKET"])


def _format_size(num_bytes):
    if num_bytes is None:
        return ""
    if num_bytes < 1024:
        return f"{num_bytes} B"
    elif num_bytes < 1024 * 1024:
        return f"{num_bytes / 1024:.1f} KB"
    else:
        return f"{num_bytes / (1024 * 1024):.1f} MB"


def _is_html_preferred(req):
    accept = req.headers.get("Accept", "")
    return "text/html" in accept


def _build_breadcrumbs(rurl, path):
    vpath = "history"
    if path:
        vpath += "/" + path.strip("/")
    path_split = vpath.split("/")
    base_url = rurl.rstrip("/")
    breadcrumbs_html = [
        '<li class="pf-v6-c-breadcrumb__item"><a href="/fbrat">Home</a></li>',
        '<li class="pf-v6-c-breadcrumb__item"><span class="pf-v6-c-breadcrumb__item-divider">/</span></li>'
    ]
    for i, directory in enumerate(path_split):
        if i == 0:
            link_url = base_url
        else:
            link_url = base_url + "/" + "/".join(path_split[1:i+1])
        if i == len(path_split) - 1:
            breadcrumbs_html.append(
                f'<li class="pf-v6-c-breadcrumb__item pf-v6-c-breadcrumb__item--active">{escape(directory)}</li>'
            )
        else:
            breadcrumbs_html.append(
                f'<li class="pf-v6-c-breadcrumb__item"><a href="{escape(link_url)}">{escape(directory)}</a></li>'
            )
            breadcrumbs_html.append(
                '<li class="pf-v6-c-breadcrumb__item"><span class="pf-v6-c-breadcrumb__item-divider">/</span></li>'
            )
    return "\n".join(breadcrumbs_html)


def _render_json_node(key, val, is_last=True):
    comma = "" if is_last else ","
    key_html = f'<span class="json-key">"{escape(str(key))}"</span>: ' if key is not None else ""
    if isinstance(val, dict):
        if not val:
            return f'<div class="json-row">{key_html}<span class="json-brace">{{}}</span>{comma}</div>'
        count = f"{len(val)} key" if len(val) == 1 else f"{len(val)} keys"
        items = list(val.items())
        inner = "\n".join(_render_json_node(k, v, i == len(items) - 1) for i, (k, v) in enumerate(items))
        return (f'<details class="json-node" open>'
                f'<summary class="json-summary">{key_html}<span class="json-brace">{{</span><span class="json-badge">{count}</span></summary>'
                f'<div class="json-children">{inner}</div>'
                f'<div class="json-close"><span class="json-brace">}}</span>{comma}</div>'
                f'</details>')
    elif isinstance(val, list):
        if not val:
            return f'<div class="json-row">{key_html}<span class="json-brace">[]</span>{comma}</div>'
        count = f"{len(val)} item" if len(val) == 1 else f"{len(val)} items"
        inner = "\n".join(_render_json_node(None, v, i == len(val) - 1) for i, v in enumerate(val))
        return (f'<details class="json-node" open>'
                f'<summary class="json-summary">{key_html}<span class="json-brace">[</span><span class="json-badge">{count}</span></summary>'
                f'<div class="json-children">{inner}</div>'
                f'<div class="json-close"><span class="json-brace">]</span>{comma}</div>'
                f'</details>')
    elif isinstance(val, bool):
        lit = "true" if val else "false"
        return f'<div class="json-row">{key_html}<span class="json-bool">{lit}</span>{comma}</div>'
    elif val is None:
        return f'<div class="json-row">{key_html}<span class="json-null">null</span>{comma}</div>'
    elif isinstance(val, (int, float)):
        return f'<div class="json-row">{key_html}<span class="json-num">{val}</span>{comma}</div>'
    else:
        return f'<div class="json-row">{key_html}<span class="json-str">"{escape(str(val))}"</span>{comma}</div>'


_COMMON_CSS = """
    :root {
      --pf-font-family: "Red Hat Text", -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      --pf-font-mono: SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace;
      --pf-bg-page: #f0f0f0;
      --pf-bg-card: #ffffff;
      --pf-bg-masthead: #151515;
      --pf-color-text: #151515;
      --pf-color-text-secondary: #6a6e73;
      --pf-color-link: #0066cc;
      --pf-color-link-hover: #004080;
      --pf-border-color: #d2d2d2;
      --pf-border-subtle: #ebebeb;
      --pf-row-hover: #f5f8fa;
      --pf-table-header-bg: #f8f9fa;
      --pf-badge-bg: #e7e8ea;
      --pf-badge-color: #151515;
      --pf-code-bg: #f8f9fa;
      --pf-json-key: #0066cc;
      --pf-json-str: #1b7a2b;
      --pf-json-num: #8250df;
      --pf-json-bool: #cf222e;
      --pf-json-null: #6a6e73;
      --pf-json-bracket: #24292f;
    }
    html.pf-v6-theme-dark {
      --pf-bg-page: #0f1214;
      --pf-bg-card: #1b1d21;
      --pf-bg-masthead: #151515;
      --pf-color-text: #f0f0f0;
      --pf-color-text-secondary: #8a8d90;
      --pf-color-link: #73bcf7;
      --pf-color-link-hover: #a1d1fa;
      --pf-border-color: #383838;
      --pf-border-subtle: #292d32;
      --pf-row-hover: #26292d;
      --pf-table-header-bg: #212427;
      --pf-badge-bg: #2c2f33;
      --pf-badge-color: #f0f0f0;
      --pf-code-bg: #13161a;
      --pf-json-key: #73bcf7;
      --pf-json-str: #7ee787;
      --pf-json-num: #d2a8ff;
      --pf-json-bool: #ff7b72;
      --pf-json-null: #8b949e;
      --pf-json-bracket: #c9d1d9;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      padding: 0;
      font-family: var(--pf-font-family);
      font-size: 14px;
      line-height: 1.5;
      color: var(--pf-color-text);
      background-color: var(--pf-bg-page);
      -webkit-font-smoothing: antialiased;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
    }
    .pf-v6-c-masthead {
      background-color: var(--pf-bg-masthead);
      color: #ffffff;
      height: 60px;
      padding: 0 24px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      box-shadow: 0 1px 3px rgba(0, 0, 0, 0.2);
      position: sticky;
      top: 0;
      z-index: 100;
    }
    .pf-v6-c-masthead__brand {
      display: flex;
      align-items: center;
      gap: 10px;
      text-decoration: none;
      color: #ffffff;
    }
    .pf-brand-logo {
      font-size: 18px;
      font-weight: 700;
      letter-spacing: -0.02em;
      color: #ffffff;
    }
    .pf-brand-badge {
      font-size: 11px;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      background-color: #c9190b;
      color: #ffffff;
      padding: 2px 6px;
      border-radius: 3px;
      font-weight: 600;
    }
    .pf-masthead-actions {
      display: flex;
      align-items: center;
      gap: 16px;
    }
    .pf-back-link {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      color: #d2d2d2;
      text-decoration: none;
      font-size: 13px;
      padding: 6px 10px;
      border-radius: 4px;
      transition: background-color 0.15s, color 0.15s;
    }
    .pf-back-link:hover {
      color: #ffffff;
      background-color: rgba(255, 255, 255, 0.1);
    }
    .pf-theme-toggle-btn {
      background: transparent;
      border: 1px solid #3c3f42;
      color: #d2d2d2;
      border-radius: 4px;
      width: 34px;
      height: 34px;
      display: flex;
      align-items: center;
      justify-content: center;
      cursor: pointer;
      padding: 0;
      transition: color 0.15s, border-color 0.15s, background-color 0.15s;
    }
    .pf-theme-toggle-btn:hover {
      color: #ffffff;
      border-color: #6a6e73;
      background-color: rgba(255, 255, 255, 0.08);
    }
    .sun-icon { display: none; }
    .moon-icon { display: block; }
    html.pf-v6-theme-dark .sun-icon { display: block; }
    html.pf-v6-theme-dark .moon-icon { display: none; }
    .pf-v6-c-page__breadcrumb {
      background-color: var(--pf-bg-card);
      border-bottom: 1px solid var(--pf-border-color);
      padding: 12px 0;
    }
    .pf-container {
      max-width: 1320px;
      width: 100%;
      margin: 0 auto;
      padding: 0 24px;
    }
    .pf-v6-c-breadcrumb__list {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      list-style: none;
      margin: 0;
      padding: 0;
      gap: 8px;
      font-size: 13px;
    }
    .pf-v6-c-breadcrumb__item a {
      color: var(--pf-color-link);
      text-decoration: none;
    }
    .pf-v6-c-breadcrumb__item a:hover {
      text-decoration: underline;
      color: var(--pf-color-link-hover);
    }
    .pf-v6-c-breadcrumb__item-divider {
      color: var(--pf-color-text-secondary);
      user-select: none;
    }
    .pf-v6-c-breadcrumb__item--active {
      color: var(--pf-color-text);
      font-weight: 600;
    }
    .pf-v6-c-page__main {
      flex: 1;
      padding: 24px 0;
    }
    .pf-v6-c-card {
      background-color: var(--pf-bg-card);
      border: 1px solid var(--pf-border-color);
      border-radius: 8px;
      box-shadow: 0 1px 3px rgba(0, 0, 0, 0.05);
      overflow: hidden;
    }
    .pf-v6-c-card__header {
      padding: 16px 20px;
      border-bottom: 1px solid var(--pf-border-subtle);
      display: flex;
      align-items: center;
      justify-content: space-between;
    }
    .pf-v6-c-card__title {
      margin: 0;
      font-size: 16px;
      font-weight: 600;
      color: var(--pf-color-text);
    }
    .pf-file-count-badge {
      font-size: 12px;
      font-weight: 500;
      background-color: var(--pf-badge-bg);
      color: var(--pf-badge-color);
      padding: 2px 8px;
      border-radius: 10px;
    }
    .pf-v6-c-card__body {
      padding: 0;
      overflow-x: auto;
    }
    table.dir-listing.pf-v6-c-table {
      width: 100%;
      border-collapse: collapse;
      text-align: left;
    }
    table.dir-listing.pf-v6-c-table thead th {
      background-color: var(--pf-table-header-bg);
      color: var(--pf-color-text-secondary);
      font-size: 12px;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 0.04em;
      padding: 10px 16px;
      border-bottom: 1px solid var(--pf-border-color);
    }
    table.dir-listing.pf-v6-c-table tbody tr {
      border-bottom: 1px solid var(--pf-border-subtle);
      transition: background-color 0.1s ease;
    }
    table.dir-listing.pf-v6-c-table tbody tr:hover {
      background-color: var(--pf-row-hover);
    }
    table.dir-listing.pf-v6-c-table tbody td {
      padding: 10px 16px;
      font-size: 13px;
      vertical-align: middle;
    }
    table.dir-listing.pf-v6-c-table tbody td a {
      color: var(--pf-color-link);
      text-decoration: none;
      font-weight: 500;
    }
    table.dir-listing.pf-v6-c-table tbody td a:hover {
      text-decoration: underline;
      color: var(--pf-color-link-hover);
    }
    table.dir-listing tbody tr.directory td:first-child a::before {
      content: "";
      display: inline-block;
      width: 16px;
      height: 16px;
      margin-right: 8px;
      vertical-align: -2px;
      background-repeat: no-repeat;
      background-size: contain;
      background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='%230066cc'%3E\
%3Cpath d='M10 4H4c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z'/%3E%3C/svg%3E");
    }
    html.pf-v6-theme-dark table.dir-listing tbody tr.directory td:first-child a::before {
      background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='%2373bcf7'%3E\
%3Cpath d='M10 4H4c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z'/%3E%3C/svg%3E");
    }
    table.dir-listing tbody tr.parent-dir td:first-child a::before {
      content: "";
      display: inline-block;
      width: 16px;
      height: 16px;
      margin-right: 8px;
      vertical-align: -2px;
      background-repeat: no-repeat;
      background-size: contain;
      background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' \
viewBox='0 0 24 24' fill='none' stroke='%230066cc' stroke-width='2.2'%3E\
%3Cpath d='M12 19V5M5 12l7-7 7 7'/%3E%3C/svg%3E");
    }
    html.pf-v6-theme-dark table.dir-listing tbody tr.parent-dir td:first-child a::before {
      background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' \
viewBox='0 0 24 24' fill='none' stroke='%2373bcf7' stroke-width='2.2'%3E\
%3Cpath d='M12 19V5M5 12l7-7 7 7'/%3E%3C/svg%3E");
    }
    table.dir-listing tbody tr:not(.directory) td:first-child a::before {
      content: "";
      display: inline-block;
      width: 16px;
      height: 16px;
      margin-right: 8px;
      vertical-align: -2px;
      background-repeat: no-repeat;
      background-size: contain;
      background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' \
viewBox='0 0 24 24' fill='none' stroke='%236a6e73' stroke-width='2'%3E\
%3Cpath d='M13 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z'/%3E%3Cpolyline points='13 2 13 9 20 9'/%3E%3C/svg%3E");
    }
    html.pf-v6-theme-dark table.dir-listing tbody tr:not(.directory) td:first-child a::before {
      background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' \
viewBox='0 0 24 24' fill='none' stroke='%238a8d90' stroke-width='2'%3E\
%3Cpath d='M13 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z'/%3E%3Cpolyline points='13 2 13 9 20 9'/%3E%3C/svg%3E");
    }
    .pf-text-right { text-align: right; }
    .pf-cell-muted { color: var(--pf-color-text-secondary); }
    .pf-file-type-badge {
      display: inline-block;
      padding: 2px 7px;
      border-radius: 4px;
      font-size: 11px;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 0.03em;
    }
    .pf-type-json { background-color: rgba(0, 102, 204, 0.12); color: #0066cc; }
    .pf-type-text { background-color: rgba(46, 125, 50, 0.12); color: #2e7d32; }
    .pf-type-archive { background-color: rgba(142, 68, 173, 0.12); color: #8e44ad; }
    .pf-type-file { background-color: var(--pf-badge-bg); color: var(--pf-color-text-secondary); }
    html.pf-v6-theme-dark .pf-type-json { background-color: rgba(115, 188, 247, 0.2); color: #73bcf7; }
    html.pf-v6-theme-dark .pf-type-text { background-color: rgba(126, 231, 135, 0.2); color: #7ee787; }
    html.pf-v6-theme-dark .pf-type-archive { background-color: rgba(210, 168, 255, 0.2); color: #d2a8ff; }
    .pf-download-link {
      display: inline-flex;
      align-items: center;
      font-size: 12px;
      font-weight: 500;
      padding: 3px 8px;
      border: 1px solid var(--pf-border-color);
      border-radius: 4px;
      color: var(--pf-color-link);
      text-decoration: none;
      transition: background-color 0.15s, border-color 0.15s;
    }
    .pf-download-link:hover {
      background-color: var(--pf-row-hover);
      border-color: var(--pf-color-link);
      text-decoration: none;
    }
    .pf-viewer-header {
      padding: 14px 20px;
      border-bottom: 1px solid var(--pf-border-subtle);
      display: flex;
      align-items: center;
      justify-content: space-between;
      flex-wrap: wrap;
      gap: 12px;
    }
    .pf-viewer-title-group {
      display: flex;
      align-items: center;
      gap: 12px;
      flex-wrap: wrap;
    }
    .pf-back-dir-btn {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      width: 32px;
      height: 32px;
      border: 1px solid var(--pf-border-color);
      border-radius: 4px;
      color: var(--pf-color-link);
      text-decoration: none;
      transition: background-color 0.15s, border-color 0.15s;
    }
    .pf-back-dir-btn:hover {
      background-color: var(--pf-row-hover);
      border-color: var(--pf-color-link);
    }
    .pf-viewer-title {
      margin: 0;
      font-size: 15px;
      font-weight: 600;
      font-family: var(--pf-font-mono);
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .pf-file-badge {
      font-size: 12px;
      font-weight: 500;
      background-color: var(--pf-badge-bg);
      color: var(--pf-badge-color);
      padding: 2px 8px;
      border-radius: 10px;
    }
    .pf-viewer-actions {
      display: flex;
      align-items: center;
      gap: 8px;
      flex-wrap: wrap;
    }
    .pf-btn-group {
      display: inline-flex;
      border: 1px solid var(--pf-border-color);
      border-radius: 4px;
      overflow: hidden;
    }
    .pf-view-btn {
      background: transparent;
      border: none;
      padding: 5px 12px;
      font-size: 12px;
      font-weight: 500;
      color: var(--pf-color-text-secondary);
      cursor: pointer;
      transition: background-color 0.15s, color 0.15s;
    }
    .pf-view-btn.active {
      background: var(--pf-color-link);
      color: #ffffff;
    }
    .pf-action-btn {
      display: inline-flex;
      align-items: center;
      gap: 5px;
      padding: 5px 10px;
      font-size: 12px;
      font-weight: 500;
      border: 1px solid var(--pf-border-color);
      border-radius: 4px;
      background: var(--pf-bg-card);
      color: var(--pf-color-text);
      text-decoration: none;
      cursor: pointer;
      transition: background-color 0.15s, border-color 0.15s;
    }
    .pf-action-btn:hover {
      background: var(--pf-row-hover);
      border-color: var(--pf-color-link);
      color: var(--pf-color-link);
    }
    .pf-action-btn.copied {
      background-color: #2e7d32;
      color: #ffffff;
      border-color: #2e7d32;
    }
    .pf-action-primary {
      background: var(--pf-color-link);
      color: #ffffff;
      border-color: var(--pf-color-link);
    }
    .pf-action-primary:hover {
      background: var(--pf-color-link-hover);
      color: #ffffff;
    }
    .pf-viewer-body {
      padding: 0;
      background-color: var(--pf-code-bg);
      font-family: var(--pf-font-mono);
      font-size: 13px;
      line-height: 1.6;
      overflow-x: auto;
      max-height: 80vh;
      overflow-y: auto;
    }
    .pf-json-tree {
      padding: 16px 20px;
    }
    details.json-node {
      margin-left: 18px;
    }
    .pf-json-tree > details.json-node {
      margin-left: 0;
    }
    summary.json-summary {
      cursor: pointer;
      outline: none;
      user-select: none;
      padding: 2px 0;
      white-space: pre-wrap;
      word-break: break-word;
    }
    summary.json-summary:hover {
      background-color: var(--pf-row-hover);
      border-radius: 3px;
    }
    .json-children {
      border-left: 1px dashed var(--pf-border-color);
    }
    .json-row {
      margin-left: 18px;
      white-space: pre-wrap;
      word-break: break-word;
      padding: 1px 0;
    }
    .json-close {
      margin-left: 18px;
    }
    .pf-json-tree > details.json-node > .json-close {
      margin-left: 0;
    }
    .json-key { color: var(--pf-json-key); font-weight: 600; }
    .json-str { color: var(--pf-json-str); word-break: break-all; }
    .json-num { color: var(--pf-json-num); }
    .json-bool { color: var(--pf-json-bool); font-weight: 600; }
    .json-null { color: var(--pf-json-null); font-style: italic; }
    .json-brace { color: var(--pf-json-bracket); font-weight: bold; }
    .json-badge {
      font-size: 11px;
      font-family: var(--pf-font-family);
      background-color: var(--pf-badge-bg);
      color: var(--pf-color-text-secondary);
      padding: 1px 6px;
      border-radius: 8px;
      margin-left: 6px;
      user-select: none;
    }
    .pf-code-pre {
      margin: 0;
      padding: 16px 20px;
      font-family: var(--pf-font-mono);
      font-size: 13px;
      line-height: 1.5;
      color: var(--pf-color-text);
      white-space: pre;
      overflow-x: auto;
    }
    .pf-tree-disabled {
      padding: 24px;
      color: var(--pf-color-text-secondary);
      font-style: italic;
    }
    .pf-page-footer {
      padding: 16px 0 24px;
      text-align: center;
      font-size: 12px;
      color: var(--pf-color-text-secondary);
      border-top: 1px solid var(--pf-border-subtle);
      background-color: var(--pf-bg-card);
    }
"""

_COMMON_SCRIPT = """
    (function () {
      function applyTheme() {
        try {
          var stored = localStorage.getItem('pf-theme');
          var isDark = stored === 'dark' || (stored === null && window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches);
          document.documentElement.classList.toggle('pf-v6-theme-dark', isDark);
        } catch (e) {}
      }
      applyTheme();
      document.addEventListener('DOMContentLoaded', function () {
        var toggleBtn = document.getElementById('theme-toggle');
        if (toggleBtn) {
          toggleBtn.addEventListener('click', function () {
            var isDark = document.documentElement.classList.toggle('pf-v6-theme-dark');
            try {
              localStorage.setItem('pf-theme', isDark ? 'dark' : 'light');
            } catch (e) {}
          });
        }
        if (window.matchMedia) {
          window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', function (e) {
            try {
              if (localStorage.getItem('pf-theme') === null) {
                document.documentElement.classList.toggle('pf-v6-theme-dark', e.matches);
              }
            } catch (err) {}
          });
        }

        // View toggle: Tree vs Code
        var btnTree = document.getElementById('btn-tree-view');
        var btnCode = document.getElementById('btn-code-view');
        var treeView = document.getElementById('pf-tree-view');
        var codeView = document.getElementById('pf-code-view');
        var treeControls = document.getElementById('pf-tree-controls');

        if (btnTree && btnCode && treeView && codeView) {
          btnTree.addEventListener('click', function () {
            btnTree.classList.add('active');
            btnCode.classList.remove('active');
            treeView.style.display = 'block';
            codeView.style.display = 'none';
            if (treeControls) treeControls.style.display = 'inline-flex';
          });
          btnCode.addEventListener('click', function () {
            btnCode.classList.add('active');
            btnTree.classList.remove('active');
            treeView.style.display = 'none';
            codeView.style.display = 'block';
            if (treeControls) treeControls.style.display = 'none';
          });
        }

        // Expand / Collapse All
        var btnExpand = document.getElementById('btn-expand-all');
        var btnCollapse = document.getElementById('btn-collapse-all');
        if (btnExpand) {
          btnExpand.addEventListener('click', function () {
            document.querySelectorAll('.pf-json-tree details.json-node').forEach(function (d) {
              d.open = true;
            });
          });
        }
        if (btnCollapse) {
          btnCollapse.addEventListener('click', function () {
            document.querySelectorAll('.pf-json-tree details.json-node').forEach(function (d, i) {
              if (i > 0) d.open = false;
            });
          });
        }

        // Copy to Clipboard
        var btnCopy = document.getElementById('btn-copy');
        if (btnCopy) {
          btnCopy.addEventListener('click', function () {
            var textEl = document.getElementById('raw-text-content');
            var text = textEl ? textEl.textContent : '';
            function showCopied() {
              var origHtml = btnCopy.innerHTML;
              btnCopy.innerHTML = '<svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5">\
<polyline points="20 6 9 17 4 12"/></svg> Copied!';
              btnCopy.classList.add('copied');
              setTimeout(function () {
                btnCopy.innerHTML = origHtml;
                btnCopy.classList.remove('copied');
              }, 2000);
            }
            if (navigator.clipboard && navigator.clipboard.writeText) {
              navigator.clipboard.writeText(text).then(showCopied);
            } else {
              var ta = document.createElement('textarea');
              ta.value = text;
              ta.style.position = 'fixed';
              ta.style.opacity = '0';
              document.body.appendChild(ta);
              ta.select();
              try {
                document.execCommand('copy');
                showCopied();
              } catch (err) {}
              document.body.removeChild(ta);
            }
          });
        }
      });
    })();
"""


def _render_page_header():
    return """  <header class="pf-v6-c-masthead">
    <div class="pf-v6-c-masthead__brand">
      <a class="pf-v6-c-masthead__brand" href="/fbrat" title="Return to OpenAFC Dashboard">
        <span class="pf-brand-logo">OpenAFC</span>
      </a>
      <span class="pf-brand-badge">Debug Files</span>
    </div>
    <div class="pf-masthead-actions">
      <a class="pf-back-link" href="/fbrat">
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M19 12H5M12 19l-7-7 7-7"/></svg>
        Back to Dashboard
      </a>
      <button id="theme-toggle" class="pf-theme-toggle-btn" type="button" aria-label="Toggle light/dark theme" title="Toggle theme">
        <svg class="sun-icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <circle cx="12" cy="12" r="5"/><path d="M12 1v2M12 21v2M4.22 4.22l1.42 1.42M18.36 18.36l1.42 1.42M1 12h2M21 12h2"/>
        </svg>
        <svg class="moon-icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"/>
        </svg>
      </button>
    </div>
  </header>"""


def _render_page_footer():
    return """  <footer class="pf-page-footer">
    <div class="pf-container">
      <span>OpenAFC Automated Frequency Coordination</span>
    </div>
  </footer>"""


def generateHtml(rurl, path, dirs, files):
    hist_app.logger.debug(f"generateHtml({rurl}, {path}, {dirs}, {files})")
    dirs.sort()
    files.sort()

    vpath = "history"
    if path:
        vpath += "/" + path.strip("/")

    bc_str = _build_breadcrumbs(rurl, path)

    rows_html = []
    base_href = rurl.rstrip("/")
    if path and path.strip("/"):
        base_href += "/" + path.strip("/")

    # Parent directory row
    if path and path.strip("/"):
        parent_parts = path.strip("/").split("/")[:-1]
        parent_href = rurl.rstrip("/") + ("/" + "/".join(parent_parts) if parent_parts else "")
        rows_html.append(
            f'<tr class="directory parent-dir">'
            f'<td><a href="{escape(parent_href)}">..</a></td>'
            f'<td>Directory</td>'
            f'<td class="pf-text-right pf-cell-muted">-</td>'
            f'</tr>\n'
        )

    for d in dirs:
        d_clean = d.strip("/")
        href = f"{base_href}/{d_clean}"
        rows_html.append(
            f'<tr class="directory">'
            f'<td><a href="{escape(href)}">{escape(d_clean)}/</a></td>'
            f'<td>Directory</td>'
            f'<td class="pf-text-right pf-cell-muted">-</td>'
            f'</tr>\n'
        )

    for f in files:
        href = f"{base_href}/{f}"
        dl_href = f"{href}?download=1"
        ext = os.path.splitext(f)[1].lower()
        if f.endswith(".json.gz"):
            type_label = "JSON (gz)"
            type_cls = "json"
        elif f.endswith(".csv.gz"):
            type_label = "CSV (gz)"
            type_cls = "text"
        elif ext == ".json":
            type_label = "JSON"
            type_cls = "json"
        elif ext in (".txt", ".log"):
            type_label = "Text"
            type_cls = "text"
        elif ext == ".csv":
            type_label = "CSV"
            type_cls = "text"
        elif ext == ".kmz":
            type_label = "KMZ"
            type_cls = "archive"
        elif ext == ".gz":
            type_label = "Gzip"
            type_cls = "archive"
        else:
            type_label = "File"
            type_cls = "file"

        rows_html.append(
            f'<tr>'
            f'<td><a href="{escape(href)}">{escape(f)}</a></td>'
            f'<td><span class="pf-file-type-badge pf-type-{type_cls}">{escape(type_label)}</span></td>'
            f'<td class="pf-text-right"><a class="pf-download-link" href="{escape(dl_href)}" download="{escape(f)}">'
            f'<svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" '
            f'style="margin-right:4px;vertical-align:-1px"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4M7 10l5 5 5-5M12 15V3"/>'
            f'</svg>Download</a></td>'
            f'</tr>\n')

    total_items = len(dirs) + len(files)
    rows_str = "".join(rows_html)

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>OpenAFC - Index of /{escape(vpath)}</title>
  <style>{_COMMON_CSS}</style>
  <script>{_COMMON_SCRIPT}</script>
</head>
<body>
{_render_page_header()}
  <div class="pf-v6-c-page__breadcrumb">
    <div class="pf-container">
      <nav class="pf-v6-c-breadcrumb" aria-label="Breadcrumb">
        <ol class="pf-v6-c-breadcrumb__list">
          {bc_str}
        </ol>
      </nav>
    </div>
  </div>

  <main class="pf-v6-c-page__main">
    <div class="pf-container">
      <div class="pf-v6-c-card">
        <div class="pf-v6-c-card__header">
          <h1 class="pf-v6-c-card__title">Index of /{escape(vpath)}</h1>
          <span class="pf-file-count-badge">{total_items} items</span>
        </div>
        <div class="pf-v6-c-card__body">
          <table class="dir-listing pf-v6-c-table">
            <thead>
              <tr>
                <th scope="col">Name</th>
                <th scope="col">Type</th>
                <th scope="col" class="pf-text-right">Action</th>
              </tr>
            </thead>
            <tbody>
              {rows_str}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  </main>
{_render_page_footer()}
</body>
</html>
"""
    hist_app.logger.debug(html)
    return html.encode()


def generateJsonViewerHtml(rurl, path, parsed_json, raw_bytes, is_gz=False):
    fname = os.path.basename(path)
    base_href = rurl.rstrip("/")
    if path and path.strip("/"):
        base_href += "/" + path.strip("/")
    view_url = base_href
    raw_url = f"{view_url}?raw=1"
    download_url = f"{view_url}?download=1"

    parent_parts = path.strip("/").split("/")[:-1]
    dir_url = rurl.rstrip("/") + ("/" + "/".join(parent_parts) if parent_parts else "")

    bc_str = _build_breadcrumbs(rurl, path)
    size_str = _format_size(len(raw_bytes))
    if is_gz:
        size_str += " (decompressed)"

    if isinstance(parsed_json, dict):
        count_str = f"{len(parsed_json)} keys" if len(parsed_json) != 1 else "1 key"
    elif isinstance(parsed_json, list):
        count_str = f"{len(parsed_json)} items" if len(parsed_json) != 1 else "1 item"
    else:
        count_str = "value"

    formatted_json_str = json.dumps(parsed_json, indent=2)

    has_tree = len(raw_bytes) <= 2 * 1024 * 1024
    if has_tree:
        tree_body = _render_json_node(None, parsed_json)
        tree_html = f'<div class="pf-json-tree">{tree_body}</div>'
    else:
        tree_html = '<div class="pf-tree-disabled">Tree view disabled for payloads &gt; 2 MB. Switch to Code view.</div>'

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>OpenAFC - {escape(fname)}</title>
  <style>{_COMMON_CSS}</style>
  <script>{_COMMON_SCRIPT}</script>
</head>
<body>
{_render_page_header()}
  <div class="pf-v6-c-page__breadcrumb">
    <div class="pf-container">
      <nav class="pf-v6-c-breadcrumb" aria-label="Breadcrumb">
        <ol class="pf-v6-c-breadcrumb__list">
          {bc_str}
        </ol>
      </nav>
    </div>
  </div>

  <main class="pf-v6-c-page__main">
    <div class="pf-container">
      <div class="pf-v6-c-card">
        <div class="pf-viewer-header">
          <div class="pf-viewer-title-group">
            <a href="{escape(dir_url)}" class="pf-back-dir-btn" title="Back to directory">
              <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2">
                <path d="M19 12H5M12 19l-7-7 7-7"/>
              </svg>
            </a>
            <h1 class="pf-viewer-title">
              <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="color:var(--pf-color-link)">
                <path d="M13 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z"/><polyline points="13 2 13 9 20 9"/>
              </svg>
              <span>{escape(fname)}</span>
            </h1>
            <span class="pf-file-badge">{escape(size_str)}</span>
            <span class="pf-file-badge">{escape(count_str)}</span>
          </div>
          <div class="pf-viewer-actions">
            <div class="pf-btn-group">
              <button id="btn-tree-view" class="pf-view-btn active" type="button">Tree</button>
              <button id="btn-code-view" class="pf-view-btn" type="button">Code</button>
            </div>
            <div id="pf-tree-controls" class="pf-tree-actions" style="display:inline-flex;gap:6px">
              <button id="btn-expand-all" class="pf-action-btn" type="button" title="Expand all nodes">Expand All</button>
              <button id="btn-collapse-all" class="pf-action-btn" type="button" title="Collapse all nodes">Collapse All</button>
            </div>
            <button id="btn-copy" class="pf-action-btn" type="button" title="Copy JSON to clipboard">
              <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <rect x="9" y="9" width="13" height="13" rx="2" ry="2"/><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/>
              </svg>
              Copy
            </button>
            <a class="pf-action-btn" href="{escape(raw_url)}" target="_blank" title="View raw JSON">
              <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <polyline points="16 18 22 12 16 6"/><polyline points="8 6 2 12 8 18"/>
              </svg>
              Raw
            </a>
            <a class="pf-action-btn pf-action-primary" href="{escape(download_url)}" download="{escape(fname)}" title="Download file">
              <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4M7 10l5 5 5-5M12 15V3"/>
              </svg>
              Download
            </a>
          </div>
        </div>
        <div class="pf-viewer-body">
          <div id="pf-tree-view">
            {tree_html}
          </div>
          <div id="pf-code-view" style="display:none">
            <pre class="pf-code-pre"><code>{escape(formatted_json_str)}</code></pre>
          </div>
        </div>
        <textarea id="raw-text-content" style="display:none">{escape(formatted_json_str)}</textarea>
      </div>
    </div>
  </main>
{_render_page_footer()}
</body>
</html>
"""
    return html.encode()


def generateTextViewerHtml(rurl, path, text_content, raw_bytes, is_gz=False):
    fname = os.path.basename(path)
    base_href = rurl.rstrip("/")
    if path and path.strip("/"):
        base_href += "/" + path.strip("/")
    view_url = base_href
    raw_url = f"{view_url}?raw=1"
    download_url = f"{view_url}?download=1"

    parent_parts = path.strip("/").split("/")[:-1]
    dir_url = rurl.rstrip("/") + ("/" + "/".join(parent_parts) if parent_parts else "")

    bc_str = _build_breadcrumbs(rurl, path)
    size_str = _format_size(len(raw_bytes))
    if is_gz:
        size_str += " (decompressed)"

    line_count = len(text_content.splitlines())
    count_str = f"{line_count} line" if line_count == 1 else f"{line_count} lines"

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>OpenAFC - {escape(fname)}</title>
  <style>{_COMMON_CSS}</style>
  <script>{_COMMON_SCRIPT}</script>
</head>
<body>
{_render_page_header()}
  <div class="pf-v6-c-page__breadcrumb">
    <div class="pf-container">
      <nav class="pf-v6-c-breadcrumb" aria-label="Breadcrumb">
        <ol class="pf-v6-c-breadcrumb__list">
          {bc_str}
        </ol>
      </nav>
    </div>
  </div>

  <main class="pf-v6-c-page__main">
    <div class="pf-container">
      <div class="pf-v6-c-card">
        <div class="pf-viewer-header">
          <div class="pf-viewer-title-group">
            <a href="{escape(dir_url)}" class="pf-back-dir-btn" title="Back to directory">
              <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2">
                <path d="M19 12H5M12 19l-7-7 7-7"/>
              </svg>
            </a>
            <h1 class="pf-viewer-title">
              <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="color:var(--pf-color-link)">
                <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/>
                <line x1="16" y1="13" x2="8" y2="13"/><line x1="16" y1="17" x2="8" y2="17"/><polyline points="10 9 9 9 8 9"/>
              </svg>
              <span>{escape(fname)}</span>
            </h1>
            <span class="pf-file-badge">{escape(size_str)}</span>
            <span class="pf-file-badge">{escape(count_str)}</span>
          </div>
          <div class="pf-viewer-actions">
            <button id="btn-copy" class="pf-action-btn" type="button" title="Copy text to clipboard">
              <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <rect x="9" y="9" width="13" height="13" rx="2" ry="2"/><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/>
              </svg>
              Copy
            </button>
            <a class="pf-action-btn" href="{escape(raw_url)}" target="_blank" title="View raw text">
              <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <polyline points="16 18 22 12 16 6"/><polyline points="8 6 2 12 8 18"/>
              </svg>
              Raw
            </a>
            <a class="pf-action-btn pf-action-primary" href="{escape(download_url)}" download="{escape(fname)}" title="Download file">
              <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4M7 10l5 5 5-5M12 15V3"/>
              </svg>
              Download
            </a>
          </div>
        </div>
        <div class="pf-viewer-body">
          <pre class="pf-code-pre"><code>{escape(text_content)}</code></pre>
        </div>
        <textarea id="raw-text-content" style="display:none">{escape(text_content)}</textarea>
      </div>
    </div>
  </main>
{_render_page_footer()}
</body>
</html>
"""
    return html.encode()


class ObjInt:
    """ Abstract class for data prot operations """
    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def isdir(self):
        pass

    @abc.abstractmethod
    def list(self):
        pass

    @abc.abstractmethod
    def read(self):
        pass

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        pass  # No resources to clean up in base class


class ObjIntLocalFS(ObjInt):
    def __init__(self, file_name):
        self.__file_name = file_name

    def isdir(self):
        return os.path.isdir(self.__file_name)

    def list(self):
        hist_app.logger.debug("ObjIntLocalFS.list")
        ls = os.listdir(self.__file_name)
        files = [f for f in ls if os.path.isfile(
            os.path.join(self.__file_name, f))]
        dirs = [f for f in ls if os.path.isdir(
            os.path.join(self.__file_name, f))]
        return dirs, files

    def read(self):
        hist_app.logger.debug(
            "ObjIntLocalFS.read({})".format(self.__file_name))
        if os.path.isfile(self.__file_name):
            with open(self.__file_name, "rb") as hfile:
                return hfile.read()
        return None


class ObjIntGoogleCloudBucket(ObjInt):
    def __init__(self, file_name):
        self.__file_name = file_name
        self.__blob = bucket.blob(self.__file_name)

    def isdir(self):
        return not self.__blob.exists()

    def list(self):
        hist_app.logger.debug("ObjIntGoogleCloudBucket.list")
        blobs = bucket.list_blobs(prefix=self.__file_name + "/")
        files = []
        dirs = set()
        for blob in blobs:
            name = blob.name.removeprefix(self.__file_name + "/")
            if name.count("/"):
                dirs.add(name.split("/")[0])
            else:
                files.append(name)
        return list(dirs), files

    def read(self):
        blob = bucket.blob(self.__file_name)
        return blob.download_as_bytes(raw_download=True,
                                      timeout=NET_TIMEOUT)


class Objstorage:
    def open(self, name):
        """ Create ObjInt instance """
        hist_app.logger.debug("Objstorage.open({})".format(name))
        if hist_app.config["AFC_OBJST_MEDIA"] == "GoogleCloudBucket":
            return ObjIntGoogleCloudBucket(name)
        if hist_app.config["AFC_OBJST_MEDIA"] == "LocalFS":
            return ObjIntLocalFS(name)
        raise RuntimeError("Unsupported AFC_OBJST_MEDIA \"{}\"".
                           format(hist_app.config["AFC_OBJST_MEDIA"]))


@hist_app.route('/', defaults={'path': ""}, methods=['GET'])
@hist_app.route('/' + '<path:path>', methods=['GET'])
def get(path):
    ''' File download and viewing handler. '''
    # ratapi URL preffix
    rurl = request.args["url"]
    # Validate scheme to block non-HTTP(S) URI schemes, and reject any URL that
    # carries a network location (host) or is protocol-relative ('//host'): the
    # link base must stay same-origin so generateHtml() never emits off-origin
    # navigation hrefs.
    import urllib.parse as _urlparse
    _parsed_rurl = _urlparse.urlparse(rurl)
    if _parsed_rurl.scheme not in ('http', 'https', ''):
        return abort(400)
    if rurl.startswith('//') or _parsed_rurl.netloc:
        # Only same-origin is permitted; the proxy supplies request.base_url.
        # A client-supplied off-origin base is rejected.
        if _parsed_rurl.netloc != request.host:
            return abort(400)
    fwd_proto = request.headers.get('X-Forwarded-Proto')
    if (fwd_proto == 'https') and (request.scheme == "http"):
        rurl = rurl.replace("http:", "https:")
    hist_app.logger.debug(
        f'get method={request.method}, path={path} url={rurl}')
    # local path in the storage
    hroot = os.path.realpath(os.path.join(
        hist_app.config["AFC_OBJST_FILE_LOCATION"], "history"))
    os.makedirs(hroot, exist_ok=True)
    lpath = os.path.realpath(os.path.join(hroot, path))
    # This uses os.path.commonpath instead of manual os.sep checks
    if lpath != hroot and os.path.commonpath([lpath, hroot]) != hroot:
        hist_app.logger.error(
            "get invalid path rejected: {}".format(path))
        return abort(400)

    try:
        objst = Objstorage()
        with objst.open(lpath) as hobj:
            if hobj.isdir() is True:
                dirs, files = hobj.list()
                html_bytes = generateHtml(rurl, path, dirs, files)
                response = Response(html_bytes, content_type="text/html; charset=utf-8")
                response.headers["X-Content-Type-Options"] = "nosniff"
                return response
            data = hobj.read()
            if data is None:
                return abort(404)

            fname = os.path.basename(path)
            is_download = request.args.get("download") == "1"
            is_raw = request.args.get("raw") == "1"

            # If download was requested explicitly, return exact bytes as attachment
            if is_download:
                response = helpers.send_file(
                    io.BytesIO(data),
                    mimetype="application/octet-stream",
                    as_attachment=True,
                    download_name=fname)
                response.headers["X-Content-Type-Options"] = "nosniff"
                return response

            # Check if this is a gzip file that can be unpacked for preview
            decompressed_data = None
            if fname.endswith(".gz"):
                try:
                    d_buf = gzip.decompress(data)
                    # Defensively cap decompressed preview buffer to 10 MB
                    if len(d_buf) <= 10 * 1024 * 1024:
                        decompressed_data = d_buf
                except Exception:
                    decompressed_data = None

            preview_data = decompressed_data if decompressed_data is not None else data
            view_fname = fname[:-3] if (fname.endswith(".gz") and decompressed_data is not None) else fname
            ext = os.path.splitext(view_fname)[1].lower()

            # Safe inline rendering for JSON
            if ext == ".json":
                parsed_json = None
                try:
                    parsed_json = json.loads(preview_data.decode("utf-8"))
                except Exception:
                    pass

                if parsed_json is not None:
                    if not is_raw and _is_html_preferred(request):
                        html_bytes = generateJsonViewerHtml(
                            rurl, path, parsed_json, preview_data,
                            is_gz=(decompressed_data is not None))
                        response = Response(html_bytes, content_type="text/html; charset=utf-8")
                        response.headers["X-Content-Type-Options"] = "nosniff"
                        return response
                    else:
                        formatted_bytes = json.dumps(parsed_json, indent=2).encode("utf-8")
                        response = helpers.send_file(
                            io.BytesIO(formatted_bytes),
                            mimetype="application/json",
                            as_attachment=False,
                            download_name=view_fname)
                        response.headers["Content-Disposition"] = f'inline; filename="{view_fname}"'
                        response.headers["X-Content-Type-Options"] = "nosniff"
                        return response

            # Safe inline rendering for text / log / CSV
            if ext in (".txt", ".log", ".csv"):
                text_content = None
                for enc in ("utf-8", "latin-1"):
                    try:
                        text_content = preview_data.decode(enc)
                        break
                    except Exception:
                        continue

                if text_content is not None:
                    if not is_raw and _is_html_preferred(request):
                        html_bytes = generateTextViewerHtml(
                            rurl, path, text_content, preview_data,
                            is_gz=(decompressed_data is not None))
                        response = Response(html_bytes, content_type="text/html; charset=utf-8")
                        response.headers["X-Content-Type-Options"] = "nosniff"
                        return response
                    else:
                        response = helpers.send_file(
                            io.BytesIO(preview_data),
                            mimetype="text/plain",
                            as_attachment=False,
                            download_name=view_fname)
                        response.headers["Content-Disposition"] = f'inline; filename="{view_fname}"'
                        response.headers["X-Content-Type-Options"] = "nosniff"
                        return response

            # Default for all binary / unknown / active formats: force attachment download
            response = helpers.send_file(
                io.BytesIO(data),
                mimetype="application/octet-stream",
                as_attachment=True,
                download_name=fname)
            response.headers["X-Content-Type-Options"] = "nosniff"
            return response
    except Exception as e:
        hist_app.logger.error(e)
        return abort(500)


if __name__ == '__main__':
    import waitress
    os.makedirs(os.path.join(
        hist_app.config["AFC_OBJST_FILE_LOCATION"], "history"), exist_ok=True)
    waitress.serve(
        hist_app, port=hist_app.config["AFC_OBJST_HIST_PORT"], host="127.0.0.1")

    # hist_app.run(port=hist_app.config['AFC_OBJST_HIST_PORT'], host="0.0.0.0", debug=True)
