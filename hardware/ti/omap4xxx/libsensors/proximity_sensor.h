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


#ifndef PROXIMITY_SENSOR_H_
#define PROXIMITY_SENSOR_H_

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include "SensorBase.h"
#include "InputEventReader.h"
#include "sensors.h"

struct input_event;


class ProximitySensor : public SensorBase {
private:
	int mEnabled;
	InputEventCircularReader mInputReader;
	sensors_event_t mPendingEvent;
	bool mHasPendingEvent;
	static size_t numEvents;

	int setInitialState();
	float indexToValue(size_t index) const;

public:
	ProximitySensor ();
	virtual ~ProximitySensor ();
	virtual int readEvents (sensors_event_t* data, int count);
	virtual bool hasPendingEvents () const;
	virtual int enable (int32_t handle, int enabled);
    virtual int setEnable(int32_t handle, int enabled);
    virtual int getEnable(int32_t handle);
};


#endif /* PROXIMITY_SENSOR_H_ */
