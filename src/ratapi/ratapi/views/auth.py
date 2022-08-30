import logging
import flask
from flask.views import MethodView
import werkzeug
import hashlib
from ..models.base import db
from flask_user import current_user
from flask import session

LOGGER = logging.getLogger(__name__)

# using https://github.com/realpython/flask-jwt-auth
# LICENCE: MIT

module = flask.Blueprint('auth', __name__)

PY3 = False  # using Python2


def auth(ignore_active=False, roles=None, is_user=None):
    ''' Provide application authentication for API methods. Returns the user_id if successful.

        :param ignore_active: if True, allows inactive users to access resource

        :param roles: array of strings which specify the roles that can access the resource.
        User must have at least 1 of the roles
        :type roles: str[]

        :param is_user: is a single user_id. checks if the user is identical. Use for personal information.

        :returns: user Id
        :rtype: int
    '''
    if not current_user.is_authenticated:
        raise werkzeug.exceptions.Unauthorized()
    (user_id, active, user_roles) = (current_user.id, current_user.active,
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


class UserAPI(MethodView):
    """
    User Resource
    """

    def get(self):
        if not current_user.is_authenticated:
            return flask.make_response("User not authenticated", 401)

        data= {
           'userId': current_user.id,
           'email': current_user.email,
           'roles': [ r.name for r in current_user.roles ],
           'active': current_user.active,
           'firstName': current_user.first_name,
           'lastName': current_user.last_name,
        }

        data['email_confirmed_at'] = current_user.email_confirmed_at

        responseObject = {
            'status': 'success',
            'data': data,
        }

        return flask.make_response(flask.jsonify(responseObject)), 200


# define the API resources
user_view = UserAPI.as_view('UserAPI')

# add Rules for API Endpoints
module.add_url_rule(
    '/status',
    view_func=user_view,
    methods=['GET']
)
