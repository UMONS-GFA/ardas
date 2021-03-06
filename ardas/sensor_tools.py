import numpy as np
import os
from pickle import dump, load
import logging

cur_dir = os.path.dirname(os.path.realpath(__file__))


def polynomial(value, coefs):
    """Compute polynomial using Horner method

    :param value: given value of the variable
    :param coefs: coefficients of the polynomial
    :return: evaluation of polynomial for the given value of the variable
    :rtype: float
    """

    result = coefs[-1]
    for i in range(-2, -len(coefs) - 1, -1):
        result = result * value + coefs[i]
    assert isinstance(result, float)
    return result


def running_average(n):
    """Compute a running average (not centered)

    :param n: number of former samples used to compute the running average
    :return: evaluation of polynomial for the given value of the variable (freq)
    :rtype: float
    """

    l = []
    average = None
    while True:
        new_elt = yield average
        if len(l) == n:
            del l[0]
        l.append(new_elt)
        if len(l) > 0:
            average = sum(l) / len(l)
        else:
            average = np.nan


def load_sensor(sensor_id):
    """Loads a sensor object from a sensor '.ssr' file

    :param sensor_id: a unique identification number of the sensor
    :return: a sensor object
    :rtype: sensor"""
    f_name = os.path.join(cur_dir,'sensor_' + sensor_id + '.ssr')
    with open(f_name, 'rb') as sensor_file:
        sensor = load(sensor_file)
    return sensor


class FMSensor(object):
    def __init__(self, sensor_id='0000', processing_method=polynomial, processing_parameters=(0., 1., 0., 0., 0.),
                 quantity='freq.', units='Hz', output_format='%11.4f', log_output=True):
        self.sensor_id = sensor_id
        self.processing_method = processing_method
        self.processing_parameters = processing_parameters
        self.quantity = quantity
        self.units = units
        self.output_format = output_format
        self.log = log_output

    @property
    def sensor_id(self):
        """ Gets and sets sensor id
        """
        return self.__sensor_id

    @sensor_id.setter
    def sensor_id(self, val):
        self.__sensor_id = str(val[:4])

    @property
    def units(self):
        """ Gets and sets sensor units
        """
        return self.__units

    @units.setter
    def units(self, val):
        self.__units = val

    def output(self, value):
        """Outputs an output computed using the processing method and parameters

        :return: processed quantity
        :rtype: float
        """

        output = self.processing_method(value, self.processing_parameters)
        return output

    def output_repr(self, value):
        """Gets a representation of the output

        :return: representation of the processed quantity
        :rtype: string
        """

        try:
            s = self.output_format + ' ' + self.units
            calibrated_output = s % self.output(value)
        except Exception as e:
            calibrated_output = '*** error : %s ***' % e
        assert isinstance(calibrated_output, str)
        return calibrated_output

    def save(self):
        """Save the sensor as a serialized object to a file """
        f_name = cur_dir + '/sensor_' + self.sensor_id + '.ssr'
        if os.path.isfile(f_name):
            logging.warning('Sensor file ' + f_name + ' already exists, unable to save sensor')
        else:
            with open(f_name, 'wb') as sensor_file:
                dump(self, sensor_file)


class UncalibratedFMSensor(FMSensor):
    """ A subclass of the sensor object with a simpler """
    def __init__(self, sensor_id='0000', log_output=True):
        super().__init__(sensor_id=sensor_id, log_output=log_output)


if __name__ == '__main__':
    t = polynomial(25000, [-16.9224032438, 0.0041525221, -1.31475837290789e-07, 2.39122208189129e-12,
                           -1.72530800355418e-17])
    print(t)
    print(cur_dir)
