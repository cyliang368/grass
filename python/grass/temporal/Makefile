MODULE_TOPDIR = ../../..

include $(MODULE_TOPDIR)/include/Make/Other.make
include $(MODULE_TOPDIR)/include/Make/Python.make

PYDIR = $(ETC)/python
GDIR = $(PYDIR)/grass
DSTDIR = $(GDIR)/temporal
DSTDIRPLY = $(DSTDIR)/ply

MODULES = ply/__init__ ply/lex ply/yacc base core abstract_dataset abstract_map_dataset abstract_space_time_dataset space_time_datasets open_stds factory gui_support list_stds register sampling metadata spatial_extent temporal_extent datetime_math temporal_granularity spatio_temporal_relationships unit_tests aggregation stds_export stds_import extract mapcalc univar_statistics temporal_topology_dataset_connector spatial_topology_dataset_connector c_libraries_interface temporal_algebra temporal_vector_algebra temporal_raster_base_algebra temporal_raster_algebra temporal_raster3d_algebra temporal_operator

CLEAN_SUBDIRS = ply

PYFILES := $(patsubst %,$(DSTDIR)/%.py,$(MODULES) __init__)
PYCFILES := $(patsubst %,$(DSTDIR)/%.pyc,$(MODULES) __init__)

default: $(PYFILES) $(PYCFILES) $(GDIR)/__init__.py $(GDIR)/__init__.pyc

$(PYDIR):
	$(MKDIR) $@

$(GDIR): | $(PYDIR)
	$(MKDIR) $@

$(DSTDIR): | $(GDIR)
	$(MKDIR) $@

$(DSTDIRPLY): | $(DSTDIR)
	$(MKDIR) $@

$(DSTDIR)/%: % | $(DSTDIR) $(DSTDIRPLY)
	$(INSTALL_DATA) $< $@
