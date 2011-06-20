
/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "dispd.h"
#include "uevent.h"
#include "dispmgr.h"

#define DEBUG_UEVENT 0

#define UEVENT_PARAMS_MAX 32

//define the switch name for this switch
#define DISPD_SWITCH_NAME "dvi_det"

//Config this based on hardware connections
#ifdef MX51_BBG_DISPD
//DI1 -- WVGA
//DI0 -- DVI
#define NON_DVI_DISP_PORT "DI1"
#endif

//Config this based on hardware connections
#ifdef MX53_SMD_DISPD
//DI1 -- LVDS
//DI0 -- HDMI
#define NON_HDMI_DISP_PORT "DI1"
#endif

enum uevent_action { action_add, action_remove, action_change };

struct uevent {
    char *path;
    enum uevent_action action;
    char *subsystem;
    char *param[UEVENT_PARAMS_MAX];
    unsigned int seqnum;
};

struct uevent_dispatch {
    char *subsystem;
    char *devpath;
    int (* dispatch) (struct uevent *);
};

static void dump_uevent(struct uevent *);
static int dispatch_uevent(struct uevent *event);
static void free_uevent(struct uevent *event);
static char *get_uevent_param(struct uevent *event, char *param_name);

static int handle_switch_event(struct uevent *);
static int handle_sii9022_event(struct uevent *);
static int handle_dvi_event(struct uevent *);

static struct uevent_dispatch dispatch_table[] = {
    { "switch", NULL, handle_switch_event }, 
    { "mxc_ddc", "/devices/platform/mxc_ddc.0", handle_dvi_event },
    { "sii902x", "/devices/platform/sii902x.0",handle_sii9022_event }, 
    { NULL, NULL }
};

int process_uevent_message(int socket)
{
    char buffer[64 * 1024]; // Thank god we're not in the kernel :)
    int count;
    char *s = buffer;
    char *end;
    struct uevent *event;
    int param_idx = 0;
    int i;
    int first = 1;
    int rc = 0;

    if ((count = recv(socket, buffer, sizeof(buffer), 0)) < 0) {
        LOGE("Error receiving uevent (%s)", strerror(errno));
        return -errno;
    }

    if (!(event = malloc(sizeof(struct uevent)))) {
        LOGE("Error allocating memory (%s)", strerror(errno));
        return -errno;
    }

    memset(event, 0, sizeof(struct uevent));

    end = s + count;
    while (s < end) {
        if (first) {
            char *p;
            for (p = s; *p != '@'; p++);
            p++;
            event->path = strdup(p);
#if DEBUG_UEVENT
            LOGI("path:%s", event->path);
#endif
            first = 0;
        } else {
            if (!strncmp(s, "ACTION=", strlen("ACTION="))) {
                char *a = s + strlen("ACTION=");
               
                if (!strcmp(a, "add"))
                    event->action = action_add;
                else if (!strcmp(a, "change"))
                    event->action = action_change;
                else if (!strcmp(a, "remove"))
                    event->action = action_remove;
            } else if (!strncmp(s, "SEQNUM=", strlen("SEQNUM=")))
                event->seqnum = atoi(s + strlen("SEQNUM="));
            else if (!strncmp(s, "SUBSYSTEM=", strlen("SUBSYSTEM=")))
                event->subsystem = strdup(s + strlen("SUBSYSTEM="));
            else
                event->param[param_idx++] = strdup(s);
        }
        s+= strlen(s) + 1;
    }

    rc = dispatch_uevent(event);
    
    free_uevent(event);
    return rc;
}

int simulate_uevent(char *subsys, char *path, char *action, char **params)
{
    struct uevent *event;
    char tmp[255];
    int i, rc;

    if (!(event = malloc(sizeof(struct uevent)))) {
        LOGE("Error allocating memory (%s)", strerror(errno));
        return -errno;
    }

    memset(event, 0, sizeof(struct uevent));

    event->subsystem = strdup(subsys);

    if (!strcmp(action, "add"))
        event->action = action_add;
    else if (!strcmp(action, "change"))
        event->action = action_change;
    else if (!strcmp(action, "remove"))
        event->action = action_remove;
    else {
        LOGE("Invalid action '%s'", action);
        return -1;
    }

    event->path = strdup(path);

    for (i = 0; i < UEVENT_PARAMS_MAX; i++) {
        if (!params[i])
            break;
        event->param[i] = strdup(params[i]);
    }

    rc = dispatch_uevent(event);
    free_uevent(event);
    return rc;
}

static int dispatch_uevent(struct uevent *event)
{
    int i;

#if DEBUG_UEVENT
    dump_uevent(event);
#endif
    for (i = 0; dispatch_table[i].subsystem != NULL; i++) {
        if (((dispatch_table[i].devpath != NULL)&&(!strcmp(dispatch_table[i].devpath, event->path)))||
            (!strcmp(dispatch_table[i].subsystem, event->subsystem)))
            return dispatch_table[i].dispatch(event);
    }

#if DEBUG_UEVENT
    LOG_DISP("No uevent handlers registered for '%s' subsystem", event->subsystem);
#endif
    return 0;
}

static void dump_uevent(struct uevent *event)
{
    int i;

    LOG_DISP("[UEVENT] Sq: %u S: %s A: %d P: %s",
              event->seqnum, event->subsystem, event->action, event->path);
    for (i = 0; i < UEVENT_PARAMS_MAX; i++) {
        if (!event->param[i])
            break;
        LOG_DISP("%s", event->param[i]);
    }
}

static void free_uevent(struct uevent *event)
{
    int i;
    free(event->path);
    free(event->subsystem);
    for (i = 0; i < UEVENT_PARAMS_MAX; i++) {
        if (!event->param[i])
            break;
        free(event->param[i]);
    }
    free(event);
}

static char *get_uevent_param(struct uevent *event, char *param_name)
{
    int i;

    for (i = 0; i < UEVENT_PARAMS_MAX; i++) {
        if (!event->param[i])
            break;
        if (!strncmp(event->param[i], param_name, strlen(param_name)))
            return &event->param[i][strlen(param_name) + 1];
    }

    LOGE("get_uevent_param(): No parameter '%s' found", param_name);
    return NULL;
}

//Return 1 if the switch need
//Rely on the hardware config and kernel parameters
#if defined(MX51_BBG_DISPD)
int needDisplaySwitch()
{
    int ret = 0;
    char name[256];
    FILE *fp;
    //Read the string in /sys/class/graphics/fb0/name
    //Check which display port for fb0
    //Check whether the display port is DVI
    memset(name, 0 ,256);
    fp = fopen("/sys/class/graphics/fb0/name", "r");
    if (!fgets(name, sizeof(name), fp)) {
        LOGE("Error!Unable to read fb0 name");
        fclose(fp);
        return 0;
    }
    LOGI("Current FB name:%s",name);
    fclose(fp);
    //If it has a string "DI1", switch to DVI DI0 can be done
    //Otherwise the primarly display is already on DVI DI0
    if(strstr(name,NON_DVI_DISP_PORT) != NULL) {
        return 1;
    }
    else{
        return 0;
    }
}
#elif defined(MX53_SMD_DISPD)
int needDisplaySwitch()
{
   int ret = 0;
   char name[256];
   FILE *fp;
   //Read the string in /sys/class/graphics/fb0/name
   //Check which display port for fb0
   //Check whether the display port is HDMI
   memset(name, 0 ,256);
   fp = fopen("/sys/class/graphics/fb0/name", "r");
   if (!fgets(name, sizeof(name), fp)) {
        LOGE("Error!Unable to read fb0 name");
        fclose(fp);
        return 0;
   }
   LOGI("Current FB name:%s",name);
   fclose(fp);
   //If it has a string "DI1", switch to HDMI DI0 can be done
   //Otherwise the primarly display is already on HDMI DI0
   if(strstr(name,NON_HDMI_DISP_PORT) != NULL) {
        return 1;
   }
   else{
        return 0;
   }
}
#else
int needDisplaySwitch()
{
    return 0;
}
#endif

/*
 * ---------------
 * Uevent Handlers
 * ---------------
 */
static int handle_switch_event(struct uevent *event)
{
    char *name = get_uevent_param(event, "SWITCH_NAME");
    char *state = get_uevent_param(event, "SWITCH_STATE");

    LOGI("handle_switch_event: state %s",state);
    //If dvi is already the primarly display, not need to do the switch
    if ((!strcmp(name, DISPD_SWITCH_NAME))&&needDisplaySwitch()) {
        if (!strcmp(state, "online")) {
            dispmgr_connected_set(true);
        } else {
            dispmgr_connected_set(false);
        }
    } 

    return 0;
}

/*
 * ---------------
 * Uevent Handlers for dvi plugin/plugout 
 * ---------------
 */
static int handle_dvi_event(struct uevent *event)
{
    char *state = get_uevent_param(event, "EVENT");

    LOGI("handle_dvi_event: EVENT %s",state);
    //If dvi is already the primarly display, not need to do the switch
    if (needDisplaySwitch()) {
        if (!strcmp(state, "plugin")) {
            dispmgr_connected_set(true);
        } else {
            dispmgr_connected_set(false);
        }
    } 

    return 0;
}
/*
 * ---------------
 * Uevent Handlers for HDMI plugin and plugout
 * ---------------
 */
static int handle_sii9022_event(struct uevent *event)
{
    char *state = get_uevent_param(event, "EVENT");

    LOGI("handle_sii9022_event: EVENT %s",state);
    //If dvi is already the primarly display, not need to do the switch
    if (needDisplaySwitch()) {
        if (!strcmp(state, "plugin")) {
            dispmgr_connected_set(true);
        } else {
            dispmgr_connected_set(false);
        }
    } 

    return 0;
}
