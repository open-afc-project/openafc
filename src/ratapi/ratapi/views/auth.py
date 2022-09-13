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
from .. import config
from ..models.aaa import User

OIDC_LOGIN = config.OIDC_LOGIN
try:
    from .. import priv_config
    OIDC_LOGIN = priv_config.OIDC_LOGIN
except:
    pass

if OIDC_LOGIN:
    from flask_login import (
        current_user,
        login_user,
        logout_user,
    )
else:
    from flask_user import current_user

LOGGER = logging.getLogger(__name__)

# using https://github.com/realpython/flask-jwt-auth
# LICENCE: MIT

module = flask.Blueprint('auth', __name__)

PY3 = False  # using Python2


def auth(ignore_active=False, roles=None, is_user=None):
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
    (user_id, active, user_roles) = \
        (current_user.id, current_user.active,
         [r.name for r in current_user.roles])

    if not isinstance(user_id, str):
        if not active and not ignore_active:
            raise werkzeug.exceptions.Forbidden("Inactive user")
        if is_user:
            if user_id == is_user:
                LOGGER.debug("User id matches: %i", user_id)
                return user_id
            elif "Admin" in user_roles:
                LOGGER.debug(
                    "Admin overrides user_id match. Impersonating: %i", is_user)
                return is_user  # return impersonating user
            else:
                # give 404 to protect privacy
                raise werkzeug.exceptions.NotFound()
        # ignore roles, only need to be a registered user
        if roles is None:
            LOGGER.debug("User %i is authenticated", user_id)
            return user_id

        for r in roles:
            if r in user_roles:
                LOGGER.debug(
                    "User %i is authenticated with role %s", user_id, r)
                return user_id
        raise werkzeug.exceptions.Forbidden(
            "You do not have access to this resource")
    else:
        raise werkzeug.exceptions.Unauthorized()

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
            return "Unexpected application state", 406
        if not code:
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
            raise werkzeug.exceptions.Unauthorized(
                'An unexpected error occured. Please try again.')

        login_user(user)
        return flask.redirect(flask.url_for("root"))


class LogoutAPI(MethodView):
    """
    Logout Resource
    """

    def get(self):
        # store app state and code verifier in session
        if not flask.current_app.config['OIDC_LOGIN']:
            return flask.redirect(flask.url_for('user.logout'))

        logout_user()
        return flask.redirect(flask.url_for("root"))


class UserAPI(MethodView):
    """
    User Resource
    """

    def get(self):
        if not current_user.is_authenticated:
            return flask.make_response("User not authenticated", 401)

        data = {
            'userId': current_user.id,
            'email': current_user.email,
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
    '/callback',
    view_func=callback_view,
    methods=['GET']
)
