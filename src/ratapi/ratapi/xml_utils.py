''' Shared utility classes and functions related to XML processing.
'''

import re
import datetime


def int_to_xml(value):
    ''' Convert to XML Schema canonical "int" text.

    :param value: The number to convert.
    :type value: int or None
    :return: The integer text.
    :rtype: unicode or None
    '''
    if value is None:
        return None

    return unicode(value)


def xml_to_int(text, default=None):
    ''' Convert from XML Schema "integer" text to python int.

    :param text: The text to convert.
    :type text: str or unicode or None
    :param default: The default value to use in case text is None.
    :type default: int or None
    :return: The integer value.
    :rtype: int or None
    '''
    if text is None:
        return default

    return int(text)


def datetime_to_xml(value):
    ''' Convert to XML Schema canonical "dateTime" text.
    '''
    if value is None:
        return None
    return value.strftime(u'%Y-%m-%dT%H:%M:%SZ')


#: Regular expression for matching XML Schema date-time values in UTC (with or without separators)
XSD_DATETIME_ZULU_RE = re.compile(
    r'(\d{4})-?(\d{2})-?(\d{2})T(\d{2}):?(\d{2}):?(\d{2})(\.(\d{1,6}))?Z')


def xml_to_datetime(text, default=None):
    ''' Convert from XML Schema "dateTime" text to python datetime object.

    :param text: The text to convert.
    :type text: str or unicode
    :param default: The default value to use in case text is None.
    :type default: int or None
    :return: The time value.
    :rtype: :py:cls:`datetime.datetime`
    '''
    import math

    if text is None:
        return default

    match = XSD_DATETIME_ZULU_RE.match(text)
    if match is None:
        raise ValueError('Invalid XML datetime "{0}"'.format(text))

    kwargs = dict(
        year=xml_to_int(match.group(1)),
        month=xml_to_int(match.group(2)),
        day=xml_to_int(match.group(3)),
        hour=xml_to_int(match.group(4)),
        minute=xml_to_int(match.group(5)),
        second=xml_to_int(match.group(6)),
    )
    subsec = match.group(8)
    if subsec:
        kwargs['microsecond'] = int(xml_to_int(
            subsec) * math.pow(10, 6 - len(subsec)))

    return datetime.datetime(**kwargs)
