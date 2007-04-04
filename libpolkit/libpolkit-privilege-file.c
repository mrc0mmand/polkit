/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/***************************************************************************
 *
 * libpolkit-privilege-file.c : privilege files
 *
 * Copyright (C) 2007 David Zeuthen, <david@fubar.dk>
 *
 * Licensed under the Academic Free License version 2.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307	 USA
 *
 **************************************************************************/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <errno.h>

#include <glib.h>
#include "libpolkit-error.h"
#include "libpolkit-result.h"
#include "libpolkit-privilege-file.h"

/**
 * SECTION:libpolkit-privilege-file
 * @short_description: Privileges files.
 *
 * This class is used to represent a privilege files.
 **/

/**
 * PolKitPrivilegeFile:
 *
 * Objects of this class are used to record information about a
 * privilege.
 **/
struct PolKitPrivilegeFile
{
        int refcount;
        char *group;
        char *identifier;
        char *description;

        PolKitResult default_remote_inactive;
        PolKitResult default_remote_active;
        PolKitResult default_local_inactive;
        PolKitResult default_local_active;
};

static gboolean
parse_default (const char *key, char *s, PolKitResult* target, GError **error)
{
        gboolean ret;

        ret = libpolkit_result_from_string_representation (s, target);
        if (!ret) {
                int n;
                char *s2;
                GString *str;

                str = g_string_new (NULL);
                for (n = 0; n < LIBPOLKIT_RESULT_N_RESULTS; n++) {
                        if (n == LIBPOLKIT_RESULT_NOT_AUTHORIZED_TO_KNOW)
                                continue;

                        if (str->len > 0) {
                                g_string_append (str, ", ");
                        }
                        g_string_append (str, libpolkit_result_to_string_representation (n));
                }
                s2 = g_string_free (str, FALSE);

                g_set_error (error, 
                             POLKIT_ERROR, 
                             POLKIT_ERROR_PRIVILEGE_FILE_INVALID_VALUE,
                             "Value %s is not allowed for key %s - supported values are: %s", 
                             s, 
                             key,
                             s2);
                g_free (s2);
        }
        
        g_free (s);
        return ret;
}

/**
 * libpolkit_privilege_file_new:
 * @path: path to privilege file
 * @error: return location for error
 * 
 * Create a new #PolKitPrivilegeFile object. If the file does not
 * validate, a human readable explanation of why will be set in
 * @error.
 * 
 * Returns: the new object or #NULL if error is set
 **/
PolKitPrivilegeFile *
libpolkit_privilege_file_new (const char *path, GError **error)
{
        GKeyFile *key_file;
        PolKitPrivilegeFile *pf;
        char *s;
        const char *key;
        const char *group;

        pf = NULL;

        key_file = g_key_file_new ();
        if (!g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, error))
                goto error;

        pf = g_new0 (PolKitPrivilegeFile, 1);
        pf->refcount = 1;

        group = "Privilege";
        if ((pf->group = g_key_file_get_string (key_file, group, "Group", error)) == NULL)
                goto error;
        if ((pf->identifier = g_key_file_get_string (key_file, group, "Identifier", error)) == NULL)
                goto error;
        if ((pf->description = g_key_file_get_string (key_file, group, "Description", error)) == NULL)
                goto error;

        group = "Defaults";
        key = "AllowRemoteInactive";
        if ((s = g_key_file_get_string (key_file, group, key, error)) == NULL)
                goto error;
        if (!parse_default (key, s, &pf->default_remote_inactive, error))
                goto error;
        key = "AllowRemoteActive";
        if ((s = g_key_file_get_string (key_file, group, key, error)) == NULL)
                goto error;
        if (!parse_default (key, s, &pf->default_remote_active, error))
                goto error;
        key = "AllowLocalInactive";
        if ((s = g_key_file_get_string (key_file, group, key, error)) == NULL)
                goto error;
        if (!parse_default (key, s, &pf->default_local_inactive, error))
                goto error;
        key = "AllowLocalActive";
        if ((s = g_key_file_get_string (key_file, group, key, error)) == NULL)
                goto error;
        if (!parse_default (key, s, &pf->default_local_active, error))
                goto error;

        g_key_file_free (key_file);
        return pf;
error:
        g_key_file_free (key_file);
        if (pf != NULL)
                libpolkit_privilege_file_unref (pf);
        return NULL;
}

/**
 * libpolkit_privilege_file_ref:
 * @privilege_file: the privilege file object
 * 
 * Increase reference count.
 * 
 * Returns: the object
 **/
PolKitPrivilegeFile *
libpolkit_privilege_file_ref (PolKitPrivilegeFile *privilege_file)
{
        g_return_val_if_fail (privilege_file != NULL, privilege_file);
        privilege_file->refcount++;
        return privilege_file;
}

/**
 * libpolkit_privilege_file_unref:
 * @privilege_file: the privilege file object
 * 
 * Decreases the reference count of the object. If it becomes zero,
 * the object is freed. Before freeing, reference counts on embedded
 * objects are decresed by one.
 **/
void
libpolkit_privilege_file_unref (PolKitPrivilegeFile *privilege_file)
{
        g_return_if_fail (privilege_file != NULL);
        privilege_file->refcount--;
        if (privilege_file->refcount > 0) 
                return;
        g_free (privilege_file->group);
        g_free (privilege_file->identifier);
        g_free (privilege_file->description);
        g_free (privilege_file);
}
