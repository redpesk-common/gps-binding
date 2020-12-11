# GPS binding API

The API exposed by the binding is : ```gps```

The binding has been made for clients to subscribe to it, so it has only three verbs :

| Verb          | Description                                       |
|---------------|---------------------------------------------------|
| gps_data      | Get freshest data that came from GPSD             |
| subscribe     | Subscribe to gps data with specific conditions    |
| unsubscribe   | Unsubscribe to gps data with specific conditions  |

## gps_data

```bash
gps gps_data
```

## subscribe/unsubscribe

- Avalaible __data__ :
    - gps_data

- Available __condition__ & __value__ :
    - frequency (hz)
        * 1, 10, 20, 50, 100
    - movement (m)
        * 1, 10, 100, 300, 500, 1000
    - max_speed (km/h)
        * 20, 30, 50, 90, 110, 130

- examples :

Get gps_data a 10Hz
```bash
gps subscribe {"data" : "gps_data", "condition" : "frequency", "value" : 10}
```

Get gps_data if there is a movement of at least 1 meter since last event
```bash
gps subscribe {"data" : "gps_data", "condition" : "movement", "value" : 1}
```

Get gps_data each time the speed exceeds 20 km/h
```bash
gps subscribe {"data" : "gps_data", "condition" : "max_speed", "value" : 20}
```

## JSON Answer format

Wether it's coming from a subscription or the direct call "gps_data" verb the structure of the answer is the same, values are rawly coming from the libgps, you can find a lot of information about them directly in this library.

Here is the answered JSON content :

| Key                   | Type      | Description                                           |
|-----------------------|-----------|-------------------------------------------------------|
| visible satellites    | Int       | Number of visible satellites                          |
| used satellites       | Int       | Number of satellites in used                          |
| mode                  | Int       | Mode of fix (0 to 3)                                  |
| latitude              | Double    | Latitude in degrees                                   |
| longitude             | Double    | Longitude in degrees                                  |
| speed                 | Double    | Speed over ground, meters/sec                         |
| altitude              | Double    | Altitude in meters                                    |
| climb                 | Double    | Vertical speed, meters/sec                            |
| heading (true north)  | Double    | Course made good (relative to true north)             |
| timestamp             | Double    | Standard timestamp                                    |

Each value from "Latitude" is also accompanied by its error value expressed in the same type and unit as this one. (ex : latitude error).

### JSON example

```bash
ON-REPLY 1:gps/gps_data: OK
{
  "response":{
    "visible satellites":0,
    "used satellites":0,
    "mode":3,
    "latitude":48.613849999999999,
    "latitude error":NaN,
    "longitude":2.1203666669999999,
    "longitude error":NaN,
    "speed":0.0,
    "speed error":NaN,
    "altitude":0.0,
    "altitude error":23.0,
    "climb":0.0,
    "climb error":46.0,
    "heading (true north)":254.5,
    "heading error":NaN,
    "timestamp":1593435057.194,
    "timestamp error":0.0050000000000000001
  },
  "jtype":"afb-reply",
  "request":{
    "status":"success",
    "info":"GNSS location data"
  }
}
```
