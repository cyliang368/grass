/*
 * r.univar
 *
 *  Calculates univariate statistics from the non-null cells of a GRASS raster
 *  map
 *
 *   Copyright (C) 2004-2006, 2012 by the GRASS Development Team
 *   Author(s): Hamish Bowman, University of Otago, New Zealand
 *              Extended stats: Martin Landa
 *              Zonal stats: Markus Metz
 *
 *      This program is free software under the GNU General Public
 *      License (>=v2). Read the file COPYING that comes with GRASS
 *      for details.
 *
 *   This program is a replacement for the r.univar shell script
 */

#if defined(_OPENMP)
#include <omp.h>
#endif

#include <assert.h>
#include <string.h>
#include <float.h>
#include "globals.h"

param_type param;
zone_type zone_info;

/* Parallelization
 * Only raster statistics reduction in process_raster() is parallelized.
 * print_stats*() where sorting takes place for percentile computation are not.
 * We may try parallel sorting algorithms in the future for further speedup.
 */
typedef struct zone_bucket {
    size_t n;
    size_t n_alloc;
    void *nextp;
    CELL *cells;
    FCELL *fcells;
    DCELL *dcells;
} zone_bucket;

typedef struct zone_workspace {
    double sum;
    double sumsq;
    double sum_abs;
    size_t size;
    double min;
    double max;
    zone_bucket bucket;
} zone_workspace;

typedef struct thread_workspace {
    int fd;
    int fdz;
    void *raster_row;
    CELL *zoneraster_row;
} thread_workspace;

/* ************************************************************************* */
/* Set up the arguments we are expecting ********************************** */
/* ************************************************************************* */
void set_params(void)
{
    param.inputfile = G_define_standard_option(G_OPT_R_MAPS);

    param.zonefile = G_define_standard_option(G_OPT_R_MAP);
    param.zonefile->key = "zones";
    param.zonefile->required = NO;
    param.zonefile->description =
        _("Raster map used for zoning, must be of type CELL");

    param.output_file = G_define_standard_option(G_OPT_F_OUTPUT);
    param.output_file->required = NO;
    param.output_file->description =
        _("Name for output file (if omitted or \"-\" output to stdout)");
    param.output_file->guisection = _("Output settings");

    param.percentile = G_define_option();
    param.percentile->key = "percentile";
    param.percentile->type = TYPE_DOUBLE;
    param.percentile->required = NO;
    param.percentile->multiple = YES;
    param.percentile->options = "0-100";
    param.percentile->answer = "90";
    param.percentile->description =
        _("Percentile to calculate (requires extended statistics flag)");
    param.percentile->guisection = _("Extended");

    param.nprocs = G_define_standard_option(G_OPT_M_NPROCS);

    param.separator = G_define_standard_option(G_OPT_F_SEP);
    param.separator->answer = NULL;
    param.separator->guisection = _("Formatting");

    param.shell_style = G_define_flag();
    param.shell_style->key = 'g';
    param.shell_style->label =
        _("Print the stats in shell script style [deprecated]");
    param.shell_style->description = _(
        "This flag is deprecated and will be removed in a future release. Use "
        "format=shell instead.");
    param.shell_style->guisection = _("Formatting");

    param.extended = G_define_flag();
    param.extended->key = 'e';
    param.extended->description = _("Calculate extended statistics");
    param.extended->guisection = _("Extended");

    param.table = G_define_flag();
    param.table->key = 't';
    param.table->label =
        _("Table output format instead of standard output format [deprecated]");
    param.table->description = _(
        "This flag is deprecated and will be removed in a future release. Use "
        "format=csv instead.");
    param.table->guisection = _("Formatting");

    param.format = G_define_standard_option(G_OPT_F_FORMAT);
    param.format->options = "plain,shell,csv,json";
    param.format->descriptions = ("plain;Human readable text output;"
                                  "shell;shell script style text output;"
                                  "csv;CSV (Comma Separated Values);"
                                  "json;JSON (JavaScript Object Notation);");
    param.format->guisection = _("Print");

    param.use_rast_region = G_define_flag();
    param.use_rast_region->key = 'r';
    param.use_rast_region->description =
        _("Use the native resolution and extent of the raster map, instead of "
          "the current region");

    return;
}

static int open_raster(const char *infile);
static void process_raster(univar_stat *stats, thread_workspace *tw,
                           const struct Cell_head *region, int nprocs,
                           enum OutputFormat format);
static void kahan_sum(double *sum, double *c, double x);

/* *************************************************************** */
/* **** the main functions for r.univar ************************** */
/* *************************************************************** */
int main(int argc, char *argv[])
{
    int rasters;

    struct Cell_head region;
    struct GModule *module;
    univar_stat *stats;
    char **infile, *zonemap;
    int cell_type, min, max;
    struct Range zone_range;
    const char *mapset, *name;
    int t;

    enum OutputFormat format;

    G_gisinit(argv[0]);

    module = G_define_module();
    G_add_keyword(_("raster"));
    G_add_keyword(_("statistics"));
    G_add_keyword(_("univariate statistics"));
    G_add_keyword(_("zonal statistics"));
    G_add_keyword(_("parallel"));

    module->label = _("Calculates univariate statistics from the non-null "
                      "cells of a raster map.");
    module->description =
        _("Statistics include number of cells counted, minimum and maximum cell"
          " values, range, arithmetic mean, population variance, standard "
          "deviation,"
          " coefficient of variation, and sum.");

    /* Define the different options */
    set_params();

    if (G_parser(argc, argv))
        exit(EXIT_FAILURE);

    if (param.zonefile->answer && param.use_rast_region->answer) {
        G_fatal_error(
            _("zones option and region flag -r are mutually exclusive"));
    }

    name = param.output_file->answer;
    if (name != NULL && strcmp(name, "-") != 0) {
        if (NULL == freopen(name, "w", stdout)) {
            G_fatal_error(_("Unable to open file <%s> for writing"), name);
        }
    }

    /* For backward compatibility */
    if (!param.separator->answer) {
        if (strcmp(param.format->answer, "csv") == 0)
            param.separator->answer = "comma";
        else
            param.separator->answer = "pipe";
    }

    if (strcmp(param.format->answer, "json") == 0) {
        format = JSON;
    }
    else if (strcmp(param.format->answer, "shell") == 0) {
        format = SHELL;
    }
    else if (strcmp(param.format->answer, "csv") == 0) {
        format = CSV;
    }
    else {
        format = PLAIN;
    }

    if (param.shell_style->answer) {
        G_verbose_message(
            _("Flag 'g' is deprecated and will be removed in a future "
              "release. Please use format=shell instead."));
        if (format == JSON || format == CSV) {
            G_fatal_error(
                _("The -g flag cannot be used with format=json or format=csv. "
                  "Please select only one output format."));
        }
        format = SHELL;
    }

    if (param.table->answer) {
        G_verbose_message(
            _("Flag 't' is deprecated and will be removed in a future "
              "release. Please use format=csv instead."));
        if (format == JSON || format == SHELL) {
            G_fatal_error(_(
                "The -t flag cannot be used with format=json or format=shell. "
                "Please select only one output format."));
        }
        format = CSV;
    }

    /* set nprocs parameter */
    int nprocs;
    nprocs = G_set_omp_num_threads(param.nprocs);
    nprocs = Rast_disable_omp_on_mask(nprocs);
    if (nprocs < 1)
        G_fatal_error(_("<%d> is not valid number of nprocs."), nprocs);

    /* table field separator */
    zone_info.sep = G_option_to_separator(param.separator);

    zone_info.min = 0;
    zone_info.max = 0;
    zone_info.n_zones = 0;

    /* setting up thread workspace */
    thread_workspace *tw = G_malloc(nprocs * sizeof *tw);

    /* open zoning raster */
    if ((zonemap = param.zonefile->answer)) {
        mapset = G_find_raster2(zonemap, "");

        for (t = 0; t < nprocs; ++t)
            tw[t].fdz = open_raster(zonemap);

        cell_type = Rast_get_map_type(tw->fdz);
        if (cell_type != CELL_TYPE)
            G_fatal_error("Zoning raster must be of type CELL");

        if (Rast_read_range(zonemap, mapset, &zone_range) == -1)
            G_fatal_error("Can not read range for zoning raster");
        Rast_get_range_min_max(&zone_range, &min, &max);
        if (Rast_read_cats(zonemap, mapset, &(zone_info.cats)))
            G_warning("No category support for zoning raster");

        zone_info.min = min;
        zone_info.max = max;
        zone_info.n_zones = max - min + 1;
    }

    /* count the input rasters given */
    for (infile = (char **)param.inputfile->answers, rasters = 0; *infile;
         infile++, rasters++)
        ;

    /* process all input rasters */
    int map_type = param.extended->answer ? -2 : -1;

    stats = ((map_type == -1) ? create_univar_stat_struct(-1, 0) : 0);

    for (infile = param.inputfile->answers; *infile; infile++) {

        /* Check if the native extent and resolution
           of the input map should be used */
        if (param.use_rast_region->answer) {
            mapset = G_find_raster2(*infile, "");
            Rast_get_cellhd(*infile, mapset, &region);
            /* Set the computational region */
            Rast_set_window(&region);
        }
        else {
            G_get_window(&region);
        }

        for (t = 0; t < nprocs; t++)
            tw[t].fd = open_raster(*infile);

        if (map_type != -1) {
            /* NB: map_type must match when doing extended stats */
            int this_type = Rast_get_map_type(tw->fd);

            assert(this_type > -1);
            if (map_type < -1) {
                /* extended stats */
                assert(stats == 0);
                map_type = this_type;
                stats = univar_stat_with_percentiles(map_type);
            }
            else if (this_type != map_type) {
                G_fatal_error(_("Raster <%s> type mismatch"), *infile);
            }
        }

        process_raster(stats, tw, &region, nprocs, format);

        /* close input raster */
        for (t = 0; t < nprocs; t++)
            Rast_close(tw[t].fd);
    }

    /* close zoning raster */
    if (zonemap) {
        for (t = 0; t < nprocs; t++)
            Rast_close(tw[t].fdz);
    }

    /* create the output */
    if (format == CSV)
        print_stats_table(stats);
    else
        print_stats(stats, format);

    /* release memory */
    free_univar_stat_struct(stats);

    exit(EXIT_SUCCESS);
}

static int open_raster(const char *infile)
{
    const char *mapset;
    int fd;

    mapset = G_find_raster2(infile, "");
    if (mapset == NULL) {
        G_fatal_error(_("Raster map <%s> not found"), infile);
    }

    fd = Rast_open_old(infile, mapset);
    G_free((void *)mapset);

    return fd;
}

static void process_raster(univar_stat *stats, thread_workspace *tw,
                           const struct Cell_head *region, int nprocs,
                           enum OutputFormat format)
{
    /* use G_window_rows(), G_window_cols() here? */
    const int rows = region->rows;
    const int cols = region->cols;

    const RASTER_MAP_TYPE map_type = Rast_get_map_type(tw->fd);
    const size_t value_sz = Rast_cell_size(map_type);

    const int n_zones = zone_info.n_zones;
    const int n_alloc = n_zones ? n_zones : 1;

    /* initialize for KhanSum through rows */
    double c_sum = 0.0;
    double c_sumsq = 0.0;
    double c_sum_abs = 0.0;

    for (int t = 0; t < nprocs; t++) {
        tw[t].raster_row = Rast_allocate_buf(map_type);
        if (n_zones) {
            tw[t].zoneraster_row = Rast_allocate_c_buf();
        }
    }

#if defined(_OPENMP)
    omp_lock_t *minmax = G_malloc(n_alloc * sizeof *minmax);

    for (int z = 0; z < n_alloc; z++) {
        omp_init_lock(&minmax[z]);
    }
#endif

    int computed = 0;
    int row;

#pragma omp parallel private(row, c_sum, c_sumsq, c_sum_abs)
    {
        int t_id = 0;
        c_sum = 0;
        c_sumsq = 0;
        c_sum_abs = 0;
#if defined(_OPENMP)
        t_id = omp_get_thread_num();
#endif
        zone_workspace *zw = G_malloc(n_alloc * sizeof *zw);

        for (int z = 0; z < n_alloc; z++) {
            zone_workspace *zd = &zw[z];
            zd->sum = 0;
            zd->sumsq = 0;
            zd->sum_abs = 0;
            zd->size = 0;
            zd->min = DBL_MAX;
            zd->max = -DBL_MAX;
            zd->bucket.n = 0;
            zd->bucket.n_alloc = 0;
            zd->bucket.nextp = NULL;
            zd->bucket.cells = NULL;
            zd->bucket.fcells = NULL;
            zd->bucket.dcells = NULL;
        }

#pragma omp for
        for (row = 0; row < rows; row++) {
            thread_workspace *w = &tw[t_id];

            Rast_get_row(w->fd, w->raster_row, row, map_type);
            void *ptr = w->raster_row;

            CELL *zptr = NULL;
            if (n_zones) {
                Rast_get_c_row(w->fdz, w->zoneraster_row, row);
                zptr = w->zoneraster_row;
            }

            for (int col = 0; col < cols; col++) {
                int zone = 0;

                if (n_zones) {
                    /* skip NULL cells in zone map */
                    if (Rast_is_c_null_value(zptr)) {
                        ptr = G_incr_void_ptr(ptr, value_sz);
                        zptr++;
                        continue;
                    }
                    zone = *zptr - zone_info.min;
                }
                zone_workspace *zd = &zw[zone];

                /* count all including NULL cells in input map */
                zd->size++;

                /* can't do stats with NULL cells in input map */
                if (Rast_is_null_value(ptr, map_type)) {
                    ptr = G_incr_void_ptr(ptr, value_sz);
                    if (n_zones)
                        zptr++;
                    continue;
                }

                if (param.extended->answer) {
                    zone_bucket *bucket = &zd->bucket;

                    /* check allocated memory */
                    if (bucket->n >= bucket->n_alloc) {
                        bucket->n_alloc += 1000;
                        size_t msize;

                        switch (map_type) {
                        case DCELL_TYPE:
                            msize = bucket->n_alloc * sizeof(DCELL);
                            bucket->dcells = (DCELL *)G_realloc(
                                (void *)bucket->dcells, msize);
                            bucket->nextp =
                                (void *)&(bucket->dcells[bucket->n]);
                            break;
                        case FCELL_TYPE:
                            msize = bucket->n_alloc * sizeof(FCELL);
                            bucket->fcells = (FCELL *)G_realloc(
                                (void *)bucket->fcells, msize);
                            bucket->nextp =
                                (void *)&(bucket->fcells[bucket->n]);
                            break;
                        case CELL_TYPE:
                            msize = bucket->n_alloc * sizeof(CELL);
                            bucket->cells =
                                (CELL *)G_realloc((void *)bucket->cells, msize);
                            bucket->nextp = (void *)&(bucket->cells[bucket->n]);
                            break;
                        }
                    }
                    /* put the value into stats->XXXcell_array */
                    memcpy(bucket->nextp, ptr, value_sz);
                    bucket->nextp = G_incr_void_ptr(bucket->nextp, value_sz);
                }

                if ((map_type == DCELL_TYPE) || (map_type == FCELL_TYPE)) {
                    /* use Kaham sum for floating point */
                    double val = ((map_type == DCELL_TYPE) ? *((DCELL *)ptr)
                                                           : *((FCELL *)ptr));
                    kahan_sum(&zd->sum, &c_sum, val);
                    kahan_sum(&zd->sumsq, &c_sumsq, val * val);
                    kahan_sum(&zd->sum_abs, &c_sum_abs, fabs(val));
                    if (val > zd->max)
                        zd->max = val;
                    if (val < zd->min)
                        zd->min = val;
                }
                else if (map_type == CELL_TYPE) {
                    /* integer does not have floating point error */
                    int val = *((CELL *)ptr);
                    zd->sum += val;
                    zd->sumsq += val * val;
                    zd->sum_abs += abs(val);
                    if (val > zd->max)
                        zd->max = val;
                    if (val < zd->min)
                        zd->min = val;
                }
                else
                    G_fatal_error(_("Unknown map type"));

                ptr = G_incr_void_ptr(ptr, value_sz);
                if (n_zones)
                    zptr++;
                zd->bucket.n++;
            } /* end column loop */
            if (format != SHELL) {
#pragma omp atomic update
                computed++;
                G_percent(computed, rows, 2);
            }
        } /* end row loop */

        /* initialize for KhanSum through threads */
        c_sum = 0.0;
        c_sumsq = 0.0;
        c_sum_abs = 0.0;

        for (int z = 0; z < n_alloc; z++) {
            zone_workspace *zd = &zw[z];
            if (param.extended->answer) {
#pragma omp critical
                {
                    /*
                       Transfers for each thread from local bucket to global
                       buffer. Case 1: first transfer, skip reallocation and
                       point to the buffer. Case 2: other transfers, reallocate
                       exactly if there is any non-empty bucket.
                     */
                    zone_bucket *bucket = &zd->bucket;
                    univar_stat *g_bfr = &stats[z];
                    size_t old_size;
                    size_t add_size;

                    switch (map_type) {
                    case DCELL_TYPE:
                        if (NULL == g_bfr->dcell_array) {
                            g_bfr->dcell_array = bucket->dcells;
                            bucket->dcells = NULL;
                        }
                        else if (bucket->n != 0) {
                            old_size = g_bfr->n * sizeof(DCELL);
                            add_size = bucket->n * sizeof(DCELL);

                            g_bfr->dcell_array =
                                (DCELL *)G_realloc((void *)g_bfr->dcell_array,
                                                   old_size + add_size);
                            memcpy(&g_bfr->dcell_array[g_bfr->n],
                                   bucket->dcells, add_size);
                        }
                        break;
                    case FCELL_TYPE:
                        if (NULL == g_bfr->fcell_array) {
                            g_bfr->fcell_array = bucket->fcells;
                            bucket->fcells = NULL;
                        }
                        else if (bucket->n != 0) {
                            old_size = g_bfr->n * sizeof(FCELL);
                            add_size = bucket->n * sizeof(FCELL);

                            g_bfr->fcell_array =
                                (FCELL *)G_realloc((void *)g_bfr->fcell_array,
                                                   old_size + add_size);
                            memcpy(&g_bfr->fcell_array[g_bfr->n],
                                   bucket->fcells, add_size);
                        }
                        break;
                    case CELL_TYPE:
                        if (NULL == g_bfr->cell_array) {
                            g_bfr->cell_array = bucket->cells;
                            bucket->cells = NULL;
                        }
                        else if (bucket->n != 0) {
                            old_size = g_bfr->n * sizeof(CELL);
                            add_size = bucket->n * sizeof(CELL);

                            g_bfr->cell_array = (CELL *)G_realloc(
                                (void *)g_bfr->cell_array, old_size + add_size);
                            memcpy(&g_bfr->cell_array[g_bfr->n], bucket->cells,
                                   add_size);
                        }
                        break;
                    }

                    g_bfr->n += bucket->n;
                }
            }
            else {
#pragma omp atomic update
                stats[z].n += zd->bucket.n;
            }
#pragma omp atomic update
            stats[z].size += zd->size;
#pragma omp critical
            {
                if ((map_type == DCELL_TYPE) || (map_type == FCELL_TYPE)) {
                    /* use Kahan sum for floating point */
                    kahan_sum(&stats[z].sum, &c_sum, zd->sum);
                    kahan_sum(&stats[z].sumsq, &c_sumsq, zd->sumsq);
                    kahan_sum(&stats[z].sum_abs, &c_sum_abs, zd->sum_abs);
                }
                else if (map_type == CELL_TYPE) {
                    /* integer does not have floating point error */
                    stats[z].sum += zd->sum;
                    stats[z].sumsq += zd->sumsq;
                    stats[z].sum_abs += zd->sum_abs;
                }
                else
                    G_fatal_error(_("Unknown map type"));
            }

#if defined(_OPENMP)
            omp_set_lock(&minmax[z]);
#endif
            if (stats[z].max < zd->max ||
                (stats[z].max != stats[z].max && zd->max != DBL_MIN)) {
                stats[z].max = zd->max;
            }
            if (stats[z].min > zd->min ||
                (stats[z].min != stats[z].min && zd->min != DBL_MAX)) {
                stats[z].min = zd->min;
            }
#if defined(_OPENMP)
            omp_unset_lock(&minmax[z]);
#endif
        }

        /* Free per-thread variables */
        for (int z = 0; z < n_alloc; z++) {
            zone_workspace *zd = &zw[z];
            if (zd->bucket.cells)
                G_free(zd->bucket.cells);
            if (zd->bucket.fcells)
                G_free(zd->bucket.fcells);
            if (zd->bucket.dcells)
                G_free(zd->bucket.dcells);
        }
    } /* end parallel region */

#if defined(_OPENMP)
    for (int z = 0; z < n_alloc; z++) {
        omp_destroy_lock(&minmax[z]);
    }

    G_free(minmax);
#endif

    for (int t = 0; t < nprocs; t++) {
        G_free(tw[t].raster_row);
    }
    if (n_zones) {
        for (int t = 0; t < nprocs; t++) {
            G_free(tw[t].zoneraster_row);
        }
    }
    if (format != SHELL)
        G_percent(rows, rows, 2);
}

/* Use Kahan sum to avoid floating point error from lots of summations */
static void kahan_sum(double *sum, double *c, double x)
{
    double y = x - *c;
    double t = *sum + y;
    *c = (t - *sum) - y; /* (t - sum) recovers the high-order part of y; */
    *sum = t;            /* Algebraically, c should always be zero. */
}
