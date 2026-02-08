{# 

markdown.tpl

(c) 2015 by Andreas Kraft
License: Apache 2. See the LICENSE file for further details.
#}

{%import "markdown.macros" as md with context -%}
# Domain "{{ domain.id }}"
{% if not hideDetails %}
{{ md.doc(domain.doc) }}
{% endif %}
{{ md.printLicense(license) }}
{{ md.printIncludes(domain.includes) }}
{{ md.printDataTypes(domain.dataTypes) }}
{{ md.printModuleClasses(domain.moduleClasses, 'false') }}
{{ md.printSubDevices(domain.subDevices) }}
{{ md.printDeviceClasses(domain.deviceClasses) }}
{{ md.printProductClasses(domain.productClasses) }}
