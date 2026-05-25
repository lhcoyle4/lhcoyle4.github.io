import urllib.request
import json
import math
import random

# Fix random seed for reproducibility of contour lines noise
random.seed(42)

def add_noise_to_line(points, noise_level=0.08, detail_steps=3):
    """
    Subdivide segments and add high-frequency noise to make lines look detailed/organic.
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
url_states = "https://raw.githubusercontent.com/PublicaMundi/MappingAPI/master/data/geojson/us-states.json"
response = urllib.request.urlopen(url_states)
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

# 2. Fetch World Boundaries & US National Boundary
print("Downloading 50m world countries GeoJSON...")
url_countries = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/ne_50m_admin_0_countries.geojson"
req = urllib.request.Request(url_countries, headers={'User-Agent': 'Mozilla/5.0'})
response_countries = urllib.request.urlopen(req)
countries_data = json.loads(response_countries.read().decode('utf-8'))

us_border_lons = []
us_border_lats = []
us_border_parts = []

world_lons = []
world_lats = []
world_parts = []
world_countries = []

for feat in countries_data['features']:
    name = feat['properties'].get('NAME', feat['properties'].get('name', 'Unnamed'))
    if name is None:
        name = 'Unnamed'
    geom = feat['geometry']
    gtype = geom['type']
    
    is_us = (name == 'United States of America' or name == 'United States')
    
    rings_list = []
    if gtype == 'Polygon':
        for ring in geom['coordinates']:
            rings_list.append(ring)
    elif gtype == 'MultiPolygon':
        for poly in geom['coordinates']:
            for ring in poly:
                rings_list.append(ring)
                
    if is_us:
        alaska_parts = []
        hawaii_parts = []
        other_us_parts = []
        for ring in rings_list:
            is_coterminous = any(-126.0 <= pt[0] <= -65.0 and 24.0 <= pt[1] <= 50.0 for pt in ring)
            if is_coterminous:
                start_idx = len(us_border_lons)
                for pt in ring:
                    us_border_lons.append(pt[0])
                    us_border_lats.append(pt[1])
                count = len(us_border_lons) - start_idx
                us_border_parts.append((start_idx, count))
            else:
                # Add Alaska/Hawaii/territories to world countries so they render as background
                start_idx = len(world_lons)
                for idx, pt in enumerate(ring):
                    if idx % 2 == 0 or idx == len(ring) - 1:
                        world_lons.append(pt[0])
                        world_lats.append(pt[1])
                count = len(world_lons) - start_idx
                if count >= 3:
                    # Classify ring based on average coordinates
                    lons = [pt[0] for pt in ring]
                    lats = [pt[1] for pt in ring]
                    avg_lon = sum(lons) / len(lons)
                    avg_lat = sum(lats) / len(lats)
                    if avg_lat > 50.0:
                        alaska_parts.append((start_idx, count))
                    elif avg_lat < 30.0 and avg_lon < -140.0:
                        hawaii_parts.append((start_idx, count))
                    else:
                        other_us_parts.append((start_idx, count))
                        
        if alaska_parts:
            part_start = len(world_parts)
            for p in alaska_parts:
                world_parts.append(p)
            world_countries.append(("Alaska", part_start, len(alaska_parts)))
            
        if hawaii_parts:
            part_start = len(world_parts)
            for p in hawaii_parts:
                world_parts.append(p)
            world_countries.append(("Hawaii", part_start, len(hawaii_parts)))
            
        if other_us_parts:
            part_start = len(world_parts)
            for p in other_us_parts:
                world_parts.append(p)
            world_countries.append(("United States", part_start, len(other_us_parts)))
    else:
        part_start = len(world_parts)
        for ring in rings_list:
            start_idx = len(world_lons)
            # Down-sample slightly (every 2nd point) to keep world file small
            for idx, pt in enumerate(ring):
                if idx % 2 == 0 or idx == len(ring) - 1:
                    world_lons.append(pt[0])
                    world_lats.append(pt[1])
            count = len(world_lons) - start_idx
            if count >= 3:
                world_parts.append((start_idx, count))
        part_count = len(world_parts) - part_start
        if part_count > 0:
            world_countries.append((name, part_start, part_count))

# 3. Fetch 50m Lakes (Detailed)
print("Downloading 50m lakes GeoJSON...")
url_lakes = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/ne_50m_lakes.geojson"
req_lakes = urllib.request.Request(url_lakes, headers={'User-Agent': 'Mozilla/5.0'})
response_lakes = urllib.request.urlopen(req_lakes)
lakes_data = json.loads(response_lakes.read().decode('utf-8'))

lake_lons = []
lake_lats = []
lake_parts = []
lake_features = []

for feat in lakes_data['features']:
    name = feat['properties'].get('name', 'Unnamed')
    if name is None:
        name = 'Unnamed'
    geom = feat['geometry']
    gtype = geom['type']
    coords = geom['coordinates']
    
    pts = []
    rings_list = []
    if gtype == 'Polygon':
        for ring in coords:
            pts.extend(ring)
            rings_list.append(ring)
    elif gtype == 'MultiPolygon':
        for poly in coords:
            for ring in poly:
                pts.extend(ring)
                rings_list.append(ring)
                
    # Filter for lakes intersecting coterminous US bounding box
    in_us = any(-125.0 <= pt[0] <= -65.0 and 25.0 <= pt[1] <= 50.0 for pt in pts)
    if in_us:
        part_start = len(lake_parts)
        for ring in rings_list:
            start_idx = len(lake_lons)
            for pt in ring:
                lake_lons.append(pt[0])
                lake_lats.append(pt[1])
            count = len(lake_lons) - start_idx
            if count >= 3:
                lake_parts.append((start_idx, count))
        part_count = len(lake_parts) - part_start
        if part_count > 0:
            lake_features.append((name, part_start, part_count))

# 4. Fetch 50m Rivers
print("Downloading 50m rivers GeoJSON...")
url_rivers = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/ne_50m_rivers_lake_centerlines.geojson"
req_rivers = urllib.request.Request(url_rivers, headers={'User-Agent': 'Mozilla/5.0'})
response_rivers = urllib.request.urlopen(req_rivers)
rivers_data = json.loads(response_rivers.read().decode('utf-8'))

river_lons = []
river_lats = []
river_parts = []
rivers = []

for feat in rivers_data['features']:
    name = feat['properties'].get('name', 'Unnamed')
    if name is None:
        name = 'Unnamed'
    geom = feat['geometry']
    gtype = geom['type']
    
    pts = []
    parts_list = []
    if gtype == 'LineString':
        pts = geom['coordinates']
        parts_list.append(pts)
    elif gtype == 'MultiLineString':
        for line in geom['coordinates']:
            pts.extend(line)
            parts_list.append(line)
            
    in_us = any(-125.0 <= pt[0] <= -65.0 and 25.0 <= pt[1] <= 50.0 for pt in pts)
    if in_us:
        part_start = len(river_parts)
        for line in parts_list:
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

# 5. Fetch US Power Stations from WRI Global Database CSV
print("Downloading complete US power stations from WRI CSV Database...")
url_pp = "https://raw.githubusercontent.com/wri/global-power-plant-database/master/output_database/global_power_plant_database.csv"
req_pp = urllib.request.Request(url_pp, headers={'User-Agent': 'Mozilla/5.0'})
response_pp = urllib.request.urlopen(req_pp)
lines = [line.decode('utf-8') for line in response_pp.readlines()]
import csv
reader = csv.DictReader(lines)

power_plants = [] # (name, lon, lat, fuel_type, capacity_MW)
for row in reader:
    if row.get('country') == 'USA':
        try:
            cap = float(row.get('capacity_mw', 0.0) or 0.0)
        except ValueError:
            cap = 0.0
        # Filter for major plants (>= 700 MW) to have a dense but high-performance dataset covering the entire nation
        if cap >= 700.0:
            name = row.get('name', 'Unnamed Plant')
            fuel = row.get('primary_fuel', 'UNKNOWN')
            
            # Map fuel types to fit C++ backend color mapping
            if fuel == 'Nuclear': fuel_code = 'NUC'
            elif fuel == 'Hydro': fuel_code = 'HYC'
            elif fuel == 'Coal': fuel_code = 'COL'
            elif fuel in ('Gas', 'Oil'): fuel_code = 'GAS'
            else: fuel_code = 'OTH'
            
            try:
                lon = float(row.get('longitude', 0.0) or 0.0)
                lat = float(row.get('latitude', 0.0) or 0.0)
                power_plants.append((name, lon, lat, fuel_code, cap))
            except ValueError:
                pass

# 6. Define Cities for Networking
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

# 7. Generate wiggled railroads
print("Generating wiggled railroads...")
railroads_paths = {
    "BNSF Transcon": ["Chicago", "Kansas City", "Amarillo", "Albuquerque", "Flagstaff", "Los Angeles"],
    "Union Pacific Overland": ["Chicago", "Omaha", "Cheyenne", "Salt Lake City", "Reno", "Sacramento", "San Francisco"],
    "CSX Dixie Line": ["Chicago", "Cincinnati", "Knoxville", "Atlanta", "Jacksonville", "Miami"],
    "NS Northeast Corridor": ["Boston", "New York", "Philadelphia", "Baltimore", "Washington DC", "Richmond", "Atlanta", "New Orleans"],
    "Sunset Route": ["Los Angeles", "Phoenix", "Tucson", "El Paso", "San Antonio", "Houston", "New Orleans"],
    "Midwest Rail": ["Minneapolis", "Sioux Falls", "Omaha", "Kansas City", "Wichita", "Oklahoma City", "Dallas", "Houston"],
    "Cascades Rail": ["Seattle", "Portland", "Sacramento"],
    "Great Northern Rail": ["Seattle", "Spokane", "Great Falls", "Billings", "Minneapolis", "Chicago"],
    "Heartland Rail": ["Detroit", "Chicago", "Indianapolis", "St. Louis", "Kansas City"],
    "Rustbelt Rail": ["New York", "Pittsburgh", "Cleveland", "Detroit", "Chicago"],
    "Atlantic Coast Rail": ["Richmond", "Savannah", "Jacksonville", "Tampa", "Miami"],
    "Gulf Coast Rail": ["Houston", "New Orleans", "Tampa"],
    "Texas Express": ["Laredo", "San Antonio", "Dallas", "Oklahoma City"]
}
railroads_coords = {}
for name, path in railroads_paths.items():
    points = [cities[c] for c in path]
    wiggled = add_noise_to_line(points, noise_level=0.02, detail_steps=2)
    railroads_coords[name] = wiggled

# 8. Generate energy pipelines
print("Generating energy pipelines...")
pipeline_paths = {
    "Transcontinental Gas Pipe": ["Houston", "New Orleans", "Atlanta", "Savannah", "Richmond", "Washington DC", "Philadelphia", "New York"],
    "El Paso Natural Gas": ["Dallas", "El Paso", "Tucson", "Phoenix", "Los Angeles"],
    "Northern Border Pipeline": ["Billings", "Sioux Falls", "Omaha", "Chicago"],
    "Rockies Express Pipeline": ["Denver", "Cheyenne", "Omaha", "Chicago", "Cleveland", "Pittsburgh"],
    "Gulf Coast Gas Pipeline": ["Laredo", "San Antonio", "Houston", "New Orleans", "Tampa", "Miami"],
    "Marcellus Energy Pipe": ["Pittsburgh", "Buffalo", "New York", "Boston"],
    "Pacific Gas Pipe": ["Seattle", "Portland", "Sacramento", "San Francisco"],
    "Midcontinent Gas Pipe": ["Amarillo", "Oklahoma City", "Wichita", "Kansas City", "St. Louis", "Indianapolis", "Detroit"]
}
pipeline_coords = {}
for name, path in pipeline_paths.items():
    points = [cities[c] for c in path]
    wiggled = add_noise_to_line(points, noise_level=0.03, detail_steps=2)
    pipeline_coords[name] = wiggled

# 9. Generate energy corridors (HV transmission lines)
print("Generating energy corridors...")
corridor_paths = {
    "Western Grid Backbone A": ["Seattle", "Spokane", "Great Falls", "Billings", "Denver"],
    "Western Grid Backbone B": ["Seattle", "Portland", "Sacramento", "San Francisco", "Los Angeles", "San Diego"],
    "Western Grid Backbone C": ["San Francisco", "Reno", "Salt Lake City", "Las Vegas", "Los Angeles"],
    "Western Grid Backbone D": ["Los Angeles", "Phoenix", "Tucson", "El Paso"],
    "Eastern Grid Backbone A": ["Chicago", "Indianapolis", "Cincinnati", "Knoxville", "Atlanta", "Jacksonville", "Tampa", "Miami"],
    "Eastern Grid Backbone B": ["Chicago", "Detroit", "Cleveland", "Pittsburgh", "Philadelphia", "New York", "Boston"],
    "Eastern Grid Backbone C": ["Atlanta", "Savannah", "Richmond", "Washington DC", "Baltimore", "Philadelphia"],
    "Eastern Grid Backbone D": ["Minneapolis", "Sioux Falls", "Omaha", "Kansas City", "St. Louis", "Memphis", "New Orleans"],
    "Texas ERCOT Backbone A": ["Houston", "San Antonio", "Laredo"],
    "Texas ERCOT Backbone B": ["Houston", "Dallas", "San Antonio"],
    "Texas ERCOT Backbone C": ["Dallas", "Amarillo"]
}
corridor_coords = {}
for name, path in corridor_paths.items():
    points = [cities[c] for c in path]
    wiggled = add_noise_to_line(points, noise_level=0.02, detail_steps=2)
    corridor_coords[name] = wiggled

# 10. Generate US Highways (Secondary roads)
print("Generating U.S. highways...")
us_highways_paths = {
    "US-101": ["Seattle", "Portland", "Sacramento", "San Francisco", "Los Angeles"],
    "US-1": ["Portland ME", "Boston", "New Haven", "New York", "Philadelphia", "Baltimore", "Washington DC", "Richmond", "Savannah", "Jacksonville", "Miami"],
    "US-66": ["Chicago", "St. Louis", "Little Rock", "Oklahoma City", "Amarillo", "Albuquerque", "Flagstaff", "Las Vegas", "Los Angeles"],
    "US-30": ["Portland", "Seattle", "Spokane", "Salt Lake City", "Cheyenne", "Omaha", "Des Moines", "Chicago", "Pittsburgh", "Philadelphia", "New York"],
    "US-20": ["Seattle", "Spokane", "Billings", "Sioux Falls", "Minneapolis", "Chicago", "Detroit", "Buffalo", "Boston"],
    "US-50": ["San Francisco", "Sacramento", "Reno", "Salt Lake City", "Denver", "Kansas City", "St. Louis", "Cincinnati", "Columbus", "Washington DC"]
}
us_highways_coords = {}
for name, path in us_highways_paths.items():
    points = [cities[c] for c in path]
    wiggled = add_noise_to_line(points, noise_level=0.025, detail_steps=2)
    us_highways_coords[name] = wiggled

# 11. Generate Substations
print("Generating substations...")
substations = [] # (name, lon, lat)
for idx, (name, coord) in enumerate(cities.items()):
    substations.append((f"Substation {name} North", coord[0] + 0.12, coord[1] + 0.08))
    substations.append((f"Substation {name} South", coord[0] - 0.08, coord[1] - 0.12))
for idx, (name, lon, lat, fuel, cap) in enumerate(power_plants):
    if cap > 500.0: # Only major ones
        substations.append((f"Substation PP {name[:15]}", lon + 0.05, lat - 0.05))

# 12. Interstate Highways (Roads)
print("Generating interstate highways...")
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
interstates_coords = {}
for name, path in interstates_paths.items():
    points = [cities[c] for c in path]
    wiggled = add_noise_to_line(points, noise_level=0.03, detail_steps=2)
    interstates_coords[name] = wiggled

# 13. Generate Procedural Contours
print("Generating topographic contours...")
contours = [] # list of (name, points)
app_base = [(-86.0, 34.0), (-84.0, 36.0), (-81.0, 38.0), (-79.0, 41.0), (-75.0, 43.0), (-71.0, 45.0), (-68.0, 47.0)]
app_1000 = add_noise_to_line(app_base, noise_level=0.06, detail_steps=3)
contours.append(("Appalachian 1000m", app_1000))

app_base_1500 = [(-84.0, 35.0), (-82.0, 37.0), (-80.0, 40.0), (-74.0, 44.0)]
app_1500 = add_noise_to_line(app_base_1500, noise_level=0.04, detail_steps=3)
contours.append(("Appalachian 1500m", app_1500))

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

casc_base_2500 = [(-117.0, 33.0), (-119.0, 36.0), (-120.0, 39.0), (-121.5, 43.0), (-121.8, 48.5)]
casc_2500 = add_noise_to_line(casc_base_2500, noise_level=0.05, detail_steps=4)
contours.append(("Cascades & Sierras 2500m", casc_2500))

casc_base_3500 = [(-118.2, 35.5), (-119.5, 38.0), (-121.7, 44.0), (-121.9, 47.0)]
casc_3500 = add_noise_to_line(casc_base_3500, noise_level=0.04, detail_steps=4)
contours.append(("Cascades & Sierras 3500m", casc_3500))

# 14. Generate C++ Header file
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
    
    # Write US Detailed Outline Boundary
    f.write(f"const float US_Border_Lon[] = {{\n")
    for i, lon in enumerate(us_border_lons):
        f.write(f"    {lon:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const float US_Border_Lat[] = {{\n")
    for i, lat in enumerate(us_border_lats):
        f.write(f"    {lat:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const MapPart US_Border_Parts[] = {{\n")
    for p in us_border_parts:
        f.write(f"    {{ {p[0]}, {p[1]} }},\n")
    f.write("};\n\n")
    f.write(f"const int US_Border_Parts_Count = {len(us_border_parts)};\n\n")
    f.write(f"const int US_Border_Total_Points = {len(us_border_lons)};\n\n")

    # Write World Boundaries
    f.write(f"const float World_Countries_Lon[] = {{\n")
    for i, lon in enumerate(world_lons):
        f.write(f"    {lon:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const float World_Countries_Lat[] = {{\n")
    for i, lat in enumerate(world_lats):
        f.write(f"    {lat:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const MapPart World_Countries_Parts[] = {{\n")
    for p in world_parts:
        f.write(f"    {{ {p[0]}, {p[1]} }},\n")
    f.write("};\n\n")
    
    f.write("struct MapWorldFeature {\n    const char* name;\n    int part_start;\n    int part_count;\n};\n\n")
    f.write(f"const MapWorldFeature World_Countries[] = {{\n")
    for wc in world_countries:
        clean_name = wc[0].replace('"', '\\"')
        f.write(f'    {{ "{clean_name}", {wc[1]}, {wc[2]} }},\n')
    f.write("};\n\n")
    f.write(f"const int World_Countries_Count = {len(world_countries)};\n\n")

    # Write Detailed Lakes
    f.write(f"const float US_Lakes_Lon[] = {{\n")
    for i, lon in enumerate(lake_lons):
        f.write(f"    {lon:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const float US_Lakes_Lat[] = {{\n")
    for i, lat in enumerate(lake_lats):
        f.write(f"    {lat:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const MapPart US_Lakes_Parts[] = {{\n")
    for p in lake_parts:
        f.write(f"    {{ {p[0]}, {p[1]} }},\n")
    f.write("};\n\n")
    
    f.write("struct MapLakeFeature {\n    const char* name;\n    int part_start;\n    int part_count;\n};\n\n")
    f.write(f"const MapLakeFeature US_Lakes[] = {{\n")
    for lf in lake_features:
        clean_name = lf[0].replace('"', '\\"')
        f.write(f'    {{ "{clean_name}", {lf[1]}, {lf[2]} }},\n')
    f.write("};\n\n")
    f.write(f"const int US_Lakes_Count = {len(lake_features)};\n\n")

    # Write Interstate Highways
    f.write("// Interstate Highways (Roads)\n")
    f.write("struct HighwayPart {\n    const char* name;\n    int start_index;\n    int count;\n};\n\n")
    
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

    # Write Secondary US Highways (More Roads)
    f.write("// Secondary US Highways (More Roads)\n")
    ushw_lons = []
    ushw_lats = []
    ushw_parts = []
    for name, pts in us_highways_coords.items():
        start_idx = len(ushw_lons)
        for pt in pts:
            ushw_lons.append(pt[0])
            ushw_lats.append(pt[1])
        count = len(ushw_lons) - start_idx
        ushw_parts.append((name, start_idx, count))
        
    f.write(f"const float US_SecondaryHighways_Lon[] = {{\n")
    for i, lon in enumerate(ushw_lons):
        f.write(f"    {lon:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const float US_SecondaryHighways_Lat[] = {{\n")
    for i, lat in enumerate(ushw_lats):
        f.write(f"    {lat:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const HighwayPart US_SecondaryHighways[] = {{\n")
    for hp in ushw_parts:
        f.write(f'    {{ "{hp[0]}", {hp[1]}, {hp[2]} }},\n')
    f.write("};\n\n")
    f.write(f"const int US_SecondaryHighways_Count = {len(ushw_parts)};\n\n")

    # Write Railroads
    f.write("// Railroads\n")
    f.write("struct RailroadPart {\n    const char* name;\n    int start_index;\n    int count;\n};\n\n")
    
    rr_lons = []
    rr_lats = []
    rr_parts = []
    for name, pts in railroads_coords.items():
        start_idx = len(rr_lons)
        for pt in pts:
            rr_lons.append(pt[0])
            rr_lats.append(pt[1])
        count = len(rr_lons) - start_idx
        rr_parts.append((name, start_idx, count))
        
    f.write(f"const float US_Railways_Lon[] = {{\n")
    for i, lon in enumerate(rr_lons):
        f.write(f"    {lon:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const float US_Railways_Lat[] = {{\n")
    for i, lat in enumerate(rr_lats):
        f.write(f"    {lat:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const RailroadPart US_Railways[] = {{\n")
    for rp in rr_parts:
        f.write(f'    {{ "{rp[0]}", {rp[1]}, {rp[2]} }},\n')
    f.write("};\n\n")
    f.write(f"const int US_Railways_Count = {len(rr_parts)};\n\n")

    # Write Energy Pipelines
    f.write("// Energy Pipelines\n")
    f.write("struct PipelinePart {\n    const char* name;\n    int start_index;\n    int count;\n};\n\n")
    
    pl_lons = []
    pl_lats = []
    pl_parts = []
    for name, pts in pipeline_coords.items():
        start_idx = len(pl_lons)
        for pt in pts:
            pl_lons.append(pt[0])
            pl_lats.append(pt[1])
        count = len(pl_lons) - start_idx
        pl_parts.append((name, start_idx, count))
        
    f.write(f"const float US_Pipelines_Lon[] = {{\n")
    for i, lon in enumerate(pl_lons):
        f.write(f"    {lon:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const float US_Pipelines_Lat[] = {{\n")
    for i, lat in enumerate(pl_lats):
        f.write(f"    {lat:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const PipelinePart US_Pipelines[] = {{\n")
    for pp in pl_parts:
        f.write(f'    {{ "{pp[0]}", {pp[1]}, {pp[2]} }},\n')
    f.write("};\n\n")
    f.write(f"const int US_Pipelines_Count = {len(pl_parts)};\n\n")

    # Write Energy Corridors (HV lines)
    f.write("// HV Transmission Corridors\n")
    f.write("struct CorridorPart {\n    const char* name;\n    int start_index;\n    int count;\n};\n\n")
    
    ec_lons = []
    ec_lats = []
    ec_parts = []
    for name, pts in corridor_coords.items():
        start_idx = len(ec_lons)
        for pt in pts:
            ec_lons.append(pt[0])
            ec_lats.append(pt[1])
        count = len(ec_lons) - start_idx
        ec_parts.append((name, start_idx, count))
        
    f.write(f"const float US_EnergyCorridors_Lon[] = {{\n")
    for i, lon in enumerate(ec_lons):
        f.write(f"    {lon:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const float US_EnergyCorridors_Lat[] = {{\n")
    for i, lat in enumerate(ec_lats):
        f.write(f"    {lat:.5f}f,")
        if i % 10 == 9: f.write("\n")
    f.write("\n};\n\n")
    
    f.write(f"const CorridorPart US_EnergyCorridors[] = {{\n")
    for cp in ec_parts:
        f.write(f'    {{ "{cp[0]}", {cp[1]}, {cp[2]} }},\n')
    f.write("};\n\n")
    f.write(f"const int US_EnergyCorridors_Count = {len(ec_parts)};\n\n")

    # Write Power Stations
    f.write("// Power Stations\n")
    f.write("struct MapPowerStation {\n    const char* name;\n    float lon;\n    float lat;\n    const char* fuel;\n    float capacity;\n};\n\n")
    f.write(f"const MapPowerStation US_PowerStations[] = {{\n")
    for p in power_plants:
        clean_name = p[0].replace('"', '\\"')
        f.write(f'    {{ "{clean_name}", {p[1]:.5f}f, {p[2]:.5f}f, "{p[3]}", {p[4]:.2f}f }},\n')
    f.write("};\n\n")
    f.write(f"const int US_PowerStations_Count = {len(power_plants)};\n\n")

    # Write Substations
    f.write("// Substations\n")
    f.write("struct MapSubstation {\n    const char* name;\n    float lon;\n    float lat;\n};\n\n")
    f.write(f"const MapSubstation US_Substations[] = {{\n")
    for s in substations:
        f.write(f'    {{ "{s[0]}", {s[1]:.5f}f, {s[2]:.5f}f }},\n')
    f.write("};\n\n")
    f.write(f"const int US_Substations_Count = {len(substations)};\n\n")

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
