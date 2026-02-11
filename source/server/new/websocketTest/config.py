#
#	config.py
#
#	(c) 2023 by Andreas Kraft
#	License: BSD 3-Clause License. See the LICENSE file for further details.
#
#	Configuration for the websocket tests
#

# CSE configuration

cseUrl = 'ws://localhost:8081'
notificationHost = 'localhost'
notificationPort = 8190
subProtocol = 'oneM2M.json'
adminOriginator = 'CAdmin'
aeName = 'TinyFarm'
subName = 'mySub'
timeout = 1
wsServerPOA = f'ws://{notificationHost}:{notificationPort}'
unreachablePOA = 'ws://default'