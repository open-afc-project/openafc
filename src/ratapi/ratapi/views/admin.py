# This Python file uses the following encoding: utf-8
#
# Portions copyright © 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate
# affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy
# of which is included with this software program.
#

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
from .ratapi import nraToRegionStr

#: Logger for this module
LOGGER = logging.getLogger(__name__)

#: All views under this API blueprint
module = flask.Blueprint('admin', 'admin')

class User(MethodView):
    ''' Administration resources for managing users. '''
    methods = ['POST', 'GET', 'DELETE']

    def get(self, user_id):
        ''' Get User infor with specific user_id. If none is provided,
            return list of all users. Query parameters used for params. '''

        id = auth(roles=['Admin'], is_user=(None if user_id == 0 else user_id))
        if (user_id == 0):
            # check if we limit to org or query all
            cur_user = aaa.User.query.filter_by(id=id).first()
            roles = [r.name for r in cur_user.roles]

            LOGGER.debug("USER got user: %s org %s roles %s",
                         cur_user.email, cur_user.org, str(cur_user.roles))
            if "Super" in roles:
                users = aaa.User.query.all()
            else:
                org = cur_user.org if cur_user.org else ""
                users = aaa.User.query.filter_by(org=org).all()

            return flask.jsonify(users=[
                {
                    'id': u.id,
                    'email': u.email,
                    'org':  u.org if u.org else "",
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
                    'org': user.org if user.org else "",
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
        org = user.org if user.org else ""

        if 'setProps' in content:
            auth(roles=['Admin'], is_user=user_id)
            # update all user props
            user_props = content
            user.email = user_props.get('email', user.email)
            if 'email' in user_props:
                user.email_confirmed_at = datetime.datetime.now()
            user.active = user_props.get('active', user.active)
            if 'password' in user_props:
                if flask.current_app.config['OIDC_LOGIN']:
                    # OIDC, password is never stored locally
                    # Still we hash it so that if we switch back
                    # to non OIDC, the hash still match, and can be logged in
                    from passlib.context import CryptContext
                    password_crypt_context = CryptContext(['bcrypt'])
                    pass_hash = password_crypt_context.encrypt(password_in)
                else:
                    pass_hash = flask.current_app.user_manager.password_manager.hash_password(user_props['password'])
                user.password = pass_hash
            db.session.commit() # pylint: disable=no-member

        elif 'addRole' in content:
            # just add a single role
            role = content.get('addRole')
            if role == "Super":
                auth(roles=['Super'], org=org)
            else:
                auth(roles=['Admin'], org=org)
            LOGGER.debug('Adding role: %s', role)
            # check if user already has role
            if not role in [ r.name for r in user.roles ]:
                # add role
                to_add_role = aaa.Role.query.filter_by(name=role).first()
                user.roles.append(to_add_role)
                # When adding Super role, add Admin role too
                if role=="Super" and not "Admin" in [ r.name for r in user.roles ]:
                    to_add_role = aaa.Role.query.filter_by(name="Admin").first()
                    user.roles.append(to_add_role)
                db.session.commit() # pylint: disable=no-member

        elif 'removeRole' in content:
            # just remove a single role
            role = content.get('removeRole')
            if role=="Super":
                # only super user can remove someone elses super role
                auth(roles=['Super'])
            else:
                auth(roles=['Admin'], org=org)

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

        user = aaa.User.query.filter_by(id=user_id).first()
        org = user.org if user.org else ""
        auth(roles=['Admin'], org=org)
        db.session.delete(user) # pylint: disable=no-member
        db.session.commit() # pylint: disable=no-member

        return flask.make_response()

class AccessPoint(MethodView):
    ''' resources to manage access points'''

    methods = ['PUT', 'GET', 'DELETE']

    def get(self, id):
        ''' Get AP info with specific user_id. '''
        id = auth(roles=['Admin', 'AP'])
        user = aaa.User.query.filter_by(id=id).first()
        roles = [r.name for r in user.roles]
        if "Super" in roles:
            # Super user gets all access points
            access_points = db.session.query(aaa.AccessPoint).all()
        else:
            # Admin only user gets all access points within own org
            org = user.org if user.org else ""
            access_points = []
            for ap in db.session.query(aaa.AccessPoint).filter(aaa.AccessPoint.org == org).all():
                access_points.append(ap)

        return flask.jsonify(accessPoints=[
            {
                'id': ap.id,
                'serialNumber': ap.serial_number,
                'model': ap.model,
                'manufacturer': ap.manufacturer,
                'certificationId': ap.certification_id,
                'org': ap.org
            }
            for ap in access_points
        ])

    def put(self, id):
        ''' add an AP. '''

        content = flask.request.json
        user = aaa.User.query.filter_by(id=id).first()
        org = content.get('org')
        auth(roles=['Admin'], org=org)
        certId = content.get('certificationId')
        nra = certId.split()[0]
        # check nra is valid
        region = nraToRegionStr(nra)

        try:
            ap = aaa.AccessPoint(content.get('serialNumber'), content.get('model'), content.get('manufacturer'), content.get('certificationId'), org)

            db.session.add(ap) # pylint: disable=no-member
            db.session.commit() # pylint: disable=no-member

            return flask.jsonify(id=ap.id)
        except IntegrityError:
            raise exceptions.BadRequest("Serial number must be unique")


    def delete(self, id):
        ''' Remove an AP from the system. Here the id is the AP id instead of the user_id '''

        ap = aaa.AccessPoint.query.filter_by(id=id).first()

        auth(roles=['Admin'], org=ap.org)

        LOGGER.info("Deleting ap: %s", id)

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
                'certificationId': ap.certification_id,
                'org': ap.org
            }
        )



class Limits(MethodView):
    methods = ['PUT', 'GET']
    def put(self):
        ''' set eirp limit '''

        content = flask.request.get_json()
        auth(roles=['Super'])
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

        config_path = os.path.join(flask.current_app.config['NFS_MOUNT_PATH'], 'rat_transfer', 'frequency_bands')
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
        user_id = auth(roles=['Super'])
        LOGGER.debug("current user: %s", user_id)
        if filename not in self.ACCEPTABLE_FILES:
            raise werkzeug.exceptions.NotFound()
        filedesc = self.ACCEPTABLE_FILES[filename]
        if flask.request.content_type != filedesc['content_type']:
            raise werkzeug.exceptions.UnsupportedMediaType()

        with contextlib.closing(self._open(filename, 'wb', user_id)) as outfile:
            shutil.copyfileobj(flask.request.stream, outfile)
        return flask.make_response('Allowed frequency ranges updated', 204)

class MTLS(MethodView):
    ''' resources to manage mtls certificates '''

    methods = ['POST', 'GET', 'DELETE']

    def get(self, id):
        ''' Get MTLS info with specific user_id. '''

        if id == 0:
            user_id = auth(roles=['Admin'])
            user = aaa.User.query.filter_by(id=user_id).first()
            roles = [r.name for r in user.roles]
            if "Super" in roles:
                mtls_list = aaa.MTLS.query.all()
            else:
                # Admin user gets certificates for his/her own org
                org = user.org if user.org else ""
                mtls_list = db.session.query(aaa.MTLS).filter(aaa.MTLS.org == org).all()
        else:
            raise werkzeug.exceptions.NotFound()

        return flask.jsonify(mtls=[
            {
                'id': mtls.id,
                'cert': mtls.cert,
                'note': mtls.note if mtls.note else "",
                'org': mtls.org if mtls.org else "",
                'created': str(mtls.created),
            }
            for mtls in mtls_list
        ])

    def post(self, id):
        ''' add an mtls cert by a user id. '''

        content = flask.request.json
        org = content.get('org')
        auth(roles=['Admin'], org=org)

        LOGGER.debug("PUT mtls: %s org: %s", str(id), org)

        try:
            # check if certificate is already there.
            cert = content.get('cert')
            LOGGER.info("PUT mtls: %s org: %s", str(id), org)
            try:
                import base64
                strip_chars = 'base64,'
                index = cert.index(strip_chars)
                cert = base64.b64decode(cert[index + len(strip_chars):])
            except:
                LOGGER.error("PUT mtls: %s org: %s exception", str(id), org)
                raise exceptions.BadRequest("Unexpected format of certificate")

            mtls = aaa.MTLS(cert, content.get('note'), org)
            db.session.add(mtls)
            db.session.commit() # pylint: disable=no-member
            return flask.jsonify(id=mtls.id)
        except IntegrityError:
            LOGGER.error("PUT mtls: %s org: %s exception", str(id), org)
            raise exceptions.BadRequest("Unable to add certificate")


    def delete(self, id):
        ''' Remove an mtls cert from the system. Here the id is the mtls cert id instead of the user_id '''

        mtls = aaa.MTLS.query.filter_by(id=id).first()
        user_id = auth(roles=['Admin'], org=mtls.org)
        user = aaa.User.query.filter_by(id=user_id).first()
        LOGGER.info("Deleting mtls: %s", str(mtls.id))

        db.session.delete(mtls) # pylint: disable=no-member
        db.session.commit() # pylint: disable=no-member

        return flask.make_response()

module.add_url_rule('/user/<int:user_id>', view_func=User.as_view('User'))
module.add_url_rule('/user/ap/<int:id>', view_func=AccessPoint.as_view('AccessPoint'))
module.add_url_rule('/user/eirp_min', view_func=Limits.as_view('Eirp'))
module.add_url_rule('/user/frequency_range', view_func=AllowedFreqRanges.as_view('Frequency'))
module.add_url_rule('/user/ap/trial', view_func=AccessPoint.as_view('AccessPointTrial'))
module.add_url_rule('/user/mtls/<int:id>', view_func=MTLS.as_view('MTLS'))
