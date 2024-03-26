#!/usr/bin/env python3
#

"""
Description

Non-table database definitions
"""

from flask_sqlalchemy import SQLAlchemy

#: Application database object
db = SQLAlchemy()


class UserDbInfo():
    VER = 1
