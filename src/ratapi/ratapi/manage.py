#
# This Python file uses the following encoding: utf-8
#
# Portions copyright © 2021 Broadcom.
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
from . import create_app
from .models.base import db
from .db.generators import shp_to_spatialite, spatialite_to_raster
from prettytable import PrettyTable
from flask_script import Manager, Command, Option, commands
from . import cmd_utils
from . import data_if

LOGGER = logging.getLogger(__name__)


def json_lookup(key, json_obj, val):
    """Loookup for key in json and change it value if required"""
    keepit = []

    def lookup(key, json_obj, val, keepit):
        '''lookup for key in json
        '''
        if isinstance(json_obj, dict):
            for k, v in json_obj.items():
                #LOGGER.debug('%s ... %s', k, type(v))
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
            from .models.aaa import Role
            from flask_migrate import stamp
            db.create_all()
            get_or_create(db.session, Role, name='Admin')
            get_or_create(db.session, Role, name='Super')
            get_or_create(db.session, Role, name='Analysis')
            get_or_create(db.session, Role, name='AP')
            get_or_create(db.session, Role, name='Trial')
            stamp(revision='head')

class DbDrop(Command):
    ''' Create a full new database outside of alembic migrations. '''
    def __call__(self, flaskapp):
        LOGGER.debug('DbDrop.__call__()')
        with flaskapp.app_context():
            from .models.aaa import User, Role
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
        from .models.aaa import User, UserRole, Role, AccessPoint, Limit

        filename = dst

        with flaskapp.app_context():
            limit = None
            user_info = {}
            user_cfg = {}
            for user, _, role in db.session.query(User, UserRole,
Role).filter(User.id == UserRole.user_id).filter(UserRole.role_id ==
Role.id).all():  # pylint: disable=no-member
                key = user.email + ":" + str(user.id)
                if key in user_cfg:
                    user_cfg[key]['rolename'].append(role.name)
                else:
                    user_cfg[key] = {
                         'id':user.id,
                         'email':user.email,
                         'password':user.password,
                         'rolename':[role.name],
                         'ap':[]
                    }

                    try:
                        user_cfg[key]['username'] = user.username
                    except:
                        # if old db has no username field, use email field.
                        user_cfg[key]['username'] = user.email

                    try:
                        user_cfg[key]['org'] = user.org
                    except:
                        # if old db has no org field, derive from email field.
                        if '@' in user.email:
                            user_cfg[key]['org'] = user.email[user.email.index('@') + 1:]
                        else: # dummy user - dummy org
                            user_cfg[key]['org'] = ""


            try:
                limits = db.session.query(Limit).filter(id=0).first()
                limit = {'min_eirp':float(limits.min_eirp), 'enforce':bool(limits.enforce)}

            except:
                LOGGER.debug("Error exporting EIRP Limit")

            with open(filename, 'w') as fpout:
                for k, v in user_cfg.items():
                    fpout.write("%s\n" %json.dumps({'userConfig':v}))
                if limit:
                    fpout.write("%s\n" %json.dumps({'Limit':limit}))

def setUserIdNextVal():
    # Set nextval for the sequence so that next user record
    # will not reuse older id.
    cmd = 'select max(id) from aaa_user'
    res = db.session.execute(cmd)
    val = res.fetchone()[0]
    cmd = 'ALTER SEQUENCE aaa_user_id_seq RESTART WITH ' + str(val+1)
    db.session.execute(cmd)

class DbImport(Command):
    ''' Import User Database. '''
    option_list = (
        Option('--src', type=str, help='configuration source file'),
    )

    def __init__(self, *args, **kwargs):
        LOGGER.debug('DbImport.__init__()')

    def __call__(self, flaskapp, src):
        LOGGER.debug('DbImport.__call__() %s', src)
        from .models.aaa import User, Limit

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

                        try:
                            email = username[0]
                            if '@' in email:
                                org = email[email.index('@') + 1:]
                            else:
                                org = ""

                            ap_list = json_lookup('ap', user_rcrd, None)
                            for ap in ap_list[0]:
                                AccessPointCreate(flaskapp, ap['serial_number'],
                                    ap['certification_id'], org)
                        except RuntimeError:
                            LOGGER.debug('AccessPoint %s user %s already exists', ap['serial_number'], username[0])
                    else:
                        limit = json_lookup('Limit', new_rcrd, None)
                        try:
                            limits = db.session.query(Limit).filter_by(id=0).first()
                            #insert case
                            if limits is None:
                                limits = Limit(limit[0]['min_eirp'])
                                db.session.add(limits)
                            elif (limit[0]['enforce'] == False):
                                limits.enforce = False
                            else:
                                limits.min_eirp = limit[0]['min_eirp']
                                limits.enforce = True
                            db.session.commit()
                        except:
                            raise RuntimeError("Can't commit DB for EIRP limits")

                setUserIdNextVal()


class DbUpgrade(Command):
    ''' Upgrade User Database. '''

    def __init__(self, *args, **kwargs):
        LOGGER.debug('DbUpgrade.__init__()')

    def __call__(self, flaskapp):
        with flaskapp.app_context():
            import flask
            from .models.aaa import AFCConfig
            from flask_migrate import (upgrade, stamp)
            setUserIdNextVal()
            try:
                from .models.aaa import User
                user = db.session.query(User).first()  # pylint: disable=no-member
            except Exception as exception:
                if 'aaa_user.username does not exist' in str(exception.args):
                    LOGGER.error("upgrade from preOIDC version")
                    stamp(revision='4c904e86218d')
                elif 'aaa_user.org does not exist' in str(exception.args):
                    LOGGER.error("upgrade from mtls version")
                    stamp(revision='230b7680b81e')

            upgrade()

            # If AFCConfig is empty, copy from fcc config file
            region = 'fcc'
            config = AFCConfig.query.filter(AFCConfig.config['regionStr'].astext == region).first()
            if not config:
                dataif = data_if.DataIf(
                    fsroot=flask.current_app.config['STATE_ROOT_PATH'])
                with dataif.open("cfg", region + "/afc_config.json") as hfile:
                    if hfile.head():
                        config_bytes = hfile.read()
                        rcrd = json.loads(config_bytes)
                        config = AFCConfig(rcrd)
                        db.session.add(config)
                        db.session.commit()


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
    )

    def _create_user(self, flaskapp, id, username, email, password_in, role,
hashed, org = None):
        ''' Create user in database. '''
        from contextlib import closing
        import datetime
        from .models.aaa import User, Role
        LOGGER.debug('UserCreate.__create_user() %s %s %s',
                     email, password_in, role)

        if 'UPGRADE_REQ' in flaskapp.config and flaskapp.config['UPGRADE_REQ']:
            return
        try:
            if not hashed:
                # hash password field in non OIDC mode
                if isinstance(password_in, (str, unicode)):
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
                    except:
                        org = ""

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
org=None):
        # command does not provide email.  Populate email field with username
        self._create_user(flaskapp, None, username, username, password_in, role,
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

    def _update_user(self, flaskapp, email, role, org = None):
        ''' Create user in database. '''
        from contextlib import closing
        from .models.aaa import User, Role

        if 'UPGRADE_REQ' in flaskapp.config and flaskapp.config['UPGRADE_REQ']:
            return

        try:
            with flaskapp.app_context():
                user = User.getemail(email)

                if user:
                    user.roles = []
                    for rolename in role:
                        if not rolename in user.roles:
                            user.roles.append(get_or_create(
                                db.session, Role, name=rolename))
                    user.active = True,
                    if org:
                        user.org = org;
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
        from .models.aaa import User, Role
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
        from .models.aaa import User, UserRole, Role
        table.field_names = ["ID", "UserName", "Email", "Org", "Roles"]
 
        if 'UPGRADE_REQ' in flaskapp.config and flaskapp.config['UPGRADE_REQ']:
            return

        with flaskapp.app_context():
            user_info = {}
            # select email, name from aaa_user as au join aaa_user_role as aur
            # on au.id = aur.user_id join aaa_role as ar on ar.id = aur.role_id;
            for user, _, role in db.session.query(User, UserRole,
Role).filter(User.id == UserRole.user_id).filter(UserRole.role_id ==
Role.id).all():  # pylint: disable=no-member

                key = user.email + ":" + user.org + ":" + str(user.id) + ":" + user.username

                if key in user_info:
                    user_info[key] = user_info[key] + ", " + role.name
                else:
                    user_info[key] = role.name

            # Find all users without roles and show them last
            for user in  db.session.query(User).filter(~User.roles.any()).all():
                key = user.email + ":" + user.org + ":" + str(user.id) + ":" + user.username
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


class AccessPointCreate(Command):
    ''' Create a new access point. '''

    option_list = (
        Option('serial', type=str,
               help='serial number of the ap'),
        Option('cert_id', type=str,
               help='certification id of the ap'),
        Option('--model', type=str, default=None,
               help="model number"),
        Option('--manuf', type=str, default=None,
               help="manufacturer"),
        Option('--org', type=str,
               help="organization with this access point.")
    )

    def _create_ap(self, flaskapp, serial, cert_id,
                   model=None, manuf=None, org=None):
        from contextlib import closing
        import datetime
        from .models.aaa import AccessPoint
        LOGGER.debug('AccessPointCreate._create_ap() %s %s %s',
                      serial, cert_id, org)
        with flaskapp.app_context():
            if AccessPoint.query.filter(AccessPoint.serial_number ==
serial).filter(AccessPoint.certification_id==cert_id).count() > 0:
                raise RuntimeError(
                    'Existing access point found with serial number "{0}"'.format(serial))

            if not org: org = ""

            ap = AccessPoint(
                serial_number=serial,
                model=model,
                manufacturer=manuf,
                certification_id=cert_id,
                org=org
            )
            db.session.add(ap)  # pylint: disable=no-member
            db.session.commit()  # pylint: disable=no-member

    def __init__(self, flaskapp=None, serial_id=None,
                 cert_id=None, org=None):
        if flaskapp and serial_id:
            self._create_ap(flaskapp, str(serial_id), cert_id, org=org)

    def __call__(self, flaskapp, serial, cert_id, model, manuf, org=None):
        self._create_ap(flaskapp, serial, cert_id, model, manuf, org)


class AccessPointRemove(Command):
    '''Removes an access point by serial number '''

    option_list = (
        Option('serial', type=str,
               help='Serial number of an Access Point'),
    )

    def _remove_ap(self, flaskapp, serial):
        from .models.aaa import AccessPoint
        LOGGER.debug('AccessPointRemove._remove_ap() %s', serial)
        with flaskapp.app_context():
            try:
                # select * from access_point as ap where ap.serial_number = ?
                ap = AccessPoint.query.filter(
                    AccessPoint.serial_number == serial).one()
            except sqlalchemy.orm.exc.NoResultFound:
                raise RuntimeError(
                    'No access point found with serial number "{0}"'.format(serial))
            db.session.delete(ap)  # pylint: disable=no-member
            db.session.commit()  # pylint: disable=no-member

    def __init__(self, flaskapp=None, serial=None):
        if flaskapp and serial:
            self._remove_ap(flaskapp, serial)

    def __call__(self, flaskapp, serial):
        self._remove_ap(flaskapp, serial)


class AccessPointList(Command):
    '''Lists all access points'''

    def __call__(self, flaskapp):
        table = PrettyTable()
        from .models.aaa import AccessPoint

        table.field_names = ["Serial Number", "Cert ID", "Org"]
        with flaskapp.app_context():
            for ap in db.session.query(AccessPoint).all():  # pylint: disable=no-member
                table.add_row([ap.serial_number, ap.certification_id, ap.org])
            print(table)


class AccessPoints(Manager):
    '''View and manage Access Points'''

    def __init__(self, *args, **kwargs):
        Manager.__init__(self, *args, **kwargs)
        self.add_command("create", AccessPointCreate())
        self.add_command("remove", AccessPointRemove())
        self.add_command("list", AccessPointList())

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
        from .models.aaa import MTLS, User
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
        from .models.aaa import MTLS, User
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
        from .models.aaa import MTLS

        table.field_names = ["ID", "Note", "Org", "Create"]
        with flaskapp.app_context():
            for mtls in db.session.query(MTLS).all():  # pylint: disable=no-member
                org = mtls.org if mtls.org else ""
                note = mtls.note if mtls.note else ""
                table.add_row([mtls.id, mtls.note, mtls.org , str(mtls.created)])
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
        from .models.aaa import MTLS, User
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
                fpout.write("%s" %(mtls.cert));

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

    def __call__(self, flaskapp, request_type, request_file, afc_config, response_file,
                 afc_engine, user_id, username, response_dir, temp_dir, history_dir, debug):
        with flaskapp.app_context():
            from ratapi.tasks.afc_worker import run
            import flask

            if not os.path.exists(temp_dir):
                os.makedirs(temp_dir)

            if afc_engine is None:
                afc_engine = flask.current_app.config['AFC_ENGINE']
            if response_file is None:
                response_file = os.path.join(
                    temp_dir, request_type + '_response.json')
            if response_dir is None:
                response_dir = os.path.join(flask.current_app.config['STATE_ROOT_PATH'],
                                            'responses')
            if history_dir is None:
                history_dir = flask.current_app.config['HISTORY_DIR']

            LOGGER.info('Submitting task...')
            task = run.apply_async(args=[
                user_id,
                username,
                afc_engine,
                flask.current_app.config['STATE_ROOT_PATH'],
                temp_dir,
                request_type,
                request_file,
                afc_config,
                response_file,
                history_dir,
                debug,
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
        from .models.aaa import User

        REGION_MAP = {'CONUS':'fcc'}
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
                    cert_id = json_lookup('certificationId', ap_rcrd, None)
                    for i in range(len(serial_id)):
                        cert_str = cert_id[i][0]['nra'] + ' ' + cert_id[i][0]['id']
                        email = username[0]
                        if '@' in email:
                            org = email[email.index('@') + 1:]
                        else:
                            org = ""

                        AccessPointCreate(flaskapp, serial_id[i],
                                          cert_str, org)
                        f_str = "AccessPointRemove(flaskapp, '" + serial_id[i] + "')"
                        rollback.insert(0, f_str)

                    with flaskapp.app_context():
                        user = User.query.filter(User.email == username[0]).one()
                        LOGGER.debug('New user id %d', user.id)

                    cfg_rcrd = json_lookup('afcConfig', new_rcrd, None)
                    region_rcrd = json_lookup('regionStr', cfg_rcrd, None)
                    regionStr = REGION_MAP[region_rcrd[0]]

                    with flaskapp.app_context():
                        import flask

                        dataif = data_if.DataIf(
                            fsroot=flask.current_app.config['STATE_ROOT_PATH'])
                        with dataif.open("cfg", regionStr + "/afc_config.json") as hfile:
                            LOGGER.debug('Opening config file "%s"',
                                         dataif.rname("cfg", regionStr + "/afc_config.json"))
                            hfile.write(json.dumps(cfg_rcrd[0]))
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
        from .models.aaa import User

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
                serial_id = json_lookup('serialNumber', ap_rcrd, None)
                for i in range(len(serial_id)):
                    try:
                        AccessPointRemove(flaskapp, serial_id[i])
                    except RuntimeError:
                        LOGGER.debug('AP %s not found', serial_id[i])

                user_rcrd = json_lookup('userConfig', new_rcrd, None)
                username = json_lookup('username', user_rcrd, None)
                with flaskapp.app_context():
                    try:
                        user = User.query.filter(User.email == username[0]).one()
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
    manager.add_command('ap', AccessPoints())
    manager.add_command('cfg', Config())

    manager.run()

if __name__ == '__main__':
    sys.exit(main())

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
