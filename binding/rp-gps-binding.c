/*
 * LICENSE file & header to complete
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

#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>

#include "rp-gps-binding.h"

#if GPSD_API_MAJOR_VERSION>6
	#define gps_read(arg) gps_read(arg, NULL, 0)
#endif

// enable workaround
#define AGL_SPEC_802 on

// 60 second max between 2 gpsd connection attemps
#define GPSD_CONNECT_MAX_DELAY 60

#define GPSD_POLLING_MAX_RETRIES 60
#define GPSD_POLLING_DELAY_MS 2000

//Threads management
static pthread_t thread;
static pthread_t thread_event;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct gpsd_connection_management_thread_userdate_s {
	char *host;
	char *port;
	int max_retries; 	// 0 or negative values => no maximum
	int nb_retries;   	// current count of retries
	struct gps_data_t *gps_data; // as for now, point on global `data`
	int result;
} gpsd_connection_management_thread_userdata_t;

static struct event_list_node *list;
static struct gps_data_t data;
static bool gpsd_online;
static int max_freq;

#define MSECS_TO_USECS(x) (x * 1000)
#define HZ_TO_USECS(x) (1000000/x)

double distance_m(double lat1, double long1, double lat2, double long2){
	lat1 = lat1 * (M_PI/180);
	long1 = long1 * (M_PI/180);
	lat2 = lat2 * (M_PI/180);
	long2 = long2 * (M_PI/180);

	double dlat = lat2 - lat1;
	double dlong = long2 - long1;

	double ans = pow(sin(dlat / 2), 2) +  cos(lat1) * cos(lat2) *  pow(sin(dlong / 2), 2);

	ans = 2 * asin(sqrt(ans));

	//radius of earth in km
	ans = ans * 6371;

	//km to m
	ans = ans * 1000;

	AFB_INFO("distance in metters : %f", ans);

	return ans;

}

void UpdateMaxFreq(){
	pthread_mutex_lock(&list_mutex);
	event_list_node *list_cpy = list;
	pthread_mutex_unlock(&list_mutex);

	event_list_node *tmp_node = cds_list_entry(list_cpy->list_head.next, event_list_node, list_head);
	int tmp_freq = 0;

	//parcours de la liste
	while(tmp_node != list_cpy){
		if(tmp_node->condition_type == FREQUENCY){
			if(tmp_node->condition_value.freq > tmp_freq){
				tmp_freq = tmp_node->condition_value.freq;
			}
		}

		tmp_node = cds_list_entry(tmp_node->list_head.next, event_list_node, list_head);

		usleep(100000);
	}
	if(max_freq != tmp_freq){
		max_freq = tmp_freq;
		AFB_INFO("New max freq %d Hz", max_freq);
	}
}

int EventJsonToName(json_object *jcondition, char** result){
	char* event_name;
	struct json_object *json_condition_type;
	if(!json_object_object_get_ex(jcondition, "condition", &json_condition_type)) return -1;
	if(!json_object_is_type(json_condition_type, json_type_string)) return -1;
	const char* type = json_object_get_string(json_condition_type);

	struct json_object *json_condition_value;
	if(!json_object_object_get_ex(jcondition, "value", &json_condition_value)) return -1;

	//only way to know the type of "value" is to check the type of the condition before
	if(!strcasecmp(type, "frequency")) {
		if(!json_object_is_type(json_condition_value, json_type_int)) return -1;
		int value = json_object_get_int(json_condition_value);
		if(asprintf(&event_name, "gps_data_freq_%d", value) == -1) return -1;
	}
	else if(!strcasecmp(type, "movement")) {
		if(!json_object_is_type(json_condition_value, json_type_int)) return -1;
		int value = json_object_get_int(json_condition_value);
		if(asprintf(&event_name, "gps_data_movement_%d", value) == -1) return -1;
	}
	else if (!strcasecmp(type, "max_speed")){
		if(!json_object_is_type(json_condition_value, json_type_int))	return -1;
		int value = json_object_get_int(json_condition_value);
		if(asprintf(&event_name, "gps_data_speed_%d", value) == -1) return -1;
	}
	else {
		AFB_ERROR("Unsuported event type.");
		return -1;
	}

	if(result != NULL) *result = event_name;


	return 0;
}

int EventListAdd(json_object *jcondition, bool is_disposable, event_list_node **node){
	event_list_node *newEvent = malloc(sizeof(event_list_node));
	CDS_INIT_LIST_HEAD(&newEvent->list_head);
	if (!newEvent) {
		AFB_ERROR("Allocation error.");
		return -1;
	}

	char* event_name;
	struct json_object *json_condition_type;
	if(!json_object_object_get_ex(jcondition, "condition", &json_condition_type)) return -1;
	if(!json_object_is_type(json_condition_type, json_type_string)) return -1;
	const char* type = json_object_get_string(json_condition_type);

	struct json_object *json_value;
	if(!json_object_object_get_ex(jcondition, "value", &json_value)) return -1;

	if(!strcasecmp(type, "frequency")) {
		if(!json_object_is_type(json_value, json_type_int)) return -1;
		int value = json_object_get_int(json_value);

		if(value == 1 || value == 10 || value == 20 || value == 50 || value == 100) {
			if(asprintf(&event_name, "gps_data_freq_%d", value) > 0){
				newEvent->event = afb_daemon_make_event(event_name);
				newEvent->is_disposable = is_disposable;
				newEvent->not_used_count = 0;
				newEvent->condition_type = FREQUENCY;
				newEvent->condition_value.freq = value;
				clock_gettime(CLOCK_MONOTONIC, &newEvent->last_value.freq_last_send);
			}
			else return -1;
		}
		else {
			AFB_ERROR("Unsuported frequency.");
			return -1;
		}
	}
	else if(!strcasecmp(type, "movement")) {
		if(!json_object_is_type(json_value, json_type_int)) return -1;
		int value = json_object_get_int(json_value);

		if(value == 1 || value == 10 || value == 100 || value == 300 || value == 500 || value == 1000) {
			if(asprintf(&event_name, "gps_data_movement_%d", value) > 0){
				newEvent->event = afb_daemon_make_event(event_name);
				newEvent->is_disposable = is_disposable;
				newEvent->not_used_count = 0;
				newEvent->condition_type = MOVEMENT;
				newEvent->condition_value.movement_range = value;
				newEvent->last_value.movement_last_lat_lon.latitude = 0.0;
				newEvent->last_value.movement_last_lat_lon.longitude = 0.0;
			}
			else return -1;
		}
		else {
			AFB_ERROR("Unsuported movement range.");
			return -1;
		}
	}
	else if (!strcasecmp(type, "max_speed")){
		if(!json_object_is_type(json_value, json_type_int)) return -1;
		int value = json_object_get_int(json_value);

		if(value == 20 || value == 30 || value == 50 || value == 90 || value == 110 || value == 130) {
			if(asprintf(&event_name, "gps_data_speed_%d", value) > 0){
				newEvent->event = afb_daemon_make_event(event_name);
				newEvent->is_disposable = is_disposable;
				newEvent->not_used_count = 0;
				newEvent->condition_type = MAX_SPEED;
				newEvent->condition_value.max_speed = value;
				newEvent->last_value.above_speed = false;
			}
			else return -1;
		}
		else {
			AFB_ERROR("Unsuported max speed.");
			return -1;
		}
	}
	else {
		AFB_ERROR("Unsuported event type.");
		return -1;
	}

	//add NewEvent to the list
	pthread_mutex_lock(&list_mutex);
	cds_list_add_tail(&newEvent->list_head, &list->list_head);
	pthread_mutex_unlock(&list_mutex);

	if(*node != NULL) *node = newEvent;

	AFB_INFO("Event well created.");
	return 0;
}

bool EventListFind(json_object *jcondition, event_list_node **from_node, event_list_node **found_node, int mode){
	event_list_node *iterator;
	//event_list_node *cpy_start = *from_node;
	bool found = false;
	if (mode == 1){
		//find next event by type+value (using its event name)
		char* event_name = NULL;
		if(EventJsonToName(jcondition, &event_name) == -1) return false;

		pthread_mutex_lock(&list_mutex);
		cds_list_for_each_entry(iterator, &list->list_head, list_head) {
			if (strcmp (event_name, afb_event_name(iterator->event))) continue;

			if(*found_node != NULL) *found_node = iterator;
			found = true;
			break;
		}
		pthread_mutex_unlock(&list_mutex);
		free(event_name);
		return found;
	}
	else if(mode == 2){
		//find next event by type
		struct json_object *json_condition_type;
		if(!json_object_object_get_ex(jcondition, "condition", &json_condition_type)) return -1;
		if(!json_object_is_type(json_condition_type, json_type_string)) return -1;
		const char* type = json_object_get_string(json_condition_type);
		enum condition_type_enum enum_type;

		if(!strcmp(type, "frequency")) enum_type = FREQUENCY;
		if(!strcmp(type, "movement")) enum_type = MOVEMENT;
		if(!strcmp(type, "max_speed")) enum_type = MAX_SPEED;

		pthread_mutex_lock(&list_mutex);
		cds_list_for_each_entry(iterator, &list->list_head, list_head) {
			if (iterator->condition_type != enum_type) continue;

			if(*found_node != NULL) *found_node = iterator;
			found = true;
			break;
		}
		pthread_mutex_unlock(&list_mutex);
		return found;
	}
	else
	{
		AFB_ERROR("[EventFind] Unrecognized mode.");
		return false;
	}
}

bool EventListDeleteByJson(json_object *jcondition){
	char* event_name = NULL;
	if(EventJsonToName(jcondition, &event_name) == -1) return false;

	event_list_node *iterator, *tmp;
	bool suppressed = false;
	pthread_mutex_lock(&list_mutex);
	cds_list_for_each_entry_safe(iterator, tmp, &list->list_head, list_head) {
		if (strcmp (event_name, afb_event_name(iterator->event))) continue;

		cds_list_del(&iterator->list_head);
		suppressed = true;
		break;
	}
	pthread_mutex_unlock(&list_mutex);
	free(event_name);
	return suppressed;
}

bool EventListDeleteByNode(event_list_node **node){
	event_list_node *iterator, *tmp;
	event_list_node *cpy_node = *node;
	bool suppressed = false;
	pthread_mutex_lock(&list_mutex);
	cds_list_for_each_entry_safe(iterator, tmp, &list->list_head, list_head) {
		if (strcmp (afb_event_name(cpy_node->event), afb_event_name(iterator->event))) continue;

		cds_list_del(&iterator->list_head);
		free(iterator);
		suppressed = true;
		break;
	}
	pthread_mutex_unlock(&list_mutex);
	return suppressed;
}

static json_object *json_data_completion(json_object *jdata)
{
	json_object *value = NULL;

	if (data.fix.mode < 2) {
		json_object_put(jdata);
		return NULL;
	}

	if(data.set & SATELLITE_SET){
		value = json_object_new_int(data.satellites_visible);
		json_object_object_add(jdata, "visible satellites", value);

		value = json_object_new_int(data.satellites_used);
		json_object_object_add(jdata, "used satellites", value);
	}

	if(data.set & MODE_SET) {
		value = json_object_new_int(data.fix.mode);
		json_object_object_add(jdata, "mode", value);
	}

	if (data.set & LATLON_SET) {
		value = json_object_new_double(data.fix.latitude);
		json_object_object_add(jdata, "latitude", value);

		value = json_object_new_double(data.fix.epy);
		json_object_object_add(jdata, "latitude error", value);

		value = json_object_new_double(data.fix.longitude);
		json_object_object_add(jdata, "longitude", value);

		value = json_object_new_double(data.fix.epx);
		json_object_object_add(jdata, "longitude error", value);
	}

	if (data.set & SPEED_SET) {
		value = json_object_new_double(data.fix.speed);
		json_object_object_add(jdata, "speed", value);
	}

	if (data.set & SPEEDERR_SET){
		value = json_object_new_double(data.fix.eps);
		json_object_object_add(jdata, "speed error", value);
	}

	if(data.fix.mode == MODE_3D){
		if (data.set & ALTITUDE_SET) {
			value = json_object_new_double(data.fix.altitude);
			json_object_object_add(jdata, "altitude", value);
		}

		if(data.set & VERR_SET) {
			value = json_object_new_double(data.fix.epv);
			json_object_object_add(jdata, "altitude error", value);
		}

		if (data.set & CLIMB_SET){
			value = json_object_new_double(data.fix.climb);
			json_object_object_add(jdata, "climb", value);
		}

		if (data.set & CLIMBERR_SET){
			value = json_object_new_double(data.fix.epc);
			json_object_object_add(jdata, "climb error", value);
		}
	}

	if (data.set & TRACK_SET) {
		value = json_object_new_double(data.fix.track);
		json_object_object_add(jdata, "heading (true north)", value);
	}

	if (data.set & TRACKERR_SET){
		value = json_object_new_double(data.fix.epd);
		json_object_object_add(jdata, "heading error", value);
	}

	if(data.set & TIME_SET) {
		//support the change from timestamp_t (double) to timespec struct (done with API 9.0)
		#if GPSD_API_MAJOR_VERSION>8
			double timestamp_converted = (double)data.fix.time.tv_sec + ((double)data.fix.time.tv_nsec / 1000000000);
			value = json_object_new_double(timestamp_converted);
		#else
			value = json_object_new_double(data.fix.time);
		#endif

		json_object_object_add(jdata, "timestamp", value);
	}

	if(data.set & TIMERR_SET){
		value = json_object_new_double(data.fix.ept);
		json_object_object_add(jdata, "timestamp error", value);
	}

	return jdata;
}

static void get_gps_data(afb_req_t request)
{
	json_object *jdata = NULL;
	pthread_mutex_lock(&mutex);

	jdata = json_data_completion(json_object_new_object());
	if (jdata != NULL) {
		afb_req_success(request, jdata, "GNSS location data");
	} else {
		afb_req_fail(request, "failed", "No GNSS fix");
	}

	pthread_mutex_unlock(&mutex);
}

static void subscribe(afb_req_t request)
{
	json_object *json_request = afb_req_json(request);
	event_list_node *event_to_subscribe;

	if(!EventJsonToName(json_request, NULL)){
		if(!EventListFind(json_request, &list, &event_to_subscribe, 1)){
			//event does not exist
			if(!EventListAdd(json_request, false, &event_to_subscribe)){
				//event has been added to the list
				UpdateMaxFreq();
				if(afb_req_subscribe(request, event_to_subscribe->event) == 0){
					//well subscribed to the new event
					afb_req_success(request, NULL, NULL);
				}
				else {
					//subscribe failed
					afb_req_fail(request, "failed", "Subscription error");
				}
			}
			else {
				//event failed to be added to the list
				afb_req_fail(request, "failed", "Subscription error");
			}
		}
		else{
			//event exist
			if(afb_req_subscribe(request, event_to_subscribe->event) == 0){
				//well subscribed to the new event
				afb_req_success(request, NULL, NULL);
			}
			else{
				//subscribe failed
				afb_req_fail(request, "failed", "Subscription error");
			}
		}
		return;
	}
	else{
		afb_req_fail(request, "failed", "Request isn't well formated");
		return;
	}
}

static void unsubscribe(afb_req_t request)
{
	json_object *json_request = afb_req_json(request);
	event_list_node *event_to_unsubscribe;

	if(!EventJsonToName(json_request, NULL)){
		if(EventListFind(json_request, &list, &event_to_unsubscribe, 1)){
			//event exist
			if(afb_req_unsubscribe(request, event_to_unsubscribe->event) == 0){
				//well unsubscribe, we dont delete the event in case anyone else needs it
				afb_req_success(request, NULL, NULL);
			}
			else {
				afb_req_fail(request, "failed", "Unsubscription error");
			}
		}
		else {
			//event does not exist
			afb_req_fail(request, "failed", "Event doesn't exist");
		}
		return;
	}
	else{
		afb_req_fail(request, "failed", "Request isn't well formated");
		return;
	}
}

//keep reading till an error condition happens
static void* gpsd_polling(void *ptr)
{
	int tries = 0;

	while (tries < GPSD_POLLING_MAX_RETRIES) {
		if (!gps_waiting(&data, MSECS_TO_USECS(GPSD_POLLING_DELAY_MS))) {
			tries++;
			continue;
		}
		pthread_mutex_lock(&mutex);
		if (gps_read(&data) == -1) {
			AFB_ERROR("Cannot read from GPS daemon (errno: %d, \"%s\").\n", errno, gps_errstr(errno));
			pthread_mutex_unlock(&mutex);
			break;
		}
		pthread_mutex_unlock(&mutex);
	}

	AFB_INFO("GPSD connection lost, closing.\n");
	gpsd_online = false;
	gps_stream(&data, WATCH_DISABLE, NULL);
	gps_close(&data);

	return NULL;
}

static void* event_management_thread(void *arg) {
	AFB_INFO("Event management thread online !");

	while(gpsd_online){
		//start from the head of the list
		pthread_mutex_lock(&list_mutex);
		event_list_node *list_cpy = list;
		pthread_mutex_unlock(&list_mutex);

		json_object *jdata = json_data_completion(json_object_new_object());
		event_list_node *tmp = cds_list_entry(list_cpy->list_head.next, event_list_node, list_head);
		event_list_node *next = tmp;

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);

		bool found_freq_event = false;

		//parcours de la liste
		while(tmp != list_cpy){
			//AFB_INFO("event name : %s", afb_event_name(tmp->event));
			next = cds_list_entry(tmp->list_head.next, event_list_node, list_head);

			if(tmp->condition_type == FREQUENCY){
				found_freq_event = true;
				long accum_us = ( now.tv_sec - tmp->last_value.freq_last_send.tv_sec) * 1000000 + ( now.tv_nsec - tmp->last_value.freq_last_send.tv_nsec) / 1000;
				//AFB_INFO("event %s, target = %d, accum = %ld", afb_event_name(tmp->event), HZ_TO_USECS(tmp->condition_value.freq), accum_us);

				if(accum_us > HZ_TO_USECS(tmp->condition_value.freq)){
					if(afb_event_push(tmp->event, json_object_get(jdata)) == 0){
						//protect from an instant deletion of new event
						tmp->not_used_count++;
						//AFB_INFO("event : %s, count : %d", afb_event_name(tmp->event), tmp->not_used_count);
						if(tmp->not_used_count >= 5){
							if (EventListDeleteByNode(&tmp)) {
								AFB_INFO("Event %s deleted", afb_event_name(tmp->event));
								UpdateMaxFreq();
							}
						}
					}
					else {
						if(tmp->not_used_count) tmp->not_used_count = 0;
					}
					tmp->last_value.freq_last_send = now;
				}

			}
			else if(tmp->condition_type == MOVEMENT){
				struct json_object *json_latitude, *json_longitude;
				json_object_object_get_ex(jdata, "latitude", &json_latitude);
				json_object_object_get_ex(jdata, "longitude", &json_longitude);
				double latitude = json_object_get_double(json_latitude);
				double longitude = json_object_get_double(json_longitude);

				if(distance_m( tmp->last_value.movement_last_lat_lon.latitude, tmp->last_value.movement_last_lat_lon.longitude, latitude, longitude) > tmp->condition_value.movement_range) {
					if(afb_event_push(tmp->event, json_object_get(jdata)) == 0){
						//protect from an instant deletion of new event
						tmp->not_used_count++;
						//AFB_INFO("event : %s, count : %d", afb_event_name(tmp->event), tmp->not_used_count);
						if(tmp->not_used_count >= 5){
							if (EventListDeleteByNode(&tmp)) {
								AFB_INFO("Event %s deleted", afb_event_name(tmp->event));
							}
						}
					}
					else{
						if(tmp->not_used_count) tmp->not_used_count = 0;
						tmp->last_value.movement_last_lat_lon.latitude = latitude;
						tmp->last_value.movement_last_lat_lon.longitude = longitude;
					}
				}

			}
			else if(tmp->condition_type == MAX_SPEED){
				struct json_object *json_speed;
				json_object_object_get_ex(jdata, "speed", &json_speed);
				double speed = json_object_get_double(json_speed);

				if((speed*3.6) > (double)(tmp->condition_value.max_speed)){
					if(!tmp->last_value.above_speed){
						if(afb_event_push(tmp->event, json_object_get(jdata)) == 0){
							//protect from an instant deletion of new event
							tmp->not_used_count++;
							//AFB_INFO("event : %s, count : %d", afb_event_name(tmp->event), tmp->not_used_count);
							if(tmp->not_used_count >= 5){
								if (EventListDeleteByNode(&tmp)) {
									AFB_INFO("Event %s deleted", afb_event_name(tmp->event));
								}
							}
						}
						else {
							if(tmp->not_used_count) tmp->not_used_count = 0;
							tmp->last_value.above_speed = true;
						}

					}
					json_object_put(json_speed);
				}
				else{
					tmp->last_value.above_speed = false;
				}
			}

			tmp = next;

		}

		//if no frequency related event, poll the list at 1Hz
		if(found_freq_event && (max_freq > 0)) usleep(HZ_TO_USECS(max_freq));
		else sleep(1);
	}
	AFB_INFO("Event management thread offline !");
	pthread_exit(NULL);
}

static void* gpsd_connection_management_thread(void *arg) {
	gpsd_connection_management_thread_userdata_t *userdata = arg;

	while(true) { // exit condition, if any, occurs in connection loop
		int ret = -1;
		unsigned int delay = 1;
		// try connect...
		// retry forever when max_retries < 1
		while(ret != 0
			  && (userdata->max_retries <= 0
			      || userdata->nb_retries++ < userdata->max_retries))
		{
			ret = gps_open(userdata->host, userdata->port, userdata->gps_data);
			if (ret != 0) {
				AFB_NOTICE("gpsd not available yet (errno: %d, \"%s\"). Wait for %.2d seconds before retry...", errno, gps_errstr(errno), delay);
				sleep(delay);
				delay *= 2;
				if (delay > GPSD_CONNECT_MAX_DELAY) delay=GPSD_CONNECT_MAX_DELAY;
			}
		}
		if (ret != 0) {
			AFB_ERROR("Too many retries, aborting...");
			userdata->result = ret;
			return &userdata->result; //aka thread_exit()
		}

		gps_stream(userdata->gps_data, WATCH_ENABLE | WATCH_JSON, NULL);
		#ifdef AGL_SPEC_802
			int tries = 5;
			// due to the gpsd.socket race condition need to loop till initial event
			do {
				gps_read(userdata->gps_data);
			} while (!gps_waiting(userdata->gps_data, MSECS_TO_USECS(2500)) && tries--);
		#endif
		AFB_INFO("Connected to GPSd");
		// connected
		gpsd_online = TRUE;
		userdata->nb_retries = 0; // reset counter
		ret = pthread_create(&thread_event, NULL, &event_management_thread, NULL);
		if (ret != 0) {
			AFB_ERROR("Could not create thread for event handling...");
		}

		pthread_detach(thread_event);

		gpsd_polling(NULL);
		// gpsd_polling will return when gpsd connection's lost...
	}
}

//return 0 if pthread_create went fine, any other value for pthread_create error
static int gps_init()
{
	int ret;

	gpsd_connection_management_thread_userdata_t *userdata = malloc(sizeof(gpsd_connection_management_thread_userdata_t));
	if (userdata == NULL) {
		AFB_ERROR("Memory allocation error for GPSd thread");
		return -1;
	}

	userdata->host = getenv("AFBGPS_HOST") ? : "localhost";
	userdata->port = getenv("AFBGPS_SERVICE") ? : "2947";
	userdata->max_retries = -1;
	userdata->nb_retries = 0;
	userdata->gps_data = &data;

	ret = pthread_create(&thread, NULL, &gpsd_connection_management_thread, userdata);
	if (ret != 0) {
		AFB_ERROR("Could not create thread for listening to gpsd socket...");
		return ret;
	}

	pthread_detach(thread);
	return 0;
}

static int init(afb_api_t api)
{
	gpsd_online = false;
	max_freq = 0;
	list = malloc(sizeof(event_list_node));
	CDS_INIT_LIST_HEAD(&list->list_head);

	AFB_NOTICE("Initiating GPSD mode !");
	if(gps_init()){
		AFB_ERROR("Encountered a problem when initiating GPSD mode");
		return -1;
	}

	return 0;
}

static const struct afb_verb_v3 binding_verbs[] = {
	{ .verb = "gps_data",    .callback = get_gps_data,     .info = "Get GNSS data" },
	//get2Dlocation, etc ??
	{ .verb = "subscribe",   .callback = subscribe,    .info = "Subscribe to GNSS events" },
	{ .verb = "unsubscribe", .callback = unsubscribe,  .info = "Unsubscribe to GNSS events" },
	{ }
};

/*
 * binder API description
 */
const struct afb_binding_v3 afbBindingV3 = {
	.api = "gps",
	.specification = "GNSS/GPS API",
	.verbs = binding_verbs,
	.init = init,
};
