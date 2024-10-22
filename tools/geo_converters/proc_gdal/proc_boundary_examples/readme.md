Examples of processing country boundaries.

(1) Process USA boundary:

COMMAND:
```
../proc_gdal -templ templates/templ_proc_boundary_us_50km.txt >| data_proc_boundary_us_50km.txt
```

Reads file: ```/mnt/nfs/rat_transfer/country_bdy/cb_2018_us_nation_5m.shp```

Creates file: ```proc_boundary_us_50km.kml```

(2) Process Canada boundary:

COMMAND:
```
../proc_gdal -templ templates/templ_proc_boundary_ca_50km.txt >| data_proc_boundary_ca_50km.txt
```

Reads file: ```/mnt/nfs/rat_transfer/country_bdy/lpr_000b21a_e_wgs84.shp```

Creates file: ```proc_boundary_ca_50km.kml```

The program reads a shape file downloaded from an official website and converts the shapefile to a kml file that can be used with afc-engine.  Note that, in general, downloaded shape files contain extremely large number of vertices making them unsuitable for use with afc-engine.  The afc-engine requires a polygon that encompasses the desired simulation region, and extends across the regions boundary into adjacent regions within some reasonable tolerance.  The key template parameters that control the processing of the region polygon are:

```SAMPLES_PER_DEG```: This controls the resolution at which polygons are sampled.  120 points per degree is 30 arcsec which corresponds to about 900 meters.

```POLYGON_EXPANSION```: This controls how polygons are expanded to extend across the boundary.

```POLYGON_SIMPLIFY```: This controls the polygon simplification algorithm where vertices are removed from the polygon.  

