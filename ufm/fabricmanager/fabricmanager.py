# External Imports
import os
import os.path
import sys
import json
import traceback
import socket
import signal
import time
import argparse
import uuid
import datetime
from multiprocessing import Process
from subprocess import PIPE, Popen, STDOUT

import etcd3

from common.clusterlib import lib, lib_constants

# Flask Imports
from flask import Flask, request, make_response, render_template
from flask_restful import reqparse, Api, Resource

from common.ufmdb.redfish_ufmdb import redfish_ufmdb

# REST API Imports
from rest_api.resource_manager import ResourceManager
from rest_api.redfish.System_api import UfmdbSystemAPI
from rest_api.redfish.Fabric_api import UfmdbFabricAPI

# Internal Imports
import config
from common.ufmlog import ufmlog
from ufm_status import UfmStatus, UfmLeaderStatus, UfmHealthStatus

from rest_api.redfish.ServiceRoot import ServiceRoot
from rest_api.redfish.System_api import SystemCollectionEmulationAPI, SystemEmulationAPI
from rest_api.redfish.Storage import StorageCollectionEmulationAPI, StorageEmulationAPI
from rest_api.redfish.Drive import DriveCollectionEmulationAPI, DriveEmulationAPI
from rest_api.redfish.EthernetInterface import EthernetInterfaceCollectionEmulationAPI, EthernetInterfaceEmulationAPI

from rest_api.redfish.Fabric_api import FabricCollectionAPI, FabricAPI
from rest_api.redfish.Switch import SwitchCollection, SwitchApi
from rest_api.redfish.Port import PortCollection, Port
from rest_api.redfish.VLAN import VLANCollection, VLAN

from backend.populate import populate
from systems.switch.switch import Switch
from systems.smart.smart import Smart
from systems.essd.essd import Essd
from systems.ebof.ebof import Ebof
from systems.nkv.nkv import Nkv


# If application is installed, then append the module search path
global ufmlog
if os.path.dirname(os.path.realpath(__file__)) == "/usr/share/ufm":
    sys.path.append("/usr/share/ufm/")
    # This log path is created by the installer

    ufmlog.ufmlog.filename = "/var/log/ufm/ufm.log"
else:
    ufmlog.ufmlog.filename = "ufm.log"

log = ufmlog.log(module="main", mask=ufmlog.UFM_MAIN)

# Global variables to initialize resource manager
CONFIG_LOGFILE = '/tmp/fabricmanager-log.conf'

LOCAL_HOST = "127.0.0.1"

# Base URL of REST APIs
REST_BASE = '/redfish/v1/'

# Create Flask server
app = Flask(__name__)

# Create RESTful API
api = Api(app)

connect_to_single_db_node = False

# Time between each check of which FM is the lead
MASTER_CHECK_INTERVAL = 1

CLUSTER_TIMEOUT = (3 * 60)

g_is_main_running = True

# MODE: static, local or db
# MODE determines the data source for REST API resources
MODE = 'db'
local_path = 'backend/local-config.json'

# Server's arguments
kwargs = {}


def signal_handler(sig, frame):
    global g_is_main_running
    g_is_main_running = False


# End point defined for http://<host:port>/
@app.route("/")
def index():
    return render_template('index.html')


@api.representation('application/json')
def output_json(data, code, headers=None):
    """
    Overriding how JSON is returned by the server so that it looks nice
    """
    response = make_response(json.dumps(data, indent=4), code)
    response.headers.extend(headers or {})
    return response


def error_response(msg, status, jsonify=False):
    data = {
        'Status': status,
        'Message': '{}'.format(msg)
    }

    if jsonify:
        data = json.dumps(data, indent=4)

    return data, status


global resource_manager

# Industry Standard Software Defined Management for Converged, Hybrid IT,
# developed by DMTF group, that utilizes JSON schema or OData


class RedfishAPI(Resource):
    def __init__(self):
        super(RedfishAPI, self).__init__()

    """
    Return ServiceRoot
    """
    def get(self, path=None):
        try:
            if path is not None:
                config = RedfishAPI.load_configuration(resource_manager, path)
            else:
                config = resource_manager.configuration

            response = config, 200
        except Exception:
            traceback.print_exc()
            response = error_response('Internal Server Error', 500)

        return response

    # For any path, other than rest base
    @staticmethod
    def load_configuration(resource_manager, path):
        config = resource_manager.load_resource_dictionary(path)
        return config


def call_populate(local_path):
    """
    Construct dynamic resources using local data at backend/local-config.json
    """
    try:
        if os.path.exists(local_path) and os.path.isfile(local_path):
            with open(local_path, 'r') as f:
                local_config = json.load(f)
                populate(local_config['LOCAL'])

    except Exception as e:
        print(e)
        print('ERR: Reading Local Config File')


# For any other RESTful requests, navigate them to RedfishAPI object
# Note: <path:path> specifies any path
if (MODE is not None and MODE.lower() == 'db'):
    rfdb = redfish_ufmdb(root_uuid=str(uuid.uuid4()), auto_update=True)
    # api.add_resource(ServiceRoot, REST_BASE,
    #                  resource_class_kwargs={'rest_base': REST_BASE})
    api.add_resource(
        UfmdbSystemAPI, '/<path:path>', resource_class_kwargs={'rfdb': rfdb})
    api.add_resource(
        UfmdbFabricAPI, '/<path:path>', resource_class_kwargs={'rfdb': rfdb})
elif (MODE is not None and MODE.lower() == 'local'):
    api.add_resource(ServiceRoot, REST_BASE,
                     resource_class_kwargs={'rest_base': REST_BASE})
    api.add_resource(SystemCollectionEmulationAPI, REST_BASE + 'Systems')
    api.add_resource(SystemEmulationAPI, REST_BASE + 'Systems/<string:ident>')
    api.add_resource(StorageCollectionEmulationAPI, REST_BASE + 'Systems/<string:ident>/Storage',
                     resource_class_kwargs={'rest_base': REST_BASE, 'suffix': 'Systems'})
    api.add_resource(StorageEmulationAPI, REST_BASE + 'Systems/<string:ident1>/Storage/<string:ident2>',
                     resource_class_kwargs={'rest_base': REST_BASE})
    api.add_resource(DriveCollectionEmulationAPI, REST_BASE + 'Systems/<string:ident1>/Storage/<string:ident2>/Drives',
                     resource_class_kwargs={'rest_base': REST_BASE, 'suffix': 'Systems'})
    api.add_resource(DriveEmulationAPI, REST_BASE + 'Systems/<string:ident1>/Storage/<string:ident2>/Drives/<string:ident3>',
                     resource_class_kwargs={'rest_base': REST_BASE})
    api.add_resource(EthernetInterfaceCollectionEmulationAPI, REST_BASE + 'Systems/<string:ident>/EthernetInterfaces',
                     resource_class_kwargs={'rest_base': REST_BASE, 'suffix': 'Systems'})
    api.add_resource(EthernetInterfaceEmulationAPI, REST_BASE + 'Systems/<string:ident1>/EthernetInterfaces/<string:ident2>',
                     resource_class_kwargs={'rest_base': REST_BASE})


    api.add_resource(FabricCollectionAPI, REST_BASE + 'Fabrics')
    api.add_resource(FabricAPI, REST_BASE + 'Fabrics/<string:ident>')
    api.add_resource(SwitchCollection, REST_BASE + 'Fabrics/<string:ident>/Switches',
                     resource_class_kwargs={'rest_base': REST_BASE, 'suffix': 'Fabrics'})
    api.add_resource(SwitchApi, REST_BASE + 'Fabrics/<string:ident1>/Switches/<string:ident2>',
                     resource_class_kwargs={'rest_base': REST_BASE})
    api.add_resource(PortCollection, REST_BASE + 'Fabrics/<string:ident1>/Switches/<string:ident2>/Ports',
                     resource_class_kwargs={'rest_base': REST_BASE, 'suffix': 'Fabrics'})
    api.add_resource(Port, REST_BASE + 'Fabrics/<string:ident1>/Switches/<string:ident2>/Ports/<string:ident3>',
                     resource_class_kwargs={'rest_base': REST_BASE})
    api.add_resource(VLANCollection, REST_BASE + 'Fabrics/<string:ident1>/Switches/<string:ident2>/VLANs',
                     resource_class_kwargs={'rest_base': REST_BASE, 'suffix': 'Fabrics'})
    api.add_resource(VLAN, REST_BASE + 'Fabrics/<string:ident1>/Switches/<string:ident2>/VLANs/<string:ident3>',
                     resource_class_kwargs={'rest_base': REST_BASE})


    call_populate(local_path)
else:
    api.add_resource(RedfishAPI, REST_BASE,
                     REST_BASE + '<path:path>')
    try:
        resource_manager = ResourceManager(REST_BASE, MODE)
    except:
        log.error('Failed to Initialize Resource Manager')


def start_all_sub_systems(sub_systems):
    """
    Start all sub_systems and pass in a ref to logger
    """
    for sub in sub_systems:
        if not sub.is_running():
            sub.start()


def stop_all_sub_systems(sub_systems):
    for sub in sub_systems:
        sub.stop()


def connect_to_db(ip_address=None, logger=None):
    global lib

    try:
        return lib.create_db_connection(ip_address, log=logger)
    except:
        logger.error("Failed to connect to DB. Exiting")
        sys.exit(-1)


class StatusChangeCbArg(object):
    def __init__(self, subsystems, target, kwargs, log):
        self.subsystems = subsystems
        self.target = target
        self.kwargs = kwargs
        self.apiServer = None
        self.isRunning = False
        self.log = log


def serverStateChange(startup, cbArgs):
    if startup is True and cbArgs.isRunning is not True:
        cbArgs.log.info(f'serverStateChange: Starting up UFM')
        cbArgs.server = Process(target=cbArgs.target, kwargs=cbArgs.kwargs)
        cbArgs.server.start()
        start_all_sub_systems(cbArgs.subsystems)
        cbArgs.isRunning = True
    elif startup is not True and cbArgs.isRunning is True:
        cbArgs.log.info(f'serverStateChange: Shutting down UFM')
        cbArgs.server.terminate()
        cbArgs.server.join()
        stop_all_sub_systems(cbArgs.subsystems)
        cbArgs.isRunning = False


def onHealthChange(ufmHealthStatus, cbArgs):
    cbArgs.log.info(f'onHealthChange: {ufmHealthStatus}, isRunning: {cbArgs.isRunning}')
    if UfmHealthStatus.HEALTHY is not ufmHealthStatus:
        serverStateChange(False, cbArgs)


def onLeaderChange(ufmLeaderStatus, cbArgs):
    cbArgs.log.info(f'onLeaderChange: {ufmLeaderStatus}, isRunning: {cbArgs.isRunning}')
    if UfmLeaderStatus.LEADER is ufmLeaderStatus:
        serverStateChange(True, cbArgs)
    else:
        serverStateChange(False, cbArgs)


def main():
    """
    The master fabricmanager does all the work. The node that has the
    ETCD master(lead) also run the master fabricmanager.

    Note: The machines with the cluster must have names
    """
    log.log_detail_on()
    log.log_debug_on()
    log.info('===> FabricManager Startup <===')

    # To initialize REST API resources
    global CONFIG_LOGFILE
    global LOCAL_HOST
    global resource_manager

    log.detail('main: Setting up signal handler')
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGQUIT, signal_handler)

    log.detail('main: Parsing args')
    parser = argparse.ArgumentParser(
        description='Process Server\'s Configuration Settings.')
    parser.add_argument("--host", help="IP Address of Server",
                        dest="host", default="0.0.0.0")
    parser.add_argument("--port", help="Port of Server",
                        dest="port", default=5001)
    parser.add_argument("--logger_config", help="fabricmanager logger config file ",
                        dest="config_logfile", default=CONFIG_LOGFILE)

    args = parser.parse_args()

    log.detail('config_logfile: %s', args.config_logfile)

    # Default fabricmanager-config.json data. Override with user input data if any
    kwargs['host'] = args.host
    kwargs['port'] = args.port

    # Determine hostname
    # TODO: We can do this within each subsystem
    hostname = socket.gethostname()

    # Use this hostname when using snapshot data that Tom took
    # hostname = "msl-dc-client8"

    # Connect to database.
    #
    # This is legacy code and should be removed
    # when nkv-monitor is refactored.  All db code should go through
    # the ufmdb layer
    #
    # TODO: We can do this within the NKV subsystem
    db = connect_to_db(ip_address=LOCAL_HOST, logger=log)

    config.rest_base = REST_BASE

    # Create subsystem classes
    # TODO: Determine which subsystem to enable via config file or database
    sub_systems = (
        Nkv(hostname=hostname, db=db),
        Ebof()
        # Essd(hostname=hostname, db=db),
        # Smart(),
        # Switch()
    )

    #
    # ufm_status will handle all health and leader checks
    #
    # UfmStatus monitors cluster for health and determines leader
    status_cb_arg = StatusChangeCbArg(subsystems=sub_systems,
                                      target=app.run,
                                      kwargs=kwargs,
                                      log=log)

    ufm_status = UfmStatus(onHealthChangeCb=onHealthChange,
                           onHealthChangeCbArgs=status_cb_arg,
                           onLeaderChangeCb=onLeaderChange,
                           onLeaderChangeCbArgs=status_cb_arg)

    # Start watching status, need cbs and cb args
    ufm_status.start()

    global g_is_main_running
    while g_is_main_running:
        time.sleep(MASTER_CHECK_INTERVAL)

    ufm_status.stop()

    log.info(" ===> FabricManager Stopped! <===")


if __name__ == '__main__':
    main()
