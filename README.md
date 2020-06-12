
## Subscribtion

- avalaible data :
    - gps_data

- available condition & values :
    - frequency (hz)
        * 10, 20, 50, 100
    - movement (m)
        * 1, 10, 100, 300, 500, 1000
    - max_speed (km/h)
        * 20, 30, 50, 90, 110, 130

gps subscribe {"data" : "gps_data", "condition" : "frequency", "value" : 10}
gps subscribe {"data" : "gps_data", "condition" : "movement", "value" : 1}
gps subscribe {"data" : "gps_data", "condition" : "max_speed", "value" : 20}
