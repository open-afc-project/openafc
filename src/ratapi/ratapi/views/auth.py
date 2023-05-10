# This Python file uses the following encoding: utf-8
#
# Portions copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate
# affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy
# of which is included with this software program.
#
from appcfg import OIDCConfigurator
import os
import logging
import datetime
import hashlib
import base64
import secrets
import werkzeug
import flask
from flask.views import MethodView
import requests
from ..models.base import db
from ..models.aaa import User, Organization
from flask_login import current_user
from .. import als

OIDC_LOGIN=OIDCConfigurator().OIDC_LOGIN

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


def auth(ignore_active=False, roles=None, is_user=None, org=None):
    ''' Provide application authentication for API methods. Returns the user_id if successful.

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
    if "Super" in user_roles and not "Admin" in user_roles:
        user_roles.append("Admin")

    if not isinstance(user_id, str):
        if not active and not ignore_active:
            raise werkzeug.exceptions.Forbidden("Inactive user")
        if is_user:
            if user_id == is_user:
                LOGGER.debug("User id matches: %i", user_id)
            elif "Super" in user_roles:
                return is_user  # return impersonating user. Don't check org
            elif not "Admin" in user_roles:
                raise werkzeug.exceptions.NotFound()
            target_user = User.get(is_user)
            target_org = target_user.org if target_user.org else ""

        else:
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

            is_user = user_id
            target_org = cur_org if cur_org else ""
    else:
        raise werkzeug.exceptions.Unauthorized()

    # done checking roles/userid. now check org
    if not "Super" in user_roles:
        if (not cur_org == target_org) or (org and not cur_org == org):
            raise werkzeug.exceptions.Forbidden(
                "You do not have access to this resource")

    return is_user


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


class LoginAPI(MethodView):
    """
    User Login Resource
    """

    def get(self):
        # store app state and code verifier in session
        if not flask.current_app.config['OIDC_LOGIN']:
            return flask.redirect(flask.url_for('user.login'))

        flask.session['app_state'] = secrets.token_urlsafe(64)
        flask.session['code_verifier'] = secrets.token_urlsafe(64)
        # calculate code challenge
        hashed = hashlib.sha256(flask.session['code_verifier'].encode('ascii')).digest()
        encoded = base64.urlsafe_b64encode(hashed)
        code_challenge = encoded.decode('ascii').strip('=')
        redirect_uri = flask.request.base_url
        redirect_uri = redirect_uri.replace("login", "callback")

        # get request params
        query_params = {'client_id': flask.current_app.config['OIDC_CLIENT_ID'],
                        'redirect_uri': redirect_uri,
                        'scope': "openid email profile",
                        'state': flask.session['app_state'],
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
            als.als_json_log('user_access', {'action':'login', 'user':'unknown', 'from':flask.request.remote_addr, 'status':'bad state'})
            return "Unexpected application state", 406
        if not code:
            LOGGER.debug('user:%s login no code', 'unknown')
            als.als_json_log('user_access', {'action':'login', 'user':'unknown', 'from':flask.request.remote_addr, 'status':'no code'})
            return "The code was not returned or is not accessible", 406

        query_params = {'grant_type': 'authorization_code',
                        'code': code,
                        'redirect_uri': flask.request.base_url,
                        'code_verifier': flask.session['code_verifier'],
                       }
        query_params = requests.compat.urlencode(query_params)
        exchange = requests.post(
            flask.current_app.config['OIDC_ORG_TOKEN_URL'],
            headers=headers,
            data=query_params,
            auth=(flask.current_app.config['OIDC_CLIENT_ID'],
                  flask.current_app.config['OIDC_CLIENT_SECRET']),
        ).json()

        # Get tokens and validate
        if not exchange.get("token_type"):
            return "Unsupported token type. Should be 'Bearer'.", 403
        access_token = exchange["access_token"]

        # Authorization flow successful, get userinfo and login user
        userinfo_response = requests.get( \
            flask.current_app.config['OIDC_ORG_USER_INFO_URL'], \
            headers={'Authorization': 'Bearer %s' %(access_token)}).json()

        user_sub = userinfo_response["sub"]
        user_email = userinfo_response["email"]
        first_name = userinfo_response["given_name"]
        last_name = userinfo_response["family_name"]
        user = User.getsub(user_sub)

        try:
            if user:
                if (user.email != user_email
                        or user.first_name != first_name
                        or user.last_name != last_name):
                    user.email = user_email
                    user.first_name = first_name
                    user.last_name = last_name
                    update_user = True
                else:
                    update_user = False
            else:
                # User logs in first time.  If matched email, reuse that entry,
                # otherwise, create new active user entry
                user = User.getemail(user_email)
                if user:
                    # update the record
                    user.sub = user_sub
                    user.username = user_email
                    user.first_name = first_name
                    user.last_name = last_name
                    user.active = True
                    user.email_confirmed_at = datetime.datetime.now()
                else:
                    user = User(sub=user_sub, email=user_email,
                                username=user_email, # fake user name
                                first_name=first_name,
                                last_name=last_name, active=True,
                                password="",
                                email_confirmed_at=datetime.datetime.now())
                    db.session.add(user)  # pylint: disable=no-member
                update_user = True
            if update_user:
                db.session.commit()  # pylint: disable=no-member

        except Exception as e:
            LOGGER.debug('user:%s login unauthorized', user_email)
            als.als_json_log('user_access', {'action':'login', 'user':user_email, 'from':flask.request.remote_addr, 'status':'unauthorized'})
            raise werkzeug.exceptions.Unauthorized(
                'An unexpected error occured. Please try again.')

        login_user(user)

        LOGGER.debug('user:%s login success', user.username)
        als.als_json_log('user_access', {'action':'login', 'user':user.username, 'from':flask.request.remote_addr, 'status':'success'})
        return flask.redirect(flask.url_for("root"))


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
           als.als_json_log('user_access', {'action':'logout', 'user':current_user.username, 'from':flask.request.remote_addr})
        except:
           LOGGER.debug('user:%s logout', 'unknown')
           als.als_json_log('user_access', {'action':'logout', 'user':'unknown', 'from':flask.request.remote_addr})

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
                current_user.org = current_user.email[current_user.email.index('@') + 1:]
                user = User.query.filter(User.id == current_user.id).first()
                user.org = current_user.org
                db.session.commit()
            except:
                current_user.org = ""

        # add organization if not exist.
        org = current_user.org if current_user.org else ""
        organization = Organization.query.filter(Organization.name == org).first()
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


# define the API resources
user_view = UserAPI.as_view('UserAPI')
logout_view = LogoutAPI.as_view('LogoutAPI')
login_view = LoginAPI.as_view('LoginAPI')
about_login_view = AboutLoginAPI.as_view('AboutLoginAPI')
callback_view = CallbackAPI.as_view('CallbackAPI')

# add Rules for API Endpoints
module.add_url_rule(
    '/status',
    view_func=user_view,
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
