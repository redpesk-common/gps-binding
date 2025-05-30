// clang-format off
const char *info_verbS =
"{"
    "\"metadata\": {"
        "\"uid\": \"gps\","
        "\"info\": \"GPS Binding\","
        "\"version\": \"1.0\""
    "},"
    "\"groups\": ["
        "{"
            "\"uid\": \"gps\","
            "\"info\": \"GPS API\","
            "\"verbs\": ["
                "{"
                    "\"uid\": \"gps_data\","
                    "\"info\": \"get latest gps data\","
                    "\"verb\": \"gps_data\""
                "},"
                "{"
                    "\"uid\": \"subscribe\","
                    "\"info\": \"Subscribe to gps data with condition\","
                    "\"verb\": \"subscribe\","
                    "\"usage\": {"
                        "\"data\": \"gps_data\", \"condition\" : \"condition_type\", \"value\" : \"condition_value (see readme for available values)\""
                    "},"
                    "\"sample\": ["
                        "{"
                            "\"data\" : \"gps_data\", \"condition\" : \"frequency\", \"value\" : 10"
                        "},"
                        "{"
                            "\"data\" : \"gps_data\", \"condition\" : \"movement\", \"value\" : 1"
                        "},"
                        "{"
                            "\"data\" : \"gps_data\", \"condition\" : \"max_speed\", \"value\" : 20"
                        "}"
                    "]"
                "},"
                "{"
                  "\"uid\": \"unsubscribe\","
                  "\"info\": \"Unsubscribe to gps data with condition\","
                  "\"verb\": \"unsubscribe\","
                  "\"usage\": {"
                      "\"data\": \"gps_data\", \"condition\" : \"condition_type\", \"value\" : \"condition_value (see readme for available values)\""
                  "},"
                  "\"sample\": ["
                      "{"
                          "\"data\" : \"gps_data\", \"condition\" : \"frequency\", \"value\" : 10"
                      "},"
                      "{"
                          "\"data\" : \"gps_data\", \"condition\" : \"movement\", \"value\" : 1"
                      "},"
                      "{"
                          "\"data\" : \"gps_data\", \"condition\" : \"max_speed\", \"value\" : 20"
                      "}"
                  "]"
              "},"
              "{"
                  "\"uid\": \"info\","
                  "\"info\": \"get GPS binding info\","
                  "\"verb\": \"info\""
              "}"
            "]"
        "}"
    "]"
"}"
;
// clang-format on
