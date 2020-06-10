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

#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>

#if GPSD_API_MAJOR_VERSION>6
	#define gps_read(arg) gps_read(arg, NULL, 0)
#endif

// enable workaround
#define AGL_SPEC_802 on

// 60 second max between 2 gpsd connection attemps
#define GPSD_CONNECT_MAX_DELAY 60

#define GPSD_POLLING_MAX_RETRIES 60
#define GPSD_POLLING_DELAY_MS 2000

static struct gps_data_t data;
static afb_event_t gps_data_event;
static bool gps_data_event_subscribed;

static pthread_t thread;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#define MSECS_TO_USECS(x) (x * 1000)

typedef struct gpsd_thread_userdate_s {
	char *host;
	char *port;
	int max_retries; // 0 or negative values => no maximum
	int nb_retries;   // current count of retries
	struct gps_data_t *gps_data; // as for now, point on global `data`
	int result;
} gpsd_thread_userdata_t;

static json_object *json_data_completion(json_object *jresp)
{
	json_object *value = NULL;

	if (data.fix.mode < 2) {
		json_object_put(jresp);
		return NULL;
	}

	if(data.set & SATELLITE_SET){
		value = json_object_new_int(data.satellites_visible);
		json_object_object_add(jresp, "visible satellites", value);

		value = json_object_new_int(data.satellites_used);
		json_object_object_add(jresp, "used satellites", value);
	}

	if(data.set & MODE_SET) {
		value = json_object_new_int(data.fix.mode);
		json_object_object_add(jresp, "mode", value);
	}

	if (data.set & LATLON_SET) {
		value = json_object_new_double(data.fix.latitude);
		json_object_object_add(jresp, "latitude", value);

		value = json_object_new_double(data.fix.epy);
		json_object_object_add(jresp, "latitude error", value);

		value = json_object_new_double(data.fix.longitude);
		json_object_object_add(jresp, "longitude", value);

		value = json_object_new_double(data.fix.epx);
		json_object_object_add(jresp, "longitude error", value);
	}

	if (data.set & SPEED_SET) {
		value = json_object_new_double(data.fix.speed);
		json_object_object_add(jresp, "speed", value);
	}

	if (data.set & SPEEDERR_SET){
		value = json_object_new_double(data.fix.eps);
		json_object_object_add(jresp, "speed error", value);
	}

	if(data.fix.mode == MODE_3D){
		if (data.set & ALTITUDE_SET) {
			value = json_object_new_double(data.fix.altitude);
			json_object_object_add(jresp, "altitude", value);
		}

		if(data.set & VERR_SET) {
			value = json_object_new_double(data.fix.epv);
			json_object_object_add(jresp, "altitude error", value);
		}

		if (data.set & CLIMB_SET){
			value = json_object_new_double(data.fix.climb);
			json_object_object_add(jresp, "climb", value);
		}

		if (data.set & CLIMBERR_SET){
			value = json_object_new_double(data.fix.epc);
			json_object_object_add(jresp, "climb error", value);
		}
	}

	if (data.set & TRACK_SET) {
		value = json_object_new_double(data.fix.track);
		json_object_object_add(jresp, "heading (true north)", value);
	}

	if (data.set & TRACKERR_SET){
		value = json_object_new_double(data.fix.epd);
		json_object_object_add(jresp, "heading error", value);
	}

	/*
	//suport suppression of the unix_to_iso converter by API 9.0
	#if GPSD_API_MAJOR_VERSION<9
		if (data.set & TIME_SET) {
			char time[30];
			unix_to_iso8601(data.fix.time, (char *) &time, sizeof(time));
			value = json_object_new_string(time);
			json_object_object_add(jresp, "timestamp", value);
		}
	#endif
	*/

	if(data.set & TIME_SET) {
		//support the change from timestamp_t (double) to timespec struct (done with API 9.0)
		#if GPSD_API_MAJOR_VERSION>8
			double timestamp_converted = (double)data.fix.time.tv_sec + ((double)data.fix.time.tv_nsec / 1000000000);
			value = json_object_new_double(timestamp_converted);
		#else
			value = json_object_new_double(data.fix.time);
		#endif

		json_object_object_add(jresp, "timestamp", value);
	}

	if(data.set & TIMERR_SET){
		value = json_object_new_double(data.fix.ept);
		json_object_object_add(jresp, "timestamp error", value);
	}

	return jresp;
}

static void get_gps_data(afb_req_t request)
{
	json_object *jresp = NULL;
	pthread_mutex_lock(&mutex);

	jresp = json_data_completion(json_object_new_object());
	if (jresp != NULL) {
		afb_req_success(request, jresp, "GNSS location data");
	} else {
		afb_req_fail(request, "failed", "No GNSS fix");
	}

	pthread_mutex_unlock(&mutex);
}

static void subscribe(afb_req_t request)
{
	const char *value = afb_req_value(request, "value");

	if (value && !strcasecmp(value, "gps_data")) {
		if(afb_req_subscribe(request, gps_data_event) == 0) {
			afb_req_success(request, NULL, NULL);
			gps_data_event_subscribed = true;
		}
		else {
			afb_req_fail(request, "failed", "Subscription error");
		}

		return;
	}

	afb_req_fail(request, "failed", "Invalid event");
}

static void unsubscribe(afb_req_t request)
{
	const char *value = afb_req_value(request, "value");

	if (value && !strcasecmp(value, "gps_data")) {
		if(afb_req_unsubscribe(request, gps_data_event) == 0){
			afb_req_success(request, NULL, NULL);
		}
		else {
			afb_req_fail(request, "failed", "Unsubscription error");
		}

		return;
	}

	afb_req_fail(request, "failed", "Invalid event");
}

//keep reading till an error condition happens
static void *data_poll(void *ptr)
{
	int tries = 0;

	while (tries < GPSD_POLLING_MAX_RETRIES) {
		json_object *jresp = NULL;
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

		if (gps_data_event_subscribed) {
			jresp = json_data_completion(json_object_new_object());

			if (jresp != NULL) {
				if(gps_data_event_subscribed) {
					if(afb_event_push(gps_data_event, jresp) == 0) {
						gps_data_event_subscribed = false;
					}
				}
			}
		}

		pthread_mutex_unlock(&mutex);
	}

	AFB_INFO("GPSD connection lost, closing.\n");
	gps_stream(&data, WATCH_DISABLE, NULL);
	gps_close(&data);

	return NULL;
}

static void* gpsd_thread(void *arg) {
	gpsd_thread_userdata_t *userdata = arg;

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
				AFB_NOTICE("gpsd not available yet (errno: %d, \"%s\"). Wait for %d seconds before retry...", errno, gps_errstr(errno), delay);
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
		userdata->nb_retries = 0; // reset counter
		data_poll(NULL);
		// data_poll will return when gpsd connection's lost...
	}
}

//return 0 if pthread_create went fine, any other value for pthread_create error
static int gps_init()
{
	int ret;

	gpsd_thread_userdata_t *userdata = malloc(sizeof(gpsd_thread_userdata_t));
	if (userdata == NULL) {
		AFB_ERROR("Memory allocation error for GPSd thread");
		return -1;
	}

	userdata->host = getenv("AFBGPS_HOST") ? : "localhost";
	userdata->port = getenv("AFBGPS_SERVICE") ? : "2947";
	userdata->max_retries = -1;
	userdata->nb_retries = 0;
	userdata->gps_data = &data;

	ret = pthread_create(&thread, NULL, &gpsd_thread, userdata);
	if (ret != 0) {
		AFB_ERROR("Could not create thread for listening to gpsd socket...");
		return ret;
	}

	pthread_detach(thread);
	return 0;
}

static int init(afb_api_t api)
{
	gps_data_event = afb_daemon_make_event("gps_data");
	gps_data_event_subscribed = false;

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
