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

local testPrefix ="rp_gps_BasicTests_"
local api="gps"

_AFT.setBeforeEach(function() print("~~~~~ Begin Test ~~~~~") end)
_AFT.setAfterEach(function() print("~~~~~ End Test ~~~~~") end)

_AFT.setBeforeAll(function() print("~~~~~~~~~~ BEGIN BASIC TESTS ~~~~~~~~~~") return 0 end)
_AFT.setAfterAll(function() print("~~~~~~~~~~ END BASIC TESTS ~~~~~~~~~~") return 0 end)

-- This tests the 'subscribe_frequency' verb of the gps API
_AFT.testVerbStatusSuccess(testPrefix.."subscribe_frequency",api,"subscribe", {data = "gps_data", condition = "frequency", value = 10},
nil,
function()
    _AFT.callVerb(api,"unsubscribe", {data = "gps_data", condition = "frequency", value = 10})
end)

-- This tests the 'subscribe_movement' verb of the gps API
_AFT.testVerbStatusSuccess(testPrefix.."subscribe_movement",api,"subscribe", {data = "gps_data", condition = "movement", value = 1},
nil,
function()
    _AFT.callVerb(api,"unsubscribe", {data = "gps_data", condition = "movement", value = 1})
end)

-- This tests the 'subscribe_max_speed' verb of the gps API
_AFT.testVerbStatusSuccess(testPrefix.."subscribe_max_speed",api,"subscribe", {data = "gps_data", condition = "max_speed", value = 20},
nil,
function()
    _AFT.callVerb(api,"unsubscribe", {data = "gps_data", condition = "max_speed", value = 20})
end)

-- This tests the 'unsubscribe' verb of the gps API
_AFT.testVerbStatusSuccess(testPrefix.."unsubscribe",api,"unsubscribe",{data = "gps_data", condition = "frequency", value = 10},
function()
    _AFT.callVerb(api,"subscribe",{data = "gps_data", condition = "frequency", value = 10})
end,
nil)

-- This tests 'wrong_verb'
_AFT.testVerbStatusError(testPrefix.."wrong_verb",api,"error",{}, nil, nil)

-- This tests 'double_subscribe'
_AFT.describe(testPrefix.."double_subscribe",
function()
    _AFT.assertVerbStatusSuccess(api, "subscribe",{data = "gps_data", condition = "frequency", value = 10})
    _AFT.assertVerbStatusSuccess(api, "subscribe",{data = "gps_data", condition = "frequency", value = 10})
end)

-- This tests 'subscribe wrong frequency value'
_AFT.testVerbStatusError(testPrefix.."subscribe_wrong_frequency_value",api,"subscribe",{data = "gps_data", condition = "frequency", value = 25}, nil, nil)

-- This tests 'subscribe wrong movement value'
_AFT.testVerbStatusError(testPrefix.."subscribe_wrong_movement_value",api,"subscribe",{data = "gps_data", condition = "movement", value = 150}, nil, nil)

-- This tests 'subscribe wrong frequency value'
_AFT.testVerbStatusError(testPrefix.."subscribe_wrong_max_speed_value",api,"subscribe",{data = "gps_data", condition = "max_speed", value = 250}, nil, nil)

-- This tests 'subscribe wrong data argument'
_AFT.testVerbStatusError(testPrefix.."subscribe_wrong_data_argument",api,"subscribe",{data = "error", condition = "frequency", value = 10}, nil, nil)

-- This tests 'subscribe wrong condition argument'
_AFT.testVerbStatusError(testPrefix.."subscribe_wrong_condition_argument",api,"subscribe",{data = "gps_data", condition = "error", value = 10}, nil, nil)

-- This tests 'subscribe wrong argument'
_AFT.testVerbStatusError(testPrefix.."subscribe_wrong_argument",api,"subscribe",{data = "gps_data", condition = "frequency", value = "error"}, nil, nil)

-- This tests 'subscribe without argument'
_AFT.testVerbStatusError(testPrefix.."subscribe_without_argument",api,"subscribe",{}, nil, nil)

-- This tests 'unsubscribe wrong data argument'
_AFT.testVerbStatusError(testPrefix.."unsubscribe_wrong_data_argument",api,"unsubscribe",{data = "error", condition = "frequency", value = 10}, nil, nil)

-- This tests 'unsubscribe wrong condition argument'
_AFT.testVerbStatusError(testPrefix.."unsubscribe_wrong_condition_argument",api,"unsubscribe",{data = "gps_data", condition = "error", value = 10}, nil, nil)

-- This tests 'unsubscribe wrong value argument'
_AFT.testVerbStatusError(testPrefix.."unsubscribe_wrong_value_argument",api,"unsubscribe",{data = "gps_data", condition = "frequency", value = "error"}, nil, nil)

-- This tests 'unsubscribe without argument'
_AFT.testVerbStatusError(testPrefix.."unsubscribe_without_argument",api,"unsubscribe",{}, nil, nil)

-- This tests 'let the event being destroyed' used for coverage
_AFT.testVerbStatusSuccess(testPrefix.."let_event_being_detroyed", api, "subscribe", {data = "gps_data", condition = "frequency", value = 100}, nil, nil)

_AFT.exitAtEnd()
