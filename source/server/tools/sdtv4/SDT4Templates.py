#	SDT4Templates.py
#
#	Print SDT4 with templates
#
#	(c) 2015 by Andreas Kraft
#	License: Apache 2. See the LICENSE file for further details.

""" This module provides functions to render SDT4 domains using Jinja2 templates.
"""
from __future__ import annotations
from typing import Optional, cast
import re, os
from pathlib import Path
from typing import Any
from .SDT4Classes import *
from common.SDTHelper import *
from common.SDTAbbreviate import *
from rich import print, inspect

import jinja2


class SDT4DataTypes(object):
	"""	Class to hold data types for SDT4.
	"""

	def __init__(self, dataTypes:list[SDT4DataType]) -> None:
		""" Initialize the SDT4DataTypes object with a list of data types.

			Args:
				dataTypes: A list of SDT4DataType objects.
		"""
		self.dataTypes = dataTypes
		""" List of SDT4DataType objects. """


class SDT4Commons(object):
	"""	Class to hold common types for SDT4.
	"""

	def __init__(self, name:str) -> None:
		""" Initialize the SDT4Commons object with a name.

			Args:
				name: The name of the commons object.
		"""
		self.name = name
		""" Name of the commons object. """

		self.extendedModuleClasses:dict[str, str] = {}
		""" Dictionary of extended module classes. The key is the name of the module class, the value is the extended entity. """

		self.extendedModuleClassesExtend:dict[str, str] = {}
		""" Dictionary of extended module classes with their extend entity. The key is the name of the module class, the value is the extended entity. """

		self.extendedSubDevices:dict[str, str] = {}
		""" Dictionary of extended sub devices. The key is the name of the sub device, the value is the extended entity. """

		self.extendedSubDevicesExtend:dict[str, str] = {}
		""" Dictionary of extended sub devices with their extend entity. The key is the name of the sub device, the value is the extended entity. """


templates = {
	'markdown'		: ('markdown.tpl', None, True, None),
	'onem2m-xsd'	: ('onem2m-xsd.tpl', 'enumerationTypes', False, 'xsd'),
	'apjson'		: ('apjson.tpl', None, True, None),
}
""" Dictionary of templates for rendering SDT4 domains.
"""


# Mapping between domain and namespace prefix
# TODO make this mapping more configurable, maybe on the command line?

namespacePrefixMappings = {
	('agd', 'org.onem2m.agriculture',	'http://www.onem2m.org/xml/protocols/agriculturedomain', 'agriculturedomain'),
	('cid', 'org.onem2m.city',			'http://www.onem2m.org/xml/protocols/citydomain', 'citydomain'),
	('cod', 'org.onem2m.common',		'http://www.onem2m.org/xml/protocols/commondomain', 'commondomain'),
	('hd',  'org.onem2m.horizontal',	'http://www.onem2m.org/xml/protocols/horizontal', 'horizontaldomain'),
	('hed', 'org.onem2m.health',		'http://www.onem2m.org/xml/protocols/healthdomain', 'healthdomain'),
	('hod', 'org.onem2m.home',			'http://www.onem2m.org/xml/protocols/homedomain', 'homedomain'),
	('ind', 'org.onem2m.industry',		'http://www.onem2m.org/xml/protocols/industrydomain', 'industrydomain'),
	('mad', 'org.onem2m.management',	'http://www.onem2m.org/xml/protocols/managementdomain', 'managementdomain'),
	('mdd', 'org.onem2m.metadata',		'http://www.onem2m.org/xml/protocols/metadata', 'metadatadomain'),
	('psd', 'org.onem2m.publicsafety',	'http://www.onem2m.org/xml/protocols/publicsafetydomain', 'publicsafetydomain'),
	('rad', 'org.onem2m.railway',		'http://www.onem2m.org/xml/protocols/railwaydomain', 'railwaydomain'),
	('ved', 'org.onem2m.vehicular',		'http://www.onem2m.org/xml/protocols/vehiculardomain', 'vehiculardomain'),
}
""" Mapping between namespace prefix and domain."""


actions:set[SDT4Action] = set()
""" Set of actions found in the domain. This is used to render actions in the templates. """

enumTypes:set[SDT4EnumType] = set()
""" Set of enum types found in the domain. This is used to render enum types in the templates. """

extendedModuleClasses:dict[str, str] = {}
""" Dictionary of extended module classes. The key is the name of the module class, the value is the extended entity. """

extendedModuleClassesExtend:dict[str, str] = {}
""" Dictionary of extended module classes with their extend entity. The key is the name of the module class, the value is the extended entity. """

extendedSubDevices:dict[str, str] = {}
""" Dictionary of extended sub devices. The key is the name of the sub device, the value is the extended entity. """

extendedSubDevicesExtend:dict[str, str] = {}
""" Dictionary of extended sub devices with their extend entity. The key is the name of the sub device, the value is the extended entity. """

context:Optional[ContextT] = None
""" Global context for the templates. This is used to pass data to the templates. """

optionArgs:Optional[Options] = None
""" Global options for the templates. This is used to pass options to the templates. """

# constants for shortname file
_shortnameCSVFile = 'Shortnames.csv'
""" CSV file for shortnames. This file contains the shortnames used in the templates. """

_undefinedShortnamesCSVFile = 'UndefinedShortnames.csv'
""" CSV file for undefined shortnames. This file contains the shortnames that were not found in the shortnames file. """

_shortnameMAPFile = 'Shortnames.py'
""" Python file for shortnames. This file contains the shortnames used in the templates. """


def print4SDT(domain:SDT4Domain, options:Options, directory:Optional[str]=None) -> Optional[str]:
	""" Render the SDT4 domain using templates.

		Args:
			domain: The SDT4Domain object to be rendered.
			options: The options for rendering the templates.
			directory: The directory where the output files will be saved. If None, the output is printed to stdout.

		Returns:
			The rendered result as a string if the context is a single file, otherwise None.
	"""
	global context
	global optionArgs
	importOccursIn()
	context = getContext(domain, options, directory)
	optionArgs = options
	if context['isSingleFile']:
	# Read shortnames
		readShortnames(context['shortnamesinfile'])
		result = render(context['templateFile'], context)
		# Export NEW shortnames
		# TODO perhaps check whether the shortnames are really new?
		# exportShortnames(getShortnames(), _shortnameCSVFile, None, str(options['namespaceprefix']))
		exportShortnames(getNewShortnames(), _undefinedShortnamesCSVFile, None, str(options['namespaceprefix']))
		return result
	else:
		renderMultiple(context['templateFile'], context, domain)
		# Export shortnames happened in renderMultiple
	# printShortNames(context) # TODO improve printShortnames
	exportOccursIn()
	return None



def render(templateFile:str, context:ContextT) -> str:
	"""	Render a template with the given context.

		Args:
			templateFile: The template file to be use for rendering.
			context: The context to be used for rendering the template.

		Returns:
			The rendered result as a string.
	"""
	_, filename = os.path.split(templateFile)
	(path, _) = os.path.split(os.path.realpath(__file__))
	return jinja2.Environment(
        loader=jinja2.FileSystemLoader(path + '/templates'),
        trim_blocks=True,
        lstrip_blocks=True
    ).get_template(filename).render(context)



def renderMultiple(templateFile:str, context:ContextT, domain:SDT4Domain) -> None:
	""" Render multiple inputs with the given template and context. The output is
		written to files.

		Args:
			templateFile: The template file to be use for rendering.
			context: The context to be used for rendering the template.
			domain: The domain object to be used for rendering the template.
	"""
	try:
		path = context['path']
		if path:
			path.mkdir(parents=True)
	except FileExistsError as e:
		pass # on purpose. We override files for now
	
	# Prefix for shortname files
	prefix = f'{context["namespaceprefix"]}-{"dev-" if domain.id and domain.id.endswith("device") else "mod-"}'
	
	# Read shortnames
	readShortnames(context['shortnamesinfile'])
	# Add already existing shortnames
	readShortnames(f'{str(context["path"])}{os.sep}{prefix}{_shortnameCSVFile}', predefined=False)


	# Export ModuleClasses
	for moduleClass in domain.moduleClasses:
		context['object'] = moduleClass
		# Do an shortname first
		if moduleClass.name:
			toShortname(moduleClass.name)
		renderComponentToFile(context, outType=OutType.moduleClass)

	# Export Devices
	for subDevice in domain.subDevices:
		context['object'] = subDevice
		if subDevice.id:
			toShortname(subDevice.id)
		renderComponentToFile(context, outType=OutType.subDevice)

	# Export Devices
	for deviceClass in domain.deviceClasses:
		context['object'] = deviceClass
		if deviceClass.id:
			toShortname(deviceClass.id)
		renderComponentToFile(context, outType=OutType.device)

	# Export enum types

	context['object'] = SDT4DataTypes(domain.dataTypes)
	renderComponentToFile(context, outType=OutType.enumeration)
	# for dataType in domain.dataTypes:
	# 	if isinstance(dataType.type, SDT4EnumType):
	# 		print(dataType)
	# for enm in enumTypes:
	# 	context['object'] = enm
	# 	renderComponentToFile(context, isEnum=True)

	# Export found Actions
	for action in actions:
		context['object'] = action
		if action.name:
			toShortname(action.name)
		renderComponentToFile(context, outType=OutType.action)

	# Export extras
	commons = SDT4Commons('commonTypes')
	commons.extendedModuleClasses = extendedModuleClasses

	commons.extendedModuleClassesExtend = extendedModuleClassesExtend
	commons.extendedSubDevices = extendedSubDevices
	commons.extendedSubDevicesExtend = extendedSubDevicesExtend
	context['object'] = commons
	renderComponentToFile(context, outType=OutType.unknown)

	# Export shortnames
	# exportShortnames(getShortnames(),
	# 				 	f'{context["path"]}{os.sep}{prefix}{_shortnameCSVFile}',
	# 					None,
	# 					str(context['namespaceprefix']))
	exportShortnames(getNewShortnames(), 
					 	f'{context["path"]}{os.sep}{prefix}{_undefinedShortnamesCSVFile}', 
						None,
						str(context['namespaceprefix']))
		


def getContext(domain:SDT4Domain, options:Options, directory:Optional[str]=None) -> ContextT:
	""" Create the context for rendering the templates.

		Args:
			domain: The SDT4Domain object to be used for rendering.
			options: The options for rendering the templates.
			directory: The directory where the output files will be saved.

		Returns:
			A dictionary containing the context for rendering the templates.
	"""

	path = None
	if directory:
		path = Path(directory)

	# read the optional licensefile into the header
	lfile = options['licensefile']
	licenseText = ''
	if lfile != None:
		with open(lfile, 'rt') as f:
			licenseText = f.read()

	templateFile, enumerationsFile, isSingleFile, extension = templates[cast(str, options['outputFormat'])]
	return {
		'domain'						: domain,
	    'hideDetails'					: options['hideDetails'],
    	'markdownPageBreak'				: options['markdownPageBreak'],
    	'licensefile'					: options['licensefile'],
    	'namespaceprefix'				: options['namespaceprefix'],
    	'xsdtargetnamespace' 			: options['xsdtargetnamespace'],
    	'shortnamesinfile'				: options['shortnamesinfile'],
    	'modelversion'					: options['modelversion'],
    	'domaindefinition'				: options['domain'],
    	'CDTVersion'					: options['cdtversion'],
    	'license'						: licenseText,
    	'path'							: path,
		'package'						: sanitizePackage(domain.id) if domain.id else None,
		'templateFile'					: templateFile,
		'extension'						: extension,
		'isSingleFile'					: isSingleFile,
		'enumerationsFile'				: enumerationsFile,
		'extendedModuleClasses'			: extendedModuleClasses,
		'extendedModuleClassesExtend'	: extendedModuleClassesExtend,
		'extendedSubDevices'			: extendedSubDevices,
		'extendedSubDevicesExtend'		: extendedSubDevicesExtend,


    	# pointer to functions
    	'renderComponentToFile'			: renderComponentToFile,
    	'instanceType'					: instanceType,
    	'isString'						: lambda s : isinstance(s, str),
    	'getNSName'						: getNSName,
    	'incLevel'						: incLevel,
    	'decLevel'						: decLevel,
    	'getLevel'						: getLevel,
    	'match'							: match,
    	'addToActions'					: addToActions,
    	'getVersionedFilename'			: getVersionedFilename,
    	'sanitizeName'					: templateSanitizeName,
    	'renderObject'					: renderObject,
    	'debug'							: debug,
    	'countExtend'					: countExtend,
    	'countUnextend'					: countUnextend,
		'getNamespacePrefix'			: getNamespacePrefix,
		'componentName'					: componentName,
		'getNamespaceFromPrefix'		: getNamespaceFromPrefix,
		'getDomainFromPrefix'			: getDomainFromPrefix,
		'getXMLNameSpaces'				: getXMLNameSpaces,
		'shortname'						: shortname,

		# Add types
		'OutType'						: OutType,
	}


#############################################################################
#
#	Shortname output
#

def printShortNames(context:ContextT) -> None:
	""" Print short names to files. This is used to create a CSV file with short names
		for devices, sub-devices, module classes, data points, and actions.

		Args:
			context: The context to be used for rendering the templates.
	"""
	domain = context['domain']
	namespaceprefix = context['namespaceprefix'] if 'namespaceprefix' in context else None

	# TODO
	#
	#	Sort
	#	Uniqe
	#	combined files?

	# devices
	fileName = sanitizeName(f'devices-{getTimeStamp()}', False)
	fullFilename = getVersionedFilename(fileName, 'csv', path = str(context['path']), outType = OutType.shortName, modelVersion=context['modelversion'], namespacePrefix=namespaceprefix)
	with open(fullFilename, 'w') as outputFile:
		for deviceClass in domain.deviceClasses:
			outputFile.write(f'{deviceClass.id},{getShortname(deviceClass.id)}\n')
	deleteEmptyFile(fullFilename)

	# sub.devices - Instances
	fileName = sanitizeName(f'subDevicesInstances-{getTimeStamp()}', False)
	fullFilename = getVersionedFilename(fileName, 'csv', path=str(context['path']), outType = OutType.shortName, modelVersion=context['modelversion'], namespacePrefix=namespaceprefix)
	with open(fullFilename, 'w') as outputFile:
		for deviceClass in domain.deviceClasses:
			for subDevice in deviceClass.subDevices:
				outputFile.write(f'{subDevice.id},{getShortname(subDevice.id)}\n')
	deleteEmptyFile(fullFilename)

	# sub.devices
	fileName = sanitizeName(f'subDevice-{getTimeStamp()}', False)
	fullFilename = getVersionedFilename(fileName, 'csv', path=str(context['path']), outType = OutType.shortName, modelVersion=context['modelversion'], namespacePrefix=namespaceprefix)
	with open(fullFilename, 'w') as outputFile:
		for name in extendedSubDevicesExtend:
			if (abbr := getShortname(name)) is None:
				continue
			outputFile.write(f'{name},{abbr}\n')
	deleteEmptyFile(fullFilename)


	# ModuleClasses
	fileName = sanitizeName(f'moduleClasses-{getTimeStamp()}', False)
	fullFilename = getVersionedFilename(fileName, 'csv', path=str(context['path']), outType = OutType.shortName, modelVersion=context['modelversion'], namespacePrefix=namespaceprefix)
	with open(fullFilename, 'w') as outputFile:
		for moduleClass in domain.moduleClasses:
			outputFile.write(f'{moduleClass.name}{getShortname(moduleClass.name)}\n')
		for deviceClass in domain.deviceClasses:
			for moduleClass in deviceClass.moduleClasses:
				outputFile.write(f'{moduleClass.name},{getShortname(moduleClass.name)}\n')
			for subDevice in deviceClass.subDevices:
				for moduleClass in subDevice.moduleClasses:
					outputFile.write(f'{moduleClass.name},{getShortname(moduleClass.name)}\n')
	deleteEmptyFile(fullFilename)


	# DataPoints
	fileName = sanitizeName(f'dataPoints-{getTimeStamp()}', False)
	fullFilename = getVersionedFilename(fileName, 'csv', path=str(context['path']), outType = OutType.shortName, modelVersion=context['modelversion'], namespacePrefix=namespaceprefix)
	with open(fullFilename, 'w') as outputFile:
		for moduleClass in domain.moduleClasses:
			for dp in moduleClass.data:
				outputFile.write(f'{dp.name},{moduleClass.name},{getShortname(dp.name)}\n')
		for deviceClass in domain.deviceClasses:
			for moduleClass in deviceClass.moduleClasses:
				for dp in moduleClass.data:
					outputFile.write(f'{dp.name},{moduleClass.name},{getShortname(dp.name)}\n')
			for subDevice in deviceClass.subDevices:
				for moduleClass in deviceClass.moduleClasses:
					for dp in moduleClass.data:
						outputFile.write(f'{dp.name},{moduleClass.name},{getShortname(dp.name)}\n')
	deleteEmptyFile(fullFilename)


	# Actions
	fileName = sanitizeName(f'actions-{getTimeStamp()}', False)
	fullFilename = getVersionedFilename(fileName, 'csv', path=str(context['path']), outType = OutType.shortName, modelVersion=context['modelversion'], namespacePrefix=namespaceprefix)
	with open(fullFilename, 'w') as outputFile:
		for moduleClass in domain.moduleClasses:
			for ac in moduleClass.actions:
				outputFile.write(f'{ac.name},{getShortname(ac.name)}\n')
		for deviceClass in domain.deviceClasses:
			for moduleClass in deviceClass.moduleClasses:
				for ac in moduleClass.actions:
					outputFile.write(f'{ac.name},{getShortname(ac.name)}\n')
			for subDevice in deviceClass.subDevices:
				for moduleClass in deviceClass.moduleClasses:
					for ac in moduleClass.actions:
						outputFile.write(f'{ac.name},{getShortname(ac.name)}\n')
	deleteEmptyFile(fullFilename)



#############################################################################
#
#	Helpers for template processing
#


def renderComponentToFile(context:ContextT, 
						  name:Optional[str]=None, 
						  outType:OutType=OutType.unknown, 
						  namespaceprefix:Optional[str]=None) -> None:
	""" Render a component to a single file.

		Args:
			context: The context to be used for rendering the template.
			name: The name of the file to be created. If None, the name is derived from the object.	
			outType: The type of the output file. This is used to determine the file name and extension.
			namespaceprefix: The namespace prefix to be used in the file name. If None, the default namespace prefix is used.
	"""
	namespaceprefix = context['namespaceprefix'] if 'namespaceprefix' in context else None

	match outType:
		case OutType.subDevice if context['object'].extend:
			fileName = sanitizeName(context['object'].extend.entity, False)
		case OutType.enumeration:
			fileName = sanitizeName(context['enumerationsFile'], False)
		case _:
			fileName = sanitizeName(context['object'].name if outType in [ OutType.moduleClass, OutType.enumeration, OutType.action, OutType.unknown ] else context['object'].id, False)
	
	fullFilename = getVersionedFilename(fileName, context['extension'], name=name, path=str(context['path']), outType=outType, namespacePrefix=namespaceprefix)
	#print('---' + fullFilename)
	outputFile = None
	try:
		with open(fullFilename, 'w') as outputFile:
			# print(fullFilename)
			outputFile.write(render(context['templateFile'], context))
	except IOError as err:
		print(f'[red]File not found: {str(err)}')


def templateSanitizeName(name:str, 
						 isClass:bool, 
						 annc:bool=False, 
						 elementType:Optional[ElementType]=None, 
						 occursIn:Optional[str]=None) -> str:
	""" Sanitize a (file)name. Also add it to the list of shortnames. 

		Args:
			name: The name to be sanitized.
			isClass: Whether the name is a class name or not.
			annc: Whether the name is an announced resource or not.
			elementType: The type of the element, if applicable.
			occursIn: The context in which the element occurs, if applicable.

		Returns:
			The sanitized name as a string.
	"""

	sanitizedName = sanitizeName(name, isClass)
	# If this name is an announced resource, add "Annc" Postfix to both the
	# name as well as the shortname.
	if ':' not in name and optionArgs and 'shortnamelength' in optionArgs:	# ignore, for example, type/enum definitions
		abbr = toShortname(sanitizedName, optionArgs['shortnamelength'], elementType, occursIn)	# type:ignore[arg-type]
		addShortname(f'{sanitizedName}{"Annc" if annc else ""}', f'{abbr}{"Annc" if annc else ""}')
		# if annc:
		# 	addShortname(f'{result}Annc', f'{abbr}')
		# else:
		# 	addShortname(result, abbr)
		# if annc:
		# 	abbr = toShortname(result)
		# 	addShortname(result + 'Annc', abbr + 'Annc')
		# else:
		# 	abbr = toShortname(result)
		# 	addShortname(result, abbr)
	return sanitizedName


def instanceType(ty:SDT4DataType, withNameSpace:bool=True) -> Optional[str]:
	""" Return the type of an object as a string. Replace domain with namespace, if set.

		Args:
			ty: The type to be converted to a string.
			withNameSpace: Whether to include the namespace prefix in the result.

		Returns:
			The type as a string.
	"""
	# Handle just strings
	if isinstance(ty, str): # ty is just a string and contains the type already
		return ty
	# Handle normal classes
	tyn = type(ty).__name__
	if tyn in ('SDT4ModuleClass',
			   'SDT4ArrayType', 
			   'SDT4SimpleType', 
			   'SDT4EnumType', 
			   'SDT4Action', 
			   'SDT4DeviceClass', 
			   'SDT4SubDevice', 
			   'SDT4Commons',
			   'SDT4DataTypes',
			   'SDT4Property', 
			   'SDT4DataPoint', 
			   'SDT4Arg',
			   'NoneType'):
		return tyn

	# Handle data types
	ns = context['namespaceprefix']	# type:ignore[index]
	if ty.type is None and ty.extend is not None:
		if ty.extend.domain == context['domain'].id and ns is not None:	# type:ignore[index]
			return f'{ns}:{ty.extend.entity}' if withNameSpace else ty.extend.entity
		return f'{ty.extend.domain}:{ty.extend.entity}' if withNameSpace else ty.extend.entity
	return type(ty.type).__name__


def getNSName(name:str) -> str:
	""" Return the correct name, including namespace prefix, if set. 

		Args:
			name: The name to be converted to a string.

		Returns:
			The name as a string.
	"""
	return f'{context["namespaceprefix"]}:{name}' if context['namespaceprefix'] is not None else name	# type:ignore[index]


def countExtend(lst:list[SDT4Extendable]) -> int:
	""" Count the number of objects in a list that are extended.

		Args:
			lst: The list of objects to be counted.

		Returns:
			The number of objects that are extended.
	"""
	return sum(map(lambda o : o.extend is not None, lst))


def countUnextend(lst:list[SDT4Extendable]) -> int:
	""" Count the number of objects in a list that are not extended.

		Args:
			lst: The list of objects to be counted.

		Returns:
			The number of objects that are not extended.
	"""
	return len(lst) - countExtend(lst)


def shortname(name:str) -> str:
	"""	Create a short name for the given name. If the name is already a shortname, return it.
		If the name is not an shortname, create a new shortname and return it.

		Args:
			name: The name to be shortnames.

		Returns:
			The shortname of the name.
	"""
	assert(optionArgs)
	abbr = toShortname(name, cast(int, optionArgs['shortnamelength']))
	addShortname(name, abbr)
	return abbr
	


def getNamespacePrefix(obj:SDT4Element) -> str:
	"""	Try to map the extended domain to a short name prefix.
		Otherwise return the defined namespace prefix for this object.

		Args:
			obj: The object for which the namespace prefix is to be determined.

		Returns:
			The namespace prefix as a string.
	"""
	assert(context)
	if isinstance(obj, SDT4Extendable) and obj.extend is not None:
		domain = obj.extend.domain
		for (k,v,d, do) in namespacePrefixMappings:
			if domain and domain.startswith(v):
				return k
		print(f'[yellow]WARNING: No definition found for domain: {domain}')
		return cast(str, context['namespaceprefix'])
	return cast(str, context['namespaceprefix'])


def getNamespaceFromPrefix(prfx:str) -> str:
	"""	Try to find the full prefix from the short prefix.

		Args:
			prfx: The prefix for which the full prefix is to be found.

		Returns:
			The full prefix as a string.
	"""
	for (k,v,d, do) in namespacePrefixMappings:
		if k == prfx:
			return v
	raise ValueError(f'Namespace not found for prefix: {prfx}')


def getDomainFromPrefix(prfx:str) -> str:
	"""	Try to find the domain from the short prefix.

		Args:
			prfx: The prefix for which the domain is to be found.
		
		Returns:
			The domain as a string.
	"""
	for (k,v,d, do) in namespacePrefixMappings:
		if k == prfx:
			return do
	raise ValueError(f'Domain not found for prefix: {prfx}')


def getXMLNameSpaces() -> str:
	"""	Return a string with all XML namespaces defined in the namespacePrefixMappings.
	
		Returns:
			A string with all XML namespaces defined in the namespacePrefixMappings.
	"""
	result = ''
	for (k,v,d, do) in namespacePrefixMappings:
		result += f'xmlns:{k}="{d}" '
	return result


def componentName(obj:SDT4Element) -> str:
	"""	Return the name or id of an SDT object as a string.
		If the object extends another object then that name is returned.

		Args:
			obj: The object for which the name is to be returned.

		Returns:
			The name or id of the object as a string.
	"""
	if isinstance(obj, SDT4Extendable) and obj.extend:
		return obj.extend.entity	# type:ignore[return-value]
	return cast(str, obj.id if isinstance(obj, (SDT4DeviceClass, SDT4SubDevice, SDT4Domain)) else obj.name)	# type:ignore[attr-defined]


#
#	Header level
#

level = 1
""" Current indention level for headers. This is used to create headers in the templates."""

def incLevel(name:Optional[str]=None) -> str:
	""" Increment the current indention level. 

		Args:
			name: An optional name to be added to the header.
		
		Returns:
			A string with the current indention level and the name, if provided.
	"""
	global level
	level += 1
	return ''.rjust(level, '#') + ' ' + name if name else ''


def decLevel() -> str:
	""" Decrement the current indention level. 

		Returns:
			An empty string if the level is greater than 0, otherwise a string with the current indention level.
	"""
	global level
	level -= 1
	if level == 0:
		level = 1
	return ''


def getLevel() -> int:
	""" Return the current indention level. 

		Returns:
			The current indention level.
	"""
	return level


def debug(txt:str, obj:Optional[Any] = None) -> str:
	""" Print text to console.

		Args:
			txt: The text to be printed.
			obj: An optional object to be inspected.

		Returns:
			Always an empty string.
	"""
	print(txt)
	if obj is not None:
		inspect(obj)
	return ''


def match(expr:str, val:str) -> Optional[re.Match]:	# type:ignore[type-arg]
	""" Support method: provide matching to templates. 

		Args:
			expr: The regular expression to be used for matching.
			val: The value to be matched against the regular expression.

		Returns:
			A match object if the value matches the regular expression, otherwise None.
	"""
	return re.match(expr, val)


def addToActions(action:SDT4Action) -> str:
	""" Add an action to the list of action found during rendering. 

		Args:
			action: The action to be added to the list of actions.

		Returns:
			Always an empty string.
	"""
	actions.add(action)
	return ''


@jinja2.utils.pass_context
def renderObject(context:ContextT, obj:SDT4Element) -> str:
	""" Recursively render another (sub)object while the other is till rendering. 
	
		Args:
			context: The context to be used for rendering the template.
			obj: The object to be rendered.

		Returns:
			Always an empty string.
	"""
	newContext = {key: value for key, value in context.items()} # Copy the old context
	if isinstance(obj, SDT4SubDevice):
		if obj.extend is None:	# Only when not extending
			newContext['object'] = obj
			renderComponentToFile(newContext, outType = OutType.subDevice)
	return ''

