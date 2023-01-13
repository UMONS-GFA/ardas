from ardas import sensor_tools as st
import import paho.mqtt.client as mqtt
import logging

# connexion to InfluxDB
DATABASE = {
    'host': '',
    'port': 8086,
    'user': '',
    'password': '',
    'dbname': '',
    'series': ''
}

# ArDAS configuration
ARDAS_CONFIG = {
    'station': '0001',  # 4 characters
    'net_id': '255',   # 3 characters
    'shield_id': '', # 3 characters
    'integration_period': '0001', # 4 characters convertible to int
    'tty': '/dev/ttyACM0',
    'raw_data_on_disk': False  # to implement in raspardas
}

# ArDAS sensors objects connected to the ArDAS channels
SENSORS = (st.load_sensor('0001'), st.load_sensor('0002'), st.load_sensor('0003'), st.load_sensor('0004'))

# connection to master
MASTER_CONFIG = {
    'local_host': '0.0.0.0',
    'local_port': 10001
}

# messages logging configuration
LOGGING_CONFIG ={
    'debug_mode': False,
    'logging_to_console': False,
    'file_name': 'raspardas_log',
    'max_bytes': 262144,
    'backup_count': 30,
    'when': 'd',
    'interval': 1
}

# data logging configuration
DATA_LOGGING_CONFIG ={
    'logging_level': logging.INFO,
    'file_name': 'data_log',
    'max_bytes': 16777216,
    'backup_count': 1024,
    'when': 'd',
    'interval': 1,
    'influxdb_logging': True
}


# MQTT logging config
MQTT_LOGGING_CONFIG = {
    'hostname': '',
    'port': 1883,
    'qos': 2,
    'retain': False,
    'keepalive': 60,
    'will': None,
    'auth': { 
        'username': '',
        'password': ''
    },
    'tls': None,
    'protocol': MQTTv31,
    'transport': 'tcp',
    'client_id': ARDAS_CONFIG["station"] + '-' + ARDAS_CONFIG["shield_id"],
    'exec_topic': 'ardas_' + ARDAS_CONFIG["station"] + '-' + ARDAS_CONFIG["shield_id"] + '/exec',
    'data_topic': 'ardas_' + ARDAS_CONFIG["station"] + '-' + ARDAS_CONFIG["shield_id"] + '/data',
    'soh_topic': 'ardas_' + ARDAS_CONFIG["station"] + '-' + ARDAS_CONFIG["shield_id"] + '/soh'
}

# parameters to push code to raspberry via sftp_transfer.py
SFTP = {
}
