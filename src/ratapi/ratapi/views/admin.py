# This Python file uses the following encoding: utf-8
#
# Portions copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate
# affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy
# of which is included with this software program.
#

import json
import logging
import os
import inspect
import contextlib
import shutil
import flask
import datetime
from flask.views import MethodView
from fst import DataIf
from ncli import MsgPublisher
from werkzeug import exceptions
from sqlalchemy.exc import IntegrityError
import werkzeug
import afcmodels.aaa as aaa
from afcmodels.hardcoded_relations import RulesetVsRegion
from .auth import auth
from afcmodels.base import db

#: Logger for this module
LOGGER = logging.getLogger(__name__)

#: All views under this API blueprint
module = flask.Blueprint("admin", "admin")

dbg_msg = "inspect.getframeinfo(inspect.currentframe().f_back)[2]"


class User(MethodView):
    """Administration resources for managing users."""

    methods = ["POST", "GET", "DELETE"]

    def get(self, user_id):
        """Get User infor with specific user_id. If none is provided,
        return list of all users. Query parameters used for params."""

        id = auth(roles=["Admin"], is_user=(None if user_id == 0 else user_id))
        if user_id == 0:
            # check if we limit to org or query all
            cur_user = aaa.User.query.filter_by(id=id).first()
            roles = [r.name for r in cur_user.roles]

            LOGGER.debug(
                "USER got user: %s org %s roles %s",
                cur_user.email,
                cur_user.org,
                str(cur_user.roles),
            )
            if "Super" in roles:
                users = aaa.User.query.all()
            else:
                org = cur_user.org if cur_user.org else ""
                users = aaa.User.query.filter_by(org=org).all()

            return flask.jsonify(
                users=[
                    {
                        "id": u.id,
                        "email": u.email,
                        "org": u.org if u.org else "",
                        "firstName": u.first_name,
                        "lastName": u.last_name,
                        "active": u.active,
                        "roles": [r.name for r in u.roles],
                    }
                    for u in users
                ]
            )
        else:
            user = aaa.User.query.filter_by(id=user_id).first()
            if user is None:
                raise exceptions.NotFound("User does not exist")
            return flask.jsonify(
                user={
                    "id": user.id,
                    "email": user.email,
                    "org": user.org if user.org else "",
                    "firstName": user.first_name,
                    "lastName": user.last_name,
                    "active": user.active,
                    "roles": [r.name for r in user.roles],
                }
            )

    def post(self, user_id):
        """Update user information."""

        content = flask.request.json
        user = aaa.User.query.filter_by(id=user_id).first()
        LOGGER.debug("got user: %s", user.email)
        org = user.org if user.org else ""

        if "setProps" in content:
            auth(roles=["Admin"], is_user=user_id)
            # update all user props
            user_props = content
            user.email = user_props.get("email", user.email)
            if "email" in user_props:
                user.email_confirmed_at = datetime.datetime.now()
            user.active = user_props.get("active", user.active)
            if "password" in user_props:
                if flask.current_app.config["OIDC_LOGIN"]:
                    # OIDC, password is never stored locally
                    # Still we hash it so that if we switch back
                    # to non OIDC, the hash still match, and can be logged in
                    from passlib.context import CryptContext

                    password_crypt_context = CryptContext(["bcrypt"])
                    pass_hash = password_crypt_context.encrypt(password_in)
                else:
                    pass_hash = (
                        flask.current_app.user_manager.password_manager.hash_password(
                            user_props["password"]))
                user.password = pass_hash
            db.session.commit()  # pylint: disable=no-member

        elif "addRole" in content:
            # just add a single role
            role = content.get("addRole")
            if role == "Super":
                auth(roles=["Super"], org=org)
            else:
                auth(roles=["Admin"], org=org)
            LOGGER.debug("Adding role: %s", role)
            # check if user already has role
            if role not in [r.name for r in user.roles]:
                # add role
                to_add_role = aaa.Role.query.filter_by(name=role).first()
                user.roles.append(to_add_role)
                # When adding Super role, add Admin role too
                if role == "Super" and "Admin" not in [
                        r.name for r in user.roles]:
                    to_add_role = aaa.Role.query.filter_by(
                        name="Admin").first()
                    user.roles.append(to_add_role)
                db.session.commit()  # pylint: disable=no-member

        elif "removeRole" in content:
            # just remove a single role
            role = content.get("removeRole")
            if role == "Super":
                # only super user can remove someone elses super role
                auth(roles=["Super"])
            else:
                auth(roles=["Admin"], org=org)

            LOGGER.debug("Removing role: %s", role)
            # check if user has role
            if role in [r.name for r in user.roles]:
                # remove role
                for r in user.roles:
                    if r.name == role:
                        link = aaa.UserRole.query.filter_by(
                            user_id=user.id, role_id=r.id
                        ).first()
                        db.session.delete(link)  # pylint: disable=no-member
                db.session.commit()  # pylint: disable=no-member

        else:
            raise exceptions.BadRequest()

        return flask.make_response()

    def delete(self, user_id):
        """Remove a user from the system."""

        user = aaa.User.query.filter_by(id=user_id).first()
        org = user.org if user.org else ""
        auth(roles=["Admin"], org=org)
        db.session.delete(user)  # pylint: disable=no-member
        db.session.commit()  # pylint: disable=no-member

        return flask.make_response()


class AccessPointDeny(MethodView):
    """resources to manage access points"""

    methods = ["PUT", "GET", "DELETE", "POST"]

    def get(self, id):
        """Get APs info with an org"""
        id = auth(roles=["Admin", "AP"])

        user = aaa.User.query.filter_by(id=id).first()
        roles = [r.name for r in user.roles]
        if "Super" in roles:
            # Super user gets all access points
            access_points = db.session.query(aaa.AccessPointDeny).all()
        else:
            # Admin only user gets all access points within own org
            org = user.org if user.org else ""
            organization = (
                db.session.query(aaa.Organization)
                .filter(aaa.Organization.name == org)
                .first()
            )
            if organization:
                access_points = organization.aps

        # translate ruleset.id to index into rulesets
        rules = RulesetVsRegion.ruleset_list()
        rule_map = {}
        for idx, rule in enumerate(rules):
            r = db.session.query(aaa.Ruleset).filter(
                aaa.Ruleset.name == rule).first()
            if r:
                rule_map[r.id] = idx

        return flask.jsonify(
            rulesets=RulesetVsRegion.ruleset_list(),
            access_points={
                "data": [
                    "{},{},{},{}".format(
                        ap.serial_number,
                        ap.certification_id,
                        rule_map[ap.ruleset_id],
                        ap.org.name,
                    )
                    for ap in access_points
                ]
            },
        )

    def put(self, id):
        """add an AP."""

        content = flask.request.json
        user = aaa.User.query.filter_by(id=id).first()
        org = content.get("org")
        auth(roles=["Admin"], org=org)

        serial = content.get("serialNumber")
        if serial == "*" or not serial:
            # match all
            serial = None

        cert_id = content.get("certificationId")
        if not cert_id:
            raise exceptions.BadRequest("Certification Id required")

        rulesetId = content.get("rulesetId")
        if not rulesetId:
            raise exceptions.BadRequest("Ruleset Id required")

        ap = (
            aaa.AccessPointDeny.query.filter_by(certification_id=cert_id)
            .filter_by(serial_number=serial)
            .first()
        )
        if ap:
            # We detect existing entry
            raise exceptions.BadRequest("Duplicate device detected")

        organization = aaa.Organization.query.filter_by(name=org).first()
        if not organization:
            raise exceptions.BadRequest("Organization does not exist")

        ruleset = aaa.Ruleset.query.filter_by(name=rulesetId).first()
        if not ruleset:
            raise exceptions.BadRequest("Ruleset does not exist")

        ap = aaa.AccessPointDeny(serial, cert_id)
        organization.aps.append(ap)
        ruleset.aps.append(ap)
        db.session.add(ap)  # pylint: disable=no-member
        db.session.commit()  # pylint: disable=no-member

        return flask.jsonify(id=ap.id)

    def post(self, id):
        """replace list of APs."""

        import json

        content = flask.request.json
        user = aaa.User.query.filter_by(id=id).first()
        user_org = user.org
        auth(roles=["Admin"])

        # format as below.  The organization (e.g. my_org) is optional, and if not listed will default
        # to the org of the logged in user.
        # e.g.
        # {'rulesets':['US_47_CFR_PART_15_SUBPART_E', 'CA_RES_DBS-06'],
        #  'access_points': {
        #    'data': ['serial1, cert1, 0, my_org',
        #             'serial2, cert2, 0']
        #    }
        # }

        roles = [r.name for r in user.roles]
        if "Super" in roles:
            # delete, replace whole list
            aaa.AccessPointDeny.query.delete()
        else:
            # delete, replace list belonging to the admin's org
            organization = aaa.Organization.query.filter_by(
                name=user_org).first()
            aaa.AccessPointDeny.query.filter_by(
                org_id=organization.id).delete()

        payload = content.get("accessPoints")
        rcrd = json.loads(payload)
        ruleset_list = rcrd["rulesets"]
        access_points = rcrd["access_points"]["data"]
        rows = list(map(lambda a: a.strip("\r").split(","), access_points))

        for row in rows:
            if len(row) < 3:
                raise exceptions.BadRequest(
                    "serial number, cert id, ruleset are required"
                )

            org = user_org
            serial = row[0].strip()
            if serial == "*" or serial == "None" or not serial:
                # match all
                serial = None

            cert_id = row[1].strip()
            try:
                ruleset_id = ruleset_list[int(row[2].strip())]
                ruleset = aaa.Ruleset.query.filter_by(name=ruleset_id).first()
                if not ruleset:
                    raise exceptions.BadRequest(
                        "ruleset {} does not exist".format(ruleset_id)
                    )
            except BaseException:
                raise exceptions.BadRequest("ruleset does not exist")

            if len(row) > 3:  # this column is to override the AP org if user is Super
                if not "".join(row):  # ignore empty row
                    continue

                org_override = row[3].strip().lower()
                if "Super" in [r.name for r in user.roles]:
                    org = org_override
                elif user_org != org_override:
                    # can't override org
                    raise exceptions.BadRequest(
                        "organization {} not accessible".format(org_override)
                    )
            ap = (
                aaa.AccessPointDeny.query.filter_by(certification_id=cert_id)
                .filter_by(serial_number=serial)
                .first()
            )
            if not ap:
                organization = aaa.Organization.query.filter_by(
                    name=org).first()
                if not organization:
                    raise exceptions.BadRequest(
                        "organization {} does not exist".format(org)
                    )

                ap = aaa.AccessPointDeny(serial, cert_id)
                organization.aps.append(ap)
                ruleset.aps.append(ap)
                db.session.add(ap)  # pylint: disable=no-member
            else:
                raise exceptions.BadRequest("duplicate entry")

        db.session.commit()  # pylint: disable=no-member
        return "Success", 200

    def delete(self, id):
        """Remove an AP from the system. Here the id is the AP id"""

        LOGGER.info("Deleting ap: %s", id)
        ap = aaa.AccessPointDeny.query.filter_by(id=id).first()
        if not ap:
            raise exceptions.BadRequest("Bad AP")

        # check user roles
        auth(roles=["Admin"], org=ap.org.name)
        db.session.delete(ap)  # pylint: disable=no-member
        db.session.commit()  # pylint: disable=no-member
        return flask.make_response()


class DeniedRegion(MethodView):
    """resources to manage denied regions"""

    methods = ["PUT", "GET"]

    def _open(self, rel_path, mode, user=None):
        """Open a configuration file.

        :param rel_path: The specific config name to open.
        :param mode: The file open mode.
        :return: The opened file.
        :rtype: file-like
        """

        config_path = os.path.join(
            flask.current_app.config["NFS_MOUNT_PATH"],
            "rat_transfer",
            "denied_regions")
        if not os.path.exists(config_path):
            os.makedirs(config_path)

        file_path = os.path.join(config_path, rel_path)
        LOGGER.debug('Opening denied region file "%s"', file_path)
        if not os.path.exists(file_path) and mode != "wb":
            raise werkzeug.exceptions.NotFound()

        handle = open(file_path, mode)

        if mode == "wb":
            os.chmod(file_path, 0o666)

        return handle

    def get(self, regionStr):
        """GET method for denied regions"""
        LOGGER.debug("getting denied regions")
        filename = regionStr + "_denied_regions.csv"

        resp = flask.make_response()
        with self._open(filename, "rb") as conf_file:
            resp.data = conf_file.read()
        resp.content_type = "text/csv"
        return resp

    def put(self, regionStr):
        """PUT method for denied regions"""
        user_id = auth(roles=["Super"])
        LOGGER.debug("current user: %s", user_id)
        filename = regionStr + "_denied_regions.csv"

        if flask.request.content_type != "text/csv":
            raise werkzeug.exceptions.UnsupportedMediaType()

        with contextlib.closing(self._open(filename, "wb", user_id)) as outfile:
            shutil.copyfileobj(flask.request.stream, outfile)
        return flask.make_response("Denied regions updated", 204)


class CertId(MethodView):
    """resources to manage access points"""

    methods = ["GET"]

    def get(self, id):
        """Get Certification Id info with specific user_id."""
        id = auth(roles=["Super", "Admin"])
        user = aaa.User.query.filter_by(id=id).first()
        roles = [r.name for r in user.roles]
        # Super user gets all access points
        cert_ids = db.session.query(aaa.CertId).all()

        return flask.jsonify(
            certIds=[
                {
                    "id": cert.id,
                    "certificationId": cert.certification_id,
                    "rulesetId": cert.ruleset_id,
                    "org": ap.org,
                }
                for cert in cert_ids
            ]
        )


class Limits(MethodView):
    methods = ["PUT", "GET"]

    def put(self):
        """set eirp limit"""

        content = flask.request.get_json()
        auth(roles=["Super"])
        try:
            LOGGER.error("content: %s ", content)
            newIndoorEnforce = content.get('indoorEnforce')
            newIndoorLimit = content.get('indoorLimit')
            newOutdoorEnforce = content.get('outdoorEnforce')
            newOutdoorLimit = content.get('outdoorLimit')

            indoorlimitRecord = aaa.Limit.query.filter_by(id=0).first()
            outdoorlimitRecord = aaa.Limit.query.filter_by(id=1).first()

            if (indoorlimitRecord is None and outdoorlimitRecord is None):
                # create new records
                if (not newIndoorEnforce and not newOutdoorEnforce):
                    raise exceptions.BadRequest("No change")
                limit0 = aaa.Limit(newIndoorLimit, newIndoorEnforce, False)
                limit1 = aaa.Limit(newOutdoorLimit, newOutdoorEnforce, True)
                db.session.add(limit0)
                db.session.add(limit1)
            elif (indoorlimitRecord is None and outdoorlimitRecord is not None):
                limit0 = aaa.Limit(newIndoorLimit, newIndoorEnforce, False)
                db.session.add(limit0)
                outdoorlimitRecord.enforce = newOutdoorEnforce
                outdoorlimitRecord.limit = newOutdoorLimit
            elif (indoorlimitRecord is not None and outdoorlimitRecord is None):
                limit1 = aaa.Limit(newOutdoorLimit, newOutdoorEnforce, True)
                db.session.add(limit1)
                indoorlimitRecord.enforce = newIndoorEnforce
                indoorlimitRecord.limit = newIndoorLimit
            else:
                outdoorlimitRecord.enforce = newOutdoorEnforce
                outdoorlimitRecord.limit = newOutdoorLimit
                indoorlimitRecord.enforce = newIndoorEnforce
                indoorlimitRecord.limit = newIndoorLimit
            db.session.commit()
            return flask.jsonify(
                indoorLimit=float(newIndoorLimit),
                outdoorLimit=float(newOutdoorLimit),
                indoorEnforce=newIndoorEnforce,
                outdoorEnforce=newOutdoorEnforce,
            )
        except IntegrityError:
            raise exceptions.BadRequest("DB Error")

    def get(self):
        """get eirp limit"""
        try:
            # First get the indoor limit (id 0)
            indoorlimits = aaa.Limit.query.filter_by(id=0).first()
            # Then get the outdoor limit (id 1)
            outdoorlimits = aaa.Limit.query.filter_by(id=1).first()
            if indoorlimits or outdoorlimits:
                return flask.jsonify(
                    indoorLimit=float(indoorlimits.limit),
                    outdoorLimit=float(outdoorlimits.limit),
                    indoorEnforce=indoorlimits.enforce,
                    outdoorEnforce=outdoorlimits.enforce,
                )
            else:
                return flask.make_response("Min Eirp not configured", 404)

        except IntegrityError:
            raise exceptions.BadRequest("DB Error")


class AllowedFreqRanges(MethodView):
    """Allows an admin to update the JSON containing the allowed
    frequency bands and allow any user to view but not edit the file
    """

    methods = ["PUT", "GET"]
    ACCEPTABLE_FILES = {
        "allowed_frequencies.json": dict(
            content_type="application/json",
        )
    }

    def _open(self, rel_path, mode, user=None):
        """Open a configuration file.

        :param rel_path: The specific config name to open.
        :param mode: The file open mode.
        :return: The opened file.
        :rtype: file-like
        """

        config_path = os.path.join(
            flask.current_app.config["NFS_MOUNT_PATH"],
            "rat_transfer",
            "frequency_bands",
        )
        if not os.path.exists(config_path):
            os.makedirs(config_path)

        file_path = os.path.join(config_path, rel_path)
        LOGGER.debug('Opening frequncy file "%s"', file_path)
        if not os.path.exists(file_path) and mode != "wb":
            raise werkzeug.exceptions.NotFound()

        handle = open(file_path, mode)

        if mode == "wb":
            os.chmod(file_path, 0o666)

        return handle

    def get(self):
        """GET method for allowed frequency bands"""
        LOGGER.debug("getting admin supplied frequncy bands")
        filename = "allowed_frequencies.json"
        if filename not in self.ACCEPTABLE_FILES:
            LOGGER.debug("Could not find allowed_frequencies.json")
            raise werkzeug.exceptions.NotFound()
        filedesc = self.ACCEPTABLE_FILES[filename]

        resp = flask.make_response()
        with self._open("allowed_frequencies.json", "rb") as conf_file:
            resp.data = conf_file.read()
        resp.content_type = filedesc["content_type"]
        return resp

    def put(self, filename="allowed_frequencies.json"):
        """PUT method for afc config"""
        user_id = auth(roles=["Super"])
        LOGGER.debug("current user: %s", user_id)
        if filename not in self.ACCEPTABLE_FILES:
            raise werkzeug.exceptions.NotFound()
        filedesc = self.ACCEPTABLE_FILES[filename]
        if flask.request.content_type != filedesc["content_type"]:
            raise werkzeug.exceptions.UnsupportedMediaType()

        with contextlib.closing(self._open(filename, "wb", user_id)) as outfile:
            shutil.copyfileobj(flask.request.stream, outfile)
        return flask.make_response("Allowed frequency ranges updated", 204)


class MTLS(MethodView):
    """resources to manage mtls certificates"""

    methods = ["POST", "GET", "DELETE"]

    def _rebuild_cert_bundle(self) -> None:
        LOGGER.debug(f"{type(self)}.{eval(dbg_msg)}() ")
        bundle_data = ""
        cmd = "cmd_restart"
        for certs in db.session.query(aaa.MTLS).all():
            LOGGER.info(f"{certs.id}")
            bundle_data += certs.cert
        db.session.commit()  # pylint: disable=no-member
        LOGGER.debug(
            f"{type(self)}.{eval(dbg_msg)}() {bundle_data} {len(bundle_data)}")
        with DataIf().open("certificate/client.bundle.pem") as hfile:
            if len(bundle_data) == 0:
                hfile.delete()
                cmd = "cmd_remove"
            else:
                hfile.write(bundle_data)
        LOGGER.debug(
            f"{type(self)}.{eval(dbg_msg)}() "
            f"{flask.current_app.config['BROKER_URL']}"
        )
        publisher = MsgPublisher(
            flask.current_app.config["BROKER_URL"],
            flask.current_app.config["BROKER_EXCH_DISPAT"],
        )
        publisher.publish(cmd)
        publisher.close()

    def get(self, id):
        """Get MTLS info with specific user_id."""
        LOGGER.debug(f"{type(self)}.{eval(dbg_msg)}() ")

        if id == 0:
            user_id = auth(roles=["Admin"])
            user = aaa.User.query.filter_by(id=user_id).first()
            roles = [r.name for r in user.roles]
            if "Super" in roles:
                mtls_list = aaa.MTLS.query.all()
            else:
                # Admin user gets certificates for his/her own org
                org = user.org if user.org else ""
                mtls_list = db.session.query(aaa.MTLS).filter(
                    aaa.MTLS.org == org).all()
        else:
            raise werkzeug.exceptions.NotFound()

        return flask.jsonify(
            mtls=[
                {
                    "id": mtls.id,
                    "cert": mtls.cert,
                    "note": mtls.note if mtls.note else "",
                    "org": mtls.org if mtls.org else "",
                    "created": str(mtls.created),
                }
                for mtls in mtls_list
            ]
        )

    def post(self, id):
        """Insert an mtls certificate by a user id to a database table,
        fetch all certificates from the table and create a new
        certificate bundle. Copy the bundle to a predefined place
        and send command to correspondent clients.
        """
        content = flask.request.json
        org = content.get("org")
        auth(roles=["Admin"], org=org)
        LOGGER.debug(
            f"{type(self)}.{eval(dbg_msg)}() " f"mtls: {str(id)} org: {org}")

        # check if certificate is already there.
        cert = content.get("cert")
        try:
            import base64

            strip_chars = "base64,"
            index = cert.index(strip_chars)
            cert = base64.b64decode(cert[index + len(strip_chars):])
            cert = str(cert, "UTF-8").replace("\\n", "\n")
        except BaseException:
            LOGGER.error(f"PUT mtls: {str(id)} org: {org} exception")
            raise exceptions.BadRequest("Unexpected certificate format")

        try:
            mtls = aaa.MTLS(cert, content.get("note"), org)
            db.session.add(mtls)
            db.session.flush()  # pylint: disable=no-member
        except Exception as e:
            LOGGER.error(
                f"Failed to insert new cert into table " f"({type(e)} {e})")
            raise exceptions.BadRequest("Failed to insert new cert into table")

        LOGGER.debug(
            f"{type(self)}.{eval(dbg_msg)}() "
            f"Added cert id: {str(mtls.id)} org: {org}"
        )

        try:
            self._rebuild_cert_bundle()
        except Exception as e:
            LOGGER.error(
                f"Failed to prepare new bundle with mtls: "
                f"{str(mtls.id)}, ({type(e)} {e})"
            )
            self.delete(mtls.id)
            raise exceptions.BadRequest("Failed to prepare new bundle file")

        return flask.jsonify(id=mtls.id)

    def delete(self, id):
        """Remove an mtls cert from the system.
        Here the id is the mtls cert id instead of the user_id
        """
        LOGGER.debug(f"{type(self)}.{eval(dbg_msg)}()")

        mtls = aaa.MTLS.query.filter_by(id=id).first()
        user_id = auth(roles=["Admin"], org=mtls.org)
        user = aaa.User.query.filter_by(id=user_id).first()
        LOGGER.info("Deleting mtls: %s", str(mtls.id))

        db.session.delete(mtls)  # pylint: disable=no-member
        db.session.commit()  # pylint: disable=no-member

        try:
            self._rebuild_cert_bundle()
        except Exception as e:
            LOGGER.error(
                f"Failed to prepare new bundle without mtls: "
                f"{str(mtls.id)}, ({type(e)} {e})"
            )
            raise exceptions.BadRequest("Failed to prepare new bundle file")

        return flask.make_response()


module.add_url_rule("/user/<int:user_id>", view_func=User.as_view("User"))
module.add_url_rule("/user/ap_deny/<int:id>",
                    view_func=AccessPointDeny.as_view("AccessPointDeny"))
module.add_url_rule("/user/cert/<int:id>", view_func=CertId.as_view("CertId"))
module.add_url_rule("/user/eirp_min", view_func=Limits.as_view("Eirp"))
module.add_url_rule(
    "/user/frequency_range", view_func=AllowedFreqRanges.as_view("Frequency")
)
module.add_url_rule("/user/mtls/<int:id>", view_func=MTLS.as_view("MTLS"))
module.add_url_rule(
    "/user/denied_regions/<string:regionStr>",
    view_func=DeniedRegion.as_view("DeniedRegion"),
)
