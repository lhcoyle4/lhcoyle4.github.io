import urllib.request
import json
import math
import random

# Fix random seed for reproducibility of contour lines noise
random.seed(42)

def add_noise_to_line(points, noise_level=0.08, detail_steps=3):
    """
    Subdivide segments and add high-frequency noise to make contours look detailed/organic.
    """
    current_points = list(points)
    for _ in range(detail_steps):
        next_points = []
        for i in range(len(current_points) - 1):
            p1 = current_points[i]
            p2 = current_points[i+1]
            mid_x = (p1[0] + p2[0]) / 2.0
            mid_y = (p1[1] + p2[1]) / 2.0
            
            # Distance between points
            dist = math.sqrt((p2[0]-p1[0])**2 + (p2[1]-p1[1])**2)
            # Offset perpendicular to segment
            angle = math.atan2(p2[1]-p1[1], p2[0]-p1[0]) + math.pi/2.0
            offset = (random.random() - 0.5) * dist * noise_level
            
            mid_x += math.cos(angle) * offset
            mid_y += math.sin(angle) * offset
            
            next_points.append(p1)
            next_points.append((mid_x, mid_y))
        next_points.append(current_points[-1])
        current_points = next_points
    return current_points

# 1. Fetch States GeoJSON
print("Downloading states GeoJSON...")
url = "https://raw.githubusercontent.com/PublicaMundi/MappingAPI/master/data/geojson/us-states.json"
response = urllib.request.urlopen(url)
states_data = json.loads(response.read().decode('utf-8'))

# Filter states to coterminous US (exclude AK, HI, PR)
excluded_states = {"Alaska", "Hawaii", "Puerto Rico"}
filtered_features = [f for f in states_data['features'] if f['properties']['name'] not in excluded_states]

# Compile states coordinates
flat_lons = []
flat_lats = []
parts = [] # (start_index, count)
states = [] # (name, part_start, part_count)

for feat in filtered_features:
    name = feat['properties']['name']
    geom = feat['geometry']
    gtype = geom['type']
    
    part_start = len(parts)
    
    if gtype == 'Polygon':
        for ring in geom['coordinates']:
            start_idx = len(flat_lons)
            for pt in ring:
                flat_lons.append(pt[0])
                flat_lats.append(pt[1])
            count = len(flat_lons) - start_idx
            parts.append((start_idx, count))
    elif gtype == 'MultiPolygon':
        for poly in geom['coordinates']:
            for ring in poly:
                start_idx = len(flat_lons)
                for pt in ring:
                    flat_lons.append(pt[0])
                    flat_lats.append(pt[1])
                count = len(flat_lons) - start_idx
                parts.append((start_idx, count))
                
    part_count = len(parts) - part_start
    states.append((name, part_start, part_count))

# 2. Define Interstate Highways
cities = {
    "Seattle": (-122.33, 47.60), "Portland": (-122.68, 45.52), "Sacramento": (-121.49, 38.58),
    "San Francisco": (-122.42, 37.77), "Los Angeles": (-118.24, 34.05), "San Diego": (-117.16, 32.72),
    "Las Vegas": (-115.14, 36.17), "Salt Lake City": (-111.89, 40.76), "Phoenix": (-112.07, 33.45),
    "Tucson": (-110.97, 32.22), "El Paso": (-106.49, 31.76), "Denver": (-104.99, 39.73),
    "Albuquerque": (-106.65, 35.08), "Amarillo": (-101.83, 35.22), "Oklahoma City": (-97.52, 35.47),
    "Wichita": (-97.33, 37.69), "Dallas": (-96.79, 32.78), "San Antonio": (-98.49, 29.42),
    "Houston": (-95.36, 29.76), "Laredo": (-99.51, 27.50), "Kansas City": (-94.58, 39.10),
    "Omaha": (-95.94, 41.26), "Des Moines": (-93.61, 41.60), "Minneapolis": (-93.26, 44.98),
    "Duluth": (-92.11, 46.78), "St. Louis": (-90.20, 38.63), "Little Rock": (-92.29, 34.75),
    "Memphis": (-90.05, 35.15), "New Orleans": (-90.07, 29.95), "Chicago": (-87.63, 41.88),
    "Detroit": (-83.05, 42.33), "Cleveland": (-81.69, 41.50), "Columbus": (-83.00, 39.96),
    "Cincinnati": (-84.51, 39.10), "Indianapolis": (-86.16, 39.77), "Nashville": (-86.78, 36.16),
    "Atlanta": (-84.39, 33.75), "Tampa": (-82.46, 27.95), "Miami": (-80.19, 25.76),
    "Jacksonville": (-81.66, 30.33), "Savannah": (-81.09, 32.08), "Richmond": (-77.44, 37.54),
    "Washington DC": (-77.03, 38.90), "Baltimore": (-76.61, 39.29), "Philadelphia": (-75.16, 39.95),
    "New York": (-74.00, 40.71), "Boston": (-71.06, 42.36), "Portland ME": (-70.25, 43.66),
    "Buffalo": (-78.88, 42.89), "Pittsburgh": (-79.99, 40.44), "Great Falls": (-111.30, 47.50),
    "Reno": (-119.81, 39.53), "Cheyenne": (-104.82, 41.14), "Spokane": (-117.42, 47.66),
    "Billings": (-108.50, 45.78), "Sioux Falls": (-96.73, 43.54), "Flagstaff": (-111.65, 35.20),
    "Knoxville": (-83.92, 35.96), "New Haven": (-72.92, 41.31)
}

interstates_paths = {
    "I-5": ["Seattle", "Portland", "Sacramento", "Los Angeles", "San Diego"],
    "I-10": ["Los Angeles", "Phoenix", "Tucson", "El Paso", "San Antonio", "Houston", "New Orleans", "Jacksonville"],
    "I-15": ["San Diego", "Las Vegas", "Salt Lake City", "Great Falls"],
    "I-35": ["Laredo", "San Antonio", "Dallas", "Oklahoma City", "Wichita", "Kansas City", "Des Moines", "Minneapolis", "Duluth"],
    "I-40": ["Los Angeles", "Flagstaff", "Albuquerque", "Amarillo", "Oklahoma City", "Little Rock", "Memphis", "Nashville", "Atlanta", "Jacksonville"],
    "I-70": ["Salt Lake City", "Denver", "Kansas City", "St. Louis", "Indianapolis", "Columbus", "Pittsburgh", "Baltimore"],
    "I-75": ["Miami", "Tampa", "Atlanta", "Knoxville", "Cincinnati", "Detroit"],
    "I-80": ["San Francisco", "Sacramento", "Reno", "Salt Lake City", "Cheyenne", "Omaha", "Chicago", "Cleveland", "New York"],
    "I-90": ["Seattle", "Spokane", "Billings", "Sioux Falls", "Chicago", "Cleveland", "Buffalo", "Boston"],
    "I-95": ["Miami", "Jacksonville", "Savannah", "Richmond", "Washington DC", "Baltimore", "Philadelphia", "New York", "New Haven", "Boston", "Portland ME"]
}

# Add noise/wiggles to interstates so they don't look like straight lines
interstates_coords = {}
for name, path in interstates_paths.items():
    points = [cities[c] for c in path]
    wiggled = add_noise_to_line(points, noise_level=0.03, detail_steps=2)
    interstates_coords[name] = wiggled

# 3. Generate Procedural Contours
contours = [] # list of (name, points)

# Appalachians: 1000m and 1500m
app_base = [(-86.0, 34.0), (-84.0, 36.0), (-81.0, 38.0), (-79.0, 41.0), (-75.0, 43.0), (-71.0, 45.0), (-68.0, 47.0)]
app_1000 = add_noise_to_line(app_base, noise_level=0.06, detail_steps=3)
contours.append(("Appalachian 1000m", app_1000))

app_base_1500 = [(-84.0, 35.0), (-82.0, 37.0), (-80.0, 40.0), (-74.0, 44.0)]
app_1500 = add_noise_to_line(app_base_1500, noise_level=0.04, detail_steps=3)
contours.append(("Appalachian 1500m", app_1500))

# Rockies: 2000m, 3000m, 4000m
rock_base_2000 = [(-105.0, 35.0), (-106.0, 39.0), (-109.0, 42.0), (-111.0, 45.0), (-115.0, 48.0)]
rock_2000 = add_noise_to_line(rock_base_2000, noise_level=0.05, detail_steps=4)
contours.append(("Rockies 2000m", rock_2000))

rock_base_3000_1 = [(-106.0, 36.0), (-106.5, 40.0), (-110.0, 43.0), (-113.0, 47.0)]
rock_3000_1 = add_noise_to_line(rock_base_3000_1, noise_level=0.04, detail_steps=4)
contours.append(("Rockies 3000m Ridge A", rock_3000_1))

rock_base_3000_2 = [(-112.0, 35.0), (-114.0, 39.0), (-116.0, 44.0), (-118.0, 48.0)]
rock_3000_2 = add_noise_to_line(rock_base_3000_2, noise_level=0.04, detail_steps=4)
contours.append(("Rockies 3000m Ridge B", rock_3000_2))

rock_base_4000 = [(-106.2, 37.0), (-105.8, 39.5), (-109.5, 41.5)]
rock_4000 = add_noise_to_line(rock_base_4000, noise_level=0.03, detail_steps=4)
contours.append(("Rockies Peak 4000m", rock_4000))

# Cascades & Sierra Nevada: 2500m and 3500m
casc_base_2500 = [(-117.0, 33.0), (-119.0, 36.0), (-120.0, 39.0), (-121.5, 43.0), (-121.8, 48.5)]
casc_2500 = add_noise_to_line(casc_base_2500, noise_level=0.05, detail_steps=4)
contours.append(("Cascades & Sierras 2500m", casc_2500))

casc_base_3500 = [(-118.2, 35.5), (-119.5, 38.0), (-121.7, 44.0), (-121.9, 47.0)]
casc_3500 = add_noise_to_line(casc_base_3500, noise_level=0.04, detail_steps=4)
contours.append(("Cascades & Sierras 3500m", casc_3500))

# 4. Fetch Rivers GeoJSON
print("Downloading rivers GeoJSON...")
url_rivers = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/ne_50m_rivers_lake_centerlines.geojson"
req_rivers = urllib.request.Request(url_rivers, headers={'User-Agent': 'Mozilla/5.0'})
response_rivers = urllib.request.urlopen(req_rivers)
rivers_data = json.loads(response_rivers.read().decode('utf-8'))

# Filter rivers intersecting coterminous US bounding box
us_rivers_features = []
for feat in rivers_data['features']:
    geom = feat['geometry']
    gtype = geom['type']
    coords = geom['coordinates']
    
    pts = []
    if gtype == 'LineString':
        pts = coords
    elif gtype == 'MultiLineString':
        for line in coords:
            pts.extend(line)
            
    in_us = any(-125.0 <= pt[0] <= -65.0 and 25.0 <= pt[1] <= 50.0 for pt in pts)
    if in_us:
        us_rivers_features.append(feat)

# Compile rivers coordinates
river_lons = []
river_lats = []
river_parts = [] # (start_index, count)
rivers = [] # (name, part_start, part_count)

for feat in us_rivers_features:
    name = feat['properties'].get('name', 'Unnamed')
    if name is None:
        name = 'Unnamed'
    geom = feat['geometry']
    gtype = geom['type']
    
    part_start = len(river_parts)
    
    if gtype == 'LineString':
        start_idx = len(river_lons)
        for pt in geom['coordinates']:
            river_lons.append(pt[0])
            river_lats.append(pt[1])
        count = len(river_lons) - start_idx
        if count >= 2:
            river_parts.append((start_idx, count))
    elif gtype == 'MultiLineString':
        for line in geom['coordinates']:
            start_idx = len(river_lons)
            for pt in line:
                river_lons.append(pt[0])
                river_lats.append(pt[1])
            count = len(river_lons) - start_idx
            if count >= 2:
                river_parts.append((start_idx, count))
                
    part_count = len(river_parts) - part_start
    if part_count > 0:
        rivers.append((name, part_start, part_count))

# 5. Generate C++ Header file
header_path = "src/map_data.h"
print(f"Writing map data to {header_path}...")

with open(header_path, 'w', encoding='utf-8') as f:
    f.write("// map_data.h\n")
    f.write("// Automatically generated high-resolution vector layers for the US Map Viewer\n\n")
    f.write("#pragma once\n\n")
    
    # Write States boundary flat points
    f.write(f"const float US_States_Lon[] = {{\n")
    for i, lon in enumerate(flat_lons):
        f.write(f"    {lon:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const float US_States_Lat[] = {{\n")
    for i, lat in enumerate(flat_lats):
        f.write(f"    {lat:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    f.write(f"const int US_States_Total_Points = {len(flat_lons)};\n\n")
    
    # Write State parts list
    f.write("struct MapPart {\n    int start_index;\n    int count;\n};\n\n")
    f.write(f"const MapPart US_States_Parts[] = {{\n")
    for p in parts:
        f.write(f"    {{ {p[0]}, {p[1]} }},\n")
    f.write("};\n\n")
    
    # Write States structure list
    f.write("struct MapStateFeature {\n    const char* name;\n    int part_start;\n    int part_count;\n};\n\n")
    f.write(f"const MapStateFeature US_States[] = {{\n")
    for s in states:
        f.write(f'    {{ "{s[0]}", {s[1]}, {s[2]} }},\n')
    f.write("};\n\n")
    f.write(f"const int US_States_Count = {len(states)};\n\n")
    
    # Write Interstate Highways
    f.write("// Interstate Highways\n")
    f.write("struct HighwayPart {\n    const char* name;\n    int start_index;\n    int count;\n};\n\n")
    
    # Compile flat lists for highways
    hw_lons = []
    hw_lats = []
    hw_parts = []
    
    for name, pts in interstates_coords.items():
        start_idx = len(hw_lons)
        for pt in pts:
            hw_lons.append(pt[0])
            hw_lats.append(pt[1])
        count = len(hw_lons) - start_idx
        hw_parts.append((name, start_idx, count))
        
    f.write(f"const float US_Highways_Lon[] = {{\n")
    for i, lon in enumerate(hw_lons):
        f.write(f"    {lon:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const float US_Highways_Lat[] = {{\n")
    for i, lat in enumerate(hw_lats):
        f.write(f"    {lat:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const HighwayPart US_Highways[] = {{\n")
    for hp in hw_parts:
        f.write(f'    {{ "{hp[0]}", {hp[1]}, {hp[2]} }},\n')
    f.write("};\n\n")
    f.write(f"const int US_Highways_Count = {len(hw_parts)};\n\n")
    
    # Write Contours
    f.write("// Topographic Contours\n")
    f.write("struct ContourPart {\n    const char* name;\n    int start_index;\n    int count;\n};\n\n")
    
    ct_lons = []
    ct_lats = []
    ct_parts = []
    
    for name, pts in contours:
        start_idx = len(ct_lons)
        for pt in pts:
            ct_lons.append(pt[0])
            ct_lats.append(pt[1])
        count = len(ct_lons) - start_idx
        ct_parts.append((name, start_idx, count))
        
    f.write(f"const float US_Contours_Lon[] = {{\n")
    for i, lon in enumerate(ct_lons):
        f.write(f"    {lon:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const float US_Contours_Lat[] = {{\n")
    for i, lat in enumerate(ct_lats):
        f.write(f"    {lat:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const ContourPart US_Contours[] = {{\n")
    for cp in ct_parts:
        f.write(f'    {{ "{cp[0]}", {cp[1]}, {cp[2]} }},\n')
    f.write("};\n\n")
    f.write(f"const int US_Contours_Count = {len(ct_parts)};\n\n")

    # Write Rivers
    f.write("// Hydrographic Rivers\n")
    f.write("struct RiverPart {\n    int start_index;\n    int count;\n};\n\n")
    f.write("struct MapRiverFeature {\n    const char* name;\n    int part_start;\n    int part_count;\n};\n\n")
    
    f.write(f"const float US_Rivers_Lon[] = {{\n")
    for i, lon in enumerate(river_lons):
        f.write(f"    {lon:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const float US_Rivers_Lat[] = {{\n")
    for i, lat in enumerate(river_lats):
        f.write(f"    {lat:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const int US_Rivers_Total_Points = {len(river_lons)};\n\n")
    
    f.write(f"const RiverPart US_Rivers_Parts[] = {{\n")
    for p in river_parts:
        f.write(f"    {{ {p[0]}, {p[1]} }},\n")
    f.write("};\n\n")
    
    f.write(f"const MapRiverFeature US_Rivers[] = {{\n")
    for r in rivers:
        clean_name = r[0].replace('"', '\\"')
        f.write(f'    {{ "{clean_name}", {r[1]}, {r[2]} }},\n')
    f.write("};\n\n")
    f.write(f"const int US_Rivers_Count = {len(rivers)};\n\n")

print("C++ map_data.h successfully generated!")

