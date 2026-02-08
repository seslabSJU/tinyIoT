#	SDT4PrintOneM2MSVG.py
#
#	Generate SVG in oneM2M's Resource format 
#
#	(c) 2015 by Andreas Kraft
#	License: Apache 2. See the LICENSE file for further details.

""" This module provides functions to generate SVG representations of SDT4ModuleClass and SDT4DeviceClass.
"""

from typing import Optional
import os, pathlib
from .SDT4Classes import *
from common.SDTSVG import *
from common.SDTHelper import *

# definition of cardinality constants
cardinality1 = '1'
""" Represents a cardinality of exactly one instance. """

cardinality01 = '0..1'
""" Represents a cardinality of zero or one instance. """

cardinality0n = '0..n'
""" Represents a cardinality of zero or more instances. """

# variable that hold an optional header text
headerText = ''
""" Represents the header text to be included in the SVG output."""

# variable that holds the domain prefix for the oneM2M XSD
namespacePrefix = ''
""" Represents the namespace prefix for the oneM2M XSD. """

# variable that holds the version of the data model
modelVersion:str = ''
""" Represents the version of the data model being used. """

# variable that holds whether datapoints should be exported as well
exportDataPoints = False
""" Represents whether data points should be exported in the SVG output. """

def print4OneM2MSVG(domain:SDT4Domain, options:Options, directory:str) -> None:
	""" Print the SDT in OneM2M SVG format.

		Args:
			domain: The SDT4Domain object to be printed.
			options: The options for printing the SDT.
			directory: The directory where the SVG files will be saved.
	"""
	global headerText, modelVersion, namespacePrefix, exportDataPoints

	lfile = options['licensefile']
	if lfile is not None:
		with open(lfile, 'rt') as f:
			headerText = f.read()

	# get the version of the data model
	modelVersion = options['modelversion']						# type:ignore[assignment]
	if not (namespacePrefix := options['namespaceprefix']):		# type:ignore[assignment]
		print('Error: name space prefix not set')
		return

	# Generate SVG's for ModuleClass attributes as well?
	exportDataPoints = options['svgwithattributes']				# type:ignore[assignment]

	# Create package path and make directories
	path = pathlib.Path(directory)
	try:
		path.mkdir(parents=True)
	except FileExistsError as e:
		pass # on purpose. We override files for now

	package = sanitizePackage(domain.id) if domain.id else ''

	# Export ModuleClasses
	for moduleClass in domain.moduleClasses:
		exportModuleClass(moduleClass, package, str(path))

	# Export Devices
	for deviceClass in domain.deviceClasses:
		exportDevice(deviceClass, package, str(path))
		for subDevice in deviceClass.subDevices:
			exportDevice(subDevice, package, str(path), parent=deviceClass)

	# Export possible SubDevices
	for subDevice in domain.subDevices:
		exportDevice(subDevice, package, str(path))


def cardinality(obj:SDT4ModuleClass|SDT4SubDevice) -> str:
	""" Get the cardinality for a ModuleClass or SubDevice.
		
		Args:
			obj: The ModuleClass or SubDevice object for which the cardinality is determined.
		
		Returns:
			A string representing the cardinality of the object.
	"""
	if obj.minOccurs == '0' and obj.maxOccurs == '1':
		return cardinality01
	if obj.minOccurs == '0' and obj.maxOccurs == 'n':
		return cardinality0n
	return cardinality1


#############################################################################

# Export a ModuleClass definition to a file
def exportModuleClass(module:SDT4ModuleClass, package:str, path:str, name:Optional[str]=None) -> None:
	""" Export a ModuleClass's SVG to a file.

		Args:
			module: The module class to be exported
			package: The package name
			path: The path to the directory where the module class is exported
			name: The name of the module class (optional, if not given then the name from the module is used)
	"""
	# export the module class itself

	name = sanitizeName(module.name, False)
	fileName = getVersionedFilename(name,
									'svg', 
									path=path, 
									outType=OutType.moduleClass,
									modelVersion=modelVersion,
									namespacePrefix=namespacePrefix)
	try:
		with open(fileName, 'w') as outputFile:
			outputFile.write(getModuleClassSVG(module, package, path, name))		
	except IOError as err:
		raise err


# Get the ModuleClass resource
def getModuleClassSVG(module:SDT4ModuleClass, package:str, path:str, name:str) -> str:
	""" Get the SVG for a ModuleClass resource

		Args:
			module: The module class to be drawn.
			package: The package name
			path: The path to the directory where the module class is exported
			name: The name of the module class

		Returns:
			The SVG representation of the module class as a string.
	"""
	res = Resource(sanitizeName(name, False), specialization=True)

	# TODO: Extends?
	# TODO: events?
	addModuleClassHeaderToResource(res)

	# Add properties
	for prop in module.properties:
		res.add(Attribute(sanitizeName(prop.name, False), cardinality=cardinality01 if prop.optional == "true" else cardinality1))

	# DataPoints 
	getDataPoints(res, module.data, name, path)
	# Actions
	getActions(res, module.actions, name, path)
	addModuleClassFooterToResource(res)
	return svgStart(res.width(), res.height(), headerText) + res.draw() + svgFinish()


# Add standard header attributes to a module class resource
def addModuleClassHeaderToResource(resource:Resource) -> None:
	""" Add standard header to a module class resource

		Args:
			resource: The resource to which the header is added
	"""
	resource.add(Attribute('containerDefinition', cardinality=cardinality1))
	resource.add(Attribute('ontologyRef', cardinality=cardinality01))
	resource.add(Attribute('contentSize', cardinality=cardinality1))


# Add standard footer to a module class resource
def addModuleClassFooterToResource(resource:Resource) -> None:
	""" Add standard footer to a module class resource

		Args:
			resource: The resource to which the footer is added
	"""
	resource.add(Resource('subscription', cardinality=cardinality0n))

#############################################################################

# Export a Device definition to a file
def exportDevice(device:SDT4DeviceClass|SDT4SubDevice, package:str, path:str, parent:Optional[SDT4DeviceClass]=None) -> None:
	""" Export a Device's SVG to a file.

		Args:
			device: The device to be exported
			package: The package name
			path: The path to the directory where the device is exported
			parent: The parent device class (if any)
	"""
	name = sanitizeName(device.id, False)
	pth:Optional[pathlib.Path] = None
	match device:
		case SDT4DeviceClass():
			pth = pathlib.Path(str(path) + os.sep + name)
		case SDT4SubDevice() if parent is not None and isinstance(parent, SDT4DeviceClass):
			pth = pathlib.Path(f'{str(path)}{os.sep}{sanitizeName(parent.id, False)}{os.sep}{name}')
		case SDT4SubDevice() if parent is None:
			pth = pathlib.Path(f'{str(path)}{os.sep}{name}')
		case _:
			return

	try:
		pth.mkdir(parents=True)
	except FileExistsError as e:
		pass # on purpose. We override files for now

	fileName = getVersionedFilename(name, 'svg', path=str(pth), modelVersion=modelVersion, namespacePrefix=namespacePrefix)
	try:
		with open(fileName, 'w') as outputFile:
			outputFile.write(getDeviceSVG(device, package, name))
	except IOError as err:
		raise err

	# export module classes of the device
	if exportDataPoints:
		for moduleClass in device.moduleClasses:
			exportModuleClass(moduleClass, package, str(pth), name)


# Get the Device resource

def getDeviceSVG(device:SDT4DeviceClass|SDT4SubDevice, package:str, name:str) -> str:
	""" Get the SVG for a Device resource

		Args:
			device: The device to be drawn.
			package: The package name
			name: The name of the device
	"""
	res = Resource(sanitizeName(name, False), specialization=True)
	addDeviceHeaderToResource(res)

	# Add properties
	for prop in device.properties:
		res.add( Attribute(sanitizeName(prop.name, False), cardinality=cardinality01 if prop.optional == "true" else cardinality1))

	# Add modules
	for module in device.moduleClasses:
		res.add(Resource(sanitizeName(module.name, False), cardinality=cardinality(module), specialization=True))

	# Add sub-devices
	if isinstance(device, SDT4DeviceClass):
		for subDevice in device.subDevices:
			res.add(Resource(sanitizeName(subDevice.id, False), cardinality=cardinality(subDevice), specialization=True))

	addDeviceFooterToResource(res)
	return svgStart(res.width(), res.height(), headerText) + res.draw() + svgFinish()


# Add standard header attributes to a device resource
def addDeviceHeaderToResource(resource:Resource) -> None:
	""" Add standard header to a device

		Args:
			resource: The resource to which the header is added
	"""
	resource.add(Attribute('containerDefinition', cardinality=cardinality1))
	resource.add(Attribute('ontologyRef', cardinality=cardinality01))
	resource.add(Attribute('contentSize', cardinality=cardinality1))
	resource.add(Attribute('nodeLink', cardinality=cardinality01))


# Add standard footer to a device resource
def addDeviceFooterToResource(resource:Resource) -> None:
	""" Add standard footer to a device
	
		Args:
			resource: The resource to which the footer is added
	"""
	resource.add(Resource('subscription', cardinality=cardinality0n))


#############################################################################

# Export a DataPoint definiton to a file
def exportDataPoint(dataPoint:SDT4DataPoint, moduleName:str, path:str) -> None:
	""" Export a DataPoint's SVG to a file.

		Args:
			dataPoint: The data point to be exported
			moduleName: The name of the module class
			path: The path to the directory where the data point is exported
	"""
	name = sanitizeName(dataPoint.name, False)
	mName = sanitizeName(moduleName, False)
	fileName = getVersionedFilename(mName + '_' + name, 'svg', path=str(path), modelVersion=modelVersion, namespacePrefix=namespacePrefix)
	try:
		with open(fileName, 'w') as outputFile:
			outputFile.write(getDataPointSVG(dataPoint))		
	except IOError as err:
		raise err


# Get the DataPoint resource
def getDataPointSVG(dataPoint:SDT4DataPoint) -> str:
	""" Get the SVG for a DataPoint resource

		Args:
			dataPoint: The data point to be drawn.
	"""
	res = Resource(sanitizeName(dataPoint.name, False), specialization=True)
	addDataPointHeaderToResource(res)
	# Nothing in between
	addDataPointFooterToResource(res)
	return svgStart(res.width(), res.height(), headerText) + res.draw() + svgFinish()


# Construct data points export
def getDataPoints(resource:Resource, dataPoints:list[SDT4DataPoint], moduleName:str, path:str) -> None:
	""" Construct for all data points for a module class.
		
		Args:
			resource: The resource to which the data points are added
			dataPoints: The list of data points to be added
			moduleName: The name of the module class
			path: The path to the directory where the data points are exported
	"""
	if dataPoints is None or len(dataPoints) == 0:
		return
	for dataPoint in dataPoints:
		# First add it to the resource
		resource.add(Attribute(sanitizeName(dataPoint.name, False), \
			cardinality=cardinality01 if dataPoint.optional == 'true' else cardinality1))
		# write out to a file
		if exportDataPoints:
			exportDataPoint(dataPoint, moduleName, path)


# Add standard header attributes to a data point resource
def addDataPointHeaderToResource(resource:Resource) -> None:
	""" Add standard header to a data point

		Args:
			resource: The resource to which the header is added
	"""
	resource.add(Attribute('containerDefinition', cardinality=cardinality1))
	resource.add(Attribute('ontologyRef', cardinality=cardinality01))
	resource.add(Attribute('contentSize', cardinality=cardinality1))


# Add standard footer to a data point resource
def addDataPointFooterToResource(resource:Resource) -> None:
	""" Add standard footer to a data point

		Args:
			resource: The resource to which the footer is added
	"""
	resource.add(Resource('subscription', cardinality=cardinality0n, specialization=False))


########################################################################

# Export an action definiton to a file
def exportAction(action:SDT4Action, moduleName:str, path:str) -> None:
	""" Export an action's SVG to a file.

		Args:
			action: The action to be exported
			moduleName: The name of the module class
			path: The path to the directory where the action is exported
	"""
	fileName = getVersionedFilename(sanitizeName(action.name, False),
									'svg', 
									path=str(path),
									outType=OutType.action,
									modelVersion=modelVersion,
									namespacePrefix=namespacePrefix)
	try:
		with open(fileName, 'w') as outputFile:
			outputFile.write(getActionSVG(action))
	except IOError as err:
		 raise err


# Get the Action resource
def getActionSVG(action:SDT4Action) -> str:
	""" Get the SVG for an Action resource

		Args:
			action: The action to be drawn.
	"""
	res = Resource(sanitizeName(action.name, False), specialization=True)
	addActionHeaderToResource(res)
	# Nothing in between
	addActionFooterToResource(res)
	return svgStart(res.width(), res.height(), headerText) + res.draw() + svgFinish()


# Construct actions export
def getActions(resource:Resource, actions:list[SDT4Action], moduleName:str, path:str) -> None:
	""" Construct for all actions for a module class.

		Args:
			resource: The resource to which the actions are added
			actions: The list of actions to be added
			moduleName: The name of the module class
			path: The path to the directory where the actions are exported
	"""

	if actions is None or len(actions) == 0:
		return
	for action in actions:
		# First add it to the resource
		resource.add(Attribute(sanitizeName(action.name, False),
					 cardinality=cardinality01 if action.optional == 'true' else cardinality1))
		# write out to a file
		exportAction(action, moduleName, path)


# Add standard header attributes to a device resource
def addActionHeaderToResource(resource:Resource) -> None:
	""" Add standard header to an action resource

		Args:
			resource: The resource to which the header is added
	"""
	resource.add(Attribute('containerDefinition', cardinality=cardinality1))
	resource.add(Attribute('ontologyRef', cardinality=cardinality01))
	resource.add(Attribute('contentSize', cardinality=cardinality1))


# Add standard footer to an  resource
def addActionFooterToResource(resource:Resource) -> None:
	""" Add standard footer to an action resource

		Args:
			resource: The resource to which the footer is added
	"""	
	resource.add(Resource('subscription', cardinality=cardinality0n))

