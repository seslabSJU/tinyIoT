#	SDT4Parser.py
#
#	Callback target class for the ElementTree parser to parse a SDT4
#
#	(c) 2015 by Andreas Kraft
#	License: Apache 2. See the LICENSE file for further details.

""" This module provides the SDT4Parser class, which extends ElementTree to parse SDT4 XML data.
"""

from __future__ import annotations
from typing import cast, Any, Optional, Callable

from enum import Enum
from .SDT4Classes import *
from xml.etree.ElementTree import ElementTree

class SDT4Tag(Enum):
	""" Enum for the SDT4 tags. This is used to identify the tags in the XML data.
		It also provides methods to check whether a tag is ignored or processable.
	"""
	actionTag = 'action'
	""" Represents an action tag in the SDT4 XML data. """

	actionsTag = 'actions'
	""" Represents a collection of actions in the SDT4 XML data. """

	argTag = 'arg'
	""" Represents an argument tag in the SDT4 XML data. """

	argsTag = 'args'
	""" Represents a collection of arguments in the SDT4 XML data. """

	arrayTypeTag = 'array'
	""" Represents an array type tag in the SDT4 XML data."""

	constraintTag = 'constraint'
	""" Represents a constraint tag in the SDT4 XML data. """

	constraintsTag = 'constraints'
	""" Represents a collection of constraints in the SDT4 XML data. """

	dataPointTag = 'datapoint'
	""" Represents a data point tag in the SDT4 XML data. """

	dataTag = 'data'
	""" Represents a data tag in the SDT4 XML data. This is used to define data types and properties. """

	dataTypeTag = 'datatype'
	""" Represents a data type tag in the SDT4 XML data. This is used to define data types. """

	dataTypesTag = 'datatypes'
	""" Represents a collection of data types in the SDT4 XML data. """

	deviceClassTag = 'deviceclass'
	""" Represents a device class tag in the SDT4 XML data. """

	deviceClassesTag = 'deviceclasses'
	""" Represents a collection of device classes in the SDT4 XML data. """

	domainTag = 'domain'
	""" Represents a domain tag in the SDT4 XML data. This is the root element of the SDT4 XML data. """

	enumTypeTag = 'enum'
	""" Represents an enumeration type tag in the SDT4 XML data. This is used to define enumerations. """

	enumValueTag = 'enumvalue'
	""" Represents an enumeration value tag in the SDT4 XML data. This is used to define values in enumerations. """

	eventTag = 'event'
	""" Represents an event tag in the SDT4 XML data. This is used to define events in module classes. """

	eventsTag = 'events'
	""" Represents a collection of events in the SDT4 XML data. """

	excludeTag = 'exclude'
	""" Represents an exclude tag in the SDT4 XML data. This is used to exclude certain elements from being extended. """

	extendDeviceTag = 'extenddevice'
	""" Represents an extend device tag in the SDT4 XML data. This is used to extend a device class. """

	extendTag = 'extend'
	""" Represents an extend tag in the SDT4 XML data. This is used to extend a device class or product class. """

	importsTag = 'imports'
	""" Represents an imports tag in the SDT4 XML data. This is used to import other SDT4 files. """

	includeTag = 'include'
	""" Represents an include tag in the SDT4 XML data. This is used to include other SDT4 files or elements. """

	moduleClassTag = 'moduleclass'
	""" Represents a module class tag in the SDT4 XML data. This is used to define module classes. """

	moduleClassesTag = 'moduleclasses'
	""" Represents a collection of module classes in the SDT4 XML data. """

	productClassTag = 'productclass'
	""" Represents a product class tag in the SDT4 XML data. This is used to define product classes. """

	productClassesTag = 'productclasses'
	""" Represents a collection of product classes in the SDT4 XML data. """

	propertiesTag = 'properties'
	""" Represents a collection of properties in the SDT4 XML data. This is used to define properties of module classes, device classes, and sub-devices. """

	propertyTag = 'property'
	""" Represents a property tag in the SDT4 XML data. This is used to define properties of module classes, device classes, and sub-devices. """

	simpleTypeTag = 'simple'
	""" Represents a simple type tag in the SDT4 XML data. This is used to define simple data types. """

	structTypeTag = 'struct'
	""" Represents a struct type tag in the SDT4 XML data. This is used to define structured data types. """

	subDeviceTag = 'subdevice'
	""" Represents a sub-device tag in the SDT4 XML data. This is used to define sub-devices of device classes or product classes. """

	subDevicesTag = 'subdevices'
	""" Represents a collection of sub-devices in the SDT4 XML data. This is used to define sub-devices of device classes or product classes. """

	# Document tags
	docTag = 'doc'
	""" Represents a documentation tag in the SDT4 XML data. This is used to provide documentation for elements. """

	ttTag = 'tt'
	""" Represents a <tt> documentation tag in the SDT4 XML data. This is used for inline code formatting in documentation. """

	emTag = 'em'
	""" Represents an <em> documentation tag in the SDT4 XML data. This is used for emphasis in documentation. """

	bTag = 'b'
	""" Represents a <b> documentation tag in the SDT4 XML data. This is used for bold text in documentation. """

	pTag = 'p'
	""" Represents a <p> documentation tag in the SDT4 XML data. This is used for paragraphs in documentation. """

	imgTag = 'img'
	""" Represents an <img> documentation tag in the SDT4 XML data. This is used to include images in documentation. """

	imgCaptionTag = 'caption'
	""" Represents a <caption> documentation tag in the SDT4 XML data. This is used to provide captions for images in documentation. """


	def isIgnored(self) -> bool:
		""" Check whether this tag is ignored or not.
			
			Returns:
				True if the tag is ignored, False otherwise.
		"""
		return self in _ignoredTags


	def isProcessable(self) -> bool:
		""" Check whether this tag is processable or not.
			
			Returns:
				True if the tag is processable, False otherwise.
		"""
		return self in _tagHandlers
	

	def handler(self) -> TagHandlerAction:
		""" Get the handler function and the allowed parent instances for this tag.
			
			Returns:
				A tuple containing the handler function and a tuple of allowed parent instances.
				If the tag is not processable, an exception is raised.
		"""
		return _tagHandlers[self]
	


# Static list of ignored tags
_ignoredTags = (
	SDT4Tag.actionsTag,
	SDT4Tag.argsTag,
	SDT4Tag.constraintsTag,
	SDT4Tag.dataTag,
	SDT4Tag.dataTypesTag,
	SDT4Tag.deviceClassesTag,
	SDT4Tag.eventsTag,
	SDT4Tag.extendDeviceTag,
	SDT4Tag.importsTag, 
	SDT4Tag.moduleClassesTag,
	SDT4Tag.productClassesTag,
	SDT4Tag.propertiesTag,
	SDT4Tag.subDevicesTag
)
"""List of tags that are ignored by the parser."""


class SDT4Parser(ElementTree):
	"""Parser for the SDT4 XML data. This class extends ElementTree and provides methods to parse the SDT4 XML data.
		It handles the start and end of elements, as well as the data and comments within the elements.
		It also maintains a stack of elements and a list of name spaces used in the SDT4 data.
	"""
	
	def __init__(self) -> None:
		"""Initialize the SDT4Parser.
		"""
		super().__init__()
		self.elementStack:list[SDT4Element] = []
		"""Stack of elements currently being processed. This is used to keep track of the current context in the XML data."""

		self.nameSpaces:list[str] = []
		"""List of name spaces used in the SDT4 data. This is used to keep track of the name spaces used in the XML data."""

		self.domain:Optional[SDT4Domain] = None
		"""The parsed SDT4Domain object. This is the result of parsing the SDT4 XML data."""


	def start(self, tag:str, attrib:ElementAttributes) -> None:
		"""Handle the start of an element.
			This method is called when an element starts in the XML data.
			It creates the appropriate object for the element and adds it to the stack.
			If the tag is not recognized, a SyntaxError is raised.
			
			Args:
				tag: The tag of the element.
				attrib: The attributes of the element.
		"""

		# First add the name space to the list of used name spaces
		uri, _, otag = tag[1:].partition("}")
		if uri not in self.nameSpaces:
			self.nameSpaces.append(uri)
		
		try:
			ntag = SDT4Tag(otag.lower())
		except Exception:
			raise SyntaxError(self._unknownTag(otag))

		# Check non-emptyness of attributes
		for at in attrib:
			if len(attrib[at].strip()) == 0:
				raise SyntaxError(f'empty attribute: {at} for element: {tag}')

		# Handle all elements 

		# The lastElem always contains the last element on the stack and is
		# used transparently in the code below.
		lastElem = self.elementStack[-1] if len(self.elementStack) > 0 else None

		# Call the handler function for that element tag.
		# First, chech whether this is allowed for the current parent, or raise an exception

		if ntag.isProcessable():
			(func, instances) = ntag.handler()
			if instances is None or isinstance(lastElem, instances):
				func(attrib, lastElem, self.elementStack)
			else:
				raise SyntaxError(f'{otag} definition is only allowed in {[v._name for v in instances]} elements')

		# Other tags to ignore / just containers
		elif ntag.isIgnored():
			pass

		# Encountered an unknown element
		else:
			raise SyntaxError(f'Unknown Element: {tag} {attrib}')


	def end(self, tag:str) -> None:
		"""Handle the end of an element.
			This method is called when an element ends in the XML data.
			It processes the element based on its tag and removes it from the stack.
			If the tag is not recognized, a SyntaxError is raised.
			
			Args:
				tag: The tag of the element that is ending.
		"""
		_, _, otag = tag[1:].partition("}")
		try:
			ntag = SDT4Tag(otag.lower())
		except Exception:
			raise SyntaxError(self._unknownTag(otag))
		if ntag == SDT4Tag.domainTag:
			self.domain = cast(SDT4Domain, self.elementStack.pop()) # Assign the domain to the parser as result
		elif ntag.isProcessable():
			obj = self.elementStack.pop()
			obj.endElement()

			match ntag:
				# Special handling for simplified data types. Copy the type to the dataPoint
				case SDT4Tag.dataPointTag | SDT4Tag.argTag:
					obj = cast(SDT4DataPoint, obj)
					if obj.dataPointType:
						match obj.dataPointType.type:
							case SDT4SimpleType():
								obj.dataPointType = obj.dataPointType.type
							case SDT4ArrayType():
								obj.dataPointType = obj.dataPointType.type
							case SDT4StructType():
								obj.dataPointType = obj.dataPointType.type
							case _:
								pass	# could be an extended type
				
				# Similar handling for the data type of array types
				case SDT4Tag.arrayTypeTag:
					obj = cast(SDT4ArrayType, obj)
					if obj.arrayType:
						match obj.arrayType.type:
							case SDT4SimpleType():
								obj.arrayType = obj.arrayType.type
							case SDT4ArrayType():
								obj.arrayType = obj.arrayType.type
							case SDT4StructType():
								obj.arrayType = obj.arrayType.type
							case _:
								pass
				
				# ... and SDT4Arg
				case SDT4Tag.argTag:
					obj = cast(SDT4Arg, obj)
					obj.type
					if obj.arrayType:
						match obj.arrayType.type:
							case SDT4SimpleType():
								obj.arrayType = obj.arrayType.type
							case SDT4ArrayType():
								obj.arrayType = obj.arrayType.type
							case SDT4StructType():
								obj.arrayType = obj.arrayType.type
							case _:
								pass

		else:
			# ignore others
			pass


	def data(self, data:str) -> None:
		"""Handle the data within an element.

			This method is called when data is found within an element in the XML data.

			Args:
				data: The data found within the element.
		"""
		if len(self.elementStack) < 1:
			return

		obj = self.elementStack[-1]
		match obj:
			case SDT4Doc():
				obj.addContent(' ' + ' '.join(data.split()))
			case SDT4DocTT() | SDT4DocEM() | SDT4DocB() | SDT4DocP() | SDT4DocIMG() | SDT4DocCaption():
				obj.addContent(' ' + ' '.join(data.split()))


	def comment(self, data:str) -> None: # ignore comments
		"""Handle comments in the XML data.

			Args:
				data: The comment data to be ignored.
		"""
		pass

	
	def _unknownTag(self, tag:str) -> str:
		""" Generate an error message for an unknown tag.

			Args:
				tag: The tag that is unknown.
			
			Returns:
				A string containing the error message.
		"""
		help = ''
		if tag.lower() == 'simpletype':	help = 'Do you mean "Simple"?'
		return f'unknown tag: {tag}. {help}'



def getAttribute(attrib:ElementAttributes, attribName:str) -> Optional[str]:
	"""	Retrieve an attribute from the given attributes dictionary.

		Args:
			attrib: The attributes dictionary from which to retrieve the attribute.
			attribName: The name of the attribute to retrieve.

		Returns:

	"""
	return attrib[attribName].strip() if attribName in attrib else None


#
#	Handler for each of the element types
#

def handleAction(attrib:ElementAttributes, lastElem:SDT4ModuleClass, elementStack:ElementList) -> None:
	"""	Create and add an Action.

		Args:
			attrib: The attributes of the action element.
			lastElem: The last element in the stack, which should be a ModuleClass.
			elementStack: The stack of elements being parsed.
	"""
	lastElem.actions.append(action := SDT4Action(attrib))
	elementStack.append(action)


def handleArg(attrib:ElementAttributes, lastElem:SDT4Action, elementStack:ElementList) -> None:
	"""	Create and add an Action argument.

		Args:
			attrib: The attributes of the argument element.
			lastElem: The last element in the stack, which should be an Action.
			elementStack: The stack of elements being parsed.
	"""
	lastElem.args.append(arg := SDT4Arg(attrib))
	elementStack.append(arg)


def handleArrayType(attrib:ElementAttributes, lastElem:SDT4DataType, elementStack:ElementList) -> None:
	"""	Create and add an ArrayType data type.

		Args:
			attrib: The attributes of the array type element.
			lastElem: The last element in the stack, which should be a DataType.
			elementStack: The stack of elements being parsed.
	"""
	arrayType = SDT4ArrayType(attrib)
	lastElem.type = arrayType
	elementStack.append(arrayType)


def handleB(attrib:ElementAttributes, lastElem:SDT4Doc, elementStack:ElementList) -> None:
	"""	Create and add a <b> documentation tag.

		Args:
			attrib: The attributes of the <b> element.
			lastElem: The last element in the stack, which should be a Doc.
			elementStack: The stack of elements being parsed.
	"""
	b = SDT4DocB(attrib)
	b.doc = lastElem.doc
	elementStack.append(b)


def handleConstraint(attrib:ElementAttributes, lastElem:SDT4DataType, elementStack:ElementList) -> None:
	"""	Create and add a Constraint element.

		Args:
			attrib: The attributes of the constraint element.
			lastElem: The last element in the stack, which should be a DataType.
			elementStack: The stack of elements being parsed.
	"""
	lastElem.constraints.append(constraint := SDT4Constraint(attrib))
	elementStack.append(constraint)


def handleDataPoint(attrib:ElementAttributes, lastElem:SDT4ModuleClass, elementStack:ElementList) -> None:
	"""	Create and add a DataPoint element.

		Args:
			attrib: The attributes of the data point element.
			lastElem: The last element in the stack, which should be a ModuleClass.
			elementStack: The stack of elements being parsed.
	"""
	lastElem.data.append(dataPoint := SDT4DataPoint(attrib))
	elementStack.append(dataPoint)


def handleDataType(attrib:ElementAttributes, lastElem:SDT4DataPoint|SDT4ArrayType|SDT4StructType|SDT4Domain|SDT4Action, elementStack:ElementList) -> None:
	"""	Create and add a DataType. Depending on the DataType the handling is a bit different.

		Args:
			attrib: The attributes of the data type element.
			lastElem: The last element in the stack, which should be a DataPoint, ArrayType, StructType, Domain, or Action.
			elementStack: The stack of elements being parsed.
	"""
	dataType = SDT4DataType(attrib)
	match lastElem:
		case SDT4ArrayType():
			lastElem.arrayType = dataType
		case SDT4StructType():
			lastElem.structElements.append(dataType)
		case SDT4Action():
			lastElem.type = dataType
		case SDT4Domain():
			lastElem.dataTypes.append(dataType)
		case SDT4DataPoint():
			lastElem.dataPointType = dataType
	elementStack.append(dataType)


def handleDeviceClass(attrib:ElementAttributes, lastElem:SDT4Domain, elementStack:ElementList) -> None:
	"""	Create and add a DeviceClass.

		Args:
			attrib: The attributes of the device class element.
			lastElem: The last element in the stack, which should be a Domain.
			elementStack: The stack of elements being parsed.
	"""
	lastElem.deviceClasses.append(device := SDT4DeviceClass(attrib))
	elementStack.append(device)


def handleDoc(attrib:ElementAttributes, lastElem:SDT4Element, elementStack:ElementList) -> None:
	"""	Create and add a documentation element.

		Args:
			attrib: The attributes of the documentation element.
			lastElem: The last element in the stack, which should be an Element.
			elementStack: The stack of elements being parsed.
	"""
	lastElem.doc = SDT4Doc(attrib)
	elementStack.append(lastElem.doc)


def handleDomain(attrib:ElementAttributes, lastElem:Any, elementStack:ElementList) -> None:
	"""	Create and add a Domain.

		Args:
			attrib: The attributes of the domain element.
			lastElem: The last element in the stack, which should be None or an Element.
			elementStack: The stack of elements being parsed.
	"""
	elementStack.append(SDT4Domain(attrib))


def handleEM(attrib:ElementAttributes, lastElem:SDT4Doc, elementStack:ElementList) -> None:
	"""	Create and add an <em> documentation tag.

		Args:
			attrib: The attributes of the <em> element.
			lastElem: The last element in the stack, which should be a Doc.
			elementStack: The stack of elements being parsed.
	"""
	em = SDT4DocEM(attrib)
	em.doc = lastElem.doc
	elementStack.append(em)


def handleEnumType(attrib:ElementAttributes, lastElem:SDT4DataType, elementStack:ElementList) -> None:
	"""	Create and add an Enum data type.

		Args:
			attrib: The attributes of the enum type element.
			lastElem: The last element in the stack, which should be a DataType.
			elementStack: The stack of elements being parsed.
	"""
	lastElem.type = SDT4EnumType(attrib)
	elementStack.append(lastElem.type)


def handleEnumValue(attrib:ElementAttributes, lastElem:SDT4EnumType, elementStack:ElementList) -> None:
	"""	Create and add a EnumValue element.

		Args:
			attrib: The attributes of the enum value element.
			lastElem: The last element in the stack, which should be an EnumType.
			elementStack: The stack of elements being parsed.
	"""
	lastElem.enumValues.append(value := SDT4EnumValue(attrib))
	elementStack.append(value)


def handleEvent(attrib:ElementAttributes, lastElem:SDT4ModuleClass, elementStack:ElementList) -> None:
	"""	Create and add an Event element.

		Args:
			attrib: The attributes of the event element.
			lastElem: The last element in the stack, which should be a ModuleClass.
			elementStack: The stack of elements being parsed.
	"""
	lastElem.events.append(event := SDT4Event(attrib))
	elementStack.append(event)


def handleExtendExclude(attrib:ElementAttributes, lastElem:SDT4Extend, elementStack:ElementList) -> None:
	"""	Create and add an Extends->Exclude element.

		Args:
			attrib: The attributes of the exclude element.
			lastElem: The last element in the stack, which should be an Extend.
			elementStack: The stack of elements being parsed.
	"""
	lastElem.excludes.append(extend := SDT4ExtendExclude(attrib))
	elementStack.append(extend)


def handleExtend(attrib:ElementAttributes, lastElem:SDT4ProductClass|SDT4Extendable, elementStack:ElementList) -> None:
	"""	Create and add an Extend element.

		Args:
			attrib: The attributes of the extend element.
			lastElem: The last element in the stack, which should be a ProductClass or Extendable.
			elementStack: The stack of elements being parsed.
	"""
	match lastElem:
		case SDT4ProductClass():
			lastElem.extendDevice.append(extend := SDT4DeviceClass(attrib))
			elementStack.append(extend)
		case _:
			lastElem.extend = SDT4Extend(attrib)
			elementStack.append(lastElem.extend)
			

def handleImg(attrib:ElementAttributes, lastElem:SDT4Doc, elementStack:ElementList) -> None:
	"""	Create and add a <img> documentation tag.

		Args:
			attrib: The attributes of the <img> element.
			lastElem: The last element in the stack, which should be a Doc.
			elementStack: The stack of elements being parsed.
	"""
	img = SDT4DocIMG(attrib)
	img.doc = lastElem.doc
	if src := getAttribute(attrib, 'src'):
		img.startImage(src)
	elementStack.append(img)


def handleImgCaption(attrib:ElementAttributes, lastElem:SDT4Doc, elementStack:ElementList) -> None:
	"""	Create and add a <caption> documentation tag.

		Args:
			attrib: The attributes of the <caption> element.
			lastElem: The last element in the stack, which should be a Doc.
			elementStack: The stack of elements being parsed.
	"""
	caption = SDT4DocCaption(attrib)
	caption.doc = lastElem.doc
	elementStack.append(caption)


def handleInclude(attrib:ElementAttributes, lastElem:SDT4Extend|SDT4Domain, elementStack:ElementList) -> None:
	"""	Create and add include elements. 
		Unfortunately, there are two "include" element types to handle.

		Args:
			attrib: The attributes of the include element.
			lastElem: The last element in the stack, which should be an Extend or Domain.
			elementStack: The stack of elements being parsed.
	"""
	match lastElem:
		case SDT4Extend():
			lastElem.includes.append(include := SDT4ExtendInclude(attrib))
			elementStack.append(include)
		case SDT4Domain():
			lastElem.includes.append(extendInclude := SDT4Include(attrib))
			elementStack.append(extendInclude)


def handleModuleClass(attrib:ElementAttributes, lastElem:SDT4Domain|SDT4ProductClass|SDT4DeviceClass|SDT4SubDevice, elementStack:ElementList) -> None:
	"""	Create and add a ModuleClass element.

		Args:
			attrib: The attributes of the module class element.
			lastElem: The last element in the stack, which should be a Domain, ProductClass, DeviceClass, or SubDevice.
			elementStack: The stack of elements being parsed.
	"""
	lastElem.moduleClasses.append(mc := SDT4ModuleClass(attrib))
	elementStack.append(mc)


def handleP(attrib:ElementAttributes, lastElem:SDT4Doc|SDT4DocP, elementStack:ElementList) -> None:
	"""	Create and add a <p> documentation tag.

		Args:
			attrib: The attributes of the <p> element.
			lastElem: The last element in the stack, which should be a Doc or DocP.
			elementStack: The stack of elements being parsed.
	"""
	p = SDT4DocP(attrib)
	p.doc = lastElem.doc
	p.startParagraph()
	elementStack.append(p)


def handleProductClass(attrib:ElementAttributes, lastElem:SDT4Domain, elementStack:ElementList) -> None:
	"""	Create and add a ProductClass.

		Args:
			attrib: The attributes of the product class element.
			lastElem: The last element in the stack, which should be a Domain.
			elementStack: The stack of elements being parsed.
	"""
	lastElem.productClasses.append(product := SDT4ProductClass(attrib))
	elementStack.append(product)


def handleProperty(attrib:ElementAttributes, lastElem:SDT4ProductClass|SDT4DeviceClass|SDT4SubDevice|SDT4ModuleClass, elementStack:ElementList) -> None:
	"""	Create and add a Property.

		Args:
			attrib: The attributes of the property element.
			lastElem: The last element in the stack, which should be a ProductClass, DeviceClass, SubDevice, or ModuleClass.
			elementStack: The stack of elements being parsed.
	"""
	lastElem.properties.append(prop := SDT4Property(attrib))
	elementStack.append(prop)


def handleSimpleType(attrib:ElementAttributes, lastElem:SDT4DataType|SDT4Property, elementStack:ElementList) -> None:
	"""	Create and add a SimpleType data type element.

		Args:
			attrib: The attributes of the simple type element.
			lastElem: The last element in the stack, which should be a DataType or Property.
			elementStack: The stack of elements being parsed.
	"""
	lastElem.type = SDT4SimpleType(attrib)
	elementStack.append(lastElem.type)


def handleStructType(attrib:ElementAttributes, lastElem:SDT4DataType, elementStack:ElementList) -> None:
	"""	Create and add Struct data type.

		Args:
			attrib: The attributes of the struct type element.
			lastElem: The last element in the stack, which should be a DataType.
			elementStack: The stack of elements being parsed.
	"""
	lastElem.type = SDT4StructType(attrib)
	elementStack.append(lastElem.type)


def handleSubDevice(attrib:ElementAttributes, lastElem:SDT4Domain|SDT4DeviceClass|SDT4ProductClass, elementStack:ElementList) -> None:
	"""	Create and add a SubDevice.

		This is a sub-device of a device class or product class.

		Args:
			attrib: The attributes of the sub-device element.
			lastElem: The last element in the stack, which should be a Domain, DeviceClass, or ProductClass.
			elementStack: The stack of elements being parsed.
	"""
	lastElem.subDevices.append(subDevice := SDT4SubDevice(attrib))
	elementStack.append(subDevice)


def handleTT(attrib:ElementAttributes, lastElem:SDT4Doc|SDT4DocP, elementStack:ElementList) -> None:
	"""	Create and add a <tt> documentation tag.

		This tag is used for inline code formatting in documentation.

		Args:
			attrib: The attributes of the <tt> element.
			lastElem: The last element in the stack, which should be a Doc or DocP.
			elementStack: The stack of elements being parsed.
	"""
	tt = SDT4DocTT(attrib)
	tt.doc = lastElem.doc
	elementStack.append(tt)

TagHandlerAction = tuple[Callable, Optional[tuple[type[SDT4Element], ...]]] # type:ignore[type-arg]
"""Type alias for the tag handler action, which is a tuple containing the handler function and a tuple of allowed parent instances."""

TagHandlers = dict[SDT4Tag, TagHandlerAction]
"""Type alias for the tag handlers, which is a dictionary mapping SDT4Tag to TagHandlerAction."""

# Static assignment of element types and (handlerFunction, (tuple of allowed parents))
# This only happens here bc of the function declarations above
_tagHandlers:TagHandlers = {
	SDT4Tag.actionTag 		: (handleAction, (SDT4ModuleClass,)),
	SDT4Tag.argTag 			: (handleArg, (SDT4Action,)),
	SDT4Tag.arrayTypeTag 	: (handleArrayType, (SDT4DataType,)),
	SDT4Tag.bTag 			: (handleB, (SDT4Doc, SDT4DocP)),
	SDT4Tag.constraintTag 	: (handleConstraint, (SDT4DataType,)),
	SDT4Tag.dataPointTag	: (handleDataPoint, (SDT4Event, SDT4ModuleClass)),
	SDT4Tag.dataTypeTag 	: (handleDataType, (SDT4Action, SDT4DataPoint, SDT4Event, SDT4Arg, SDT4SimpleType, SDT4StructType, SDT4ArrayType, SDT4Domain)),
	SDT4Tag.deviceClassTag 	: (handleDeviceClass, (SDT4Domain,)),
	SDT4Tag.docTag 			: (handleDoc, (SDT4Domain, SDT4ProductClass, SDT4DeviceClass, SDT4SubDevice, SDT4DataType, SDT4ModuleClass, SDT4Action, SDT4DataPoint, SDT4Event, SDT4EnumValue, SDT4Arg, SDT4Constraint, SDT4Property)),
	SDT4Tag.domainTag 		: (handleDomain, None),
	SDT4Tag.emTag 			: (handleEM, (SDT4Doc, SDT4DocP)),
	SDT4Tag.enumTypeTag 	: (handleEnumType, (SDT4DataType,)),
	SDT4Tag.enumValueTag 	: (handleEnumValue, (SDT4EnumType,)),
	SDT4Tag.eventTag 		: (handleEvent, (SDT4ModuleClass,)),
	SDT4Tag.excludeTag 		: (handleExtendExclude, (SDT4Extend,)),
	# SDT4Tag.extendTag 		: (handleExtend, (SDT4ModuleClass, SDT4DataType, SDT4ProductClass, SDT4SubDevice)),
	SDT4Tag.extendTag 		: (handleExtend, (SDT4Extendable, SDT4ProductClass)),
	SDT4Tag.imgTag 			: (handleImg, (SDT4Doc, SDT4DocP)),
	SDT4Tag.imgCaptionTag 	: (handleImgCaption, (SDT4DocIMG,)),
	SDT4Tag.includeTag 		: (handleInclude, (SDT4Domain, SDT4Extend)),
	SDT4Tag.moduleClassTag 	: (handleModuleClass, (SDT4Domain, SDT4ProductClass, SDT4DeviceClass, SDT4SubDevice)),
	SDT4Tag.pTag 			: (handleP, (SDT4Doc, SDT4DocP)),
	SDT4Tag.productClassTag	: (handleProductClass, (SDT4Domain,)),
	SDT4Tag.propertyTag		: (handleProperty, (SDT4ProductClass, SDT4DeviceClass, SDT4SubDevice, SDT4ModuleClass)),
	SDT4Tag.simpleTypeTag 	: (handleSimpleType, (SDT4DataType, SDT4Property)),
	SDT4Tag.structTypeTag	: (handleStructType, (SDT4DataType,)),
	SDT4Tag.subDeviceTag 	: (handleSubDevice, (SDT4Domain, SDT4DeviceClass, SDT4ProductClass)),
	SDT4Tag.ttTag 			: (handleTT, (SDT4Doc, SDT4DocP))
}
"""Static mapping of SDT4Tag to handler functions and allowed parent instances.
	This is used to determine which function to call when a tag is encountered in the XML data.
"""