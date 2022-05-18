import logging, os
import contextlib
import shutil
import flask
import datetime
from flask.views import MethodView
from werkzeug import exceptions
from sqlalchemy.exc import IntegrityError
import werkzeug
from ..models import aaa
from .auth import auth
from ..models.base import db

#: Logger for this module
LOGGER = logging.getLogger(__name__)

#: All views under this API blueprint
module = flask.Blueprint('admin', 'admin')

class User(MethodView):
    ''' Administration resources for managing users. '''
    methods = ['POST', 'GET', 'DELETE']

    def get(self, user_id):
        ''' Get User infor with specific user_id. If none is provided, return list of all users. Query parameters used for params. '''

        auth(roles=['Admin'], is_user=(None if user_id == 0 else user_id))

        if (user_id == 0):
            # query users
            # get query params
            users = aaa.User.query.all()
            return flask.jsonify(users=[
                { 
                    'id': u.id,
                    'email': u.email, 
                    'firstName': u.first_name, 
                    'lastName': u.last_name, 
                    'active': u.active, 
                    'roles': [ r.name for r in u.roles ] }
                for u in users
            ])
        else:
            user = aaa.User.query.filter_by(id=user_id).first()
            if user is None:
                raise exceptions.NotFound("User does not exist")
            return flask.jsonify(user=
                { 
                    'id': user.id,
                    'email': user.email, 
                    'firstName': user.first_name, 
                    'lastName': user.last_name, 
                    'active': user.active, 
                    'roles': [ r.name for r in user.roles ],
                }
            )

    def post(self, user_id):
        ''' Update user information. '''

        content = flask.request.json


        user = aaa.User.query.filter_by(id=user_id).first()
        LOGGER.debug("got user: %s", user.email)

        if content.has_key('setProps'):
            auth(roles=['Admin'], is_user=user_id)
            # update all user props
            user_props = content
            user.email = user_props.get('email', user.email)
            if user_props.has_key('email'):
                user.email_confirmed_at = datetime.datetime.now()
            user.active = user_props.get('active', user.active)
            if user_props.has_key('password'):
                pass_hash = flask.current_app.user_manager.password_manager.hash_password(user_props['password'])
                user.password = pass_hash
            db.session.commit() # pylint: disable=no-member

        elif content.has_key('addRole'):
            auth(roles=['Admin'])
            # just add a single role
            role = content.get('addRole')
            LOGGER.debug('Adding role: %s', role)
            # check if user already has role
            if not role in [ r.name for r in user.roles ]:
                # add role
                to_add_role = aaa.Role.query.filter_by(name=role).first()
                user.roles.append(to_add_role)
                db.session.commit() # pylint: disable=no-member

        elif content.has_key('removeRole'):
            auth(roles=['Admin'])
            # just remove a single role
            role = content.get('removeRole')
            LOGGER.debug('Removing role: %s', role)
            # check if user has role
            if role in [ r.name for r in user.roles ]:
                # remove role
                for r in user.roles:
                    if r.name == role:
                        link = aaa.UserRole.query.filter_by(user_id=user.id, role_id=r.id).first()
                        db.session.delete(link) # pylint: disable=no-member
                db.session.commit()# pylint: disable=no-member

        else:
            raise exceptions.BadRequest()


        return flask.make_response()
    
    def delete(self, user_id):
        ''' Remove a user from the system. '''

        auth(roles=['Admin'])

        user = aaa.User.query.filter_by(id=user_id).first()
        user_ap_list = aaa.AccessPoint.query.filter_by(user_id=user.id).all()
        LOGGER.info("Deleting user: %s", user.email)

        # delete the ap's accociated with a user since cascade doesn't seem to be working
        for ap in user_ap_list:
            db.session.delete(ap)
        db.session.delete(user) # pylint: disable=no-member
        db.session.commit() # pylint: disable=no-member

        return flask.make_response()

class AccessPoint(MethodView):
    ''' resources to manage access points'''

    methods = ['PUT', 'GET', 'DELETE']

    def get(self, id):
        ''' Get AP info with specific user_id. '''

        if id == 0:
            auth(roles=['Admin'])
            access_points = aaa.AccessPoint.query.all()
        else:
            auth(roles=['Admin'], is_user=id)
            access_points = aaa.AccessPoint.query.filter_by(user_id=id).all()


        return flask.jsonify(accessPoints=[
            { 
                'id': ap.id,
                'serialNumber': ap.serial_number, 
                'model': ap.model, 
                'manufacturer': ap.manufacturer, 
                'ownerId': ap.user_id,
                'certificationId': ap.certification_id
            }
            for ap in access_points
        ])

    def put(self, id):
        ''' add an AP. '''

        content = flask.request.json

        auth(roles=['Admin'], is_user=id)
        user = aaa.User.query.filter_by(id=id).first()

        try:
            ap = aaa.AccessPoint(content.get('serialNumber'), content.get('model'), content.get('manufacturer'), content.get('certificationId'))

            user.access_points.append(ap)
            db.session.commit() # pylint: disable=no-member

            return flask.jsonify(id=ap.id)
        except IntegrityError:
            raise exceptions.BadRequest("Serial number must be unique")

    
    def delete(self, id):
        ''' Remove an AP from the system. Here the id is the AP id instead of the user_id '''

        ap = aaa.AccessPoint.query.filter_by(id=id).first()

        auth(roles=['Admin'], is_user=ap.user_id)

        LOGGER.info("Deleting ap: %s", ap.id)

        db.session.delete(ap) # pylint: disable=no-member
        db.session.commit() # pylint: disable=no-member

        return flask.make_response()

class AccessPointTrial(MethodView):
    ''' resource to get the access point for getting trial configuration'''

    methods = ['GET']

    def get(self):
      
        ap = aaa.AccessPoint.query.filter_by(serial_number="TestSerialNumber", certification_id = "FCC TestCertificationId").first()


        return flask.jsonify(accessPoint=
            { 
                'id': ap.id,
                'serialNumber': ap.serial_number, 
                'model': ap.model, 
                'manufacturer': ap.manufacturer, 
                'ownerId': ap.user_id,
                'certificationId': ap.certification_id
            }    
        )



class Limits(MethodView): 
    methods = ['PUT', 'GET']
    def put(self):
        ''' set eirp limit '''

        content = flask.request.get_json()
        auth(roles=['Admin'])
        try:
            limits = aaa.Limit.query.filter_by(id=0).first()
            #insert case
            if limits is None:
                if(content == False):
                    raise exceptions.BadRequest("No change")
                limit = aaa.Limit(content)
                db.session.add(limit)
            elif (content == False):
                limits.enforce = False
            else:
                limits.min_eirp = content
                limits.enforce = True
            db.session.commit()
            return flask.jsonify(limit=content) 
        except IntegrityError:
            raise exceptions.BadRequest("DB Error")
    
    def get(self):
        ''' get eirp limit '''
        try:
            limits = aaa.Limit.query.filter_by(id=0).first()      
            return flask.jsonify(limit=float(limits.min_eirp), enforce=limits.enforce)
        except IntegrityError:
            raise exceptions.BadRequest("DB Error")

class AllowedFreqRanges(MethodView): 
    ''' Allows an admin to update the JSON containing the allowed frequency bands and allow any user to view but not edit the file ''' 
    methods = ['PUT', 'GET']
    ACCEPTABLE_FILES = {
        'allowed_frequencies.json': dict(
            content_type='application/json',
        )
    }

    def _open(self, rel_path, mode, user=None):
        ''' Open a configuration file.

        :param rel_path: The specific config name to open.
        :param mode: The file open mode.
        :return: The opened file.
        :rtype: file-like
        '''

        config_path = os.path.join(flask.current_app.config['STATE_ROOT_PATH'], 'frequency_bands')
        if not os.path.exists(config_path):
            os.makedirs(config_path)

        file_path = os.path.join(config_path, rel_path)
        LOGGER.debug('Opening frequncy file "%s"', file_path)
        if not os.path.exists(file_path) and mode != 'wb':
            raise werkzeug.exceptions.NotFound()

        handle = open(file_path, mode)

        if mode == 'wb':
            os.chmod(file_path, 0o666)

        return handle

    def get(self):
        ''' GET method for allowed frequency bands
        '''
        LOGGER.debug('getting admin supplied frequncy bands')
        filename="allowed_frequencies.json"
        if filename not in self.ACCEPTABLE_FILES:
            LOGGER.debug('Could not find allowed_frequencies.json')
            raise werkzeug.exceptions.NotFound()
        filedesc = self.ACCEPTABLE_FILES[filename]

        resp = flask.make_response()
        with self._open('allowed_frequencies.json', 'rb') as conf_file:
            resp.data = conf_file.read()
        resp.content_type = filedesc['content_type']
        return resp

    def put(self, filename="allowed_frequencies.json"):
        ''' PUT method for afc config
        '''
        user_id = auth(roles=['Admin'])
        LOGGER.debug("current user: %s", user_id)
        if filename not in self.ACCEPTABLE_FILES:
            raise werkzeug.exceptions.NotFound()
        filedesc = self.ACCEPTABLE_FILES[filename]
        if flask.request.content_type != filedesc['content_type']:
            raise werkzeug.exceptions.UnsupportedMediaType()

        with contextlib.closing(self._open(filename, 'wb', user_id)) as outfile:
            shutil.copyfileobj(flask.request.stream, outfile)
        return flask.make_response('Allowed frequency ranges updated', 204)


module.add_url_rule('/user/<int:user_id>', view_func=User.as_view('User'))
module.add_url_rule('/user/ap/<int:id>', view_func=AccessPoint.as_view('AccessPoint'))
module.add_url_rule('/user/eirp_min', view_func=Limits.as_view('Eirp'))
module.add_url_rule('/user/frequency_range', view_func=AllowedFreqRanges.as_view('Frequency'))
module.add_url_rule('/user/ap/trial', view_func=AccessPoint.as_view('AccessPointTrial'))