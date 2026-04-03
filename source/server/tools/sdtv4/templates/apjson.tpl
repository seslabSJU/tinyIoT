{# 

apjson.tpl

(c) 2015 by Andreas Kraft
License: Apache 2. See the LICENSE file for further details.
#}

{%import "apjson.macros" as ac with context -%}
// Created by SDTTool
// oneM2M Release: {{ modelversion }}
// Domain:         {{ getNamespaceFromPrefix(namespaceprefix) }}
[
{{ ac.printModuleClasses(domain.moduleClasses) -}}
{{ ac.printSubDevices(domain.subDevices) -}}{{- "," if domain.subDevices | length > 0 and domain.deviceClasses | length > 0 }}
{{ ac.printDeviceClasses(domain.deviceClasses) }}
]