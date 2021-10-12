import logging
import flask
from flask.views import MethodView
import werkzeug
#from passlib.handlers.bcrypt import bcrypt
import bcrypt
import hashlib
from ..models.aaa import User, BlacklistToken
from ..models.base import db

LOGGER = logging.getLogger(__name__)

# using https://github.com/realpython/flask-jwt-auth
# LICENCE: MIT

module = flask.Blueprint('auth', __name__)

PY3 = False  # using Python2


def _unicode_to_bytes(unicode_string):
    ''' Converts a unicode string to a bytes object.

        :param unicode_string: The unicode string to convert.
    '''
    if PY3:
        if isinstance(unicode_string, str):
            bytes_object = bytes(unicode_string, 'utf-8')
        else:
            bytes_object = unicode_string
    else:
        if isinstance(unicode_string, unicode):
            bytes_object = unicode_string.encode('utf-8')
        else:
            bytes_object = unicode_string
    return bytes_object


def _check_password_hash(pw_hash, password):
    ''' Tests a password hash against a candidate password. The candidate 
        password is first hashed and then subsequently compared in constant 
        time to the existing hash. 
        Example usage of :class:`check_password_hash` would look something 
        like this::
            pw_hash = bcrypt.generate_password_hash('secret', 10)
            bcrypt.check_password_hash(pw_hash, 'secret') # returns True

        :param pw_hash: The hash to be compared against.

        :param password: The password to compare.

        :rtype: boolean
    '''

    # Python 3 unicode strings must be encoded as bytes before hashing.
    pw_hash = _unicode_to_bytes(pw_hash)
    password = _unicode_to_bytes(password)

    return werkzeug.security.safe_str_cmp(bcrypt.hashpw(password, pw_hash), pw_hash)


def _generate_password_hash(self, password, rounds=12, prefix='2b'):
    ''' Generates a password hash using bcrypt. Specifying `rounds`
        sets the log_rounds parameter of `bcrypt.gensalt()` which determines
        the complexity of the salt. 12 is the default value. Specifying `prefix`
        sets the `prefix` parameter of `bcrypt.gensalt()` which determines the 
        version of the algorithm used to create the hash.
        Example usage of :class:`generate_password_hash` might look something 
        like this::
            pw_hash = bcrypt.generate_password_hash('secret', 10)

        :param password: The password to be hashed.

        :param rounds: The optional number of rounds.

        :param prefix: The algorithm version to use.

        :rtype: password hash
    '''

    if not password:
        raise ValueError('Password must be non-empty.')

    # Python 3 unicode strings must be encoded as bytes before hashing.
    password = _unicode_to_bytes(password)
    prefix = _unicode_to_bytes(prefix)

    salt = bcrypt.gensalt(rounds=rounds, prefix=prefix)
    return bcrypt.hashpw(password, salt)


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
    request = flask.request
    secret_key = flask.current_app.config['SECRET_KEY']
    auth_token = request.headers.get('Authorization')

    if auth_token:
        (user_id, active, user_roles) = User.decode_auth_token(
            auth_token, secret_key)

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
            raise werkzeug.exceptions.Unauthorized(user_id)

    else:
        # user has not provided a token.
        raise werkzeug.exceptions.Unauthorized("No token provided")


class RegisterAPI(MethodView):
    """
    User Registration Resource
    """

    def post(self):
        # get the post data
        post_data = flask.request.get_json()
        LOGGER.warn("post data: %s", post_data)
        # check if user already exists
        user = User.query.filter_by(email=post_data.get('email')).first()
        if not user:
            try:
                user = User(
                    email=post_data.get('email'),
                    password=flask.current_app.user_manager.password_manager.hash_password(
                        password),  # pylint: disable=undefined-variable
                )
                # insert the user
                db.session.add(user)  # pylint: disable=no-member
                db.session.commit()  # pylint: disable=no-member
                # generate the auth token
                auth_token = user.encode_auth_token(
                    user.id, False, [], flask.current_app.config['SECRET_KEY'])
                responseObject = {
                    'status': 'success',
                    'message': 'Successfully registered.',
                    'auth_token': auth_token.decode()
                }
                return flask.make_response(flask.jsonify(responseObject)), 201
            except Exception as e:
                raise werkzeug.exceptions.Unauthorized(
                    'An unexpected error occured. Please try again.')
        else:
            raise werkzeug.exceptions.Conflict(
                'User already exists. Please log in')
            responseObject = {
                'status': 'fail',
                'message': 'User already exists. Please Log in.',
            }
            return flask.make_response(flask.jsonify(responseObject)), 202


class LoginAPI(MethodView):
    """
    User Login Resource
    """

    def post(self):
        # get the post data
        LOGGER.debug('logging in')
        post_data = flask.request.get_json()

        # fetch the user data
        LOGGER.debug("Getting user")
        user = User.query.filter_by(
            email=post_data.get('email')
        ).first()
        LOGGER.debug("Got user: %s", user)
        if user and _check_password_hash(
            user.password, post_data.get('password')
        ):
            LOGGER.debug("Password good")
            roles = [r.name for r in user.roles]
            auth_token = user.encode_auth_token(
                user.id, user.active, roles, flask.current_app.config['SECRET_KEY'])
            LOGGER.debug("token generated")
            if auth_token:
                responseObject = {
                    'status': 'success',
                    'message': 'Successfully logged in.',
                    'token': auth_token.decode()
                }
                return flask.make_response(flask.jsonify(responseObject)), 200
        else:
            raise werkzeug.exceptions.NotFound('Invalid email and/or password')


class UserAPI(MethodView):
    """
    User Resource
    """

    def get(self):
        # get the auth token
        auth_token = flask.request.headers.get('Authorization')
        if auth_token:
            (resp, active, roles) = User.decode_auth_token(
                auth_token, flask.current_app.config['SECRET_KEY'])
            if not isinstance(resp, str):
                user = User.query.filter_by(id=resp).first()
                responseObject = {
                    'status': 'success',
                    'data': {
                        'userId': user.id,
                        'email': user.email,
                        'roles': roles,
                        'email_confirmed_at': user.email_confirmed_at,
                        'active': active,
                        'firstName': user.first_name,
                        'lastName': user.last_name,
                    }
                }
                return flask.make_response(flask.jsonify(responseObject)), 200

            raise werkzeug.exceptions.Unauthorized(resp)
        else:
            raise werkzeug.exceptions.Unauthorized(
                'Provide a valid auth token.')


class LogoutAPI(MethodView):
    """
    Logout Resource
    """

    def post(self):
        # get auth token
        auth_token = flask.request.headers.get('Authorization')
        if auth_token:
            (resp, _, _) = User.decode_auth_token(
                auth_token, flask.current_app.config['SECRET_KEY'])
            if not isinstance(resp, str):
                # mark the token as blacklisted
                blacklist_token = BlacklistToken(token=auth_token)
                try:
                    # insert the token
                    db.session.add(  # pylint: disable=no-member
                        blacklist_token)
                    db.session.commit()  # pylint: disable=no-member
                    responseObject = {
                        'status': 'success',
                        'message': 'Successfully logged out.'
                    }
                    return flask.make_response(flask.jsonify(responseObject)), 200
                except Exception as e:
                    raise werkzeug.exceptions.InternalServerError(e)
            else:
                raise werkzeug.exceptions.Unauthorized(resp)
        else:
            raise werkzeug.exceptions.Unauthorized(
                'Provide a valid auth token.')


# define the API resources
registration_view = RegisterAPI.as_view('RegisterAPI')
login_view = LoginAPI.as_view('LoginAPI')
user_view = UserAPI.as_view('UserAPI')
logout_view = LogoutAPI.as_view('LogoutAPI')

# add Rules for API Endpoints
module.add_url_rule(
    '/register',
    view_func=registration_view,
    methods=['POST']
)
module.add_url_rule(
    '/login',
    view_func=login_view,
    methods=['POST']
)
module.add_url_rule(
    '/status',
    view_func=user_view,
    methods=['GET']
)
module.add_url_rule(
    '/logout',
    view_func=logout_view,
    methods=['POST']
)
