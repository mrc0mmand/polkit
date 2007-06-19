/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/***************************************************************************
 *
 * polkit-list-actions.c : list all registered PolicyKit actions
 *
 * Copyright (C) 2007 David Zeuthen, <david@fubar.dk>
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

#include <stdbool.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <errno.h>

#include <polkit/polkit.h>

static void
usage (int argc, char *argv[])
{
	fprintf (stderr,
                 "\n"
                 "usage : polkit-list-actions [--version] [--help]\n"
                 "\n");
	fprintf (stderr,
                 "\n"
                 "        --version        Show version and exit\n"
                 "        --help           Show this information and exit\n"
                 "\n"
                 "List the actions registered with PolicyKit.\n");
}

static void
_print_entry (PolKitPolicyCache *policy_cache,
              PolKitPolicyFileEntry *pfe,
              void *user_data)
{
        const char *action_id;
        const char *group_id;
        PolKitPolicyDefault *def;
        PolKitResult default_inactive;
        PolKitResult default_active;

        action_id = polkit_policy_file_entry_get_id (pfe);
        group_id = polkit_policy_file_entry_get_group_id (pfe);
        def = polkit_policy_file_entry_get_default (pfe);
        default_inactive = polkit_policy_default_get_allow_inactive (def);
        default_active = polkit_policy_default_get_allow_active (def);

        printf ("Policy\n"
                "------\n"
                "group             = %s ('%s')\n"
                "action            = %s ('%s')\n"
                "default_inactive  = %s\n"
                "default_active    = %s\n"
                "\n", 
                group_id, 
                polkit_policy_file_get_group_description (pfe),
                action_id,
                polkit_policy_file_get_action_description (pfe),
                polkit_result_to_string_representation (default_inactive),
                polkit_result_to_string_representation (default_active));
}

int
main (int argc, char *argv[])
{
        int n;

        for (n = 1; n < argc; n++) {
                if (strcmp (argv[n], "--help") == 0) {
                        usage (argc, argv);
                        return 0;
                }
                if (strcmp (argv[n], "--version") == 0) {
                        printf ("polkit-list-actions " PACKAGE_VERSION "\n");
                        return 0;
                }
	}

        int ret;
        ret = 1;

        PolKitContext *ctx;
        PolKitPolicyCache *cache;
        PolKitError *error;
        ctx = polkit_context_new ();
        if (ctx == NULL)
                goto out;
        error = NULL;
        polkit_context_set_load_descriptions (ctx);
        if (!polkit_context_init (ctx, &error)) {
                printf ("Init failed: %s\n", polkit_error_get_error_message (error));
                polkit_context_unref (ctx);
                goto out;
        }

        cache = polkit_context_get_policy_cache (ctx);
        if (cache == NULL) {
                polkit_context_unref (ctx);
                goto out;
        }

        polkit_policy_cache_foreach (cache, _print_entry, NULL);

        polkit_context_unref (ctx);
out:
        return ret;
}
