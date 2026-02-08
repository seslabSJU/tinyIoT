#	SDT4PrintOPML.py
#
#	Print SDT4 to OPML
#
# 	(c) 2015 by Andreas Kraft
#	License: Apache 2. See the LICENSE file for further details.


""" This module provides functions to print an SDT4Domain in OPML format, including devices, modules, properties, and other elements.
"""

import html

from .SDT4Classes import *
from common.SDTHelper import *

hideDetails:bool = False
"""Global variable to control the visibility of details in the printed output."""


#
#	Print functions
#

def print4DomainOPML(domain:SDT4Domain, options:Options) -> str:
	"""	Print the SDT4Domain in OPML format.

		Args:
			domain: The SDT4Domain object to be printed.
			options: The options for printing the SDT.

		Returns:
			A string containing the OPML representation of the SDT4Domain.
	"""
	global hideDetails
	
	# set double white space as an indention for ompl files
	setTabChar('\t')
	
	hideDetails = options['hideDetails']	# type:ignore[assignment]

	result = f'''\
<?xml version="1.0" encoding="ISO-8859-1"?>
<opml version="1.0">
<head>
</head>
<body>
<outline text="{domain.id if domain.id else ""}">'''
	incTab()

	if domain.includes:
		result += f'{newLine()}<outline text="Includes">'
		incTab()
		for include in domain.includes:
			result += f'{newLine()}{printInclude(include)}'
		decTab()
		result += f'{newLine()}</outline>'

	result += f'{newLine()}{printDoc(domain.doc)}' if domain.doc and not hideDetails else ''

	if domain.moduleClasses:
		result += f'{newLine()}<outline text="ModuleClasses">'
		incTab()
		for module in domain.moduleClasses:
			result += f'{newLine()}{printModuleClass(module)}'
		decTab()
		result += f'{newLine()}</outline>'

	if domain.deviceClasses:
		result += f'{newLine()}<outline text="Devices">'
		incTab()
		for device in domain.deviceClasses:
			result += f'{newLine()}{printDevice(device)}'
		decTab()
		result += f'{newLine()}</outline>'

	decTab()
	result += f'''
</outline>
</body>
</opml>
'''
	return result


def printInclude(include:SDT4Include) -> str:
	"""	Print an include in OPML format.

		Args:
			include: The include to print.

		Returns:
			A string containing the OPML representation of the include.
	"""
	return f'<outline text="{include.href} ({include.parse})" />'


#
#	Device, SubDevice
#

def printDevice(device:SDT4DeviceClass) -> str:
	"""	Print a device in OPML format.

		Args:
			device: The device to print.

		Returns:
			A string containing the OPML representation of the device.
	"""
	result = f'<outline text="{device.id if device.id else ""}">'
	incTab()

	result += f'{newLine()}{printDoc(device.doc)}' if device.doc and not hideDetails else ''
	if device.properties:
		result += f'{newLine()}<outline text="Properties">'
		incTab()
		for prop in device.properties:
			result += f'{newLine()}{printProperty(prop)}'
		decTab()
		result += f'{newLine()}</outline>'
	if device.moduleClasses:
		result += f'{newLine()}<outline text="Modules">'
		incTab()
		for module in device.moduleClasses:
			result += f'{newLine()}{printModule(module)}'
		decTab()
		result += f'{newLine()}</outline>'
	if device.subDevices:
		result += f'{newLine()}<outline text="SubDevices">'
		incTab()
		for subDevice in device.subDevices:
			result += f'{newLine()}{printSubDevice(subDevice)}'
		decTab()
		result += f'{newLine()}</outline>'

	decTab()
	result += f'{newLine()}</outline>'
	return result


def printSubDevice(subDevice:SDT4SubDevice) -> str:
	"""	Print a sub-device in OPML format.

		Args:
			subDevice: The sub-device to print.

		Returns:
			A string containing the OPML representation of the sub-device.
	"""
	result = f'<outline text="{subDevice.id if subDevice.id else ""}">'
	incTab()

	result += f'{newLine()}{printDoc(subDevice.doc)}' if subDevice.doc and not hideDetails else ''
	if subDevice.properties:
		result += f'{newLine()}<outline text="Properties">'
		incTab()
		for prop in subDevice.properties:
			result += f'{newLine()}{printProperty(prop)}'
		decTab()
		result += f'{newLine()}</outline>'
	if subDevice.moduleClasses:
		result += f'{newLine()}<outline text="Modules">'
		incTab()
		for module in subDevice.moduleClasses:
			result += f'{newLine()}{printModule(module)}'
		decTab()
		result += f'{newLine()}</outline>'
	decTab()
	result += f'{newLine()}</outline>'
	return result


#
#	Property
#

def printProperty(prop:SDT4Property) -> str:
	"""	Print a property in OPML format.

		Args:
			prop: The property to print.

		Returns:
			A string containing the OPML representation of the property.
	"""
	result = f'<outline text="{prop.name if prop.name else ""}" >'
	incTab()
	result += f'{newLine()}{printDoc(prop.doc)}' if prop.doc else ''
	result += f'{newLine()}<outline text="{printSimpleTypeProperty(prop.type)}" />' if prop.type else ''
	opt = []
	result += f'{newLine()}<outline text="value: {prop.value}" />' if prop.value else ''
	presult = printOptional(prop.optional) if prop.optional else ''
	if presult:
		opt.append(presult)
	result += f'<outline text="{"&#10;".join(opt)}" />' if opt else ''
	decTab()
	result += f'{newLine()}</outline>'
	return result


#
#	Print Module, ModuleClass
#

def printModule(module:SDT4ModuleClass) -> str:
	"""	Print a module in OPML format.

		Args:
			module: The module to print.

		Returns:
			A string containing the OPML representation of the module.
	"""
	result =  f'<outline text="{module.name if module.name else ""}">'
	result += printModuleDetails(module) if not hideDetails else ''
	result += newLine() + '</outline>'
	return result


def printModuleClass(moduleClass:SDT4ModuleClass) -> str:
	"""	Print a module class in OPML format.

		Args:
			moduleClass: The module class to print.

		Returns:
			A string containing the OPML representation of the module class.
	"""
	result =  f'<outline text="{moduleClass.name}">'
	result += printModuleDetails(moduleClass) if moduleClass else ''
	result += f'{newLine()}</outline>'
	return result


def printModuleDetails(module:SDT4ModuleClass) -> str:
	"""	Print the details of a module in OPML format.

		Args:
			module: The module to print.

		Returns:
			A string containing the OPML representation of the module details.
	"""
	incTab()
	result = ''
	result += f'{newLine()}{printDoc(module.doc)}' if module.doc else ''
	result += f'{newLine()}{printExtends(module.extend)}' if module.extend else ''
	result += f'{newLine()}<outline text="optional: True" />' if module.minOccurs and module.minOccurs != '0' else ''
	if module.data:
		result += f'{newLine()}<outline text="DataPoints">'
		incTab()
		for data in module.data:
			result += f'{newLine()}{printDataPoint(data)}'
		decTab()
		result += f'{newLine()}</outline>'
	if module.actions:
		result += f'{newLine()}<outline text="Actions">'
		incTab()
		for action in module.actions:
			result += f'{newLine()}{printAction(action)}'
		decTab()
		result += f'{newLine()}</outline>'
	if module.events:
		result += f'{newLine()}<outline text="Events">'
		incTab()
		for event in module.events:
			result += f'{newLine()}{printEvent(event)}'
		decTab()
		result += f'{newLine()}</outline>'
	if module.properties:
		result += f'{newLine()}<outline text="Properties">'
		incTab()
		for prop in module.properties:
			result += f'{newLine()}{printProperty(prop)}'
		decTab()
		result += f'{newLine()}</outline>'

	decTab()
	return result


def printExtends(extend:SDT4Extend) -> str:
	"""	Print the extends information in OPML format.

		Args:
			extend: The extend information to print.

		Returns:
			A string containing the OPML representation of the extends information.
	"""
	return f'<outline text="Extends&#10;&#10;domain: {extend.domain}&#10;class: {extend.domain}" />'


#
#	Action, Argument
#

def printAction(action:SDT4Action) -> str:
	"""	Print an action in OPML format.

		Args:
			action: The action to print.

		Returns:
			A string containing the OPML representation of the action.
	"""
	result = f'<outline text="{action.name if action.name else ""}">'
	incTab()
	result += f'{newLine()}{printDoc(action.doc)}' if action.doc else ''
	result += f'{newLine()}<outline text="optional: {action.optional}" />' if action.optional else ''
	result += f'{newLine()}{printDataType(action.type, "Returns&#10;&#10;")}' if action.type else ''
	if action.args:
		result += f'{newLine()}<outline text="Args">'
		incTab()
		for argument in action.args:
			result += f'{newLine()}{printArgument(argument)}'
		decTab()
		result += f'{newLine()}</outline>'

	decTab()
	result += f'{newLine()}</outline>'
	return result


def printArgument(argument:SDT4Arg) -> str:
	"""	Print an argument in OPML format.

		Args:
			argument: The argument to print.

		Returns:
			A string containing the OPML representation of the argument.
	"""
	result = f'<outline text="{argument.name if argument.name else ""}">' 
	incTab()
	result += f'{newLine()}{printDataType(argument.dataPointType) if argument.dataPointType else ""}'
	decTab()
	result += f'{newLine()}</outline>'
	return result


#
#   Event
#

def printEvent(event:SDT4Event) -> str:
	"""	Print an event in OPML format.

		Args:
			event: The event to print.

		Returns:
			A string containing the OPML representation of the event.
	"""
	result = f'<outline text="{event.name}" >'
	incTab()
	result += newLine() + printDoc(event.doc) if event.doc else ''
	result += f'<outline text="optional: {event.optional}" />' if event.optional else ''
	if len(event.data) > 0:
		result += newLine() + '<outline text="DataPoints">'
		incTab()
		for dataPoint in event.data:
			result += newLine() + printDataPoint(dataPoint)
		decTab()
		result += newLine() + '</outline>'
	decTab()
	result += newLine() + '</outline>'
	return result

 

#
#	DataPoint
#

def printDataPoint(datapoint:SDT4DataPoint) -> str:
	"""	Print a data point in OPML format.

		Args:
			datapoint: The data point to print.

		Returns:
			A string containing the OPML representation of the data point.
	"""
	result = f'<outline text="{datapoint.name}">' if datapoint.name else ''
	incTab()
	result += newLine() + printDoc(datapoint.doc) if datapoint.doc else ''
	result += newLine() + printDataType(datapoint.dataPointType) if datapoint.dataPointType else ''

	# Handle some optional attributes
	opt = []
	presult = printOptional(datapoint.optional) if datapoint.optional else None
	if presult:
		opt.append(presult)
	presult = printReadable(datapoint.readable) if datapoint.readable else None
	if presult:
		opt.append(presult)
	presult = printWritable(datapoint.writable) if datapoint.writable else None
	if presult:
		opt.append(presult)
	presult = printEventable(datapoint.eventable) if datapoint.eventable else None
	if presult:
		opt.append(presult)
	if len(opt) > 0:
		result += f'<outline text="{"&#10;".join(opt)}" />'
	decTab()
	result += newLine() + '</outline>'
	return result


#
#	DataTypes
#

def printDataType(dataType:SDT4DataType, prefixtext:str='') -> str:
	"""	Print a data type in OPML format.

		Args:
			dataType: The data type to print.
			prefixtext: A prefix text to be added to the data type name.

		Returns:
			A string containing the OPML representation of the data type.
	"""
	match dataType:
		case SDT4SimpleType():
			result = printSimpleType(dataType, prefixtext)
		case SDT4StructType():
			result = printStructType(dataType, prefixtext)
		case SDT4ArrayType():
			result = printArrayType(dataType, prefixtext)
		case SDT4DataType():
			if dataType.extend:
				result = f'<outline text="{prefixtext}{dataType.extend.entity}" />'
			else:
				result = f'<outline text="{prefixtext}Unknown data type" />'
	return result


def printSimpleType(dataType:SDT4SimpleType, prefixtext:str='') -> str:
	"""	Print a simple data type in OPML format.

		Args:
			dataType: The simple data type to print.
			prefixtext: A prefix text to be added to the data type name.

		Returns:
			A string containing the OPML representation of the simple data type.
	"""
	result = ''
	if dataType.tpe:
		result += f'<outline text="{prefixtext}{dataType.tpe}">'
	elif dataType.extend:
		result += f'<outline text="{prefixtext}{dataType.extend.entity}">'
	incTab()
	result += newLine() + printDoc(dataType.doc) if dataType.doc else ''
	result += printDataTypeAttributes(dataType)
	for constraint in dataType.constraints:
		result += f'{newLine()}{printConstraint(constraint)}'
	decTab()
	result += f'{newLine()}</outline>'
	return result


def printSimpleTypeProperty(simpleType:SDT4SimpleType) -> str:
	"""	Print a simple type property in OPML format.

		Args:
			simpleType: The simple type to print.
		
		Returns:
			A string containing the OPML representation of the simple type property.
	"""
	return simpleType.tpe if simpleType.tpe else ''


def printStructType(dataType:SDT4StructType, prefixtext:str='') -> str:
	"""	Print a struct data type in OPML format.

		Args:
			dataType: The struct data type to print.
			prefixtext: A prefix text to be added to the data type name.
		
		Returns:
			A string containing the OPML representation of the struct data type.
	"""
	result = f'<outline text="{prefixtext}'
	result += f'{dataType.name}: ' if dataType.name else ''
	result += 'Struct">'
	incTab()
	result += f'{newLine()}{printDoc(dataType.doc)}' if dataType.doc else ''
	result += printDataTypeAttributes(dataType)
	for element in dataType.structElements:
		result += f'{newLine()}{printDataType(element)}'
	for constraint in dataType.constraints:
		result += f'{newLine()}{printConstraint(constraint)}'
	decTab()
	result += f'{newLine()}</outline>'
	return result


def printArrayType(dataType:SDT4ArrayType, prefixtext:str='') -> str:
	"""	Print an array data type in OPML format.

		Args:
			dataType: The array data type to print.
			prefixtext: A prefix text to be added to the data type name.
		
		Returns:
			A string containing the OPML representation of the array data type.
	"""
	result = f'<outline text="{prefixtext}'
	result += f'{dataType.name}: ' if dataType.name else ''
	result += 'Array">'
	incTab()
	result += newLine() + printDoc(dataType.doc) if dataType.doc else ''
	result += printDataTypeAttributes(dataType)
	result += newLine() + printDataType(dataType.arrayType) if dataType.arrayType else ''
	for constraint in dataType.constraints:
		result += newLine() + printConstraint(constraint)
	decTab()
	result += newLine() + '</outline>'
	return result


def printDataTypeAttributes(dataType:SDT4DataType) -> str:
	"""	Print the attributes of a data type in OPML format.

		Args:
			dataType: The data type whose attributes are to be printed.
		
		Returns:
			A string containing the OPML representation of the data type attributes.
	"""
	return f'{newLine()}<outline text="unitOfMeasure: {dataType.unitOfMeasure}" />' if dataType.unitOfMeasure else ''


def printConstraint(constraint:SDT4Constraint) -> str:
	"""	Print a constraint in OPML format.

		Args:
			constraint: The constraint to print.
		
		Returns:
			A string containing the OPML representation of the constraint.
	"""
	attr   = []
	result = ''
	attr.append(f'name: {constraint.name}') if constraint.name else ''
	attr.append(f'type: {constraint.type}') if constraint.type else ''
	attr.append(f'value: {constraint.value}') if constraint.value else ''
	result += f'<outline text="Constraint&#10;{"&#10;".join(attr)}">' if len(attr) > 0 else ''
	incTab()
	attr += f'{newLine()}{printDoc(constraint.doc)}' if constraint.doc else ''
	decTab()
	result += f'{newLine()}</outline>'
	return result


#
#	Doc
#
def printDoc(doc:SDT4Doc) -> str:
	"""	Print a document in OPML format.

		Args:
			doc: The document to print.
		
		Returns:
			A string containing the OPML representation of the document.
	"""
	result = ''
	incTab()
	s = html.escape(doc.content.strip(), quote=True).replace('"', '&quot;')
	result += f'{newLine()}<outline text="{s}" />'
	decTab()
	return result

#
#	various attribute
#
def printOptional(optional:str) -> str:
	"""	Print whether a property is optional in OPML format.

		Args:
			optional: A string indicating whether the property is optional.
		
		Returns:
			A string containing the OPML representation of the optional attribute, if present.
	"""
	return f'optional: {optional} ' if optional else ''


def printWritable(writable:str) -> str:
	"""	Print whether a property is writable in OPML format.

		Args:
			writable: A string indicating whether the property is writable.
		
		Returns:
			A string containing the OPML representation of the writable attribute, if present.
	"""
	return f'writable: {writable} ' if writable else ''


def printReadable(readable:str) -> str:
	"""	Print whether a property is readable in OPML format.

		Args:
			readable: A string indicating whether the property is readable.
		
		Returns:
			A string containing the OPML representation of the readable attribute, if present.
	"""
	return f'readable: {readable} ' if readable else ''


def printEventable(eventable:str) -> str:
	"""	Print whether a property is eventable in OPML format.

		Args:
			eventable: A string indicating whether the property is eventable.
		
		Returns:
			A string containing the OPML representation of the eventable attribute, if present.
	"""
	return f'eventable: {eventable} ' if eventable else ''

