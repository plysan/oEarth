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

transformer = Transformer.from_crs("epsg:4326", "epsg:3857")
tl_x, tl_y = transformer.transform(tl_lat, tl_lng)
br_x, br_y = transformer.transform(br_lat, br_lng)
del_x = br_x - tl_x
del_y = tl_y - br_y
res_x = 1024
res_y = int(res_x * del_y / del_x)
if res_y > 1280:
    res_y = 1280
    res_x = int(res_y * del_x / del_y)

print("res_x: %s, res_y: %s" % (res_x, res_y))

url = "https://api.mapbox.com/styles/v1/mapbox/satellite-v9/static/[%s,%s,%s,%s]/%sx%s?access_token=%s"
url = url % (tl_lng, br_lat, br_lng, tl_lat, res_x, res_y, sys.argv[4])
print("url: " + url)
data = requests.get(url)
fname = "%s_%.17g_%.17g" % (level, tl_lat, tl_lng)
open(fname + "_mercator.jpg", 'wb').write(data.content)
os.system('gdal_translate -of Gtiff -co "tfw=yes" -a_ullr %g %g %g %g -a_srs "EPSG:3857" %s_mercator.jpg %s_mercator.tiff' % (tl_x, tl_y, br_x, br_y, fname, fname))
os.system("rm -fv %s.jpg" % fname)
os.system("gdalwarp -s_srs EPSG:3857 -t_srs EPSG:4326 -ts 1024 1024 %s_mercator.tiff %s.jpg" % (fname, fname))
os.system("rm -fv %s_mercator* %s.jpg.*" % (fname, fname))


