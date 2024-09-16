MODULE_TOPDIR = ../..

PGM = r.texture
NFM = -ffp-contract=fast

LIBES = $(RASTERLIB) $(GISLIB) $(MATHLIB) $(OPENMP_LIBPATH) $(OPENMP_LIB)
DEPENDENCIES = $(RASTERDEP) $(GISDEP)
EXTRA_CFLAGS = $(OPENMP_CFLAGS) $(NFM)
EXTRA_INC = $(OPENMP_INCPATH)

include $(MODULE_TOPDIR)/include/Make/Module.make

default: cmd
