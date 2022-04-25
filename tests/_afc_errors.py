#
# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

class AfcError(Exception):
    pass

class IncompleteRange(AfcError):
    def __init__(self, left, right, msg='Incomplete range'):
        self.msg = msg
        self.left = left
        self.right = right
        super().__init__(self.msg, self.left, self.right)

class IncompleteFreqRange(IncompleteRange):
    def __init__(self, left, right, msg='Incomplete freq range'):
        IncompleteRange.__init__(self, left, right, msg)

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
