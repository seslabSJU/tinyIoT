#	SDT4oneM2MHelper.py
#
#	Helpers for oneM2M special cases
#
#	(c) 2015 by Andreas Kraft
#	License: Apache 2. See the LICENSE file for further details.

""" This module provides helper functions, for example, to prepare an SDT4Domain for oneM2M compatibility."""

from .SDT4Classes import *

def prepare4OneM2M(domain:SDT4Domain) -> None:
	""" Prepare the SDT4Domain for oneM2M by adding nodeLink and flexNodeLink properties.
		This function modifies the domain in place, adding necessary properties to device classes,
		subDevices, and product classes to ensure compatibility with oneM2M standards.

		Args:
			domain: The SDT4Domain object to be prepared for oneM2M.
	"""
	_prepareSubDevices(domain.subDevices)
	_prepareDeviceClasses(domain.deviceClasses)
	_prepareModuleClasses(domain.moduleClasses)
	_prepareProductClasses(domain.productClasses)


def _prepareProductClasses(productClasses:list[SDT4ProductClass]) -> None:
	""" Prepare product classes for oneM2M by adding nodeLink and flexNodeLink properties.

		Args:
			productClasses: A list of SDT4ProductClass objects to be prepared.
	"""
	for pc in productClasses:
		_prepareModuleClasses(pc.moduleClasses)
		_prepareSubDevices(pc.subDevices)


def _prepareDeviceClasses(deviceClasses:list[SDT4DeviceClass]) -> None:
	""" Prepare device classes for oneM2M by adding nodeLink and flexNodeLink properties.

		Args:
			deviceClasses: A list of SDT4DeviceClass objects to be prepared.
	"""
	for dc in deviceClasses:
		dc.properties.insert(0, _createProperty('flexNodeLink', 'string', True))
		dc.properties.insert(0, _createProperty('nodeLink', 'string', True))
		_prepareModuleClasses(dc.moduleClasses)
		_prepareSubDevices(dc.subDevices)


def _prepareSubDevices(subDevices:list[SDT4SubDevice]) -> None:
	""" Prepare subDevices for oneM2M by adding dataGenerationTime and result arguments.
		The changes are made directly to the subDevices and their moduleClasses.
		
		Args:
			subDevices: A list of SDT4SubDevice objects to be prepared.
	"""
	for sd in subDevices:
		_prepareModuleClasses(sd.moduleClasses)


def _prepareModuleClasses(moduleClasses:list[SDT4ModuleClass]) -> None:
	""" Prepare module classes for oneM2M by adding dataGenerationTime and result arguments.

		Args:
			moduleClasses: A list of SDT4ModuleClass objects to be prepared.
	"""
	for mc in moduleClasses:
		# Add dataGenerationTime attribute to every ModuleClass
		mc.data.insert(0, _createSimpleDataPoint('dataGenerationTime', 'datetime', True))
		_prepareActions(mc.actions)


def _prepareActions(actions:list[SDT4Action]) -> None:
	""" Prepare actions for oneM2M by adding dataGenerationTime and result arguments.

		Args:
			actions: A list of SDT4Action objects to be prepared.
	"""
	for action in actions:	# Add dataGenerationTime to Actions as well
		action.args.insert(0, _createArg('dataGenerationTime', 'datetime', True))
		# Add result if the action has a data type
		if action.type and action.type.tpe:
			action.args.append(_createArg('result', action.type.tpe, False))


def _createSimpleDataPoint(name:str, tpe:str, optional:bool) -> SDT4DataPoint:
	""" Create a simple data point for oneM2M.

		Args:
			name: The name of the data point.
			tpe: The type of the data point.
			optional: Whether the data point is optional or not.
		
		Returns:
			SDT4DataPoint: The created data point.
	"""
	return SDT4DataPoint({'name': name, 'optional': str(optional).lower(), 'type': tpe})


def _createArg(name:str, tpe:str, optional:bool) -> SDT4Arg:
	""" Create an argument element for oneM2M.

		Args:
			name: The name of the argument.
			tpe: The type of the argument.
			optional: Whether the argument is optional or not.
		
		Returns:
			SDT4Arg: The created argument.
	"""
	return SDT4Arg({'name': name, 'optional': str(optional).lower(), 'type': tpe})


def _createProperty(name:str, tpe:str, optional:bool) -> SDT4Property:
	""" Create a property for oneM2M.

		Args:
			name: The name of the property.
			tpe: The type of the property.
			optional: Whether the property is optional or not.
		
		Returns:
			SDT4Property: The created property.
	"""
	return SDT4Property({'name': name, 'optional': str(optional).lower(), 'type': tpe})