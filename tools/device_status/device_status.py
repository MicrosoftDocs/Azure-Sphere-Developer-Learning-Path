import requests
import json
import time
import os
from datetime import datetime


def datetime_from_utc_to_local(utc_datetime):
    now_timestamp = time.time()
    offset = datetime.fromtimestamp(now_timestamp) - datetime.utcfromtimestamp(now_timestamp)
    return utc_datetime + offset

devices = {}

# Generate DEVICE_TWIN_AUTHORIZATION with
# az iot hub generate-sas-token --hub-name "hub name" --duration 8640000 (eg 8640000 = 100 days)


authorization = os.environ["DEVICE_TWIN_AUTHORIZATION"]
hub_name = os.environ["DEVICE_TWIN_HUB_NAME"]

api_version_device_twin = "2018-06-30"
api_version_list_devices = "2020-05-31-preview"

url_list_devices = f"https://{hub_name}.azure-devices.net/devices?api-version={api_version_list_devices}"
headers = {'Authorization': authorization}

device_list_json_binary = requests.get(url_list_devices, headers=headers)


device_list_json = device_list_json_binary.content.decode('utf8')

device_list = json.loads(device_list_json)

for device in device_list:
    # if the device has a long device id then we'll assume it's an Azure Sphere device
    if len(device["deviceId"]) > 60:

        print(device["deviceId"])

        # Get the device twin
        url_device_twin = f'https://{hub_name}.azure-devices.net/twins/{device["deviceId"]}?api-version={api_version_device_twin}'
        twin_json_binary = requests.get(url_device_twin, headers=headers)

        twin_json_string = twin_json_binary.content.decode('utf8')
        twin_json = json.loads(twin_json_string)
        reported_properties = twin_json["properties"]["reported"]
        heartbeatUtc = reported_properties.get("HeartbeatUtc")
        deviceStartUtc = reported_properties.get("DeviceStartUtc")
        softwareVersion = reported_properties.get("SoftwareVersion")

        if heartbeatUtc is None:
            print("Missing heartbeat")
        else:
            heartbeatLocal = datetime.fromisoformat(heartbeatUtc[:-1])
            deviceStartLocal = datetime.fromisoformat(deviceStartUtc[:-1])

            print(f'{twin_json["connectionState"]}, {datetime_from_utc_to_local(heartbeatLocal)}')
            devices[device["deviceId"]] = [twin_json["connectionState"], 
                datetime_from_utc_to_local(deviceStartLocal).strftime("%Y/%m/%d, %H:%M:%S"),
                datetime_from_utc_to_local(heartbeatLocal).strftime("%Y/%m/%d, %H:%M:%S"),
                softwareVersion ]

        print()


with open('devices.json', 'w') as fp:
    json.dump(devices, fp)
