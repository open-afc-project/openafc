#
# This Python file uses the following encoding: utf-8
#
# Portions copyright (C) 2021 Broadcom.
# All rights reserved. The term “Broadcom” refers solely
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
from flask_migrate import MigrateCommand
import werkzeug.exceptions
from . import create_app
from afcmodels.base import db
from afcmodels.hardcoded_relations import RulesetVsRegion
from .db.generators import shp_to_spatialite, spatialite_to_raster
from prettytable import PrettyTable
from flask_script import Manager, Command, Option, commands
from . import cmd_utils
import als

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


class CleanHistory(Command):  # pylint: disable=abstract-method
    ''' Remove old history files
    '''

    # no extra options needed
    option_list = ()

    def __call__(self, flaskapp):  # pylint: disable=signature-differs

        history_dir = flaskapp.config['HISTORY_DIR']
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


class CleanTmpFiles(Command):
    ''' Remove old temporary files that have been orphaned by web clients
    '''

    # no extra options needed
    option_list = ()

    def __call__(self, flaskapp):

        task_queue = flaskapp.config['TASK_QUEUE']
        # the [7:] removes file:// prefix

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


class RasterizeBuildings(Command):
    ''' Convert building shape file into tiff raster
    '''

    option_list = (
        Option('--source', '-s', type=str, dest='source', required=True,
               help="source shape file"),
        Option('--target', '-t', type=str, dest='target', default=None,
               help="target raster file"),
        Option('--layer', '-l', type=str, dest='layer', required=True,
               help="layer name to access polygons from"),
        Option('--attribute', '-a', type=str, dest='attribute', required=True,
               help="attribute name to access height data from"),
    )

    def __call__(self, flaskapp, source, target, layer, attribute):
        from db.generators import shp_to_spatialite, spatialite_to_raster

        if target is None:
            target = os.path.splitext(source)[0]

        if not os.path.exists(source):
            raise RuntimeError(
                '"{}" source file does not exist'.format(source))

        shp_to_spatialite(target + '.sqlite3', source)

        spatialite_to_raster(target + '.tiff', target +
                             '.sqlite3', layer, attribute)


class Data(Manager):
    ''' View and manage data files in RAT '''

    def __init__(self, *args, **kwargs):
        Manager.__init__(self, *args, **kwargs)
        self.add_command('clean-history', CleanHistory())
        self.add_command('clean-tmp-files', CleanTmpFiles())
        self.add_command('rasterize-buildings', RasterizeBuildings())


class DbCreate(Command):
    ''' Create a full new database outside of alembic migrations. '''

    def __call__(self, flaskapp):
        LOGGER.debug('DbCreate.__call__()')
        with flaskapp.app_context():
            from afcmodels.aaa import Role, Ruleset
            from flask_migrate import stamp
            ruleset_list = RulesetVsRegion.ruleset_list()
            db.create_all()
            get_or_create(db.session, Role, name='Admin')
            get_or_create(db.session, Role, name='Super')
            get_or_create(db.session, Role, name='Analysis')
            get_or_create(db.session, Role, name='AP')
            get_or_create(db.session, Role, name='Trial')
            for rule in ruleset_list:
                get_or_create(db.session, Ruleset, name=rule)
            stamp(revision='head')


class DbDrop(Command):
    ''' Create a full new database outside of alembic migrations. '''

    def __call__(self, flaskapp):
        LOGGER.debug('DbDrop.__call__()')
        with flaskapp.app_context():
            from afcmodels.aaa import User, Role
            db.drop_all()


class DbExport(Command):
    ''' Export database in db to a file in json. '''

    option_list = (
        Option('--dst', type=str, help='export user data file'),
    )

    def __init__(self, *args, **kwargs):
        LOGGER.debug('DbExport.__init__()')

    def __call__(self, flaskapp, dst):
        LOGGER.debug('DbExportPrev.__call__()')
        from afcmodels.aaa import User, UserRole, Role, Limit

        filename = dst

        with flaskapp.app_context():
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


class DbImport(Command):
    ''' Import User Database. '''
    option_list = (
        Option('--src', type=str, help='configuration source file'),
    )

    def __init__(self, *args, **kwargs):
        LOGGER.debug('DbImport.__init__()')

    def __call__(self, flaskapp, src):
        LOGGER.debug('DbImport.__call__() %s', src)
        from afcmodels.aaa import User, Limit

        filename = src
        if not os.path.exists(filename):
            raise RuntimeError(
                '"{}" source file does not exist'.format(filename))

        LOGGER.debug('Open admin cfg src file - %s', filename)
        with flaskapp.app_context():
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
                            UserCreate(flaskapp, user_rcrd[0], True)
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


class DbUpgrade(Command):
    ''' Upgrade User Database. '''

    def __init__(self, *args, **kwargs):
        LOGGER.debug('DbUpgrade.__init__()')

    def __call__(self, flaskapp):
        with flaskapp.app_context():
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
                    stamp(revision='4c904e86218d')
                elif 'aaa_user.org does not exist' in str(exception.args):
                    LOGGER.error("upgrade from mtls version")
                    stamp(revision='230b7680b81e')
            db.session.commit()
            upgrade()
            ruleset_list = RulesetVsRegion.ruleset_list()
            for rule in ruleset_list:
                get_or_create(db.session, Ruleset, name=rule)


class UserCreate(Command):
    ''' Create a new user functionality. '''

    option_list = (
        Option('username', type=str,
               help='User name'),
        Option('password_in', type=str,
               help='Users password\n'
                    'example: rat-manage-api'
                    ' user create email password'),
        Option('--role', type=str, default=[], action='append',
               choices=['Admin', 'Super', 'Analysis', 'AP', 'Trial'],
               help='role to include with the new user'),
        Option('--org', type=str, help='Organization'),
        Option('--email', type=str, help='User email'),
    )

    def _create_user(self, flaskapp, id, username, email, password_in, role,
                     hashed, org=None):
        ''' Create user in database. '''
        from contextlib import closing
        import datetime
        from afcmodels.aaa import User, Role, Organization
        LOGGER.debug('UserCreate.__create_user() %s %s %s',
                     email, password_in, role)

        if 'UPGRADE_REQ' in flaskapp.config and flaskapp.config['UPGRADE_REQ']:
            return
        try:
            if not hashed:
                # hash password field in non OIDC mode
                if isinstance(password_in, str):
                    password = password_in.strip()
                else:
                    with closing(open(password_in)) as pwfile:
                        password = pwfile.read().strip()

                if flaskapp.config['OIDC_LOGIN']:
                    # OIDC, password is never stored locally
                    # Still we hash it so that if we switch back
                    # to non OIDC, the hash still match, and can be logged in
                    from passlib.context import CryptContext
                    password_crypt_context = CryptContext(['bcrypt'])
                    passhash = password_crypt_context.encrypt(password_in)
                else:
                    passhash = flaskapp.user_manager.password_manager.hash_password(
                        password)
            else:
                passhash = password_in

            with flaskapp.app_context():
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

                if id:
                    user = User(
                        id=id,
                        username=username,
                        email=email,
                        org=org,
                        email_confirmed_at=datetime.datetime.now(),
                        password=passhash,
                        active=True,
                    )
                else:
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

    def __init__(self, flaskapp=None, user_params=None, hashed=False):
        if flaskapp and isinstance(user_params, dict):
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

            self._create_user(flaskapp,
                              id,
                              user_params['username'],
                              email,
                              user_params['password'],
                              user_params['rolename'],
                              hashed,
                              org)

    def __call__(self, flaskapp, username, password_in, role, hashed=False,
                 org=None, email=None):
        # If command does not provide email.  Populate email field with
        # username
        if not email:
            email = username
        self._create_user(flaskapp, None, username, email, password_in, role,
                          hashed, org)


class UserUpdate(Command):
    ''' Create a new user functionality. '''

    option_list = (
        Option('--email', type=str,
               help='Email'),
        Option('--role', type=str, default=[], action='append',
               choices=['Admin', 'Super', 'Analysis', 'AP', 'Trial'],
               help='role to include with the new user'),
        Option('--org', type=str, help='Organization'),
    )

    def _update_user(self, flaskapp, email, role, org=None):
        ''' Create user in database. '''
        from contextlib import closing
        from afcmodels.aaa import User, Role, Organization

        if 'UPGRADE_REQ' in flaskapp.config and flaskapp.config['UPGRADE_REQ']:
            return

        try:
            with flaskapp.app_context():
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
                        organization = aaa.Organization(name=org)
                        db.session.add(
                            organization)  # pylint: disable=no-member

                    db.session.commit()  # pylint: disable=no-member
                else:
                    raise RuntimeError("User update: User not found")
        except IOError:
            raise RuntimeError("User update encounters unexpected error")

    def __init__(self, flaskapp=None, user_params=None):
        if flaskapp and isinstance(user_params, dict):
            self._update_user(flaskapp,
                              user_params['username'],
                              user_params['rolename'],
                              user_params['org'])

    def __call__(self, flaskapp, email, role, org):
        self._update_user(flaskapp, email, role, org)


class UserRemove(Command):
    ''' Remove a user by email. '''

    option_list = (
        Option('email', type=str,
               help="user's email address"),
    )

    def _remove_user(self, flaskapp, email):
        from afcmodels.aaa import User, Role
        LOGGER.debug('UserRemove._remove_user() %s', email)

        with flaskapp.app_context():
            try:
                # select * from aaa_user where email = ? limit 1
                user = User.query.filter(User.email == email).one()
            except sqlalchemy.orm.exc.NoResultFound:
                raise RuntimeError(
                    'No user found with email "{0}"'.format(email))
            db.session.delete(user)  # pylint: disable=no-member
            db.session.commit()  # pylint: disable=no-member

    def __init__(self, flaskapp=None, email=None):
        if flaskapp and email:
            self._remove_user(flaskapp, email)

    def __call__(self, flaskapp, email):
        self._remove_user(flaskapp, email)


class UserList(Command):
    '''Lists all users.'''

    def __call__(self, flaskapp):
        LOGGER.debug('UserList.__call__()')
        table = PrettyTable()
        from afcmodels.aaa import User, UserRole, Role
        table.field_names = ["ID", "UserName", "Email", "Org", "Roles"]

        if 'UPGRADE_REQ' in flaskapp.config and flaskapp.config['UPGRADE_REQ']:
            return

        with flaskapp.app_context():
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


class User(Manager):
    ''' View and manage AAA state '''

    def __init__(self, *args, **kwargs):
        LOGGER.debug('User.__init__()')
        Manager.__init__(self, *args, **kwargs)
        self.add_command('update', UserUpdate())
        self.add_command('create', UserCreate())
        self.add_command('remove', UserRemove())
        self.add_command('list', UserList())


class AccessPointDenyCreate(Command):
    ''' Create a new access point. '''

    option_list = (
        Option('--serial', type=str, default=None,
               help='serial number of the ap'),
        Option('--cert_id', type=str, default=None, required=True,
               help='certification id of the ap'),
        Option('--ruleset', type=str, default=None, required=True,
               help='ruleset of the ap'),
        Option('--org', type=str, default=None, required=True,
               help='org of the ap'),
    )

    def _create_ap(self, flaskapp, serial, cert_id, ruleset, org):
        from contextlib import closing
        import datetime
        from afcmodels.aaa import AccessPointDeny, Organization, Ruleset
        LOGGER.debug('AccessPointDenyCreate._create_ap() %s %s %s',
                     serial, cert_id, ruleset)
        with flaskapp.app_context():
            if not cert_id or not org or not ruleset:
                raise RuntimeError('Certification, org and ruleset required')

            ruleset = Ruleset.query.filter_by(name=ruleset).first()
            if not ruleset:
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
            ruleset.aps.append(ap)
            db.session.add(ap)
            db.session.commit()

    def __init__(self, flaskapp=None, serial_id=None,
                 cert_id=None, ruleset=None, org=None):
        if flaskapp:
            self._create_ap(flaskapp, str(serial_id), cert_id, ruleset, org)

    def __call__(self, flaskapp, serial=None,
                 cert_id=None, ruleset=None, org=None):
        self._create_ap(flaskapp, serial, cert_id, ruleset, org)


class AccessPointDenyRemove(Command):
    '''Removes an access point by serial number and or certification id'''

    option_list = (
        Option('--serial', type=str, default=None,
               help='Serial number of an Access Point'),
        Option('--cert_id', type=str, default=None,
               help='certification id of the ap'),
    )

    def _remove_ap(self, flaskapp, serial, cert_id):
        from afcmodels.aaa import AccessPointDeny
        LOGGER.debug('AccessPointDenyRemove._remove_ap() %s', serial)
        with flaskapp.app_context():
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

    def __init__(self, flaskapp=None, serial=None, cert_id=None):
        if flaskapp:
            self._remove_ap(flaskapp, serial, cert_id)

    def __call__(self, flaskapp, serial=None, cert_id=None):
        self._remove_ap(flaskapp, serial, cert_id)


class AccessPointDenyList(Command):
    '''Lists all access points'''

    def __call__(self, flaskapp):
        table = PrettyTable()
        from afcmodels.aaa import AccessPointDeny

        table.field_names = ["Serial Number", "Cert ID", "Ruleset", "Org"]
        with flaskapp.app_context():
            for ap in db.session.query(
                    AccessPointDeny).all():  # pylint: disable=no-member
                table.add_row([ap.serial_number, ap.certification_id,
                               ap.ruleset.name, ap.org.name])
            print(table)


class AccessPointsDeny(Manager):
    '''View and manage Access Points Denied'''

    def __init__(self, *args, **kwargs):
        Manager.__init__(self, *args, **kwargs)
        self.add_command("create", AccessPointDenyCreate())
        self.add_command("remove", AccessPointDenyRemove())
        self.add_command("list", AccessPointDenyList())


class CertificationId(Manager):
    '''View and manage CertificationId '''

    def __init__(self, *args, **kwargs):
        Manager.__init__(self, *args, **kwargs)
        self.add_command("create", CertIdCreate())
        self.add_command("remove", CertIdRemove())
        self.add_command("list", CertIdList())
        self.add_command("sweep", CertIdSweep())


class CertIdList(Command):
    '''Lists all access points'''

    def __call__(self, flaskapp):
        table = PrettyTable()
        from afcmodels.aaa import CertId, Organization

        table.field_names = ["Cert ID", "Ruleset", "Loc", "Refreshed"]
        with flaskapp.app_context():
            for cert in db.session.query(
                    CertId).all():  # pylint: disable=no-member
                table.add_row([cert.certification_id, cert.ruleset.name,
                               cert.location, cert.refreshed_at])
            print(table)


class CertIdRemove(Command):
    '''Removes an Certificate Id by certificate id '''

    option_list = (
        Option('--cert_id', type=str,
               help='certificate id',
               required=True),
    )

    def _remove_cert_id(self, flaskapp, cert_id):
        from afcmodels.aaa import CertId
        LOGGER.debug('CertIdRemove._remove_cert_id() %s', cert_id)
        with flaskapp.app_context():
            try:
                cert = CertId.query.filter(
                    CertId.certification_id == cert_id).one()
            except sqlalchemy.orm.exc.NoResultFound:
                raise RuntimeError(
                    'No certificate found with id "{0}"'.format(cert_id))
            db.session.delete(cert)  # pylint: disable=no-member
            db.session.commit()  # pylint: disable=no-member

    def __init__(self, flaskapp=None, cert_id=None):
        if flaskapp:
            self._remove_cert_id(flaskapp, cert_id)

    def __call__(self, flaskapp, cert_id):
        self._remove_cert_id(flaskapp, cert_id)


class CertIdCreate(Command):
    ''' Create a new certification Id. '''
    option_list = (
        Option('--cert_id', type=str,
               help='certification id',
               required=True),
        Option('--ruleset_id', type=str,
               help='ruleset id',
               required=True),
        Option('--location', type=int,
               help="location. 1 indoor - 2 outdoor - 3 both",
               required=True)
    )

    def _create_cert_id(self, flaskapp, cert_id, ruleset_id, location=0):
        from contextlib import closing
        import datetime
        from afcmodels.aaa import CertId, Ruleset, Organization
        LOGGER.debug('CertIdCreate._create_cert_id() %s %s',
                     cert_id, ruleset_id)
        with flaskapp.app_context():
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

    def __init__(self, flaskapp=None, cert_id=None,
                 ruleset_id=None, location=0):
        if flaskapp and cert_id:
            self._create_cert_id(flaskapp, cert_id, ruleset_id=ruleset_id,
                                 location=location)

    def __call__(self, flaskapp, cert_id, ruleset_id=None, location=0):
        self._create_cert_id(flaskapp, cert_id, ruleset_id, location)


class CertIdSweep(Command):
    '''Lists all access points'''
    option_list = (
        Option('--country', type=str,
               help='country e.g. US or CA'),
    )

    def sweep_canada(self, flaskapp):
        import csv
        import requests
        from afcmodels.aaa import CertId, Ruleset
        import datetime
        now = datetime.datetime.now()

        with flaskapp.app_context():
            url = \
                "https://www.ic.gc.ca/engineering/Certified_Standard_Power_Access_Points_6GHz.csv"
            headers = {
                'accept': 'text/html,application/xhtml+xml,application/xml',
                'cache-control': 'max-age=0',
                'content-type': 'application/x-www-form-urlencoded',
                'user-agent': 'rat_server/1.0'
            }
            try:
                with requests.get(url, headers, stream=True) as r:
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
                                location = CertId.OUTDOOR
                            elif code == 111:
                                location = CertId.INDOOR | CertId.OUTDOOR
                            else:
                                location = CertId.UNKNOWN
                            if not location == CertId.UNKNOWN:
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
                self.mailAlert(flaskapp, 'CA', 'none')

    def sweep_fcc_id(self, flaskapp):
        from afcmodels.aaa import CertId
        id_data = "grantee_code=&product_code=&applicant_name=&grant_date_from=&grant_date_to=&comments=&application_purpose=&application_purpose_description=&grant_code_1=&grant_code_2=&grant_code_3=&test_firm=&application_status=&application_status_description=&equipment_class=243&equipment_class_description=6ID-15E+6+GHz+Low+Power+Indoor+Access+Point&lower_frequency=&upper_frequency=&freq_exact_match=on&bandwidth_from=&emission_designator=&tolerance_from=&tolerance_to=&tolerance_exact_match=on&power_output_from=&power_output_to=&power_exact_match=on&rule_part_1=&rule_part_2=&rule_part_3=&rule_part_exact_match=on&product_description=&modular_type_description=&tcb_code=&tcb_code_description=&tcb_scope=&tcb_scope_description=&outputformat=XML&show_records=10&fetchfrom=0&calledFromFrame=N"  # noqa
        self.sweep_fcc_data(flaskapp, id_data, CertId.INDOOR)

    def sweep_fcc_sd(self, flaskapp):
        from afcmodels.aaa import CertId

        sd_data = "grantee_code=&product_code=&applicant_name=&grant_date_from=&grant_date_to=&comments=&application_purpose=&application_purpose_description=&grant_code_1=&grant_code_2=&grant_code_3=&test_firm=&application_status=&application_status_description=&equipment_class=250&equipment_class_description=6SD-15E+6+GHz+Standard+Power+Access+Point&lower_frequency=&upper_frequency=&freq_exact_match=on&bandwidth_from=&emission_designator=&tolerance_from=&tolerance_to=&tolerance_exact_match=on&power_output_from=&power_output_to=&power_exact_match=on&rule_part_1=&rule_part_2=&rule_part_3=&rule_part_exact_match=on&product_description=&modular_type_description=&tcb_code=&tcb_code_description=&tcb_scope=&tcb_scope_description=&outputformat=XML&show_records=10&fetchfrom=0&calledFromFrame=N"  # noqa
        self.sweep_fcc_data(flaskapp, sd_data, CertId.OUTDOOR)

    def sweep_fcc_data(self, flaskapp, data, location):
        from afcmodels.aaa import CertId, Ruleset
        import requests
        import datetime

        now = datetime.datetime.now()
        with flaskapp.app_context():
            url = 'https://apps.fcc.gov/oetcf/eas/reports/GenericSearchResult.cfm?RequestTimeout=500'
            headers = {
                'accept': 'text/html,application/xhtml+xml,application/xml',
                'cache-control': 'max-age=0',
                'content-type': 'application/x-www-form-urlencoded',
                'user-agent': 'rat_server/1.0'
            }

            try:
                resp = requests.post(url, headers=headers, data=data)
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
                            elif location == CertId.OUTDOOR:
                                # add new entries that are in 6SD list.
                                cert = CertId(certification_id=fcc_id,
                                              location=CertId.OUTDOOR)
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
            except BaseException:
                als.als_json_log(
                    'cert_db', {
                        'action': 'sweep', 'country': 'US', 'status': 'failed download'})
                self.mailAlert(flaskapp, 'US', str(location))

    def mailAlert(self, flaskapp, country, other):
        import flask
        from flask_mail import Mail, Message

        with flaskapp.app_context():
            src_email = flask.current_app.config['REGISTRATION_SRC_EMAIL']
            dest_email = flask.current_app.config['REGISTRATION_DEST_PDL_EMAIL']

            mail = Mail(flask.current_app)
            msg = Message(f"AFC CertId download failure",
                          sender=src_email,
                          recipients=[dest_email])
            msg.body = f'''Fail to download CertId for country: {country} info {other}'''
            mail.send(msg)

    def __call__(self, flaskapp, country):
        als.als_json_log('cert_db', {'action': 'sweep', 'country': country,
                                     'status': 'starting'})
        if country == "US":
            # first sweep SD (outdoor) entries, then sweep ID list.
            self.sweep_fcc_sd(flaskapp)
            self.sweep_fcc_id(flaskapp)
        else:
            self.sweep_canada(flaskapp)


class Organization(Manager):
    '''View and manage Organizations '''

    def __init__(self, *args, **kwargs):
        Manager.__init__(self, *args, **kwargs)
        self.add_command("create", OrganizationCreate())
        self.add_command("remove", OrganizationRemove())
        self.add_command("list", OrganizationList())


class OrganizationCreate(Command):
    ''' Create a new Organization. '''

    option_list = (
        Option('--name', type=str, default=None,
               help='Name of Organization',
               required=True),
    )

    def _create_org(self, flaskapp, name):
        from contextlib import closing
        import datetime
        from afcmodels.aaa import Organization
        LOGGER.debug('OrganizationCreate._create_org() %s', name)
        with flaskapp.app_context():
            if name is None:
                raise RuntimeError('Name required')

            org = Organization.query.filter(Organization.
                                            name == name).first()
            if org:
                raise RuntimeError('Duplicate org detected')

            organization = Organization(name=name)
            db.session.add(organization)  # pylint: disable=no-member
            db.session.commit()  # pylint: disable=no-member

    def __init__(self, flaskapp=None, name=None):
        if flaskapp:
            self._create_org(flaskapp, name)

    def __call__(self, flaskapp, name=None):
        self._create_org(flaskapp, name)


class OrganizationRemove(Command):
    '''Removes an access point by serial number and or certification id'''
    option_list = (
        Option('--name', type=str, default=None,
               help='Name of Organization',
               required=True),
    )

    def _remove_org(self, flaskapp, name):
        from afcmodels.aaa import Organization
        LOGGER.debug('Organization._remove_org() %s', name)
        with flaskapp.app_context():
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

    def __init__(self, flaskapp=None, name=None):
        if flaskapp:
            self._remove_org(flaskapp, name)

    def __call__(self, flaskapp, name=None):
        self._remove_org(flaskapp, name)


class OrganizationList(Command):
    '''Lists all access points'''

    def __call__(self, flaskapp):
        table = PrettyTable()
        from afcmodels.aaa import Organization
        table.field_names = ["Name"]
        with flaskapp.app_context():
            for org in db.session.query(
                    Organization).all():  # pylint: disable=no-member
                table.add_row([org.name])
            print(table)


class MTLSCreate(Command):
    ''' Create a new mtls certificate. '''

    option_list = (
        Option('--note', type=str, default=None,
               help="note"),
        Option('--src', type=str, default=None,
               help="certificate file"),
        Option('--org', type=str, default="",
               help="email of user assocated with this access point.")
    )

    def _create_mtls(self, flaskapp, note="", org="", src=None):
        from contextlib import closing
        import datetime
        from afcmodels.aaa import MTLS, User
        LOGGER.debug('MTLS._create_mtls() %s %s %s',
                     note, org, src)
        if not src:
            raise RuntimeError('certificate data file required')

        cert = src
        with flaskapp.app_context():
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

    def __init__(self, flaskapp=None, note="",
                 org="", src=None):
        if flaskapp and cert:
            self._create_mtls(flaskapp, note, org, src)

    def __call__(self, flaskapp, note, org, src):
        self._create_mtls(flaskapp, note, org, src)


class MTLSRemove(Command):
    ''' Remove MTLS certificate by id. '''

    option_list = (
        Option('--id', type=int, default=None,
               help="id"),
    )

    def _remove_mtls(self, flaskapp, id=None):
        from contextlib import closing
        import datetime
        from afcmodels.aaa import MTLS, User
        LOGGER.debug('MTLS._remove_mtls() %d', id)

        if not id:
            raise RuntimeError('mtls id required')

        with flaskapp.app_context():
            try:
                mtls = MTLS.query.filter(MTLS.id == id).one()
            except sqlalchemy.orm.exc.NoResultFound:
                raise RuntimeError(
                    'No mtls certificate found with id"{0}"'.format(id))
            db.session.delete(mtls)  # pylint: disable=no-member
            db.session.commit()  # pylint: disable=no-member

    def __init__(self, flaskapp=None, id=None):
        if flaskapp and id:
            self._remove_mtls(flaskapp, id)

    def __call__(self, flaskapp, id):
        self._remove_mtls(flaskapp, id)


class MTLSList(Command):
    '''Lists all mtls certificates'''

    def __call__(self, flaskapp):
        table = PrettyTable()
        from afcmodels.aaa import MTLS

        table.field_names = ["ID", "Note", "Org", "Create"]
        with flaskapp.app_context():
            for mtls in db.session.query(
                    MTLS).all():  # pylint: disable=no-member
                org = mtls.org if mtls.org else ""
                note = mtls.note if mtls.note else ""
                table.add_row(
                    [mtls.id, mtls.note, mtls.org, str(mtls.created)])
            print(table)


class MTLSDump(Command):
    ''' Remove MTLS certificate by id. '''

    option_list = (
        Option('--id', type=int, default=None,
               help="id"),
        Option('--dst', type=str, default=None,
               help="output file"),
    )

    def _dump_mtls(self, flaskapp, id=None, dst=None):
        from contextlib import closing
        import datetime
        from afcmodels.aaa import MTLS, User
        LOGGER.debug('MTLS._remove_mtls() %d', id)
        if not id:
            raise RuntimeError('mtls id required')
        if not dst:
            raise RuntimeError('output filename required')
        filename = dst

        with flaskapp.app_context():
            try:
                mtls = MTLS.query.filter(MTLS.id == id).one()
            except sqlalchemy.orm.exc.NoResultFound:
                raise RuntimeError(
                    'No mtls certificate found with id"{0}"'.format(id))

            with open(filename, 'w') as fpout:
                fpout.write("%s" % (mtls.cert))

    def __init__(self, flaskapp=None, id=None, dst=None):
        if flaskapp and id and filename:
            self._dump_mtls(flaskapp, id, filename)

    def __call__(self, flaskapp, id, dst):
        self._dump_mtls(flaskapp, id, dst)


class MTLS(Manager):
    '''View and manage Access Points'''

    def __init__(self, *args, **kwargs):
        Manager.__init__(self, *args, **kwargs)
        self.add_command("create", MTLSCreate())
        self.add_command("remove", MTLSRemove())
        self.add_command("list", MTLSList())
        self.add_command("dump", MTLSDump())


class CeleryStatus(Command):  # pylint: disable=abstract-method
    ''' Get status of celery workers '''

    def __call__(self, flaskapp):  # pylint: signature-differs
        import subprocess
        subprocess.call(['celery', 'status'])


class TestCelery(Command):  # pylint: disable=abstract-method
    ''' Run celery task in isolation '''

    option_list = (
        Option('request_type', type=str,
               choices=['PointAnalysis',
                        'ExclusionZoneAnalysis', 'HeatmapAnalysis'],
               help='analysis type'),
        Option('request_file', type=str,
               help='request input file path'),
        Option('afc_config', type=str, help="path to afc config file"),
        Option('--response-file', default=None,
               help='destination for json results'),
        Option('--afc-engine', default=None),
        Option('--user-id', type=int, default=1),
        Option('--username', default='test_user'),
        Option('--response-dir', default=None),
        Option('--temp-dir', default='./test-celery-tmp'),
        Option('--history-dir', default=None),
        Option('--debug', action='store_true', dest='debug'),
    )

    def __call__(
            self,
            flaskapp,
            request_type,
            request_file,
            afc_config,
            response_file,
            user_id,
            username,
            response_dir,
            temp_dir,
            history_dir):
        with flaskapp.app_context():
            from afc_worker import run
            import flask

            if not os.path.exists(temp_dir):
                os.makedirs(temp_dir)

            if response_file is None:
                response_file = os.path.join(
                    temp_dir, request_type + '_response.json')
            if response_dir is None:
                response_dir = os.path.join(
                    flask.current_app.config['NFS_MOUNT_PATH'], 'responses')
            if history_dir is None:
                history_dir = flask.current_app.config['HISTORY_DIR']

            LOGGER.info('Submitting task...')
            task = run.apply_async(args=[
                user_id,
                username,
                flask.current_app.config['STATE_ROOT_PATH'],
                temp_dir,
                request_type,
                request_file,
                afc_config,
                response_file,
                history_dir,
                flask.current_app.config['NFS_MOUNT_PATH'],
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


class Celery(Manager):
    ''' Celery commands '''

    def __init__(self, *args, **kwargs):
        Manager.__init__(self, *args, **kwargs)
        self.add_command('status', CeleryStatus())
        self.add_command('test', TestCelery())


class ConfigAdd(Command):
    ''' Create a new admin configuration. '''
    option_list = (
        Option('src', type=str, help='configuration source file'),
    )

    def __init__(self, *args, **kwargs):
        LOGGER.debug('ConfigAdd.__init__()')

    def __call__(self, flaskapp, src):
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
                    UserCreate(flaskapp, user_rcrd[0], False)
                    f_str = "UserRemove(flaskapp, '" + username[0] + "')"
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
                            OrganizationCreate(flaskapp, org)
                            f_str = "OrganizationRemove(flaskapp, '" + \
                                org + "')"
                            rollback.insert(0, f_str)
                        except BaseException:
                            pass

                        try:
                            # Since this is test, we mark these as both indoor
                            # and indoor certified
                            CertIdCreate(flaskapp, cert_id_str,
                                         ruleset_id_str, loc)
                            f_str = "CertIdRemove(flaskapp, '" + \
                                cert_id_str + "')"
                            rollback.insert(0, f_str)
                        except BaseException:
                            LOGGER.debug(
                                'CertId %s already exists', cert_id_str)

                    with flaskapp.app_context():
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
                        if not config:
                            config = AFCConfig(cfg_rcrd[0])
                            config.config['regionStr'] = config.config['regionStr'].upper(
                            )
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


class ConfigRemove(Command):
    ''' Remove a user by email. '''
    option_list = (
        Option('src', type=str, help='configuration source file'),
    )

    def __init__(self, *args, **kwargs):
        LOGGER.debug('ConfigRemove.__init__()')

    def __call__(self, flaskapp, src):
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
                with flaskapp.app_context():
                    try:
                        user = User.query.filter(
                            User.email == username[0]).one()
                        LOGGER.debug('Found user id %d', user.id)
                        UserRemove(flaskapp, username[0])

                    except RuntimeError:
                        LOGGER.debug('Delete missing user %s', username[0])
                    except Exception as e:
                        LOGGER.debug('Missing user %s in DB', username[0])


class ConfigShow(Command):
    '''Show all configurations.'''
    option_list = (
        Option('src', type=str, help="user's source file"),
    )

    def __init__(self, *args, **kwargs):
        LOGGER.debug('ConfigShow.__init__()')

    def __call__(self, flaskapp, src):
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


class Config(Manager):
    ''' View and manage configuration records '''

    def __init__(self, *args, **kwargs):
        LOGGER.debug('Config.__init__()')
        Manager.__init__(self, *args, **kwargs)
        self.add_command('add', ConfigAdd())
        self.add_command('del', ConfigRemove())
        self.add_command('list', ConfigShow())


def main():

    def appfact(log_level):
        ''' Construct an application based on command parameters. '''
        # Override some parameters for console use
        log_level = log_level.upper()
        conf = dict(
            DEBUG=(log_level == 'DEBUG'),
            PROPAGATE_EXCEPTIONS=(log_level == 'DEBUG'),
            # converts str log_level to int value
            LOG_LEVEL=logging.getLevelName(log_level),
            LOG_HANDLERS=[logging.StreamHandler()]
        )
        return create_app(config_override=conf)

    version_name = cmd_utils.packageversion(__package__)

    manager = Manager(appfact)

    if version_name is not None:
        dispver = '%(prog)s {0}'.format(version_name)
        manager.add_option('--version', action='version',
                           version=dispver)
    manager.add_option('--log-level', dest='log_level', default='info',
                       help='Console logging lowest level displayed.')
    manager.add_command('showurls', commands.ShowUrls())
    manager.add_command('db', MigrateCommand)
    manager.add_command('db-create', DbCreate())
    manager.add_command('db-drop', DbDrop())
    manager.add_command('db-export', DbExport())
    manager.add_command('db-import', DbImport())
    manager.add_command('db-upgrade', DbUpgrade())
    manager.add_command('user', User())
    manager.add_command('mtls', MTLS())
    manager.add_command('data', Data())
    manager.add_command('celery', Celery())
    manager.add_command('ap-deny', AccessPointsDeny())
    manager.add_command('org', Organization())
    manager.add_command('cert_id', CertificationId())
    manager.add_command('cfg', Config())

    try:
        manager.run()
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
