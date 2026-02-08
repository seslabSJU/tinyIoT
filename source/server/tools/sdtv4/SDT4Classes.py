#	SDT4Classes.py
#
#	SDT4 Base Classes
#
#	(c) 2015 by Andreas Kraft
#	License: Apache 2. See the LICENSE file for further details.


""" This module defines the base classes for SDT4 elements, including domains, device classes, properties, and more.
"""

from __future__ import annotations
from typing import Optional, Any

class SDT4Element:
	""" Base class for all SDT4 elements.
		This class provides basic functionality for SDT4 elements, such as attribute handling and end element processing.
	"""

	_name = 'Element'
	""" The name of the SDT4 element, used for identification and processing. """

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4Element with optional attributes.
			
			Args:
				attrib: Optional attributes for the element.
		"""
		self.doc:Optional[SDT4Doc] = None
		""" The documentation associated with the element, if any. """


	# May be overwritten by a child class
	def endElement(self) -> None:
		""" End element processing.
			This method can be overridden by child classes to perform specific end element actions.
		"""
		pass


	def getAttribute(self, attrib:Optional[ElementAttributes], attribName:str, default:Optional[str]=None) -> Optional[str]:
		""" Get an attribute from the element's attributes.

			Args:
				attrib: The attributes of the element.
				attribName: The name of the attribute to retrieve.
				default: The default value to return if the attribute is not found.
			
			Returns:
				The value of the attribute if it exists, otherwise the default value.
		"""
		if attrib is None:
			return default
		return attrib[attribName].strip() if attribName in attrib else default	


class SDT4Extendable(SDT4Element):
	""" Base class for extendable SDT4 elements.
		This class is used for elements that can be extended with additional properties or actions.
	"""
	_name = 'Extendable'

	def __init__(self, attrib:Optional[ElementAttributes]) -> None:
		""" Initialize the SDT4Extendable with optional attributes.
			
			Args:
				attrib: Optional attributes for the extendable element.
		"""
		super().__init__(attrib)
		self.extend:Optional[SDT4Extend] = None
		""" The extend element associated with the extendable element, if any. """

#
#	Domain, Includes
#
class SDT4Domain(SDT4Element):
	""" Represents a domain in the SDT4 structure.
		This class contains information about the domain, including its ID, semantic URI, and various classes within the domain.
	"""
	_name = 'Domain'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4Domain with optional attributes.
			
			Args:
				attrib: Optional attributes for the domain.
		"""
		super().__init__(attrib)

		self._version = '4'
		""" The version of the SDT4 domain, default is '4'. """

		self.id = self.getAttribute(attrib, 'id')
		""" The ID of the domain, used for identification and processing. """

		self.semanticURI = self.getAttribute(attrib, 'semanticURI')
		""" The semantic URI of the domain, used for linking to external resources or documentation. """

		self.includes:list[SDT4Include] = []			# imports
		""" A list of includes in the domain, which can be SDT4Include instances. """

		self.dataTypes:list[SDT4DataType] = []
		""" A list of data types in the domain, which can be SDT4DataType instances. """

		self.moduleClasses:list[SDT4ModuleClass] = []
		""" A list of module classes in the domain, which can be SDT4ModuleClass instances. """

		self.subDevices:list[SDT4SubDevice] = []
		""" A list of sub-devices in the domain, which can be SDT4SubDevice instances. """

		self.deviceClasses:list[SDT4DeviceClass] = []
		""" A list of device classes in the domain, which can be SDT4DeviceClass instances. """

		self.productClasses:list[SDT4ProductClass] = []
		""" A list of product classes in the domain, which can be SDT4ProductClass instances. """


class SDT4Include(SDT4Element):
	""" Represents an include in the SDT4 structure.
		This class is used to include other SDT files or resources into the current domain.
	"""
	_name = 'Include'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4Include with optional attributes.
			
			Args:
				attrib: Optional attributes for the include.
		"""
		super().__init__(attrib)

		self.parse = self.getAttribute(attrib, 'parse')
		""" Whether to parse the included file or not. Default is None, which means it will be parsed. """

		self.href = self.getAttribute(attrib, 'href')
		""" The href attribute of the include, which specifies the file or resource to be included. """


#	Product
class SDT4ProductClass(SDT4Extendable):
	""" Represents a product class in the SDT4 structure.
		This class contains information about the product class, including its ID, semantic URI, properties, module classes, sub-devices, and extended devices.
	"""

	_name = 'ProductClass'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4ProductClass with optional attributes.
			
			Args:
				attrib: Optional attributes for the product class.
		"""
		super().__init__(attrib)

		self.id = self.getAttribute(attrib, 'name')
		""" The ID of the product class, used for identification and processing. """

		self.semanticURI = self.getAttribute(attrib, 'semanticURI')
		""" The semantic URI of the product class, used for linking to external resources or documentation. """

		self.properties:list[SDT4Property] = []
		""" A list of properties associated with the product class, which can be SDT4Property instances. """

		self.moduleClasses:list[SDT4ModuleClass] = []
		""" A list of module classes associated with the product class, which can be SDT4ModuleClass instances. """

		self.subDevices:list[SDT4SubDevice] = []
		""" A list of sub-devices associated with the product class, which can be SDT4SubDevice instances. """

		self.extendDevice:list[SDT4DeviceClass] = [] # actually an extend
		""" A list of extended devices associated with the product class, which can be SDT4DeviceClass instances. """


#	DeviceClass
class SDT4DeviceClass(SDT4Element):
	""" Represents a device class in the SDT4 structure.
		This class contains information about the device class, including its ID, semantic URI, properties, module classes, and sub-devices.
	"""
	_name = 'DeviceClass'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4DeviceClass with optional attributes.
			
			Args:
				attrib: Optional attributes for the device class.
		"""
		super().__init__(attrib)

		self.id = self.getAttribute(attrib, 'id')
		""" The ID of the device class, used for identification and processing. """

		self.semanticURI = self.getAttribute(attrib, 'semanticURI')
		""" The semantic URI of the device class, used for linking to external resources or documentation. """

		self.properties:list[SDT4Property] = []
		""" A list of properties associated with the device class, which can be SDT4Property instances. """

		self.moduleClasses:list[SDT4ModuleClass] = []
		""" A list of module classes associated with the device class, which can be SDT4ModuleClass instances. """

		self.subDevices:list[SDT4SubDevice] = []
		""" A list of sub-devices associated with the device class, which can be SDT4SubDevice instances. """


#	SubDevice
class SDT4SubDevice(SDT4Extendable):
	""" Represents a sub-device in the SDT4 structure.
		This class contains information about the sub-device, including its ID, semantic URI, properties, and module classes.
	"""
	_name = 'SubDevice'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4SubDevice with optional attributes.
			
			Args:
				attrib: Optional attributes for the sub-device.
		"""
		super().__init__(attrib)

		self.id = self.getAttribute(attrib, 'id')
		""" The ID of the sub-device, used for identification and processing. """

		self.semanticURI = self.getAttribute(attrib, 'semanticURI')
		""" The semantic URI of the sub-device, used for linking to external resources or documentation. """

		self.minOccurs = self.getAttribute(attrib, 'minOccurs')
		""" The minimum number of occurrences of the sub-device, used for validation. """

		self.maxOccurs = self.getAttribute(attrib, 'maxOccurs')
		""" The maximum number of occurrences of the sub-device, used for validation. """

		self.properties:list[SDT4Property] = []
		""" A list of properties associated with the sub-device, which can be SDT4Property instances. """

		self.moduleClasses:list[SDT4ModuleClass] = []
		""" A list of module classes associated with the sub-device, which can be SDT4ModuleClass instances. """


#	Properties
class SDT4Property(SDT4Element):
	""" Represents a property in the SDT4 structure.
		This class contains information about the property, including its name, optional status, semantic URI, value, and type.
	"""
	_name = 'Property'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4Property with optional attributes.
			
			Args:
				attrib: Optional attributes for the property.
		"""
		super().__init__(attrib)

		self.name = self.getAttribute(attrib, 'name')
		""" The name of the property, used for identification and processing. """

		self.optional = self.getAttribute(attrib, 'optional', 'false')
		""" Whether the property is optional or not. Default is 'false'. """

		self.semanticURI = self.getAttribute(attrib, 'semanticURI')
		""" The semantic URI of the property, used for linking to external resources or documentation. """

		self.value:Optional[str] = self.getAttribute(attrib, 'value')
		""" The value of the property, which can be a string or number. If not specified, it will be None. """

		# TODO self.type:Optional[SDT4DataType] = self.getAttribute(attrib, 'type')	
		
		self.type:Optional[SDT4SimpleType] = None
		""" The type of the property, which can be an SDT4DataType instance. If not specified, it will be None. """

		if attrib and 'type' in attrib:
			self.type = SDT4SimpleType({'type': self.getAttribute(attrib, 'type')}) # type:ignore[dict-item]


#	ModuleClass
class SDT4ModuleClass(SDT4Extendable):
	""" Represents a module class in the SDT4 structure.
		This class contains information about the module class, including its name, semantic URI, minimum and maximum occurrences, and various actions, data points, events, and properties.
	"""
	_name = 'ModuleClass'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4ModuleClass with optional attributes.
			
			Args:
				attrib: Optional attributes for the module class.
		"""
		super().__init__(attrib)

		self.name = self.getAttribute(attrib, 'name')
		""" The name of the module class, used for identification and processing. """

		self.semanticURI = self.getAttribute(attrib, 'semanticURI')
		""" The semantic URI of the module class, used for linking to external resources or documentation. """

		self.minOccurs = self.getAttribute(attrib, 'minOccurs')
		""" The minimum number of occurrences of the module class, used for validation. """

		self.maxOccurs = self.getAttribute(attrib, 'maxOccurs')
		""" The maximum number of occurrences of the module class, used for validation. """

		self.actions:list[SDT4Action] = []
		""" A list of actions associated with the module class, which can be SDT4Action instances. """

		self.data:list[SDT4DataPoint] = []
		""" A list of data points associated with the module class, which can be SDT4DataPoint instances. """

		self.events:list[SDT4Event] = []
		""" A list of events associated with the module class, which can be SDT4Event instances. """

		self.properties:list[SDT4Property]	= []
		""" A list of properties associated with the module class, which can be SDT4Property instances. """



#	Extend
class SDT4Extend(SDT4Element):
	""" Represents an extend element in the SDT4 structure.
		This class contains information about the domain and entity being extended, as well as lists of excluded and included elements.
	"""
	_name = 'Extend'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4Extend with optional attributes.
			
			Args:
				attrib: Optional attributes for the extend element.
		"""
		super().__init__(attrib)

		self.domain = self.getAttribute(attrib, 'domain')
		""" The domain being extended, used for identification and processing. """

		self.entity = self.getAttribute(attrib, 'entity')
		""" The entity being extended, used for identification and processing. """

		self.excludes:list[SDT4ExtendExclude] = []
		""" A list of excluded elements in the extend, which can be SDT4ExtendExclude instances. """

		self.includes:list[SDT4ExtendInclude] = []
		""" A list of included elements in the extend, which can be SDT4ExtendInclude instances. """

		if not self.domain:
			raise SyntaxError('Extend: "domain" attribute is missing')
		if not self.entity:
			raise SyntaxError('Extend: "entity" attribute is missing')


class SDT4ExtendExclude(SDT4Element):
	""" Represents an exclude element in the SDT4 extend structure.
		This class contains information about the name and type of the excluded element.
	"""
	_name = 'Exclude'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4ExtendExclude with optional attributes.
			
			Args:
				attrib: Optional attributes for the exclude element.
		"""
		super().__init__(attrib)

		self.name = self.getAttribute(attrib, 'name')
		""" The name of the excluded element, used for identification and processing. """

		self.type = self.getAttribute(attrib, 'type', 'datapoint')
		""" The type of the excluded element, default is 'datapoint'. This can be used to specify the type of the excluded element. """


class SDT4ExtendInclude(SDT4Element):
	""" Represents an include element in the SDT4 extend structure.
		This class contains information about the name and type of the included element.
	"""
	_name = 'Include'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4ExtendInclude with optional attributes.
			
			Args:
				attrib: Optional attributes for the include element.
		"""
		super().__init__(attrib)

		self.name = self.getAttribute(attrib, 'name')
		""" The name of the included element, used for identification and processing. """

		self.type = self.getAttribute(attrib, 'type', 'datapoint')
		""" The type of the included element, default is 'datapoint'. This can be used to specify the type of the included element. """


#	Action & Arg
class SDT4Action(SDT4Extendable):
	""" Represents an action in the SDT4 structure.
		This class contains information about the action, including its name, optional status, semantic URI, type, and arguments.
	"""
	_name = 'Action'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4Action with optional attributes.
			
			Args:
				attrib: Optional attributes for the action.
		"""
		# Action does not have 'extend' but it is added here to simplify handling in templates
		super().__init__(attrib)
		
		self.name = self.getAttribute(attrib, 'name')
		""" The name of the action, used for identification and processing. """

		self.optional = self.getAttribute(attrib, 'optional')
		""" Whether the action is optional or not. """
		
		self.semanticURI = self.getAttribute(attrib, 'semanticURI')
		""" The semantic URI of the action, used for linking to external resources or documentation. """

		self.type:Optional[SDT4DataType] = None
		""" The result (return) type of the action"""

		self.args:list[SDT4Arg] = []	# Could be also a data point, inserted during preparation, e.g. "result" attribute
		""" A list of arguments for the action, which can be SDT4Arg or SDT4DataPoint instances."""


#	Event
class SDT4Event(SDT4Element):
	""" Represents an event in the SDT4 structure.
		This class contains information about the event, including its name, optional status, semantic URI, and associated data points.
	"""
	_name = 'Event'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4Event with optional attributes.
			
			Args:
				attrib: Optional attributes for the event.
		"""
		super().__init__(attrib)

		self.name = self.getAttribute(attrib, 'name')
		""" The name of the event, used for identification and processing. """

		self.optional = self.getAttribute(attrib, 'optional', 'false')
		""" Whether the event is optional or not. Default is 'false'. """

		self.semanticURI = self.getAttribute(attrib, 'semanticURI')
		""" The semantic URI of the event, used for linking to external resources or documentation. """

		self.data:list[SDT4DataPoint] = []
		""" A list of data points associated with the event, which can be SDT4DataPoint instances. """

#	DataPoint
class SDT4DataPoint(SDT4Element):
	""" Represents a data point in the SDT4 structure.
		This class contains information about the data point, including its name, optional status, writable status, readable status, eventable status, default value, semantic URI, and data type.
	"""
	_name = 'DataPoint'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4DataPoint with optional attributes.
			
			Args:
				attrib: Optional attributes for the data point.
		"""
		super().__init__(attrib)

		self.name = self.getAttribute(attrib, 'name')
		""" The name of the data point, used for identification and processing. """

		self.optional = self.getAttribute(attrib, 'optional', 'false')
		""" Whether the data point is optional or not. Default is 'false'. """

		self.writable = self.getAttribute(attrib, 'writable', 'true')
		""" Whether the data point is writable or not. Default is 'true'. """

		self.readable = self.getAttribute(attrib, 'readable', 'true')
		""" Whether the data point is readable or not. Default is 'true'. """

		self.eventable = self.getAttribute(attrib, 'eventable', 'false')
		""" Whether the data point is eventable or not. Default is 'false'. """

		self.default = self.getAttribute(attrib, 'default')
		""" The default value of the data point, if specified. """

		self.semanticURI = self.getAttribute(attrib, 'semanticURI')
		""" The semantic URI of the data point, used for linking to external resources or documentation. """

		self.dataPointType:Optional[SDT4DataType]
		""" The type of the data point, which can be an SDT4DataType instance. If not specified, it will be None. """
		if attrib and 'type' in attrib:
			self.dataPointType = SDT4SimpleType({'type': self.getAttribute(attrib, 'type')})	# type:ignore[dict-item]
		else:
			self.dataPointType = SDT4SimpleType()


class SDT4Arg(SDT4DataPoint):
	""" Represents an argument in the SDT4 structure.
		This class is a specialized version of SDT4DataPoint, used for arguments in actions.
	"""
	_name = 'Arg'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4Arg with optional attributes.
			
			Args:
				attrib: Optional attributes for the argument.
		"""
		super().__init__(attrib)
		self.default = self.getAttribute(attrib, 'default')
		""" The default value of the argument, if specified. This is used to provide a default value for the argument in actions. """


#	DataTypes
class SDT4DataType(SDT4Extendable):
	""" Represents a data type in the SDT4 structure.
		This class contains information about the data type, including its name, unit of measure, semantic URI, constraints, and type.
	"""
	_name = 'DataType'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4DataType with optional attributes.
			
			Args:
				attrib: Optional attributes for the data type.
		"""

		super().__init__(attrib)

		self.name = self.getAttribute(attrib, 'name')
		""" The name of the data type, used for identification and processing. """
		
		self.unitOfMeasure = self.getAttribute(attrib, 'unitOfMeasure')
		""" The unit of measure for the data type, if applicable. """

		self.semanticURI = self.getAttribute(attrib, 'semanticURI')
		""" The semantic URI of the data type, used for linking to external resources or documentation. """

		self.constraints:list[SDT4Constraint] = []
		""" A list of constraints associated with the data type, which can be SDT4Constraint instances. """

		self.type:Optional[SDT4DataType] = None
		""" The type of the data type, which can be another SDT4DataType instance. """

		self.tpe:Optional[str] = self.getAttribute(attrib, 'type', None)
		""" The type of the data type as a string, if specified in the attributes. """

	
	def copy(self, SDT4DataType:SDT4DataType) -> None:
		""" Copy attributes from another SDT4DataType instance.
			
			Args:
				SDT4DataType: The instance from which to copy attributes.
		"""
		self.name = SDT4DataType.name
		self.unitOfMeasure = SDT4DataType.unitOfMeasure
		self.semanticURI = SDT4DataType.semanticURI
		self.constraints = SDT4DataType.constraints
		self.type = SDT4DataType.type
		self.tpe = SDT4DataType.tpe


class SDT4SimpleType(SDT4DataType):
	""" A simple data type, e.g. integer, string, boolean, etc.
		This is a base class for all simple types.
	"""
	_name = 'SimpleType'


class SDT4StructType(SDT4DataType):
	""" A struct data type, which can contain other data types.
	"""
	_name = 'Struct'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize a struct data type.
			
			Args:
				attrib: Optional attributes for the struct.
		"""
		super().__init__(attrib)

		self.structElements:list[SDT4DataType] = []
		""" A list of elements in the struct, which can be SDT4DataType instances. """


class SDT4ArrayType(SDT4DataType):
	""" Represents an array data type in the SDT4 structure.
		This class contains information about the array type, including its element type.
	"""


	_name = 'Array'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4ArrayType with optional attributes.
			
			Args:
				attrib: Optional attributes for the array type.
		"""
		super().__init__(attrib)

		self.arrayType:Optional[SDT4DataType] = None
		""" The type of the elements in the array, which can be another SDT4DataType instance. """


class SDT4EnumType(SDT4DataType):
	""" Represents an enumeration data type in the SDT4 structure.
		This class contains information about the enumeration, including its values.
	"""
	_name = 'Enum'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4EnumType with optional attributes.
			
			Args:
				attrib: Optional attributes for the enumeration type.
		"""
		super().__init__(attrib)

		self.enumValues:list[SDT4EnumValue] = []
		""" A list of enumeration values, which can be SDT4EnumValue instances. """


class SDT4EnumValue(SDT4Element):
	""" Represents an enumeration value in the SDT4 structure.
		This class contains information about the enumeration value, including its name, value, type, and semantic URI.
	"""
	_name = 'EnumValue'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4EnumValue with optional attributes.
			
			Args:
				attrib: Optional attributes for the enumeration value.
		"""
		super().__init__(attrib)

		self.name = self.getAttribute(attrib, 'name')
		""" The name of the enumeration value, used for identification and processing. """

		self.value = self.getAttribute(attrib, 'value')
		""" The value of the enumeration, which can be a string or number. """

		self.type = self.getAttribute(attrib, 'type', 'integer')
		""" The type of the enumeration value, which can be a string, integer, etc. """

		self.semanticURI = self.getAttribute(attrib, 'semanticURI')
		""" The semantic URI of the enumeration value, used for linking to external resources or documentation. """


class SDT4Constraint(SDT4Element):
	""" Represents a constraint in the SDT4 structure.
		This class contains information about the constraint, including its name, type, value, and semantic URI.
	"""

	_name = 'Constraint'

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4Constraint with optional attributes.
			
			Args:
				attrib: Optional attributes for the constraint.
		"""
		super().__init__(attrib)

		self.name = self.getAttribute(attrib, 'name')
		""" The name of the constraint, used for identification and processing. """

		self.type = self.getAttribute(attrib, 'type')
		""" The type of the constraint, which can be a string, integer, etc. """

		self.value = self.getAttribute(attrib, 'value')
		""" The value of the constraint, which can be a string or number. """

		self.semanticURI = self.getAttribute(attrib, 'semanticURI')
		""" The semantic URI of the constraint, used for linking to external resources or documentation. """

#
#	Doc & elements
#
class SDT4DocBase(SDT4Element):
	""" Base class for SDT4 documentation elements.
		This class provides basic functionality for SDT4 documentation elements, such as adding content and handling attributes.
	"""

	def __init__(self, attrib:Optional[ElementAttributes]=None) -> None:
		""" Initialize the SDT4DocBase with optional attributes.
			
			Args:
				attrib: Optional attributes for the documentation element.
		"""
		super().__init__(attrib)
		self.content = ''
		""" The content of the documentation element, which can be a string containing text or HTML-like tags. """


	def addContent(self, content:str) -> None:
		""" Add content to the documentation element.
			
			Args:
				content: The content to be added to the documentation element.
		"""
		self.content += content

	
class SDT4Doc(SDT4DocBase):
	""" Represents the main documentation element in the SDT4 structure.
		This class contains information about the documentation, including its name and content.
	"""
	_name = 'Doc'


class SDT4DocTT(SDT4DocBase):
	""" Represents a 'tt' (teletype) element in the SDT4 documentation.
		This class is used to add teletype-styled text to the documentation.
	"""
	_name = 'tt'

	def addContent(self, content:str) -> None:
		""" Add content to the 'tt' element.
			
			Args:
				content: The content to be added to the 'tt' element.
		"""
		super().addContent(' <tt>' + content + '</tt> ')


class SDT4DocEM(SDT4DocBase):
	""" Represents an 'em' (emphasis) element in the SDT4 documentation.
		This class is used to add emphasized text to the documentation.
	"""
	_name = 'em'

	def addContent(self, content:str) -> None:
		""" Add content to the 'em' element.

			Args:
				content: The content to be added to the 'em' element.
		"""
		super().addContent(' <em>' + content + '</em> ')


class SDT4DocB(SDT4DocBase):
	""" Represents a 'b' (bold) element in the SDT4 documentation.
		This class is used to add bold text to the documentation.
	"""

	_name = 'b'

	def addContent(self, content:str) -> None:
		""" Add content to the 'b' element.
		
			Args:
				content: The content to be added to the 'b' element.
		"""
		super().addContent(' <b>' + content + '</b> ')


class SDT4DocP(SDT4DocBase):
	""" Represents a paragraph element in the SDT4 documentation.
		This class is used to add paragraphs to the documentation with appropriate opening and closing tags.
	"""
	_name = 'p'

	def startParagraph(self) -> None:
		""" Start a paragraph element.
			This method is called to open a paragraph tag.
		"""
		super().addContent(' <p>')
	

	def addContent(self, content:str) -> None:
		""" Add content to the paragraph element.

			Args:
				content: The content to be added to the paragraph element.
		"""
		super().addContent(content)


	def endElement(self) -> None:
		""" End the paragraph element.
			This method is called to close the paragraph tag.
		"""
		super().addContent('</p> ')


class SDT4DocIMG(SDT4DocBase):
	""" Represents an image element in the SDT4 documentation.
		This class is used to add images to the documentation with a source attribute.
	"""
	_name = 'img'

	def startImage(self, src:str) -> None:
		""" Start an image element with the given source.
			
			Args:
				src: The source URL of the image.
		"""
		self.addContent(' <img')
		if (src != None):
			self.addContent(' src="' + src + '"')
		self.addContent('>')


	def addContent(self, content:str) -> None:
		""" Add content to the image element.
			
			Args:
				content: The content to be added to the image element.
		"""
		super().addContent(content)


	def endElement(self) -> None:
		""" End the image element.
			This method is called to close the image tag.
		"""
		self.addContent('</img> ')


class SDT4DocCaption(SDT4DocBase):
	""" Represents a caption element in the SDT4 documentation.
		This class is used to add captions to elements such as images or tables.
	"""
	_name = 'caption'

	def addContent(self, content:str) -> None:
		""" Add content to the caption element.
			
			Args:
				content: The content to be added to the caption.
		"""
		super().addContent('<caption>' + content + '</caption>')


#
#	Further Types
#

ElementAttributes = dict[str, str]
"""A dictionary representing attributes of an SDT4 element, where keys are attribute names and values are their corresponding values."""

ElementList = list[SDT4Element]
"""A list of SDT4 elements, which can include any subclass of `SDT4Element`."""

ContextT = dict[str, Any]
"""A dictionary representing the context in which SDT4 elements are processed, where keys are context names and values can be of any type."""

Options = dict[str, str|int|bool]
"""A dictionary representing options for processing SDT4 elements, where keys are option names and values can be strings, integers, or booleans."""

