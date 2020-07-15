--[[
    Copyright (C) 2019 "IoT.bzh"
    Author  Aymeric Aillet <aymeric.aillet@iot.bzh>

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.


    NOTE: strict mode: every global variables should be prefixed by '_'
--]]

local testPrefix ="rp_gps_FakeEventTests_"
local api="gps"

_AFT.setBeforeEach(function() print("~~~~~ Begin Test ~~~~~") end)
_AFT.setAfterEach(function() print("~~~~~ End Test ~~~~~") end)

_AFT.setBeforeAll(function() print("~~~~~~~~~~ BEGIN FAKE EVENTS TESTS ~~~~~~~~~~") return 0 end)
_AFT.setAfterAll(function() print("~~~~~~~~~~ END FAKE EVENTS TESTS ~~~~~~~~~~") return 0 end)


-- This tests 'frequency_event'
_AFT.describe(testPrefix.."frequency_event",
function()
    _AFT.addEventToMonitor("gps/gps_data_freq_20")
    _AFT.assertVerbStatusSuccess(api, "subscribe", {data = "gps_data", condition = "frequency", value = 20})
    os.execute("sleep 1")
    _AFT.assertEvtReceived("gps/gps_data_freq_20", 300000)
    _AFT.assertVerbStatusSuccess(api, "unsubscribe", {data = "gps_data", condition = "frequency", value = 20})
end)

-- This tests 'movement_event'
_AFT.describe(testPrefix.."movement_event",
function()
    _AFT.addEventToMonitor("gps/gps_data_movement_1")
    _AFT.assertVerbStatusSuccess(api, "subscribe", {data = "gps_data", condition = "movement", value = 1})
    os.execute("sleep 1")
    _AFT.assertEvtReceived("gps/gps_data_movement_1", 200000)
    _AFT.assertVerbStatusSuccess(api, "unsubscribe", {data = "gps_data", condition = "movement", value = 1})
end)

-- This tests 'max_speed_event'
_AFT.describe(testPrefix.."max_speed_event",
function()
    _AFT.addEventToMonitor("gps/gps_data_speed_20")
    _AFT.assertVerbStatusSuccess(api, "subscribe", {data = "gps_data", condition = "max_speed", value = 20})
    os.execute("sleep 1")
    _AFT.assertEvtReceived("gps/gps_data_speed_20", 4000000)
    _AFT.assertVerbStatusSuccess(api, "unsubscribe", {data = "gps_data", condition = "max_speed", value = 20})
end)

-- This tests the 'gps_data' verb of the gps API
_AFT.testVerbStatusSuccess(testPrefix.."gps_data",api,"gps_data", {}, nil, nil)

_AFT.exitAtEnd()
