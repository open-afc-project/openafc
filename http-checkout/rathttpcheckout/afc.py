from .base import (UserLoginBaseTestCase)


class TestAfcEngine(UserLoginBaseTestCase):
    ''' Class for testing the results of the AFC Engine
    '''

    def setUp(self):
        UserLoginBaseTestCase.setUp(self)

    def tearDown(self):
        UserLoginBaseTestCase.tearDown(self)

    def _set_afc_config(self, afc_config):
        ''' Uploads the _afc_config variable to the server to be used for AFC Engine tests
        '''
        self._test_modify_request(self._resolve_url(
            'ratapi/v1/afcconfig/afc_config.json'), afc_config)

    def _generate_params(self, lat, lng, height, semi_maj=0, semi_min=0, orientation=0,
                         height_type="AGL", height_cert=0, in_out_door="INDOOR", ruleset_ids=None):
        ''' Uses parameters to generate a well formed JSON object to be used for analysis.

        :param lat: latitude
        :type lat: number

        :param lng: longitude
        :type lng: number

        :param height: height
        :type height: number

        :param semi_maj: ellipse semi-major axis (default 0)
        :type semi_maj: number

        :param semi_min: ellipse semi-minor axis (default 0)
        :type semi_min: number

        :param orientation: ellipse orientation. degrees clockwise from north (defualt 0)
        :type orientation: number

        :param height_type: "AMSL" (above mean sea level)(default) | "AGL" (above ground level)

        :param height_cert: height uncertainty (default 0)
        :type height_cert: number

        :param in_out_door: "INDOOR" | "OUTDOOR" | "ANY"

        :param ruleset_ids: list of ruleset IDs (default ['AFC-6GHZ-DEMO-1.0'])

        :returns: PawsRequest
        '''

        if ruleset_ids is None:
            ruleset_ids = ['AFC-6GHZ-DEMO-1.0']

        return {
            'deviceDesc': {
                'rulesetIds': ruleset_ids,
            },
            'location': {
                'point': {
                    'center': {
                        'latitude': lat,
                        'longitude': lng,
                    },
                    'semiMajorAxis': semi_maj,
                    'semiMinorAxis': semi_min,
                    'orientation': orientation,
                },
            },
            'antenna': {
                'height': height,
                'heightType': height_type,
                'heightUncertainty': height_cert,
            },
            'capabilities': {
                'indoorOutdoor': in_out_door,
            }
        }

    def _test_geojson_result_valid(self, result):
        '''
        '''

    def _test_channel_result_valid(self, result):
        '''
        '''

    def _test_paws_result_valid(self, result, req_devicedesc):
        ''' Tests that the structure of a returned paws object is correct
        '''

        # check for same device description
        self.assertEqual(req_devicedesc, result.get('deviceDesc'))

        for spec in result.get('spectrumSpecs'):

            # check matching ruleset
            self.assertEqual(
                {
                    'authority': 'US',
                    'rulesetId': 'AFC-6GHZ-DEMO-1.0',
                },
                spec.get('rulesetInfo')
            )

            for schedule in spec.get('spectrumSchedules'):

                # check properly formatted time
                self._test_iso_time(schedule.get('eventTime').get('startTime'))
                self._test_iso_time(schedule.get('eventTime').get('stopTime'))

                # must have four groups of channels
                self.assertEqual(len(schedule.get('spectra')), 4)

                # validate spectra contents
                self._test_present_bandwidths(
                    schedule.get('spectra'),
                    [20000000, 40000000, 80000000, 160000000])

    def _test_iso_time(self, time):
        ''' Tests that the time is a properly formatted ISO time string
        '''
        self.assertRegexpMatches(
            time, r'[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z')

    def _test_present_bandwidths(self, spectra, bandwidths):
        ''' Tests to make sure each bandwidth is present in the spectra profiles
        '''
        present_bands = [s.get('resolutionBwHz') for s in spectra]
        for band_width in bandwidths:
            self.assertIn(band_width, present_bands)
