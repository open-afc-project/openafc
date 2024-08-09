""" Hardcoded relations that eventually may go to some DB """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

from typing import Dict, List, NamedTuple, Optional, Set

from . import aaa


class RulesetVsRegion:
    """ Translations between Ruleset IDs and Afc Config's region strings """
    # Domain (region) descriptotr as stored internally
    _DomainDsc = \
        NamedTuple(
            "_DomainDsc",
            [
             # AFC Config region string
             ("region", str),
             # AFC Request Ruleset ID
             ("ruleset", str),
             # True for base (specified in constructor) domain, False for
             # derived
             ("is_base", bool),
             # True to provide in list function
             ("is_listable", bool),
             # None or string to overwrite region string in config being sent
             # to AFC Engine
             ("overwrite_region", Optional[str])])

    # Domain descriptors by ruleset ID. Initialized on first class method
    # invocation
    _by_ruleset: Dict[str, "RulesetVsRegion._DomainDsc"] = {}

    # Domain descriptors by AFC Config region string. Initialized on first
    # class method invocation
    _by_region: Dict[str, "RulesetVsRegion._DomainDsc"] = {}

    @classmethod
    def base_region_list(cls) -> List[str]:
        """ List of listable base (nonderived) AFC Region strings """
        cls._initialize()
        return [r for r, dd in cls._by_region.iyems() if dd.is_base]

    @classmethod
    def region_list(cls) -> List[str]:
        """ List of listable AFC Region strings """
        cls._initialize()
        return [r for r, dd in cls._by_region.items() if dd.is_listable]

    @classmethod
    def ruleset_list(cls) -> List[str]:
        """ List of listable Ruleset ID strings """
        cls._initialize()
        return [r for r, dd in cls._by_ruleset.items() if dd.is_listable]

    @classmethod
    def region_to_ruleset(cls, region: str, exc: Exception) -> str:
        """ Returns Ruleset ID for given AFC Config region string.
        Generates exception of given type is not found """
        cls._initialize()
        try:
            return cls._by_region[region].ruleset
        except KeyError:
            raise exc(f"Invalid region name '{region}'")

    @classmethod
    def ruleset_to_region(cls, ruleset: str, exc: Exception) -> str:
        """ Returns AFC Config region string for given Ruleset ID.
        Generates exception of given type is not found """
        cls._initialize()
        try:
            return cls._by_ruleset[ruleset].region
        except KeyError:
            raise exc(f"Invalid ruleset ID '{ruleset}'")

    @classmethod
    def overwrite_region(cls, region: str, exc: Exception) -> Optional[str]:
        """ Returns None or AFC Region string to use in AFC Config to send to\
        AFC Engine. Generates exception of given type is not found """
        cls._initialize()
        try:
            return cls._by_region[region].overwrite_region
        except KeyError:
            raise exc(f"Invalid region name '{region}'")

    @classmethod
    def _initialize(cls) -> None:
        """ Initializes dictionaries on first call """
        if cls._by_region and cls._by_ruleset:
            return
        base_domains = \
            [("US", "US_47_CFR_PART_15_SUBPART_E"),  # First is 'DEFAULT'
             ("CA", "CA_RES_DBS-06"),
             ("BR", "BRAZIL_RULESETID"),
             ("GB", "UNITEDKINGDOM_RULESETID")]
        for idx, (region, ruleset) in enumerate(base_domains):
            assert ruleset not in cls._by_ruleset
            assert region not in cls._by_region
            cls._by_ruleset[ruleset] = cls._by_region[region] = \
                RulesetVsRegion._DomainDsc(
                    region=region, ruleset=ruleset, is_base=True,
                    is_listable=True, overwrite_region=None)
            for prefix in ("TEST_", "DEMO_"):
                cls._by_ruleset[prefix + ruleset] = \
                    cls._by_region[prefix + region] = \
                    RulesetVsRegion._DomainDsc(
                        region=prefix + region, ruleset=prefix + region,
                        is_base=False, is_listable=True,
                        overwrite_region=region)
            if idx == 0:
                cls._by_region["DEFAULT"] = \
                    RulesetVsRegion._DomainDsc(
                        region="DEFAULT", ruleset=ruleset,
                        is_base=False, is_listable=False,
                        overwrite_region=None)


class SpecialCertifications:
    """ Special Certificate/serial names.
    Better be explicitly disabled in production database """
    _Key = NamedTuple("_Key", [("cert_id", str), ("serial_number", str)])
    Properties = NamedTuple("Properties", [("location_flags", int)])

    # Certificate dictionary. Initialized on first call to class methods
    _special_certificates: Dict["SpecialCertifications._Key",
                               "SpecialCertifications.Properties"] = {}

    @classmethod
    def get_properties(cls, cert_id: str, serial_number: str) \
            -> Optional["SpecialCertifications.Properties"]:
        cls._initialize()
        return \
            cls._special_certificates.get(
                cls._Key(cert_id=cert_id, serial_number=serial_number))

    @classmethod
    def _initialize(cls) -> None:
        if cls._special_certificates:
            return
        for cert_id, serial_number, location_flags in \
                [("TestCertificationId", "TestSerialNumber",
                  aaa.CertId.OUTDOOR | aaa.CertId.INDOOR),
                 ("HeatMapCertificationId", "HeatMapSerialNumber",
                  aaa.CertId.OUTDOOR | aaa.CertId.INDOOR)]:
            key = cls._Key(cert_id=cert_id, serial_number=serial_number)
            assert key not in cls._special_certificates
            cls._special_certificates[key] = \
                cls.Properties(location_flags=location_flags)


class VendorExtensionFilter:
    """ Filters Vendor Extensions from Messages and requests/responses
    according to type-specific whitelists """
    class _WhitelistType(NamedTuple):
        """ Whitelist type """
        # True for message, False for request/response, None for both
        is_message: Optional[bool] = None
        # True for input (request), false for output(response), None for both
        is_input: Optional[bool] = None
        # True for GUI, False for non-GUI, None for both
        is_gui: Optional[bool] = None
        # True for internal, False for external, None for both
        is_internal: Optional[bool] = None

        def is_partial(self):
            """ True for partial whitelist type that has fields set to None """
            return any(getattr(self, attr) is None for attr in self._fields)

    class _Whitelist:
        """ Whitelist for a message/request/response type(s)

        Public attributes:
        extensions -- Set of allowed extension IDs

        Private attributes
        _partial_type -- Specifies type(s) of messages/resuests/response this
                          whitelist is for
        """

        def __init__(self, extensions: List[str],
                     partial_type: "VendorExtensionFilter._WhitelistType") \
                -> None:
            """ Constructor

            Arguments:
            extensions   -- List of whitelisted extension names
            partial_type -- Type(s) of messages/resuests/response this
                             whitelist is for
            """
            assert isinstance(extensions, list)
            self._partial_type = partial_type
            self.extensions = set(extensions)

        def is_for(
                self,
                whitelist_type: "VendorExtensionFilter._WhitelistType") \
                -> bool:
            """ True if this whilelist is for messages/requests/responses
            specified by nonpartial whitelist type """
            assert not whitelist_type.is_partial()
            for attr in whitelist_type._fields:
                value = getattr(whitelist_type, attr)
                if getattr(self._partial_type, attr) not in (None, value):
                    return False
            return True

    # List of whilelists. Only nonempty whitelists are presented
    # It is OK for more than one whitelist to cover some type
    # It is OK to add more whitelists (e.g. for different whitelist types)
    _whitelists = [
        # Allowed vendor extensions for individual requests from AFC services
        # (NOT from APs)
        _Whitelist(
            ["openAfc.overrideAfcConfig"],
            _WhitelistType(is_input=True, is_message=False, is_internal=True)),
        # Allowed vendor extensions for individual requests received from APs
        _Whitelist(
            ["rlanAntenna"],
            _WhitelistType(is_input=True, is_message=False)),
        # Allowed vendor extensions for individual responses sent to AFC Web
        # GUI
        _Whitelist(
            ["openAfc.redBlackData", "openAfc.mapinfo"],
            _WhitelistType(is_input=False, is_message=False, is_gui=True)),
        _Whitelist(
            ["openAfc.heatMap"],
            _WhitelistType(is_input=True, is_gui=True))]

    # Dictionary of allowed extensions, indexed by nonpartial whitelist types
    # Filled on first call of member function
    _whitelist_dict: Dict["VendorExtensionFilter._WhitelistType",
                          Set[str]] = {}

    @classmethod
    def allowed_extension(cls, extension: str, is_message: bool,
                          is_input: bool, is_gui: bool, is_internal: bool) \
            -> bool:
        """ True if given vendor extension is allowed for given message type

        Arguments:
        extension   -- Extension ID in question
        is_message  -- True for message, false for individual request/response
        is_input    -- True for request, False for response
        is_gui      -- True if from GUI
        is_internal -- True if internal (from within AFC Cluster)
        """
        cls._initialize()
        return \
            extension in \
            cls._whitelist_dict.get(
                cls._WhitelistType(
                    is_message=is_message, is_input=is_input,
                    is_gui=is_gui, is_internal=is_internal),
                set())

    @classmethod
    def _initialize(cls):
        """ One-time initialization """
        if cls._whitelist_dict:
            return
        # Maps whitelist types to sets of allowed extensions
        for is_message in (True, False):
            for is_input in (True, False):
                for is_gui in (True, False):
                    for is_internal in (True, False):
                        whitelist_type = \
                            cls._WhitelistType(
                                is_input=is_input, is_message=is_message,
                                is_gui=is_gui, is_internal=is_internal)
                        # Ensure that no attributes were omitted
                        assert not whitelist_type.is_partial()
                        # Finding whitelist for key
                        for wl in cls._whitelists:
                            if wl.is_for(whitelist_type):
                                cls._whitelist_dict.setdefault(
                                    whitelist_type, set()).\
                                    update(wl.extensions)
