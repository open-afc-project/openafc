#
# This Python file uses the following encoding: utf-8
#
# Portions copyright © 2021 Broadcom.
# All rights reserved. The term “Broadcom” refers solely
# to the Broadcom Inc. corporate affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which
# is included with this software program.
#
# pylint: disable=no-member
''' Authentication, authorization, and accounting classes.
'''

from flask_user import UserMixin
import jwt
from .base import db
import datetime


class User(db.Model, UserMixin):
    ''' Each user account in the system. '''
    __tablename__ = 'aaa_user'
    __table_args__ = (
        db.UniqueConstraint('email'),
    )
    id = db.Column(db.Integer, primary_key=True)

    # Authentication fields (email is account name)
    email = db.Column(db.String(255), nullable=False)
    email_confirmed_at = db.Column(db.DateTime())
    password = db.Column(db.String(255), nullable=False)
    active = db.Column(db.Boolean())

    # Application data fields
    first_name = db.Column(db.String(50))
    last_name = db.Column(db.String(50))

    # Relationships
    roles = db.relationship('Role', secondary='aaa_user_role')
    access_points = db.relationship('AccessPoint')

    def encode_auth_token(self, user_id, active, roles, secret_key):
        """
        Generates the Auth Token
        :return: string
        """
        try:
            payload = {
                'exp': datetime.datetime.utcnow() + datetime.timedelta(days=30, seconds=0),
                'iat': datetime.datetime.utcnow(),
                'sub': user_id,
                'active': active,
                'roles': roles,
            }
            return jwt.encode(
                payload,
                secret_key,
                algorithm='HS256'
            )
        except Exception as err:
            return err

    @staticmethod
    def decode_auth_token(auth_token, secret_key):
        """
        Validates the auth token
        :param auth_token:
        :return: integer|string
        """
        try:
            payload = jwt.decode(auth_token, secret_key)
            is_blacklisted_token = BlacklistToken.check_blacklist(auth_token)
            if is_blacklisted_token:
                return 'Token blacklisted. Please log in again.', None, None
            else:
                return payload['sub'], payload['active'], payload['roles']
        except jwt.ExpiredSignatureError:
            return 'Signature expired. Please log in again.', None, None
        except jwt.InvalidTokenError:
            return 'Invalid token. Please log in again.', None, None


class Role(db.Model):
    ''' A role is used for authorization. '''
    __tablename__ = 'aaa_role'
    __table_args__ = (
        db.UniqueConstraint('name'),
    )
    id = db.Column(db.Integer(), primary_key=True)

    #: Role definition
    name = db.Column(db.String(50))

    # Relationships
    users = db.relationship('User', secondary='aaa_user_role')


class UserRole(db.Model):
    ''' Map users to roles. '''
    __tablename__ = 'aaa_user_role'
    __table_args__ = (
        db.UniqueConstraint('user_id', 'role_id'),
    )
    id = db.Column(db.Integer(), primary_key=True)

    user_id = db.Column(db.Integer(), db.ForeignKey(
        'aaa_user.id', ondelete='CASCADE'))
    role_id = db.Column(db.Integer(), db.ForeignKey(
        'aaa_role.id', ondelete='CASCADE'))


class BlacklistToken(db.Model):
    """
    Token Model for storing JWT tokens
    """
    __tablename__ = 'blacklist_tokens'

    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    token = db.Column(db.String(500), unique=True, nullable=False)
    blacklisted_on = db.Column(db.DateTime, nullable=False)

    def __init__(self, token):
        self.token = token
        self.blacklisted_on = datetime.datetime.now()

    def __repr__(self):
        return '<id: token: {}'.format(self.token)

    @staticmethod
    def check_blacklist(auth_token):
        # check whether auth token has been blacklisted
        res = BlacklistToken.query.filter_by(token=str(auth_token)).first()
        if res:
            return True
        else:
            return False


class AccessPoint(db.Model):
    ''' entry to designate allowed AP's for the PAWS interface '''

    __tablename__ = 'access_point'
    __table_args__ = (
        db.UniqueConstraint('serial_number', 'serial_number'),
    )

    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    serial_number = db.Column(db.String(64), nullable=False, index=True)
    model = db.Column(db.String(64))
    manufacturer = db.Column(db.String(64))
    certification_id = db.Column(db.String(64))

    # owner user
    user_id = db.Column(db.Integer(), db.ForeignKey(
        'aaa_user.id', ondelete='CASCADE'))

    def __init__(self, serial_number, model, manufacturer, certification_id, user_id=None):
        if not serial_number:
            raise RuntimeError("Serial number cannot be empty")
        self.serial_number = serial_number
        self.model = model
        self.manufacturer = manufacturer
        self.certification_id = certification_id
        if user_id:
            self.user_id = user_id

class Limit(db.Model):
    ''' entry for limits '''

    __tablename__ = 'limits'
    __table_args__ = (
        db.UniqueConstraint('min_eirp'),
    )
    #only one set of limits currently 
    id = db.Column(db.Integer(), primary_key=True)
    # Application data fields
    min_eirp = db.Column(db.Numeric(50))

    enforce = db.Column(db.Boolean())

    def __init__(self, min_eirp):
        self.id = 0
        self.min_eirp = min_eirp 
        self.enforce = True

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
