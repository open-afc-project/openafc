import logging
import flask
import datetime
from flask.views import MethodView
from werkzeug import exceptions
from sqlalchemy.exc import IntegrityError
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

module.add_url_rule('/user/<int:user_id>', view_func=User.as_view('User'))
module.add_url_rule('/user/ap/<int:id>', view_func=AccessPoint.as_view('AccessPoint'))
module.add_url_rule('/user/eirp_min', view_func=Limits.as_view('Eirp'))
