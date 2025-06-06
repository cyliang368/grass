/*!
   \file lib/db/dbmi_base/dirent.c

   \brief DBMI Library (base) - directory entities management

   (C) 1999-2010 by the GRASS Development Team

   This program is free software under the GNU General Public License
   (>=v2). Read the file COPYING that comes with GRASS for details.

   \author Joel Jones (CERL/UIUC)
   \author Upgraded to GRASS 5.7 by Radim Blazek
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#include <grass/dbmi.h>

/* NOTE: these should come from <unistd.h> or from <sys/file.h> */
#ifndef R_OK
#if !defined(HAVE_UNISTD_H)
#define R_OK 4
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef X_OK
#define X_OK 1
#endif
#endif

static int cmp_dirent(const void *, const void *);
static int get_perm(char *);
static void sort_dirent(dbDirent *, int);

/*!
   \brief Read directory and build an array of dbDirent's

   Append one entry with name = NULL to mark end of array

   \param dirname directory name
   \param[out] n number of entities

   \return pointer to dbDirent
   \return NULL on error
 */
dbDirent *db_dirent(const char *dirname, int *n)
{
    DIR *dp;
    struct dirent *entry;
    dbDirent *db_dirent;
    int i, count;
    char *path;
    int len, max;

    db_clear_error();

    *n = 0;
    dp = opendir(dirname);
    if (dp == NULL) {
        db_syserror(dirname);
        return (dbDirent *)NULL;
    }

    /* count the number of entries and get the strlen of the longest name */
    count = 0;
    max = 0;
    while ((entry = readdir(dp))) {
        count++;
        len = strlen(entry->d_name);
        if (len > max)
            max = len;
    }
    rewinddir(dp);

    size_t path_len = strlen(dirname) + max + 2; // extra 2 for / and NULL
    path = db_malloc(path_len);
    if (path == NULL) {
        closedir(dp);
        return (dbDirent *)NULL;
    }
    db_dirent = db_alloc_dirent_array(count);
    if (db_dirent == NULL) {
        closedir(dp);
        return (dbDirent *)NULL;
    }
    *n = count;
    for (i = 0; i < count; i++) {
        entry = readdir(dp);
        if (entry == NULL) /* this shouldn't happen */
            break;

        if (DB_OK != db_set_string(&db_dirent[i].name, entry->d_name))
            break;
        snprintf(path, path_len, "%s/%s", dirname, entry->d_name);
        db_dirent[i].perm = get_perm(path);
        db_dirent[i].isdir = (db_isdir(path) == DB_OK);
    }
    closedir(dp);
    db_free(path);

    sort_dirent(db_dirent, *n);

    return db_dirent;
}

/*!
   \brief Free dbDirent

   \param db_dirent pointer to dbDirent
   \param count number of entities in the array
 */
void db_free_dirent_array(dbDirent *db_dirent, int count)
{
    int i;

    if (db_dirent) {
        for (i = 0; i < count; i++)
            db_free_string(&db_dirent[i].name);
        db_free(db_dirent);
    }
}

static int get_perm(char *path)
{
    int perm;

    perm = 0;

    if (access(path, R_OK) == 0)
        perm |= DB_PERM_R;
    if (access(path, W_OK) == 0)
        perm |= DB_PERM_W;
    if (access(path, X_OK) == 0)
        perm |= DB_PERM_X;

    return perm;
}

static int cmp_dirent(const void *aa, const void *bb)
{
    const dbDirent *a = aa;
    const dbDirent *b = bb;

    return strcmp(db_get_string((dbString *)&a->name),
                  db_get_string((dbString *)&b->name));
}

static void sort_dirent(dbDirent *a, int n)
{
    qsort(a, n, sizeof(dbDirent), cmp_dirent);
}

/*!
   \brief Allocate dirent array

   \param count number of entities in the array

   \return pointer to dbDirent array
   \return NULL on failure
 */
dbDirent *db_alloc_dirent_array(int count)
{
    int i;
    dbDirent *db_dirent;

    db_dirent = (dbDirent *)db_calloc(count, sizeof(dbDirent));
    if (db_dirent == NULL)
        return db_dirent;

    for (i = 0; i < count; i++)
        db_init_string(&db_dirent[i].name);

    return db_dirent;
}
