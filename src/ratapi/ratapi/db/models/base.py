''' Base classes underlying model classes.
'''

import sqlalchemy as sa
from sqlalchemy import types
import sqlalchemy.ext.declarative as declarative

#: Base class for declarative models
Base = declarative.declarative_base()
#: Naming conventions codified
# Base.metadata.naming_convention = {
#   "ix": 'ix_%(column_0_label)s',
#   "uq": "uq_%(table_name)s_%(column_0_name)s",
# #  "ck": "ck_%(table_name)s_%(constraint_name)s",
#   "fk": "fk_%(table_name)s_%(column_0_name)s_%(referred_table_name)s",
#   "pk": "pk_%(table_name)s"
# }
