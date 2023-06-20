#
# This Python file uses the following encoding: utf-8
#
# Portions copyright (C) 2021 Broadcom.
# All rights reserved. The term “Broadcom” refers solely
# to the Broadcom Inc. corporate affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which
# is included with this software program.
#
# pylint: disable=no-member
''' Authentication, authorization, and accounting classes.
'''
from appcfg import OIDCConfigurator
import os
from sqlalchemy.schema import Sequence
from sqlalchemy.dialects.postgresql import JSON

OIDC_LOGIN=OIDCConfigurator().OIDC_LOGIN

if OIDC_LOGIN:
    from flask_login import UserMixin
else:
    from flask_user import UserMixin

import jwt
from .base import db, UserDbInfo
import datetime
import time

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
    org = db.Column(db.String(255), nullable=True)
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
    roles = db.relationship('Role', secondary='aaa_user_role', back_populates='users')

    @staticmethod
    def get(user_id):
        return User.query.filter_by(id=user_id).first()

    @staticmethod
    def getsub(user_sub):
        return User.query.filter_by(sub=user_sub).first()

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
    users = db.relationship('User', secondary='aaa_user_role', back_populates='roles')


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


class CertId(db.Model):
    ''' entry to designate allowed AP's for the PAWS interface '''

    __tablename__ = 'cert_id'
    UNKNOWN = 0
    OUTDOOR = 2
    INDOOR = 1
    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    certification_id = db.Column(db.String(64), nullable=False)
    refreshed_at = db.Column(db.DateTime())
    ruleset_id = db.Column(db.Integer, db.ForeignKey(
        'aaa_ruleset.id', ondelete='CASCADE'), nullable=False)
    location = db.Column(db.Integer, nullable=False)

    def __init__(self, certification_id, location):
        self.certification_id = certification_id
        self.location = location
        self.refreshed_at = datetime.datetime.now()


class AccessPointDeny(db.Model):                                                     
    ''' entry to designate allowed AP's for the PAWS interface '''               
                                                                                 
    __tablename__ = 'access_point_deny'                                               

    id = db.Column(db.Integer, primary_key=True, autoincrement=True)             
    serial_number = db.Column(db.String(64), nullable=True, index=True)         
    certification_id = db.Column(db.String(64))                                  
    org_id = db.Column(db.Integer, db.ForeignKey(
        'aaa_org.id', ondelete='CASCADE'))
    ruleset_id = db.Column(db.Integer, db.ForeignKey(
        'aaa_ruleset.id', ondelete='CASCADE'))

    def __init__(self, serial_number=None, certification_id=None):
        self.serial_number = serial_number
        self.certification_id = certification_id



class MTLS(db.Model):
    ''' entry to designate allowed MTLS's for the PAWS interface '''

    __tablename__ = 'MTLS'

    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    # 32KB limit of certificate data
    cert = db.Column(db.String(32768), nullable=False, unique=False)
    note = db.Column(db.String(128), nullable=True, unique=False)
    org = db.Column(db.String(64), nullable=False)
    created = db.Column(db.DateTime(), nullable=False)

    def __init__(self, cert, note, org):
        self.cert = cert
        self.note = note
        self.org = org
        self.created = datetime.datetime.fromtimestamp(time.time())


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

class AFCConfig(db.Model):
    ''' entry for AFC Config '''

    __tablename__ = 'AFCConfig'

    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    # 4KB limit of afc config json string
    config = db.Column(JSON)
    created = db.Column(db.DateTime(), nullable=False)

    def __init__(self, config):
        self.config = config
        self.created = datetime.datetime.fromtimestamp(time.time())

class Organization(db.Model):
    ''' entry for Organization '''

    __tablename__ = 'aaa_org'
    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    name = db.Column(db.String(50), nullable=False, unique=True)
    aps = db.relationship("AccessPointDeny", backref="org")

    def __init__(self, name):
        self.name = name

class Ruleset(db.Model):
    ''' entry for Organization '''

    __tablename__ = 'aaa_ruleset'
    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    name = db.Column(db.String(50), nullable=False, unique=True)
    aps = db.relationship("AccessPointDeny", backref="ruleset")
    cert_ids = db.relationship("CertId", backref="ruleset")

    def __init__(self, name):
        self.name = name


# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
