#include <stdbool.h>
#include <time.h>
#include <urcu/list.h>

#define AFB_BINDING_VERSION 4
#include <afb-helpers4/afb-data-utils.h>
#include <afb-helpers4/afb-req-utils.h>
#include <afb/afb-binding.h>

enum condition_type_enum { FREQUENCY, MOVEMENT, MAX_SPEED };

typedef struct event_list_node
{
    struct cds_list_head list_head;
    afb_event_t event;  // event
    bool is_protected;  // is the event protected from deletion ?
    int not_used_count;
    enum condition_type_enum condition_type;  // condition type of the event
    union                                     // condition value of the event
    {
        int freq;            // in hz
        int movement_range;  // in m
        int max_speed;       // in km/h
    } condition_value;
    union {
        struct timespec freq_last_send;
        struct
        {
            double latitude;
            double longitude;
        } movement_last_lat_lon;
        bool above_speed;
    } last_value;

} event_list_node;

extern void UpdateMaxFreq();
extern int EventJsonToName(json_object *jcondition, char **result);
extern int EventListAdd(json_object *jcondition,
                        bool is_disposable,
                        event_list_node **node,
                        afb_req_t request);
extern bool EventListFind(json_object *jcondition, event_list_node **found_node);
extern bool EventListDeleteByNode(event_list_node **node);
