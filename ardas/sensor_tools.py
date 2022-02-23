import numpy as np
import os
from pathlib import Path
from pickle import dump, load
import logging
import RPi.GPIO as GPIO
import time

cur_dir = os.path.dirname(os.path.realpath(__file__))


def polynomial(value, **kwargs):
    """Compute polynomial using Horner method

    :param value: given value of the variable
    :return: evaluation of polynomial for the given value of the variable
    :rtype: float
    """
    coefs = kwargs.pop('coefs', (1,0))  # coefficients of the polynomial
    result = coefs[-1]
    for i in range(-2, -len(coefs) - 1, -1):
        result = result * value[-1] + coefs[i]
    result = result[0]
    assert isinstance(result, float)
    return result


def running_average(x, **kwargs):
    """Compute a running average (not centered)

    :param x: np.array of samples used to compute the running average
    :return: evaluation of the running average
    :rtype: float
    """

    return np.mean(x)


def activate_pin(pin, delay, safe_pins=(12, 6, 13, 16, 19, 20, 26, 21)):
    """ Activates a pin of the raspberry pi in output mode for the number of seconds specified in delay"""
    if pin in safe_pins:
        GPIO.setmode(GPIO.BCM)
        GPIO.setwarnings(False)
        GPIO.setup(pin, GPIO.OUT)
        GPIO.output(pin, GPIO.HIGH)
        logging.info('Activating ' + str(pin) + ' for ' + str(delay) + ' seconds.')
        time.sleep(delay)
        GPIO.cleanup()
        logging.info('Deactivating ' + str(pin) + '.')
    else:
        logging.warning('Pin number ' + str(pin) + ' is not in safe_pins. Skipped activating pin '+ str(pin) + '.')


def running_median(x, **kwargs):
    """Compute a running average (not centered)

    :param x: np.array of samples used to compute the running median
    :return: evaluation of the running median
    :rtype: float
    """

    return np.median(x)


def open_valve_if_full(x, **kwargs):
    status = 0
    condition = kwargs.pop('condition', '==0.')
    pin = kwargs.pop('pin', 12)
    delay = kwargs.pop('delay', 30.)
    safe_pins = kwargs.pop('safe_pins', (12))
    logging.debug('Sensor freq. :' + ', '.join([str(i[0]) for i in x]) + 'Hz')
    empty_dict = {}
    cond = 'not (' + str(x[-2][0]) + condition + ') and (' + str(x[-1][0]) + condition + ')'
    print('Condition: '+ cond)
    if eval(cond):
        activate_pin(pin, delay, safe_pins)
        status = 1
    return status


def load_sensor(sensor_id):
    """Loads a sensor object from a sensor '.ssr' file

    :param sensor_id: a unique identification number of the sensor
    :return: a sensor object
    :rtype: sensor"""
    f_name = cur_dir + '/sensor_' + sensor_id + '.ssr'
    with open(f_name, 'rb') as sensor_file:
        sensor = load(sensor_file)
    return sensor


class FMSensor(object):
    def __init__(self, sensor_id='0000', processing_method=polynomial, processing_parameters={'coefs': (0,1)},
                 quantity='freq.', units='Hz', output_format='%11.4f', short_term_memory=1, log_output=True,
                 initial_values=None):
        self.__sensor_id = sensor_id
        self.processing_method = processing_method
        self.processing_parameters = processing_parameters
        self._value = np.empty((short_term_memory,1))
        self.short_term_memory = short_term_memory
        if initial_values is None:
            self._value[:] = 0.
        else:
            assert isinstance(initial_values, np.ndarray)
            self._value = initial_values
        self._processed_value = self.processing_method(self.value, **self.processing_parameters)
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
    def value(self):
        """ Gets and sets sensor value
        """
        return self._value

    @value.setter
    def value(self, val):
        assert isinstance(val, float)
        self._value = np.roll(self._value, -1)
        self._value[-1] = val
        self._processed_value = self.processing_method(self.value, **self.processing_parameters)


    @property
    def units(self):
        """ Gets and sets sensor value
        """
        return self.__units

    @units.setter
    def units(self, val):
        self.__units = val

    def output(self):
        """Outputs an output computed using the processing method and parameters

        :return: processed quantity
        :rtype: float
        """

        return self._processed_value

    def output_repr(self):
        """Gets a representation of the output

        :return: representation of the processed quantity
        :rtype: string
        """

        try:
            s = self.output_format + ' ' + self.units
            calibrated_output = s % self.output()
        except Exception as e:
            calibrated_output = '*** error : %s ***' % e
        assert isinstance(calibrated_output, str)
        return calibrated_output

    def save(self):
        """Save the sensor as a serialized object to a file """
        f_name = cur_dir + '/sensor_' + self.sensor_id + '.ssr'
        if Path(f_name).exists():
            logging.warning('Sensor file ' + f_name + ' already exists, unable to save sensor')
        else:
            with open(f_name, 'wb') as sensor_file:
                dump(self, sensor_file)


class UncalibratedFMSensor(FMSensor):
    """ A subclass of the sensor object with a simpler """
    def __init__(self, sensor_id='0000', log_output=True):
        super().__init__(sensor_id=sensor_id, log_output=log_output)


if __name__ == '__main__':
    #t = polynomial(25000, [-16.9224032438, 0.0041525221, -1.31475837290789e-07, 2.39122208189129e-12,
    #                       -1.72530800355418e-17])
    #print(t)
    #print(cur_dir)
    # sensor = FMSensor(sensor_id='9999', processing_method=running_average, processing_parameters={},
    #                   short_term_memory=3, log_output=False)
    # while True:
    #     reply = input('Please enter sensor value')
    #     sensor.value = float(reply)
    #     print(sensor.output())
    sensor = FMSensor(sensor_id='9999', processing_method=polynomial,
                      processing_parameters={'coefs':(-16.922, 0.0041525221, -1.314e-07, 2.391e-12, -1.725e-17)},
                      log_output=False)
    sensor.value = 25000.
    print(sensor.output())

