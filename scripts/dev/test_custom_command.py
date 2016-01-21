#!/usr/bin/env python

import json

# pretend we are doing some stuff here

# then report it out
print(json.dumps({"line": "116", "filename": "src/EnergyPlus/Boilers.cc", "tool": "tab_checker", "message": "This is a fake warning about boilers line 116", "messagetype": "warning"}))
