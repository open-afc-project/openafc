#
# This Python file uses the following encoding: utf-8
#
# Portions copyright (C) 2021 Broadcom.
# All rights reserved. The term "Broadcom" refers solely
# to the Broadcom Inc. corporate affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which
# is included with this software program.
#
''' External management of this application.
'''

import json
import logging
import os
import ratapi
import shutil
import sqlalchemy
import time
import subprocess
import sys
import click
from flask.cli import FlaskGroup, with_appcontext
from flask import current_app
from flask_migrate import Migrate
import werkzeug.exceptions
from . import create_app
from afcmodels.base import db
from afcmodels.hardcoded_relations import RulesetVsRegion, \
    CERT_ID_LOCATION_UNKNOWN, CERT_ID_LOCATION_OUTDOOR, CERT_ID_LOCATION_INDOOR
from .db.generators import shp_to_spatialite, spatialite_to_raster
from prettytable import PrettyTable
from . import cmd_utils
import als
from .util import als_log_afc_config_change

LOGGER = logging.getLogger(__name__)


def json_lookup(key, json_obj, val):
    """Loookup for key in json and change it value if required"""
    keepit = []

    def lookup(key, json_obj, val, keepit):
        '''lookup for key in json
        '''
        if isinstance(json_obj, dict):
            for k, v in json_obj.items():
                # LOGGER.debug('%s ... %s', k, type(v))
                if k == key:
                    keepit.append(v)
                    if val:
                        json_obj[k] = val
                elif isinstance(v, (dict, list)):
                    lookup(key, v, val, keepit)
        elif isinstance(json_obj, list):
            for node in json_obj:
                lookup(key, node, val, keepit)
        return keepit

    found = lookup(key, json_obj, val, keepit)
    return found


def get_or_create(session, model, **kwargs):
    ''' Ensure a specific object exists in the DB.
    '''
    instance = session.query(model).filter_by(**kwargs).first()
    if instance:
        return instance
    else:
        instance = model(**kwargs)
        session.add(instance)
        session.commit()
        return instance


# Create data group
@click.group(name='data')
def data_cli():
    """View and manage data files in RAT"""
    pass


@data_cli.command('clean-history')
@with_appcontext
def clean_history():
    """Remove old history files"""
    history_dir = current_app.config['HISTORY_DIR']
    if history_dir is None:
        return

    LOGGER.debug('Removing history files from "%s"...', history_dir)

    now = time.time()
    # delete file if older than 14 days
    cutoff = now - (14 * 86400)

    logs = os.listdir(history_dir)
    for record in logs:
        t = os.stat(os.path.join(history_dir, record))
        c = t.st_ctime

        if c < cutoff:
            shutil.rmtree(os.path.join(history_dir, record))


@data_cli.command('clean-tmp-files')
@with_appcontext
def clean_tmp_files():
    """Remove old temporary files that have been orphaned by web clients"""
    task_queue = current_app.config['TASK_QUEUE']

    LOGGER.debug('Removing temp files from "%s"...', task_queue)

    now = time.time()
    # delete file if older than 14 days
    cutoff = now - (14 * 86400)

    logs = os.listdir(task_queue)
    for record in logs:
        t = os.stat(os.path.join(task_queue, record))
        c = t.st_ctime

        if c < cutoff:
            shutil.rmtree(os.path.join(task_queue, record))


@data_cli.command('rasterize-buildings')
@click.option('--source', '-s', type=str, required=True, help="source shape file")
@click.option('--target', '-t', type=str, default=None, help="target raster file")
@click.option('--layer', '-l', type=str, required=True, help="layer name to access polygons from")
@click.option('--attribute', '-a', type=str, required=True, help="attribute name to access height data from")
@with_appcontext
def rasterize_buildings(source, target, layer, attribute):
    """Convert building shape file into tiff raster"""
    if target is None:
        target = os.path.splitext(source)[0]

    if not os.path.exists(source):
        raise RuntimeError(
            '"{}" source file does not exist'.format(source))

    shp_to_spatialite(target + '.sqlite3', source)

    spatialite_to_raster(target + '.tiff', target +
                         '.sqlite3', layer, attribute)


# 'db-create' command. Click binding is in main()
def db_create(if_absent):
    """Create a full new database outside of alembic migrations."""
    LOGGER.debug('db_create()')
    from afcmodels.aaa import Role, Ruleset
    from flask_migrate import stamp
    import db_creator

    if if_absent:
        try:
            with db.engine.connect():
                LOGGER.info("Database already exists")
                return
        except sqlalchemy.exc.SQLAlchemyError:
            pass
    ruleset_list = RulesetVsRegion.ruleset_list()
    db_creator.ensure_dsn(
        dsn=current_app.config.get('SQLALCHEMY_DATABASE_URI'),
        password=current_app.config.get('SQLALCHEMY_DATABASE_PASSWORD'),
        local=True)
    db.create_all()
    get_or_create(db.session, Role, name='Admin')
    get_or_create(db.session, Role, name='Super')
    get_or_create(db.session, Role, name='Analysis')
    get_or_create(db.session, Role, name='AP')
    get_or_create(db.session, Role, name='Trial')
    for rule in ruleset_list:
        get_or_create(db.session, Ruleset, name=rule)
    stamp(directory=os.path.join(os.path.dirname(__file__), "migrations"),
          revision='head')


# 'db-drop' command. Click binding is in main()
def db_drop():
    """Drop the database."""
    LOGGER.debug('db_drop()')
    from afcmodels.aaa import User, Role
    db.drop_all()


# 'db-export' command. Click binding is in main()
def db_export(dst):
    """Export database in db to a file in json."""
    LOGGER.debug('db_export()')
    from afcmodels.aaa import User, UserRole, Role, Limit

    filename = dst

    limit = None
    user_info = {}
    user_cfg = {}
    for user, _, role in db.session.query(
            User, UserRole, Role).filter(
            User.id == UserRole.user_id).filter(
            UserRole.role_id == Role.id).all():  # pylint: disable=no-member
        key = user.email + ":" + str(user.id)
        if key in user_cfg:
            user_cfg[key]['rolename'].append(role.name)
        else:
            user_cfg[key] = {
                'id': user.id,
                'email': user.email,
                'password': user.password,
                'rolename': [role.name],
                'ap': []
            }

            try:
                user_cfg[key]['username'] = user.username
            except BaseException:
                # if old db has no username field, use email field.
                user_cfg[key]['username'] = user.email

            try:
                user_cfg[key]['org'] = user.org
            except BaseException:
                # if old db has no org field, derive from email field.
                if '@' in user.email:
                    user_cfg[key]['org'] = user.email[user.email.index(
                        '@') + 1:]
                else:  # dummy user - dummy org
                    user_cfg[key]['org'] = ""

    try:
        limits = db.session.query(Limit).filter(id=0).first()
        limit = {'min_eirp': float(
            limits.min_eirp), 'enforce': bool(limits.enforce)}

    except BaseException:
        LOGGER.debug("Error exporting EIRP Limit")

    with open(filename, 'w') as fpout:
        for k, v in user_cfg.items():
            fpout.write("%s\n" % json.dumps({'userConfig': v}))
        if limit:
            fpout.write("%s\n" % json.dumps({'Limit': limit}))


def setUserIdNextVal():
    # Set nextval for the sequence so that next user record
    # will not reuse older id.
    cmd = 'select max(id) from aaa_user'
    res = db.session.execute(cmd)
    val = res.fetchone()[0]
    if val:
        cmd = 'ALTER SEQUENCE aaa_user_id_seq RESTART WITH ' + str(val + 1)
    db.session.execute(cmd)
    db.session.commit()


# 'db-import' command. Click binding is in main()
def db_import(src):
    """Import User Database."""
    LOGGER.debug('db_import() %s', src)
    from afcmodels.aaa import User, Limit

    filename = src
    if not os.path.exists(filename):
        raise RuntimeError(
            '"{}" source file does not exist'.format(filename))

    LOGGER.debug('Open admin cfg src file - %s', filename)
    with open(filename, 'r') as fp_src:
        while True:
            dataline = fp_src.readline()
            if not dataline:
                break
            # add user, APs and server configuration
            new_rcrd = json.loads(dataline)
            user_rcrd = json_lookup('userConfig', new_rcrd, None)
            if user_rcrd:
                username = json_lookup('username', user_rcrd, None)
                try:
                    user_create_internal(current_app, user_rcrd[0], True)
                except RuntimeError:
                    LOGGER.debug('User %s already exists', username[0])

            else:
                limit = json_lookup('Limit', new_rcrd, None)
                try:
                    limits = db.session.query(
                        Limit).filter_by(id=0).first()
                    # insert case
                    if limits is None:
                        limits = Limit(limit[0]['min_eirp'])
                        db.session.add(limits)
                    elif (limit[0]['enforce'] == False):
                        limits.enforce = False
                    else:
                        limits.min_eirp = limit[0]['min_eirp']
                        limits.enforce = True
                    db.session.commit()
                except BaseException:
                    raise RuntimeError(
                        "Can't commit DB for EIRP limits")

        setUserIdNextVal()


def db_upgrade():
    """Upgrade User Database."""
    import flask
    from afcmodels.aaa import Ruleset
    from flask_migrate import (upgrade, stamp)
    setUserIdNextVal()
    try:
        from afcmodels.aaa import User
        user = db.session.query(
            User).first()  # pylint: disable=no-member
    except Exception as exception:
        if 'aaa_user.username does not exist' in str(exception.args):
            LOGGER.error("upgrade from preOIDC version")
            stamp(directory=os.path.join(os.path.dirname(__file__),
                                         "migrations"),
                  revision='4c904e86218d')
        elif 'aaa_user.org does not exist' in str(exception.args):
            LOGGER.error("upgrade from mtls version")
            stamp(directory=os.path.join(os.path.dirname(__file__),
                                         "migrations"),
                  revision='230b7680b81e')
    db.session.commit()
    upgrade(directory=os.path.join(os.path.dirname(__file__), "migrations"))
    ruleset_list = RulesetVsRegion.ruleset_list()
    for rule in ruleset_list:
        get_or_create(db.session, Ruleset, name=rule)


# Create user group
@click.group(name='user')
def user_cli():
    """View and manage AAA state"""
    pass


def user_create_internal(app, user_params, hashed=False):
    from contextlib import closing
    import datetime
    from afcmodels.aaa import User, Role, Organization
    LOGGER.debug('user_create_internal() %s %s %s',
                 user_params['email'] if 'email' in user_params else user_params['username'],
                 user_params['password'],
                 user_params['rolename'])

    if 'UPGRADE_REQ' in app.config and app.config['UPGRADE_REQ']:
        return

    try:
        if 'id' in user_params.keys():
            id = user_params['id']
        else:
            id = None

        if 'email' in user_params.keys():
            email = user_params['email']
        else:
            email = user_params['username']

        if 'org' in user_params.keys():
            org = user_params['org']
        else:
            org = None
            try:
                org = email[email.index('@') + 1:]
            except BaseException:
                org = ""

        if not hashed:
            # hash password field in non OIDC mode
            password_in = user_params['password']
            if isinstance(password_in, str):
                password = password_in.strip()
            else:
                with closing(open(password_in)) as pwfile:
                    password = pwfile.read().strip()

            if app.config['OIDC_LOGIN']:
                # OIDC, password is never stored locally
                # Still we hash it so that if we switch back
                # to non OIDC, the hash still match, and can be logged in
                from passlib.context import CryptContext
                password_crypt_context = CryptContext(['bcrypt'])
                passhash = password_crypt_context.encrypt(password_in)
            else:
                passhash = app.user_manager.password_manager.hash_password(
                    password)
        else:
            passhash = user_params['password']

        # select count(*) from aaa_user where email = ?
        if User.query.filter(User.email == email).count() > 0:
            raise RuntimeError(
                'Existing user found with email "{0}"'.format(email))

        organization = Organization.query.filter_by(name=org).first()
        if not organization:
            organization = Organization(name=org)
            db.session.add(organization)

        if id:
            user = User(
                id=id,
                username=user_params['username'],
                email=email,
                org=org,
                email_confirmed_at=datetime.datetime.now(),
                password=passhash,
                active=True,
            )
        else:
            user = User(
                username=user_params['username'],
                email=email,
                org=org,
                email_confirmed_at=datetime.datetime.now(),
                password=passhash,
                active=True,
            )
        for rolename in user_params['rolename']:
            user.roles.append(get_or_create(
                db.session, Role, name=rolename))
        db.session.add(user)  # pylint: disable=no-member
        db.session.commit()  # pylint: disable=no-member
    except IOError:
        raise RuntimeError(
            'Password received was not readable.'
            'Enter as a readable pipe.\n'
            'i.e. echo "pass" | rat-manage api'
            'user create email /dev/stdin')


@user_cli.command('create')
@click.argument('username', type=str)
@click.argument('password_in', type=str)
@click.option('--role', type=click.Choice(['Admin', 'Super', 'Analysis', 'AP', 'Trial']), multiple=True, help='role to include with the new user')
@click.option('--org', type=str, help='Organization')
@click.option('--email', type=str, help='User email')
@with_appcontext
def user_create(username, password_in, role, org, email):
    """Create a new user."""
    # If command does not provide email. Populate email field with username
    if not email:
        email = username

    from contextlib import closing
    import datetime
    from afcmodels.aaa import User, Role, Organization

    if 'UPGRADE_REQ' in current_app.config and current_app.config['UPGRADE_REQ']:
        return

    try:
        # hash password field in non OIDC mode
        if isinstance(password_in, str):
            password = password_in.strip()
        else:
            with closing(open(password_in)) as pwfile:
                password = pwfile.read().strip()

        if current_app.config['OIDC_LOGIN']:
            # OIDC, password is never stored locally
            # Still we hash it so that if we switch back
            # to non OIDC, the hash still match, and can be logged in
            from passlib.context import CryptContext
            password_crypt_context = CryptContext(['bcrypt'])
            passhash = password_crypt_context.encrypt(password_in)
        else:
            passhash = current_app.user_manager.password_manager.hash_password(
                password)

        # select count(*) from aaa_user where email = ?
        if User.query.filter(User.email == email).count() > 0:
            raise RuntimeError(
                'Existing user found with email "{0}"'.format(email))

        if not org:
            try:
                org = email[email.index('@') + 1:]
            except BaseException:
                org = ""

        organization = Organization.query.filter_by(name=org).first()
        if not organization:
            organization = Organization(name=org)
            db.session.add(organization)

        user = User(
            username=username,
            email=email,
            org=org,
            email_confirmed_at=datetime.datetime.now(),
            password=passhash,
            active=True,
        )
        for rolename in role:
            user.roles.append(get_or_create(
                db.session, Role, name=rolename))
        db.session.add(user)  # pylint: disable=no-member
        db.session.commit()  # pylint: disable=no-member
    except IOError:
        raise RuntimeError(
            'Password received was not readable.'
            'Enter as a readable pipe.\n'
            'i.e. echo "pass" | rat-manage api'
            'user create email /dev/stdin')


@user_cli.command('update')
@click.option('--email', type=str, required=True, help='Email')
@click.option('--role', type=click.Choice(['Admin', 'Super', 'Analysis', 'AP', 'Trial']), multiple=True, help='role to include with the new user')
@click.option('--org', type=str, help='Organization')
@with_appcontext
def user_update(email, role, org):
    """Update a user."""
    from contextlib import closing
    from afcmodels.aaa import User, Role, Organization

    if 'UPGRADE_REQ' in current_app.config and current_app.config['UPGRADE_REQ']:
        return

    try:
        user = User.getemail(email)

        if user:
            user.roles = []
            for rolename in role:
                user.roles.append(get_or_create(
                    db.session, Role, name=rolename))
            user.active = True
            if org:
                user.org = org
            org = user.org
            organization = Organization.query.filter_by(
                name=org).first()
            if not organization:
                organization = Organization(name=org)
                db.session.add(
                    organization)  # pylint: disable=no-member

            db.session.commit()  # pylint: disable=no-member
        else:
            raise RuntimeError("User update: User not found")
    except IOError:
        raise RuntimeError("User update encounters unexpected error")


@user_cli.command('remove')
@click.argument('email', type=str)
@with_appcontext
def user_remove(email):
    """Remove a user by email."""
    from afcmodels.aaa import User, Role
    LOGGER.debug('user_remove() %s', email)

    try:
        # select * from aaa_user where email = ? limit 1
        user = User.query.filter(User.email == email).one()
    except sqlalchemy.orm.exc.NoResultFound:
        raise RuntimeError(
            'No user found with email "{0}"'.format(email))
    db.session.delete(user)  # pylint: disable=no-member
    db.session.commit()  # pylint: disable=no-member


@user_cli.command('list')
@with_appcontext
def user_list():
    """List all users."""
    LOGGER.debug('user_list()')
    table = PrettyTable()
    from afcmodels.aaa import User, UserRole, Role
    table.field_names = ["ID", "UserName", "Email", "Org", "Roles"]

    if 'UPGRADE_REQ' in current_app.config and current_app.config['UPGRADE_REQ']:
        return

    user_info = {}
    # select email, name from aaa_user as a join aaa_user_role as aur
    # on au.id = aur.user_id join aaa_role as ar on ar.id =
    # aur.role_id;
    for user, _, role in db.session.query(
            User, UserRole, Role).filter(
            User.id == UserRole.user_id).filter(
            UserRole.role_id == Role.id).all():  # pylint: disable=no-member

        key = user.email + ":" + user.org + ":" + \
            str(user.id) + ":" + user.username

        if key in user_info:
            user_info[key] = user_info[key] + ", " + role.name
        else:
            user_info[key] = role.name

    # Find all users without roles and show them last
    for user in db.session.query(User).filter(~User.roles.any()).all():
        key = user.email + ":" + user.org + ":" + \
            str(user.id) + ":" + user.username
        user_info[key] = ""

    for k, v in user_info.items():
        email, org, _id, name = k.split(":")
        table.add_row([_id, name, email, org, v])

    print(table)


# Access Points Deny commands
@click.group(name='ap-deny')
def ap_deny_group():
    '''View and manage Access Points Denied'''
    pass


@ap_deny_group.command('create')
@click.option('--serial', type=str, default=None, help='serial number of the ap')
@click.option('--cert_id', type=str, required=True, help='certification id of the ap')
@click.option('--ruleset', type=str, required=True, help='ruleset of the ap')
@click.option('--org', type=str, required=True, help='org of the ap')
@with_appcontext
def ap_deny_create(serial, cert_id, ruleset, org):
    ''' Create a new access point. '''
    from contextlib import closing
    import datetime
    from afcmodels.aaa import AccessPointDeny, Organization, Ruleset
    LOGGER.debug('AccessPointDenyCreate._create_ap() %s %s %s',
                 serial, cert_id, ruleset)
    if not cert_id or not org or not ruleset:
        raise RuntimeError('Certification, org and ruleset required')

    ruleset_obj = Ruleset.query.filter_by(name=ruleset).first()
    if not ruleset_obj:
        raise RuntimeError('Bad ruleset')

    ap = AccessPointDeny.query.filter(AccessPointDeny.
                                      certification_id == cert_id).\
        filter(AccessPointDeny.serial_number == serial).first()
    if ap:
        raise RuntimeError('Duplicate device detected')

    organization = Organization.query.filter_by(name=org).first()
    if not organization:
        organization = Organization(name=org)
        db.session.add(organization)
    ap = AccessPointDeny(
        serial_number=serial,
        certification_id=cert_id
    )

    organization.aps.append(ap)
    ruleset_obj.aps.append(ap)
    db.session.add(ap)
    db.session.commit()


@ap_deny_group.command('remove')
@click.option('--serial', type=str, default=None, help='Serial number of an Access Point')
@click.option('--cert_id', type=str, default=None, help='certification id of the ap')
@with_appcontext
def ap_deny_remove(serial, cert_id):
    '''Removes an access point by serial number and or certification id'''
    from afcmodels.aaa import AccessPointDeny
    LOGGER.debug('AccessPointDenyRemove._remove_ap() %s', serial)
    try:
        # select * from access_point as ap where ap.serial_number = ?
        ap = AccessPointDeny.query.filter(
            AccessPointDeny.serial_number == serial).\
            filter(AccessPointDeny.certification_id == cert_id).one()

        if not ap:
            raise RuntimeError('No access point found')
    except BaseException:
        raise RuntimeError('No access point found')

    db.session.delete(ap)  # pylint: disable=no-member
    db.session.commit()  # pylint: disable=no-member


@ap_deny_group.command('list')
@with_appcontext
def ap_deny_list():
    '''Lists all access points'''
    table = PrettyTable()
    from afcmodels.aaa import AccessPointDeny

    table.field_names = ["Serial Number", "Cert ID", "Ruleset", "Org"]
    for ap in db.session.query(
            AccessPointDeny).all():  # pylint: disable=no-member
        table.add_row([ap.serial_number, ap.certification_id,
                       ap.ruleset.name, ap.org.name])
    print(table)


# CertificationId commands
@click.group(name='cert_id')
def cert_id_group():
    '''View and manage CertificationId'''
    pass


@cert_id_group.command('list')
@with_appcontext
def cert_id_list():
    '''Lists all certification IDs'''
    table = PrettyTable()
    from afcmodels.aaa import CertId, Organization

    table.field_names = ["Cert ID", "Ruleset", "Loc", "Refreshed"]
    for cert in db.session.query(
            CertId).all():  # pylint: disable=no-member
        table.add_row([cert.certification_id, cert.ruleset.name,
                       cert.location, cert.refreshed_at])
    print(table)


@cert_id_group.command('remove')
@click.option('--cert_id', type=str, required=True, help='certificate id')
@with_appcontext
def cert_id_remove(cert_id):
    '''Removes a Certificate Id by certificate id'''
    from afcmodels.aaa import CertId
    LOGGER.debug('CertIdRemove._remove_cert_id() %s', cert_id)
    try:
        cert = CertId.query.filter(
            CertId.certification_id == cert_id).one()
    except sqlalchemy.orm.exc.NoResultFound:
        raise RuntimeError(
            'No certificate found with id "{0}"'.format(cert_id))
    db.session.delete(cert)  # pylint: disable=no-member
    db.session.commit()  # pylint: disable=no-member


@cert_id_group.command('create')
@click.option('--cert_id', type=str, required=True, help='certification id')
@click.option('--ruleset_id', type=str, required=True, help='ruleset id')
@click.option('--location', type=int, required=True,
              help="location. 1 indoor - 2 outdoor - 3 both")
@with_appcontext
def cert_id_create(cert_id, ruleset_id, location=0):
    ''' Create a new certification Id. '''
    cert_id_create_internal(cert_id, ruleset_id, location=0)


def cert_id_create_internal(cert_id, ruleset_id, location=0):
    from contextlib import closing
    import datetime
    from afcmodels.aaa import CertId, Ruleset, Organization
    LOGGER.debug('CertIdCreate._create_cert_id() %s %s',
                 cert_id, ruleset_id)
    if not ruleset_id:
        raise RuntimeError("Ruleset is required")

    # validate ruleset
    ruleset = Ruleset.query.filter_by(name=ruleset_id).first()
    if not ruleset:
        raise RuntimeError("Invalid Ruleset")

    if CertId.query.filter(
            CertId.certification_id == cert_id).count() > 0:
        raise RuntimeError(
            'Existing certificate found with id "{0}"'.format(cert_id))

    cert = CertId(certification_id=cert_id, location=location)
    ruleset.cert_ids.append(cert)

    db.session.add(cert)  # pylint: disable=no-member
    db.session.commit()  # pylint: disable=no-member


@cert_id_group.command('sweep')
@click.option('--country', type=str, help='country e.g. US or CA')
@with_appcontext
def cert_id_sweep(country):
    '''Update certification IDs from official sources'''
    als.als_json_log('cert_db', {'action': 'sweep', 'country': country,
                                 'status': 'starting'})
    if country == "US":
        # first sweep SD (outdoor) entries, then sweep ID list.
        _sweep_fcc_sd()
        _sweep_fcc_id()
    else:
        _sweep_canada()


def _sweep_canada():
    import csv
    import requests
    from afcmodels.aaa import CertId, Ruleset
    import datetime
    now = datetime.datetime.now()

    url = \
        "https://www.ic.gc.ca/engineering/Certified_Standard_Power_Access_Points_6GHz.csv"
    headers = {
        'accept': 'text/html,application/xhtml+xml,application/xml',
        'cache-control': 'max-age=0',
        'content-type': 'application/x-www-form-urlencoded',
        'user-agent': 'rat_server/1.0'
    }
    try:
        timeout = os.environ.get("REQUEST_TIMEOUT_SEC")
        if timeout is not None:
            timeout = float(timeout)
        with requests.get(url, headers, stream=True, timeout=timeout) as r:
            r.raise_for_status()

            local_filename = "/tmp/SD6_list.csv"
            with open(local_filename, 'wb') as f:
                for chunk in r.iter_content(chunk_size=8192):
                    f.write(chunk)

        with open(local_filename, newline='') as csvfile:
            rdr = csv.reader(csvfile, delimiter=',')
            for row in rdr:
                try:
                    cert_id = row[7]
                    code = int(row[6])
                    if code == 103:
                        location = CERT_ID_LOCATION_OUTDOOR
                    elif code == 111:
                        location = \
                            CERT_ID_LOCATION_INDOOR | \
                            CERT_ID_LOCATION_OUTDOOR
                    else:
                        location = CERT_ID_LOCATION_UNKNOWN
                    if not location == CERT_ID_LOCATION_UNKNOWN:
                        cert = CertId.query.filter_by(
                            certification_id=cert_id).first()
                        if cert:
                            cert.refreshed_at = now
                            cert.location = location
                        else:
                            cert = CertId(certification_id=cert_id,
                                          location=location)
                            ruleset_id_str = \
                                RulesetVsRegion.region_to_ruleset(
                                    "CA",
                                    exc=werkzeug.exceptions.NotFound)
                            ruleset = Ruleset.query.filter_by(
                                name=ruleset_id_str).first()
                            ruleset.cert_ids.append(cert)
                            db.session.add(cert)
                except BaseException:
                    # ignore badly formatted rows
                    pass
            als.als_json_log(
                'cert_db', {
                    'action': 'sweep', 'country': 'CA', 'status': 'success'})
            db.session.commit()  # pylint: disable=no-member
    except BaseException:
        als.als_json_log(
            'cert_db', {
                'action': 'sweep', 'country': 'CA', 'status': 'failed download'})
        _mail_alert('CA', 'none')


def _sweep_fcc_id():
    id_data = "grantee_code=&product_code=&applicant_name=&grant_date_from=&grant_date_to=&comments=&application_purpose=&application_purpose_description=&grant_code_1=&grant_code_2=&grant_code_3=&test_firm=&application_status=&application_status_description=&equipment_class=243&equipment_class_description=6ID-15E+6+GHz+Low+Power+Indoor+Access+Point&lower_frequency=&upper_frequency=&freq_exact_match=on&bandwidth_from=&emission_designator=&tolerance_from=&tolerance_to=&tolerance_exact_match=on&power_output_from=&power_output_to=&power_exact_match=on&rule_part_1=&rule_part_2=&rule_part_3=&rule_part_exact_match=on&product_description=&modular_type_description=&tcb_code=&tcb_code_description=&tcb_scope=&tcb_scope_description=&outputformat=XML&show_records=10&fetchfrom=0&calledFromFrame=N"  # noqa
    _sweep_fcc_data(id_data, CERT_ID_LOCATION_INDOOR)


def _sweep_fcc_sd():
    sd_data = "grantee_code=&product_code=&applicant_name=&grant_date_from=&grant_date_to=&comments=&application_purpose=&application_purpose_description=&grant_code_1=&grant_code_2=&grant_code_3=&test_firm=&application_status=&application_status_description=&equipment_class=250&equipment_class_description=6SD-15E+6+GHz+Standard+Power+Access+Point&lower_frequency=&upper_frequency=&freq_exact_match=on&bandwidth_from=&emission_designator=&tolerance_from=&tolerance_to=&tolerance_exact_match=on&power_output_from=&power_output_to=&power_exact_match=on&rule_part_1=&rule_part_2=&rule_part_3=&rule_part_exact_match=on&product_description=&modular_type_description=&tcb_code=&tcb_code_description=&tcb_scope=&tcb_scope_description=&outputformat=XML&show_records=10&fetchfrom=0&calledFromFrame=N"  # noqa
    _sweep_fcc_data(sd_data, CERT_ID_LOCATION_OUTDOOR)


def _sweep_fcc_data(data, location):
    from afcmodels.aaa import CertId, Ruleset
    import requests
    import datetime

    url = f'https://apps.fcc.gov/oetcf/eas/reports/GenericSearchResult.cfm?RequestTimeout={os.environ.get("REQUEST_TIMEOUT_SEC", "500")}'
    headers = {
        'accept': 'text/html,application/xhtml+xml,application/xml',
        'accept-encoding': 'deflate, gzip, br, zstd',
        'cache-control': 'max-age=0',
        'content-type': 'application/x-www-form-urlencoded',
        'user-agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.2 Safari/605.1.15'
    }
    for _ in range(5):
        now = datetime.datetime.now()
        try:
            timeout = os.environ.get("REQUEST_TIMEOUT_SEC")
            if timeout is not None:
                timeout = float(timeout)
            resp = requests.post(url, headers=headers, data=data, timeout=timeout)
            if resp.status_code == 200:
                try:
                    from xml.etree import ElementTree
                    tree = ElementTree.fromstring(resp.content)
                    for node in tree:
                        fcc_id = node.find('fcc_id').text
                        cert = CertId.query.filter_by(
                            certification_id=fcc_id).first()

                        if cert:
                            cert.refreshed_at = now
                            cert.location = cert.location | location
                        elif location == CERT_ID_LOCATION_OUTDOOR:
                            # add new entries that are in 6SD list.
                            cert = CertId(certification_id=fcc_id,
                                          location=CERT_ID_LOCATION_OUTDOOR)
                            ruleset_id_str = \
                                RulesetVsRegion.region_to_ruleset(
                                    "US", exc=werkzeug.exceptions.NotFound)
                            ruleset = Ruleset.query.filter_by(
                                name=ruleset_id_str).first()
                            ruleset.cert_ids.append(cert)
                            db.session.add(cert)

                    als.als_json_log(
                        'cert_db', {
                            'action': 'sweep', 'country': 'US', 'status': 'success'})
                except BaseException:
                    raise RuntimeError("Bad XML in Cert Id download")
            else:
                raise RuntimeError("Bad Cert Id download")
            db.session.commit()  # pylint: disable=no-member
            return
        except BaseException:
            time.sleep(5)
    als.als_json_log(
        'cert_db', {
            'action': 'sweep', 'country': 'US', 'status': 'failed download'})
    _mail_alert('US', str(location))

def _mail_alert(country, other):
    from flask_mail import Mail, Message

    src_email = current_app.config['REGISTRATION_SRC_EMAIL']
    dest_email = current_app.config['REGISTRATION_DEST_PDL_EMAIL']

    mail = Mail(current_app)
    msg = Message(f"AFC CertId download failure",
                  sender=src_email,
                  recipients=[dest_email])
    msg.body = f'''Fail to download CertId for country: {country} info {other}'''
    mail.send(msg)


# Create organization group
@click.group(name='org')
def org_cli():
    """View and manage Organizations"""
    pass


@org_cli.command('create')
@click.option('--name', type=str, required=True, help='Name of Organization')
@with_appcontext
def org_create(name):
    """Create a new Organization."""
    org_create_internal(name)

def org_create_internal(name):
    from contextlib import closing
    import datetime
    from afcmodels.aaa import Organization
    LOGGER.debug('org_create() %s', name)
    if name is None:
        raise RuntimeError('Name required')

    org = Organization.query.filter(Organization.
                                    name == name).first()
    if org:
        raise RuntimeError('Duplicate org detected')

    organization = Organization(name=name)
    db.session.add(organization)  # pylint: disable=no-member
    db.session.commit()  # pylint: disable=no-member


@org_cli.command('remove')
@click.option('--name', type=str, required=True, help='Name of Organization')
@with_appcontext
def org_remove(name):
    """Remove an organization by name."""
    from afcmodels.aaa import Organization
    LOGGER.debug('org_remove() %s', name)
    try:
        # select * from access_point as ap where ap.serial_number = ?
        org = Organization.query.filter(
            Organization.name == name).one()
        if not org:
            raise RuntimeError('No organization found')
    except BaseException:
        raise RuntimeError('No organization found')

    for ap in org.aps:
        db.session.delete(ap)

    db.session.delete(org)  # pylint: disable=no-member
    db.session.commit()  # pylint: disable=no-member


@org_cli.command('list')
@with_appcontext
def org_list():
    """List all organizations."""
    table = PrettyTable()
    from afcmodels.aaa import Organization
    table.field_names = ["Name"]
    for org in db.session.query(
            Organization).all():  # pylint: disable=no-member
        table.add_row([org.name])
    print(table)


# MTLS commands
@click.group(name='mtls')
def mtls_group():
    '''View and manage MTLS certificates'''
    pass


@mtls_group.command('create')
@click.option('--note', type=str, default=None, help="note")
@click.option('--src', type=str, default=None, help="certificate file")
@click.option('--org', type=str, default="", help="email of user assocated with this access point.")
@with_appcontext
def mtls_create(note, src, org):
    ''' Create a new mtls certificate. '''
    from contextlib import closing
    import datetime
    from afcmodels.aaa import MTLS, User
    LOGGER.debug('MTLS._create_mtls() %s %s %s',
                 note, org, src)
    if not src:
        raise RuntimeError('certificate data file required')

    cert = src
    cert_data = ""
    with open(cert, 'r') as fp_cert:
        while True:
            dataline = fp_cert.readline()
            if not dataline:
                break
            cert_data = cert_data + dataline
        mtls = MTLS(cert_data, note, org)
        db.session.add(mtls)  # pylint: disable=no-member
        db.session.commit()  # pylint: disable=no-member


@mtls_group.command('remove')
@click.option('--id', type=int, default=None, help="id")
@with_appcontext
def mtls_remove(id):
    ''' Remove MTLS certificate by id. '''
    from contextlib import closing
    import datetime
    from afcmodels.aaa import MTLS, User
    LOGGER.debug('MTLS._remove_mtls() %d', id)

    if not id:
        raise RuntimeError('mtls id required')

    try:
        mtls = MTLS.query.filter(MTLS.id == id).one()
    except sqlalchemy.orm.exc.NoResultFound:
        raise RuntimeError(
            'No mtls certificate found with id"{0}"'.format(id))
    db.session.delete(mtls)  # pylint: disable=no-member
    db.session.commit()  # pylint: disable=no-member


@mtls_group.command('list')
@with_appcontext
def mtls_list():
    '''Lists all mtls certificates'''
    table = PrettyTable()
    from afcmodels.aaa import MTLS

    table.field_names = ["ID", "Note", "Org", "Create"]
    for mtls in db.session.query(
            MTLS).all():  # pylint: disable=no-member
        org = mtls.org if mtls.org else ""
        note = mtls.note if mtls.note else ""
        table.add_row(
            [mtls.id, mtls.note, mtls.org, str(mtls.created)])
    print(table)


@mtls_group.command('dump')
@click.option('--id', type=int, default=None, help="id")
@click.option('--dst', type=str, default=None, help="output file")
@with_appcontext
def mtls_dump(id, dst):
    ''' Dump MTLS certificate to file. '''
    from contextlib import closing
    import datetime
    from afcmodels.aaa import MTLS, User
    LOGGER.debug('MTLS._remove_mtls() %d', id)
    if not id:
        raise RuntimeError('mtls id required')
    if not dst:
        raise RuntimeError('output filename required')
    filename = dst

    try:
        mtls = MTLS.query.filter(MTLS.id == id).one()
    except sqlalchemy.orm.exc.NoResultFound:
        raise RuntimeError(
            'No mtls certificate found with id"{0}"'.format(id))

    with open(filename, 'w') as fpout:
        fpout.write("%s" % (mtls.cert))


# Celery commands group
@click.group(name='celery')
def celery_group():
    """Celery commands."""
    pass


@celery_group.command('status')
@with_appcontext
def celery_status():
    """Get status of celery workers."""
    subprocess.call(['celery', 'status'])


@celery_group.command('test')
@click.argument('request_type', type=click.Choice(['PointAnalysis', 'ExclusionZoneAnalysis', 'HeatmapAnalysis']))
@click.argument('request_file', type=click.Path(exists=True))
@click.argument('afc_config', type=click.Path(exists=True))
@click.option('--response-file', default=None, help='destination for json results')
@click.option('--afc-engine', default=None)
@click.option('--user-id', type=int, default=1)
@click.option('--username', default='test_user')
@click.option('--response-dir', default=None)
@click.option('--temp-dir', default='./test-celery-tmp')
@click.option('--history-dir', default=None)
@click.option('--debug', is_flag=True)
@with_appcontext
def test_celery(request_type, request_file, afc_config, response_file, afc_engine, user_id,
                username, response_dir, temp_dir, history_dir, debug):
    """Run celery task in isolation."""
    from afc_worker import run

    if not os.path.exists(temp_dir):
        os.makedirs(temp_dir)

    if response_file is None:
        response_file = os.path.join(
            temp_dir, request_type + '_response.json')
    if response_dir is None:
        response_dir = os.path.join(
            current_app.config['NFS_MOUNT_PATH'], 'responses')
    if history_dir is None:
        history_dir = current_app.config['HISTORY_DIR']

    LOGGER.info('Submitting task...')
    task = run.apply_async(args=[
        user_id,
        username,
        current_app.config['STATE_ROOT_PATH'],
        temp_dir,
        request_type,
        request_file,
        afc_config,
        response_file,
        history_dir,
        current_app.config['NFS_MOUNT_PATH'],
    ])

    if task.state == 'FAILURE':
        raise Exception('Task was unable to be started')

    # wait for task to complete and get result
    task.wait()

    if task.failed():
        raise Exception('Task excecution failed')

    if task.successful() and task.result['status'] == 'DONE':
        if not os.path.exists(task.result['result_path']):
            raise Exception('Resource already deleted')
        # read temporary file generated by afc-engine
        LOGGER.debug("Reading result file: %s",
                     task.result['result_path'])
        LOGGER.info('SUCCESS: result file located at "%s"',
                    task.result['result_path'])

    elif task.successful() and task.result['status'] == 'ERROR':
        if not os.path.exists(task.result['error_path']):
            raise Exception('Resource already deleted')
        # read temporary file generated by afc-engine
        LOGGER.debug("Reading error file: %s",
                     task.result['error_path'])
        LOGGER.error('AFC ENGINE ERROR: error file located at "%s"',
                     task.result['error_path'])
    else:
        raise Exception('Invalid task state')


# Config commands group
@click.group(name='cfg')
def cfg_group():
    """View and manage configuration records."""
    pass


@cfg_group.command('add')
@click.argument('src', metavar='SRC=FILENAME')
@with_appcontext
def config_add(src):
    """Create a new admin configuration."""
    LOGGER.debug('ConfigAdd.__call__() %s', src)
    from afcmodels.aaa import AFCConfig, CertId, User
    import datetime

    split_items = src.split('=', 1)
    filename = split_items[1].strip()
    if not os.path.exists(filename):
        raise RuntimeError(
            '"{}" source file does not exist'.format(filename))

    rollback = []
    LOGGER.debug('Open admin cfg src file - %s', filename)
    with open(filename, 'r') as fp_src:
        while True:
            dataline = fp_src.readline()
            if not dataline:
                break
            # add user, APs and server configuration
            new_rcrd = json.loads(dataline)
            user_rcrd = json_lookup('userConfig', new_rcrd, None)
            username = json_lookup('username', user_rcrd, None)
            try:
                user_create_internal(current_app,
                                     {'username': username[0],
                                      'password': '',
                                      'email': username[0],
                                      'rolename': 'Trial'},
                                     False)
                f_str = "UserRemove('" + username[0] + "')"
                rollback.insert(0, f_str)
            except RuntimeError:
                LOGGER.debug('User %s already exists', username[0])

            try:
                ap_rcrd = json_lookup('apConfig', new_rcrd, None)
                serial_id = json_lookup('serialNumber', ap_rcrd, None)
                location = json_lookup('location', ap_rcrd, None)
                cert_id = json_lookup('certificationId', ap_rcrd, None)
                for i in range(len(serial_id)):
                    cert_id_str = cert_id[i][0]['id']
                    ruleset_id_str = cert_id[i][0]['rulesetId']
                    loc = int(location[i])

                    email = username[0]
                    if '@' in email:
                        org = email[email.index('@') + 1:]
                    else:
                        org = ""

                    try:
                        org_create_internal(org)
                        f_str = "OrganizationRemove('" + \
                            org + "')"
                        rollback.insert(0, f_str)
                    except BaseException:
                        pass

                    try:
                        # Since this is test, we mark these as both indoor
                        # and indoor certified
                        cert_id_create_internal(cert_id_str, ruleset_id_str,
                                                loc)
                        f_str = "CertIdRemove('" + \
                            cert_id_str + "')"
                        rollback.insert(0, f_str)
                    except BaseException:
                        LOGGER.debug(
                            'CertId %s already exists', cert_id_str)

                user = User.query.filter(
                    User.email == username[0]).one()
                LOGGER.debug('New user id %d', user.id)

                cfg_rcrd = json_lookup('afcConfig', new_rcrd, None)
                region_rcrd = json_lookup('regionStr', cfg_rcrd, None)
                # validate the region string
                RulesetVsRegion.region_to_ruleset(
                    region_rcrd[0], exc=werkzeug.exceptions.NotFound)

                config = AFCConfig.query.filter(
                    AFCConfig.config['regionStr'].astext == region_rcrd[0]).first()
                als_log_afc_config_change(
                    old_config=config.config if config else None,
                    new_config=cfg_rcrd[0], user=username[0],
                    region=region_rcrd[0], source='manage.py')
                if not config:
                    config = AFCConfig(cfg_rcrd[0])
                    config.config['regionStr'] = config.config['regionStr'].upper()
                    db.session.add(config)
                else:
                    config.config = cfg_rcrd[0]
                    config.created = datetime.datetime.now()
                db.session.commit()

            except Exception as e:
                LOGGER.error(e)
                LOGGER.error('Rolling back...')
                for f in rollback:
                    eval(f)


@cfg_group.command('del')
@click.argument('src', metavar='SRC=FILENAME')
@with_appcontext
def config_remove(src):
    """Remove a user by email."""
    LOGGER.debug('ConfigRemove.__call__() %s', src)
    from afcmodels.aaa import User

    split_items = src.split('=', 1)
    filename = split_items[1].strip()
    if not os.path.exists(filename):
        raise RuntimeError(
            '"{}" source file does not exist'.format(filename))

    LOGGER.debug('Open admin cfg src file - %s', filename)
    with open(filename, 'r') as fp_src:
        while True:
            dataline = fp_src.readline()
            if not dataline:
                break
            new_rcrd = json.loads(dataline)

            ap_rcrd = json_lookup('apConfig', new_rcrd, None)
            user_rcrd = json_lookup('userConfig', new_rcrd, None)
            username = json_lookup('username', user_rcrd, None)
            try:
                user = User.query.filter(
                    User.email == username[0]).one()
                LOGGER.debug('Found user id %d', user.id)
                UserRemove(username[0])
            except RuntimeError:
                LOGGER.debug('Delete missing user %s', username[0])
            except Exception as e:
                LOGGER.debug('Missing user %s in DB', username[0])


@cfg_group.command('list')
@click.argument('src', metavar='SRC=FILENAME')
@with_appcontext
def config_show(src):
    """Show all configurations."""
    LOGGER.debug('ConfigShow.__call__()')
    split_items = src.split('=', 1)
    filename = split_items[1].strip()
    if not os.path.exists(filename):
        raise RuntimeError(
            '"{}" source file does not exist'.format(filename))

    LOGGER.debug('Open admin cfg src file - %s', filename)
    with open(filename, 'r') as fp_src:
        while True:
            dataline = fp_src.readline()
            if not dataline:
                break
            new_rcrd = json.loads(dataline)
            user_rcrd = json_lookup('userConfig', new_rcrd, None)
            LOGGER.info('\nRecord ...\n\tuserConfig\n')
            LOGGER.info(user_rcrd)

            ap_rcrd = json_lookup('apConfig', new_rcrd, None)
            LOGGER.info('\n\tapConfig\n')
            for i in range(len(ap_rcrd[0])):
                LOGGER.info(ap_rcrd[0][i])

            cfg_rcrd = json_lookup('afcConfig', new_rcrd, None)
            LOGGER.info('\n\tafcConfig\n')
            LOGGER.info(cfg_rcrd)


def create_cli_app():
    """Create an application instance with the CLI."""
    app = create_app()

    # Initialize Flask Migrate with the app
    migrate = Migrate(app, db)

    # Register all CLI command groups
    app.cli.add_command(data_cli)
    app.cli.add_command(user_cli)
    app.cli.add_command(mtls_group)
    app.cli.add_command(celery_group)
    app.cli.add_command(ap_deny_group)
    app.cli.add_command(org_cli)
    app.cli.add_command(cert_id_group)
    app.cli.add_command(cfg_group)

    return app


def main():
    """Main entry point for the application."""

    # Configure logging for the CLI
    logging.basicConfig(level=logging.INFO)

    # Create a FlaskGroup for handling CLI commands
    cli = FlaskGroup(create_app=lambda: create_cli_app())

    # Add version command
    version_name = cmd_utils.packageversion(__package__)
    if version_name is not None:
        @cli.command('version')
        def version():
            """Show the application version."""
            click.echo(f"{sys.argv[0]} {version_name}")

    # Add other ungrouped commands
    @cli.command('db-create')
    @click.option('--if_absent', is_flag=True, help="Do nothing if database already exists")
    @with_appcontext
    def db_create_(if_absent):
        db_create(if_absent)

    @cli.command('db-drop')
    @with_appcontext
    def db_drop_():
        db_drop()

    @cli.command('db-export')
    @click.option('--dst', type=str, required=True, help='export user data file')
    @with_appcontext
    def db_export_(dst):
        db_export(dst)

    @cli.command('db-import')
    @click.option('--src', type=str, required=True, help='configuration source file')
    @with_appcontext
    def db_import_(src):
        db_import(src)

    @cli.command('db-upgrade')
    @with_appcontext
    def db_upgrade_():
        db_upgrade()

    try:
        cli()
    finally:
        als.als_flush()


if __name__ == '__main__':
    sys.exit(main())

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
