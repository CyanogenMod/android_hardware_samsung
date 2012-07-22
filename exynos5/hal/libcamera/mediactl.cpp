/*
 * Media controller interface library
 *
 * Copyright (C) 2010-2011 Ideas on board SPRL
 *
 * Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

//#include "config.h"
#define LOG_NDEBUG 0
#define LOG_TAG "Mediactl"

#include <utils/Log.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <linux/videodev2.h>
#include <media.h>
#include <linux/kdev_t.h>
#include <linux/types.h>

#include "mediactl.h"

#define KF_MSG                              0x1
#define KF_ANY                              0x2

#define perror_exit(cond, func) \
    if (cond) { \
        fprintf(stderr, "[%s:%d]: ", __func__, __LINE__);\
        perror(func);\
        exit(EXIT_FAILURE);\
    }

struct media_pad *media_entity_remote_source(struct media_pad *pad)
{
    unsigned int i;

    if (!(pad->flags & MEDIA_PAD_FL_SINK))
        return NULL;

    for (i = 0; i < pad->entity->num_links; ++i) {
        struct media_link *link = &pad->entity->links[i];

        if (!(link->flags & MEDIA_LNK_FL_ENABLED))
            continue;

        if (link->sink == pad)
            return link->source;
    }

    return NULL;
}

struct media_entity *media_get_entity_by_name(struct media_device *media,
                          const char *name, size_t length)
{
    unsigned int i;
        struct media_entity *entity;
        entity = (struct media_entity*)calloc(1,  sizeof(struct media_entity));
    for (i = 0; i < media->entities_count; ++i) {
        entity = &media->entities[i];

        if (strncmp(entity->info.name, name, length) == 0)
            return entity;
    }

    return NULL;
}

struct media_entity *media_get_entity_by_id(struct media_device *media,
                        __u32 id)
{
    unsigned int i;

    for (i = 0; i < media->entities_count; ++i) {
        struct media_entity *entity = &media->entities[i];

        if (entity->info.id == id)
            return entity;
    }

    return NULL;
}

int media_setup_link(struct media_device *media,
             struct media_pad *source,
             struct media_pad *sink,
             __u32 flags)
{
    struct media_link *link;
    struct media_link_desc ulink;
    unsigned int i;
    int ret;

    for (i = 0; i < source->entity->num_links; i++) {
        link = &source->entity->links[i];

        if (link->source->entity == source->entity &&
            link->source->index == source->index &&
            link->sink->entity == sink->entity &&
            link->sink->index == sink->index)
            break;
    }

    if (i == source->entity->num_links) {
        ALOGE("%s: Link not found", __func__);
        return -ENOENT;
    }

    /* source pad */
    ulink.source.entity = source->entity->info.id;
    ulink.source.index = source->index;
    ulink.source.flags = MEDIA_PAD_FL_SOURCE;

    /* sink pad */
    ulink.sink.entity = sink->entity->info.id;
    ulink.sink.index = sink->index;
    ulink.sink.flags = MEDIA_PAD_FL_SINK;

    ulink.flags = flags | (link->flags & MEDIA_LNK_FL_IMMUTABLE);

    ret = ioctl(media->fd, MEDIA_IOC_SETUP_LINK, &ulink);
    if (ret == -1) {
        ALOGE("%s: Unable to setup link (%s)",
              __func__, strerror(errno));
        return -errno;
    }

    link->flags = ulink.flags;
    link->twin->flags = ulink.flags;
    return 0;
}

int media_reset_links(struct media_device *media)
{
    unsigned int i, j;
    int ret;

    for (i = 0; i < media->entities_count; ++i) {
        struct media_entity *entity = &media->entities[i];

        for (j = 0; j < entity->num_links; j++) {
            struct media_link *link = &entity->links[j];

            if (link->flags & MEDIA_LNK_FL_IMMUTABLE ||
                link->source->entity != entity)
                continue;

            ret = media_setup_link(media, link->source, link->sink,
                           link->flags & ~MEDIA_LNK_FL_ENABLED);
            if (ret < 0)
                return ret;
        }
    }

    return 0;
}

static struct media_link *media_entity_add_link(struct media_entity *entity)
{
    if (entity->num_links >= entity->max_links) {
        struct media_link *links = entity->links;
        unsigned int max_links = entity->max_links * 2;
        unsigned int i;

        links = (struct media_link*)realloc(links, max_links * sizeof *links);
        if (links == NULL)
            return NULL;

        for (i = 0; i < entity->num_links; ++i)
            links[i].twin->twin = &links[i];

        entity->max_links = max_links;
        entity->links = links;
    }

    return &entity->links[entity->num_links++];
}

static int media_enum_links(struct media_device *media)
{
    ALOGV("%s: start", __func__);
    __u32 id;
    int ret = 0;

    for (id = 1; id <= media->entities_count; id++) {
        struct media_entity *entity = &media->entities[id - 1];
        struct media_links_enum links;
        unsigned int i;

        links.entity = entity->info.id;
        links.pads = (struct media_pad_desc*)malloc(entity->info.pads * sizeof(struct media_pad_desc));
        links.links = (struct media_link_desc*)malloc(entity->info.links * sizeof(struct media_link_desc));

        if (ioctl(media->fd, MEDIA_IOC_ENUM_LINKS, &links) < 0) {
            ALOGE(
                  "%s: Unable to enumerate pads and links (%s).",
                  __func__, strerror(errno));
            free(links.pads);
            free(links.links);
            return -errno;
        }

        for (i = 0; i < entity->info.pads; ++i) {
            entity->pads[i].entity = entity;
            entity->pads[i].index = links.pads[i].index;
            entity->pads[i].flags = links.pads[i].flags;
        }

        for (i = 0; i < entity->info.links; ++i) {
            struct media_link_desc *link = &links.links[i];
            struct media_link *fwdlink;
            struct media_link *backlink;
            struct media_entity *source;
            struct media_entity *sink;

            source = media_get_entity_by_id(media, link->source.entity);
            sink = media_get_entity_by_id(media, link->sink.entity);
            if (source == NULL || sink == NULL) {
                ALOGE(
                      "WARNING entity %u link %u from %u/%u to %u/%u is invalid!",
                      id, i, link->source.entity,
                      link->source.index,
                      link->sink.entity,
                      link->sink.index);
                ret = -EINVAL;
            } else {
                fwdlink = media_entity_add_link(source);
                fwdlink->source = &source->pads[link->source.index];
                fwdlink->sink = &sink->pads[link->sink.index];
                fwdlink->flags = link->flags;

                backlink = media_entity_add_link(sink);
                backlink->source = &source->pads[link->source.index];
                backlink->sink = &sink->pads[link->sink.index];
                backlink->flags = link->flags;

                fwdlink->twin = backlink;
                backlink->twin = fwdlink;
            }
        }

        free(links.pads);
        free(links.links);
    }
    return ret;
}

#ifdef HAVE_LIBUDEV

#include <libudev.h>

static inline int media_udev_open(struct udev **udev)
{
    *udev = udev_new();
    if (*udev == NULL)
        return -ENOMEM;
    return 0;
}

static inline void media_udev_close(struct udev *udev)
{
    if (udev != NULL)
        udev_unref(udev);
}

static int media_get_devname_udev(struct udev *udev,
        struct media_entity *entity)
{
    struct udev_device *device;
    dev_t devnum;
    const char *p;
    int ret = -ENODEV;

    if (udev == NULL)
        return -EINVAL;

    devnum = makedev(entity->info.v4l.major, entity->info.v4l.minor);
    ALOGE("looking up device: %u:%u",
          major(devnum), minor(devnum));
    device = udev_device_new_from_devnum(udev, 'c', devnum);
    if (device) {
        p = udev_device_get_devnode(device);
        if (p) {
            strncpy(entity->devname, p, sizeof(entity->devname));
            entity->devname[sizeof(entity->devname) - 1] = '\0';
        }
        ret = 0;
    }

    udev_device_unref(device);

    return ret;
}

#else    /* HAVE_LIBUDEV */

struct udev;

static inline int media_udev_open(struct udev **udev) { return 0; }

static inline void media_udev_close(struct udev *udev) { }

static inline int media_get_devname_udev(struct udev *udev,
        struct media_entity *entity)
{
    return -ENOTSUP;
}

#endif    /* HAVE_LIBUDEV */

static int media_get_devname_sysfs(struct media_entity *entity)
{
    //struct stat devstat;
    char devname[32];
    char sysname[32];
    char target[1024];
    char *p;
    int ret;

    sprintf(sysname, "/sys/dev/char/%u:%u", entity->info.v4l.major,
        entity->info.v4l.minor);

    ret = readlink(sysname, target, sizeof(target));
    if (ret < 0)
        return -errno;

    target[ret] = '\0';
    p = strrchr(target, '/');
    if (p == NULL)
        return -EINVAL;

    sprintf(devname, "/tmp/%s", p + 1);

    ret = mknod(devname, 0666 | S_IFCHR, MKDEV(81, entity->info.v4l.minor));
    strcpy(entity->devname, devname);

    return 0;
}

int get_media_fd(struct media_device *media)
{
    ssize_t num;
    int media_node;
    char *ptr;
    char media_buf[6];

    ALOGV("%s(%s)", __func__, MEDIA_DEV);

    media->fd = open(MEDIA_DEV, O_RDWR, 0);
    if( media->fd < 0) {
        ALOGE("Open sysfs media device failed, media->fd : 0x%p", media->fd);
        return -1;
    }
    ALOGV("media->fd : %p", media->fd);

    return media->fd;

}

static int media_enum_entities(struct media_device *media)
{
    struct media_entity *entity;
    unsigned int size;
    __u32 id;
    int ret;
    entity = (struct media_entity*)calloc(1,  sizeof(struct media_entity));
    for (id = 0, ret = 0; ; id = entity->info.id) {
        size = (media->entities_count + 1) * sizeof(*media->entities);
        media->entities = (struct media_entity*)realloc(media->entities, size);

        entity = &media->entities[media->entities_count];
        memset(entity, 0, sizeof(*entity));
        entity->fd = -1;
        entity->info.id = id | MEDIA_ENT_ID_FLAG_NEXT;
        entity->media = media;

        ret = ioctl(media->fd, MEDIA_IOC_ENUM_ENTITIES, &entity->info);

        if (ret < 0) {
            ret = errno != EINVAL ? -errno : 0;
            break;
        }

        /* Number of links (for outbound links) plus number of pads (for
         * inbound links) is a good safe initial estimate of the total
         * number of links.
         */
        entity->max_links = entity->info.pads + entity->info.links;

        entity->pads = (struct media_pad*)malloc(entity->info.pads * sizeof(*entity->pads));
        entity->links = (struct media_link*)malloc(entity->max_links * sizeof(*entity->links));
        if (entity->pads == NULL || entity->links == NULL) {
            ret = -ENOMEM;
            break;
        }

        media->entities_count++;

        /* Find the corresponding device name. */
        if (media_entity_type(entity) != MEDIA_ENT_T_DEVNODE &&
            media_entity_type(entity) != MEDIA_ENT_T_V4L2_SUBDEV)
            continue;

        /* Fall back to get the device name via sysfs */
        media_get_devname_sysfs(entity);
        if (ret < 0)
            ALOGE("media_get_devname failed");
    }

    return ret;
}

static void media_debug_default(void *ptr, ...)
{
    va_list argptr;
    va_start(argptr, ptr);
    vprintf((const char*)ptr, argptr);
    va_end(argptr);
}

void media_debug_set_handler(struct media_device *media,
                 void (*debug_handler)(void *, ...),
                 void *debug_priv)
{
    if (debug_handler) {
        media->debug_handler = debug_handler;
        media->debug_priv = debug_priv;
    } else {
        media->debug_handler = media_debug_default;
        media->debug_priv = NULL;
    }
}

struct media_device *media_open_debug(
    const char *name, void (*debug_handler)(void *, ...),
    void *debug_priv)
{
    struct media_device *media;
    int ret;

    media = (struct media_device*)calloc(1, sizeof(struct media_device));
    if (media == NULL) {
        ALOGE("%s: media : %p", __func__, media);
        return NULL;
    }

    media_debug_set_handler(media, debug_handler, debug_priv);

    ALOGV("Opening media device %s", name);
    ALOGV("%s: media : %p", __func__, media);

    media->fd = get_media_fd(media);
    if (media->fd < 0) {
        media_close(media);
        ALOGE("%s: failed get_media_fd %s",
              __func__, name);
        return NULL;
    }

    ALOGV("%s: media->fd : %p", __func__, media->fd);
    ret = media_enum_entities(media);

    if (ret < 0) {
        ALOGE(
              "%s: Unable to enumerate entities for device %s (%s)",
              __func__, name, strerror(-ret));
        media_close(media);
        return NULL;
    }

    ALOGV("Found %u entities", media->entities_count);
    ALOGV("Enumerating pads and links");

    ret = media_enum_links(media);
    if (ret < 0) {
        ALOGE(
              "%s: Unable to enumerate pads and linksfor device %s",
              __func__, name);
        media_close(media);
        return NULL;
    }

    return media;
}

struct media_device *media_open(void)
{
    return media_open_debug(NULL, (void (*)(void *, ...))fprintf, stdout);
}

void media_close(struct media_device *media)
{
    unsigned int i;

    if (media->fd != -1)
        close(media->fd);

    for (i = 0; i < media->entities_count; ++i) {
        struct media_entity *entity = &media->entities[i];

        free(entity->pads);
        free(entity->links);
        if (entity->fd != -1)
            close(entity->fd);
    }

    free(media->entities);
    free(media);
}

struct media_pad *media_parse_pad(struct media_device *media,
                  const char *p, char **endp)
{
    unsigned int entity_id, pad;
    struct media_entity *entity;
    char *end;

    for (; isspace(*p); ++p);

    if (*p == '"') {
        for (end = (char *)p + 1; *end && *end != '"'; ++end);
        if (*end != '"')
            return NULL;

        entity = media_get_entity_by_name(media, p + 1, end - p - 1);
        if (entity == NULL)
            return NULL;

        ++end;
    } else {
        entity_id = strtoul(p, &end, 10);
        entity = media_get_entity_by_id(media, entity_id);
        if (entity == NULL)
            return NULL;
    }
    for (; isspace(*end); ++end);

    if (*end != ':')
        return NULL;
    for (p = end + 1; isspace(*p); ++p);

    pad = strtoul(p, &end, 10);
    for (p = end; isspace(*p); ++p);

    if (pad >= entity->info.pads)
        return NULL;

    for (p = end; isspace(*p); ++p);
    if (endp)
        *endp = (char *)p;

    return &entity->pads[pad];
}

struct media_link *media_parse_link(struct media_device *media,
                    const char *p, char **endp)
{
    struct media_link *link;
    struct media_pad *source;
    struct media_pad *sink;
    unsigned int i;
    char *end;

    source = media_parse_pad(media, p, &end);
    if (source == NULL)
        return NULL;

    if (end[0] != '-' || end[1] != '>')
        return NULL;
    p = end + 2;

    sink = media_parse_pad(media, p, &end);
    if (sink == NULL)
        return NULL;

    *endp = end;

    for (i = 0; i < source->entity->num_links; i++) {
        link = &source->entity->links[i];

        if (link->source == source && link->sink == sink)
            return link;
    }

    return NULL;
}

int media_parse_setup_link(struct media_device *media,
               const char *p, char **endp)
{
    struct media_link *link;
    __u32 flags;
    char *end;

    link = media_parse_link(media, p, &end);
    if (link == NULL) {
        ALOGE(
              "%s: Unable to parse link", __func__);
        return -EINVAL;
    }

    p = end;
    if (*p++ != '[') {
        ALOGE("Unable to parse link flags");
        return -EINVAL;
    }

    flags = strtoul(p, &end, 10);
    for (p = end; isspace(*p); p++);
    if (*p++ != ']') {
        ALOGE("Unable to parse link flags");
        return -EINVAL;
    }

    for (; isspace(*p); p++);
    *endp = (char *)p;

    ALOGV(
          "Setting up link %u:%u -> %u:%u [%u]",
          link->source->entity->info.id, link->source->index,
          link->sink->entity->info.id, link->sink->index,
          flags);

    return media_setup_link(media, link->source, link->sink, flags);
}

int media_parse_setup_links(struct media_device *media, const char *p)
{
    char *end;
    int ret;

    do {
        ret = media_parse_setup_link(media, p, &end);
        if (ret < 0)
            return ret;

        p = end + 1;
    } while (*end == ',');

    return *end ? -EINVAL : 0;
}
