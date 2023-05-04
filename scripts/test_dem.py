import os
import sys
import requests
from pyproj import Transformer

level = int(sys.argv[1])
span = 180.0 / (2**level)
tl_lat = float(sys.argv[2])
tl_lat_mod = tl_lat % span
if tl_lat_mod > 0:
    tl_lat = tl_lat - tl_lat_mod + span
tl_lng = float(sys.argv[3])
tl_lng_mod = tl_lng % span
if tl_lng_mod > 0:
    tl_lng = tl_lng - tl_lng_mod
br_lat = tl_lat - span
br_lng = tl_lng + span

print("level: %s, lat: %.17g~%.17g, lng: %.17g~%.17g" % (level, br_lat, tl_lat, tl_lng, br_lng))


