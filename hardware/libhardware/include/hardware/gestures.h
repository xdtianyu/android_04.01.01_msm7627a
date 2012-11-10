/*
 * Copyright (c) 2012 Code Aurora Forum. All rights reserved.
 * Copyright (C) 2010-2011 The Android Open Source Project
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

#ifndef ANDROID_INCLUDE_GESTURES_H
#define ANDROID_INCLUDE_GESTURES_H

#include <hardware/hardware.h>
#include <system/gestures.h>

__BEGIN_DECLS

/**
 * The id of this module
 */
#define GESTURE_HARDWARE_MODULE_ID "gestures"

typedef void (*gesture_notify_callback)(int32_t msg_type,
                                        int32_t ext1,
                                        int32_t ext2,
                                        void *user);

typedef void (*gesture_data_callback)(gesture_result_t* gs_results,
                                       void *user);

struct gesture_device;
typedef struct gesture_device_ops {
    /** Set the notification and data callbacks */
    void (*set_callbacks)(struct gesture_device *,
            gesture_notify_callback notify_cb,
            gesture_data_callback data_cb,
            void *user,
            bool isreg);

    /**
     * Start gesture.
     */
    int (*start)(struct gesture_device *);

    /**
     * Stop gesture.
     */
    void (*stop)(struct gesture_device *);

    /**
     * Set the vision parameters. This returns BAD_VALUE if any 
     * parameter is invalid or not supported. 
     */
    int (*set_parameters)(struct gesture_device *, const char *parms);

    /** Retrieve the vision parameters.  The buffer returned by the
        camera HAL must be returned back to it with put_parameters,
        if put_parameters is not NULL.
     */
    char *(*get_parameters)(struct gesture_device *);

    /**
     * Send command to vision driver.
     */
    int (*send_command)(struct gesture_device *,
                int32_t cmd, int32_t arg1, int32_t arg2);

    /**
     * Dump state of the gesture device
     */
    int (*dump)(struct gesture_device *, int fd);
} gesture_device_ops_t;

typedef struct gesture_device {
    hw_device_t common;
    gesture_device_ops_t *ops;
    void *priv;
} gesture_device_t;

typedef struct gesture_module {
    hw_module_t common;
    int (*get_number_of_gesture_devices)(void);
} gesture_module_t;

__END_DECLS

#endif /* ANDROID_INCLUDE_GESTURES_H */
