/*
 * Copyright (c) 2010
 *      The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Nolan Lum <nol888@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/queue.h>

#include <ctype.h>
#include <devattr.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static SLIST_HEAD(, sl_entry) props = SLIST_HEAD_INITIALIZER(props);
struct sl_entry {
    char *val;
    SLIST_ENTRY(sl_entry) entries;
} *ent;

static void
usage(const char* name) {
    fprintf(stderr, "devattr\n\n"
    
                    "Usage: %s [-Ah] [-p property] [-d device] [-m key:value] [-r key:value]\n\n"
                    
                    "  A : Don't display aliases.\n"
                    "  h : Print this help message.\n\n"
                    
                    "  p property  : Only display property, can be specified multiple times and\n"
                    "                combined with all other options.\n"
                    "  d device    : Only display devices with name `device'. When used with\n"
                    "                -p, only properties `-p' of device `-d' are listed. Can be\n"
                    "                specified multiple times.\n"
                    "  m key:value : Only display devices whose property `key' matches with wildcards\n"
                    "                value `value'. Stacks with -p, -d, and -m. Can be specified\n"
                    "                multiple times.\n"
                    "  r key:value : Behaves similarly to `-m', but matches with regex.\n"
            , name);
}

static int
parse_args(int argc, char* argv[], struct udev_enumerate *enumerate) {
    SLIST_INIT(&props);

    /* A = no aliases */
    /* p = properties to list (defaults to all) */
    /* d = devices to list (defaults to all) */
    /* m = display only devices in d that match these prop values */
    /* r = display only devices in d that match these prop values (regex) */
    const char* const opts = "Ap:d:m:h";
    int ch;
    char *colon;
    while ((ch = getopt(argc, argv, opts)) != -1) {
        switch (ch) {
        case 'A':
            udev_enumerate_add_match_expr(enumerate, "alias", "0");
            break;
        case 'p':
            ent = malloc(sizeof(struct sl_entry));
            ent->val = optarg;
            SLIST_INSERT_HEAD(&props, ent, entries);
            break;
        case 'd':
            udev_enumerate_add_match_expr(enumerate, "name", optarg);
            break;
        case 'm':
        case 'r':
            colon = strchr(optarg, ':');
            if (colon == NULL) {
                fprintf(stderr, "Invalid property key/value pair `%s'.\n", optarg);
                return (0);
            }
            
            *colon = '\0';
            if (ch == 'r')
                udev_enumerate_add_match_regex(enumerate, optarg, colon + 1);
            else
                udev_enumerate_add_match_expr(enumerate, optarg, colon + 1);
            break;
        case 'h':
            usage(argv[0]);
            return (1);
        case '?':
            usage(argv[0]);
            return (0);
        }
    }
    return (1);
}

void
print_prop(const char* key, prop_object_t value) {
    char *valStr;

    printf("\t%s = ", key);

    prop_type_t valType = prop_object_type(value);
    switch (valType) {
    case PROP_TYPE_BOOL:
        printf("%s\n", prop_bool_true((prop_bool_t) value) ? "true" : "false");
        break;
    case PROP_TYPE_NUMBER:
        if (prop_number_unsigned((prop_number_t) value))
            printf("%1$llu (0x%1$llx)\n", prop_number_unsigned_integer_value((prop_number_t) value));
        else
            printf("%lld\n", prop_number_integer_value((prop_number_t) value));
        break;
    case PROP_TYPE_STRING:
        valStr = prop_string_cstring(value);
        printf("%s\n", valStr);
        free(valStr);
        break;
    }
}

int
main(int argc, char* argv[]) {
    struct udev *ctx;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *current;
    int ret;
    
    ctx = udev_new();
    if(ctx == NULL)
        err(1, "udev_new");

    enumerate = udev_enumerate_new(ctx);
    if(enumerate == NULL)
        err(1, "udev_enumerate_new");
    
    if(!parse_args(argc, argv, enumerate))
        return (1);

    ret = udev_enumerate_scan_devices(enumerate);
    if(ret != 0)
        err(1, "udev_enumerate_scan_devices ret = %d", ret);
    
    udev_list_entry_foreach (current, udev_enumerate_get_list_entry(enumerate)) {
        struct udev_device *dev = udev_list_entry_get_device(current);
        if(dev == NULL)
            continue;
        prop_dictionary_t dict = udev_device_get_dictionary(dev);
        if(dict == NULL)
            continue;
        prop_object_iterator_t iter = prop_dictionary_iterator(dict);
        prop_dictionary_keysym_t curKey = NULL;
        
        
        printf("Device:\n");
        if (!SLIST_EMPTY(&props)) {
            SLIST_FOREACH(ent, &props, entries) {
                prop_object_t keyVal = prop_dictionary_get(dict, ent->val);
                if(keyVal != NULL)
                    print_prop(ent->val, keyVal);
            }
        } else {
            while ((curKey = (prop_dictionary_keysym_t) prop_object_iterator_next(iter)) != NULL) {
                const char *keyStr = prop_dictionary_keysym_cstring_nocopy(curKey);
                prop_object_t keyVal = prop_dictionary_get_keysym(dict, curKey);
                print_prop(keyStr, keyVal);
            }
        }
        
        
        printf("\n");
    }
    
    udev_enumerate_unref(enumerate);
    udev_unref(ctx);

    return (0);
}
