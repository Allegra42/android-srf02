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


#include <hardware/sensors.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>

#include <linux/input.h>

#include <utils/Atomic.h>
#include <utils/Log.h>

#include "sensors.h"
#include "proximity_sensor.h"


#define LOCAL_SENSORS (1)
#define SENSORS_PROXIMITY_HANDLE 	(ID_PX)


/*
* Sensor_t struct for describing all available sensors to Android.
*/
struct sensor_t sSensorList[] = {
		{"Proximity Sensor", "SRF", 1, SENSORS_PROXIMITY_HANDLE, SENSOR_TYPE_PROXIMITY,
		700.0f, 1.0f, 0.23f, 10000000, 0, 0, 0, 0, 0 },
};


static int sensors = (sizeof (sSensorList) / sizeof (sensor_t));

static int open_sensors (const struct hw_module_t* module, const char* id, struct hw_device_t ** device);

/*
* Returns sensor_t struct describing available sensors.
*/
static int sensors__get_sensors_list (struct sensors_module_t* module, struct sensor_t const** list) {
	// ALOGE("sensor in sensors sensors__get_sensors_list()");
	*list = sSensorList;
	return sensors;
}

static struct hw_module_methods_t sensors_module_methods = {
	open: open_sensors
};

struct sensors_module_t HAL_MODULE_INFO_SYM = {
		common: {
			tag : HARDWARE_MODULE_TAG,
			version_major: 1,
			version_minor: 0,
			id: SENSORS_HARDWARE_MODULE_ID,
			name: "SRF02 Sensor module",
			author: "Anna-Lena Marx",
			methods: &sensors_module_methods,
			dso: NULL,
			reserved: {0},
		},
		get_sensors_list: sensors__get_sensors_list,
};

struct sensors_poll_context_t {
	sensors_poll_device_1_t device;

	sensors_poll_context_t ();
	~sensors_poll_context_t ();
	int activate (int handle, int enabled);
	int setDelay (int handle, int64_t ns);
	int pollEvents (sensors_event_t* data, int count);
	int batch (int handle, int flags, int64_t period_ns, int64_t timeout);

	bool isValid () {return mInitalized; };
	int flush (int handle);

private:
	enum {
		proximity = 0,
		numSensorDrivers,   
		numFds,
	};

	static const size_t wake = numFds -1; 
	const char WAKE_MESSAGE = 'W'; 
	struct pollfd mPollFds[numFds];
	int mWritePipeFd;
	SensorBase *mSensor [numSensorDrivers];
	ProximitySensor *mProximitySensor;
	bool mInitalized;

	int handleToDriver (int handle) const {
		switch (handle) {
			default:
				return proximity;
		}
		return -EINVAL;
	}
};

sensors_poll_context_t::sensors_poll_context_t() {
	
	mInitalized = false;
	memset (mSensor, 0, sizeof (mSensor));

	sensors = LOCAL_SENSORS; 

	mSensor[proximity] = new ProximitySensor();
	mPollFds[proximity].fd = mSensor[proximity]->getFd();
	mPollFds[proximity].events = POLLIN;
	mPollFds[proximity].revents	= 0;

	int wakeFds[2];
	int result = pipe(wakeFds);
	ALOGE_IF(result < 0, "error creating wake pipe (%s)", strerror(errno));
	fcntl(wakeFds[0], F_SETFL, O_NONBLOCK);
	fcntl(wakeFds[1], F_SETFL, O_NONBLOCK);
	mWritePipeFd = wakeFds[1];

	mPollFds[wake].fd = wakeFds[0];
	mPollFds[wake].events = POLLIN;
	mPollFds[wake].revents = 0;
	mInitalized = true;

}

sensors_poll_context_t::~sensors_poll_context_t() {
	for (int i = 0; i < numSensorDrivers; i++) {
		delete mSensor[i];
	}
	delete mProximitySensor;
	close (mPollFds[wake].fd);
	close (mWritePipeFd);
	mInitalized = false;
}

int sensors_poll_context_t::activate(int handle, int enabled) {
	if (!mInitalized) {
		return -EINVAL;
	}
	int index = handleToDriver(handle);
	if (index < 0) {
		return index;
	}
	int err = 0;
	return err;
}

int sensors_poll_context_t::setDelay(int handle, int64_t ns) {
	//at the moment my driver is not able to change delay
	int index = handleToDriver(handle);
	if (index < 0) {
		return index;
	}
	return mSensor[index]->setDelay(handle, ns);
}

int sensors_poll_context_t::pollEvents(sensors_event_t *data, int count) {
	int nbEvents = 0;
	int n = 0;
	int nb, polltime = -1;

	do {
		for (int i = 0; count && i < numSensorDrivers; i++) {
			SensorBase* const sensor(mSensor[i]);

			if (1) {
				nb = 0;

				nb = sensor->readEvents(data, count);
				if (nb < count) {
					mPollFds[i].revents = 0;
				}
				count -= nb;
				nbEvents += nb;
				data += nb;

				ALOGI_IF (0, "sensors:readEvents() - nb=%d, count=%d, nbEvents=%d, data->timestamp=%lld, data->data[0]=%f, ", nb, count, nbEvents, data->timestamp, data->data[0]);
			}
		}
		if (count) {
			do {
				n = poll(mPollFds, numFds, nbEvents ? 0 : polltime);
			} 
			while (n < 0 && errno == EINTR);

			if (n < 0) {
				return -errno;
			}
			if (mPollFds[wake].revents & (POLLIN | POLLPRI)) {
				char msg;
				int result = read(mPollFds[wake].fd, &msg, 1);
				ALOGE_IF (result <0, "sensor in sensors poll Events : error reading frome wake pipe (%s)", strerror(errno));
				ALOGE_IF (msg != WAKE_MESSAGE, "sensor in sensors poll Events : unknown message on wake queue (0x%02x)", int(msg));
				mPollFds[wake].revents = 0;
			}
		}

	} 
	while (n && count);

	return nbEvents;
}

int sensors_poll_context_t::batch(int handle, int flags, int64_t period_ns, int64_t timeout) {
	return 0;
}

int sensors_poll_context_t::flush(int handle) {
	return 0;
}

static int poll__close (struct hw_device_t *dev) {
	sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
	if (ctx) {
		delete ctx;
	}
	return 0;
}

static int poll__activate (struct sensors_poll_device_t *dev, int handle, int enabled) {
	sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
	return ctx->activate(handle, enabled);
}

static int poll__setDelay (struct sensors_poll_device_t *dev, int handle, int64_t ns) {
	sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
	int s = ctx->setDelay(handle, ns);
	return s;
	//my driver can not change delay at the moment
}

static int poll__poll (struct sensors_poll_device_t *dev, sensors_event_t* data, int count) {
	sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
	return ctx->pollEvents(data, count);
}

static int poll__batch (struct sensors_poll_device_1 *dev, int handle, int flags,
		int64_t period_ns, int64_t timeout) {
	sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
	return ctx->batch(handle, flags, period_ns, timeout);
}

static int poll__flush (struct sensors_poll_device_1 *dev, int handle) {
	sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
	return ctx->flush(handle);
}


static int open_sensors (const struct hw_module_t* module, const char* id, struct hw_device_t** device) {
	int status = -EINVAL;
	sensors_poll_context_t *dev = new sensors_poll_context_t();

	if (!dev->isValid()) {
		ALOGE ("Failed to open the sensors");
		return status;
	}

	memset (&dev->device, 0, sizeof (sensors_poll_device_1));

	dev->device.common.tag = HARDWARE_DEVICE_TAG;
	dev->device.common.version = SENSORS_DEVICE_API_VERSION_1_0;
	dev->device.common.module = const_cast<hw_module_t*>(module);
	dev->device.common.close = poll__close;
	dev->device.activate = poll__activate;
	dev->device.setDelay = poll__setDelay;
	dev->device.poll = poll__poll;

  	dev->device.batch = poll__batch;
    	dev->device.flush = poll__flush;

	*device = &dev->device.common;
	status = 0;

	return status;
}
