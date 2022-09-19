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

from .. import config
OIDC_LOGIN = config.OIDC_LOGIN

try:
    # priv_config overrides defaults
    from .. import priv_config
    OIDC_LOGIN = priv_config.OIDC_LOGIN
except:
    pass

if OIDC_LOGIN:
    from flask_login import UserMixin
else:
    from flask_user import UserMixin

import jwt
from .base import db, UserDbInfo
import datetime


class User(db.Model, UserMixin):
    ''' Each user account in the system. '''
    __tablename__ = 'aaa_user'
    __table_args__ = (
        db.UniqueConstraint('email'),
    )
    id = db.Column(db.Integer, primary_key=True)

    # UserDbInfo.VER indicates the version of the user database.
    # This can be overriden with env variables in case the server
    # is boot up with an older database before it's migrated.
    try:
        import os
        DBVER = int(os.getenv('RAT_DBVER'))
        UserDbInfo.VER = DBVER
    except:
        pass

    if UserDbInfo.VER >= 1:
        username = db.Column(db.String(50), nullable=False, unique=True)
        sub = db.Column(db.String(255))

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

    @staticmethod
    def get(user_id):
        return User.query.filter_by(id=user_id).first()

    @staticmethod
    def getsub(user_sub):
        if UserDbInfo.VER >= 1:
            return User.query.filter_by(sub=user_sub).first()
        else:
            return None

    @staticmethod
    def getemail(user_email):
        return User.query.filter_by(email=user_email).first()

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
