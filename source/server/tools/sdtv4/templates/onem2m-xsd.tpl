{# 

onem2m-xsd.tpl

(c) 2015 by Andreas Kraft
License: Apache 2. See the LICENSE file for further details.
#}

{% import "onem2m-xsd.macros" as xsd with context -%}
{% if instanceType(object) == 'SDT4ModuleClass' -%}
{{ xsd.renderModuleClass(object) -}}
{% elif instanceType(object) == 'SDT4DeviceClass' -%}
{{ xsd.renderDeviceClass(object) -}}
{% if object.subDevices and object.subDevices | length > 0 -%}
{% for subDevice in object.subDevices -%}
{{ renderObject(subDevice) -}}
{% endfor -%}
{% endif -%}
{% elif instanceType(object) == 'SDT4SubDevice' -%}
{{ xsd.renderDeviceClass(object) -}}
{% elif instanceType(object) == 'SDT4DataTypes' -%}
{{ xsd.renderDataTypes(object) -}}
{% elif instanceType(object) == 'SDT4Action' -%}
{{ xsd.renderAction(object) -}}
{% elif instanceType(object) == 'SDT4Commons' -%}
{{ xsd.renderCommons(object) -}}
{% endif -%}
