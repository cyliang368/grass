#!/usr/bin/env python3

############################################################################
#
# MODULE:       v.import
#
# AUTHOR(S):    Markus Metz
#
# PURPOSE:      Import and reproject vector data on the fly
#
# COPYRIGHT:    (C) 2015 by GRASS development team
#
#               This program is free software under the GNU General
#               Public License (>=v2). Read the file COPYING that
#               comes with GRASS for details.
#
#############################################################################

# %module
# % description: Imports vector data into a GRASS vector map using OGR library and reprojects on the fly.
# % keyword: vector
# % keyword: import
# % keyword: projection
# %end
# %option
# % key: input
# % type: string
# % required: yes
# % description: Name of OGR datasource to be imported
# % gisprompt: old,datasource,datasource
# % guisection: Input
# %end
# %option
# % key: layer
# % type: string
# % multiple: yes
# % description: OGR layer name. If not given, all available layers are imported
# % guisection: Input
# % gisprompt: old,datasource_layer,datasource_layer
# %end
# %option G_OPT_V_OUTPUT
# % description: Name for output vector map (default: input)
# % required: no
# % guisection: Output
# %end
# %option
# % key: extent
# % type: string
# % options: input,region
# % answer: input
# % description: Output vector map extent
# % descriptions: input;extent of input map;region;extent of current region
# % guisection: Output
# %end
# %option
# % key: encoding
# % type: string
# % label: Encoding value for attribute data
# % descriptions: Overrides encoding interpretation, useful when importing ESRI Shapefile
# % guisection: Output
# %end
# %option
# % key: snap
# % type: double
# % label: Snapping threshold for boundaries (map units)
# % description: A suitable threshold is estimated during import
# % answer: -1
# % guisection: Output
# %end
# %option
# % key: epsg
# % type: integer
# % options: 1-1000000
# % guisection: Input SRS
# % description: EPSG projection code
# %end
# %option
# % key: datum_trans
# % type: integer
# % options: -1-100
# % guisection: Input SRS
# % label: Index number of datum transform parameters
# % description: -1 to list available datum transform parameters
# %end
# %flag
# % key: f
# % description: List supported OGR formats and exit
# % suppress_required: yes
# %end
# %flag
# % key: l
# % description: List available OGR layers in data source and exit
# %end
# %flag
# % key: o
# % label: Override projection check (use current project's CRS)
# % description: Assume that the dataset has the same coordinate reference system (CRS) as the current project
# %end

import sys
import os
import atexit
import xml.etree.ElementTree as ET  # only needed for GDAL version < 2.4.1
import re  # only needed for GDAL version < 2.4.1

import grass.script as gs
from grass.exceptions import CalledModuleError

# initialize global vars
TMPLOC = None
SRCGISRC = None
TGTGISRC = None
GISDBASE = None


def cleanup():
    if TGTGISRC:
        os.environ["GISRC"] = str(TGTGISRC)
    # remove temp location
    if TMPLOC:
        gs.try_rmdir(os.path.join(GISDBASE, TMPLOC))
    if SRCGISRC:
        gs.try_remove(SRCGISRC)


def gdal_version():
    """Returns the GDAL version as tuple"""
    return gs.parse_command("g.version", flags="reg")["gdal"]


def GDAL_COMPUTE_VERSION(maj, min, rev):
    return (maj) * 1000000 + (min) * 10000 + (rev) * 100


def is_projection_matching(OGRdatasource, layer):
    """Returns True if current location projection
    matches dataset projection, otherwise False"""
    try:
        gs.run_command(
            "v.in.ogr", input=OGRdatasource, layer=layer, flags="j", quiet=True
        )
        return True
    except CalledModuleError:
        return False


def fix_gfsfile(input):
    """Fixes the gfs file of an gml file by adding the SRSName

    :param input: gml file name to import with v.import
    :type input: str
    """
    # get srs string from gml file
    gmltree = ET.parse(input)
    gmlroot = gmltree.getroot()
    gmlstring = ET.tostring(gmlroot).decode("utf-8")
    srs_str = re.search(r"srsname=\"(.*?)\"", gmlstring.lower()).groups()[0]

    # set srs string in gfs file
    gml = os.path.basename(input).split(".")[-1]
    gfsfile = input.replace(gml, "gfs")
    if os.path.isfile(gfsfile):
        tree = ET.parse(gfsfile)
        root = tree.getroot()
        gfsstring = ET.tostring(root).decode("utf-8")
        if "srsname" not in gfsstring.lower():
            for featClass in root.findall("GMLFeatureClass"):
                ET.SubElement(featClass, "SRSName").text = srs_str
            tree.write(gfsfile)


def main():
    global TMPLOC, SRCGISRC, TGTGISRC, GISDBASE
    overwrite = gs.overwrite()

    # list formats and exit
    if flags["f"]:
        gs.run_command("v.in.ogr", flags="f")
        return 0

    # list layers and exit
    if flags["l"]:
        try:
            gs.run_command("v.in.ogr", flags="l", input=options["input"])
        except CalledModuleError:
            return 1
        return 0

    OGRdatasource = options["input"]
    output = options["output"]
    layers = options["layer"]

    vflags = ""
    if options["extent"] == "region":
        vflags += "r"
    if flags["o"]:
        vflags += "o"

    vopts = {}
    if options["encoding"]:
        vopts["encoding"] = options["encoding"]

    if options["datum_trans"] and options["datum_trans"] == "-1":
        # list datum transform parameters
        if not options["epsg"]:
            gs.fatal(_("Missing value for parameter <%s>") % "epsg")

        return gs.run_command(
            "g.proj", epsg=options["epsg"], datum_trans=options["datum_trans"]
        )

    if layers:
        vopts["layer"] = layers
    if output:
        vopts["output"] = output
    vopts["snap"] = options["snap"]

    # try v.in.ogr directly
    if flags["o"] or is_projection_matching(OGRdatasource, layers):
        try:
            gs.run_command(
                "v.in.ogr",
                input=OGRdatasource,
                flags=vflags,
                overwrite=overwrite,
                **vopts,
            )
            gs.message(
                _("Input <%s> successfully imported without reprojection")
                % OGRdatasource
            )
            return 0
        except CalledModuleError:
            gs.fatal(_("Unable to import <%s>") % OGRdatasource)

    grassenv = gs.gisenv()
    tgtloc = grassenv["LOCATION_NAME"]

    # make sure target is not xy
    if (
        gs.parse_command("g.proj", flags="p", format="shell")["name"]
        == "xy_location_unprojected"
    ):
        gs.fatal(
            _("Coordinate reference system not available for current project <%s>")
            % tgtloc
        )

    tgtmapset = grassenv["MAPSET"]
    GISDBASE = grassenv["GISDBASE"]
    TGTGISRC = os.environ["GISRC"]
    SRCGISRC = gs.tempfile()

    TMPLOC = gs.append_node_pid("tmp_v_import_location")

    f = open(SRCGISRC, "w")
    f.write("MAPSET: PERMANENT\n")
    f.write("GISDBASE: %s\n" % GISDBASE)
    f.write("LOCATION_NAME: %s\n" % TMPLOC)
    f.write("GUI: text\n")
    f.close()

    # create temp location from input without import
    gs.verbose(_("Creating temporary project for <%s>...") % OGRdatasource)
    try:
        if OGRdatasource.lower().endswith("gml"):
            try:
                from osgeo import gdal
            except ImportError:
                gs.fatal(
                    _(
                        "Unable to load GDAL Python bindings (requires package "
                        "'python-gdal' being installed)"
                    )
                )
            if int(gdal.VersionInfo("VERSION_NUM")) < GDAL_COMPUTE_VERSION(2, 4, 1):
                fix_gfsfile(OGRdatasource)
        gs.run_command(
            "v.in.ogr",
            input=OGRdatasource,
            project=TMPLOC,
            flags="i",
            quiet=True,
            overwrite=overwrite,
            **vopts,
        )
    except CalledModuleError:
        gs.fatal(_("Unable to create project from OGR datasource <%s>") % OGRdatasource)

    # switch to temp location
    os.environ["GISRC"] = str(SRCGISRC)

    if options["epsg"]:  # force given EPSG
        kwargs = {}
        if options["datum_trans"]:
            kwargs["datum_trans"] = options["datum_trans"]
        gs.run_command("g.proj", flags="c", epsg=options["epsg"], **kwargs)

    # print projection at verbose level
    gs.verbose(gs.read_command("g.proj", flags="p").rstrip(os.linesep))

    # make sure input is not xy
    if (
        gs.parse_command("g.proj", flags="p", format="shell")["name"]
        == "xy_location_unprojected"
    ):
        gs.fatal(
            _("Coordinate reference system not available for input <%s>")
            % OGRdatasource
        )

    if options["extent"] == "region":
        # switch to target location
        os.environ["GISRC"] = str(TGTGISRC)

        # v.in.region in tgt
        vreg = gs.append_node_pid("tmp_v_import_region")

        gs.run_command("v.in.region", output=vreg, quiet=True)

        # reproject to src
        # switch to temp location
        os.environ["GISRC"] = str(SRCGISRC)
        try:
            gs.run_command(
                "v.proj",
                input=vreg,
                output=vreg,
                project=tgtloc,
                mapset=tgtmapset,
                quiet=True,
                overwrite=overwrite,
            )
        except CalledModuleError:
            gs.fatal(_("Unable to reproject to source project"))

        # set region from region vector
        gs.run_command("g.region", res="1")
        gs.run_command("g.region", vector=vreg)

    # import into temp location
    gs.message(_("Importing <%s> ...") % OGRdatasource)
    try:
        if OGRdatasource.lower().endswith("gml"):
            try:
                from osgeo import gdal
            except ImportError:
                gs.fatal(
                    _(
                        "Unable to load GDAL Python bindings (requires package "
                        "'python-gdal' being installed)"
                    )
                )
            if int(gdal.VersionInfo("VERSION_NUM")) < GDAL_COMPUTE_VERSION(2, 4, 1):
                fix_gfsfile(OGRdatasource)
        gs.run_command(
            "v.in.ogr", input=OGRdatasource, flags=vflags, overwrite=overwrite, **vopts
        )
    except CalledModuleError:
        gs.fatal(_("Unable to import OGR datasource <%s>") % OGRdatasource)

    # if output is not define check source mapset
    if not output:
        output = gs.list_grouped("vector")["PERMANENT"][0]

    # switch to target location
    os.environ["GISRC"] = str(TGTGISRC)

    # check if map exists
    if (
        not gs.overwrite()
        and gs.find_file(output, element="vector", mapset=".")["mapset"]
    ):
        gs.fatal(
            _("option <{key}>: <{value}> exists.").format(key="output", value=output)
        )

    if options["extent"] == "region":
        gs.run_command("g.remove", type="vector", name=vreg, flags="f", quiet=True)

    # v.proj
    gs.message(_("Reprojecting <%s>...") % output)
    try:
        gs.run_command(
            "v.proj",
            project=TMPLOC,
            mapset="PERMANENT",
            input=output,
            overwrite=overwrite,
        )
    except CalledModuleError:
        gs.fatal(_("Unable to to reproject vector <%s>") % output)

    return 0


if __name__ == "__main__":
    options, flags = gs.parser()
    atexit.register(cleanup)
    sys.exit(main())
