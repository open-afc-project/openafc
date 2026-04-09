# This Python file uses the following encoding: utf-8
#
# Portions copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate
# affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy
# of which is included with this software program.
#
from appcfg import OIDCConfigurator
import logging
import datetime
import hashlib
import base64
import os
import secrets
import werkzeug
import flask
from flask.views import MethodView
import requests
import jwt
from afcmodels.base import db
from afcmodels.aaa import User, Organization, Role
from flask_login import current_user
import als

OIDC_LOGIN = OIDCConfigurator().OIDC_LOGIN

if OIDC_LOGIN:
    from flask_login import (
        login_user,
        logout_user,
    )

LOGGER = logging.getLogger(__name__)

# using https://github.com/realpython/flask-jwt-auth
# LICENCE: MIT

module = flask.Blueprint('auth', __name__)

PY3 = False  # using Python2


def public_route(f):
    """Mark a view class or function as publicly accessible (no auth required)."""
    f.is_public = True
    return f


def auth(ignore_active=False, roles=None, is_user=None, org=None):
    ''' Provide application authentication for API methods. Returns the user_id if successful.

        When ``roles`` is non-empty, the caller must have at least one of those
        roles **before** any ``is_user`` ownership / org checks. (Self-service
        profile edits that intentionally skip the Admin role belong in a
        dedicated code path, not in ``auth(roles=["Admin"], is_user=own_id)``.)

        :param ignore_active: if True, allows inactive users to access resource

        :param roles: array of strings which specify the roles that can
        access the resource.  User must have at least 1 of the roles
        :type roles: str[]

        :param is_user: is a single user_id. checks if the user is identical.
        Use for personal information.

        :returns: user Id
        :rtype: int
    '''
    if not current_user.is_authenticated:
        raise werkzeug.exceptions.Unauthorized()
    (user_id, active, user_roles, cur_org) = \
        (current_user.id, current_user.active,
         [r.name for r in current_user.roles],
         current_user.org if current_user.org else "")

    # Super always has admin roles
    if "Super" in user_roles and "Admin" not in user_roles:
        user_roles.append("Admin")

    if not isinstance(user_id, str):
        if not active and not ignore_active:
            raise werkzeug.exceptions.Forbidden("Inactive user")

        if roles:
            found_role = False
            for r in roles:
                if r in user_roles:
                    LOGGER.debug(
                        "User %i is authenticated with role %s", user_id, r)
                    found_role = True
                    break

            if not found_role:
                raise werkzeug.exceptions.Forbidden(
                    "You do not have access to this resource")

        if is_user:
            if user_id == is_user:
                LOGGER.debug("User id matches: %i", user_id)
            elif "Super" in user_roles:
                return is_user  # return impersonating user. Don't check org
            elif "Admin" not in user_roles:
                raise werkzeug.exceptions.NotFound()
            target_user = User.get(is_user)
            if target_user is None:
                raise werkzeug.exceptions.NotFound()
            target_org = target_user.org if target_user.org else ""

        else:
            is_user = user_id
            target_org = cur_org if cur_org else ""
    else:
        raise werkzeug.exceptions.Unauthorized()

    # done checking roles/userid. now check org
    if "Super" not in user_roles:
        # Treat a falsy `org` argument the same as a mismatch: callers that
        # intend org-scoped access must pass a non-empty org string.  An empty
        # or None org used to silently short-circuit the check (CWE-285).
        if (not cur_org == target_org) or (org is not None and cur_org != org):
            if is_user != user_id:
                # Uniform 404: when the caller asked about another user's
                # id (is_user path), a cross-org target must be
                # indistinguishable from a nonexistent one, else the
                # 404/403 differential is a user-ID existence oracle
                # across organizations (mirrors the uniform-404 approach
                # documented in admin.py User.delete).
                raise werkzeug.exceptions.NotFound()
            raise werkzeug.exceptions.Forbidden(
                "You do not have access to this resource")

    return is_user


@public_route
class AboutLoginAPI(MethodView):
    ''' Allow the web UI to manipulate configuration directly.
    '''

    def get(self):
        ''' GET method for About
        '''

        resp = flask.make_response()

        resp.data = flask.render_template("about_login.html")
        resp.content_type = 'text/html'
        return resp


@public_route
class LoginAPI(MethodView):
    """
    User Login Resource
    """

    def get(self):
        # store app state and code verifier in session
        if not flask.current_app.config['OIDC_LOGIN']:
            return flask.redirect(flask.url_for('user.login'))

        next_url = flask.request.args.get('next')
        if next_url:
            flask.session['next'] = next_url
        else:
            flask.session.pop('next', None)

        flask.session['app_state'] = secrets.token_urlsafe(64)
        flask.session['code_verifier'] = secrets.token_urlsafe(64)
        flask.session['oidc_nonce'] = secrets.token_urlsafe(32)
        # calculate code challenge
        hashed = hashlib.sha256(
            flask.session['code_verifier'].encode('ascii')).digest()
        encoded = base64.urlsafe_b64encode(hashed)
        code_challenge = encoded.decode('ascii').strip('=')
        # OIDC_REDIRECT_BASE is mandatory when OIDC_LOGIN is enabled
        # (enforced in create_app()); never derive redirect_uri from the
        # client-supplied Host header.
        _oidc_redirect_base = flask.current_app.config['OIDC_REDIRECT_BASE']
        redirect_uri = _oidc_redirect_base.rstrip('/') + '/fbrat/auth/callback'
        # get request params
        query_params = {
            'client_id': flask.current_app.config['OIDC_CLIENT_ID'],
            'redirect_uri': redirect_uri,
            'scope': "openid email profile",
            'state': flask.session['app_state'],
            'nonce': flask.session['oidc_nonce'],
            'code_challenge': code_challenge,
            'code_challenge_method': 'S256',
            'response_type': 'code',
            'response_mode': 'query'}

        # build request_uri
        request_uri = "{base_url}?{query_params}".format(
            base_url=flask.current_app.config['OIDC_ORG_AUTH_URL'],
            query_params=requests.compat.urlencode(query_params)
        )
        response = flask.redirect(request_uri)
        return response


_MONITORING_PREFIXES = (
    '/fbrat/grafana/',
    '/fbrat/prometheus/',
    '/fbrat/cadvisor/',
    '/fbrat/rabbitmq/',
    '/fbrat/kafka-ui/',
)


def _sanitize_next_url(url):
    """Redirect to the base UI path when the next URL points at an
    internal API endpoint of a proxied monitoring service (e.g.
    Grafana's /api/user/auth-tokens/rotate)."""
    if not url:
        return flask.url_for("root")

    import urllib.parse
    parsed = urllib.parse.urlsplit(url)
    if parsed.scheme or parsed.netloc:
        return flask.url_for("root")
    if not url.startswith('/') or url.startswith('//'):
        return flask.url_for("root")

    decoded = urllib.parse.unquote(url)
    if decoded.startswith('//') or decoded.startswith('/\\'):
        return flask.url_for("root")

    for prefix in _MONITORING_PREFIXES:
        if decoded.startswith(prefix) and '/api/' in decoded:
            return prefix
    return url


@public_route
class CallbackAPI(MethodView):
    """
    Callback Resource
    """

    def get(self):
        if not flask.current_app.config['OIDC_LOGIN']:
            return "Invalid Access", 403

        headers = {'Content-Type': 'application/x-www-form-urlencoded'}
        code = flask.request.args.get("code")
        app_state = flask.request.args.get("state")

        if app_state != flask.session['app_state']:
            LOGGER.debug('user:%s login bad state', 'unknown')
            als.als_json_log('user_access',
                             {'action': 'login',
                              'user': 'unknown',
                              'from': flask.request.remote_addr,
                              'status': 'bad state'})
            return "Unexpected application state", 406
        if not code:
            LOGGER.debug('user:%s login no code', 'unknown')
            als.als_json_log('user_access',
                             {'action': 'login',
                              'user': 'unknown',
                              'from': flask.request.remote_addr,
                              'status': 'no code'})
            return "The code was not returned or is not accessible", 406

        _oidc_redirect_base = flask.current_app.config['OIDC_REDIRECT_BASE']
        redirect_uri = _oidc_redirect_base.rstrip('/') + '/fbrat/auth/callback'

        query_params = {'grant_type': 'authorization_code',
                        'code': code,
                        'redirect_uri': redirect_uri,
                        'code_verifier': flask.session['code_verifier'],
                        }
        query_params = requests.compat.urlencode(query_params)
        exchange = requests.post(
            flask.current_app.config['OIDC_ORG_TOKEN_URL'],
            headers=headers,
            data=query_params,
            auth=(flask.current_app.config['OIDC_CLIENT_ID'],
                  flask.current_app.config['OIDC_CLIENT_SECRET']),
            timeout=30,
        ).json()

        # Get tokens and validate
        if not exchange.get("token_type"):
            return "Unsupported token type. Should be 'Bearer'.", 403
        access_token = exchange["access_token"]

        # Validate the IdP-signed id_token (OIDC Core 3.1.3.7) before
        # trusting any identity claims
        id_token = exchange.get("id_token")
        if not id_token:
            return "Token response missing id_token", 403
        try:
            # Pass a browser-like User-Agent so the JWKS endpoint does not
            # reject the fetch.  PyJWKClient(headers=...) passes these to the
            # underlying urllib.request.Request call.
            jwks_client = jwt.PyJWKClient(
                flask.current_app.config['OIDC_ORG_JWKS_URL'],
                headers={"User-Agent": "python-afc/1.0"})
            signing_key = jwks_client.get_signing_key_from_jwt(id_token)
            # Only require 'nonce' when we actually sent one during the
            # authorization request (some IdPs omit it; the manual check
            # below validates the session nonce regardless).
            require_claims = ["iss", "aud", "exp", "sub"]
            if flask.session.get('oidc_nonce'):
                require_claims.append("nonce")
            id_claims = jwt.decode(
                id_token,
                signing_key.key,
                algorithms=["RS256", "RS384", "RS512",
                            "ES256", "ES384", "ES512"],
                audience=flask.current_app.config['OIDC_CLIENT_ID'],
                issuer=flask.current_app.config['OIDC_ORG_ISSUER'],
                options={"require": require_claims},
            )
        except jwt.PyJWTError as exc:
            LOGGER.warning('id_token validation failed: %s', exc)
            return "Invalid id_token", 403
        expected_nonce = flask.session.pop('oidc_nonce', None)
        if not expected_nonce or id_claims.get('nonce') != expected_nonce:
            LOGGER.warning('id_token nonce mismatch')
            return "Invalid id_token nonce", 403

        # Authorization flow successful, get userinfo and login user
        userinfo_response = requests.get(
            flask.current_app.config['OIDC_ORG_USER_INFO_URL'],
            headers={'Authorization': 'Bearer %s' % (access_token)}, timeout=30).json()

        user_sub = userinfo_response["sub"]
        if user_sub != id_claims["sub"]:
            LOGGER.warning('userinfo sub does not match id_token sub')
            return "Identity mismatch", 403
        user_email = userinfo_response["email"]
        first_name = userinfo_response["given_name"]
        last_name = userinfo_response["family_name"]
        user = User.getsub(user_sub)

        try:
            if user:
                if (user.email != user_email
                        or user.first_name != first_name
                        or user.last_name != last_name):
                    if user.email != user_email:
                        # Re-apply the first-login trust gates
                        # (email_verified + OIDC_ALLOWED_EMAIL_DOMAINS)
                        # before rewriting a linked account's email.
                        allowed_domains_raw = os.environ.get(
                            "OIDC_ALLOWED_EMAIL_DOMAINS", "")
                        allowed_domains = [
                            d.strip().lower() for d in
                            allowed_domains_raw.split(",") if d.strip()]
                        email_domain = user_email.rsplit(
                            "@", 1)[-1].lower()
                        if (userinfo_response.get("email_verified") is True
                                and (not allowed_domains
                                     or email_domain in allowed_domains)):
                            user.email = user_email
                        else:
                            LOGGER.warning(
                                'OIDC: refusing email update for sub %s to '
                                '%s — email_verified/domain-allowlist '
                                'check failed.', user_sub, user_email)
                    user.first_name = first_name
                    user.last_name = last_name
                    update_user = True
                else:
                    update_user = False
            else:
                # User logs in first time.  If matched email, reuse that entry,
                # otherwise, create new active user entry
                user = None
                if userinfo_response.get("email_verified") is True:
                    user = User.getemail(user_email)

                if user:
                    # Only bind IdP identity to an existing account if
                    # sub is still unset (fresh admin-provisioned row).
                    # An already-linked account must not have its binding overwritten.
                    if user.sub is not None and user.sub != user_sub:
                        LOGGER.warning(
                            'OIDC: email %s already linked to a different sub '
                            '— new sub rejected; creating new account instead',
                            user_email)
                        user = None
                    else:
                        # Refuse auto-link for ALL pre-provisioned accounts
                        # (sub IS NULL) regardless of role.  Extend the
                        # privileged-role guard to cover Trial and
                        # no-role rows so that only IdP-issued registrations
                        # (sub != NULL) can bind.  An environment-supplied
                        # OIDC_ALLOWED_EMAIL_DOMAINS allowlist further
                        # restricts which IdP domains may auto-register.
                        allowed_domains_raw = os.environ.get(
                            "OIDC_ALLOWED_EMAIL_DOMAINS", "")
                        allowed_domains = [
                            d.strip().lower() for d in
                            allowed_domains_raw.split(",") if d.strip()]
                        if allowed_domains:
                            email_domain = user_email.rsplit(
                                "@", 1)[-1].lower()
                            if email_domain not in allowed_domains:
                                LOGGER.warning(
                                    'OIDC: refusing auto-link/register for '
                                    'email %s — domain not in '
                                    'OIDC_ALLOWED_EMAIL_DOMAINS.',
                                    user_email)
                                user = None
                        if user and user.sub is None:
                            LOGGER.warning(
                                'OIDC: refusing to auto-link email %s to '
                                'pre-provisioned account (sub IS NULL); '
                                'explicit admin approval required.',
                                user_email)
                            user = None
                        else:
                            # update the record
                            user.sub = user_sub
                            user.username = user_email
                            user.first_name = first_name
                            user.last_name = last_name
                # Enforce OIDC_ALLOWED_EMAIL_DOMAINS before auto-registering
                # a new account — the allowlist must gate fresh-create, not
                # just auto-link to an existing row.
                allowed_domains_raw = os.environ.get(
                    "OIDC_ALLOWED_EMAIL_DOMAINS", "")
                allowed_domains = [
                    d.strip().lower() for d in
                    allowed_domains_raw.split(",") if d.strip()]
                if allowed_domains:
                    email_domain = user_email.rsplit("@", 1)[-1].lower()
                    if email_domain not in allowed_domains:
                        LOGGER.warning(
                            'OIDC: refusing auto-register for email %s — '
                            'domain not in OIDC_ALLOWED_EMAIL_DOMAINS.',
                            user_email)
                        raise werkzeug.exceptions.Unauthorized()
                if not user:
                    user = User(sub=user_sub, email=user_email,
                                username=user_email,  # fake user name
                                first_name=first_name,
                                last_name=last_name, active=True,
                                password="",
                                email_confirmed_at=datetime.datetime.now())
                    db.session.add(user)  # pylint: disable=no-member
                update_user = True
            if update_user:
                db.session.commit()  # pylint: disable=no-member

        except Exception:
            LOGGER.debug('user:%s login unauthorized', user_email)
            als.als_json_log('user_access',
                             {'action': 'login',
                              'user': user_email,
                              'from': flask.request.remote_addr,
                              'status': 'unauthorized'})
            raise werkzeug.exceptions.Unauthorized(
                'An unexpected error occured. Please try again.')

        login_user(user)

        LOGGER.debug('user:%s login success', user.username)
        als.als_json_log('user_access',
                         {'action': 'login',
                          'user': user.username,
                          'from': flask.request.remote_addr,
                          'status': 'success'})
        next_url = flask.session.pop('next', None) or flask.url_for("root")
        next_url = _sanitize_next_url(next_url)
        if not next_url.startswith('/') or next_url.startswith('//') or next_url.startswith('/\\'):
            next_url = flask.url_for("root")
        return flask.redirect(next_url)


@public_route
class LogoutAPI(MethodView):
    """
    Logout Resource
    """

    def get(self):
        # store app state and code verifier in session
        if not flask.current_app.config['OIDC_LOGIN']:
            return flask.redirect(flask.url_for('user.logout'))

        try:
            LOGGER.debug('user:%s logout', current_user.username)
            als.als_json_log('user_access',
                             {'action': 'logout',
                              'user': current_user.username,
                              'from': flask.request.remote_addr})
        except Exception:
            LOGGER.debug('user:%s logout', 'unknown')
            als.als_json_log(
                'user_access', {
                    'action': 'logout', 'user': 'unknown', 'from': flask.request.remote_addr})

        logout_user()
        return flask.redirect(flask.url_for("root"))


class UserAPI(MethodView):
    """
    User Resource
    """

    def get(self):
        if not current_user.is_authenticated:
            return flask.make_response("User not authenticated", 401)

        if not current_user.org:
            try:
                # Do NOT auto-derive org from email domain.
                # New users land with org="" until a Super/Admin explicitly assigns one.
                pass
            except Exception:
                current_user.org = ""

        if not current_user.roles:
            pass  # Do NOT auto-grant the Trial role to role-less users.
            # Roles must be assigned explicitly by an Admin/Super.

        # add organization if not exist.
        org = current_user.org if current_user.org else ""
        # Only auto-create a named organization; never create a shared
        # empty-name org that would collapse all unassigned users into one tenant.
        if org:
            organization = Organization.query.filter(
                Organization.name == org).first()
            if not organization:
                organization = Organization(org)
                db.session.add(organization)
                db.session.commit()

        data = {
            'userId': current_user.id,
            'email': current_user.email,
            'org': org,
            'roles': [r.name for r in current_user.roles],
            'email_confirmed_at': current_user.email_confirmed_at,
            'active': current_user.active,
            'firstName': current_user.first_name,
            'lastName': current_user.last_name,
        }

        if flask.current_app.config['OIDC_LOGIN']:
            data['editCredential'] = False
        else:
            data['editCredential'] = True

        responseObject = {
            'status': 'success',
            'data': data,
        }

        return flask.make_response(flask.jsonify(responseObject)), 200


@public_route
class AuthCheckAPI(MethodView):
    """Returns 200 for any authenticated user, 401 otherwise.
    Used by nginx auth_request for locations accessible to all logged-in users."""

    def get(self):
        if not current_user.is_authenticated:
            return flask.make_response("", 401)
        if not current_user.active:
            return flask.make_response("", 403)
        return flask.make_response("", 200)


@public_route
class AdminCheckAPI(MethodView):
    """Auth check that returns 200 for Admin/Super users, 403 otherwise.
    Used by Nginx auth_request for role-gated locations."""

    def get(self):
        if not current_user.is_authenticated:
            return flask.make_response("", 401)
        if not current_user.active:
            return flask.make_response("", 403)
        role_names = [r.name for r in current_user.roles]
        if "Super" in role_names or "Admin" in role_names:
            return flask.make_response("", 200)
        return flask.make_response("Forbidden", 403)


@public_route
class SuperCheckAPI(MethodView):
    """Auth check that returns 200 only for Super users, 403 otherwise.
    Used by Nginx auth_request for monitoring locations that may expose
    cross-tenant data (Grafana, Prometheus, cAdvisor, RabbitMQ, Kafka UI,
    Alloy)."""

    def get(self):
        if not current_user.is_authenticated:
            return flask.make_response("", 401)
        if not current_user.active:
            return flask.make_response("", 403)
        role_names = [r.name for r in current_user.roles]
        if "Super" in role_names:
            return flask.make_response("", 200)
        return flask.make_response("Forbidden", 403)


# define the API resources
user_view = UserAPI.as_view('UserAPI')
logout_view = LogoutAPI.as_view('LogoutAPI')
login_view = LoginAPI.as_view('LoginAPI')
about_login_view = AboutLoginAPI.as_view('AboutLoginAPI')
callback_view = CallbackAPI.as_view('CallbackAPI')
admin_check_view = AdminCheckAPI.as_view('AdminCheckAPI')
super_check_view = SuperCheckAPI.as_view('SuperCheckAPI')
auth_check_view = AuthCheckAPI.as_view('AuthCheckAPI')

# add Rules for API Endpoints
module.add_url_rule(
    '/status',
    view_func=user_view,
    methods=['GET']
)
module.add_url_rule(
    '/check',
    view_func=auth_check_view,
    methods=['GET']
)
module.add_url_rule(
    '/admin_check',
    view_func=admin_check_view,
    methods=['GET']
)
module.add_url_rule(
    '/super_check',
    view_func=super_check_view,
    methods=['GET']
)
module.add_url_rule(
    '/logout',
    view_func=logout_view,
    methods=['GET']
)
module.add_url_rule(
    '/login',
    view_func=login_view,
    methods=['GET']
)
module.add_url_rule(
    '/about_login',
    view_func=about_login_view,
    methods=['GET']
)
module.add_url_rule(
    '/callback',
    view_func=callback_view,
    methods=['GET']
)
