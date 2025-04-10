## DESCRIPTION

*v.lidar.growing* is the second of three steps to filter LiDAR data. The
filter aims to recognize and extract attached and detached object (such
as buildings, bridges, power lines, trees, etc.) in order to create a
Digital Terrain Model.  
  
The modules identifies which is the internal area of every object on a
LiDAR point surface. The classification categories from
*v.lidar.edgedetection* are now rasterized. For each cell, it is
evaluated if it (the cell) contains a point with double impulse
(difference between the first and last pulse greater than a given
threshold). Starting from cells classified as OBJECT and with only one
pulse all linked cells are selected and a convex hull algorithm is
applied to them. Simultaneously, the mean of the corresponding heights
(mean edge height) are computed. Points inside the convex hull are
classified as OBJECT if their height is greater than or equal to the
previously mean computed edge height. This last step is done only in
case of high planimetric resolution.

## NOTES

The input data should be the output result of the
*v.lidar.edgedetection*, module. Otherwise, it goes to error! The output
of this module will be the input of *v.lidar.correction* module. The
output will be a vector map which points are pre-classified as:  
  
TERRAIN SINGLE PULSE (cat = 1, layer = 2)  
TERRAIN DOUBLE PULSE (cat = 2, layer = 2)  
OBJECT SINGLE PULSE (cat = 3, layer = 2)  
OBJECT DOUBLE PULSE (cat = 4, layer = 2)  
  
The final result of the whole procedure (*v.lidar.edgedetection*,
*v.lidar.growing*, *v.lidar.correction*) will be a point classification
in the same categories as above.

## EXAMPLES

### Basic region growing procedure

```sh
v.lidar.growing input=edge output=growing first=firstpulse
```

## REFERENCES

Antolin, R. et al., 2006. Digital terrain models determination by LiDAR
technology: Po basin experimentation. Bolletino di Geodesia e Scienze
Affini, anno LXV, n. 2, pp. 69-89.

Brovelli M. A., Cannata M., Longoni U.M., 2004. LIDAR Data Filtering and
DTM Interpolation Within GRASS, Transactions in GIS, April 2004, vol. 8,
iss. 2, pp. 155-174(20), Blackwell Publishing Ltd.

Brovelli M. A., Cannata M., 2004. Digital Terrain model reconstruction
in urban areas from airborne laser scanning data: the method and an
example for Pavia (Northern Italy). Computers and Geosciences 30 (2004)
pp.325-331

Brovelli M. A. and Longoni U.M., 2003. Software per il filtraggio di
dati LIDAR, Rivista dell?Agenzia del Territorio, n. 3-2003, pp. 11-22
(ISSN 1593-2192).

Brovelli M. A., Cannata M. and Longoni U.M., 2002. DTM LIDAR in area
urbana, Bollettino SIFET N.2, pp. 7-26.

Performances of the filter can be seen in the [ISPRS WG III/3 Comparison
of Filters](https://www.itc.nl/isprs/wgIII-3/filtertest/) report by
Sithole, G. and Vosselman, G., 2003.

## SEE ALSO

*[v.lidar.edgedetection](v.lidar.edgedetection.md),
[v.lidar.correction](v.lidar.correction.md),
[v.surf.bspline](v.surf.bspline.md), [v.surf.rst](v.surf.rst.md),
[v.in.pdal](v.in.pdal.md), [v.in.ascii](v.in.ascii.md)*

## AUTHORS

Original version of program in GRASS 5.4:  
Maria Antonia Brovelli, Massimiliano Cannata, Ulisse Longoni and Mirko
Reguzzoni

Update for GRASS 6.X:  
Roberto Antolin and Gonzalo Moreno
