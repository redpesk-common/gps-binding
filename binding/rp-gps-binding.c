/**
 * Copyright (C) 2019-2020 IoT.bzh Company
 * Contact: https://www.iot.bzh/licensing
 * Author: Aymeric Aillet <aymeric.aillet@iot.bzh>
 *
 * This file is part of the rp-webserver module of the RedPesk project.
 *
 * $RP_BEGIN_LICENSE$
 * Commercial License Usage
 *  Licensees holding valid commercial IoT.bzh licenses may use this file in
 *  accordance with the commercial license agreement provided with the
 *  Software or, alternatively, in accordance with the terms contained in
 *  a written agreement between you and The IoT.bzh Company. For licensing terms
 *  and conditions see https://www.iot.bzh/terms-conditions. For further
 *  information use the contact form at https://www.iot.bzh/contact.
 *
 * GNU General Public License Usage
 *  Alternatively, this file may be used under the terms of the GNU General
 *  Public license version 3. This license is as published by the Free Software
 *  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
 *  of this file. Please review the following information to ensure the GNU
 *  General Public License requirements will be met
 *  https://www.gnu.org/licenses/gpl-3.0.html.
 * $RP_END_LICENSE$
 *
 * rp-server: RedPesk webui server.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <gps.h>
#include <time.h>
#include <pthread.h>
#include <json-c/json.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <urcu/list.h>
#include <math.h>


#include "rp-gps-binding.h"

#if GPSD_API_MAJOR_VERSION>6
	#define gps_read(arg) gps_read(arg, NULL, 0)
#endif

//Enable workaround
#define AGL_SPEC_802 on

//Enable NaN values from gpsd, to ensure a consistant json structure
//Disabled by default to ensure compatibility with as many clients as possible
#define SEND_NAN_VALUES false

//60 second max between 2 GPSd connection attemps
#define GPSD_CONNECT_MAX_DELAY 60

#define GPSD_POLLING_MAX_RETRIES 60
#define GPSD_POLLING_DELAY_MS 1000

//Define the max not used count for an event
#define EVENT_MAX_NOT_USED 5

//Threads management
static pthread_t MainThread;
static pthread_t EventThread;
static pthread_mutex_t GpsDataMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t EventListMutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct gpsd_connection_management_thread_userdate_s {
	char *host;                     //GPSd host address
	char *port;                     //GPSd port
	int max_retries;                //Number of retries before closing
	int nb_retries;                 //Current count of retries
	int result;                     //Used to return error
	struct gps_data_t *gps_data;    //As for now, point on global `data`			
} gpsd_connection_management_thread_userdata_t;

static struct event_list_node *list;
static struct gps_data_t data;
static bool gpsd_online;
static int max_freq;

//Supported values for each condition type
static int supported_freq[5] = {1, 10, 20, 50, 100};
static int supported_movement[6] = {1, 10, 100, 300, 500, 1000};
static int supported_speed[6] = {20, 30, 50, 90, 110, 130};

#define MSECS_TO_USECS(x) (x * 1000)
#define HZ_TO_USECS(x) (1000000/x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/* Function:  ValueIsInArray
 * --------------------
 * Check is a value is in the provided array.
 *
 * value: value to find
 * array: array to search in
 * array_size: size of the array
 *
 * returns: 1 if value was found in array
 * 			0 otherwise
 */
int ValueIsInArray(int value, int *array, int array_size) {
	int i;
	for (i = 0; i < array_size; i++) {
		if(array[i] == value) {
			return 1;
		}
	}
	return 0;
}

/* Function:  GetDistanceInMeters
 * ------------------------------
 * Calculation of the distance between two GPS 
 * points thanks to their latitude and longitude.
 *
 * lat1:  latitude of first point
 * long1: longitude of first point
 * lat2:  latitude of second point
 * long2: longitude of second point
 *
 * returns: Distance between the points in meters
 */
double GetDistanceInMeters(double lat1, double long1, double lat2, double long2) {
	lat1 = lat1 * (M_PI/180);
	long1 = long1 * (M_PI/180);
	lat2 = lat2 * (M_PI/180);
	long2 = long2 * (M_PI/180);

	double dlat = lat2 - lat1;
	double dlong = long2 - long1;

	double ans = pow(sin(dlat / 2), 2) +  cos(lat1) * cos(lat2) *  pow(sin(dlong / 2), 2);

	ans = 2 * asin(sqrt(ans));

	//Radius of earth in km
	ans = ans * 6371;

	//Conversion from km to m
	ans = ans * 1000;

	return ans;
}

/* Function:  UpdateMaxFreq
 * ------------------------
 * Browse the event list, find the event with the
 * highest operating frequency and fill the global
 * variable max_freq accordingly 
 *
 * returns: nothing
 */
void UpdateMaxFreq() {
	pthread_mutex_lock(&EventListMutex);
	event_list_node *list_cpy = list;
	pthread_mutex_unlock(&EventListMutex);

	event_list_node *tmp_node = cds_list_entry(list_cpy->list_head.next, event_list_node, list_head);
	int tmp_freq = 0;

	//Find the highest operating frequency in the event list
	while (tmp_node != list_cpy) {
		if (tmp_node->condition_type == FREQUENCY) {
			if (tmp_node->condition_value.freq > tmp_freq) {
				tmp_freq = tmp_node->condition_value.freq;
			}
		}

		tmp_node = cds_list_entry(tmp_node->list_head.next, event_list_node, list_head);
	}

	//Apply the new highest frequency
	if (max_freq != tmp_freq) {
		max_freq = tmp_freq;
		AFB_INFO("New max freq %d Hz", max_freq);
	}
}

/* Function:  AddDoubleToJson
 * --------------------------
 * Add a double to a JsonObject
 * It handle double Nan value and
 * the SEND_NAN_VALUES option.
 *  
 * data : double to add
 * jdata : json_object to add to
 * key : json key
 * 
 * returns: nothing
 */
static void AddDoubleToJson(double data, json_object *jdata, char *key) {
	json_object *JsonValue = NULL;

	if(!isnan(data) || SEND_NAN_VALUES)  {
		JsonValue = json_object_new_double(data);
		json_object_object_add(jdata, key, JsonValue);
	}
}

/* Function:  EventJsonToName
 * --------------------------
 * Generates the name of an event thanks to the 
 * information about it (condition type, value ...).
 *
 * jcondition : Json oject containing the event information.
 * result : Storing string for the event name. The caller owns the allocated string.
 *
 * returns: -1 if failed
 *          0 if name well generated
 */
int EventJsonToName(json_object *jcondition, char** result) {
	char* event_name;

	//Verification of the json structure
	struct json_object *json_data_type;
	if (!json_object_object_get_ex(jcondition, "data", &json_data_type)) return -1;
	if (!json_object_is_type(json_data_type, json_type_string)) return -1;
	const char* data_type = json_object_get_string(json_data_type);

	struct json_object *json_condition_type;
	if (!json_object_object_get_ex(jcondition, "condition", &json_condition_type)) return -1;
	if (!json_object_is_type(json_condition_type, json_type_string)) return -1;
	const char* type = json_object_get_string(json_condition_type);

	struct json_object *json_condition_value;
	if (!json_object_object_get_ex(jcondition, "value", &json_condition_value)) return -1;

	//Define event name depending of data, condition and value
	if (!strcasecmp(data_type, "gps_data")) {
		//Value type is depending on condition type
		if (!strcasecmp(type, "frequency")) {
	    	if (!json_object_is_type(json_condition_value, json_type_int)) return -1;
	    	int value = json_object_get_int(json_condition_value);
	    	if (asprintf(&event_name, "gps_data_freq_%d", value) == -1) return -1;
		}
		else if (!strcasecmp(type, "movement")) {
        	if (!json_object_is_type(json_condition_value, json_type_int)) return -1;
        	int value = json_object_get_int(json_condition_value);
        	if (asprintf(&event_name, "gps_data_movement_%d", value) == -1) return -1;
		}
		else if (!strcasecmp(type, "max_speed")) {
			if (!json_object_is_type(json_condition_value, json_type_int)) return -1;
			int value = json_object_get_int(json_condition_value);
			if (asprintf(&event_name, "gps_data_speed_%d", value) == -1) return -1;
		}
		else {
			AFB_ERROR("Unsupported event type.");
			return -1;
		}
	}
	else {
		AFB_ERROR("Unsupported data type.");
		return -1;
	}

	if (result != NULL)
		*result = event_name;
	else
		free(event_name);

	return 0;
}

/* Function:  EventListAdd
 * -----------------------
 * Add an event to the event list.
 *
 * jcondition : Json oject containing the event information.
 * is_protected : true : if the event has to be protected from deletion
 *                 false : if not
 * node : where to store the pointer to the created event
 *
 * returns: -1 if failed
 *          0 if event well created
 */
int EventListAdd(json_object *jcondition, bool is_protected, event_list_node **node, afb_req_t request) {
	event_list_node *newEvent = malloc(sizeof(event_list_node));
	CDS_INIT_LIST_HEAD(&newEvent->list_head);
	if (!newEvent) {
		AFB_ERROR("Allocation error.");
		return -1;
	}

	char* event_name;
	json_object *json_condition_type;
	if (!json_object_object_get_ex(jcondition, "condition", &json_condition_type)) return -1;
	if (!json_object_is_type(json_condition_type, json_type_string)) return -1;
	const char* type = json_object_get_string(json_condition_type);

	struct json_object *json_value;
	if (!json_object_object_get_ex(jcondition, "value", &json_value)) return -1;

	//Create the new event
	if (!strcasecmp(type, "frequency")) {
		if (!json_object_is_type(json_value, json_type_int)) return -1;
		int value = json_object_get_int(json_value);

		if (ValueIsInArray(value, supported_freq, ARRAY_SIZE(supported_freq))) {
			if (asprintf(&event_name, "gps_data_freq_%d", value) > 0) {
				afb_api_t api = afb_req_get_api(request);
				if (afb_api_new_event(api, event_name, &newEvent->event) < 0) {
					free(event_name);
					return -1;
				}
				free(event_name);
				newEvent->is_protected = is_protected;
				newEvent->not_used_count = 0;
				newEvent->condition_type = FREQUENCY;
				newEvent->condition_value.freq = value;
				clock_gettime(CLOCK_MONOTONIC, &newEvent->last_value.freq_last_send);
			}
			else return -1;
		}
		else {
			AFB_ERROR("Unsupported frequency.");
			return -1;
		}
	}
	else if (!strcasecmp(type, "movement")) {
		if (!json_object_is_type(json_value, json_type_int)) return -1;
		int value = json_object_get_int(json_value);

		if (ValueIsInArray(value, supported_movement, ARRAY_SIZE(supported_movement))) {
			if (asprintf(&event_name, "gps_data_movement_%d", value) > 0) {
				afb_api_t api = afb_req_get_api(request);
				if(afb_api_new_event(api, event_name, &newEvent->event) < 0) {
					free(event_name);
					return -1;
				}
				free(event_name);
				newEvent->is_protected = is_protected;
				newEvent->not_used_count = 0;
				newEvent->condition_type = MOVEMENT;
				newEvent->condition_value.movement_range = value;
				newEvent->last_value.movement_last_lat_lon.latitude = 0.0;
				newEvent->last_value.movement_last_lat_lon.longitude = 0.0;
			}
			else return -1;
		}
		else {
			AFB_ERROR("Unsupported movement range.");
			return -1;
		}
	}
	else if (!strcasecmp(type, "max_speed")) {
		if (!json_object_is_type(json_value, json_type_int)) return -1;
		int value = json_object_get_int(json_value);

		if (ValueIsInArray(value, supported_speed, ARRAY_SIZE(supported_speed))) {
			if (asprintf(&event_name, "gps_data_speed_%d", value) > 0) {
				afb_api_t api = afb_req_get_api(request);
				if(afb_api_new_event(api, event_name, &newEvent->event) < 0) {
					free(event_name);
					return -1;
				}
				free(event_name);
				newEvent->is_protected = is_protected;
				newEvent->not_used_count = 0;
				newEvent->condition_type = MAX_SPEED;
				newEvent->condition_value.max_speed = value;
				newEvent->last_value.above_speed = false;
			}
			else return -1;
		}
		else {
			AFB_ERROR("Unsupported max speed.");
			return -1;
		}
	}
	else {
		AFB_ERROR("Unsupported event type.");
		return -1;
	}

	//Add NewEvent to the list
	pthread_mutex_lock(&EventListMutex);
	cds_list_add_tail(&newEvent->list_head, &list->list_head);
	pthread_mutex_unlock(&EventListMutex);

	if (node != NULL) *node = newEvent;

	AFB_INFO("Event %s well created.", afb_event_name(newEvent->event));
	return 0;
}

/* Function:  EventListFind
 * ------------------------
 * Find an event in the list.
 *
 * jcondition : Json oject containing the event information.
 * found_node : where to store the pointer to the found event
 *
 * returns: false if failed
 *          true  if event found
 */
bool EventListFind(json_object *jcondition, event_list_node **found_node) {
	event_list_node *iterator;
	bool found = false;

    //Generate the name of the event we are looking for
	char* event_name = NULL;
	if (EventJsonToName(jcondition, &event_name) == -1) {
		free(event_name);
		return false;
	}

	//Browse the event list for an event with this name
	pthread_mutex_lock(&EventListMutex);
	cds_list_for_each_entry(iterator, &list->list_head, list_head) {
		if (strcmp (event_name, afb_event_name(iterator->event))) continue;

		if (*found_node != NULL) *found_node = iterator;
		found = true;
		break;
	}
	pthread_mutex_unlock(&EventListMutex);
	free(event_name);
	return found;
}

/* Function:  EventListDeleteByNode
 * --------------------------------
 * Delete an event in the list.
 *
 * node : pointer to the event to delete
 *
 * returns: false if not found
 *          true  if the event has been found and deleted
 */
bool EventListDeleteByNode(event_list_node **node) {
	event_list_node *iterator, *tmp;
	event_list_node *cpy_node = *node;
	bool deleted = false;
	pthread_mutex_lock(&EventListMutex);
	cds_list_for_each_entry_safe(iterator, tmp, &list->list_head, list_head) {
		if (strcmp (afb_event_name(cpy_node->event), afb_event_name(iterator->event))) continue;

		//Delete the event
		cds_list_del(&iterator->list_head);
		deleted = true;
		
		free(iterator);
		break;
	}
	pthread_mutex_unlock(&EventListMutex);
	return deleted;
}

/* Function:  JsonDataCompletion
 * -----------------------------
 * Marcheling of gps data in a Json object
 *
 * jdata : Json object where to temporary store data
 *
 * returns: NULL if mode fix unavailable
 *          Json object containing gps data
 */
static json_object *JsonDataCompletion(json_object *jdata) {
	json_object *JsonValue = NULL;

	if (data.fix.mode < 2) {
		json_object_put(jdata);
		return NULL;
	}

	JsonValue = json_object_new_int(data.satellites_visible);
	json_object_object_add(jdata, "visible satellites", JsonValue);

	JsonValue = json_object_new_int(data.satellites_used);
	json_object_object_add(jdata, "used satellites", JsonValue);

	JsonValue = json_object_new_int(data.fix.mode);
	json_object_object_add(jdata, "mode", JsonValue);

	AddDoubleToJson(data.fix.latitude,		jdata, "latitude");
	AddDoubleToJson(data.fix.epy,			jdata, "latitude error");
	AddDoubleToJson(data.fix.longitude,		jdata, "longitude");
	AddDoubleToJson(data.fix.epx,			jdata, "longitude error");
	AddDoubleToJson(data.fix.speed,			jdata, "speed");
	AddDoubleToJson(data.fix.eps,			jdata, "speed error");

	if (data.fix.mode == MODE_3D) {
		AddDoubleToJson(data.fix.altitude,	jdata, "altitude");
		AddDoubleToJson(data.fix.epv, 		jdata, "altitude error");
		AddDoubleToJson(data.fix.climb, 	jdata, "climb");
		AddDoubleToJson(data.fix.epc, 		jdata, "climb error");
	}

	AddDoubleToJson(data.fix.track, 		jdata, "heading (true north)");
	AddDoubleToJson(data.fix.epd, 			jdata, "heading error");

	//Support the change from timestamp_t (double) to timespec struct (done with API 9.0)
	#if GPSD_API_MAJOR_VERSION>8
		double timestamp_converted = (double)data.fix.time.tv_sec + ((double)data.fix.time.tv_nsec / 1000000000);
		JsonValue = json_object_new_double(timestamp_converted);
	#else
		JsonValue = json_object_new_double(data.fix.time);
	#endif

	json_object_object_add(jdata, "timestamp", JsonValue);

	AddDoubleToJson(data.fix.ept, 			jdata, "timestamp error");

	return jdata;
}

/* Function:  GetGpsData
 * ---------------------
 * Callback for "gps-data" verb.
 * It builds the gps date json structure and returns it.
 *
 * request : Request from the afb client.
 *
 * returns: nothing
 * Error code 1 : JsonData does not have enough data to be reliable 
 */
static void GetGpsData(afb_req_t request, unsigned argc, afb_data_t const argv[]) {
	json_object *JsonData = NULL;
	
	pthread_mutex_lock(&GpsDataMutex);
	JsonData = JsonDataCompletion(json_object_new_object());
	pthread_mutex_unlock(&GpsDataMutex);
	
	if (JsonData) {
		afb_req_reply_json_c_hold(request, 0, JsonData);
	}
	else {
		afb_req_reply_string(request, AFB_USER_ERRNO(1), "not enough data to be reliable\n");
	} 
}

/* Function:  Subscribe
 * --------------------
 * Callback for "subscribe" verb.
 * Subscribe a client to a dynamic event.
 * Create the asked event if not found in the event list.
 *
 * request : Request from the client
 *
 * returns: nothing
 */
static void Subscribe(afb_req_t request, unsigned argc, afb_data_t const argv[]) {
	afb_data_t result;
	
	if (afb_req_param_convert(request, 0, AFB_PREDEFINED_TYPE_JSON_C, &result) < 0) {
		afb_req_reply_string(request, AFB_ERRNO_INVALID_REQUEST, "failed to convert argument to JSON_C");
		return;
	}

	json_object *json_request = (json_object *) afb_data_ro_pointer(result);
	if (!json_request) {
		afb_req_reply_string(request, AFB_ERRNO_INVALID_REQUEST, "failed to get pointer to argument");
		return;
	}

	event_list_node *event_to_subscribe;

	if (!EventJsonToName(json_request, NULL)) {
		if (!EventListFind(json_request, &event_to_subscribe)) {
			AFB_INFO("Event not found.");
			if (!EventListAdd(json_request, false, &event_to_subscribe, request)) {
				AFB_INFO("Event %s added.", afb_event_name(event_to_subscribe->event));
				UpdateMaxFreq();
			}
			else {
				afb_req_reply_string(request, AFB_ERRNO_INVALID_REQUEST, "Event creation failed");
				return;
			}
		}

		if (afb_req_subscribe(request, event_to_subscribe->event) == 0) {
			AFB_INFO("Subscribed to event %s.", afb_event_name(event_to_subscribe->event));
			afb_data_addref(result);
			afb_req_reply(request, 0, 1, &result);
		}
				
		else afb_req_reply_string(request, AFB_ERRNO_INVALID_REQUEST, "Subscription error");
	}
	else afb_req_reply_string(request, AFB_ERRNO_INVALID_REQUEST, "Request isn't well formated, please see readme");
	
	return;
}

/* Function:  Unsubscribe
 * ----------------------
 * Callback for "unsubscribe" verb.
 * Unsubscribe a client to a dynamic event.
 *
 * request : Request from the client
 *
 * returns: nothing
 */
static void Unsubscribe(afb_req_t request, unsigned argc, afb_data_t const argv[]) {
	afb_data_t result;
	
	if (afb_req_param_convert(request, 0, AFB_PREDEFINED_TYPE_JSON_C, &result) < 0) {
		afb_req_reply_string(request, AFB_ERRNO_INVALID_REQUEST, "failed to convert argument to JSON_C");
		return;
	}

	json_object *json_request = (json_object *) afb_data_ro_pointer(result);
	if (!json_request) {
		afb_req_reply_string(request, AFB_ERRNO_INVALID_REQUEST, "failed to get pointer to argument");
		return;
	}
	event_list_node *event_to_unsubscribe;

	if (!EventJsonToName(json_request, NULL)) {
		if (EventListFind(json_request, &event_to_unsubscribe)) {
			//Event was found in list
			if (afb_req_unsubscribe(request, event_to_unsubscribe->event) == 0) {
				//Unsubscribe successfully, keep the event for another hypothetical client

				afb_data_addref(result);
				afb_req_reply(request, 0, 1, &result);
			}
			else afb_req_reply_string(request, AFB_ERRNO_INVALID_REQUEST, "Unsubscription error");
		}
		else afb_req_reply_string(request, AFB_ERRNO_INVALID_REQUEST, "Event does not exist");
	}
	else afb_req_reply_string(request, AFB_ERRNO_INVALID_REQUEST, "Request isn't well formated");

	return;
}

extern const char * info_verbS;

/* Function:  infoVerb
 * ----------------------
 * Callback for "infoVerb" verb.
 *
 * request :
 *
 * returns: 
 */
static void infoVerb (afb_req_t request, unsigned argc, afb_data_t const argv[]) {

	enum json_tokener_error jerr;

	json_object * infoArgsJ = json_tokener_parse_verbose(info_verbS, &jerr);
	if (infoArgsJ == NULL || jerr != json_tokener_success) {
		afb_req_reply_string(request, AFB_ERRNO_INVALID_REQUEST, "failure while packing info() verb arguments");
        return;
    }
	afb_req_reply_json_c_hold(request, 0, infoArgsJ);
	
	return;
}

/* Function:  GpsdPolling
 * ----------------------
 * Store gps data as long as the GPSd connection is sustainable.
 *
 * returns: nothing
 */
static void* GpsdPolling(void *ptr) {
	int tries = 0;

	while(tries < GPSD_POLLING_MAX_RETRIES) {
		if (!gps_waiting(&data, MSECS_TO_USECS(GPSD_POLLING_DELAY_MS))) {
			tries++;
			continue;
		}
		pthread_mutex_lock(&GpsDataMutex);
		if (gps_read(&data) == -1) {
			AFB_ERROR("Cannot read from GPS daemon (errno: %d, \"%s\").\n", errno, gps_errstr(errno));
			pthread_mutex_unlock(&GpsDataMutex);
			break;
		}
		pthread_mutex_unlock(&GpsDataMutex);
	}

	AFB_INFO("GPSd connection lost, closing.\n");
	gpsd_online = false;
	gps_stream(&data, WATCH_DISABLE, NULL);
	gps_close(&data);

	return NULL;
}

/* Function:  EventManagementThread
 * --------------------------------
 * Thread browsing the list and sending events to clients 
 * as long as the connection with GPSd is sustainable.
 *
 * returns: nothing
 */
static void* EventManagementThread(void *arg) {
	AFB_INFO("Event management thread online !");

	while (gpsd_online) {
		//Start from the head of the list
		pthread_mutex_lock(&EventListMutex);
		event_list_node *list_cpy = list;
		pthread_mutex_unlock(&EventListMutex);


		pthread_mutex_lock(&GpsDataMutex);
		json_object *jdata = JsonDataCompletion(json_object_new_object());
		pthread_mutex_unlock(&GpsDataMutex);

		if (!jdata) continue;
        
		event_list_node *tmp = cds_list_entry(list_cpy->list_head.next, event_list_node, list_head);
		event_list_node *next = tmp;

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);

		bool found_freq_event = false;

		//Browsing list
		while (tmp != list_cpy) {
			next = cds_list_entry(tmp->list_head.next, event_list_node, list_head);

			if (tmp->condition_type == FREQUENCY) {
				found_freq_event = true;
				long accum_us = (now.tv_sec - tmp->last_value.freq_last_send.tv_sec) * 1000000 + (now.tv_nsec - tmp->last_value.freq_last_send.tv_nsec) / 1000;
				
				//Enough time has passed
				if (accum_us > HZ_TO_USECS(tmp->condition_value.freq)) {
					json_object_get(jdata);
					afb_data_t data = afb_data_json_c_hold(jdata);					
					if (afb_event_push(tmp->event, 1, &data) == 0) {
						if(!tmp->is_protected) {
							//If an unprotected event is not used anymore, delete it
							tmp->not_used_count++;
							if (tmp->not_used_count >= EVENT_MAX_NOT_USED) {
								if (EventListDeleteByNode(&tmp)) {
									UpdateMaxFreq();
								}
							}
						}
					}
					//Event well pushed
					else {
						if (tmp->not_used_count) tmp->not_used_count = 0;
					}
					//Update time from last check even if not pushed
					tmp->last_value.freq_last_send = now;
				}

			}
			else if (tmp->condition_type == MOVEMENT) {
				struct json_object *json_latitude, *json_longitude;
				json_object_object_get_ex(jdata, "latitude", &json_latitude);
				json_object_object_get_ex(jdata, "longitude", &json_longitude);
				double latitude = json_object_get_double(json_latitude);
				double longitude = json_object_get_double(json_longitude);

				//Distance is higher than the event trigger
				if (GetDistanceInMeters(tmp->last_value.movement_last_lat_lon.latitude, tmp->last_value.movement_last_lat_lon.longitude, latitude, longitude) > tmp->condition_value.movement_range) {
					//Event push return an error
					json_object_get(jdata);
					afb_data_t data = afb_data_json_c_hold(jdata);
					if (afb_event_push(tmp->event, 1, &data) == 0) {
						if(!tmp->is_protected) {
							//If an unprotected event is not used anymore, delete it
							tmp->not_used_count++;
							if (tmp->not_used_count >= EVENT_MAX_NOT_USED) {
								EventListDeleteByNode(&tmp);
							}
						}
					}
					//Event well pushed
					else {
						if (tmp->not_used_count) tmp->not_used_count = 0;
						tmp->last_value.movement_last_lat_lon.latitude = latitude;
						tmp->last_value.movement_last_lat_lon.longitude = longitude;
					}
				}

			}
			else if (tmp->condition_type == MAX_SPEED) {
				struct json_object *json_speed;
				json_object_object_get_ex(jdata, "speed", &json_speed);
				double speed = json_object_get_double(json_speed);

				//Speed is higher than the event trigger
				if ((speed*3.6) > (double)(tmp->condition_value.max_speed)) {
					//Speed wasn't higher than trigger last time
					if (!tmp->last_value.above_speed) {
						//Event push return an error
						json_object_get(jdata);
						afb_data_t data = afb_data_json_c_hold(jdata);
						if (afb_event_push(tmp->event, 1, &data) == 0) {
							if(!tmp->is_protected) {
								//If an unprotected event is not used anymore, delete it
								tmp->not_used_count++;
								if (tmp->not_used_count >= EVENT_MAX_NOT_USED) {
									EventListDeleteByNode(&tmp);
								}
							}
						}
						//Event well pushed
						else {
							if (tmp->not_used_count) tmp->not_used_count = 0;
							tmp->last_value.above_speed = true;
						}
					}
				}
				//Speed isn't higher than trigger
				else {
					tmp->last_value.above_speed = false;
				}
			}

			tmp = next;
		}

		json_object_put(jdata);

		//Wait 1s if no frequency related event have been found
        //Be sure that max_freq has been populated (to avoid synchronisation problem, causing a 0s waiting time)
		if (found_freq_event && (max_freq > 0)) usleep(HZ_TO_USECS(max_freq));
		else sleep(1);
	}
	AFB_INFO("Event management thread offline !");
	pthread_exit(NULL);
}

/* Function:  GpsdConnectionManagementThread
 * -----------------------------------------
 * Main thread of the gps binding.
 * It manage the first connection to GPSd and also further connection attemps.
 * It also manage the launch of the event management thread if the GPSd connection is up.
 *
 * returns: nothing
 */
static void* GpsdConnectionManagementThread(void *arg) {
	gpsd_connection_management_thread_userdata_t *userdata = arg;

	//Exit condition, if any, occurs in connection loop
	while (true) { 
		int ret = -1;
		unsigned int delay = 1;

		//Try to open GPSd connection
		//Retry forever if max_retries <= 0
		while (ret != 0 && (userdata->max_retries <= 0 || userdata->nb_retries++ < userdata->max_retries)) {
			ret = gps_open(userdata->host, userdata->port, userdata->gps_data);
			if (ret != 0) {
				AFB_NOTICE("GPSd not available yet (errno: %d, \"%s\"). Wait for %.2d seconds before retry...", errno, gps_errstr(errno), delay);
				sleep(delay);
				delay *= 2;
				if (delay > GPSD_CONNECT_MAX_DELAY) {
					delay=GPSD_CONNECT_MAX_DELAY;
				}
			}
		}
		//Exit if gps_open returned an error at last try
		if (ret != 0) {
			AFB_ERROR("Too many retries, aborting...");
			userdata->result = ret;
			return &userdata->result; //aka thread_exit()
		}

		gps_stream(userdata->gps_data, WATCH_ENABLE | WATCH_JSON, NULL);
		#ifdef AGL_SPEC_802
			int tries = 5;
			//Due to the gpsd.socket race condition need to loop until initial event
			do {
				gps_read(userdata->gps_data);
			} while (!gps_waiting(userdata->gps_data, MSECS_TO_USECS(1000)) && tries--);
		#endif
		AFB_INFO("Connected to GPSd");
		
		gpsd_online = true;
		userdata->nb_retries = 0; //Reset counter for next try
		
		//Launch the event management thread
		ret = pthread_create(&EventThread, NULL, &EventManagementThread, NULL);
		if (ret != 0) {
			AFB_ERROR("Could not create thread for event handling...");
		}

		pthread_detach(EventThread);

		//GpsdPolling returns if GPSd connection's lost
		GpsdPolling(NULL);
	}
}

/* Function:  GpsInit
 * ------------------
 * Initialize the connection to GSPd and start the main thread.
 *
 * returns: 0 if went well
 *          other if not
 */
static int GpsInit() {
	int ret;

	gpsd_connection_management_thread_userdata_t *userdata = malloc(sizeof(gpsd_connection_management_thread_userdata_t));
	if (userdata == NULL) {
		AFB_ERROR("Memory allocation error for GPSd thread");
		return -1;
	}

	userdata->host = getenv("RPGPS_HOST") ? : "localhost";
	userdata->port = getenv("RPGPS_SERVICE") ? : "2947";
	userdata->max_retries = -1;		//Value <= 0 for infinite
	userdata->nb_retries = 0;
	userdata->gps_data = &data;

	ret = pthread_create(&MainThread, NULL, &GpsdConnectionManagementThread, userdata);
	if (ret != 0) {
		AFB_ERROR("Could not create thread for listening to GPSd socket...");
		return ret;
	}

	pthread_detach(MainThread);
	return 0;
}

/**
* Binding CallBack
* @param api the api that receive the CallBack
* @param ctlarg    data associated to the call
* @param userdata  the userdata of the api (@see afb_api_get_userdata)
*/
int binding_ctl(afb_api_t api, afb_ctlid_t ctlid, afb_ctlarg_t ctlarg, void *userdata) {
	switch(ctlid) {
	case afb_ctlid_Init:
		gpsd_online = false;
		max_freq = 0;
		list = malloc(sizeof(event_list_node));
		CDS_INIT_LIST_HEAD(&list->list_head);

		AFB_API_NOTICE(api, "Configuring GPSd connection !");
		if (GpsInit()) {
			AFB_API_ERROR(api, "Encountered a problem while launching GPSd management thread");
			return -1;
		}
		break;
	default:
		break;
	}
	return 0;
}

static const afb_verb_t binding_verbs[] = {
	{
		.verb = "gps-data",
		.callback = GetGpsData,
		.info = "Get GNSS data"
	},
	{
		.verb = "subscribe",
		.callback = Subscribe,
		.info = "Subscribe to GNSS events with condition"
	},
	{
		.verb = "unsubscribe",
		.callback = Unsubscribe,
		.info = "Unsubscribe to GNSS events with conditions"
	},
	{
		.verb = "info",
		.callback = infoVerb,
		.info = "API info"
	},
	{
		.verb = NULL /*marker for the end of the array*/
	}
};


/*
 * binder API description
 */
const struct afb_binding_v4 afbBindingV4 = {
	.api = "gps",
	.specification = "GNSS/GPS API",
	.verbs = binding_verbs,
	.mainctl = binding_ctl,
};
