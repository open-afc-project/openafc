''' External management of this application.
'''

import logging
from flask_script import Manager, Command, Option, commands
from . import cmd_utils
import ratapi
import os
import time
import shutil
import sqlalchemy
from flask_migrate import MigrateCommand
from . import create_app
from .models.base import db
from .db.generators import shp_to_spatialite, spatialite_to_raster
from prettytable import PrettyTable

LOGGER = logging.getLogger(__name__)


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
        celery_backend = flaskapp.config['CELERY_RESULT_BACKEND'][7:]

        LOGGER.debug('Removing temp files from "%s", "%s"...',
                     task_queue, celery_backend)

        now = time.time()
        # delete file if older than 14 days
        cutoff = now - (14 * 86400)

        logs = os.listdir(task_queue)
        for record in logs:
            t = os.stat(os.path.join(task_queue, record))
            c = t.st_ctime

            if c < cutoff:
                shutil.rmtree(os.path.join(task_queue, record))

        logs = os.listdir(celery_backend)
        for record in logs:
            t = os.stat(os.path.join(celery_backend, record))
            c = t.st_ctime

            if c < cutoff:
                shutil.rmtree(os.path.join(celery_backend, record))


class CleanBlacklistTokens(Command):
    ''' Remove old blacklist tokens
    '''

    # no extra options needed
    option_list = ()

    def __call__(self, flaskapp):
        from datetime import timedelta, datetime
        LOGGER.debug('removing blacklist tokens')

        # delete if older than 30 days
        since = datetime.now() - timedelta(days=30)
        with flaskapp.app_context():
            from .models.aaa import BlacklistToken
            # select * from blacklist_tokens where blacklisted_on < since
            tokens = BlacklistToken.query.filter(
                BlacklistToken.blacklisted_on < since).all()
            for t in tokens:
                LOGGER.debug("removing token: %i", t.id)
                db.session.remove(t)  # pylint: disable=too-many-function-args
            LOGGER.info("%i tokens deleted", len(tokens))
            db.session.commit()  # pylint: disable=no-member


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
        self.add_command('clean-blacklist-tokens', CleanBlacklistTokens())
        self.add_command('rasterize-buildings', RasterizeBuildings())


class DbCreate(Command):
    ''' Create a full new database outside of alembic migrations. '''

    def __call__(self, flaskapp):
        with flaskapp.app_context():
            from .models.aaa import User, Role
            db.create_all()
            get_or_create(db.session, Role, name='Admin')
            get_or_create(db.session, Role, name='Analysis')
            get_or_create(db.session, Role, name='AP')


class UserCreate(Command):
    ''' Create a new user. '''

    option_list = (
        Option('email', type=str,
               help="user's email address"),
        Option('password_in', type=str,
               help="user's password as a readable pipe\nexample: echo 'pass' | rat-manage api user create email /dev/stdin"),
        Option('--role', type=str, default=[], action='append', choices=['Admin', 'Analysis', 'AP'],
               help="role to include with the new user"),
    )

    def __call__(self, flaskapp, email, password_in, role):
        from contextlib import closing
        import datetime
        from .models.aaa import User, Role

        try:
            with closing(open(password_in)) as pwfile:
                password = pwfile.read().strip()
                passhash = flaskapp.user_manager.password_manager.hash_password(
                    password)

            with flaskapp.app_context():
                # select count(*) from aaa_user where email = ?
                if User.query.filter(User.email == email).count() > 0:
                    raise RuntimeError(
                        'Existing user found with email "{0}"'.format(email))

                user = User(
                    email=email,
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
                'Password received was not readable. Enter as a readable pipe.\ni.e. echo "pass" | rat-manage api user create email /dev/stdin')


class UserRemove(Command):
    ''' Remove a user by email. '''

    option_list = (
        Option('email', type=str,
               help="user's email address"),
    )

    def __call__(self, flaskapp, email):
        from .models.aaa import User, Role

        with flaskapp.app_context():
            try:
                # select * from aaa_user where email = ? limit 1
                user = User.query.filter(User.email == email).one()
            except sqlalchemy.orm.exc.NoResultFound:
                raise RuntimeError(
                    'No user found with email "{0}"'.format(email))
            db.session.delete(user)  # pylint: disable=no-member
            db.session.commit()  # pylint: disable=no-member


class UserList(Command):
    '''Lists all users.'''

    def __call__(self, flaskapp):
        table = PrettyTable()
        from .models.aaa import User, UserRole, Role

        table.field_names = ["ID", "Email", "Roles"]
        with flaskapp.app_context():
            user_info = {}
            # select email, name from aaa_user as au join aaa_user_role as aur on au.id = aur.user_id join aaa_role as ar on ar.id = aur.role_id;
            for user, _, role in db.session.query(User, UserRole, Role).filter(User.id == UserRole.user_id).filter(UserRole.role_id == Role.id).all():  # pylint: disable=no-member

                if user.email in user_info:
                    user_info[user.email + ":" +
                              str(user.id)] = user_info[user.email] + ", " + role.name
                else:
                    user_info[user.email + ":" + str(user.id)] = role.name
            for k, v in user_info.items():
                email, _id = k.split(":")
                table.add_row([_id, email, v])
            print(table)


class User(Manager):
    ''' View and manage AAA state '''

    def __init__(self, *args, **kwargs):
        Manager.__init__(self, *args, **kwargs)
        self.add_command('create', UserCreate())
        self.add_command('remove', UserRemove())
        self.add_command('list', UserList())


class AccessPointCreate(Command):
    ''' Create a new user. '''

    option_list = (
        Option('serial', type=str,
               help="access point's serial number"),
        Option('--model', type=str, default=None,
               help="model number"),
        Option('--manuf', type=str, default=None,
               help="manufacturer"),
        Option('email', type=str,
               help="email of user assocated with this access point.")
    )

    def __call__(self, flaskapp, serial, model, manuf, email):
        from contextlib import closing
        import datetime
        from .models.aaa import AccessPoint, User
        with flaskapp.app_context():
            try:
                # select * from aaa_user where email = ? limit 1
                user = User.query.filter(User.email == email).one()
            except sqlalchemy.orm.exc.NoResultFound:
                raise RuntimeError(
                    'No user found with email "{0}"'.format(email))
            # select count(*) from access_point as ap where ap.serial_number = ?
            if AccessPoint.query.filter(AccessPoint.serial_number == serial).count() > 0:
                raise RuntimeError(
                    'Existing access point found with serial number "{0}"'.format(serial))

            ap = AccessPoint(
                serial_number=serial,
                model=model,
                manufacturer=manuf,
                user_id=user.id
            )
            db.session.add(ap)  # pylint: disable=no-member
            db.session.commit()  # pylint: disable=no-member


class AccessPointRemove(Command):
    '''Removes an access point by serial number '''

    option_list = (
        Option('serial', type=str,
               help="Access Point's serial number"),
    )

    def __call__(self, flaskapp, serial):
        from .models.aaa import AccessPoint, User
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


class AccessPointList(Command):
    '''Lists all access points'''

    def __call__(self, flaskapp):
        table = PrettyTable()
        from .models.aaa import AccessPoint, User

        table.field_names = ["Serial Number", "Email"]
        with flaskapp.app_context():
            # select email, serial_number from access_point as ap join aaa_user as au on ap.user_id = au.id;
            for ap, user in db.session.query(AccessPoint, User).filter(User.id == AccessPoint.user_id).all():  # pylint: disable=no-member
                table.add_row([ap.serial_number, user.email])
            print(table)


class AccessPoints(Manager):
    '''View and manage Access Points'''

    def __init__(self, *args, **kwargs):
        Manager.__init__(self, *args, **kwargs)
        self.add_command("create", AccessPointCreate())
        self.add_command("remove", AccessPointRemove())
        self.add_command("list", AccessPointList())


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
    manager.add_command('user', User())
    manager.add_command('data', Data())
    manager.add_command('celery', Celery())
    manager.add_command('ap', AccessPoints())

    manager.run()


if __name__ == '__main__':
    import sys
    sys.exit(main())
