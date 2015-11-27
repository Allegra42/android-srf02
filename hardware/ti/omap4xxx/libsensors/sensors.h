/*
 * Copyright (C) 2015 Anna-Lena Marx
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


#ifndef SENSORS_H_
#define SENSORS_H_

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <linux/input.h>

#include <hardware/hardware.h>
#include <../../../akm/AK8975_FS/libsensors/sensors.h>

__BEGIN_DECLS


#define I2C 	"/sys/bus/i2c/devices/i2c-4/4-0070/"
#define PROXIMITY_DATA "SRF02 proximity sensor"
#define INPUT_EVENT_DEBUG (0)
#define DEBUG (0)


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof (a) / sizeof(a[0]))
#endif

enum {
	ID_PX
};


__END_DECLS

#endif /* SENSORS_H_ */
