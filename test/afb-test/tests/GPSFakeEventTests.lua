--[[
    Copyright (C) 2019-2020 IoT.bzh Company
    Author: Aymeric Aillet <aymeric.aillet@iot.bzh>

    $RP_BEGIN_LICENSE$
    Commercial License Usage
     Licensees holding valid commercial IoT.bzh licenses may use this file in
     accordance with the commercial license agreement provided with the
     Software or, alternatively, in accordance with the terms contained in
     a written agreement between you and The IoT.bzh Company. For licensing terms
     and conditions see https://www.iot.bzh/terms-conditions. For further
     information use the contact form at https://www.iot.bzh/contact.

    GNU General Public License Usage
     Alternatively, this file may be used under the terms of the GNU General
     Public license version 3. This license is as published by the Free Software
     Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
     of this file. Please review the following information to ensure the GNU
     General Public License requirements will be met
     https://www.gnu.org/licenses/gpl-3.0.html.
    $RP_END_LICENSE$


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
