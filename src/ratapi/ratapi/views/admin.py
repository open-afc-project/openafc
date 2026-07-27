# This Python file uses the following encoding: utf-8
#
# Portions copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate
# affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy
# of which is included with this software program.
#

import json
import hmac
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
from .auth import auth, public_route
from afcmodels.base import db
import db_creator
import db_utils

#: Logger for this module
LOGGER = logging.getLogger(__name__)


def _read_file(path):
    """Return stripped contents of a file, or None if path is falsy/unreadable."""
    if not path:
        return None
    try:
        with open(path) as fh:
            return fh.read().strip() or None
    except OSError:
        return None


#: All views under this API blueprint
module = flask.Blueprint("admin", "admin")


def _ensure_admin_or_self_read(user_id):
    """Require Admin for list-all (user_id==0); single-user GET allows self.

    Used only by ``User.get``. Call sites (frontend ``getUser`` / same URL):

    - **Account** ``UserAccountPage``: ``user_id`` is always the logged-in user
      (self read after ``_ensure`` relaxed this path for non-Admins).
    - **Admin → Edit User modal**: ``user_id`` is the row being edited; callers
      are Admins and still go through ``auth(roles=['Admin'], is_user=…)`` when
      ``current_user.id != user_id`` (same org / Super rules as before).
    - **MTLS** ``NewMTLS``: ``user_id`` is the page's ``MTLS`` prop (Admin MTLS
      route passes the logged-in admin's id); ``getUser`` is only for org label.

    Non-admins cannot enumerate others: unknown ``user_id`` still hits
    ``auth(..., is_user=…)`` and returns **403** / **404** per existing rules.
    """
    from flask_login import current_user

    if user_id == 0:
        auth(roles=["Admin"])
        return
    if not current_user.is_authenticated:
        raise werkzeug.exceptions.Unauthorized()
    if not current_user.active:
        raise werkzeug.exceptions.Forbidden("Inactive user")
    if current_user.id != user_id:
        auth(roles=["Admin"], is_user=user_id)


def _apply_self_setprops(user, content):
    """Limited profile updates for the logged-in user on their own account.

    Avoids the VULN-FINDINGS2 issue where ``auth(roles=['Admin'], is_user=…)``
    would be the wrong gate for self-service: we never auto-confirm email on
    self-initiated email change, never allow ``active`` changes, and never
    allow local email/password changes when OIDC owns credentials.
    """
    oidc = flask.current_app.config.get("OIDC_LOGIN", False)
    if "active" in content and content.get("active") != user.active:
        raise exceptions.Forbidden(
            "Only an administrator can change account active status."
        )
    changing_email = (not oidc and "email" in content
                      and content.get("email") != user.email)
    changing_password = (not oidc and content.get("editCredential")
                         and content.get("password"))
    if changing_email or changing_password:
        from flask_security import verify_password

        if not verify_password(content.get("current_password") or "",
                               user.password or ""):
            raise exceptions.Forbidden(
                "Current password required to change email or password."
            )
    if oidc and "email" in content and content.get("email", user.email) != user.email:
        raise exceptions.Forbidden(
            "Email is managed by your identity provider and cannot be changed here."
        )
    if not oidc and "email" in content:
        new_email = content["email"]
        if new_email != user.email:
            user.email = new_email
            user.email_confirmed_at = None
    if "firstName" in content:
        user.first_name = content.get("firstName", user.first_name)
    if "lastName" in content:
        user.last_name = content.get("lastName", user.last_name)
    if content.get("editCredential") and content.get("password"):
        if oidc:
            raise exceptions.Forbidden(
                "Password is managed by your identity provider."
            )
        from flask_security import hash_password

        user.password = hash_password(content["password"])


class User(MethodView):
    """Administration resources for managing users."""

    methods = ["POST", "GET", "DELETE"]

    def get(self, user_id):
        """Get User infor with specific user_id. If none is provided,
        return list of all users. Query parameters used for params."""

        _ensure_admin_or_self_read(user_id)
        if user_id == 0:
            from flask_login import current_user

            id = current_user.id
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
        from flask_login import current_user
        if not current_user.is_authenticated:
            raise werkzeug.exceptions.Unauthorized()

        # Authenticate before touching the target record to avoid a 404/403
        # oracle that leaks user-ID existence across organizations.
        # For self-mutation (setProps) this gates the call appropriately; the
        # more-specific auth() calls below further constrain the operation.
        content = flask.request.json
        if "setProps" not in content or current_user.id != user_id:
            auth(roles=["Admin"])

        user = aaa.User.query.filter_by(id=user_id).first()
        if user is None:
            raise exceptions.NotFound("User does not exist")
        LOGGER.debug("got user: %s", user.email)
        org = user.org if user.org else ""

        if "setProps" in content:
            from flask_login import current_user  # noqa: F811

            if not current_user.is_authenticated:
                raise werkzeug.exceptions.Unauthorized()
            if not current_user.active:
                raise werkzeug.exceptions.Forbidden("Inactive user")
            is_self = current_user.id == user_id
            if is_self:
                _apply_self_setprops(user, content)
            else:
                auth(roles=["Admin"], is_user=user_id)
                if "Super" in [r.name for r in user.roles]:
                    if "Super" not in [r.name for r in current_user.roles]:
                        raise exceptions.Forbidden(
                            "Only Super users can modify Super users"
                        )
                user_props = content
                changing_email = ("email" in user_props
                                  and user_props.get("email") != user.email)
                pw = user_props.get("password")
                if (changing_email or pw) and \
                        "Super" not in [r.name for r in current_user.roles]:
                    raise exceptions.Forbidden(
                        "Only Super users can change another user's "
                        "email or password"
                    )
                target_roles = [r.name for r in user.roles]
                if "active" in user_props and \
                        user_props.get("active") != user.active and \
                        "Admin" in target_roles and \
                        "Super" not in [r.name for r in current_user.roles]:
                    raise exceptions.Forbidden(
                        "Only Super users can change the active status "
                        "of an Admin user"
                    )
                if changing_email:
                    user.email = user_props["email"]
                    user.email_confirmed_at = datetime.datetime.now()
                user.active = user_props.get("active", user.active)
                if pw:
                    if flask.current_app.config["OIDC_LOGIN"]:
                        from passlib.context import CryptContext

                        password_crypt_context = CryptContext(["bcrypt"])
                        pass_hash = password_crypt_context.encrypt(pw)
                    else:
                        from flask_security import hash_password

                        pass_hash = hash_password(pw)
                    user.password = pass_hash
            db.session.commit()  # pylint: disable=no-member

        elif "addRole" in content:
            # just add a single role
            role = content.get("addRole")
            if role == "Super":
                auth(roles=["Super"], org=org)
            else:
                auth(roles=["Admin"], org=org)
            if "Super" in [r.name for r in user.roles]:
                auth(roles=["Super"])
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
            if "Super" in [r.name for r in user.roles]:
                auth(roles=["Super"])
            # No principal may demote an account of equal administrative
            # rank: removing a role from an Admin user requires Super,
            # mirroring the S0127-11 guard already enforced on the
            # 'active' flag in setProps above.
            if "Admin" in [r.name for r in user.roles]:
                auth(roles=["Super"])

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

        # Authenticate before touching the target record. Dereferencing a
        # missing user's .org before auth() produced a 500-vs-401 oracle that
        # let an unauthenticated client enumerate valid user IDs across all
        # orgs. Require Admin first, then return a uniform 404 for both missing
        # and not-found targets.
        auth(roles=["Admin"])
        user = aaa.User.query.filter_by(id=user_id).first()
        if user is None:
            raise exceptions.NotFound("User does not exist")
        org = user.org if user.org else ""
        auth(roles=["Admin"], org=org)
        # A lower-privileged role must never be able to delete a Super user.
        target_roles = [r.name for r in user.roles]
        if "Super" in target_roles:
            auth(roles=["Super"])
        # No principal may delete an account of equal administrative rank:
        # deleting an Admin is strictly stronger than deactivating one,
        # which already requires Super (S0127-11).
        elif "Admin" in target_roles:
            auth(roles=["Super"])
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
        # Derive org from request body only for Super callers; non-Super must
        # use their own org so they cannot create deny entries in foreign orgs.
        org = content.get("org")
        if not org:
            raise exceptions.BadRequest("org is required")
        # Super-only: deny entries require cross-tenant administrative
        # authority; per-org Admin role is not sufficient.
        caller_id = auth(roles=["Super"], org=org)
        caller = aaa.User.query.filter_by(id=caller_id).first()
        caller_roles = [r.name for r in caller.roles] if caller else []

        serial = content.get("serialNumber")
        if serial == "*" or not serial:
            serial = None
        else:
            serial = serial.strip().upper()

        cert_id = content.get("certificationId")
        if not cert_id:
            raise exceptions.BadRequest("Certification Id required")

        rulesetId = content.get("rulesetId")
        if not rulesetId:
            raise exceptions.BadRequest("Ruleset Id required")

        organization = aaa.Organization.query.filter_by(name=org).first()
        if not organization:
            raise exceptions.BadRequest("Organization does not exist")

        ap = (
            aaa.AccessPointDeny.query.filter_by(certification_id=cert_id)
            .filter_by(serial_number=serial)
            .filter_by(org_id=organization.id)
            .first()
        )
        if ap:
            # We detect existing entry
            raise exceptions.BadRequest("Duplicate device detected")

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

        content = flask.request.json
        # Super-only: consistent with put(); see comment there.
        id = auth(roles=["Super"])
        user = aaa.User.query.filter_by(id=id).first()
        user_org = user.org

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
                # Wildcard deny affects every tenant's devices sharing this
                # cert ID — same Super-only restriction as the single-entry
                # PUT path.
                if "Super" not in roles:
                    raise exceptions.Forbidden(
                        "Only Super users may create wildcard "
                        "(serial=*) deny entries")
                serial = None
            else:
                serial = serial.upper()

            cert_id = row[1].strip()
            try:
                ruleset_id = ruleset_list[int(row[2].strip())]
                ruleset = aaa.Ruleset.query.filter_by(name=ruleset_id).first()
                if not ruleset:
                    raise exceptions.BadRequest(
                        "ruleset {} does not exist".format(ruleset_id)
                    )
            except Exception:
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

        # Require Super before touching the record so a present-vs-absent id
        # cannot be distinguished by an unauthenticated/low-priv caller.
        # Super-only to match put()/post(): deny entries are globally enforced
        # by RatAfc._auth_ap(), so the same-org Admin must not be able to
        # remove a Super-created block.
        auth(roles=["Super"])
        LOGGER.info("Deleting ap: %s", id)
        ap = aaa.AccessPointDeny.query.filter_by(id=id).first()
        if not ap:
            raise exceptions.NotFound("AP does not exist")

        # check user roles (org-scoped)
        auth(roles=["Super"], org=ap.org.name)
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
            os.chmod(file_path, 0o644)

        return handle

    def get(self, regionStr):
        """GET method for denied regions"""
        user_id = auth(roles=["Admin", "AP", "Analysis"])
        LOGGER.debug("current user: %s", user_id)
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
        if "Super" in roles:
            # Super user gets all cert ids
            cert_ids = db.session.query(aaa.CertId).all()
        else:
            # Admin user gets cert ids for his/her own org only
            org = user.org if user.org else ""
            cert_ids = db.session.query(aaa.CertId).filter(
                aaa.CertId.org == org).all()

        return flask.jsonify(
            certIds=[
                {
                    "id": cert.id,
                    "certificationId": cert.certification_id,
                    "rulesetId": cert.ruleset_id,
                    "org": cert.org,
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
        auth(roles=["Admin", "AP", "Analysis"])
        try:
            # First get the indoor limit (id 0)
            indoorlimits = aaa.Limit.query.filter_by(id=0).first()
            # Then get the outdoor limit (id 1)
            outdoorlimits = aaa.Limit.query.filter_by(id=1).first()
            if indoorlimits or outdoorlimits:
                return flask.jsonify(
                    indoorLimit=float(indoorlimits.limit)
                    if indoorlimits else 0.0,
                    outdoorLimit=float(outdoorlimits.limit)
                    if outdoorlimits else 0.0,
                    indoorEnforce=indoorlimits.enforce
                    if indoorlimits else False,
                    outdoorEnforce=outdoorlimits.enforce
                    if outdoorlimits else False,
                    limitsConfigured=bool(indoorlimits and outdoorlimits),
                )
            else:
                # Not an error condition for fresh installs; GUI treats via flag.
                return flask.jsonify(
                    indoorLimit=18.0,
                    outdoorLimit=18.0,
                    indoorEnforce=False,
                    outdoorEnforce=False,
                    limitsConfigured=False)

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
            os.chmod(file_path, 0o644)

        return handle

    def get(self):
        """GET method for allowed frequency bands"""
        user_id = auth(roles=["Admin", "AP", "Analysis"])
        LOGGER.debug("current user: %s", user_id)
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
        LOGGER.debug(f"{type(self)}.{inspect.currentframe().f_code.co_name}() ")
        bundle_data = ""
        cmd = "cmd_restart"
        # Serialize concurrent MTLS mutations (POST/DELETE) so the
        # read-rebuild-write span is mutually exclusive across gunicorn workers.
        # Without the lock a lost-update race under PostgreSQL READ COMMITTED
        # can resurrect a deleted CA in the bundle written to objstore.
        db.session.execute(
            db.text("SELECT pg_advisory_xact_lock(42000)"))
        for certs in db.session.query(aaa.MTLS).all():
            LOGGER.info(f"{certs.id}")
            bundle_data += certs.cert
        LOGGER.debug(
            f"{type(self)}.{inspect.currentframe().f_code.co_name}()"
            f" {bundle_data} {len(bundle_data)}")
        with DataIf().open("certificate/client.bundle.pem") as hfile:
            if len(bundle_data) == 0:
                hfile.delete()
                # cmd_remove disables the dispatcher's mTLS trust anchor, so
                # it must be authenticated the same way bundle installation
                # is (SUB-0138-18): sign 'cmd_remove:<ts_ns>:<nonce>:<hexsig>'
                # with the dedicated DISPATCHER_BUNDLE_HMAC_KEY_FILE secret,
                # matching dispatcher/acceptor.py's _verify_remove().
                _token = _read_file(
                    os.environ.get("DISPATCHER_BUNDLE_HMAC_KEY_FILE"))
                if not _token:
                    raise RuntimeError(
                        "DISPATCHER_BUNDLE_HMAC_KEY_FILE not set or "
                        "unreadable — refusing to send an unsigned "
                        "cmd_remove (the dispatcher would not honor it)")
                import hashlib as _hl
                import secrets as _secrets
                import time as _t
                _ts = _t.time_ns()
                _nonce = _secrets.token_hex(16)
                _sig = hmac.new(
                    _token.encode(),
                    f"cmd_remove|{_ts}|{_nonce}".encode(),
                    _hl.sha256).hexdigest()
                cmd = f"cmd_remove:{_ts}:{_nonce}:{_sig}"
            else:
                # Fail closed when the bundle-signing key is missing: the
                # dispatcher acceptor refuses to install any bundle lacking
                # a valid HMAC sidecar, so writing an unsigned bundle here
                # and reporting success would let a CA addition/revocation
                # silently fail to propagate while nginx keeps trusting a
                # since-removed CA indefinitely.
                _token = _read_file(
                    os.environ.get("DISPATCHER_BUNDLE_HMAC_KEY_FILE"))
                if not _token:
                    raise RuntimeError(
                        "DISPATCHER_BUNDLE_HMAC_KEY_FILE not set or "
                        "unreadable — refusing to write an unsigned mTLS "
                        "bundle (the dispatcher would not install it)")
                hfile.write(bundle_data)
                # Sign the bundle with HMAC-SHA256 keyed on a dedicated
                # secret (DISPATCHER_BUNDLE_HMAC_KEY_FILE) shared only with
                # the dispatcher so bundle integrity is independent of both
                # object storage access and the widely-shared
                # AFC_INTERNAL_TOKEN gateway bearer.
                import hashlib as _hl
                import time as _t
                # Bind a monotonic version (epoch nanoseconds) under the
                # MAC so an objstorage writer cannot replay a previously-
                # valid bundle after a CA has been removed.  max(MTLS.id)
                # is NOT monotonic across deletions, so use wall-clock.
                _ver = _t.time_ns()
                _bd = (bundle_data.encode() if isinstance(bundle_data, str)
                       else bundle_data)
                _sig = hmac.new(
                    _token.encode(),
                    f"{_ver}|{len(_bd)}|".encode() + _bd,
                    _hl.sha256).hexdigest()
                with DataIf().open(
                        "certificate/client.bundle.pem.hmac") as _hf:
                    _hf.write(f"{_ver}:{_sig}".encode())
        import re as _re
        _safe_url = _re.sub(r':[^:@/]+@', ':***@',
                            flask.current_app.config['BROKER_URL'])
        LOGGER.debug(
            f"{type(self)}.{inspect.currentframe().f_code.co_name}() "
            f"{_safe_url}"
        )
        publisher = MsgPublisher(
            flask.current_app.config["BROKER_URL"],
            flask.current_app.config["BROKER_EXCH_DISPAT"],
        )
        publisher.publish(cmd)
        publisher.close()
        # Commit last so pg_advisory_xact_lock(42000) is held across the
        # full read-rebuild-write-publish span (SUB-0127-07).
        db.session.commit()  # pylint: disable=no-member

    def get(self, id):
        """Get MTLS info with specific user_id."""
        LOGGER.debug(f"{type(self)}.{inspect.currentframe().f_code.co_name}() ")

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
        # mTLS CA upload is restricted to Super-only: the nginx CA bundle is
        # global (ssl_client_certificate is not per-org), so CA changes must
        # be controlled at the Super-admin level.
        auth(roles=["Super"])
        LOGGER.debug(
            f"{type(self)}.{inspect.currentframe().f_code.co_name}()"
            f" mtls: {str(id)} org: {org}")

        # check if certificate is already there.
        cert = content.get("cert")
        try:
            import base64

            strip_chars = "base64,"
            index = cert.index(strip_chars)
            cert = base64.b64decode(cert[index + len(strip_chars):])
            cert = str(cert, "UTF-8").replace("\\n", "\n")
        except Exception:
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
            f"{type(self)}.{inspect.currentframe().f_code.co_name}() "
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
        LOGGER.debug(f"{type(self)}.{inspect.currentframe().f_code.co_name}()")

        mtls = aaa.MTLS.query.filter_by(id=id).first()
        # Super-only for same reason as post(): modifying the global nginx CA
        # bundle must not be allowed to per-org Admins.
        auth(roles=["Super"])
        LOGGER.info("Deleting mtls: %s", str(mtls.id))
        deleted_id = mtls.id

        # Flush only (mirror post()): _rebuild_cert_bundle() commits last
        # under the advisory lock, so the row delete and the bundle
        # republication take effect atomically - either both or neither.
        # Committing here first (as before) let a rebuild failure leave the
        # DB state (CA revoked) diverged from the enforced state (CA still
        # in the dispatcher bundle) with no way to compensate or retry.
        db.session.delete(mtls)  # pylint: disable=no-member
        db.session.flush()  # pylint: disable=no-member

        try:
            self._rebuild_cert_bundle()
        except Exception as e:
            # Roll back the flushed delete so the DB state cannot diverge
            # from the enforced (bundle) state; the DELETE can then simply
            # be retried once the transient failure clears.
            db.session.rollback()  # pylint: disable=no-member
            LOGGER.error(
                f"Failed to prepare new bundle without mtls: "
                f"{deleted_id}, ({type(e)} {e})"
            )
            raise exceptions.BadRequest("Failed to prepare new bundle file")

        return flask.make_response()


@public_route
class CreateDb(MethodView):
    """ (Re)creator of absent Postgres databases """

    # Default PostgreSQL port
    DEFAULT_PORT = 5432
    # Prefix for db creator user/database (usually `postgres/postgres`) DSNs
    # on various servers
    DSN_ENV_PREFIX = "AFC_DB_CREATOR_DSN_"
    # Prefix for correspondent password filename environment variables
    PASSWORD_FILE_ENV_PREFIX = "AFC_DB_CREATOR_PASSWORD_FILE_"
    # Prefix for pre-declared service DSNs that token-authenticated callers
    # may request creation of (allowlist of (user, db) tuples)
    SERVICE_DSN_ENV_PREFIX = "AFC_DB_CREATOR_SERVICE_DSN_"
    # Prefix for per-service password filename env vars (operator-provisioned;
    # token-authenticated CreateDb derives the role password from these,
    # ignoring any caller-supplied body['password'])
    SERVICE_PASSWORD_FILE_ENV_PREFIX = \
        "AFC_DB_CREATOR_SERVICE_PASSWORD_FILE_"
    # Env var naming the single readonly role token callers may grant
    READONLY_ROLE_ENV = "AFC_DB_CREATOR_READONLY_ROLE"

    def post(self):
        """ Postgres database creation REST API

        POST CreateDb?dsn=<DSN>[&recreate=<True/False>][&owner=<True/False>]

        Internal services (uls_downloader, als_siphon) may authenticate with
        the x-afc-internal-token header instead of a session cookie.
        """
        expected_token = os.environ.get("AFC_INTERNAL_TOKEN") or \
            _read_file(os.environ.get("AFC_INTERNAL_TOKEN_FILE"))
        supplied_token = flask.request.headers.get("x-afc-internal-token") or ""
        token_ok = (
            bool(expected_token)
            and hmac.compare_digest(supplied_token, expected_token)
        )
        if not token_ok:
            auth(roles=['Super'])
            super_authed = True
        else:
            super_authed = False
        dsn = flask.request.args.get('dsn')
        recreate = flask.request.args.get('recreate', 'False').lower() == 'true'
        owner = flask.request.args.get('owner', 'False').lower() == 'true'
        # DROP DATABASE (recreate=True) is destructive and must be
        # gated on a Super session. The shared internal bearer token is
        # intentionally not sufficient here; internal services never legitimately
        # use recreate.
        if recreate and not super_authed:
            flask.abort(403, "Database drop/recreate requires Super role, not service token")
        # owner=False reaches the GRANT-on-existing-database branch in
        # db_creator.ensure_dsn (confused-deputy: token holder mints a new
        # Postgres principal on a pre-existing database). Restrict to Super.
        if (not owner) and not super_authed:
            flask.abort(403, "owner=False requires Super role, not service token")
        # Read password from JSON body to avoid URL query-string logging.
        # Use silent=True so that callers that send no body (password=None path)
        # don't trigger Flask's 415 Unsupported Media Type response.
        body = flask.request.get_json(silent=True) or {}
        password = body.get('password') or None

        safe_dsn_str = db_utils.safe_dsn(dsn if dsn else 'Unknown')
        grant_readonly_role = flask.request.args.get('grant_readonly_role') or None
        if not super_authed:
            # Token-authenticated callers may only create pre-declared
            # service principals; reject any DSN whose (user, db) is not
            # enumerated in AFC_DB_CREATOR_SERVICE_DSN_* env vars, so a
            # compromised internal-token holder cannot mint arbitrary
            # Postgres LOGIN roles via the superuser deputy.
            try:
                req_info = db_creator.DsnInfo(dsn=dsn, password=password)
            except RuntimeError as ex:
                flask.abort(400, str(ex))
            allowed_principals: dict = {}
            for env_name, env_val in os.environ.items():
                if env_name.startswith(self.SERVICE_DSN_ENV_PREFIX) \
                        and env_val:
                    try:
                        svc = db_creator.DsnInfo(dsn=env_val)
                        allowed_principals[(svc.user, svc.db)] = \
                            (env_name[len(self.SERVICE_DSN_ENV_PREFIX):], svc)
                    except RuntimeError:
                        continue
            if (req_info.user, req_info.db) not in allowed_principals:
                flask.abort(
                    403,
                    "Service token may only create pre-declared "
                    "(user, db) principals")
            # Confused-deputy hardening: every internal service shares the
            # same bearer token, so the caller-supplied body['password']
            # would let service A choose service B's role password. Override
            # it with the operator-provisioned secret for the matched
            # service (AFC_DB_CREATOR_SERVICE_PASSWORD_FILE_<suffix>, or the
            # password embedded in AFC_DB_CREATOR_SERVICE_DSN_<suffix>).
            svc_suffix, svc_info = \
                allowed_principals[(req_info.user, req_info.db)]
            password = _read_file(
                os.environ.get(
                    self.SERVICE_PASSWORD_FILE_ENV_PREFIX + svc_suffix)) \
                or svc_info.password
            allowed_ro = os.environ.get(self.READONLY_ROLE_ENV) or None
            if grant_readonly_role is not None and \
                    grant_readonly_role != allowed_ro:
                flask.abort(
                    403,
                    "Service token may not grant arbitrary readonly role")
        try:
            db_creator.ensure_dsn(dsn=dsn, password=password,
                                  recreate=recreate, owner=owner, local=True,
                                  grant_readonly_role=grant_readonly_role)
            LOGGER.info(f"Database '{safe_dsn_str}' created")
            return flask.make_response()
        except RuntimeError as ex:
            # Use the redacted DSN (safe_dsn_str) in the log — the raw `dsn`
            # query parameter may embed cleartext credentials.
            LOGGER.error(f"Error creating database '{safe_dsn_str}': {ex}")
            raise werkzeug.exceptions.BadRequest(
                f"Error creating database '{safe_dsn_str}'")


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
module.add_url_rule("/CreateDb", view_func=CreateDb.as_view("CreateDb"))
