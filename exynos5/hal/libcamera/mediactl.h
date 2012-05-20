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

#ifndef __MEDIA_H__
#define __MEDIA_H__

#include <media.h>

#define GAIA_FW_BETA 1

#ifndef GAIA_FW_BETA
#define MEDIA_DEV                          "/dev/media1"    //M5MO : External ISP
#else
#define MEDIA_DEV                          "/dev/media2"    //4E5  : Internal ISP
#endif

#define MEDIA_MINOR                         0

#define KF_MSG                              0x1
#define KF_ANY                              0x2

struct media_link {
    struct media_pad *source;
    struct media_pad *sink;
    struct media_link *twin;
    __u32 flags;
    __u32 padding[3];
};

struct media_pad {
    struct media_entity *entity;
    __u32 index;
    __u32 flags;
    __u32 padding[3];
};

struct media_entity {
    struct media_device *media;
    struct media_entity_desc info;
    struct media_pad *pads;
    struct media_link *links;
    unsigned int max_links;
    unsigned int num_links;

    char devname[32];
    int fd;
    __u32 padding[6];
};

struct media_device {
    int fd;
    struct media_entity *entities;
    unsigned int entities_count;
    void (*debug_handler)(void *, ...);
    void *debug_priv;
    __u32 padding[6];
};

#define media_dbg(media, ...) \
    (media)->debug_handler((media)->debug_priv, __VA_ARGS__)

/**
 * @brief Set a handler for debug messages.
 * @param media - device instance.
 * @param debug_handler - debug message handler
 * @param debug_priv - first argument to debug message handler
 *
 * Set a handler for debug messages that will be called whenever
 * debugging information is to be printed. The handler expects an
 * fprintf-like function.
 */
void media_debug_set_handler(
    struct media_device *media, void (*debug_handler)(void *, ...),
    void *debug_priv);

/**
 * @brief Open a media device with debugging enabled.
 * @param name - name (including path) of the device node.
 * @param debug_handler - debug message handler
 * @param debug_priv - first argument to debug message handler
 *
 * Open the media device referenced by @a name and enumerate entities, pads and
 * links.
 *
 * Calling media_open_debug() instead of media_open() is equivalent to
 * media_open() and media_debug_set_handler() except that debugging is
 * also enabled during media_open().
 *
 * @return A pointer to a newly allocated media_device structure instance on
 * success and NULL on failure. The returned pointer must be freed with
 * media_close when the device isn't needed anymore.
 */
struct media_device *media_open_debug(
    const char *name, void (*debug_handler)(void *, ...),
    void *debug_priv);

/**
 * @brief Open a media device.
 * @param name - name (including path) of the device node.
 *
 * Open the media device referenced by @a name and enumerate entities, pads and
 * links.
 *
 * @return A pointer to a newly allocated media_device structure instance on
 * success and NULL on failure. The returned pointer must be freed with
 * media_close when the device isn't needed anymore.
 */
struct media_device *media_open(void);

/**
 * @brief Close a media device.
 * @param media - device instance.
 *
 * Close the @a media device instance and free allocated resources. Access to the
 * device instance is forbidden after this function returns.
 */
void media_close(struct media_device *media);

/**
 * @brief Locate the pad at the other end of a link.
 * @param pad - sink pad at one end of the link.
 *
 * Locate the source pad connected to @a pad through an enabled link. As only one
 * link connected to a sink pad can be enabled at a time, the connected source
 * pad is guaranteed to be unique.
 *
 * @return A pointer to the connected source pad, or NULL if all links connected
 * to @a pad are disabled. Return NULL also if @a pad is not a sink pad.
 */
struct media_pad *media_entity_remote_source(struct media_pad *pad);

/**
 * @brief Get the type of an entity.
 * @param entity - the entity.
 *
 * @return The type of @a entity.
 */
static inline unsigned int media_entity_type(struct media_entity *entity)
{
    return entity->info.type & MEDIA_ENT_TYPE_MASK;
}

/**
 * @brief Find an entity by its name.
 * @param media - media device.
 * @param name - entity name.
 * @param length - size of @a name.
 *
 * Search for an entity with a name equal to @a name.
 *
 * @return A pointer to the entity if found, or NULL otherwise.
 */
struct media_entity *media_get_entity_by_name(struct media_device *media,
    const char *name, size_t length);

/**
 * @brief Find an entity by its ID.
 * @param media - media device.
 * @param id - entity ID.
 *
 * Search for an entity with an ID equal to @a id.
 *
 * @return A pointer to the entity if found, or NULL otherwise.
 */
struct media_entity *media_get_entity_by_id(struct media_device *media,
    __u32 id);

/**
 * @brief Configure a link.
 * @param media - media device.
 * @param source - source pad at the link origin.
 * @param sink - sink pad at the link target.
 * @param flags - configuration flags.
 *
 * Locate the link between @a source and @a sink, and configure it by applying
 * the new @a flags.
 *
 * Only the MEDIA_LINK_FLAG_ENABLED flag is writable.
 *
 * @return 0 on success, -1 on failure:
 *       -ENOENT: link not found
 *       - other error codes returned by MEDIA_IOC_SETUP_LINK
 */
int media_setup_link(struct media_device *media,
    struct media_pad *source, struct media_pad *sink,
    __u32 flags);

/**
 * @brief Reset all links to the disabled state.
 * @param media - media device.
 *
 * Disable all links in the media device. This function is usually used after
 * opening a media device to reset all links to a known state.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int media_reset_links(struct media_device *media);

/**
 * @brief Parse string to a pad on the media device.
 * @param media - media device.
 * @param p - input string
 * @param endp - pointer to string where parsing ended
 *
 * Parse NULL terminated string describing a pad and return its struct
 * media_pad instance.
 *
 * @return Pointer to struct media_pad on success, NULL on failure.
 */
struct media_pad *media_parse_pad(struct media_device *media,
                  const char *p, char **endp);

/**
 * @brief Parse string to a link on the media device.
 * @param media - media device.
 * @param p - input string
 * @param endp - pointer to p where parsing ended
 *
 * Parse NULL terminated string p describing a link and return its struct
 * media_link instance.
 *
 * @return Pointer to struct media_link on success, NULL on failure.
 */
struct media_link *media_parse_link(struct media_device *media,
                    const char *p, char **endp);

/**
 * @brief Parse string to a link on the media device and set it up.
 * @param media - media device.
 * @param p - input string
 *
 * Parse NULL terminated string p describing a link and its configuration
 * and configure the link.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int media_parse_setup_link(struct media_device *media,
               const char *p, char **endp);

/**
 * @brief Parse string to link(s) on the media device and set it up.
 * @param media - media device.
 * @param p - input string
 *
 * Parse NULL terminated string p describing link(s) separated by
 * commas (,) and configure the link(s).
 *
 * @return 0 on success, or a negative error code on failure.
 */
int media_parse_setup_links(struct media_device *media, const char *p);

#endif
