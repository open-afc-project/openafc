''' Utilities related to command argument parsing and sub-commands.
'''

import argparse
import logging

#: logger for this module
LOGGER = logging.getLogger(__name__)

def packageversion(pkg_name):
    ''' Get the version identifier for a python package.
    
    :param str pkg_name: The package name to get the version for.
    :return: The version identifier or None.
    :rtype: str
    '''
    import pkg_resources
    try:
        return pkg_resources.require(pkg_name)[0].version
    except Exception as err:
        LOGGER.error('Failed to fetch package %s version: %s', pkg_name, err)
        return None

class CmdMixin(object):
    ''' Interface for command mixin classes.
    
    Mixins provide argument parser manipulation and helper functions
    based on those arguments.
    '''
    
    #: List of function names from this class to donate
    donated_funcs = []
    #: List of other mixin classes depended-upon by this class
    depend_mixins = []
    
    def __init__(self):
        self.__cmd = None
    
    def __str__(self, *args, **kwargs):
        ''' Display the type of this mixin.
        '''
        return str(self.__class__.__name__)
    
    def set_command(self, cmd):
        ''' Register this mixin for a specific command.
        
        :param cmd: The command which is using this mixin object.
        :type cmd: :py:cls:`Command`
        '''
        self.__cmd = cmd
    
    def get_command(self):
        ''' Get the command associated with this mixin.
        
        :return: The associated command, which may be None.
        :rtype: :py:cls:`Command`
        '''
        return self.__cmd
    
    def get_logger(self):
        ''' Get the command logger for this mixin.
        
        :return: A logger object.
        '''
        if self.__cmd:
            return self.__cmd.logger
        else:
            return LOGGER
    
    def donate_funcs(self):
        ''' Get a set of functions to be donated to the parent Command object.
        
        :return: A dictionary of function names to callables.
        :rtype: dict
        '''
        funcs = dict()
        for name in self.donated_funcs:
            funcs[name] = getattr(self, name)
        return funcs
    
    def config_args(self, parser):
        ''' Add options to the command parser object.

        The default behavior is to add no command-specific options or arguments.
        :param parser: The command-line parser to manipulate.
        :type parser: :py:class:`argparse.ArgumentParser`
        '''
        pass

class Command(object):
    ''' Abstract base class for sub-command handler.
    
    Class attributes useful to define commands are:
    
    .. py:attribute:: action_name
        The command-line name of this command, also used for the instance
        logger name.
    
    .. py:attribute:: action_help
        Text for the auto-generated command-line help information.
    
    .. py:attribute:: logger
        An instance-level :py:class:`logging.logger` object usable by
        derived classes.
    
    Constructor arguments are:
    :param action_name: The per-instance action name, defaults to 
        :py:attr:`Command.action_name` class attribute.
    '''
    
    #: Default action name for all instances of this class
    action_name = None
    #: Default action help for all instances of this class
    action_help = None
    #: Initial set of mixins for this class
    mixin_classes = []

    def __init__(self, *args, **kwargs):
        action_name = kwargs.get('action_name')
        if action_name:
            self.action_name = action_name
        if not self.action_name:
            raise RuntimeError('Command {0} must define an "action_name"'.format(self))
        self.logger = logging.getLogger('action.' + self.action_name)
        
        self._mixins = []
        
        # scan mixin dependencies
        unchecked_classes = set(self.mixin_classes)
        all_mixin_classes = set()
        while unchecked_classes:
            # iterate on copy
            add_classes = list(unchecked_classes)
            unchecked_classes = set()
            for cls in add_classes:
                all_mixin_classes.add(cls)
                unchecked_classes = unchecked_classes.union(set(cls.depend_mixins))
        self.logger.debug('Using command mixins: %s', [str(obj) for obj in all_mixin_classes])
        
        # actually instantiate the mixins
        for cls in all_mixin_classes:
            self.add_mixin(cls())
    
    def add_mixin(self, obj):
        ''' Add a new mixin to this command.
        
        :param obj: The mixin object to add.
        :type obj: :py:cls:`CmdMixin` or similar
        '''
        self._mixins.append(obj)
        obj.set_command(self)
        
        for (name,func) in obj.donate_funcs().items():
            setattr(self, name, func)
    
    def apply_mixin_args(self, parser):
        ''' Apply all mixin augmentation to a command parser.
        
        :param parser: The sub-parser to manipulate.
        :type parser: :py:class:`argparse.ArgumentParser`
        '''
        for obj in self._mixins:
            obj.config_args(parser)
    
    def config_args(self, parser):
        ''' Add options to the command sub-parser object.

        The default behavior is to add no command-specific options or arguments.
        :param parser: The sub-parser to manipulate.
        :type parser: :py:class:`argparse.ArgumentParser`
        '''
        self.apply_mixin_args(parser)

    def run(self, args):
        ''' Execute the command.
        :param args: The full set of command-line arguments.
        :type args: :py:cls:`argparse.Namespace`
        :return: The status code for this command run.
        :rtype: int or None
        '''
        raise NotImplementedError()

class CommandSet(object):
    ''' A collection of :py:class:`Command` definition objects.
    The initial command set is empty (see :py:meth:`add_command`).
    
    :param init_parser: An initial argument parser to append per-command
        sub-parsers.
    :type init_parser: :py:class:`argparse.ArgumentParser`
    '''
    def __init__(self, dest_suffix=None):
        if dest_suffix is None:
            dest_suffix = 'root'
        self.action_dest = 'action-' + dest_suffix
        
        self._cmdobjs = {}
        self._cmddefns = []
    
    def add_command(self, cmd):
        ''' Add a new commad action to this set.
        This must be called before :py:func:`config_args` is used.
        
        :param cmd: The command object to add.
        :type cmd: :py:class:`Command`
        '''
        name = cmd.action_name
        if not name:
            raise ValueError('Undefined action_name')
        if name in self._cmdobjs:
            raise KeyError('Duplicate command name "{0}"'.format(name))
        
        # find first non-None help text
        helpsources = [cmd.action_help, cmd.__doc__]
        helptext = next((item for item in helpsources if item is not None), None)
        
        self._cmdobjs[name] = cmd
        self._cmddefns.append({
            'name': name,
            'helptext': helptext,
            'command': cmd,
        })
    
    def config_args(self, parser):
        ''' Add options to the command parser object.

        The default behavior is to add no command-specific options or arguments.
        :param parser: The command-line parser to manipulate.
        :type parser: :py:class:`argparse.ArgumentParser`
        '''
        subparsers = parser.add_subparsers(dest=self.action_dest,
                                           metavar='SUB_COMMAND',
                                           help='The maintenance action to perform.')
        
        for defn in self._cmddefns:
            sub_parser = subparsers.add_parser(defn['name'], help=defn['helptext'])
            defn['command'].config_args(sub_parser)
    
    def run(self, args):
        ''' Execute the command.
        :param args: The full set of command-line arguments.
        :type args: :py:cls:`argparse.Namespace`
        :return: The status code for this command run.
        :rtype: int or None
        '''
        try:
            cmdname = getattr(args, self.action_dest)
        except AttributeError:
            raise ValueError('No command given')
        try:
            cmd = self._cmdobjs[cmdname]
        except KeyError:
            raise ValueError('Unknown command "{0}"'.format(args.action))
        
        return cmd.run(args)

class NestingCommand(Command):
    ''' A command which is itself contains a command set.
    
    The default set of sub-commands present are defined by the 
    :py:cvar:`sub_classes` variable.
    '''
    
    #: Set of classes to be instantiated as sub-commands
    sub_classes = []
    
    def __init__(self, *args, **kwargs):
        Command.__init__(self, *args, **kwargs)
        
        self._cset = CommandSet(dest_suffix=self.action_name)
        
        for cls in self.sub_classes:
            self._cset.add_command(cls())
    
    def config_args(self, parser):
        self._cset.config_args(parser)
    
    def run(self, args):
        return self._cset.run(args)

class RootCommandSet(CommandSet):
    ''' Shared top-level options for administrative utility programs.
    '''
    
    def __init__(self, *args, **kwargs):
        self.version_name = kwargs.pop('version_name', None)
        CommandSet.__init__(self, *args, **kwargs)
    
    def parse_and_run(self, argv):
        ''' Parse command-line arguments and execute command actions.
        
        :param argv: The full set of command-line arguments.
        :type argv: list of str
        :return: The status code for this command run.
        :rtype: int or None
        '''
        import sys
        from .cmd_log import configure_logging # pylint: disable=import-error
        
        parser = argparse.ArgumentParser()
        if self.version_name is not None:
            dispver = '%(prog)s {0}'.format(self.version_name)
            parser.add_argument('--version', action='version', 
                                version=dispver)
        parser.add_argument('--log-level', dest='log_level', default='info',
                            metavar='LEVEL',
                            help='Console logging lowest level displayed.')
        self.config_args(parser)

        args = parser.parse_args(argv[1:])
        log_level_name = args.log_level.upper()
        configure_logging(level=log_level_name, stream=sys.stderr)
        if log_level_name == 'DEBUG':
            logging.getLogger('sqlalchemy.engine').setLevel(logging.INFO)
        
        LOGGER.debug('Running with args: {0}'.format(args))
        try:
            return self.run(args)
        except Exception as err:
            LOGGER.error('Failed to run: ({0}) {1}'.format(err.__class__.__name__, err))
            if log_level_name == 'DEBUG':
                raise
            return 1


class DbusMixin(CmdMixin):
    ''' Add an argument to override dbus address and a function to
    obtain a dbus connection.
    '''
    donated_funcs = ['_dbus_conn']
    
    def config_args(self, sub_parser):
        sub_parser.add_argument('--dbus-address', type=str, default=None,
                                help='D-Bus address to use instead of system bus')
    
    def _dbus_conn_addr(self, args):
        ''' Get the D-Bus address (or standard bus type) to connect to.
        
        :param args: Command argument namespace.
        :type args: object
        :return: The bus address.
        '''
        import dbus
        
        addr = args.dbus_address
        if not addr:
            addr = dbus.bus.BusConnection.TYPE_SESSION
        return addr
    
    def _dbus_conn(self, args):
        ''' Open a D-Bus connection.
        
        :param args: Command argument namespace.
        :type args: object
        :return: The bus connection.
        :rtype: :py:cls:`dbus.bus.BusConnection`
        '''
        import dbus
        
        addr = self._dbus_conn_addr(args)
        self.get_logger().debug('Connecting to dbus address "%s"', addr)
        return dbus.bus.BusConnection(addr)

class ConfFileMixin(CmdMixin):
    ''' Add an argument to override config path and a function to
    obtain and cache config file contents.
    '''
    donated_funcs = ['_get_config_xml']
    
    def __init__(self, *args, **kwargs):
        import lxml.etree as etree
        
        CmdMixin.__init__(self, *args, **kwargs)
        self._conffiles = {}
        self.parser = etree.XMLParser()
    
    def config_args(self, sub_parser):
        sub_parser.add_argument('--configfile', type=str, default=self.config_file_path,
                                help='Daemon config file to read from instead of system default')
    
    def _get_config_xml(self, args):
        ''' Read and cache a configuration file as XML document.
        
        :param args: The parsed command arguments.
        :type args: :py:cls:`argparse.Namespace`
        :return: An XML DOM document.
        '''
        import lxml.etree as etree
        
        filepath = args.configfile
        if filepath in self._conffiles:
            return self._conffiles[filepath]
        
        self.get_logger().debug('Loading config from "%s"...', filepath)
        try:
            tree = etree.parse(filepath, parser=self.parser)
        except Exception as err:
            self.get_logger().debug('Failed with: %s', err)
            raise
        self.get_logger().debug('Loaded as "%s"', tree.getroot().tag)
        
        self._conffiles[filepath] = tree
        return tree

class RdbMixin(CmdMixin):
    ''' Add an argument to override config path and a function to
    extract database context manager from config.
    '''
    depend_mixins = [ConfFileMixin]
    donated_funcs = ['_db_contextmgr']
    
    #: The XML element (etree.QName object) to extract from the configuration file
    config_file_element = None
    
    def _db_contextmgr(self, args):
        ''' Get a DB connection context manager.
        
        :param args: The parsed command arguments.
        :type args: :py:cls:`argparse.Namespace`
        :return: A context manager for DB connections.
        '''
        from cpodb.utils import session_context_manager # pylint: disable=import-error
        
        tree = self.get_command()._get_config_xml(args)
        try:
            db_uri = tree.find(self.config_file_element).text
        except:
            raise RuntimeError('Unable to find config parameter "{0}"'.format(self.config_file_element))
        return session_context_manager(db_uri)

class FileMixin(CmdMixin):
    ''' Handle file names including stdin/out default.
    '''
    donated_funcs = ['_open_file']
    
    def _open_file(self, file_path, mode):
        ''' Open a file on the local filesystem, or stdin/stdout.
        
        :param file_path: The file to open, or None for stdin/stdout.
        :type file_path: str or None
        '''
        if file_path is None:
            import sys
            if 'r' in mode:
                return sys.stdin
            elif 'w' in mode:
                return sys.stdout
        return open(file_path, mode)

def value_format(val):
    ''' Provide standard formatting for output values.

    :param val: The value to format.
    :return: The formatted value
    '''
    import numbers
    import datetime
    from .time_utils import TIME_FORMAT_EXTENDED # pylint: disable=import-error
    
    if isinstance(val, numbers.Integral):
        return '{:,}'.format(val)
    if isinstance(val, datetime.datetime):
        return datetime.datetime.strftime(val, TIME_FORMAT_EXTENDED)
    else:
        return val

class ColDefn(object):
    ''' Represent a table column schema for use with :py:mod:`prettytable`.
    
    :param key: The dictionary key for the column.
    :param heading: The human-readable text heading for the column.
    '''
    def __init__(self, key, heading=None, align=None):
        self.key = key
        if not heading:
            heading = key
        self.heading = heading
        if not align:
            align = 'r'
        self.align = align
    
    def format(self, val):
        ''' Apply locale-aware number formatting.
        '''
        return value_format(val)
